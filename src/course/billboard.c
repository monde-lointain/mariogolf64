/*
 * The billboards that stand for a hole's trees: placing them on the ground,
 * sorting them by depth and drawing them, translucent close to the camera. The
 * file also reads and writes files on the host.
 */

#include "course/billboard.h"

#include <string.h>
#include <ultra64.h>

#include "camera/camera.h"
#include "camera/shot_camera.h"
#include "camera/view_projection.h"
#include "course/course_space.h"
#include "course/hole_setup.h"
#include "course/terrain.h"
#include "debug/game_flags.h"
#include "game/game.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round/game_state.h"
#include "screen/display_list.h"
#include "shot/current_shot.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/host_file.h"
#include "system/memory_address.h"

// Textures of this type are read from the asset archive when first drawn
#define TEXTURE_LOADED_ON_DEMAND 2

// Vertices per billboard shape, of which the first four form its quad
#define SHAPE_VERTEX_COUNT 8

// f_LoadedTextureIndex while no texture is loaded, so that the next billboard
// drawn loads its own
#define NO_TEXTURE_LOADED (-1)

// Room for the file name in a host command, including padding
#define HOST_FILE_NAME_SIZE_MAX 0x100

typedef struct BillboardTexture {
    WideAssetId assetId;
    s16 loadMode;
    s16 width;
    s16 height;
    u16 *pixels;
} BillboardTexture;

// The size of every billboard texture, in texels
#define BILLBOARD_TEXTURE_WIDTH 32
#define BILLBOARD_TEXTURE_HEIGHT 64

// A texture read from an asset when first drawn
#define BILLBOARD_TEXTURE(assetId) \
    {(assetId), TEXTURE_LOADED_ON_DEMAND, BILLBOARD_TEXTURE_WIDTH, BILLBOARD_TEXTURE_HEIGHT, NULL}

Billboard g_Billboards[BILLBOARD_COUNT];

FILE_STATIC BillboardTransform f_BillboardTransforms[BILLBOARD_COUNT];

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
#ifdef AVOID_UB
// DrawBillboards relights the shapes every frame, while the task drawing the
// previous frame, queued before, may not have read them yet. Each draw
// buffer's frame loads copies of the shapes instead.
static Vtx f_DrawnBillboardVertices[DRAW_BUFFER_COUNT][ARRAY_COUNT(f_BillboardVertices)]
                                   [QUAD_VERTEX_COUNT];

// Returns this frame's copy of the shape's vertices, taken now; a later copy
// in the frame overwrites it before the frame's task is queued.
static Vtx *DrawnShapeVertices(s32 shapeIndex) {
    Vtx *vertices = f_DrawnBillboardVertices[g_DrawBufferIndex][shapeIndex];

    memcpy(vertices, f_BillboardVertices[shapeIndex], sizeof(f_DrawnBillboardVertices[0][0]));
    return vertices;
}
#else
#define DrawnShapeVertices(shapeIndex) f_BillboardVertices[shapeIndex]
#endif

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
    BILLBOARD_TEXTURE(0x407), // Again, where 0x408 would follow
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
static Boolean8 f_IsHeapReady = FALSE;
static u8 *f_HeapStart = NULL;
static u8 *f_HeapEnd = NULL;
static u16 f_DrawOrder[BILLBOARD_COUNT] = {0};
static s8 f_UnusedBytesAfterDrawOrder[2] UNUSED = {1, 1};

// Billboards this close to the course's edges are hidden
#define COURSE_EDGE_X_MIN 256.0f
#define COURSE_EDGE_Z_MIN 256.0f
#define COURSE_EDGE_X_MAX 3840.0f
#define COURSE_EDGE_Z_MAX 7936.0f

// GetGroundHeight works in fixed point with 10 fraction bits
#define GROUND_HEIGHT_SCALE 1024

// GetViewDepth gives billboards closer than this to the camera a depth
// between it and 0
#define VIEW_DEPTH_NEAR (-0x800)

// Depth given to the ball when no billboard needs to stay out of its way
#define VIEW_DEPTH_BALL_HIDDEN (-0xFFFF)

// The least opacity of a billboard close to the camera
#define NEAR_BILLBOARD_ALPHA_MIN 0x80

// Billboards are only lit from a light at least this high
#define LIGHT_HEIGHT_MIN 0.1f

// In the debug view, near billboards this close in front of the ball are
// not drawn
#define BALL_CLEARANCE_DEPTH 100

// Depth of unused billboards, behind every used one
#define VIEW_DEPTH_FAR 0x40000000

