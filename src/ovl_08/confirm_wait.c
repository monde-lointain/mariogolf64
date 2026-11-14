#include "confirm_wait.h"

#include <ultra64.h>

#include "controllers/controller.h"
#include "scene.h"
#include "sky_and_hud_animations.h"
#include "sound.h"
#include "system/compiler.h"
#include "system/overlay_manager.h"
#include "text.h"

static u8 f_UnusedConfirmWaitBytes[0x10] UNUSED;

void WaitForConfirmButton(void) {
    if (g_ControllerStates[0].pressedButtons & (A_BUTTON | START_BUTTON)) {
        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
        g_Scene = SCENE_SWAPPING_MODULES;
        g_NextScene = SCENE_BOOT_LOGOS;
        g_SceneModuleToUnload = MODULE_CONFIRM_WAIT;
        g_SceneModuleToLoad = MODULE_BOOT_LOGOS;
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);

        SetTextSlot(
            CLEARED_TEXT_X,
            CLEARED_TEXT_Y,
            NO_MESSAGE,
            TEXT_SLOT_LARGE_FONT,
            0,
            0,
            0,
            0,
            0,
            0
        );
    }
}

void DoNothingWithDisplayList(Gfx **displayListHead) {
    *displayListHead = *displayListHead;
}
