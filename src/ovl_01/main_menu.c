#include "common.h"
#include "asset.h"
#include "billboard.h"
#include "character_slot.h"
#include "course.h"
#include "game_flags.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "libc.h"
#include "menu.h"
#include "menu_memory.h"
#include "overlay_manager.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "variables.h"

/* The wind and light of the hole that loads behind the menus */
#define CHOSEN_HOLE_WIND_SPEED_MASK 0x3F
#define CHOSEN_HOLE_WIND_SPEED_STEP 6000
#define CHOSEN_HOLE_LIGHT_PITCH 0x1000
#define CHOSEN_HOLE_LIGHT_YAW 0x2000

/* The module that the menus draw as */
#define MODULE_MENU 2

/* How the menus' bobbing items move: the phase step per frame, and the height in pixels */
#define MENU_BOB_PHASE_STEP 0.13f
#define MENU_BOB_HEIGHT 5.0f

/* Labels in the shadowed font */
extern char g_ContinueLabelText[];
extern char g_BirdieBadgeLabelText[];

/* The bobbing of the menus' items: its phase, and the offset that it gives */
extern f32 sMenuBobOffset;
extern f32 sMenuBobPhase;

/* Players whose D_800C1FF4 holds this never clash with the others */
#define UNSHARED_SLOT_INDEX 4

/*
 * The 256 color images of D_801052BC and D_801B60D0 that func_801FD7B0 and
 * func_801FCEF8 draw, in strips that fit the texture memory
 */
#define MENU_BANNER_WIDTH 200
#define MENU_BANNER_STRIP_HEIGHT 10
#define MENU_BANNER_STRIP_COUNT 3
#define MENU_BANNER_LEFT 60
#define MENU_BANNER_TOP 18
#define UNUSED_BANNER_TOP 16

/* func_801FD1B4 squeezes D_801B60D0 to this width at a given position; nothing calls it */
#define SMALL_BANNER_WIDTH 96

/* func_801FD4F0 stretches D_801B60D0; its strips overlap, as they are taller than their spacing */
#define LARGE_BANNER_LEFT 26
#define LARGE_BANNER_TOP 174
#define LARGE_BANNER_WIDTH 270
#define LARGE_BANNER_STRIP_HEIGHT 20
#define LARGE_BANNER_STRIP_SPACING 16

/*
 * func_801FCB74 reads D_800FF420's buffer as a 160 texel wide image, though
 * menu.c loads the 256 texel wide board into it; its strips overlap, as each
 * samples its rows at 8/9 of a texel
 */
#define NARROW_BOARD_WIDTH 160
#define NARROW_BOARD_STRIP_HEIGHT 8
#define NARROW_BOARD_STRIP_COUNT 20
#define NARROW_BOARD_STRIP_SPACING 7
#define NARROW_BOARD_ROW_SCALE 9
#define NARROW_BOARD_LEFT 15
#define NARROW_BOARD_TOP 36
#define NARROW_BOARD_SHOWN_WIDTH 170

/*
 * func_801FC754 draws the course backdrop of D_801F4A20 at double size. Each
 * strip loads a row either side of the rows it shows, so that the filtering
 * blends across the seams.
 */
#define BACKDROP_WIDTH 152
#define BACKDROP_SCALE 2
#define BACKDROP_SEAM_ROW_COUNT 2
#define BACKDROP_SECOND_ROW_START (1 << 5)
#define BACKDROP_STRIP_ROW_COUNT 11
#define BACKDROP_STRIP_COUNT 11
#define BACKDROP_LEFT 8
#define BACKDROP_FIRST_TOP 8
#define BACKDROP_TOP 10

