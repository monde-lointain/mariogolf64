#include "common.h"
#include "controller.h"
#include "scene.h"
#include "sound.h"

/* Given this image, func_800719A0 stores it in the slot and sets up nothing else */
#define NO_IMAGE (-1)

void func_8009226C(s32);
void func_800719A0(s32 x, s32 y, s32 image, s32, s32 slot, s32, s32, s32, s32, s32);

/*
 * On A or Start, clears the image in slot 0 and swaps this overlay's module
 * for 0x0C to enter scene 27. UpdateGame calls whatever sits at this address
 * in scene 26.
 */
void WaitForConfirmButton(void) {
    if (g_ControllerStates[0].pressedButtons & (A_BUTTON | START_BUTTON)) {
        func_8009226C(8);
        g_Scene = SCENE_SWAPPING_MODULES;
        g_NextScene = 27;
        g_SceneModuleToUnload = 0x0D;
        g_SceneModuleToLoad = 0x0C;
        PlaySound(0x5D, 0xF, 0x7F);
        /* The position goes unused with NO_IMAGE */
        func_800719A0(160, 114, NO_IMAGE, 1, 0, 0, 0, 0, 0, 0);
    }
}

void RewriteWord(s32 *word) {
    *word = *word;
}
