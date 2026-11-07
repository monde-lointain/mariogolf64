/*
 * The billboards that stand for a hole's trees: placing them on the ground,
 * sorting them by depth and drawing them, translucent close to the camera. The
 * file also reads and writes files on the host.
 */
#include "billboard.h"

#include <string.h>
#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "camera.h"
#include "compiler.h"
#include "course_space.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "game.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "heap.h"
#include "host_file.h"
#include "include_asm.h"
#include "language.h"
#include "math_util.h"
#include "shot_camera.h"
#include "terrain.h"

/* Textures of this type are read from the asset archive when first drawn */
#define TEXTURE_LOADED_ON_DEMAND 2

/* Every billboard texture file starts with a header before its texels */
#define TEXTURE_HEADER_SIZE 8

/* Vertices per billboard shape, of which the first four form its quad */
#define SHAPE_VERTEX_COUNT 8

/* f_LoadedTextureIndex while no texture is loaded, so that the next billboard
 * drawn loads its own */
#define NO_TEXTURE_LOADED (-1)

/* Room for the file name in a host command, including padding */
#define HOST_FILE_NAME_SIZE_MAX 0x100

typedef struct BillboardTexture {
    /* 0x0 */ s32 assetId;
    /* 0x4 */ s16 type;
    /* 0x6 */ s16 width;
    /* 0x8 */ s16 height;
    /* 0xC */ u16 *pixels;
} BillboardTexture; /* size = 0x10 */

/* A color for the billboards' combiner, a channel to a word */
typedef struct BillboardColor {
    /* 0x0 */ s32 red;
    /* 0x4 */ s32 green;
    /* 0x8 */ s32 blue;
} BillboardColor; /* size = 0xC */

/* The size of every billboard texture, in texels */
#define BILLBOARD_TEXTURE_WIDTH 32
#define BILLBOARD_TEXTURE_HEIGHT 64

/* A texture read from an asset when first drawn */
#define BILLBOARD_TEXTURE(assetId) \
    {(assetId), TEXTURE_LOADED_ON_DEMAND, BILLBOARD_TEXTURE_WIDTH, BILLBOARD_TEXTURE_HEIGHT, NULL}

Billboard g_Billboards[BILLBOARD_COUNT];

FILE_STATIC BillboardTransform g_BillboardTransforms[BILLBOARD_COUNT];

