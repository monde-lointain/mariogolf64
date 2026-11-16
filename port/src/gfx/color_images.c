/*
 * The game's color and depth images and their render targets.
 */

#include "color_images.h"

#include <PR/gbi.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_stdinc.h>

#include "gfx_backend.h"
#include "hash.h"
#include "port/os_host.h"
#include "tmem.h"

#define COLOR_IMAGE_COUNT_MAX 32
#define DEPTH_IMAGE_COUNT_MAX 8

struct ColorImage {
    uint8_t *address; // NULL for an unused entry
    uint16_t width;
    uint16_t height;
    uint8_t bytesPerPixel;
    bool isMemoryBehind; // The GPU has drawn what memory lacks
    uint64_t memoryHash; // Of the memory, when it last agreed with the target
    uint64_t lastUse;
    GfxTarget *target;
};

typedef struct DepthImage {
    const uint8_t *address;
    uint16_t width;
    uint16_t height;
    GfxTarget *target;
} DepthImage;

static ColorImage f_ColorImages[COLOR_IMAGE_COUNT_MAX];
static DepthImage f_DepthImages[DEPTH_IMAGE_COUNT_MAX];
static SDL_Mutex *f_Lock;
static uint64_t f_UseCount;
// The pixels passing between memory and a target, at the largest size
static uint32_t *f_Pixels;
static uint32_t f_PixelCapacity;

void InitColorImages(void) {
    SDL_zeroa(f_ColorImages);
    SDL_zeroa(f_DepthImages);
    if (f_Lock == NULL) {
        f_Lock = SDL_CreateMutex();
        if (f_Lock == NULL) {
            PcFatalError("Can't create the color images' lock: %s", SDL_GetError());
        }
    }
}

static uint32_t *PixelBuffer(uint32_t count) {
    if (count > f_PixelCapacity) {
        SDL_free(f_Pixels);
        f_Pixels = SDL_malloc(count * sizeof(uint32_t));
        if (f_Pixels == NULL) {
            PcFatalError("Out of memory for %u pixels", count);
        }

        f_PixelCapacity = count;
    }

    return f_Pixels;
}

static uintptr_t ImageByteCount(const ColorImage *image) {
    return (uintptr_t)image->width * image->height * image->bytesPerPixel;
}

static uint64_t HashImageMemory(const ColorImage *image) {
    return FinishHash(HashBytes(HASH_SEED, image->address, ImageByteCount(image)));
}

// The memory's pixels as RGBA8, red lowest. RGBA16 keeps one bit of alpha,
// its coverage bit.
static void ReadMemoryPixels(const ColorImage *image, uint32_t *rgba) {
    uint32_t count = (uint32_t)image->width * image->height;
    const uint8_t *bytes = image->address;
    uint32_t i;

    if (image->bytesPerPixel == 2) {
        for (i = 0; i < count; i++) {
            rgba[i] = ExpandRgba16((uint16_t)((bytes[i * 2] << 8) | bytes[(i * 2) + 1]));
        }
    }
    else {
        for (i = 0; i < count; i++) {
            const uint8_t *pixel = &bytes[i * 4];

            rgba[i] = pixel[0] | (pixel[1] << 8) | (pixel[2] << 16) | ((uint32_t)pixel[3] << 24);
        }
    }
}

static void WriteMemoryPixels(ColorImage *image, const uint32_t *rgba) {
    uint32_t count = (uint32_t)image->width * image->height;
    uint8_t *bytes = image->address;
    uint32_t i;

    if (image->bytesPerPixel == 2) {
        for (i = 0; i < count; i++) {
            uint32_t pixel = rgba[i];
            uint32_t texel = ((pixel & 0xF8) << 8) | ((pixel & 0xF800) >> 5) |
                ((pixel & 0xF80000) >> 18) | ((pixel >> 31) & 1);

            bytes[i * 2] = (uint8_t)(texel >> 8);
            bytes[(i * 2) + 1] = (uint8_t)texel;
        }
    }
    else {
        for (i = 0; i < count; i++) {
            uint32_t pixel = rgba[i];

            bytes[i * 4] = (uint8_t)pixel;
            bytes[(i * 4) + 1] = (uint8_t)(pixel >> 8);
            bytes[(i * 4) + 2] = (uint8_t)(pixel >> 16);
            bytes[(i * 4) + 3] = (uint8_t)(pixel >> 24);
        }
    }
}

