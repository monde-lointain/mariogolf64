#ifndef SCORECARD_H
#define SCORECARD_H

#include "common.h"
#include "round_stats.h"
#include "save.h"

/* The leaderboard shows this many of the tournament's contestants at a time */
#define LEADERBOARD_ROW_COUNT 10

#define HOLE_WINNER_HALVED (-1) /* Nobody won the last hole outright */
#define HOLE_WINNER_UNSET (-2)  /* What SetScorecardLayout starts it as */

typedef struct ScorecardContent {
    /* 0x00 */ f32 rowFlipAngles[LEADERBOARD_ROW_COUNT];
    /* 0x28 */ s8 isRowFlipping[LEADERBOARD_ROW_COUNT];
    /* 0x34 */ s32 flipTime;
    /* 0x38 */ s32 unk38[MAX_PLAYER_COUNT];
    /* 0x48 */ s32 courseIndex;
    /* 0x4C */ s32 holeIndex;
    /* 0x50 */ s32 unk50;
} ScorecardContent; /* size = 0x54 */

typedef struct Scorecard {
    /* 0x00 */ u8 unk0[0xC];
    /* 0x0C */ u8 *tableImage;
    /* 0x10 */ u8 *unk10;
    /* 0x14 */ u8 *unk14;
    /* 0x18 */ u8 *unk18;
    /* 0x1C */ u8 *labelsImage;
    /* 0x20 */ u8 *pageTitleImages[3];
    /* 0x2C */ u8 *unk2C;
    /* 0x30 */ u8 *digitsImage;
    /* 0x34 */ u8 *placesImage;
    /* 0x38 */ u8 *unk38;
    /* 0x3C */ u8 *unk3C;
    /* 0x40 */ u8 *unk40;
    /* 0x44 */ u8 *betImages[BET_COUNT][2];
    /* 0x7C */ u8 unk7C[8];
    /* 0x84 */ u8 *unk84;
    /* 0x88 */ u8 *unk88;
    /* 0x8C */ u8 *unk8C;
    /* 0x90 */ u8 *unk90;
    /* 0x94 */ u8 *unk94;
    /* 0x98 */ u8 *faceImages[MAX_PLAYER_COUNT];
    /* 0xA8 */ u16 *facePalettes[MAX_PLAYER_COUNT];
    /* 0xB8 */ u8 *unkB8;
    /* 0xBC */ u8 unkBC;
    /* 0xBD */ u8 unkBD;
    /* 0xBE */ u8 unkBE;
    /* 0xBF */ s8 unkBF;
    /* 0xC0 */ s32 topRow; /* The contestant at the top of the leaderboard */
    /* 0xC4 */ s32 x; /* The scorecard's offset from its place on the screen */
    /* 0xC8 */ s32 y;
    /* 0xCC */ s32 unkCC;
    /* 0xD0 */ s32 stateTime; /* Frames since sScorecardState changed */
    /* 0xD4 */ s32 unkD4;
    /* 0xD8 */ s32 holeWinnerIndex; /* The player who won the last hole, or a HOLE_WINNER_* value */
    /* 0xDC */ s32 layout;
    /* 0xE0 */ s32 pageCount;
    /* 0xE4 */ s32 hasBets;
    /* 0xE8 */ SkinsCarryover skinsCarryover;
    /* 0xEB */ u8 unkEB[5];
    /* 0xF0 */ ScorecardContent content;
} Scorecard; /* size = 0x144 */

/* The scorecard overlay's state, which TallySkins also keeps its skins in */
extern Scorecard g_Scorecard;

void DrawScorecard(Gfx **displayListHead);

#endif
