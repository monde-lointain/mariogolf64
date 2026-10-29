#ifndef SAVE_H
#define SAVE_H

#include <ultra64.h>

#include "character.h"
#include "club.h"
#include "course.h"
#include "math_util.h"
#include "variables.h"

/* Marks save data that has been written at least once */
#define SAVE_MAGIC 0x12345678

#define SAVE_COPY_COUNT 2
#define RING_SHOT_RECORD_COUNT 5
#define RANKED_RECORD_COUNT 20
/* The games saved partway through, to continue later */
#define SAVED_GAME_COUNT 3
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

/*
 * The games kept for continuing by game mode rather than by slot, and the
 * players and records kept with them
 */
#define MODE_GAME_COUNT 3
#define MODE_PLAYER_COUNT 7
#define MODE_RECORD_COUNT 6

/*
 * Where each game mode keeps its game in the save's mode tables. Mode 4's
 * two records overlap mode 5's one.
 */
#define MODE4_GAME_INDEX 0
#define MODE4_PLAYER_INDEX 0
#define MODE4_RECORD_INDEX 0
#define MODE4_PLAYER_COUNT 2
#define MODE5_GAME_INDEX 1
#define MODE5_PLAYER_INDEX 2
#define MODE5_RECORD_INDEX 1
#define MODE5_PLAYER_COUNT 1
#define MODE0_GAME_INDEX 2
#define MODE0_PLAYER_INDEX 3
#define MODE0_RECORD_INDEX 2
#define MODE0_PLAYER_COUNT 4

/* Indices of SaveData's options */
#define SAVE_OPTION_MUSIC_OFF 0 /* The options screen's MUSIC set to OFF */
#define SAVE_OPTION_MONO 1 /* Plays the sound in mono instead of stereo */
/* Copied into each player's UnkB8.gridColor */
#define SAVE_OPTION_GRID_COLOR 2
/* The options screen's CAMERA set to SIMPLE instead of DYNAMIC */
#define SAVE_OPTION_SIMPLE_CAMERA 3
#define SAVE_OPTION_COUNT 4

/* SAVE_OPTION_GRID_COLOR values */
enum GridColor {
  GRID_COLOR_WHITE,
  GRID_COLOR_BLACK
};

/* Bits of MenuMemory's optionBits, each mirroring one of the saved options */
#define OPTION_BIT_MUSIC_OFF (1 << 0)
#define OPTION_BIT_MONO (1 << 1)
#define OPTION_BIT_SIMPLE_CAMERA (1 << 2)
/* A copy of MenuMemory's optionBits, from which the music code reads
 * OPTION_BIT_MONO */
extern u8 g_SoundOptionBits;

/* Results of reading the save data */
enum SaveReadResult {
  SAVE_READ_NEVER_WRITTEN = -2, /* Neither copy has the magic number */
  SAVE_READ_FAILED,
  SAVE_READ_FIRST_COPY = 1,
  SAVE_READ_SECOND_COPY
};

#define COURSE_COUNT 6
#define COURSE_HOLE_COUNT 18
#define HOLES_PER_NINE 9
#define RING_SHOT_COURSE_COUNT 6
#define RING_SHOT_HOLE_COUNT 6
#define SAVE_UNK1344_ENTRY_COUNT 18

/* GameState.holeSelection values; code tests the nines as bits */
#define HOLE_SELECTION_ALL 0
#define HOLE_SELECTION_BACK_NINE (1 << 1)
#define HOLE_SELECTION_FRONT_NINE (1 << 2)

/* The bets of a game, indexing GameState.betRates and UnkB8.betTallies */
#define BET_LONGEST_DRIVE 0 /* On marked par 5 holes */
#define BET_NEAREST_PIN 1 /* On par 3 holes */
#define BET_STROKES 2 /* Paid per stroke, between each pair of players */
/* Paid on the front nine, the back nine and the round */
#define BET_NASSAU 3
/* Settled on each hole; a tie carries the pot over */
#define BET_UNK4 4
#define BET_UNK5 5 /* Settled as BET_UNK4, when that bet is not set */
#define BET_LAS_VEGAS 6 /* Paid per stroke, between the two teams of two */
#define BET_COUNT 7
/* The rate of a bet that is not played */
#define UNSET_BET_RATE (-1)

