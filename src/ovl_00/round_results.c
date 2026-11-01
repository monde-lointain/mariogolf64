/*
 * The round overlay. It begins with the results that slide in when a
 * round ends: the players' portraits and scores, the course points the
 * round earned and the notices of what those points unlocked.
 */
#include "common.h"

#include "asset.h"
#include "billboard.h"
#include "character.h"
#include "controller.h"
#include "course.h"
#include "debug_text.h"
#include "effects.h"
#include "game_flags.h"
#include "heap.h"
#include "main.h"
#include "math_util.h"
#include "menu.h"
#include "password.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "scorecard.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "tournament.h"
#include "transfer_pak.h"
/* OpenRoundResults's request that frees the results */
#define CLOSE_ROUND_RESULTS (-1)

/* f_ResultsState values */
enum ResultsState {
  RESULTS_SLIDING_IN,
  RESULTS_SHOWN,
  RESULTS_CLOSING /* Saves and frees them */
};

/* f_BannerKind values: what the results banner shows */
enum BannerKind {
  BANNER_KIND_NONE, /* Set with BANNER_HIDDEN as the results load */
  BANNER_KIND_TOURNAMENT_PLACE,
  BANNER_KIND_UNK2,
  BANNER_KIND_POINTS,
  BANNER_KIND_UNK4,
  BANNER_KIND_UNK5
};

/* f_BannerState values */
enum BannerState {
  BANNER_UNSET = -1, /* Until the results first load */
  BANNER_SLIDING_IN,
  BANNER_SHOWN,
  BANNER_HIDDEN = 3,
  BANNER_SLIDING_OUT
};

/* LoadUnlockNotice's unlocks */
enum UnlockNotice {
  UNLOCK_NOTICE_DONKEY_KONG,
  UNLOCK_NOTICE_MAPLE,
  UNLOCK_NOTICE_METAL_MARIO,
  UNLOCK_NOTICE_COURSES /* The first of the courses' unlocks */
};

/* f_UnlockNoticeKind values */
enum UnlockNoticeKind {
  UNLOCK_NOTICE_KIND_NONE,
  UNLOCK_NOTICE_KIND_COURSE,
  UNLOCK_NOTICE_KIND_CHARACTER
};

/* f_TallySpeed normally, and while A is held */
#define NORMAL_TALLY_SPEED 1
#define FAST_TALLY_SPEED 10
/* At the normal speed, the tally ticks on every fourth call */
#define TALLY_SOUND_INTERVAL_MASK 3
/* The points that the debug point flags add */
#define SMALL_DEBUG_POINTS_BONUS 500
#define LARGE_DEBUG_POINTS_BONUS 9999
/* Where the debug unlock choice prints the chosen character */
#define DEBUG_UNLOCK_CHOICE_COLUMN 25
#define DEBUG_UNLOCK_CHOICE_ROW 5
#define CONFIRM_SOUND_CHANNEL 15
#define CONFIRM_SOUND_VOLUME 100

enum PortraitAnimation {
  PORTRAIT_ANIMATION_UNK1 = 1,
  PORTRAIT_ANIMATION_UNK4 = 4
};

/*
 * The course notice drops in on a half sine swing, NOTICE_SWING_STEP an
 * update, and is done NOTICE_DONE_FRAME frames after it starts
 */
#define NOTICE_SWING_HEIGHT 160.0f
/* Nearly a quarter turn back, so that the swing starts at its full height */
#define NOTICE_SWING_START_ANGLE (-1.57f)
#define NOTICE_SWING_STEP (M_PI / 9)
#define NOTICE_SWING_FRAMES 46
#define NOTICE_DONE_FRAME 51

/*
 * The character notice's portrait changes face every few frames after
 * a pause, then lifts away, faster and faster, until it is off screen
 */
#define NOTICE_FACE_DELAY 10
/* Every fourth frame */
#define NOTICE_FACE_INTERVAL_MASK 3
#define NOTICE_LAST_FACE 3
#define NOTICE_LIFT_DELAY 50
#define NOTICE_LIFT_START_SPEED 8
/*
 * Until the lift passes this, it rises NOTICE_LIFT_START_SPEED a frame; after,
 * it grows each frame by this fraction of itself
 */
#define NOTICE_LIFT_START_LIMIT (-2)
#define NOTICE_LIFT_GROWTH_NUMERATOR 2
#define NOTICE_LIFT_GROWTH_DENOMINATOR 3
#define NOTICE_LIFT_EXIT (-200)

/* The places image's 32x32 icons, after its 256-color palette */
#define PLACE_ICON_SIZE 32

/* The labels image's 16-color strips. Each is stored wider than drawn. */
#define RESULTS_LABEL_WIDTH 72
#define RESULTS_LABEL_STORED_WIDTH 80
#define RESULTS_LABEL_HEIGHT 18

#define PORTRAIT_SIZE 32
/* The portrait's mirrored wrap masks its S at its width, 1 << 5 */
#define PORTRAIT_SIZE_LOG2 5

/* Course points are clamped there rather than overflow */
#define MAX_COURSE_POINTS S32_MAX

/* The places of f_TournamentPrizes's columns after the trophy winners' */
#define TROPHY_PLACE_COUNT 3
#define PRIZE_TOP_TEN 10
#define PRIZE_TOP_TWENTY 20

/* The columns of f_TournamentPrizes: a prize for each trophy place, then one
 * for each wider bracket of places */
enum PrizeBracket {
  PRIZE_BRACKET_FIRST,
  PRIZE_BRACKET_SECOND,
  PRIZE_BRACKET_THIRD,
  PRIZE_BRACKET_TOP_TEN,
  PRIZE_BRACKET_TOP_TWENTY,
  PRIZE_BRACKET_REST,
  PRIZE_BRACKET_COUNT
};

/* The results' images. g_GameState.isClubTournament swaps in another set. */
#define RESULTS_BACKDROP_ASSET 0x6A0
#define CLUB_TOURNAMENT_BACKDROP_ASSET 0x947
#define FINAL_RESULTS_TITLE_ASSET 0x69F
#define COURSE_NOTICE_TITLE_ASSET 0x55F
#define CHARACTER_NOTICE_TITLE_ASSET 0x560
#define RESULTS_UNK72C_IMAGE_ASSET 0x72C
#define RESULTS_UNK72D_PALETTE_ASSET 0x72D
#define MATCH_PLACES_IMAGE_ASSET 0x6A2
#define MATCH_UNK6A3_ASSET 0x6A3
/* The words UP, DOWN and EVEN, stacked, for match play standings */
#define MATCH_STANDING_WORDS_ASSET 0x8FE
#define TOURNAMENT_UNK6A1_ASSET 0x6A1

/* Makes DrawRank draw the rank as a place: 1st, 2nd and so on */
#define RANK_AS_PLACE 0x1000
#define NOTICE_TITLE_STRIP_HEIGHT 8
#define NOTICE_SHINE_SIZE 64
#define NOTICE_SHINE_STEP 18
#define NOTICE_SHINE_START 16
#define NOTICE_SHINE_END 40
/* The title's depth, and the shine's behind it, so that the shine only lights
 * the title's letters */
#define NOTICE_TITLE_DEPTH 0x3FFF
#define NOTICE_SHINE_DEPTH 0x4027
#define NOTICE_SWING_SOUND_CHANNEL 13
#define NOTICE_SWING_SOUND_VOLUME 95
#define NOTICE_SHINE_SOUND_CHANNEL 14
#define NOTICE_SHINE_SOUND_VOLUME 63
#define RESULTS_LABEL_SCORE 2
#define RESULTS_LABEL_POINTS 5
#define MESSAGE_POINTS_TO_NEXT_COURSE 0x46
#define MESSAGE_ALL_COURSES_OPEN 0x47
#define RESULTS_TEXT_SLOT 0
#define RESULTS_TEXT_RED 230
#define RESULTS_TEXT_GREEN 230
#define RESULTS_TEXT_BLUE 255
#define RESULTS_TEXT_ALPHA 255
#define LARGE_DIGIT_WIDTH 16
#define LARGE_DIGITS_HEIGHT 256
/*
 * The Course Points footer: the points total, then the text of the points
 * left to the next course, or else the notice that every course is open,
 * whose row differs by screen
 */
#define POINTS_TOTAL_X 29
#define POINTS_TOTAL_Y 201
#define POINTS_TO_NEXT_COURSE_X 64
#define POINTS_TO_NEXT_COURSE_Y 200
#define ALL_COURSES_OPEN_X 32
#define SOLO_ALL_COURSES_OPEN_Y 192
#define PLAYERS_ALL_COURSES_OPEN_Y 200
#define MATCH_ALL_COURSES_OPEN_Y 204
/* The minutes wrap here, since the time shows two digits */
#define SHOWN_MINUTES_LIMIT 99
#define RESULTS_LABEL_BETS 4
#define RESULTS_HEADING_Y 54
/* Bet totals of four digits all shift left, so that the column stays centered
 */
#define FOUR_DIGIT_BET_SHIFT (-16)
#define FOUR_DIGIT_BET_MIN 999
#define FOUR_DIGIT_BET_MAX 9999
extern const char f_NumberFormat[];
extern const char f_PlaceNumberFormat[];

/* f_PlayerRanks of a match's players */
#define MATCH_RANK_WON 100
#define MATCH_RANK_LOST 101

/* The digit font draws '#' as a plus-minus sign, for an even score */
extern const char f_EvenScoreFormat[];
extern const char f_ScoreFormat[];
extern const char f_PointsFormat[];
extern const char f_TimeFormat[];
extern const char f_TimeLabel[];
/* Computer players earn no Course Points, which shows as a dash */
extern const char f_NoPointsText[];

s32 GetClubTournamentCourse(void);

void FreeRoundResults(void);
void LoadRoundResults(void);
void DrawCourseResultsNotice(Gfx** displayListHead);
void DrawCharacterResultsNotice(Gfx** displayListHead);
s32 UpdateTournamentResults(ControllerState* controller);
s32 ShowScorePassword(ControllerState* controller);
s32 UpdateCoursePointsResults(ControllerState* controller);
s32 UpdateMatchResults(ControllerState* controller);
void RecordComCharacterResult(s32 row, s32 column, Boolean isWin);

/* The score password shows as two lines of equal length */
#define SCORE_PASSWORD_LINE_LENGTH (SCORE_PASSWORD_LENGTH / 2)

/* The images of the trophy a course's tournament gives its top three */
typedef struct TrophyAssets {
  /* 0x0 */ s32 palette; /* For first place; second and third follow */
  /* 0x4 */ s32 image;
  /* 0x8 */ s32 frameCount; /* Of image, as the trophy turns */
} TrophyAssets; /* size = 0xC */

/* Each course's trophy image's sizes, indexed by these */
#define TROPHY_WIDTH 0
/* TROPHY_PART_COUNT of them */
#define TROPHY_PART_HEIGHTS 1
#define TROPHY_SHINE_HEIGHT 3
#define TROPHY_SIZE_COUNT 4

/* "StrokeSpeedGolfMini-Golf", which only an unread pointer points to */
extern char f_UnusedModeNamesText[];

static Boolean f_AreResultsLoaded = FALSE;
static s32 f_ResultsState = RESULTS_SLIDING_IN;
static s32 f_ResultsUnk78 = 0;
static s32 f_ResultsUnk7C = 0;
static s32 f_ResultsSlide = SCREEN_WIDTH;
static s32 f_ResultsTop = 0;
static s32 f_TallySpeed = NORMAL_TALLY_SPEED;
/* The pressing hand shows while the results wait for A */
static Boolean f_IsAwaitingConfirm = FALSE;
/*
 * Each course's tournament prizes, in course points: first to third
 * place, then up to PRIZE_TOP_TEN, PRIZE_TOP_TWENTY and the rest
 */
static s32 f_TournamentPrizes[COURSE_COUNT][PRIZE_BRACKET_COUNT] = {
    {50, 25, 12, 8, 5, 3}, {200, 100, 50, 20, 10, 6},
    {600, 300, 150, 75, 35, 17}, {1200, 600, 300, 100, 50, 25},
    {2000, 1000, 500, 200, 100, 50}, {2000, 1000, 500, 200, 100, 50}};
static f32 f_CoursePointRates[] = {
    1.0f, 1.5f, 2.0f, 2.5f, 3.5f, 3.5f, 1.0f, 1.0f};
static s32 f_OpenCourseCount = 1;
static s32 f_TournamentOpenCourseCount = 1;
static s32 f_PointsToNextCourse = 0;
static TrophyAssets f_TrophyAssets[COURSE_COUNT] = {{0x88A, 0x88D, 16},
    {0x8AF, 0x8B2, 16}, {0x877, 0x87A, 16}, {0x8D5, 0x8D8, 16},
    {0x8C2, 0x8C5, 16}, {0x89D, 0x8A0, 15}};
static s32 f_BannerState = BANNER_UNSET;
static s32 f_BannerKind = BANNER_KIND_NONE;
static s32 f_BannerX = SCREEN_WIDTH;
static s32 f_BannerY = 0;
static s32 f_CourseNoticeAssets[] = {0x96C, 0x96D, 0x96E, 0x96F, 0x970};
static s32 f_TallySoundCount = 0;
/* Read by no routine */
static s32 f_UnusedPingPongFrames[6] UNUSED = {0, 1, 2, 3, 2, 1};
static s32 f_TrophySizes[COURSE_COUNT][TROPHY_SIZE_COUNT] = {{40, 24, 24, 38},
    {40, 40, 11, 40}, {40, 40, 11, 50}, {32, 32, 32, 64}, {48, 22, 20, 21},
    {36, 40, 11, 40}};
static s32 f_TrophyShineFrame = 0;
static s32 f_TrophyFrame = 0;
static CharacterId f_DebugUnlockCharacter = 0;
/* Read by no routine */
static void* f_UnusedModeNamesPointers[2] UNUSED = {
    NULL, f_UnusedModeNamesText};
/* The text color of each text slot, from slot 1 */
static u8 f_TextSlotColors[][3] = {{0xFF, 0xFF, 0xFF}, {0x7F, 0xBF, 0xFF},
    {0x7F, 0xBF, 0xFF}, {0x7F, 0xFF, 0x4B}, {0xFF, 0x7F, 0x9F}};
/* Counts up through the pressing hand's frames, HAND_FRAME_DRAWS draws to a
 * frame */
static s32 f_HandFrame = 0;
static Radians f_TitleBobAngle = 0.0f;

/* Read by no routine */
static u8 f_UnusedBufferBeforeBackdrop[0x10] UNUSED;
static u8* f_ResultsBackdropImage;
static u8* f_FinalResultsTitleImage;
static u8* f_ResultsCourseImage;
static u8* f_PortraitImage;
static u8* f_PortraitPalette;
static u8* f_PlayerPortraitImages[MAX_PLAYER_COUNT];
static u8* f_PlayerPortraitPalettes[MAX_PLAYER_COUNT];
static u8* f_ResultsCloudImage;
static u8* f_ResultsLabelsImage;
static u8* f_ResultsLargeDigitsImage;
static u8* f_ResultsPlacesImage;
/* The banner that DrawRank draws for a rank past RANK_BANNER */
static u8* f_SecondBannerImage;
static u8* f_NameplateImage;
static u8* f_NoticeTitleImage;
static u8* f_ResultsShineImage;
/* An image and palette that are loaded and freed, but never drawn */
static u8* f_UnusedResultsImage;
static u8* f_UnusedResultsPalette;
static u8* f_TrophyImage;
static u8* f_TrophyPalette;
static u8* f_ResultsHandImage;
static u8* f_MatchStandingWordsImage;
static u8* f_NintendoBadgeImage;
static u8* f_DigitsImage;
static u8* f_PlaceDigitsImage;
static s32 f_UnlockNoticeKind;
static Boolean f_HasBets;
static CharacterId f_PortraitCharacterIndex;
static s32 f_NoticeStage;
static s32 f_NoticeFrameCount;
static s32 f_NoticeSwing;
static s32 f_NoticeUnk54;
static Radians f_NoticeAngle;
static Boolean f_IsNoticeHeld;
static s32 f_NoticeTime;
static s32 f_NoticeFaceIndex;
static s32 f_NoticeLift;
static s32 f_NoticeUnk6C;
/* The score password of a round of a tournament entered by password */
static char f_ScorePassword[SCORE_PASSWORD_LENGTH];
static s32 f_PlayerResults[MAX_PLAYER_COUNT];
static s32 f_PlayerPoints[MAX_PLAYER_COUNT];
static s32 f_PlayerBetTotals[MAX_PLAYER_COUNT];
static s32 f_PlayerRanks[MAX_PLAYER_COUNT];
static s32 f_PrizePoints;
static s32 f_PrizeShown;
static s32 f_PointsEarned;
static s32 f_PointsShown;
static s32 f_PortraitAnimations[MAX_PLAYER_COUNT];
static s32 f_PortraitFrames[MAX_PLAYER_COUNT];
static s32 f_PortraitTimers[MAX_PLAYER_COUNT];

/* The course points that open the courses up to courseIndex, together */
s32 SumCoursePointsNeeded(CourseIndex courseIndex) {
  s32 total = 0;
  s32 i;

  for (i = 1; i < courseIndex + 1; i++) {
    total += GetCoursePointsNeeded(i);
  }

  return total;
}

/* Loads the results and slides them in, or with CLOSE_ROUND_RESULTS frees them
 */
void OpenRoundResults(s32 request) {
  if (request == CLOSE_ROUND_RESULTS) {
    FreeRoundResults();
    return;
  }

  LoadRoundResults();
  f_ResultsSlide = SCREEN_WIDTH;
  f_ResultsState = RESULTS_SLIDING_IN;
}

#ifdef NON_MATCHING
/*
 * Adds to the save the average points of the players that have them,
 * scaled by the course's rate. A macro, since the original repeats it
 * for each mode.
 */
#define AddRoundPoints(save) \
  { \
    s32 counted_; \
    s32 total_; \
    s32 i_; \
\
    f_OpenCourseCount = CountOpenCourses((save)->coursePoints); \
    if (f_OpenCourseCount < COURSE_COUNT) { \
      counted_ = 0; \
      f_PointsToNextCourse = \
          GetCoursePointsNeeded(f_OpenCourseCount) - (save)->coursePoints; \
      f_PointsEarned = 0; \
      for (i_ = 0; i_ != g_GameState.playerCount; i_++) { \
        if (f_PlayerPoints[i_] >= 0) { \
          counted_++; \
          f_PointsEarned += f_PlayerPoints[i_]; \
        } \
      } \
      if (counted_ == 0) { \
        counted_ = 1; \
      } \
      f_PointsEarned = f_PointsEarned * \
          f_CoursePointRates[GetCourseRecordIndex()] / counted_; \
      f_PointsShown = f_PointsEarned; \
      f_PointsToNextCourse -= f_PointsEarned; \
    } \
    total_ = (save)->coursePoints + f_PointsEarned; \
    if (total_ > MAX_COURSE_POINTS) { \
      (save)->coursePoints = MAX_COURSE_POINTS; \
    } else { \
      (save)->coursePoints = total_; \
    } \
  }

