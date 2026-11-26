/*
 * Tests of the display list interpreter, through a GPU double that keeps
 * the frames that the interpreter sends it.
 */

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_endian.h>
#include <SDL3/SDL_stdinc.h>
#include <ultra64.h>

#include "color_images.h"
#include "frame.h"
#include "gfx_backend.h"
#include "rcp.h"
#include "rsp.h"
#include "test_support.h"
#include "tmem.h"

#define IMAGE_WIDTH 64
#define IMAGE_HEIGHT 32
#define CAPTURE_VERTEX_MAX 256
#define CAPTURE_DRAW_MAX 16
#define CAPTURE_PASS_MAX 8
#define CAPTURE_COPY_MAX 8
#define READ_BACK_COLOR 0xFF0000FFu // Opaque red

// The targets that the double has created and not destroyed
static int f_LiveTargetCount;

struct GfxTarget {
    uint32_t width;
    uint32_t height;
    bool isDepth;
};

// The last frame that reached the GPU double, and what it was asked for
static struct {
    GfxVertex vertices[CAPTURE_VERTEX_MAX];
    uint32_t vertexCount;
    GfxMaterial materials[CAPTURE_DRAW_MAX];
    uint32_t materialCount;
    GfxDraw draws[CAPTURE_DRAW_MAX];
    uint32_t drawCount;
    GfxPass passes[CAPTURE_PASS_MAX];
    uint32_t passCount;
    GfxCopy copies[CAPTURE_COPY_MAX];
    uint32_t copyCount;
    uint32_t uploadCount;
    uint32_t firstTexel; // The first texel of the frame's first upload
    int frameCount;
    int readCount;
    int writeCount;
    uint32_t lastWrittenRow; // The first row of the last write
    uint32_t writtenRowCount; // And its rows
} f_Gpu;

static uint16_t f_ColorImage[IMAGE_WIDTH * IMAGE_HEIGHT];
static uint16_t f_DepthImage[IMAGE_WIDTH * IMAGE_HEIGHT];
static uint16_t f_OtherImage[IMAGE_WIDTH * IMAGE_HEIGHT];
static uint16_t f_SmallImage[IMAGE_WIDTH * IMAGE_HEIGHT];
static uint16_t f_GrowingImage[IMAGE_WIDTH * IMAGE_HEIGHT];
static uint16_t f_CopiedImage[IMAGE_WIDTH * IMAGE_HEIGHT];
static uint16_t f_PlainTexture[IMAGE_WIDTH * IMAGE_HEIGHT]; // Never drawn
static uint16_t f_ShortImage[IMAGE_WIDTH * IMAGE_HEIGHT];
// A color image of half the rows, whose depth image has as many
static uint16_t f_HalfImage[IMAGE_WIDTH * (IMAGE_HEIGHT / 2)];
static uint16_t f_WideImage[IMAGE_WIDTH * IMAGE_HEIGHT];
static Gfx f_DisplayList[256];

GfxTarget *BackendCreateTarget(uint32_t width, uint32_t height, bool isDepth) {
    GfxTarget *target = SDL_calloc(1, sizeof(*target));

    target->width = width;
    target->height = height;
    target->isDepth = isDepth;
    f_LiveTargetCount++;
    return target;
}

void BackendDestroyTarget(GfxTarget *target) {
    if (target != NULL) {
        f_LiveTargetCount--;
    }

    SDL_free(target);
}

void BackendCopyTarget(GfxTarget *source, GfxTarget *destination, uint32_t width, uint32_t height) {
    (void)source;
    (void)destination;
    (void)width;
    (void)height;
}

void BackendExecuteFrame(const GfxFrame *frame) {
    f_Gpu.vertexCount = SDL_min(frame->vertexCount, CAPTURE_VERTEX_MAX);
    SDL_memcpy(f_Gpu.vertices, frame->vertices, f_Gpu.vertexCount * sizeof(GfxVertex));
    f_Gpu.materialCount = SDL_min(frame->materialCount, CAPTURE_DRAW_MAX);
    SDL_memcpy(f_Gpu.materials, frame->materials, f_Gpu.materialCount * sizeof(GfxMaterial));
    f_Gpu.drawCount = SDL_min(frame->drawCount, CAPTURE_DRAW_MAX);
    SDL_memcpy(f_Gpu.draws, frame->draws, f_Gpu.drawCount * sizeof(GfxDraw));
    f_Gpu.passCount = SDL_min(frame->passCount, CAPTURE_PASS_MAX);
    SDL_memcpy(f_Gpu.passes, frame->passes, f_Gpu.passCount * sizeof(GfxPass));
    f_Gpu.copyCount = SDL_min(frame->copyCount, CAPTURE_COPY_MAX);
    SDL_memcpy(f_Gpu.copies, frame->copies, f_Gpu.copyCount * sizeof(GfxCopy));
    f_Gpu.uploadCount = frame->uploadCount;
    if (frame->uploadCount != 0) {
        f_Gpu.firstTexel = frame->texels[frame->uploads[0].texelOffset];
    }
    f_Gpu.frameCount++;
}

void BackendReadTarget(GfxTarget *target, uint32_t width, uint32_t height, uint32_t *rgba) {
    uint32_t i;

    (void)target;
    for (i = 0; i < width * height; i++) {
        rgba[i] = READ_BACK_COLOR;
    }

    f_Gpu.readCount++;
}

void BackendWriteTarget(
    GfxTarget *target,
    uint32_t width,
    uint32_t firstRow,
    uint32_t rowCount,
    const uint32_t *rgba
) {
    (void)target;
    (void)width;
    (void)rgba;
    f_Gpu.lastWrittenRow = firstRow;
    f_Gpu.writtenRowCount = rowCount;
    f_Gpu.writeCount++;
}

static void ResetCapture(void) {
    SDL_zero(f_Gpu);
}

// The commands that start every test's display list: a 16-bit color image
// with its scissor
static Gfx *BeginList(Gfx *gfx) {
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_ColorImage);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT);
    return gfx;
}

static void RunList(Gfx *end) {
    gSPEndDisplayList(end++);
    RunRspTask(f_DisplayList);
}

// A fill in the fill mode draws one rectangle of the fill color, which
// writes alpha, without blending or depth
static void TestFillRectangle(void) {
    Gfx *gfx = BeginList(f_DisplayList);

    ResetCapture();
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, (GPACK_RGBA5551(255, 0, 0, 1) << 16) | GPACK_RGBA5551(255, 0, 0, 1));
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    RunList(gfx);

    EXPECT(f_Gpu.frameCount == 1);
    EXPECT(f_Gpu.passCount == 1 && f_Gpu.drawCount == 1);
    EXPECT(f_Gpu.passes[0].depth == NULL);
    EXPECT(f_Gpu.passes[0].target->width == IMAGE_WIDTH);
    EXPECT(f_Gpu.draws[0].pipelineKey == PIPELINE_WRITE_ALPHA);
    EXPECT(f_Gpu.vertexCount == 6);
    EXPECT(f_Gpu.vertices[0].color == 0xFF0000FF);
    // The whole image, corner to corner in clip space
    EXPECT_NEAR(f_Gpu.vertices[0].position[0], -1.0, 1e-6);
    EXPECT_NEAR(f_Gpu.vertices[0].position[1], 1.0, 1e-6);
    EXPECT_NEAR(f_Gpu.vertices[5].position[0], 1.0, 1e-6);
    EXPECT_NEAR(f_Gpu.vertices[5].position[1], -1.0, 1e-6);
}

// The vertices stored for a list of one texture rectangle in the cycle type,
// its corners in quarter pixels
static uint32_t CountTextureRectangleVertices(
    u32 cycleType,
    int left,
    int top,
    int right,
    int bottom
) {
    Gfx *gfx = BeginList(f_DisplayList);

    ResetCapture();
    gDPSetCycleType(gfx++, cycleType);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gSPTextureRectangle(gfx++, left, top, right, bottom, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    RunList(gfx);
    return f_Gpu.vertexCount;
}

// The vertices stored for a list of one fill, its corners in pixels
static uint32_t CountFillRectangleVertices(int left, int top, int right, int bottom) {
    Gfx *gfx = BeginList(f_DisplayList);

    ResetCapture();
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPFillRectangle(gfx++, left, top, right, bottom);
    RunList(gfx);
    return f_Gpu.vertexCount;
}

// The RDP draws a rectangle's rows from its top down to its bottom and each
// from its left to its right, so one whose top isn't above its bottom, or
// left left of its right, draws nothing
static void TestReversedRectanglesDrawNothing(void) {
    // A top that wraps in its 12 bits past the bottom, as a ball's or
    // shadow's does when it's projected about an image's height below the
    // screen
    EXPECT(CountTextureRectangleVertices(G_CYC_1CYCLE, 4 << 2, -4, 8 << 2, 4 << 2) == 0);
    // A quarter pixel above the bottom, at it and below it
    EXPECT(CountTextureRectangleVertices(G_CYC_1CYCLE, 4 << 2, (4 << 2) - 1, 8 << 2, 4 << 2) == 6);
    EXPECT(CountTextureRectangleVertices(G_CYC_1CYCLE, 4 << 2, 4 << 2, 8 << 2, 4 << 2) == 0);
    EXPECT(CountTextureRectangleVertices(G_CYC_1CYCLE, 4 << 2, (4 << 2) + 1, 8 << 2, 4 << 2) == 0);
    // And the same of the right
    EXPECT(CountTextureRectangleVertices(G_CYC_1CYCLE, (8 << 2) - 1, 0, 8 << 2, 4 << 2) == 6);
    EXPECT(CountTextureRectangleVertices(G_CYC_1CYCLE, 8 << 2, 0, 8 << 2, 4 << 2) == 0);
    EXPECT(CountTextureRectangleVertices(G_CYC_1CYCLE, (8 << 2) + 1, 0, 8 << 2, 4 << 2) == 0);
    // Fills and copies cover whole pixels, the bottom row and right column
    // too, so a row or column past them is the first that draws nothing
    EXPECT(CountFillRectangleVertices(0, 4, 7, 4) == 6);
    EXPECT(CountFillRectangleVertices(0, 5, 7, 4) == 0);
    EXPECT(CountFillRectangleVertices(7, 0, 7, 4) == 6);
    EXPECT(CountFillRectangleVertices(8, 0, 7, 4) == 0);
    EXPECT(CountTextureRectangleVertices(G_CYC_COPY, 0, 4 << 2, 7 << 2, 4 << 2) == 6);
    EXPECT(CountTextureRectangleVertices(G_CYC_COPY, 0, 5 << 2, 7 << 2, 4 << 2) == 0);
    EXPECT(CountTextureRectangleVertices(G_CYC_COPY, 7 << 2, 0, 7 << 2, 4 << 2) == 6);
    EXPECT(CountTextureRectangleVertices(G_CYC_COPY, 8 << 2, 0, 7 << 2, 4 << 2) == 0);
}

static Vp f_Viewport = {{
    {IMAGE_WIDTH * 2, IMAGE_HEIGHT * 2, G_MAXZ / 2, 0},
    {IMAGE_WIDTH * 2, IMAGE_HEIGHT * 2, G_MAXZ / 2, 0},
}};
static Mtx f_Identity;
static Vtx f_Triangle[3] = {
    {{{-1, -1, 0}, 0, {0, 0}, {0xFF, 0, 0, 0xFF}}},
    {{{1, -1, 0}, 0, {32 << 5, 0}, {0, 0xFF, 0, 0xFF}}},
    {{{-1, 1, 0}, 0, {0, 32 << 5}, {0, 0, 0xFF, 0xFF}}},
};

static Gfx *BeginTriangles(Gfx *gfx, u32 geometryMode) {
    gfx = BeginList(gfx);
    gSPViewport(gfx++, &f_Viewport);
    gSPMatrix(gfx++, &f_Identity, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gfx++, &f_Identity, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPLoadGeometryMode(gfx++, geometryMode);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    return gfx;
}

// A counterclockwise triangle under identity matrices lands on the
// viewport's pixels and keeps its vertices' colors
static void TestTriangleTransform(void) {
    Gfx *gfx = BeginTriangles(f_DisplayList, G_SHADE | G_SHADING_SMOOTH | G_CULL_BACK);

    ResetCapture();
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);

    EXPECT(f_Gpu.vertexCount == 3);
    EXPECT(f_Gpu.draws[0].pipelineKey == 0);
    // (-1, -1) is the lower left corner, which is (-1, -1) in clip space
    EXPECT_NEAR(f_Gpu.vertices[0].position[0], -1.0, 1e-5);
    EXPECT_NEAR(f_Gpu.vertices[0].position[1], -1.0, 1e-5);
    EXPECT_NEAR(f_Gpu.vertices[1].position[0], 1.0, 1e-5);
    EXPECT_NEAR(f_Gpu.vertices[2].position[1], 1.0, 1e-5);
    EXPECT_NEAR(f_Gpu.vertices[0].position[2], 0.5, 1e-3);
    EXPECT(f_Gpu.vertices[1].color == 0xFF00FF00);
    EXPECT_NEAR(f_Gpu.vertices[1].texCoord[0], 32.0, 1e-3);
    EXPECT(
        f_Gpu.materials[0].combine[2] ==
        (COMBINER_ZERO | (COMBINER_ZERO << 8) | (COMBINER_ZERO << 16) |
            ((uint32_t)COMBINER_SHADE << 24))
    );

    // Wound the other way it faces away
    gfx = BeginTriangles(f_DisplayList, G_SHADE | G_SHADING_SMOOTH | G_CULL_BACK);
    ResetCapture();
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 2, 1, 0);
    RunList(gfx);
    EXPECT(f_Gpu.vertexCount == 0);
}

