/*
 * The golfers in the round: each one's state, ball and results so far
 */

#ifndef PLAYER_H
#define PLAYER_H

#include <ultra64.h>

#include "audio/sound.h"
#include "characters/character.h"
#include "course/course.h"
#include "course/course_space.h"
#include "course/surface.h"
#include "round/bets.h"
#include "shot/club.h"
#include "system/asset.h"
#include "system/c_language.h"

#define PLAYER_COUNT_MAX 4

// The face of each player's character, and its palette in the player's colors
extern ImageAsset *g_PlayerFaceImages[PLAYER_COUNT_MAX];
extern PaletteColor *g_PlayerFacePalettes[PLAYER_COUNT_MAX];

// The most rings that a hole of Ring Shot has
#define RING_COUNT 3

// Values of the players' ringStates. Above RING_STATE_SHOWN, a state counts
// the frames of the ring fading out, while sparkles circle it
enum RingState {
    RING_STATE_NONE,
    RING_STATE_SHOWN,
    // The state a ring takes when the ball passes through it, the first frame
    // of its fade
    RING_STATE_PASSED,
    RING_STATE_FADED = 32
};

typedef s32 RingState;
typedef s8 RingState8;

// The contest distances that are not measured yet, or do not count
#define UNCOUNTED_DISTANCE (-1)
#define UNMEASURED_DISTANCE (-2)

// PlayerHole.putts when they were not counted
#define UNCOUNTED_PUTTS (-1)

// A player's results on one hole
typedef struct PlayerHole {
    s8 strokeCount;
    s8 puttCount; // Or UNCOUNTED_PUTTS
} PlayerHole;

// The size of Player.name, with its terminator
#define PLAYER_NAME_SIZE 12

// Player.state's values, which the game's messages call the PlayerState. A
// saved game keeps PLAYER_STATE_WAITING and PLAYER_STATE_HOLE_DONE, restarts
// STARTING_TURN, AIMING, UNK3 and PREPARING_AIM as STARTING_TURN, and calls
// the rest illegal.
enum PlayerState {
    // Sets the turn up
    PLAYER_STATE_STARTING_TURN,
    // Shows the hole on its first turn
    PLAYER_STATE_HOLE_INTRO,
    // Hides the gauge's markers
    PLAYER_STATE_AIMING,
    // Never set, but treated as aiming: the shot preview counts it as before
    // the power and impact are set, and a saved game resumes it at the turn's
    // start
    PLAYER_STATE_UNUSED_AIMING,
    // Hides the gauge's markers
    PLAYER_STATE_SETTING_POWER,
    // Hides the gauge's markers
    PLAYER_STATE_SETTING_IMPACT,
    PLAYER_STATE_STARTING_SWING,
    // Swings until the ball is launched
    PLAYER_STATE_SWINGING = 8,
    PLAYER_STATE_BALL_IN_FLIGHT,
    // Judges where the ball came to rest
    PLAYER_STATE_BALL_AT_REST,
    // Starts the character's reaction to ending the hole
    PLAYER_STATE_STARTING_REACTION,
    PLAYER_STATE_REACTING,
    // Asks what to do next, in training and the ring shot
    PLAYER_STATE_CHOOSING = 15,
    PLAYER_STATE_ENDING_TURN,
    // PlayContestMovies waits for every player to be in this state or
    // PLAYER_STATE_HOLE_DONE. Players in either may make their characters call
    // out during another player's turn.
    PLAYER_STATE_WAITING,
    // Records the hole and shows its banner
    PLAYER_STATE_FINISHING_HOLE,
    PLAYER_STATE_HOLE_DONE = 20,
    // Waits for a replayed shot and the HUD before aiming
    PLAYER_STATE_PREPARING_AIM,
    // Picks the next player who went out on the hole
    PLAYER_STATE_FINDING_OUT_PLAYER,
    // Shows that player losing
    PLAYER_STATE_SHOWING_OUT_PLAYER
};

typedef s32 PlayerState;

// Where a player stands on the current hole
enum HoleStatus {
    HOLE_STATUS_PLAYING,
    HOLE_STATUS_HOLED_OUT,
    // Put out by the group's result, until PLAYER_STATE_FINDING_OUT_PLAYER
    // shows it and makes it HOLE_STATUS_OUT
    HOLE_STATUS_NEWLY_OUT,
    // Out of the hole without holing out
    HOLE_STATUS_OUT
};

typedef s8 HoleStatus;

// Player.powerShotCount's value when power shots are not counted; the HUD
// then shows no count
#define POWER_SHOTS_UNLIMITED (-1)

