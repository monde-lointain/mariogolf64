/*
 * The presenter. Each retrace it blits one texture to the swapchain: the
 * renderer's target for the frame buffer, at the renderer's scale, or a
 * texture of the screen's size that it fills from memory when the CPU drew
 * the frame. The blit clears the bars in the same pass. Shapes such as the
 * touch controls go over it, in a pass of their own.
 *
 * The presenter doesn't wait for the swapchain: when the GPU is still busy
 * with earlier frames, it skips the frame, so that the retraces, which
 * drive the game, keep their pace.
 */

#include "presenter.h"

#include "color_images.h"
#include "rcp.h"
#include "sdl_gpu_backend.h"
#include "shape_overlay.h"

#define SCREEN_PIXELS (SCREEN_WIDTH * SCREEN_HEIGHT)
#define BYTES_PER_TEXEL 4

static SDL_GPUDevice *f_Device;
static SDL_Window *f_Window;
static SDL_GPUTexture *f_MemoryScreen; // The CPU's frames
static SDL_GPUTransferBuffer *f_Upload;
static ScreenFit f_Fit;

SDL_Rect FitScreenToWindow(uint32_t windowWidth, uint32_t windowHeight, ScreenFit fit) {
    const uint32_t multiple = SDL_min(windowWidth / SCREEN_WIDTH, windowHeight / SCREEN_HEIGHT);
    SDL_Rect area;

    if (fit == SCREEN_FIT_STRETCH) {
        area.w = (int)windowWidth;
        area.h = (int)windowHeight;
    }
    else if (fit == SCREEN_FIT_INTEGER && multiple != 0) {
        area.w = (int)(multiple * SCREEN_WIDTH);
        area.h = (int)(multiple * SCREEN_HEIGHT);
    }
    // Whichever side runs out first limits the screen
    else if ((uint64_t)windowWidth * SCREEN_HEIGHT <= (uint64_t)windowHeight * SCREEN_WIDTH) {
        area.w = (int)windowWidth;
        area.h = (int)(((uint64_t)windowWidth * SCREEN_HEIGHT) / SCREEN_WIDTH);
    }
    else {
        area.h = (int)windowHeight;
        area.w = (int)(((uint64_t)windowHeight * SCREEN_WIDTH) / SCREEN_HEIGHT);
    }

    area.x = ((int)windowWidth - area.w) / 2;
    area.y = ((int)windowHeight - area.h) / 2;
    return area;
}

uint32_t ChooseScreenScale(uint32_t displayWidth, uint32_t displayHeight, ScreenFit fit) {
    uint32_t scale;

    if (fit == SCREEN_FIT_INTEGER) {
        scale = SDL_min(displayWidth / SCREEN_WIDTH, displayHeight / SCREEN_HEIGHT);
    }
    else {
        scale = (displayHeight + SCREEN_HEIGHT - 1) / SCREEN_HEIGHT;
    }

    return SDL_max(scale, 1u);
}

SDL_GPUFilter PickScreenFilter(uint32_t sourceWidth, uint32_t sourceHeight, SDL_Rect area) {
    SDL_assert(sourceWidth != 0 && sourceHeight != 0);
    if ((uint32_t)area.w % sourceWidth == 0 && (uint32_t)area.h % sourceHeight == 0) {
        return SDL_GPU_FILTER_NEAREST;
    }

    return SDL_GPU_FILTER_LINEAR;
}

// Each 5-bit channel fills 8 bits by repeating its top bits, so that full
// stays full and zero stays zero. The pixels are read as bytes, as the VI
// reads them, since the game's CPU code reads the same memory as words too.
void ExpandScreenPixels(const void *pixels, uint32_t count, uint32_t *rgba) {
    const uint8_t *bytes = pixels;
    uint32_t i;

    for (i = 0; i < count; i++) {
        uint16_t pixel;
        uint32_t red;
        uint32_t green;
        uint32_t blue;

        SDL_memcpy(&pixel, &bytes[i * sizeof(pixel)], sizeof(pixel));
        red = (pixel >> 11) & 0x1F;
        green = (pixel >> 6) & 0x1F;
        blue = (pixel >> 1) & 0x1F;

        rgba[i] = ((red << 3) | (red >> 2)) | (((green << 3) | (green >> 2)) << 8) |
            (((blue << 3) | (blue >> 2)) << 16) | 0xFF000000u;
    }
}

