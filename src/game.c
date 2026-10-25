#include "common.h"
#include "billboard.h"
#include "controller.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "debug_text.h"
#include "game_flags.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "hole_triangles.h"
#include "libc.h"
#include "main.h"
#include "camera.h"
#include "character_slot.h"
#include "functions.h"
#include <lib/libnusys/nusys.h>
#include "overlay_manager.h"
#include "performance_meter.h"
#include "pin.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "variables.h"
#include "work_area.h"

/* Sub-modules that RequestSubModule loads for some game modes */
#define SUB_MODULE_DEFAULT 0x05
#define SUB_MODULE_ALTERNATE 0x15
#define SUB_MODULE_SPECIAL 0x1A

/* Sounds that the inset view plays as it opens and closes */
#define SOUND_INSET_VIEW_OPEN 0x64
#define SOUND_INSET_VIEW_CLOSE 0x65
#define INSET_VIEW_SOUND_VOLUME 0x3F
#define INSET_VIEW_SOUND_REVERB 0x1E
#define INSET_VIEW_SOUND_PAN 0x7F

/* Steps that the inset view takes to open or close fully */
#define INSET_VIEW_STEP_COUNT 8

extern s32 sInsetViewCenterX;
extern s32 sInsetViewCenterY;
extern s32 sInsetViewWidth;
extern s32 sInsetViewHeight;
void DrawSpinEffect(Gfx **displayListHead, CameraView *view);
void DrawSavedFrameEffect(Gfx **displayListHead, CameraView *view);

/*
 * The spin effect's mesh and its rotation, one per draw buffer. Each draw
 * buffer's vertices start one vertex after the other's, so most are shared,
 * and sSpinVertices runs on over sSpinMatrices, so a frame's vertex updates
 * overwrite parts of the matrices.
 */
extern Vtx sSpinVertices[];
extern Mtx sSpinMatrices[];

extern f32 sSpinAngle; /* In radians */

/* The frame CopyPreviousFrame copies, and the one shown last */
extern u16 *sCopySourceFrame;
extern u16 *sPreviousFrame;

/* When the last graphics task started, in microseconds */
extern u32 sGfxTaskStartTime;

/* When UpdateGame last started and finished, in microseconds */
extern u32 sUpdateStartTime;
extern u32 sUpdateEndTime;

/* Values a debug menu edits; nothing reads them */
#define DEBUG_MENU_VALUE_COUNT 3
#define DEBUG_MENU_VALUE_MAX 128
extern s32 sDebugMenuValues[DEBUG_MENU_VALUE_COUNT];
extern s32 sDebugMenuValueIndex;

/* The sources of g_FlameTexture */
extern u8 sNoiseTexture[];
extern u16 sFlameGradientTexture[];

/* How far BuildDynamicTextures has scrolled its textures */
extern s32 sDynamicTextureScroll;

/* How far BuildHorizonDynamicTextures has scrolled its textures */
extern s32 sHorizonTextureScroll;

extern s32 sScreenEffect;
/* Holds a frame for the saved-frame effect */
extern u16 *sSavedFrameBuffer;
/* The effect's length in frames; the tiled image counts it down */
extern s32 sScreenEffectFrameCount;
/* Set when the effects stop, never read */
extern s32 sUnreadEffectSetting;
extern s32 sHasUnloadedSceneModule;
extern s32 sHasUnloadedSubModule;

/* How the saved-frame effect splits the screen into strips, in rows */
#define SAVED_FRAME_STRIP_HEIGHT 6
#define SAVED_FRAME_STRIP_COUNT 38
#define SAVED_FRAME_LAST_STRIP_HEIGHT 2

/*
 * The spin effect's mesh holds two vertices per row, loaded in batches of
 * up to SPIN_BATCH_VERTEX_COUNT
 */
#define SPIN_BATCH_COUNT 3
#define SPIN_BATCH_VERTEX_COUNT 32
#define SPIN_LAST_BATCH_VERTEX_COUNT 18
#define SPIN_LAST_BATCH_STRIP_COUNT 8
/*
 * The mesh's model is scaled down by SPIN_MESH_SCALE, so it spans the screen
 * inside its border at SPIN_MESH_DISTANCE from the camera
 */
#define SPIN_MESH_ROW_COUNT 42
#define SPIN_MESH_HALF_WIDTH ((SCREEN_WIDTH / 2 - SCREEN_BORDER) * 10)
#define SPIN_MESH_SCALE 0.1f
#define SPIN_MESH_DISTANCE -290.0f
#define IS_LAST_SPIN_STRIP(batchIndex, stripIndex) \
    (((batchIndex) == SPIN_BATCH_COUNT - 1) & ((stripIndex) == SPIN_LAST_BATCH_STRIP_COUNT - 1))

#define RESET_FADE_ALPHA 0x40

/* The scenes are still known only by number */

/*
 * In scene 2, the C buttons move the map camera, the stick turns and raises
 * it, and it looks ahead along the ground
 */
#define MAP_CAMERA_STEP 10.0f
#define MAP_CAMERA_HEIGHT 120.0f
#define MAP_CAMERA_LIFT_PER_STICK 32
#define MAP_CAMERA_TURN_PER_STICK 1024.0
#define MAP_CAMERA_LOOK_DISTANCE 1000.0f

/* GetGroundHeight's units per world unit */
#define GROUND_UNITS_PER_UNIT 1024

/*
 * Scene 14 opens a larger, square inset view, in more steps and without
 * sounds. Each step adds this many pixels to its sides.
 */
#define LARGE_INSET_VIEW_STEP_COUNT 32
#define LARGE_INSET_VIEW_GROWTH_PER_STEP 4
#define LARGE_INSET_VIEW_CENTER_X 160
#define LARGE_INSET_VIEW_CENTER_Y 140

/* Where the debug menu draws its values, in text cells */
#define DEBUG_MENU_COLUMN 34
#define DEBUG_MENU_CURSOR_COLUMN 33
#define DEBUG_MENU_ROW 3

/*
 * BuildDynamicTextures shrinks a strip of the frame into g_MirrorTexture
 * by copying blocks of MIRROR_BLOCK_WIDTH pixels, spaced out across it,
 * from every MIRROR_ROW_SPACING-th row
 */
#define MIRROR_BLOCK_COUNT 4
#define MIRROR_BLOCK_WIDTH (DYNAMIC_TEXTURE_SIZE / MIRROR_BLOCK_COUNT)
#define MIRROR_SOURCE_LEFT 34
#define MIRROR_SOURCE_TOP 48
#define MIRROR_BLOCK_SPACING 76
#define MIRROR_ROW_SPACING 3

/* How strongly each layer covers what is under it */
#define MIRROR_TINT_ALPHA 0xC8
#define MIRROR_NOISE_ALPHA 0x9F
#define FLAME_NOISE_ALPHA 0x30
#define BLENDED_TEXTURE_ALPHA 0x7F

/*
 * BuildHorizonDynamicTextures copies every HORIZON_ROW_SPACING-th row of
 * a strip of the frame that starts HORIZON_STRIP_RISE rows above the
 * horizon and ends at HORIZON_STRIP_BOTTOM
 */
#define HORIZON_STRIP_RISE 70
#define HORIZON_FIRST_ROW 8
#define HORIZON_ROW_SPACING 7
#define HORIZON_STRIP_BOTTOM 224

/* The window of the saved image that the tiled-image effect repeats */
#define TILED_IMAGE_LEFT 64
#define TILED_IMAGE_RIGHT 191
#define TILED_IMAGE_TOP 104
#define TILED_IMAGE_STRIP_HEIGHT 16
#define TILED_IMAGE_STRIP_COUNT 6
#define TILED_IMAGE_MASK_S 7 /* Wraps s every 2^7 = 128 texels, the window's width */

/* How the tiled-image effect lays the window out on the screen */
#define TILED_IMAGE_STRIP_SCREEN_HEIGHT 13
#define TILED_IMAGE_COPY_COUNT 4
#define TILED_IMAGE_COPY_SPACING 72
#define TILED_IMAGE_SCALE 0x04EC /* Texels per pixel, s5.10: about 1.23 */

/* Turns gamma correction off and gamma dithering and the dither filter on */
#define VI_FEATURES (OS_VI_GAMMA_OFF | OS_VI_GAMMA_DITHER_ON | OS_VI_DITHER_FILTER_ON)

void StartGame(void) {
    g_MapCamera.view.eye[0] = 0.0f;
    g_MapCamera.view.eye[1] = 0.0f;
    g_MapCamera.view.eye[2] = 0.0f;
    g_MapCamera.view.lookAt[0] = 0.0f;
    g_MapCamera.view.lookAt[1] = 0.0f;
    g_MapCamera.view.lookAt[2] = 1.0f;
    g_MapCamera.view.heading = 0.0f;
    g_MapCamera.view.scissorX = SCREEN_BORDER;
    g_MapCamera.view.scissorY = SCREEN_BORDER;
    g_MapCamera.view.scissorWidth = SCREEN_WIDTH - 2 * SCREEN_BORDER;
    g_MapCamera.view.scissorHeight = SCREEN_HEIGHT - 2 * SCREEN_BORDER;
    g_MapCamera.view.unreadSettings[0] = SCREEN_HEIGHT - 2 * SCREEN_BORDER;
    g_MapCamera.view.unreadSettings[1] = SCREEN_HEIGHT - 2 * SCREEN_BORDER;
    g_MapCamera.view.viewportCenterX = SCREEN_WIDTH;
    g_MapCamera.view.viewportCenterY = SCREEN_HEIGHT;
    g_MapCamera.view.zoom = 1.0f;
    g_SpareCamera.view.eye[0] = 0.0f;
    g_SpareCamera.view.eye[1] = 0.0f;
    g_SpareCamera.view.eye[2] = 0.0f;
    g_SpareCamera.view.lookAt[0] = 0.0f;
    g_SpareCamera.view.lookAt[1] = 0.0f;
    g_SpareCamera.view.lookAt[2] = 1.0f;
    g_SpareCamera.view.heading = 0.0f;
    g_SpareCamera.view.scissorX = 128;
    g_SpareCamera.view.scissorY = 64;
    g_SpareCamera.view.scissorWidth = 160;
    g_SpareCamera.view.scissorHeight = 120;
    g_SpareCamera.view.unreadSettings[0] = 0;
    g_SpareCamera.view.unreadSettings[1] = 30;
    g_SpareCamera.view.viewportCenterX = 416;
    g_SpareCamera.view.viewportCenterY = 248;
    g_SpareCamera.view.zoom = 0.5f;
    g_Cameras[0].view.mode = CAMERA_MODE_BEHIND_GOLFER;
    g_Cameras[0].view.eye[0] = 0.0f;
    g_Cameras[0].view.eye[1] = 0.0f;
    g_Cameras[0].view.eye[2] = 0.0f;
    g_Cameras[0].view.lookAt[0] = 0.0f;
    g_Cameras[0].view.lookAt[1] = 0.0f;
    g_Cameras[0].view.lookAt[2] = -100.0f;
    g_Cameras[0].view.scissorX = SCREEN_BORDER;
    g_Cameras[0].view.scissorY = SCREEN_BORDER;
    g_Cameras[0].view.scissorWidth = SCREEN_WIDTH - 2 * SCREEN_BORDER;
    g_Cameras[0].view.scissorHeight = SCREEN_HEIGHT - 2 * SCREEN_BORDER;
    g_Cameras[0].view.unreadSettings[0] = SCREEN_HEIGHT - 2 * SCREEN_BORDER;
    g_Cameras[0].view.unreadSettings[1] = SCREEN_HEIGHT - 2 * SCREEN_BORDER;
    g_Cameras[0].view.zoom = 1.0f;
    g_Cameras[0].view.viewportCenterX = SCREEN_WIDTH;
    g_Cameras[0].view.viewportCenterY = SCREEN_HEIGHT;
    g_Cameras[0].view.drawFlags = CAMERA_DRAW_SCENE;
    g_Cameras[0].view.fieldOfView = 45.0f;
    g_Cameras[1].view.mode = CAMERA_MODE_BEHIND_GOLFER;
    g_Cameras[1].view.eye[0] = 0.0f;
    g_Cameras[1].view.eye[1] = 0.0f;
    g_Cameras[1].view.eye[2] = 0.0f;
    g_Cameras[1].view.lookAt[0] = 0.0f;
    g_Cameras[1].view.lookAt[1] = 0.0f;
    g_Cameras[1].view.lookAt[2] = -100.0f;
    g_Cameras[1].view.scissorX = 64;
    g_Cameras[1].view.scissorY = 96;
    g_Cameras[1].view.scissorWidth = 128;
    g_Cameras[1].view.scissorHeight = 64;
    g_Cameras[1].view.unreadSettings[0] = 0;
    g_Cameras[1].view.unreadSettings[1] = 0;
    g_Cameras[1].view.zoom = 0.9f;
    g_Cameras[1].view.viewportCenterX = 256;
    g_Cameras[1].view.viewportCenterY = 256;
    g_Cameras[1].view.drawFlags = 0;
    g_Cameras[1].view.fieldOfView = 60.0f;
    osViSetSpecialFeatures(VI_FEATURES);
    ClearDebugText();
    LoadTextAssets();
    func_80081284();
    LoadPinTextures();

}

void TranslateMatrix(Mtx *matrix, f32 x, f32 y, f32 z) {
    Mtx translation;

    guTranslate(&translation, x, y, z);
    guMtxCatL(matrix, &translation, matrix);
}

void ScaleMatrix(Mtx *matrix, f32 x, f32 y, f32 z) {
    Mtx scale;

    guScale(&scale, x, y, z);
    guMtxCatL(matrix, &scale, matrix);
}

