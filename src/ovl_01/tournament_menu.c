#include "common.h"
#include "asset.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"

extern s32 g_MenuWindowState;
/* How far the window has opened across and down, from 0 to 1 */
extern f32 g_MenuWindowScaleX;
extern f32 g_MenuWindowScaleY;
extern s32 D_80230E9C_ovl_01;
extern s32 D_80230EA0_ovl_01;
/* The images that the window can show, and the buffer that holds the one it shows */
extern u16 g_MenuWindowImageAssets[15];
extern void *g_MenuWindowImage;

extern s32 D_80230F68_ovl_01;
extern u8 D_80230FD4_ovl_01[8];
extern s8 D_80105DBC[5];

void ResetMenuWindow(void) {
    g_MenuWindowState = MENU_WINDOW_OPENING;
    g_MenuWindowScaleX = 0.0f;
    g_MenuWindowScaleY = 0.0f;
    D_80230E9C_ovl_01 = 0;
    D_80230EA0_ovl_01 = 0;
}

void ShowMenuWindow(void) {
    D_80230E9C_ovl_01 = 0;
    D_80230EA0_ovl_01 = 0;
    g_MenuWindowState = MENU_WINDOW_OPEN;
    g_MenuWindowScaleX = 1.0f;
    g_MenuWindowScaleY = 1.0f;
}

void CloseMenuWindow(void) {
    g_MenuWindowState = MENU_WINDOW_CLOSING;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80226764_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80226788_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_802267B0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80226904_ovl_01);

void LoadMenuWindowImage(s32 imageIndex) {
    EncodedAsset asset;

    if (imageIndex < 0) {
        imageIndex = 0;
    }
    if (imageIndex >= ARRAY_COUNT(g_MenuWindowImageAssets)) {
        imageIndex = ARRAY_COUNT(g_MenuWindowImageAssets) - 1;
    }
    OpenEncodedAsset(g_MenuWindowImageAssets[imageIndex], &asset);
    ReadEncodedAsset(g_MenuWindowImage, &asset);
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_802269AC_ovl_01);

void func_80226A34_ovl_01(void) {
    GetSaveData();
    func_8005AF74();
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80226A58_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022747C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_802287EC_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80228A80_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80228B60_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80228C00_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80229278_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_802296C4_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80229CB0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022A090_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022A43C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022A8F4_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022ADE8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022B360_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022B46C_ovl_01);

s32 func_8022B5E0_ovl_01(void) {
    s32 isEmpty = TRUE;

    if (D_80230F68_ovl_01 > 0) {
        if (D_80230F68_ovl_01 == 8) {
            D_80230F68_ovl_01 = 7;
        }
        D_80230FD4_ovl_01[D_80230F68_ovl_01] = 0xFF;
        D_80230F68_ovl_01--;
        isEmpty = FALSE;
    } else if (D_80230FD4_ovl_01[0] != 0xFF) {
        D_80230FD4_ovl_01[0] = 0xFF;
        isEmpty = FALSE;
    }
    return isEmpty;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022B668_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022B75C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022B7E0_ovl_01);

s32 func_8022BDB0_ovl_01(void) {
    return D_80105DBC[4] == -1;
}

s32 func_8022BDC4_ovl_01(void) {
    return D_80105DBC[4];
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022BDD0_ovl_01);

void func_8022BDF8_ovl_01(void) {
}