// Like G_RM_ZB_XLU_SURF2, but also updating the depth buffer
#define BILLBOARD_RENDER_MODE_BLENDED2 \
    (Z_CMP | Z_UPD | IM_RD | CVG_DST_FULL | ZMODE_XLU | FORCE_BL | \
        GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA))

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

// The initial values of the colors local to DrawBillboards and
// DrawFadingBillboards, which both copy
const RgbColor g_BillboardLightColor = {255, 220, 160};
const RgbColor g_BillboardEnvColor = {128, 64, 32};

// Whether a point is near the edge of a course; no point of the
// mini-golf course is. It is a macro, as an inline function compiles
// differently from the original.
#define IsNearCourseEdge(x, z) \
    (!g_GameState.isMiniGolf && \
        (((x) <= COURSE_EDGE_X_MIN) || \
            ((z) <= COURSE_EDGE_Z_MIN) || \
            ((x) >= COURSE_EDGE_X_MAX) || \
            ((z) >= COURSE_EDGE_Z_MAX)))

void PlaceBillboards(void) {
    s32 billboardIndex;
    f32 x;
    f32 z;
    f32 y;

    for (billboardIndex = 0; billboardIndex != BILLBOARD_COUNT; billboardIndex++) {
        f_DrawOrder[billboardIndex] = billboardIndex;
    }

    for (billboardIndex = 0; billboardIndex < BILLBOARD_COUNT; billboardIndex++) {
        if (g_Billboards[billboardIndex].textureIndex >= 0) {
            x = g_Billboards[billboardIndex].x;
            z = g_Billboards[billboardIndex].z;
            if (IsNearCourseEdge(x, z)) {
                g_Billboards[billboardIndex].isNearCourseEdge = TRUE;
            }
            else {
                g_Billboards[billboardIndex].isNearCourseEdge = FALSE;
            }

            y = GetGroundHeight(x * GROUND_HEIGHT_SCALE, z * GROUND_HEIGHT_SCALE) /
                GROUND_HEIGHT_SCALE;
            g_Billboards[billboardIndex].y = y;
            guTranslate(&f_BillboardTransforms[billboardIndex].translation, x, y, z);
            guScale(
                &f_BillboardTransforms[billboardIndex].scale,
                g_Billboards[billboardIndex].width,
                g_Billboards[billboardIndex].height,
                1.0f
            );
        }
    }
}

void SortBillboardsByDepth(void) {
    s32 orderIndex;
    s32 sortedIndex;
    u16 insertedIndex;

    for (orderIndex = 0; orderIndex != BILLBOARD_COUNT; orderIndex++) {
        if (g_Billboards[orderIndex].textureIndex < 0) {
            f_BillboardTransforms[orderIndex].viewDepth = VIEW_DEPTH_FAR;
        }
        else {
            f_BillboardTransforms[orderIndex].viewDepth = GetViewDepth(
                g_Billboards[orderIndex].x,
                g_Billboards[orderIndex].y,
                g_Billboards[orderIndex].z
            );
        }
    }

    // Insertion sort, farthest first
    for (orderIndex = 1; orderIndex != BILLBOARD_COUNT; orderIndex++) {
        insertedIndex = f_DrawOrder[orderIndex];
        for (sortedIndex = orderIndex - 1; sortedIndex >= 0 &&
            f_BillboardTransforms[f_DrawOrder[sortedIndex]].viewDepth <
                f_BillboardTransforms[insertedIndex].viewDepth;
            sortedIndex--) {
            f_DrawOrder[sortedIndex + 1] = f_DrawOrder[sortedIndex];
        }

        f_DrawOrder[sortedIndex + 1] = insertedIndex;
    }
}

// Whether a billboard's texture is one read in on demand, has been read
// and is not the one loaded for drawing. It is a macro, as an inline function
// compiles differently from the original.
#define NeedsTextureLoad(billboard, textureIndex) \
    (((billboard)->textureIndex != f_LoadedTextureIndex) && \
        (f_BillboardTextures[textureIndex].loadMode == TEXTURE_LOADED_ON_DEMAND) && \
        (f_BillboardTextures[textureIndex].pixels != NULL))