void SetScreenEffect(s32 effect, s32 frameCount) {
    sScreenEffect = effect;
    sScreenEffectFrameCount = frameCount;
    if (effect == SCREEN_EFFECT_NONE) {
        g_Cameras[0].view.drawFlags = CAMERA_DRAW_SCENE;
        sUnreadEffectSetting = -640;
    } else {
        g_Cameras[0].view.drawFlags = CAMERA_DRAW_SCENE | CAMERA_DRAW_SCREEN_EFFECT;
    }
}

/*
 * Draws a 128 by 96 window of the image saved in the depth buffer, shrunk
 * and repeated across the screen, for a number of frames
 */
void DrawTiledImageEffect(Gfx **displayListHead, CameraView *view) {
    Gfx *gfx = *displayListHead;
    s32 stripTop;
    s32 texelPairOffset;
    s32 copyIndex;

    sScreenEffectFrameCount--;
    if (sScreenEffectFrameCount == 0) {
        view->drawFlags = CAMERA_DRAW_SCENE;
    }
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(gfx++);
    texelPairOffset = TILED_IMAGE_TOP * SCREEN_WIDTH / 2;
    for (stripTop = 0; stripTop != TILED_IMAGE_STRIP_COUNT * TILED_IMAGE_STRIP_SCREEN_HEIGHT;
         stripTop += TILED_IMAGE_STRIP_SCREEN_HEIGHT) {
        /* Each 32-bit step of texelPairOffset covers two 16-bit texels */
        gDPLoadTextureTile(gfx++, (u32)((u32 *)g_DepthBuffer + texelPairOffset) & ~7, G_IM_FMT_RGBA,
                           G_IM_SIZ_16b, SCREEN_WIDTH, 0, TILED_IMAGE_LEFT, 0, TILED_IMAGE_RIGHT,
                           TILED_IMAGE_STRIP_HEIGHT - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                           TILED_IMAGE_MASK_S, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        texelPairOffset += TILED_IMAGE_STRIP_HEIGHT * SCREEN_WIDTH / 2;
        for (copyIndex = 0; copyIndex != TILED_IMAGE_COPY_COUNT; copyIndex++) {
            gSPScisTextureRectangle(
                gfx++, SCREEN_BORDER << 2, (SCREEN_BORDER + copyIndex * TILED_IMAGE_COPY_SPACING + stripTop) << 2,
                (SCREEN_WIDTH - SCREEN_BORDER) << 2,
                (SCREEN_BORDER + copyIndex * TILED_IMAGE_COPY_SPACING + stripTop + TILED_IMAGE_STRIP_SCREEN_HEIGHT) << 2,
                G_TX_RENDERTILE, TILED_IMAGE_LEFT << 5, 0, TILED_IMAGE_SCALE, TILED_IMAGE_SCALE);
        }
    }
    *displayListHead = gfx;
}

/*
 * Draws the frame saved in the depth buffer on the spin effect's mesh: one
 * strip of the frame between each pair of rows of vertices
 */
void DrawSpinMesh(Gfx **displayListHead) {
    s32 isTileSetUp = FALSE;
    Gfx *gfx = *displayListHead;
    s32 texelPairOffset = SCREEN_BORDER * SCREEN_WIDTH / 2;
    s32 batchIndex;
    s32 stripIndex;
    s32 stripHeight;
    u8 isLastStrip;

    gSPMatrix(gfx++, &g_ScreenEffectProjection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gfx++, &sSpinMatrices[g_DrawBufferIndex], G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
    for (batchIndex = 0; batchIndex != SPIN_BATCH_COUNT; batchIndex++) {
        if (batchIndex == SPIN_BATCH_COUNT - 1) {
            gSPVertex(gfx++, &sSpinVertices[g_DrawBufferIndex] + batchIndex * SPIN_BATCH_VERTEX_COUNT,
                      SPIN_LAST_BATCH_VERTEX_COUNT, 0);
        } else {
            gSPVertex(gfx++, &sSpinVertices[g_DrawBufferIndex] + batchIndex * SPIN_BATCH_VERTEX_COUNT,
                      SPIN_BATCH_VERTEX_COUNT, 0);
        }
        for (stripIndex = 0; stripIndex != SPIN_BATCH_VERTEX_COUNT / 2 - 1; stripIndex++) {
            isLastStrip = IS_LAST_SPIN_STRIP(batchIndex, stripIndex);
            stripHeight = isLastStrip ? SAVED_FRAME_LAST_STRIP_HEIGHT : SAVED_FRAME_STRIP_HEIGHT;
            if (!isTileSetUp) {
                gDPLoadTextureBlock(gfx++, (u32)((u32 *)g_DepthBuffer + texelPairOffset) & ~7, G_IM_FMT_RGBA,
                                    G_IM_SIZ_16b, SCREEN_WIDTH, stripHeight, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
                isTileSetUp = TRUE;
            } else {
                gDPSetTextureImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1,
                                   (u32)((u32 *)g_DepthBuffer + texelPairOffset) & ~7);
                gDPLoadSync(gfx++);
                gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, SCREEN_WIDTH * stripHeight - 1,
                             CALC_DXT(SCREEN_WIDTH, G_IM_SIZ_16b_BYTES));
                gDPPipeSync(gfx++);
            }
            gDPLoadTextureTile(gfx++, (u32)((u32 *)g_DepthBuffer + texelPairOffset) & ~7, G_IM_FMT_RGBA,
                               G_IM_SIZ_16b, SCREEN_WIDTH, 0, SCREEN_BORDER, 0, SCREEN_WIDTH - SCREEN_BORDER - 1,
                               stripHeight - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                               G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            /* The original tests again here; the build depends on it */
            isLastStrip = IS_LAST_SPIN_STRIP(batchIndex, stripIndex);
            stripHeight = isLastStrip ? SAVED_FRAME_LAST_STRIP_HEIGHT : SAVED_FRAME_STRIP_HEIGHT;
            gSPModifyVertex(gfx++, stripIndex * 2, G_MWO_POINT_ST, SCREEN_BORDER << 21);
            gSPModifyVertex(gfx++, stripIndex * 2 + 1, G_MWO_POINT_ST, (SCREEN_WIDTH - SCREEN_BORDER) << 21);
            gSPModifyVertex(gfx++, stripIndex * 2 + 2, G_MWO_POINT_ST, (SCREEN_BORDER << 21) + (stripHeight << 5));
            gSPModifyVertex(gfx++, stripIndex * 2 + 3, G_MWO_POINT_ST,
                            ((SCREEN_WIDTH - SCREEN_BORDER) << 21) + (stripHeight << 5));
            gSP2Triangles(gfx++, stripIndex * 2, stripIndex * 2 + 2, stripIndex * 2 + 1, 0, stripIndex * 2 + 1,
                          stripIndex * 2 + 2, stripIndex * 2 + 3, 0);
            gDPPipeSync(gfx++);
            if (isLastStrip) {
                break;
            }
            texelPairOffset += SAVED_FRAME_STRIP_HEIGHT * SCREEN_WIDTH / 2;
        }
    }
    *displayListHead = gfx;
}

/*
 * Copies the frame kept in sSavedFrameBuffer to the screen, through the
 * depth buffer's memory, a strip at a time in copy mode
 */
void DrawSavedFrameEffect(Gfx **displayListHead, CameraView *view) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s16 stripHeight;

    sScreenEffectFrameCount = 1;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_COPY);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, (u32)nuGfxZBuffer & ~7);
    gDPPipeSync(gfx++);
    for (stripIndex = 0; stripIndex != SAVED_FRAME_STRIP_COUNT; stripIndex++) {
        stripHeight = (stripIndex == SAVED_FRAME_STRIP_COUNT - 1) ? SAVED_FRAME_LAST_STRIP_HEIGHT
                                                                  : SAVED_FRAME_STRIP_HEIGHT;
        gDPLoadTextureTile(gfx++, (u32)(sSavedFrameBuffer + stripIndex * SAVED_FRAME_STRIP_HEIGHT * SCREEN_WIDTH) & ~7,
                           G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, 0, SCREEN_BORDER, SCREEN_BORDER,
                           SCREEN_WIDTH - SCREEN_BORDER - 1, SCREEN_BORDER + stripHeight - 1, 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, SCREEN_BORDER << 2, (SCREEN_BORDER + stripIndex * SAVED_FRAME_STRIP_HEIGHT) << 2,
                                (SCREEN_WIDTH - SCREEN_BORDER - 1) << 2,
                                (stripIndex * SAVED_FRAME_STRIP_HEIGHT + SCREEN_BORDER + stripHeight - 1) << 2,
                                G_TX_RENDERTILE, SCREEN_BORDER << 5, SCREEN_BORDER << 5, 4 << 10, 1 << 10);
        gDPPipeSync(gfx++);
    }
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
    gDPPipeSync(gfx++);
    for (stripIndex = 0; stripIndex != SAVED_FRAME_STRIP_COUNT; stripIndex++) {
        stripHeight = (stripIndex == SAVED_FRAME_STRIP_COUNT - 1) ? SAVED_FRAME_LAST_STRIP_HEIGHT
                                                                  : SAVED_FRAME_STRIP_HEIGHT;
        gDPLoadTextureTile(gfx++, (u32)(g_DepthBuffer + stripIndex * SAVED_FRAME_STRIP_HEIGHT * SCREEN_WIDTH) & ~7,
                           G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, 0, SCREEN_BORDER, SCREEN_BORDER,
                           SCREEN_WIDTH - SCREEN_BORDER - 1, SCREEN_BORDER + stripHeight - 1, 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, SCREEN_BORDER << 2, (SCREEN_BORDER + stripIndex * SAVED_FRAME_STRIP_HEIGHT) << 2,
                                (SCREEN_WIDTH - SCREEN_BORDER - 1) << 2,
                                (stripIndex * SAVED_FRAME_STRIP_HEIGHT + SCREEN_BORDER + stripHeight - 1) << 2,
                                G_TX_RENDERTILE, SCREEN_BORDER << 5, SCREEN_BORDER << 5, 4 << 10, 1 << 10);
        gDPPipeSync(gfx++);
    }
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/*
 * Turns the frame saved in the depth buffer about the vertical axis, a half
 * turn over sScreenEffectFrameCount frames, then shows the scene again
 */
void DrawSpinEffect(Gfx **displayListHead, CameraView *view) {
    Mat4 transform;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Gfx *gfx = *displayListHead;
    f32 angleStep;
    Vtx *vertex;
    s32 rowIndex;

    rotation[0] = 0.0f;
    rotation[1] = sSpinAngle;
    rotation[2] = 0.0f;
    translation[0] = 0.0f;
    translation[1] = 0.0f;
    translation[2] = SPIN_MESH_DISTANCE;
    scale[0] = SPIN_MESH_SCALE;
    scale[1] = SPIN_MESH_SCALE;
    scale[2] = SPIN_MESH_SCALE;
    BuildTransformMatrix(transform, rotation, translation, scale);
    ConvertMatrixToFixed(transform, &sSpinMatrices[g_DrawBufferIndex]);

    angleStep = M_PI / sScreenEffectFrameCount;
    sSpinAngle += angleStep;
    /* Skips the half turn in which the frame's back would face the camera */
    if (sSpinAngle > M_PI / 2 && sSpinAngle < angleStep + M_PI / 2) {
        sSpinAngle += M_PI;
    }
    if (sSpinAngle > 2 * M_PI) {
        sSpinAngle = 0.0f;
        view->drawFlags = CAMERA_DRAW_SCENE;
    }

    vertex = &sSpinVertices[g_DrawBufferIndex];
    for (rowIndex = 0; rowIndex != SPIN_MESH_ROW_COUNT; rowIndex++) {
        vertex[0].v.ob[0] = -SPIN_MESH_HALF_WIDTH;
        vertex[1].v.ob[0] = SPIN_MESH_HALF_WIDTH;
        vertex += 2;
    }

    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPPipeSync(gfx++);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1));
    gDPFillRectangle(gfx++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, view->scissorX, view->scissorY, view->scissorX + view->scissorWidth,
                  view->scissorY + view->scissorHeight);
    gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
    gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPSetCombineMode(gfx++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    DrawSpinMesh(&gfx);
    *displayListHead = gfx;
}

void DrawEmptyEffect(Gfx **displayListHead, CameraView *view) {
}

