/*
 * Tests of the display list interpreter, through a GPU double that keeps
 * the frames that the interpreter sends it.
 */

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
} f_Gpu;

static uint16_t f_ColorImage[IMAGE_WIDTH * IMAGE_HEIGHT];
static uint16_t f_DepthImage[IMAGE_WIDTH * IMAGE_HEIGHT];
static uint16_t f_OtherImage[IMAGE_WIDTH * IMAGE_HEIGHT];
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

void BackendWriteTarget(GfxTarget *target, uint32_t width, uint32_t height, const uint32_t *rgba) {
    (void)target;
    (void)width;
    (void)height;
    (void)rgba;
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

// A list from ROM holds the N64's words, big-endian, and reaches its
// vertices through a segment
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
    MakeIdentity(&f_Identity);
    InitRdp();
    InitRsp();
    TestFillRectangle();
    TestTriangleTransform();
    TestAlphaCompareModes();
    TestDepthClear();
    TestRomOrderList();
    TestTextureFromDrawnImage();
    TestShownTarget();
    TestCpuWritesOutliveDrawing();
    TestShutdownReleasesTargets();
    return ReportTestResults("rcp_test");
}