// Player.paletteIndex values, for the C button that chooses each: on the
// character select, the A button wears the held one and the faces preview
// it, and on the club tournament's start, the player wears the one held
enum CButtonPalette {
    NO_PALETTE = -1,
    UP_C_PALETTE,
    LEFT_C_PALETTE,
    DOWN_C_PALETTE,
    RIGHT_C_PALETTE
};

typedef s32 CButtonPalette;
typedef s8 CButtonPalette8;

// SAVE_OPTION_GRID_COLOR values
enum GridColor {
    GRID_COLOR_WHITE,
    GRID_COLOR_BLACK
};

typedef s8 GridColor;
#define GRID_COLOR_FLIP 1

// The kinds of notable shot that earn Course Points. Most are named after the
// banner that the shot shows.
enum NotableShot {
    NOTABLE_SHOT_GREAT, // Onto the green in three or more under par
    NOTABLE_SHOT_EXCELLENT, // Close to the cup, on the green in two under par
    NOTABLE_SHOT_PIN_SHOT, // Hits the pin
    NOTABLE_SHOT_CHIP_IN, // Holed from off the green
    NOTABLE_SHOT_LONG_CHIP_IN, // Holed from far off the green
    NOTABLE_SHOT_UNUSED, // Never counted, but scored as a pin shot
    NOTABLE_SHOT_FLAG_SHOT, // Hits the flag
    NOTABLE_SHOT_HOLE_IN_ONE,
    NOTABLE_SHOT_ALBATROSS,
    NOTABLE_SHOT_EAGLE,
    NOTABLE_SHOT_BIRDIE,
    // Crosses water without stopping in it, the debug text's "WaterCut"
    NOTABLE_SHOT_WATER_CUT,
    NOTABLE_SHOT_COUNT
};

typedef s32 NotableShot;

// A golfer in the round, human or computer, and their round so far
typedef struct Player {
    char name[PLAYER_NAME_SIZE];
    PlayerState state;
    CoursePoint ballPosition;
    Surface8 ballSurface;
    CharacterIndex8 characterIndex; // Also the index of the character's club set
    Boolean8 isComputer; // Played by the computer, with no controller
    // Set to -9 as the computer starts planning a shot; nothing reads it
    s8 ignoredPlanMark;
    u8 unusedBeforeLefty[0x22 - 0x20];
    Boolean8 isLefty; // Swings left-handed, so stands mirrored
    s8 strokeCount; // On the current hole
    s8 puttCount; // Strokes from the green on the current hole
    s8 teeOrder; // From 0, the player's turn on the next tee
    HoleStatus holeStatus;
    s8 controllerIndex; // Into g_PlayerControllerPorts, when below MAXCONTROLLERS
    s8 powerShotCount; // Left in the round; POWER_SHOTS_UNLIMITED in training and replays
    s8 handicap; // Gives strokes on the holes it ranks within
    SoundPan8 soundPan;
    PlayerHole holes[COURSE_HOLE_COUNT];
    u8 unusedAfterHoles[0x52 - 0x4F];
    ClubIndex16 clubs[CLUB_SLOT_COUNT]; // Indices into g_Clubs, by bag slot
    u8 unusedAfterClubs[0x74 - 0x72];
    s32 stopwatchFrameCount; // The player's time in the stopwatch modes
    RingState8 ringStates[RING_COUNT]; // For Ring Shot
    CButtonPalette8 paletteIndex; // Which of the character's palettes it wears
    s8 gameBoyCharacterIndex; // For the Game Boy characters, from CHARACTER_KID on
    u8 unusedAfterGameBoyIndex[3];
    s32 driveDistance; // UNMEASURED_DISTANCE, or UNCOUNTED_DISTANCE
    s32 pinDistance; // UNMEASURED_DISTANCE, or UNCOUNTED_DISTANCE
    s32 noiseSeed; // Added into the ball's flight noise, and kept in the replay records
    GridColor gridColor; // For the grid drawn over the green
    u8 unusedAfterGridColor;
    s16 betTallies[BET_COUNT]; // What the player has won on each bet
    Boolean8 hasBeenInBunker; // On the hole being played
    u8 unusedAfterBunkerVisit[3];
    s32 lastDriveDistance; // The driveDistance that RecordDrive took
    s32 points; // Earned in the round, towards opening courses
    // Only ever cleared, as the ball is placed at rest
    s16 unusedRestValue;
    s8 notableShotCounts[NOTABLE_SHOT_COUNT]; // By ShotKind
    u8 unusedAfterShotCounts[0xB8 - 0xB6];
} Player;

// The current game's, which a saved game is loaded into and stored from
extern Player g_Players[PLAYER_COUNT_MAX];

#endif