void DrawScreenEffect(CameraMatrices *matrices, CameraView *view) {
    /* The spin effect pushes a modelview matrix and changes the projection */
    if (sScreenEffect == SCREEN_EFFECT_SPIN) {
        DrawSpinEffect(&g_DisplayListHead, view);
        gSPPopMatrix(g_DisplayListHead++, G_MTX_MODELVIEW);
        gSPMatrix(g_DisplayListHead++, &matrices->projection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    } else if (sScreenEffect == SCREEN_EFFECT_TILED_IMAGE) {
        DrawTiledImageEffect(&g_DisplayListHead, view);
    } else if (sScreenEffect == SCREEN_EFFECT_EMPTY) {
        DrawEmptyEffect(&g_DisplayListHead, view);
        /* Pops although DrawEmptyEffect pushed nothing, as if it once spun too */
        gSPPopMatrix(g_DisplayListHead++, G_MTX_MODELVIEW);
        gSPMatrix(g_DisplayListHead++, &matrices->projection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    } else if (sScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
        DrawSavedFrameEffect(&g_DisplayListHead, view);
    }
}

extern s32 D_800C306C;

/*
 * Draws the dynamic textures, filling g_MirrorTexture from a strip of the
 * frame above the horizon. Only scenes 12 and 14 use it.
 */
void BuildHorizonDynamicTextures(Gfx **displayListHead) {
    s32 stripTop;
    s32 sourceTop;
    s32 destOffset;
    s32 rowCount;
    Gfx *gfx = *displayListHead;
    u32 frame = osVirtualToPhysical(nuGfxCfb_ptr);
    s32 mode;
    s32 rowIndex;
    s32 blockIndex;

    /* Keeps the strip's rows on the screen */
    void FitStripToScreen(void) {
        if (stripTop < 0) {
            sourceTop = 0;
        } else {
            sourceTop = stripTop;
        }
        if (stripTop > 0) {
            destOffset = 0;
        } else {
            destOffset = -stripTop / HORIZON_ROW_SPACING;
        }
        if (sourceTop > 0) {
            rowCount = (HORIZON_STRIP_BOTTOM - sourceTop) / HORIZON_ROW_SPACING;
        } else {
            rowCount = DYNAMIC_TEXTURE_SIZE - destOffset;
        }
    }

    if ((g_Scene != SCENE_PLAY_UNK12) & (g_Scene != SCENE_MENU)) {
        return;
    }
    stripTop = g_HorizonRow - HORIZON_STRIP_RISE;
    mode = g_GameState.unk0;
    if (mode == 3 || mode == 4) {
        sourceTop = 0;
        destOffset = 0;
        rowCount = DYNAMIC_TEXTURE_SIZE;
    } else {
        /* The value read here is never used */
        if (mode == 6) {
            mode = *(volatile s32 *)&g_CurrentShot.playerIndex;
        }
        FitStripToScreen();
    }
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, TEXEL0, 0, 0, 0, 0, 1, TEXEL0, 0, TEXEL0, 0, 0, 0, 0, 1);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, MIRROR_TINT_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, DYNAMIC_TEXTURE_SIZE,
                     osVirtualToPhysical(g_MirrorTexture) & ~7);
    gDPPipeSync(gfx++);
    if (g_Cameras[1].view.drawFlags != CAMERA_DRAW_SCENE && D_800C306C != 4 &&
        sourceTop <= HORIZON_STRIP_BOTTOM && destOffset < DYNAMIC_TEXTURE_SIZE) {
        for (rowIndex = 0; rowIndex != rowCount; rowIndex++) {
            for (blockIndex = 0; blockIndex != MIRROR_BLOCK_COUNT; blockIndex++) {
                gDPLoadTextureTile(gfx++, frame & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, 0,
                                   MIRROR_SOURCE_LEFT + blockIndex * MIRROR_BLOCK_SPACING,
                                   sourceTop + HORIZON_FIRST_ROW + rowIndex * HORIZON_ROW_SPACING,
                                   MIRROR_SOURCE_LEFT + blockIndex * MIRROR_BLOCK_SPACING + MIRROR_BLOCK_WIDTH - 1,
                                   sourceTop + HORIZON_FIRST_ROW + rowIndex * HORIZON_ROW_SPACING, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
                gSPScisTextureRectangle(gfx++, (blockIndex * MIRROR_BLOCK_WIDTH) << 2,
                                        (DYNAMIC_TEXTURE_SIZE - 1 - rowIndex - destOffset) << 2,
                                        (blockIndex * MIRROR_BLOCK_WIDTH + MIRROR_BLOCK_WIDTH) << 2,
                                        (DYNAMIC_TEXTURE_SIZE - 1 - rowIndex - destOffset + 1) << 2,
                                        G_TX_RENDERTILE,
                                        (MIRROR_SOURCE_LEFT + blockIndex * MIRROR_BLOCK_SPACING) << 5,
                                        (sourceTop + HORIZON_FIRST_ROW + rowIndex * HORIZON_ROW_SPACING) << 5, 1 << 10, 1 << 10);
            }
        }
    }
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPSetCombineLERP(gfx++, TEXEL0, TEXEL1, PRIMITIVE_ALPHA, TEXEL1, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, COMBINED, 0,
                      0, 0, 0, COMBINED);
    sHorizonTextureScroll++;
    gDPLoadTextureBlock(gfx++, (u32)sNoiseTexture & ~7, G_IM_FMT_I, G_IM_SIZ_8b, DYNAMIC_TEXTURE_SIZE,
                        DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5,
                        G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock(gfx++, (u32)g_MirrorTexture & ~7, SECOND_TEXTURE_TMEM, 1, G_IM_FMT_RGBA, G_IM_SIZ_16b,
                      DYNAMIC_TEXTURE_SIZE, DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                      G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, MIRROR_NOISE_ALPHA);
    if (rowCount > 0) {
        gSPScisTextureRectangle(gfx++, 0, (DYNAMIC_TEXTURE_SIZE - destOffset - rowCount) << 2, DYNAMIC_TEXTURE_SIZE << 2, (DYNAMIC_TEXTURE_SIZE - destOffset) << 2,
                                G_TX_RENDERTILE, 0, (DYNAMIC_TEXTURE_SIZE - destOffset - rowCount) << 5, 1 << 10, 1 << 10);
    }
    gDPSetCombineLERP(gfx++, TEXEL0, TEXEL1, PRIMITIVE_ALPHA, TEXEL1, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, COMBINED, 0,
                      0, 0, 0, COMBINED);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, DYNAMIC_TEXTURE_SIZE,
                     osVirtualToPhysical(g_FlameTexture) & ~7);
    gDPLoadTextureBlock(gfx++, (u32)sNoiseTexture & ~7, G_IM_FMT_I, G_IM_SIZ_8b, DYNAMIC_TEXTURE_SIZE,
                        DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5,
                        G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock(gfx++, (u32)sFlameGradientTexture & ~7, SECOND_TEXTURE_TMEM, 1, G_IM_FMT_RGBA,
                      G_IM_SIZ_16b, DYNAMIC_TEXTURE_SIZE, DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                      G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, FLAME_NOISE_ALPHA);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, sHorizonTextureScroll, 32, sHorizonTextureScroll + 32);
    gDPSetTileSize(gfx++, 1, 0, 0, 32, 32);
    gSPTextureRectangle(gfx++, 0, 0, DYNAMIC_TEXTURE_SIZE << 2, DYNAMIC_TEXTURE_SIZE << 2, G_TX_RENDERTILE, 0,
                        0, 1 << 10, 1 << 10);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, DYNAMIC_TEXTURE_SIZE,
                     osVirtualToPhysical(g_BlendedTexture) & ~7);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, BLENDED_TEXTURE_ALPHA);
    gDPLoadTextureBlock(gfx++, (u32)(g_BlendedTextureScrollingLayer + 8) & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b,
                        DYNAMIC_TEXTURE_SIZE, DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock(gfx++, (u32)(g_BlendedTextureStillLayer + 8) & ~7, SECOND_TEXTURE_TMEM, 1, G_IM_FMT_RGBA,
                      G_IM_SIZ_16b, DYNAMIC_TEXTURE_SIZE, DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                      G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, sHorizonTextureScroll, sHorizonTextureScroll, sHorizonTextureScroll + 32, sHorizonTextureScroll + 32);
    gDPSetTileSize(gfx++, 1, 0, 0, 32, 32);
    gSPTextureRectangle(gfx++, 0, 0, DYNAMIC_TEXTURE_SIZE << 2, DYNAMIC_TEXTURE_SIZE << 2, G_TX_RENDERTILE, 0,
                        0, 1 << 10, 1 << 10);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
    gDPPipeSync(gfx++);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, SCREEN_BORDER, SCREEN_BORDER, SCREEN_WIDTH - SCREEN_BORDER,
                  SCREEN_HEIGHT - SCREEN_BORDER);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/*
 * Draws the dynamic textures from a fixed strip of the frame. Restores
 * the frame as the color image when done.
 */
void BuildDynamicTextures(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    u32 frame = osVirtualToPhysical(nuGfxCfb_ptr);
    s32 rowIndex;
    s32 blockIndex;

    sDynamicTextureScroll++;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineLERP(gfx++, TEXEL0, PRIMITIVE, PRIMITIVE_ALPHA, PRIMITIVE, 0, 0, 0, 1, TEXEL0, PRIMITIVE,
                      PRIMITIVE_ALPHA, PRIMITIVE, 0, 0, 0, 1);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, MIRROR_TINT_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, DYNAMIC_TEXTURE_SIZE,
                     osVirtualToPhysical(g_MirrorTexture) & ~7);
    gDPPipeSync(gfx++);
    for (rowIndex = 0; rowIndex != DYNAMIC_TEXTURE_SIZE; rowIndex++) {
        for (blockIndex = 0; blockIndex != MIRROR_BLOCK_COUNT; blockIndex++) {
            gDPLoadTextureTile(gfx++, frame & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, 0,
                               MIRROR_SOURCE_LEFT + blockIndex * MIRROR_BLOCK_SPACING,
                               MIRROR_SOURCE_TOP + rowIndex * MIRROR_ROW_SPACING,
                               MIRROR_SOURCE_LEFT + blockIndex * MIRROR_BLOCK_SPACING + MIRROR_BLOCK_WIDTH - 1,
                               MIRROR_SOURCE_TOP + rowIndex * MIRROR_ROW_SPACING + MIRROR_ROW_SPACING, 0,
                               G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                               G_TX_NOLOD, G_TX_NOLOD);
            gSPScisTextureRectangle(gfx++, (blockIndex * MIRROR_BLOCK_WIDTH) << 2,
                                    (DYNAMIC_TEXTURE_SIZE - 1 - rowIndex) << 2,
                                    (blockIndex * MIRROR_BLOCK_WIDTH + MIRROR_BLOCK_WIDTH) << 2,
                                    (DYNAMIC_TEXTURE_SIZE - rowIndex) << 2, G_TX_RENDERTILE,
                                    (MIRROR_SOURCE_LEFT + blockIndex * MIRROR_BLOCK_SPACING) << 5,
                                    (MIRROR_SOURCE_TOP + rowIndex * MIRROR_ROW_SPACING) << 5, 1 << 10, 1 << 10);
        }
    }
    gDPLoadTextureBlock(gfx++, (u32)sNoiseTexture & ~7, G_IM_FMT_I, G_IM_SIZ_8b, DYNAMIC_TEXTURE_SIZE,
                        DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, MIRROR_NOISE_ALPHA);
    gSPTextureRectangle(gfx++, 0, 0, DYNAMIC_TEXTURE_SIZE << 2, DYNAMIC_TEXTURE_SIZE << 2, G_TX_RENDERTILE, 0, 0,
                        1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineLERP(gfx++, TEXEL0, TEXEL1, PRIMITIVE_ALPHA, TEXEL1, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, COMBINED, 0,
                      0, 0, 0, COMBINED);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, DYNAMIC_TEXTURE_SIZE,
                     osVirtualToPhysical(g_FlameTexture) & ~7);
    gDPLoadTextureBlock(gfx++, (u32)sNoiseTexture & ~7, G_IM_FMT_I, G_IM_SIZ_8b, DYNAMIC_TEXTURE_SIZE,
                        DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5,
                        G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock(gfx++, (u32)sFlameGradientTexture & ~7, SECOND_TEXTURE_TMEM, 1, G_IM_FMT_RGBA, G_IM_SIZ_16b,
                      DYNAMIC_TEXTURE_SIZE, DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                      G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, FLAME_NOISE_ALPHA);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, sDynamicTextureScroll, 32, sDynamicTextureScroll + 32);
    gDPSetTileSize(gfx++, 1, 0, 0, 32, 32);
    gSPTextureRectangle(gfx++, 0, 0, DYNAMIC_TEXTURE_SIZE << 2, DYNAMIC_TEXTURE_SIZE << 2, G_TX_RENDERTILE, 0, 0,
                        1 << 10, 1 << 10);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, DYNAMIC_TEXTURE_SIZE,
                     osVirtualToPhysical(g_BlendedTexture) & ~7);
    gDPLoadTextureBlock(gfx++, (u32)(g_BlendedTextureScrollingLayer + 8) & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b,
                        DYNAMIC_TEXTURE_SIZE, DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
    gDPLoadMultiBlock(gfx++, (u32)(g_BlendedTextureStillLayer + 8) & ~7, SECOND_TEXTURE_TMEM, 1, G_IM_FMT_RGBA,
                      G_IM_SIZ_16b, DYNAMIC_TEXTURE_SIZE, DYNAMIC_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                      G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, BLENDED_TEXTURE_ALPHA);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, sDynamicTextureScroll, sDynamicTextureScroll, sDynamicTextureScroll + 32,
                   sDynamicTextureScroll + 32);
    gDPSetTileSize(gfx++, 1, 0, 0, 32, 32);
    gSPTextureRectangle(gfx++, 0, 0, DYNAMIC_TEXTURE_SIZE << 2, DYNAMIC_TEXTURE_SIZE << 2, G_TX_RENDERTILE, 0, 0,
                        1 << 10, 1 << 10);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
    gDPPipeSync(gfx++);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, SCREEN_BORDER, SCREEN_BORDER, SCREEN_WIDTH - SCREEN_BORDER,
                  SCREEN_HEIGHT - SCREEN_BORDER);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/*
 * How DrawGame draws a frame, chosen by scene. The scene modules' own
 * screens are still known only by number.
 */
#define DRAW_MODE_INSET_SCREEN 0 /* A module's screen, with the course in the inset view */
#define DRAW_MODE_COURSE 1
#define DRAW_MODE_MAP_PERSPECTIVE 3
#define DRAW_MODE_MAP_ISOMETRIC 4
#define DRAW_MODE_MAP_MINIATURE 5
#define DRAW_MODE_MAP_TOP_DOWN 6
#define DRAW_MODE_NOTHING 13
#define DRAW_MODE_BLANK 14

/* The map cameras' views, in world units */
#define MAP_NEAR_CLIP 10.0f
#define MAP_FAR_CLIP 8000.0f
#define MAP_ORTHO_HALF_WIDTH 160.0f
#define MAP_ORTHO_HALF_HEIGHT 120.0f
#define MAP_ORTHO_FAR_CLIP 20000.0f
#define MAP_ISOMETRIC_OFFSET_X 5000.0f
#define MAP_ISOMETRIC_OFFSET_Y 4000.0f
#define MAP_TOP_DOWN_HEIGHT 4000.0f
#define MAP_TOP_DOWN_TILT 50.0f
#define MAP_MINIATURE_SCALE 0.07f

