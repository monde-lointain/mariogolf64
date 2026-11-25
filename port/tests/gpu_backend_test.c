/*
 * Tests of the renderer on a real GPU: display lists run through the
 * interpreter and the SDL_GPU backend, and the test reads the pixels that
 * come back to memory. Without a GPU the test is skipped.
 */

#include <SDL3/SDL.h>
#include <ultra64.h>

#include "color_images.h"
#include "rcp.h"
#include "rsp.h"
#include "sdl_gpu_backend.h"
#include "test_support.h"

#define IMAGE_WIDTH 64
#define IMAGE_HEIGHT 32
#define DISPLAY_LIST_LENGTH 128
// CTest's code for a skipped test
#define SKIPPED_EXIT_CODE 77
// How far a channel may stray, in 5-bit steps
#define CHANNEL_TOLERANCE 1
// The largest scale that tests read a target at in full
#define SCALE_MAX 6
// Long enough for a second thread, were it free to, to start recording
#define RECORDING_WAIT_MS 5
#define RECORDING_ROUNDS 20

static u16 f_ColorImage[IMAGE_WIDTH * IMAGE_HEIGHT] __attribute__((aligned(64)));
static u16 f_DepthImage[IMAGE_WIDTH * IMAGE_HEIGHT] __attribute__((aligned(64)));
static Gfx f_DisplayList[DISPLAY_LIST_LENGTH];
static SDL_GPUDevice *f_Device;
static uint32_t f_TargetPixels[(IMAGE_WIDTH * SCALE_MAX) * (IMAGE_HEIGHT * SCALE_MAX)];
static Mtx f_Identity;
// Scales the vertices' coordinates of 128 down to 1
static Mtx f_Shrink;
// Scales x and y as f_Shrink does, and makes w 1 more than z / 64, so that
// a vertex's distance sets how far it is
static Mtx f_Recede;
static Vp f_Viewport = {{
    {IMAGE_WIDTH * 2, IMAGE_HEIGHT * 2, G_MAXZ / 2, 0},
    {IMAGE_WIDTH * 2, IMAGE_HEIGHT * 2, G_MAXZ / 2, 0},
}};

// A matrix with a diagonal of scale / 65536, in the RSP's split 16.16 form
static void MakeDiagonal(Mtx *matrix, int32_t scale) {
    int32_t *words = (int32_t *)matrix->m;
    int32_t whole = scale >> 16;
    int32_t fraction = scale & 0xFFFF;

    SDL_zerop(matrix);
    words[0] = whole << 16;
    words[2] = whole;
    words[5] = whole << 16;
    words[7] = 1;
    words[8] = fraction << 16;
    words[10] = fraction;
    words[13] = fraction << 16;
}

// A matrix of these elements, in the RSP's split 16.16 form: the whole
// halves of the elements in order, then their fractions
static void MakeMatrix(Mtx *matrix, const float elements[4][4]) {
    uint32_t *words = (uint32_t *)matrix->m;
    int i;

    SDL_zerop(matrix);
    for (i = 0; i < 16; i++) {
        uint32_t fixed = (uint32_t)(int32_t)SDL_lroundf(elements[i / 4][i % 4] * 65536.0f);
        int shift = (i % 2) == 0 ? 16 : 0;

        words[i / 2] |= (fixed >> 16) << shift;
        words[8 + (i / 2)] |= (fixed & 0xFFFF) << shift;
    }
}

// A vertex that f_Recede puts at x and y over the image, from -1 to 1, at
// distance w, with texture coordinate s in texels
static void PlaceVertex(Vtx *vertex, float x, float y, float w, int s) {
    SDL_zerop(vertex);
    vertex->v.ob[0] = (s16)(x * w * 128.0f);
    vertex->v.ob[1] = (s16)(y * w * 128.0f);
    vertex->v.ob[2] = (s16)((w - 1.0f) * 64.0f);
    vertex->v.tc[0] = (s16)(s << 5);
    vertex->v.cn[1] = 0xFF;
    vertex->v.cn[3] = 0xFF;
}

