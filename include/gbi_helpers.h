#ifndef GBI_HELPERS_H
#define GBI_HELPERS_H

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

#define TEXTURE_SCALE_HALF 0x8000
#define TEXTURE_SCALE_FULL 0xFFFF
#define HalfScaleTexelCoordinate(texel) ((texel) << 6)
// Half a texel in the S10.5 texture coordinates of textured rectangles
#define HALF_TEXEL 16.0f

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

#define QUAD_VERTEX_COUNT 4

#define ALL_GEOMETRY_MODES 0xFFFFFFFF

#define COLOR_CHANNEL_MAX 0xFF

static inline void SetLightColor(Light *light, s32 red, s32 green, s32 blue) {
    light->l.col[0] = red;
    light->l.col[1] = green;
    light->l.col[2] = blue;
    light->l.colc[0] = red;
    light->l.colc[1] = green;
    light->l.colc[2] = blue;
}

#define FillColor16(red, green, blue, alpha) \
    (GPACK_RGBA5551(red, green, blue, alpha) << 16 | GPACK_RGBA5551(red, green, blue, alpha))

#define OPAQUE_ALPHA_COVERAGE_RENDER_MODE \
    (AA_EN | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | \
        GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1))
#define OPAQUE_ALPHA_COVERAGE_RENDER_MODE2 \
    (AA_EN | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | \
        GBL_c2(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1))

#define TRANSPARENT_ALPHA 0
#define OPAQUE_ALPHA 0xFF

#define RgbaColor(red, green, blue, alpha) \
    (((red) << 24) | ((green) << 16) | ((blue) << 8) | (alpha))

#define AppendSyncedRdpState(displayList, setState, stateArguments...) \
    { \
        Gfx *stateCommand; \
        gDPPipeSync((displayList)++); \
        stateCommand = (displayList)++; \
        setState(stateCommand, stateArguments); \
    }

#endif
