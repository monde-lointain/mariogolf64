#include "common.h"
#include "camera.h"
#include "character.h"
#include "club.h"
#include "controller.h"
#include "course.h"
#include "current_shot.h"
#include "game_flags.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "hole_triangles.h"
#include "libc.h"
#include "main.h"
#include "math_util.h"
#include "movie.h"
#include "overlay_manager.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "shot_effects.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "tournament.h"
#include "variables.h"

/* D_800E12B0 is a square of random bytes this many on a side */
#define RANDOM_GRID_SIZE 32

/* UnkB8.unkC values; PlayContestMovies waits for every player to have one */
#define PLAYER_STATE_UNK11 0x11
#define PLAYER_STATE_UNK14 0x14

/* A fog color and the range over which the fog thickens */
/* How far above the tee a ball starts on the main courses */
#define TEE_HEIGHT 307.2f

extern PlayRecord D_800FEDD8[];
extern RingShotRecord D_80105F40[];
extern s32 D_800C3070;
extern s32 D_800C3074;
extern s16 D_800C3078;
extern s16 D_800C307A;
extern s32 D_800C307C;
extern s32 D_800C3080;
extern s32 D_800C3084;
extern s32 D_800C3088;
extern s32 D_800C308C;
extern s8 D_800C30A4;
extern s32 g_PlayExit;
extern s32 g_HoleEndStep;
extern u8 g_IsGroundTileNearby[GROUND_TILE_COUNT];
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
void func_80407D64(void);
void FillRandomGrid(s32 range, s32 base);
void SetUpPlayers(void);
void WriteBallDistancesIfEnabled(void);
void InitExpansionPakFeatures(void);
void LoadSuperShotReplay(PlayRecord *record);

s32 func_8005F180(void) {
    return IsGameFlagSet(0x32) || IsGameFlagSet(0x9B);
}

void func_8005F1C8(void) {
    g_PauseScreen = PAUSE_SCREEN_NONE;
    D_800C3070 = 0;
    D_800C3074 = 0;
    D_800C3078 = 0;
    D_800C3084 = 0;
    D_800C3088 = 0;
    D_800C308C = 0;
    g_IsSettingImpactPoint = 0;
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
            g_Players[0].paletteIndex = D_80105F40->paletteIndex;
            g_Players[0].unk88 = D_80105F40->unk3C;
        } else {
            g_Players[0].unk1E[0] = 0;
            g_Players[0].teeOrder = 0;
            g_Players[0].characterIndex = D_800FEDD8->characterIndex;
            g_Players[0].unk22 = D_800FEDD8->unk3C;
            g_Players[0].paletteIndex = D_800FEDD8->paletteIndex;
            g_Players[0].unk88 = D_800FEDD8->unk40;
        }
        for (hole = 0; hole < COURSE_HOLE_COUNT; hole++) {
            g_Players[0].holes[hole].strokes = 0;
            g_Players[0].holes[hole].putts = UNCOUNTED_PUTTS;
        }
        g_Players[0].unk28 = -1;
        LoadPlayerFace(0, (s8)g_Players[0].characterIndex, (s8)g_Players[0].paletteIndex);
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
            g_Players[i].gridColor = GetSaveData()->options[SAVE_OPTION_GRID_COLOR];
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
            g_Players[i].stopwatchFrameCount = 0;
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

    D_80104F78 = D_800FEDD8->shots[index].heading;
    D_80104F7C = D_800FEDD8->shots[index].unk2;
    D_80104F80 = D_800FEDD8->shots[index].unk3;
    D_80104F84 = D_800FEDD8->shots[index].shotMode;
    D_80104F88 = D_800FEDD8->shots[index].clubSlot;
    D_80104F8C = D_800FEDD8->shots[index].unk6;
    D_80104F90 = D_800FEDD8->shots[index].unk7;
}

void func_80060128(s32 index) {
    D_80104F78 = D_80105F40->shots[index].heading;
    D_80104F7C = D_80105F40->shots[index].unk2;
    D_80104F80 = D_80105F40->shots[index].unk3;
    D_80104F84 = D_80105F40->shots[index].shotMode;
    D_80104F88 = D_80105F40->shots[index].clubSlot;
    D_80104F8C = D_80105F40->shots[index].unk6;
    D_80104F90 = D_80105F40->shots[index].unk7;
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
    D_800FE330 = record->strokeCount;
    SetUpPlayers();
    osSyncPrintf("SetSuperShotReplay\n");
}

/* Sets play up for the Ring Shot hole of a record */
void LoadRingShotRecord(RingShotRecord *record) {
    D_80105F40[0] = *record;
    g_GameMode = GAME_MODE_RING_SHOT;
    g_CourseIndex = record->courseIndex;
    g_HoleIndex = record->holeIndex;
    D_800FE330 = record->strokeCount;
    SetUpPlayers();
}

void func_8006034C(s8 value) {
    D_800C30A4 = value;
}