extern s8 D_800C1FF4[MAX_PLAYER_COUNT];
extern void *D_800FF420[2];
extern s32 D_801051F0;
extern void *D_801052BC[2];
extern void *D_801B60D0;
extern void *D_801F4A20;
extern s16 D_8022BE8C_ovl_01;
extern s16 D_8022BE92_ovl_01;
extern s32 D_8022F8C8_ovl_01;
extern s8 D_8022F8CD_ovl_01;
extern u8 D_8022F8CE_ovl_01;
extern s32 D_80105B68;
extern s32 D_8022F8C0_ovl_01;
extern u8 D_8022F8C4_ovl_01;
extern s8 D_8022F8C5_ovl_01;
extern void *D_8010526C[2];
extern void *D_800FBE30[2];
extern u8 D_8022F8E0_ovl_01[MAX_PLAYER_COUNT + 1][MAX_PLAYER_COUNT];
/* The 24x24 icon that func_80207E28 animates, such as the birdie badge; each frame shows for two calls */
#define ANIMATED_ICON_SIZE 24
#define ANIMATED_ICON_IMAGE_SIZE (ANIMATED_ICON_SIZE * ANIMATED_ICON_SIZE / 2)
#define ANIMATED_ICON_FRAME_COUNT 6
#define ANIMATED_ICON_FRAME_DURATION 2
/* func_80200A40 draws the chosen mode's icon, 64x64 CI8, in two strips */
#define MODE_ICON_WIDTH 64
#define MODE_ICON_STRIP_HEIGHT 32
#define MODE_ICON_STRIP_COUNT 2
#define MODE_ICON_FRAME_DURATION 4
/* The most modes the mode select offers for a player count */
#define MODE_CHOICE_LIMIT 12

extern u8 D_8022BEBC_ovl_01[MAX_PLAYER_COUNT][MODE_CHOICE_LIMIT];

/* func_80208F8C draws a frame of the waving Toad (assets 0x710 onward), 48x51 CI8, in two strips */
#define TOAD_WIDTH 48
#define TOAD_STRIP_HEIGHT 30
#define TOAD_LAST_STRIP_HEIGHT 21
#define TOAD_STRIP_COUNT 2
/* The parts of the menu backdrop that func_801FC60C draws */
#define MENU_LAYER_UNK1 (1 << 0)
#define MENU_LAYER_UNK4 (1 << 2)
#define MENU_LAYER_UNK8 (1 << 3)
#define MENU_LAYER_UNK10 (1 << 4)

/* The choices of both main menus; the one without Transmit stops at its options */
#define MAIN_CHOICE_CLUBHOUSE 0
#define MAIN_CHOICE_HELP 1
#define MAIN_CHOICE_OPTIONS 2
#define MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT 3
#define MAIN_CHOICE_CLUB_TOURNAMENT 2
#define MAIN_CHOICE_FULL_OPTIONS 3
#define MAIN_CHOICE_COUNT 4

/* The clubhouse's rows, and the rooms that D_8022BF1C gives each of them */
#define CLUBHOUSE_CHOICE_COUNT 5
#define CLUBHOUSE_ROOM_UNK11 0
#define CLUBHOUSE_ROOM_UNK12 1
#define CLUBHOUSE_ROOM_UNK13 2
#define CLUBHOUSE_ROOM_BIRDIE_BADGES 3
#define CLUBHOUSE_ROOM_CHARACTER_TABLE 4
#define CLUBHOUSE_ROOM_BEST_TIMES 5

/* Past func_80217B88's presets, so that it clears the values instead */
#define MENU_UNKE_PRESET_NONE 10

/* A byte of g_MapCamera that the menus set: the top of its view's mode */
#define MAP_CAMERA_UNK4C 0x4C

/* The choices of the list that func_801FAFEC runs, which no menu screen dispatches to */
#define UNUSED_LIST_CHOICE_COUNT 21

/* The debug caddy select: its two caddies, and where it draws its title and names */
#define CADDY_COUNT 2
#define CADDY_SELECT_TITLE_X 64
#define CADDY_SELECT_TITLE_Y 32
#define CADDY_SELECT_NAME_X 96
#define CADDY_SELECT_FIRST_Y 64
#define CADDY_SELECT_LINE_HEIGHT 8

extern char *D_8022C0D0_ovl_01[CADDY_COUNT];
extern const char D_8023176C_ovl_01[];
extern s8 D_801052E8;
extern s32 D_800FE334;
extern s8 D_801B60B9;
extern u8 D_801323E6;
extern s16 D_8022BF1C_ovl_01[CLUBHOUSE_CHOICE_COUNT];
extern s32 D_8022BE18_ovl_01;
extern s32 D_801B609C;
extern s32 D_800FBD40[];
extern s8 D_801323A0[];
extern s32 D_8022BE0C_ovl_01;
extern s32 D_801B7070;
extern u8 D_801B9410;
extern s8 D_800FF4C0;
extern s8 D_801B5634;

