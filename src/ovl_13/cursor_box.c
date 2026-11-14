/*
 * Loads the cursor box's image and draws it around a point.
 */
#include "cursor_box.h"

#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "compiler.h"
#include "game.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "math_util.h"
#include "screen.h"

#define CURSOR_BOX_CORNER_COUNT 4
/* Half the size of the box the cursor frames, whose top-left corner the callers
 * give */
#define CURSOR_BOX_HALF_SIZE 24
/* A corner's red swings this far to either side of its middle as its phase
 * turns */
#define CURSOR_RED_SWING 127.0f
#define CURSOR_RED_MIDDLE 127
#define CURSOR_GREEN COLOR_CHANNEL_MAX
#define CURSOR_BLUE 32
#define CURSOR_ALPHA 191
/* How far each corner's phase turns in a frame */
#define CURSOR_PHASE_STEP DegreesToRadians(15)

static void *f_CursorBoxImage = NULL;
static Vtx f_CursorBoxVertices[] = {
    {{{-24, 24, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-24, -24, 0}, 0, {0, 0xC00}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{24, -24, 0}, 0, {0xC00, 0xC00}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{24, 24, 0}, 0, {0xC00, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
};
/* The corners' color pulses start a quarter turn apart */
static f32 f_CursorCornerPhases[] =
    {0.0f, QUARTER_TURN_RADIANS, 2 * QUARTER_TURN_RADIANS, 3 * QUARTER_TURN_RADIANS};

/* Read by no routine */
static u8 f_UnusedBufferAfterCornerPhases[0x40] UNUSED;
static Mtx f_CursorBoxProjection;
static Mtx f_CursorBoxTranslations[DRAW_BUFFER_COUNT];

/* Reads the cursor box image into the given buffer and sizes the quad to it */
void LoadCursorBox(void *image) {
    EncodedAsset asset;

    /*
     * s32 sizes are there to match: u16 sizes give 12 words differing (none in
     * shape), u32 48 (4) and s16 68 (14). Reading the sizes as the header's
     * halfwords, rather than a byte at a time, gives 10 (10).
     */
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

/*
 * Draws the cursor box with its top-left corner at the given screen position.
 * Each corner's red pulses with the sine of its own phase, which the four
 * corners start a quarter turn apart.
 *
 * The green and blue are variables, held to a channel's range as the red is
 * though they are constants. The original sets both in the loop, where it sets
 * the alpha in $s5 before the loop: the green in $s0, a saved register as its
 * life crosses sinf's call, and the blue in $a0 after the call. loop.c moves
 * out of a loop only a register set once in it, and its dump shows each of the
 * two with a second set, the one that holds it at zero or above. Stored as
 * constants, or as variables that are not held, they are set before the loop as
 * the alpha is.
 *
 * Measured against the match, as words differing and how many of them in shape:
 *   The green and blue not held                               173 (11)
 *   The green held and not the blue                           135 (7)
 *   The blue held and not the green                           38 (2)
 *   The two held only above, or only below                    173 (11)
 *   The red set before the green                              173 (5)
 *   The green and then the blue set before the red            167 (5)
 *   The blue and then the green set before the red            169 (5)
 *   The blue set before the red and the green after it        8 (0)
 *   The green held before the red                             38 (2)
 *   The blue held before the red                              135 (7)
 *   Both held before the red                                  173 (11)
 *   The phase stepped in IN_OWN_BLOCK                         143 (11)
 *   The loop's test as corner < CURSOR_BOX_CORNER_COUNT       141 (11)
 *   u8 green and blue                                         173 (11)
 *   The alpha a variable too, held as the others are          139 (9)
 *   The red stored after the green and blue                   6 (0)
 *   boxY's difference written in the call                     2 (2)
 *   The old draft: the green and blue stored as constants and
 *   the phase stepped in IN_OWN_BLOCK                         42 (10)
 *
 * Also matching: the green set before the phase is stepped, or declared in the
 * loop's block with its value; the blue held before the green; all three held
 * above and then all three below; the alpha as a variable set after the blue
 * and not held; and the stepped phase in a local.
 */
void DrawCursorBox(Gfx **displayListHead, s32 left, s32 top) {
    Gfx *displayList = *displayListHead;
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
        /* The green is set before the red and the blue after it, to match */
        green = CURSOR_GREEN;
        red = (s32)(sinf(f_CursorCornerPhases[corner]) * CURSOR_RED_SWING) + CURSOR_RED_MIDDLE;
        blue = CURSOR_BLUE;
        if (red > COLOR_CHANNEL_MAX) {
            red = COLOR_CHANNEL_MAX;
        }

        if (red < 0) {
            red = 0;
        }

        /* Held as the red is, though they are constants, to match */
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

        f_CursorBoxVertices[corner].v.cn[0] = red;
        f_CursorBoxVertices[corner].v.cn[1] = green;
        f_CursorBoxVertices[corner].v.cn[2] = blue;
        f_CursorBoxVertices[corner].v.cn[3] = CURSOR_ALPHA;
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
    /* In a variable, to match */
    boxY = (SCREEN_HEIGHT / 2) - CURSOR_BOX_HALF_SIZE - top;
    guTranslate(
        &f_CursorBoxTranslations[g_DrawBufferIndex],
        (f32)(left + CURSOR_BOX_HALF_SIZE - (SCREEN_WIDTH / 2)) - 0.5f,
        (f32)boxY + 0.5f,
        -100.0f
    );
    gDPPipeSync(displayList++);
    gDPPipeSync(displayList++);
    gDPSetTextureLUT(displayList++, G_TT_NONE);
    gDPLoadTextureBlock_4b(
        displayList++,
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
    gDPPipeSync(displayList++);
    gDPPipeSync(displayList++);
    gDPSetCycleType(displayList++, G_CYC_1CYCLE);
    gDPPipeSync(displayList++);
    gDPPipeSync(displayList++);
    gDPSetTexturePersp(displayList++, G_TP_PERSP);
    gDPPipeSync(displayList++);
    gDPSetTextureFilter(displayList++, G_TF_POINT);
    gDPPipeSync(displayList++);
    gDPSetRenderMode(displayList++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineLERP(
        displayList++,
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
    gSPClearGeometryMode(displayList++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(displayList++, G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(displayList++);
    gSPMatrix(
        displayList++,
        PhysicalAddress(&f_CursorBoxProjection),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gSPMatrix(
        displayList++,
        PhysicalAddress(&f_CursorBoxTranslations[g_DrawBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
    );
    gDPPipeSync(displayList++);
    gSPVertex(displayList++, f_CursorBoxVertices, CURSOR_BOX_CORNER_COUNT, 0);
    gSP2Triangles(displayList++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(displayList++);
    *displayListHead = displayList;
}
