#ifndef PLAY_RECORD_H
#define PLAY_RECORD_H

#include <ultra64.h>

#include "c_language.h"
#include "character.h"
#include "club.h"
#include "course.h"
#include "hole_setup.h"
#include "math_util.h"
#include "player.h"
#include "scene.h"

typedef struct RecordedShot {
    BinaryAngle heading;
    s8 powerGaugeFrame;
    s8 impactGaugeFrame;
    ShotMode8 shotMode;
    ClubSlot8 clubSlot;
    s8 offsetX;
    s8 offsetY;
} RecordedShot;

#define SCORE_KIND_MASK 0xF
#define SCORE_KIND_UNUSED_BONUS 0x40
#define SCORE_KIND_RECORDED 0x80

enum ScoreKind {
    SCORE_KIND_HOLE_IN_ONE,
    SCORE_KIND_ALBATROSS,
    SCORE_KIND_EAGLE,
    SCORE_KIND_BIRDIE,
    SCORE_KIND_PAR,
    SCORE_KIND_COUNT
};

typedef s32 ScoreKind;

typedef struct PlayRecord {
    HoleSetup hole;
    GameMode gameMode;
    CourseIndex8 courseIndex;
    s8 holeIndex;
    u8 scoreKind;
    CharacterIndex8 characterIndex;
    UBoolean8 isLefty;
    CButtonPalette8 paletteIndex;
    u8 strokeCount;
    u8 unusedAfterStrokeCount;
    s32 noiseSeed;
    RecordedShot shots[4];
    u8 fifthShotStart[0x68 - 0x64];
} PlayRecord;

typedef struct RingShotRecord {
    HoleSetup hole;
    CourseIndex8 courseIndex;
    s8 holeIndex;
    s8 scoreKind;
    CharacterIndex8 characterIndex;
    UBoolean8 isLefty;
    CButtonPalette8 paletteIndex;
    u8 strokeCount;
    u8 unusedAfterStrokeCount;
    s32 noiseSeed;
    RecordedShot shots[5];
} RingShotRecord;

extern PlayRecord g_PlayRecords[PLAYER_COUNT_MAX];
extern RingShotRecord g_RingShotRecords[PLAYER_COUNT_MAX];

#endif
