#include "common.h"
#include "club.h"
#include "current_shot.h"
#include "game_flags.h"
#include "libc.h"
#include "main.h"
#include "save.h"

extern s32 D_800C306C;
extern s32 D_800C3070;
extern s32 D_800C3074;
extern s16 D_800C3078;
extern s16 D_800C307A;
extern s32 D_800C307C;
extern s32 D_800C3080;
extern s32 D_800C3084;
extern s32 D_800C3088;
extern s32 D_800C308C;
extern s32 D_800C3090;
extern s8 D_800C30A4;
extern s32 D_800C30A8;
extern s32 D_800C30AC;
extern u8 D_80104EB8[0x80];
extern s8 g_IsMiniGolf;

s32 GetGroundOrWaterHeight(s32 x, s32 z);
s32 SaveOrContinueGameByMode(s32 isContinuing);
void func_80216B74(s32);
void func_80407D64(void);
void func_800624F0(s32 range, s32 base);

s32 func_8005F180(void) {
    return IsGameFlagSet(0x32) || IsGameFlagSet(0x9B);
}

void func_8005F1C8(void) {
    D_800C306C = 0;
    D_800C3070 = 0;
    D_800C3074 = 0;
    D_800C3078 = 0;
    D_800C3084 = 0;
    D_800C3088 = 0;
    D_800C308C = 0;
    D_800C3090 = 0;
    D_800C307A = 0;
    D_800C307C = 0;
    D_800C3080 = 0;
}

s32 func_8005F224(void) {
    s32 value = g_CurrentShot.unk40;

    if (value >= 31) {
        value = 60 - value;
    }
    return value;
}

s32 func_8005F248(s32 value) {
    if (value >= 31) {
        value = 60 - value;
    }
    if (value < 0) {
        value = 0;
    }
    return (value << 8) / 30;
}

/*
 * Sets each of a club set's flags in isUsable to whether the current
 * surface allows the club, and returns the set's size. The first argument
 * is not read.
 */
s32 MarkUsableClubs(s32 unused, s16 *clubs, s8 *isUsable) {
    s32 i;

    for (i = 0; i < CLUB_SET_SIZE; i++) {
        isUsable[i] = (GetClub(clubs[i])->surfaceMask & GetCurrentSurfaceClubMask(TRUE)) != 0;
    }
    return CLUB_SET_SIZE;
}

s32 func_8005F30C(s32 x, s32 z) {
    s32 height = GetGroundOrWaterHeight(x, z);
    s32 ballHeight;

    if (!g_IsMiniGolf & (height > 0)) {
        ballHeight = g_CurrentShot.ballPosition[AXIS_Y];
        if (ballHeight <= 0) {
            ballHeight = 0;
        }
        height = ballHeight;
    }
    return height;
}

void func_8005F360(void) {
    switch (IsGameFlagSet(0x33) * 2 + IsGameFlagSet(0x34)) {
        case 0:
            g_GameState.courseIndex = 0;
            g_GameState.holeIndex = 0;
            g_GameState.unk14 = 0;
            g_GameState.playerCount = IsGameFlagSet(0x30) * 2 + IsGameFlagSet(0x31) + 1;
            g_GameState.unk0 = 0;
            g_GameState.gameMode = 0;
            break;
        case 1:
            g_GameState.courseIndex = 0;
            g_GameState.holeIndex = 0;
            g_GameState.unk14 = 0;
            g_GameState.playerCount = 1;
            g_GameState.unk0 = 0;
            g_GameState.gameMode = 3;
            break;
        case 2:
            g_GameState.courseIndex = 0;
            g_GameState.holeIndex = 0;
            g_GameState.unk14 = 0;
            g_GameState.playerCount = 2;
            g_GameState.unk0 = 0;
            g_GameState.gameMode = 2;
            /* The original has no break here, so the default case overrides this one */
        default:
            g_GameState.playerCount = 3;
            g_GameState.courseIndex = 0;
            g_GameState.holeIndex = 0;
            g_GameState.unk14 = 0;
            g_GameState.unk0 = 0;
            g_GameState.gameMode = 10;
            break;
    }
}

void func_8005F4AC(void) {
    g_GameState.courseIndex = 0;
    g_GameState.playerCount = 1;
    g_GameState.holeIndex = 0;
    g_GameState.unk14 = 0;
    g_GameState.unk0 = 11;
    if (IsGameFlagSet(0x35)) {
        g_GameState.gameMode = 9;
    } else {
        g_GameState.gameMode = 0;
    }
}

/* Writes the name a player of the given index has by default */
void FormatDefaultPlayerName(s32 index, char *name) {
    sprintf(name, "MARIO%d", index + 1);
}

INCLUDE_ASM("asm/nonmatchings/play", func_8005F540);

INCLUDE_ASM("asm/nonmatchings/play", InitPlay);

INCLUDE_ASM("asm/nonmatchings/play", func_8005F964);

INCLUDE_ASM("asm/nonmatchings/play", func_8005FB58);

INCLUDE_ASM("asm/nonmatchings/play", func_8005FCB8);

INCLUDE_ASM("asm/nonmatchings/play", func_8005FE8C);

INCLUDE_ASM("asm/nonmatchings/play", func_800600C0);

INCLUDE_ASM("asm/nonmatchings/play", func_80060128);

INCLUDE_ASM("asm/nonmatchings/play", func_80060190);

INCLUDE_ASM("asm/nonmatchings/play", func_80060210);

INCLUDE_ASM("asm/nonmatchings/play", func_800602B4);

void func_8006034C(s8 value) {
    D_800C30A4 = value;
}

void func_80060358(void) {
    D_800C306C = 0;
    SaveOrContinueGameByMode(FALSE);
    func_80216B74(0);
    g_GameState.unk0 = 7;
    D_800C30A8 = 3;
    D_800C30AC = 9;
}

s32 func_800603A8(void) {
    return (IsGameFlagSet(0x82) << 4) | (IsGameFlagSet(0x83) << 3) | (IsGameFlagSet(0x84) << 2) |
           (IsGameFlagSet(0x85) << 1) | IsGameFlagSet(0x86);
}

INCLUDE_ASM("asm/nonmatchings/play", func_80060434);

INCLUDE_ASM("asm/nonmatchings/play", func_800604F4);

INCLUDE_ASM("asm/nonmatchings/play", func_800605EC);

INCLUDE_ASM("asm/nonmatchings/play", func_800624F0);

void InitExpansionPakFeatures(void) {
    if (g_HasExpansionPak) {
        func_80407D64();
    }
    func_800624F0(0x60, 0xBE);
}

INCLUDE_ASM("asm/nonmatchings/play", func_800625FC);

INCLUDE_ASM("asm/nonmatchings/play", func_800627F8);

INCLUDE_ASM("asm/nonmatchings/play", func_8006280C);

void func_800628A0(void) {
    bzero(D_80104EB8, sizeof(D_80104EB8));
}

INCLUDE_ASM("asm/nonmatchings/play", func_800628C4);

INCLUDE_ASM("asm/nonmatchings/play", func_80062A24);

INCLUDE_ASM("asm/nonmatchings/play", func_80062A8C);