s32 GetClubTournamentCourse(void);
s32 SaveOrContinueGameByMode(s32 isContinuing);
s32 func_801F7660_ovl_01(s32, s32);
void func_80217B88_ovl_01(s32 requestedPreset);
void func_80228B60_ovl_01(void);
void func_801FB54C_ovl_01(Gfx **displayListHead, s32);
void func_801FC754_ovl_01(Gfx **displayListHead);
void func_801FCB74_ovl_01(Gfx **displayListHead);
void func_801FD4F0_ovl_01(Gfx **displayListHead);
void func_801FD7B0_ovl_01(Gfx **displayListHead);
void func_801FDA40_ovl_01(Gfx **displayListHead);
void func_801FE4D0_ovl_01(Gfx **displayListHead);
void func_80201574_ovl_01(Gfx **displayListHead);
void func_80202000_ovl_01(Gfx **displayListHead);
void func_80205038_ovl_01(Gfx **displayListHead);
void func_802058B8_ovl_01(Gfx **displayListHead);
void func_80206180_ovl_01(Gfx **displayListHead);
void func_80206A68_ovl_01(Gfx **displayListHead);
void func_80207D08_ovl_01(Gfx **displayListHead);
void func_80207E18_ovl_01(Gfx **displayListHead);
void DrawBirdieBadgeLabel(Gfx **displayListHead);
void func_8020B590_ovl_01(Gfx **displayListHead, u8 isSinglePlayer);
void func_8020E898_ovl_01(Gfx **displayListHead);
void func_80213748_ovl_01(Gfx **displayListHead);
void func_80215DC4_ovl_01(Gfx **displayListHead);
void func_8021D3AC_ovl_01(Gfx **displayListHead);
void func_8021FC5C_ovl_01(Gfx **displayListHead);
void func_802230C0_ovl_01(Gfx **displayListHead);
void func_80229CB0_ovl_01(Gfx **displayListHead);
void DrawHelpBoard(Gfx **displayListHead);
void SetTextColor(Gfx **displayListHead, s32 red, s32 green, s32 blue);

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

s32 GetChosenCourseIndex(void) {
    if (g_IsMiniGolf) {
        return g_MenuChoices.unk14 + MINI_GOLF_COURSE_INDEX + g_MenuChoices.unk13 * MINI_GOLF_COURSES_PER_RECORD;
    }
    return D_801323A0[0x16];
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F4C78_ovl_01);

/*
 * Doesn't match: the original reloads g_PlayerCount for each pass of the
 * outer loop, where this keeps it in a register.
 */
#ifdef NON_MATCHING
void func_801F6A38_ovl_01(void) {
    s32 playerIndex;
    s32 otherIndex;

    D_801051F0 = TRUE;
    for (playerIndex = 0; playerIndex != g_PlayerCount; playerIndex++) {
        for (otherIndex = 0; otherIndex != g_PlayerCount; otherIndex++) {
            if (playerIndex != otherIndex && D_800C1FF4[playerIndex] != UNSHARED_SLOT_INDEX &&
                D_800C1FF4[otherIndex] != UNSHARED_SLOT_INDEX && D_800C1FF4[playerIndex] == D_800C1FF4[otherIndex]) {
                D_801051F0 = FALSE;
            }
        }
    }
    for (playerIndex = 0; playerIndex != g_PlayerCount; playerIndex++) {
        g_Players[playerIndex].soundPan = D_8022F8E0_ovl_01[g_PlayerCount][playerIndex];
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F6A38_ovl_01);
#endif

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

void func_801F9C58_ovl_01(void) {
    s32 isDone = IsHudAnimationDone();

    if (isDone == TRUE) {
        if (g_MenuPressedButtons & A_BUTTON) {
            switch (g_MenuChoices.mainChoice) {
                case MAIN_CHOICE_CLUBHOUSE:
                    g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
                    break;
                case MAIN_CHOICE_CLUB_TOURNAMENT:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    func_80228B60_ovl_01();
                    g_MenuScreen = MENU_SCREEN_CLUB_TOURNAMENT;
                    break;
                case MAIN_CHOICE_HELP:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_HELP;
                    if (IsGameFlagSet(GAME_FLAG_UNK56)) {
                        g_GameMode = GAME_MODE_GET_CHARACTER;
                        g_PlayerCount = 2;
                        D_800FF4C0 = 1;
                        ((u8 *)&g_MapCamera)[MAP_CAMERA_UNK4C] = 2;
                        D_801B5634 = 0;
                        func_80217B88_ovl_01(MENU_UNKE_PRESET_NONE);
                    }
                    break;
                case MAIN_CHOICE_FULL_OPTIONS:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_OPTIONS;
                    break;
            }
            PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            return;
        }
        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            g_MenuScreen = MENU_SCREEN_TITLE;
            PlaySong(SONG_TITLE, 0);
            return;
        }
        if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuChoices.mainChoice--;
        } else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuChoices.mainChoice++;
        }
        if (g_MenuChoices.mainChoice >= MAIN_CHOICE_COUNT) {
            g_MenuChoices.mainChoice = 0;
        }
        if (g_MenuChoices.mainChoice < 0) {
            g_MenuChoices.mainChoice = MAIN_CHOICE_COUNT - 1;
        }
    }
}