/* The contest distances that are not measured yet, or do not count */
#define UNCOUNTED_DISTANCE -1
#define UNMEASURED_DISTANCE -2

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
  GAME_STATE_UNK5,
  /* Taking turns. It and GAME_STATE_UNK11 play a single ball, rather than one
   * for each player */
  GAME_STATE_UNK6,
  GAME_STATE_HOLE_END,
  GAME_STATE_SLOTS, /* Spinning the club slots before the hole */
  GAME_STATE_UNK11 = 11,
  GAME_STATE_END_INTRO,
  GAME_STATE_RELOAD_HOLE
};

typedef s32 GameStateStep;

/* Whether a game state step shows the whole hole, rather than a turn */
#define IsHoleOverviewStep(step) \
  ((step) == GAME_STATE_HOLE_INTRO || (step) == GAME_STATE_FLYOVER)

/* GameState.replayKind's values: the replay that leaving the menus starts */
enum ReplayKind {
  REPLAY_KIND_NONE,
  REPLAY_KIND_BEST_SHOT,
  REPLAY_KIND_RING_SHOT
};

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
  /* 0x14 */ s32 unk14;
  /* 0x18 */ Boolean isSuddenDeath;
  /* 0x1C */ s32 unk1C;
  /* 0x20 */ s32 currentPlayerIndex;
  /* 0x24 */ s32 holeSelection; /* A HOLE_SELECTION_* value */
  /* 0x28 */ s32
      stopwatchFrameCount; /* The time the stopwatch modes show, in frames */
  /* 0x2C */ s32 unk2C;
  /* 0x30 */ s8 unk30;
  /* 0x31 */ s8 replayKind; /* The replay that leaving the menus starts */
  /* 0x32 */ s8 betRates[BET_COUNT]; /* What each BET_* bet is worth; the
                                        menus also set one past the end */
  /* 0x39 */ u8 unk39;
  /* 0x3A */ s8 isMiniGolf; /* Keeps billboards near the course's edge, and
                               tilts them less */
  /* 0x3B */ s8 isClubTournament; /* Entered with a tournament code */
  /* 0x3C */ u8 unk3C;
  /* 0x3D */ s8 saveCount; /* Raised by one, up to 99, when the game is saved */
  /* 0x3E */ s16
      carriedPots[CARRIED_POT_COUNT]; /* The pots that tied holes carry over */
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
  /* 0x2E */ u8 unk2E[0x34 - 0x2E];
} HoleSetup; /* size = 0x34 */

/* A shot as a record keeps it, from the CurrentShot it was played with */
typedef struct RecordedShot {
  /* 0x0 */ BinaryAngle heading;
  /* 0x2 */ s8 powerGaugeFrame;
  /* 0x3 */ s8 impactGaugeFrame;
  /* 0x4 */ s8 shotMode;
  /* 0x5 */ s8 clubSlot;
  /* 0x6 */ s8 offsetX;
  /* 0x7 */ s8 offsetY;
} RecordedShot; /* size = 0x8 */

/*
 * A record's scoreKind: SCORE_KIND_RECORDED marks a score worth keeping, and
 * the low bits hold its ScoreKind
 */
#define SCORE_KIND_MASK 0xF
#define SCORE_KIND_UNK40 0x40
#define SCORE_KIND_RECORDED 0x80

enum ScoreKind {
  SCORE_KIND_HOLE_IN_ONE,
  SCORE_KIND_ALBATROSS,
  SCORE_KIND_EAGLE,
  SCORE_KIND_BIRDIE,
  SCORE_KIND_PAR, /* Recorded in Ring Shot alone */
  SCORE_KIND_COUNT
};

/* A hole that a player finished in few strokes, kept to be replayed */
typedef struct PlayRecord {
  /* 0x00 */ HoleSetup hole;
  /* 0x34 */ s32 gameMode;
  /* 0x38 */ s8 courseIndex;
  /* 0x39 */ s8 holeIndex;
  /* 0x3A */ u8
      scoreKind; /* What GetRecordedScoreKind returns for the strokes */
  /* 0x3B */ u8 characterIndex;
  /* 0x3C */ u8 unk3C;
  /* 0x3D */ u8 paletteIndex;
  /* 0x3E */ u8 strokeCount;
  /* 0x3F */ u8 unk3F;
  /* 0x40 */ s32 unk40;
  /* 0x44 */ RecordedShot
      shots[4]; /* RecordHoleOut writes a fifth past the end */
  /* 0x64 */ u8 unk64[0x68 - 0x64];
} PlayRecord; /* size = 0x68 */