// A projection whose w is the model's z, with the near plane, z = -w, at
// z = 1: clip space is (x, y, z - 2, z)
static Mtx f_DepthProjection;
// Red corners in front of the near plane, and a blue one behind the eye.
// The edges to it cross the near plane a quarter of the way along.
static Vtx f_NearTriangle[3] = {
    {{{-1, -1, 2}, 0, {0, 0}, {0xFF, 0, 0, 0xFF}}},
    {{{1, -1, 2}, 0, {0, 0}, {0xFF, 0, 0, 0xFF}}},
    {{{0, 3, -2}, 0, {0, 0}, {0, 0, 0xFF, 0xFF}}},
};

// The color a quarter of the way from red to blue, truncated, as packed
#define NEAR_PLANE_COLOR 0x3F00BFu

static void RunNearTriangle(u32 geometryMode, int first, int second, int third) {
    Gfx *gfx = BeginTriangles(f_DisplayList, geometryMode);

    ResetCapture();
    gSPMatrix(gfx++, &f_DepthProjection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPFogFactor(gfx++, 256, 0);
    gSPVertex(gfx++, f_NearTriangle, 3, 0);
    gSP1Triangle(gfx++, first, second, third, 0);
    RunList(gfx);
}

// The RSP cuts a triangle off at the near plane, making corners there
// whose colors it steps to along the edges, and draws the rest as a fan
static void TestNearPlaneClipping(void) {
    uint32_t i;

    RunNearTriangle(G_SHADE | G_SHADING_SMOOTH, 0, 1, 2);
    // The fan about the last corner: (near, A, near) and (A, B, near)
    EXPECT(f_Gpu.vertexCount == 6);
    for (i = 0; i < f_Gpu.vertexCount; i++) {
        EXPECT(f_Gpu.vertices[i].position[3] > 0.0f);
    }

    EXPECT((f_Gpu.vertices[0].color & 0xFFFFFF) == NEAR_PLANE_COLOR);
    EXPECT((f_Gpu.vertices[1].color & 0xFFFFFF) == 0xFF);
    EXPECT((f_Gpu.vertices[2].color & 0xFFFFFF) == NEAR_PLANE_COLOR);
    EXPECT((f_Gpu.vertices[3].color & 0xFFFFFF) == 0xFF);
    EXPECT((f_Gpu.vertices[5].color & 0xFFFFFF) == NEAR_PLANE_COLOR);
    // On the near plane: w is one and the depth zero
    EXPECT_NEAR(f_Gpu.vertices[0].position[3], 1.0, 1e-5);
    EXPECT_NEAR(f_Gpu.vertices[0].position[2], 0.0, 1e-5);
    EXPECT_NEAR(f_Gpu.vertices[0].position[0], -0.75, 1e-5);

    // The pieces face as the triangle does, and are culled as it is
    RunNearTriangle(G_SHADE | G_SHADING_SMOOTH | G_CULL_BACK, 0, 1, 2);
    EXPECT(f_Gpu.vertexCount == 6);
    RunNearTriangle(G_SHADE | G_SHADING_SMOOTH | G_CULL_BACK, 0, 2, 1);
    EXPECT(f_Gpu.vertexCount == 0);
}

// Behind the eye the fog goes to the end that z points to, here none, so
// the near plane's corners have none either
static void TestFogBehindEye(void) {
    uint32_t i;

    RunNearTriangle(G_SHADE | G_SHADING_SMOOTH | G_FOG, 0, 1, 2);
    EXPECT(f_Gpu.vertexCount == 6);
    for (i = 0; i < f_Gpu.vertexCount; i++) {
        EXPECT((f_Gpu.vertices[i].color >> 24) == 0);
    }
}

// x reaching past the screen's edge by half of w is clipped only once the
// clip ratio comes in from two to one
static void TestClipRatio(void) {
    static Vtx wide[3] = {
        {{{-1, -1, 0}, 0, {0, 0}, {0xFF, 0, 0, 0xFF}}},
        {{{3, -1, 0}, 0, {0, 0}, {0xFF, 0, 0, 0xFF}}},
        {{{-1, 1, 0}, 0, {0, 0}, {0xFF, 0, 0, 0xFF}}},
    };
    Gfx *gfx = BeginTriangles(f_DisplayList, G_SHADE);
    uint32_t i;

    ResetCapture();
    gSPVertex(gfx++, wide, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);
    // The fan (A, cut, C) and (cut, cut, C), whose first cut is at x = 2
    EXPECT(f_Gpu.vertexCount == 6);
    EXPECT_NEAR(f_Gpu.vertices[1].position[0], 2.0, 1e-5);

    gfx = BeginTriangles(f_DisplayList, G_SHADE);
    ResetCapture();
    gSPClipRatio(gfx++, FRUSTRATIO_1);
    gSPVertex(gfx++, wide, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);
    EXPECT(f_Gpu.vertexCount == 6);
    for (i = 0; i < f_Gpu.vertexCount; i++) {
        EXPECT(f_Gpu.vertices[i].position[0] <= 1.0f + 1e-5f);
    }

    EXPECT_NEAR(f_Gpu.vertices[1].position[0], 1.0, 1e-5);
}

// Loads a 4-texel square for tile 0 and an 8-texel one for tile 1, so a
// material's samplers tell which tile each read
static Gfx *LoadTwoTiles(Gfx *gfx) {
    static u16 texels[8 * 8] __attribute__((aligned(8)));

    gDPLoadTextureBlock(
        gfx++,
        texels,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        4,
        4,
        0,
        G_TX_CLAMP,
        G_TX_CLAMP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPLoadMultiBlock(
        gfx++,
        texels,
        0x100,
        1,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        8,
        8,
        0,
        G_TX_CLAMP,
        G_TX_CLAMP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    return gfx;
}

static Gfx *DrawTriangle(Gfx *gfx) {
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    gDPPipeSync(gfx++);
    return gfx;
}

// The texels move down the RDP's pipeline between two cycles, so the second
// cycle's TEXEL0 reads the next tile and its TEXEL1 the first. One cycle
// reads the first tile for either.
static void TestTexelsOfEachCycle(void) {
    Gfx *gfx = BeginTriangles(f_DisplayList, G_SHADE | G_SHADING_SMOOTH);

    ResetCapture();
    gfx = LoadTwoTiles(gfx);
    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPSetRenderMode(gfx++, G_RM_PASS, G_RM_OPA_SURF2);
    gDPSetCombineLERP(gfx++, 0, 0, 0, SHADE, 0, 0, 0, 1, 0, 0, 0, TEXEL0, 0, 0, 0, 1);
    gfx = DrawTriangle(gfx);
    gDPSetCombineLERP(gfx++, 0, 0, 0, SHADE, 0, 0, 0, 1, 0, 0, 0, TEXEL1, 0, 0, 0, 1);
    gfx = DrawTriangle(gfx);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL1, 0, 0, 0, 1, 0, 0, 0, COMBINED, 0, 0, 0, 1);
    gfx = DrawTriangle(gfx);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL1, 0, 0, 0, 1, 0, 0, 0, TEXEL1, 0, 0, 0, 1);
    gfx = DrawTriangle(gfx);
    RunList(gfx);

    EXPECT(f_Gpu.materialCount == 4);
    // The second cycle's TEXEL0 is the next tile's texel
    EXPECT(f_Gpu.materials[0].tiles[0].size[0] == 0);
    EXPECT(f_Gpu.materials[0].tiles[1].size[0] == 8);
    // Its TEXEL1 is the first tile's
    EXPECT(f_Gpu.materials[1].tiles[0].size[0] == 4);
    EXPECT(f_Gpu.materials[1].tiles[1].size[0] == 0);
    // The first cycle reads them as named
    EXPECT(f_Gpu.materials[2].tiles[0].size[0] == 0);
    EXPECT(f_Gpu.materials[2].tiles[1].size[0] == 8);
    // One cycle has one tile's texel
    EXPECT(f_Gpu.materials[3].tiles[0].size[0] == 4);
    EXPECT(f_Gpu.materials[3].tiles[1].size[0] == 0);
}

// A threshold compare reads the blend color's alpha; a dithered one
// compares with a random threshold instead
static void TestAlphaCompareModes(void) {
    Gfx *gfx = BeginTriangles(f_DisplayList, G_SHADE | G_SHADING_SMOOTH);

    ResetCapture();
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_DITHER);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);

    EXPECT(f_Gpu.materialCount == 2);
    EXPECT(
        (f_Gpu.materials[0].modes & (MATERIAL_ALPHA_THRESHOLD | MATERIAL_ALPHA_DITHER)) ==
        MATERIAL_ALPHA_THRESHOLD
    );
    EXPECT(
        (f_Gpu.materials[1].modes & (MATERIAL_ALPHA_THRESHOLD | MATERIAL_ALPHA_DITHER)) ==
        (MATERIAL_ALPHA_THRESHOLD | MATERIAL_ALPHA_DITHER)
    );
}

// Draws a textured rectangle in the given cycle type with the given filter,
// as a material of its own
static Gfx *DrawFilteredRectangle(Gfx *gfx, u32 cycleType, u32 filter) {
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, cycleType);
    gDPSetTextureFilter(gfx++, filter);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gSPTextureRectangle(gfx++, 0, 0, 7 << 2, 7 << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    return gfx;
}

// Whether the material at this index samples its tiles with a filter
static bool IsMaterialFiltered(int materialIndex) {
    return (f_Gpu.materials[materialIndex].modes & MATERIAL_FILTERED) != 0;
}

// The combiner's texels are filtered under bilerp and averaging, in either
// cycle type that combines, and point sampled otherwise
static void TestTextureFilterModes(void) {
    Gfx *gfx = BeginList(f_DisplayList);

    ResetCapture();
    gfx = DrawFilteredRectangle(gfx, G_CYC_1CYCLE, G_TF_POINT);
    gfx = DrawFilteredRectangle(gfx, G_CYC_1CYCLE, G_TF_BILERP);
    gfx = DrawFilteredRectangle(gfx, G_CYC_1CYCLE, G_TF_POINT);
    gfx = DrawFilteredRectangle(gfx, G_CYC_1CYCLE, G_TF_AVERAGE);
    gfx = DrawFilteredRectangle(gfx, G_CYC_2CYCLE, G_TF_POINT);
    gfx = DrawFilteredRectangle(gfx, G_CYC_2CYCLE, G_TF_BILERP);
    // The palette's mode, beside the filter's, leaves it alone
    gDPSetTextureLUT(gfx++, G_TT_IA16);
    gfx = DrawFilteredRectangle(gfx, G_CYC_1CYCLE, G_TF_POINT);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    RunList(gfx);

    // Each differs from the last, so none is merged with it
    EXPECT(f_Gpu.materialCount == 7);
    EXPECT(!IsMaterialFiltered(0));
    EXPECT(IsMaterialFiltered(1));
    EXPECT(!IsMaterialFiltered(2));
    EXPECT(IsMaterialFiltered(3));
    EXPECT(!IsMaterialFiltered(4));
    EXPECT(IsMaterialFiltered(5));
    EXPECT(!IsMaterialFiltered(6));
}

// The copy cycle copies texels as they are, whatever the filter, as the
// RDP's copy pipeline has none: filtered, a copied picture's cut-out edges
// mixed in its transparent texels' colors once drawn larger
static void TestCopyCycleNeverFilters(void) {
    Gfx *gfx = BeginList(f_DisplayList);

    ResetCapture();
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gfx = DrawFilteredRectangle(gfx, G_CYC_COPY, G_TF_BILERP);
    gfx = DrawFilteredRectangle(gfx, G_CYC_COPY, G_TF_AVERAGE);
    RunList(gfx);

    EXPECT(f_Gpu.materialCount == 1);
    EXPECT(!IsMaterialFiltered(0));
}

// Draws the triangle under a render mode whose blender's first cycle takes
// first times its factor plus second times its own; the second cycle, which
// one-cycle mode leaves out, passes the color
static Gfx *DrawBlendedTriangle(Gfx *gfx, u32 flags, u32 first, u32 a, u32 second, u32 b) {
    gDPPipeSync(gfx++);
    gDPSetRenderMode(
        gfx++,
        flags | GBL_c1(first, a, second, b),
        GBL_c2(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)
    );
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    return gfx;
}

static uint32_t BlendShapeOf(int materialIndex) {
    return (f_Gpu.materials[materialIndex].modes & MATERIAL_BLEND_MASK) >> MATERIAL_BLEND_SHIFT;
}

static uint16_t BlendingKey(DestinationFactor destination) {
    return (uint16_t)(PIPELINE_BLEND | (destination << PIPELINE_DESTINATION_SHIFT));
}

