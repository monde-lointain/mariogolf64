/*
 * Helpers for building display lists: colors, texture scales and coordinates,
 * quads and geometry modes.
 */

#ifndef DISPLAY_LIST_H
#define DISPLAY_LIST_H

#include <ultra64.h>

enum ColorChannel {
    RED_CHANNEL,
    GREEN_CHANNEL,
    BLUE_CHANNEL,
    ALPHA_CHANNEL
};

#define RGB_CHANNEL_COUNT 3

typedef struct RgbColor {
    s32 red;
    s32 green;
    s32 blue;
} RgbColor;

// gSPTexture's scales, in 0.16 fixed point: one half, which most of the
// game's display lists set, and the largest, just under one
#define TEXTURE_SCALE_HALF 0x8000
#define TEXTURE_SCALE_FULL 0xFFFF
// A texel position as a vertex's texture coordinate, in 10.5 fixed point,
// doubled for TEXTURE_SCALE_HALF
#define HalfScaleTexelCoordinate(texel) ((texel) << 6)
#define HALF_TEXEL 16.0f

// The texture coordinate mask that wraps or mirrors a texture every size
// texels, the base-2 logarithm of size, for a power of two up to 256
#define TextureMask(size) \
    ((size) >= 256        ? 8 : \
            (size) >= 128 ? 7 : \
            (size) >= 64  ? 6 : \
            (size) >= 32  ? 5 : \
            (size) >= 16  ? 4 : \
            (size) >= 8   ? 3 : \
            (size) >= 4   ? 2 : \
            (size) >= 2   ? 1 : \
                            0)

// The vertices of a quad, drawn as two triangles
#define QUAD_VERTEX_COUNT 4

// Every geometry mode bit, for gSPClearGeometryMode to clear them all
#define ALL_GEOMETRY_MODES 0xFFFFFFFF

// The largest value of an 8-bit color channel
#define COLOR_CHANNEL_MAX 0xFF

// Sets a directional light's color and its copy to the same red, green
// and blue
static inline void SetLightColor(Light *light, s32 red, s32 green, s32 blue) {
    light->l.col[0] = red;
    light->l.col[1] = green;
    light->l.col[2] = blue;
    light->l.colc[0] = red;
    light->l.colc[1] = green;
    light->l.colc[2] = blue;
}

// A fill color for a 16-bit color image: the RGBA5551 pixel in both halves,
// as the RDP fills two pixels at a time
#define FillColor16(red, green, blue, alpha) \
    (GPACK_RGBA5551(red, green, blue, alpha) << 16 | GPACK_RGBA5551(red, green, blue, alpha))

// A render mode that draws opaque, passing the color through the blender,
// but keeps the alpha as coverage so that antialiasing smooths the edges
#define OPAQUE_ALPHA_COVERAGE_RENDER_MODE \
    (AA_EN | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | \
        GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1))
#define OPAQUE_ALPHA_COVERAGE_RENDER_MODE2 \
    (AA_EN | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | \
        GBL_c2(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1))

// The ends of an 8-bit color's alpha
#define TRANSPARENT_ALPHA 0
#define OPAQUE_ALPHA 0xFF

// A 32-bit RGBA color, as gSPModifyVertex takes a vertex color. A red of
// 0x80 or more reaches the sign bit, which C leaves undefined to shift as an
// int; shifting a u32 gives the same bits.
#ifdef AVOID_UB
#    define RgbaColor(red, green, blue, alpha) \
        (((u32)(red) << 24) | ((green) << 16) | ((blue) << 8) | (alpha))
#else
#    define RgbaColor(red, green, blue, alpha) \
        (((red) << 24) | ((green) << 16) | ((blue) << 8) | (alpha))
#endif

// Appends a pipe sync followed by a single-command GBI state setter.
// displayList must be a side-effect-free Gfx* lvalue with room for both
// commands; it advances twice. setState is a gDPSet... macro, followed by
// its state arguments. Those arguments must not modify displayList.
// Like the SDK's GBI macros, this expands to a block: brace callers' if/else
// bodies. Named variadic arguments support KMC's pre-C99 GCC.
#define AppendSyncedRdpState(displayList, setState, stateArguments...) \
    { \
        Gfx *stateCommand; \
        gDPPipeSync((displayList)++); \
        stateCommand = (displayList)++; \
        setState(stateCommand, stateArguments); \
    }

#endif
