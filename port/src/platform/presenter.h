/*
 * Showing the N64's screen in the window: the frame buffer that the video
 * interface shows, scaled to fit at the N64's 4:3 shape, with black bars
 * around it.
 *
 * Every routine here runs on the window's thread.
 */

#ifndef PORT_PLATFORM_PRESENTER_H
#define PORT_PLATFORM_PRESENTER_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#include "port/overlay_shape.h"

// The N64's screen, as the game sets the video interface
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// The largest rectangle of the N64's shape that fits the window's pixels,
// centered
SDL_Rect FitScreenToWindow(uint32_t windowWidth, uint32_t windowHeight);

// Turns 16-bit RGBA5551 pixels, in the host's order, into RGBA8 with red
// lowest. The pixels needn't be stored as uint16_t.
void ExpandScreenPixels(const void *pixels, uint32_t count, uint32_t *rgba);

// The present mode to use when the player wants one and the display has
// what it has: immediate falls back to mailbox, and mailbox to vsync,
// which every display has
SDL_GPUPresentMode PickPresentMode(SDL_GPUPresentMode wanted, bool hasMailbox, bool hasImmediate);

// Readies the window's swapchain on the device, presenting as wanted or
// as near as the display allows; false, with an error logged, when it
// can't
bool InitPresenter(SDL_GPUDevice *device, SDL_Window *window, SDL_GPUPresentMode wanted);
void ShutdownPresenter(void);

// Shows the frame buffer at framebuffer, an address in the N64's memory,
// or black for NULL, with up to OVERLAY_MAX_SHAPES shapes over it. What the
// renderer drew there comes from the GPU; what the CPU drew comes from
// memory.
void PresentFramebuffer(const void *framebuffer, const OverlayShape *shapes, int shapeCount);

// Writes the frame buffer at framebuffer, as the N64 shows it, to a PNG
// file at path, after bringing its memory up to date with the GPU's
// drawing. The game must be halted. Returns whether it could.
bool SaveScreenImage(const void *framebuffer, const char *path);

#endif