// Forced blending shapes the GPU's blending by where memory's color enters
// the blender's last cycle, and memory's factor; without it, the first
// color passes. One-cycle mode blends with the first cycle's settings.
static void TestBlendShapes(void) {
    Gfx *gfx = BeginTriangles(f_DisplayList, G_SHADE | G_SHADING_SMOOTH);

    ResetCapture();
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gfx = DrawBlendedTriangle(gfx, 0, G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM);
    gfx = DrawBlendedTriangle(gfx, FORCE_BL, G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA);
    gfx = DrawBlendedTriangle(gfx, FORCE_BL, G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_IN, G_BL_1MA);
    gfx = DrawBlendedTriangle(gfx, FORCE_BL, G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM);
    gfx = DrawBlendedTriangle(gfx, FORCE_BL, G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_FOG, G_BL_1MA);
    gfx = DrawBlendedTriangle(gfx, FORCE_BL, G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM);
    gfx = DrawBlendedTriangle(gfx, FORCE_BL, G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_0);
    // Two cycles blend with memory in the second alone
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPSetRenderMode(
        gfx++,
        GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1),
        FORCE_BL | GBL_c2(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)
    );
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(
        gfx++,
        GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1),
        FORCE_BL | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)
    );
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);

    EXPECT(f_Gpu.materialCount == 9 && f_Gpu.drawCount == 9);
    EXPECT(BlendShapeOf(0) == BLEND_FIRST_ONLY);
    EXPECT(f_Gpu.draws[0].pipelineKey == 0);
    EXPECT(BlendShapeOf(1) == BLEND_FIRST_OVER_MEMORY);
    EXPECT(f_Gpu.draws[1].pipelineKey == BlendingKey(DESTINATION_ONE_MINUS_SOURCE_ALPHA));
    EXPECT(BlendShapeOf(2) == BLEND_SECOND_OVER_MEMORY);
    EXPECT(f_Gpu.draws[2].pipelineKey == BlendingKey(DESTINATION_SOURCE_ALPHA));
    EXPECT(BlendShapeOf(3) == BLEND_KEEP_MEMORY);
    EXPECT(f_Gpu.draws[3].pipelineKey == BlendingKey(DESTINATION_ONE));
    EXPECT(BlendShapeOf(4) == BLEND_NO_MEMORY);
    EXPECT(f_Gpu.draws[4].pipelineKey == 0);
    EXPECT(BlendShapeOf(5) == BLEND_FIRST_OVER_MEMORY);
    EXPECT(f_Gpu.draws[5].pipelineKey == BlendingKey(DESTINATION_DESTINATION_ALPHA));
    EXPECT(BlendShapeOf(6) == BLEND_FIRST_OVER_MEMORY);
    EXPECT(f_Gpu.draws[6].pipelineKey == BlendingKey(DESTINATION_ZERO));
    EXPECT(BlendShapeOf(7) == BLEND_KEEP_MEMORY);
    EXPECT(f_Gpu.draws[7].pipelineKey == BlendingKey(DESTINATION_ONE));
    EXPECT(BlendShapeOf(8) == BLEND_FIRST_OVER_MEMORY);
    EXPECT(f_Gpu.draws[8].pipelineKey == BlendingKey(DESTINATION_ONE_MINUS_SOURCE_ALPHA));
}

// Without forced blending the blender's last cycle gives its first color
// as it is, which may be memory's: such a draw writes depth alone
static void TestUnforcedBlenderPassesFirstColor(void) {
    Gfx *gfx = f_DisplayList;

    ResetCapture();
    gDPSetDepthImage(gfx++, f_DepthImage);
    gfx = BeginTriangles(gfx, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gfx =
        DrawBlendedTriangle(gfx, Z_CMP | Z_UPD, G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM);
    gfx = DrawBlendedTriangle(gfx, 0, G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPSetRenderMode(
        gfx++,
        GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1),
        GBL_c2(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)
    );
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(
        gfx++,
        GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
        GBL_c2(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)
    );
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);

    EXPECT(f_Gpu.materialCount == 4 && f_Gpu.drawCount == 4);
    EXPECT(BlendShapeOf(0) == BLEND_KEEP_MEMORY);
    EXPECT(
        f_Gpu.draws[0].pipelineKey ==
        (PIPELINE_DEPTH_TEST | PIPELINE_DEPTH_WRITE | BlendingKey(DESTINATION_ONE))
    );
    EXPECT(BlendShapeOf(1) == BLEND_FIRST_ONLY);
    EXPECT(f_Gpu.draws[1].pipelineKey == 0);
    // In two cycles, the second cycle's first color passes
    EXPECT(BlendShapeOf(2) == BLEND_KEEP_MEMORY);
    EXPECT(f_Gpu.draws[2].pipelineKey == BlendingKey(DESTINATION_ONE));
    EXPECT(BlendShapeOf(3) == BLEND_FIRST_ONLY);
    EXPECT(f_Gpu.draws[3].pipelineKey == 0);
}

// A fill of the depth image clears it for the next pass that tests depth,
// rather than drawing
static void TestDepthClear(void) {
    Gfx *gfx = f_DisplayList;

    ResetCapture();
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT);
    gDPSetDepthImage(gfx++, f_DepthImage);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_DepthImage);
    gDPSetFillColor(gfx++, (GPACK_ZDZ(G_MAXFBZ, 0) << 16) | GPACK_ZDZ(G_MAXFBZ, 0));
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    gfx = BeginTriangles(gfx, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);

    EXPECT(f_Gpu.passCount == 1);
    EXPECT(f_Gpu.passes[0].depth != NULL && f_Gpu.passes[0].depth->isDepth);
    EXPECT(f_Gpu.passes[0].clearsDepth);
    EXPECT_NEAR(f_Gpu.passes[0].depthClearValue, 1.0, 1e-6);
    EXPECT(f_Gpu.draws[0].pipelineKey == (PIPELINE_DEPTH_TEST | PIPELINE_DEPTH_WRITE));
}

// The primitive's depth, and as the GPU's depth. The RSP gives the RDP a
// vertex's depth in 32 units for each of the viewport's, so the farthest
// that a vertex reaches, the viewport's G_MAXZ, is G_MAXZ * 32 to the RDP.
#define PRIMITIVE_DEPTH 0x2000
#define PRIMITIVE_DEPTH_SHARE (8192.0 / (G_MAXZ * 32.0))

// Adds to a list the depth-buffered opaque mode, with the depth image set,
// depth taken from the source, and the primitive's depth PRIMITIVE_DEPTH
// with the 16th bit set, which the RDP ignores
static Gfx *SourceDepth(Gfx *gfx, u32 source) {
    gDPSetDepthImage(gfx++, f_DepthImage);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gDPSetDepthSource(gfx++, source);
    gDPSetPrimDepth(gfx++, 0x8000 | PRIMITIVE_DEPTH, 0);
    return gfx;
}

// Draws f_Triangle under the geometry mode, with depth from the source
static void DrawDepthSourcedTriangle(u32 geometryMode, u32 source) {
    Gfx *gfx = BeginTriangles(f_DisplayList, geometryMode | G_SHADE | G_SHADING_SMOOTH);

    ResetCapture();
    gfx = SourceDepth(gfx, source);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);
}

// Draws a texture rectangle with depth from the source
static void DrawDepthSourcedRectangle(u32 source) {
    Gfx *gfx = BeginList(f_DisplayList);

    ResetCapture();
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gfx = SourceDepth(gfx, source);
    gSPTextureRectangle(gfx++, 0, 0, 8 << 2, 8 << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    RunList(gfx);
}

// Expects each of the first count vertices captured at the depth
static void ExpectVertexDepths(uint32_t count, double depth) {
    uint32_t i;

    EXPECT(f_Gpu.vertexCount == count);
    for (i = 0; i < count && i < f_Gpu.vertexCount; i++) {
        const GfxVertex *vertex = &f_Gpu.vertices[i];

        EXPECT_NEAR(vertex->position[2] / vertex->position[3], depth, 1e-6);
    }
}

// A triangle with depth takes the primitive's depth where the depth source
// says so, and its own otherwise. A rectangle, which has none of its own,
// tests and writes the primitive's, and otherwise draws without depth, as
// does a triangle that the RSP sends without depth. (The RDP would test
// and write a depth of zero for those two.)
static void TestDepthSources(void) {
    const uint16_t depthKey = PIPELINE_DEPTH_TEST | PIPELINE_DEPTH_WRITE;

    DrawDepthSourcedTriangle(G_ZBUFFER, G_ZS_PRIM);
    EXPECT(f_Gpu.draws[0].pipelineKey == depthKey);
    ExpectVertexDepths(3, PRIMITIVE_DEPTH_SHARE);
    DrawDepthSourcedTriangle(G_ZBUFFER, G_ZS_PIXEL);
    EXPECT(f_Gpu.draws[0].pipelineKey == depthKey);
    // The viewport's scale, half the most, G_MAXZ, rounded down, for z 0
    ExpectVertexDepths(3, (double)(G_MAXZ / 2) / G_MAXZ);
    DrawDepthSourcedRectangle(G_ZS_PRIM);
    EXPECT(f_Gpu.draws[0].pipelineKey == depthKey);
    ExpectVertexDepths(6, PRIMITIVE_DEPTH_SHARE);
    DrawDepthSourcedRectangle(G_ZS_PIXEL);
    EXPECT(f_Gpu.draws[0].pipelineKey == 0);
    EXPECT(f_Gpu.vertexCount == 6);
    DrawDepthSourcedTriangle(0, G_ZS_PIXEL);
    EXPECT(f_Gpu.draws[0].pipelineKey == 0);
    EXPECT(f_Gpu.vertexCount == 3);
}

// The RDP puts the primitive's depth in place of a triangle's own, so a
// triangle that the RSP sends without depth, as the game sends the letters
// it draws about the ball, still tests and writes the primitive's
static void TestTrianglesWithoutDepthTakePrimitives(void) {
    DrawDepthSourcedTriangle(0, G_ZS_PRIM);
    EXPECT(f_Gpu.draws[0].pipelineKey == (PIPELINE_DEPTH_TEST | PIPELINE_DEPTH_WRITE));
    ExpectVertexDepths(3, PRIMITIVE_DEPTH_SHARE);
}

// The game works out a sprite's primitive depth on the scale on which the
// RSP passes a vertex's depth to the RDP, so a primitive at a vertex's
// depth lies at the vertex's depth on the GPU as well, and the small steps
// by which the game puts a sprite before the ground keep their order
static void TestPrimitiveDepthMatchesVertexDepth(void) {
    double vertexDepth;
    Gfx *gfx;

    DrawDepthSourcedTriangle(G_ZBUFFER, G_ZS_PIXEL);
    vertexDepth = f_Gpu.vertices[0].position[2] / f_Gpu.vertices[0].position[3];

    gfx = BeginTriangles(f_DisplayList, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    ResetCapture();
    gfx = SourceDepth(gfx, G_ZS_PRIM);
    // f_Triangle's z of 0 lies at the viewport's G_MAXZ / 2
    gDPSetPrimDepth(gfx++, (G_MAXZ / 2) * 32, 0);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);
    ExpectVertexDepths(3, vertexDepth);
}

// Fills the other image in the fill cycle, as a color image, from left,
// top to right, bottom in pixels, within a scissor scissorRight wide, with
// the depth image elsewhere; then draws a triangle that tests depth against
// the other image's memory
static void FillThenDrawOnDepth(
    uint32_t depth,
    int scissorRight,
    int left,
    int top,
    int right,
    int bottom
) {
    Gfx *gfx = f_DisplayList;

    ResetCapture();
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, scissorRight, IMAGE_HEIGHT);
    gDPSetDepthImage(gfx++, f_DepthImage);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_OtherImage);
    gDPSetFillColor(gfx++, (GPACK_ZDZ(depth, 0) << 16) | GPACK_ZDZ(depth, 0));
    gDPFillRectangle(gfx++, left, top, right, bottom);
    gDPPipeSync(gfx++);
    gDPSetDepthImage(gfx++, f_OtherImage);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_ColorImage);
    gfx = BeginTriangles(gfx, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);
}

// The pass that draws the triangle after FillThenDrawOnDepth
static const GfxPass *DepthPass(void) {
    EXPECT(f_Gpu.passCount == 2);
    EXPECT(f_Gpu.passes[1].depth != NULL && f_Gpu.passes[1].depth->isDepth);
    return &f_Gpu.passes[1];
}

