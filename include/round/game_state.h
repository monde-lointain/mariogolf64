#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <ultra64.h>

#include "course/course.h"
#include "game/scene.h"
#include "round/bets.h"
#include "round/player.h"
#include "system/c_language.h"

#define HOLE_SELECTION_ALL 0
#define HOLE_SELECTION_BACK_NINE (1 << 1)
#define HOLE_SELECTION_FRONT_NINE (1 << 2)

#define CARRIED_POT_LONGEST_DRIVE 0
#define CARRIED_POT_NEAREST_PIN 1
#define CARRIED_POT_HOLE 2
#define CARRIED_POT_COUNT 3

enum GameStateStep {
    GAME_STATE_START,
    GAME_STATE_LOAD_HOLE,
    GAME_STATE_WAIT_FOR_HOLE,
    GAME_STATE_HOLE_INTRO,
    GAME_STATE_FLYOVER,
    GAME_STATE_UNUSED_IDLE,
    GAME_STATE_TAKING_TURNS,
    GAME_STATE_HOLE_END,
    GAME_STATE_SLOTS,
    GAME_STATE_REPEATING_HOLE = 11,
    GAME_STATE_END_INTRO,
    GAME_STATE_RELOAD_HOLE
};

typedef s32 GameStateStep;

#define IsHoleOverviewStep(step) ((step) == GAME_STATE_HOLE_INTRO || (step) == GAME_STATE_FLYOVER)
#define IsTurnTakingStep(step) \
    ((step) == GAME_STATE_TAKING_TURNS || (step) == GAME_STATE_REPEATING_HOLE)

enum ReplayKind {
    REPLAY_KIND_NONE,
    REPLAY_KIND_BEST_SHOT,
    REPLAY_KIND_RING_SHOT
};

typedef s8 ReplayKind;

#define ROUND_CONTINUED 2

typedef struct GameState {
    GameStateStep step;
    GameMode gameMode;
    s32 playerCount;
    CourseIndex courseIndex;
    s32 holeIndex;
    s32 unusedHoleValue;
    Boolean isSuddenDeath;
    s32 unusedAfterSuddenDeath;
    s32 currentPlayerIndex;
    s32 holeSelection;
    s32 stopwatchFrameCount;
    s32 roundSeed;
    Boolean8 isRoundContinued;
    ReplayKind replayKind;
    s8 betRates[BET_COUNT];
    u8 unusedAfterBetRates;
    Boolean8 isMiniGolf;
    Boolean8 isClubTournament;
    UBoolean8 isImportedCharacterChosen;
    s8 saveCount;
    s16 carriedPots[CARRIED_POT_COUNT];
} GameState;

extern GameState g_GameState;

extern GameState g_BackedUpGameState;
extern Player g_BackedUpPlayers[PLAYER_COUNT_MAX];

#endif
