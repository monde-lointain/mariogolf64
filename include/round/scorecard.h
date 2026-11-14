/*
 * The scorecard shown between holes and its pages for each game mode.
 */

#ifndef SCORECARD_H
#define SCORECARD_H

#include <ultra64.h>

#include "course/course.h"
#include "math/math_util.h"
#include "round/bets.h"
#include "round/player.h"
#include "round/round_stats.h"
#include "system/asset.h"
#include "system/c_language.h"

// The leaderboard shows this many of the tournament's contestants at a time
#define LEADERBOARD_ROW_COUNT 10

// Nobody won the last hole outright
#define HOLE_WINNER_HALVED (-1)
// What SetScorecardLayout starts it as
#define HOLE_WINNER_UNSET (-2)

// The layouts that SetScorecardLayout takes for the game modes
enum ScorecardLayout {
    SCORECARD_LAYOUT_STROKE,
    SCORECARD_LAYOUT_MATCH, // Match games and Get Character
    SCORECARD_LAYOUT_SKINS, // Skins matches and club slots
    SCORECARD_LAYOUT_TOURNAMENT,
    SCORECARD_LAYOUT_SPEED_GOLF
};

typedef s32 ScorecardLayout;

// g_ScorecardState values
enum ScorecardState {
    SCORECARD_OPENING, // Sliding in
    SCORECARD_CLOSING, // Sliding out
    SCORECARD_SHOWN, // The player can turn its pages
    SCORECARD_TALLYING, // Counting skins before it is shown
    SCORECARD_CLOSED,
    // Never set: DrawMatchResult would show who won the hole
    SCORECARD_UNUSED_MATCH_RESULT,
    // Never set: nothing would be drawn
    SCORECARD_UNUSED_HIDDEN
};

typedef s32 ScorecardState;

typedef struct ScorecardContent {
    Radians rowFlipAngles[LEADERBOARD_ROW_COUNT];
    Boolean8 isRowFlipping[LEADERBOARD_ROW_COUNT];
    s32 flipTime;
    s32 faceImageOffsets[PLAYER_COUNT_MAX]; // Added to each player's first face image
    CourseIndex courseIndex;
    s32 holeIndex;
    Boolean isSuddenDeath; // The page shows SUDDEN DEATH
} ScorecardContent;

// Values of Scorecard.placeChange
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
    ImageAsset *placeArrowImage; // Beside the player's row when their place changed
    // scoresPanelImage's cells lit from the other side; loaded but never drawn
    ImageAsset *raisedPanelImage;
    ImageAsset *scoresPanelImage; // Fills and edges the scores page's panels and cells
    ImageAsset *labelsImage;
    ImageAsset *pageTitleImages[PAGE_TITLE_COUNT];
    ImageAsset *matchPanelImage; // Behind each player's row of a match
    ImageAsset *digitsImage;
    ImageAsset *placesImage;
    ImageAsset *winMarkImage; // Marks a match's hole won, or a skin
    ImageAsset *winsNeededImage; // The tiles of the holes a match still needs
    ImageAsset *winnerArrowImage; // Rises over the hole's winner in a match
    ImageAsset *betImages[BET_COUNT][2];
    u8 unusedAfterBetImages[8];
    ImageAsset *totalHeadingImage; // Heads the bets page's column of totals
    ImageAsset *leaderboardRowImage; // Behind each leaderboard row, and on its flaps
    ImageAsset *totalLabelsImage; // Over the scores page's columns of totals
    ImageAsset *flapNameImages; // One for each row, over the names on its flaps
    ImageAsset *gridImage; // The pieces of the bets page's grid lines
    ImageAsset *faceImages[PLAYER_COUNT_MAX];
    PaletteColor *facePalettes[PLAYER_COUNT_MAX];
    ImageAsset *betSubtitlesImage; // Rows of the subtitles under some bets' headings
    UBoolean8 isOddFaceStep; // Flips at each face step; never read
    u8 faceFrameTime; // Frames since the faces were opened
    u8 faceFrame; // Which of the faces' images they show
    // Which way a tournament player's place moved on the hole, a PLACE_* value
    PlaceChange placeChange;
    s32 topRow; // The contestant at the top of the leaderboard
    s32 x; // The scorecard's offset from its place on the screen
    s32 y;
    s32 unusedAfterOffset;
    s32 stateTime; // Frames since g_ScorecardState changed
    // The last hole's result is already on the scorecard
    Boolean isTallied;
    s32 holeWinnerIndex; // The player who won the last hole, or a HOLE_WINNER_* value
    ScorecardLayout layout;
    s32 pageCount;
    Boolean hasBets;
    SkinsCarryover skinsCarryover;
    u8 unusedAfterCarryover[5];
    ScorecardContent content;
} Scorecard;

// The scorecard overlay's state, which TallySkins also keeps its skins in
extern Scorecard g_Scorecard;
extern ScorecardState g_ScorecardState;

// The channel of the tally's sounds, and the volume of the scorecard's
#define TALLY_SOUND_CHANNEL 13
#define SCORECARD_SOUND_VOLUME 100

// Each leaderboard row starts to flip over this many frames after the one above
// it
#define ROW_FLIP_STAGGER 4
#define ROW_FLIP_TIME (LEADERBOARD_ROW_COUNT * ROW_FLIP_STAGGER)

// Match games are between two players
#define MATCH_PLAYER_COUNT 2

// The match result's characters: f_ResultCharactersLoadStep counts up to
// LOADING, then waits for LoadResultCharacters
enum ResultCharactersState {
    RESULT_CHARACTERS_IDLE = -1, // No characters are loading
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