// The game clears memory that it uses as a depth image only later, as the
// saved frame's depth, by filling it as a color image: a fill of the whole
// image, in the fill cycle, both draws and clears that memory's depth for
// its next use as the depth image. A fill short of any edge leaves
// depth as it was.
static void TestColorFillClearsLaterDepth(void) {
    const GfxPass *pass;

    FillThenDrawOnDepth(G_MAXFBZ, IMAGE_WIDTH, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    EXPECT(f_Gpu.passes[0].depth == NULL);
    EXPECT(f_Gpu.draws[0].pipelineKey == PIPELINE_WRITE_ALPHA);
    pass = DepthPass();
    EXPECT(pass->clearsDepth);
    EXPECT_NEAR(pass->depthClearValue, 1.0, 1e-6);
    EXPECT(f_Gpu.draws[1].pipelineKey == (PIPELINE_DEPTH_TEST | PIPELINE_DEPTH_WRITE));

    FillThenDrawOnDepth(0x1000, IMAGE_WIDTH, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    pass = DepthPass();
    EXPECT(pass->clearsDepth);
    EXPECT_NEAR(pass->depthClearValue, 4096.0 / 16383.0, 1e-6);

    FillThenDrawOnDepth(G_MAXFBZ, IMAGE_WIDTH, 1, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    EXPECT(!DepthPass()->clearsDepth);
    FillThenDrawOnDepth(G_MAXFBZ, IMAGE_WIDTH, 0, 1, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    EXPECT(!DepthPass()->clearsDepth);
    FillThenDrawOnDepth(G_MAXFBZ, IMAGE_WIDTH, 0, 0, IMAGE_WIDTH - 2, IMAGE_HEIGHT - 1);
    EXPECT(!DepthPass()->clearsDepth);
    FillThenDrawOnDepth(G_MAXFBZ, IMAGE_WIDTH, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 2);
    EXPECT(!DepthPass()->clearsDepth);

    // A scissor wider than the image reaches only the image's edge
    FillThenDrawOnDepth(G_MAXFBZ, IMAGE_WIDTH + 8, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    EXPECT(DepthPass()->clearsDepth);
}

// Fills the depth image, then noteCount other images whole as color
// images, then draws a triangle that tests depth: whether the depth
// image's clear is still pending
static bool DoesDepthClearOutlastFills(int noteCount) {
    static uint16_t images[4][IMAGE_WIDTH * IMAGE_HEIGHT];
    const GfxPass *pass;
    Gfx *gfx = f_DisplayList;
    int i;

    SDL_assert(noteCount <= (int)SDL_arraysize(images));
    ResetCapture();
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT);
    gDPSetDepthImage(gfx++, f_DepthImage);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_DepthImage);
    gDPSetFillColor(gfx++, (GPACK_ZDZ(0x2000, 0) << 16) | GPACK_ZDZ(0x2000, 0));
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    for (i = 0; i < noteCount; i++) {
        gDPPipeSync(gfx++);
        gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, images[i]);
        gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    }

    gDPPipeSync(gfx++);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_ColorImage);
    gfx = BeginTriangles(gfx, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);

    pass = &f_Gpu.passes[f_Gpu.passCount - 1];
    EXPECT(pass->depth != NULL);
    return pass->clearsDepth;
}

// The RDP keeps four pending clears: three fills of other memory leave the
// depth image's waiting, and a fourth drops it, the oldest
static void TestDepthClearNotesHoldFour(void) {
    EXPECT(!DoesDepthClearOutlastFills(4));
    EXPECT(DoesDepthClearOutlastFills(3));
}

// A fill of the color image between the depth image's clear and its use
// leaves that clear pending
static void TestDepthClearOutlastsColorFill(void) {
    Gfx *gfx = f_DisplayList;

    ResetCapture();
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT);
    gDPSetDepthImage(gfx++, f_DepthImage);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_DepthImage);
    gDPSetFillColor(gfx++, (GPACK_ZDZ(0x2000, 0) << 16) | GPACK_ZDZ(0x2000, 0));
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    gDPPipeSync(gfx++);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_ColorImage);
    gDPSetFillColor(gfx++, 0);
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    gfx = BeginTriangles(gfx, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);

    // A pass clears only before its first draw, so the triangle has its own
    EXPECT(f_Gpu.passCount == 2);
    EXPECT(f_Gpu.passes[0].depth == NULL);
    EXPECT(f_Gpu.passes[1].depth != NULL && f_Gpu.passes[1].clearsDepth);
    EXPECT_NEAR(f_Gpu.passes[1].depthClearValue, 8192.0 / 16383.0, 1e-6);
}

// Of two fills of the depth image before its use, the later clears it
static void TestLaterDepthFillWins(void) {
    Gfx *gfx = f_DisplayList;

    ResetCapture();
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT);
    gDPSetDepthImage(gfx++, f_DepthImage);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_DepthImage);
    gDPSetFillColor(gfx++, (GPACK_ZDZ(0x1000, 0) << 16) | GPACK_ZDZ(0x1000, 0));
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    gDPPipeSync(gfx++);
    gDPSetFillColor(gfx++, (GPACK_ZDZ(0x3000, 0) << 16) | GPACK_ZDZ(0x3000, 0));
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    gDPPipeSync(gfx++);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_ColorImage);
    gfx = BeginTriangles(gfx, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);

    // Taking the clear forgets it, so the second triangle shares the pass
    EXPECT(f_Gpu.passCount == 1);
    EXPECT(f_Gpu.passes[0].clearsDepth);
    EXPECT_NEAR(f_Gpu.passes[0].depthClearValue, 12288.0 / 16383.0, 1e-6);
}

// The key of a draw that fills part of the depth image
#define DEPTH_FILL_KEY (PIPELINE_DEPTH_WRITE | PIPELINE_DEPTH_ONLY)

// Starts a list that fills the depth image within a scissor right pixels
// wide
static Gfx *BeginDepthFills(Gfx *gfx, int right) {
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, right, IMAGE_HEIGHT);
    gDPSetDepthImage(gfx++, f_DepthImage);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_DepthImage);
    return gfx;
}

// Fills the depth image with depth from left, top to right, bottom in
// pixels, both corners drawn
static Gfx *FillDepth(Gfx *gfx, uint32_t depth, int left, int top, int right, int bottom) {
    gDPSetFillColor(gfx++, (GPACK_ZDZ(depth, 0) << 16) | GPACK_ZDZ(depth, 0));
    gDPFillRectangle(gfx++, left, top, right, bottom);
    gDPPipeSync(gfx++);
    return gfx;
}

// Draws a triangle into the color image that tests depth, and runs the list
static void DrawOnDepth(Gfx *gfx) {
    gfx = BeginTriangles(gfx, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);
}

// Checks that the six vertices from first cover left, top to right, bottom
// in pixels, exclusive, at depth
static void ExpectDepthFillAt(
    uint32_t first,
    int left,
    int top,
    int right,
    int bottom,
    float depth
) {
    float minimum[2] = {1e9f, 1e9f};
    float maximum[2] = {-1e9f, -1e9f};
    uint32_t i;

    EXPECT(first + 6 <= f_Gpu.vertexCount);
    for (i = first; i < first + 6 && i < f_Gpu.vertexCount; i++) {
        const GfxVertex *vertex = &f_Gpu.vertices[i];
        // Back from clip space to the image's pixels
        float x = (vertex->position[0] + 1.0f) * (IMAGE_WIDTH / 2);
        float y = (1.0f - vertex->position[1]) * (IMAGE_HEIGHT / 2);

        minimum[0] = SDL_min(minimum[0], x);
        minimum[1] = SDL_min(minimum[1], y);
        maximum[0] = SDL_max(maximum[0], x);
        maximum[1] = SDL_max(maximum[1], y);
        EXPECT_NEAR(vertex->position[2], depth, 1e-6);
        EXPECT_NEAR(vertex->position[3], 1.0, 1e-6);
    }

    EXPECT_NEAR(minimum[0], left, 1e-4);
    EXPECT_NEAR(minimum[1], top, 1e-4);
    EXPECT_NEAR(maximum[0], right, 1e-4);
    EXPECT_NEAR(maximum[1], bottom, 1e-4);
}

// A fill of part of the depth image waits for the image's next use, then
// draws its depth there, after any clear and before the draw that uses it.
// It writes no color, and isn't held to the scissor of that draw.
static void TestPartialDepthFillDraws(void) {
    Gfx *gfx = BeginDepthFills(f_DisplayList, IMAGE_WIDTH);
    const GfxRect wholeImage = {0, 0, IMAGE_WIDTH, IMAGE_HEIGHT};

    ResetCapture();
    gfx = FillDepth(gfx, G_MAXFBZ, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    gfx = FillDepth(gfx, 0, 8, 4, 23, 11);
    gfx = BeginTriangles(gfx, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, 4, 4);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);

    // Drawing the fill forgets it, so the second triangle shares the first's
    // draw
    EXPECT(f_Gpu.passCount == 1);
    EXPECT(f_Gpu.passes[0].clearsDepth);
    EXPECT_NEAR(f_Gpu.passes[0].depthClearValue, 1.0, 1e-6);
    EXPECT(f_Gpu.drawCount == 2);
    EXPECT(f_Gpu.draws[1].vertexCount == 6);
    EXPECT(f_Gpu.draws[0].pipelineKey == DEPTH_FILL_KEY);
    EXPECT(f_Gpu.draws[0].vertexCount == 6);
    EXPECT(SDL_memcmp(&f_Gpu.draws[0].scissor, &wholeImage, sizeof(wholeImage)) == 0);
    ExpectDepthFillAt(f_Gpu.draws[0].firstVertex, 8, 4, 24, 12, 0.0f);
    EXPECT(f_Gpu.draws[1].pipelineKey == (PIPELINE_DEPTH_TEST | PIPELINE_DEPTH_WRITE));
}

// Fills the depth image from left, top to right, bottom within a scissor
// scissorRight wide, then draws on it: whether the fill cleared rather
// than drew
static bool DoesDepthFillClear(int scissorRight, int left, int top, int right, int bottom) {
    Gfx *gfx = BeginDepthFills(f_DisplayList, scissorRight);
    bool isDrawn;

    ResetCapture();
    gfx = FillDepth(gfx, 0x2000, left, top, right, bottom);
    DrawOnDepth(gfx);
    isDrawn = f_Gpu.drawCount != 0 && f_Gpu.draws[0].pipelineKey == DEPTH_FILL_KEY;
    EXPECT(f_Gpu.passCount == 1);
    EXPECT(f_Gpu.passes[0].clearsDepth != isDrawn);
    return f_Gpu.passes[0].clearsDepth;
}

// A fill clears the depth image when it covers what the scissor lets it
// reach, and draws when it falls short of any edge by a pixel
static void TestDepthFillClearsOnlyWhole(void) {
    EXPECT(DoesDepthFillClear(IMAGE_WIDTH, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1));
    EXPECT(!DoesDepthFillClear(IMAGE_WIDTH, 1, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1));
    EXPECT(!DoesDepthFillClear(IMAGE_WIDTH, 0, 1, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1));
    EXPECT(!DoesDepthFillClear(IMAGE_WIDTH, 0, 0, IMAGE_WIDTH - 2, IMAGE_HEIGHT - 1));
    EXPECT(!DoesDepthFillClear(IMAGE_WIDTH, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 2));
    // The scissor, not the image, bounds what a fill must cover
    EXPECT(DoesDepthFillClear(16, 0, 0, 15, IMAGE_HEIGHT - 1));
    EXPECT(DoesDepthFillClear(IMAGE_WIDTH + 8, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1));
}

// A fill draws only what its scissor lets it reach, on every side
static void TestDepthFillKeepsItsScissor(void) {
    Gfx *gfx = BeginDepthFills(f_DisplayList, 16);

    ResetCapture();
    gfx = FillDepth(gfx, 0, 8, 4, 23, 11);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 8, 4, 16, IMAGE_HEIGHT);
    gfx = FillDepth(gfx, 0, 0, 0, 23, 11);
    DrawOnDepth(gfx);

    EXPECT(f_Gpu.draws[0].pipelineKey == DEPTH_FILL_KEY);
    EXPECT(f_Gpu.draws[0].vertexCount == 12);
    ExpectDepthFillAt(0, 8, 4, 16, 12, 0.0f);
    ExpectDepthFillAt(6, 8, 4, 16, 12, 0.0f);
}

