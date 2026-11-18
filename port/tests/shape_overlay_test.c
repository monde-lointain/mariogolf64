/*
 * Tests of the shape overlay on a real GPU: shapes drawn over an opaque
 * black target, whose pixels the test reads back. Without a GPU the test
 * is skipped.
 */

#include <SDL3/SDL.h>

#include "sdl_gpu_backend.h"
#include "shape_overlay.h"
#include "test_support.h"

// Wider than tall, so that the shapes keep to the target's pixels
#define TARGET_WIDTH 96
#define TARGET_HEIGHT 64
#define CENTER 32.0f
#define RADIUS 24.0f
// CTest's code for a skipped test
#define SKIPPED_EXIT_CODE 77
// How far a channel may stray from its expected value
#define CHANNEL_TOLERANCE 3

typedef struct Pixel {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a;
} Pixel;

static SDL_GPUDevice *f_Device;
static SDL_GPUTexture *f_Target;
static SDL_GPUTransferBuffer *f_Download;
static Pixel f_Pixels[TARGET_WIDTH * TARGET_HEIGHT];
static float f_Background; // The gray that the target is cleared to

// RGBA8, red in the lowest byte, as OverlayShape takes it
static uint32_t PackColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

static OverlayShape MakeShape(
    float x,
    float y,
    uint32_t color,
    OverlayStyle style,
    OverlayLabel label
) {
    OverlayShape shape;

    SDL_zero(shape);
    shape.x = x;
    shape.y = y;
    shape.radius = RADIUS;
    shape.color = color;
    shape.style = (uint8_t)style;
    shape.label = (uint8_t)label;
    return shape;
}

// Clears the target to opaque black, draws the shapes over it, and reads
// it back into f_Pixels
static void DrawShapes(const OverlayShape *shapes, int count) {
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(f_Device);
    SDL_GPUColorTargetInfo clear;
    SDL_GPURenderPass *pass;
    SDL_GPUCopyPass *copy;
    SDL_GPUTextureRegion region;
    SDL_GPUTextureTransferInfo transfer;
    SDL_GPUFence *fence;
    void *mapped;

    SDL_zero(clear);
    clear.texture = f_Target;
    clear.clear_color.r = f_Background;
    clear.clear_color.g = f_Background;
    clear.clear_color.b = f_Background;
    clear.clear_color.a = 1.0f;
    clear.load_op = SDL_GPU_LOADOP_CLEAR;
    clear.store_op = SDL_GPU_STOREOP_STORE;
    pass = SDL_BeginGPURenderPass(commands, &clear, 1, NULL);
    SDL_EndGPURenderPass(pass);
    DrawShapeOverlay(commands, f_Target, TARGET_WIDTH, TARGET_HEIGHT, shapes, count);

    SDL_zero(region);
    region.texture = f_Target;
    region.w = TARGET_WIDTH;
    region.h = TARGET_HEIGHT;
    region.d = 1;
    SDL_zero(transfer);
    transfer.transfer_buffer = f_Download;
    copy = SDL_BeginGPUCopyPass(commands);
    SDL_DownloadFromGPUTexture(copy, &region, &transfer);
    SDL_EndGPUCopyPass(copy);
    fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    EXPECT(fence != NULL);
    EXPECT(SDL_WaitForGPUFences(f_Device, true, &fence, 1));
    SDL_ReleaseGPUFence(f_Device, fence);
    mapped = SDL_MapGPUTransferBuffer(f_Device, f_Download, false);
    EXPECT(mapped != NULL);
    if (mapped != NULL) {
        SDL_memcpy(f_Pixels, mapped, sizeof(f_Pixels));
        SDL_UnmapGPUTransferBuffer(f_Device, f_Download);
    }
}

static Pixel PixelAt(int x, int y) {
    return f_Pixels[(y * TARGET_WIDTH) + x];
}

static bool IsNear(int actual, int expected) {
    return SDL_abs(actual - expected) <= CHANNEL_TOLERANCE;
}

static bool IsColor(Pixel pixel, int r, int g, int b) {
    return IsNear(pixel.r, r) && IsNear(pixel.g, g) && IsNear(pixel.b, b);
}

// Nothing to draw leaves the target alone
static void TestNoShapes(void) {
    DrawShapes(NULL, 0);
    EXPECT(IsColor(PixelAt(32, 32), 0, 0, 0));
}