static Vtx f_BillboardVertices[][SHAPE_VERTEX_COUNT] = {
    {
        {{{-1, 0, 0}, 0, {0, 4096}, {0x96, 0x96, 0x96, 0xFF}}},
        {{{1, 0, 0}, 0, {8192, 4096}, {0xFF, 0xFF, 0xFF, 0xFF}}},
        {{{1, -1, 0}, 0, {8192, 0}, {0xFF, 0xFF, 0xFF, 0x00}}},
        {{{-1, -1, 0}, 0, {0, 0}, {0x96, 0x96, 0x96, 0x00}}},
        {{{-1, 0, 0}, 0, {0, 4096}, {0x3F, 0x3F, 0x7F, 0xFF}}},
        {{{1, 0, 0}, 0, {8192, 4096}, {0x7F, 0x7F, 0xBF, 0xFF}}},
        {{{1, 1, 0}, 0, {8192, 0}, {0x7F, 0x7F, 0xBF, 0x00}}},
        {{{-1, 1, 0}, 0, {0, 0}, {0x3F, 0x3F, 0x7F, 0x00}}},
    },
};
static s32 f_LoadedTextureIndex = NO_TEXTURE_LOADED;
static BillboardTexture f_BillboardTextures[BILLBOARD_TEXTURE_COUNT] = {
    BILLBOARD_TEXTURE(0x3F7),
    BILLBOARD_TEXTURE(0x3F8),
    BILLBOARD_TEXTURE(0x3F9),
    BILLBOARD_TEXTURE(0x3FA),
    BILLBOARD_TEXTURE(0x3FB),
    BILLBOARD_TEXTURE(0x3FC),
    BILLBOARD_TEXTURE(0x3FD),
    BILLBOARD_TEXTURE(0x3FE),
    BILLBOARD_TEXTURE(0x3FF),
    BILLBOARD_TEXTURE(0x400),
    BILLBOARD_TEXTURE(0x401),
    BILLBOARD_TEXTURE(0x402),
    BILLBOARD_TEXTURE(0x403),
    BILLBOARD_TEXTURE(0x404),
    BILLBOARD_TEXTURE(0x405),
    BILLBOARD_TEXTURE(0x406),
    BILLBOARD_TEXTURE(0x407),
    BILLBOARD_TEXTURE(0x407), /* Again, where 0x408 would follow */
    BILLBOARD_TEXTURE(0x409),
    BILLBOARD_TEXTURE(0x40A),
    BILLBOARD_TEXTURE(0x40B),
    BILLBOARD_TEXTURE(0x40C),
    BILLBOARD_TEXTURE(0x40D),
    BILLBOARD_TEXTURE(0x40E),
    BILLBOARD_TEXTURE(0x40F),
    BILLBOARD_TEXTURE(0x410),
    BILLBOARD_TEXTURE(0x411),
    BILLBOARD_TEXTURE(0x412),
    BILLBOARD_TEXTURE(0x413),
    BILLBOARD_TEXTURE(0x414),
    BILLBOARD_TEXTURE(0x415),
    BILLBOARD_TEXTURE(0x416),
    BILLBOARD_TEXTURE(0x417),
    BILLBOARD_TEXTURE(0x418),
    BILLBOARD_TEXTURE(0x419),
    BILLBOARD_TEXTURE(0x41A),
    BILLBOARD_TEXTURE(0x41B),
    BILLBOARD_TEXTURE(0x41C),
    BILLBOARD_TEXTURE(0x41D),
    BILLBOARD_TEXTURE(0x41E),
    BILLBOARD_TEXTURE(0x41F),
    BILLBOARD_TEXTURE(0x90F),
    BILLBOARD_TEXTURE(0x910),
    BILLBOARD_TEXTURE(0x911),
    BILLBOARD_TEXTURE(0x912),
    BILLBOARD_TEXTURE(0x913),
};
static s8 f_IsHeapReady = FALSE;
static u8 *f_HeapStart = NULL;
static u8 *f_HeapEnd = NULL;
static u16 f_DrawOrder[BILLBOARD_COUNT] = {0};
/* Read by no routine */
static s8 f_UnusedBytesAfterDrawOrder[2] UNUSED = {1, 1};

/* Billboards this close to the course's edges are hidden */
#define COURSE_EDGE_MIN_X 256.0f
#define COURSE_EDGE_MIN_Z 256.0f
#define COURSE_EDGE_MAX_X 3840.0f
#define COURSE_EDGE_MAX_Z 7936.0f

/* GetGroundHeight works in fixed point with 10 fraction bits */
#define GROUND_HEIGHT_SCALE 1024

/* GetViewDepth gives billboards closer than this to the camera a depth
   between it and 0 */
#define VIEW_DEPTH_NEAR (-0x800)

/* Depth given to the ball when no billboard needs to stay out of its way */
#define VIEW_DEPTH_BALL_HIDDEN (-0xFFFF)

/* The least opacity of a billboard close to the camera */
#define NEAR_BILLBOARD_ALPHA_MIN 0x80

/* Billboards are only lit from a light at least this high */
#define LIGHT_HEIGHT_MIN 0.1f

/* In the debug view, near billboards this close in front of the ball are
   not drawn */
#define BALL_CLEARANCE_DEPTH 100

/* Depth of unused billboards, behind every used one */
#define VIEW_DEPTH_FAR 0x40000000

void DestroyBillboardManager(void) {
    u32 offset;

    osSyncPrintf("DestBillMan\n");
    for (offset = 0; offset < sizeof(f_BillboardTextures); offset += sizeof(BillboardTexture)) {
        ((BillboardTexture *)AddressPast(f_BillboardTextures, offset))->pixels = NULL;
    }

    ResetHeap(HEAP_BILLBOARD);
    f_IsHeapReady = FALSE;
}