#define ASPECT_RATIO (4.0f / 3.0f)

/* The course camera's clip planes, in world units */
#define COURSE_NEAR_CLIP 10.0f
#define COURSE_FAR_CLIP 32000.0f
#define CLOSE_UP_NEAR_CLIP 200.0f
#define SHORT_FAR_CLIP 500.0f

/* Values of sFadeColor */
#define FADE_COLOR_NONE 0
#define FADE_COLOR_WHITE 1
#define FADE_COLOR_NOISE 2
#define FADE_COLOR_BLACK 3
#define FADE_NOISE_GREY 0xBF

/* Values of g_FramePacingMode, which the second controller's Start cycles */
#define PACING_MODE_NORMAL 0
#define PACING_MODE_PERFORMANCE_METER 1 /* The other modes print debug text */
#define PACING_MODE_COUNT 4

/* Vertices that the course drawers may add to a frame */
#define VERTEX_BUFFER_LENGTH 512
#define EXPANSION_VERTEX_BUFFER_LENGTH 5000
#define WORK_AREA_VERTEX_OFFSET 0x6A920

#define GFX_TASK_FLAGS (NU_SC_TASK_DP_WAIT | NU_SC_SWAPBUFFER)

/* When DrawGame last started, in microseconds */
extern u32 sDrawStartTime;
extern f32 sMapFieldOfView; /* In degrees */
extern s8 sNeedsIdentityMatrix;
extern Mtx sIdentityMatrix;
/* Indexed by view, then by draw buffer */
extern CameraMatrices sCameraMatrices[2 * 2];
extern s32 g_PerspectiveNormalization;
/* The next free vertex of the frame's vertex buffer */
extern Vtx *g_VertexListHead;
extern Vtx g_ExpansionVertexBuffers[2][EXPANSION_VERTEX_BUFFER_LENGTH];
extern s32 sHasThreeFrameBuffers;
extern s32 sFadeAlpha;
extern s32 sFadeColor;
/* Toggled by a debug button; nothing reads it */
extern s32 sUnreadDebugSwitch;
/*
 * The hole flyover's frame; DrawGame fades the screen out over its last
 * frames, and back in while it is -1
 */
extern s32 g_HoleFlyoverFrame;
extern f32 D_800C30D4;
extern s32 D_8022BE08;
extern s32 _mus_cpu_last;
extern s32 _mus_cpu_worst;

void MusPrintDmaUsage(void);
s32 __MusIntMemRemaining(void);
void DrawBillboards(CameraMatrices *matrices, s32 cameraIndex);
void DrawFadingBillboards(CameraMatrices *matrices, s32 cameraIndex);
void func_8005029C(void);
void func_80062A8C(CameraMatrices *matrices, s32);
void DrawEffects(CameraMatrices *matrices, Gfx **displayListHead);
void DrawRain(Gfx **displayListHead, CameraMatrices *matrices);
void func_8008156C(void);
void func_80082534(Gfx **displayListHead, CameraMatrices *matrices);
void func_80083AC8(Gfx **displayListHead, CameraMatrices *matrices);
void func_80086864(Gfx **displayListHead);
void func_80088BDC(Gfx **displayListHead);
void func_80089094(CameraView *view);
void func_8008C6B0(Gfx **displayListHead);
void func_8008CD30(Gfx **displayListHead);
void func_8008D3F4(Gfx **displayListHead);
void func_80091110(Gfx **displayListHead, CameraView *view, s32 cameraIndex);
void func_8009351C(Gfx **displayListHead, s32);
void func_800939E8(Gfx **displayListHead);
void func_800957F0(void);
void func_801F4AD8(Gfx **displayListHead);
void DrawTitleScreen(Gfx **displayListHead);
void DrawBootLogos(Gfx **displayListHead);
void DrawStaffRoll(Gfx **displayListHead);
void func_801F8110(Gfx **displayListHead);
void func_801FA4EC(CameraMatrices *matrices, Gfx **displayListHead);
void func_801FB198(Gfx **displayListHead);
void func_801FCC48(Gfx **displayListHead);
void func_801FD578(Gfx **displayListHead);
void func_801FE370(CameraMatrices *matrices);
void func_80200CBC(Gfx **displayListHead, s32);
void func_8020DCA4(Gfx **displayListHead);
void func_8020EEF8(Gfx **displayListHead);
void func_8020FC10(Gfx **displayListHead);
void func_80210370(Gfx **displayListHead);
void func_80214250(Gfx **displayListHead, CameraMatrices *matrices);
void func_802161D4(Gfx **displayListHead);
void func_80217164(Gfx **displayListHead);
void func_8021857C(Gfx **displayListHead, CameraMatrices *matrices);
void func_80219C74(Gfx **displayListHead);
void func_8021A940(Gfx **displayListHead);
void func_8021C6B4(CameraMatrices *matrices);
void DrawGameSaveMenu(Gfx **displayListHead);
void func_802438D0(Gfx **displayListHead);
void DrawReplaySaveMenu(Gfx **displayListHead);
void DrawControlsScreen(Gfx **displayListHead);
void func_8040BFC8(void);
void func_8040C124(void);
void func_8040C360(CameraMatrices *matrices, Gfx **displayListHead);

#define SET_CAMERA_SCISSOR(view) \
    gDPSetScissor(g_DisplayListHead++, G_SC_NON_INTERLACE, (view).scissorX, (view).scissorY, \
                  (view).scissorX + (view).scissorWidth, (view).scissorY + (view).scissorHeight)

