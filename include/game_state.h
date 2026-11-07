#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <ultra64.h>

#include "bets.h"
#include "course.h"
#include "course_space.h"
#include "hole_conditions.h"
#include "language.h"
#include "player.h"

/*
 * The current game: its settings, the step it is on and the hole
 * being played
 */

/* GameState.holeSelection values; code tests the nines as bits */
#define HOLE_SELECTION_ALL 0
#define HOLE_SELECTION_BACK_NINE (1 << 1)
#define HOLE_SELECTION_FRONT_NINE (1 << 2)

/* Indices of GameState.carriedPots */
#define CARRIED_POT_LONGEST_DRIVE 0
#define CARRIED_POT_NEAREST_PIN 1
#define CARRIED_POT_HOLE 2
#define CARRIED_POT_COUNT 3

/* GameState.step's values: the step that UpdatePlay is on */
enum GameStateStep {
    GAME_STATE_START,
    GAME_STATE_LOAD_HOLE,
    GAME_STATE_WAIT_FOR_HOLE,
    GAME_STATE_HOLE_INTRO,
    GAME_STATE_FLYOVER,
    /* Never set: would only count frames */
    GAME_STATE_UNUSED_IDLE,
    /* Taking turns. It and GAME_STATE_REPEATING_HOLE play a single ball, rather
     * than one for each player */
    GAME_STATE_TAKING_TURNS,
    GAME_STATE_HOLE_END,
    GAME_STATE_SLOTS, /* Spinning the club slots before the hole */
    /* Taking turns on SetUpOneHole's hole, which starts again once over */
    GAME_STATE_REPEATING_HOLE = 11,
    GAME_STATE_END_INTRO,
    GAME_STATE_RELOAD_HOLE
};

typedef s32 GameStateStep;

/* Whether a game state step shows the whole hole, rather than a turn */
#define IsHoleOverviewStep(step) ((step) == GAME_STATE_HOLE_INTRO || (step) == GAME_STATE_FLYOVER)
/* Whether a game state step has the players taking turns, on any hole */
#define IsTurnTakingStep(step) \
    ((step) == GAME_STATE_TAKING_TURNS || (step) == GAME_STATE_REPEATING_HOLE)

/* GameState.replayKind's values: the replay that leaving the menus starts */
enum ReplayKind {
    REPLAY_KIND_NONE,
    REPLAY_KIND_BEST_SHOT,
    REPLAY_KIND_RING_SHOT
};

/*
 * GameState.isRoundContinued's value once a saved round is continued; the
 * players, cup and rings then stay as saved
 */
#define ROUND_CONTINUED 2

/*
 * The current game's settings and place, which a saved game keeps for it
 * to be continued with.
 */
typedef struct GameState {
    /* 0x00 */ GameStateStep step;
    /* 0x04 */ u32 gameMode; /* A GAME_MODE_* value */
    /* 0x08 */ s32 playerCount;
    /* 0x0C */ CourseIndex courseIndex;
    /* 0x10 */ s32 holeIndex;
    /* Only ever cleared, beside holeIndex as each round is set up */
    /* 0x14 */ s32 unusedHoleValue;
    /* 0x18 */ Boolean isSuddenDeath;
    /* 0x1C */ s32 unusedAfterSuddenDeath;
    /* 0x20 */ s32 currentPlayerIndex;
    /* 0x24 */ s32 holeSelection; /* A HOLE_SELECTION_* value */
    /* 0x28 */ s32 stopwatchFrameCount; /* The time the stopwatch modes show, in frames */
    /* 0x2C */ s32 roundSeed; /* Draws the cups, the tournament field's scores
                                and the computers' luck */
    /* 0x30 */ s8 isRoundContinued; /* ROUND_CONTINUED or FALSE */
    /* 0x31 */ s8 replayKind; /* The replay that leaving the menus starts */
    /* 0x32 */ s8 betRates[BET_COUNT]; /* What each BET_* bet is worth; the
                                          menus also set one past the end */
    /* 0x39 */ u8 unusedAfterBetRates;
    /* 0x3A */ s8 isMiniGolf; /* Keeps billboards near the course's edge, and
                                 tilts them less */
    /* 0x3B */ s8 isClubTournament; /* Entered with a tournament code */
    /* Set when a player chooses a character brought over from a Game Boy;
     * nothing reads it */
    /* 0x3C */ u8 isImportedCharacterChosen;
    /* 0x3D */ s8 saveCount; /* Raised by one, up to 99, when the game is saved */
    /* 0x3E */ s16 carriedPots[CARRIED_POT_COUNT]; /* The pots that tied holes carry over */
} GameState; /* size = 0x44 */

/*
 * The hole being played: where its cup and tee are and how it plays. The
 * records keep a copy, and a saved game keeps one to resume the hole.
 */
typedef struct HoleSetup {
    /* 0x00 */ CoursePoint cupPosition;
    /* 0x0C */ CoursePoint teePosition; /* Where each player starts the hole */
    /* 0x18 */ HoleConditions conditions;
    /* 0x2C */ s8 par;
    /* 0x2D */ s8 nextCupIndex;
    /* 0x2E */ u8 unusedAfterNextCup[0x34 - 0x2E];
} HoleSetup; /* size = 0x34 */

extern HoleSetup g_HoleSetup;

extern GameState g_GameState;

/* The game state and players as BackUpPlayState last copied them */
extern GameState g_BackedUpGameState;
extern Player g_BackedUpPlayers[MAX_PLAYER_COUNT];

#endif
