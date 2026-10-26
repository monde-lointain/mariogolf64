#include "common.h"
#include "asset.h"
#include "heap.h"
#include "hole_display.h"
#include "round_hud.h"
#include "screen.h"
#include "sound.h"
#include "variables.h"

/*
 * The pause menu over a round, and the choice box that asks the player to
 * pick between a few options during a turn.
 */

#define PAUSE_CURSOR_IMAGE_ASSET 0x70F /* A pointing glove and two tees */
#define PAUSE_CLOUD_IMAGE_ASSET 0x70B  /* The wide cloud behind the pause menu */
#define PAUSE_ARROWS_IMAGE_ASSET 0x71E
#define CHOICE_BOX_CLOUD_IMAGE_ASSET 0x71F /* The small cloud behind a choice box */

#define ROUND_FRAMEBUFFER_COUNT 3
#define FULL_SONG_VOLUME 0x7F

extern s32 sIsPauseMenuLoaded;
extern u8 *sPauseCursorImage;
extern u8 *sMenuCloudImage; /* The pause menu's cloud, or the choice box's */
extern u8 *sPauseArrowsImage;
extern u8 *sChoiceBoxCursorImage;

void SetFramebufferCount(s32 count);

void HideRoundHud(void) {
    g_RoundHud.isLetterboxShown = FALSE;
    g_RoundHud.unkC = 0;
    g_RoundHud.unk7 = 0;
    g_RoundHud.unk89 = 0;
    g_RoundHud.unk8A = 0;
    g_RoundHud.unk8B = 0;
    g_RoundHud.unk8 = 0;
    g_RoundHud.unk9 = 0;
    g_RoundHud.unkA = 0;
    g_RoundHud.unkB = 0;
    g_RoundHud.unkD = 0;
}

void ResumeRound(void) {
    g_RoundHud.isLetterboxShown = TRUE;
    g_RoundHud.unkC = 1;
    g_RoundHud.unk7 = 1;
    g_RoundHud.unk89 = 0;
    g_RoundHud.unk8A = 0;
    g_RoundHud.unk8B = 0;
    g_RoundHud.unk8 = 1;
    g_RoundHud.unk9 = 1;
    g_RoundHud.unkA = 1;
    g_RoundHud.unkB = 1;
    g_RoundHud.unkD = 1;
    SetMenuBackdrop(MENU_BACKDROP_NONE);
    g_PauseScreen = PAUSE_SCREEN_NONE;
    SetFramebufferCount(ROUND_FRAMEBUFFER_COUNT);
    SetScreenEffect(SCREEN_EFFECT_NONE, 0);
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", func_80218DB0_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225440_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225450_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_8022545C_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225468_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225474_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225480_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225484_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225488_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225490_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225498_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_802254A0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", func_80218DD0_ovl_00);

void FreePauseMenu(void) {
    if (sIsPauseMenuLoaded == TRUE) {
        sIsPauseMenuLoaded = FALSE;
        FreeMemory(&sPauseCursorImage);
        FreeMemory(&sMenuCloudImage);
        FreeMemory(&sPauseArrowsImage);
    }
    SetSongVolume(FULL_SONG_VOLUME);
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", func_80219034_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", func_802199EC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", func_80219C74_ovl_00);

void LoadChoiceBoxImages(void) {
    EncodedAsset asset;

    sChoiceBoxCursorImage = AllocMemory(OpenEncodedAsset(PAUSE_CURSOR_IMAGE_ASSET, &asset));
    ReadEncodedAsset(sChoiceBoxCursorImage, &asset);
    sMenuCloudImage = AllocMemory(OpenEncodedAsset(CHOICE_BOX_CLOUD_IMAGE_ASSET, &asset));
    ReadEncodedAsset(sMenuCloudImage, &asset);
}

void FreeChoiceBoxImages(void) {
    FreeMemory(&sChoiceBoxCursorImage);
    FreeMemory(&sMenuCloudImage);
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", func_8021A6BC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", func_8021A724_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", func_8021A940_ovl_00);