static Gfx *BeginList(Gfx *gfx) {
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_ColorImage);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT);
    return gfx;
}

static Gfx *FillImage(Gfx *gfx, u16 color) {
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(gfx++, ((u32)color << 16) | color);
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    gDPPipeSync(gfx++);
    return gfx;
}

// Runs the list and brings the color image's memory up to date with it
static void RunList(Gfx *end) {
    gSPEndDisplayList(end++);
    SDL_assert(end <= f_DisplayList + DISPLAY_LIST_LENGTH);
    RunRspTask(f_DisplayList);
    SyncRdpMemory(f_ColorImage, sizeof(f_ColorImage));
}

static bool IsChannelNear(u16 pixel, int shift, int expected) {
    int channel = (pixel >> shift) & 0x1F;

    return SDL_abs(channel - (expected >> 3)) <= CHANNEL_TOLERANCE;
}

static void ExpectPixel(int line, int x, int y, int red, int green, int blue) {
    u16 pixel = f_ColorImage[(y * IMAGE_WIDTH) + x];

    if (!IsChannelNear(pixel, 11, red) ||
        !IsChannelNear(pixel, 6, green) ||
        !IsChannelNear(pixel, 1, blue)) {
        fprintf(
            stderr,
            "line %d: pixel (%d, %d) is 0x%04X, not near (%d, %d, %d)\n",
            line,
            x,
            y,
            pixel,
            red,
            green,
            blue
        );
        g_TestFailureCount++;
    }
}

#define EXPECT_PIXEL(x, y, red, green, blue) ExpectPixel(__LINE__, x, y, red, green, blue)

// Every pixel of a fill takes its color
static void TestFill(void) {
    Gfx *gfx = BeginList(f_DisplayList);

    gfx = FillImage(gfx, GPACK_RGBA5551(255, 0, 0, 1));
    RunList(gfx);

    EXPECT_PIXEL(0, 0, 255, 0, 0);
    EXPECT_PIXEL(IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1, 255, 0, 0);
    EXPECT(f_ColorImage[17] == GPACK_RGBA5551(255, 0, 0, 1));
}

static Gfx *BeginTriangles(Gfx *gfx, u32 geometryMode, u32 renderMode, u32 renderMode2) {
    gSPViewport(gfx++, &f_Viewport);
    gSPMatrix(gfx++, &f_Shrink, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gfx++, &f_Identity, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPLoadGeometryMode(gfx++, geometryMode);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, renderMode, renderMode2);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    return gfx;
}

