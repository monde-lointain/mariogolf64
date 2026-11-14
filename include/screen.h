#ifndef SCREEN_H
#define SCREEN_H

#include <ultra64.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define SCREEN_ASPECT_RATIO (4.0f / 3.0f)

#define SCREEN_BORDER 8

#define QUARTER_PIXEL_SHIFT 2
#define QUARTER_PIXELS_PER_PIXEL (1 << QUARTER_PIXEL_SHIFT)
#define QuarterPixels(pixels) ((pixels) << QUARTER_PIXEL_SHIFT)

#define TEXEL_STEP_SHIFT 10
#define TEXEL_STEP_ONE (1 << TEXEL_STEP_SHIFT)
#define TexelRunStep(texels, pixels) (((texels) << TEXEL_STEP_SHIFT) / (pixels))
#define TEXTURE_AXIS_S 0
#define TEXTURE_AXIS_T 1
#define COPY_MODE_TEXELS_PER_CYCLE 4
#define COPY_MODE_TEXEL_STEP (COPY_MODE_TEXELS_PER_CYCLE * TEXEL_STEP_ONE)
#define TexelCoordinate(texel) ((texel) << 5)
#define ScreenPointWord(x, y) (((x) << 16) + (y))
#define TextureCoordinateWord(s, t) (((s) << 16) + (t))

#define RGBA16_RED_SHIFT 11
#define RGBA16_GREEN_SHIFT 6
#define RGBA16_BLUE_SHIFT 1
#define RGBA16_CHANNEL_MASK 0x1F
#define RGBA16_CHANNEL_WIDEN_SHIFT 3

#define Rgba16(red, green, blue, alpha) \
    (((red) << RGBA16_RED_SHIFT) | ((green) << RGBA16_GREEN_SHIFT) | \
        ((blue) << RGBA16_BLUE_SHIFT) | (alpha))

enum ScreenEffect {
    SCREEN_EFFECT_SPIN,
    SCREEN_EFFECT_EMPTY,
    SCREEN_EFFECT_TILED_IMAGE,
    SCREEN_EFFECT_SAVED_FRAME,
    SCREEN_EFFECT_NONE
};

typedef s32 ScreenEffect;

enum SavedFrameStep {
    SAVED_FRAME_STEP_SHOWING = 1,
    SAVED_FRAME_STEP_SAVING
};

extern ScreenEffect g_ScreenEffect;

extern Mtx g_ScreenOrthoMatrix;

typedef struct ScreenRectangle {
    s16 left;
    s16 top;
    s16 right;
    s16 bottom;
} ScreenRectangle;

extern ScreenRectangle g_HudScissor;

void SetScreenEffect(s32 effect, s32 frameCount);

#endif
