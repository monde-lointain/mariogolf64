#ifndef PLAYER_H
#define PLAYER_H

#include <ultra64.h>

#include "asset.h"
#include "bets.h"
#include "c_language.h"
#include "character.h"
#include "club.h"
#include "course.h"
#include "course_space.h"
#include "surface.h"

#define PLAYER_COUNT_MAX 4

extern ImageAsset *g_PlayerFaceImages[PLAYER_COUNT_MAX];
extern PaletteColor *g_PlayerFacePalettes[PLAYER_COUNT_MAX];

#define RING_COUNT 3

#define RING_STATE_NONE 0
#define RING_STATE_SHOWN 1
#define RING_STATE_PASSED 2
#define RING_STATE_FADED 32

#define UNCOUNTED_DISTANCE (-1)
#define UNMEASURED_DISTANCE (-2)

#define UNCOUNTED_PUTTS (-1)

typedef struct PlayerHole {
    s8 strokeCount;
    s8 puttCount;
} PlayerHole;

#define PLAYER_NAME_SIZE 12

enum PlayerState {
    PLAYER_STATE_STARTING_TURN,
    PLAYER_STATE_HOLE_INTRO,
    PLAYER_STATE_AIMING,
    PLAYER_STATE_UNUSED_AIMING,
    PLAYER_STATE_SETTING_POWER,
    PLAYER_STATE_SETTING_IMPACT,
    PLAYER_STATE_STARTING_SWING,
    PLAYER_STATE_SWINGING = 8,
    PLAYER_STATE_BALL_IN_FLIGHT,
    PLAYER_STATE_BALL_AT_REST,
    PLAYER_STATE_STARTING_REACTION,
    PLAYER_STATE_REACTING,
    PLAYER_STATE_CHOOSING = 15,
    PLAYER_STATE_ENDING_TURN,
    PLAYER_STATE_WAITING,
    PLAYER_STATE_FINISHING_HOLE,
    PLAYER_STATE_HOLE_DONE = 20,
    PLAYER_STATE_PREPARING_AIM,
    PLAYER_STATE_FINDING_OUT_PLAYER,
    PLAYER_STATE_SHOWING_OUT_PLAYER
};

typedef s32 PlayerState;

enum HoleStatus {
    HOLE_STATUS_PLAYING,
    HOLE_STATUS_HOLED_OUT,
    HOLE_STATUS_NEWLY_OUT,
    HOLE_STATUS_OUT
};

typedef s8 HoleStatus;

#define POWER_SHOTS_UNLIMITED (-1)

enum CButtonPalette {
    NO_PALETTE = -1,
    UP_C_PALETTE,
    LEFT_C_PALETTE,
    DOWN_C_PALETTE,
    RIGHT_C_PALETTE
};

typedef s32 CButtonPalette;
typedef s8 CButtonPalette8;

enum GridColor {
    GRID_COLOR_WHITE,
    GRID_COLOR_BLACK
};

typedef s8 GridColor;

#define GRID_COLOR_FLIP 1

enum NotableShot {
    NOTABLE_SHOT_GREAT,
    NOTABLE_SHOT_EXCELLENT,
    NOTABLE_SHOT_PIN_SHOT,
    NOTABLE_SHOT_CHIP_IN,
    NOTABLE_SHOT_LONG_CHIP_IN,
    NOTABLE_SHOT_UNUSED,
    NOTABLE_SHOT_FLAG_SHOT,
    NOTABLE_SHOT_HOLE_IN_ONE,
    NOTABLE_SHOT_ALBATROSS,
    NOTABLE_SHOT_EAGLE,
    NOTABLE_SHOT_BIRDIE,
    NOTABLE_SHOT_WATER_CUT,
    NOTABLE_SHOT_COUNT
};

typedef s32 NotableShot;

typedef struct Player {
    char name[PLAYER_NAME_SIZE];
    PlayerState state;
    CoursePoint ballPosition;
    Surface8 ballSurface;
    CharacterIndex8 characterIndex;
    Boolean8 isComputer;
    s8 ignoredPlanMark;
    u8 unusedBeforeLefty[0x22 - 0x20];
    Boolean8 isLefty;
    s8 strokeCount;
    s8 puttCount;
    s8 teeOrder;
    HoleStatus holeStatus;
    s8 controllerIndex;
    s8 powerShotCount;
    s8 handicap;
    s8 soundPan;
    PlayerHole holes[COURSE_HOLE_COUNT];
    u8 unusedAfterHoles[0x52 - 0x4F];
    ClubIndex16 clubs[CLUB_SLOT_COUNT];
    u8 unusedAfterClubs[0x74 - 0x72];
    s32 stopwatchFrameCount;
    s8 ringStates[RING_COUNT];
    CButtonPalette8 paletteIndex;
    s8 gameBoyCharacterIndex;
    u8 unusedAfterGameBoyIndex[3];
    s32 driveDistance;
    s32 pinDistance;
    s32 noiseSeed;
    GridColor gridColor;
    u8 unusedAfterGridColor;
    s16 betTallies[BET_COUNT];
    Boolean8 hasBeenInBunker;
    u8 unusedAfterBunkerVisit[3];
    s32 lastDriveDistance;
    s32 points;
    s16 unusedRestValue;
    s8 notableShotCounts[NOTABLE_SHOT_COUNT];
    u8 unusedAfterShotCounts[0xB8 - 0xB6];
} Player;

extern Player g_Players[PLAYER_COUNT_MAX];

#endif