/* Doesn't match: 231 words differ. */
void LoadRoundResults(void) {
  EncodedAsset asset;
  AssetFile file;
  SaveData* save;
  s32 i;
  s32 j;
  s32* rank;
  u32 size;

  save = GetSaveData();
  if (f_AreResultsLoaded == TRUE) {
    return;
  }

  StopSound(RAIN_SOUND_CHANNEL);
  f_AreResultsLoaded = TRUE;
  f_UnlockNoticeKind = UNLOCK_NOTICE_KIND_NONE;
  f_BannerState = BANNER_HIDDEN;
  f_BannerKind = BANNER_KIND_NONE;
  f_TallySpeed = NORMAL_TALLY_SPEED;
  f_IsAwaitingConfirm = FALSE;
  if (IsGameFlagSet(GAME_FLAG_ADD_500_POINTS)) {
    g_Players[0].points += SMALL_DEBUG_POINTS_BONUS;
  }

  if (IsGameFlagSet(GAME_FLAG_ADD_9999_POINTS)) {
    g_Players[0].points += LARGE_DEBUG_POINTS_BONUS;
  }

  f_ResultsBackdropImage = AllocMemory(OpenEncodedAsset(
      g_GameState.isClubTournament ? CLUB_TOURNAMENT_BACKDROP_ASSET
                                   : RESULTS_BACKDROP_ASSET,
      &asset));
  ReadEncodedAsset(f_ResultsBackdropImage, &asset);
  f_ResultsHandImage =
      AllocMemory(OpenEncodedAsset(ASSET_PRESSING_HAND, &asset));
  ReadEncodedAsset(f_ResultsHandImage, &asset);
  f_DigitsImage = AllocMemory(OpenEncodedAsset(ASSET_DISTANCE_DIGITS, &asset));
  ReadEncodedAsset(f_DigitsImage, &asset);
  f_PlaceDigitsImage =
      AllocMemory(OpenEncodedAsset(ASSET_PLACE_DIGITS, &asset));
  ReadEncodedAsset(f_PlaceDigitsImage, &asset);
  if (g_GameState.isClubTournament) {
    f_NintendoBadgeImage =
        AllocMemory(OpenEncodedAsset(ASSET_NINTENDO_BADGE, &asset));
    ReadEncodedAsset(f_NintendoBadgeImage, &asset);
    f_ResultsCourseImage = AllocMemory(OpenEncodedAsset(
        g_TournamentIconAssets[GetClubTournamentCourse()], &asset));
    ReadEncodedAsset(f_ResultsCourseImage, &asset);
  } else {
    f_FinalResultsTitleImage =
        AllocMemory(OpenEncodedAsset(FINAL_RESULTS_TITLE_ASSET, &asset));
    ReadEncodedAsset(f_FinalResultsTitleImage, &asset);
    f_ResultsLabelsImage =
        AllocMemory(OpenEncodedAsset(ASSET_SCORE_LABELS, &asset));
    ReadEncodedAsset(f_ResultsLabelsImage, &asset);
    f_ResultsLargeDigitsImage =
        AllocMemory(OpenEncodedAsset(ASSET_LARGE_DIGITS, &asset));
    ReadEncodedAsset(f_ResultsLargeDigitsImage, &asset);
    f_ResultsCloudImage =
        AllocMemory(OpenEncodedAsset(ASSET_WIDE_CLOUD, &asset));
    ReadEncodedAsset(f_ResultsCloudImage, &asset);
    f_ResultsCourseImage =
        AllocMemory(OpenEncodedAsset(g_TournamentIconAssets[0], &asset));
    ReadEncodedAsset(f_ResultsCourseImage, &asset);
    f_NoticeTitleImage =
        AllocMemory(OpenEncodedAsset(COURSE_NOTICE_TITLE_ASSET, &asset));
    ReadEncodedAsset(f_NoticeTitleImage, &asset);
    f_ResultsShineImage =
        AllocMemory(OpenEncodedAsset(ASSET_SHINE_IMAGE, &asset));
    ReadEncodedAsset(f_ResultsShineImage, &asset);
    f_UnusedResultsImage =
        AllocMemory(OpenEncodedAsset(RESULTS_UNK72C_IMAGE_ASSET, &asset));
    ReadEncodedAsset(f_UnusedResultsImage, &asset);
    OpenAssetFile(RESULTS_UNK72D_PALETTE_ASSET, &file);
    size = GetAssetFileSize(&file);
    f_UnusedResultsPalette = AllocMemory(size);
    ReadAssetFile(f_UnusedResultsPalette, size, &file);
  }

  for (i = 0; i != g_GameState.playerCount; i++) {
    f_PlayerPortraitImages[i] = AllocMemory(OpenEncodedAsset(
        g_CharacterImageAssets[(s8)g_Players[i].characterIndex], &asset));
    ReadEncodedAsset(f_PlayerPortraitImages[i], &asset);
    OpenAssetFile(g_CharacterPaletteAssets[(s8)g_Players[i].characterIndex] +
            (s8)g_Players[i].paletteIndex,
        &file);
    size = GetAssetFileSize(&file);
    IN_OWN_BLOCK(f_PlayerPortraitPalettes[i] = AllocMemory(size));
    ReadAssetFile(f_PlayerPortraitPalettes[i], size, &file);
    if (!g_Players[i].isComputer) {
      f_PlayerPoints[i] = g_Players[i].points;
    } else {
      f_PlayerPoints[i] = -1;
    }
    f_PlayerBetTotals[i] = 0;
    for (j = 0; j != BET_COUNT; j++) {
      f_PlayerBetTotals[i] += g_Players[i].betTallies[j];
    }
  }

  if (!g_GameState.isClubTournament) {
    if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
      f_ResultsPlacesImage =
          AllocMemory(OpenEncodedAsset(MATCH_PLACES_IMAGE_ASSET, &asset));
      ReadEncodedAsset(f_ResultsPlacesImage, &asset);
      f_SecondBannerImage =
          AllocMemory(OpenEncodedAsset(MATCH_UNK6A3_ASSET, &asset));
      ReadEncodedAsset(f_SecondBannerImage, &asset);
      f_NameplateImage =
          AllocMemory(OpenEncodedAsset(ASSET_WOODEN_PLAQUE, &asset));
      ReadEncodedAsset(f_NameplateImage, &asset);
      f_MatchStandingWordsImage =
          AllocMemory(OpenEncodedAsset(MATCH_STANDING_WORDS_ASSET, &asset));
      ReadEncodedAsset(f_MatchStandingWordsImage, &asset);
      f_PortraitCharacterIndex = (s8)g_Players[1].characterIndex;
    } else {
      f_ResultsPlacesImage =
          AllocMemory(OpenEncodedAsset(ASSET_PLACES, &asset));
      ReadEncodedAsset(f_ResultsPlacesImage, &asset);
    }
  }

  f_HasBets = FALSE;
  for (i = 0; i != BET_COUNT; i++) {
    if (g_GameState.betRates[i] > 0) {
      f_HasBets = TRUE;
    }
  }

  for (i = 0; i != g_GameState.playerCount; i++) {
    f_PortraitAnimations[i] = 0;
    f_PortraitFrames[i] = -1;
    f_PortraitTimers[i] = 0;
  }

  if (g_GameState.isClubTournament) {
    for (i = 0; i != g_GameState.playerCount; i++) {
      f_PlayerResults[i] =
          GetNetScoreToPar(GetFirstHoleIndex() + GetRoundHoleCount(), i);
    }
  } else {
    switch (g_GameState.gameMode) {
      case GAME_MODE_STROKE:
      case GAME_MODE_SPEED_GOLF:
        for (i = 0; i != g_GameState.playerCount; i++) {
          f_PlayerResults[i] =
              GetNetScoreToPar(GetFirstHoleIndex() + GetRoundHoleCount(), i);
        }

        for (i = 0; i != g_GameState.playerCount; i++) {
          rank = &f_PlayerRanks[i];
          *rank = 0;
          for (j = 0; j != g_GameState.playerCount; j++) {
            if (i != j && f_PlayerResults[i] > f_PlayerResults[j]) {
              (*rank)++;
            }
          }
        }

        AddRoundPoints(save);
        break;
      case GAME_MODE_MATCH_GAME:
      case GAME_MODE_GET_CHARACTER:
        f_PlayerResults[1] = 0;
        f_PlayerResults[0] = 0;
        for (i = GetFirstHoleIndex(); i != g_GameState.holeIndex; i++) {
          if (g_Players[0].holes[i].strokes < g_Players[1].holes[i].strokes) {
            f_PlayerResults[0]++;
            f_PlayerResults[1]--;
          } else if (g_Players[1].holes[i].strokes <
              g_Players[0].holes[i].strokes) {
            f_PlayerResults[0]--;
            f_PlayerResults[1]++;
          }
        }

        if (f_PlayerResults[0] > f_PlayerResults[1]) {
          f_PlayerRanks[0] = MATCH_RANK_WON;
          f_PlayerRanks[1] = MATCH_RANK_LOST;
        } else {
          f_PlayerRanks[0] = MATCH_RANK_LOST;
          f_PlayerRanks[1] = MATCH_RANK_WON;
        }

        f_PortraitImage = AllocMemory(OpenEncodedAsset(
            g_CharacterImageAssets[(s8)g_Players[1].characterIndex], &asset));
        ReadEncodedAsset(f_PortraitImage, &asset);
        OpenAssetFile(
            g_CharacterPaletteAssets[(s8)g_Players[1].characterIndex] +
                (s8)g_Players[1].paletteIndex,
            &file);
        size = GetAssetFileSize(&file);
        f_PortraitPalette = AllocMemory(size);
        ReadAssetFile(f_PortraitPalette, size, &file);
        AddRoundPoints(save);
        break;
      case GAME_MODE_UNK1:
      case GAME_MODE_TRAINING:
      case GAME_MODE_UNK6:
      case GAME_MODE_UNK8:
      case GAME_MODE_RING_SHOT:
      case GAME_MODE_SKINS_MATCH:
      case GAME_MODE_CLUB_SLOTS:
        TallySkins(GetFirstHoleIndex() + GetRoundHoleCount());
        for (i = 0; i != g_GameState.playerCount; i++) {
          f_PlayerResults[i] = g_RoundHud.skinsTotals[i];
        }

        for (i = 0; i != g_GameState.playerCount; i++) {
          rank = &f_PlayerRanks[i];
          *rank = 0;
          for (j = 0; j != g_GameState.playerCount; j++) {
            if (i != j && f_PlayerResults[i] < f_PlayerResults[j]) {
              (*rank)++;
            }
          }
        }

        AddRoundPoints(save);
        break;
      case GAME_MODE_TOURNAMENT:
        f_SecondBannerImage =
            AllocMemory(OpenEncodedAsset(TOURNAMENT_UNK6A1_ASSET, &asset));
        ReadEncodedAsset(f_SecondBannerImage, &asset);
        f_PlayerResults[0] =
            GetNetScoreToPar(GetFirstHoleIndex() + GetRoundHoleCount(), 0);
        RankTournament(
            COURSE_HOLE_COUNT, 0, f_PlayerResults[0], g_GameState.unk2C);
        for (i = 0; i != TOURNAMENT_CONTESTANT_COUNT; i++) {
          if (g_TournamentStandings[i].contestantId == -1) {
            f_PlayerRanks[0] = g_TournamentStandings[i].place & PLACE_MASK;
            break;
          }
        }

        if (f_PlayerRanks[0] <= TROPHY_PLACE_COUNT) {
          f_PrizePoints =
              f_TournamentPrizes[g_GameState.courseIndex][f_PlayerRanks[0] - 1];
        } else if (f_PlayerRanks[0] <= PRIZE_TOP_TEN) {
          f_PrizePoints = f_TournamentPrizes[g_GameState.courseIndex]
                                            [PRIZE_BRACKET_TOP_TEN];
        } else if (f_PlayerRanks[0] <= PRIZE_TOP_TWENTY) {
          f_PrizePoints = f_TournamentPrizes[g_GameState.courseIndex]
                                            [PRIZE_BRACKET_TOP_TWENTY];
        } else {
          f_PrizePoints =
              f_TournamentPrizes[g_GameState.courseIndex][PRIZE_BRACKET_REST];
        }

        f_PrizeShown = f_PrizePoints;
        if (f_PlayerRanks[0] <= TROPHY_PLACE_COUNT) {
          f_TrophyImage = AllocMemory(OpenEncodedAsset(
              f_TrophyAssets[g_GameState.courseIndex].image, &asset));
          ReadEncodedAsset(f_TrophyImage, &asset);
          OpenAssetFile(f_TrophyAssets[g_GameState.courseIndex].palette +
                  f_PlayerRanks[0] - 1,
              &file);
          size = GetAssetFileSize(&file);
          f_TrophyPalette = AllocMemory(size);
          ReadAssetFile(f_TrophyPalette, size, &file);
        }

        f_PointsEarned =
            f_PlayerPoints[0] * f_CoursePointRates[g_GameState.courseIndex];
        f_PointsShown = f_PointsEarned;
        f_OpenCourseCount = CountOpenCourses(save->coursePoints);
        f_TournamentOpenCourseCount = f_OpenCourseCount;
        if (f_OpenCourseCount < COURSE_COUNT) {
          f_PointsToNextCourse = GetCoursePointsNeeded(f_OpenCourseCount) -
              save->coursePoints - f_PrizePoints - f_PointsEarned;
        }

        if (save->coursePoints + f_PrizePoints + f_PointsEarned >
            MAX_COURSE_POINTS) {
          save->coursePoints = MAX_COURSE_POINTS;
        } else {
          save->coursePoints += f_PrizePoints + f_PointsEarned;
        }

        break;
    }
  }

  for (i = 0; i != g_GameState.playerCount; i++) {
    if ((s8)g_Players[i].characterIndex >= CHARACTER_KID &&
        f_PlayerPoints[i] > 0) {
      AddGameBoyExperience(
          g_Players[i].gameBoyCharacterIndex, f_PlayerPoints[i]);
    }
  }

  switch (g_GameState.gameMode) {
    case GAME_MODE_STROKE:
      PlaySong(SONG_RESULTS, 0);
      break;
    case GAME_MODE_SPEED_GOLF:
      PlaySong(SONG_RESULTS, 0);
      break;
    case GAME_MODE_MATCH_GAME:
      PlaySong(SONG_RESULTS, 0);
      break;
    case GAME_MODE_GET_CHARACTER:
      PlaySong(SONG_RESULTS, 0);
      break;
    case GAME_MODE_SKINS_MATCH:
    case GAME_MODE_CLUB_SLOTS:
      PlaySong(SONG_RESULTS, 0);
      break;
    case GAME_MODE_TOURNAMENT:
      PlaySong(SONG_TOURNAMENT_RESULTS, 0);
      break;
  }
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", LoadRoundResults);
#endif

/*
 * Loads the notice of what the round unlocked: one of the characters for
 * UNLOCK_NOTICE_* below UNLOCK_NOTICE_COURSES, or else the course from
 * UNLOCK_NOTICE_COURSES on.
 */
void LoadUnlockNotice(s32 unlock) {
  EncodedAsset asset;
  AssetFile file;
  CourseIndex courseIndex;
  u32 size;

  GetSaveData();
  f_IsNoticeHeld = FALSE;
  f_NoticeTime = 0;
  f_NoticeFaceIndex = 0;
  f_NoticeLift = 0;
  f_NoticeUnk6C = 0;
  f_NoticeFrameCount = 0;
  f_NoticeStage = 0;
  f_NoticeSwing = 0;
  f_NoticeUnk54 = 0;
  f_NoticeAngle = 0;
  courseIndex = 0;
  if (unlock == UNLOCK_NOTICE_DONKEY_KONG) {
    f_PortraitCharacterIndex = CHARACTER_DONKEY_KONG;
    f_UnlockNoticeKind = UNLOCK_NOTICE_KIND_CHARACTER;
  } else if (unlock == UNLOCK_NOTICE_MAPLE) {
    f_PortraitCharacterIndex = CHARACTER_MAPLE;
    f_UnlockNoticeKind = UNLOCK_NOTICE_KIND_CHARACTER;
  } else if (unlock == UNLOCK_NOTICE_METAL_MARIO) {
    f_PortraitCharacterIndex = CHARACTER_METAL_MARIO;
    f_UnlockNoticeKind = UNLOCK_NOTICE_KIND_CHARACTER;
  } else {
    courseIndex = unlock - UNLOCK_NOTICE_COURSES;
    f_UnlockNoticeKind = UNLOCK_NOTICE_KIND_COURSE;
  }

  f_ResultsShineImage =
      AllocMemory(OpenEncodedAsset(ASSET_SHINE_IMAGE, &asset));
  ReadEncodedAsset(f_ResultsShineImage, &asset);
  if (f_UnlockNoticeKind == UNLOCK_NOTICE_KIND_COURSE) {
    f_NoticeTitleImage =
        AllocMemory(OpenEncodedAsset(COURSE_NOTICE_TITLE_ASSET, &asset));
    ReadEncodedAsset(f_NoticeTitleImage, &asset);
    f_ResultsCourseImage = AllocMemory(
        OpenEncodedAsset(f_CourseNoticeAssets[courseIndex], &asset));
    ReadEncodedAsset(f_ResultsCourseImage, &asset);
  }

  if (f_UnlockNoticeKind == UNLOCK_NOTICE_KIND_CHARACTER) {
    f_NoticeTitleImage =
        AllocMemory(OpenEncodedAsset(CHARACTER_NOTICE_TITLE_ASSET, &asset));
    ReadEncodedAsset(f_NoticeTitleImage, &asset);
    f_NameplateImage =
        AllocMemory(OpenEncodedAsset(ASSET_WOODEN_PLAQUE, &asset));
    ReadEncodedAsset(f_NameplateImage, &asset);
    f_PortraitImage = AllocMemory(OpenEncodedAsset(
        g_CharacterImageAssets[f_PortraitCharacterIndex], &asset));
    ReadEncodedAsset(f_PortraitImage, &asset);
    OpenAssetFile(g_CharacterPaletteAssets[f_PortraitCharacterIndex], &file);
    size = GetAssetFileSize(&file);
    f_PortraitPalette = AllocMemory(size);
    ReadAssetFile(f_PortraitPalette, size, &file);
  }
}

void FreeUnlockNotice(void) {
  FreeMemory(&f_NoticeTitleImage);
  FreeMemory(&f_ResultsShineImage);
  if (f_UnlockNoticeKind == UNLOCK_NOTICE_KIND_COURSE) {
    FreeMemory(&f_ResultsCourseImage);
  }

  if (f_UnlockNoticeKind == UNLOCK_NOTICE_KIND_CHARACTER) {
    FreeMemory(&f_NameplateImage);
    FreeMemory(&f_PortraitImage);
    FreeMemory(&f_PortraitPalette);
  }

  f_UnlockNoticeKind = UNLOCK_NOTICE_KIND_NONE;
}

/* Swings the course in, or raises the character's face; returns whether the
 * notice is done */
s32 UpdateUnlockNotice(void) {
  EncodedAsset asset;

  if (f_UnlockNoticeKind == UNLOCK_NOTICE_KIND_COURSE) {
    f_NoticeFrameCount++;
    switch (f_NoticeStage) {
      case 0:
        f_NoticeSwing = -sinf(f_NoticeAngle) * NOTICE_SWING_HEIGHT;
        f_NoticeUnk54 = 0;
        f_NoticeAngle += NOTICE_SWING_STEP;
        if (f_NoticeAngle > M_PI) {
          f_NoticeAngle = M_PI;
        }

        if (f_NoticeFrameCount >= NOTICE_SWING_FRAMES) {
          f_NoticeStage = 1;
        }

        break;
      case 1:
        f_NoticeSwing = -sinf(f_NoticeAngle) * NOTICE_SWING_HEIGHT;
        f_NoticeAngle += NOTICE_SWING_STEP;
        if (f_NoticeFrameCount < NOTICE_DONE_FRAME) {
          break;
        }

        return TRUE;
    }
  } else {
    f_NoticeTime++;
    if (f_IsNoticeHeld) {
      return FALSE;
    }

    if (f_NoticeTime > NOTICE_FACE_DELAY &&
        !(f_NoticeTime & NOTICE_FACE_INTERVAL_MASK) &&
        f_NoticeFaceIndex < NOTICE_LAST_FACE) {
      f_NoticeFaceIndex++;
      OpenEncodedAsset(
          g_CharacterImageAssets[f_PortraitCharacterIndex] + f_NoticeFaceIndex,
          &asset);
      ReadEncodedAsset(f_PortraitImage, &asset);
    }

    if (f_NoticeTime > NOTICE_LIFT_DELAY) {
      if (f_NoticeLift > NOTICE_LIFT_START_LIMIT) {
        f_NoticeLift -= NOTICE_LIFT_START_SPEED;
      } else {
        f_NoticeLift += f_NoticeLift * NOTICE_LIFT_GROWTH_NUMERATOR /
            NOTICE_LIFT_GROWTH_DENOMINATOR;
      }

      if (f_NoticeLift < NOTICE_LIFT_EXIT) {
        return TRUE;
      }
    }
  }

  return FALSE;
}

void FreeRoundResults(void) {
  s32 i;

  if (f_AreResultsLoaded) {
    f_AreResultsLoaded = FALSE;
    FreeMemory(&f_ResultsBackdropImage);
    FreeMemory(&f_ResultsHandImage);
    FreeMemory(&f_DigitsImage);
    FreeMemory(&f_PlaceDigitsImage);
    if (g_GameState.isClubTournament) {
      FreeMemory(&f_NintendoBadgeImage);
      FreeMemory(&f_ResultsCourseImage);
    } else {
      FreeMemory(&f_FinalResultsTitleImage);
      FreeMemory(&f_ResultsLabelsImage);
      FreeMemory(&f_ResultsLargeDigitsImage);
      FreeMemory(&f_ResultsCloudImage);
      FreeMemory(&f_ResultsCourseImage);
      FreeMemory(&f_NoticeTitleImage);
      FreeMemory(&f_ResultsShineImage);
      FreeMemory(&f_UnusedResultsImage);
      FreeMemory(&f_UnusedResultsPalette);
    }

    for (i = 0; i != g_GameState.playerCount; i++) {
      FreeMemory(&f_PlayerPortraitImages[i]);
      FreeMemory(&f_PlayerPortraitPalettes[i]);
    }

    if (!g_GameState.isClubTournament) {
      if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
        FreeMemory(&f_ResultsPlacesImage);
        FreeMemory(&f_SecondBannerImage);
        FreeMemory(&f_NameplateImage);
        FreeMemory(&f_MatchStandingWordsImage);
      } else {
        FreeMemory(&f_ResultsPlacesImage);
      }

      switch (g_GameState.gameMode) {
        case GAME_MODE_STROKE:
        case GAME_MODE_UNK1:
        case GAME_MODE_TRAINING:
        case GAME_MODE_UNK6:
        case GAME_MODE_SPEED_GOLF:
        case GAME_MODE_UNK8:
        case GAME_MODE_RING_SHOT:
        case GAME_MODE_SKINS_MATCH:
        case GAME_MODE_CLUB_SLOTS:
          break;
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_GET_CHARACTER:
          FreeMemory(&f_PortraitImage);
          FreeMemory(&f_PortraitPalette);
          break;
        case GAME_MODE_TOURNAMENT:
          FreeMemory(&f_SecondBannerImage);
          if (f_PlayerRanks[0] <= TROPHY_PLACE_COUNT) {
            FreeMemory(&f_TrophyImage);
            FreeMemory(&f_TrophyPalette);
          }

          break;
      }
    }
  }
}

void StartPortraitAnimation(s32 animation, s32 playerIndex) {
  f_PortraitAnimations[playerIndex] = animation;
  f_PortraitFrames[playerIndex] = 0;
  f_PortraitTimers[playerIndex] = 0;
}

/* Ticks as the points count up, on every fourth call at the normal speed */
void PlayTallySound(void) {
  f_TallySoundCount++;
  if (f_TallySpeed != NORMAL_TALLY_SPEED ||
      !(f_TallySoundCount & TALLY_SOUND_INTERVAL_MASK)) {
    PlaySound(SOUND_TICK, g_NextSoundChannel, SOUND_FULL_SCALE);
  }
}

