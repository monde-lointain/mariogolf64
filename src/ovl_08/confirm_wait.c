/*
 * MODULE_CONFIRM_WAIT, which UpdateGame runs in SCENE_CONFIRM_WAIT. On A or
 * Start it swaps itself for the boot logos' module and enters their scene.
 * Its draw routine, RewriteWord, draws nothing. No code found enters
 * SCENE_CONFIRM_WAIT.
 */
#include "common.h"

#include "controller.h"
#include "hole_display.h"
#include "overlay_manager.h"
#include "scene.h"
#include "sound.h"
#include "text.h"

/* Read by no routine */
static u8 D_801F4AF0_ovl_08[0x10] UNUSED;

/*
 * On A or Start, clears the image in slot 0 and swaps this overlay's module
 * for the boot logos' to enter their scene. UpdateGame calls whatever sits
 * at this address in SCENE_CONFIRM_WAIT.
 */
void WaitForConfirmButton(void) {
  if (g_ControllerStates[0].pressedButtons & (A_BUTTON | START_BUTTON)) {
    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
    g_Scene = SCENE_SWAPPING_MODULES;
    g_NextScene = SCENE_BOOT_LOGOS;
    g_SceneModuleToUnload = MODULE_CONFIRM_WAIT;
    g_SceneModuleToLoad = MODULE_BOOT_LOGOS;
    PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);

    /* The position goes unused with NO_MESSAGE */
    SetTextSlot(160, 114, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0, 0, 0, 0, 0, 0);
  }
}

void RewriteWord(s32* word) {
  *word = *word;
}
