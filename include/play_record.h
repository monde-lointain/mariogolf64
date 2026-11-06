#ifndef PLAY_RECORD_H
#define PLAY_RECORD_H

#include <ultra64.h>

#include "game_state.h"
#include "math_util.h"
#include "player.h"

/*
 * The records of holes played well, which the menus replay
 */

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
/* Never set: GetRecordPriority would rank a birdie or eagle higher */
#define SCORE_KIND_UNUSED_BONUS 0x40
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
  /* 0x3B */ s8 characterIndex;
  /* 0x3C */ u8 isLefty;
  /* 0x3D */ s8 paletteIndex;
  /* 0x3E */ u8 strokeCount;
  /* 0x3F */ u8 unusedAfterStrokeCount;
  /* 0x40 */ s32 noiseSeed;
  /* 0x44 */ RecordedShot
      shots[4]; /* RecordHoleOut writes a fifth past the end */
  /*
   * The first half of the fifth shot, whose heading and gauge frames land
   * here; its other half lands in the next record
   */
  /* 0x64 */ u8 fifthShotStart[0x68 - 0x64];
} PlayRecord; /* size = 0x68 */

/* A Ring Shot hole as a player played it, kept to be replayed */
typedef struct RingShotRecord {
  /* 0x00 */ HoleSetup hole;
  /* 0x34 */ s8 courseIndex;
  /* 0x35 */ s8 holeIndex;
  /* 0x36 */ s8
      scoreKind; /* What GetRecordedScoreKind returns for the strokes */
  /* 0x37 */ s8 characterIndex;
  /* 0x38 */ u8 isLefty;
  /* 0x39 */ s8 paletteIndex;
  /* 0x3A */ u8 strokeCount;
  /* 0x3B */ u8 unusedAfterStrokeCount;
  /* 0x3C */ s32 noiseSeed;
  /* RecordRingShotHole writes a sixth past the end, into the next record */
  /* 0x40 */ RecordedShot shots[5];
} RingShotRecord; /* size = 0x68 */

/* Each player's last hole, and each player's last Ring Shot hole */
extern PlayRecord g_PlayRecords[MAX_PLAYER_COUNT];
extern RingShotRecord g_RingShotRecords[MAX_PLAYER_COUNT];

#endif