// Fills reach the depth image in their order, and a clear drops those
// before it
static void TestDepthFillsKeepTheirOrder(void) {
    Gfx *gfx = BeginDepthFills(f_DisplayList, IMAGE_WIDTH);

    ResetCapture();
    gfx = FillDepth(gfx, 0, 0, 0, 31, 7);
    gfx = FillDepth(gfx, 0x2000, IMAGE_WIDTH - 1, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    gfx = FillDepth(gfx, G_MAXFBZ, 16, 0, 47, 7);
    DrawOnDepth(gfx);

    EXPECT(!f_Gpu.passes[0].clearsDepth);
    EXPECT(f_Gpu.draws[0].pipelineKey == DEPTH_FILL_KEY);
    EXPECT(f_Gpu.draws[0].vertexCount == 18);
    ExpectDepthFillAt(0, 0, 0, 32, 8, 0.0f);
    ExpectDepthFillAt(6, IMAGE_WIDTH - 1, 0, IMAGE_WIDTH, IMAGE_HEIGHT, 8192.0f / 16383.0f);
    ExpectDepthFillAt(12, 16, 0, 48, 8, 1.0f);

    gfx = BeginDepthFills(f_DisplayList, IMAGE_WIDTH);
    ResetCapture();
    gfx = FillDepth(gfx, 0, 0, 0, 31, 7);
    gfx = FillDepth(gfx, 0x2000, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    DrawOnDepth(gfx);

    EXPECT(f_Gpu.passes[0].clearsDepth);
    EXPECT_NEAR(f_Gpu.passes[0].depthClearValue, 8192.0 / 16383.0, 1e-6);
    EXPECT(f_Gpu.drawCount == 1);
    EXPECT(f_Gpu.draws[0].pipelineKey == (PIPELINE_DEPTH_TEST | PIPELINE_DEPTH_WRITE));
}

// A fill waits for its own depth image, past draws that use another
static void TestDepthFillWaitsForItsImage(void) {
    Gfx *gfx = BeginDepthFills(f_DisplayList, IMAGE_WIDTH);

    ResetCapture();
    gfx = FillDepth(gfx, 0, 8, 4, 23, 11);
    gDPSetDepthImage(gfx++, f_OtherImage);
    gfx = BeginTriangles(gfx, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    gDPPipeSync(gfx++);
    gDPSetDepthImage(gfx++, f_DepthImage);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);

    EXPECT(f_Gpu.drawCount == 3);
    EXPECT(f_Gpu.draws[0].pipelineKey == (PIPELINE_DEPTH_TEST | PIPELINE_DEPTH_WRITE));
    EXPECT(f_Gpu.draws[1].pipelineKey == DEPTH_FILL_KEY);
    EXPECT(f_Gpu.draws[2].pipelineKey == (PIPELINE_DEPTH_TEST | PIPELINE_DEPTH_WRITE));
    EXPECT(f_Gpu.passCount == 2);
    EXPECT(f_Gpu.passes[0].depth != f_Gpu.passes[1].depth);
}

// Fills count fills of the depth image, each a pixel wide in its own
// column, then draws on it: the column of the first fill drawn, or -1
static int FindFirstDepthFillDrawn(int count) {
    Gfx *gfx = BeginDepthFills(f_DisplayList, IMAGE_WIDTH);
    int i;

    ResetCapture();
    for (i = 0; i < count; i++) {
        gfx = FillDepth(gfx, 0, i, 0, i, IMAGE_HEIGHT - 1);
    }

    DrawOnDepth(gfx);
    if (f_Gpu.draws[0].pipelineKey != DEPTH_FILL_KEY) {
        return -1;
    }

    EXPECT(f_Gpu.draws[0].vertexCount == 6 * (uint32_t)SDL_min(count, 4));
    return (int)SDL_lroundf((f_Gpu.vertices[0].position[0] + 1.0f) * (IMAGE_WIDTH / 2));
}

// The RDP keeps four fills waiting; a fifth drops the oldest
static void TestDepthFillsHoldFour(void) {
    EXPECT(FindFirstDepthFillDrawn(1) == 0);
    EXPECT(FindFirstDepthFillDrawn(4) == 0);
    EXPECT(FindFirstDepthFillDrawn(5) == 1);
}

// A list from ROM holds the N64's words, big-endian, and reaches its
// vertices through a segment
// The ROM's data holds the N64's addresses, which lead into RDRAM as the
// microcode leads them, through the segment in bits 24 to 27
static void TestN64Addresses(void) {
    static uint8_t hostData[16];
    RcpMemory memory;

    SDL_zero(memory);
    memory.segments[6] = 0x100000;
    memory.segments[3] = (uintptr_t)hostData;
    EXPECT(ResolveRcpAddress(&memory, 0x80001000) == &osPcRdram[0x1000]);
    EXPECT(ResolveRcpAddress(&memory, 0xA0001000) == &osPcRdram[0x1000]);
    EXPECT(ResolveRcpAddress(&memory, 0x06000010) == &osPcRdram[0x100010]);
    EXPECT(ResolveRcpAddress(&memory, 0x03000004) == &hostData[4]);
    // Bits above the segment's are dropped, and the sum wraps at 16 MB
    EXPECT(ResolveRcpAddress(&memory, 0x81189320) == &osPcRdram[0x189320]);
    memory.segments[1] = 0x200000;
    EXPECT(ResolveRcpAddress(&memory, 0x81000010) == &osPcRdram[0x200010]);
    memory.segments[0xE] = 0xFFFFF0;
    EXPECT(ResolveRcpAddress(&memory, 0x0E000020) == &osPcRdram[0x10]);
}

static void TestRomOrderList(void) {
    static uint8_t romData[256];
    uint32_t *words = (uint32_t *)romData;
    uint8_t *vertices = romData + 128;
    Gfx *gfx;
    int i;

    for (i = 0; i < 3; i++) {
        const Vtx_t *source = &f_Triangle[i].v;
        uint16_t *halves = (uint16_t *)(vertices + (i * 16));

        halves[0] = SDL_Swap16BE((uint16_t)source->ob[0]);
        halves[1] = SDL_Swap16BE((uint16_t)source->ob[1]);
        halves[2] = SDL_Swap16BE((uint16_t)source->ob[2]);
        halves[3] = 0;
        halves[4] = SDL_Swap16BE((uint16_t)source->tc[0]);
        halves[5] = SDL_Swap16BE((uint16_t)source->tc[1]);
        SDL_memcpy(vertices + (i * 16) + 12, source->cn, 4);
    }

    // gsSPVertex of segment 6's three vertices, a triangle, the end
    words[0] = SDL_Swap32BE((G_VTX << 24) | (3 << 12) | (3 << 1));
    words[1] = SDL_Swap32BE(0x06000000 + 128);
    words[2] = SDL_Swap32BE((G_TRI1 << 24) | (0 << 16) | (2 << 8) | 4);
    words[3] = 0;
    words[4] = SDL_Swap32BE((uint32_t)G_ENDDL << 24);
    words[5] = 0;
    osPcMarkRomOrder(romData, sizeof(romData));

    gfx = BeginTriangles(f_DisplayList, G_SHADE | G_SHADING_SMOOTH);
    gSPSegment(gfx++, 6, romData);
    gSPDisplayList(gfx++, osPcSegmentAddress(6, 0));
    ResetCapture();
    RunList(gfx);
    osPcUnmarkRomOrder(romData, sizeof(romData));

    EXPECT(f_Gpu.vertexCount == 3);
    EXPECT_NEAR(f_Gpu.vertices[1].position[0], 1.0, 1e-5);
    EXPECT(f_Gpu.vertices[2].color == 0xFFFF0000);
}

// Puts the big-endian words of a G_DL that pushes ADDRESS at WORDS
static void PutRomCall(uint32_t *words, uint32_t address) {
    words[0] = SDL_Swap32BE((uint32_t)G_DL << 24);
    words[1] = SDL_Swap32BE(address);
}

static void PutRomEnd(uint32_t *words) {
    words[0] = SDL_Swap32BE((uint32_t)G_ENDDL << 24);
    words[1] = 0;
}

// A table of the game's own lists, as the game branches to by an entry's
// offset: two lists that set the primitive color's green, each ended
static Gfx f_GameTable[] = {
    gsDPSetPrimColor(0, 0, 0, 64, 0, 255),
    gsSPEndDisplayList(),
    gsDPSetPrimColor(0, 0, 0, 255, 0, 255),
    gsSPEndDisplayList(),
};

// A table longer than 64 KB in the N64's commands, of ended lists, whose
// last sets green
#define LARGE_TABLE_LENGTH 8194
static Gfx f_LargeTable[LARGE_TABLE_LENGTH];

// Runs LIST through segment 6, with TABLE as segment 9, and returns the
// green of the primitive color that the frame's one triangle has; 0 unless
// a list sets it
static float RunListAndReadGreen(const void *list, uintptr_t table) {
    Gfx *gfx = BeginTriangles(f_DisplayList, G_SHADE | G_SHADING_SMOOTH);

    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 255);
    gSPSegment(gfx++, 6, list);
    gSPSegment(gfx++, 9, table);
    gSPDisplayList(gfx++, osPcSegmentAddress(6, 0));
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    ResetCapture();
    RunList(gfx);
    return f_Gpu.materialCount == 0 ? -1.0f : f_Gpu.materials[0].primitiveColor[1];
}

// A list from ROM counts its way into a list of the game's own in the
// N64's commands, of 8 bytes, as the game's tables of branches expect
static void TestRomListCallsIntoGameList(void) {
    static uint8_t romData[16];
    uint32_t *words = (uint32_t *)romData;
    uintptr_t table = (uintptr_t)f_GameTable;
    int i;

    osPcMarkRomOrder(romData, sizeof(romData));
    // The table's second list, at the N64's 16 bytes in
    PutRomCall(&words[0], 0x09000000 + (2 * 8));
    PutRomEnd(&words[2]);
    EXPECT_NEAR(RunListAndReadGreen(romData, table), 1.0, 1e-6);

    PutRomCall(&words[0], 0x09000000);
    EXPECT_NEAR(RunListAndReadGreen(romData, table), 64.0 / 255.0, 1e-6);

    // The RSP's DMA drops an offset's low 3 bits
    PutRomCall(&words[0], 0x09000000 + (2 * 8) + 4);
    EXPECT_NEAR(RunListAndReadGreen(romData, table), 1.0, 1e-6);

    // The segment's 24 bits of offset count
    for (i = 0; i < LARGE_TABLE_LENGTH; i++) {
        gSPEndDisplayList(&f_LargeTable[i]);
    }
    gDPSetPrimColor(&f_LargeTable[LARGE_TABLE_LENGTH - 2], 0, 0, 0, 32, 0, 255);
    PutRomCall(&words[0], 0x09000000 + ((LARGE_TABLE_LENGTH - 2) * 8));
    EXPECT_NEAR(RunListAndReadGreen(romData, (uintptr_t)f_LargeTable), 32.0 / 255.0, 1e-6);
    osPcUnmarkRomOrder(romData, sizeof(romData));
}

// A list from ROM reaches another from ROM in bytes as they are
static void TestRomListCallsIntoRomList(void) {
    static uint8_t romData[48];
    uint32_t *words = (uint32_t *)romData;

    PutRomCall(&words[0], 0x06000000 + 24);
    PutRomEnd(&words[2]);
    words[6] = SDL_Swap32BE((uint32_t)G_SETPRIMCOLOR << 24);
    words[7] = SDL_Swap32BE(0x0080FFFF);
    PutRomEnd(&words[8]);
    osPcMarkRomOrder(romData, sizeof(romData));
    EXPECT_NEAR(RunListAndReadGreen(romData, (uintptr_t)f_GameTable), 128.0 / 255.0, 1e-6);
    osPcUnmarkRomOrder(romData, sizeof(romData));
}

// A list from ROM in data whose first bytes the host has written, as a
// model's frame data starts with joint matrices that the host computes, is
// still read in bytes as it is
static void TestRomListCallsPastHostWrittenData(void) {
    static uint8_t romData[16];
    static uint8_t frameData[48];
    uint32_t *words = (uint32_t *)romData;
    uint32_t *frameWords = (uint32_t *)frameData;

    PutRomCall(&words[0], 0x09000000 + 32);
    PutRomEnd(&words[2]);
    frameWords[8] = SDL_Swap32BE((uint32_t)G_SETPRIMCOLOR << 24);
    frameWords[9] = SDL_Swap32BE(0x0060FFFF);
    PutRomEnd(&frameWords[10]);
    osPcMarkRomOrder(romData, sizeof(romData));
    osPcMarkRomOrder(frameData + 32, sizeof(frameData) - 32);
    EXPECT_NEAR(RunListAndReadGreen(romData, (uintptr_t)frameData), 96.0 / 255.0, 1e-6);
    osPcUnmarkRomOrder(frameData + 32, sizeof(frameData) - 32);
    osPcUnmarkRomOrder(romData, sizeof(romData));
}

// A list from ROM reaches the host's list in RDRAM, through a segment of an
// N64 address, in bytes as they are
static void TestRomListCallsIntoRdram(void) {
    static uint8_t romData[16];
    uint32_t *words = (uint32_t *)romData;
    Gfx *rdramList = (Gfx *)&osPcRdram[0x100000 + 32];

    PutRomCall(&words[0], 0x09000000 + 32);
    PutRomEnd(&words[2]);
    gDPSetPrimColor(&rdramList[0], 0, 0, 0, 200, 0, 255);
    gSPEndDisplayList(&rdramList[1]);
    osPcMarkRomOrder(romData, sizeof(romData));
    EXPECT_NEAR(RunListAndReadGreen(romData, 0x100000), 200.0 / 255.0, 1e-6);
    osPcUnmarkRomOrder(romData, sizeof(romData));
    SDL_memset(rdramList, 0, 2 * sizeof(Gfx));
}

// A list of the game's own counts its way into another in the host's
// bytes, by a segment's address of either kind
static void TestGameListCallsIntoGameList(void) {
    Gfx caller[2];

    gSPDisplayList(&caller[0], osPcSegmentAddress(9, 2 * sizeof(Gfx)));
    gSPEndDisplayList(&caller[1]);
    EXPECT_NEAR(RunListAndReadGreen(caller, (uintptr_t)f_GameTable), 1.0, 1e-6);

    gSPDisplayList(&caller[0], 0x09000000 + (2 * sizeof(Gfx)));
    EXPECT_NEAR(RunListAndReadGreen(caller, (uintptr_t)f_GameTable), 1.0, 1e-6);
}

// A texture loaded from a color image that the GPU has drawn reads what it
// drew, which comes back to memory first. Flipped, the copy can't be made
// between the targets.
static void TestTextureFromDrawnImage(void) {
    Gfx *gfx = BeginList(f_DisplayList);

    ResetCapture();
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, 0);
    gDPFillRectangle(gfx++, 0, 0, 7, 7);
    gDPPipeSync(gfx++);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_OtherImage);
    gDPSetCycleType(gfx++, G_CYC_COPY);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPLoadTextureTile(
        gfx++,
        f_ColorImage,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        IMAGE_WIDTH,
        IMAGE_HEIGHT,
        0,
        0,
        7,
        7,
        0,
        G_TX_CLAMP,
        G_TX_CLAMP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gSPTextureRectangleFlip(gfx++, 0, 0, 7 << 2, 7 << 2, G_TX_RENDERTILE, 0, 0, 4 << 10, 1 << 10);
    RunList(gfx);

    EXPECT(f_Gpu.readCount == 1);
    // The red that the double read back, as the game's CPU reads it
    EXPECT(f_ColorImage[0] == GPACK_RGBA5551(255, 0, 0, 1));
    EXPECT(f_Gpu.uploadCount == 1);
    // The texture reads the image's memory as the CPU wrote it
    EXPECT(f_Gpu.firstTexel == ExpandRgba16(GPACK_RGBA5551(255, 0, 0, 1)));
    EXPECT(f_Gpu.vertexCount == 6);
    EXPECT(f_Gpu.draws[0].pipelineKey == PIPELINE_WRITE_ALPHA);
    EXPECT(f_Gpu.materials[0].tiles[0].size[0] == 8);
    EXPECT_NEAR(f_Gpu.vertices[5].texCoord[0], 8.0, 1e-4);
    EXPECT_NEAR(f_Gpu.vertices[5].texCoord[1], 8.0, 1e-4);
}

// The identity in Mtx's fixed point: the diagonal's integer halves are one
static void MakeIdentity(Mtx *matrix) {
    int32_t *words = (int32_t *)matrix->m;

    SDL_zerop(matrix);
    words[0] = 0x00010000;
    words[2] = 0x00000001;
    words[5] = 0x00010000;
    words[7] = 0x00000001;
}