void InitBillboardManager(void *heapStart, s32 heapSize) {
    u8 *heapEnd;

    osSyncPrintf("InitBillMan\n");
    heapEnd = AddressPast(heapStart, heapSize);
    DestroyBillboardManager();
    f_HeapStart = heapStart;
    f_HeapEnd = heapEnd;
    AddHeapRegion(HEAP_BILLBOARD, heapStart, heapEnd);
    f_IsHeapReady = TRUE;
}

void RevertBillboardManager(void) {
    osSyncPrintf("RevertBillMan\n");
    DestroyBillboardManager();
    AddHeapRegion(HEAP_BILLBOARD, f_HeapStart, f_HeapEnd);
    f_IsHeapReady = TRUE;
}

/*
 * The initial values of the colors local to DrawBillboards and
 * DrawFadingBillboards, which both copy
 */
const BillboardColor g_BillboardLightColor = {255, 220, 160};
const BillboardColor g_BillboardEnvColor = {128, 64, 32};

/*
 * Whether a point is near the edge of a course; no point of the
 * mini-golf course is. It is a macro, as an inline function compiles
 * differently from the original.
 */
#define IsNearCourseEdge(x, z) \
    (!g_GameState.isMiniGolf && \
        (((x) <= COURSE_EDGE_MIN_X) || \
            ((z) <= COURSE_EDGE_MIN_Z) || \
            ((x) >= COURSE_EDGE_MAX_X) || \
            ((z) >= COURSE_EDGE_MAX_Z)))

void PlaceBillboards(void) {
    s32 i;
    f32 x;
    f32 z;
    f32 y;

    for (i = 0; i != BILLBOARD_COUNT; i++) {
        f_DrawOrder[i] = i;
    }

    for (i = 0; i < BILLBOARD_COUNT; i++) {
        if (g_Billboards[i].textureIndex >= 0) {
            x = g_Billboards[i].x;
            z = g_Billboards[i].z;
            if (IsNearCourseEdge(x, z)) {
                g_Billboards[i].isNearCourseEdge = TRUE;
            }
            else {
                g_Billboards[i].isNearCourseEdge = FALSE;
            }

            y = GetGroundHeight(x * GROUND_HEIGHT_SCALE, z * GROUND_HEIGHT_SCALE) /
                GROUND_HEIGHT_SCALE;
            g_Billboards[i].y = y;
            guTranslate(&g_BillboardTransforms[i].translation, x, y, z);
            guScale(
                &g_BillboardTransforms[i].scale,
                g_Billboards[i].width,
                g_Billboards[i].height,
                1.0f
            );
        }
    }
}

void SortBillboardsByDepth(void) {
    s32 i;
    s32 sortedIndex;
    u16 insertedIndex;

    for (i = 0; i != BILLBOARD_COUNT; i++) {
        if (g_Billboards[i].textureIndex < 0) {
            g_BillboardTransforms[i].viewDepth = VIEW_DEPTH_FAR;
        }
        else {
            g_BillboardTransforms[i].viewDepth =
                GetViewDepth(g_Billboards[i].x, g_Billboards[i].y, g_Billboards[i].z);
        }
    }

    /* Insertion sort, farthest first */
    for (i = 1; i != BILLBOARD_COUNT; i++) {
        insertedIndex = f_DrawOrder[i];
        for (sortedIndex = i - 1; sortedIndex >= 0 &&
            g_BillboardTransforms[f_DrawOrder[sortedIndex]].viewDepth <
                g_BillboardTransforms[insertedIndex].viewDepth;
            sortedIndex--) {
            f_DrawOrder[sortedIndex + 1] = f_DrawOrder[sortedIndex];
        }

        f_DrawOrder[sortedIndex + 1] = insertedIndex;
    }
}

/*
 * Whether a billboard's texture is one read in on demand, has been read
 * and is not the one loaded for drawing. It is a macro, as an inline function
 * compiles differently from the original.
 */
#define NeedsTextureLoad(billboard, textureIndex) \
    (((billboard)->textureIndex != f_LoadedTextureIndex) && \
        (f_BillboardTextures[textureIndex].type == TEXTURE_LOADED_ON_DEMAND) && \
        (f_BillboardTextures[textureIndex].pixels != NULL))