void func_801F9E38_ovl_01(void) {
    s32 isDone = IsHudAnimationDone();

    if (isDone == TRUE) {
        if (g_MenuPressedButtons & A_BUTTON) {
            switch (g_MenuChoices.mainChoiceWithoutTransmit) {
                case MAIN_CHOICE_CLUBHOUSE:
                    g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
                    break;
                case MAIN_CHOICE_HELP:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_HELP;
                    if (IsGameFlagSet(GAME_FLAG_UNK56)) {
                        g_GameMode = GAME_MODE_GET_CHARACTER;
                        g_PlayerCount = 2;
                        D_800FF4C0 = 1;
                        ((u8 *)&g_MapCamera)[MAP_CAMERA_UNK4C] = 2;
                        D_801B5634 = 0;
                        func_80217B88_ovl_01(MENU_UNKE_PRESET_NONE);
                    }
                    break;
                case MAIN_CHOICE_OPTIONS:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_OPTIONS;
                    break;
            }
            PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            return;
        }
        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            g_MenuScreen = MENU_SCREEN_TITLE;
            PlaySong(SONG_TITLE, 0);
            return;
        }
        if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuChoices.mainChoiceWithoutTransmit--;
        } else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuChoices.mainChoiceWithoutTransmit++;
        }
        if (g_MenuChoices.mainChoiceWithoutTransmit >= MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT) {
            g_MenuChoices.mainChoiceWithoutTransmit = 0;
        }
        if (g_MenuChoices.mainChoiceWithoutTransmit < 0) {
            g_MenuChoices.mainChoiceWithoutTransmit = MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT - 1;
        }
    }
}

void func_801F9FF8_ovl_01(void) {
    s32 isDone = IsHudAnimationDone();

    if (isDone == TRUE) {
        if (g_MenuPressedButtons & A_BUTTON) {
            switch (D_8022BF1C_ovl_01[g_MenuChoices.clubhouseChoice]) {
                case CLUBHOUSE_ROOM_UNK11:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_UNK11;
                    break;
                case CLUBHOUSE_ROOM_UNK12:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_UNK12;
                    break;
                case CLUBHOUSE_ROOM_UNK13:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_UNK13;
                    break;
                case CLUBHOUSE_ROOM_BIRDIE_BADGES:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_BIRDIE_BADGES;
                    break;
                case CLUBHOUSE_ROOM_CHARACTER_TABLE:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_CHARACTER_TABLE;
                    break;
                case CLUBHOUSE_ROOM_BEST_TIMES:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_BEST_TIMES;
                    break;
            }
            PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            return;
        }
        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            if (D_801323E6 == isDone) {
                g_MenuScreen = MENU_SCREEN_MAIN;
            } else {
                g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
            }
            PlaySong(SONG_TITLE, 0);
            return;
        }
        if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuChoices.clubhouseChoice--;
        } else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuChoices.clubhouseChoice++;
        }
        if (g_MenuChoices.clubhouseChoice >= CLUBHOUSE_CHOICE_COUNT) {
            g_MenuChoices.clubhouseChoice = 0;
        }
        if (g_MenuChoices.clubhouseChoice < 0) {
            g_MenuChoices.clubhouseChoice = CLUBHOUSE_CHOICE_COUNT - 1;
        }
    }
}

void func_801FA1B8_ovl_01(void) {
    if (IsHudAnimationDone() == TRUE && (g_MenuPressedButtons & B_BUTTON)) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        g_MenuScreen = MENU_SCREEN_MAIN;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FA20C_ovl_01);

