#include "common.h"
#include "character.h"
#include "club.h"
#include "course.h"
#include "current_shot.h"
#include "game_flags.h"
#include "libc.h"
#include "main.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "tournament.h"

/* D_80104EB8 divides the main courses into cells of 1 << CELL_SIZE_SHIFT units */
#define CELL_SIZE_SHIFT 19
#define CELL_COLUMN_COUNT 8
#define CELL_ROW_COUNT 16

/* D_800E12B0 is a square of random bytes this many on a side */
#define RANDOM_GRID_SIZE 32

typedef struct PlayUnk8 {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ s8 unk2;
    /* 0x3 */ s8 unk3;
    /* 0x4 */ s8 unk4;
    /* 0x5 */ s8 unk5;
    /* 0x6 */ s8 unk6;
    /* 0x7 */ s8 unk7;
} PlayUnk8; /* size = 0x8 */

typedef struct PlayRecord {
    /* 0x00 */ Unk800BA9D8 hole;
    /* 0x34 */ s32 gameMode;
    /* 0x38 */ s8 courseIndex;
    /* 0x39 */ s8 holeIndex;
    /* 0x3A */ u8 unk3A[0x3E - 0x3A];
    /* 0x3E */ u8 unk3E;
    /* 0x3F */ u8 unk3F[0x44 - 0x3F];
    /* 0x44 */ PlayUnk8 unk44[4];
    /* 0x64 */ u8 unk64[0x68 - 0x64];
} PlayRecord; /* size = 0x68 */

typedef struct RingShotRecord {
    /* 0x00 */ Unk800BA9D8 hole;
    /* 0x34 */ s8 courseIndex;
    /* 0x35 */ s8 holeIndex;
    /* 0x36 */ u8 unk36[0x3A - 0x36];
    /* 0x3A */ u8 unk3A;
    /* 0x3B */ u8 unk3B[0x40 - 0x3B];
    /* 0x40 */ PlayUnk8 unk40[4];
    /* 0x60 */ u8 unk60[0x68 - 0x60];
} RingShotRecord; /* size = 0x68 */

extern PlayRecord D_800FEDD8[];
extern RingShotRecord D_80105F40[];
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
extern u8 D_80104EB8[CELL_ROW_COUNT * CELL_COLUMN_COUNT];
extern u8 D_800E12B0[RANDOM_GRID_SIZE * RANDOM_GRID_SIZE];
extern s16 D_80104F78;
extern s32 D_80104F7C;
extern s32 D_80104F80;
extern s32 D_80104F84;
extern s32 D_80104F88;
extern s32 D_80104F8C;
extern s32 D_80104F90;
extern u8 D_800FE330;
extern s32 D_801B60B4;
extern GameState D_801B55F0;
extern UnkB8 D_8012D0C8[MAX_PLAYER_COUNT];
extern s8 g_IsMiniGolf;

s32 GetGroundOrWaterHeight(s32 x, s32 z);
s32 SaveOrContinueGameByMode(s32 isContinuing);
void func_80216B74(s32);
void func_80407D64(void);
void FillRandomGrid(s32 range, s32 base);
void func_8005F540(void);

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

void func_800600C0(s32 index) {

    D_80104F78 = D_800FEDD8->unk44[index].unk0;
    D_80104F7C = D_800FEDD8->unk44[index].unk2;
    D_80104F80 = D_800FEDD8->unk44[index].unk3;
    D_80104F84 = D_800FEDD8->unk44[index].unk4;
    D_80104F88 = D_800FEDD8->unk44[index].unk5;
    D_80104F8C = D_800FEDD8->unk44[index].unk6;
    D_80104F90 = D_800FEDD8->unk44[index].unk7;
}

void func_80060128(s32 index) {
    D_80104F78 = D_80105F40->unk40[index].unk0;
    D_80104F7C = D_80105F40->unk40[index].unk2;
    D_80104F80 = D_80105F40->unk40[index].unk3;
    D_80104F84 = D_80105F40->unk40[index].unk4;
    D_80104F88 = D_80105F40->unk40[index].unk5;
    D_80104F8C = D_80105F40->unk40[index].unk6;
    D_80104F90 = D_80105F40->unk40[index].unk7;
}

s32 func_80060190(s32 strokes) {
    s32 toPar = strokes - g_HolePar;

    if (strokes == 1) {
        return 0x80;
    }
    switch (toPar) {
        case -3:
            return 0x81;
        case -2:
            return 0x82;
        case -1:
            return 0x83;
        case 0:
            if (g_GameMode == GAME_MODE_RING_SHOT) {
                return 0x84;
            }
            break;
    }
    return 0;
}