SDL_GPUPresentMode PickPresentMode(SDL_GPUPresentMode wanted, bool hasMailbox, bool hasImmediate) {
    if (wanted == SDL_GPU_PRESENTMODE_IMMEDIATE && hasImmediate) {
        return SDL_GPU_PRESENTMODE_IMMEDIATE;
    }

    if (wanted != SDL_GPU_PRESENTMODE_VSYNC && hasMailbox) {
        return SDL_GPU_PRESENTMODE_MAILBOX;
    }

    return SDL_GPU_PRESENTMODE_VSYNC;
}

bool InitPresenter(
    SDL_GPUDevice *device,
    SDL_Window *window,
    SDL_GPUPresentMode wanted,
    ScreenFit fit
) {
    SDL_GPUPresentMode mode;

    SDL_GPUTextureCreateInfo texture;
    SDL_GPUTransferBufferCreateInfo upload;

    SDL_assert(device != NULL && window != NULL);
    f_Device = device;
    f_Window = window;
    f_Fit = fit;
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("Can't show the GPU's frames in the window: %s", SDL_GetError());
        return false;
    }

    mode = PickPresentMode(
        wanted,
        SDL_WindowSupportsGPUPresentMode(device, window, SDL_GPU_PRESENTMODE_MAILBOX),
        SDL_WindowSupportsGPUPresentMode(device, window, SDL_GPU_PRESENTMODE_IMMEDIATE)
    );
    if (mode != wanted) {
        SDL_Log(
            "The display can't present as asked; presenting %s instead",
            (mode == SDL_GPU_PRESENTMODE_MAILBOX) ? "by mailbox" : "with vsync"
        );
    }

    if (!SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode)) {
        SDL_Log("Can't set the swapchain up: %s", SDL_GetError());
    }

    SDL_zero(texture);
    texture.type = SDL_GPU_TEXTURETYPE_2D;
    texture.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture.width = SCREEN_WIDTH;
    texture.height = SCREEN_HEIGHT;
    texture.layer_count_or_depth = 1;
    texture.num_levels = 1;
    f_MemoryScreen = SDL_CreateGPUTexture(device, &texture);
    SDL_zero(upload);
    upload.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    upload.size = SCREEN_PIXELS * BYTES_PER_TEXEL;
    f_Upload = SDL_CreateGPUTransferBuffer(device, &upload);
    if (f_MemoryScreen == NULL || f_Upload == NULL) {
        SDL_Log("Can't create the presenter's texture: %s", SDL_GetError());
        ShutdownPresenter();
        return false;
    }

    if (!InitShapeOverlay(device, SDL_GetGPUSwapchainTextureFormat(device, window))) {
        ShutdownPresenter();
        return false;
    }

    return true;
}

void ShutdownPresenter(void) {
    if (f_Device == NULL) {
        return;
    }

    ShutdownShapeOverlay();
    SDL_ReleaseGPUTransferBuffer(f_Device, f_Upload);
    SDL_ReleaseGPUTexture(f_Device, f_MemoryScreen);
    SDL_ReleaseWindowFromGPUDevice(f_Device, f_Window);
    f_Upload = NULL;
    f_MemoryScreen = NULL;
    f_Device = NULL;
    f_Window = NULL;
}

// Copies the CPU's frame into the memory screen's texture
static void UploadMemoryScreen(SDL_GPUCommandBuffer *commands, const void *framebuffer) {
    uint32_t *texels = SDL_MapGPUTransferBuffer(f_Device, f_Upload, true);
    SDL_GPUTextureTransferInfo source;
    SDL_GPUTextureRegion destination;
    SDL_GPUCopyPass *pass;

    if (texels == NULL) {
        return;
    }

    ExpandScreenPixels(framebuffer, SCREEN_PIXELS, texels);
    SDL_UnmapGPUTransferBuffer(f_Device, f_Upload);
    SDL_zero(source);
    source.transfer_buffer = f_Upload;
    SDL_zero(destination);
    destination.texture = f_MemoryScreen;
    destination.w = SCREEN_WIDTH;
    destination.h = SCREEN_HEIGHT;
    destination.d = 1;
    pass = SDL_BeginGPUCopyPass(commands);
    SDL_UploadToGPUTexture(pass, &source, &destination, true);
    SDL_EndGPUCopyPass(pass);
}

