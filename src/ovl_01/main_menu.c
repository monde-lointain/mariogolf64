#include "common.h"
#include "billboard.h"
#include "character_slot.h"
#include "course.h"
#include "game_flags.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "libc.h"
#include "menu.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "variables.h"

/* The wind and light of the hole that loads behind the menus */
#define CHOSEN_HOLE_WIND_SPEED_MASK 0x3F
#define CHOSEN_HOLE_WIND_SPEED_STEP 6000
#define CHOSEN_HOLE_LIGHT_PITCH 0x1000
#define CHOSEN_HOLE_LIGHT_YAW 0x2000

/* Labels in the shadowed font */
extern char g_ContinueLabelText[];
extern char g_BirdieBadgeLabelText[];

extern s8 D_801052E8;
extern s32 D_800FE334;
extern s32 D_801B609C;
extern s32 D_800FBD40[];
extern s8 D_801323A0[];
extern s32 D_8022BE0C_ovl_01;
extern s32 D_801B7070;
extern u8 D_801B9410;

s32 GetClubTournamentCourse(void);
s32 SaveOrContinueGameByMode(s32 isContinuing);
s32 func_801F7660_ovl_01(s32, s32);
void func_801FB54C_ovl_01(Gfx **displayListHead, s32);

void *GetMenuImageBuffer(void) {
    return g_MenuImageBuffer;
}

void StartLoadingChosenHole(void) {
    u32 slotIndex;

    GetSaveData();
    slotIndex = 0;
    func_8005AF74();
    do {
        if (IsCharacterSlotBusy(slotIndex)) {
            FreeCharacterSlot(slotIndex);
        }
        slotIndex++;
    } while (slotIndex != CHARACTER_SLOT_COUNT);
    D_800FE334 = 1;
    if (g_MenuScreen == MENU_SCREEN_UNK23 || g_MenuScreen == MENU_SCREEN_UNK13) {
        g_CourseIndex = g_MenuChoices.courseIndex;
        g_HoleIndex = g_MenuChoices.holeIndex;
    } else if (g_MenuScreen == MENU_SCREEN_CLUB_TOURNAMENT) {
        g_CourseIndex = GetClubTournamentCourse();
        g_IsMiniGolf = g_CourseIndex >= MINI_GOLF_COURSE_INDEX;
        g_HoleIndex = 0;
    } else if (g_GameMode == GAME_MODE_TRAINING) {
        g_CourseIndex = 0;
        g_HoleIndex = 0;
    } else {
        if (g_IsMiniGolf) {
            g_CourseIndex =
                g_MenuChoices.unk14 + MINI_GOLF_COURSE_INDEX + g_MenuChoices.unk13 * MINI_GOLF_COURSES_PER_RECORD;
        } else {
            g_CourseIndex = D_801323A0[0x16];
        }
        g_HoleIndex = GetFirstHoleIndex();
    }
    D_801B609C = 0;
    g_HoleConditions.isRaining = FALSE;
    g_HoleConditions.windSpeed = (rand() & CHOSEN_HOLE_WIND_SPEED_MASK) * CHOSEN_HOLE_WIND_SPEED_STEP;
    g_HoleConditions.windDirection = rand();
    g_HoleConditions.sceneColorIndex = SCENE_COLOR_INDEX_DRIVING_RANGE;
    g_LightPitch = CHOSEN_HOLE_LIGHT_PITCH;
    g_LightYaw = CHOSEN_HOLE_LIGHT_YAW;
    SetGameFlag(GAME_FLAG_HOLE_LOADED);
    g_PlayerCount = 1;
    StopLoaderThread();
    StartLoaderThread();
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F4C34_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F4C78_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F6A38_ovl_01);

void func_801F6B20_ovl_01(void) {
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F6B28_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F70F0_ovl_01);

void func_801F75EC_ovl_01(void) {
    D_801323A0[2] = 0;
    if (D_800FBD40[2] == 1) {
        do {
            D_801323A0[2]++;
            if (D_801323A0[2] > D_8022BE0C_ovl_01 - 1) {
                D_801323A0[2] = 0;
            }
        } while (D_800FBD40[D_801323A0[2] + 2] == 1);
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F7660_ovl_01);

void func_801F7CD0_ovl_01(void) {
    s32 isDone = IsHudAnimationDone();

    if (isDone == TRUE && func_801F7660_ovl_01(1, 2) == -1) {
        D_801B7070 = isDone;
        g_MenuScreen = MENU_SCREEN_TITLE;
        FreeAllCharacterSlots();
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F7D28_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F8BAC_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F93D8_ovl_01);

void func_801F9BCC_ovl_01(void) {
    if (IsHudAnimationDone() == TRUE) {
        if (g_MenuPressedButtons & A_BUTTON) {
            PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuScreen = MENU_SCREEN_EXIT;
            g_GameMode = GAME_MODE_STROKE;
            SaveOrContinueGameByMode(TRUE);
            return;
        }
        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            g_MenuScreen = MENU_SCREEN_TITLE;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F9C58_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F9E38_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F9FF8_ovl_01);

void func_801FA1B8_ovl_01(void) {
    if (IsHudAnimationDone() == TRUE && (g_MenuPressedButtons & B_BUTTON)) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        g_MenuScreen = MENU_SCREEN_MAIN;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FA20C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FADBC_ovl_01);

void LeaveMenuWithWipe(void) {
    if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
        if (GetWipeProgress() == 0) {
            g_MenuScreen = MENU_SCREEN_EXIT;
        }
    } else {
        LoadWipeShape(0);
        StartWipe(WIPE_CLOSING);
        D_801052E8 = 0;
        D_801B9410 = 0;
    }
}

void LeaveMenuWithCourseWipe(void) {
    if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
        if (GetWipeProgress() == 0) {
            g_MenuScreen = MENU_SCREEN_EXIT;
        }
    } else {
        LoadWipeShape(g_MenuChoices.courseIndex);
        StartWipe(WIPE_CLOSING);
        D_801052E8 = 0;
        D_801B9410 = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FAFEC_ovl_01);

void func_801FB11C_ovl_01(void) {
    if (g_MenuPressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        g_MenuScreen = MENU_SCREEN_MAIN;
    }
}

void func_801FB15C_ovl_01(void) {
    if (g_MenuPressedButtons & A_BUTTON) {
        PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        g_MenuScreen = MENU_SCREEN_TITLE;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FB198_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FB54C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FC60C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FC754_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FCB74_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FCEF8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FD1B4_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FD4F0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FD7B0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FDA40_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FE4D0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_802004A0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80200A40_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80200EF0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80201574_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80202000_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_802047F0_ovl_01);

void DrawContinueLabel(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    DrawShadowedText(&gfx, 96, 64, g_ContinueLabelText);
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80205038_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_802058B8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80206180_ovl_01);

void func_80206A2C_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    func_801FB54C_ovl_01(&gfx, 61);
    *displayListHead = gfx;
}

const char D_802316EC_ovl_01[] = "Peach's Castle";
const char D_802316FC_ovl_01[] = "Luigi's Garden";

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80206A68_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80207D08_ovl_01);

void func_80207E18_ovl_01(void) {
}

void func_80207E20_ovl_01(void) {
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80207E28_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_802081E8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_8020879C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80208F8C_ovl_01);

void DrawBirdieBadgeLabel(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    DrawShadowedText(&gfx, 64, 32, g_BirdieBadgeLabelText);
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80209478_ovl_01);
