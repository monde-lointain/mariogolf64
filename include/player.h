#ifndef PLAYER_H
#define PLAYER_H

#include <ultra64.h>

#include "bets.h"
#include "club.h"
#include "course.h"
#include "course_space.h"
#include "turn.h"

/*
 * The golfers in the round: each one's state, ball and results so far
 */

#define MAX_PLAYER_COUNT 4

/* The face of each player's character, and its palette in the player's colors
 */
extern u8* g_PlayerFaceImages[MAX_PLAYER_COUNT];
extern u16* g_PlayerFacePalettes[MAX_PLAYER_COUNT];

/* The most rings that a hole of Ring Shot has */
#define RING_COUNT 3

/* Values of the players' ringStates. Above RING_STATE_SHOWN, a state counts
   the frames of the ring fading out, while sparkles circle it */
#define RING_STATE_NONE 0
#define RING_STATE_SHOWN 1
/* The state a ring takes when the ball passes through it, the first frame of
 * its fade */
#define RING_STATE_PASSED 2
#define RING_STATE_FADED 32

/* The contest distances that are not measured yet, or do not count */
#define UNCOUNTED_DISTANCE (-1)
#define UNMEASURED_DISTANCE (-2)

/* PlayerHole.putts when they were not counted */
#define UNCOUNTED_PUTTS (-1)

/* A player's results on one hole */
typedef struct PlayerHole {
  /* 0x0 */ s8 strokes;
  /* 0x1 */ s8 putts; /* Or UNCOUNTED_PUTTS */
} PlayerHole; /* size = 0x2 */

/* The size of Player.name, with its terminator */
#define PLAYER_NAME_SIZE 12

/*
 * Player.state's values, which the game's messages call the PlayerState. A
 * saved game keeps PLAYER_STATE_WAITING and PLAYER_STATE_HOLE_DONE, restarts
 * STARTING_TURN, AIMING, UNK3 and PREPARING_AIM as STARTING_TURN, and calls
 * the rest illegal.
 */
enum PlayerState {
  /* Sets the turn up */
  PLAYER_STATE_STARTING_TURN,
  /* Shows the hole on its first turn */
  PLAYER_STATE_HOLE_INTRO,
  /* Hides the gauge's markers */
  PLAYER_STATE_AIMING,
  /*
   * Never set, but treated as aiming: the shot preview counts it as before
   * the power and impact are set, and a saved game resumes it at the turn's
   * start
   */
  PLAYER_STATE_UNUSED_AIMING,
  /* Hides the gauge's markers */
  PLAYER_STATE_SETTING_POWER,
  /* Hides the gauge's markers */
  PLAYER_STATE_SETTING_IMPACT,
  PLAYER_STATE_STARTING_SWING,
  /* Swings until the ball is launched */
  PLAYER_STATE_SWINGING = 8,
  PLAYER_STATE_BALL_IN_FLIGHT,
  /* Judges where the ball came to rest */
  PLAYER_STATE_BALL_AT_REST,
  /* Starts the character's reaction to ending the hole */
  PLAYER_STATE_STARTING_REACTION,
  PLAYER_STATE_REACTING,
  /* Asks what to do next, in training and the ring shot */
  PLAYER_STATE_CHOOSING = 15,
  PLAYER_STATE_ENDING_TURN,
  /*
   * PlayContestMovies waits for every player to be in this state or
   * PLAYER_STATE_HOLE_DONE. Players in either may make their characters call
   * out during another player's turn.
   */
  PLAYER_STATE_WAITING,
  /* Records the hole and shows its banner */
  PLAYER_STATE_FINISHING_HOLE,
  PLAYER_STATE_HOLE_DONE = 20,
  /* Waits for a replayed shot and the HUD before aiming */
  PLAYER_STATE_PREPARING_AIM,
  /* Picks the next player who went out on the hole */
  PLAYER_STATE_FINDING_OUT_PLAYER,
  /* Shows that player losing */
  PLAYER_STATE_SHOWING_OUT_PLAYER
};

/* Where a player stands on the current hole */
enum HoleStatus {
  HOLE_STATUS_PLAYING,
  HOLE_STATUS_HOLED_OUT,
  /* Put out by the group's result, until PLAYER_STATE_FINDING_OUT_PLAYER
   * shows it and makes it HOLE_STATUS_OUT */
  HOLE_STATUS_NEWLY_OUT,
  /* Out of the hole without holing out */
  HOLE_STATUS_OUT
};

