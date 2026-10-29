#ifndef SCORECARD_H
#define SCORECARD_H

#include "common.h"

#include "math_util.h"
#include "round_stats.h"
#include "save.h"

/* The leaderboard shows this many of the tournament's contestants at a time */
#define LEADERBOARD_ROW_COUNT 10

#define HOLE_WINNER_HALVED (-1) /* Nobody won the last hole outright */
#define HOLE_WINNER_UNSET (-2) /* What SetScorecardLayout starts it as */

/* The layouts that SetScorecardLayout takes for the game modes */
enum ScorecardLayout {
  SCORECARD_LAYOUT_STROKE,
  SCORECARD_LAYOUT_MATCH, /* Match games and Get Character */
  SCORECARD_LAYOUT_SKINS, /* Skins matches and club slots */
  SCORECARD_LAYOUT_TOURNAMENT,
  SCORECARD_LAYOUT_SPEED_GOLF
};

/* g_ScorecardState values */
enum ScorecardState {
  SCORECARD_OPENING, /* Sliding in */
  SCORECARD_CLOSING, /* Sliding out */
  SCORECARD_SHOWN, /* The player can turn its pages */
  SCORECARD_TALLYING, /* Counting skins before it is shown */
  SCORECARD_CLOSED,
  SCORECARD_MATCH_RESULT, /* DrawMatchResult shows who won the hole */
  SCORECARD_UNK6
};

typedef struct ScorecardContent {
  /* 0x00 */ Radians rowFlipAngles[LEADERBOARD_ROW_COUNT];
  /* 0x28 */ s8 isRowFlipping[LEADERBOARD_ROW_COUNT];
  /* 0x34 */ s32 flipTime;
  /* 0x38 */ s32 faceImageOffsets[MAX_PLAYER_COUNT]; /* Added to each player's
                                                        first face image */
  /* 0x48 */ s32 courseIndex;
  /* 0x4C */ s32 holeIndex;
  /* 0x50 */ Boolean isSuddenDeath; /* The page shows SUDDEN DEATH */
} ScorecardContent; /* size = 0x54 */

typedef struct Scorecard {
  /* 0x00 */ u8 unk0[0xC];
  /* 0x0C */ u8* tableImage;
  /* 0x10 */ u8*
      placeArrowImage; /* Beside the player's row when their place changed */
  /* 0x14 */ u8* unk14;
  /* 0x18 */ u8*
      scoresPanelImage; /* Fills and edges the scores page's panels and cells */
  /* 0x1C */ u8* labelsImage;
  /* 0x20 */ u8* pageTitleImages[3];
  /* 0x2C */ u8* matchPanelImage; /* Behind each player's row of a match */
  /* 0x30 */ u8* digitsImage;
  /* 0x34 */ u8* placesImage;
  /* 0x38 */ u8* winMarkImage; /* Marks a match's hole won, or a skin */
  /* 0x3C */ u8*
      winsNeededImage; /* The tiles of the holes a match still needs */
  /* 0x40 */ u8* winnerArrowImage; /* Rises over the hole's winner in a match */
  /* 0x44 */ u8* betImages[BET_COUNT][2];
  /* 0x7C */ u8 unk7C[8];
  /* 0x84 */ u8* totalHeadingImage; /* Heads the bets page's column of totals */
  /* 0x88 */ u8*
      leaderboardRowImage; /* Behind each leaderboard row, and on its flaps */
  /* 0x8C */ u8*
      totalLabelsImage; /* Over the scores page's columns of totals */
  /* 0x90 */ u8*
      flapNameImages; /* One for each row, over the names on its flaps */
  /* 0x94 */ u8* gridImage; /* The pieces of the bets page's grid lines */
  /* 0x98 */ u8* faceImages[MAX_PLAYER_COUNT];
  /* 0xA8 */ u16* facePalettes[MAX_PLAYER_COUNT];
  /* 0xB8 */ u8*
      betSubtitlesImage; /* Rows of the subtitles under some bets' headings */
  /* 0xBC */ u8 unkBC;
  /* 0xBD */ u8 faceTime; /* Frames since the faces were opened */
  /* 0xBE */ u8 faceFrame; /* Which of the faces' images they show */
  /* 0xBF */ s8 placeChange; /* Which way a tournament player's place moved on
                                the hole, a PLACE_* value */
  /* 0xC0 */ s32 topRow; /* The contestant at the top of the leaderboard */
  /* 0xC4 */ s32 x; /* The scorecard's offset from its place on the screen */
  /* 0xC8 */ s32 y;
  /* 0xCC */ s32 unkCC;
  /* 0xD0 */ s32 stateTime; /* Frames since g_ScorecardState changed */
  /* The last hole's result is already on the scorecard */
  /* 0xD4 */ Boolean isTallied;
  /* 0xD8 */ s32 holeWinnerIndex; /* The player who won the last hole, or a
                                     HOLE_WINNER_* value */
  /* 0xDC */ s32 layout;
  /* 0xE0 */ s32 pageCount;
  /* 0xE4 */ Boolean hasBets;
  /* 0xE8 */ SkinsCarryover skinsCarryover;
  /* 0xEB */ u8 unkEB[5];
  /* 0xF0 */ ScorecardContent content;
} Scorecard; /* size = 0x144 */

/* The scorecard overlay's state, which TallySkins also keeps its skins in */
extern Scorecard g_Scorecard;
/* A SCORECARD_* value */
extern s32 g_ScorecardState;

/* Values of Scorecard.placeChange */
enum PlaceChange {
  PLACE_HELD = -1,
  PLACE_ROSE,
  PLACE_FELL
};

/* The channel of the tally's sounds, and the volume of the scorecard's */
#define TALLY_SOUND_CHANNEL 13
#define SCORECARD_SOUND_VOLUME 100

/* Each leaderboard row starts to flip over this many frames after the one above
 * it */
#define ROW_FLIP_STAGGER 4
#define ROW_FLIP_TIME (LEADERBOARD_ROW_COUNT * ROW_FLIP_STAGGER)

/* Match games are between two players */
#define MATCH_PLAYER_COUNT 2

/*
 * The match result's characters: f_ResultCharactersLoadStep counts up to
 * LOADING, then waits for LoadResultCharacters
 */
enum ResultCharactersState {
  RESULT_CHARACTERS_IDLE = -1, /* No characters are loading */
  RESULT_CHARACTERS_LOADING = 2,
  RESULT_CHARACTERS_LOADED
};

extern s32 f_ResultCharactersLoadStep;
s32 LoadResultCharacters(void);

void DrawMatchResult(Gfx** displayListHead);
void DrawStandingsPage(Gfx** displayListHead);
void DrawMatchPage(Gfx** displayListHead);
void DrawSkinsPage(Gfx** displayListHead);
void DrawBetsPage(Gfx** displayListHead);
void DrawSpeedGolfPage(Gfx** displayListHead);
void DrawLeaderboardPage(Gfx** displayListHead);
void DrawScoresPage(Gfx** displayListHead);

void OpenScorecard(s32 courseIndex, s32 holeIndex);
void SetScorecardLayout(s32 layout);
void UpdateScorecard(u16 buttons, u16 pressedButtons);
void DrawScorecard(Gfx** displayListHead);

#endif