// A disc blends its color by its alpha inside, with a rim opaquer and
// whiter, and leaves the outside alone; the target's top is the screen's
static void TestDisc(void) {
    const OverlayShape disc = MakeShape(
        CENTER,
        CENTER,
        PackColor(0xFF, 0, 0, 0x80),
        OVERLAY_STYLE_DISC,
        OVERLAY_LABEL_NONE
    );
    const OverlayShape high = MakeShape(
        16.0f,
        16.0f,
        PackColor(0, 0xFF, 0, 0xFF),
        OVERLAY_STYLE_DISC,
        OVERLAY_LABEL_NONE
    );
    Pixel rim;

    DrawShapes(&disc, 1);
    // Red at 0x80 of 0xFF over black
    EXPECT(IsColor(PixelAt(32, 32), 0x80, 0, 0));
    EXPECT(IsColor(PixelAt(32, 10), 0x80, 0, 0));
    // Outside, beyond its smoothed edge
    EXPECT(IsColor(PixelAt(2, 2), 0, 0, 0));
    EXPECT(IsColor(PixelAt(32, 32 + 26), 0, 0, 0));
    // The rim, 22.5 pixels out of 24
    rim = PixelAt(54, 32);
    EXPECT(rim.r > 0x80 + 0x30 && rim.g > 0x30 && rim.g == rim.b);

    DrawShapes(&high, 1);
    EXPECT(IsColor(PixelAt(16, 16), 0, 0xFF, 0));
    EXPECT(IsColor(PixelAt(16, 48), 0, 0, 0));
    EXPECT(IsColor(PixelAt(48, 16), 0, 0, 0));
}

// A ring is the rim alone; the shapes draw in their order
static void TestRingOverDisc(void) {
    OverlayShape shapes[2];
    Pixel rim;

    shapes[0] = MakeShape(
        CENTER,
        CENTER,
        PackColor(0, 0, 0xFF, 0xFF),
        OVERLAY_STYLE_DISC,
        OVERLAY_LABEL_NONE
    );
    shapes[1] = MakeShape(
        CENTER,
        CENTER,
        PackColor(0xFF, 0, 0, 0x60),
        OVERLAY_STYLE_RING,
        OVERLAY_LABEL_NONE
    );
    DrawShapes(&shapes[1], 1);
    EXPECT(IsColor(PixelAt(32, 32), 0, 0, 0));
    rim = PixelAt(32, 32 - 23);
    EXPECT(rim.r > 0x60 && rim.g > 0x10);

    DrawShapes(shapes, 2);
    EXPECT(IsColor(PixelAt(32, 32), 0, 0, 0xFF));
    rim = PixelAt(9, 32);
    EXPECT(rim.r > 0x60 && rim.b < 0xFF);
}

static bool IsLabelWhite(Pixel pixel) {
    return pixel.r > 0xA0 && pixel.r == pixel.g && pixel.g == pixel.b;
}

// A letter's set cells are white, its clear ones the disc's fill: the
// middle of A's crossbar and its top left corner, and the right end of Z's
// second row and its left end. A cell is 0.9 / 7 radii, 3.09 pixels.
static void TestLetters(void) {
    OverlayShape letter =
        MakeShape(CENTER, CENTER, PackColor(0, 0, 0xFF, 0x60), OVERLAY_STYLE_DISC, OVERLAY_LABEL_A);

    DrawShapes(&letter, 1);
    EXPECT(IsLabelWhite(PixelAt(32, 31)));
    EXPECT(IsColor(PixelAt(25, 22), 0, 0, 0x60));

    letter.label = OVERLAY_LABEL_Z;
    DrawShapes(&letter, 1);
    EXPECT(IsLabelWhite(PixelAt(37, 25)));
    EXPECT(IsColor(PixelAt(25, 25), 0, 0, 0x60));
}

