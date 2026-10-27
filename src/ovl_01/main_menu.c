#include "common.h"
#include "billboard.h"
#include "character_slot.h"
#include "course.h"
#include "game_flags.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "libc.h"
#include "menu.h"
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

extern s8 D_800C1FF4[MAX_PLAYER_COUNT];
extern s32 D_801051F0;
extern u8 D_8022F8E0_ovl_01[MAX_PLAYER_COUNT + 1][MAX_PLAYER_COUNT];
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
extern s32 D_8022BE18_ovl_01;
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
void func_8020B590_ovl_01(Gfx **displayListHead, s32);
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
        case 17:
            if (IsGameFlagSet(0x56)) {
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
        case 12:
            func_8021D3AC_ovl_01(&gfx);
            break;
        case MENU_SCREEN_UNK13:
            func_8021FC5C_ovl_01(&gfx);
            break;
        case 11:
            func_8020E898_ovl_01(&gfx);
            break;
        case 7:
            func_80213748_ovl_01(&gfx);
            break;
        case 14:
            DrawBirdieBadgeLabel(&gfx);
            break;
        case 15:
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

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80207E28_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_802081E8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_8020879C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80208F8C_ovl_01);

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
