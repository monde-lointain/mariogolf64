/*
 * The records of holes played well, which the menus replay
 */

#ifndef PLAY_RECORD_H
#define PLAY_RECORD_H

#include <ultra64.h>

#include "characters/character.h"
#include "course/course.h"
#include "course/hole_setup.h"
#include "game/scene.h"
#include "math/math_util.h"
#include "round/player.h"
#include "shot/club.h"
#include "system/c_language.h"

// A shot as a record keeps it, from the CurrentShot it was played with
typedef struct RecordedShot {
    BinaryAngle heading;
    s8 powerGaugeFrame;
    s8 impactGaugeFrame;
    ShotMode8 shotMode;
    ClubSlot8 clubSlot;
    s8 offsetX;
    s8 offsetY;
} RecordedShot;

// A record's scoreKind: SCORE_KIND_RECORDED marks a score worth keeping, and
// the low bits hold its ScoreKind
#define SCORE_KIND_MASK 0xF
// Never set: GetRecordPriority would rank a birdie or eagle higher
#define SCORE_KIND_UNUSED_BONUS 0x40
#define SCORE_KIND_RECORDED 0x80

enum ScoreKind {
    SCORE_KIND_HOLE_IN_ONE,
    SCORE_KIND_ALBATROSS,
    SCORE_KIND_EAGLE,
    SCORE_KIND_BIRDIE,
    SCORE_KIND_PAR, // Recorded in Ring Shot alone
    SCORE_KIND_COUNT
};

typedef s32 ScoreKind;

// A hole that a player finished in few strokes, kept to be replayed
typedef struct PlayRecord {
    HoleSetup hole;
    GameMode gameMode;
    CourseIndex8 courseIndex;
    s8 holeIndex;
    u8 scoreKind; // What GetRecordedScoreKind returns for the strokes
    CharacterIndex8 characterIndex;
    UBoolean8 isLefty;
    CButtonPalette8 paletteIndex;
    u8 strokeCount;
    u8 unusedAfterStrokeCount;
    s32 noiseSeed;
    RecordedShot shots[4]; // RecordHoleOut writes a fifth past the end
    // The first half of the fifth shot, whose heading and gauge frames land
    // here; its other half lands in the next record
    u8 fifthShotStart[0x68 - 0x64];
} PlayRecord;

// A Ring Shot hole as a player played it, kept to be replayed
typedef struct RingShotRecord {
    HoleSetup hole;
    CourseIndex8 courseIndex;
    s8 holeIndex;
    s8 scoreKind; // What GetRecordedScoreKind returns for the strokes
    CharacterIndex8 characterIndex;
    UBoolean8 isLefty;
    CButtonPalette8 paletteIndex;
    u8 strokeCount;
    u8 unusedAfterStrokeCount;
    s32 noiseSeed;
    // RecordRingShotHole writes a sixth past the end, into the next record
    RecordedShot shots[5];
} RingShotRecord;

// Each player's last hole, and each player's last Ring Shot hole
extern PlayRecord g_PlayRecords[PLAYER_COUNT_MAX];
extern RingShotRecord g_RingShotRecords[PLAYER_COUNT_MAX];

#endif