void func_801FADBC_ovl_01(void) {
    if (IsHudAnimationDone() != TRUE) {
        return;
    }
    if (GetWipeKind() != WIPE_NONE) {
        if (GetWipeProgress() == 0) {
            g_MenuScreen = MENU_SCREEN_EXIT;
        }
        return;
    }
    if (g_MenuPressedButtons & A_BUTTON) {
        PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartWipe(WIPE_CLOSING);
        return;
    }
    if (g_MenuPressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        if (D_801323A0[0xB] == MENU_SCREEN_UNK5) {
            g_MenuScreen = D_801323A0[0xB];
        } else {
            g_MenuScreen = MENU_SCREEN_UNK6;
        }
        return;
    }
    if ((g_MenuRepeatButtons & U_JPAD) && g_MenuChoices.unk1 > 0) {
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        g_MenuChoices.unk1--;
    } else if ((g_MenuRepeatButtons & D_JPAD) && g_MenuChoices.unk1 < CADDY_COUNT - 1) {
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        g_MenuChoices.unk1++;
    }
}

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

void func_801FAFEC_ovl_01(void) {
    GetSaveData();
    if (g_MenuPressedButtons & A_BUTTON) {
        PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        g_MenuScreen = MENU_SCREEN_EXIT;
        D_800FE334 = 0;
        D_801B60B9 = 2;
        return;
    }
    if (g_MenuPressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        if (D_801323E6) {
            g_MenuScreen = MENU_SCREEN_MAIN;
        } else {
            g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
        }
        return;
    }
    if (g_MenuRepeatButtons & U_JPAD) {
        D_8022BE18_ovl_01--;
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    } else if (g_MenuRepeatButtons & D_JPAD) {
        D_8022BE18_ovl_01++;
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    }
    if (D_8022BE18_ovl_01 < 0) {
        D_8022BE18_ovl_01 = UNUSED_LIST_CHOICE_COUNT - 1;
    }
    if (D_8022BE18_ovl_01 >= UNUSED_LIST_CHOICE_COUNT) {
        D_8022BE18_ovl_01 = 0;
    }
}

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

