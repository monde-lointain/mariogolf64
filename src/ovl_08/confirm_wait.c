/*
 * Module 0x0D, which UpdateGame runs in scene 26. On A or Start it swaps itself
 * for the boot logos' module and enters their scene, scene 27. Its draw
 * routine, RewriteWord, draws nothing. No code found enters scene 26.
 */
#include "common.h"

#include "controller.h"
#include "hole_display.h"
#include "overlay_manager.h"
#include "scene.h"
#include "sound.h"
#include "text.h"

/*
 * On A or Start, clears the image in slot 0 and swaps this overlay's module
 * for 0x0C to enter scene 27. UpdateGame calls whatever sits at this address
 * in scene 26.
 */
void WaitForConfirmButton(void) {
  if (g_ControllerStates[0].pressedButtons & (A_BUTTON | START_BUTTON)) {
    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
    g_Scene = SCENE_SWAPPING_MODULES;
    g_NextScene = 27;
    g_SceneModuleToUnload = 0x0D;
    g_SceneModuleToLoad = MODULE_BOOT_LOGOS;
    PlaySound(0x5D, 0xF, 0x7F);

    /* The position goes unused with NO_MESSAGE */
    SetTextSlot(160, 114, NO_MESSAGE, 1, 0, 0, 0, 0, 0, 0);
  }
}

void RewriteWord(s32* word) {
  *word = *word;
}
