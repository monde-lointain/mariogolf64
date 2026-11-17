/*
 * The game's color and depth images and their render targets.
 */

#include "color_images.h"

#include <PR/gbi.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_endian.h>
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
    uint16_t height; // The rows of memory, as far as drawing has reached
    uint16_t targetHeight; // The target's rows, at least height
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

void ReleaseColorImages(void) {
    int i;

    SDL_LockMutex(f_Lock);
    for (i = 0; i < COLOR_IMAGE_COUNT_MAX; i++) {
        BackendDestroyTarget(f_ColorImages[i].target);
    }

    for (i = 0; i < DEPTH_IMAGE_COUNT_MAX; i++) {
        BackendDestroyTarget(f_DepthImages[i].target);
    }

    SDL_zeroa(f_ColorImages);
    SDL_zeroa(f_DepthImages);
    SDL_UnlockMutex(f_Lock);
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
// The game's CPU code reads and writes its images as host words: a 16-bit
// pixel is 5-5-5-1 from red, and a 32-bit one red to alpha from the top
// byte. The renderer's pixels are RGBA8 bytes.
static void ReadMemoryPixels(
    const ColorImage *image,
    uint32_t firstRow,
    uint32_t rowCount,
    uint32_t *rgba
) {
    uint32_t count = (uint32_t)image->width * rowCount;
    const uint8_t *bytes =
        image->address + ((uintptr_t)image->width * firstRow * image->bytesPerPixel);
    uint32_t i;

    if (image->bytesPerPixel == 2) {
        for (i = 0; i < count; i++) {
            uint16_t pixel;

            SDL_memcpy(&pixel, &bytes[i * 2], sizeof(pixel));
            rgba[i] = ExpandRgba16(pixel);
        }
    }
    else {
        for (i = 0; i < count; i++) {
            uint32_t pixel;

            SDL_memcpy(&pixel, &bytes[i * 4], sizeof(pixel));
            rgba[i] = SDL_Swap32(pixel);
        }
    }
}

static void WriteMemoryPixels(ColorImage *image, const uint32_t *rgba) {
    uint32_t count = (uint32_t)image->width * image->height;
    uint8_t *bytes = image->address;
    uint32_t i;

    if (image->bytesPerPixel == 2) {
        for (i = 0; i < count; i++) {
            uint32_t color = rgba[i];
            uint16_t pixel = (uint16_t)(((color & 0xF8) << 8) | ((color & 0xF800) >> 5) |
                ((color & 0xF80000) >> 18) | ((color >> 31) & 1));

            SDL_memcpy(&bytes[i * 2], &pixel, sizeof(pixel));
        }
    }
    else {
        for (i = 0; i < count; i++) {
            uint32_t pixel = SDL_Swap32(rgba[i]);

            SDL_memcpy(&bytes[i * 4], &pixel, sizeof(pixel));
        }
    }
}

// Replaces the target's rows from firstRow to the image's height with
// memory's
static void UploadMemoryRows(ColorImage *image, uint16_t firstRow) {
    uint32_t rowCount = (uint32_t)image->height - firstRow;
    uint32_t *pixels = PixelBuffer((uint32_t)image->width * rowCount);

    ReadMemoryPixels(image, firstRow, rowCount, pixels);
    BackendWriteTarget(image->target, image->width, firstRow, rowCount, pixels);
    image->memoryHash = HashImageMemory(image);
}

static void UploadMemory(ColorImage *image) {
    UploadMemoryRows(image, 0);
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

    return image != NULL &&
        image->targetHeight >= height &&
        HashImageMemory(image) == image->memoryHash;
}

static void DownloadTarget(ColorImage *image) {
    uint32_t *pixels = PixelBuffer((uint32_t)image->width * image->height);

    BackendReadTarget(image->target, image->width, image->height, pixels);
    WriteMemoryPixels(image, pixels);
    image->memoryHash = HashImageMemory(image);
    image->isMemoryBehind = false;
}

// Brings memory up to date with the GPU's drawing, unless the CPU has
// written the memory since it last agreed with the target. The CPU's writes
// are then the newer, as when the game loads data into a frame's memory, and
// the drawing is dropped; the image's next use uploads the memory.
static void SyncImageToMemory(ColorImage *image) {
    if (HashImageMemory(image) == image->memoryHash) {
        DownloadTarget(image);
    }
    else {
        image->isMemoryBehind = false;
    }
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
        SyncImageToMemory(oldest);
    }

    BackendDestroyTarget(oldest->target);
    SDL_zerop(oldest);
    return oldest;
}

// The ranges can lie in different objects, such as RDRAM and a heap block
// that the TLB maps, which C leaves the order of undefined: their
// addresses' order isn't
static bool DoRangesOverlap(
    const uint8_t *start,
    uintptr_t size,
    const uint8_t *otherStart,
    uintptr_t otherSize
) {
    uintptr_t first = (uintptr_t)start;
    uintptr_t otherFirst = (uintptr_t)otherStart;

    return first < otherFirst + otherSize && otherFirst < first + size;
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

// Makes the target at least height rows tall, keeping what it holds. The
// caller has flushed the frame, so the copy follows the frame's draws.
static void FitTarget(ColorImage *image, uint16_t height) {
    GfxTarget *grown;

    if (image->targetHeight >= height) {
        return;
    }

    grown = BackendCreateTarget(image->width, height, false);
    BackendCopyTarget(image->target, grown, image->width, image->targetHeight);
    BackendDestroyTarget(image->target);
    image->target = grown;
    image->targetHeight = height;
}

// Takes in memory's rows down to height. No draw has reached below the old
// height, so the new rows come from memory and the drawing above stays.
static void ExtendImage(ColorImage *image, uint16_t height) {
    uint16_t oldHeight = image->height;

    image->height = height;
    DropOverlappedImages(image->address, ImageByteCount(image), image);
    UploadMemoryRows(image, oldHeight);
}

ColorImage *UseColorImage(
    uint8_t *address,
    uint16_t width,
    uint16_t height,
    uint16_t targetHeight,
    int size
) {
    int bytesPerPixel = BytesPerPixel(size);
    ColorImage *image;

    SDL_assert(address != NULL && width != 0 && height != 0 && height <= targetHeight);
    SDL_LockMutex(f_Lock);
    image = FindColorImage(address, width, bytesPerPixel);
    if (image == NULL) {
        image = FreeColorImage();
        image->address = address;
        image->width = width;
        image->height = height;
        image->targetHeight = targetHeight;
        image->bytesPerPixel = (uint8_t)bytesPerPixel;
        DropOverlappedImages(address, ImageByteCount(image), image);
        image->target = BackendCreateTarget(width, targetHeight, false);
        UploadMemory(image);
    }
    else if (HashImageMemory(image) != image->memoryHash) {
        // The CPU's writes are newer than any drawing
        FitTarget(image, height);
        image->height = SDL_max(image->height, height);
        DropOverlappedImages(address, ImageByteCount(image), image);
        UploadMemory(image);
    }
    else if (image->height < height) {
        FitTarget(image, height);
        ExtendImage(image, height);
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

uint16_t ColorImageTargetHeight(const ColorImage *image) {
    return image->targetHeight;
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
            depth->height == colorImage->targetHeight) {
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
    free->height = colorImage->targetHeight;
    free->target = BackendCreateTarget(colorImage->width, colorImage->targetHeight, true);
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
            SyncImageToMemory(image);
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