// A quad over the whole image at depth z, of one color
static Gfx *DrawQuad(Gfx *gfx, Vtx *vertices, s16 z, u8 red, u8 green, u8 blue) {
    static const s16 corners[4][2] = {{-128, -128}, {128, -128}, {128, 128}, {-128, 128}};
    int i;

    for (i = 0; i < 4; i++) {
        Vtx_t *vertex = &vertices[i].v;

        SDL_zerop(vertex);
        vertex->ob[0] = corners[i][0];
        vertex->ob[1] = corners[i][1];
        vertex->ob[2] = z;
        vertex->cn[0] = red;
        vertex->cn[1] = green;
        vertex->cn[2] = blue;
        vertex->cn[3] = 0xFF;
    }

    gSPVertex(gfx++, vertices, 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    return gfx;
}

// Shaded triangles cover the pixels within them with their vertices' color
static void TestShadedTriangles(void) {
    static Vtx vertices[4];
    Gfx *gfx = BeginList(f_DisplayList);

    gfx = FillImage(gfx, GPACK_RGBA5551(0, 0, 0, 1));
    gfx = BeginTriangles(gfx, G_SHADE | G_SHADING_SMOOTH, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gfx = DrawQuad(gfx, vertices, 0, 0, 0xFF, 0);
    RunList(gfx);

    EXPECT_PIXEL(0, 0, 0, 255, 0);
    EXPECT_PIXEL(IMAGE_WIDTH / 2, IMAGE_HEIGHT / 2, 0, 255, 0);
    EXPECT_PIXEL(IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1, 0, 255, 0);
}

// A copy of a texture lays its texels on the pixels one for one, read from
// the host words that the game's arrays hold
static void TestCopiedTexture(void) {
    static u16 texture[4 * 4] __attribute__((aligned(8)));
    Gfx *gfx = BeginList(f_DisplayList);
    int i;

    for (i = 0; i < 4 * 4; i++) {
        texture[i] = (i & 1) ? GPACK_RGBA5551(0, 0, 255, 1) : GPACK_RGBA5551(255, 255, 0, 1);
    }

    texture[15] = GPACK_RGBA5551(255, 255, 255, 1);
    gfx = FillImage(gfx, GPACK_RGBA5551(0, 0, 0, 1));
    gDPSetCycleType(gfx++, G_CYC_COPY);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPLoadTextureBlock(
        gfx++,
        texture,
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
    gSPTextureRectangle(gfx++, 0, 0, 3 << 2, 3 << 2, G_TX_RENDERTILE, 0, 0, 4 << 10, 1 << 10);
    RunList(gfx);

    EXPECT_PIXEL(0, 0, 255, 255, 0);
    EXPECT_PIXEL(1, 0, 0, 0, 255);
    EXPECT_PIXEL(2, 1, 255, 255, 0);
    EXPECT_PIXEL(3, 3, 255, 255, 255);
    EXPECT_PIXEL(4, 0, 0, 0, 0);
    EXPECT_PIXEL(0, 4, 0, 0, 0);
}

// The RDP finds a pixel's texture coordinates at its corner, not its
// center, so a filtered texture laid one texel to a pixel keeps its texels
// whole rather than blending each with its neighbors
static void TestFilteredTexelsOneForOne(void) {
    static u16 texture[4 * 4] __attribute__((aligned(8)));
    Gfx *gfx = BeginList(f_DisplayList);
    int i;

    for (i = 0; i < 4 * 4; i++) {
        bool isOdd = (((i % 4) + (i / 4)) & 1) != 0;

        texture[i] = isOdd ? GPACK_RGBA5551(0, 0, 255, 1) : GPACK_RGBA5551(255, 255, 0, 1);
    }

    gfx = FillImage(gfx, GPACK_RGBA5551(0, 0, 0, 1));
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 0, 0, 1, 0, 0, 0, TEXEL0, 0, 0, 0, 1);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPLoadTextureBlock(
        gfx++,
        texture,
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
    gSPTextureRectangle(
        gfx++,
        8 << 2,
        8 << 2,
        12 << 2,
        12 << 2,
        G_TX_RENDERTILE,
        0,
        0,
        1 << 10,
        1 << 10
    );
    RunList(gfx);

    EXPECT_PIXEL(8, 8, 255, 255, 0);
    EXPECT_PIXEL(9, 8, 0, 0, 255);
    EXPECT_PIXEL(8, 9, 0, 0, 255);
    EXPECT_PIXEL(10, 9, 0, 0, 255);
    EXPECT_PIXEL(10, 10, 255, 255, 0);
}

// Reads the color image's target at its full scale into f_TargetPixels,
// RGBA8 with red lowest, giving its width
static uint32_t ReadScaledTarget(void) {
    uint32_t scale = GpuBackendScale();
    uint16_t width;
    uint16_t height;
    GfxTarget *target = LockShownTarget((const uint8_t *)f_ColorImage, &width, &height);
    SDL_GPUTransferBufferCreateInfo info;
    SDL_GPUTransferBuffer *buffer;
    SDL_GPUTextureRegion region;
    SDL_GPUTextureTransferInfo destination;
    SDL_GPUCommandBuffer *commands;
    SDL_GPUCopyPass *copy;
    SDL_GPUFence *fence;

    SDL_assert(target != NULL);
    SDL_assert(scale <= SCALE_MAX);
    SDL_zero(info);
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    info.size = width * scale * height * scale * (uint32_t)sizeof(uint32_t);
    buffer = SDL_CreateGPUTransferBuffer(f_Device, &info);
    SDL_zero(region);
    region.texture = GpuTargetTexture(target);
    region.w = width * scale;
    region.h = height * scale;
    region.d = 1;
    SDL_zero(destination);
    destination.transfer_buffer = buffer;
    commands = BeginGpuCommands();
    copy = SDL_BeginGPUCopyPass(commands);
    SDL_DownloadFromGPUTexture(copy, &region, &destination);
    SDL_EndGPUCopyPass(copy);
    fence = SubmitGpuCommandsWithFence(commands);
    UnlockShownTarget();
    SDL_WaitForGPUFences(f_Device, true, &fence, 1);
    SDL_ReleaseGPUFence(f_Device, fence);
    SDL_memcpy(f_TargetPixels, SDL_MapGPUTransferBuffer(f_Device, buffer, false), info.size);
    SDL_UnmapGPUTransferBuffer(f_Device, buffer);
    SDL_ReleaseGPUTransferBuffer(f_Device, buffer);
    return region.w;
}

// Readies rectangles of the RGBA16 texture's texels, width by height of
// them, point sampled and wrapping, in the 1-cycle mode
static Gfx *BeginTexturedRectangles(Gfx *gfx, const u16 *texture, int width, int height) {
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 0, 0, 1, 0, 0, 0, TEXEL0, 0, 0, 0, 1);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPLoadTextureBlock(
        gfx++,
        texture,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        width,
        height,
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

// Upscaled, each target pixel takes the texel at its own corner, so a
// rectangle that starts on a texel's corner shows only that texel and the
// ones after it, never a sliver of the texels before
static void TestUpscaledTexelsStartAtTheirCorner(void) {
    static u16 texture[16 * 32] __attribute__((aligned(8)));
    Gfx *gfx = BeginList(f_DisplayList);
    int darkCount = 0;
    uint32_t width;
    int x;
    int y;
    int i;

    // White from s 8 and t 16, where each rectangle starts, black before
    for (i = 0; i < 16 * 32; i++) {
        bool isWhite = (i % 16) >= 8 && (i / 16) >= 16;

        texture[i] = isWhite ? GPACK_RGBA5551(255, 255, 255, 1) : GPACK_RGBA5551(0, 0, 0, 1);
    }

    gfx = FillImage(gfx, GPACK_RGBA5551(0, 0, 255, 1));
    gfx = BeginTexturedRectangles(gfx, texture, 16, 32);
    for (y = 0; y < IMAGE_HEIGHT; y += 8) {
        for (x = 0; x < IMAGE_WIDTH; x += 8) {
            gSPTextureRectangle(
                gfx++,
                x << 2,
                y << 2,
                (x + 8) << 2,
                (y + 8) << 2,
                G_TX_RENDERTILE,
                8 << 5,
                16 << 5,
                1 << 10,
                1 << 10
            );
        }
    }

    RunList(gfx);

    width = ReadScaledTarget();
    for (i = 0; i < (int)(width * IMAGE_HEIGHT * GpuBackendScale()); i++) {
        darkCount += f_TargetPixels[i] != 0xFFFFFFFFu;
    }

    if (darkCount != 0) {
        fprintf(stderr, "%d pixels of upscaled rectangles are not white\n", darkCount);
    }

    EXPECT(darkCount == 0);
}

// The RDP counts a texture coordinate's steps in 1/1024 texel, so a pixel
// that a step of 1023/1024 texel brings 1/1024 short of a texel's edge
// takes the texel before; upscaled, so does the target's pixel at its
// corner, and those after it within the pixel take the next
static void TestUpscaledTexelsKeepFineSteps(void) {
    static u16 texture[8] __attribute__((aligned(8)));
    Gfx *gfx = BeginList(f_DisplayList);
    uint32_t scale = GpuBackendScale();
    uint32_t width;
    int wrongCount = 0;
    uint32_t x;
    uint32_t y;
    int i;

    for (i = 0; i < 8; i++) {
        texture[i] = (i & 1) ? GPACK_RGBA5551(0, 0, 0, 1) : GPACK_RGBA5551(255, 255, 255, 1);
    }

    gfx = FillImage(gfx, GPACK_RGBA5551(0, 0, 255, 1));
    gfx = BeginTexturedRectangles(gfx, texture, 8, 1);
    gSPTextureRectangle(gfx++, 0, 0, 8 << 2, 1 << 2, G_TX_RENDERTILE, 0, 0, 1023, 1 << 10);
    RunList(gfx);

    width = ReadScaledTarget();
    for (y = 0; y < scale; y++) {
        for (x = 0; x < 8 * scale; x++) {
            // At the corner of target pixel x, s is x / scale * 1023 / 1024
            uint32_t texel = (x * 1023) / (scale * 1024);
            uint32_t expected = (texel & 1) ? 0xFF000000u : 0xFFFFFFFFu;

            wrongCount += f_TargetPixels[(y * width) + x] != expected;
        }
    }

    if (wrongCount != 0) {
        fprintf(stderr, "%d pixels of a finely stepped rectangle are wrong\n", wrongCount);
    }

    EXPECT(wrongCount == 0);
}

// A translucent surface mixes its color with memory's by its alpha
static void TestTranslucentBlend(void) {
    static Vtx vertices[4];
    Gfx *gfx = BeginList(f_DisplayList);
    int i;

    gfx = FillImage(gfx, GPACK_RGBA5551(255, 0, 0, 1));
    gfx = BeginTriangles(gfx, G_SHADE | G_SHADING_SMOOTH, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gfx = DrawQuad(gfx, vertices, 0, 0, 0, 0xFF);
    for (i = 0; i < 4; i++) {
        vertices[i].v.cn[3] = 0x80;
    }

    RunList(gfx);

    EXPECT_PIXEL(IMAGE_WIDTH / 2, IMAGE_HEIGHT / 2, 127, 0, 128);
}

// Fills the depth image with the farthest depth, as games clear it, and
// goes back to the color image
static Gfx *ClearDepth(Gfx *gfx) {
    gDPSetDepthImage(gfx++, f_DepthImage);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_DepthImage);
    gDPSetFillColor(gfx++, (GPACK_ZDZ(G_MAXFBZ, 0) << 16) | GPACK_ZDZ(G_MAXFBZ, 0));
    gDPFillRectangle(gfx++, 0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    gDPPipeSync(gfx++);
    return BeginList(gfx);
}

// Surfaces hide those behind them that come later, and not those before
static void TestDepth(void) {
    static Vtx near[4];
    static Vtx far[4];
    static Vtx nearer[4];
    Gfx *gfx = ClearDepth(f_DisplayList);

    gfx = FillImage(gfx, GPACK_RGBA5551(0, 0, 0, 1));
    gfx = BeginTriangles(
        gfx,
        G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH,
        G_RM_ZB_OPA_SURF,
        G_RM_ZB_OPA_SURF2
    );
    gfx = DrawQuad(gfx, near, -64, 0, 0xFF, 0);
    gfx = DrawQuad(gfx, far, 64, 0xFF, 0, 0);
    RunList(gfx);
    EXPECT_PIXEL(IMAGE_WIDTH / 2, IMAGE_HEIGHT / 2, 0, 255, 0);

    // The depth stays for the next task
    gfx = BeginList(f_DisplayList);
    gDPSetDepthImage(gfx++, f_DepthImage);
    gfx = BeginTriangles(
        gfx,
        G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH,
        G_RM_ZB_OPA_SURF,
        G_RM_ZB_OPA_SURF2
    );
    gfx = DrawQuad(gfx, nearer, -100, 0, 0, 0xFF);
    gfx = DrawQuad(gfx, far, 64, 0xFF, 0, 0);
    RunList(gfx);
    EXPECT_PIXEL(IMAGE_WIDTH / 2, IMAGE_HEIGHT / 2, 0, 0, 255);
}

// A fill of part of the depth image sets the depth there alone, nearer or
// farther than it was, and draws no color
static void TestPartialDepthFill(void) {
    static Vtx far[4];
    Gfx *gfx = ClearDepth(f_DisplayList);

    // The nearest depth over the left half, then the farthest over its
    // left half
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, IMAGE_WIDTH, f_DepthImage);
    gDPSetFillColor(gfx++, (GPACK_ZDZ(0, 0) << 16) | GPACK_ZDZ(0, 0));
    gDPFillRectangle(gfx++, 0, 0, (IMAGE_WIDTH / 2) - 1, IMAGE_HEIGHT - 1);
    gDPPipeSync(gfx++);
    gDPSetFillColor(gfx++, (GPACK_ZDZ(G_MAXFBZ, 0) << 16) | GPACK_ZDZ(G_MAXFBZ, 0));
    gDPFillRectangle(gfx++, 0, 0, (IMAGE_WIDTH / 4) - 1, IMAGE_HEIGHT - 1);
    gDPPipeSync(gfx++);
    gfx = BeginList(gfx);
    gfx = FillImage(gfx, GPACK_RGBA5551(0, 0, 255, 1));
    gfx = BeginTriangles(
        gfx,
        G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH,
        G_RM_ZB_OPA_SURF,
        G_RM_ZB_OPA_SURF2
    );
    gfx = DrawQuad(gfx, far, 64, 0xFF, 0, 0);
    RunList(gfx);
    EXPECT_PIXEL(0, IMAGE_HEIGHT / 2, 255, 0, 0);
    EXPECT_PIXEL((IMAGE_WIDTH / 4) - 1, IMAGE_HEIGHT / 2, 255, 0, 0);
    EXPECT_PIXEL(IMAGE_WIDTH / 4, IMAGE_HEIGHT / 2, 0, 0, 255);
    EXPECT_PIXEL((IMAGE_WIDTH / 2) - 1, IMAGE_HEIGHT / 2, 0, 0, 255);
    EXPECT_PIXEL(IMAGE_WIDTH / 2, IMAGE_HEIGHT / 2, 255, 0, 0);
    EXPECT_PIXEL(IMAGE_WIDTH * 3 / 4, IMAGE_HEIGHT / 2, 255, 0, 0);
}

// Fills a texture of 4 by 4 texels with one color
static void FillTexture(u16 *texels, u16 color) {
    int i;

    for (i = 0; i < 4 * 4; i++) {
        texels[i] = color;
    }
}

// In two cycles the second cycle's TEXEL0 is the texel of the tile after
// the one that the first cycle's TEXEL0 reads
static void TestSecondCycleTexel(void) {
    static u16 red[4 * 4] __attribute__((aligned(8)));
    static u16 blue[4 * 4] __attribute__((aligned(8)));
    Gfx *gfx = BeginList(f_DisplayList);

    FillTexture(red, GPACK_RGBA5551(255, 0, 0, 1));
    FillTexture(blue, GPACK_RGBA5551(0, 0, 255, 1));
    gfx = FillImage(gfx, GPACK_RGBA5551(0, 0, 0, 1));
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPLoadTextureBlock(
        gfx++,
        red,
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
        blue,
        0x100,
        1,
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
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPSetRenderMode(gfx++, G_RM_PASS, G_RM_OPA_SURF2);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 0, 0, 1, 0, 0, 0, TEXEL0, 0, 0, 0, 1);
    gSPTextureRectangle(gfx++, 0, 0, 4 << 2, 4 << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    RunList(gfx);

    EXPECT_PIXEL(1, 1, 0, 0, 255);
}

// Draws a rectangle of the primitive color in two cycles, whose first
// cycle's alpha is firstAlpha and second's is secondAlpha, through a
// threshold compare at half
static void DrawComparedRectangle(u8 firstAlpha, u8 secondAlpha) {
    Gfx *gfx = BeginList(f_DisplayList);

    gfx = FillImage(gfx, GPACK_RGBA5551(0, 0, 0, 1));
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPSetRenderMode(gfx++, G_RM_PASS, G_RM_OPA_SURF2);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0, 0, 0, 0x80);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0, 0, firstAlpha);
    gDPSetEnvColor(gfx++, 0, 0, 0, secondAlpha);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        PRIMITIVE,
        0,
        0,
        0,
        PRIMITIVE,
        0,
        0,
        0,
        COMBINED,
        0,
        0,
        0,
        ENVIRONMENT
    );
    gSPTextureRectangle(gfx++, 0, 0, 4 << 2, 4 << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    RunList(gfx);
}

// In two cycles the alpha compare tests the first cycle's alpha, not the
// second's
static void TestTwoCycleAlphaCompare(void) {
    DrawComparedRectangle(0, 0xFF);
    EXPECT_PIXEL(1, 1, 0, 0, 0);

    DrawComparedRectangle(0xFF, 0);
    EXPECT_PIXEL(1, 1, 255, 0, 0);
}

// Texture coordinates change across a receding surface as the RDP finds
// them, by perspective: as their ratios to w, divided by w's reciprocal
static void TestPerspectiveTexture(void) {
    static u16 texture[32 * 2] __attribute__((aligned(8)));
    static Vtx vertices[4];
    Gfx *gfx = BeginList(f_DisplayList);
    int i;

    // Texels 0 to 9 are yellow, 10 to 19 blue and the rest white
    for (i = 0; i < 32 * 2; i++) {
        int s = i % 32;

        texture[i] = s < 10 ? GPACK_RGBA5551(255, 255, 0, 1) :
            s < 20          ? GPACK_RGBA5551(0, 0, 255, 1) :
                              GPACK_RGBA5551(255, 255, 255, 1);
    }

    gfx = FillImage(gfx, GPACK_RGBA5551(0, 0, 0, 1));
    gfx = BeginTriangles(gfx, G_SHADE | G_SHADING_SMOOTH, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gSPMatrix(gfx++, &f_Recede, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 0, 0, 1, 0, 0, 0, TEXEL0, 0, 0, 0, 1);
    gDPLoadTextureBlock(
        gfx++,
        texture,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        32,
        2,
        0,
        G_TX_CLAMP,
        G_TX_CLAMP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    // The left edge is at distance 1 and the right at 4, with s from 0 to 32
    PlaceVertex(&vertices[0], -1.0f, -1.0f, 1.0f, 0);
    PlaceVertex(&vertices[1], 1.0f, -1.0f, 4.0f, 32);
    PlaceVertex(&vertices[2], 1.0f, 1.0f, 4.0f, 32);
    PlaceVertex(&vertices[3], -1.0f, 1.0f, 1.0f, 0);
    gSPVertex(gfx++, vertices, 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    RunList(gfx);

    // A fraction f of the way across, s is 8f / (1 - 0.75f): 6.4 halfway,
    // where without perspective it would be 16, and 13.7 at three
    // quarters, rather than 24
    EXPECT_PIXEL(IMAGE_WIDTH / 2, IMAGE_HEIGHT / 2, 255, 255, 0);
    EXPECT_PIXEL(IMAGE_WIDTH * 3 / 4, IMAGE_HEIGHT / 2, 0, 0, 255);
}

// A surface that the GPU clips keeps its shade where a corner lies past
// two edges of the image and the corners are at different distances.
// Lavapipe, clipping such a surface in clip space with a depth image
// bound, gave screen-linear shade no number at all.
static void TestShadeClippedAtTwoEdges(void) {
    static Vtx vertices[3];
    Gfx *gfx = ClearDepth(f_DisplayList);

    gfx = FillImage(gfx, GPACK_RGBA5551(0, 0, 0, 1));
    gfx = BeginTriangles(
        gfx,
        G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH,
        G_RM_ZB_OPA_SURF,
        G_RM_ZB_OPA_SURF2
    );
    gSPMatrix(gfx++, &f_Recede, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    // Inside the RSP's clipping ratio of 2, so only the GPU clips them
    PlaceVertex(&vertices[0], -1.1f, -1.1f, 1.0f, 0);
    PlaceVertex(&vertices[1], 1.0f, -1.1f, 2.0f, 0);
    PlaceVertex(&vertices[2], -1.1f, 1.0f, 4.0f, 0);
    gSPVertex(gfx++, vertices, 3, 0);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    RunList(gfx);

    // Next to the corner past both edges, then inside, then outside
    EXPECT_PIXEL(1, IMAGE_HEIGHT - 2, 0, 255, 0);
    EXPECT_PIXEL(IMAGE_WIDTH / 4, IMAGE_HEIGHT * 3 / 4, 0, 255, 0);
    EXPECT_PIXEL(IMAGE_WIDTH * 3 / 4, IMAGE_HEIGHT / 4, 0, 0, 0);
}

static SDL_AtomicInt f_HasSecondBegun;

// The second thread: starts a command buffer, says so, then submits it
static int SDLCALL BeginSecondCommands(void *unused) {
    SDL_GPUCommandBuffer *commands = BeginGpuCommands();

    (void)unused;
    SDL_SetAtomicInt(&f_HasSecondBegun, 1);
    return commands != NULL && SubmitGpuCommands(commands);
}

// While one thread records, another that starts a command buffer waits
// until the first is submitted, with or without a fence
static void TestOneThreadRecordsAtOnce(void) {
    int round;

    for (round = 0; round < RECORDING_ROUNDS; round++) {
        SDL_GPUCommandBuffer *commands = BeginGpuCommands();
        SDL_Thread *second;
        int hasSecondSubmitted = 0;

        EXPECT(commands != NULL);
        SDL_SetAtomicInt(&f_HasSecondBegun, 0);
        second = SDL_CreateThread(BeginSecondCommands, "second", NULL);
        EXPECT(second != NULL);
        SDL_Delay(RECORDING_WAIT_MS);
        EXPECT(SDL_GetAtomicInt(&f_HasSecondBegun) == 0);
        if ((round % 2) == 0) {
            EXPECT(SubmitGpuCommands(commands));
        }
        else {
            SDL_GPUFence *fence = SubmitGpuCommandsWithFence(commands);

            EXPECT(fence != NULL);
            SDL_ReleaseGPUFence(f_Device, fence);
        }

        SDL_WaitThread(second, &hasSecondSubmitted);
        EXPECT(SDL_GetAtomicInt(&f_HasSecondBegun) == 1);
        EXPECT(hasSecondSubmitted == 1);
    }
}

static SDL_GPUDevice *CreateDevice(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        // A machine with no display can still have a GPU
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            return NULL;
        }
    }

    return CreateGpuDevice(true);
}

int main(void) {
    SDL_GPUDevice *device = CreateDevice();

    f_Device = device;
    if (device == NULL) {
        printf("gpu_backend_test: skipped, with no GPU: %s\n", SDL_GetError());
        return SKIPPED_EXIT_CODE;
    }

    MakeDiagonal(&f_Identity, 0x10000);
    MakeDiagonal(&f_Shrink, 0x10000 / 128);
    MakeMatrix(
        &f_Recede,
        (const float[4][4]) {
            {1.0f / 128.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f / 128.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f / 64.0f},
            {0.0f, 0.0f, 0.0f, 1.0f},
        }
    );
    InitGpuBackend(device, 2);
    InitRdp();
    InitRsp();
    TestFill();
    TestShadedTriangles();
    TestCopiedTexture();
    TestTranslucentBlend();
    TestDepth();
    TestPartialDepthFill();
    TestSecondCycleTexel();
    TestTwoCycleAlphaCompare();
    TestPerspectiveTexture();
    TestShadeClippedAtTwoEdges();
    TestOneThreadRecordsAtOnce();
    ShutdownRdp();
    ShutdownGpuBackend();

    // Upscaled, a pixel in memory averages the target's, so the N64's own
    // sampling shows only at its resolution
    InitGpuBackend(device, 1);
    InitRdp();
    TestFilteredTexelsOneForOne();
    ShutdownRdp();
    ShutdownGpuBackend();

    // At the player's usual scale, each pixel of memory spans six of the
    // target's
    InitGpuBackend(device, 6);
    InitRdp();
    TestUpscaledTexelsStartAtTheirCorner();
    TestUpscaledTexelsKeepFineSteps();
    ShutdownRdp();
    ShutdownGpuBackend();
    SDL_DestroyGPUDevice(device);
    SDL_Quit();
    return ReportTestResults("gpu_backend_test");
}