// The edge is smoothed over a pixel: a pixel whose center is a quarter of
// a pixel outside the circle is a quarter covered, and one a quarter
// inside, three quarters
static void TestSmoothEdge(void) {
    const uint32_t white = PackColor(0xFF, 0xFF, 0xFF, 0xFF);
    // Pixel 56's center is at 56.5, as row 32's is at 32.5
    const OverlayShape outside =
        MakeShape(32.25f, 32.5f, white, OVERLAY_STYLE_DISC, OVERLAY_LABEL_NONE);
    const OverlayShape inside =
        MakeShape(32.75f, 32.5f, white, OVERLAY_STYLE_DISC, OVERLAY_LABEL_NONE);

    DrawShapes(&outside, 1);
    EXPECT(IsColor(PixelAt(56, 32), 0x40, 0x40, 0x40));
    DrawShapes(&inside, 1);
    EXPECT(IsColor(PixelAt(56, 32), 0xBF, 0xBF, 0xBF));
}

// What the target held stays beside the shapes: the frame they go over
static void TestKeepsTarget(void) {
    const OverlayShape corner = MakeShape(
        8.0f,
        8.0f,
        PackColor(0xFF, 0xFF, 0xFF, 0xFF),
        OVERLAY_STYLE_DISC,
        OVERLAY_LABEL_NONE
    );

    f_Background = 0.5f;
    DrawShapes(&corner, 1);
    f_Background = 0.0f;
    EXPECT(IsColor(PixelAt(8, 8), 0xFF, 0xFF, 0xFF));
    EXPECT(IsColor(PixelAt(48, 48), 0x80, 0x80, 0x80));
}

static int MeasureWhite(Pixel pixel) {
    return pixel.r + pixel.g;
}

// An arrow points its label's way: up, toward the target's top, for
// ARROW_UP. Its base is 0.3 radii from the center, so a pixel behind it,
// whose center is 1.3 pixels past the base, is the fill alone.
static void TestArrows(void) {
    static const struct {
        OverlayLabel label;
        int aheadX;
        int aheadY;
        int behindX;
        int behindY;
    } arrows[] = {
        {OVERLAY_LABEL_ARROW_UP, 32, 26, 32, 40},
        {OVERLAY_LABEL_ARROW_DOWN, 32, 38, 32, 23},
        {OVERLAY_LABEL_ARROW_LEFT, 26, 32, 40, 32},
        {OVERLAY_LABEL_ARROW_RIGHT, 38, 32, 23, 32},
    };

    size_t i;

    for (i = 0; i < SDL_arraysize(arrows); i++) {
        const OverlayShape shape = MakeShape(
            CENTER,
            CENTER,
            PackColor(0, 0, 0xFF, 0x60),
            OVERLAY_STYLE_DISC,
            arrows[i].label
        );
        Pixel ahead;
        Pixel behind;

        DrawShapes(&shape, 1);
        ahead = PixelAt(arrows[i].aheadX, arrows[i].aheadY);
        behind = PixelAt(arrows[i].behindX, arrows[i].behindY);
        EXPECT(MeasureWhite(ahead) > 0x140);
        EXPECT(IsColor(behind, 0, 0, 0x60));
        EXPECT(MeasureWhite(PixelAt(32, 32)) > 0x140);
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
    SDL_GPUTextureCreateInfo texture;
    SDL_GPUTransferBufferCreateInfo download;

    f_Device = CreateDevice();
    if (f_Device == NULL) {
        printf("shape_overlay_test: skipped, with no GPU: %s\n", SDL_GetError());
        return SKIPPED_EXIT_CODE;
    }

    SDL_zero(texture);
    texture.type = SDL_GPU_TEXTURETYPE_2D;
    texture.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    texture.width = TARGET_WIDTH;
    texture.height = TARGET_HEIGHT;
    texture.layer_count_or_depth = 1;
    texture.num_levels = 1;
    f_Target = SDL_CreateGPUTexture(f_Device, &texture);
    SDL_zero(download);
    download.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    download.size = sizeof(f_Pixels);
    f_Download = SDL_CreateGPUTransferBuffer(f_Device, &download);
    EXPECT(f_Target != NULL && f_Download != NULL);
    EXPECT(InitShapeOverlay(f_Device, texture.format));

    TestNoShapes();
    TestDisc();
    TestRingOverDisc();
    TestLetters();
    TestSmoothEdge();
    TestKeepsTarget();
    TestArrows();

    ShutdownShapeOverlay();
    SDL_ReleaseGPUTransferBuffer(f_Device, f_Download);
    SDL_ReleaseGPUTexture(f_Device, f_Target);
    SDL_DestroyGPUDevice(f_Device);
    SDL_Quit();
    return ReportTestResults("shape_overlay_test");
}