static void BlitScreen(
    SDL_GPUCommandBuffer *commands,
    SDL_GPUTexture *swapchain,
    uint32_t swapchainWidth,
    uint32_t swapchainHeight,
    SDL_GPUTexture *screen,
    uint32_t screenWidth,
    uint32_t screenHeight
) {
    const SDL_Rect area = FitScreenToWindow(swapchainWidth, swapchainHeight, f_Fit);
    SDL_GPUBlitInfo blit;

    SDL_zero(blit);
    blit.source.texture = screen;
    blit.source.w = screenWidth;
    blit.source.h = screenHeight;
    blit.destination.texture = swapchain;
    blit.destination.x = (Uint32)area.x;
    blit.destination.y = (Uint32)area.y;
    blit.destination.w = (Uint32)area.w;
    blit.destination.h = (Uint32)area.h;
    blit.load_op = SDL_GPU_LOADOP_CLEAR;
    blit.clear_color.a = 1.0f;
    blit.filter = PickScreenFilter(screenWidth, screenHeight, area);
    SDL_BlitGPUTexture(commands, &blit);
}

// Clears the swapchain to black, for a black screen or an empty window
static void ClearSwapchain(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *swapchain) {
    SDL_GPUColorTargetInfo target;

    SDL_zero(target);
    target.texture = swapchain;
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    target.clear_color.a = 1.0f;
    SDL_EndGPURenderPass(SDL_BeginGPURenderPass(commands, &target, 1, NULL));
}

// The renderer's target that shows the frame buffer
typedef struct ShownTarget {
    GfxTarget *target; // NULL where the screen comes from memory
    uint16_t width;
    uint16_t height;
} ShownTarget;

// Draws the frame buffer, or black for NULL, into the swapchain, from the
// shown target where there is one
static void DrawScreen(
    SDL_GPUCommandBuffer *commands,
    SDL_GPUTexture *swapchain,
    uint32_t width,
    uint32_t height,
    const void *framebuffer,
    const ShownTarget *shown
) {
    if (framebuffer == NULL) {
        ClearSwapchain(commands, swapchain);
    }
    else if (shown->target != NULL) {
        const uint32_t scale = GpuBackendScale();

        BlitScreen(
            commands,
            swapchain,
            width,
            height,
            GpuTargetTexture(shown->target),
            shown->width * scale,
            shown->height * scale
        );
    }
    else {
        UploadMemoryScreen(commands, framebuffer);
        BlitScreen(commands, swapchain, width, height, f_MemoryScreen, SCREEN_WIDTH, SCREEN_HEIGHT);
    }
}

void PresentFramebuffer(const void *framebuffer, const OverlayShape *shapes, int shapeCount) {
    SDL_GPUCommandBuffer *commands;
    SDL_GPUTexture *swapchain = NULL;
    Uint32 width = 0;
    Uint32 height = 0;
    ShownTarget shown;

    SDL_assert(f_Device != NULL);
    // The shown target's lock keeps the renderer from releasing the target
    // until the blit that reads it is submitted. It's taken before the
    // command buffer, as the renderer takes it before its own.
    SDL_zero(shown);
    if (framebuffer != NULL) {
        shown.target = LockShownTarget(framebuffer, &shown.width, &shown.height);
        if (shown.target == NULL) {
            UnlockShownTarget();
        }
    }

    commands = BeginGpuCommands();
    if (commands == NULL) {
        SDL_Log("Can't present: %s", SDL_GetError());
    }
    else if (!SDL_AcquireGPUSwapchainTexture(commands, f_Window, &swapchain, &width, &height) ||
        swapchain == NULL) {
        // Minimized, or the GPU is behind: nothing to show this time
        SubmitGpuCommands(commands);
    }
    else {
        DrawScreen(commands, swapchain, width, height, framebuffer, &shown);
        DrawShapeOverlay(commands, swapchain, width, height, shapes, shapeCount);
        SubmitGpuCommands(commands);
    }

    if (shown.target != NULL) {
        UnlockShownTarget();
    }
}

bool SaveScreenImage(const void *framebuffer, const char *path) {
    SDL_Surface *image;
    bool isSaved;

    SDL_assert(path != NULL);
    if (framebuffer == NULL) {
        SDL_Log("No screen to save: the screen is black");
        return false;
    }

    image = SDL_CreateSurface(SCREEN_WIDTH, SCREEN_HEIGHT, SDL_PIXELFORMAT_RGBA32);
    if (image == NULL) {
        SDL_Log("Can't save the screen: %s", SDL_GetError());
        return false;
    }

    SyncRdpMemory(framebuffer, SCREEN_PIXELS * sizeof(uint16_t));
    SDL_assert(image->pitch == SCREEN_WIDTH * BYTES_PER_TEXEL);
    ExpandScreenPixels(framebuffer, SCREEN_PIXELS, image->pixels);
    isSaved = SDL_SavePNG(image, path);
    if (!isSaved) {
        SDL_Log("Can't save the screen to %s: %s", path, SDL_GetError());
    }

    SDL_DestroySurface(image);
    return isSaved;
}