// The integer matrix whose rows are given, in Mtx's fixed point
static void MakeIntegerMatrix(Mtx *matrix, const int16_t rows[4][4]) {
    int32_t *words = (int32_t *)matrix->m;
    int element;

    SDL_zerop(matrix);
    for (element = 0; element < 16; element += 2) {
        uint16_t high = (uint16_t)rows[element / 4][element % 4];
        uint16_t low = (uint16_t)rows[element / 4][(element % 4) + 1];

        words[element / 2] = (int32_t)(((uint32_t)high << 16) | low);
    }
}

// The GPU's drawing shows until the CPU writes the image's memory after it
// last agreed with the target
static void TestShownTarget(void) {
    Gfx *gfx = BeginList(f_DisplayList);
    uint16_t width = 0;
    uint16_t height = 0;
    GfxTarget *target;

    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, 0);
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    RunList(gfx);

    target = LockShownTarget((const uint8_t *)f_ColorImage, &width, &height);
    UnlockShownTarget();
    EXPECT(target != NULL && width == IMAGE_WIDTH && height == IMAGE_HEIGHT);

    SyncColorImagesToMemory((const uint8_t *)f_ColorImage, sizeof(f_ColorImage));
    f_ColorImage[0] ^= 0xFFFF;
    target = LockShownTarget((const uint8_t *)f_ColorImage, &width, &height);
    UnlockShownTarget();
    EXPECT(target == NULL);
    EXPECT(LockShownTarget((const uint8_t *)f_OtherImage + 2, &width, &height) == NULL);
    UnlockShownTarget();
}

// Memory that the CPU fills after the GPU drew there, as when the game loads
// data into a frame's memory, keeps what the CPU wrote: a sync doesn't bring
// back the older drawing, and the next use of the image reads the memory
static void TestCpuWritesOutliveDrawing(void) {
    Gfx *gfx = BeginList(f_DisplayList);
    int writeCount;
    int i;

    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, 0);
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    RunList(gfx);

    ResetCapture();
    for (i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++) {
        f_ColorImage[i] = (uint16_t)(i * 3);
    }

    SyncRdpMemory(f_ColorImage, sizeof(f_ColorImage));
    EXPECT(f_Gpu.readCount == 0);
    EXPECT(f_ColorImage[0] == 0 && f_ColorImage[100] == 300);

    writeCount = f_Gpu.writeCount;
    gfx = BeginList(f_DisplayList);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPFillRectangle(gfx++, 0, 0, 0, 0);
    RunList(gfx);
    EXPECT(f_Gpu.writeCount == writeCount + 1);
}

// Zeroes f_ColorImage's memory and fills its target on the GPU, which leaves
// the memory behind
static void DrawOverZeroedImage(void) {
    Gfx *gfx = BeginList(f_DisplayList);
    int i;

    for (i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++) {
        f_ColorImage[i] = 0;
    }

    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, 0);
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    RunList(gfx);
    ResetCapture();
}

// The game's CPU reads a frame that the GPU drew, as CaptureHudAnimationFrame
// does, once the native build's call has brought it to memory. A range that
// misses the image leaves it on the GPU; one that holds only its first or
// its last pixel brings the whole image.
static void TestCpuReadsDrawing(void) {
    const int lastPixel = (IMAGE_WIDTH * IMAGE_HEIGHT) - 1;

    SyncRdpMemory(f_ColorImage, sizeof(f_ColorImage));
    SyncRdpMemory(f_OtherImage, sizeof(f_OtherImage));
    DrawOverZeroedImage();
    osPcSyncRdpWrites(f_OtherImage, sizeof(f_OtherImage));
    EXPECT(f_Gpu.readCount == 0);
    osPcSyncRdpWrites(&f_ColorImage[0], sizeof(uint16_t));
    EXPECT(f_Gpu.readCount == 1);
    EXPECT(f_ColorImage[lastPixel] == GPACK_RGBA5551(255, 0, 0, 1));

    DrawOverZeroedImage();
    osPcSyncRdpWrites(&f_ColorImage[lastPixel], sizeof(uint16_t));
    EXPECT(f_Gpu.readCount == 1);
    EXPECT(f_ColorImage[0] == GPACK_RGBA5551(255, 0, 0, 1));
}

#define UNTOUCHED_PIXEL 0x1234

// The height of the target that holds f_SmallImage
static uint16_t FindSmallImageHeight(void) {
    uint16_t width = 0;
    uint16_t height = 0;

    LockShownTarget((const uint8_t *)f_SmallImage, &width, &height);
    UnlockShownTarget();
    return height;
}

// Whether a sync of the whole of f_SmallImage changed exactly its first rows
static bool SyncsOnlyFirstRows(int rowCount) {
    int i;

    SyncColorImagesToMemory((const uint8_t *)f_SmallImage, sizeof(f_SmallImage));
    for (i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++) {
        bool isTouched = f_SmallImage[i] != UNTOUCHED_PIXEL;

        if (isTouched != (i < rowCount * IMAGE_WIDTH)) {
            return false;
        }
    }

    return true;
}

// An image is as tall as its drawing reaches, with the scissor left lower
// as the game leaves it to draw into its small textures: syncing it writes
// no memory beyond. A primitive reaching lower grows it.
static void TestImageSizedByDrawing(void) {
    static const Vtx upperHalf[3] = {
        {{{-1, 0, 0}, 0, {0, 0}, {0xFF, 0, 0, 0xFF}}},
        {{{1, 0, 0}, 0, {0, 0}, {0xFF, 0, 0, 0xFF}}},
        {{{-1, 1, 0}, 0, {0, 0}, {0xFF, 0, 0, 0xFF}}},
    };
    static Mtx doubled;
    int32_t *words = (int32_t *)doubled.m;
    Gfx *gfx = f_DisplayList;
    int i;

    for (i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++) {
        f_SmallImage[i] = UNTOUCHED_PIXEL;
    }

    // Two on the diagonal, in Mtx's fixed point
    SDL_zero(doubled);
    words[0] = 0x00020000;
    words[2] = 0x00000002;
    words[5] = 0x00020000;
    words[7] = 0x00000002;

    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_SmallImage);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, 0);
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, 3);
    RunList(gfx);
    EXPECT(FindSmallImageHeight() == 4);
    EXPECT(SyncsOnlyFirstRows(4));

    // The viewport puts clip space's y of 0 at row 16. The projection
    // doubles every coordinate, w included, which leaves the triangle where
    // it was on the screen.
    gfx = BeginTriangles(f_DisplayList, G_SHADE);
    gSPMatrix(gfx++, &doubled, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_SmallImage);
    gSPVertex(gfx++, upperHalf, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);
    EXPECT(FindSmallImageHeight() == IMAGE_HEIGHT / 2);
    EXPECT(SyncsOnlyFirstRows(IMAGE_HEIGHT / 2));
}

// An image that a lower primitive grows in the middle of a task keeps what
// was drawn above: only memory's new rows go up, into a target made with
// room for the scissor, so the frame's draws needn't reach the GPU first
static void TestImageGrowsInPlace(void) {
    Gfx *gfx = f_DisplayList;
    GfxTarget *target;
    uint16_t width = 0;
    uint16_t height = 0;
    int frameCount;

    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_GrowingImage);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, 0);
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, 3);
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, 15);
    ResetCapture();
    frameCount = f_Gpu.frameCount;
    RunList(gfx);

    EXPECT(f_Gpu.frameCount == frameCount + 1);
    EXPECT(f_Gpu.passCount == 1);
    EXPECT(f_Gpu.lastWrittenRow == 4 && f_Gpu.writtenRowCount == 12);
    target = LockShownTarget((const uint8_t *)f_GrowingImage, &width, &height);
    EXPECT(target != NULL && target->height == IMAGE_HEIGHT && height == 16);
    UnlockShownTarget();
}

// Shutting down releases every target, and drawing after it makes new ones
static void TestShutdownReleasesTargets(void) {
    Gfx *gfx;

    EXPECT(f_LiveTargetCount != 0);
    ShutdownRdp();
    EXPECT(f_LiveTargetCount == 0);

    gfx = BeginList(f_DisplayList);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, 0);
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    RunList(gfx);
    EXPECT(f_LiveTargetCount == 1);
}

// The columns of f_CopiedImage that the copy tests load
#define COPIED_FIRST_COLUMN 8
#define COPIED_COLUMN_COUNT 16

// Draws the first rowCount rows of the image on the GPU in a task of their
// own, then starts a list that copies into f_ColorImage
static Gfx *BeginCopyList(void *image, int rowCount) {
    Gfx *gfx = BeginList(f_DisplayList);

    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, image);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, (GPACK_RGBA5551(0, 255, 0, 1) << 16) | GPACK_RGBA5551(0, 255, 0, 1));
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, rowCount - 1);
    RunList(gfx);

    gfx = BeginList(f_DisplayList);
    gDPSetCycleType(gfx++, G_CYC_COPY);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    return gfx;
}

// Loads rowCount rows of the image from firstRow, of its copied columns, as
// the game loads the saved frame's strips
static Gfx *LoadRows(Gfx *gfx, const void *image, int imageWidth, int firstRow, int rowCount) {
    gDPLoadTextureTile(
        gfx++,
        image,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        imageWidth,
        0,
        COPIED_FIRST_COLUMN,
        firstRow,
        COPIED_FIRST_COLUMN + COPIED_COLUMN_COUNT - 1,
        firstRow + rowCount - 1,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    return gfx;
}

// Copies the loaded rows from firstRow to x, y, a texel to a pixel
static Gfx *CopyRows(Gfx *gfx, int x, int y, int firstRow, int rowCount) {
    gSPTextureRectangle(
        gfx++,
        x << 2,
        y << 2,
        (x + COPIED_COLUMN_COUNT - 1) << 2,
        (y + rowCount - 1) << 2,
        G_TX_RENDERTILE,
        COPIED_FIRST_COLUMN << 5,
        firstRow << 5,
        4 << 10,
        1 << 10
    );
    return gfx;
}

// The target that holds the image, as the presenter would find it
static GfxTarget *FindImageTarget(const void *image) {
    uint16_t width;
    uint16_t height;
    GfxTarget *target = LockShownTarget(image, &width, &height);

    UnlockShownTarget();
    return target;
}

// A copy, a texel to a pixel, of an image's rows that the GPU has drawn
// copies its target's pixels into the color image's, at the renderer's
// scale, rather than drawing them as the CPU would read them; the color
// image's memory is then behind, as after a draw
static void TestCopyBetweenDrawnImages(void) {
    Gfx *gfx = BeginCopyList(f_CopiedImage, IMAGE_HEIGHT);
    int readCount;

    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 2, 10, 4, 6);
    ResetCapture();
    RunList(gfx);

    EXPECT(f_Gpu.vertexCount == 0 && f_Gpu.drawCount == 0);
    EXPECT(f_Gpu.copyCount == 1 && f_Gpu.passCount == 1);
    EXPECT(f_Gpu.passes[0].copyCount == 1 && f_Gpu.passes[0].firstCopy == 0);
    EXPECT(f_Gpu.passes[0].target == FindImageTarget(f_ColorImage));
    EXPECT(f_Gpu.copies[0].source == FindImageTarget(f_CopiedImage));
    EXPECT(f_Gpu.copies[0].sourceX == COPIED_FIRST_COLUMN && f_Gpu.copies[0].sourceY == 4);
    EXPECT(f_Gpu.copies[0].x == 2 && f_Gpu.copies[0].y == 10);
    EXPECT(f_Gpu.copies[0].width == COPIED_COLUMN_COUNT && f_Gpu.copies[0].height == 6);

    readCount = f_Gpu.readCount;
    osPcSyncRdpWrites(f_ColorImage, sizeof(f_ColorImage));
    EXPECT(f_Gpu.readCount == readCount + 1);
}

// The game copies the saved frame a strip at a time, each loaded alone:
// strips that carry on one another become one copy
static void TestCopiedStripsBecomeOne(void) {
    Gfx *gfx = BeginCopyList(f_CopiedImage, IMAGE_HEIGHT);

    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 2, 10, 4, 6);
    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 10, 2);
    gfx = CopyRows(gfx, 2, 16, 10, 2);
    ResetCapture();
    RunList(gfx);

    EXPECT(f_Gpu.copyCount == 1 && f_Gpu.vertexCount == 0);
    EXPECT(f_Gpu.copies[0].sourceY == 4 && f_Gpu.copies[0].height == 8);
}

// The scissor cuts a copy as it cuts a draw, and the source's pixels shift
// with its edges
static void TestCopyKeepsToTheScissor(void) {
    Gfx *gfx = BeginCopyList(f_CopiedImage, IMAGE_HEIGHT);

    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 4, 12, 14, 14);
    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 0, 10, 4, 6);
    ResetCapture();
    RunList(gfx);

    EXPECT(f_Gpu.copyCount == 1);
    EXPECT(f_Gpu.copies[0].x == 4 && f_Gpu.copies[0].sourceX == COPIED_FIRST_COLUMN + 4);
    EXPECT(f_Gpu.copies[0].width == 10);
    EXPECT(f_Gpu.copies[0].y == 12 && f_Gpu.copies[0].sourceY == 6);
    EXPECT(f_Gpu.copies[0].height == 2);
}

// Whether a list that loads rows 4 to 9 of the image and copies them, with
// the change made between, draws them rather than copying targets. The list
// first draws drawnRowCount rows of drawnImage.
static bool DoesChangedCopyDraw(
    const void *image,
    int imageWidth,
    void *drawnImage,
    int drawnRowCount,
    Gfx *(*change)(Gfx *gfx)
) {
    Gfx *gfx = BeginCopyList(drawnImage, drawnRowCount);

    gfx = LoadRows(gfx, image, imageWidth, 4, 6);
    gfx = change(gfx);
    ResetCapture();
    RunList(gfx);
    if (f_Gpu.copyCount == 0 && f_Gpu.vertexCount == 6) {
        return true;
    }

    EXPECT(f_Gpu.copyCount == 1 && f_Gpu.vertexCount == 0);
    return false;
}