static void UploadMemory(ColorImage *image) {
    uint32_t *pixels = PixelBuffer((uint32_t)image->width * image->height);

    ReadMemoryPixels(image, pixels);
    BackendWriteTarget(image->target, image->width, image->height, pixels);
    image->memoryHash = HashImageMemory(image);
    image->isMemoryBehind = false;
}

static ColorImage *FindColorImage(const uint8_t *address, uint16_t width, int bytesPerPixel) {
    int i;

    for (i = 0; i < COLOR_IMAGE_COUNT_MAX; i++) {
        ColorImage *image = &f_ColorImages[i];

        if (image->address == address &&
            image->width == width &&
            image->bytesPerPixel == bytesPerPixel) {
            return image;
        }
    }

    return NULL;
}

static int BytesPerPixel(int size) {
    return size == G_IM_SIZ_32b ? 4 : 2;
}

bool IsColorImageCurrent(const uint8_t *address, uint16_t width, uint16_t height, int size) {
    ColorImage *image = FindColorImage(address, width, BytesPerPixel(size));

    return image != NULL && image->height >= height && HashImageMemory(image) == image->memoryHash;
}

static void DownloadTarget(ColorImage *image) {
    uint32_t *pixels = PixelBuffer((uint32_t)image->width * image->height);

    BackendReadTarget(image->target, image->width, image->height, pixels);
    WriteMemoryPixels(image, pixels);
    image->memoryHash = HashImageMemory(image);
    image->isMemoryBehind = false;
}

// An unused entry, or else the least recently used one, whose drawing goes
// back to memory first. The caller has flushed the frame, so no draw still
// waits on the evicted target.
static ColorImage *FreeColorImage(void) {
    ColorImage *oldest = &f_ColorImages[0];
    int i;

    for (i = 0; i < COLOR_IMAGE_COUNT_MAX; i++) {
        ColorImage *image = &f_ColorImages[i];

        if (image->address == NULL) {
            return image;
        }

        if (image->lastUse < oldest->lastUse) {
            oldest = image;
        }
    }

    if (oldest->isMemoryBehind) {
        DownloadTarget(oldest);
    }

    BackendDestroyTarget(oldest->target);
    SDL_zerop(oldest);
    return oldest;
}

static bool DoRangesOverlap(
    const uint8_t *start,
    uintptr_t size,
    const uint8_t *otherStart,
    uintptr_t otherSize
) {
    return start < otherStart + otherSize && otherStart < start + size;
}

// Drops the images that a new one's memory overlaps: the game has given
// their memory to the new one
static void DropOverlappedImages(const uint8_t *address, uintptr_t size, const ColorImage *kept) {
    int i;

    for (i = 0; i < COLOR_IMAGE_COUNT_MAX; i++) {
        ColorImage *image = &f_ColorImages[i];

        if (image != kept &&
            image->address != NULL &&
            DoRangesOverlap(address, size, image->address, ImageByteCount(image))) {
            BackendDestroyTarget(image->target);
            SDL_zerop(image);
        }
    }
}