/* Sets play up to replay a super shot record */
void LoadSuperShotReplay(PlayRecord *record) {
    D_800FEDD8[0] = *record;
    g_GameMode = record->gameMode;
    g_CourseIndex = record->courseIndex;
    g_HoleIndex = record->holeIndex;
    D_800FE330 = record->unk3E;
    func_8005F540();
    osSyncPrintf("SetSuperShotReplay\n");
}

/* Sets play up for the Ring Shot hole of a record */
void LoadRingShotRecord(RingShotRecord *record) {
    D_80105F40[0] = *record;
    g_GameMode = GAME_MODE_RING_SHOT;
    g_CourseIndex = record->courseIndex;
    g_HoleIndex = record->holeIndex;
    D_800FE330 = record->unk3A;
    func_8005F540();
}

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

/* Copies the game state and players to D_801B55F0 and D_8012D0C8 */
void BackUpPlayState(void) {
    s32 i;

    D_801B55F0 = g_GameState;
    for (i = 0; i < MAX_PLAYER_COUNT; i++) {
        D_8012D0C8[i] = g_Players[i];
    }
}

/*
 * Whether the player has beaten Bowser in a Get Character match, or has
 * won first place in a Boo Valley or Mario's Star tournament
 */
s32 HasWonFinalChallenge(void) {
    s32 firstHole;
    TournamentStanding *standing;
    s32 i;

    if (g_GameMode == GAME_MODE_GET_CHARACTER) {
        if (GetMatchWinner(0, 1) == MATCH_SIDE_PLAYER) {
            if ((s8)g_Players[1].characterIndex == CHARACTER_BOWSER) {
                return TRUE;
            }
        }
    } else if (g_GameMode == GAME_MODE_TOURNAMENT) {
        if (g_CourseIndex == COURSE_RECORD_BOO_VALLEY || g_CourseIndex == COURSE_RECORD_MARIOS_STAR) {
            firstHole = GetFirstHoleIndex();
            RankTournament(COURSE_HOLE_COUNT, 0, GetNetScoreToPar(firstHole + GetRoundHoleCount(), 0), D_801B60B4);
            for (i = 0; i != TOURNAMENT_CONTESTANT_COUNT; i++) {
                standing = &g_TournamentStandings[i];
                if (standing->contestantId == PLAYER_CONTESTANT && (standing->place & PLACE_MASK) == 1) {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

INCLUDE_ASM("asm/nonmatchings/play", func_800605EC);

/* Fills D_800E12B0 with random values from base up, capped at 255 */
void FillRandomGrid(s32 range, s32 base) {
    s32 cell = 0;
    s32 row;
    s32 column;
    s32 value;

    for (row = 0; row < RANDOM_GRID_SIZE; row++) {
        for (column = 0; column < RANDOM_GRID_SIZE; column++) {
            value = rand() % range + base;
            if (value >= 256) {
                value = 255;
            }
            D_800E12B0[cell++] = value;
        }
    }
}

void InitExpansionPakFeatures(void) {
    if (g_HasExpansionPak) {
        func_80407D64();
    }
    FillRandomGrid(0x60, 0xBE);
}

INCLUDE_ASM("asm/nonmatchings/play", func_800625FC);

void CopyDoublePair(f64 *from, f64 *to) {
    to[0] = from[0];
    to[1] = from[1];
}

/*
 * Unless on a mini-golf course, marks the cell holding a point and the
 * cells around it in D_80104EB8
 */
void MarkNearbyCells(u32 x, u32 z) {
    u32 column;
    u32 row;
    s32 i;
    s32 j;

    if (!g_IsMiniGolf) {
        column = x >> CELL_SIZE_SHIFT;
        row = z >> CELL_SIZE_SHIFT;
        if (column < CELL_COLUMN_COUNT) {
            if (row < CELL_ROW_COUNT) {
                for (i = -1; i < 2; i++) {
                    for (j = -1; j < 2; j++) {
                        if (column + j < CELL_COLUMN_COUNT && row + i < CELL_ROW_COUNT) {
                            D_80104EB8[(row + i) * CELL_COLUMN_COUNT + (column + j)] = TRUE;
                        }
                    }
                }
            }
        }
    }
}

void func_800628A0(void) {
    bzero(D_80104EB8, sizeof(D_80104EB8));
}

INCLUDE_ASM("asm/nonmatchings/play", func_800628C4);

INCLUDE_ASM("asm/nonmatchings/play", func_80062A24);

INCLUDE_ASM("asm/nonmatchings/play", func_80062A8C);