void DrawBillboard(
    Gfx **displayListHead,
    Billboard *billboard,
    BillboardTransform *transform,
    s32 textureIndex,
    CameraMatrices *matrices
) {
    Gfx *gfx = *displayListHead;
    AssetFile file;
    s32 texelSize;

    if (f_BillboardTextures[textureIndex].pixels == NULL &&
        f_BillboardTextures[textureIndex].loadMode == TEXTURE_LOADED_ON_DEMAND) {
        texelSize = f_BillboardTextures[textureIndex].width *
            f_BillboardTextures[textureIndex].height * sizeof(u16);
        f_BillboardTextures[textureIndex].pixels =
            AllocFromHeap(HEAP_BILLBOARD, texelSize + IMAGE_HEADER_SIZE);
        if (f_BillboardTextures[textureIndex].pixels != NULL) {
            OpenAssetFile(f_BillboardTextures[textureIndex].assetId, &file);
            ReadAssetFile(f_BillboardTextures[textureIndex].pixels, IMAGE_HEADER_SIZE, &file);
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
    gSPMatrix(gfx++, &matrices->billboard, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
    gSPMatrix(gfx++, &transform->scale, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, DrawnShapeVertices(billboard->shapeIndex), QUAD_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    f_LoadedTextureIndex = billboard->textureIndex;
    *displayListHead = gfx;
}

// Draws the billboards. Those far from the camera are drawn first, from
// the nearest to the farthest; DrawFadingBillboards draws the close ones
// that need to stay see-through. Here, the close ones are only drawn when
// they stay opaque.
void DrawBillboards(CameraMatrices *matrices, s32 cameraIndex) {
    Gfx *gfx = g_DisplayListHead;
    RgbColor lightColor UNUSED = g_BillboardLightColor;
    RgbColor envColor = g_BillboardEnvColor;
    Vec3 viewDirection;
    Vec3 lightDirection;
    // Nothing uses it, but the original's frame has the room for it
    Vec3 unusedVector UNUSED;
    Radians lightPitch;
    Radians lightYaw;
    f32 alignment;
    f32 lightHeight;
    f32 leftAlignment;
    f32 rightAlignment;
    Boolean hasTranslucentMode;
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
    viewDirection[AXIS_X] =
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] - g_Cameras[MAIN_CAMERA].view.eye[AXIS_X];
    viewDirection[AXIS_Y] = 0.0f;
    viewDirection[AXIS_Z] =
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] - g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z];
    lightDirection[AXIS_X] = cosf(lightPitch) * cosf(lightYaw);
    lightDirection[AXIS_Z] = cosf(lightPitch) * sinf(lightYaw);
    lightDirection[AXIS_Y] = sinf(lightPitch);
    if (fabsf(viewDirection[AXIS_X]) + fabsf(viewDirection[AXIS_Z]) >= 1.0f) {
        guNormalize(&viewDirection[AXIS_X], &viewDirection[AXIS_Y], &viewDirection[AXIS_Z]);
        guNormalize(&lightDirection[AXIS_X], &lightDirection[AXIS_Y], &lightDirection[AXIS_Z]);
        alignment = (viewDirection[AXIS_X] * lightDirection[AXIS_X]) +
            (viewDirection[AXIS_Z] * lightDirection[AXIS_Z]);
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

            f_BillboardVertices[0][0].v.cn[RED_CHANNEL] =
                (((-lightHeight * 0.5f) + (leftAlignment * 0.5f)) * 127.0f) + 255.0f;
            f_BillboardVertices[0][0].v.cn[GREEN_CHANNEL] =
                (((-lightHeight * 0.5f) + (leftAlignment * 0.5f)) * 127.0f) + 255.0f;
            f_BillboardVertices[0][0].v.cn[BLUE_CHANNEL] =
                (((-lightHeight * 0.5f) + (leftAlignment * 0.5f)) * 127.0f) + 255.0f;
            f_BillboardVertices[0][1].v.cn[RED_CHANNEL] =
                (((-lightHeight * 0.5f) + (rightAlignment * 0.5f)) * 127.0f) + 255.0f;
            f_BillboardVertices[0][1].v.cn[GREEN_CHANNEL] =
                (((-lightHeight * 0.5f) + (rightAlignment * 0.5f)) * 127.0f) + 255.0f;
            f_BillboardVertices[0][1].v.cn[BLUE_CHANNEL] =
                (((-lightHeight * 0.5f) + (rightAlignment * 0.5f)) * 127.0f) + 255.0f;
            f_BillboardVertices[0][2].v.cn[RED_CHANNEL] =
                (((-lightHeight * 0.5f) + (rightAlignment * 0.5f)) * 255.0f) + 255.0f;
            f_BillboardVertices[0][2].v.cn[GREEN_CHANNEL] =
                (((-lightHeight * 0.5f) + (rightAlignment * 0.5f)) * 255.0f) + 255.0f;
            f_BillboardVertices[0][2].v.cn[BLUE_CHANNEL] =
                (((-lightHeight * 0.5f) + (rightAlignment * 0.5f)) * 255.0f) + 255.0f;
            f_BillboardVertices[0][3].v.cn[RED_CHANNEL] =
                (((-lightHeight * 0.5f) + (leftAlignment * 0.5f)) * 255.0f) + 255.0f;
            f_BillboardVertices[0][3].v.cn[GREEN_CHANNEL] =
                (((-lightHeight * 0.5f) + (leftAlignment * 0.5f)) * 255.0f) + 255.0f;
            f_BillboardVertices[0][3].v.cn[BLUE_CHANNEL] =
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

        if (f_BillboardTransforms[billboardIndex].viewDepth < VIEW_DEPTH_NEAR) {
            DrawBillboard(
                &gfx,
                &g_Billboards[billboardIndex],
                &f_BillboardTransforms[billboardIndex],
                textureIndex,
                matrices
            );
        }
    }

    gDPPipeSync(gfx++);
    hasTranslucentMode = FALSE;
    gDPSetRenderMode(gfx++, G_RM_FOG_SHADE_A, BILLBOARD_RENDER_MODE_BLENDED2);
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

        viewDepth = f_BillboardTransforms[billboardIndex].viewDepth;
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
            f_BillboardTransforms[billboardIndex].viewDepth > ballDepth - BALL_CLEARANCE_DEPTH) {
            continue;
        }

        if (alpha < OPAQUE_ALPHA) {
            continue;
        }

        // Never true, as this loop draws no fading billboard that would set
        // the flag, and the compiler drops the test: it only leaves the
        // flag's unread register and 16 unused bytes of the frame. So the
        // mode restored here is a guess; the match needs the test against
        // TRUE, the flag cleared and any command drawn
        if (hasTranslucentMode == TRUE && alpha == OPAQUE_ALPHA) {
            hasTranslucentMode = FALSE;
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_FOG_SHADE_A, BILLBOARD_RENDER_MODE_BLENDED2);
        }

        DrawBillboard(
            &gfx,
            &g_Billboards[billboardIndex],
            &f_BillboardTransforms[billboardIndex],
            textureIndex,
            matrices
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

// Whether the billboards are hidden: by the game flag, while their heap
// is not ready, or by the debug view. It is a macro, as an inline
// function compiles differently from the original.
#define AreBillboardsHidden() \
    (IsGameFlagSet(GAME_FLAG_HIDE_BILLBOARDS) || !f_IsHeapReady || IsDebugBButtonView())

// Draws the billboards close to the camera, translucent so that the camera
// can see through them, from the farthest to the nearest. Those farther
// than the ball stay opaque.
void DrawFadingBillboards(CameraMatrices *matrices, s32 cameraIndex) {
    Gfx *gfx = g_DisplayListHead;
    RgbColor lightColor UNUSED = g_BillboardLightColor;
    RgbColor envColor = g_BillboardEnvColor;
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

    // The blended mode set here is not the translucent surface mode that the
    // loop switches to
    hasTranslucentMode = FALSE;
    gDPSetRenderMode(gfx++, G_RM_FOG_SHADE_A, BILLBOARD_RENDER_MODE_BLENDED2);
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

        viewDepth = f_BillboardTransforms[billboardIndex].viewDepth;
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
            &f_BillboardTransforms[billboardIndex],
            textureIndex,
            matrices
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

// Development hardware could pass files to the host computer. These two
// routines remain from that, but send their commands nowhere.

enum HostCommand {
    HOST_COMMAND_WRITE_FILE = 10,
    HOST_COMMAND_READ_FILE
};

HostFileResult WriteHostFile(const char *fileName, u32 size) {
    u32 *command;
    char *nameBuffer;
    u32 nameSize;
    HostFileResult result;

    if (size == 0) {
        // "The transfer size is too small."
        osSyncPrintf("転送サイズが小さいです。");
        return HOST_FILE_SIZE_TOO_SMALL;
    }

    nameSize = strlen(fileName);
    if (nameSize == 0) {
        // "The file name is too short"
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
        // "The file name is too long"
        osSyncPrintf("ファイル名が長すぎます");
        result = HOST_FILE_NAME_TOO_LONG;
    }

    return result;
}

HostFileResult ReadHostFile(const char *fileName, u32 *size) {
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
        // "The file has no data."
        osSyncPrintf("ファイルにデータがありません。");
    }

    FreeMemory(&command);
    FreeMemory(&nameBuffer);
    return 1;
}