static Gfx *CopyAsIs(Gfx *gfx) {
    return CopyRows(gfx, 2, 10, 4, 6);
}

static Gfx *CopyFlipped(Gfx *gfx) {
    gSPTextureRectangleFlip(
        gfx++,
        2 << 2,
        10 << 2,
        17 << 2,
        15 << 2,
        G_TX_RENDERTILE,
        COPIED_FIRST_COLUMN << 5,
        4 << 5,
        4 << 10,
        1 << 10
    );
    return gfx;
}

// Two texels across to a pixel
static Gfx *CopyStretched(Gfx *gfx) {
    gSPTextureRectangle(
        gfx++,
        2 << 2,
        10 << 2,
        9 << 2,
        15 << 2,
        G_TX_RENDERTILE,
        COPIED_FIRST_COLUMN << 5,
        4 << 5,
        8 << 10,
        1 << 10
    );
    return gfx;
}

// Two rows down to a pixel
static Gfx *CopyRowsSkipped(Gfx *gfx) {
    gSPTextureRectangle(
        gfx++,
        2 << 2,
        10 << 2,
        17 << 2,
        12 << 2,
        G_TX_RENDERTILE,
        COPIED_FIRST_COLUMN << 5,
        4 << 5,
        4 << 10,
        2 << 10
    );
    return gfx;
}

// From half a texel in
static Gfx *CopyFromBetweenTexels(Gfx *gfx) {
    gSPTextureRectangle(
        gfx++,
        2 << 2,
        10 << 2,
        16 << 2,
        15 << 2,
        G_TX_RENDERTILE,
        (COPIED_FIRST_COLUMN << 5) | 16,
        4 << 5,
        4 << 10,
        1 << 10
    );
    return gfx;
}

// From half a row down
static Gfx *CopyFromBetweenRows(Gfx *gfx) {
    gSPTextureRectangle(
        gfx++,
        2 << 2,
        10 << 2,
        17 << 2,
        14 << 2,
        G_TX_RENDERTILE,
        COPIED_FIRST_COLUMN << 5,
        (4 << 5) | 16,
        4 << 10,
        1 << 10
    );
    return gfx;
}

// The alpha compare leaves out texels whose alpha is clear
static Gfx *CopyComparingAlpha(Gfx *gfx) {
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gfx = CopyAsIs(gfx);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    return gfx;
}

// The render tile set again as the load left it, but for its format
static Gfx *SetRenderTileFormat(Gfx *gfx, u32 format) {
    gDPSetTile(
        gfx++,
        format,
        G_IM_SIZ_16b,
        (COPIED_COLUMN_COUNT * 2) / 8,
        0,
        G_TX_RENDERTILE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOLOD
    );
    return gfx;
}

static Gfx *CopyRgbaTileSetAgain(Gfx *gfx) {
    return CopyAsIs(SetRenderTileFormat(gfx, G_IM_FMT_RGBA));
}

// Copying moves texels raw, which a draw of other formats doesn't
static Gfx *CopyIntensityAlphaTile(Gfx *gfx) {
    return CopyAsIs(SetRenderTileFormat(gfx, G_IM_FMT_IA));
}

static Gfx *CopyThroughPalette(Gfx *gfx) {
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gfx = CopyAsIs(gfx);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    return gfx;
}

static Gfx *CopyIntoWideImage(Gfx *gfx) {
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_32b, IMAGE_WIDTH / 2, f_ColorImage);
    return CopyAsIs(gfx);
}

static Gfx *CopyInOneCycle(Gfx *gfx) {
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    return CopyAsIs(gfx);
}

// Whether a list that draws f_CopiedImage, then copies rows 4 to 9 of the
// image with the change made, draws them rather than copying targets
static bool DoesCopyDraw(const void *image, int imageWidth, Gfx *(*change)(Gfx *gfx)) {
    return DoesChangedCopyDraw(image, imageWidth, f_CopiedImage, IMAGE_HEIGHT, change);
}

// Copies with nothing that a target holds, or that don't take a texel to a
// pixel as copying between targets does, draw as before
static void TestCopiesThatDraw(void) {
    EXPECT(!DoesCopyDraw(f_CopiedImage, IMAGE_WIDTH, CopyAsIs));
    EXPECT(DoesCopyDraw(f_CopiedImage, IMAGE_WIDTH, CopyFlipped));
    EXPECT(DoesCopyDraw(f_CopiedImage, IMAGE_WIDTH, CopyStretched));
    EXPECT(DoesCopyDraw(f_CopiedImage, IMAGE_WIDTH, CopyRowsSkipped));
    EXPECT(DoesCopyDraw(f_CopiedImage, IMAGE_WIDTH, CopyFromBetweenTexels));
    EXPECT(DoesCopyDraw(f_CopiedImage, IMAGE_WIDTH, CopyFromBetweenRows));
    EXPECT(DoesCopyDraw(f_CopiedImage, IMAGE_WIDTH, CopyComparingAlpha));
    EXPECT(DoesCopyDraw(f_CopiedImage, IMAGE_WIDTH, CopyIntoWideImage));
    EXPECT(DoesCopyDraw(f_CopiedImage, IMAGE_WIDTH, CopyInOneCycle));
    EXPECT(!DoesCopyDraw(f_CopiedImage, IMAGE_WIDTH, CopyRgbaTileSetAgain));
    EXPECT(DoesCopyDraw(f_CopiedImage, IMAGE_WIDTH, CopyIntensityAlphaTile));
    EXPECT(DoesCopyDraw(f_CopiedImage, IMAGE_WIDTH, CopyThroughPalette));
    // Memory that no image holds, rows of another width, and the color
    // image's own pixels
    EXPECT(DoesCopyDraw(f_PlainTexture, IMAGE_WIDTH, CopyAsIs));
    EXPECT(DoesCopyDraw(f_CopiedImage, IMAGE_WIDTH / 2, CopyAsIs));
    EXPECT(DoesCopyDraw(f_ColorImage, IMAGE_WIDTH, CopyAsIs));
    // Texels from between two of the image's pixels
    EXPECT(DoesCopyDraw((const uint8_t *)f_CopiedImage + 1, IMAGE_WIDTH, CopyAsIs));
}

// Whether a copy of 16 columns of f_CopiedImage's rows 4 to 9, from
// firstColumn, draws them rather than copying targets
static bool DoesCopyFromColumnDraw(int firstColumn) {
    Gfx *gfx = BeginCopyList(f_CopiedImage, IMAGE_HEIGHT);

    gDPLoadTextureTile(
        gfx++,
        f_CopiedImage,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        IMAGE_WIDTH,
        0,
        firstColumn,
        4,
        firstColumn + 15,
        9,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gSPTextureRectangle(
        gfx++,
        2 << 2,
        10 << 2,
        17 << 2,
        15 << 2,
        G_TX_RENDERTILE,
        firstColumn << 5,
        4 << 5,
        4 << 10,
        1 << 10
    );
    ResetCapture();
    RunList(gfx);
    return f_Gpu.copyCount == 0 && f_Gpu.vertexCount == 6;
}

// Texels loaded from rows of 16-bit texels that a 32-bit image of their
// width holds, and from past the end of an image's row, draw: no target
// holds them so. Up to the row's end, they copy.
static void TestCopyOfPixelsNoTargetHoldsDraws(void) {
    Gfx *gfx = BeginList(f_DisplayList);

    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_32b, IMAGE_WIDTH / 2, f_WideImage);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPFillRectangle(gfx++, 0, 0, (IMAGE_WIDTH / 2) - 1, IMAGE_HEIGHT - 1);
    RunList(gfx);
    EXPECT(
        DoesChangedCopyDraw(f_WideImage, IMAGE_WIDTH / 2, f_CopiedImage, IMAGE_HEIGHT, CopyAsIs)
    );

    EXPECT(!DoesCopyFromColumnDraw(IMAGE_WIDTH - 16));
    EXPECT(DoesCopyFromColumnDraw(IMAGE_WIDTH - 15));
}

// An image holds the rows that drawing has reached, so a copy of rows below
// them draws memory's: one row short of the copy's last, then up to it
static void TestCopyNeedsDrawnRows(void) {
    EXPECT(DoesChangedCopyDraw(f_ShortImage, IMAGE_WIDTH, f_ShortImage, 9, CopyAsIs));
    EXPECT(!DoesChangedCopyDraw(f_ShortImage, IMAGE_WIDTH, f_ShortImage, 10, CopyAsIs));
}

// Once the CPU writes the source's memory, which the target never saw, a
// later task's copy draws what memory holds
static void TestCopyAfterCpuWritesDraws(void) {
    Gfx *gfx = BeginCopyList(f_CopiedImage, IMAGE_HEIGHT);

    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    RunList(gfx);
    f_CopiedImage[(5 * IMAGE_WIDTH) + COPIED_FIRST_COLUMN] ^= 0x0100;

    gfx = BeginList(f_DisplayList);
    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 2, 10, 4, 6);
    ResetCapture();
    RunList(gfx);
    EXPECT(f_Gpu.copyCount == 0 && f_Gpu.vertexCount == 6);
}

// Saves a frame's depth as the game does, in a task of its own: fills
// f_CopiedImage whole as a color image, then draws a triangle into scene,
// a color image of sceneHeight rows, that writes depth with depth, memory
// within f_CopiedImage, as the depth image. Gives the depth target that
// the triangle wrote.
static GfxTarget *SaveDepthOf(uint16_t *depth, uint16_t *scene, int sceneHeight) {
    Gfx *gfx = BeginList(f_DisplayList);

    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_CopiedImage);
    gDPSetFillColor(gfx++, (GPACK_ZDZ(G_MAXFBZ, 0) << 16) | GPACK_ZDZ(G_MAXFBZ, 0));
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    gDPPipeSync(gfx++);
    gDPSetDepthImage(gfx++, depth);
    gfx = BeginTriangles(gfx, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, scene);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, IMAGE_WIDTH, sceneHeight);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    ResetCapture();
    RunList(gfx);
    EXPECT(f_Gpu.passCount != 0 && f_Gpu.passes[f_Gpu.passCount - 1].depth != NULL);
    return f_Gpu.passes[f_Gpu.passCount - 1].depth;
}

static GfxTarget *SaveDepth(void) {
    return SaveDepthOf(f_CopiedImage, f_ColorImage, IMAGE_HEIGHT);
}

// Loads 16 columns from firstColumn of rowCount rows from firstRow of
// f_CopiedImage, and copies them to x, y
static Gfx *CopyColumns(Gfx *gfx, int firstColumn, int firstRow, int rowCount, int x, int y) {
    gDPLoadTextureTile(
        gfx++,
        f_CopiedImage,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        IMAGE_WIDTH,
        0,
        firstColumn,
        firstRow,
        firstColumn + COPIED_COLUMN_COUNT - 1,
        firstRow + rowCount - 1,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gSPTextureRectangle(
        gfx++,
        x << 2,
        y << 2,
        (x + COPIED_COLUMN_COUNT - 1) << 2,
        (y + rowCount - 1) << 2,
        G_TX_RENDERTILE,
        firstColumn << 5,
        firstRow << 5,
        4 << 10,
        1 << 10
    );
    return gfx;
}

// Starts a list that copies into f_DepthImage, both the color and the
// depth image, as the game restores the saved frame's depth
static Gfx *BeginDepthCopyList(void) {
    Gfx *gfx = BeginList(f_DisplayList);

    gDPSetDepthImage(gfx++, f_DepthImage);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_DepthImage);
    gDPSetCycleType(gfx++, G_CYC_COPY);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    return gfx;
}

// Ends the list with a triangle into f_ColorImage that tests depth on
// f_DepthImage, runs it, and gives the triangle's pass
static const GfxPass *DrawOnCopiedDepth(Gfx *gfx) {
    gDPPipeSync(gfx++);
    ResetCapture();
    DrawOnDepth(gfx);
    EXPECT(f_Gpu.passCount != 0);
    return &f_Gpu.passes[f_Gpu.passCount - 1];
}

// Whether the pass draws a depth copy from source first, and the draw that
// tests depth after it
static bool DoesPassCopyDepth(const GfxPass *pass, const GfxTarget *source) {
    const GfxDraw *copy = &f_Gpu.draws[pass->firstDraw];

    return pass->drawCount == 2 &&
        copy->pipelineKey == DEPTH_COPY_KEY &&
        copy->source == source &&
        f_Gpu.draws[pass->firstDraw + 1].source == NULL &&
        f_Gpu.draws[pass->firstDraw + 1].pipelineKey ==
        (PIPELINE_DEPTH_TEST | PIPELINE_DEPTH_WRITE);
}

// Checks that the six vertices from first cover left, top to right, bottom
// in pixels, exclusive, and read the source's pixels from sourceX, sourceY
static void ExpectDepthCopyAt(
    uint32_t first,
    int left,
    int top,
    int right,
    int bottom,
    int sourceX,
    int sourceY
) {
    uint32_t i;

    ExpectDepthFillAt(first, left, top, right, bottom, 0.0f);
    for (i = first; i < first + 6 && i < f_Gpu.vertexCount; i++) {
        const GfxVertex *vertex = &f_Gpu.vertices[i];
        float x = (vertex->position[0] + 1.0f) * (IMAGE_WIDTH / 2);
        float y = (1.0f - vertex->position[1]) * (IMAGE_HEIGHT / 2);

        EXPECT_NEAR(vertex->texCoord[0] - x, sourceX - left, 1e-4);
        EXPECT_NEAR(vertex->texCoord[1] - y, sourceY - top, 1e-4);
    }
}