/* A Ring Shot hole as a player played it, kept to be replayed */
typedef struct RingShotRecord {
  /* 0x00 */ HoleSetup hole;
  /* 0x34 */ s8 courseIndex;
  /* 0x35 */ s8 holeIndex;
  /* 0x36 */ s8
      scoreKind; /* What GetRecordedScoreKind returns for the strokes */
  /* 0x37 */ u8 characterIndex;
  /* 0x38 */ u8 unk38;
  /* 0x39 */ u8 paletteIndex;
  /* 0x3A */ u8 strokeCount;
  /* 0x3B */ u8 unk3B;
  /* 0x3C */ s32 unk3C;
  /* 0x40 */ RecordedShot shots[4]; /* RecordRingShotHole writes up to six */
  /* 0x60 */ u8 unk60[0x68 - 0x60];
} RingShotRecord; /* size = 0x68 */

extern HoleSetup g_HoleSetup;

/* Each player's last hole, and each player's last Ring Shot hole */
extern PlayRecord D_800FEDD8[MAX_PLAYER_COUNT];
extern RingShotRecord D_80105F40[MAX_PLAYER_COUNT];

/* The current round's statistics, for the course's records */
typedef struct RoundStats {
  /* Holes played from a bunker in par or fewer strokes, and in more */
  /* 0x00 */ s32 sandSaveCount;
  /* 0x04 */ s32 missedSandSaveCount;
  /* 0x08 */ s32 measuredDriveCount;
  /* 0x0C */ s32 longestDrive; /* Starts as the course's record */
  /* 0x10 */ s32 averagedDriveTotal;
  /* 0x14 */ s32 averagedDriveCount;
  /* Whether the drive was hit with the 1W */
  /* 0x18 */ Boolean isDriveAveraged;
} RoundStats; /* size = 0x1C */

/* Indices of UnkE98.hasPlace */
#define THIRD_PLACE_INDEX 0
#define SECOND_PLACE_INDEX 1
#define FIRST_PLACE_INDEX 2
#define TOURNAMENT_PLACE_COUNT 3

typedef struct UnkE98 {
  /* 0x00 */ s32 roundCount;
  /* 0x04 */ s32 bestScore; /* The lowest net score to par over a round */
  /* 0x08 */ s32 strokeTotal;
  /* 0x0C */ s32 longestDrive;
  /* 0x10 */ s32 averageDrive;
  /* 0x14 */ s32 averagedDriveCount;
  /* 0x18 */ s32
      measuredDriveTotal; /* The sum of RoundStats.measuredDriveCount */
  /* 0x1C */ s32 puttTotal;
  /* 0x20 */ s32 parOrBetterCount; /* Holes played in par or fewer strokes */
  /* 0x24 */ s32
      parOnCount; /* Holes whose green was reached with two strokes to spare */
  /* 0x28 */ s32 sandSaveCount; /* The sum of RoundStats.sandSaveCount */
  /* 0x2C */ s32
      missedSandSaveCount; /* The sum of RoundStats.missedSandSaveCount */
  /* In a tournament on the course */
  /* 0x30 */ Boolean hasPlace[TOURNAMENT_PLACE_COUNT];
  /* 0x3C */ s16
      characterBestScores[CHARACTER_COUNT]; /* bestScore, by character */
  /* 0x60 */ u8 hasCharacterBestScore[CHARACTER_COUNT];
  /* 0x72 */ u8 unk72[0x74 - 0x72];
} UnkE98; /* size = 0x74 */

typedef struct SavedGame {
  /* 0x00 */ Boolean isValid;
  /* 0x04 */ u8 unk4[4];
  /* 0x08 */ HoleSetup hole;
  /* 0x3C */ GameState state;
  /* 0x80 */ RoundStats roundStats;
  /* 0x9C */ u8 unk9C[4];
} SavedGame; /* size = 0xA0 */

/* PlayerHole.putts when they were not counted */
#define UNCOUNTED_PUTTS -1

/* A player's results on one hole */
typedef struct PlayerHole {
  /* 0x0 */ s8 strokes;
  /* 0x1 */ s8 putts; /* Or UNCOUNTED_PUTTS */
} PlayerHole; /* size = 0x2 */

/* The size of UnkB8.name, with its terminator */
#define PLAYER_NAME_SIZE 12

#define PLAYER_UNKAA_COUNT 12

/*
 * UnkB8.state's values, which the game's messages call the PlayerState. A
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
  /* Never set */
  PLAYER_STATE_UNK3,
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

