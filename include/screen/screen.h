/*
 * The screen's size and coordinates, and the screen effects.
 */

#ifndef SCREEN_H
#define SCREEN_H

#include <ultra64.h>

// Screen sizes, in pixels
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
// The perspectives that fill the screen take its width over its height
#define SCREEN_ASPECT_RATIO (4.0f / 3.0f)

// The border around the main camera's scissor rectangle, in pixels
#define SCREEN_BORDER 8

// Screen coordinates in display lists are in quarter pixels
#define QUARTER_PIXEL_SHIFT 2
#define QUARTER_PIXELS_PER_PIXEL (1 << QUARTER_PIXEL_SHIFT)
// A screen position in pixels, in the quarter pixels display lists take. It
// shifts rather than multiplies, since the GBI's masks fold differently
// around a multiply. A position left of or above the screen is negative,
// which C leaves the shift of undefined, and the multiply gives the same.
#ifdef AVOID_UB
#    define QuarterPixels(pixels) ((pixels) * QUARTER_PIXELS_PER_PIXEL)
#else
#    define QuarterPixels(pixels) ((pixels) << QUARTER_PIXEL_SHIFT)
#endif

// Texture rectangle steps are 5.10 fixed point
#define TEXEL_STEP_SHIFT 10
// A texture rectangle step of one texel per pixel
#define TEXEL_STEP_ONE (1 << TEXEL_STEP_SHIFT)
// The step that draws a run of texels across a run of pixels
#define TexelRunStep(texels, pixels) (((texels) << TEXEL_STEP_SHIFT) / (pixels))
// Indices of a vertex's texture coordinates
#define TEXTURE_AXIS_S 0
#define TEXTURE_AXIS_T 1
// Copy mode writes four 16-bit texels a cycle, so its texture rectangles step
// this many texels a pixel
#define COPY_MODE_TEXELS_PER_CYCLE 4
#define COPY_MODE_TEXEL_STEP (COPY_MODE_TEXELS_PER_CYCLE * TEXEL_STEP_ONE)
// A texel position, as texture coordinates take it in 10.5 fixed point. A
// texel left of or above a texture's origin is negative, which C leaves
// undefined to shift; multiplying gives the same bits in two's complement.
#ifdef AVOID_UB
#    define TexelCoordinate(texel) ((texel) * (1 << 5))
#else
#    define TexelCoordinate(texel) ((texel) << 5)
#endif
// The words gSPModifyVertex takes for a point's screen position, x and y in
// quarter pixels, and for its texture coordinates, s and t in 10.5
#define ScreenPointWord(x, y) (((x) << 16) + (y))
#define TextureCoordinateWord(s, t) (((s) << 16) + (t))

// Where the 5-bit channels sit in an RGBA16 pixel, and the shift that widens
// one to 8 bits
#define RGBA16_RED_SHIFT 11
#define RGBA16_GREEN_SHIFT 6
#define RGBA16_BLUE_SHIFT 1
#define RGBA16_CHANNEL_MASK 0x1F
#define RGBA16_CHANNEL_WIDEN_SHIFT 3

// An RGBA16 pixel from its 5-bit channels and its 1-bit alpha
#define Rgba16(red, green, blue, alpha) \
    (((red) << RGBA16_RED_SHIFT) | ((green) << RGBA16_GREEN_SHIFT) | \
        ((blue) << RGBA16_BLUE_SHIFT) | (alpha))

// SetScreenEffect's effects
enum ScreenEffect {
    SCREEN_EFFECT_SPIN, // Spins the saved frame away
    SCREEN_EFFECT_EMPTY, // Draws nothing
    SCREEN_EFFECT_TILED_IMAGE, // Repeats part of the saved frame
    SCREEN_EFFECT_SAVED_FRAME, // Draws a frame saved elsewhere
    SCREEN_EFFECT_NONE
};

typedef s32 ScreenEffect;

// SCREEN_EFFECT_SAVED_FRAME's steps, which its frame count holds
enum SavedFrameStep {
    // Copies the frame kept in the saved frame buffer to the screen
    SAVED_FRAME_STEP_SHOWING = 1,
    // Clears the saved frame buffer and draws the scene into it
    SAVED_FRAME_STEP_SAVING
};

extern ScreenEffect g_ScreenEffect; // The effect SetScreenEffect last set

// An orthographic projection of one unit per pixel, centered on the screen,
// with y rising
extern Mtx g_ScreenOrthoMatrix;

// A rectangle of the screen, in pixels
typedef struct ScreenRectangle {
    s16 left;
    s16 top;
    s16 right;
    s16 bottom;
} ScreenRectangle;

// The rectangle that the HUD is drawn within
extern ScreenRectangle g_HudScissor;

void SetScreenEffect(ScreenEffect effect, s32 frameCount);

#endif