// The game restores a saved frame's depth by copying the memory that held
// it as the depth image into the depth image's memory. The GPU drew that
// depth into a depth target, not memory, so the copy reads it there: the
// depth image's next use draws the copy into its target first, a draw
// that writes no color, before the draw that uses it.
static void TestCopyOfDrawnDepthDrawsIt(void) {
    GfxTarget *saved = SaveDepth();
    Gfx *gfx = BeginDepthCopyList();
    const GfxPass *pass;
    const GfxRect wholeImage = {0, 0, IMAGE_WIDTH, IMAGE_HEIGHT};

    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 2, 10, 4, 6);
    pass = DrawOnCopiedDepth(gfx);

    // The color copy still happens, as the colors that memory holds
    EXPECT(f_Gpu.copyCount == 1);
    EXPECT(pass->depth != NULL && pass->depth != saved);
    EXPECT(DoesPassCopyDepth(pass, saved));
    EXPECT(f_Gpu.draws[pass->firstDraw].vertexCount == 6);
    EXPECT(SDL_memcmp(&f_Gpu.draws[pass->firstDraw].scissor, &wholeImage, sizeof(wholeImage)) == 0);
    ExpectDepthCopyAt(f_Gpu.draws[pass->firstDraw].firstVertex, 2, 10, 18, 16, 8, 4);
}

// The strips of the saved frame's depth become one copy, as its colors do
static void TestDepthStripsBecomeOne(void) {
    GfxTarget *saved = SaveDepth();
    Gfx *gfx = BeginDepthCopyList();
    const GfxPass *pass;

    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 2, 10, 4, 6);
    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 10, 2);
    gfx = CopyRows(gfx, 2, 16, 10, 2);
    pass = DrawOnCopiedDepth(gfx);

    EXPECT(DoesPassCopyDepth(pass, saved));
    EXPECT(f_Gpu.draws[pass->firstDraw].vertexCount == 6);
    ExpectDepthCopyAt(f_Gpu.draws[pass->firstDraw].firstVertex, 2, 10, 18, 18, 8, 4);
}

// A copy that ends at the saved depth's last row and column still lies
// within its depth image, so it copies depth
static void TestDepthCopyReachesTheImageEdge(void) {
    GfxTarget *saved = SaveDepth();
    Gfx *gfx = BeginDepthCopyList();
    const GfxPass *pass;

    gfx = CopyColumns(
        gfx,
        IMAGE_WIDTH - COPIED_COLUMN_COUNT,
        IMAGE_HEIGHT - 6,
        6,
        2,
        IMAGE_HEIGHT - 6
    );
    pass = DrawOnCopiedDepth(gfx);

    EXPECT(DoesPassCopyDepth(pass, saved));
    ExpectDepthCopyAt(
        f_Gpu.draws[pass->firstDraw].firstVertex,
        2,
        IMAGE_HEIGHT - 6,
        2 + COPIED_COLUMN_COUNT,
        IMAGE_HEIGHT,
        IMAGE_WIDTH - COPIED_COLUMN_COUNT,
        IMAGE_HEIGHT - 6
    );
}

// The depth image drawn under a color image of half the rows has half the
// rows: a copy of its last rows copies its depth, and one of the rows past
// them, which it lacks, copies none
static void TestDepthCopyNeedsDrawnDepthRows(void) {
    GfxTarget *saved = SaveDepthOf(f_CopiedImage, f_HalfImage, IMAGE_HEIGHT / 2);
    Gfx *gfx = BeginDepthCopyList();

    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, (IMAGE_HEIGHT / 2) - 6, 6);
    gfx = CopyRows(gfx, 2, 10, (IMAGE_HEIGHT / 2) - 6, 6);
    EXPECT(DoesPassCopyDepth(DrawOnCopiedDepth(gfx), saved));

    SaveDepthOf(f_CopiedImage, f_HalfImage, IMAGE_HEIGHT / 2);
    gfx = BeginDepthCopyList();
    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, (IMAGE_HEIGHT / 2) - 5, 6);
    gfx = CopyRows(gfx, 2, 10, (IMAGE_HEIGHT / 2) - 5, 6);
    EXPECT(DrawOnCopiedDepth(gfx)->drawCount == 1);
}

// Depth drawn into memory that a color image holds from its fourth row is
// newer than that color image, so a copy of the memory copies it, from
// four rows higher in the depth target
static void TestDepthWithinColorImageCopies(void) {
    GfxTarget *saved = SaveDepthOf(&f_CopiedImage[4 * IMAGE_WIDTH], f_ColorImage, IMAGE_HEIGHT);
    Gfx *gfx = BeginDepthCopyList();
    const GfxPass *pass;

    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 8, 6);
    gfx = CopyRows(gfx, 2, 10, 8, 6);
    pass = DrawOnCopiedDepth(gfx);

    EXPECT(DoesPassCopyDepth(pass, saved));
    ExpectDepthCopyAt(f_Gpu.draws[pass->firstDraw].firstVertex, 2, 10, 18, 16, 8, 4);
}

// A strip copied after the saved depth is drawn on again doesn't lengthen
// the copy before: that copy read what is gone, so only the later strip
// copies depth
static void TestRedrawnDepthStartsAnotherCopy(void) {
    GfxTarget *saved = SaveDepth();
    const GfxPass *pass;
    Gfx *gfx = BeginDepthCopyList();

    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 2, 10, 4, 6);
    gDPPipeSync(gfx++);
    gDPSetDepthImage(gfx++, f_CopiedImage);
    gfx = BeginTriangles(gfx, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    gDPPipeSync(gfx++);
    gDPSetDepthImage(gfx++, f_DepthImage);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_DepthImage);
    gDPSetCycleType(gfx++, G_CYC_COPY);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 10, 2);
    gfx = CopyRows(gfx, 2, 16, 10, 2);
    pass = DrawOnCopiedDepth(gfx);

    EXPECT(DoesPassCopyDepth(pass, saved));
    ExpectDepthCopyAt(f_Gpu.draws[pass->firstDraw].firstVertex, 2, 16, 18, 18, 8, 10);
}

// Draws as the color image into memory that was drawn on as the depth
// image, in a task of its own: what memory holds is then the color image's
static void FillCopiedImageAsColor(void) {
    Gfx *gfx = BeginList(f_DisplayList);

    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetDepthImage(gfx++, f_DepthImage);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_CopiedImage);
    gDPSetFillColor(gfx++, 0);
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, 3);
    RunList(gfx);
}

// Fills the saved depth's rows from top to bottom as the depth image, in a
// task of its own: the fill waits for the memory's next use as the depth
// image, so its target lacks it
static void FillCopiedImageAsDepth(int top, int bottom) {
    Gfx *gfx = BeginList(f_DisplayList);

    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetDepthImage(gfx++, f_CopiedImage);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_CopiedImage);
    gfx = FillDepth(gfx, 0, 0, top, IMAGE_WIDTH - 1, bottom);
    RunList(gfx);
}

// A copy of memory whose depth target no longer holds it copies no depth:
// when the color image drew there since, a fill or clear of it as the
// depth image waits, or the CPU wrote there since
static void TestCopyOfStaleDepthCopiesNone(void) {
    Gfx *gfx;

    SaveDepth();
    FillCopiedImageAsColor();
    gfx = BeginDepthCopyList();
    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 2, 10, 4, 6);
    EXPECT(DrawOnCopiedDepth(gfx)->drawCount == 1);

    SaveDepth();
    FillCopiedImageAsDepth(4, 9);
    gfx = BeginDepthCopyList();
    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 2, 10, 4, 6);
    EXPECT(DrawOnCopiedDepth(gfx)->drawCount == 1);

    SaveDepth();
    FillCopiedImageAsDepth(0, IMAGE_HEIGHT - 1);
    gfx = BeginDepthCopyList();
    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 2, 10, 4, 6);
    EXPECT(DrawOnCopiedDepth(gfx)->drawCount == 1);

    SaveDepth();
    f_CopiedImage[(5 * IMAGE_WIDTH) + COPIED_FIRST_COLUMN] ^= 0x0100;
    gfx = BeginDepthCopyList();
    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 2, 10, 4, 6);
    EXPECT(DrawOnCopiedDepth(gfx)->drawCount == 1);
}

// A depth copy waits for the depth image's next use; drawing on the
// copied depth in the meantime leaves the copy nothing sure to read
static void TestDepthDrawnAfterCopyCopiesNone(void) {
    Gfx *gfx;

    SaveDepth();
    gfx = BeginDepthCopyList();
    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 2, 10, 4, 6);
    gDPPipeSync(gfx++);
    gDPSetDepthImage(gfx++, f_CopiedImage);
    gfx = BeginTriangles(gfx, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPVertex(gfx++, f_Triangle, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    gDPSetDepthImage(gfx++, f_DepthImage);
    EXPECT(DrawOnCopiedDepth(gfx)->drawCount == 1);
}

// A fill of the whole depth image after a copy into it covers the copy
static void TestDepthClearCoversCopy(void) {
    const GfxPass *pass;
    Gfx *gfx;

    SaveDepth();
    gfx = BeginDepthCopyList();
    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 2, 10, 4, 6);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gfx = FillDepth(gfx, G_MAXFBZ, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    pass = DrawOnCopiedDepth(gfx);
    EXPECT(pass->clearsDepth && pass->drawCount == 1);
}

// Fills of part of the depth image and copies into it draw in the order
// that the game made them
static void TestDepthWritesKeepTheirOrder(void) {
    GfxTarget *saved = SaveDepth();
    const GfxPass *pass;
    Gfx *gfx = BeginDepthCopyList();

    gDPSetCycleType(gfx++, G_CYC_FILL);
    gfx = FillDepth(gfx, 0, 0, 0, 3, 3);
    gDPSetCycleType(gfx++, G_CYC_COPY);
    gfx = LoadRows(gfx, f_CopiedImage, IMAGE_WIDTH, 4, 6);
    gfx = CopyRows(gfx, 2, 10, 4, 6);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gfx = FillDepth(gfx, 0, 4, 4, 7, 7);
    pass = DrawOnCopiedDepth(gfx);

    EXPECT(pass->drawCount == 4);
    EXPECT(f_Gpu.draws[pass->firstDraw].pipelineKey == DEPTH_FILL_KEY);
    EXPECT(f_Gpu.draws[pass->firstDraw + 1].pipelineKey == DEPTH_COPY_KEY);
    EXPECT(f_Gpu.draws[pass->firstDraw + 1].source == saved);
    EXPECT(f_Gpu.draws[pass->firstDraw + 2].pipelineKey == DEPTH_FILL_KEY);
    ExpectDepthFillAt(f_Gpu.draws[pass->firstDraw + 2].firstVertex, 4, 4, 8, 8, 0.0f);
}

int main(void) {
    static const int16_t depthProjection[4][4] = {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 1},
        {0, 0, -2, 0},
    };

    MakeIdentity(&f_Identity);
    MakeIntegerMatrix(&f_DepthProjection, depthProjection);
    InitRdp();
    InitRsp();
    TestFillRectangle();
    TestReversedRectanglesDrawNothing();
    TestTriangleTransform();
    TestNearPlaneClipping();
    TestFogBehindEye();
    TestClipRatio();
    TestTexelsOfEachCycle();
    TestAlphaCompareModes();
    TestTextureFilterModes();
    TestCopyCycleNeverFilters();
    TestBlendShapes();
    TestUnforcedBlenderPassesFirstColor();
    TestDepthClear();
    TestDepthSources();
    TestTrianglesWithoutDepthTakePrimitives();
    TestPrimitiveDepthMatchesVertexDepth();
    TestColorFillClearsLaterDepth();
    TestDepthClearNotesHoldFour();
    TestDepthClearOutlastsColorFill();
    TestLaterDepthFillWins();
    TestPartialDepthFillDraws();
    TestDepthFillClearsOnlyWhole();
    TestDepthFillKeepsItsScissor();
    TestDepthFillsKeepTheirOrder();
    TestDepthFillWaitsForItsImage();
    TestDepthFillsHoldFour();
    TestN64Addresses();
    TestRomOrderList();
    TestRomListCallsIntoRomList();
    TestRomListCallsIntoGameList();
    TestRomListCallsPastHostWrittenData();
    TestRomListCallsIntoRdram();
    TestGameListCallsIntoGameList();
    TestTextureFromDrawnImage();
    TestShownTarget();
    TestCpuWritesOutliveDrawing();
    TestCpuReadsDrawing();
    TestImageSizedByDrawing();
    TestImageGrowsInPlace();
    TestCopyBetweenDrawnImages();
    TestCopiedStripsBecomeOne();
    TestCopyKeepsToTheScissor();
    TestCopiesThatDraw();
    TestCopyNeedsDrawnRows();
    TestCopyOfPixelsNoTargetHoldsDraws();
    TestCopyAfterCpuWritesDraws();
    TestCopyOfDrawnDepthDrawsIt();
    TestDepthStripsBecomeOne();
    TestDepthCopyReachesTheImageEdge();
    TestDepthCopyNeedsDrawnDepthRows();
    TestRedrawnDepthStartsAnotherCopy();
    TestDepthWithinColorImageCopies();
    TestCopyOfStaleDepthCopiesNone();
    TestDepthDrawnAfterCopyCopiesNone();
    TestDepthClearCoversCopy();
    TestDepthWritesKeepTheirOrder();
    TestShutdownReleasesTargets();
    return ReportTestResults("rcp_test");
}
