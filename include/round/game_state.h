/*
 * The current game: its settings, the step it is on and the hole
 * being played
 */

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <ultra64.h>

#include "course/course.h"
#include "game/scene.h"
#include "round/bets.h"
#include "round/player.h"
#include "system/c_language.h"

// GameState.holeSelection values; code tests the nines as bits
#define HOLE_SELECTION_ALL 0
#define HOLE_SELECTION_BACK_NINE (1 << 1)
#define HOLE_SELECTION_FRONT_NINE (1 << 2)

// Indices of GameState.carriedPots
#define CARRIED_POT_LONGEST_DRIVE 0
#define CARRIED_POT_NEAREST_PIN 1
#define CARRIED_POT_HOLE 2
#define CARRIED_POT_COUNT 3

// GameState.step's values: the step that UpdatePlay is on
enum GameStateStep {
    GAME_STATE_START,
    GAME_STATE_LOAD_HOLE,
    GAME_STATE_WAIT_FOR_HOLE,
    GAME_STATE_HOLE_INTRO,
    GAME_STATE_FLYOVER,
    // Never set: would only count frames
    GAME_STATE_UNUSED_IDLE,
    // Taking turns. It and GAME_STATE_REPEATING_HOLE play a single ball, rather
    // than one for each player
    GAME_STATE_TAKING_TURNS,
    GAME_STATE_HOLE_END,
    GAME_STATE_SLOTS, // Spinning the club slots before the hole
    // Taking turns on SetUpOneHole's hole, which starts again once over
    GAME_STATE_REPEATING_HOLE = 11,
    GAME_STATE_END_INTRO,
    GAME_STATE_RELOAD_HOLE
};

typedef s32 GameStateStep;

// Whether a game state step shows the whole hole, rather than a turn
#define IsHoleOverviewStep(step) ((step) == GAME_STATE_HOLE_INTRO || (step) == GAME_STATE_FLYOVER)
// Whether a game state step has the players taking turns, on any hole
#define IsTurnTakingStep(step) \
    ((step) == GAME_STATE_TAKING_TURNS || (step) == GAME_STATE_REPEATING_HOLE)

// GameState.replayKind's values: the replay that leaving the menus starts
enum ReplayKind {
    REPLAY_KIND_NONE,
    REPLAY_KIND_BEST_SHOT,
    REPLAY_KIND_RING_SHOT
};

typedef s8 ReplayKind;

// GameState.isRoundContinued's value once a saved round is continued; the
// players, cup and rings then stay as saved
#define ROUND_CONTINUED 2

// The current game's settings and place, which a saved game keeps for it
// to be continued with.
typedef struct GameState {
    GameStateStep step;
    GameMode gameMode;
    s32 playerCount;
    CourseIndex courseIndex;
    s32 holeIndex;
    // Only ever cleared, beside holeIndex as each round is set up
    s32 unusedHoleValue;
    Boolean isSuddenDeath;
    s32 unusedAfterSuddenDeath;
    s32 currentPlayerIndex;
    s32 holeSelection; // A HOLE_SELECTION_ value
    s32 stopwatchFrameCount; // The time the stopwatch modes show, in frames
    s32 roundSeed; // Draws the cups, the tournament field's scores and the computers' luck
    Boolean8 isRoundContinued; // ROUND_CONTINUED or FALSE
    ReplayKind replayKind; // The replay that leaving the menus starts
    s8 betRates[BET_COUNT]; // What each bet is worth; the menus also set one past the end
    u8 unusedAfterBetRates;
    Boolean8 isMiniGolf; // Keeps billboards near the course's edge, and tilts them less
    Boolean8 isClubTournament; // Entered with a tournament code
    // Set when a player chooses a character brought over from a Game Boy;
    // nothing reads it
    UBoolean8 isImportedCharacterChosen;
    s8 saveCount; // Raised by one, up to 99, when the game is saved
    s16 carriedPots[CARRIED_POT_COUNT]; // The pots that tied holes carry over
} GameState;

extern GameState g_GameState;

// The game state and players as BackUpPlayState last copied them
extern GameState g_BackedUpGameState;
extern Player g_BackedUpPlayers[PLAYER_COUNT_MAX];

#endif