#ifdef NON_MATCHING
/* Returns the frame count, which the caller ignores */
s32 DrawGame(void) {
    s32 usesFloatMatrices;
    CameraMatrices *matrices;
    u16 mapPerspNorm;
    u16 insetPerspNorm;
    u16 perspNorm;
    s32 drawsScene;
    s32 terrainStyle;
    u32 drawMode;
    s32 viewIndex;
    s32 playerIndex;
    s32 commandCount;
    f32 nearZ;
    f32 farClip;
    f32 billboardRise;
    f32 eyeX;
    f32 eyeY;
    f32 eyeZ;
    f32 topDownEyeX;
    f32 topDownEyeY;
    f32 topDownEyeZ;
    f32 lookAtX;
    f32 lookAtY;
    f32 lookAtZ;
    s32 mode;
    Camera *camera;
    CameraView *courseView;
    f32 *target;
    u16 *trigger;

    /* Loads the projection of the camera being drawn */
    void LoadProjection(void) {
        Mat4 projection;
        Mat4 translation;
        Mat4 unusedMatrix;

        if (usesFloatMatrices) {
            guTranslateF(translation, 0.0f, 0.0f, 0.0f);
            guMtxCatF(matrices->viewingF, translation, matrices->viewingF);
            MultiplyMatrices(matrices->viewingF, matrices->perspectiveF, projection);
            ConvertMatrixToFixed(projection, &matrices->projection);
            gSPMatrix(g_DisplayListHead++, &matrices->projection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
            SetCpuProjectionF(matrices->perspectiveF, matrices->viewingF, NULL);
        } else {
            gSPMatrix(g_DisplayListHead++, &matrices->perspective, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
            gSPMatrix(g_DisplayListHead++, &matrices->placement, G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);
            gSPMatrix(g_DisplayListHead++, &matrices->viewing, G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);
            SetCpuProjection(&matrices->perspective, &matrices->viewing, NULL);
        }
    }

    sDrawStartTime = OS_CYCLES_TO_USEC(osGetTime());
    if (IsGameFlagSet(0x28)) {
        PrintDebugPaddedHex(_mus_cpu_last, 20, 10);
        PrintDebugPaddedHex(_mus_cpu_worst, 20, 11);
        MusPrintDmaUsage();
    }
    if (IsGameFlagSet(0x29)) {
        PrintDebugPaddedHex(__MusIntMemRemaining(), 20, 10);
        trigger = &g_ControllerData[1].trigger;
        if (*trigger & B_BUTTON) {
            RequestSongStop();
            _mus_cpu_worst = 0;
        }
        if (*trigger & A_BUTTON) {
            PlaySound(0x87, 3, 0x60);
            PlaySound(400, 4, 0x60);
            PlaySound(0x76, 11, 0x60);
        }
    }

    switch (g_Scene) {
        case SCENE_HEIGHTS:
        case SCENE_TREES:
        case SCENE_PIN:
        case SCENE_TRIANGLES:
        case SCENE_RINGS:
        case SCENE_WALLS:
            drawMode = DRAW_MODE_MAP_PERSPECTIVE;
            break;
        case SCENE_UNK9:
        case SCENE_UNK10:
            drawMode = DRAW_MODE_MAP_TOP_DOWN;
            break;
        case SCENE_MAP_OVERVIEW:
        case SCENE_GROUND_CAMERA:
            drawMode = DRAW_MODE_MAP_MINIATURE;
            break;
        case SCENE_MENU:
            drawMode = DRAW_MODE_INSET_SCREEN;
            break;
        case SCENE_BOOT_LOGOS:
            drawMode = 9;
            break;
        case 26:
            drawMode = 8;
            break;
        case 21:
            drawMode = 10;
            break;
        case 30:
            drawMode = IsGameFlagSet(0x9A) ? DRAW_MODE_COURSE : 11;
            break;
        case 29:
            drawMode = 12;
            break;
        case 20:
        case 22:
            drawMode = DRAW_MODE_NOTHING;
            break;
        case 8:
        case 17:
        case 18:
        case SCENE_SWAPPING_MODULES:
            drawMode = DRAW_MODE_BLANK;
            break;
        default:
            drawMode = DRAW_MODE_COURSE;
            break;
    }

    g_DisplayListHead = g_DisplayLists[g_DrawBufferIndex];
    gDPPipeSync(g_DisplayListHead++);
    gDPPipelineMode(g_DisplayListHead++, G_PM_1PRIMITIVE);
    if (g_HasExpansionPak) {
        g_VertexListHead = g_ExpansionVertexBuffers[g_DrawBufferIndex];
    } else {
        g_VertexListHead = (Vtx *)&g_WorkArea.bytes[WORK_AREA_VERTEX_OFFSET] + g_DrawBufferIndex * VERTEX_BUFFER_LENGTH;
    }

    for (viewIndex = 0, camera = g_Cameras; viewIndex < 2; viewIndex++, camera++) {
        drawsScene = TRUE;
        terrainStyle = 0;
        usesFloatMatrices = FALSE;
        if (g_Scene == 30) {
            InitRcp(g_DrawBufferIndex + 4);
            if (viewIndex == 1) {
                break;
            }
            gDPPipeSync(g_DisplayListHead++);
            gDPSetScissor(g_DisplayListHead++, G_SC_NON_INTERLACE, SCREEN_BORDER, SCREEN_BORDER,
                          SCREEN_WIDTH - SCREEN_BORDER, SCREEN_HEIGHT - SCREEN_BORDER);
        } else if (drawMode == 2) {
            InitRcp(viewIndex * 2 + g_DrawBufferIndex);
        } else if (drawMode < 2) {
            InitRcp(viewIndex * 2 + g_DrawBufferIndex);
        } else {
            if (viewIndex == 1) {
                break;
            }
            InitRcp(g_DrawBufferIndex);
        }
        matrices = &sCameraMatrices[viewIndex * 2 + g_DrawBufferIndex];

        switch (drawMode) {
            case DRAW_MODE_BLANK:
                ClearBuffers(BUFFER_CLEAR_WHOLE);
                FillScreen();
                break;
            case DRAW_MODE_MAP_PERSPECTIVE:
                if (viewIndex == 0) {
                    ClearBuffers(BUFFER_CLEAR_BY_LINE);
                }
                guLookAt(&matrices->viewing, g_MapCamera.view.eye[0], g_MapCamera.view.eye[1],
                         g_MapCamera.view.eye[2], g_MapCamera.view.lookAt[0], g_MapCamera.view.lookAt[1],
                         g_MapCamera.view.lookAt[2], 0.0f, -1.0f, 0.0f);
                guAlign(&matrices->billboard, 0.0f, g_MapCamera.view.lookAt[0] - g_MapCamera.view.eye[0],
                        g_MapCamera.view.lookAt[1] - g_MapCamera.view.eye[1],
                        g_MapCamera.view.lookAt[2] - g_MapCamera.view.eye[2]);
                guAlign(&matrices->antiBillboard, M_PI, -(g_MapCamera.view.lookAt[0] - g_MapCamera.view.eye[0]),
                        -(g_MapCamera.view.lookAt[1] - g_MapCamera.view.eye[1]),
                        -(g_MapCamera.view.lookAt[2] - g_MapCamera.view.eye[2]));
                terrainStyle = 2;
                guPerspective(&matrices->perspective, &mapPerspNorm, sMapFieldOfView, ASPECT_RATIO, MAP_NEAR_CLIP,
                              MAP_FAR_CLIP, 1.0f);
                gSPPerspNormalize(g_DisplayListHead++, mapPerspNorm);
                guTranslate(&matrices->placement, 0.0f, 0.0f, 0.0f);
                LoadProjection();
                break;
            case DRAW_MODE_MAP_ISOMETRIC:
                if (viewIndex == 0) {
                    ClearBuffers(BUFFER_CLEAR_BY_LINE);
                }
                eyeX = g_MapCamera.view.eye[0] - MAP_ISOMETRIC_OFFSET_X;
                eyeY = g_MapCamera.view.eye[1] - MAP_ISOMETRIC_OFFSET_Y;
                eyeZ = g_MapCamera.view.eye[2] - MAP_ISOMETRIC_OFFSET_X;
                guLookAt(&matrices->viewing, eyeX, eyeY, eyeZ, g_MapCamera.view.eye[0], g_MapCamera.view.eye[1],
                         g_MapCamera.view.eye[2], 0.0f, -1.0f, 0.0f);
                guAlign(&matrices->billboard, 0.0f, g_MapCamera.view.eye[0] - eyeX, g_MapCamera.view.eye[1] - eyeY,
                        g_MapCamera.view.eye[2] - eyeZ);
                guAlign(&matrices->antiBillboard, M_PI, eyeX - g_MapCamera.view.eye[0],
                        eyeY - g_MapCamera.view.eye[1], eyeZ - g_MapCamera.view.eye[2]);
                guOrtho(&matrices->perspective, -MAP_ORTHO_HALF_WIDTH, MAP_ORTHO_HALF_WIDTH, -MAP_ORTHO_HALF_HEIGHT,
                        MAP_ORTHO_HALF_HEIGHT, 10.0f, MAP_ORTHO_FAR_CLIP, 8.0f);
                guTranslate(&matrices->placement, 0.0f, 0.0f, -1000.0f);
                terrainStyle = 2;
                ScaleMatrix(&matrices->viewing, g_MapCamera.view.zoom, g_MapCamera.view.zoom, 1.0f);
                LoadProjection();
                break;
            case DRAW_MODE_MAP_TOP_DOWN:
                if (viewIndex == 0) {
                    ClearBuffers(BUFFER_CLEAR_BY_LINE);
                }
                topDownEyeX = g_MapCamera.view.eye[0];
                topDownEyeY = g_MapCamera.view.eye[1] - MAP_TOP_DOWN_HEIGHT;
                topDownEyeZ = g_MapCamera.view.eye[2] - MAP_TOP_DOWN_TILT;
                terrainStyle = (g_Scene == SCENE_UNK9) ? 3 : 2;
                guLookAt(&matrices->viewing, topDownEyeX, topDownEyeY, topDownEyeZ, topDownEyeX, g_MapCamera.view.eye[1], g_MapCamera.view.eye[2],
                         0.0f, -1.0f, 0.0f);
                guAlign(&matrices->billboard, 0.0f, g_MapCamera.view.eye[0] - topDownEyeX, g_MapCamera.view.eye[1] - topDownEyeY,
                        g_MapCamera.view.eye[2] - topDownEyeZ);
                guAlign(&matrices->antiBillboard, M_PI, topDownEyeX - g_MapCamera.view.eye[0],
                        topDownEyeY - g_MapCamera.view.eye[1], topDownEyeZ - g_MapCamera.view.eye[2]);
                guOrtho(&matrices->perspective, -MAP_ORTHO_HALF_WIDTH, MAP_ORTHO_HALF_WIDTH, -MAP_ORTHO_HALF_HEIGHT,
                        MAP_ORTHO_HALF_HEIGHT, 10.0f, MAP_ORTHO_FAR_CLIP, 8.0f);
                guTranslate(&matrices->placement, 0.0f, 0.0f, -1000.0f);
                ScaleMatrix(&matrices->viewing, g_MapCamera.view.zoom, g_MapCamera.view.zoom, 1.0f);
                LoadProjection();
                break;
            case DRAW_MODE_MAP_MINIATURE:
                if (viewIndex == 0) {
                    ClearBuffers(BUFFER_CLEAR_BY_LINE);
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    FillScreen();
                }
                lookAtY = g_MapCamera.view.eye[1];
                lookAtY += 1000.0f;
                lookAtX = g_MapCamera.view.eye[0] + 10.0f;
                lookAtZ = g_MapCamera.view.eye[2];
                guLookAt(&matrices->viewing, g_MapCamera.view.eye[0], g_MapCamera.view.eye[1],
                         g_MapCamera.view.eye[2], lookAtX, lookAtY, lookAtZ, 0.0f, -1.0f, 0.0f);
                guAlign(&matrices->billboard, 0.0f, lookAtX - g_MapCamera.view.eye[0], lookAtY - g_MapCamera.view.eye[1],
                        lookAtZ - g_MapCamera.view.eye[2]);
                guAlign(&matrices->antiBillboard, M_PI, g_MapCamera.view.eye[0] - lookAtX,
                        g_MapCamera.view.eye[1] - lookAtY, g_MapCamera.view.eye[2] - lookAtZ);
                guOrtho(&matrices->perspective, -MAP_ORTHO_HALF_WIDTH, MAP_ORTHO_HALF_WIDTH, -MAP_ORTHO_HALF_HEIGHT,
                        MAP_ORTHO_HALF_HEIGHT, 1.0f, 30000.0f, 8.0f);
                guTranslate(&matrices->placement, 0.0f, 0.0f, -100.0f);
                ScaleMatrix(&matrices->viewing, MAP_MINIATURE_SCALE, MAP_MINIATURE_SCALE, MAP_MINIATURE_SCALE);
                LoadProjection();
                break;
            case 2:
            case 8:
                if (viewIndex == 0) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    gDPPipeSync(g_DisplayListHead++);
                    SET_CAMERA_SCISSOR(g_Cameras[0].view);
                    func_801F4AD8(&g_DisplayListHead);
                }
                break;
            case 9:
                if (viewIndex == 0) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    gDPPipeSync(g_DisplayListHead++);
                    SET_CAMERA_SCISSOR(g_Cameras[0].view);
                    DrawBootLogos(&g_DisplayListHead);
                }
                break;
            case 7:
                if (viewIndex == 0) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    gDPPipeSync(g_DisplayListHead++);
                    SET_CAMERA_SCISSOR(g_Cameras[0].view);
                    func_801FA4EC(matrices, &g_DisplayListHead);
                }
                break;
            case 10:
                if (viewIndex == 0) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    FillScreen();
                    gDPPipeSync(g_DisplayListHead++);
                    SET_CAMERA_SCISSOR(g_Cameras[0].view);
                    DrawTitleScreen(&g_DisplayListHead);
                }
                break;
            case 11:
                if (viewIndex == 0) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    FillScreen();
                    gDPPipeSync(g_DisplayListHead++);
                    SET_CAMERA_SCISSOR(g_Cameras[0].view);
                    DrawStaffRoll(&g_DisplayListHead);
                }
                break;
            case 12:
                if (viewIndex == 0) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    gDPPipeSync(g_DisplayListHead++);
                    SET_CAMERA_SCISSOR(g_Cameras[0].view);
                    func_801F8110(&g_DisplayListHead);
                }
                break;
            case DRAW_MODE_INSET_SCREEN:
                if (viewIndex == 0) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    gDPPipeSync(g_DisplayListHead++);
                    SET_CAMERA_SCISSOR(g_Cameras[0].view);
                    func_801FB198(&g_DisplayListHead);
                    g_Cameras[0].view.eye[0] = 0.0f;
                    g_Cameras[0].view.lookAt[0] = 0.0f;
                    g_Cameras[0].view.lookAt[1] = 0.0f;
                    g_Cameras[0].view.lookAt[2] = 0.0f;
                    g_Cameras[0].view.eye[1] = 10.0f;
                    g_Cameras[0].view.eye[2] = 60.0f;
                    SetCharacterHilite(matrices);
                } else if (D_8022BE08 != 0 && g_HasLoaderFinished == 1) {
                    usesFloatMatrices = TRUE;
                    guPerspectiveF(matrices->perspectiveF, &insetPerspNorm, g_Cameras[viewIndex].view.fieldOfView,

                                   ASPECT_RATIO, MAP_NEAR_CLIP, MAP_FAR_CLIP, 1.0f);
                    ConvertMatrixToFixed(matrices->perspectiveF, &matrices->perspective);
                    guTranslate(&matrices->placement, 0.0f, 0.0f, 0.0f);
                    guLookAtF(matrices->viewingF, camera->view.eye[0], camera->view.eye[1],
                              camera->view.eye[2], camera->view.lookAt[0],
                              camera->view.lookAt[1], camera->view.lookAt[2], 0.0f, -1.0f,
                              0.0f);
                    ConvertMatrixToFixed(matrices->viewingF, &matrices->viewing);
                    guAlign(&matrices->billboard, 0.0f,
                            camera->view.lookAt[0] - camera->view.eye[0],
                            camera->view.lookAt[1] - camera->view.eye[1],
                            camera->view.lookAt[2] - camera->view.eye[2]);
                    guAlign(&matrices->antiBillboard, 0.0f,
                            -(camera->view.lookAt[0] - camera->view.eye[0]),
                            -(camera->view.lookAt[1] - camera->view.eye[1]),
                            -(camera->view.lookAt[2] - camera->view.eye[2]));
                    LoadProjection();
                    gSPPerspNormalize(g_DisplayListHead++, insetPerspNorm);
                    gDPPipeSync(g_DisplayListHead++);
                    SET_CAMERA_SCISSOR(g_Cameras[INSET_CAMERA].view);
                    gDPPipeSync(g_DisplayListHead++);
                    gDPPipeSync(g_DisplayListHead++);
                    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
                    UsePrimitiveDepth(1);
                    func_80091110(&g_DisplayListHead, &g_Cameras[INSET_CAMERA].view, INSET_CAMERA);
                    LoadProjection();
                }
                break;
            case DRAW_MODE_COURSE:
                if (g_MenuBackdropStyle != 0 && D_800C306C != 4) {
                    ClearBuffers(BUFFER_CLEAR_DEPTH_ONLY);
                } else if (g_HasLoaderFinished == 0) {
                    if (viewIndex == 0) {
                        ClearBuffers(BUFFER_CLEAR_WHOLE);
                    }
                } else {
                    if (g_Scene == 30) {
                        ClearBuffers(BUFFER_CLEAR_WHOLE);
                    } else if (viewIndex == 0) {
                        ClearBuffers(BUFFER_CLEAR_DEPTH_ONLY);
                    }
                    usesFloatMatrices = TRUE;
                    if (g_Cameras[viewIndex].view.drawFlags != 0) {
                        nearZ = -COURSE_NEAR_CLIP;
                        farClip = -D_800C30D4 * 1.2f;
                        if (g_Cameras[viewIndex].view.mode == CAMERA_MODE_UNK8
                            || g_Cameras[viewIndex].view.mode == CAMERA_MODE_UNK2) {
                            nearZ = -CLOSE_UP_NEAR_CLIP;
                        }
                        /* Only chooses between two fixed planes; the computed distance itself is dropped */
                        if (farClip < SHORT_FAR_CLIP) {
                            farClip = SHORT_FAR_CLIP;
                        } else {
                            farClip = COURSE_FAR_CLIP;
                        }
                        guPerspectiveF(matrices->perspectiveF, &perspNorm, g_Cameras[viewIndex].view.fieldOfView,
                                       ASPECT_RATIO, -nearZ, farClip, 1.0f);
                        ConvertMatrixToFixed(matrices->perspectiveF, &matrices->perspective);
                        g_PerspectiveNormalization = perspNorm;
                        gSPPerspNormalize(g_DisplayListHead++, perspNorm);
                        guTranslate(&matrices->placement, 0.0f, 0.0f, 0.0f);
                        courseView = &g_Cameras[viewIndex].view;
                        guLookAtReflectF(matrices->viewingF, &matrices->lookAt, courseView->eye[0],
                                         courseView->eye[1], courseView->eye[2],
                                         courseView->lookAt[0], courseView->lookAt[1],
                                         courseView->lookAt[2], 0.0f, -1.0f, 0.0f);
                        gSPLookAt(g_DisplayListHead++, &matrices->lookAt);
                        SetCharacterHilite(matrices);
                        ConvertMatrixToFixed(matrices->viewingF, &matrices->viewing);
                        target = courseView->lookAt;
                        if (g_IsMiniGolf) {
                            billboardRise =
                                (courseView->lookAt[1] - courseView->eye[1]) * 0.125f;
                        } else {
                            billboardRise =
                                (courseView->lookAt[1] - courseView->eye[1]) * 0.5f;
                        }
                        guAlign(&matrices->billboard, 0.0f,
                                courseView->lookAt[0] - courseView->eye[0], billboardRise,
                                courseView->lookAt[2] - courseView->eye[2]);
                        guAlign(&matrices->antiBillboard, 0.0f,
                                -(target[0] - courseView->eye[0]), -(target[1] - courseView->eye[1]),
                                -(target[2] - courseView->eye[2]));
                        LoadProjection();
                    }
                    if (viewIndex == 0) {
                        if (g_Cameras[0].view.drawFlags == (CAMERA_DRAW_SCENE | CAMERA_DRAW_SCREEN_EFFECT)) {
                            if (sScreenEffect == SCREEN_EFFECT_SAVED_FRAME && sScreenEffectFrameCount == 2) {
                                gDPPipeSync(g_DisplayListHead++);
                                gDPSetRenderMode(g_DisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
                                gDPPipeSync(g_DisplayListHead++);
                                gDPPipeSync(g_DisplayListHead++);
                                gDPSetCycleType(g_DisplayListHead++, G_CYC_FILL);
                                gDPSetColorImage(g_DisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                                 (u32)sSavedFrameBuffer & ~7);
                                gDPSetFillColor(g_DisplayListHead++,
                                                GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0));
                                gDPFillRectangle(g_DisplayListHead++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
                                gDPPipeSync(g_DisplayListHead++);
                            }
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetColorImage(g_DisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                             osVirtualToPhysical(g_DepthBuffer) & ~7);
                            gDPPipeSync(g_DisplayListHead++);
                            if (sScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
                                gDPSetDepthImage(g_DisplayListHead++, (u32)sSavedFrameBuffer & ~7);
                            }
                        } else {
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetRenderMode(g_DisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
                            gDPSetDepthImage(g_DisplayListHead++, (u32)nuGfxZBuffer & ~7);
                            gDPPipeSync(g_DisplayListHead++);
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetCycleType(g_DisplayListHead++, G_CYC_FILL);
                            gDPSetColorImage(g_DisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                             (u32)nuGfxZBuffer & ~7);
                            gDPSetFillColor(g_DisplayListHead++,
                                            GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0));
                            gDPFillRectangle(g_DisplayListHead++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetColorImage(g_DisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                             osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
                        }
                        gDPPipeSync(g_DisplayListHead++);
                        SET_CAMERA_SCISSOR(g_Cameras[0].view);
                        if ((g_Cameras[0].view.drawFlags != (CAMERA_DRAW_SCENE | CAMERA_DRAW_SCREEN_EFFECT) ||
                             sScreenEffect != SCREEN_EFFECT_SAVED_FRAME || sScreenEffectFrameCount != 1) &&
                            D_800C306C != 4) {
                            UsePrimitiveDepth(0);
                            if (!IsGameFlagSet(0x15)) {
                                func_80091110(&g_DisplayListHead, &g_Cameras[0].view, 0);
                            }
                            LoadProjection();
                            /* Keeps the main view out of the inset view, which clears it again */
                            if (g_Cameras[INSET_CAMERA].view.drawFlags == CAMERA_DRAW_SCENE) {
                                gDPPipeSync(g_DisplayListHead++);
                                gDPSetRenderMode(g_DisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
                                gDPPipeSync(g_DisplayListHead++);
                                gDPPipeSync(g_DisplayListHead++);
                                gDPSetCycleType(g_DisplayListHead++, G_CYC_FILL);
                                gDPSetColorImage(g_DisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                                 (u32)nuGfxZBuffer & ~7);
                                gDPSetFillColor(g_DisplayListHead++, GPACK_ZDZ(0, 0) << 16 | GPACK_ZDZ(0, 0));
                                gDPFillRectangle(
                                    g_DisplayListHead++, g_Cameras[INSET_CAMERA].view.scissorX,
                                    g_Cameras[INSET_CAMERA].view.scissorY,
                                    g_Cameras[INSET_CAMERA].view.scissorX + g_Cameras[INSET_CAMERA].view.scissorWidth - 1,
                                    g_Cameras[INSET_CAMERA].view.scissorY + g_Cameras[INSET_CAMERA].view.scissorHeight - 1);
                                gDPPipeSync(g_DisplayListHead++);
                                gDPSetColorImage(g_DisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                                 osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
                                gDPSetFillColor(g_DisplayListHead++, 0xFFFFFFFF);
                                gDPPipeSync(g_DisplayListHead++);
                                gDPPipeSync(g_DisplayListHead++);
                                gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
                                gDPPipeSync(g_DisplayListHead++);
                            }
                        }
                    } else {
                        if (g_Cameras[INSET_CAMERA].view.drawFlags == CAMERA_DRAW_SCENE) {
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetRenderMode(g_DisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
                            gDPPipeSync(g_DisplayListHead++);
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetCycleType(g_DisplayListHead++, G_CYC_FILL);
                            gDPSetColorImage(g_DisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                             (u32)nuGfxZBuffer & ~7);
                            gDPSetFillColor(g_DisplayListHead++,
                                            GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0));
                            gDPFillRectangle(
                                g_DisplayListHead++, g_Cameras[INSET_CAMERA].view.scissorX,
                                g_Cameras[INSET_CAMERA].view.scissorY,
                                g_Cameras[INSET_CAMERA].view.scissorX + g_Cameras[INSET_CAMERA].view.scissorWidth - 1,
                                g_Cameras[INSET_CAMERA].view.scissorY + g_Cameras[INSET_CAMERA].view.scissorHeight - 1);
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetFillColor(g_DisplayListHead++, 0xFFFFFFFF);
                            gDPSetColorImage(g_DisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                             osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
                        } else if (g_Cameras[INSET_CAMERA].view.drawFlags == CAMERA_DRAW_SCREEN_EFFECT) {
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetColorImage(g_DisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                             osVirtualToPhysical(g_DepthBuffer) & ~7);
                        }
                        if (g_Cameras[INSET_CAMERA].view.drawFlags != 0) {
                            gDPPipeSync(g_DisplayListHead++);
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
                            gDPPipeSync(g_DisplayListHead++);
                            SET_CAMERA_SCISSOR(g_Cameras[INSET_CAMERA].view);
                            UsePrimitiveDepth(0);
                            func_80091110(&g_DisplayListHead, &g_Cameras[INSET_CAMERA].view, INSET_CAMERA);
                            LoadProjection();
                        }
                    }
                }
                break;
            case DRAW_MODE_NOTHING:
                break;
            default:
                drawsScene = FALSE;
                break;
        }

        if (sNeedsIdentityMatrix) {
            sNeedsIdentityMatrix = FALSE;
            guTranslate(&sIdentityMatrix, 0.0f, 0.0f, 0.0f);
        }
        gSPMatrix(g_DisplayListHead++, &sIdentityMatrix, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        if (g_MenuBackdropStyle != 0) {
            if (viewIndex == 0) {
                func_8009351C(&g_DisplayListHead, g_MenuBackdropStyle);
                func_800939E8(&g_DisplayListHead);
            }
        }

        if (drawMode == DRAW_MODE_COURSE) {
            if (g_HasLoaderFinished == 1 && (g_MenuBackdropStyle == 0 || D_800C306C == 4)) {
                if (g_Cameras[viewIndex].view.drawFlags != 0) {
                    if (sScreenEffect == SCREEN_EFFECT_SAVED_FRAME && sScreenEffectFrameCount == 1) {
                        gDPPipeSync(g_DisplayListHead++);
                        gDPSetColorImage(g_DisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                         osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
                        gDPSetDepthImage(g_DisplayListHead++, (u32)nuGfxZBuffer & ~7);
                        gDPPipeSync(g_DisplayListHead++);
                    } else {
                        SetSceneFog(&g_DisplayListHead);
                        if (!IsGameFlagSet(6)) {
                            func_80062A8C(matrices, terrainStyle);
                        }
                        DrawBillboards(matrices, viewIndex);
                        if (sScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
                            DrawRings(&g_DisplayListHead, matrices, &g_Cameras[viewIndex]);
                            if (viewIndex == 0) {
                                BuildHorizonDynamicTextures(&g_DisplayListHead);
                                func_80082534(&g_DisplayListHead, matrices);
                            }
                            DrawHoleTriangles(&g_DisplayListHead, matrices, &g_Cameras[viewIndex]);
                            SetSceneFog(&g_DisplayListHead);
                            DrawFadingBillboards(matrices, viewIndex);
                        }
                    }
                    if (g_Cameras[viewIndex].view.drawFlags == (CAMERA_DRAW_SCENE | CAMERA_DRAW_SCREEN_EFFECT) &&
                        sScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
                        gDPPipeSync(g_DisplayListHead++);
                        gDPSetColorImage(g_DisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                         osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
                        gDPSetDepthImage(g_DisplayListHead++, (u32)nuGfxZBuffer & ~7);
                        gDPPipeSync(g_DisplayListHead++);
                        DrawScreenEffect(matrices, &g_Cameras[viewIndex].view);
                    }
                    SetSceneFog(&g_DisplayListHead);
                    if (g_Scene == 19) {
                        func_801FA4EC(matrices, &g_DisplayListHead);
                        DrawEffects(matrices, &g_DisplayListHead);
                    } else {
                        if (!IsGameFlagSet(0x17)) {
                            DrawPin(matrices, &camera->view, viewIndex);
                        }
                        if (sScreenEffect == SCREEN_EFFECT_SAVED_FRAME && sScreenEffectFrameCount == 1) {
                            DrawRings(&g_DisplayListHead, matrices, &g_Cameras[viewIndex]);
                            if (viewIndex == 0) {
                                BuildHorizonDynamicTextures(&g_DisplayListHead);
                                func_80082534(&g_DisplayListHead, matrices);
                            }
                            DrawHoleTriangles(&g_DisplayListHead, matrices, &g_Cameras[viewIndex]);
                            SetSceneFog(&g_DisplayListHead);
                            DrawFadingBillboards(matrices, viewIndex);
                        }
                        if (g_GameState.unk0 != 3 && g_GameState.unk0 != 4) {
                            mode = g_Cameras[viewIndex].view.mode;
                            if (mode != 8 && mode != 2) {
                                if (!IsGameFlagSet(0x18) && g_IsModuleAlive[1]) {
                                    func_801FE370(matrices);
                                }
                                if (!IsGameFlagSet(0x19)) {
                                    for (playerIndex = 0; playerIndex < 4; playerIndex++) {
                                        DrawCharacterAndShadow(matrices, viewIndex, playerIndex);
                                        if (g_Scene != 30) {
                                            break;
                                        }
                                    }
                                }
                                if (g_IsModuleAlive[11]) {
                                    if (viewIndex == 0) {
                                        func_8020DCA4(&g_DisplayListHead);
                                    }
                                }
                            }
                        }
                        if (g_GameState.unk0 != 3 && g_GameState.unk0 != 4) {
                            if (g_IsModuleAlive[23] && !IsGameFlagSet(0x2D)) {
                                func_8021C6B4(matrices);
                            }
                            if (g_IsModuleAlive[17]) {
                                func_80214250(&g_DisplayListHead, matrices);
                            }
                            DrawEffects(matrices, &g_DisplayListHead);
                            if (viewIndex == 0 && g_IsModuleAlive[18]) {
                                func_8021857C(&g_DisplayListHead, matrices);
                            }
                        }
                        DrawRain(&g_DisplayListHead, matrices);
                        if (g_GameState.unk0 != 3 && g_GameState.unk0 != 4) {
                            if (viewIndex == 0) {
                                func_80083AC8(&g_DisplayListHead, matrices);
                                func_80086864(&g_DisplayListHead);
                                if (g_IsModuleAlive[18]) {
                                    func_80217164(&g_DisplayListHead);
                                }
                            }
                            if (D_800C306C != 4) {
                                if (viewIndex == 0) {
                                    func_8008D3F4(&g_DisplayListHead);
                                }
                            }
                        }
                        if (g_Cameras[viewIndex].view.drawFlags == (CAMERA_DRAW_SCENE | CAMERA_DRAW_SCREEN_EFFECT) &&
                            (sScreenEffect == SCREEN_EFFECT_SPIN || sScreenEffect == SCREEN_EFFECT_TILED_IMAGE ||
                             sScreenEffect == SCREEN_EFFECT_EMPTY)) {
                            gDPSetColorImage(g_DisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                             osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
                            gDPSetDepthImage(g_DisplayListHead++, (u32)nuGfxZBuffer & ~7);
                            gDPPipeSync(g_DisplayListHead++);
                            DrawScreenEffect(matrices, &g_Cameras[viewIndex].view);
                        }
                    }
                }
                if (g_Cameras[viewIndex].view.drawFlags == CAMERA_DRAW_SCREEN_EFFECT) {
                    if (viewIndex == 1) {
                        gDPSetColorImage(g_DisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                         osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
                        gDPPipeSync(g_DisplayListHead++);
                    }
                }
            }
        } else if (drawMode == DRAW_MODE_INSET_SCREEN) {
            if (D_8022BE08 != 0 && viewIndex == 1 && g_HasLoaderFinished == 1) {
                SetSceneFog(&g_DisplayListHead);
                func_80062A8C(matrices, terrainStyle);
                DrawBillboards(matrices, viewIndex);
                if (!IsGameFlagSet(0x17)) {
                    DrawPin(matrices, &camera->view, viewIndex);
                }
                DrawHoleTriangles(&g_DisplayListHead, matrices, &g_Cameras[viewIndex]);
                gDPPipeSync(g_DisplayListHead++);
                SET_CAMERA_SCISSOR(g_Cameras[0].view);
            }
        } else if (drawsScene && IsOverlayInExpansionMemory()) {
            if (!IsGameFlagSet(6)) {
                func_80062A8C(matrices, terrainStyle);
            }
            DrawHoleTriangles(&g_DisplayListHead, matrices, &g_Cameras[viewIndex]);
            DrawPin(matrices, &g_MapCamera.view, viewIndex);
            if (drawMode != DRAW_MODE_MAP_MINIATURE && drawMode != DRAW_MODE_MAP_TOP_DOWN) {
                DrawBillboards(matrices, viewIndex);
            }
            if (g_IsModuleAlive[1]) {
                func_801FE370(matrices);
            }
            if (IsGameFlagSet(0x50) && g_Scene != SCENE_FLAG_PANEL) {
                func_8040C360(matrices, &g_DisplayListHead);
            }
        }
    }

    UpdateDebugScenes();
    if (drawMode == DRAW_MODE_COURSE) {
        gDPPipeSync(g_DisplayListHead++);
        gDPPipeSync(g_DisplayListHead++);
        SET_CAMERA_SCISSOR(g_Cameras[0].view);
        gSPViewport(g_DisplayListHead++, &g_Viewports[g_DrawBufferIndex]);
        if (g_Cameras[INSET_CAMERA].view.drawFlags == CAMERA_DRAW_SCENE) {
            func_8008156C();
        }
        if (g_IsModuleAlive[20]) {
            func_80219C74(&g_DisplayListHead);
        }
        if (g_HasLoaderFinished == 1 && !IsGameFlagSet(0x13) && !IsGameFlagSet(0x2D)) {
            func_80089094(&g_Cameras[0].view);
        }
        if (g_IsModuleAlive[17]) {
            func_80210370(&g_DisplayListHead);
        }
        if (g_IsModuleAlive[17]) {
            func_8020EEF8(&g_DisplayListHead);
        }
        if (g_IsModuleAlive[17]) {
            func_802161D4(&g_DisplayListHead);
        }
        func_8008C6B0(&g_DisplayListHead);
        if (g_IsModuleAlive[5]) {
            func_802438D0(&g_DisplayListHead);
        }
        if (g_IsModuleAlive[15]) {
            func_801FCC48(&g_DisplayListHead);
            func_801FD578(&g_DisplayListHead);
        }
        DrawHalfTurn(&g_DisplayListHead);
        if (g_IsModuleAlive[22]) {
            DrawGameSaveMenu(&g_DisplayListHead);
            DrawReplaySaveMenu(&g_DisplayListHead);
            DrawControlsScreen(&g_DisplayListHead);
        }
    }
    SwapMatrixBuffers();
    func_8005029C();
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gDPPipeSync(g_DisplayListHead++);

    if (g_HoleFlyoverFrame > 0xAA) {
        sFadeAlpha = (g_HoleFlyoverFrame - 0xAA) * 16;
        if (sFadeColor == FADE_COLOR_NONE) {
            SetScreenEffect(SCREEN_EFFECT_SPIN, 30);
        }
    } else if (g_HoleFlyoverFrame == -1) {
        sFadeAlpha -= 32;
        if (sFadeAlpha < 0) {
            sFadeAlpha = 0;
            g_HoleFlyoverFrame = 0;
        }
    } else {
        sFadeAlpha = 0;
    }
    if (sFadeAlpha != 0) {
        if (sFadeColor == FADE_COLOR_BLACK) {
            gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0, 0, 0, sFadeAlpha);
            gDPSetCombineMode(g_DisplayListHead++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        } else if (sFadeColor == FADE_COLOR_WHITE) {
            gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0xFF, 0xFF, 0xFF, sFadeAlpha);
            gDPSetCombineMode(g_DisplayListHead++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        } else if (sFadeColor == FADE_COLOR_NOISE) {
            gDPSetPrimColor(g_DisplayListHead++, 0, 0, FADE_NOISE_GREY, FADE_NOISE_GREY, FADE_NOISE_GREY,
                            sFadeAlpha);
            gDPSetCombineLERP(g_DisplayListHead++, NOISE, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, NOISE, 0, PRIMITIVE,
                              0, 0, 0, 0, PRIMITIVE);
        }
        if (sFadeColor != FADE_COLOR_NONE) {
            gDPPipeSync(g_DisplayListHead++);
            gDPSetRenderMode(g_DisplayListHead++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
            gSPTextureRectangle(g_DisplayListHead++, g_Cameras[0].view.scissorX << 2,
                                g_Cameras[0].view.scissorY << 2,
                                (g_Cameras[0].view.scissorX + g_Cameras[0].view.scissorWidth) << 2,
                                (g_Cameras[0].view.scissorY + g_Cameras[0].view.scissorHeight) << 2,
                                G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        }
    }

    if (g_IsDebugEnabled && (g_ControllerStates[1].pressedButtons & L_TRIG)) {
        g_FrameStepState = FRAME_STEP_PAUSED;
    }
    if (IsGameFlagSet(0x2F)) {
        if (g_ControllerStates[1].buttons & D_CBUTTONS) {
            SetScreenEffect(SCREEN_EFFECT_SAVED_FRAME, 2);
        }
        if (g_ControllerStates[1].buttons & U_CBUTTONS) {
            SetScreenEffect(SCREEN_EFFECT_SPIN, 16);
        }
        if (g_ControllerStates[1].buttons & L_CBUTTONS) {
            SetScreenEffect(SCREEN_EFFECT_TILED_IMAGE, 64);
        }
        if (g_ControllerStates[1].buttons & R_CBUTTONS) {
            SetScreenEffect(SCREEN_EFFECT_EMPTY, 64);
        }
        if (g_ControllerStates[1].buttons & D_CBUTTONS) {
            sFadeAlpha -= 8;
        }
        if (g_ControllerStates[1].buttons & U_CBUTTONS) {
            sFadeAlpha += 8;
        }
        if (sFadeAlpha > 0xFF) {
            sFadeAlpha = 0xFF;
        }
        if (sFadeAlpha < 0) {
            sFadeAlpha = 0;
        }
        if (g_ControllerStates[1].pressedButtons & B_BUTTON) {
            g_FrameStepState = FRAME_STEP_PAUSED;
        }
        if (g_ControllerStates[1].pressedButtons & D_JPAD) {
            sUnreadDebugSwitch ^= 1;
        }
        if (g_ControllerStates[1].pressedButtons & U_JPAD) {
            g_Cameras[INSET_CAMERA].view.drawFlags ^= CAMERA_DRAW_SCENE;
        }
    }

    DrawPrompt(&g_DisplayListHead);
    if (g_IsModuleAlive[20]) {
        func_8021A940(&g_DisplayListHead);
    }
    DrawTextSlots(&g_DisplayListHead);
    if (g_IsModuleAlive[17]) {
        func_8020FC10(&g_DisplayListHead);
    }
    DrawWipe(&g_DisplayListHead);
    DrawScreenFade(&g_DisplayListHead);
    func_8008CD30(&g_DisplayListHead);
    if (IsGameFlagSet(0x46)) {
        func_80088BDC(&g_DisplayListHead);
    }
    func_800957F0();
    if (g_IsModuleAlive[3]) {
        func_80200CBC(&g_DisplayListHead, 1);
    }
    if (g_Scene == 30 && g_IsModuleAlive[31]) {
        DrawStaffRoll(&g_DisplayListHead);
    }
    if (IsGameFlagSet(1)) {
        PrintDebugText(!sHasThreeFrameBuffers ? "2 Frame Buffer" : "3 Frame Buffer", 3, 4);
        sprintf(g_PrintBuffer, "%2d draw/sec", g_FramesPerSecond);
        PrintDebugText(g_PrintBuffer, 3, 5);
    }
    if (IsGameFlagSet(0x39)) {
        ShowDebugLog();
    }
    DrawDebugText(&g_DisplayListHead);
    if (IsGameFlagSet(0x1B) && (g_ControllerData[1].trigger & Z_TRIG)) {
        if (IsGameFlagSet(0x1D)) {
            ClearGameFlag(0x1D);
        } else {
            SetGameFlag(0x1D);
        }
    }
    if (IsGameFlagSet(0x2F)) {
        if (g_ControllerStates[1].pressedButtons & Z_TRIG) {
            func_8040C124();
        }
        if (g_ControllerStates[1].pressedButtons & R_JPAD) {
            func_8040BFC8();
        }
    }

    gDPFullSync(g_DisplayListHead++);
    gSPEndDisplayList(g_DisplayListHead++);
    commandCount = g_DisplayListHead - g_DisplayLists[g_DrawBufferIndex];
    if ((u32)commandCount >= DISPLAY_LIST_LENGTH) {
        ReportFailedAssertion("EX", "c:/usgolf64/stage00.c", 2636);
    }
    if (!g_HasExpansionPak && g_VertexListHead - ((Vtx *)&g_WorkArea.bytes[WORK_AREA_VERTEX_OFFSET] +
                                                  g_DrawBufferIndex * VERTEX_BUFFER_LENGTH) >=
                                  VERTEX_BUFFER_LENGTH) {
        ReportFailedAssertion("EX", "c:/usgolf64/stage00.c", 2637);
    }
    RecordDisplayListLength(commandCount);

    if (g_IsDebugEnabled) {
        if ((g_ControllerData[1].button & START_BUTTON) && !(g_PreviousControllerData[1].button & START_BUTTON)) {
            g_FramePacingMode = (g_FramePacingMode + 1) % PACING_MODE_COUNT;
        }
        /* Reads the second controller's last state for the third's */
        if (g_IsDebugEnabled && (g_ControllerData[2].button & START_BUTTON) &&
            !(g_PreviousControllerData[1].button & START_BUTTON)) {
            g_FramePacingMode = (g_FramePacingMode + 2) % PACING_MODE_COUNT;
        }
    }

    sGfxTaskStartTime = OS_CYCLES_TO_USEC(osGetTime());
    if (g_FramePacingMode == PACING_MODE_NORMAL) {
        nuGfxTaskStart(g_DisplayLists[g_DrawBufferIndex],
                       (s32)(g_DisplayListHead - g_DisplayLists[g_DrawBufferIndex]) * sizeof(Gfx),
                       NU_GFX_UCODE_F3DEX, GFX_TASK_FLAGS);
    } else if (g_FramePacingMode == PACING_MODE_PERFORMANCE_METER) {
        nuGfxTaskStart(g_DisplayLists[g_DrawBufferIndex],
                       (s32)(g_DisplayListHead - g_DisplayLists[g_DrawBufferIndex]) * sizeof(Gfx),
                       NU_GFX_UCODE_F3DEX, NU_SC_TASK_DP_WAIT | NU_SC_NOSWAPBUFFER);
        DrawPerformanceMeter(2, 200, sDrawStartTime, sGfxTaskStartTime + (sUpdateEndTime - sUpdateStartTime),
                             GFX_TASK_FLAGS, METER_READOUT_FREE_MEMORY);
    } else {
        nuGfxTaskStart(g_DisplayLists[g_DrawBufferIndex],
                       (s32)(g_DisplayListHead - g_DisplayLists[g_DrawBufferIndex]) * sizeof(Gfx),
                       NU_GFX_UCODE_F3DEX, NU_SC_TASK_DP_WAIT | NU_SC_NOSWAPBUFFER);
        DrawMemoryMeter(192, GFX_TASK_FLAGS);
    }
    g_DrawBufferIndex ^= 1;
    nuGfxDisplayOn();
    return g_FrameCount;
}
#else
INCLUDE_ASM("asm/nonmatchings/game", LoadProjection);
INCLUDE_ASM("asm/nonmatchings/game", DrawGame);
#endif

/*
 * Copies the frame shown last onto the frame being drawn, so a frame can be
 * built over the previous one without redrawing the scene
 */
void CopyPreviousFrame(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s16 stripHeight;

    if (sCopySourceFrame == NULL) {
        sCopySourceFrame = osViGetNextFramebuffer();
    } else {
        sCopySourceFrame = sPreviousFrame;
    }
    sPreviousFrame = nuGfxCfb_ptr;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_COPY);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
    gDPPipeSync(gfx++);
    for (stripIndex = 0; stripIndex != SAVED_FRAME_STRIP_COUNT; stripIndex++) {
        stripHeight = (stripIndex == SAVED_FRAME_STRIP_COUNT - 1) ? SAVED_FRAME_LAST_STRIP_HEIGHT
                                                                  : SAVED_FRAME_STRIP_HEIGHT;
        gDPLoadTextureTile(gfx++, (u32)(sCopySourceFrame + stripIndex * SAVED_FRAME_STRIP_HEIGHT * SCREEN_WIDTH) & ~7,
                           G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, 0, SCREEN_BORDER, SCREEN_BORDER,
                           SCREEN_WIDTH - SCREEN_BORDER - 1, SCREEN_BORDER + stripHeight - 1, 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, SCREEN_BORDER << 2, (SCREEN_BORDER + stripIndex * SAVED_FRAME_STRIP_HEIGHT) << 2,
                                (SCREEN_WIDTH - SCREEN_BORDER - 1) << 2,
                                (stripIndex * SAVED_FRAME_STRIP_HEIGHT + SCREEN_BORDER + stripHeight - 1) << 2,
                                G_TX_RENDERTILE, SCREEN_BORDER << 5, SCREEN_BORDER << 5, 4 << 10, 1 << 10);
        gDPPipeSync(gfx++);
    }
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/*
 * Darkens the last frame while the reset button is held. Each frame lays a
 * translucent dark blue over the one before, so the screen fades out.
 */
void DrawResetFade(void) {
    Gfx *gfx;
    Gfx *displayList;
    s32 passIndex;

    displayList = gfx = g_DisplayLists[g_DrawBufferIndex];
    /* The original builds the frame in a loop that runs once */
    for (passIndex = 0; passIndex != 1; passIndex++) {
        AppendRcpInit(g_DrawBufferIndex, &gfx);
        AppendBufferClear(BUFFER_CLEAR_DEPTH_ONLY, &gfx);
        gDPPipeSync(gfx++);
        gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        CopyPreviousFrame(&gfx);
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 0x00, 0x00, 0x10, RESET_FADE_ALPHA);
        gDPSetCombineLERP(gfx++, 1, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, 1, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gSPTextureRectangle(gfx++, 0, 0, SCREEN_WIDTH << 2, SCREEN_HEIGHT << 2, G_TX_RENDERTILE, 0, 0, 1 << 10,
                            1 << 10);
        gDPFullSync(gfx++);
        gSPEndDisplayList(gfx++);
        nuGfxTaskStart(displayList, (s32)(gfx - displayList) * sizeof(Gfx), NU_GFX_UCODE_F3DEX, NU_SC_SWAPBUFFER);
        sGfxTaskStartTime = OS_CYCLES_TO_USEC(osGetTime());
    }
    g_DrawBufferIndex ^= 1;
}

/*
 * Runs the parts of a frame's update that depend on the scene, then fits
 * the inset view's viewport to its window
 */
void UpdateGame(void) {
    s32 *animation;
    u16 *windDirection;

    sUpdateStartTime = OS_CYCLES_TO_USEC(osGetTime());
    if (g_Scene == SCENE_GROUND_CAMERA) {
        if (g_ControllerData[0].button & U_CBUTTONS) {
            g_MapCamera.view.eye[2] += MAP_CAMERA_STEP;
        }
        if (g_ControllerData[0].button & D_CBUTTONS) {
            g_MapCamera.view.eye[2] -= MAP_CAMERA_STEP;
        }
        if (g_ControllerData[0].button & L_CBUTTONS) {
            g_MapCamera.view.eye[0] -= MAP_CAMERA_STEP;
        }
        if (g_ControllerData[0].button & R_CBUTTONS) {
            g_MapCamera.view.eye[0] += MAP_CAMERA_STEP;
        }
        g_MapCamera.view.heading -= g_ControllerData[0].stick_x / MAP_CAMERA_TURN_PER_STICK;
        g_MapCamera.view.eye[1] =
            (f32)(-g_ControllerData[0].stick_y * MAP_CAMERA_LIFT_PER_STICK) - MAP_CAMERA_HEIGHT +
            (f32)GetGroundHeight(g_MapCamera.view.eye[0] * GROUND_UNITS_PER_UNIT,
                                 g_MapCamera.view.eye[2] * GROUND_UNITS_PER_UNIT) / GROUND_UNITS_PER_UNIT;
        g_MapCamera.view.lookAt[0] = g_MapCamera.view.eye[0] + cosf(g_MapCamera.view.heading) * MAP_CAMERA_LOOK_DISTANCE;
        g_MapCamera.view.lookAt[1] = 0.0f;
        g_MapCamera.view.lookAt[2] = g_MapCamera.view.eye[2] + sinf(g_MapCamera.view.heading) * MAP_CAMERA_LOOK_DISTANCE;
    }
    if (g_Scene == 19) {
        StepNoiseTexture();
        g_ViewedWindAngles[0] = VIEWED_WIND_ANGLE(g_HoleConditions.windDirection, &g_Cameras[0].view);
        if (g_ViewedWindAngles[0] > 2 * M_PI) {
            g_ViewedWindAngles[0] -= 2 * M_PI;
        }
        if (g_ViewedWindAngles[0] < 0.0f) {
            g_ViewedWindAngles[0] += 2 * M_PI;
        }
        func_801F6788();
        UpdateEffects();
    } else if (g_Scene == SCENE_PLAY_HOLE || g_Scene == SCENE_PLAY_UNK12 || g_Scene == SCENE_PLAY_UNK13) {
        if (sScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
            StepNoiseTexture();
        }
        if (IsGameFlagSet(GAME_FLAG_INSET_VIEW_BUTTONS)) {
            if ((g_ControllerStates[1].pressedButtons & L_JPAD) && g_InsetView.step < INSET_VIEW_STEP_COUNT) {
                g_InsetView.animation = INSET_VIEW_OPENING;
            }
            if ((g_ControllerStates[1].pressedButtons & R_JPAD) && g_InsetView.step > 0) {
                g_InsetView.animation = INSET_VIEW_CLOSING;
            }
            if ((g_ControllerStates[3].pressedButtons & L_JPAD) && g_InsetView.step < INSET_VIEW_STEP_COUNT) {
                g_InsetView.animation = INSET_VIEW_OPENING;
            }
            if ((g_ControllerStates[3].pressedButtons & R_JPAD) && g_InsetView.step > 0) {
                g_InsetView.animation = INSET_VIEW_CLOSING;
            }
        }
        UpdateInsetView();
        UpdateEffects();
        func_800605EC();
        windDirection = &g_HoleConditions.windDirection;
        g_ViewedWindAngles[0] = VIEWED_WIND_ANGLE(*windDirection, &g_Cameras[0].view);
        g_ViewedWindAngles[1] = VIEWED_WIND_ANGLE(*windDirection, &g_Cameras[INSET_CAMERA].view);
        if (g_ViewedWindAngles[0] > 2 * M_PI) {
            g_ViewedWindAngles[0] -= 2 * M_PI;
        }
        if (g_ViewedWindAngles[0] < 0.0f) {
            g_ViewedWindAngles[0] += 2 * M_PI;
        }
        if (g_ViewedWindAngles[1] > 2 * M_PI) {
            g_ViewedWindAngles[1] -= 2 * M_PI;
        }
        if (g_ViewedWindAngles[1] < 0.0f) {
            g_ViewedWindAngles[1] += 2 * M_PI;
        }
    } else if (g_Scene == SCENE_MENU) {
        StepNoiseTexture();
        animation = &g_InsetView.animation;
        if (*animation == INSET_VIEW_OPENING) {
            g_Cameras[INSET_CAMERA].view.drawFlags = CAMERA_DRAW_SCENE;
            g_InsetView.step++;
            if (g_InsetView.step >= LARGE_INSET_VIEW_STEP_COUNT) {
                g_InsetView.step = LARGE_INSET_VIEW_STEP_COUNT;
                *animation = INSET_VIEW_STILL;
            }
        } else if (*animation == INSET_VIEW_CLOSING) {
            g_InsetView.step--;
            if (g_InsetView.step <= 0) {
                g_InsetView.step = 0;
                *animation = INSET_VIEW_STILL;
                g_Cameras[INSET_CAMERA].view.drawFlags = 0;
            }
        }
        g_Cameras[INSET_CAMERA].view.scissorWidth = g_InsetView.step * LARGE_INSET_VIEW_GROWTH_PER_STEP;
        g_Cameras[INSET_CAMERA].view.scissorHeight = g_InsetView.step * LARGE_INSET_VIEW_GROWTH_PER_STEP;
        g_Cameras[INSET_CAMERA].view.scissorX =
            LARGE_INSET_VIEW_CENTER_X - g_InsetView.step * (LARGE_INSET_VIEW_GROWTH_PER_STEP / 2);
        g_Cameras[INSET_CAMERA].view.scissorY =
            LARGE_INSET_VIEW_CENTER_Y - g_InsetView.step * (LARGE_INSET_VIEW_GROWTH_PER_STEP / 2);
        g_Cameras[INSET_CAMERA].view.zoom = 0.9f;
        func_801F4C78();
        if (IsGameFlagSet(GAME_FLAG_DEBUG_MENU_VALUES)) {
            sprintf(g_PrintBuffer, "%3d", sDebugMenuValues[0]);
            PrintDebugText(g_PrintBuffer, DEBUG_MENU_COLUMN, DEBUG_MENU_ROW);
            sprintf(g_PrintBuffer, "%3d", sDebugMenuValues[1]);
            PrintDebugText(g_PrintBuffer, DEBUG_MENU_COLUMN, DEBUG_MENU_ROW + 1);
            sprintf(g_PrintBuffer, "%3d", sDebugMenuValues[2]);
            PrintDebugText(g_PrintBuffer, DEBUG_MENU_COLUMN, DEBUG_MENU_ROW + 2);
            PrintDebugText(" ", DEBUG_MENU_CURSOR_COLUMN, DEBUG_MENU_ROW);
            PrintDebugText(" ", DEBUG_MENU_CURSOR_COLUMN, DEBUG_MENU_ROW + 1);
            PrintDebugText(" ", DEBUG_MENU_CURSOR_COLUMN, DEBUG_MENU_ROW + 2);
            PrintDebugText(">", DEBUG_MENU_CURSOR_COLUMN, DEBUG_MENU_ROW + sDebugMenuValueIndex);
            if ((g_ControllerStates[0].repeatedButtons & U_CBUTTONS) && sDebugMenuValueIndex > 0) {
                sDebugMenuValueIndex--;
            }
            if ((g_ControllerStates[0].repeatedButtons & D_CBUTTONS) &&
                sDebugMenuValueIndex < DEBUG_MENU_VALUE_COUNT - 1) {
                sDebugMenuValueIndex++;
            }
            if (g_ControllerStates[0].repeatedButtons & L_CBUTTONS) {
                if (sDebugMenuValues[sDebugMenuValueIndex] > 0) {
                    sDebugMenuValues[sDebugMenuValueIndex]--;
                }
            }
            if (g_ControllerStates[0].repeatedButtons & R_CBUTTONS) {
                if (sDebugMenuValues[sDebugMenuValueIndex] < DEBUG_MENU_VALUE_MAX) {
                    sDebugMenuValues[sDebugMenuValueIndex]++;
                }
            }
        }
    } else if (g_Scene == SCENE_GROUND_CAMERA) {
        g_Cameras[INSET_CAMERA].view.scissorX = SCREEN_WIDTH / 2;
        g_Cameras[INSET_CAMERA].view.scissorY = SCREEN_HEIGHT / 2;
        g_Cameras[INSET_CAMERA].view.scissorWidth = SCREEN_WIDTH / 2;
        g_Cameras[INSET_CAMERA].view.scissorHeight = SCREEN_HEIGHT / 2;
    } else if (g_Scene == SCENE_BOOT_LOGOS) {
        UpdateBootLogos();
    } else if (g_Scene == 26) {
        func_801F4A40();
    } else if (g_Scene == SCENE_TITLE_SCREEN) {
        UpdateTitleScreen();
    } else if (g_Scene == 30) {
        UpdateStaffRoll();
    } else if (g_Scene == 29) {
        func_801F5F1C();
    }
    g_Cameras[INSET_CAMERA].view.viewportCenterX =
        g_Cameras[INSET_CAMERA].view.scissorX * 2 + g_Cameras[INSET_CAMERA].view.scissorWidth;
    g_Cameras[INSET_CAMERA].view.viewportCenterY =
        g_Cameras[INSET_CAMERA].view.scissorY * 2 + g_Cameras[INSET_CAMERA].view.scissorHeight;
    g_Viewports[VIEWPORT_INSET + g_DrawBufferIndex].vp.vscale[0] =
        2.0f * (g_Cameras[INSET_CAMERA].view.zoom * SCREEN_WIDTH);
    g_Viewports[VIEWPORT_INSET + g_DrawBufferIndex].vp.vscale[1] =
        2.0f * (g_Cameras[INSET_CAMERA].view.zoom * SCREEN_HEIGHT);
    g_Viewports[VIEWPORT_INSET + g_DrawBufferIndex].vp.vtrans[0] = g_Cameras[INSET_CAMERA].view.viewportCenterX * 2;
    g_Viewports[VIEWPORT_INSET + g_DrawBufferIndex].vp.vtrans[1] = g_Cameras[INSET_CAMERA].view.viewportCenterY * 2;
    sUpdateEndTime = OS_CYCLES_TO_USEC(osGetTime());
}

void UpdateOverlays(void) {
    if (g_Scene == SCENE_SWAPPING_MODULES) {
        if (!sHasUnloadedSceneModule) {
            sHasUnloadedSceneModule = TRUE;
            UnloadOverlay(GetOverlayOfModule(g_SceneModuleToUnload));
        } else if (CanLoadOverlay(GetOverlayOfModule(g_SceneModuleToLoad))) {
            LoadOverlay(GetOverlayOfModule(g_SceneModuleToLoad));
            sHasUnloadedSceneModule = FALSE;
            g_Scene = g_NextScene;
        }
    }
    if (g_SubModuleState == SUB_MODULE_SWAP_PENDING) {
        if (!sHasUnloadedSubModule) {
            sHasUnloadedSubModule = g_SubModuleState;
            UnloadOverlay(GetOverlayOfModule(g_SubModuleId));
            return;
        }
        if (CanLoadOverlay(GetOverlayOfModule(g_PendingSubModuleId))) {
            LoadOverlay(GetOverlayOfModule(g_PendingSubModuleId));
            g_SubModuleState = SUB_MODULE_LOADED;
            sHasUnloadedSubModule = FALSE;
            g_SubModuleId = g_PendingSubModuleId;
        }
    }
}

s32 IsSubModuleLoaded(void) {
    return g_SubModuleState == SUB_MODULE_LOADED;
}

/* The game modes are still known only by number */
void RequestSubModule(void) {
    g_SubModuleState = SUB_MODULE_SWAP_PENDING;
    switch (g_GameMode) {
        case 3:
        case 9:
            g_PendingSubModuleId = SUB_MODULE_ALTERNATE;
            break;
        case 11:
            g_PendingSubModuleId = SUB_MODULE_SPECIAL;
            break;
        default:
            g_PendingSubModuleId = SUB_MODULE_DEFAULT;
            break;
    }
}

void UnloadSubModule(void) {
    UnloadOverlay(GetOverlayOfModule(g_SubModuleId));
    g_SubModuleState = SUB_MODULE_UNLOADED;
}

void SetInsetViewRect(s32 centerX, s32 centerY, s32 width, s32 height) {
    sInsetViewWidth = width;
    sInsetViewHeight = height;
    sInsetViewCenterX = centerX;
    sInsetViewCenterY = centerY;
}

/* Grows or shrinks the inset view's window by a step and centres it */
void UpdateInsetView(void) {
    s32 *animation = &g_InsetView.animation;

    if (*animation == INSET_VIEW_OPENING) {
        if (g_InsetView.step == 1) {
            PlaySoundWithEffects(SOUND_INSET_VIEW_OPEN, g_NextSoundChannel, INSET_VIEW_SOUND_VOLUME, INSET_VIEW_SOUND_REVERB,
                          INSET_VIEW_SOUND_PAN);
        }
        g_Cameras[INSET_CAMERA].view.drawFlags = CAMERA_DRAW_SCENE;
        g_InsetView.step++;
        if (g_InsetView.step >= INSET_VIEW_STEP_COUNT) {
            g_InsetView.step = INSET_VIEW_STEP_COUNT;
            *animation = INSET_VIEW_STILL;
        }
    } else if (*animation == INSET_VIEW_CLOSING) {
        if (g_InsetView.step == INSET_VIEW_STEP_COUNT - 1) {
            PlaySoundWithEffects(SOUND_INSET_VIEW_CLOSE, g_NextSoundChannel, INSET_VIEW_SOUND_VOLUME, INSET_VIEW_SOUND_REVERB,
                          INSET_VIEW_SOUND_PAN);
        }
        g_InsetView.step--;
        if (g_InsetView.step <= 0) {
            g_InsetView.step = 0;
            *animation = INSET_VIEW_STILL;
            g_Cameras[INSET_CAMERA].view.drawFlags = 0;
        }
    }
    g_Cameras[INSET_CAMERA].view.scissorWidth = g_InsetView.step * sInsetViewWidth / INSET_VIEW_STEP_COUNT;
    g_Cameras[INSET_CAMERA].view.scissorHeight = g_InsetView.step * sInsetViewHeight / INSET_VIEW_STEP_COUNT;
    g_Cameras[INSET_CAMERA].view.scissorX = sInsetViewCenterX - g_InsetView.step * sInsetViewWidth / (2 * INSET_VIEW_STEP_COUNT);
    g_Cameras[INSET_CAMERA].view.scissorY = sInsetViewCenterY - g_InsetView.step * sInsetViewHeight / (2 * INSET_VIEW_STEP_COUNT);
    g_Cameras[INSET_CAMERA].view.zoom = 0.9f;
}