/* Draws value right-aligned in four of the 8 by 12 digits of f_DigitsImage */
void DrawResultsNumber(Gfx** displayListHead, s32 x, s32 y, s32 value) {
  Gfx* gfx = *displayListHead;
  u8* c;
  u8 ch;
  s32 offset;

  sprintf(g_PrintBuffer, f_NumberFormat, value);
  LoadSmallPalette(&gfx, 0, ImageBody(f_DigitsImage));
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTextureBlock_4b(gfx++, TextureAddress(ImageTexels(f_DigitsImage)),
      G_IM_FMT_CI, DISTANCE_FONT_WIDTH, DISTANCE_GLYPH_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  c = (u8*)g_PrintBuffer;
  offset = 0;
  while (*c != '\0') {
    ch = *c;
    if (ch != ' ') {
      gSPScisTextureRectangle(gfx++, QuarterPixels(x + offset),
          QuarterPixels(y), QuarterPixels(x + offset + DISTANCE_DIGIT_WIDTH),
          QuarterPixels(y + DISTANCE_GLYPH_HEIGHT), G_TX_RENDERTILE,
          TexelCoordinate((ch - '0') * DISTANCE_DIGIT_WIDTH), 0, TEXEL_STEP_ONE,
          TEXEL_STEP_ONE);
      END_BASIC_BLOCK();
    }

    c++;
    offset += DISTANCE_DIGIT_WIDTH;
  }

  *displayListHead = gfx;
}

#define PLACE_NUMBER_X_OFFSET (-12)

/* Loads glyph from f_PlaceDigitsImage and draws it at x, y */
#define DrawPlaceGlyph(gfx, x, y, glyph) \
  { \
    gDPLoadTextureTile((gfx)++, \
        TextureAddress(LargeImageTexels(f_PlaceDigitsImage)), G_IM_FMT_CI, \
        G_IM_SIZ_8b, PLACE_GLYPH_SIZE, 0, 0, (glyph) * PLACE_GLYPH_SIZE, \
        PLACE_GLYPH_SIZE - 1, \
        ((glyph) * PLACE_GLYPH_SIZE) + PLACE_GLYPH_SIZE - 1, 0, \
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, \
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD); \
    gSPScisTextureRectangle((gfx)++, QuarterPixels(x), QuarterPixels(y), \
        ((x) + PLACE_GLYPH_SIZE) << 2, QuarterPixels((y) + PLACE_GLYPH_SIZE), \
        G_TX_RENDERTILE, 0, TexelCoordinate((glyph) * PLACE_GLYPH_SIZE), \
        TEXEL_STEP_ONE, TEXEL_STEP_ONE); \
  }

/* Draws place, as in "21st", centered on x */
void DrawPlaceNumber(Gfx** displayListHead, s32 x, s32 y, s32 place) {
  Gfx* gfx = *displayListHead;
  char* c;
  s32 width;
  s32 suffix;
  s32 lastDigit;
  u8 character;
  s32 glyph;

  sprintf(g_PrintBuffer, f_PlaceNumberFormat, place);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  LoadLargePalette(&gfx, ImageBody(f_PlaceDigitsImage));
  width = 0;
  for (c = g_PrintBuffer; *c != '\0'; c++) {
    width += PLACE_GLYPH_SPACING;
  }

  x += PLACE_NUMBER_X_OFFSET - (width / 2);
  width = 0;
  for (c = g_PrintBuffer; *c != '\0'; c++, width += PLACE_GLYPH_SPACING) {
    character = *c;
    if (character == ' ') {
      continue;
    }
    glyph = character - '0';
    DrawPlaceGlyph(gfx, x + width, y, glyph);
  }

  suffix = place;
  if (suffix > PLACES_PER_HUNDRED) {
    suffix %= PLACES_PER_HUNDRED;
  }

  suffix--;
  if (suffix > PLACE_SUFFIX_TH - 1) {
    if (suffix < PLACE_TEENS_END) {
      suffix = PLACE_SUFFIX_TH;
    } else if (suffix < PLACES_PER_HUNDRED) {
      lastDigit = suffix % DECIMAL_BASE;
      if (lastDigit < PLACE_SUFFIX_TH) {
        suffix = lastDigit;
      } else {
        suffix = PLACE_SUFFIX_TH;
      }
    }
  }

  DrawPlaceGlyph(gfx, x + width, y, suffix + PLACE_SUFFIX_GLYPH_FIRST);
  *displayListHead = gfx;
}

void DrawPlaceIcon(Gfx** displayListHead, s32 x, s32 y, s32 place) {
  Gfx* gfx = *displayListHead;

  LoadLargePalette(&gfx, ImageBody(f_ResultsPlacesImage));
  LoadTextureBlock(&gfx,
      f_ResultsPlacesImage +
          ((place * PLACE_ICON_SIZE * PLACE_ICON_SIZE) +
              LARGE_PALETTED_TEXELS_OFFSET),
      G_IM_FMT_CI, G_IM_SIZ_8b, PLACE_ICON_SIZE, PLACE_ICON_SIZE, 0, 0, 0, 0, 0,
      0, 0);
  DrawTextureRect(&gfx, QuarterPixels(x), QuarterPixels(y),
      QuarterPixels(x + PLACE_ICON_SIZE), QuarterPixels(y + PLACE_ICON_SIZE),
      G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  *displayListHead = gfx;
}

void DrawRank(Gfx** displayListHead, s32 x, s32 y, s32 rank);

/* Ranks from this one draw a banner instead */
#define RANK_BANNER 100
#define RANK_BANNER_WIDTH 96
#define RANK_BANNER_STRIP_HEIGHT 16
#define RANK_BANNER_STRIP_COUNT 3
/*
 * DrawRank's ranks count from 0 for the 1st place. From the 4th, it draws
 * the place as digits and a suffix icon: st, nd, rd or th.
 */
#define RANK_FOURTH 3
#define RANK_TWENTY_FIRST 20
#define RANK_TWENTY_FOURTH 23
#define PLACE_DIGIT_WIDTH 24
#define PLACE_DIGIT_SPACING 20
#define PLACE_DIGIT_X 4
#define PLACE_DIGIT_Y 10
/* Two-digit places start further left, from the 10th */
#define RANK_TENTH 9
#define PLACE_TWO_DIGIT_SHIFT 16
/* The suffix icons follow the 1st to 4th place icons in the image */
#define PLACE_SUFFIX_ICON_FIRST 4
#define RANK_BANNER_X_OFFSET (-12)

/*
 * Draws the place icon for a rank. With RANK_AS_PLACE, places past 3rd
 * are written as digits followed by a suffix.
 */
void DrawRank(Gfx** displayListHead, s32 x, s32 y, s32 rank) {
  Gfx* gfx = *displayListHead;
  s32 suffix;
  s32 digitX;
  s32 digit;
  u8* image;
  s32 i;
  s32 stripY;
  s32 offset;

  if (rank & RANK_AS_PLACE) {
    rank &= ~RANK_AS_PLACE;
    suffix = rank;
    if (rank >= RANK_FOURTH) {
      if (rank < RANK_TWENTY_FIRST) {
        suffix = PLACE_SUFFIX_TH;
      } else if (rank < RANK_TWENTY_FOURTH) {
        suffix = rank - RANK_TWENTY_FIRST;
      } else {
        suffix = PLACE_SUFFIX_TH;
      }
    }

    LoadLargePalette(&gfx, ImageBody(f_ResultsPlacesImage));
    LoadTextureBlock(&gfx,
        f_ResultsPlacesImage +
            ((suffix * PLACE_ICON_SIZE * PLACE_ICON_SIZE) +
                LARGE_PALETTED_TEXELS_OFFSET +
                (PLACE_SUFFIX_ICON_FIRST * PLACE_ICON_SIZE * PLACE_ICON_SIZE)),
        G_IM_FMT_CI, G_IM_SIZ_8b, PLACE_ICON_SIZE, PLACE_ICON_SIZE, 0, 0, 0, 0,
        0, 0, 0);
    DrawTextureRect(&gfx, QuarterPixels(x + PLACE_DIGIT_WIDTH),
        QuarterPixels(y),
        QuarterPixels(x + PLACE_DIGIT_WIDTH + PLACE_ICON_SIZE),
        QuarterPixels(y + PLACE_ICON_SIZE), G_TX_RENDERTILE, 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    if (rank >= RANK_FOURTH) {
      sprintf(g_PrintBuffer, f_PlaceNumberFormat, rank + 1);
      LoadSmallPalette(&gfx, 0, ImageBody(f_SecondBannerImage));
      if (rank >= RANK_TENTH) {
        x -= PLACE_TWO_DIGIT_SHIFT;
      }

      for (i = 0; g_PrintBuffer[i] != '\0'; i++) {
        digit = g_PrintBuffer[i] - '0';
        digitX = (i * PLACE_DIGIT_SPACING) + PLACE_DIGIT_X;
        gDPLoadTextureTile_4b(gfx++,
            TextureAddress(ImageTexels(f_SecondBannerImage)), G_IM_FMT_CI,
            PLACE_DIGIT_WIDTH, 0, 0, digit * PLACE_DIGIT_WIDTH,
            PLACE_DIGIT_WIDTH - 1,
            digit * PLACE_DIGIT_WIDTH + PLACE_DIGIT_WIDTH - 1, 0,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        DrawTextureRect(&gfx, QuarterPixels(x + digitX),
            QuarterPixels(y + PLACE_DIGIT_Y),
            QuarterPixels(x + digitX + PLACE_DIGIT_WIDTH),
            QuarterPixels(y + PLACE_DIGIT_Y + PLACE_DIGIT_WIDTH),
            G_TX_RENDERTILE, 0, TexelCoordinate(digit * PLACE_DIGIT_WIDTH),
            TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      }
    } else {
      DrawPlaceIcon(&gfx, x, y, rank);
    }
  } else if (rank >= RANK_BANNER) {
    if (rank == RANK_BANNER) {
      image = f_ResultsPlacesImage;
    } else {
      image = f_SecondBannerImage;
    }

    LoadLargePalette(&gfx, ImageBody(image));
    for (i = 0; i != RANK_BANNER_STRIP_COUNT; i++) {
      offset = LARGE_PALETTED_TEXELS_OFFSET +
          (i * RANK_BANNER_WIDTH * RANK_BANNER_STRIP_HEIGHT);
      stripY = y + (i * RANK_BANNER_STRIP_HEIGHT);
      LoadTextureBlock(&gfx, image + offset, G_IM_FMT_CI, G_IM_SIZ_8b,
          RANK_BANNER_WIDTH, RANK_BANNER_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      DrawTextureRect(&gfx, QuarterPixels(x + RANK_BANNER_X_OFFSET),
          QuarterPixels(stripY),
          QuarterPixels(x + RANK_BANNER_X_OFFSET + RANK_BANNER_WIDTH),
          QuarterPixels(stripY + RANK_BANNER_STRIP_HEIGHT), G_TX_RENDERTILE, 0,
          0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  } else {
    LoadLargePalette(&gfx, ImageBody(f_ResultsPlacesImage));
    LoadTextureBlock(&gfx,
        f_ResultsPlacesImage +
            ((rank * PLACE_ICON_SIZE * PLACE_ICON_SIZE) +
                LARGE_PALETTED_TEXELS_OFFSET +
                (PLACE_SUFFIX_ICON_FIRST * PLACE_ICON_SIZE * PLACE_ICON_SIZE)),
        G_IM_FMT_CI, G_IM_SIZ_8b, PLACE_ICON_SIZE, PLACE_ICON_SIZE, 0, 0, 0, 0,
        0, 0, 0);
    DrawTextureRect(&gfx, QuarterPixels(x + PLACE_DIGIT_WIDTH),
        QuarterPixels(y),
        QuarterPixels(x + PLACE_DIGIT_WIDTH + PLACE_ICON_SIZE),
        QuarterPixels(y + PLACE_ICON_SIZE), G_TX_RENDERTILE, 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    DrawPlaceIcon(&gfx, x, y, rank);
  }

  *displayListHead = gfx;
}

void DrawResultsLabel(Gfx** displayListHead, s32 x, s32 y, s32 label) {
  Gfx* gfx = *displayListHead;

  LoadSmallPalette(&gfx, 0, ImageBody(f_ResultsLabelsImage));
  gDPLoadTextureTile_4b(gfx++,
      TextureAddress(f_ResultsLabelsImage +
          ((label * (RESULTS_LABEL_STORED_WIDTH * RESULTS_LABEL_HEIGHT / 2)) +
              PALETTED_TEXELS_OFFSET)),
      G_IM_FMT_CI, RESULTS_LABEL_STORED_WIDTH, 0, 0, 0, RESULTS_LABEL_WIDTH - 1,
      RESULTS_LABEL_HEIGHT - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  DrawTextureRect(&gfx, QuarterPixels(x), QuarterPixels(y),
      QuarterPixels(x + RESULTS_LABEL_WIDTH),
      QuarterPixels(y + RESULTS_LABEL_HEIGHT), G_TX_RENDERTILE, 0, 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  *displayListHead = gfx;
}

/*
 * isMirrored starts the texture a width in (S10.5 texels), where its
 * mirrored wrap flips it
 */
void DrawPlayerPortrait(
    Gfx** displayListHead, s32 x, s32 y, s32 playerIndex, Boolean isMirrored) {
  Gfx* gfx = *displayListHead;

  LoadLargePalette(&gfx, f_PlayerPortraitPalettes[playerIndex]);
  LoadTextureBlock(&gfx, f_PlayerPortraitImages[playerIndex], G_IM_FMT_CI,
      G_IM_SIZ_8b, PORTRAIT_SIZE, PORTRAIT_SIZE, 0, G_TX_MIRROR, G_TX_NOMIRROR,
      PORTRAIT_SIZE_LOG2, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  DrawTextureRect(&gfx, QuarterPixels(x), QuarterPixels(y),
      QuarterPixels(x + PORTRAIT_SIZE), QuarterPixels(y + PORTRAIT_SIZE),
      G_TX_RENDERTILE, isMirrored * TexelCoordinate(PORTRAIT_SIZE), 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  *displayListHead = gfx;
}

void StartResultsNotice(s32 kind) {
  f_UnlockNoticeKind = kind;
  switch (kind) {
    case UNLOCK_NOTICE_KIND_NONE:
      break;
    case UNLOCK_NOTICE_KIND_COURSE:
      f_NoticeStage = 0;
      f_NoticeFrameCount = 0;
      f_NoticeSwing = SCREEN_WIDTH;
      f_NoticeUnk54 = 0;
      f_NoticeAngle = NOTICE_SWING_START_ANGLE;
      break;
    case UNLOCK_NOTICE_KIND_CHARACTER:
      f_IsNoticeHeld = FALSE;
      f_NoticeTime = 0;
      f_NoticeFaceIndex = 0;
      f_NoticeLift = 0;
      f_NoticeUnk6C = 0;
      break;
  }
}

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_NumberFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_PlaceNumberFormat);

/* Swings the notice in, then counts the course it opened */
void UpdateCourseResultsNotice(void) {
  SaveData* save = GetSaveData();

  f_NoticeFrameCount++;
  switch (f_NoticeStage) {
    case 0:
      f_NoticeSwing = -sinf(f_NoticeAngle) * NOTICE_SWING_HEIGHT;
      f_NoticeUnk54 = 0;
      f_NoticeAngle += NOTICE_SWING_STEP;
      if (f_NoticeAngle > M_PI) {
        f_NoticeAngle = M_PI;
      }

      if (f_NoticeFrameCount >= NOTICE_SWING_FRAMES) {
        f_NoticeStage = 1;
      }

      break;
    case 1:
      f_NoticeSwing = -sinf(f_NoticeAngle) * NOTICE_SWING_HEIGHT;
      f_NoticeAngle += NOTICE_SWING_STEP;
      if (f_NoticeFrameCount >= NOTICE_DONE_FRAME) {
        StartResultsNotice(UNLOCK_NOTICE_KIND_NONE);
        f_OpenCourseCount++;
        if (f_OpenCourseCount < COURSE_COUNT) {
          f_PointsToNextCourse =
              GetCoursePointsNeeded(f_OpenCourseCount) - save->coursePoints;
        }
      }

      break;
  }
}

/* The course image is two strips of 64x32 that shrink from twice their size to
 * full size as the notice swings */
#define NOTICE_COURSE_STRIP_WIDTH 64
#define NOTICE_COURSE_STRIP_HEIGHT 32
#define NOTICE_COURSE_STRIP_COUNT 2
#define NOTICE_COURSE_CENTER_X 160
#define NOTICE_COURSE_CENTER_Y 128
#define NOTICE_COURSE_SCALE_BIAS 2.0f
#define NOTICE_TITLE_STRIP_WIDTH 160
#define NOTICE_TITLE_STRIP_COUNT 8
#define NOTICE_TITLE_LEFT 80
#define NOTICE_TITLE_TOP 56
#define NOTICE_TITLE_LIFT 96.0f
/* Wider than the shine texture, so that it sweeps across the title */
#define NOTICE_SHINE_DSDX ((NOTICE_SHINE_SIZE << 10) / NOTICE_TITLE_STRIP_WIDTH)
#define NOTICE_PAN_START 3
#define NOTICE_PAN_END 60
#define NOTICE_PAN_MAX 255
#define NOTICE_PAN_RANGE 126
#define NOTICE_CHEER_SOUND_CHANNEL 3
#define NOTICE_SOUND_PAN_RIGHT 255

/*
 * The notice that a course has opened: its picture and title swing in, then a
 * shine sweeps the title
 *
 * Doesn't match: 67 words differ.
 */
#ifdef NON_MATCHING
void DrawCourseResultsNotice(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 size;
  s32 step;
  s32 lift;
  s32 top;
  SoundPan pan;
  s32 centerX;
  s32 i;

  gSPDisplayList(gfx++, g_DecalTextureSetup);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

  /* Once the swing settles the picture is drawn at full size, where point
   * sampling is sharper */
  if (f_NoticeAngle == M_PI) {
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
  } else {
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
  }

  LoadLargePalette(&gfx, ImageBody(f_ResultsCourseImage));
  for (i = 0; i != NOTICE_COURSE_STRIP_COUNT; i++) {
    size = (cosf(f_NoticeAngle) + NOTICE_COURSE_SCALE_BIAS) *
        NOTICE_COURSE_STRIP_HEIGHT;
    step = (NOTICE_COURSE_STRIP_HEIGHT << 10) / size;
    LoadTextureBlock(&gfx,
        LargeImageTexels(f_ResultsCourseImage) +
            (i * NOTICE_COURSE_STRIP_WIDTH * NOTICE_COURSE_STRIP_HEIGHT),
        G_IM_FMT_CI, G_IM_SIZ_8b, NOTICE_COURSE_STRIP_WIDTH,
        NOTICE_COURSE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    top = (i * size) + NOTICE_COURSE_CENTER_Y - size;
    centerX = f_NoticeSwing + NOTICE_COURSE_CENTER_X;
    DrawTextureRect(&gfx, QuarterPixels(centerX - size),
        QuarterPixels(top + f_NoticeUnk54), QuarterPixels(size + centerX),
        QuarterPixels(top + size + f_NoticeUnk54), G_TX_RENDERTILE, 0, 0, step,
        step);
  }

  LoadLargePalette(&gfx, ImageBody(f_NoticeTitleImage));
  for (i = 0; i != NOTICE_TITLE_STRIP_COUNT; i++) {
    lift = -(s32)(cosf(f_NoticeAngle) * NOTICE_TITLE_LIFT);
    lift -= (s32)NOTICE_TITLE_LIFT;
    LoadTextureBlock(&gfx,
        LargeImageTexels(f_NoticeTitleImage) +
            (i * NOTICE_TITLE_STRIP_WIDTH * NOTICE_TITLE_STRIP_HEIGHT),
        G_IM_FMT_CI, G_IM_SIZ_8b, NOTICE_TITLE_STRIP_WIDTH,
        NOTICE_TITLE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    top = lift + NOTICE_TITLE_TOP + (i * NOTICE_TITLE_STRIP_HEIGHT);
    DrawTextureRect(&gfx, QuarterPixels(f_NoticeSwing + NOTICE_TITLE_LEFT),
        QuarterPixels(top + f_NoticeUnk54),
        QuarterPixels(
            f_NoticeSwing + NOTICE_TITLE_LEFT + NOTICE_TITLE_STRIP_WIDTH),
        QuarterPixels(top + (f_NoticeUnk54 + NOTICE_TITLE_STRIP_HEIGHT)),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  /* The title again, into the depth buffer alone */
  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PRIM);
  gDPSetPrimDepth(gfx++, NOTICE_TITLE_DEPTH, 0);
  gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 1, TEXEL0, 1, 0, 0, 0, TEXEL0, 0,
      1, TEXEL0, 1);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++,
      GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
      G_RM_AA_ZB_TEX_EDGE2);
  LoadLargePalette(&gfx, ImageBody(f_NoticeTitleImage));
  for (i = 0; i != NOTICE_TITLE_STRIP_COUNT; i++) {
    lift = -(s32)(cosf(f_NoticeAngle) * NOTICE_TITLE_LIFT);
    lift -= (s32)NOTICE_TITLE_LIFT;
    LoadTextureBlock(&gfx,
        LargeImageTexels(f_NoticeTitleImage) +
            (i * NOTICE_TITLE_STRIP_WIDTH * NOTICE_TITLE_STRIP_HEIGHT),
        G_IM_FMT_CI, G_IM_SIZ_8b, NOTICE_TITLE_STRIP_WIDTH,
        NOTICE_TITLE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    top = lift + NOTICE_TITLE_TOP + (i * NOTICE_TITLE_STRIP_HEIGHT);
    DrawTextureRect(&gfx, QuarterPixels(f_NoticeSwing + NOTICE_TITLE_LEFT),
        QuarterPixels(top + f_NoticeUnk54),
        QuarterPixels(
            f_NoticeSwing + NOTICE_TITLE_LEFT + NOTICE_TITLE_STRIP_WIDTH),
        QuarterPixels(top + (f_NoticeUnk54 + NOTICE_TITLE_STRIP_HEIGHT)),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  gDPSetPrimDepth(gfx++, NOTICE_SHINE_DEPTH, 0);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_PATTERN);
  gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
      PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
  LoadTextureBlock(&gfx, ImageBody(f_ResultsShineImage), G_IM_FMT_I,
      G_IM_SIZ_8b, NOTICE_SHINE_SIZE, NOTICE_SHINE_SIZE, 0, 0, 0, 0, 0, 0, 0);
  if (f_NoticeFrameCount == NOTICE_SHINE_START) {
    PlaySoundWithEffects(SOUND_SHRINKING_IMAGE, NOTICE_SHINE_SOUND_CHANNEL,
        NOTICE_SHINE_SOUND_VOLUME, 0, SOUND_FULL_SCALE);
  }

  if (f_NoticeFrameCount == 2) {
    PlaySoundWithEffects(SOUND_UNLOCK_EVERYTHING, NOTICE_SWING_SOUND_CHANNEL,
        NOTICE_SWING_SOUND_VOLUME, 0, NOTICE_SOUND_PAN_RIGHT);
    PlaySound(SOUND_COURSE_NOTICE_CHEER, NOTICE_CHEER_SOUND_CHANNEL,
        SOUND_FULL_SCALE);
  }

  /* The swing sound follows the notice across the screen */
  if (f_NoticeFrameCount >= NOTICE_PAN_START &&
      f_NoticeFrameCount < NOTICE_PAN_END) {
    pan = (f_NoticeSwing + NOTICE_COURSE_CENTER_X) * NOTICE_PAN_RANGE /
        NOTICE_COURSE_CENTER_X;
    if (pan < 0) {
      pan = 0;
    }

    if (pan > NOTICE_PAN_MAX) {
      pan = NOTICE_PAN_MAX;
    }

    SetSoundPan(NOTICE_SWING_SOUND_CHANNEL, pan);
  }

  lift = -(s32)(cosf(f_NoticeAngle) * NOTICE_TITLE_LIFT);
  lift -= (s32)NOTICE_TITLE_LIFT;
  if (f_NoticeFrameCount >= NOTICE_SHINE_START) {
    if (f_NoticeFrameCount < NOTICE_SHINE_END) {
      DrawTextureRect(&gfx, QuarterPixels(f_NoticeSwing + NOTICE_TITLE_LEFT),
          QuarterPixels(lift + f_NoticeUnk54 + NOTICE_TITLE_TOP),
          QuarterPixels(
              f_NoticeSwing + NOTICE_TITLE_LEFT + NOTICE_TITLE_STRIP_WIDTH),
          QuarterPixels(
              lift + f_NoticeUnk54 + NOTICE_TITLE_TOP + NOTICE_SHINE_SIZE),
          G_TX_RENDERTILE,
          TexelCoordinate(NOTICE_SHINE_SIZE -
              ((f_NoticeFrameCount - NOTICE_SHINE_START) * NOTICE_SHINE_STEP)),
          0, NOTICE_SHINE_DSDX, TEXEL_STEP_ONE);
    }
  }

  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", DrawCourseResultsNotice);
#endif

/* Changes the beaten character's face, then lifts the notice away */
void UpdateCharacterResultsNotice(void) {
  EncodedAsset asset;

  GetSaveData();
  f_NoticeTime++;
  if (f_IsNoticeHeld) {
    return;
  }

  if (f_NoticeTime > NOTICE_FACE_DELAY &&
      !(f_NoticeTime & NOTICE_FACE_INTERVAL_MASK) &&
      f_NoticeFaceIndex < NOTICE_LAST_FACE) {
    f_NoticeFaceIndex++;
    OpenEncodedAsset(g_CharacterImageAssets[(s8)g_Players[1].characterIndex] +
            f_NoticeFaceIndex,
        &asset);
    ReadEncodedAsset(f_PortraitImage, &asset);
  }

  if (f_NoticeTime > NOTICE_LIFT_DELAY) {
    if (f_NoticeLift > NOTICE_LIFT_START_LIMIT) {
      f_NoticeLift -= NOTICE_LIFT_START_SPEED;
    } else {
      f_NoticeLift += f_NoticeLift * NOTICE_LIFT_GROWTH_NUMERATOR /
          NOTICE_LIFT_GROWTH_DENOMINATOR;
    }

    if (f_NoticeLift < NOTICE_LIFT_EXIT) {
      StartResultsNotice(UNLOCK_NOTICE_KIND_NONE);
    }
  }
}

/* The character notice pops open past its full size, then settles at
 * sin(NOTICE_POP_SETTLE_ANGLE) */
#define NOTICE_POP_FRAMES 10
#define NOTICE_POP_STEP 0.15f
#define NOTICE_POP_SETTLE_ANGLE (3 * M_PI / 4)
#define NOTICE_CENTER_X 160
#define NOTICE_CENTER_Y 128
#define NOTICE_NAMEPLATE_SIZE 48
#define NOTICE_NAMEPLATE_POP_SIZE 36.0f
#define NOTICE_PORTRAIT_SIZE 32
#define NOTICE_PORTRAIT_POP_SIZE 24.0f
#define NOTICE_CHARACTER_TITLE_WIDTH 144
#define NOTICE_CHARACTER_TITLE_TOP 80
#define NOTICE_CHARACTER_TITLE_POP_WIDTH 108.0f
#define NOTICE_CHARACTER_TITLE_POP_HEIGHT 12.0f
#define NOTICE_CHARACTER_TITLE_STRIP_COUNT 7
#define NOTICE_CHARACTER_SHINE_LEFT 88
#define NOTICE_CHARACTER_SHINE_TOP 52
#define NOTICE_CHARACTER_SHINE_HEIGHT 56
#define NOTICE_CHARACTER_SHINE_DSDX ((NOTICE_SHINE_SIZE << 10) / 128)
#define NOTICE_CHARACTER_SHINE_DTDY \
  ((NOTICE_SHINE_SIZE << 10) / NOTICE_CHARACTER_SHINE_HEIGHT)
#define NOTICE_CHARACTER_VOICE_FRAME 14
#define NOTICE_CHARACTER_VOICE_SLOT 1

/*
 * The notice that a character has been beaten: their nameplate, face and title
 * pop open, then a shine sweeps the title
 *
 * Doesn't match: 58 words differ.
 */
#ifdef NON_MATCHING
void DrawCharacterResultsNotice(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  Radians angle;
  f32 pop;
  s32 size;
  s32 step;
  s32 width;
  s32 height;
  s32 top;
  s32 centerX;
  s32 centerY;
  s32 i;

  if (f_NoticeTime < NOTICE_POP_FRAMES) {
    angle = f_NoticeTime * NOTICE_POP_STEP * (M_PI / 2);
  } else {
    angle = NOTICE_POP_SETTLE_ANGLE;
  }

  pop = sinf(angle);
  gSPDisplayList(gfx++, g_DecalTextureSetup);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  size = pop * NOTICE_NAMEPLATE_POP_SIZE;
  if (size > 0) {
    LoadSmallPalette(&gfx, 0, ImageBody(f_NameplateImage));
    LoadNibbleTextureBlock(&gfx, ImageTexels(f_NameplateImage), G_IM_FMT_CI,
        NOTICE_NAMEPLATE_SIZE, NOTICE_NAMEPLATE_SIZE, 0, 0, 0, 0, 0, 0, 0);
    centerX = f_NoticeLift + NOTICE_CENTER_X;
    centerY = f_NoticeUnk6C + NOTICE_CENTER_Y;
    DrawTextureRect(&gfx, QuarterPixels(centerX - size),
        QuarterPixels(centerY - size), QuarterPixels(size + centerX),
        QuarterPixels(size + centerY), G_TX_RENDERTILE, 0, 0,
        TexelRunStep(NOTICE_NAMEPLATE_SIZE / 2, size),
        TexelRunStep(NOTICE_NAMEPLATE_SIZE / 2, size));
  }

  size = pop * NOTICE_PORTRAIT_POP_SIZE;
  if (size > 0) {
    LoadLargePalette(&gfx, f_PortraitPalette);
    LoadTextureBlock(&gfx, f_PortraitImage, G_IM_FMT_CI, G_IM_SIZ_8b,
        NOTICE_PORTRAIT_SIZE, NOTICE_PORTRAIT_SIZE, 0, 0, 0, 0, 0, 0, 0);
    /* Kept in a local, unlike the nameplate's, to match */
    step = TexelRunStep(NOTICE_PORTRAIT_SIZE / 2, size);
    IN_OWN_BLOCK(centerX = f_NoticeLift + NOTICE_CENTER_X);
    centerY = f_NoticeUnk6C + NOTICE_CENTER_Y;
    DrawTextureRect(&gfx, QuarterPixels(centerX - size),
        QuarterPixels(centerY - size), QuarterPixels(size + centerX),
        QuarterPixels(size + centerY), G_TX_RENDERTILE, 0, 0, step, step);
  }

  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  height = pop * NOTICE_CHARACTER_TITLE_POP_HEIGHT;
  width = pop * NOTICE_CHARACTER_TITLE_POP_WIDTH;
  if (width > 0 && height > 0) {
    LoadLargePalette(&gfx, ImageBody(f_NoticeTitleImage));
    for (i = 0; i != NOTICE_CHARACTER_TITLE_STRIP_COUNT; i++) {
      LoadTextureBlock(&gfx,
          LargeImageTexels(f_NoticeTitleImage) +
              (i * NOTICE_CHARACTER_TITLE_WIDTH * NOTICE_TITLE_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, NOTICE_CHARACTER_TITLE_WIDTH,
          NOTICE_TITLE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      top = NOTICE_CHARACTER_TITLE_TOP -
          (height * NOTICE_CHARACTER_TITLE_STRIP_COUNT / 2) + (i * height);
      /* This pass mirrors the slide, unlike the depth pass and the shine */
      DrawTextureRect(&gfx,
          QuarterPixels(NOTICE_CENTER_X - (f_NoticeLift + width)),
          QuarterPixels(top + f_NoticeUnk6C),
          QuarterPixels(width - (f_NoticeLift - NOTICE_CENTER_X)),
          QuarterPixels(top + height + f_NoticeUnk6C),

          G_TX_RENDERTILE, 0, 0,
          ((NOTICE_CHARACTER_TITLE_WIDTH / 2) << 10) / width,
          (NOTICE_TITLE_STRIP_HEIGHT << 10) / height);
    }

    /* The title again, into the depth buffer alone, so that the shine behind it
     * only lights the letters */
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPSetPrimDepth(gfx++, NOTICE_TITLE_DEPTH, 0);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 1, TEXEL0, 1, 0, 0, 0, TEXEL0,
        0, 1, TEXEL0, 1);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++,
        GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
        G_RM_AA_ZB_TEX_EDGE2);
    for (i = 0; i != NOTICE_CHARACTER_TITLE_STRIP_COUNT; i++) {
      LoadTextureBlock(&gfx,
          LargeImageTexels(f_NoticeTitleImage) +
              (i * NOTICE_CHARACTER_TITLE_WIDTH * NOTICE_TITLE_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, NOTICE_CHARACTER_TITLE_WIDTH,
          NOTICE_TITLE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      top = NOTICE_CHARACTER_TITLE_TOP -
          (height * NOTICE_CHARACTER_TITLE_STRIP_COUNT / 2) + (i * height);
      centerX = f_NoticeLift + NOTICE_CENTER_X;
      DrawTextureRect(&gfx, QuarterPixels(centerX - width),
          QuarterPixels(top + f_NoticeUnk6C), QuarterPixels(width + centerX),
          QuarterPixels(top + height + f_NoticeUnk6C), G_TX_RENDERTILE, 0, 0,
          ((NOTICE_CHARACTER_TITLE_WIDTH / 2) << 10) / width,
          (NOTICE_TITLE_STRIP_HEIGHT << 10) / height);
    }

    gDPSetPrimDepth(gfx++, NOTICE_SHINE_DEPTH, 0);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0,
        0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    LoadTextureBlock(&gfx, ImageBody(f_ResultsShineImage), G_IM_FMT_I,
        G_IM_SIZ_8b, NOTICE_SHINE_SIZE, NOTICE_SHINE_SIZE, 0, 0, 0, 0, 0, 0, 0);
    if (f_NoticeTime == NOTICE_CHARACTER_VOICE_FRAME) {
      PlayCharacterSound(f_PortraitCharacterIndex, NOTICE_CHARACTER_VOICE_SLOT,
          CHARACTER_SOUND_NOTICE, SOUND_FULL_SCALE, SOUND_FULL_SCALE);
    }

    if (f_NoticeTime == NOTICE_SHINE_START) {
      PlaySoundWithEffects(SOUND_SHRINKING_IMAGE, NOTICE_SHINE_SOUND_CHANNEL,
          NOTICE_SHINE_SOUND_VOLUME, 0, SOUND_FULL_SCALE);
    }

    if (f_NoticeTime == 2) {
      PlaySoundWithEffects(SOUND_UNLOCK_EVERYTHING, NOTICE_SWING_SOUND_CHANNEL,
          NOTICE_SWING_SOUND_VOLUME, 0, SOUND_FULL_SCALE);
    }

    if (f_NoticeTime >= NOTICE_SHINE_START) {
      if (f_NoticeTime < NOTICE_SHINE_END) {
        DrawTextureRect(&gfx,
            QuarterPixels(f_NoticeLift + NOTICE_CHARACTER_SHINE_LEFT),
            QuarterPixels(f_NoticeUnk6C + NOTICE_CHARACTER_SHINE_TOP),
            QuarterPixels(f_NoticeLift + NOTICE_CHARACTER_SHINE_LEFT +
                NOTICE_CHARACTER_TITLE_WIDTH),
            QuarterPixels(f_NoticeUnk6C + NOTICE_CHARACTER_SHINE_TOP +
                NOTICE_CHARACTER_SHINE_HEIGHT),
            G_TX_RENDERTILE,
            TexelCoordinate(NOTICE_SHINE_SIZE -
                ((f_NoticeTime - NOTICE_SHINE_START) * NOTICE_SHINE_STEP)),
            0, NOTICE_CHARACTER_SHINE_DSDX, NOTICE_CHARACTER_SHINE_DTDY);
      }
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", DrawCharacterResultsNotice);
#endif

/* Returns TRUE once the results have been saved and freed */
s32 UpdateRoundResults(ControllerState* controller) {
  Boolean isDone;

  switch (f_ResultsState) {
    case RESULTS_SLIDING_IN:
      if (f_ResultsSlide == 1) {
        f_ResultsState = RESULTS_SHOWN;
        f_ResultsSlide = 0;
        f_ResultsUnk78 = 0;
        f_ResultsUnk7C = 0;
        break;
      }

      f_ResultsSlide += -f_ResultsSlide / 2;
      break;
    case RESULTS_SHOWN:
      if (g_GameState.isClubTournament) {
        isDone = ShowScorePassword(controller);
      } else {
        switch (g_GameState.gameMode) {
          case GAME_MODE_STROKE:
          case GAME_MODE_UNK6:
          case GAME_MODE_SPEED_GOLF:
          case GAME_MODE_SKINS_MATCH:
          case GAME_MODE_CLUB_SLOTS:
            isDone = UpdateCoursePointsResults(controller);
            break;
          case GAME_MODE_MATCH_GAME:
          case GAME_MODE_GET_CHARACTER:
            isDone = UpdateMatchResults(controller);
            break;
          case GAME_MODE_TOURNAMENT:
            isDone = UpdateTournamentResults(controller);
            break;
          default:
            return FALSE;
        }
      }

      if (isDone) {
        f_ResultsState = RESULTS_CLOSING;
      }

      break;
    case RESULTS_CLOSING:
      WriteSaveData();
      OpenRoundResults(CLOSE_ROUND_RESULTS);
      return TRUE;
  }

  return FALSE;
}

s32 GetBannerState(void) {
  return f_BannerState;
}

void SetResultsBanner(s32 state, s32 kind) {
  f_BannerState = state;
  f_BannerKind = kind;
  if (state == BANNER_SLIDING_IN) {
    f_BannerX = SCREEN_WIDTH;
    f_BannerY = 0;
  }
}

/* The trophy is drawn in two parts, as each must fit in texture memory */
#define TROPHY_PART_COUNT 2

/* Off the screen's left */
#define BANNER_GONE_X (-SCREEN_WIDTH)
#define BANNER_SLIDE_OUT_SPEED 4

/* Plus the place */
#define MESSAGE_TROPHY_PRIZE 0x3C
#define MESSAGE_PLACE_PRIZE 0x40
/* Plus the place */
#define MESSAGE_TROPHY_PLACE 0x40
#define MESSAGE_UNK48 0x48
#define MESSAGE_PLACE 0x49
#define MESSAGE_POINTS_EARNED 0x1F3

#define BANNER_TEXT_SLOT 1
#define BANNER_TEXT_RED 32
#define BANNER_TEXT_GREEN 64
#define BANNER_TEXT_BLUE 255
#define BANNER_TEXT_ALPHA 255
#define PRIZE_TEXT_X 66
#define PRIZE_TEXT_Y 128
#define BANNER_TEXT_X 168
#define BANNER_TEXT_Y 140

/* The places that the ordinal suffixes differ for */
#define PLACE_TWENTY_FIRST 21
#define PLACE_TWENTY_SECOND 22
#define PLACE_TWENTY_THIRD 23

#define BANNER_CLOUD_ALPHA 240
#define BANNER_CLOUD_WIDTH 256
#define BANNER_CLOUD_HEIGHT 32
#define BANNER_CLOUD_STRIP_COUNT 2
#define BANNER_CLOUD_STRIP_SIZE (BANNER_CLOUD_WIDTH * BANNER_CLOUD_HEIGHT / 2)
#define BANNER_CLOUD_LEFT 24
#define BANNER_CLOUD_RIGHT 296
#define BANNER_CLOUD_TOP 112
/* The points banner is taller, its clouds stretched */
#define TALL_BANNER_CLOUD_TOP 104
#define TALL_BANNER_CLOUD_HEIGHT 40
#define BANNER_CLOUD_DSDX 0x3C3
#define TALL_BANNER_CLOUD_DTDY 0x333

/* The trophy turns, a frame each TROPHY_FRAME_TICKS, as the shine sweeps it */
#define TROPHY_SHINE_CYCLE 256
#define TROPHY_FRAME_TICKS 3
#define TROPHY_CENTER_X 48
#define TROPHY_BOTTOM 160
#define TROPHY_SHINE_TEXT_ALPHA 127
#define TROPHY_SHINE_START 16
#define TROPHY_SHINE_SOUND_FRAME 18
#define TROPHY_SHINE_SOUND_CHANNEL 15
#define TROPHY_SHINE_SOUND_VOLUME 40
#define TROPHY_SHINE_SOUND_REVERB 20
#define TROPHY_SHINE_SOUND_PAN 31
#define TROPHY_SHINE_SPEED 16
#define TROPHY_SHINE_DSDX 0x800

/*
 * Draws the banner of a round's result, and the trophy above a top three
 * tournament place
 *
 * Doesn't match: 71 words differ.
 */
#ifdef NON_MATCHING
void DrawResultsBanner(Gfx** displayListHead) {
  Gfx* gfx;
  s32 i;
  s32 rank;
  s32 place;
  s32 course;
  s32 top;

  gfx = *displayListHead;

  switch (f_BannerState) {
    case BANNER_SHOWN:
    case BANNER_HIDDEN:
      break;
    case BANNER_SLIDING_IN:
      if (f_BannerX == 1) {
        f_BannerState = BANNER_SHOWN;
      } else {
        f_BannerX += -f_BannerX / 2;
      }

      break;
    case BANNER_SLIDING_OUT:
      if (f_BannerX >= -1) {
        f_BannerX -= BANNER_SLIDE_OUT_SPEED;
      } else {
        f_BannerX += f_BannerX / 2;
      }

      if (f_BannerX < BANNER_GONE_X) {
        f_BannerState = BANNER_HIDDEN;
      }

      break;
  }

  switch (f_BannerKind) {
    case BANNER_KIND_UNK2:
    case BANNER_KIND_UNK4:
      break;
    case BANNER_KIND_POINTS:
      SetTextSlotValue(f_PointsShown, BANNER_TEXT_SLOT, 0);
      SetTextSlot(f_BannerX + BANNER_TEXT_X, f_BannerY + BANNER_TEXT_Y,
          MESSAGE_POINTS_EARNED, TEXT_SLOT_LARGE_FONT, BANNER_TEXT_SLOT,
          TEXT_FLAG_UNK20 | TEXT_FLAG_CENTERED, BANNER_TEXT_RED,
          BANNER_TEXT_GREEN, BANNER_TEXT_BLUE, BANNER_TEXT_ALPHA);
      break;
    case BANNER_KIND_TOURNAMENT_PLACE:
      rank = f_PlayerRanks[0];
      place = rank - 1;
      if (f_TournamentOpenCourseCount < COURSE_COUNT) {
        if (place < TROPHY_PLACE_COUNT) {
          SetTextSlotValue(f_PrizeShown, BANNER_TEXT_SLOT, 0);
          SetTextSlot(f_BannerX + PRIZE_TEXT_X, f_BannerY + PRIZE_TEXT_Y,
              rank + MESSAGE_TROPHY_PRIZE, TEXT_SLOT_LARGE_FONT,
              BANNER_TEXT_SLOT, TEXT_FLAG_UNK20, BANNER_TEXT_RED,
              BANNER_TEXT_GREEN, BANNER_TEXT_BLUE, BANNER_TEXT_ALPHA);
        } else {
          if (rank < PLACE_TWENTY_FIRST || rank > PLACE_TWENTY_THIRD) {
            sprintf(g_PrintBuffer, "%dth", rank);
          } else if (rank == PLACE_TWENTY_FIRST) {
            sprintf(g_PrintBuffer, "%dst", rank);
          } else if (rank == PLACE_TWENTY_SECOND) {
            sprintf(g_PrintBuffer, "%dnd", rank);
          } else if (rank == PLACE_TWENTY_THIRD) {
            sprintf(g_PrintBuffer, "%drd", rank);
          }

          SetTextName(g_PrintBuffer);
          SetTextSlotValue(f_PrizeShown, BANNER_TEXT_SLOT, 1);
          SetTextSlot(f_BannerX + PRIZE_TEXT_X, f_BannerY + PRIZE_TEXT_Y,
              MESSAGE_PLACE_PRIZE, TEXT_SLOT_LARGE_FONT, BANNER_TEXT_SLOT,
              TEXT_FLAG_UNK20, BANNER_TEXT_RED, BANNER_TEXT_GREEN,
              BANNER_TEXT_BLUE, BANNER_TEXT_ALPHA);
        }
      } else if (place < TROPHY_PLACE_COUNT) {
        SetTextSlot(f_BannerX + BANNER_TEXT_X, f_BannerY + BANNER_TEXT_Y,
            rank + MESSAGE_TROPHY_PLACE, TEXT_SLOT_LARGE_FONT, BANNER_TEXT_SLOT,
            TEXT_FLAG_UNK20 | TEXT_FLAG_CENTERED, BANNER_TEXT_RED,
            BANNER_TEXT_GREEN, BANNER_TEXT_BLUE, BANNER_TEXT_ALPHA);
      } else {
        SetTextSlotValue(rank, BANNER_TEXT_SLOT, 0);
        SetTextSlot(f_BannerX + BANNER_TEXT_X, f_BannerY + BANNER_TEXT_Y,
            MESSAGE_PLACE, TEXT_SLOT_LARGE_FONT, BANNER_TEXT_SLOT,
            TEXT_FLAG_UNK20 | TEXT_FLAG_CENTERED, BANNER_TEXT_RED,
            BANNER_TEXT_GREEN, BANNER_TEXT_BLUE, BANNER_TEXT_ALPHA);
      }

      break;
    case BANNER_KIND_UNK5:
      SetTextSlot(f_BannerX + BANNER_TEXT_X, f_BannerY + BANNER_TEXT_Y,
          MESSAGE_UNK48, TEXT_SLOT_LARGE_FONT, BANNER_TEXT_SLOT,
          TEXT_FLAG_UNK20 | TEXT_FLAG_CENTERED, BANNER_TEXT_RED,
          BANNER_TEXT_GREEN, BANNER_TEXT_BLUE, BANNER_TEXT_ALPHA);
      break;
  }

  if (f_BannerState == BANNER_HIDDEN) {
    return;
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_PATTERN);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
  gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
      PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, BANNER_CLOUD_ALPHA);
  for (i = 0; i != BANNER_CLOUD_STRIP_COUNT; i++) {
    LoadNibbleTextureBlock(&gfx,
        ImageBody(f_ResultsCloudImage) + (i * BANNER_CLOUD_STRIP_SIZE),
        G_IM_FMT_I, BANNER_CLOUD_WIDTH, BANNER_CLOUD_HEIGHT, 0, G_TX_CLAMP,
        G_TX_CLAMP, 0, 0, 0, 0);
    if (f_BannerKind == BANNER_KIND_POINTS) {
      DrawTextureRect(&gfx, QuarterPixels(f_BannerX + BANNER_CLOUD_LEFT),
          QuarterPixels(f_BannerY + TALL_BANNER_CLOUD_TOP +
              (i * TALL_BANNER_CLOUD_HEIGHT)),
          QuarterPixels(f_BannerX + BANNER_CLOUD_RIGHT),
          QuarterPixels(f_BannerY + TALL_BANNER_CLOUD_TOP +
              (i * TALL_BANNER_CLOUD_HEIGHT) + TALL_BANNER_CLOUD_HEIGHT),
          G_TX_RENDERTILE, 0, 0, BANNER_CLOUD_DSDX, TALL_BANNER_CLOUD_DTDY);
    } else {
      DrawTextureRect(&gfx, QuarterPixels(f_BannerX + BANNER_CLOUD_LEFT),
          QuarterPixels(
              f_BannerY + BANNER_CLOUD_TOP + (i * BANNER_CLOUD_HEIGHT)),
          QuarterPixels(f_BannerX + BANNER_CLOUD_RIGHT),
          QuarterPixels(f_BannerY + BANNER_CLOUD_TOP +
              (i * BANNER_CLOUD_HEIGHT) + BANNER_CLOUD_HEIGHT),
          G_TX_RENDERTILE, 0, 0, BANNER_CLOUD_DSDX, TEXEL_STEP_ONE);
    }
  }

  SetUpTextDrawing(&gfx);
  gDPSetPrimColor(gfx++, 0, 0, BANNER_TEXT_RED, BANNER_TEXT_GREEN,
      BANNER_TEXT_BLUE, BANNER_TEXT_ALPHA);
  DrawLargeTextSlot(&gfx, BANNER_TEXT_SLOT);
  if (f_BannerKind == BANNER_KIND_TOURNAMENT_PLACE) {
    course = g_GameState.courseIndex;
    if (f_PlayerRanks[0] <= TROPHY_PLACE_COUNT) {
      f_TrophyShineFrame = (f_TrophyShineFrame + 1) % TROPHY_SHINE_CYCLE;
      if (f_TallySpeed == FAST_TALLY_SPEED ||
          f_TrophyShineFrame % TROPHY_FRAME_TICKS == 0) {
        EncodedAsset asset;

        f_TrophyFrame++;
        if (f_TrophyFrame == f_TrophyAssets[course].frameCount) {
          f_TrophyFrame = 0;
        }

        OpenEncodedAsset(f_TrophyAssets[course].image + f_TrophyFrame, &asset);
        ReadEncodedAsset(f_TrophyImage, &asset);
      }

      gSPDisplayList(gfx++, g_DecalTextureSetup);
      gDPPipeSync(gfx++);
      gDPSetTextureLUT(gfx++, G_TT_RGBA16);
      gDPPipeSync(gfx++);
      gDPSetTextureFilter(gfx++, G_TF_POINT);
      LoadLargePalette(&gfx, f_TrophyPalette);
      top = -f_TrophySizes[course][TROPHY_PART_HEIGHTS] -
          f_TrophySizes[course][TROPHY_PART_HEIGHTS + 1];
      for (i = 0; i != TROPHY_PART_COUNT; i++) {
        gDPLoadTextureTile(gfx++,
            TextureAddress((u32)f_TrophyImage +
                (i * f_TrophySizes[course][TROPHY_WIDTH] *
                    f_TrophySizes[course][TROPHY_PART_HEIGHTS])),
            G_IM_FMT_CI, G_IM_SIZ_8b, f_TrophySizes[course][TROPHY_WIDTH],
            f_TrophySizes[course][TROPHY_PART_HEIGHTS + i], 0, 0,
            f_TrophySizes[course][TROPHY_WIDTH] - 1,
            f_TrophySizes[course][TROPHY_PART_HEIGHTS + i] - 1, 0, 0, 0, 0, 0,
            0, 0);
        DrawTextureRect(&gfx,
            QuarterPixels(TROPHY_CENTER_X + f_BannerX -
                (f_TrophySizes[course][TROPHY_WIDTH] / 2)),
            QuarterPixels(top + TROPHY_BOTTOM),
            QuarterPixels(TROPHY_CENTER_X + f_BannerX +
                (f_TrophySizes[course][TROPHY_WIDTH] / 2)),
            QuarterPixels(f_TrophySizes[course][TROPHY_PART_HEIGHTS + i] + top +
                TROPHY_BOTTOM),
            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        top += f_TrophySizes[course][TROPHY_PART_HEIGHTS + i];
      }

      /* The trophy again, into the depth buffer alone */
      gDPPipeSync(gfx++);
      gDPSetDepthSource(gfx++, G_ZS_PRIM);
      gDPSetPrimDepth(gfx++, NOTICE_TITLE_DEPTH, 0);
      gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 1, TEXEL0, 1, 0, 0, 0,
          TEXEL0, 0, 1, TEXEL0, 1);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++,
          GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
          G_RM_AA_ZB_TEX_EDGE2);
      top = -f_TrophySizes[course][TROPHY_PART_HEIGHTS] -
          f_TrophySizes[course][TROPHY_PART_HEIGHTS + 1];
      for (i = 0; i != TROPHY_PART_COUNT; i++) {
        gDPLoadTextureTile(gfx++,
            TextureAddress((u32)f_TrophyImage +
                (i * f_TrophySizes[course][TROPHY_WIDTH] *
                    f_TrophySizes[course][TROPHY_PART_HEIGHTS])),
            G_IM_FMT_CI, G_IM_SIZ_8b, f_TrophySizes[course][TROPHY_WIDTH],
            f_TrophySizes[course][TROPHY_PART_HEIGHTS + i], 0, 0,
            f_TrophySizes[course][TROPHY_WIDTH] - 1,
            f_TrophySizes[course][TROPHY_PART_HEIGHTS + i] - 1, 0, 0, 0, 0, 0,
            0, 0);
        DrawTextureRect(&gfx,
            QuarterPixels(TROPHY_CENTER_X + f_BannerX -
                (f_TrophySizes[course][TROPHY_WIDTH] / 2)),
            QuarterPixels(top + TROPHY_BOTTOM),
            QuarterPixels(TROPHY_CENTER_X + f_BannerX +
                (f_TrophySizes[course][TROPHY_WIDTH] / 2)),
            QuarterPixels(f_TrophySizes[course][TROPHY_PART_HEIGHTS + i] +
                TROPHY_BOTTOM + top),
            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        top += f_TrophySizes[course][TROPHY_PART_HEIGHTS + i];
      }

      gDPSetPrimDepth(gfx++, NOTICE_SHINE_DEPTH, 0);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
      gDPPipeSync(gfx++);
      gDPSetTextureLUT(gfx++, G_TT_NONE);
      gDPPipeSync(gfx++);
      gDPSetAlphaDither(gfx++, G_AD_PATTERN);
      gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0,
          0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
      gDPPipeSync(gfx++);
      gDPSetAlphaCompare(gfx++, G_AC_NONE);
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, TROPHY_SHINE_TEXT_ALPHA);
      LoadTextureBlock(&gfx, ImageBody(f_ResultsShineImage), G_IM_FMT_I,
          G_IM_SIZ_8b, NOTICE_SHINE_SIZE, NOTICE_SHINE_SIZE, 0, 0, 0, 0, 0, 0,
          0);
      if (f_TrophyShineFrame == TROPHY_SHINE_SOUND_FRAME) {
        PlaySoundWithEffects(SOUND_SHRINKING_IMAGE, TROPHY_SHINE_SOUND_CHANNEL,
            TROPHY_SHINE_SOUND_VOLUME, TROPHY_SHINE_SOUND_REVERB,
            TROPHY_SHINE_SOUND_PAN);
      }

      if (f_TrophyShineFrame >= TROPHY_SHINE_START) {
        if (f_TrophyShineFrame < TROPHY_SHINE_CYCLE) {
          top = f_TrophySizes[course][TROPHY_PART_HEIGHTS] +
              f_TrophySizes[course][TROPHY_PART_HEIGHTS + 1];
          DrawTextureRect(&gfx,
              QuarterPixels(TROPHY_CENTER_X + f_BannerX -
                  (f_TrophySizes[course][TROPHY_WIDTH] / 2)),
              QuarterPixels(TROPHY_BOTTOM - top),
              QuarterPixels(TROPHY_CENTER_X + f_BannerX +
                  (f_TrophySizes[course][TROPHY_WIDTH] / 2)),
              QuarterPixels(TROPHY_BOTTOM - top +
                  f_TrophySizes[course][TROPHY_SHINE_HEIGHT]),
              G_TX_RENDERTILE,
              TexelCoordinate(NOTICE_SHINE_SIZE -
                  ((f_TrophyShineFrame - TROPHY_SHINE_START) *
                      TROPHY_SHINE_SPEED)),
              0, TROPHY_SHINE_DSDX, TEXEL_STEP_ONE);
        }
      }

      gDPPipeSync(gfx++);
      gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    }
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", DrawResultsBanner);
#endif

/* f_ResultsUnk78 values while UpdateTournamentResults runs */
enum TournamentStep {
  TOURNAMENT_STEP_START,
  TOURNAMENT_STEP_SLIDING_IN_PRIZE,
  TOURNAMENT_STEP_TALLYING_PRIZE,
  TOURNAMENT_STEP_PRIZE_TALLIED,
  TOURNAMENT_STEP_SLIDE_OUT_PRIZE,
  TOURNAMENT_STEP_SLIDING_OUT_PRIZE,
  TOURNAMENT_STEP_SLIDING_IN_POINTS,
  TOURNAMENT_STEP_TALLYING_POINTS,
  TOURNAMENT_STEP_POINTS_TALLIED,
  TOURNAMENT_STEP_SLIDING_OUT_POINTS,
  TOURNAMENT_STEP_WAITING,
  TOURNAMENT_STEP_SHOWN
};

#define NEW_BEST_SCORE_SOUND_CHANNEL 3
#define WIN_SOUND_PAN 32

/*
 * Counts the tournament prize, then the earned course points, into the save,
 * showing a notice when they open a course, then returns TRUE once A is pressed
 */
s32 UpdateTournamentResults(ControllerState* controller) {
  EncodedAsset asset;
  CourseStats* record;
  s32 overshoot;

  GetSaveData();
  switch (f_UnlockNoticeKind) {
    case UNLOCK_NOTICE_KIND_COURSE:
      UpdateCourseResultsNotice();
      break;
    case UNLOCK_NOTICE_KIND_CHARACTER:
      break;
    case UNLOCK_NOTICE_KIND_NONE:
      if (f_OpenCourseCount < COURSE_COUNT &&
          f_PointsToNextCourse + f_PrizePoints + f_PointsEarned == 0) {
        StartPortraitAnimation(PORTRAIT_ANIMATION_UNK1, 0);
        StartResultsNotice(UNLOCK_NOTICE_KIND_COURSE);
        OpenEncodedAsset(g_TournamentIconAssets[f_OpenCourseCount], &asset);
        ReadEncodedAsset(f_ResultsCourseImage, &asset);
        OpenEncodedAsset(COURSE_NOTICE_TITLE_ASSET, &asset);
        ReadEncodedAsset(f_NoticeTitleImage, &asset);
        return FALSE;
      }

      switch (f_ResultsUnk78) {
        case TOURNAMENT_STEP_START:
          record = &GetSaveData()->courseStats[g_GameState.courseIndex];
          f_ResultsUnk78 = TOURNAMENT_STEP_SHOWN;
          if (record->roundCount <= 0 ||
              record->bestScore >= f_PlayerResults[0]) {
            PlaySound(SOUND_NEW_BEST_SCORE, NEW_BEST_SCORE_SOUND_CHANNEL,
                SOUND_FULL_SCALE);
          }

          f_IsAwaitingConfirm = TRUE;
          break;
        case TOURNAMENT_STEP_SHOWN:
          if (controller->buttons & A_BUTTON) {
            f_IsAwaitingConfirm = FALSE;
            SetResultsBanner(BANNER_SLIDING_IN, BANNER_KIND_TOURNAMENT_PLACE);
            PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL,
                CONFIRM_SOUND_VOLUME);
            f_ResultsUnk78 = TOURNAMENT_STEP_SLIDING_IN_PRIZE;
          }

          break;
        case TOURNAMENT_STEP_SLIDING_IN_PRIZE:
          if (GetBannerState() == BANNER_SHOWN) {
            f_ResultsUnk78 = TOURNAMENT_STEP_TALLYING_PRIZE;
            if (f_PlayerRanks[0] == 1) {
              StartPortraitAnimation(PORTRAIT_ANIMATION_UNK1, 0);
              PlayCharacterSound((s8)g_Players[0].characterIndex, 0,
                  CHARACTER_SOUND_WIN, SOUND_FULL_SCALE, WIN_SOUND_PAN);
            }
          }

          break;
        case TOURNAMENT_STEP_TALLYING_PRIZE:
          if (controller->buttons & A_BUTTON) {
            f_TallySpeed = FAST_TALLY_SPEED;
          }
          {
            s32* points = &f_PrizePoints;

            *points -= f_TallySpeed;
            if (*points > 0) {
              PlayTallySound();
            }

            if (f_OpenCourseCount == COURSE_COUNT) {
              *points = 0;
              f_ResultsUnk78 = TOURNAMENT_STEP_PRIZE_TALLIED;
              if (f_TournamentOpenCourseCount < COURSE_COUNT) {
                PlaySound(
                    SOUND_TALLY_DONE, g_NextSoundChannel, SOUND_FULL_SCALE);
              }

              break;
            }

            /* Stop on the points that open the next course, so that its notice
             * shows */
            overshoot = f_PointsToNextCourse + *points + f_PointsEarned;
            if (overshoot < 0) {
              *points -= overshoot;
            }

            if (*points < 0) {
              f_ResultsUnk78 = TOURNAMENT_STEP_PRIZE_TALLIED;
              *points = 0;
              f_IsAwaitingConfirm = TRUE;
              if (f_TournamentOpenCourseCount < COURSE_COUNT) {
                PlaySound(
                    SOUND_TALLY_DONE, g_NextSoundChannel, SOUND_FULL_SCALE);
              }
            }
          }

          break;
        case TOURNAMENT_STEP_PRIZE_TALLIED:
          if (controller->buttons & A_BUTTON) {
            PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL,
                CONFIRM_SOUND_VOLUME);
            f_TallySpeed = NORMAL_TALLY_SPEED;
            f_IsAwaitingConfirm = FALSE;
            f_ResultsUnk78 = TOURNAMENT_STEP_SLIDE_OUT_PRIZE;
          }

          /* A debugging shortcut to the course notice */
          if ((controller->pressedButtons & B_BUTTON) && g_IsDebugEnabled) {
            StartResultsNotice(UNLOCK_NOTICE_KIND_COURSE);
          }

          break;
        case TOURNAMENT_STEP_SLIDE_OUT_PRIZE:
          SetResultsBanner(BANNER_SLIDING_OUT, BANNER_KIND_TOURNAMENT_PLACE);
          f_ResultsUnk78 = TOURNAMENT_STEP_SLIDING_OUT_PRIZE;
          break;
        case TOURNAMENT_STEP_SLIDING_OUT_PRIZE:
          if (GetBannerState() == BANNER_HIDDEN) {
            if (f_PointsEarned > 0 && f_OpenCourseCount < COURSE_COUNT) {
              f_ResultsUnk78 = TOURNAMENT_STEP_SLIDING_IN_POINTS;
              SetResultsBanner(BANNER_SLIDING_IN, BANNER_KIND_POINTS);
            } else {
              f_ResultsUnk78 = TOURNAMENT_STEP_WAITING;
            }
          }

          break;
        case TOURNAMENT_STEP_SLIDING_IN_POINTS:
          if (GetBannerState() == BANNER_SHOWN) {
            f_ResultsUnk78 = TOURNAMENT_STEP_TALLYING_POINTS;
          }

          break;
        case TOURNAMENT_STEP_TALLYING_POINTS:
          if (controller->buttons & A_BUTTON) {
            f_TallySpeed = FAST_TALLY_SPEED;
          }
          {
            s32* points = &f_PointsEarned;

            if (*points > 0) {
              PlayTallySound();
            }

            *points -= f_TallySpeed;
            if (*points < 0) {
              *points = 0;
            }

            if (f_OpenCourseCount == COURSE_COUNT) {
              *points = 0;
            } else {
              overshoot = f_PointsToNextCourse + f_PrizePoints + *points;
              if (overshoot < 0) {
                *points -= overshoot;
              }
            }
          }

          if (f_PointsEarned == 0) {
            PlaySound(SOUND_TALLY_DONE, g_NextSoundChannel, SOUND_FULL_SCALE);
            f_ResultsUnk78 = TOURNAMENT_STEP_POINTS_TALLIED;
            f_IsAwaitingConfirm = TRUE;
          }

          break;
        case TOURNAMENT_STEP_POINTS_TALLIED:
          if (controller->buttons & A_BUTTON) {
            PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL,
                CONFIRM_SOUND_VOLUME);
            SetResultsBanner(BANNER_SLIDING_OUT, BANNER_KIND_POINTS);
            f_ResultsUnk78 = TOURNAMENT_STEP_SLIDING_OUT_POINTS;
          }

          break;
        case TOURNAMENT_STEP_SLIDING_OUT_POINTS:
          if (GetBannerState() == BANNER_HIDDEN) {
            f_IsAwaitingConfirm = TRUE;
            f_ResultsUnk78 = TOURNAMENT_STEP_WAITING;
          }

          break;
        case TOURNAMENT_STEP_WAITING:
          f_IsAwaitingConfirm = TRUE;
          if (controller->buttons & A_BUTTON) {
            PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL,
                CONFIRM_SOUND_VOLUME);
            return TRUE;
          }

          break;
      }

      break;
  }

  return FALSE;
}

/* The steps of ShowScorePassword, in f_ResultsUnk78 */
enum ScorePasswordStep {
  SCORE_PASSWORD_STEP_START,
  SCORE_PASSWORD_STEP_SHOWN,
  SCORE_PASSWORD_STEP_ASKING,
  SCORE_PASSWORD_STEP_DONE
};

/* Shows the round's score password until the player says they have it. Returns
 * TRUE then. */
s32 ShowScorePassword(ControllerState* controller) {
  ScoreReport report;
  s32 unused[2] UNUSED;

  GetSaveData();
  f_ResultsUnk7C++;
  switch (f_ResultsUnk78) {
    case SCORE_PASSWORD_STEP_START:
      f_ResultsUnk78 = SCORE_PASSWORD_STEP_SHOWN;
      StartPortraitAnimation(PORTRAIT_ANIMATION_UNK1, 0);
      CopyTournamentCode(report.tournamentCode);
      report.characterIndex = g_Players[0].characterIndex;
      report.score = f_PlayerResults[0];
      report.time = g_GameState.stopwatchFrameCount;
      report.gameBoyLevel =
          g_GameBoyCharacters[g_Players[0].gameBoyCharacterIndex].level;
      EncodeScorePassword(&report, f_ScorePassword);
      f_IsAwaitingConfirm = TRUE;
      break;
    case SCORE_PASSWORD_STEP_SHOWN:
      if (controller->buttons & A_BUTTON) {
        f_IsAwaitingConfirm = FALSE;
        f_ResultsUnk78 = SCORE_PASSWORD_STEP_ASKING;
        OpenPrompt(PROMPT_SCORE_PASSWORD);
        PlaySound(
            SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL, CONFIRM_SOUND_VOLUME);
      }

      break;
    case SCORE_PASSWORD_STEP_ASKING:
      switch (UpdatePrompt(controller)) {
        case PROMPT_RESULT_YES:
          f_ResultsUnk78 = SCORE_PASSWORD_STEP_DONE;
          break;
        case PROMPT_RESULT_NO:
          f_ResultsUnk78 = SCORE_PASSWORD_STEP_SHOWN;
          f_IsAwaitingConfirm = TRUE;
          break;
      }

      break;
    case SCORE_PASSWORD_STEP_DONE:
      f_IsAwaitingConfirm = FALSE;
      return TRUE;
  }

  return FALSE;
}

/* f_ResultsUnk78 values while UpdateCoursePointsResults runs */
enum PointsStep {
  POINTS_STEP_SHOWN,
  POINTS_STEP_SLIDING_IN_TALLY,
  POINTS_STEP_TALLIED,
  POINTS_STEP_SLIDING_OUT_TALLY,
  POINTS_STEP_WAITING,
  POINTS_STEP_TALLYING,
  POINTS_STEP_START_TALLY
};

/*
 * Counts the earned course points into the save, showing a notice when they
 * open a course, then returns TRUE once A is pressed
 *
 * Doesn't match: when starting the tally, the original reads the player
 * count through the address of g_GameState.gameMode, which moves the
 * branches after it by a word. 11 words differ.
 */
#ifdef NON_MATCHING
s32 UpdateCoursePointsResults(ControllerState* controller) {
  EncodedAsset asset;
  s32 i;
  s32 j;
  s32 overshoot;

  GetSaveData();
  f_ResultsUnk7C++;
  switch (f_UnlockNoticeKind) {
    case UNLOCK_NOTICE_KIND_COURSE:
      UpdateCourseResultsNotice();
      break;
    case UNLOCK_NOTICE_KIND_CHARACTER:
      break;

    case UNLOCK_NOTICE_KIND_NONE:
      if (f_OpenCourseCount < COURSE_COUNT &&
          f_PointsToNextCourse + f_PointsEarned == 0) {
        for (i = 0; i != g_GameState.playerCount; i++) {
          StartPortraitAnimation(PORTRAIT_ANIMATION_UNK1, i);
        }

        StartResultsNotice(UNLOCK_NOTICE_KIND_COURSE);
        OpenEncodedAsset(g_TournamentIconAssets[f_OpenCourseCount], &asset);
        ReadEncodedAsset(f_ResultsCourseImage, &asset);
        OpenEncodedAsset(COURSE_NOTICE_TITLE_ASSET, &asset);
        ReadEncodedAsset(f_NoticeTitleImage, &asset);
        return FALSE;
      }

      switch (f_ResultsUnk78) {
        case POINTS_STEP_SHOWN:
          f_IsAwaitingConfirm = TRUE;
          if (controller->buttons & A_BUTTON) {
            f_IsAwaitingConfirm = FALSE;
            f_ResultsUnk78 = POINTS_STEP_START_TALLY;
            PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL,
                CONFIRM_SOUND_VOLUME);
          }

          break;
        case POINTS_STEP_START_TALLY:
          if (f_PointsEarned > 0 && f_OpenCourseCount < COURSE_COUNT) {
            f_ResultsUnk78 = POINTS_STEP_SLIDING_IN_TALLY;
            SetResultsBanner(BANNER_SLIDING_IN, BANNER_KIND_POINTS);
          } else {
            f_ResultsUnk78 = POINTS_STEP_WAITING;
          }

          for (i = 0; i != g_GameState.playerCount; i++) {
            StartPortraitAnimation(PORTRAIT_ANIMATION_UNK1, i);
            for (j = 0; j != g_GameState.playerCount; j++) {
              if (i != j && f_PlayerRanks[i] > f_PlayerRanks[j]) {
                StartPortraitAnimation(PORTRAIT_ANIMATION_UNK4, i);
              }
            }
          }

          break;
        case POINTS_STEP_SLIDING_IN_TALLY:
          if (GetBannerState() == BANNER_SHOWN) {
            f_ResultsUnk78 = POINTS_STEP_TALLYING;
          }

          break;
        case POINTS_STEP_TALLYING:
          if (controller->buttons & A_BUTTON) {
            f_TallySpeed = FAST_TALLY_SPEED;
          }
          {
            s32* points = &f_PointsEarned;

            if (*points > 0) {
              PlayTallySound();
            }

            *points -= f_TallySpeed;
            if (*points < 0) {
              *points = 0;
            }

            if (f_OpenCourseCount == COURSE_COUNT) {
              *points = 0;
            } else {
              /* Stop on the points that open the next course, so that its
               * notice shows */
              overshoot = f_PointsToNextCourse + *points;
              if (overshoot < 0) {
                *points -= overshoot;
              }
            }
          }

          if (f_PointsEarned == 0) {
            f_ResultsUnk7C = 0;
            PlaySound(SOUND_TALLY_DONE, g_NextSoundChannel, SOUND_FULL_SCALE);
            f_ResultsUnk78 = POINTS_STEP_TALLIED;
            f_IsAwaitingConfirm = TRUE;
          }

          break;
        case POINTS_STEP_TALLIED:
          if (controller->buttons & A_BUTTON) {
            SetResultsBanner(BANNER_SLIDING_OUT, BANNER_KIND_POINTS);
            f_ResultsUnk78 = POINTS_STEP_SLIDING_OUT_TALLY;
            f_IsAwaitingConfirm = FALSE;
          }

          break;
        case POINTS_STEP_SLIDING_OUT_TALLY:
          if (GetBannerState() == BANNER_HIDDEN) {
            f_ResultsUnk78 = POINTS_STEP_WAITING;
          }

          break;
        case POINTS_STEP_WAITING:
          f_IsAwaitingConfirm = TRUE;
          if (controller->pressedButtons & A_BUTTON) {
            PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL,
                CONFIRM_SOUND_VOLUME);
            f_IsAwaitingConfirm = FALSE;
            return TRUE;
          }

          break;
      }

      break;
  }

  return FALSE;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", UpdateCoursePointsResults);
#endif

/* f_ResultsUnk78 values while UpdateMatchResults runs */
enum MatchStep {
  MATCH_STEP_SHOWN,
  MATCH_STEP_SLIDING_IN_TALLY,
  MATCH_STEP_TALLYING,
  MATCH_STEP_TALLIED,
  MATCH_STEP_SLIDING_OUT_TALLY,
  MATCH_STEP_RECORD_RESULT,
  MATCH_STEP_WAITING,
  MATCH_STEP_SHOW_CHARACTER_NOTICE,
  MATCH_STEP_START_NOTICE_WAIT,
  MATCH_STEP_NOTICE_WAIT,
  MATCH_STEP_START_TALLY
};

#define CHARACTER_NOTICE_WAIT_FRAMES 8
/*
 * Whether beating a character in Get Character unlocks them: other feats
 * unlock the rest
 */
#define IsUnlockedByBeating(character) \
  ((character) != CHARACTER_PLUM && (character) != CHARACTER_CHARLIE && \
      (character) != CHARACTER_MAPLE && (character) != CHARACTER_BABY_MARIO && \
      (character) != CHARACTER_PEACH && \
      (character) != CHARACTER_DONKEY_KONG && \
      (character) < CHARACTER_METAL_MARIO)

/*
 * Counts the earned course points into the save, then in Get Character
 * records the match and unlocks a beaten opponent, returning TRUE once
 * A is pressed
 */
s32 UpdateMatchResults(ControllerState* controller) {
  EncodedAsset asset;
  s32 i;
  s32 overshoot;

  GetSaveData();
  f_ResultsUnk7C++;
  switch (f_UnlockNoticeKind) {
    case UNLOCK_NOTICE_KIND_COURSE:
      UpdateCourseResultsNotice();
      break;
    case UNLOCK_NOTICE_KIND_CHARACTER:
      UpdateCharacterResultsNotice();
      break;
    case UNLOCK_NOTICE_KIND_NONE:
      if (f_OpenCourseCount < COURSE_COUNT &&
          f_PointsToNextCourse + f_PointsEarned == 0) {
        for (i = 0; i != g_GameState.playerCount; i++) {
          StartPortraitAnimation(PORTRAIT_ANIMATION_UNK1, i);
        }

        StartResultsNotice(UNLOCK_NOTICE_KIND_COURSE);
        OpenEncodedAsset(g_TournamentIconAssets[f_OpenCourseCount], &asset);
        ReadEncodedAsset(f_ResultsCourseImage, &asset);
        OpenEncodedAsset(COURSE_NOTICE_TITLE_ASSET, &asset);
        ReadEncodedAsset(f_NoticeTitleImage, &asset);
        return FALSE;
      }

      switch (f_ResultsUnk78) {
        case MATCH_STEP_SHOWN:
          f_IsAwaitingConfirm = TRUE;
          if (controller->buttons & A_BUTTON) {
            f_IsAwaitingConfirm = FALSE;
            f_ResultsUnk78 = MATCH_STEP_START_TALLY;
            PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL,
                CONFIRM_SOUND_VOLUME);
          }

          break;
        case MATCH_STEP_START_TALLY:
          if (f_PointsEarned > 0 && f_OpenCourseCount < COURSE_COUNT) {
            f_ResultsUnk78 = MATCH_STEP_SLIDING_IN_TALLY;
            SetResultsBanner(BANNER_SLIDING_IN, BANNER_KIND_POINTS);
          } else {
            f_ResultsUnk78 = MATCH_STEP_RECORD_RESULT;
          }

          if (f_PlayerRanks[0] == MATCH_RANK_WON) {
            StartPortraitAnimation(PORTRAIT_ANIMATION_UNK1, 0);
            StartPortraitAnimation(PORTRAIT_ANIMATION_UNK4, 1);
          } else {
            StartPortraitAnimation(PORTRAIT_ANIMATION_UNK1, 1);
            StartPortraitAnimation(PORTRAIT_ANIMATION_UNK4, 0);
          }

          break;
        case MATCH_STEP_SLIDING_IN_TALLY:
          if (GetBannerState() == BANNER_SHOWN) {
            f_ResultsUnk78 = MATCH_STEP_TALLYING;
          }

          break;
        case MATCH_STEP_TALLYING:
          if (controller->buttons & A_BUTTON) {
            f_TallySpeed = FAST_TALLY_SPEED;
          }
          {
            s32* points = &f_PointsEarned;

            if (*points > 0) {
              PlayTallySound();
            }

            *points -= f_TallySpeed;
            if (*points < 0) {
              *points = 0;
            }

            if (f_OpenCourseCount == COURSE_COUNT) {
              *points = 0;
            } else {
              /* Stop on the points that open the next course, so that its
               * notice shows */
              overshoot = f_PointsToNextCourse + *points;
              if (overshoot < 0) {
                *points -= overshoot;
              }
            }
          }

          if (f_PointsEarned == 0) {
            f_ResultsUnk78 = MATCH_STEP_TALLIED;
            PlaySound(SOUND_TALLY_DONE, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
        case MATCH_STEP_TALLIED:
          f_IsAwaitingConfirm = TRUE;
          if (controller->buttons & A_BUTTON) {
            SetResultsBanner(BANNER_SLIDING_OUT, BANNER_KIND_POINTS);
            f_ResultsUnk78 = MATCH_STEP_SLIDING_OUT_TALLY;
            f_IsAwaitingConfirm = FALSE;
          }

          break;
        case MATCH_STEP_SLIDING_OUT_TALLY:
          if (GetBannerState() == BANNER_HIDDEN) {
            f_ResultsUnk78 = MATCH_STEP_RECORD_RESULT;
          }

          break;
        case MATCH_STEP_RECORD_RESULT:
          f_ResultsUnk78 = MATCH_STEP_WAITING;
          f_ResultsUnk7C = 0;
          if (g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
            /* Beating a still locked opponent unlocks them */
            if (f_PlayerRanks[0] == MATCH_RANK_WON &&
                FindCharacterOnGrid((s8)g_Players[1].characterIndex) ==
                    CHARACTER_OFF_GRID &&
                IsUnlockedByBeating((s8)g_Players[1].characterIndex)) {
              f_ResultsUnk78 = MATCH_STEP_SHOW_CHARACTER_NOTICE;
            }

            RecordComCharacterResult((s8)g_Players[0].characterIndex,
                (s8)g_Players[1].characterIndex,
                GetMatchWinner(0, 1) == MATCH_SIDE_PLAYER);
          }

          break;
        case MATCH_STEP_SHOW_CHARACTER_NOTICE: {
          EncodedAsset titleAsset;

          StartResultsNotice(UNLOCK_NOTICE_KIND_CHARACTER);
          OpenEncodedAsset(CHARACTER_NOTICE_TITLE_ASSET, &titleAsset);
          ReadEncodedAsset(f_NoticeTitleImage, &titleAsset);
          f_ResultsUnk78 = MATCH_STEP_START_NOTICE_WAIT;
          break;
        }
        case MATCH_STEP_START_NOTICE_WAIT:
          f_ResultsUnk78 = MATCH_STEP_NOTICE_WAIT;
          f_ResultsUnk7C = 0;
          break;
        case MATCH_STEP_NOTICE_WAIT:
          if (f_ResultsUnk7C == CHARACTER_NOTICE_WAIT_FRAMES) {
            f_ResultsUnk78 = MATCH_STEP_WAITING;
          }

          break;
        case MATCH_STEP_WAITING:
          f_IsAwaitingConfirm = TRUE;

          /* A debugging aid: up and down choose a character, and B shows its
           * notice */
          if (IsGameFlagSet(GAME_FLAG_CHOOSE_UNLOCK_NOTICE)) {
            if ((g_ControllerStates[0].repeatedButtons & U_JPAD) &&
                f_DebugUnlockCharacter > 0) {
              f_DebugUnlockCharacter--;
            }

            if ((g_ControllerStates[0].repeatedButtons & D_JPAD) &&
                f_DebugUnlockCharacter < CHARACTER_COUNT - 1) {
              f_DebugUnlockCharacter++;
            }

            sprintf(g_PrintBuffer, "CHARA:%2d", (int)f_DebugUnlockCharacter);
            PrintDebugText(g_PrintBuffer, DEBUG_UNLOCK_CHOICE_COLUMN,
                DEBUG_UNLOCK_CHOICE_ROW);
            if (controller->pressedButtons & B_BUTTON) {
              EncodedAsset portraitAsset;
              AssetFile file;

              StartResultsNotice(UNLOCK_NOTICE_KIND_CHARACTER);
              g_Players[1].characterIndex = f_DebugUnlockCharacter;
              f_PortraitCharacterIndex = (s8)f_DebugUnlockCharacter;
              OpenEncodedAsset(
                  g_CharacterImageAssets[(s8)f_DebugUnlockCharacter],
                  &portraitAsset);
              ReadEncodedAsset(f_PortraitImage, &portraitAsset);
              OpenAssetFile(
                  g_CharacterPaletteAssets[(s8)g_Players[1].characterIndex] +
                      (s8)g_Players[1].paletteIndex,
                  &file);
              ReadAssetFile(f_PortraitPalette, GetAssetFileSize(&file), &file);
              OpenEncodedAsset(CHARACTER_NOTICE_TITLE_ASSET, &portraitAsset);
              ReadEncodedAsset(f_NoticeTitleImage, &portraitAsset);
              f_ResultsUnk78 = MATCH_STEP_SLIDING_OUT_TALLY;
            }
          }

          if (controller->buttons & A_BUTTON) {
            PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL,
                CONFIRM_SOUND_VOLUME);
            f_IsAwaitingConfirm = FALSE;
            return TRUE;
          }

          break;
      }

      break;
  }

  return FALSE;
}

/* Where the tournament results draw their parts */
#define TOURNAMENT_PLACE_X 40
#define TOURNAMENT_PORTRAIT_X 100
#define TOURNAMENT_ICON_Y 80
#define TOURNAMENT_SCORE_LABEL_X 144
#define TOURNAMENT_POINTS_LABEL_X 232
#define TOURNAMENT_LABEL_Y 60
#define TOURNAMENT_SCORE_X 152
#define TOURNAMENT_POINTS_X 232
#define TOURNAMENT_NUMBER_Y 88

/* The tournament results: place, portrait, score, Course Points and the points
 * to the next course */
void DrawTournamentResults(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  DrawRank(&gfx, f_ResultsSlide + TOURNAMENT_PLACE_X,
      f_ResultsTop + TOURNAMENT_ICON_Y, (f_PlayerRanks[0] - 1) | RANK_AS_PLACE);
  DrawPlayerPortrait(&gfx, f_ResultsSlide + TOURNAMENT_PORTRAIT_X,
      f_ResultsTop + TOURNAMENT_ICON_Y, 0, FALSE);
  DrawResultsLabel(&gfx, f_ResultsSlide + TOURNAMENT_SCORE_LABEL_X,
      f_ResultsTop + TOURNAMENT_LABEL_Y, RESULTS_LABEL_SCORE);
  DrawResultsLabel(&gfx, f_ResultsSlide + TOURNAMENT_POINTS_LABEL_X,
      f_ResultsTop + TOURNAMENT_LABEL_Y, RESULTS_LABEL_POINTS);
  if (f_OpenCourseCount == COURSE_COUNT) {
    SetTextSlot(f_ResultsSlide + ALL_COURSES_OPEN_X,
        f_ResultsTop + SOLO_ALL_COURSES_OPEN_Y, MESSAGE_ALL_COURSES_OPEN,
        TEXT_SLOT_LARGE_FONT, RESULTS_TEXT_SLOT, 0, RESULTS_TEXT_RED,
        RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE, RESULTS_TEXT_ALPHA);
  } else {
    DrawResultsNumber(&gfx, f_ResultsSlide + POINTS_TOTAL_X, POINTS_TOTAL_Y,
        f_PointsToNextCourse + f_PrizePoints + f_PointsEarned);
    SetTextSlot(f_ResultsSlide + POINTS_TO_NEXT_COURSE_X,
        f_ResultsTop + POINTS_TO_NEXT_COURSE_Y, MESSAGE_POINTS_TO_NEXT_COURSE,
        TEXT_SLOT_LARGE_FONT, RESULTS_TEXT_SLOT, 0, RESULTS_TEXT_RED,
        RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE, RESULTS_TEXT_ALPHA);
  }

  LoadSmallPalette(&gfx, 0, ImageBody(f_ResultsLargeDigitsImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(f_ResultsLargeDigitsImage),
      G_IM_FMT_CI, LARGE_DIGIT_WIDTH, LARGE_DIGITS_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  if (f_PlayerResults[0] == 0) {
    sprintf(g_PrintBuffer, f_EvenScoreFormat, 0);
  } else {
    sprintf(g_PrintBuffer, f_ScoreFormat, f_PlayerResults[0]);
  }

  DrawDigitText(&gfx, f_ResultsSlide + TOURNAMENT_SCORE_X,
      f_ResultsTop + TOURNAMENT_NUMBER_Y, g_PrintBuffer);
  sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerPoints[0]);
  DrawDigitText(&gfx, f_ResultsSlide + TOURNAMENT_POINTS_X,
      f_ResultsTop + TOURNAMENT_NUMBER_Y, g_PrintBuffer);
  *displayListHead = gfx;
}

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_EvenScoreFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_ScoreFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_PointsFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_TimeFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_TimeLabel);

#define RESULTS_HEADING_ENV_COLOR 0xFFFF0000
#define RESULTS_HEADING_PRIM_COLOR 0x30FF0000

/*
 * Where the Speed Golf results draw their parts: the time and the strokes
 * each sit under a heading
 */
#define SPEED_GOLF_PORTRAIT_X 40
#define SPEED_GOLF_PORTRAIT_Y 88
#define SPEED_GOLF_SCORE_LABEL_X 82
#define SPEED_GOLF_POINTS_LABEL_X 232
#define SPEED_GOLF_LABEL_Y 56
#define SPEED_GOLF_HEADING_X 86
#define SPEED_GOLF_TIME_HEADING_Y 76
#define SPEED_GOLF_STROKE_HEADING_Y 114
#define SPEED_GOLF_VALUE_X 90
#define SPEED_GOLF_TIME_Y 94
#define SPEED_GOLF_SCORE_Y 132
#define SPEED_GOLF_POINTS_X 228
#define SPEED_GOLF_POINTS_Y 88

/* The Speed Golf results: portrait, time, strokes, Course Points and the points
 * to the next course */
void DrawSpeedGolfResults(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 seconds;
  s32 hundredths;

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  DrawPlayerPortrait(&gfx, f_ResultsSlide + SPEED_GOLF_PORTRAIT_X,
      f_ResultsTop + SPEED_GOLF_PORTRAIT_Y, 0, FALSE);
  DrawResultsLabel(&gfx, f_ResultsSlide + SPEED_GOLF_SCORE_LABEL_X,
      f_ResultsTop + SPEED_GOLF_LABEL_Y, RESULTS_LABEL_SCORE);
  DrawResultsLabel(&gfx, f_ResultsSlide + SPEED_GOLF_POINTS_LABEL_X,
      f_ResultsTop + SPEED_GOLF_LABEL_Y, RESULTS_LABEL_POINTS);
  if (f_OpenCourseCount == COURSE_COUNT) {
    SetTextSlot(f_ResultsSlide + ALL_COURSES_OPEN_X,
        f_ResultsTop + SOLO_ALL_COURSES_OPEN_Y, MESSAGE_ALL_COURSES_OPEN,
        TEXT_SLOT_LARGE_FONT, RESULTS_TEXT_SLOT, 0, RESULTS_TEXT_RED,
        RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE, RESULTS_TEXT_ALPHA);
  } else {
    DrawResultsNumber(&gfx, f_ResultsSlide + POINTS_TOTAL_X, POINTS_TOTAL_Y,
        f_PointsToNextCourse + f_PrizePoints + f_PointsEarned);
    SetTextSlot(f_ResultsSlide + POINTS_TO_NEXT_COURSE_X,
        f_ResultsTop + POINTS_TO_NEXT_COURSE_Y, MESSAGE_POINTS_TO_NEXT_COURSE,
        TEXT_SLOT_LARGE_FONT, RESULTS_TEXT_SLOT, 0, RESULTS_TEXT_RED,
        RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE, RESULTS_TEXT_ALPHA);
  }

  LoadSmallPalette(&gfx, 0, ImageBody(f_ResultsLargeDigitsImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(f_ResultsLargeDigitsImage),
      G_IM_FMT_CI, LARGE_DIGIT_WIDTH, LARGE_DIGITS_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  if (f_PlayerResults[0] == 0) {
    sprintf(g_PrintBuffer, f_EvenScoreFormat, 0);
  } else {
    sprintf(g_PrintBuffer, f_ScoreFormat, f_PlayerResults[0]);
  }

  DrawDigitText(&gfx, f_ResultsSlide + SPEED_GOLF_VALUE_X,
      f_ResultsTop + SPEED_GOLF_SCORE_Y, g_PrintBuffer);
  seconds = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND;
  hundredths = (s32)(g_GameState.stopwatchFrameCount *
                   ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
      HUNDREDTHS_PER_SECOND;
  sprintf(g_PrintBuffer, f_TimeFormat,
      seconds / SECONDS_PER_MINUTE % SHOWN_MINUTES_LIMIT,
      seconds % SECONDS_PER_MINUTE, hundredths);
  DrawDigitText(&gfx, f_ResultsSlide + SPEED_GOLF_VALUE_X,
      f_ResultsTop + SPEED_GOLF_TIME_Y, g_PrintBuffer);
  sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerPoints[0]);
  DrawDigitText(&gfx, f_ResultsSlide + SPEED_GOLF_POINTS_X,
      f_ResultsTop + SPEED_GOLF_POINTS_Y, g_PrintBuffer);
  SetUpGradientTextDrawing(&gfx, 0);
  sprintf(g_PrintBuffer, f_TimeLabel);
  DrawLargeTextWithShadow(&gfx, f_ResultsSlide + SPEED_GOLF_HEADING_X,
      f_ResultsTop + SPEED_GOLF_TIME_HEADING_Y, g_PrintBuffer,
      RESULTS_HEADING_ENV_COLOR, RESULTS_HEADING_PRIM_COLOR, 0);
  sprintf(g_PrintBuffer, "Stroke");
  DrawLargeTextWithShadow(&gfx, f_ResultsSlide + SPEED_GOLF_HEADING_X,
      f_ResultsTop + SPEED_GOLF_STROKE_HEADING_Y, g_PrintBuffer,
      RESULTS_HEADING_ENV_COLOR, RESULTS_HEADING_PRIM_COLOR, 0);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

#define RESULTS_ROW_HEIGHT 32
#define RESULTS_ICON_ROW_Y 68
#define RESULTS_TEXT_ROW_Y 80
/*
 * The players' results columns. With bets, places and a bet totals column
 * squeeze the others left; without them, ranks lead the rows.
 */
#define BETTING_RESULTS_SCORE_LABEL_X 88
#define BETTING_RESULTS_BETS_LABEL_X 192
#define BETTING_RESULTS_POINTS_LABEL_X 248
#define BETTING_RESULTS_PLACE_X 18
#define BETTING_RESULTS_PORTRAIT_X 50
#define BETTING_RESULTS_SCORE_X 96
#define BETTING_RESULTS_BET_TOTAL_X 156
#define BETTING_RESULTS_POINTS_X 242
#define PLAIN_RESULTS_SCORE_LABEL_X 144
#define PLAIN_RESULTS_POINTS_LABEL_X 234
#define PLAIN_RESULTS_RANK_X 32
#define PLAIN_RESULTS_PORTRAIT_X 96
#define PLAIN_RESULTS_SCORE_X 152
#define PLAIN_RESULTS_POINTS_X 232

/* The results of several players: place, portrait, score, any bet totals and
 * Course Points */
void DrawPlayersResults(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 betShift = FOUR_DIGIT_BET_SHIFT;
  s32 i;

  for (i = 0; i != g_GameState.playerCount; i++) {
    if (f_PlayerBetTotals[i] > FOUR_DIGIT_BET_MAX) {
      betShift = 0;
    }

    if (f_PlayerBetTotals[i] < FOUR_DIGIT_BET_MIN) {
      betShift = 0;
    }
  }

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  if (f_HasBets) {
    DrawResultsLabel(&gfx, f_ResultsSlide + BETTING_RESULTS_SCORE_LABEL_X,
        f_ResultsTop + RESULTS_HEADING_Y, RESULTS_LABEL_SCORE);
    DrawResultsLabel(&gfx, f_ResultsSlide + BETTING_RESULTS_BETS_LABEL_X,
        f_ResultsTop + RESULTS_HEADING_Y, RESULTS_LABEL_BETS);
    DrawResultsLabel(&gfx, f_ResultsSlide + BETTING_RESULTS_POINTS_LABEL_X,
        f_ResultsTop + RESULTS_HEADING_Y, RESULTS_LABEL_POINTS);
    for (i = 0; i != g_GameState.playerCount; i++) {
      if (g_GameState.playerCount >= 2) {
        DrawPlaceIcon(&gfx, f_ResultsSlide + BETTING_RESULTS_PLACE_X,
            f_ResultsTop + RESULTS_ICON_ROW_Y + (i * RESULTS_ROW_HEIGHT),
            f_PlayerRanks[i]);
      }

      DrawPlayerPortrait(&gfx, f_ResultsSlide + BETTING_RESULTS_PORTRAIT_X,
          f_ResultsTop + RESULTS_ICON_ROW_Y + (i * RESULTS_ROW_HEIGHT), i,
          FALSE);
    }
  } else {
    DrawResultsLabel(&gfx, f_ResultsSlide + PLAIN_RESULTS_SCORE_LABEL_X,
        f_ResultsTop + RESULTS_HEADING_Y, RESULTS_LABEL_SCORE);
    DrawResultsLabel(&gfx, f_ResultsSlide + PLAIN_RESULTS_POINTS_LABEL_X,
        f_ResultsTop + RESULTS_HEADING_Y, RESULTS_LABEL_POINTS);
    for (i = 0; i != g_GameState.playerCount; i++) {
      if (g_GameState.playerCount >= 2) {
        DrawRank(&gfx, f_ResultsSlide + PLAIN_RESULTS_RANK_X,
            f_ResultsTop + RESULTS_ICON_ROW_Y + (i * RESULTS_ROW_HEIGHT),
            f_PlayerRanks[i]);
      }

      DrawPlayerPortrait(&gfx, f_ResultsSlide + PLAIN_RESULTS_PORTRAIT_X,
          f_ResultsTop + RESULTS_ICON_ROW_Y + (i * RESULTS_ROW_HEIGHT), i,
          FALSE);
    }
  }

  LoadSmallPalette(&gfx, 0, ImageBody(f_ResultsLargeDigitsImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(f_ResultsLargeDigitsImage),
      G_IM_FMT_CI, LARGE_DIGIT_WIDTH, LARGE_DIGITS_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  if (f_HasBets) {
    for (i = 0; i != g_GameState.playerCount; i++) {
      if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
          g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
        sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerResults[i]);
      } else if (f_PlayerResults[i] == 0) {
        sprintf(g_PrintBuffer, f_EvenScoreFormat, 0);
      } else {
        sprintf(g_PrintBuffer, f_ScoreFormat, f_PlayerResults[i]);
      }

      DrawDigitText(&gfx, f_ResultsSlide + BETTING_RESULTS_SCORE_X,
          f_ResultsTop + RESULTS_TEXT_ROW_Y + (i * RESULTS_ROW_HEIGHT),
          g_PrintBuffer);
      sprintf(g_PrintBuffer, "%5d", (int)f_PlayerBetTotals[i]);
      DrawDigitText(&gfx,
          f_ResultsSlide + BETTING_RESULTS_BET_TOTAL_X + betShift,
          f_ResultsTop + RESULTS_TEXT_ROW_Y + (i * RESULTS_ROW_HEIGHT),
          g_PrintBuffer);
      if (f_PlayerPoints[i] < 0) {
        sprintf(g_PrintBuffer, f_NoPointsText, f_PlayerPoints[i]);
      } else {
        sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerPoints[i]);
      }

      DrawDigitText(&gfx, f_ResultsSlide + BETTING_RESULTS_POINTS_X,
          f_ResultsTop + RESULTS_TEXT_ROW_Y + (i * RESULTS_ROW_HEIGHT),
          g_PrintBuffer);
    }
  } else {
    for (i = 0; i != g_GameState.playerCount; i++) {
      if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
          g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
        sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerResults[i]);
      } else if (f_PlayerResults[i] == 0) {
        sprintf(g_PrintBuffer, f_EvenScoreFormat, 0);
      } else {
        sprintf(g_PrintBuffer, f_ScoreFormat, f_PlayerResults[i]);
      }

      DrawDigitText(&gfx, f_ResultsSlide + PLAIN_RESULTS_SCORE_X,
          f_ResultsTop + RESULTS_TEXT_ROW_Y + (i * RESULTS_ROW_HEIGHT),
          g_PrintBuffer);
      if (f_PlayerPoints[i] < 0) {
        sprintf(g_PrintBuffer, f_NoPointsText, f_PlayerPoints[i]);
      } else {
        sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerPoints[i]);
      }

      DrawDigitText(&gfx, f_ResultsSlide + PLAIN_RESULTS_POINTS_X,
          f_ResultsTop + RESULTS_TEXT_ROW_Y + (i * RESULTS_ROW_HEIGHT),
          g_PrintBuffer);
    }
  }

  if (f_OpenCourseCount == COURSE_COUNT) {
    SetTextSlot(f_ResultsSlide + ALL_COURSES_OPEN_X,
        f_ResultsTop + PLAYERS_ALL_COURSES_OPEN_Y, MESSAGE_ALL_COURSES_OPEN,
        TEXT_SLOT_LARGE_FONT, RESULTS_TEXT_SLOT, 0, RESULTS_TEXT_RED,
        RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE, RESULTS_TEXT_ALPHA);
  } else {
    DrawResultsNumber(&gfx, f_ResultsSlide + POINTS_TOTAL_X, POINTS_TOTAL_Y,
        f_PointsToNextCourse + f_PointsEarned);
    SetTextSlot(f_ResultsSlide + POINTS_TO_NEXT_COURSE_X,
        f_ResultsTop + POINTS_TO_NEXT_COURSE_Y, MESSAGE_POINTS_TO_NEXT_COURSE,
        TEXT_SLOT_LARGE_FONT, RESULTS_TEXT_SLOT, 0, RESULTS_TEXT_RED,
        RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE, RESULTS_TEXT_ALPHA);
  }

  *displayListHead = gfx;
}

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_NoPointsText);

/*
 * Where the match results draw their parts, a row of MATCH_ROW_HEIGHT for
 * each player. With bets, the columns sit further left to make room for the
 * bet totals. Without them, the points step down a shorter row.
 */
#define MATCH_ROW_HEIGHT 72
#define MATCH_HEADING_Y 62
#define MATCH_RANK_Y 48
#define MATCH_PORTRAIT_Y 76
#define MATCH_NUMBER_Y 92
#define MATCH_STANDING_Y 108
#define BETTING_MATCH_BETS_LABEL_X 152
#define BETTING_MATCH_POINTS_LABEL_X 240
#define BETTING_MATCH_RANK_X 40
#define BETTING_MATCH_PORTRAIT_X 62
#define BETTING_MATCH_HOLE_COUNT_X 48
#define BETTING_MATCH_STANDING_X 80
#define BETTING_MATCH_BET_TOTAL_X 128
#define BETTING_MATCH_POINTS_X 234
#define PLAIN_MATCH_POINTS_LABEL_X 220
#define PLAIN_MATCH_RANK_X 56
#define PLAIN_MATCH_PORTRAIT_X 78
#define PLAIN_MATCH_HOLE_COUNT_X 64
#define PLAIN_MATCH_STANDING_X 96
#define PLAIN_MATCH_POINTS_X 218
#define PLAIN_MATCH_POINTS_ROW_HEIGHT 64
/* Hole counts from this one take two digits */
#define MIN_TWO_DIGIT_HOLE_COUNT 10
/* The standing words are 48x48, holding UP, DOWN and EVEN in rows of 16 */
#define STANDING_WORDS_SIZE 48
#define STANDING_WORD_WIDTH 48
#define STANDING_WORD_HEIGHT 16
#define STANDING_WORD_UP_ROW 0
#define STANDING_WORD_DOWN_ROW 16
#define STANDING_WORD_EVEN_ROW 32
/* Shifts the hole count and its word left, so that the pair stays centered */
#define STANDING_ONE_DIGIT_UP_SHIFT (-8)
#define STANDING_ONE_DIGIT_DOWN_SHIFT (-20)
#define STANDING_TWO_DIGIT_DOWN_SHIFT (-12)

/*
 * The match play results of two players: place, portrait, holes up or down,
 * any bets and Course Points.
 *
 * Doesn't match: 12 words differ. Two pairs of loads come in the other order,
 * swapping the registers they load.
 */
#ifdef NON_MATCHING
void DrawMatchResults(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 unused[4] UNUSED;
  s32 standingShift[MATCH_PLAYER_COUNT];
  s32 betShift = FOUR_DIGIT_BET_SHIFT;
  s32 standingRow;
  s32 holeCount;
  s32 i;

  for (i = 0; i != g_GameState.playerCount; i++) {
    if (f_PlayerBetTotals[i] > FOUR_DIGIT_BET_MAX) {
      betShift = 0;
    }

    if (f_PlayerBetTotals[i] < FOUR_DIGIT_BET_MIN) {
      betShift = 0;
    }
  }

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  if (f_HasBets) {
    DrawResultsLabel(&gfx, f_ResultsSlide + BETTING_MATCH_BETS_LABEL_X,
        f_ResultsTop + MATCH_HEADING_Y, RESULTS_LABEL_BETS);
    DrawResultsLabel(&gfx, f_ResultsSlide + BETTING_MATCH_POINTS_LABEL_X,
        f_ResultsTop + MATCH_HEADING_Y, RESULTS_LABEL_POINTS);
    DrawRank(&gfx, f_ResultsSlide + BETTING_MATCH_RANK_X,
        f_ResultsTop + MATCH_RANK_Y, f_PlayerRanks[0]);
    DrawRank(&gfx, f_ResultsSlide + BETTING_MATCH_RANK_X,
        f_ResultsTop + MATCH_RANK_Y + MATCH_ROW_HEIGHT, f_PlayerRanks[1]);
    DrawPlayerPortrait(&gfx, f_ResultsSlide + BETTING_MATCH_PORTRAIT_X,
        f_ResultsTop + MATCH_PORTRAIT_Y, 0, FALSE);
    DrawPlayerPortrait(&gfx, f_ResultsSlide + BETTING_MATCH_PORTRAIT_X,
        f_ResultsTop + MATCH_PORTRAIT_Y + MATCH_ROW_HEIGHT, 1, FALSE);
  } else {
    DrawResultsLabel(&gfx, f_ResultsSlide + PLAIN_MATCH_POINTS_LABEL_X,
        f_ResultsTop + MATCH_HEADING_Y, RESULTS_LABEL_POINTS);
    DrawRank(&gfx, f_ResultsSlide + PLAIN_MATCH_RANK_X,
        f_ResultsTop + MATCH_RANK_Y, f_PlayerRanks[0]);
    DrawRank(&gfx, f_ResultsSlide + PLAIN_MATCH_RANK_X,
        f_ResultsTop + MATCH_RANK_Y + MATCH_ROW_HEIGHT, f_PlayerRanks[1]);
    DrawPlayerPortrait(&gfx, f_ResultsSlide + PLAIN_MATCH_PORTRAIT_X,
        f_ResultsTop + MATCH_PORTRAIT_Y, 0, FALSE);
    DrawPlayerPortrait(&gfx, f_ResultsSlide + PLAIN_MATCH_PORTRAIT_X,
        f_ResultsTop + MATCH_PORTRAIT_Y + MATCH_ROW_HEIGHT, 1, FALSE);
  }

  LoadSmallPalette(&gfx, 0, ImageBody(f_MatchStandingWordsImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(f_MatchStandingWordsImage),
      G_IM_FMT_CI, STANDING_WORDS_SIZE, STANDING_WORDS_SIZE, 0, 0, 0, 0, 0, 0,
      0);
  for (i = 0; i != g_GameState.playerCount; i++) {
    if (f_PlayerResults[i] > 0) {
      if (f_PlayerResults[i] >= MIN_TWO_DIGIT_HOLE_COUNT) {
        standingShift[i] = 0;
      } else {
        standingShift[i] = STANDING_ONE_DIGIT_UP_SHIFT;
      }
    } else if (f_PlayerResults[i] < 0) {
      if (f_PlayerResults[i] <= -MIN_TWO_DIGIT_HOLE_COUNT) {
        standingShift[i] = STANDING_TWO_DIGIT_DOWN_SHIFT;
      } else {
        standingShift[i] = STANDING_ONE_DIGIT_DOWN_SHIFT;
      }
    }
  }

  for (i = 0; i != g_GameState.playerCount; i++) {
    if (f_PlayerResults[i] > 0) {
      standingRow = STANDING_WORD_UP_ROW;
    } else if (f_PlayerResults[i] < 0) {
      standingRow = STANDING_WORD_DOWN_ROW;
    } else {
      standingRow = STANDING_WORD_EVEN_ROW;
    }

    if (f_HasBets) {
      DrawTextureRect(&gfx,
          QuarterPixels(
              standingShift[i] + BETTING_MATCH_STANDING_X + f_ResultsSlide),
          QuarterPixels(
              f_ResultsTop + MATCH_STANDING_Y + (i * MATCH_ROW_HEIGHT)),
          QuarterPixels(standingShift[i] + BETTING_MATCH_STANDING_X +
              STANDING_WORD_WIDTH + f_ResultsSlide),
          QuarterPixels(f_ResultsTop + MATCH_STANDING_Y +
              (i * MATCH_ROW_HEIGHT) + STANDING_WORD_HEIGHT),
          G_TX_RENDERTILE, 0, TexelCoordinate(standingRow), TEXEL_STEP_ONE,
          TEXEL_STEP_ONE);
    } else {
      DrawTextureRect(&gfx,
          QuarterPixels(
              standingShift[i] + PLAIN_MATCH_STANDING_X + f_ResultsSlide),
          QuarterPixels(
              f_ResultsTop + MATCH_STANDING_Y + (i * MATCH_ROW_HEIGHT)),
          QuarterPixels(standingShift[i] + PLAIN_MATCH_STANDING_X +
              STANDING_WORD_WIDTH + f_ResultsSlide),
          QuarterPixels(f_ResultsTop + MATCH_STANDING_Y +
              (i * MATCH_ROW_HEIGHT) + STANDING_WORD_HEIGHT),
          G_TX_RENDERTILE, 0, TexelCoordinate(standingRow), TEXEL_STEP_ONE,
          TEXEL_STEP_ONE);
    }
  }

  LoadSmallPalette(&gfx, 0, ImageBody(f_ResultsLargeDigitsImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(f_ResultsLargeDigitsImage),
      G_IM_FMT_CI, LARGE_DIGIT_WIDTH, LARGE_DIGITS_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  for (i = 0; i != g_GameState.playerCount; i++) {
    if (f_HasBets) {
      /* An even match draws only the word */
      if (f_PlayerResults[0] != 0) {
        holeCount = f_PlayerResults[i];
        if (holeCount < 0) {
          holeCount = -holeCount;
        }

        sprintf(g_PrintBuffer, "%2d", holeCount);
        DrawDigitText(&gfx,
            (f_ResultsSlide + BETTING_MATCH_HOLE_COUNT_X) + standingShift[i],
            f_ResultsTop + MATCH_STANDING_Y + (i * MATCH_ROW_HEIGHT),
            g_PrintBuffer);
      }

      sprintf(g_PrintBuffer, f_NumberFormat, f_PlayerBetTotals[i]);
      DrawDigitText(&gfx,
          (BETTING_MATCH_BET_TOTAL_X + f_ResultsSlide) + betShift,
          f_ResultsTop + MATCH_NUMBER_Y + (i * MATCH_ROW_HEIGHT),
          g_PrintBuffer);
      if (f_PlayerPoints[i] < 0) {
        sprintf(g_PrintBuffer, f_NoPointsText, f_PlayerPoints[i]);
      } else {
        sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerPoints[i]);
      }

      DrawDigitText(&gfx, f_ResultsSlide + BETTING_MATCH_POINTS_X,
          f_ResultsTop + MATCH_NUMBER_Y + (i * MATCH_ROW_HEIGHT),
          g_PrintBuffer);
    } else {
      if (f_PlayerResults[0] != 0) {
        holeCount = f_PlayerResults[i];
        if (holeCount < 0) {
          holeCount = -holeCount;
        }

        sprintf(g_PrintBuffer, "%2d", holeCount);
        DrawDigitText(&gfx,
            (f_ResultsSlide + PLAIN_MATCH_HOLE_COUNT_X) + standingShift[i],
            f_ResultsTop + MATCH_STANDING_Y + (i * MATCH_ROW_HEIGHT),
            g_PrintBuffer);
      }

      if (f_PlayerPoints[i] < 0) {
        sprintf(g_PrintBuffer, f_NoPointsText, f_PlayerPoints[i]);
      } else {
        sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerPoints[i]);
      }

      DrawDigitText(&gfx, f_ResultsSlide + PLAIN_MATCH_POINTS_X,
          f_ResultsTop + MATCH_NUMBER_Y + (i * PLAIN_MATCH_POINTS_ROW_HEIGHT),
          g_PrintBuffer);
    }
  }

  if (f_OpenCourseCount == COURSE_COUNT) {
    SetTextSlot(f_ResultsSlide + ALL_COURSES_OPEN_X,
        f_ResultsTop + MATCH_ALL_COURSES_OPEN_Y, MESSAGE_ALL_COURSES_OPEN,
        TEXT_SLOT_LARGE_FONT, RESULTS_TEXT_SLOT, 0, RESULTS_TEXT_RED,
        RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE, RESULTS_TEXT_ALPHA);
  } else {
    DrawResultsNumber(&gfx, f_ResultsSlide + POINTS_TOTAL_X, POINTS_TOTAL_Y,
        f_PointsToNextCourse + f_PointsEarned);
    SetTextSlot(f_ResultsSlide + POINTS_TO_NEXT_COURSE_X,
        f_ResultsTop + POINTS_TO_NEXT_COURSE_Y, MESSAGE_POINTS_TO_NEXT_COURSE,
        TEXT_SLOT_LARGE_FONT, RESULTS_TEXT_SLOT, 0, RESULTS_TEXT_RED,
        RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE, RESULTS_TEXT_ALPHA);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", DrawMatchResults);
#endif

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_UnusedModeNamesText);

#define CLUB_TOURNAMENT_COURSE_STRIP_WIDTH 64
#define CLUB_TOURNAMENT_COURSE_STRIP_HEIGHT 32
#define CLUB_TOURNAMENT_COURSE_STRIP_COUNT 2
#define NINTENDO_BADGE_STRIP_WIDTH 144
#define NINTENDO_BADGE_STRIP_HEIGHT 10
#define NINTENDO_BADGE_STRIP_COUNT 2
/* Without the badge the details move up into its place and spread out */
#define CLUB_TOURNAMENT_DETAILS_TOP 74
#define CLUB_TOURNAMENT_LINE_HEIGHT 16
#define CLUB_TOURNAMENT_UNBADGED_DETAILS_TOP 58
#define CLUB_TOURNAMENT_UNBADGED_LINE_HEIGHT 18
#define CLUB_TOURNAMENT_NAME_SLOT 3
#define CLUB_TOURNAMENT_COURSE_SLOT 4
#define CLUB_TOURNAMENT_MODE_SLOT 5
#define CLUB_TOURNAMENT_TEXT_FLAGS (TEXT_FLAG_CENTERED | TEXT_FLAG_UNK20)
/* Where the club tournament results draw their parts */
#define CLUB_TOURNAMENT_COURSE_X 28
#define CLUB_TOURNAMENT_COURSE_Y 104
#define CLUB_TOURNAMENT_PORTRAIT_X 96
#define BADGED_PORTRAIT_Y 136
#define UNBADGED_PORTRAIT_Y 128
#define NINTENDO_BADGE_X 88
#define NINTENDO_BADGE_Y 54
#define CLUB_TOURNAMENT_DETAILS_X 160
#define CLUB_TOURNAMENT_LABEL_X 144
#define CLUB_TOURNAMENT_VALUE_X 192
/* The text lines sit this far below their line's top */
#define CLUB_TOURNAMENT_TEXT_DROP 8
#define SCORE_PASSWORD_X 116
#define SCORE_PASSWORD_FIRST_Y 182
#define SCORE_PASSWORD_SECOND_Y 200

/* The club tournament details' lines, from the tournament's number down */
enum ClubTournamentLine {
  CLUB_TOURNAMENT_LINE_NUMBER,
  CLUB_TOURNAMENT_LINE_NAME,
  CLUB_TOURNAMENT_LINE_COURSE,
  CLUB_TOURNAMENT_LINE_MODE,
  CLUB_TOURNAMENT_LINE_SCORE,
  CLUB_TOURNAMENT_LINE_TIME
};

/* The top of a club tournament details' text line */
#define ClubTournamentTextY(detailsTop, lineHeight, line) \
  (f_ResultsTop + (detailsTop) + ((lineHeight) * (line)) + \
      CLUB_TOURNAMENT_TEXT_DROP)

/* DrawLargeTextWithShadow ignores this flag */
#define CLUB_TOURNAMENT_LABEL_FLAGS 4
#define CLUB_TOURNAMENT_LABEL_ENV_COLOR 0xFF5B0000
#define CLUB_TOURNAMENT_LABEL_PRIM_COLOR 0xFFFF0000
#define SCORE_PASSWORD_ENV_COLOR 0x00FF0000
#define SCORE_PASSWORD_PRIM_COLOR 0xFFFF0000

/*
 * The results of a club tournament: its course, number, name and mode, the
 * score and the password to send in.
 *
 * Doesn't match: GCC strength-reduces the course strips' top into a register
 * and ends their loop with slti, where the original adds the strip's offset
 * to the top each time and compares the counter with its limit held in a
 * register. Ending the loop with != instead of < gives 34 words, not 20.
 * 14 words differ.
 */
#ifdef NON_MATCHING
void DrawClubTournamentResults(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 width;
  s32 height;
  s32 gameMode;
  Boolean isNintendo;
  s32 detailsTop;
  s32 lineHeight;
  s32 seconds;
  s32 minutes;
  s32 hundredths;
  s32 i;
  s32 firstStripBottom;

  isNintendo = IsNintendoClubTournament();
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gameMode = GetClubTournamentGameMode();
  LoadLargePalette(&gfx, ImageBody(f_ResultsCourseImage));
  for (i = 0; i < CLUB_TOURNAMENT_COURSE_STRIP_COUNT; i++) {
    LoadTextureBlock(&gfx,
        LargeImageTexels(f_ResultsCourseImage) +
            (i * CLUB_TOURNAMENT_COURSE_STRIP_WIDTH *
                CLUB_TOURNAMENT_COURSE_STRIP_HEIGHT),
        G_IM_FMT_CI, G_IM_SIZ_8b, CLUB_TOURNAMENT_COURSE_STRIP_WIDTH,
        CLUB_TOURNAMENT_COURSE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    firstStripBottom = f_ResultsTop + CLUB_TOURNAMENT_COURSE_Y +
        CLUB_TOURNAMENT_COURSE_STRIP_HEIGHT;
    DrawTextureRect(&gfx,
        QuarterPixels(f_ResultsSlide + CLUB_TOURNAMENT_COURSE_X),
        QuarterPixels((i * CLUB_TOURNAMENT_COURSE_STRIP_HEIGHT) +
            CLUB_TOURNAMENT_COURSE_Y + f_ResultsTop),
        QuarterPixels(f_ResultsSlide + CLUB_TOURNAMENT_COURSE_X +
            CLUB_TOURNAMENT_COURSE_STRIP_WIDTH),
        QuarterPixels(
            (i * CLUB_TOURNAMENT_COURSE_STRIP_HEIGHT) + firstStripBottom),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  detailsTop = CLUB_TOURNAMENT_DETAILS_TOP;
  if (isNintendo) {
    lineHeight = CLUB_TOURNAMENT_LINE_HEIGHT;
    DrawPlayerPortrait(&gfx, f_ResultsSlide + CLUB_TOURNAMENT_PORTRAIT_X,
        f_ResultsTop + BADGED_PORTRAIT_Y, 0, TRUE);
    LoadLargePalette(&gfx, ImageBody(f_NintendoBadgeImage));
    for (i = 0; i != NINTENDO_BADGE_STRIP_COUNT; i++) {
      LoadTextureBlock(&gfx,
          LargeImageTexels(f_NintendoBadgeImage) +
              (i * NINTENDO_BADGE_STRIP_WIDTH * NINTENDO_BADGE_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, NINTENDO_BADGE_STRIP_WIDTH,
          NINTENDO_BADGE_STRIP_HEIGHT, 0, G_TX_MIRROR, 0, 0, 0, 0, 0);
      DrawTextureRect(&gfx, QuarterPixels(f_ResultsSlide + NINTENDO_BADGE_X),
          QuarterPixels(f_ResultsTop + NINTENDO_BADGE_Y +
              (i * NINTENDO_BADGE_STRIP_HEIGHT)),
          QuarterPixels(
              f_ResultsSlide + NINTENDO_BADGE_X + NINTENDO_BADGE_STRIP_WIDTH),
          QuarterPixels(f_ResultsTop + NINTENDO_BADGE_Y +
              (i * NINTENDO_BADGE_STRIP_HEIGHT) + NINTENDO_BADGE_STRIP_HEIGHT),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  } else {
    detailsTop = CLUB_TOURNAMENT_UNBADGED_DETAILS_TOP;
    lineHeight = CLUB_TOURNAMENT_UNBADGED_LINE_HEIGHT;
    DrawPlayerPortrait(&gfx, f_ResultsSlide + CLUB_TOURNAMENT_PORTRAIT_X,
        f_ResultsTop + UNBADGED_PORTRAIT_Y, 0, TRUE);
  }

  DrawPlaceNumber(&gfx, f_ResultsSlide + CLUB_TOURNAMENT_DETAILS_X,
      f_ResultsTop + detailsTop, GetClubTournamentNumber());
  SetUpTextDrawing(&gfx);
  MeasureMessage(
      GetClubTournamentNameMessage(), FONT_SIZE_LARGE, &width, &height, 0);
  MeasureMessage(GetClubTournamentCourseTitleMessage(), FONT_SIZE_LARGE, &width,
      &height, 0);
  MeasureMessage(
      GetClubTournamentModeMessage(), FONT_SIZE_LARGE, &width, &height, 0);
  SetTextSlot(f_ResultsSlide + CLUB_TOURNAMENT_DETAILS_X,
      ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_NAME),
      GetClubTournamentNameMessage(), TEXT_SLOT_LARGE_FONT,
      CLUB_TOURNAMENT_NAME_SLOT, CLUB_TOURNAMENT_TEXT_FLAGS, 127, 191, 255,
      255);
  SetTextSlot(f_ResultsSlide + CLUB_TOURNAMENT_DETAILS_X,
      ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_COURSE),
      GetClubTournamentCourseTitleMessage(), TEXT_SLOT_LARGE_FONT,
      CLUB_TOURNAMENT_COURSE_SLOT, CLUB_TOURNAMENT_TEXT_FLAGS, 43, 255, 75,
      255);
  SetTextSlot(f_ResultsSlide + CLUB_TOURNAMENT_DETAILS_X,
      ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_MODE),
      GetClubTournamentModeMessage(), TEXT_SLOT_LARGE_FONT,
      CLUB_TOURNAMENT_MODE_SLOT, CLUB_TOURNAMENT_TEXT_FLAGS, 255, 63, 127, 255);
  for (i = CLUB_TOURNAMENT_NAME_SLOT; i != CLUB_TOURNAMENT_MODE_SLOT + 1; i++) {
    gDPSetPrimColor(gfx++, 0, 0, f_TextSlotColors[i - 1][0],
        f_TextSlotColors[i - 1][1], f_TextSlotColors[i - 1][2], 255);
    DrawLargeTextSlot(&gfx, i);
  }

  SetUpGradientTextDrawing(&gfx, 0);
  sprintf(g_PrintBuffer, "Score");
  DrawLargeTextWithShadow(&gfx, f_ResultsSlide + CLUB_TOURNAMENT_LABEL_X,
      ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_SCORE),
      g_PrintBuffer, CLUB_TOURNAMENT_LABEL_ENV_COLOR,
      CLUB_TOURNAMENT_LABEL_PRIM_COLOR, CLUB_TOURNAMENT_LABEL_FLAGS);
  if (f_PlayerResults[0] == 0) {
    sprintf(g_PrintBuffer, "#0", 0);
  } else {
    sprintf(g_PrintBuffer, "%+d", f_PlayerResults[0]);
  }

  DrawLargeTextWithShadow(&gfx, f_ResultsSlide + CLUB_TOURNAMENT_VALUE_X,
      ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_SCORE),
      g_PrintBuffer, CLUB_TOURNAMENT_LABEL_ENV_COLOR,
      CLUB_TOURNAMENT_LABEL_PRIM_COLOR, TEXT_FLAG_FIXED_WIDTH);
  if (gameMode == GAME_MODE_SPEED_GOLF) {
    hundredths = (s32)(g_GameState.stopwatchFrameCount *
                     ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
        HUNDREDTHS_PER_SECOND;
    seconds = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND %
        SECONDS_PER_MINUTE;
    minutes = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND /
        SECONDS_PER_MINUTE % SHOWN_MINUTES_LIMIT;
    sprintf(g_PrintBuffer, f_TimeLabel);
    DrawLargeTextWithShadow(&gfx, f_ResultsSlide + CLUB_TOURNAMENT_LABEL_X,
        ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_TIME),
        g_PrintBuffer, CLUB_TOURNAMENT_LABEL_ENV_COLOR,
        CLUB_TOURNAMENT_LABEL_PRIM_COLOR, CLUB_TOURNAMENT_LABEL_FLAGS);
    sprintf(g_PrintBuffer, f_TimeFormat, minutes, seconds, hundredths);
    DrawLargeTextWithShadow(&gfx, f_ResultsSlide + CLUB_TOURNAMENT_VALUE_X,
        ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_TIME),
        g_PrintBuffer, CLUB_TOURNAMENT_LABEL_ENV_COLOR,
        CLUB_TOURNAMENT_LABEL_PRIM_COLOR, TEXT_FLAG_FIXED_WIDTH);
  }

  for (i = 0; i != SCORE_PASSWORD_LINE_LENGTH; i++) {
    g_PrintBuffer[i] = f_ScorePassword[i];
  }

  g_PrintBuffer[SCORE_PASSWORD_LINE_LENGTH] = '\0';
  DrawLargeTextWithShadow(&gfx, f_ResultsSlide + SCORE_PASSWORD_X,
      f_ResultsTop + SCORE_PASSWORD_FIRST_Y, g_PrintBuffer,
      SCORE_PASSWORD_ENV_COLOR, SCORE_PASSWORD_PRIM_COLOR,
      TEXT_FLAG_FIXED_WIDTH);
  for (i = 0; i != SCORE_PASSWORD_LINE_LENGTH; i++) {
    g_PrintBuffer[i] = f_ScorePassword[SCORE_PASSWORD_LINE_LENGTH + i];
  }

  g_PrintBuffer[SCORE_PASSWORD_LINE_LENGTH] = '\0';
  DrawLargeTextWithShadow(&gfx, f_ResultsSlide + SCORE_PASSWORD_X,
      f_ResultsTop + SCORE_PASSWORD_SECOND_Y, g_PrintBuffer,
      SCORE_PASSWORD_ENV_COLOR, SCORE_PASSWORD_PRIM_COLOR,
      TEXT_FLAG_FIXED_WIDTH);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", DrawClubTournamentResults);
#endif

#define BACKDROP_X 16
#define BACKDROP_Y 24
#define BACKDROP_STRIP_WIDTH 288
#define BACKDROP_STRIP_HEIGHT 5
#define BACKDROP_STRIP_COUNT 40
/* The club tournament backdrop is taller, so it starts higher */
#define CLUB_TOURNAMENT_BACKDROP_Y 8
#define CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT 7
#define CLUB_TOURNAMENT_BACKDROP_STRIP_COUNT 32
/* The pressing hand waits in the card's lower right corner */
#define HAND_X 272
#define HAND_Y 192
#define HAND_SIZE 24
/* The pressing hand shows each of its frames for this many draws */
#define HAND_FRAME_DRAWS 3
#define HAND_FRAME_COUNT 3
#define FINAL_RESULTS_TITLE_WIDTH 200
#define FINAL_RESULTS_TITLE_STRIP_HEIGHT 8
/* Each strip loads a row more than it shows so that filtering has a row below
 * it */
#define FINAL_RESULTS_TITLE_LOADED_HEIGHT 10
#define FINAL_RESULTS_TITLE_LAST_STRIP_HEIGHT 6
#define FINAL_RESULTS_TITLE_STRIP_COUNT 4
#define FINAL_RESULTS_TITLE_X 24.0f
#define FINAL_RESULTS_TITLE_Y 20.0f
#define FINAL_RESULTS_TITLE_BOB 5.0f
#define FINAL_RESULTS_TITLE_BOB_STEP (M_PI / 18)
/* A texture coordinate of this many units steps one texel */
#define TEXEL_UNITS 32
#define SUBTEXEL_STEPS 16.0f
/* The portrait animations load their faces only every fourth draw */
#define PORTRAIT_FRAME_DRAWS_MASK 3
#define PORTRAIT_ANIMATION_FRAME_COUNT 5
#define PORTRAIT_ANIMATION_REST 0
#define NO_PORTRAIT_FRAME (-1)

/* The index of the last of stripCount strips */
static inline s32 GetLastStripIndex(s32 stripCount) {
  return stripCount - 1;
}

/*
 * The results card, its game mode's results, the bobbing title and the players'
 * portraits.
 *
 * Doesn't match: GCC merges the doubling of the title's texture coordinates
 * into the shift that packs them, where the original shifts twice. 8 words
 * differ.
 */
#ifdef NON_MATCHING
void DrawRoundResults(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  f32 titleX;
  f32 titleY;
  s32 left;
  s32 top;
  s32 right;
  s32 stripHeight;
  s16 textureS;
  s32 textureT;
  s32 i;

  if (!f_AreResultsLoaded) {
    return;
  }

  gSPDisplayList(gfx++, g_DecalTextureSetup);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  LoadLargePalette(&gfx, ImageBody(f_ResultsBackdropImage));
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  if (g_GameState.isClubTournament) {
    for (i = 0; i != CLUB_TOURNAMENT_BACKDROP_STRIP_COUNT; i++) {
      LoadTextureBlock(&gfx,
          LargeImageTexels(f_ResultsBackdropImage) +
              (i * BACKDROP_STRIP_WIDTH *
                  CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_STRIP_WIDTH,
          CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      DrawTextureRect(&gfx, QuarterPixels(f_ResultsSlide + BACKDROP_X),
          QuarterPixels((i * CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT) +
              CLUB_TOURNAMENT_BACKDROP_Y + f_ResultsTop),
          QuarterPixels(f_ResultsSlide + BACKDROP_X + BACKDROP_STRIP_WIDTH),
          QuarterPixels((i * CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT) +
              CLUB_TOURNAMENT_BACKDROP_Y +
              CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT + f_ResultsTop),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  } else {
    for (i = 0; i != BACKDROP_STRIP_COUNT; i++) {
      LoadTextureBlock(&gfx,
          LargeImageTexels(f_ResultsBackdropImage) +
              (i * BACKDROP_STRIP_WIDTH * BACKDROP_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_STRIP_WIDTH, BACKDROP_STRIP_HEIGHT,
          0, 0, 0, 0, 0, 0, 0);
      DrawTextureRect(&gfx, QuarterPixels(f_ResultsSlide + BACKDROP_X),
          QuarterPixels(
              (i * BACKDROP_STRIP_HEIGHT) + BACKDROP_Y + f_ResultsTop),
          QuarterPixels(f_ResultsSlide + BACKDROP_X + BACKDROP_STRIP_WIDTH),
          QuarterPixels((i * BACKDROP_STRIP_HEIGHT) + BACKDROP_Y +
              BACKDROP_STRIP_HEIGHT + f_ResultsTop),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  if (f_IsAwaitingConfirm == TRUE) {
    f_HandFrame = (f_HandFrame + 1) % (HAND_FRAME_COUNT * HAND_FRAME_DRAWS);
    LoadLargePalette(&gfx, ImageBody(f_ResultsHandImage));
    LoadTextureBlock(&gfx,
        LargeImageTexels(f_ResultsHandImage) +
            (f_HandFrame / HAND_FRAME_DRAWS * HAND_SIZE * HAND_SIZE),
        G_IM_FMT_CI, G_IM_SIZ_8b, HAND_SIZE, HAND_SIZE, 0, 0, 0, 0, 0, 0, 0);
    DrawTextureRect(&gfx, QuarterPixels(f_ResultsSlide + HAND_X),
        QuarterPixels(f_ResultsTop + HAND_Y),
        QuarterPixels(f_ResultsSlide + HAND_X + HAND_SIZE),
        QuarterPixels(f_ResultsTop + HAND_Y + HAND_SIZE), G_TX_RENDERTILE, 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  if (g_GameState.isClubTournament) {
    DrawClubTournamentResults(&gfx);
  } else {
    switch (g_GameState.gameMode) {
      case GAME_MODE_SPEED_GOLF:
        DrawSpeedGolfResults(&gfx);
        break;
      case GAME_MODE_STROKE:
      case GAME_MODE_UNK1:
      case GAME_MODE_TRAINING:
      case GAME_MODE_UNK6:
      case GAME_MODE_UNK8:
      case GAME_MODE_RING_SHOT:
      case GAME_MODE_SKINS_MATCH:
      case GAME_MODE_CLUB_SLOTS:
        DrawPlayersResults(&gfx);
        break;
      case GAME_MODE_MATCH_GAME:
      case GAME_MODE_GET_CHARACTER:
        DrawMatchResults(&gfx);
        break;
      case GAME_MODE_TOURNAMENT:
        DrawTournamentResults(&gfx);
        break;
    }
  }

  if (!g_GameState.isClubTournament) {
    titleX = (cosf(f_TitleBobAngle * 0.5f) * FINAL_RESULTS_TITLE_BOB) +
        FINAL_RESULTS_TITLE_X;
    titleY = (sinf(f_TitleBobAngle) * FINAL_RESULTS_TITLE_BOB) +
        FINAL_RESULTS_TITLE_Y;
    left = titleX < 0.0f ? titleX + 0.5f : titleX - 0.5f;
    top = titleY < 0.0f ? titleY + 0.5f : titleY - 0.5f;
    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_AVERAGE);
    textureS = (s32)(SUBTEXEL_STEPS - ((titleX - left) * SUBTEXEL_STEPS)) * 2;
    textureT = ((s32)(SUBTEXEL_STEPS - ((titleY - top) * SUBTEXEL_STEPS)) * 2) +
        TEXEL_UNITS;
    f_TitleBobAngle += FINAL_RESULTS_TITLE_BOB_STEP;
    LoadLargePalette(&gfx, ImageBody(f_FinalResultsTitleImage));
    right = left + FINAL_RESULTS_TITLE_WIDTH;
    for (i = 0; i != FINAL_RESULTS_TITLE_STRIP_COUNT; i++) {
      stripHeight = FINAL_RESULTS_TITLE_STRIP_HEIGHT;
      /* The helper keeps the limit of 3 in a register, to match */
      if (i == GetLastStripIndex(FINAL_RESULTS_TITLE_STRIP_COUNT)) {
        stripHeight = FINAL_RESULTS_TITLE_LAST_STRIP_HEIGHT;
      }

      LoadTextureBlock(&gfx,
          LargeImageTexels(f_FinalResultsTitleImage) +
              (i * FINAL_RESULTS_TITLE_WIDTH *
                  FINAL_RESULTS_TITLE_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, FINAL_RESULTS_TITLE_WIDTH,
          FINAL_RESULTS_TITLE_LOADED_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      gSPTextureRectangle(gfx++, QuarterPixels(left - f_ResultsSlide),
          QuarterPixels(
              i * FINAL_RESULTS_TITLE_STRIP_HEIGHT + f_ResultsTop + top),
          QuarterPixels(right - f_ResultsSlide),
          QuarterPixels(i * FINAL_RESULTS_TITLE_STRIP_HEIGHT + stripHeight +
              f_ResultsTop + top),
          G_TX_RENDERTILE, textureS, textureT, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  for (i = 0; i != g_GameState.playerCount; i++) {
    EncodedAsset asset;

    if (f_PortraitFrames[i] != NO_PORTRAIT_FRAME) {
      if (!(f_PortraitTimers[i] & PORTRAIT_FRAME_DRAWS_MASK)) {
        switch (f_PortraitAnimations[i]) {
          case PORTRAIT_ANIMATION_REST:
            OpenEncodedAsset(
                g_CharacterImageAssets[(s8)g_Players[i].characterIndex],
                &asset);
            ReadEncodedAsset(f_PlayerPortraitImages[i], &asset);
            f_PortraitFrames[i] = NO_PORTRAIT_FRAME;
            break;
          case PORTRAIT_ANIMATION_UNK1:
            OpenEncodedAsset(
                g_CharacterImageAssets[(s8)g_Players[i].characterIndex] +
                    f_PortraitFrames[i],
                &asset);
            ReadEncodedAsset(f_PlayerPortraitImages[i], &asset);
            f_PortraitFrames[i]++;
            if (f_PortraitFrames[i] == PORTRAIT_ANIMATION_FRAME_COUNT) {
              f_PortraitFrames[i] = NO_PORTRAIT_FRAME;
            }

            break;
          case PORTRAIT_ANIMATION_UNK4: {
            s32 unk4Faces[PORTRAIT_ANIMATION_FRAME_COUNT] = {0, 5, 6, 7, 8};

            OpenEncodedAsset(
                g_CharacterImageAssets[(s8)g_Players[i].characterIndex] +
                    unk4Faces[f_PortraitFrames[i]],
                &asset);
            ReadEncodedAsset(f_PlayerPortraitImages[i], &asset);
            f_PortraitFrames[i]++;
            if (f_PortraitFrames[i] == PORTRAIT_ANIMATION_FRAME_COUNT) {
              f_PortraitFrames[i] = NO_PORTRAIT_FRAME;
            }

            break;
          }
        }
      }

      f_PortraitTimers[i]++;
    }
  }

  DrawResultsBanner(&gfx);
  *displayListHead = gfx;
}
#else
INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_Unk4FaceFrames);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", DrawRoundResults);
#endif

void DrawResultsNotice(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  switch (f_UnlockNoticeKind) {
    case UNLOCK_NOTICE_KIND_NONE:
      break;
    case UNLOCK_NOTICE_KIND_COURSE:
      DrawCourseResultsNotice(&gfx);
      break;
    case UNLOCK_NOTICE_KIND_CHARACTER:
      DrawCharacterResultsNotice(&gfx);
      break;
  }

  *displayListHead = gfx;
}
