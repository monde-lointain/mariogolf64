/*
 * The game's color and depth images, as the GPU's render targets.
 *
 * Each image that the display lists draw into is a target of the GPU at the
 * renderer's scale. The image's memory and its target can each hold what
 * the other lacks: the GPU's drawing reaches memory only when something
 * reads the memory, and what the CPU writes reaches the target only when
 * the GPU next draws into it or shows it. A hash of the memory, taken when
 * the two last agreed, tells whether the CPU has written since.
 */

#ifndef PORT_GFX_COLOR_IMAGES_H
#define PORT_GFX_COLOR_IMAGES_H

#include <stdbool.h>
#include <stdint.h>

#include "frame.h"

typedef struct ColorImage ColorImage;

void InitColorImages(void);

// Destroys every image's target, without writing back what memory lacks,
// before the GPU goes away
void ReleaseColorImages(void);

// The image that the RDP draws into at address, of the G_IM_SIZ_* size,
// holding at least height rows of memory: found, made, or grown. A new
// image's target has room for targetHeight rows, so that it can grow to
// them without a flush. What the CPU has written to its memory since the
// GPU last agreed with it is uploaded first, so the caller must flush the
// frame before calling this when IsColorImageCurrent says it isn't.
ColorImage *UseColorImage(
    uint8_t *address,
    uint16_t width,
    uint16_t height,
    uint16_t targetHeight,
    int size
);

// Whether drawing into the image at address, down to height rows, could go
// on without the frame's draws reaching the GPU first: it exists, its
// target has room, and its memory is unchanged
bool IsColorImageCurrent(const uint8_t *address, uint16_t width, uint16_t height, int size);

GfxTarget *ColorImageTarget(const ColorImage *image);
uint16_t ColorImageWidth(const ColorImage *image);
// The rows of memory that the image holds
uint16_t ColorImageHeight(const ColorImage *image);
// The rows of its target, which draws use
uint16_t ColorImageTargetHeight(const ColorImage *image);

// Records that the GPU has drawn into the image, so its memory is behind
void MarkColorImageDrawn(ColorImage *image);

// The depth target that goes with a color image, for the depth image at
// address
GfxTarget *UseDepthImage(const uint8_t *address, const ColorImage *colorImage);

// Starts a task of the RDP's. The CPU doesn't write the images during one,
// so an image found unwritten by it stays so until the next.
void BeginColorImagesTask(void);

// The target that holds width by height 16-bit pixels from address, in rows
// imageWidth pixels apart, and where they lie in it: the target of an image
// whose memory the CPU hasn't written since the two last agreed. NULL when
// no image holds them all.
GfxTarget *FindPixelsTarget(
    const uint8_t *address,
    uint16_t imageWidth,
    uint16_t width,
    uint16_t height,
    uint16_t *x,
    uint16_t *y
);

// Whether any image with drawing that its memory lacks overlaps the bytes
bool HasUnsyncedColorImage(const uint8_t *start, uintptr_t size);

// Brings the memory of each image that overlaps the bytes up to date with
// what the GPU drew. The caller flushes the frame first.
void SyncColorImagesToMemory(const uint8_t *start, uintptr_t size);

// The target that shows the image at address, or NULL when the GPU has
// never drawn it or the CPU has written it since. The images stay locked,
// so that the target outlives the presenter's use of it on its own thread,
// until UnlockShownTarget, which must follow either way.
GfxTarget *LockShownTarget(const uint8_t *address, uint16_t *width, uint16_t *height);
void UnlockShownTarget(void);

#endif