void DrawMenuScreen(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    g_ModuleRdpRequestFrames[MODULE_MENU] = g_FrameCount;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1));
    gDPFillRectangle(gfx++, SCREEN_BORDER, SCREEN_BORDER, SCREEN_WIDTH - SCREEN_BORDER - 1,
                     SCREEN_HEIGHT - SCREEN_BORDER - 1);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    BeginShadowedText(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    sMenuBobPhase += MENU_BOB_PHASE_STEP;
    sMenuBobOffset = sinf(sMenuBobPhase) * MENU_BOB_HEIGHT;
    switch (g_MenuScreen) {
        case MENU_SCREEN_TITLE:
            SetTextSlot(0, 0, NO_MESSAGE, 1, 0, 1, 255, 255, 255, 255);
            func_801FDA40_ovl_01(&gfx);
            break;
        case 1:
        case 24:
            func_8020B590_ovl_01(&gfx, 0);
            break;
        case 16:
            func_8020B590_ovl_01(&gfx, 1);
            break;
        case 3:
            func_802230C0_ovl_01(&gfx);
            break;
        case MENU_SCREEN_MAIN:
            func_80205038_ovl_01(&gfx);
            break;
        case MENU_SCREEN_MAIN_WITHOUT_TRANSMIT:
            func_802058B8_ovl_01(&gfx);
            break;
        case MENU_SCREEN_CLUB_TOURNAMENT:
            func_80229CB0_ovl_01(&gfx);
            break;
        case MENU_SCREEN_CLUBHOUSE:
            func_80206180_ovl_01(&gfx);
            break;
        case MENU_SCREEN_OPTIONS:
            func_80206A68_ovl_01(&gfx);
            break;
        case MENU_SCREEN_HELP:
            if (IsGameFlagSet(GAME_FLAG_UNK56)) {
                func_80201574_ovl_01(&gfx);
            } else {
                DrawHelpBoard(&gfx);
            }
            break;
        case 8:
            func_80207D08_ovl_01(&gfx);
            break;
        case 6:
        case 20:
        case 22:
            func_80202000_ovl_01(&gfx);
            break;
        case 18:
            func_80207E18_ovl_01(&gfx);
            break;
        case MENU_SCREEN_UNK12:
            func_8021D3AC_ovl_01(&gfx);
            break;
        case MENU_SCREEN_UNK13:
            func_8021FC5C_ovl_01(&gfx);
            break;
        case MENU_SCREEN_UNK11:
            func_8020E898_ovl_01(&gfx);
            break;
        case MENU_SCREEN_BEST_TIMES:
            func_80213748_ovl_01(&gfx);
            break;
        case MENU_SCREEN_BIRDIE_BADGES:
            DrawBirdieBadgeLabel(&gfx);
            break;
        case MENU_SCREEN_CHARACTER_TABLE:
            func_80215DC4_ovl_01(&gfx);
            break;
        case 5:
        case MENU_SCREEN_UNK23:
        case 26:
            func_801FE4D0_ovl_01(&gfx);
            break;
    }
    BeginShadowedText(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FB54C_ovl_01);

void func_801FC60C_ovl_01(Gfx **displayListHead, s32 layers) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    if (layers & MENU_LAYER_UNK1) {
        func_801FC754_ovl_01(&gfx);
    }
    if (layers & MENU_LAYER_UNK4) {
        func_801FCB74_ovl_01(&gfx);
    }
    if (layers & MENU_LAYER_UNK10) {
        func_801FD4F0_ovl_01(&gfx);
    }
    if (layers & MENU_LAYER_UNK8) {
        func_801FD7B0_ovl_01(&gfx);
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void func_801FC754_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 shownRowCount;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_801F4A20 + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != BACKDROP_STRIP_COUNT; stripIndex++) {
        /* The image ends in a strip of one row */
        shownRowCount = (stripIndex != BACKDROP_STRIP_COUNT - 1) ? BACKDROP_STRIP_ROW_COUNT : 1;
        if (stripIndex == 0) {
            /* The first strip has no row above it, so it shows its first row too */
            gDPLoadTextureBlock(gfx++, ((u32)D_801F4A20 + LARGE_PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
                                G_IM_SIZ_8b, BACKDROP_WIDTH, BACKDROP_STRIP_ROW_COUNT + BACKDROP_SEAM_ROW_COUNT, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(gfx++, BACKDROP_LEFT << 2, BACKDROP_FIRST_TOP << 2,
                                    (BACKDROP_LEFT + BACKDROP_WIDTH * BACKDROP_SCALE) << 2,
                                    (BACKDROP_FIRST_TOP + (BACKDROP_STRIP_ROW_COUNT + 1) * BACKDROP_SCALE) << 2,
                                    G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE / BACKDROP_SCALE,
                                    TEXEL_STEP_ONE / BACKDROP_SCALE);
        } else {
            gDPPipeSync(gfx++);
            gDPLoadTextureBlock(gfx++,
                                ((u32)D_801F4A20 + LARGE_PALETTED_TEXELS_OFFSET +
                                 stripIndex * BACKDROP_WIDTH * BACKDROP_STRIP_ROW_COUNT) & ~7,
                                G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_WIDTH, shownRowCount + BACKDROP_SEAM_ROW_COUNT, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gDPPipeSync(gfx++);
            stripTop = BACKDROP_TOP + stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE;
            gSPScisTextureRectangle(gfx++, BACKDROP_LEFT << 2, stripTop << 2,
                                    (BACKDROP_LEFT + BACKDROP_WIDTH * BACKDROP_SCALE) << 2,
                                    (BACKDROP_TOP + stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE +
                                     shownRowCount * BACKDROP_SCALE)
                                        << 2,
                                    G_TX_RENDERTILE, 0, BACKDROP_SECOND_ROW_START, TEXEL_STEP_ONE / BACKDROP_SCALE,
                                    TEXEL_STEP_ONE / BACKDROP_SCALE);
        }
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void func_801FCB74_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_800FF420[D_8022BE92_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != NARROW_BOARD_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_800FF420[D_8022BE92_ovl_01] + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * NARROW_BOARD_WIDTH * NARROW_BOARD_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, NARROW_BOARD_WIDTH, NARROW_BOARD_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = NARROW_BOARD_TOP + stripIndex * NARROW_BOARD_STRIP_SPACING;
        gSPScisTextureRectangle(gfx++, NARROW_BOARD_LEFT << 2, stripTop << 2,
                                (NARROW_BOARD_LEFT + NARROW_BOARD_SHOWN_WIDTH) << 2,
                                (stripTop + NARROW_BOARD_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                (NARROW_BOARD_WIDTH << 10) / NARROW_BOARD_SHOWN_WIDTH,
                                (NARROW_BOARD_STRIP_HEIGHT << 10) / NARROW_BOARD_ROW_SCALE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

/* Nothing in the menu calls this; menu.c loads its image, asset 0x521 */
void func_801FCEF8_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = UNUSED_BANNER_TOP + stripIndex * MENU_BANNER_STRIP_HEIGHT;
        gSPScisTextureRectangle(gfx++, MENU_BANNER_LEFT << 2, stripTop << 2,
                                (MENU_BANNER_LEFT + MENU_BANNER_WIDTH) << 2,
                                (stripTop + MENU_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

/* Doesn't match: the original tests the s32 top << 2 for the texture's row offset, not the s16 top */
#ifdef NON_MATCHING
void func_801FD1B4_ovl_01(Gfx **displayListHead, s16 left, s16 top) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, left << 2, top << 2, (left + SMALL_BANNER_WIDTH) << 2,
                                (top + MENU_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                (MENU_BANNER_WIDTH << 10) / SMALL_BANNER_WIDTH, TEXEL_STEP_ONE);
        top += MENU_BANNER_STRIP_HEIGHT;
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FD1B4_ovl_01);
#endif

/* Nothing in the menu calls this either */
void func_801FD4F0_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = LARGE_BANNER_TOP + stripIndex * LARGE_BANNER_STRIP_SPACING;
        gSPScisTextureRectangle(gfx++, LARGE_BANNER_LEFT << 2, stripTop << 2,
                                (LARGE_BANNER_LEFT + LARGE_BANNER_WIDTH) << 2,
                                (stripTop + LARGE_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                (MENU_BANNER_WIDTH << 10) / LARGE_BANNER_WIDTH,
                                (MENU_BANNER_STRIP_HEIGHT << 10) / LARGE_BANNER_STRIP_HEIGHT);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

void func_801FD7B0_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    u8 *image = D_801052BC[D_8022BE8C_ovl_01];
    s32 stripTop;

    gDPLoadTLUT_pal256(gfx++, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)image + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = MENU_BANNER_TOP + stripIndex * MENU_BANNER_STRIP_HEIGHT;
        gSPScisTextureRectangle(gfx++, MENU_BANNER_LEFT << 2, stripTop << 2,
                                (MENU_BANNER_LEFT + MENU_BANNER_WIDTH) << 2,
                                (stripTop + MENU_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FDA40_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FE4D0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_802004A0_ovl_01);

void func_80200A40_ovl_01(Gfx **displayListHead, s32 left, s32 top, s32 halfSize, s32 scale) {
    Gfx *gfx = *displayListHead;
    EncodedAsset asset;
    s32 stripIndex;
    void *image;
    u8 iconIndex;

    gDPPipeSync(gfx++);
    image = D_8010526C[D_8022F8C4_ovl_01];
    gDPLoadTLUT_pal256(gfx++, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != MODE_ICON_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)image + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, MODE_ICON_WIDTH, MODE_ICON_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, left << 2, (top + stripIndex * halfSize) << 2, (left + halfSize + halfSize) << 2,
                                (top + halfSize + stripIndex * halfSize) << 2, G_TX_RENDERTILE, 0, 0, scale, scale);
        gDPPipeSync(gfx++);
    }
    D_8022F8C5_ovl_01++;
    if (D_8022F8C5_ovl_01 == MODE_ICON_FRAME_DURATION) {
        iconIndex = D_8022BEBC_ovl_01[D_80105B68 - 1][((MenuMemory *)D_801323A0)->modeChoice];
        D_8022F8C5_ovl_01 = 0;
        D_8022F8C0_ovl_01++;
        if (D_8022F8C0_ovl_01 >= MODE_ICON_FRAME_COUNT) {
            D_8022F8C0_ovl_01 = 0;
        }
        if (g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01] == END_OF_ICON_FRAMES) {
            D_8022F8C0_ovl_01 = 0;
        }
        D_8022F8C4_ovl_01 = D_8022F8C4_ovl_01 == 0;
        OpenEncodedAsset(g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01], &asset);
        ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
    }
    *displayListHead = gfx;
}

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

void func_80207D08_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 caddyIndex;

    func_801FB54C_ovl_01(&gfx, 61);
    BeginShadowedText(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    DrawShadowedText(&gfx, CADDY_SELECT_TITLE_X, CADDY_SELECT_TITLE_Y, D_8023176C_ovl_01);
    for (caddyIndex = 0; caddyIndex != CADDY_COUNT; caddyIndex++) {
        if (g_MenuChoices.unk1 == caddyIndex) {
            SetTextColor(&gfx, 255, 255, 0);
        } else {
            SetTextColor(&gfx, 255, 255, 255);
        }
        DrawShadowedText(&gfx, CADDY_SELECT_NAME_X, caddyIndex * CADDY_SELECT_LINE_HEIGHT + CADDY_SELECT_FIRST_Y,
                         D_8022C0D0_ovl_01[caddyIndex]);
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Defined after the function, as the INCLUDE_ASM before it leaves the assembler in .text */
const char D_8023176C_ovl_01[] = "CADDY SELECT";

void func_80207E18_ovl_01(Gfx **displayListHead) {
}

void func_80207E20_ovl_01(void) {
}

void func_80207E28_ovl_01(Gfx **displayListHead, void *image, s32 x, s32 y) {
    Gfx *gfx = *displayListHead;
    s32 unused[8];

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(gfx++);
    gDPLoadTextureTile_4b(gfx++,
                          ((u32)image + PALETTED_TEXELS_OFFSET + D_8022F8C8_ovl_01 * ANIMATED_ICON_IMAGE_SIZE) & ~7,
                          G_IM_FMT_CI, ANIMATED_ICON_SIZE, ANIMATED_ICON_SIZE, 0, 0, ANIMATED_ICON_SIZE - 1,
                          ANIMATED_ICON_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, x << 2, y << 2, (x + ANIMATED_ICON_SIZE) << 2, (y + ANIMATED_ICON_SIZE) << 2,
                            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
    D_8022F8CD_ovl_01++;
    if (D_8022F8CD_ovl_01 == ANIMATED_ICON_FRAME_DURATION) {
        D_8022F8CD_ovl_01 = 0;
        D_8022F8C8_ovl_01++;
        if (D_8022F8C8_ovl_01 >= ANIMATED_ICON_FRAME_COUNT) {
            D_8022F8C8_ovl_01 = 0;
        }
    }
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_802081E8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_8020879C_ovl_01);

void func_80208F8C_ovl_01(Gfx **displayListHead, s32 left, s32 top) {
    Gfx *gfx = *displayListHead;
    s16 stripIndex;
    s32 rowCount;
    s32 unused[8];

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_800FBE30[D_8022F8CE_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != TOAD_STRIP_COUNT; stripIndex++) {
        rowCount = TOAD_LAST_STRIP_HEIGHT;
        if (stripIndex == 0) {
            rowCount = TOAD_STRIP_HEIGHT;
        }
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_800FBE30[D_8022F8CE_ovl_01] + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * TOAD_WIDTH * TOAD_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, TOAD_WIDTH, rowCount, 0, G_TX_NOMIRROR | G_TX_WRAP,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, left << 2, (top + stripIndex * TOAD_STRIP_HEIGHT) << 2, (left + TOAD_WIDTH) << 2,
                                (top + stripIndex * TOAD_STRIP_HEIGHT + rowCount) << 2, G_TX_RENDERTILE, 0, 0,
                                TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

void DrawBirdieBadgeLabel(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    DrawShadowedText(&gfx, 64, 32, g_BirdieBadgeLabelText);
    *displayListHead = gfx;
}

s32 GetOpenCourseCount(void) {
    s32 coursePoints = GetSaveData()->coursePoints;

    if (IsGameFlagSet(GAME_FLAG_OPEN_SIX_COURSES)) {
        return 6;
    }
    if (IsGameFlagSet(GAME_FLAG_OPEN_FIVE_COURSES)) {
        return 5;
    }
    if (IsGameFlagSet(GAME_FLAG_OPEN_FOUR_COURSES)) {
        return 4;
    }
    if (IsGameFlagSet(GAME_FLAG_OPEN_THREE_COURSES)) {
        return 3;
    }
    if (IsGameFlagSet(GAME_FLAG_OPEN_TWO_COURSES)) {
        return 2;
    }
    return CountOpenCourses(coursePoints);
}