ColorImage *UseColorImage(uint8_t *address, uint16_t width, uint16_t height, int size) {
    int bytesPerPixel = BytesPerPixel(size);
    ColorImage *image;

    SDL_assert(address != NULL && width != 0 && height != 0);
    SDL_LockMutex(f_Lock);
    image = FindColorImage(address, width, bytesPerPixel);
    if (image == NULL) {
        image = FreeColorImage();
        image->address = address;
        image->width = width;
        image->height = height;
        image->bytesPerPixel = (uint8_t)bytesPerPixel;
        DropOverlappedImages(address, ImageByteCount(image), image);
        image->target = BackendCreateTarget(width, height, false);
        UploadMemory(image);
    }
    else if (image->height < height) {
        GfxTarget *grown = BackendCreateTarget(width, height, false);

        BackendCopyTarget(image->target, grown, width, image->height);
        BackendDestroyTarget(image->target);
        image->target = grown;
        image->height = height;
        DropOverlappedImages(address, ImageByteCount(image), image);
        UploadMemory(image);
    }
    else if (HashImageMemory(image) != image->memoryHash) {
        UploadMemory(image);
    }

    image->lastUse = ++f_UseCount;
    SDL_UnlockMutex(f_Lock);
    return image;
}

GfxTarget *ColorImageTarget(const ColorImage *image) {
    return image->target;
}

uint16_t ColorImageWidth(const ColorImage *image) {
    return image->width;
}

uint16_t ColorImageHeight(const ColorImage *image) {
    return image->height;
}

void MarkColorImageDrawn(ColorImage *image) {
    image->isMemoryBehind = true;
}

GfxTarget *UseDepthImage(const uint8_t *address, const ColorImage *colorImage) {
    DepthImage *free = NULL;
    int i;

    for (i = 0; i < DEPTH_IMAGE_COUNT_MAX; i++) {
        DepthImage *depth = &f_DepthImages[i];

        if (depth->target != NULL &&
            depth->address == address &&
            depth->width == colorImage->width &&
            depth->height == colorImage->height) {
            return depth->target;
        }

        if (depth->target == NULL && free == NULL) {
            free = depth;
        }
    }

    if (free == NULL) {
        // The oldest depth image goes: its contents belong to a frame long
        // drawn
        free = &f_DepthImages[0];
        BackendDestroyTarget(free->target);
        SDL_memmove(&f_DepthImages[0], &f_DepthImages[1], sizeof(f_DepthImages) - sizeof(*free));
        free = &f_DepthImages[DEPTH_IMAGE_COUNT_MAX - 1];
    }

    free->address = address;
    free->width = colorImage->width;
    free->height = colorImage->height;
    free->target = BackendCreateTarget(colorImage->width, colorImage->height, true);
    return free->target;
}

bool HasUnsyncedColorImage(const uint8_t *start, uintptr_t size) {
    int i;

    for (i = 0; i < COLOR_IMAGE_COUNT_MAX; i++) {
        const ColorImage *image = &f_ColorImages[i];

        if (image->address != NULL &&
            image->isMemoryBehind &&
            DoRangesOverlap(start, size, image->address, ImageByteCount(image))) {
            return true;
        }
    }

    return false;
}

void SyncColorImagesToMemory(const uint8_t *start, uintptr_t size) {
    int i;

    SDL_LockMutex(f_Lock);
    for (i = 0; i < COLOR_IMAGE_COUNT_MAX; i++) {
        ColorImage *image = &f_ColorImages[i];

        if (image->address != NULL &&
            image->isMemoryBehind &&
            DoRangesOverlap(start, size, image->address, ImageByteCount(image))) {
            DownloadTarget(image);
        }
    }

    SDL_UnlockMutex(f_Lock);
}

GfxTarget *LockShownTarget(const uint8_t *address, uint16_t *width, uint16_t *height) {
    int i;

    SDL_LockMutex(f_Lock);
    for (i = 0; i < COLOR_IMAGE_COUNT_MAX; i++) {
        ColorImage *image = &f_ColorImages[i];

        // A target that the CPU has drawn over since it last agreed with
        // memory isn't what the game shows
        if (image->address == address &&
            (image->isMemoryBehind || HashImageMemory(image) == image->memoryHash)) {
            *width = image->width;
            *height = image->height;
            return image->target;
        }
    }

    return NULL;
}

void UnlockShownTarget(void) {
    SDL_UnlockMutex(f_Lock);
}
