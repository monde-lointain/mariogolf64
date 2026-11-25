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
// drew, which comes back to memory first
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
    gSPTextureRectangle(gfx++, 0, 0, 7 << 2, 7 << 2, G_TX_RENDERTILE, 0, 0, 4 << 10, 1 << 10);
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
    TestColorFillClearsLaterDepth();
    TestDepthClearNotesHoldFour();
    TestDepthClearOutlastsColorFill();
    TestLaterDepthFillWins();
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
    TestShutdownReleasesTargets();
    return ReportTestResults("rcp_test");
}