typedef struct UnkB8 {
  /* 0x00 */ char name[PLAYER_NAME_SIZE];
  /* 0x0C */ s32 state; /* A PLAYER_STATE_* value */
  /* 0x10 */ CoursePoint ballPosition;
  /* 0x1C */ u8 ballSurface; /* A SURFACE_* value */
  /* 0x1D */ u8 characterIndex; /* Also the index of the character's club set */
  /* 0x1E */ s8 isComputer; /* Played by the computer, with no controller */
  /* 0x1F */ s8 unk1F;
  /* 0x20 */ u8 unk20[0x22 - 0x20];
  /* 0x22 */ s8 unk22;
  /* 0x23 */ s8 strokeCount; /* On the current hole */
  /* 0x24 */ s8 puttCount; /* Strokes from the green on the current hole */
  /* 0x25 */ s8 teeOrder; /* From 0, the player's turn on the next tee */
  /* 0x26 */ s8 holeStatus; /* A HOLE_STATUS_* value */
  /* 0x27 */ s8 controllerIndex; /* Into g_PlayerControllerPorts, when below
                                    MAXCONTROLLERS */
  /* 0x28 */ s8 unk28;
  /* 0x29 */ s8 handicap; /* Gives strokes on the holes it ranks within */
  /* 0x2A */ s8 soundPan;
  /* 0x2B */ PlayerHole holes[COURSE_HOLE_COUNT];
  /* 0x4F */ u8 unk4F[0x52 - 0x4F];
  /* 0x52 */ s16 clubs[CLUB_SLOT_COUNT]; /* Indices into g_Clubs, by bag slot */
  /* 0x72 */ u8 unk72[0x74 - 0x72];
  /* 0x74 */ s32
      stopwatchFrameCount; /* The player's time in the stopwatch modes */
  /* 0x78 */ s8 ringStates[RING_COUNT]; /* RING_STATE_* values, for Ring Shot */
  /* 0x7B */ u8 paletteIndex; /* Which of the character's palettes it wears */
  /* 0x7C */ s8 gameBoyCharacterIndex; /* For the Game Boy characters, from
                                          CHARACTER_KID on */
  /* 0x7D */ u8 unk7D[3];
  /* 0x80 */ s32 driveDistance; /* UNMEASURED_DISTANCE, or UNCOUNTED_DISTANCE */
  /* 0x84 */ s32 pinDistance; /* UNMEASURED_DISTANCE, or UNCOUNTED_DISTANCE */
  /* 0x88 */ s32 unk88;
  /* 0x8C */ u8 gridColor; /* GRID_COLOR_*, for the grid drawn over the green */
  /* 0x8D */ u8 unk8D;
  /* 0x8E */ s16
      betTallies[BET_COUNT]; /* What the player has won on each BET_* bet */
  /* 0x9C */ s8 hasBeenInBunker; /* On the hole being played */
  /* 0x9D */ u8 unk9D[3];
  /* 0xA0 */ s32
      lastDriveDistance; /* The driveDistance that RecordDrive took */
  /* 0xA4 */ s32 points; /* Earned in the round, towards opening courses */
  /* 0xA8 */ s16 unkA8;
  /* 0xAA */ s8 unkAA[PLAYER_UNKAA_COUNT];
  /* 0xB6 */ u8 unkB6[0xB8 - 0xB6];
} UnkB8; /* size = 0xB8 */

/* The best times kept for each course, with what was recorded alongside */
#define BEST_TIME_COUNT 3

#define SAVE_UNKAF0_ROW_COUNT 18
#define SAVE_UNKAF0_COLUMN_COUNT 18

/* UnkAF0.unk1 keeps a count of wins in these bits, and a flag in the top one */
#define WIN_COUNT_MASK 0x7F
#define UNKAF0_FLAG 0x80

typedef struct UnkAF0 {
  /* 0x0 */ u8 unk0;
  /* 0x1 */ u8 unk1;
} UnkAF0; /* size = 0x2 */

/*
 * Routines that update the birdie badges reach them through a pointer to
 * this; where it ends is unknown
 */
typedef struct Unk990 {
  /* 0x000 */ u8 unk0[0xF4];
  /* 0x0F4 */ s8
      birdieBadgeCounts[COURSE_COUNT]
                       [COURSE_HOLE_COUNT]; /* Nonzero holds the badge */
  /* 0x160 */ UnkAF0 unkAF0[SAVE_UNKAF0_ROW_COUNT][SAVE_UNKAF0_COLUMN_COUNT];
} Unk990; /* size = 0x3E8 */

