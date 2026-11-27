/*
 * Loads the cursor box's image and draws it around a point.
 */

#include "menus/cursor_box.h"

#include <ultra64.h>

#include "game/game.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "system/asset.h"
#include "system/compiler.h"
#include "system/memory_address.h"

#define CURSOR_BOX_CORNER_COUNT 4
// Half the size of the box the cursor frames, whose top-left corner the callers
// give
#define CURSOR_BOX_HALF_SIZE 24
// A corner's red swings this far to either side of its middle as its phase
// turns
#define CURSOR_RED_SWING 127.0f
#define CURSOR_RED_MIDDLE 127
#define CURSOR_GREEN COLOR_CHANNEL_MAX
#define CURSOR_BLUE 32
#define CURSOR_ALPHA 191
// How far each corner's phase turns in a frame
#define CURSOR_PHASE_STEP DegreesToRadians(15)

static ImageAsset *f_CursorBoxImage = NULL;
static Vtx f_CursorBoxVertices[] = {
    {{{-24, 24, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-24, -24, 0}, 0, {0, 0xC00}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{24, -24, 0}, 0, {0xC00, 0xC00}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{24, 24, 0}, 0, {0xC00, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
};
// The corners' color pulses start a quarter turn apart
static f32 f_CursorCornerPhases[] =
    {0.0f, QUARTER_TURN_RADIANS, 2 * QUARTER_TURN_RADIANS, 3 * QUARTER_TURN_RADIANS};

static u8 f_UnusedBufferAfterCornerPhases[0x40] UNUSED;
static Mtx f_CursorBoxProjection;
static Mtx f_CursorBoxTranslations[DRAW_BUFFER_COUNT];

// Reads the cursor box image into the given buffer and sizes the quad to it
void LoadCursorBox(ImageAsset *image) {
    EncodedAsset asset;

    s32 width;
    s32 height;

    f_CursorBoxImage = image;
    OpenEncodedAsset(ASSET_CURSOR_BOX, &asset);
    ReadEncodedAsset(f_CursorBoxImage, &asset);
    width = ReadImageWidth(f_CursorBoxImage);
    height = ReadImageHeight(f_CursorBoxImage);
    f_CursorBoxVertices[0].v.tc[TEXTURE_AXIS_S] = 0;
    f_CursorBoxVertices[1].v.tc[TEXTURE_AXIS_S] = 0;
    f_CursorBoxVertices[0].v.tc[TEXTURE_AXIS_T] = 0;
    f_CursorBoxVertices[3].v.tc[TEXTURE_AXIS_T] = 0;
    f_CursorBoxVertices[0].v.ob[AXIS_X] = -width / 2;
    f_CursorBoxVertices[1].v.ob[AXIS_X] = -width / 2;
    f_CursorBoxVertices[2].v.ob[AXIS_X] = width / 2;
    f_CursorBoxVertices[3].v.ob[AXIS_X] = width / 2;
    f_CursorBoxVertices[0].v.ob[AXIS_Y] = height / 2;
    f_CursorBoxVertices[1].v.ob[AXIS_Y] = -height / 2;
    f_CursorBoxVertices[2].v.ob[AXIS_Y] = -height / 2;
    f_CursorBoxVertices[3].v.ob[AXIS_Y] = height / 2;
    f_CursorBoxVertices[2].v.tc[TEXTURE_AXIS_S] = HalfScaleTexelCoordinate(width);
    f_CursorBoxVertices[3].v.tc[TEXTURE_AXIS_S] = HalfScaleTexelCoordinate(width);
    f_CursorBoxVertices[1].v.tc[TEXTURE_AXIS_T] = HalfScaleTexelCoordinate(height);
    f_CursorBoxVertices[2].v.tc[TEXTURE_AXIS_T] = HalfScaleTexelCoordinate(height);
}

// Draws the cursor box with its top-left corner at the given screen position.
// Each corner's red pulses with the sine of its own phase, which the four
// corners start a quarter turn apart.
void DrawCursorBox(Gfx **displayListHead, s32 left, s32 top) {
    Gfx *gfx = *displayListHead;
    s32 corner;
    s32 red;
    s32 green;
    s32 blue;
    s32 width;
    s32 height;
    s32 boxY;

    width = ReadImageWidth(f_CursorBoxImage);
    height = ReadImageHeight(f_CursorBoxImage);
    for (corner = 0; corner != CURSOR_BOX_CORNER_COUNT; corner++) {
        f_CursorCornerPhases[corner] += CURSOR_PHASE_STEP;
        green = CURSOR_GREEN;
        red = (s32)(sinf(f_CursorCornerPhases[corner]) * CURSOR_RED_SWING) + CURSOR_RED_MIDDLE;
        blue = CURSOR_BLUE;
        if (red > COLOR_CHANNEL_MAX) {
            red = COLOR_CHANNEL_MAX;
        }

        if (red < 0) {
            red = 0;
        }

        if (green > COLOR_CHANNEL_MAX) {
            green = COLOR_CHANNEL_MAX;
        }

        if (green < 0) {
            green = 0;
        }

        if (blue > COLOR_CHANNEL_MAX) {
            blue = COLOR_CHANNEL_MAX;
        }

        if (blue < 0) {
            blue = 0;
        }

        f_CursorBoxVertices[corner].v.cn[RED_CHANNEL] = red;
        f_CursorBoxVertices[corner].v.cn[GREEN_CHANNEL] = green;
        f_CursorBoxVertices[corner].v.cn[BLUE_CHANNEL] = blue;
        f_CursorBoxVertices[corner].v.cn[ALPHA_CHANNEL] = CURSOR_ALPHA;
    }

    guOrtho(
        &f_CursorBoxProjection,
        -SCREEN_WIDTH / 2,
        SCREEN_WIDTH / 2,
        -SCREEN_HEIGHT / 2,
        SCREEN_HEIGHT / 2,
        10.0f,
        2000.0f,
        1.0f
    );
    boxY = (SCREEN_HEIGHT / 2) - CURSOR_BOX_HALF_SIZE - top;
    guTranslate(
        &f_CursorBoxTranslations[g_DrawBufferIndex],
        (f32)(left + CURSOR_BOX_HALF_SIZE - (SCREEN_WIDTH / 2)) - 0.5f,
        (f32)boxY + 0.5f,
        -100.0f
    );
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_CursorBoxImage)),
        G_IM_FMT_IA,
        width,
        height,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        SHADE,
        SHADE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        SHADE,
        SHADE,
        0,
        TEXEL0,
        0
    );
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_CursorBoxProjection),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_CursorBoxTranslations[g_DrawBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
    );
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, f_CursorBoxVertices, CURSOR_BOX_CORNER_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
