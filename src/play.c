#include "common.h"
#include "character.h"
#include "club.h"
#include "course.h"
#include "current_shot.h"
#include "game_flags.h"
#include "hole_triangles.h"
#include "libc.h"
#include "main.h"
#include "math_util.h"
#include "movie.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "surface.h"
#include "terrain.h"
#include "tournament.h"

/* D_800E12B0 is a square of random bytes this many on a side */
#define RANDOM_GRID_SIZE 32

/* UnkB8.unkC values; PlayContestMovies waits for every player to have one */
#define PLAYER_STATE_UNK11 0x11
#define PLAYER_STATE_UNK14 0x14

/* A fog color and the range over which the fog thickens */
/* How far above the tee a ball starts on the main courses */
#define TEE_HEIGHT 307.2f

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
    /* 0x3A */ u8 unk3A;
    /* 0x3B */ u8 characterIndex;
    /* 0x3C */ u8 unk3C;
    /* 0x3D */ u8 unk3D;
    /* 0x3E */ u8 unk3E;
    /* 0x3F */ u8 unk3F;
    /* 0x40 */ s32 unk40;
    /* 0x44 */ PlayUnk8 unk44[4];
    /* 0x64 */ u8 unk64[0x68 - 0x64];
} PlayRecord; /* size = 0x68 */

