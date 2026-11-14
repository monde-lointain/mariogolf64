#ifndef SCORECARD_H
#define SCORECARD_H

#include <ultra64.h>

#include "asset.h"
#include "bets.h"
#include "c_language.h"
#include "course.h"
#include "math_util.h"
#include "player.h"
#include "round_stats.h"

#define LEADERBOARD_ROW_COUNT 10

#define HOLE_WINNER_HALVED (-1)
#define HOLE_WINNER_UNSET (-2)

enum ScorecardLayout {
    SCORECARD_LAYOUT_STROKE,
    SCORECARD_LAYOUT_MATCH,
    SCORECARD_LAYOUT_SKINS,
    SCORECARD_LAYOUT_TOURNAMENT,
    SCORECARD_LAYOUT_SPEED_GOLF
};

typedef s32 ScorecardLayout;

enum ScorecardState {
    SCORECARD_OPENING,
    SCORECARD_CLOSING,
    SCORECARD_SHOWN,
    SCORECARD_TALLYING,
    SCORECARD_CLOSED,
    SCORECARD_UNUSED_MATCH_RESULT,
    SCORECARD_UNUSED_HIDDEN
};

typedef s32 ScorecardState;

typedef struct ScorecardContent {
    Radians rowFlipAngles[LEADERBOARD_ROW_COUNT];
    Boolean8 isRowFlipping[LEADERBOARD_ROW_COUNT];
    s32 flipTime;
    s32 faceImageOffsets[PLAYER_COUNT_MAX];
    CourseIndex courseIndex;
    s32 holeIndex;
    Boolean isSuddenDeath;
} ScorecardContent;

enum PlaceChange {
    PLACE_HELD = -1,
    PLACE_ROSE,
    PLACE_FELL
};

typedef s8 PlaceChange;

#define PAGE_TITLE_COUNT 3

typedef struct Scorecard {
    u8 unusedBeforeTableImage[0xC];
    ImageAsset *tableImage;
    ImageAsset *placeArrowImage;
    ImageAsset *raisedPanelImage;
    ImageAsset *scoresPanelImage;
    ImageAsset *labelsImage;
    ImageAsset *pageTitleImages[PAGE_TITLE_COUNT];
    ImageAsset *matchPanelImage;
    ImageAsset *digitsImage;
    ImageAsset *placesImage;
    ImageAsset *winMarkImage;
    ImageAsset *winsNeededImage;
    ImageAsset *winnerArrowImage;
    ImageAsset *betImages[BET_COUNT][2];
    u8 unusedAfterBetImages[8];
    ImageAsset *totalHeadingImage;
    ImageAsset *leaderboardRowImage;
    ImageAsset *totalLabelsImage;
    ImageAsset *flapNameImages;
    ImageAsset *gridImage;
    ImageAsset *faceImages[PLAYER_COUNT_MAX];
    PaletteColor *facePalettes[PLAYER_COUNT_MAX];
    ImageAsset *betSubtitlesImage;
    UBoolean8 isOddFaceStep;
    u8 faceFrameTime;
    u8 faceFrame;
    PlaceChange placeChange;
    s32 topRow;
    s32 x;
    s32 y;
    s32 unusedAfterOffset;
    s32 stateTime;
    Boolean isTallied;
    s32 holeWinnerIndex;
    s32 layout;
    s32 pageCount;
    Boolean hasBets;
    SkinsCarryover skinsCarryover;
    u8 unusedAfterCarryover[5];
    ScorecardContent content;
} Scorecard;

extern Scorecard g_Scorecard;
extern ScorecardState g_ScorecardState;

#define TALLY_SOUND_CHANNEL 13
#define SCORECARD_SOUND_VOLUME 100

#define ROW_FLIP_STAGGER 4
#define ROW_FLIP_TIME (LEADERBOARD_ROW_COUNT * ROW_FLIP_STAGGER)

#define MATCH_PLAYER_COUNT 2

enum ResultCharactersState {
    RESULT_CHARACTERS_IDLE = -1,
    RESULT_CHARACTERS_LOADING = 2,
    RESULT_CHARACTERS_LOADED
};

typedef s32 ResultCharactersState;

extern ResultCharactersState g_ResultCharactersLoadStep;
Boolean LoadResultCharacters(void);

void DrawMatchResult(Gfx **displayListHead);
void DrawStandingsPage(Gfx **displayListHead);
void DrawMatchPage(Gfx **displayListHead);
void DrawSkinsPage(Gfx **displayListHead);
void DrawBetsPage(Gfx **displayListHead);
void DrawSpeedGolfPage(Gfx **displayListHead);
void DrawLeaderboardPage(Gfx **displayListHead);
void DrawScoresPage(Gfx **displayListHead);

void OpenScorecard(CourseIndex courseIndex, s32 holeIndex);
void SetScorecardLayout(ScorecardLayout layout);
void UpdateScorecard(u16 buttons, u16 pressedButtons);
void DrawScorecard(Gfx **displayListHead);

void FreeResultCharacters(void);
void FreeScorecard(void);
void StartScorecardTally(s32 layout);
Boolean UpdateShownScorecard(u16 buttons);

#endif