typedef struct SaveData {
  /* 0x0000 */ u32 magic;
  /* 0x0004 */ u32
      longestDisplayListLength; /* In commands, over every frame drawn */
  /* 0x0008 */ u8 invertedBuildDate[0x20]; /* The writing build's date, with
                                              each bit inverted */
  /* 0x0028 */ s8 options[SAVE_OPTION_COUNT];
  /* 0x002C */ u8 unk2C[0x38 - 0x2C];
  /* 0x0038 */ SavedGame modeGames[MODE_GAME_COUNT];
  /* 0x0218 */ UnkB8 modePlayers[MODE_PLAYER_COUNT];
  /* 0x0720 */ PlayRecord modeRecords[MODE_RECORD_COUNT];
  /* 0x0990 */ Unk990 unk990;
  /* 0x0D78 */ s32 unkD78[COURSE_COUNT][BEST_TIME_COUNT];
  /* 0x0DC0 */ s32 bestTimes[COURSE_COUNT]
                            [BEST_TIME_COUNT]; /* Fastest first; unset is -1 */
  /* 0x0E08 */ s32 unkE08[COURSE_COUNT][BEST_TIME_COUNT];
  /* 0x0E50 */ s32 unkE50[COURSE_COUNT][BEST_TIME_COUNT];
  /* 0x0E98 */ UnkE98 unkE98[COURSE_COUNT];
  /* 0x1150 */ u8 unk1150[0x1320 - 0x1150];
  /* 0x1320 */ s8 hasRingShotStar[RING_SHOT_COURSE_COUNT][RING_SHOT_HOLE_COUNT];
  /* 0x1344 */ s8 unk1344[RING_SHOT_COURSE_COUNT][RING_SHOT_HOLE_COUNT]
                         [SAVE_UNK1344_ENTRY_COUNT];
  /* 0x15CC */ s32 coursePoints; /* Earned in total, so it never goes down */
  /* 0x15D0 */ PlayRecord rankedRecords[RANKED_RECORD_COUNT];
  /* 0x1DF0 */ RingShotRecord ringShotRecords[RING_SHOT_RECORD_COUNT];
  /* 0x1FF8 */ SavedGame savedGames[SAVED_GAME_COUNT];
  /* 0x21D8 */ UnkB8 savedPlayers[SAVED_GAME_COUNT][MAX_PLAYER_COUNT];
} SaveData; /* size = 0x2A78 */

/* Each copy is stored with this, to check that it was read back intact */
typedef struct SaveCheck {
  /* 0x0 */ u16 key; /* Random */
  /* 0x2 */ u16 keyedCrc; /* The data's CRC-16 exclusive-ored with the key */
} SaveCheck;

/* Where a copy of the save data and its check are kept */
typedef struct SaveCopyLocation {
  /* 0x0 */ char* dataFileName; /* On the development host */
  /* 0x4 */ char* checkFileName; /* On the development host */
  /* 0x8 */ u32 dataSramAddress;
  /* 0xC */ u32 checkSramAddress;
} SaveCopyLocation;

extern SaveData g_SaveData;

extern GameState g_GameState;

/* The current game's, which a saved game is loaded into and stored from */
extern UnkB8 g_Players[MAX_PLAYER_COUNT];

/* The figures D_80105C38 holds for each course, then for all of them */
typedef struct Unk80105C38 {
  /* 0x00 */ f32 roundCount;
  /* 0x04 */ f32 bestScore;
  /* 0x08 */ f32 averageScore;
  /* 0x0C */ f32 bestDrive;
  /* 0x10 */ f32 averageDrive;
  /* 0x14 */ f32 unk14;
  /* 0x18 */ f32 fairwayPercent;
  /* 0x1C */ f32 averagePutts;
  /* 0x20 */ f32 parSavePercent;
  /* 0x24 */ f32 parOnPercent;
  /* 0x28 */ f32 sandSavePercent;
  /* 0x2C */ Boolean hasPlace[TOURNAMENT_PLACE_COUNT];
} Unk80105C38; /* size = 0x38 */

extern Unk80105C38 D_80105C38[COURSE_COUNT + 1];

#define HOLES_PER_ROUND 18.0f

/* Applied to the save's 0x0C and 0x10 figures; what it converts is unknown */
#define UNK_SCALE 0.00007119878f

SaveData* GetSaveData(void);
void TransferSaveData(SaveData* save, u8 isWrite);
void ResetSaveData(void);
void InitSaveData(void);
void RecordDisplayListLength(u32 commandCount);
void WriteSaveData(void);
s32 CountOpenCourses(s32 coursePoints);
s32 GetCoursePointsNeeded(CourseIndex courseIndex);
void ComputeRoundStatistics(void);

#endif