typedef struct RingShotRecord {
    /* 0x00 */ Unk800BA9D8 hole;
    /* 0x34 */ s8 courseIndex;
    /* 0x35 */ s8 holeIndex;
    /* 0x36 */ u8 unk36;
    /* 0x37 */ u8 characterIndex;
    /* 0x38 */ u8 unk38;
    /* 0x39 */ u8 unk39;
    /* 0x3A */ u8 unk3A;
    /* 0x3B */ u8 unk3B;
    /* 0x3C */ s32 unk3C;
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
extern u8 D_80104EB8[GROUND_TILE_COUNT];
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
extern s8 D_800FBDB8;
extern s32 D_800C3094;
extern s8 D_800C3098;
extern PlayRecord D_800FED40[];
extern s8 D_800FEE10;
extern s8 D_800FEE11;
extern GameState D_801B55F0;
extern UnkB8 D_8012D0C8[MAX_PLAYER_COUNT];

s32 SaveOrContinueGameByMode(s32 isContinuing);
void func_80216B74(s32);
void func_80407D64(void);
void FillRandomGrid(s32 range, s32 base);
void SetUpPlayers(void);
void WriteBallDistancesIfEnabled(void);
void InitExpansionPakFeatures(void);
void LoadSuperShotReplay(PlayRecord *record);
s32 IsMovieFinished(void);

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

    if (!g_GameState.isMiniGolf & (height > 0)) {
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

/*
 * Sets up the players for a round. In SCENE_PLAY_UNK13, in
 * GAME_MODE_UNK8 or when g_GameState.unk31 is set, only the first player
 * is set up, from the Ring Shot record or the super shot replay record;
 * otherwise all four get default names, their own character's clubs and
 * cleared scores.
 */
void SetUpPlayers(void) {
    s32 i;
    s32 hole;
    s32 slot;

    if (g_Scene == SCENE_PLAY_UNK13 || g_GameMode == GAME_MODE_UNK8 || g_GameState.unk31) {
        if (g_GameMode == GAME_MODE_RING_SHOT) {
            g_Players[0].unk1E[0] = 0;
            g_Players[0].teeOrder = 0;
            g_Players[0].characterIndex = D_80105F40->characterIndex;
            g_Players[0].unk22 = D_80105F40->unk38;
            g_Players[0].unk7B = D_80105F40->unk39;
            g_Players[0].unk88 = D_80105F40->unk3C;
        } else {
            g_Players[0].unk1E[0] = 0;
            g_Players[0].teeOrder = 0;
            g_Players[0].characterIndex = D_800FEDD8->characterIndex;
            g_Players[0].unk22 = D_800FEDD8->unk3C;
            g_Players[0].unk7B = D_800FEDD8->unk3D;
            g_Players[0].unk88 = D_800FEDD8->unk40;
        }
        for (hole = 0; hole < COURSE_HOLE_COUNT; hole++) {
            g_Players[0].holes[hole].strokes = 0;
            g_Players[0].holes[hole].putts = UNCOUNTED_PUTTS;
        }
        g_Players[0].unk28 = -1;
        LoadPlayerFace(0, (s8)g_Players[0].characterIndex, (s8)g_Players[0].unk7B);
        for (slot = 0; slot < CLUB_SLOT_COUNT; slot++) {
            if (slot >= CLUB_SET_SIZE) {
                g_Players[0].clubs[slot] = NO_CLUB;
            } else {
                g_Players[0].clubs[slot] = slot + (s8)g_Players[0].characterIndex * CLUB_SET_SIZE;
            }
        }
        /* The index variable is there to match */
        i = 0;
        for (slot = PLAYER_UNKAA_COUNT - 1; slot >= 0; slot--) {
            g_Players[i].unkAA[slot] = 0;
        }
    } else {
        for (i = 0; i < MAX_PLAYER_COUNT; i++) {
            FormatDefaultPlayerName(i, g_Players[i].name);
            g_Players[i].characterIndex = i;
            g_Players[i].unk22 = 0;
            g_Players[i].unk1E[0] = 0;
            g_Players[i].teeOrder = 0;
            g_Players[i].unk8C = GetSaveData()->options[SAVE_OPTION_UNK2];
            for (hole = 0; hole < COURSE_HOLE_COUNT; hole++) {
                g_Players[i].holes[hole].strokes = 0;
                g_Players[i].holes[hole].putts = UNCOUNTED_PUTTS;
            }
            for (slot = PLAYER_UNKAA_COUNT - 1; slot >= 0; slot--) {
                g_Players[i].unkAA[slot] = 0;
            }
            for (slot = 0; slot < CLUB_SLOT_COUNT; slot++) {
                if (slot >= CLUB_SET_SIZE) {
                    g_Players[i].clubs[slot] = NO_CLUB;
                } else {
                    g_Players[i].clubs[slot] = slot + (s8)g_Players[i].characterIndex * CLUB_SET_SIZE;
                }
            }
            g_Players[i].unk74 = 0;
            g_Players[i].unk88 = NextRandom();
        }
    }
}

void InitPlay(s32 kind) {
    if (kind == 1) {
        func_8005F360();
    } else if (kind == 0) {
        func_8005F4AC();
    } else if (kind == 2) {
        LoadSuperShotReplay(D_800FED40);
        g_GameState.unk14 = 0;
        g_GameState.playerCount = 1;
        g_GameState.unk0 = 0;
        g_GameState.gameMode = GAME_MODE_STROKE;
        g_GameState.courseIndex = D_800FEE10;
        g_GameState.holeIndex = D_800FEE11;
    }
    RenumberHoleHandicaps();
    if (!g_GameState.unk30) {
        SetUpPlayers();
    }
    if (IsGameFlagSet(0x33) * 2 + IsGameFlagSet(0x34) == 2) {
        g_Players[1].unk1E[0] = 1;
    }
    InitExpansionPakFeatures();
    if (kind != 3) {
        PrepareHoleDisplay();
    }
    WriteBallDistancesIfEnabled();
    osSyncPrintf("PlayInitComplete\n");
}

/*
 * Unless g_GameState.unk30 is 2, starts each player's ball on the tee,
 * with the hole's per-player state cleared
 */
void PlacePlayersOnTee(void) {
    s32 i;
    s32 ring;
    f32 teeHeight;

    D_800C3094 = 0;
    D_800C3098 = 0;
    if (g_GameState.unk30 != 2) {
        for (i = 0; i < MAX_PLAYER_COUNT; i++) {
            g_Players[i].unkC = PLAYER_STATE_UNK11;
            g_Players[i].ballPosition[AXIS_X] = D_800BA9D8.teePosition[AXIS_X];
            teeHeight = D_800BA9D8.teePosition[AXIS_Y];
            g_Players[i].ballPosition[AXIS_Y] = !g_GameState.isMiniGolf ? teeHeight - TEE_HEIGHT : teeHeight;
            g_Players[i].ballPosition[AXIS_Z] = D_800BA9D8.teePosition[AXIS_Z];
            g_Players[i].strokeCount = 0;
            g_Players[i].unk24 = 0;
            g_Players[i].unk26 = 0;
            g_Players[i].ballSurface = SURFACE_TEE_UP;
            if (g_GameState.isMiniGolf) {
                g_Players[i].ballSurface =
                    GetSurface(g_Players[i].ballPosition[AXIS_X], g_Players[i].ballPosition[AXIS_Z]);
            }
            g_Players[i].driveDistance = UNMEASURED_DISTANCE;
            g_Players[i].pinDistance = UNMEASURED_DISTANCE;
            g_Players[i].unk9C = 0;
            for (ring = RING_COUNT - 1; ring >= 0; ring--) {
                g_Players[i].ringStates[ring] = 0;
            }
            if (g_GameMode == GAME_MODE_RING_SHOT) {
                g_Players[i].unk28 = 1;
            }
        }
    }
    if (g_GameMode == GAME_MODE_RING_SHOT) {
        InitRings();
    }
}

/*
 * Once every player has finished the hole, plays the movies for the
 * contests won on it, then waits for them to finish
 */
void PlayContestMovies(void) {
    s32 i;

    if (D_800FBDB8 == 0) {
        for (i = 0; i < g_GameState.playerCount; i++) {
            if (g_Players[i].strokeCount == 0) {
                break;
            }
            if (g_Players[i].unkC != PLAYER_STATE_UNK11 && g_Players[i].unkC != PLAYER_STATE_UNK14) {
                break;
            }
        }
        if (!g_GameState.unk31 && i == g_GameState.playerCount) {
            if (IsMarkedParFive(g_CourseIndex, g_HoleIndex) && GetContestWinner() != NO_CONTEST_WINNER) {
                StartMovie(MOVIE_LONGEST_DRIVE);
            }
            if (IsParThree(g_CourseIndex, g_HoleIndex) && GetContestWinner() != NO_CONTEST_WINNER) {
                StartMovie(MOVIE_NEAREST_PIN);
            }
            if (g_GameMode == GAME_MODE_TOURNAMENT && RecordDrive()) {
                StartMovie(MOVIE_DRIVE_RECORD);
            }
            D_800FBDB8 = 60;
        }
    } else if (IsMovieFinished()) {
        D_800FBDB8 = -1;
    }
}

#ifdef NON_MATCHING
s32 func_8005FCB8(void) {
    s32 isDone = FALSE;
    s32 i;

    if (D_800FBDB8 == 60) {
        return FALSE;
    }
    if (!g_GameState.unk31) {
        if (g_GameMode == GAME_MODE_SKINS_MATCH || g_GameMode == GAME_MODE_CLUB_SLOTS) {
            if (!GetGroupHoleResult(NULL)) {
                return FALSE;
            }
            for (i = 0; i < g_GameState.playerCount; i++) {
                if (g_Players[i].unkC != PLAYER_STATE_UNK14 && g_Players[i].unkC != PLAYER_STATE_UNK11) {
                    break;
                }
            }
            return i == g_GameState.playerCount;
        }
        if (g_GameMode == GAME_MODE_MATCH_GAME || g_GameMode == GAME_MODE_GET_CHARACTER) {
            if (!GetMatchHoleResult(0, 1)) {
                return FALSE;
            }
            for (i = 0; i < g_GameState.playerCount; i++) {
                if (g_Players[i].unkC != PLAYER_STATE_UNK14 && g_Players[i].unkC != PLAYER_STATE_UNK11) {
                    break;
                }
            }
            return i == g_GameState.playerCount;
        }
    }
    for (i = 0; i < g_GameState.playerCount; i++) {
        if (g_Players[i].unkC != PLAYER_STATE_UNK14) {
            break;
        }
    }
    if (i == g_GameState.playerCount) {
        isDone = TRUE;
    }
    return isDone;
}

#else
INCLUDE_ASM("asm/nonmatchings/play", func_8005FCB8);
#endif

/*
 * Whether the first player tees off after the second on the next hole.
 * It is a macro, as an inline function compiles differently from the
 * original.
 */
#define TeesAfter(player, other)                                                   \
    (g_Players[player].strokeCount > g_Players[other].strokeCount                  \
     || (g_Players[player].strokeCount == g_Players[other].strokeCount             \
         && g_Players[player].teeOrder > g_Players[other].teeOrder))

/*
 * Sets each player's teeOrder for the next hole. In the group modes a
 * hole's winner goes first and the others move down one; otherwise the
 * players go in order of strokeCount, ties keeping their previous order.
 */
void SetNextTeeOrder(void) {
    s32 orders[MAX_PLAYER_COUNT];
    s8 results[MAX_PLAYER_COUNT];
    s32 i;
    s32 j;

    if (!g_GameState.unk31 && (g_GameMode == GAME_MODE_SKINS_MATCH || g_GameMode == GAME_MODE_CLUB_SLOTS)) {
        if (GetGroupHoleResult(results) != GROUP_HOLE_WON) {
            for (i = 0; i < g_GameState.playerCount; i++) {
                orders[i] = g_Players[i].teeOrder;
            }
        } else {
            for (i = 0; i < g_GameState.playerCount; i++) {
                if (results[i] != GROUP_PLAYER_WINNER) {
                    orders[i] = g_Players[i].teeOrder + 1;
                } else {
                    orders[i] = 0;
                }
            }
        }
    } else {
        for (i = 0; i < g_GameState.playerCount; i++) {
            orders[i] = 0;
            for (j = 0; j < g_GameState.playerCount; j++) {
                if (j != i && TeesAfter(i, j)) {
                    orders[i]++;
                }
            }
        }
    }
    /* The do-while is there to match */
    do {
        for (i = 0; i < g_GameState.playerCount; i++) {
            g_Players[i].teeOrder = orders[i];
        }
    } while (0);
}

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
    SetUpPlayers();
    osSyncPrintf("SetSuperShotReplay\n");
}

/* Sets play up for the Ring Shot hole of a record */
void LoadRingShotRecord(RingShotRecord *record) {
    D_80105F40[0] = *record;
    g_GameMode = GAME_MODE_RING_SHOT;
    g_CourseIndex = record->courseIndex;
    g_HoleIndex = record->holeIndex;
    D_800FE330 = record->unk3A;
    SetUpPlayers();
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

/*
 * Sets the fog range and color from g_SceneFog, or from g_DebugFog,
 * with no fog alpha, while GAME_FLAG_SKY_EDITOR is set
 */
void SetSceneFog(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    if (IsGameFlagSet(GAME_FLAG_SKY_EDITOR)) {
        gSPFogPosition(gfx++, g_DebugFog.near, g_DebugFog.far);
        gDPSetFogColor(gfx++, g_DebugFog.color[0], g_DebugFog.color[1], g_DebugFog.color[2], 0);
    } else {
        gSPFogPosition(gfx++, g_SceneFog.near, g_SceneFog.far);
        gDPSetFogColor(gfx++, g_SceneFog.color[0], g_SceneFog.color[1], g_SceneFog.color[2], 0xFF);
    }
    *displayListHead = gfx;
}

void CopyDoublePair(f64 *from, f64 *to) {
    to[0] = from[0];
    to[1] = from[1];
}

/*
 * Unless on a mini-golf course, marks the ground tile holding a point
 * and the tiles around it in D_80104EB8
 */
void MarkNearbyGroundTiles(u32 x, u32 z) {
    u32 column;
    u32 row;
    s32 i;
    s32 j;

    if (!g_GameState.isMiniGolf) {
        column = x >> GROUND_TILE_SHIFT;
        row = z >> GROUND_TILE_SHIFT;
        if (column < GROUND_TILE_COLUMN_COUNT) {
            if (row < GROUND_TILE_ROW_COUNT) {
                for (i = -1; i < 2; i++) {
                    for (j = -1; j < 2; j++) {
                        if (column + j < GROUND_TILE_COLUMN_COUNT && row + i < GROUND_TILE_ROW_COUNT) {
                            D_80104EB8[(row + i) * GROUND_TILE_COLUMN_COUNT + (column + j)] = TRUE;
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

static inline s32 GetMirroredOffset(s32 coordinate) {
    s32 tile = coordinate / 512;
    s32 offset;

    offset = coordinate - tile * 512;
    if (tile % 2) {
        offset = 512 - offset;
    }
    return offset;
}

#ifdef NON_MATCHING
void func_800628C4(void) {
    Vtx_t point;
    s32 row;
    s32 column;
    s32 offset;

    ClearGameFlag(7);
    for (row = 0; row < GROUND_POINT_ROW_COUNT; row++) {
        for (column = 0; column < GROUND_POINT_COLUMN_COUNT; column++) {
            point = GetGroundPoint(column, row)->v;
            offset = GetMirroredOffset(point.ob[0]);
            if (offset != 512) {
                point.tc[0] = offset << 5;
            } else {
                point.tc[0] = 0x4000;
            }
            point.tc[0] += 0x100;
            offset = GetMirroredOffset(point.ob[2]);
            if (offset != 512) {
                point.tc[1] = offset << 5;
            } else {
                point.tc[1] = 0x4000;
            }
            point.tc[1] += 0x100;
            SetGroundPoint(column, row, &point);
        }
    }
}

#else
INCLUDE_ASM("asm/nonmatchings/play", func_800628C4);
#endif

INCLUDE_ASM("asm/nonmatchings/play", func_80062A24);

INCLUDE_ASM("asm/nonmatchings/play", func_80062A8C);