void DrawBillboard(
    Gfx **gfxPtr,
    Billboard *billboard,
    BillboardTransform *transform,
    s32 textureIndex,
    CameraMatrices *cameraMatrices
) {
    Gfx *gfx = *gfxPtr;
    AssetFile file;
    s32 texelSize;

    if (f_BillboardTextures[textureIndex].pixels == NULL &&
        f_BillboardTextures[textureIndex].type == TEXTURE_LOADED_ON_DEMAND) {
        texelSize = f_BillboardTextures[textureIndex].width *
            f_BillboardTextures[textureIndex].height * sizeof(u16);
        f_BillboardTextures[textureIndex].pixels =
            AllocFromHeap(HEAP_BILLBOARD, texelSize + TEXTURE_HEADER_SIZE);
        if (f_BillboardTextures[textureIndex].pixels != NULL) {
            OpenAssetFile(f_BillboardTextures[textureIndex].assetId, &file);
            ReadAssetFile(f_BillboardTextures[textureIndex].pixels, TEXTURE_HEADER_SIZE, &file);
            ReadAssetFile(f_BillboardTextures[textureIndex].pixels, texelSize, &file);
        }
    }

    if (NeedsTextureLoad(billboard, textureIndex)) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(f_BillboardTextures[textureIndex].pixels),
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            f_BillboardTextures[textureIndex].width,
            f_BillboardTextures[textureIndex].height,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_MIRROR | G_TX_WRAP,
            5,
            6,
            2,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPSetCombineLERP(
            gfx++,
            ENVIRONMENT,
            TEXEL0,
            ENV_ALPHA,
            TEXEL0,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            SHADE,
            0,
            COMBINED,
            0,
            0,
            0,
            0,
            COMBINED
        );
    }

    gSPMatrix(gfx++, &transform->translation, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
    gSPMatrix(gfx++, &cameraMatrices->billboard, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
    gSPMatrix(gfx++, &transform->scale, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, f_BillboardVertices[billboard->shapeIndex], QUAD_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    f_LoadedTextureIndex = billboard->textureIndex;
    *gfxPtr = gfx;
}

/*
 * Draws the billboards. Those far from the camera are drawn first, from
 * the nearest to the farthest; DrawFadingBillboards draws the close ones
 * that need to stay see-through. Here, the close ones are only drawn when
 * they stay opaque.
 *
 * Doesn't match: the original's frame is 16 bytes larger, with the space
 * unused above the spill slots, which moves every register save. Between
 * the loops it sets $s6 to 0 and never reads it, as a biv that loop.c
 * eliminated would leave, and in the far loop it keeps ballDepth -
 * BALL_CLEARANCE_DEPTH in $fp and rebuilds both tables' addresses, where
 * ours keeps g_BillboardTransforms in $fp. Counting the far loop up and
 * indexing f_DrawOrder from the end gives more words: loop.c doesn't
 * reduce the index. Unused locals for the frame move the spill slots. The
 * permuter's flag for the near loop's viewDepth >= 0 test (131 words, 9
 * in shape) only takes an 8-byte slot that grows the frame, so it is a
 * pad in disguise. The register oracle, forcing 7 pseudos' registers,
 * leaves 77 words at best, so the code differs too. Declaring the far
 * loop's textureIndex unsigned gives 107 words (25 in shape) only because
 * it drops the unused-billboard test, which the original keeps as an lb
 * and a bltzl. Declaring it an s16 gives 131 words (9 in shape), and the
 * oracle's best 67 with 8 forced, but only changes which table's address
 * the far loop keeps; it adds no extension, but the field is an s8, which
 * as the local's type changes nothing, and the s16 in the near loop too
 * gives 156 words, or in the near loop alone 164. An s16 viewDepth gives
 * 133 words (11 in shape) but loads the field's low half with an lh, and
 * an s8 billboardIndex 135 (13) but loads each order entry's low byte
 * with an lb, where the original loads the depth with lw and the entries
 * with lhu. Reading the texture index from the billboard at both uses in
 * the second loop, which counts down, gives 121 words (21 in shape), but
 * it loads the field again after IsDebugBButtonView, where the original
 * keeps its one load in $s4 for the call; a focused permuter run's 88
 * words test textureIndex before setting it. Those counts were measured
 * with the view test taking fabs of each component converted back to f32,
 * when 139 words differed, 17 in shape. With the game's fabsf there, 125
 * words differ, 13 in shape.
 *
 * Computing ballDepth - BALL_CLEARANCE_DEPTH into its own local before
 * SortBillboardsByDepth gives 127 words (15 in shape) and a frame 8 bytes
 * short, and before the far loop the draft's 125, as loop.c hoists it
 * there anyway. A counter of the billboards drawn in the far loop that
 * nothing reads, set to 0 in or before the loop, is deleted whole and
 * gives the draft's 125; counting the far loop with it while orderIndex
 * counts down gives 137 (11 in shape) and keeps the counter's increments.
 *
 * In the second loop, a counter from 0 with orderIndex set to
 * BILLBOARD_COUNT - 1 less it gives 136 words (12 in shape), and indexing
 * f_DrawOrder with that difference 126 (12): loop.c then keeps the
 * subtraction in the loop rather than reducing the address, and the
 * counter's own test. Flags for the debug view's test, the near-ball test
 * or the opaque test, set to FALSE and then TRUE in each pass, give 125
 * to 129 words; none leaves the original's unread $s6 = 0 before the
 * loop.
 */
#ifdef NON_MATCHING
void DrawBillboards(CameraMatrices *cameraMatrices, s32 cameraIndex) {
    Gfx *gfx = g_DisplayListHead;
    BillboardColor lightColor UNUSED = g_BillboardLightColor;
    BillboardColor envColor = g_BillboardEnvColor;
    Vec3 viewDir;
    Vec3 lightDir;
    /* Pads the frame to match; without it 227 words differ, against 125 */
    Vec3 unusedVector UNUSED;
    Radians lightPitch;
    Radians lightYaw;
    f32 alignment;
    f32 lightHeight;
    f32 leftAlignment;
    f32 rightAlignment;
    s32 ballDepth;
    s32 orderIndex;
    s32 billboardIndex;

    if (IsGameFlagSet(GAME_FLAG_HIDE_BILLBOARDS) || !f_IsHeapReady) {
        return;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPPipeSync(gfx++);
    gDPSetEnvColor(gfx++, envColor.red, envColor.green, envColor.blue, 0x20);

    lightYaw = g_HoleSetup.conditions.lightYaw * BINARY_ANGLE_TO_RADIANS;
    lightPitch = g_HoleSetup.conditions.lightPitch * BINARY_ANGLE_TO_RADIANS;
    viewDir[AXIS_X] = g_Cameras[0].view.lookAt[0] - g_Cameras[0].view.eye[0];
    viewDir[AXIS_Y] = 0.0f;
    viewDir[AXIS_Z] = g_Cameras[0].view.lookAt[2] - g_Cameras[0].view.eye[2];
    lightDir[AXIS_X] = cosf(lightPitch) * cosf(lightYaw);
    lightDir[AXIS_Z] = cosf(lightPitch) * sinf(lightYaw);
    lightDir[AXIS_Y] = sinf(lightPitch);
    if (fabsf(viewDir[AXIS_X]) + fabsf(viewDir[AXIS_Z]) >= 1.0f) {
        guNormalize(&viewDir[AXIS_X], &viewDir[AXIS_Y], &viewDir[AXIS_Z]);
        guNormalize(&lightDir[AXIS_X], &lightDir[AXIS_Y], &lightDir[AXIS_Z]);
        alignment = (viewDir[AXIS_X] * lightDir[AXIS_X]) + (viewDir[AXIS_Z] * lightDir[AXIS_Z]);
        lightHeight = cosf(lightPitch);
        if (lightHeight >= LIGHT_HEIGHT_MIN) {
            leftAlignment = alignment;
            if (leftAlignment > lightHeight) {
                leftAlignment = lightHeight;
            }

            if (leftAlignment < -lightHeight) {
                leftAlignment = -lightHeight;
            }

            rightAlignment = alignment;
            if (rightAlignment > lightHeight) {
                rightAlignment = lightHeight;
            }

            if (rightAlignment < -lightHeight) {
                rightAlignment = -lightHeight;
            }

            f_BillboardVertices[0][0].v.cn[0] =
                (((-lightHeight * 0.5f) + (leftAlignment * 0.5f)) * 127.0f) + 255.0f;
            f_BillboardVertices[0][0].v.cn[1] =
                (((-lightHeight * 0.5f) + (leftAlignment * 0.5f)) * 127.0f) + 255.0f;
            f_BillboardVertices[0][0].v.cn[2] =
                (((-lightHeight * 0.5f) + (leftAlignment * 0.5f)) * 127.0f) + 255.0f;
            f_BillboardVertices[0][1].v.cn[0] =
                (((-lightHeight * 0.5f) + (rightAlignment * 0.5f)) * 127.0f) + 255.0f;
            f_BillboardVertices[0][1].v.cn[1] =
                (((-lightHeight * 0.5f) + (rightAlignment * 0.5f)) * 127.0f) + 255.0f;
            f_BillboardVertices[0][1].v.cn[2] =
                (((-lightHeight * 0.5f) + (rightAlignment * 0.5f)) * 127.0f) + 255.0f;
            f_BillboardVertices[0][2].v.cn[0] =
                (((-lightHeight * 0.5f) + (rightAlignment * 0.5f)) * 255.0f) + 255.0f;
            f_BillboardVertices[0][2].v.cn[1] =
                (((-lightHeight * 0.5f) + (rightAlignment * 0.5f)) * 255.0f) + 255.0f;
            f_BillboardVertices[0][2].v.cn[2] =
                (((-lightHeight * 0.5f) + (rightAlignment * 0.5f)) * 255.0f) + 255.0f;
            f_BillboardVertices[0][3].v.cn[0] =
                (((-lightHeight * 0.5f) + (leftAlignment * 0.5f)) * 255.0f) + 255.0f;
            f_BillboardVertices[0][3].v.cn[1] =
                (((-lightHeight * 0.5f) + (leftAlignment * 0.5f)) * 255.0f) + 255.0f;
            f_BillboardVertices[0][3].v.cn[2] =
                (((-lightHeight * 0.5f) + (leftAlignment * 0.5f)) * 255.0f) + 255.0f;
        }
    }

    gSPClipRatio(gfx++, FRUSTRATIO_1);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_FOG | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0xFE);
    gDPPipeSync(gfx++);
    gDPSetTextureLOD(gfx++, G_TL_TILE);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);

    if (g_Cameras[cameraIndex].mode == CAMERA_MODE_BEHIND_GOLFER) {
        ballDepth = GetViewDepth(
            g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
            g_CurrentShot.ballPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE,
            g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE
        );
        if (ballDepth > 0) {
            ballDepth = VIEW_DEPTH_BALL_HIDDEN;
        }
    }
    else {
        ballDepth = VIEW_DEPTH_BALL_HIDDEN;
    }

    SortBillboardsByDepth();

    f_LoadedTextureIndex = NO_TEXTURE_LOADED;
    gDPSetPrimColor(gfx++, 0, 0, 0x3F, 0xB4, 0x00, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    for (orderIndex = 0; orderIndex < BILLBOARD_COUNT; orderIndex++) {
        s32 textureIndex;

        billboardIndex = f_DrawOrder[orderIndex];
        textureIndex = g_Billboards[billboardIndex].textureIndex;
        if (g_Billboards[billboardIndex].isNearCourseEdge) {
            continue;
        }

        if (textureIndex < 0) {
            continue;
        }

        if (g_BillboardTransforms[billboardIndex].viewDepth < VIEW_DEPTH_NEAR) {
            DrawBillboard(
                &gfx,
                &g_Billboards[billboardIndex],
                &g_BillboardTransforms[billboardIndex],
                textureIndex,
                cameraMatrices
            );
        }
    }

    gDPPipeSync(gfx++);
    gDPSetRenderMode(
        gfx++,
        G_RM_FOG_SHADE_A,
        Z_CMP | Z_UPD | IM_RD | CVG_DST_FULL | ZMODE_XLU | FORCE_BL |
            GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)
    );
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0x3F);
    for (orderIndex = BILLBOARD_COUNT - 1; orderIndex >= 0; orderIndex--) {
        s32 textureIndex;
        s32 viewDepth;
        s32 alpha;

        billboardIndex = f_DrawOrder[orderIndex];
        if (g_Billboards[billboardIndex].isNearCourseEdge) {
            continue;
        }

        textureIndex = g_Billboards[billboardIndex].textureIndex;
        if (textureIndex < 0) {
            continue;
        }

        viewDepth = g_BillboardTransforms[billboardIndex].viewDepth;
        if (viewDepth < VIEW_DEPTH_NEAR) {
            continue;
        }

        if (viewDepth >= 0) {
            continue;
        }

        alpha = -viewDepth;
        if (alpha > OPAQUE_ALPHA) {
            alpha = OPAQUE_ALPHA;
        }

        if (alpha < NEAR_BILLBOARD_ALPHA_MIN) {
            alpha = NEAR_BILLBOARD_ALPHA_MIN;
        }

        if (viewDepth < ballDepth) {
            alpha = OPAQUE_ALPHA;
        }

        if (IsDebugBButtonView() &&
            g_BillboardTransforms[billboardIndex].viewDepth > ballDepth - BALL_CLEARANCE_DEPTH) {
            continue;
        }

        if (alpha < OPAQUE_ALPHA) {
            continue;
        }

        DrawBillboard(
            &gfx,
            &g_Billboards[billboardIndex],
            &g_BillboardTransforms[billboardIndex],
            textureIndex,
            cameraMatrices
        );
    }

    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPPipeSync(gfx++);
    g_DisplayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/billboard", DrawBillboards);
#endif

/*
 * Whether the billboards are hidden: by the game flag, while their heap
 * is not ready, or by the debug view. It is a macro, as an inline
 * function compiles differently from the original.
 */
#define AreBillboardsHidden() \
    (IsGameFlagSet(GAME_FLAG_HIDE_BILLBOARDS) || !f_IsHeapReady || IsDebugBButtonView())

/*
 * Draws the billboards close to the camera, translucent so that the camera
 * can see through them, from the farthest to the nearest. Those farther
 * than the ball stay opaque.
 */
void DrawFadingBillboards(CameraMatrices *cameraMatrices, s32 cameraIndex) {
    Gfx *gfx = g_DisplayListHead;
    BillboardColor lightColor UNUSED = g_BillboardLightColor;
    BillboardColor envColor = g_BillboardEnvColor;
    Boolean hasTranslucentMode;
    s32 ballDepth;
    s32 orderIndex;
    s32 billboardIndex;
    s32 textureIndex;
    s32 viewDepth;
    s32 alpha;

    if (AreBillboardsHidden()) {
        return;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPPipeSync(gfx++);
    gDPSetEnvColor(gfx++, envColor.red, envColor.green, envColor.blue, 0x20);
    gSPClipRatio(gfx++, FRUSTRATIO_1);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_FOG | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0xFE);
    gDPPipeSync(gfx++);
    gDPSetTextureLOD(gfx++, G_TL_TILE);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);

    if (g_Cameras[cameraIndex].mode == CAMERA_MODE_BEHIND_GOLFER) {
        ballDepth = GetViewDepth(
            g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
            g_CurrentShot.ballPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE,
            g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE
        );
        if (ballDepth > 0) {
            ballDepth = VIEW_DEPTH_BALL_HIDDEN;
        }
    }
    else {
        ballDepth = VIEW_DEPTH_BALL_HIDDEN;
    }

    f_LoadedTextureIndex = NO_TEXTURE_LOADED;
    gDPSetPrimColor(gfx++, 0, 0, 0x3F, 0xB4, 0x00, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);

    /* The blended mode set here is not the translucent surface mode that the
     * loop switches to */
    hasTranslucentMode = FALSE;
    gDPSetRenderMode(
        gfx++,
        G_RM_FOG_SHADE_A,
        Z_CMP | Z_UPD | IM_RD | CVG_DST_FULL | ZMODE_XLU | FORCE_BL |
            GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)
    );
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0x3F);

    for (orderIndex = BILLBOARD_COUNT - 1; orderIndex >= 0; orderIndex--) {
        billboardIndex = f_DrawOrder[orderIndex];
        if (g_Billboards[billboardIndex].isNearCourseEdge) {
            continue;
        }

        textureIndex = g_Billboards[billboardIndex].textureIndex;
        if (textureIndex < 0) {
            continue;
        }

        viewDepth = g_BillboardTransforms[billboardIndex].viewDepth;
        if (viewDepth < VIEW_DEPTH_NEAR) {
            continue;
        }

        if (viewDepth >= 0) {
            continue;
        }

        alpha = -viewDepth;
        if (alpha > OPAQUE_ALPHA) {
            alpha = OPAQUE_ALPHA;
        }

        if (alpha < NEAR_BILLBOARD_ALPHA_MIN) {
            alpha = NEAR_BILLBOARD_ALPHA_MIN;
        }

        if (viewDepth < ballDepth) {
            alpha = OPAQUE_ALPHA;
        }

        if (alpha == OPAQUE_ALPHA) {
            continue;
        }

        if (!hasTranslucentMode && alpha < OPAQUE_ALPHA) {
            hasTranslucentMode = TRUE;
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_FOG_SHADE_A, G_RM_ZB_XLU_SURF2);
        }

        if (alpha < OPAQUE_ALPHA) {
            gDPSetPrimColor(gfx++, 0, 0, 0x3F, 0xB4, 0x00, alpha);
        }

        DrawBillboard(
            &gfx,
            &g_Billboards[billboardIndex],
            &g_BillboardTransforms[billboardIndex],
            textureIndex,
            cameraMatrices
        );
    }

    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPPipeSync(gfx++);
    g_DisplayListHead = gfx;
}