/*
 * Player.powerShotCount's value when power shots are not counted; the HUD
 * then shows no count
 */
#define POWER_SHOTS_UNLIMITED (-1)

/* A golfer in the round, human or computer, and their round so far */
typedef struct Player {
  /* 0x00 */ char name[PLAYER_NAME_SIZE];
  /* 0x0C */ s32 state; /* A PLAYER_STATE_* value */
  /* 0x10 */ CoursePoint ballPosition;
  /* 0x1C */ s8 ballSurface; /* A SURFACE_* value */
  /* 0x1D */ s8 characterIndex; /* Also the index of the character's club set */
  /* 0x1E */ s8 isComputer; /* Played by the computer, with no controller */
  /* Set to -9 as the computer starts planning a shot; nothing reads it */
  /* 0x1F */ s8 ignoredPlanMark;
  /* 0x20 */ u8 unusedBeforeLefty[0x22 - 0x20];
  /* 0x22 */ s8 isLefty; /* Swings left-handed, so stands mirrored */
  /* 0x23 */ s8 strokeCount; /* On the current hole */
  /* 0x24 */ s8 puttCount; /* Strokes from the green on the current hole */
  /* 0x25 */ s8 teeOrder; /* From 0, the player's turn on the next tee */
  /* 0x26 */ s8 holeStatus; /* A HOLE_STATUS_* value */
  /* 0x27 */ s8 controllerIndex; /* Into g_PlayerControllerPorts, when below
                                    MAXCONTROLLERS */
  /* 0x28 */ s8 powerShotCount; /* Left in the round; POWER_SHOTS_UNLIMITED
                                 in training and replays */
  /* 0x29 */ s8 handicap; /* Gives strokes on the holes it ranks within */
  /* 0x2A */ s8 soundPan;
  /* 0x2B */ PlayerHole holes[COURSE_HOLE_COUNT];
  /* 0x4F */ u8 unusedAfterHoles[0x52 - 0x4F];
  /* 0x52 */ s16 clubs[CLUB_SLOT_COUNT]; /* Indices into g_Clubs, by bag slot */
  /* 0x72 */ u8 unusedAfterClubs[0x74 - 0x72];
  /* 0x74 */ s32
      stopwatchFrameCount; /* The player's time in the stopwatch modes */
  /* 0x78 */ s8 ringStates[RING_COUNT]; /* RING_STATE_* values, for Ring Shot */
  /* 0x7B */ s8 paletteIndex; /* Which of the character's palettes it wears */
  /* 0x7C */ s8 gameBoyCharacterIndex; /* For the Game Boy characters, from
                                          CHARACTER_KID on */
  /* 0x7D */ u8 unusedAfterGameBoyIndex[3];
  /* 0x80 */ s32 driveDistance; /* UNMEASURED_DISTANCE, or UNCOUNTED_DISTANCE */
  /* 0x84 */ s32 pinDistance; /* UNMEASURED_DISTANCE, or UNCOUNTED_DISTANCE */
  /* 0x88 */ s32 noiseSeed; /* Added into the ball's flight noise, and kept
                               in the replay records */
  /* 0x8C */ s8 gridColor; /* GRID_COLOR_*, for the grid drawn over the green */
  /* 0x8D */ u8 unusedAfterGridColor;
  /* 0x8E */ s16
      betTallies[BET_COUNT]; /* What the player has won on each BET_* bet */
  /* 0x9C */ s8 hasBeenInBunker; /* On the hole being played */
  /* 0x9D */ u8 unusedAfterBunkerVisit[3];
  /* 0xA0 */ s32
      lastDriveDistance; /* The driveDistance that RecordDrive took */
  /* 0xA4 */ s32 points; /* Earned in the round, towards opening courses */
  /* Only ever cleared, as the ball is placed at rest */
  /* 0xA8 */ s16 unusedRestValue;
  /* 0xAA */ s8 notableShotCounts[SHOT_KIND_COUNT]; /* By ShotKind */
  /* 0xB6 */ u8 unusedAfterShotCounts[0xB8 - 0xB6];
} Player; /* size = 0xB8 */

/* The current game's, which a saved game is loaded into and stored from */
extern Player g_Players[MAX_PLAYER_COUNT];

#endif
