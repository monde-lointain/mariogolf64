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
extern s32 sHasUnloadedSceneModule;
extern s32 sHasUnloadedSubModule;

/* Screen sizes, in pixels */
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

/* The border around the main camera's scissor rectangle, in pixels */
#define SCREEN_BORDER 8

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

INCLUDE_ASM("asm/nonmatchings/game", func_8002A97C);

INCLUDE_ASM("asm/nonmatchings/game", func_8002A9C4);

INCLUDE_ASM("asm/nonmatchings/game", func_8002ADD4);

INCLUDE_ASM("asm/nonmatchings/game", func_8002B280);

INCLUDE_ASM("asm/nonmatchings/game", func_8002B924);

INCLUDE_ASM("asm/nonmatchings/game", func_8002BD9C);

INCLUDE_ASM("asm/nonmatchings/game", func_8002BDA4);

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