/*
 * Development hardware could pass files to the host computer. These two
 * routines remain from that, but send their commands nowhere.
 */

enum HostCommand {
    HOST_COMMAND_WRITE_FILE = 10,
    HOST_COMMAND_READ_FILE
};

s32 WriteHostFile(const char *fileName, u32 size) {
    u32 *command;
    char *nameBuffer;
    u32 nameSize;
    s32 result;

    if (size == 0) {
        /* "The transfer size is too small." */
        osSyncPrintf("転送サイズが小さいです。");
        return HOST_FILE_SIZE_TOO_SMALL;
    }

    nameSize = strlen(fileName);
    if (nameSize == 0) {
        /* "The file name is too short" */
        osSyncPrintf("ファイル名が短すぎます");
        return HOST_FILE_NAME_TOO_SHORT;
    }

    nameSize = (nameSize + 3) & ~3;
    if (nameSize < HOST_FILE_NAME_SIZE_MAX) {
        command = AllocMemory(sizeof(u32));
        nameBuffer = AllocMemory(nameSize);
        memcpy(nameBuffer, fileName, nameSize);
        *command = HOST_COMMAND_WRITE_FILE;
        *command = nameSize;
        *command = size;
        FreeMemory(&command);
        FreeMemory(&nameBuffer);
        result = HOST_FILE_OK;
    }
    else {
        /* "The file name is too long" */
        osSyncPrintf("ファイル名が長すぎます");
        result = HOST_FILE_NAME_TOO_LONG;
    }

    return result;
}

s32 ReadHostFile(const char *fileName, u32 *size) {
    u32 *command;
    char *nameBuffer;
    u32 nameSize;

    nameSize = strlen(fileName);
    if (nameSize == 0) {
        osSyncPrintf("ファイル名が短すぎます");
        return HOST_FILE_NAME_TOO_SHORT;
    }

    nameSize = (nameSize + 3) & ~3;
    if (nameSize >= HOST_FILE_NAME_SIZE_MAX) {
        osSyncPrintf("ファイル名が長すぎます");
        return HOST_FILE_NAME_TOO_LONG;
    }

    command = AllocMemory(sizeof(u32));
    nameBuffer = AllocMemory(nameSize);
    memcpy(nameBuffer, fileName, nameSize);
    *command = HOST_COMMAND_READ_FILE;
    *command = nameSize;
    if (*command == 0) {
        *size = 0;
        /* "The file has no data." */
        osSyncPrintf("ファイルにデータがありません。");
    }

    FreeMemory(&command);
    FreeMemory(&nameBuffer);
    return 1;
}