void func_80060358(void) {
    g_PauseScreen = PAUSE_SCREEN_NONE;
    SaveOrContinueGameByMode(FALSE);
    ClearShotEffects(0);
    g_GameState.unk0 = 7;
    g_PlayExit = 3;
    g_HoleEndStep = 9;
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

extern s32 g_HoleResultsKind;
extern s32 g_PlayStateFrameCount;
extern s32 g_PlayFrameCount;
extern s8 g_IsPlayStateBackedUp;
extern s32 g_DebugCupIndex;
extern s32 D_800C42D0;
extern s8 g_WasDonkeyKongUnlocked;
extern s8 g_WasMapleUnlocked;
extern s8 g_WasMetalMarioUnlocked;
extern s8 g_HadEveryRingShotStar;
extern s8 g_RingShotCourseOpenedAtStart;
extern s8 D_800C3068;
extern s32 D_801B60A0;
void func_80241800();
void func_80242384();
s32 func_80241FF0();
s32 func_80241DF0();
void func_801F5C24();
s32 func_801F5F2C(void);
void func_801F5E9C(void);
void func_8020FAE8();
void func_80242334();
void func_8024186C();
s32 func_80243E28();
s32 func_801F8A30();
s32 func_802436C8();
void func_80216B74();
void func_801F4AA4();
void func_802088B0(void);
s32 func_8024216C(void);
s32 func_80242374(void);
void func_80241EE8(void);
void func_80243898(void);
void func_802439B0(void);
void func_8024407C(void);
void func_80242074(void);
void func_80205C78(void);
void func_801FDA04(void);
void func_8005D1E4(void);
void func_8005DAFC(void);
void func_8005DC50(void);
void func_8005DD10(void);
void func_8005DD44(void);
void func_8005DD78(void);
void func_8005DDAC(void);

#ifdef NON_MATCHING
/* g_GameState.unk0, the step that UpdatePlay is on */
#define GAME_STATE_START 0
#define GAME_STATE_LOAD_HOLE 1
#define GAME_STATE_WAIT_FOR_HOLE 2
#define GAME_STATE_HOLE_INTRO 3
#define GAME_STATE_FLYOVER 4
#define GAME_STATE_UNK5 5
#define GAME_STATE_UNK6 6 /* Taking turns */
#define GAME_STATE_HOLE_END 7
#define GAME_STATE_SLOTS 8 /* Spinning the club slots before the hole */
#define GAME_STATE_UNK11 11
#define GAME_STATE_END_INTRO 12
#define GAME_STATE_RELOAD_HOLE 13

/* g_HoleEndStep, the step of the sequence between holes */
#define HOLE_END_STEP_START 0
#define HOLE_END_STEP_FREE_SLOT 1 /* Frees the character slot, then loads the next hole */
#define HOLE_END_STEP_SCORECARD 2
#define HOLE_END_STEP_WIPE_OUT 3
#define HOLE_END_STEP_LOAD_NEXT_HOLE 4
#define HOLE_END_STEP_UNK5 5
#define HOLE_END_STEP_RING_SHOT_FREE_SLOT 6
#define HOLE_END_STEP_RING_SHOT_RESULTS 7
#define HOLE_END_STEP_PROMPT 8
#define HOLE_END_STEP_QUIT 9
#define HOLE_END_STEP_FINAL_RESULTS 10
#define HOLE_END_STEP_LOAD_SCORECARD 11
#define HOLE_END_STEP_LOAD_RING_SHOT 12
#define HOLE_END_STEP_DONKEY_KONG_NOTICE 13
#define HOLE_END_STEP_UNLOCK_NOTICE 14
#define HOLE_END_STEP_STAFF_ROLL 15
#define HOLE_END_STEP_RING_SHOT_COURSE_NOTICE 16

/* g_PlayExit, the menu that play returns to once the hole ends */
#define PLAY_EXIT_NONE 0 /* Plays on to the next hole */
#define PLAY_EXIT_UNK1 1
#define PLAY_EXIT_UNK2 2
#define PLAY_EXIT_UNK3 3
#define PLAY_EXIT_UNK4 4
#define PLAY_EXIT_RING_SHOT 5
#define PLAY_EXIT_QUIT 6
#define PLAY_EXIT_ROUND_OVER 7

/* g_HoleResultsKind, the results that the scorecard shows between holes */
#define HOLE_RESULTS_STROKE 0
#define HOLE_RESULTS_MATCH 1
#define HOLE_RESULTS_SKINS 2
#define HOLE_RESULTS_TOURNAMENT 3
#define HOLE_RESULTS_SPEED_GOLF 4

/* The choices that the Training and Ring Shot menus return */
#define HOLE_MENU_NONE 0
#define HOLE_MENU_RELOAD 1
#define HOLE_MENU_QUIT 2

/* What the Ring Shot results return */
#define RING_SHOT_RESULTS_DONE 1
#define RING_SHOT_RESULTS_SAVE 2
/* The argument that starts the Ring Shot results */
#define RING_SHOT_RESULTS_MODE_UNK2 2

/* The results of the prompt that the Ring Shot results open */
#define PROMPT_RESULT_UNK1 1
#define PROMPT_RESULT_UNK2 2
#define PROMPT_RESULT_UNK7 7

/* The notices shown when the hole unlocks something */
#define UNLOCK_NOTICE_DONKEY_KONG 0
#define UNLOCK_NOTICE_MAPLE 1
#define UNLOCK_NOTICE_METAL_MARIO 2
#define UNLOCK_NOTICE_RING_SHOT_COURSE_BASE 2 /* Plus the course's index */

#define MODULE_UNK6 6
#define MODULE_UNK8 8
#define MODULE_UNK10 10
#define MODULE_UNK21 21
#define MODULE_UNK26 26
#define SUB_MODULE_SCORECARD 0x05
#define SUB_MODULE_UNK22 0x16
#define SCENE_MODULE_MENU 0x02
#define SCENE_MODULE_PLAY 0x0A
#define SCENE_MODULE_STAFF_ROLL 0x1F

#define WIPE_CLOSED_PROGRESS 0
#define WIPE_OPEN_PROGRESS 20
#define ROUND_FRAMEBUFFER_COUNT 3
#define PLAYER_CHARACTER_SLOT 0
#define TOURNAMENT_HALF_TURN_HOLE 9 /* The hole index after the front nine */

#define SOUND_CANCEL 0x5F
#define SOUND_CHANNEL 15
#define SOUND_VOLUME 127

/* How long the hole's intro waits before it moves on by itself */
#define HOLE_INTRO_FRAMES 240
#define SHORT_HOLE_INTRO_FRAMES 90
/* How soon the debug flags that advance play move it on */
#define AUTO_ADVANCE_FRAMES 10
#define AUTO_ADVANCE_RESULTS_FRAMES 90

/* Whether the round has played its last hole, once g_HoleIndex has moved past it */
#define HAS_PLAYED_LAST_HOLE() (g_HoleIndex == GetFirstHoleIndex() + GetRoundHoleCount())
#define IS_A_PRESSED() (g_MergedController.pressedButtons & A_BUTTON)
/* Whether the Ring Shot hole has just earned the last star, which rolls the staff */
#define IS_STAFF_ROLL_DUE_AFTER_RING_SHOT() \
    ((HasEveryRingShotStar() && !g_HadEveryRingShotStar) || IsGameFlagSet(GAME_FLAG_SHOW_STAFF_ROLL))

/* The debug cup picker's text position */
#define DEBUG_CUP_TEXT_COLUMN 3
#define DEBUG_CUP_TEXT_ROW 3

/* The hole intro camera circles the hole at these distances */
#define INTRO_ORBIT_ANGLE_PER_FRAME 256 /* In binary angle units */
#define INTRO_ORBIT_ANGLE() \
    (((g_PlayStateFrameCount * INTRO_ORBIT_ANGLE_PER_FRAME) & BINARY_ANGLE_MASK) * BINARY_ANGLE_TO_RADIANS)
#define DEBUG_ORBIT_RADIUS 1500.0f
#define DEBUG_ORBIT_HEIGHT (-1500.0f)
#define INTRO_ORBIT_RADIUS 3000.0f
#define INTRO_ORBIT_HEIGHT (-3000.0f)
#define MINI_GOLF_ORBIT_RADIUS 600.0f
#define MINI_GOLF_ORBIT_HEIGHT (-300.0f)

/*
 * Runs play for a frame: loads each hole, shows its intro and flyover, takes
 * the turns, and steps through the scorecard, notices and menus between
 * holes until the round ends and play returns to a menu or the staff roll.
 */
void UpdatePlay(void) {
    s32 repeat;
    s32 again;
    s32 anyHuman;
    s32 i;
    s32 result;
    s32 clearHud;
    s32 nextStep;

    repeat = FALSE;
    g_PlayFrameCount++;
    if (g_Players[0].controllerIndex < MAXCONTROLLERS) {
        g_TurnController = g_ControllerStates[g_Players[0].controllerIndex];
    } else {
        g_TurnController.stickX = 0;
        g_TurnController.stickY = 0;
        g_TurnController.buttons = 0;
        g_TurnController.repeatedButtons = 0;
        g_TurnController.pressedButtons = 0;
    }
    /* Menus between holes answer to any human player */
    anyHuman = FALSE;
    g_MergedController.buttons = 0;
    g_MergedController.heldButtons = 0;
    g_MergedController.pressedButtons = 0;
    g_MergedController.repeatedButtons = 0;
    for (i = 0; i != g_PlayerCount; i++) {
        if (g_Players[i].controllerIndex < MAXCONTROLLERS) {
            g_MergedController.buttons |= g_ControllerStates[g_Players[i].controllerIndex].buttons;
            g_MergedController.heldButtons |= g_ControllerStates[g_Players[i].controllerIndex].heldButtons;
            g_MergedController.pressedButtons |= g_ControllerStates[g_Players[i].controllerIndex].pressedButtons;
            g_MergedController.repeatedButtons |= g_ControllerStates[g_Players[i].controllerIndex].repeatedButtons;
            anyHuman = TRUE;
        }
    }
    if (!anyHuman) {
        g_MergedController.buttons = g_ControllerStates[0].buttons;
        g_MergedController.heldButtons = g_ControllerStates[0].heldButtons;
        g_MergedController.pressedButtons = g_ControllerStates[0].pressedButtons;
        g_MergedController.repeatedButtons = g_ControllerStates[0].repeatedButtons;
    }
    /* A step that hands over to another within the same frame sets repeat */
    again = repeat;
    do {
        if (again) {
            osSyncPrintf("noexit\n");
        }
        repeat = FALSE;
        switch (g_GameState.unk0) {
            case GAME_STATE_START:
                g_GameState.unk0 = GAME_STATE_LOAD_HOLE;
            case GAME_STATE_LOAD_HOLE:
                StopLoaderThread();
                StartLoaderThread();
                g_GameState.unk0 = GAME_STATE_WAIT_FOR_HOLE;
                osSyncPrintf("MAKEHOLE2\n");
                break;
            case GAME_STATE_RELOAD_HOLE:
                if (g_HasLoaderFinished == TRUE) {
                    g_GameState.unk0 = GAME_STATE_WAIT_FOR_HOLE;
                }
                break;
            case GAME_STATE_WAIT_FOR_HOLE:
                if (g_HasLoaderFinished == TRUE) {
                    if (!IsSubModuleLoaded()) {
                        break;
                    }
                    g_GameState.unk0 = GAME_STATE_HOLE_INTRO;
                    func_801FDA04();
                    SetGameFlag(GAME_FLAG_HOLE_LOADED);
                    if (g_GameMode == GAME_MODE_RING_SHOT) {
                        if (GetWipeKind() == WIPE_CLOSING) {
                            StartWipe(WIPE_OPENING_FAST);
                        }
                        func_80242384(TRUE);
                    } else {
                        StartWipe(WIPE_OPENING_FAST);
                    }
                    memset(&g_RoundHud, 0, sizeof(RoundHud));
                    StartHoleIntro();
                    StartHoleCamerasAndSong();
                    g_PlayStateFrameCount = 0;
                } else {
                    g_PlayStateFrameCount++;
                    if (g_GameMode == GAME_MODE_TRAINING) {
                        if (g_IsModuleAlive[MODULE_UNK21] &&
                            func_80241FF0(g_MergedController.pressedButtons, g_MergedController.repeatedButtons) ==
                                HOLE_MENU_RELOAD) {
                            LoadWipeShape(g_CourseIndex);
                            SetMenuBackdrop(MENU_BACKDROP_STILL);
                            if (GetWipeProgress() == WIPE_OPEN_PROGRESS) {
                                StartWipe(WIPE_CLOSING);
                            }
                            g_GameState.unk0 = GAME_STATE_LOAD_HOLE;
                            repeat = TRUE;
                        }
                    } else if (g_GameMode == GAME_MODE_RING_SHOT && !g_GameState.unk31 &&
                               g_IsModuleAlive[MODULE_UNK21]) {
                        result = func_80241DF0(g_MergedController.pressedButtons, g_MergedController.repeatedButtons);
                        if (result == HOLE_MENU_RELOAD) {
                            LoadWipeShape(g_CourseIndex);
                            SetMenuBackdrop(MENU_BACKDROP_STILL);
                            func_80242384(FALSE);
                            g_GameState.unk0 = result;
                            repeat = TRUE;
                        }
                    }
                }
                break;
            case GAME_STATE_HOLE_INTRO:
                g_IsPlayStateBackedUp = 0;
                nuGfxDisplayOn();
                if (g_GameMode == GAME_MODE_TRAINING) {
                    if (g_IsModuleAlive[MODULE_UNK21]) {
                        result = func_80241FF0(g_MergedController.pressedButtons, g_MergedController.repeatedButtons);
                        switch (result) {
                            case HOLE_MENU_RELOAD:
                                SetMenuBackdrop(MENU_BACKDROP_STILL);
                                StartWipe(WIPE_CLOSING);
                                repeat = TRUE;
                                LoadWipeShape(g_CourseIndex);
                                g_GameState.unk0 = result;
                                break;
                            case HOLE_MENU_QUIT:
                                SetMenuBackdrop(MENU_BACKDROP_STILL);
                                g_PlayExit = PLAY_EXIT_QUIT;
                                PlaySound(SOUND_CANCEL, SOUND_CHANNEL, SOUND_VOLUME);
                                g_GameState.unk0 = GAME_STATE_HOLE_END;
                                g_HoleEndStep = HOLE_END_STEP_START;
                                ClearHoleIntroText();
                                break;
                            case HOLE_MENU_NONE:
                                if (GetWipeProgress() == WIPE_OPEN_PROGRESS && IS_A_PRESSED()) {
                                    g_GameState.unk0 = GAME_STATE_END_INTRO;
                                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                                }
                                break;
                        }
                    }
                } else if (g_GameMode == GAME_MODE_RING_SHOT && !g_GameState.unk31) {
                    if (g_IsModuleAlive[MODULE_UNK21]) {
                        result = func_80241DF0(g_MergedController.pressedButtons, g_MergedController.repeatedButtons);
                        switch (result) {
                            case HOLE_MENU_RELOAD:
                                SetMenuBackdrop(MENU_BACKDROP_STILL);
                                func_80242384(FALSE);
                                g_GameState.unk0 = result;
                                repeat = TRUE;
                                LoadWipeShape(g_CourseIndex);
                                break;
                            case HOLE_MENU_QUIT:
                                SetMenuBackdrop(MENU_BACKDROP_STILL);
                                g_PlayExit = PLAY_EXIT_QUIT;
                                PlaySound(SOUND_CANCEL, SOUND_CHANNEL, SOUND_VOLUME);
                                g_GameState.unk0 = GAME_STATE_HOLE_END;
                                g_HoleEndStep = HOLE_END_STEP_START;
                                ClearHoleIntroText();
                                break;
                            case HOLE_MENU_NONE:
                                if (func_80242374() && GetWipeProgress() == WIPE_OPEN_PROGRESS &&
                                    IS_A_PRESSED()) {
                                    g_GameState.unk0 = GAME_STATE_END_INTRO;
                                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                                }
                                break;
                        }
                    }
                } else if (g_GameMode == GAME_MODE_CLUB_SLOTS) {
                    if (g_IsModuleAlive[MODULE_UNK26] &&
                        (g_PlayStateFrameCount >= SHORT_HOLE_INTRO_FRAMES || IS_A_PRESSED())) {
                        if (!g_GameState.unk30) {
                            g_GameState.unk0 = GAME_STATE_SLOTS;
                            func_80241800();
                        } else {
                            StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                            g_GameState.unk0 = GAME_STATE_END_INTRO;
                        }
                    }
                } else if (g_GameState.unk31) {
                    if (g_PlayStateFrameCount >= SHORT_HOLE_INTRO_FRAMES || IS_A_PRESSED()) {
                        if (g_GameMode == GAME_MODE_SPEED_GOLF) {
                            StartHoleFlyover();
                            EndHoleIntro();
                            g_GameState.unk0 = GAME_STATE_UNK6;
                            EndHoleFlyover();
                            StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                            g_PlayStateFrameCount = 0;
                        } else {
                            StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                            g_GameState.unk0 = GAME_STATE_END_INTRO;
                        }
                    }
                } else if (GetWipeProgress() == WIPE_OPEN_PROGRESS &&
                           (g_PlayStateFrameCount >= HOLE_INTRO_FRAMES || IS_A_PRESSED() ||
                            (func_8005F180() && g_PlayStateFrameCount >= AUTO_ADVANCE_FRAMES))) {
                    if (g_GameMode == GAME_MODE_SPEED_GOLF) {
                        StartHoleFlyover();
                        EndHoleIntro();
                        g_GameState.unk0 = GAME_STATE_UNK6;
                        EndHoleFlyover();
                        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                        g_PlayStateFrameCount = 0;
                    } else {
                        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                        g_GameState.unk0 = GAME_STATE_END_INTRO;
                    }
                }
                g_PlayStateFrameCount++;
                /* The camera circles the hole while its intro is shown */
                {
                    Vec3 lookAt;
                    Vec3 eye;
                    char text[0x80];

                    /* Debug: circles the chosen cup position instead, and moves the cup there */
                    if (IsGameFlagSet(GAME_FLAG_CHOOSE_CUP)) {
                        if (g_ControllerData[1].button & L_TRIG) {
                            g_DebugCupIndex--;
                        }
                        if (g_ControllerData[1].button & R_TRIG) {
                            g_DebugCupIndex++;
                        }
                        g_DebugCupIndex &= CUP_POSITION_COUNT - 1;
                        sprintf(text, "PinNo%d ", g_DebugCupIndex);
                        PrintDebugText(text, DEBUG_CUP_TEXT_COLUMN, DEBUG_CUP_TEXT_ROW);
                        D_800BA9D8.cupPosition[AXIS_X] = g_HolePoints.cupPositions[g_DebugCupIndex][AXIS_X];
                        g_CupPositionZ = g_HolePoints.cupPositions[g_DebugCupIndex][AXIS_Z];
                        g_CupPositionY = GetGroundHeight(D_800BA9D8.cupPosition[AXIS_X], g_CupPositionZ);
                        lookAt[AXIS_X] = D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
                        lookAt[AXIS_Y] = g_CupPositionY * COURSE_TO_WORLD_SCALE;
                        lookAt[AXIS_Z] = g_CupPositionZ * COURSE_TO_WORLD_SCALE;
                        eye[AXIS_X] = cosf(INTRO_ORBIT_ANGLE()) * DEBUG_ORBIT_RADIUS + lookAt[AXIS_X];
                        eye[AXIS_Y] = lookAt[AXIS_Y] + DEBUG_ORBIT_HEIGHT;
                        eye[AXIS_Z] = sinf(INTRO_ORBIT_ANGLE()) * DEBUG_ORBIT_RADIUS + lookAt[AXIS_Z];
                    } else {
                        lookAt[AXIS_X] =
                            (D_800BA9D8.cupPosition[AXIS_X] + g_TeePositionX) * (COURSE_TO_WORLD_SCALE / 2);
                        lookAt[AXIS_Y] = (g_CupPositionY + g_TeePositionY) * (COURSE_TO_WORLD_SCALE / 2);
                        lookAt[AXIS_Z] = (g_CupPositionZ + g_TeePositionZ) * (COURSE_TO_WORLD_SCALE / 2);
                        if (!g_GameState.isMiniGolf) {
                            eye[AXIS_X] = cosf(INTRO_ORBIT_ANGLE()) * INTRO_ORBIT_RADIUS + lookAt[AXIS_X];
                            eye[AXIS_Y] = lookAt[AXIS_Y] + INTRO_ORBIT_HEIGHT;
                            eye[AXIS_Z] = sinf(INTRO_ORBIT_ANGLE()) * INTRO_ORBIT_RADIUS + lookAt[AXIS_Z];
                        } else {
                            eye[AXIS_X] = cosf(INTRO_ORBIT_ANGLE()) * MINI_GOLF_ORBIT_RADIUS + lookAt[AXIS_X];
                            eye[AXIS_Y] = lookAt[AXIS_Y] + MINI_GOLF_ORBIT_HEIGHT;
                            eye[AXIS_Z] = sinf(INTRO_ORBIT_ANGLE()) * MINI_GOLF_ORBIT_RADIUS + lookAt[AXIS_Z];
                        }
                    }
                    g_Cameras[0].view.mode = CAMERA_MODE_UNK8;
                    g_Cameras[0].view.eye[AXIS_X] = eye[AXIS_X];
                    g_Cameras[0].view.eye[AXIS_Y] = eye[AXIS_Y];
                    g_Cameras[0].view.eye[AXIS_Z] = eye[AXIS_Z];
                    g_Cameras[0].view.lookAt[AXIS_X] = lookAt[AXIS_X];
                    g_Cameras[0].view.lookAt[AXIS_Y] = lookAt[AXIS_Y];
                    g_Cameras[0].view.lookAt[AXIS_Z] = lookAt[AXIS_Z];
                }
                break;
            case GAME_STATE_SLOTS:
                if (func_8024216C() == TRUE) {
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    if (g_GameMode == GAME_MODE_CLUB_SLOTS && !g_GameState.unk30) {
                        func_80241EE8();
                    }
                    g_GameState.unk0 = GAME_STATE_END_INTRO;
                }
                break;
            case GAME_STATE_END_INTRO:
                StepHoleFlyover();
                g_GameState.unk0 = GAME_STATE_FLYOVER;
                EndHoleIntro();
                StartHoleFlyover();
                break;
            case GAME_STATE_FLYOVER:
                g_RoundHud.isHoleIntroTextShown = FALSE;
                D_800C3068 = 0;
                g_Cameras[0].view.mode = CAMERA_MODE_BEHIND_GOLFER;
                g_Cameras[1].view.mode = CAMERA_MODE_BEHIND_GOLFER;
                if (StepHoleFlyover() ||
                    (IsHudAnimationDone() && (IS_A_PRESSED() || func_8005F180()))) {
                    g_GameState.unk0 = GAME_STATE_UNK6;
                    EndHoleFlyover();
                }
                g_PlayStateFrameCount = 0;
                break;
            case GAME_STATE_UNK5:
                g_PlayStateFrameCount++;
                break;
            case GAME_STATE_UNK6:
                UnloadOverlay(GetOverlayOfModule(MODULE_UNK6));
            case GAME_STATE_UNK11:
                if (!g_IsPlayStateBackedUp) {
                    D_800FBDB8 = 0;
                    ClearContestWinner();
                    g_IsPlayStateBackedUp = TRUE;
                    BackUpPlayState();
                    g_WasDonkeyKongUnlocked = IsDonkeyKongUnlocked();
                    g_WasMapleUnlocked = IsMapleUnlocked();
                    g_WasMetalMarioUnlocked = IsMetalMarioUnlocked();
                    g_HadEveryRingShotStar = HasEveryRingShotStar();
                    g_RingShotCourseOpenedAtStart = GetRingShotCourseOpenedAtStarCount();
                }
                g_GameState.unk30 = 0;
                g_Cameras[0].view.mode = CAMERA_MODE_BEHIND_GOLFER;
                g_Cameras[1].view.mode = CAMERA_MODE_BEHIND_GOLFER;
                if (g_IsModuleAlive[MODULE_UNK10] || ReportModuleNotAlive(MODULE_UNK10)) {
                    func_802088B0();
                }
                PlayContestMovies();
                if (func_8005FCB8()) {
                    if (g_GameState.unk0 == GAME_STATE_UNK11) {
                        ChooseConditionsAndInitHole();
                    } else {
                        g_GameState.unk0 = GAME_STATE_HOLE_END;
                        g_HoleEndStep = HOLE_END_STEP_START;
                        g_PlayStateFrameCount = 0;
                    }
                }
                if (g_GameState.unk0 == GAME_STATE_HOLE_END) {
                    UnloadOverlay(GetOverlayOfModule(MODULE_UNK8));
                    SetNextTeeOrder();
                    UpdateContest(TRUE);
                    if (g_GameMode == GAME_MODE_TOURNAMENT && g_Players[0].unk26 == TRUE) {
                        RecordHoleStats();
                        if (g_Players[0].strokeCount < g_HolePar) {
                            AddBirdieBadge();
                        }
                    }
                    if (!g_GameState.unk31 &&
                        (g_GameMode == GAME_MODE_SKINS_MATCH || g_GameMode == GAME_MODE_CLUB_SLOTS)) {
                        s8 results[MAX_PLAYER_COUNT];

                        if (GetGroupHoleResult(results) != TRUE) {
                            for (i = 0; i < g_GameState.playerCount; i++) {
                                g_Players[i].holes[g_GameState.holeIndex].strokes = 0;
                            }
                        } else {
                            for (i = 0; i < g_GameState.playerCount; i++) {
                                if (results[i] != TRUE) {
                                    g_Players[i].holes[g_GameState.holeIndex].strokes = 0;
                                } else {
                                    g_Players[i].holes[g_GameState.holeIndex].strokes = TRUE;
                                }
                            }
                        }
                    }
                }
                break;
            case GAME_STATE_HOLE_END:
                g_PlayStateFrameCount++;
                clearHud = TRUE;
                switch (g_HoleEndStep) {
                    case HOLE_END_STEP_START:
                        g_HoleEndStep = HOLE_END_STEP_FREE_SLOT;
                        g_PlayStateFrameCount = 0;
                        g_RoundHud.scorecardPage = -1;
                        g_RoundHud.isHoleIntroTextShown = FALSE;
                        EndHoleIntro();
                        for (i = 0; i < g_GameState.playerCount; i++) {
                            g_Players[i].unk26 = 0;
                        }
                        if (g_GameMode == GAME_MODE_TRAINING) {
                            if (g_PlayExit == PLAY_EXIT_NONE) {
                                g_HoleIndex = (g_HoleIndex + 1) % COURSE_HOLE_COUNT;
                            }
                        } else {
                            g_HoleIndex++;
                        }
                        if (g_GameState.gameMode == GAME_MODE_UNK8 || g_GameState.unk31) {
                            g_GameState.unk31 = 0;
                            func_80060358();
                            if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                                g_PlayExit = PLAY_EXIT_RING_SHOT;
                            } else {
                                g_PlayExit = PLAY_EXIT_UNK4;
                            }
                            StartWipe(WIPE_CLOSING);
                            clearHud = FALSE;
                            g_HoleEndStep = HOLE_END_STEP_FREE_SLOT;
                            SetMenuBackdrop(MENU_BACKDROP_STILL);
                        } else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                            if (g_PlayExit == PLAY_EXIT_NONE) {
                                g_HoleIndex %= RING_SHOT_HOLE_COUNT;
                                func_8005D1E4();
                            }
                            if (g_PlayExit == PLAY_EXIT_NONE) {
                                if (!D_800C30A4) {
                                    nextStep = HOLE_END_STEP_RING_SHOT_FREE_SLOT;
                                } else if ((IsDonkeyKongUnlocked() && !g_WasDonkeyKongUnlocked) ||
                                           IsGameFlagSet(GAME_FLAG_CHOOSE_UNLOCK_NOTICE)) {
                                    func_801F5C24(UNLOCK_NOTICE_DONKEY_KONG);
                                    nextStep = HOLE_END_STEP_DONKEY_KONG_NOTICE;
                                } else if (!g_RingShotCourseOpenedAtStart && GetRingShotCourseOpenedAtStarCount()) {
                                    func_801F5C24(GetRingShotCourseOpenedAtStarCount() +
                                                  UNLOCK_NOTICE_RING_SHOT_COURSE_BASE);
                                    nextStep = HOLE_END_STEP_RING_SHOT_COURSE_NOTICE;
                                } else if (D_800C42D0) {
                                    nextStep = HOLE_END_STEP_RING_SHOT_FREE_SLOT;
                                } else {
                                    g_HoleEndStep = HOLE_END_STEP_LOAD_RING_SHOT;
                                    g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                                    g_PendingSubModuleId = SUB_MODULE_UNK22;
                                    goto dimBackdrop;
                                }
                                g_HoleEndStep = nextStep;
                            dimBackdrop:
                                SetMenuBackdrop(MENU_BACKDROP_DIMMING);
                                clearHud = FALSE;
                            } else {
                                clearHud = FALSE;
                                if (g_PlayExit == PLAY_EXIT_QUIT) {
                                    g_HoleEndStep = HOLE_END_STEP_QUIT;
                                }
                            }
                        } else if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
                            g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                            g_PendingSubModuleId = SUB_MODULE_SCORECARD;
                            g_HoleEndStep = HOLE_END_STEP_LOAD_SCORECARD;
                            SetMenuBackdrop(MENU_BACKDROP_DIMMING);
                            g_HoleResultsKind = HOLE_RESULTS_TOURNAMENT;
                            if (IsMapleUnlocked() && !g_WasMapleUnlocked) {
                                func_801F5C24(UNLOCK_NOTICE_MAPLE);
                                g_HoleEndStep = HOLE_END_STEP_UNLOCK_NOTICE;
                            } else if (IsMetalMarioUnlocked() && !g_WasMetalMarioUnlocked) {
                                func_801F5C24(UNLOCK_NOTICE_METAL_MARIO);
                                g_HoleEndStep = HOLE_END_STEP_UNLOCK_NOTICE;
                            } else if (IsGameFlagSet(GAME_FLAG_CHOOSE_UNLOCK_NOTICE)) {
                                func_801F5C24(UNLOCK_NOTICE_METAL_MARIO);
                                g_HoleEndStep = HOLE_END_STEP_UNLOCK_NOTICE;
                            } else if (IsGameFlagSet(GAME_FLAG_END_ROUND_AFTER_HOLE) ||
                                       HAS_PLAYED_LAST_HOLE()) {
                                RecordCourseRound();
                                RecordTournamentPlace();
                                g_PlayExit = PLAY_EXIT_ROUND_OVER;
                            }
                        } else if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
                            g_HoleEndStep = HOLE_END_STEP_LOAD_SCORECARD;
                            g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                            g_PendingSubModuleId = SUB_MODULE_SCORECARD;
                            SetMenuBackdrop(MENU_BACKDROP_DIMMING);
                            g_HoleResultsKind = HOLE_RESULTS_SPEED_GOLF;
                            if (IsGameFlagSet(GAME_FLAG_END_ROUND_AFTER_HOLE) || HAS_PLAYED_LAST_HOLE()) {
                                g_PlayExit = PLAY_EXIT_ROUND_OVER;
                                InsertBestTime(g_GameState.courseIndex, g_GameState.stopwatchFrameCount,
                                               GetNetScoreToPar(COURSE_HOLE_COUNT, 0), (s8)g_Players[0].characterIndex,
                                               (s8)g_Players[0].paletteIndex);
                            }
                        } else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                            g_HoleEndStep = HOLE_END_STEP_FREE_SLOT;
                            clearHud = FALSE;
                            SetMenuBackdrop(MENU_BACKDROP_STILL);
                            StartWipe(WIPE_CLOSING);
                        } else if (g_GameState.gameMode == GAME_MODE_MATCH_GAME ||
                                   g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
                            g_HoleEndStep = HOLE_END_STEP_LOAD_SCORECARD;
                            g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                            g_PendingSubModuleId = SUB_MODULE_SCORECARD;
                            SetMenuBackdrop(MENU_BACKDROP_DIMMING);
                            g_HoleResultsKind = HOLE_RESULTS_MATCH;
                            if (IsGameFlagSet(GAME_FLAG_END_ROUND_AFTER_HOLE) || GetMatchWinner(0, 1) != 0) {
                                g_PlayExit = PLAY_EXIT_ROUND_OVER;
                            }
                        } else if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                                   g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
                            g_HoleEndStep = HOLE_END_STEP_LOAD_SCORECARD;
                            g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                            g_PendingSubModuleId = SUB_MODULE_SCORECARD;
                            SetMenuBackdrop(MENU_BACKDROP_DIMMING);
                            g_HoleResultsKind = HOLE_RESULTS_SKINS;
                            if (IsGameFlagSet(GAME_FLAG_END_ROUND_AFTER_HOLE) || HAS_PLAYED_LAST_HOLE()) {
                                g_PlayExit = PLAY_EXIT_ROUND_OVER;
                            }
                        } else {
                            g_HoleEndStep = HOLE_END_STEP_LOAD_SCORECARD;
                            g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                            g_PendingSubModuleId = SUB_MODULE_SCORECARD;
                            SetMenuBackdrop(MENU_BACKDROP_DIMMING);
                            g_HoleResultsKind = HOLE_RESULTS_STROKE;
                            if (IsGameFlagSet(GAME_FLAG_END_ROUND_AFTER_HOLE)) {
                                g_PlayExit = PLAY_EXIT_ROUND_OVER;
                            } else if (HAS_PLAYED_LAST_HOLE()) {
                                if (IsGameFlagSet(GAME_FLAG_REPEAT_ROUND)) {
                                    g_HoleIndex = GetFirstHoleIndex();
                                } else {
                                    g_PlayExit = PLAY_EXIT_ROUND_OVER;
                                }
                            }
                        }
                        if (clearHud) {
                            g_RoundHud.isWindIndicatorShown = FALSE;
                            g_RoundHud.unk8 = 0;
                            g_RoundHud.isLieIconShown = FALSE;
                            g_RoundHud.isClubHeadIconShown = FALSE;
                            g_RoundHud.unkB = 0;
                            g_RoundHud.unkC = 0;
                            g_RoundHud.unk13 = 0;
                            g_RoundHud.unkD = 0;
                        }
                        g_PauseScreen = PAUSE_SCREEN_UNK2;
                        break;
                    case HOLE_END_STEP_DONKEY_KONG_NOTICE:
                        if (func_801F5F2C()) {
                            func_801F5E9C();
                            if (!g_RingShotCourseOpenedAtStart && GetRingShotCourseOpenedAtStarCount()) {
                                func_801F5C24(GetRingShotCourseOpenedAtStarCount() +
                                              UNLOCK_NOTICE_RING_SHOT_COURSE_BASE);
                                g_HoleEndStep = HOLE_END_STEP_RING_SHOT_COURSE_NOTICE;
                            } else if (D_800C42D0) {
                                g_HoleEndStep = HOLE_END_STEP_RING_SHOT_FREE_SLOT;
                            } else {
                                g_HoleEndStep = HOLE_END_STEP_LOAD_RING_SHOT;
                                g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                                g_PendingSubModuleId = SUB_MODULE_UNK22;
                            }
                        }
                        break;
                    case HOLE_END_STEP_RING_SHOT_COURSE_NOTICE:
                        if (func_801F5F2C()) {
                            func_801F5E9C();
                            if (D_800C42D0) {
                                g_HoleEndStep = HOLE_END_STEP_RING_SHOT_FREE_SLOT;
                            } else {
                                g_HoleEndStep = HOLE_END_STEP_LOAD_RING_SHOT;
                                g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                                g_PendingSubModuleId = SUB_MODULE_UNK22;
                            }
                        }
                        break;
                    case HOLE_END_STEP_UNLOCK_NOTICE:
                        if (func_801F5F2C()) {
                            func_801F5E9C();
                            g_HoleEndStep = HOLE_END_STEP_LOAD_SCORECARD;
                            if (IsGameFlagSet(GAME_FLAG_END_ROUND_AFTER_HOLE) || HAS_PLAYED_LAST_HOLE()) {
                                RecordCourseRound();
                                RecordTournamentPlace();
                                g_PlayExit = PLAY_EXIT_ROUND_OVER;
                            }
                        }
                        break;
                    case HOLE_END_STEP_UNK5:
                        if (IS_A_PRESSED() ||
                            (func_8005F180() && g_PlayStateFrameCount > AUTO_ADVANCE_RESULTS_FRAMES)) {
                            SetMenuBackdrop(MENU_BACKDROP_STILL);
                            g_PauseScreen = PAUSE_SCREEN_UNK2;
                            func_8020FAE8(-2);
                            g_HoleEndStep = HOLE_END_STEP_SCORECARD;
                            g_RoundHud.unk2 = 0;
                            func_8024407C();
                            func_80241800(GetCourseRecordIndex(g_CourseIndex), g_HoleIndex);
                            func_80242334(0);
                            if (g_PlayExit == PLAY_EXIT_NONE) {
                                StartLoaderThread();
                            }
                        }
                        break;
                    case HOLE_END_STEP_LOAD_SCORECARD:
                        if (IsSubModuleLoaded()) {
                            g_HoleEndStep = HOLE_END_STEP_FREE_SLOT;
                            func_80241800(GetCourseRecordIndex(g_CourseIndex), g_HoleIndex);
                            func_80242334(g_HoleResultsKind);
                        }
                        break;
                    case HOLE_END_STEP_FREE_SLOT:
                        if (IsCharacterSlotBusy(PLAYER_CHARACTER_SLOT)) {
                            FreeCharacterSlot(PLAYER_CHARACTER_SLOT);
                            break;
                        }
                        if (g_PlayExit == PLAY_EXIT_NONE) {
                            if (!g_GameState.unk31 &&
                                (g_GameMode == GAME_MODE_MATCH_GAME || g_GameMode == GAME_MODE_GET_CHARACTER) &&
                                HAS_PLAYED_LAST_HOLE()) {
                                g_HoleIndex = GetFirstHoleIndex();
                                D_801B60A0 = TRUE;
                            }
                            StartLoaderThread();
                        }
                        if (g_RoundHud.scorecardPage != 0) {
                            g_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                        } else {
                            g_HoleEndStep = HOLE_END_STEP_SCORECARD;
                        }
                        break;
                    case HOLE_END_STEP_LOAD_RING_SHOT:
                        if (IsSubModuleLoaded()) {
                            g_HoleEndStep = HOLE_END_STEP_RING_SHOT_FREE_SLOT;
                            func_8024186C(RING_SHOT_RESULTS_MODE_UNK2);
                        }
                        break;
                    case HOLE_END_STEP_RING_SHOT_FREE_SLOT:
                        if (IsCharacterSlotBusy(PLAYER_CHARACTER_SLOT)) {
                            FreeCharacterSlot(PLAYER_CHARACTER_SLOT);
                            break;
                        }
                        if (g_PlayExit == PLAY_EXIT_NONE) {
                            StartLoaderThread();
                            if (D_800C30A4 && !D_800C42D0) {
                                g_HoleEndStep = HOLE_END_STEP_RING_SHOT_RESULTS;
                            } else {
                                StartWipe(WIPE_CLOSING);
                                if (IS_STAFF_ROLL_DUE_AFTER_RING_SHOT()) {
                                    g_HoleEndStep = HOLE_END_STEP_STAFF_ROLL;
                                } else {
                                    g_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                                }
                            }
                        } else {
                            g_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                            StartWipe(WIPE_CLOSING);
                        }
                        break;
                    case HOLE_END_STEP_RING_SHOT_RESULTS:
                        switch (func_80243E28(&g_ControllerStates[g_CurrentShot.controllerIndex])) {
                            case RING_SHOT_RESULTS_DONE:
                                StartWipe(WIPE_CLOSING);
                                if (IS_STAFF_ROLL_DUE_AFTER_RING_SHOT()) {
                                    g_HoleEndStep = HOLE_END_STEP_STAFF_ROLL;
                                } else {
                                    g_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                                }
                                break;
                            case RING_SHOT_RESULTS_SAVE:
                                g_HoleEndStep = HOLE_END_STEP_PROMPT;
                                break;
                        }
                        break;
                    case HOLE_END_STEP_PROMPT:
                        result = UpdatePrompt(&g_TurnController);
                        switch (result) {
                            case PROMPT_RESULT_UNK7:
                                func_802439B0();
                                g_HoleEndStep = result;
                                OpenPrompt(NO_PROMPT);
                                break;
                            case PROMPT_RESULT_UNK1:
                                func_80243898();
                                g_HoleEndStep = HOLE_END_STEP_RING_SHOT_RESULTS;
                                OpenPrompt(NO_PROMPT);
                                break;
                            case PROMPT_RESULT_UNK2:
                                g_PauseScreen = PAUSE_SCREEN_UNK5;
                                OpenPrompt(NO_PROMPT);
                                g_HoleEndStep = HOLE_END_STEP_RING_SHOT_RESULTS;
                                break;
                        }
                        break;
                    case HOLE_END_STEP_SCORECARD:
                        if (func_802436C8(g_MergedController.repeatedButtons) &&
                            (IS_A_PRESSED() ||
                             (func_8005F180() && g_PlayStateFrameCount > AUTO_ADVANCE_FRAMES &&
                              g_PlayExit != PLAY_EXIT_ROUND_OVER))) {
                            if (g_PlayExit == PLAY_EXIT_ROUND_OVER) {
                                g_HoleEndStep = HOLE_END_STEP_FINAL_RESULTS;
                                func_80205C78();
                                func_80242074();
                                SetMenuBackdrop(MENU_BACKDROP_STILL);
                                func_801F4AA4(FALSE);
                            } else {
                                g_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                                func_80242074();
                                SetMenuBackdrop(MENU_BACKDROP_STILL);
                                if (g_GameMode == GAME_MODE_TOURNAMENT && g_HoleIndex == TOURNAMENT_HALF_TURN_HOLE) {
                                    ShowHalfTurn();
                                } else {
                                    StartWipe(WIPE_CLOSING);
                                }
                            }
                        }
                        break;
                    case HOLE_END_STEP_FINAL_RESULTS:
                        if (func_801F8A30(&g_MergedController)) {
                            if (HasWonFinalChallenge() || IsGameFlagSet(GAME_FLAG_SHOW_STAFF_ROLL)) {
                                g_HoleEndStep = HOLE_END_STEP_STAFF_ROLL;
                            } else {
                                g_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                            }
                            StartWipe(WIPE_CLOSING);
                            SetMenuBackdrop(MENU_BACKDROP_STILL);
                            g_PlayExit = PLAY_EXIT_UNK3;
                        }
                        break;
                    case HOLE_END_STEP_STAFF_ROLL:
                        if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
                            g_HoleEndStep = HOLE_END_STEP_LOAD_NEXT_HOLE;
                            StartWipe(WIPE_OPENING);
                            ClearTextSlots();
                            g_HoleEndStep = HOLE_END_STEP_START;
                            func_80216B74(TRUE);
                            g_Scene = SCENE_SWAPPING_MODULES;
                            g_NextScene = SCENE_STAFF_ROLL;
                            g_SceneModuleToUnload = SCENE_MODULE_PLAY;
                            g_SceneModuleToLoad = SCENE_MODULE_STAFF_ROLL;
                            UnloadSubModule();
                            func_8005DC50();
                            StopLoaderThread();
                            g_PlayExit = PLAY_EXIT_NONE;
                            SetMenuBackdrop(MENU_BACKDROP_NONE);
                            SetFramebufferCount(ROUND_FRAMEBUFFER_COUNT);
                            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                        }
                        break;
                    case HOLE_END_STEP_WIPE_OUT:
                        if (GetWipeProgress() == WIPE_CLOSED_PROGRESS && IsHalfTurnDismissed(&g_MergedController)) {
                            ForceStopRumble();
                            g_HoleEndStep = HOLE_END_STEP_LOAD_NEXT_HOLE;
                            if (g_PlayExit != PLAY_EXIT_NONE) {
                                StartWipe(WIPE_OPENING);
                                ClearTextSlots();
                                g_HoleEndStep = HOLE_END_STEP_START;
                                func_80216B74(TRUE);
                                g_Scene = SCENE_SWAPPING_MODULES;
                                g_NextScene = SCENE_MENU;
                                g_SceneModuleToUnload = SCENE_MODULE_PLAY;
                                g_SceneModuleToLoad = SCENE_MODULE_MENU;
                                UnloadSubModule();
                                switch (g_PlayExit) {
                                    case PLAY_EXIT_QUIT:
                                        func_8005DD10();
                                        break;
                                    case PLAY_EXIT_UNK1:
                                        func_8005DAFC();
                                        break;
                                    case PLAY_EXIT_UNK2:
                                        func_8005DD44();
                                        break;
                                    case PLAY_EXIT_UNK3:
                                        func_8005DC50();
                                        break;
                                    case PLAY_EXIT_UNK4:
                                        func_8005DDAC();
                                        break;
                                    case PLAY_EXIT_RING_SHOT:
                                        func_8005DD78();
                                        break;
                                }
                                StopLoaderThread();
                                g_PlayExit = PLAY_EXIT_NONE;
                                SetMenuBackdrop(MENU_BACKDROP_NONE);
                                SetFramebufferCount(ROUND_FRAMEBUFFER_COUNT);
                                SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                            } else {
                                RequestSubModule();
                            }
                        }
                        break;
                    case HOLE_END_STEP_LOAD_NEXT_HOLE:
                        if (g_HasLoaderFinished == TRUE && IsSubModuleLoaded()) {
                            g_GameState.unk0 = GAME_STATE_HOLE_INTRO;
                            SetGameFlag(GAME_FLAG_HOLE_LOADED);
                            memset(&g_RoundHud, 0, sizeof(RoundHud));
                            SetMenuBackdrop(MENU_BACKDROP_NONE);
                            g_PauseScreen = PAUSE_SCREEN_NONE;
                            SetFramebufferCount(ROUND_FRAMEBUFFER_COUNT);
                            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                            StartHoleIntro();
                            StartHoleCamerasAndSong();
                            if (g_GameMode == GAME_MODE_RING_SHOT) {
                                func_80242384(TRUE);
                            }
                            StartWipe(WIPE_OPENING);
                            g_PlayStateFrameCount = 0;
                            g_HoleEndStep = HOLE_END_STEP_START;
                        }
                        break;
                    case HOLE_END_STEP_QUIT:
                        if (IsCharacterSlotBusy(PLAYER_CHARACTER_SLOT)) {
                            FreeCharacterSlot(PLAYER_CHARACTER_SLOT);
                            break;
                        }
                        StartWipe(WIPE_CLOSING);
                        g_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                        break;
                }
                break;
        }
        again = repeat;
    } while (again);
}
#else
INCLUDE_ASM("asm/nonmatchings/play", UpdatePlay);
#endif

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
 * and the tiles around it in g_IsGroundTileNearby
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
                            g_IsGroundTileNearby[(row + i) * GROUND_TILE_COLUMN_COUNT + (column + j)] = TRUE;
                        }
                    }
                }
            }
        }
    }
}

void ClearNearbyGroundTiles(void) {
    bzero(g_IsGroundTileNearby, sizeof(g_IsGroundTileNearby));
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

INCLUDE_ASM("asm/nonmatchings/play", SetGroundCombineMode);

INCLUDE_ASM("asm/nonmatchings/play", DrawGround);

