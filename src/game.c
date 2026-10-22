#include "common.h"
#include "camera.h"
#include "functions.h"
#include "overlay_manager.h"
#include "scene.h"
#include "variables.h"

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

extern s32 sScreenEffect;
/* Holds a frame for the saved-frame effect */
extern u16 *sSavedFrameBuffer;
/* The effect's length in frames; the tiled image counts it down */
extern s32 sScreenEffectFrameCount;
/* Set when the effects stop, never read */
extern s32 sUnreadEffectSetting;
extern s32 sHasUnloadedSceneModule;
extern s32 sHasUnloadedSubModule;

/* Screen sizes, in pixels */
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

/* The border around the main camera's scissor rectangle, in pixels */
#define SCREEN_BORDER 8

/* How the saved-frame effect splits the screen into strips, in rows */
#define SAVED_FRAME_STRIP_HEIGHT 6
#define SAVED_FRAME_STRIP_COUNT 38
#define SAVED_FRAME_LAST_STRIP_HEIGHT 2

/* sScreenEffect values */
#define SCREEN_EFFECT_SPIN 0        /* Spins the saved frame away */
#define SCREEN_EFFECT_EMPTY 1       /* Draws nothing */
#define SCREEN_EFFECT_TILED_IMAGE 2 /* Repeats part of the saved frame */
#define SCREEN_EFFECT_SAVED_FRAME 3 /* Draws a frame saved elsewhere */
#define SCREEN_EFFECT_NONE 4

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
    g_MapCamera.view.unreadValue = 0;
    g_MapCamera.view.scissorX = SCREEN_BORDER;
    g_MapCamera.view.scissorY = SCREEN_BORDER;
    g_MapCamera.view.scissorWidth = SCREEN_WIDTH - 2 * SCREEN_BORDER;
    g_MapCamera.view.scissorHeight = SCREEN_HEIGHT - 2 * SCREEN_BORDER;
    g_MapCamera.view.unreadSettings[0] = SCREEN_HEIGHT - 2 * SCREEN_BORDER;
    g_MapCamera.view.unreadSettings[1] = SCREEN_HEIGHT - 2 * SCREEN_BORDER;
    g_MapCamera.view.unreadSettings[2] = SCREEN_WIDTH;
    g_MapCamera.view.unreadSettings[3] = SCREEN_HEIGHT;
    g_MapCamera.view.zoom = 1.0f;
    g_SpareCamera.view.eye[0] = 0.0f;
    g_SpareCamera.view.eye[1] = 0.0f;
    g_SpareCamera.view.eye[2] = 0.0f;
    g_SpareCamera.view.lookAt[0] = 0.0f;
    g_SpareCamera.view.lookAt[1] = 0.0f;
    g_SpareCamera.view.lookAt[2] = 1.0f;
    g_SpareCamera.view.unreadValue = 0;
    g_SpareCamera.view.scissorX = 128;
    g_SpareCamera.view.scissorY = 64;
    g_SpareCamera.view.scissorWidth = 160;
    g_SpareCamera.view.scissorHeight = 120;
    g_SpareCamera.view.unreadSettings[0] = 0;
    g_SpareCamera.view.unreadSettings[1] = 30;
    g_SpareCamera.view.unreadSettings[2] = 416;
    g_SpareCamera.view.unreadSettings[3] = 248;
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
    g_Cameras[0].view.unreadSettings[2] = SCREEN_WIDTH;
    g_Cameras[0].view.unreadSettings[3] = SCREEN_HEIGHT;
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
    g_Cameras[1].view.unreadSettings[2] = 256;
    g_Cameras[1].view.unreadSettings[3] = 256;
    g_Cameras[1].view.drawFlags = 0;
    g_Cameras[1].view.fieldOfView = 60.0f;
    osViSetSpecialFeatures(VI_FEATURES);
    func_8004D794();
    func_80071370();
    func_80081284();
    func_8003E400();

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

INCLUDE_ASM("asm/nonmatchings/game", func_8002ADD4);

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

INCLUDE_ASM("asm/nonmatchings/game", DrawSpinEffect);

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

INCLUDE_ASM("asm/nonmatchings/game", func_8002BE78);

INCLUDE_ASM("asm/nonmatchings/game", func_8002BF1C);

INCLUDE_ASM("asm/nonmatchings/game", func_8002CDA8);

INCLUDE_ASM("asm/nonmatchings/game", func_8002DAC0);

INCLUDE_ASM("asm/nonmatchings/game", DrawGame);

INCLUDE_ASM("asm/nonmatchings/game", func_80031450);

INCLUDE_ASM("asm/nonmatchings/game", DrawBlankFrame);

INCLUDE_ASM("asm/nonmatchings/game", UpdateGame);

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
    s32 *animation = &g_InsetViewAnimation;

    if (*animation == INSET_VIEW_OPENING) {
        if (g_InsetViewStep == 1) {
            func_80050DA0(SOUND_INSET_VIEW_OPEN, g_NextSoundChannel, INSET_VIEW_SOUND_VOLUME, INSET_VIEW_SOUND_REVERB,
                          INSET_VIEW_SOUND_PAN);
        }
        g_Cameras[INSET_CAMERA].view.drawFlags = CAMERA_DRAW_SCENE;
        g_InsetViewStep++;
        if (g_InsetViewStep >= INSET_VIEW_STEP_COUNT) {
            g_InsetViewStep = INSET_VIEW_STEP_COUNT;
            *animation = INSET_VIEW_STILL;
        }
    } else if (*animation == INSET_VIEW_CLOSING) {
        if (g_InsetViewStep == INSET_VIEW_STEP_COUNT - 1) {
            func_80050DA0(SOUND_INSET_VIEW_CLOSE, g_NextSoundChannel, INSET_VIEW_SOUND_VOLUME, INSET_VIEW_SOUND_REVERB,
                          INSET_VIEW_SOUND_PAN);
        }
        g_InsetViewStep--;
        if (g_InsetViewStep <= 0) {
            g_InsetViewStep = 0;
            *animation = INSET_VIEW_STILL;
            g_Cameras[INSET_CAMERA].view.drawFlags = 0;
        }
    }
    g_Cameras[INSET_CAMERA].view.scissorWidth = g_InsetViewStep * sInsetViewWidth / INSET_VIEW_STEP_COUNT;
    g_Cameras[INSET_CAMERA].view.scissorHeight = g_InsetViewStep * sInsetViewHeight / INSET_VIEW_STEP_COUNT;
    g_Cameras[INSET_CAMERA].view.scissorX = sInsetViewCenterX - g_InsetViewStep * sInsetViewWidth / (2 * INSET_VIEW_STEP_COUNT);
    g_Cameras[INSET_CAMERA].view.scissorY = sInsetViewCenterY - g_InsetViewStep * sInsetViewHeight / (2 * INSET_VIEW_STEP_COUNT);
    g_Cameras[INSET_CAMERA].view.zoom = 0.9f;
}

