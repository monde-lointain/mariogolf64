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
#include "functions.h"
#include "game_flags.h"
#include "heap.h"
#include "main.h"
#include "math_util.h"
#include "menu.h"
#include "password.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "tournament.h"
#include "transfer_pak.h"
#include "variables.h"
/* OpenRoundResults's request that frees the results */
#define CLOSE_ROUND_RESULTS -1

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
  UNLOCK_NOTICE_PEACH,
  UNLOCK_NOTICE_METAL_MARIO,
  UNLOCK_NOTICE_COURSES /* The first of the courses' unlocks */
};

/* f_UnlockNoticeKind values */
enum UnlockNoticeKind {
  UNLOCK_NOTICE_KIND_NONE,
  UNLOCK_NOTICE_KIND_COURSE,
  UNLOCK_NOTICE_KIND_CHARACTER
};

#define RESULTS_DIGIT_WIDTH 8
#define RESULTS_DIGIT_HEIGHT 12

/* f_TallySpeed normally, and while A is held */
#define NORMAL_TALLY_SPEED 1
#define FAST_TALLY_SPEED 10
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
#define NOTICE_SWING_STEP (M_PI / 9)
#define NOTICE_SWING_FRAMES 46
#define NOTICE_DONE_FRAME 51

/*
 * The character notice's portrait changes face every few frames after
 * a pause, then lifts away, faster and faster, until it is off screen
 */
#define NOTICE_FACE_DELAY 10
#define NOTICE_FACE_INTERVAL_MASK 3 /* Every fourth frame */
#define NOTICE_LAST_FACE 3
#define NOTICE_LIFT_DELAY 50
#define NOTICE_LIFT_START_SPEED 8
#define NOTICE_LIFT_EXIT -200

/* The places image's 32x32 icons, after its 256-color palette */
#define PLACE_ICON_SIZE 32

/* The labels image's 16-color strips. Each is stored wider than drawn. */
#define RESULTS_LABEL_WIDTH 72
#define RESULTS_LABEL_STORED_WIDTH 80
#define RESULTS_LABEL_HEIGHT 18

#define PORTRAIT_SIZE 32

/* Course points are clamped there rather than overflow */
#define MAX_COURSE_POINTS 0x7FFFFFFF

/* The places of f_TournamentPrizes's columns after the trophy winners' */
#define TROPHY_PLACE_COUNT 3
#define PRIZE_TOP_TEN 10
#define PRIZE_TOP_TWENTY 20

/* The results' images. g_GameState.isClubTournament swaps in another set. */
#define RESULTS_BACKDROP_ASSET 0x6A0
#define CLUB_TOURNAMENT_BACKDROP_ASSET 0x947
#define NINTENDO_BADGE_ASSET 0x949
#define FINAL_RESULTS_TITLE_ASSET 0x69F
#define NOTICE_TITLE_ASSET 0x55F
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
#define NOTICE_SWING_SOUND 0x69
#define NOTICE_SWING_SOUND_CHANNEL 13
#define NOTICE_SWING_SOUND_VOLUME 95
#define NOTICE_SHINE_SOUND 0x43
#define NOTICE_SHINE_SOUND_CHANNEL 14
#define NOTICE_SHINE_SOUND_VOLUME 63
#define RESULTS_LABEL_SCORE 2
#define RESULTS_LABEL_POINTS 5
#define MESSAGE_POINTS_TO_NEXT_COURSE 0x46
#define MESSAGE_ALL_COURSES_OPEN 0x47
#define RESULTS_TEXT_SLOT 0
#define RESULTS_TEXT_FONT 1
#define RESULTS_TEXT_RED 230
#define RESULTS_TEXT_GREEN 230
#define RESULTS_TEXT_BLUE 255
#define RESULTS_TEXT_ALPHA 255
#define LARGE_DIGIT_WIDTH 16
#define LARGE_DIGITS_HEIGHT 256
#define POINTS_TOTAL_Y 201
/* The minutes wrap here, since the time shows two digits */
#define SHOWN_MINUTES_LIMIT 99
#define RESULTS_LABEL_BETS 4
#define RESULTS_HEADING_Y 54
/* Bet totals of four digits all shift left, so that the column stays centered
 */
#define FOUR_DIGIT_BET_SHIFT -16
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
s32 UpdateTournamentResults();
/*
 * UpdateRoundResults calls this without the controller argument, so it
 * is declared without a prototype
 */
s32 ShowScorePassword();
s32 UpdateCoursePointsResults();
s32 UpdateMatchResults();
s32 func_8005CEE0(s32 index);
void RecordComCharacterResult(s32 row, s32 column, Boolean isWin);

/* The score password shows as two lines of ten symbols */
#define SCORE_PASSWORD_LINE_LENGTH 10

/* The images of the trophy a course's tournament gives its top three */
typedef struct TrophyAssets {
  /* 0x0 */ s32 palette; /* For first place; second and third follow */
  /* 0x4 */ s32 image;
  /* 0x8 */ s32 frameCount; /* Of image, as the trophy turns */
} TrophyAssets; /* size = 0xC */

/* Each course's trophy image's sizes, indexed by these */
#define TROPHY_WIDTH 0
#define TROPHY_PART_HEIGHTS 1 /* TROPHY_PART_COUNT of them */
#define TROPHY_SHINE_HEIGHT 3
#define TROPHY_SIZE_COUNT 4

/* An unread pointer to one of this file's strings */
extern char D_80224E10_ovl_00[];

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
static s32 f_TournamentPrizes[COURSE_COUNT][TROPHY_PLACE_COUNT + 3] = {
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
static s32 D_8021FDBC_ovl_00[6] = {0, 1, 2, 3, 2, 1};
static s32 f_TrophySizes[COURSE_COUNT][TROPHY_SIZE_COUNT] = {{40, 24, 24, 38},
    {40, 40, 11, 40}, {40, 40, 11, 50}, {32, 32, 32, 64}, {48, 22, 20, 21},
    {36, 40, 11, 40}};
static s32 f_TrophyShineFrame = 0;
static s32 f_TrophyFrame = 0;
static s32 f_DebugUnlockCharacter = 0;
/* Read by no routine */
static void* D_8021FE40_ovl_00[2] = {NULL, D_80224E10_ovl_00};
/* The text color of each text slot, from slot 1 */
static u8 f_TextSlotColors[][3] = {{0xFF, 0xFF, 0xFF}, {0x7F, 0xBF, 0xFF},
    {0x7F, 0xBF, 0xFF}, {0x7F, 0xFF, 0x4B}, {0xFF, 0x7F, 0x9F}};
/* Counts up through the pressing hand's frames, HAND_FRAME_DRAWS draws to a
 * frame */
static s32 f_HandFrame = 0;
static Radians f_TitleBobAngle = 0.0f;

/* Read by no routine */
static u8 D_802256B0_ovl_00[0x10];
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
static u8* D_80225708_ovl_00;
static u8* f_NameplateImage;
static u8* f_NoticeTitleImage;
static u8* f_ResultsShineImage;
static u8* D_80225718_ovl_00;
static u8* D_8022571C_ovl_00;
static u8* f_TrophyImage;
static u8* f_TrophyPalette;
static u8* f_ResultsHandImage;
static u8* f_MatchStandingWordsImage;
static u8* f_NintendoBadgeImage;
static u8* f_DigitsImage;
static u8* f_PlaceDigitsImage;
static s32 f_UnlockNoticeKind;
static Boolean f_HasBets;
static s32 f_PortraitCharacterIndex;
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
static char f_ScorePassword[2 * SCORE_PASSWORD_LINE_LENGTH];
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
s32 SumCoursePointsNeeded(s32 courseIndex) {
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
  f_IsAwaitingConfirm = 0;
  if (IsGameFlagSet(GAME_FLAG_ADD_500_POINTS)) {
    g_Players[0].points += 500;
  }

  if (IsGameFlagSet(GAME_FLAG_ADD_9999_POINTS)) {
    g_Players[0].points += 9999;
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
        AllocMemory(OpenEncodedAsset(NINTENDO_BADGE_ASSET, &asset));
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
        AllocMemory(OpenEncodedAsset(NOTICE_TITLE_ASSET, &asset));
    ReadEncodedAsset(f_NoticeTitleImage, &asset);
    f_ResultsShineImage =
        AllocMemory(OpenEncodedAsset(ASSET_SHINE_IMAGE, &asset));
    ReadEncodedAsset(f_ResultsShineImage, &asset);
    D_80225718_ovl_00 =
        AllocMemory(OpenEncodedAsset(RESULTS_UNK72C_IMAGE_ASSET, &asset));
    ReadEncodedAsset(D_80225718_ovl_00, &asset);
    OpenAssetFile(RESULTS_UNK72D_PALETTE_ASSET, &file);
    size = GetAssetFileSize(&file);
    D_8022571C_ovl_00 = AllocMemory(size);
    ReadAssetFile(D_8022571C_ovl_00, size, &file);
  }

  for (i = 0; i != g_GameState.playerCount; i++) {
    f_PlayerPortraitImages[i] = AllocMemory(OpenEncodedAsset(
        g_CharacterImageAssets[(s8)g_Players[i].characterIndex], &asset));
    ReadEncodedAsset(f_PlayerPortraitImages[i], &asset);
    OpenAssetFile(g_CharacterPaletteAssets[(s8)g_Players[i].characterIndex] +
            (s8)g_Players[i].paletteIndex,
        &file);
    size = GetAssetFileSize(&file);
    f_PlayerPortraitPalettes[i] = AllocMemory(size);
    ReadAssetFile(f_PlayerPortraitPalettes[i], size, &file);
    f_PlayerPoints[i] =
        (s8)g_Players[i].unk1E[0] == 0 ? g_Players[i].points : -1;
    f_PlayerBetTotals[i] = 0;
    for (j = 0; j != BET_COUNT; j++) {
      f_PlayerBetTotals[i] += g_Players[i].betTallies[j];
    }
  }

  if (!g_GameState.isClubTournament) {
    if (g_GameState.gameMode == GAME_MODE_MATCH_GAME ||
        g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
      f_ResultsPlacesImage =
          AllocMemory(OpenEncodedAsset(MATCH_PLACES_IMAGE_ASSET, &asset));
      ReadEncodedAsset(f_ResultsPlacesImage, &asset);
      D_80225708_ovl_00 =
          AllocMemory(OpenEncodedAsset(MATCH_UNK6A3_ASSET, &asset));
      ReadEncodedAsset(D_80225708_ovl_00, &asset);
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
            if (i != j && f_PlayerResults[j] < f_PlayerResults[i]) {
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

        if (f_PlayerResults[1] < f_PlayerResults[0]) {
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
        D_80225708_ovl_00 =
            AllocMemory(OpenEncodedAsset(TOURNAMENT_UNK6A1_ASSET, &asset));
        ReadEncodedAsset(D_80225708_ovl_00, &asset);
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
          f_PrizePoints =
              f_TournamentPrizes[g_GameState.courseIndex][TROPHY_PLACE_COUNT];
        } else if (f_PlayerRanks[0] <= PRIZE_TOP_TWENTY) {
          f_PrizePoints = f_TournamentPrizes[g_GameState.courseIndex]
                                            [TROPHY_PLACE_COUNT + 1];
        } else {
          f_PrizePoints = f_TournamentPrizes[g_GameState.courseIndex]
                                            [TROPHY_PLACE_COUNT + 2];
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
  s32 courseIndex;
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
  } else if (unlock == UNLOCK_NOTICE_PEACH) {
    f_PortraitCharacterIndex = CHARACTER_PEACH;
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
        AllocMemory(OpenEncodedAsset(NOTICE_TITLE_ASSET, &asset));
    ReadEncodedAsset(f_NoticeTitleImage, &asset);
    f_ResultsCourseImage = AllocMemory(
        OpenEncodedAsset(f_CourseNoticeAssets[courseIndex], &asset));
    ReadEncodedAsset(f_ResultsCourseImage, &asset);
  }

  if (f_UnlockNoticeKind == UNLOCK_NOTICE_KIND_CHARACTER) {
    f_NoticeTitleImage = AllocMemory(OpenEncodedAsset(0x560, &asset));
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
#ifdef NON_MATCHING
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

    return FALSE;
  }

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
    if (f_NoticeLift > -2) {
      f_NoticeLift -= NOTICE_LIFT_START_SPEED;
    } else {
      f_NoticeLift += f_NoticeLift * 2 / 3;
    }

    if (f_NoticeLift < NOTICE_LIFT_EXIT) {
      return TRUE;
    }
  }

  return FALSE;
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", UpdateUnlockNotice);
#endif

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
      FreeMemory(&D_80225718_ovl_00);
      FreeMemory(&D_8022571C_ovl_00);
    }

    for (i = 0; i != g_GameState.playerCount; i++) {
      FreeMemory(&f_PlayerPortraitImages[i]);
      FreeMemory(&f_PlayerPortraitPalettes[i]);
    }

    if (!g_GameState.isClubTournament) {
      if (g_GameState.gameMode == GAME_MODE_MATCH_GAME ||
          g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
        FreeMemory(&f_ResultsPlacesImage);
        FreeMemory(&D_80225708_ovl_00);
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
          FreeMemory(&D_80225708_ovl_00);
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

/* Ticks as the points count up, on every fourth call while f_TallySpeed is 1 */
void PlayTallySound(void) {
  f_TallySoundCount++;
  if (f_TallySpeed != 1 || !(f_TallySoundCount & 3)) {
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
  LoadSmallPalette(&gfx, 0, f_DigitsImage + IMAGE_HEADER_SIZE);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTextureBlock_4b(gfx++,
      ((u32)f_DigitsImage + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI, 128,
      RESULTS_DIGIT_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  c = (u8*)g_PrintBuffer;
  offset = 0;
  while (*c != '\0') {
    ch = *c;
    if (ch != ' ') {
      gSPScisTextureRectangle(gfx++, (x + offset) << 2, y << 2,
          (x + offset + RESULTS_DIGIT_WIDTH) << 2,
          (y + RESULTS_DIGIT_HEIGHT) << 2, G_TX_RENDERTILE,
          (ch - '0') * RESULTS_DIGIT_WIDTH << 5, 0, 1 << 10, 1 << 10);
      /* The empty do-while is there to match */
      do {
      } while (0);
    }

    c++;
    offset += RESULTS_DIGIT_WIDTH;
  }

  *displayListHead = gfx;
}

/* f_PlaceDigitsImage holds 24-pixel glyphs: the digits, then the st, nd, rd and
 * th suffixes */
#define PLACE_GLYPH_SIZE 24
#define PLACE_GLYPH_SPACING 20
#define PLACE_SUFFIX_GLYPH_FIRST 10
#define PLACE_SUFFIX_ST 0
#define PLACE_SUFFIX_TH 3
/* From the 4th to the 20th, places end in th */
#define PLACE_TEENS_END 20
#define PLACES_PER_HUNDRED 100
#define PLACE_NUMBER_X_OFFSET (-12)

/* Loads glyph from f_PlaceDigitsImage and draws it at x, y */
#define DRAW_PLACE_GLYPH(gfx, x, y, glyph) \
  { \
    gDPLoadTextureTile(gfx++, \
        (u32)(f_PlaceDigitsImage + LARGE_PALETTED_TEXELS_OFFSET) & ~7, \
        G_IM_FMT_CI, G_IM_SIZ_8b, PLACE_GLYPH_SIZE, 0, 0, \
        (glyph) * PLACE_GLYPH_SIZE, PLACE_GLYPH_SIZE - 1, \
        ((glyph) * PLACE_GLYPH_SIZE) + PLACE_GLYPH_SIZE - 1, 0, \
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, \
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD); \
    gSPScisTextureRectangle(gfx++, (x) << 2, (y) << 2, \
        ((x) + PLACE_GLYPH_SIZE) << 2, ((y) + PLACE_GLYPH_SIZE) << 2, \
        G_TX_RENDERTILE, 0, ((glyph) * PLACE_GLYPH_SIZE) << 5, TEXEL_STEP_ONE, \
        TEXEL_STEP_ONE); \
  }

/* Draws place, as in "21st", centered on x */
#ifdef NON_MATCHING
void DrawPlaceNumber(Gfx** displayListHead, s32 x, s32 y, s32 place) {
  Gfx* gfx = *displayListHead;
  char* c;
  s32 width;
  s32 suffix;
  s32 lastDigit;
  u8 character;
  s32 glyph;
  s32 left;

  sprintf(g_PrintBuffer, f_PlaceNumberFormat, place);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  LoadLargePalette(&gfx, f_PlaceDigitsImage + IMAGE_HEADER_SIZE);
  width = 0;
  for (c = g_PrintBuffer; *c != '\0'; c++) {
    width += PLACE_GLYPH_SPACING;
  }

  left = x + PLACE_NUMBER_X_OFFSET;
  left -= width / 2;
  width = 0;
  for (c = g_PrintBuffer; *c != '\0'; c++) {
    character = *c;
    if (character != ' ') {
      glyph = character - '0';
      DRAW_PLACE_GLYPH(gfx, left + width, y, glyph);
    }

    width += PLACE_GLYPH_SPACING;
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
      lastDigit = suffix % 10;
      if (lastDigit < PLACE_SUFFIX_TH) {
        suffix = lastDigit;
      } else {
        suffix = PLACE_SUFFIX_TH;
      }
    }
  }

  DRAW_PLACE_GLYPH(gfx, left + width, y, suffix + PLACE_SUFFIX_GLYPH_FIRST);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", DrawPlaceNumber);
#endif

void DrawPlaceIcon(Gfx** displayListHead, s32 x, s32 y, s32 place) {
  Gfx* gfx = *displayListHead;

  LoadLargePalette(&gfx, f_ResultsPlacesImage + IMAGE_HEADER_SIZE);
  LoadTextureBlock(&gfx,
      f_ResultsPlacesImage +
          ((place * PLACE_ICON_SIZE * PLACE_ICON_SIZE) +
              LARGE_PALETTED_TEXELS_OFFSET),
      G_IM_FMT_CI, G_IM_SIZ_8b, PLACE_ICON_SIZE, PLACE_ICON_SIZE, 0, 0, 0, 0, 0,
      0, 0);
  DrawTextureRect(&gfx, x * 4, y * 4, (x + PLACE_ICON_SIZE) * 4,
      (y + PLACE_ICON_SIZE) * 4, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
      TEXEL_STEP_ONE);
  *displayListHead = gfx;
}

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
#ifdef NON_MATCHING
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

    LoadLargePalette(&gfx, f_ResultsPlacesImage + IMAGE_HEADER_SIZE);
    LoadTextureBlock(&gfx,
        f_ResultsPlacesImage +
            ((suffix * PLACE_ICON_SIZE * PLACE_ICON_SIZE) +
                LARGE_PALETTED_TEXELS_OFFSET +
                (PLACE_SUFFIX_ICON_FIRST * PLACE_ICON_SIZE * PLACE_ICON_SIZE)),
        G_IM_FMT_CI, G_IM_SIZ_8b, PLACE_ICON_SIZE, PLACE_ICON_SIZE, 0, 0, 0, 0,
        0, 0, 0);
    DrawTextureRect(&gfx, (x + PLACE_DIGIT_WIDTH) * 4, y * 4,
        (x + PLACE_DIGIT_WIDTH + PLACE_ICON_SIZE) * 4,
        (y + PLACE_ICON_SIZE) * 4, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
        TEXEL_STEP_ONE);
    if (rank >= RANK_FOURTH) {
      sprintf(g_PrintBuffer, f_PlaceNumberFormat, rank + 1);
      LoadSmallPalette(&gfx, 0, D_80225708_ovl_00 + IMAGE_HEADER_SIZE);
      if (rank >= RANK_TENTH) {
        x -= PLACE_TWO_DIGIT_SHIFT;
      }

      for (i = 0; g_PrintBuffer[i] != '\0'; i++) {
        digit = g_PrintBuffer[i] - '0';
        digitX = (i * PLACE_DIGIT_SPACING) + PLACE_DIGIT_X;
        gDPLoadTextureTile_4b(gfx++,
            (u32)(D_80225708_ovl_00 + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
            PLACE_DIGIT_WIDTH, 0, 0, digit * PLACE_DIGIT_WIDTH,
            PLACE_DIGIT_WIDTH - 1,
            digit * PLACE_DIGIT_WIDTH + PLACE_DIGIT_WIDTH - 1, 0,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        DrawTextureRect(&gfx, (x + digitX) * 4, (y + PLACE_DIGIT_Y) * 4,
            (x + digitX + PLACE_DIGIT_WIDTH) * 4,
            (y + PLACE_DIGIT_Y + PLACE_DIGIT_WIDTH) * 4, G_TX_RENDERTILE, 0,
            (digit * PLACE_DIGIT_WIDTH) << 5, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      }
    } else {
      DrawPlaceIcon(&gfx, x, y, rank);
    }
  } else if (rank >= RANK_BANNER) {
    if (rank == RANK_BANNER) {
      image = f_ResultsPlacesImage;
    } else {
      image = D_80225708_ovl_00;
    }

    LoadLargePalette(&gfx, image + IMAGE_HEADER_SIZE);
    for (i = 0, stripY = y, offset = LARGE_PALETTED_TEXELS_OFFSET;
        i != RANK_BANNER_STRIP_COUNT; i++) {
      LoadTextureBlock(&gfx, image + offset, G_IM_FMT_CI, G_IM_SIZ_8b,
          RANK_BANNER_WIDTH, RANK_BANNER_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      DrawTextureRect(&gfx, (x + RANK_BANNER_X_OFFSET) * 4, stripY * 4,
          (x + RANK_BANNER_X_OFFSET + RANK_BANNER_WIDTH) * 4,
          (stripY + RANK_BANNER_STRIP_HEIGHT) * 4, G_TX_RENDERTILE, 0, 0,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      stripY += RANK_BANNER_STRIP_HEIGHT;
      offset += RANK_BANNER_WIDTH * RANK_BANNER_STRIP_HEIGHT;
    }
  } else {
    LoadLargePalette(&gfx, f_ResultsPlacesImage + IMAGE_HEADER_SIZE);
    LoadTextureBlock(&gfx,
        f_ResultsPlacesImage +
            ((rank * PLACE_ICON_SIZE * PLACE_ICON_SIZE) +
                LARGE_PALETTED_TEXELS_OFFSET +
                (PLACE_SUFFIX_ICON_FIRST * PLACE_ICON_SIZE * PLACE_ICON_SIZE)),
        G_IM_FMT_CI, G_IM_SIZ_8b, PLACE_ICON_SIZE, PLACE_ICON_SIZE, 0, 0, 0, 0,
        0, 0, 0);
    DrawTextureRect(&gfx, (x + PLACE_DIGIT_WIDTH) * 4, y * 4,
        (x + PLACE_DIGIT_WIDTH + PLACE_ICON_SIZE) * 4,
        (y + PLACE_ICON_SIZE) * 4, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
        TEXEL_STEP_ONE);
    DrawPlaceIcon(&gfx, x, y, rank);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", DrawRank);
#endif

void DrawResultsLabel(Gfx** displayListHead, s32 x, s32 y, s32 label) {
  Gfx* gfx = *displayListHead;

  LoadSmallPalette(&gfx, 0, f_ResultsLabelsImage + IMAGE_HEADER_SIZE);
  gDPLoadTextureTile_4b(gfx++,
      (u32)(f_ResultsLabelsImage +
          ((label * (RESULTS_LABEL_STORED_WIDTH * RESULTS_LABEL_HEIGHT / 2)) +
              PALETTED_TEXELS_OFFSET)) &
          ~7,
      G_IM_FMT_CI, RESULTS_LABEL_STORED_WIDTH, 0, 0, 0, RESULTS_LABEL_WIDTH - 1,
      RESULTS_LABEL_HEIGHT - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  DrawTextureRect(&gfx, x * 4, y * 4, (x + RESULTS_LABEL_WIDTH) * 4,
      (y + RESULTS_LABEL_HEIGHT) * 4, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
      TEXEL_STEP_ONE);
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
      5, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  DrawTextureRect(&gfx, x * 4, y * 4, (x + PORTRAIT_SIZE) * 4,
      (y + PORTRAIT_SIZE) * 4, G_TX_RENDERTILE,
      isMirrored * (PORTRAIT_SIZE << 5), 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
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
      f_NoticeAngle = -1.57f;
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
#define NOTICE_COURSE_CENTRE_X 160
#define NOTICE_COURSE_CENTRE_Y 128
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
#define NOTICE_CHEER_SOUND 0x9D
#define NOTICE_CHEER_SOUND_CHANNEL 3
#define NOTICE_SOUND_PAN_RIGHT 255

/* The notice that a course has opened: its picture and title swing in, then a
 * shine sweeps the title */
#ifdef NON_MATCHING
void DrawCourseResultsNotice(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 size;
  s32 step;
  s32 lift;
  s32 top;
  s32 pan;
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

  LoadLargePalette(&gfx, f_ResultsCourseImage + IMAGE_HEADER_SIZE);
  for (i = 0; i != NOTICE_COURSE_STRIP_COUNT; i++) {
    size = (cosf(f_NoticeAngle) + NOTICE_COURSE_SCALE_BIAS) *
        NOTICE_COURSE_STRIP_HEIGHT;
    step = (NOTICE_COURSE_STRIP_HEIGHT << 10) / size;
    LoadTextureBlock(&gfx,
        f_ResultsCourseImage + LARGE_PALETTED_TEXELS_OFFSET +
            (i * NOTICE_COURSE_STRIP_WIDTH * NOTICE_COURSE_STRIP_HEIGHT),
        G_IM_FMT_CI, G_IM_SIZ_8b, NOTICE_COURSE_STRIP_WIDTH,
        NOTICE_COURSE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    top = (i * size) + NOTICE_COURSE_CENTRE_Y - size;
    centerX = f_NoticeSwing + NOTICE_COURSE_CENTRE_X;
    DrawTextureRect(&gfx, (centerX - size) * 4, (top + f_NoticeUnk54) * 4,
        (size + centerX) * 4, (top + size + f_NoticeUnk54) * 4, G_TX_RENDERTILE,
        0, 0, step, step);
  }

  LoadLargePalette(&gfx, f_NoticeTitleImage + IMAGE_HEADER_SIZE);
  for (i = 0; i != NOTICE_TITLE_STRIP_COUNT; i++) {
    lift = -(s32)(cosf(f_NoticeAngle) * NOTICE_TITLE_LIFT);
    lift -= (s32)NOTICE_TITLE_LIFT;
    LoadTextureBlock(&gfx,
        f_NoticeTitleImage + LARGE_PALETTED_TEXELS_OFFSET +
            (i * NOTICE_TITLE_STRIP_WIDTH * NOTICE_TITLE_STRIP_HEIGHT),
        G_IM_FMT_CI, G_IM_SIZ_8b, NOTICE_TITLE_STRIP_WIDTH,
        NOTICE_TITLE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    top = lift + NOTICE_TITLE_TOP + (i * NOTICE_TITLE_STRIP_HEIGHT);
    DrawTextureRect(&gfx, (f_NoticeSwing + NOTICE_TITLE_LEFT) * 4,
        (top + f_NoticeUnk54) * 4,
        (f_NoticeSwing + NOTICE_TITLE_LEFT + NOTICE_TITLE_STRIP_WIDTH) * 4,
        (top + (f_NoticeUnk54 + NOTICE_TITLE_STRIP_HEIGHT)) * 4,
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
  LoadLargePalette(&gfx, f_NoticeTitleImage + IMAGE_HEADER_SIZE);
  for (i = 0; i != NOTICE_TITLE_STRIP_COUNT; i++) {
    lift = -(s32)(cosf(f_NoticeAngle) * NOTICE_TITLE_LIFT);
    lift -= (s32)NOTICE_TITLE_LIFT;
    LoadTextureBlock(&gfx,
        f_NoticeTitleImage + LARGE_PALETTED_TEXELS_OFFSET +
            (i * NOTICE_TITLE_STRIP_WIDTH * NOTICE_TITLE_STRIP_HEIGHT),
        G_IM_FMT_CI, G_IM_SIZ_8b, NOTICE_TITLE_STRIP_WIDTH,
        NOTICE_TITLE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    top = lift + NOTICE_TITLE_TOP + (i * NOTICE_TITLE_STRIP_HEIGHT);
    DrawTextureRect(&gfx, (f_NoticeSwing + NOTICE_TITLE_LEFT) * 4,
        (top + f_NoticeUnk54) * 4,
        (f_NoticeSwing + NOTICE_TITLE_LEFT + NOTICE_TITLE_STRIP_WIDTH) * 4,
        (top + (f_NoticeUnk54 + NOTICE_TITLE_STRIP_HEIGHT)) * 4,
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
  LoadTextureBlock(&gfx, f_ResultsShineImage + IMAGE_HEADER_SIZE, G_IM_FMT_I,
      G_IM_SIZ_8b, NOTICE_SHINE_SIZE, NOTICE_SHINE_SIZE, 0, 0, 0, 0, 0, 0, 0);
  if (f_NoticeFrameCount == NOTICE_SHINE_START) {
    PlaySoundWithEffects(NOTICE_SHINE_SOUND, NOTICE_SHINE_SOUND_CHANNEL,
        NOTICE_SHINE_SOUND_VOLUME, 0, SOUND_FULL_SCALE);
  }

  if (f_NoticeFrameCount == 2) {
    PlaySoundWithEffects(NOTICE_SWING_SOUND, NOTICE_SWING_SOUND_CHANNEL,
        NOTICE_SWING_SOUND_VOLUME, 0, NOTICE_SOUND_PAN_RIGHT);
    PlaySound(NOTICE_CHEER_SOUND, NOTICE_CHEER_SOUND_CHANNEL, SOUND_FULL_SCALE);
  }

  /* The swing sound follows the notice across the screen */
  if (f_NoticeFrameCount >= NOTICE_PAN_START &&
      f_NoticeFrameCount < NOTICE_PAN_END) {
    pan = (f_NoticeSwing + NOTICE_COURSE_CENTRE_X) * NOTICE_PAN_RANGE /
        NOTICE_COURSE_CENTRE_X;
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
      DrawTextureRect(&gfx, (f_NoticeSwing + NOTICE_TITLE_LEFT) * 4,
          (lift + (f_NoticeUnk54 + NOTICE_TITLE_TOP)) * 4,
          (f_NoticeSwing + NOTICE_TITLE_LEFT + NOTICE_TITLE_STRIP_WIDTH) * 4,
          (lift + (f_NoticeUnk54 + NOTICE_TITLE_TOP + NOTICE_SHINE_SIZE)) * 4,
          G_TX_RENDERTILE,
          (NOTICE_SHINE_SIZE -
              ((f_NoticeFrameCount - NOTICE_SHINE_START) * NOTICE_SHINE_STEP))
              << 5,
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
    if (f_NoticeLift > -2) {
      f_NoticeLift -= NOTICE_LIFT_START_SPEED;
    } else {
      f_NoticeLift += f_NoticeLift * 2 / 3;
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
#define NOTICE_CENTRE_X 160
#define NOTICE_CENTRE_Y 128
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
#define NOTICE_CHARACTER_VOICE 20

/* The notice that a character has been beaten: their nameplate, face and title
 * pop open, then a shine sweeps the title */
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
    LoadSmallPalette(&gfx, 0, f_NameplateImage + IMAGE_HEADER_SIZE);
    LoadNibbleTextureBlock(&gfx, f_NameplateImage + PALETTED_TEXELS_OFFSET,
        G_IM_FMT_CI, NOTICE_NAMEPLATE_SIZE, NOTICE_NAMEPLATE_SIZE, 0, 0, 0, 0,
        0, 0, 0);
    step = ((NOTICE_NAMEPLATE_SIZE / 2) << 10) / size;
    centerX = f_NoticeLift + NOTICE_CENTRE_X;
    centerY = f_NoticeUnk6C + NOTICE_CENTRE_Y;
    DrawTextureRect(&gfx, (centerX - size) * 4, (centerY - size) * 4,
        (size + centerX) * 4, (size + centerY) * 4, G_TX_RENDERTILE, 0, 0, step,
        step);
  }

  size = pop * NOTICE_PORTRAIT_POP_SIZE;
  if (size > 0) {
    LoadLargePalette(&gfx, f_PortraitPalette);
    LoadTextureBlock(&gfx, f_PortraitImage, G_IM_FMT_CI, G_IM_SIZ_8b,
        NOTICE_PORTRAIT_SIZE, NOTICE_PORTRAIT_SIZE, 0, 0, 0, 0, 0, 0, 0);
    step = ((NOTICE_PORTRAIT_SIZE / 2) << 10) / size;
    centerX = f_NoticeLift + NOTICE_CENTRE_X;
    centerY = f_NoticeUnk6C + NOTICE_CENTRE_Y;
    DrawTextureRect(&gfx, (centerX - size) * 4, (centerY - size) * 4,
        (size + centerX) * 4, (size + centerY) * 4, G_TX_RENDERTILE, 0, 0, step,
        step);
  }

  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  height = pop * NOTICE_CHARACTER_TITLE_POP_HEIGHT;
  width = pop * NOTICE_CHARACTER_TITLE_POP_WIDTH;
  if (width > 0 && height > 0) {
    LoadLargePalette(&gfx, f_NoticeTitleImage + IMAGE_HEADER_SIZE);
    for (i = 0; i != NOTICE_CHARACTER_TITLE_STRIP_COUNT; i++) {
      LoadTextureBlock(&gfx,
          f_NoticeTitleImage + LARGE_PALETTED_TEXELS_OFFSET +
              (i * NOTICE_CHARACTER_TITLE_WIDTH * NOTICE_TITLE_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, NOTICE_CHARACTER_TITLE_WIDTH,
          NOTICE_TITLE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      top = NOTICE_CHARACTER_TITLE_TOP -
          (height * NOTICE_CHARACTER_TITLE_STRIP_COUNT / 2) + (i * height);
      /* This pass mirrors the slide, unlike the depth pass and the shine */
      DrawTextureRect(&gfx, (NOTICE_CENTRE_X - (f_NoticeLift + width)) * 4,
          (top + f_NoticeUnk6C) * 4,
          (width - (f_NoticeLift - NOTICE_CENTRE_X)) * 4,
          (top + height + f_NoticeUnk6C) * 4,

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
          f_NoticeTitleImage + LARGE_PALETTED_TEXELS_OFFSET +
              (i * NOTICE_CHARACTER_TITLE_WIDTH * NOTICE_TITLE_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, NOTICE_CHARACTER_TITLE_WIDTH,
          NOTICE_TITLE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      top = NOTICE_CHARACTER_TITLE_TOP -
          (height * NOTICE_CHARACTER_TITLE_STRIP_COUNT / 2) + (i * height);
      centerX = f_NoticeLift + NOTICE_CENTRE_X;
      DrawTextureRect(&gfx, (centerX - width) * 4, (top + f_NoticeUnk6C) * 4,
          (width + centerX) * 4, (top + height + f_NoticeUnk6C) * 4,
          G_TX_RENDERTILE, 0, 0,
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
    LoadTextureBlock(&gfx, f_ResultsShineImage + IMAGE_HEADER_SIZE, G_IM_FMT_I,
        G_IM_SIZ_8b, NOTICE_SHINE_SIZE, NOTICE_SHINE_SIZE, 0, 0, 0, 0, 0, 0, 0);
    if (f_NoticeTime == NOTICE_CHARACTER_VOICE_FRAME) {
      PlayCharacterSound(f_PortraitCharacterIndex, NOTICE_CHARACTER_VOICE_SLOT,
          NOTICE_CHARACTER_VOICE, SOUND_FULL_SCALE, SOUND_FULL_SCALE);
    }

    if (f_NoticeTime == NOTICE_SHINE_START) {
      PlaySoundWithEffects(NOTICE_SHINE_SOUND, NOTICE_SHINE_SOUND_CHANNEL,
          NOTICE_SHINE_SOUND_VOLUME, 0, SOUND_FULL_SCALE);
    }

    if (f_NoticeTime == 2) {
      PlaySoundWithEffects(NOTICE_SWING_SOUND, NOTICE_SWING_SOUND_CHANNEL,
          NOTICE_SWING_SOUND_VOLUME, 0, SOUND_FULL_SCALE);
    }

    if (f_NoticeTime >= NOTICE_SHINE_START) {
      if (f_NoticeTime < NOTICE_SHINE_END) {
        DrawTextureRect(&gfx, (f_NoticeLift + NOTICE_CHARACTER_SHINE_LEFT) * 4,
            (f_NoticeUnk6C + NOTICE_CHARACTER_SHINE_TOP) * 4,
            (f_NoticeLift + NOTICE_CHARACTER_SHINE_LEFT +
                NOTICE_CHARACTER_TITLE_WIDTH) *
                4,
            (f_NoticeUnk6C + NOTICE_CHARACTER_SHINE_TOP +
                NOTICE_CHARACTER_SHINE_HEIGHT) *
                4,
            G_TX_RENDERTILE,
            (NOTICE_SHINE_SIZE -
                ((f_NoticeTime - NOTICE_SHINE_START) * NOTICE_SHINE_STEP))
                << 5,
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
s32 UpdateRoundResults(void) {
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
        isDone = ShowScorePassword();
      } else {
        switch (g_GameState.gameMode) {
          case GAME_MODE_STROKE:
          case GAME_MODE_UNK6:
          case GAME_MODE_SPEED_GOLF:
          case GAME_MODE_SKINS_MATCH:
          case GAME_MODE_CLUB_SLOTS:
            isDone = UpdateCoursePointsResults();
            break;
          case GAME_MODE_MATCH_GAME:
          case GAME_MODE_GET_CHARACTER:
            isDone = UpdateMatchResults();
            break;
          case GAME_MODE_TOURNAMENT:
            isDone = UpdateTournamentResults();
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

#define MESSAGE_TROPHY_PRIZE 0x3C /* Plus the place */
#define MESSAGE_PLACE_PRIZE 0x40
#define MESSAGE_TROPHY_PLACE 0x40 /* Plus the place */
#define MESSAGE_UNK48 0x48
#define MESSAGE_PLACE 0x49
#define MESSAGE_POINTS_EARNED 0x1F3

#define BANNER_TEXT_SLOT 1
#define BANNER_TEXT_FONT 1
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
#define TROPHY_CENTRE_X 48
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

/* Draws the banner of a round's result, and the trophy above a top three
 * tournament place */
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
          MESSAGE_POINTS_EARNED, BANNER_TEXT_FONT, BANNER_TEXT_SLOT,
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
              rank + MESSAGE_TROPHY_PRIZE, BANNER_TEXT_FONT, BANNER_TEXT_SLOT,
              TEXT_FLAG_UNK20, BANNER_TEXT_RED, BANNER_TEXT_GREEN,
              BANNER_TEXT_BLUE, BANNER_TEXT_ALPHA);
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
              MESSAGE_PLACE_PRIZE, BANNER_TEXT_FONT, BANNER_TEXT_SLOT,
              TEXT_FLAG_UNK20, BANNER_TEXT_RED, BANNER_TEXT_GREEN,
              BANNER_TEXT_BLUE, BANNER_TEXT_ALPHA);
        }
      } else if (place < TROPHY_PLACE_COUNT) {
        SetTextSlot(f_BannerX + BANNER_TEXT_X, f_BannerY + BANNER_TEXT_Y,
            rank + MESSAGE_TROPHY_PLACE, BANNER_TEXT_FONT, BANNER_TEXT_SLOT,
            TEXT_FLAG_UNK20 | TEXT_FLAG_CENTERED, BANNER_TEXT_RED,
            BANNER_TEXT_GREEN, BANNER_TEXT_BLUE, BANNER_TEXT_ALPHA);
      } else {
        SetTextSlotValue(rank, BANNER_TEXT_SLOT, 0);
        SetTextSlot(f_BannerX + BANNER_TEXT_X, f_BannerY + BANNER_TEXT_Y,
            MESSAGE_PLACE, BANNER_TEXT_FONT, BANNER_TEXT_SLOT,
            TEXT_FLAG_UNK20 | TEXT_FLAG_CENTERED, BANNER_TEXT_RED,
            BANNER_TEXT_GREEN, BANNER_TEXT_BLUE, BANNER_TEXT_ALPHA);
      }

      break;
    case BANNER_KIND_UNK5:
      SetTextSlot(f_BannerX + BANNER_TEXT_X, f_BannerY + BANNER_TEXT_Y,
          MESSAGE_UNK48, BANNER_TEXT_FONT, BANNER_TEXT_SLOT,
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
        f_ResultsCloudImage + IMAGE_HEADER_SIZE + (i * BANNER_CLOUD_STRIP_SIZE),
        G_IM_FMT_I, BANNER_CLOUD_WIDTH, BANNER_CLOUD_HEIGHT, 0, G_TX_CLAMP,
        G_TX_CLAMP, 0, 0, 0, 0);
    if (f_BannerKind == BANNER_KIND_POINTS) {
      DrawTextureRect(&gfx, (f_BannerX + BANNER_CLOUD_LEFT) * 4,
          (f_BannerY + TALL_BANNER_CLOUD_TOP + (i * TALL_BANNER_CLOUD_HEIGHT)) *
              4,
          (f_BannerX + BANNER_CLOUD_RIGHT) * 4,
          (f_BannerY + TALL_BANNER_CLOUD_TOP + (i * TALL_BANNER_CLOUD_HEIGHT) +
              TALL_BANNER_CLOUD_HEIGHT) *
              4,
          G_TX_RENDERTILE, 0, 0, BANNER_CLOUD_DSDX, TALL_BANNER_CLOUD_DTDY);
    } else {
      DrawTextureRect(&gfx, (f_BannerX + BANNER_CLOUD_LEFT) * 4,
          (f_BannerY + BANNER_CLOUD_TOP + (i * BANNER_CLOUD_HEIGHT)) * 4,
          (f_BannerX + BANNER_CLOUD_RIGHT) * 4,
          (f_BannerY + BANNER_CLOUD_TOP + (i * BANNER_CLOUD_HEIGHT) +
              BANNER_CLOUD_HEIGHT) *
              4,
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
            ((u32)f_TrophyImage +
                (i * f_TrophySizes[course][TROPHY_WIDTH] *
                    f_TrophySizes[course][TROPHY_PART_HEIGHTS])) &
                ~7,
            G_IM_FMT_CI, G_IM_SIZ_8b, f_TrophySizes[course][TROPHY_WIDTH],
            f_TrophySizes[course][TROPHY_PART_HEIGHTS + i], 0, 0,
            f_TrophySizes[course][TROPHY_WIDTH] - 1,
            f_TrophySizes[course][TROPHY_PART_HEIGHTS + i] - 1, 0, 0, 0, 0, 0,
            0, 0);
        DrawTextureRect(&gfx,
            (TROPHY_CENTRE_X + f_BannerX -
                (f_TrophySizes[course][TROPHY_WIDTH] / 2)) *
                4,
            (top + TROPHY_BOTTOM) * 4,
            (TROPHY_CENTRE_X + f_BannerX +
                (f_TrophySizes[course][TROPHY_WIDTH] / 2)) *
                4,
            (f_TrophySizes[course][TROPHY_PART_HEIGHTS + i] + TROPHY_BOTTOM +
                top) *
                4,
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
            ((u32)f_TrophyImage +
                (i * f_TrophySizes[course][TROPHY_WIDTH] *
                    f_TrophySizes[course][TROPHY_PART_HEIGHTS])) &
                ~7,
            G_IM_FMT_CI, G_IM_SIZ_8b, f_TrophySizes[course][TROPHY_WIDTH],
            f_TrophySizes[course][TROPHY_PART_HEIGHTS + i], 0, 0,
            f_TrophySizes[course][TROPHY_WIDTH] - 1,
            f_TrophySizes[course][TROPHY_PART_HEIGHTS + i] - 1, 0, 0, 0, 0, 0,
            0, 0);
        DrawTextureRect(&gfx,
            (TROPHY_CENTRE_X + f_BannerX -
                (f_TrophySizes[course][TROPHY_WIDTH] / 2)) *
                4,
            (top + TROPHY_BOTTOM) * 4,
            (TROPHY_CENTRE_X + f_BannerX +
                (f_TrophySizes[course][TROPHY_WIDTH] / 2)) *
                4,
            (f_TrophySizes[course][TROPHY_PART_HEIGHTS + i] + TROPHY_BOTTOM +
                top) *
                4,
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
      LoadTextureBlock(&gfx, f_ResultsShineImage + IMAGE_HEADER_SIZE,
          G_IM_FMT_I, G_IM_SIZ_8b, NOTICE_SHINE_SIZE, NOTICE_SHINE_SIZE, 0, 0,
          0, 0, 0, 0, 0);
      if (f_TrophyShineFrame == TROPHY_SHINE_SOUND_FRAME) {
        PlaySoundWithEffects(NOTICE_SHINE_SOUND, TROPHY_SHINE_SOUND_CHANNEL,
            TROPHY_SHINE_SOUND_VOLUME, TROPHY_SHINE_SOUND_REVERB,
            TROPHY_SHINE_SOUND_PAN);
      }

      if (f_TrophyShineFrame >= TROPHY_SHINE_START) {
        if (f_TrophyShineFrame < TROPHY_SHINE_CYCLE) {
          top = f_TrophySizes[course][TROPHY_PART_HEIGHTS] +
              f_TrophySizes[course][TROPHY_PART_HEIGHTS + 1];
          DrawTextureRect(&gfx,
              (TROPHY_CENTRE_X + f_BannerX -
                  (f_TrophySizes[course][TROPHY_WIDTH] / 2)) *
                  4,
              (TROPHY_BOTTOM - top) * 4,
              (TROPHY_CENTRE_X + f_BannerX +
                  (f_TrophySizes[course][TROPHY_WIDTH] / 2)) *
                  4,
              (TROPHY_BOTTOM - top +
                  f_TrophySizes[course][TROPHY_SHINE_HEIGHT]) *
                  4,
              G_TX_RENDERTILE,
              (NOTICE_SHINE_SIZE -
                  ((f_TrophyShineFrame - TROPHY_SHINE_START) *
                      TROPHY_SHINE_SPEED))
                  << 5,
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
#define TOURNAMENT_STEP_START 0
#define TOURNAMENT_STEP_SLIDING_IN_PRIZE 1
#define TOURNAMENT_STEP_TALLYING_PRIZE 2
#define TOURNAMENT_STEP_PRIZE_TALLIED 3
#define TOURNAMENT_STEP_SLIDE_OUT_PRIZE 4
#define TOURNAMENT_STEP_SLIDING_OUT_PRIZE 5
#define TOURNAMENT_STEP_SLIDING_IN_POINTS 6
#define TOURNAMENT_STEP_TALLYING_POINTS 7
#define TOURNAMENT_STEP_POINTS_TALLIED 8
#define TOURNAMENT_STEP_SLIDING_OUT_POINTS 9
#define TOURNAMENT_STEP_WAITING 10
#define TOURNAMENT_STEP_SHOWN 11
#define NEW_BEST_SCORE_SOUND_CHANNEL 3
#define CHARACTER_SOUND_WIN 0x1C
#define WIN_SOUND_PAN 32

/*
 * Counts the tournament prize, then the earned course points, into the save,
 * showing a notice when they open a course, then returns TRUE once A is pressed
 */
s32 UpdateTournamentResults(ControllerState* controller) {
  EncodedAsset asset;
  UnkE98* record;
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
        OpenEncodedAsset(NOTICE_TITLE_ASSET, &asset);
        ReadEncodedAsset(f_NoticeTitleImage, &asset);
        return FALSE;
      }

      switch (f_ResultsUnk78) {
        case TOURNAMENT_STEP_START:
          record = &GetSaveData()->unkE98[g_GameState.courseIndex];
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

/* UpdatePrompt's results for PROMPT_SCORE_PASSWORD */
enum ScorePasswordAnswer {
  SCORE_PASSWORD_PROMPT_YES = 10,
  SCORE_PASSWORD_PROMPT_NO
};

/* Shows the round's score password until the player says they have it. Returns
 * TRUE then. */
s32 ShowScorePassword(ControllerState* controller) {
  ScoreReport report;
  s32 unused[2];

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
        case SCORE_PASSWORD_PROMPT_YES:
          f_ResultsUnk78 = SCORE_PASSWORD_STEP_DONE;
          break;
        case SCORE_PASSWORD_PROMPT_NO:
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
        OpenEncodedAsset(NOTICE_TITLE_ASSET, &asset);
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
              if (i != j && f_PlayerRanks[j] < f_PlayerRanks[i]) {
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
#define MATCH_STEP_SHOWN 0
#define MATCH_STEP_SLIDING_IN_TALLY 1
#define MATCH_STEP_TALLYING 2
#define MATCH_STEP_TALLIED 3
#define MATCH_STEP_SLIDING_OUT_TALLY 4
#define MATCH_STEP_RECORD_RESULT 5
#define MATCH_STEP_WAITING 6
#define MATCH_STEP_SHOW_CHARACTER_NOTICE 7
#define MATCH_STEP_START_NOTICE_WAIT 8
#define MATCH_STEP_NOTICE_WAIT 9
#define MATCH_STEP_START_TALLY 10
#define CHARACTER_NOTICE_TITLE_ASSET 0x560
#define CHARACTER_NOTICE_WAIT_FRAMES 8

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
        OpenEncodedAsset(NOTICE_TITLE_ASSET, &asset);
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
            /* Beating a still locked opponent unlocks them, except those that
             * other feats unlock */
            if (f_PlayerRanks[0] == MATCH_RANK_WON &&
                func_8005CEE0((s8)g_Players[1].characterIndex) == -1 &&
                (s8)g_Players[1].characterIndex != CHARACTER_PLUM &&
                (s8)g_Players[1].characterIndex != CHARACTER_CHARLIE &&
                (s8)g_Players[1].characterIndex != CHARACTER_PEACH &&
                (s8)g_Players[1].characterIndex != CHARACTER_BABY_MARIO &&
                (s8)g_Players[1].characterIndex != CHARACTER_YOSHI &&
                (s8)g_Players[1].characterIndex != CHARACTER_DONKEY_KONG &&
                (s8)g_Players[1].characterIndex < CHARACTER_METAL_MARIO) {
              f_ResultsUnk78 = MATCH_STEP_SHOW_CHARACTER_NOTICE;
            }

            RecordComCharacterResult((s8)g_Players[0].characterIndex,
                (s8)g_Players[1].characterIndex, GetMatchWinner(0, 1) == 1);
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

            sprintf(g_PrintBuffer, "CHARA:%2d", f_DebugUnlockCharacter);
            PrintDebugText(g_PrintBuffer, 25, 5);
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

/* The tournament results: place, portrait, score, Course Points and the points
 * to the next course */
void DrawTournamentResults(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  DrawRank(&gfx, f_ResultsSlide + 40, f_ResultsTop + 80,
      (f_PlayerRanks[0] - 1) | RANK_AS_PLACE);
  DrawPlayerPortrait(&gfx, f_ResultsSlide + 100, f_ResultsTop + 80, 0, FALSE);
  DrawResultsLabel(
      &gfx, f_ResultsSlide + 144, f_ResultsTop + 60, RESULTS_LABEL_SCORE);
  DrawResultsLabel(
      &gfx, f_ResultsSlide + 232, f_ResultsTop + 60, RESULTS_LABEL_POINTS);
  if (f_OpenCourseCount == COURSE_COUNT) {
    SetTextSlot(f_ResultsSlide + 32, f_ResultsTop + 192,
        MESSAGE_ALL_COURSES_OPEN, RESULTS_TEXT_FONT, RESULTS_TEXT_SLOT, 0,
        RESULTS_TEXT_RED, RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE,
        RESULTS_TEXT_ALPHA);
  } else {
    DrawResultsNumber(&gfx, f_ResultsSlide + 29, POINTS_TOTAL_Y,
        f_PointsToNextCourse + f_PrizePoints + f_PointsEarned);
    SetTextSlot(f_ResultsSlide + 64, f_ResultsTop + 200,
        MESSAGE_POINTS_TO_NEXT_COURSE, RESULTS_TEXT_FONT, RESULTS_TEXT_SLOT, 0,
        RESULTS_TEXT_RED, RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE,
        RESULTS_TEXT_ALPHA);
  }

  LoadSmallPalette(&gfx, 0, f_ResultsLargeDigitsImage + IMAGE_HEADER_SIZE);
  LoadNibbleTextureBlock(&gfx,
      f_ResultsLargeDigitsImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI,
      LARGE_DIGIT_WIDTH, LARGE_DIGITS_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  if (f_PlayerResults[0] == 0) {
    sprintf(g_PrintBuffer, f_EvenScoreFormat, 0);
  } else {
    sprintf(g_PrintBuffer, f_ScoreFormat, f_PlayerResults[0]);
  }

  DrawDigitText(&gfx, f_ResultsSlide + 152, f_ResultsTop + 88, g_PrintBuffer);
  sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerPoints[0]);
  DrawDigitText(&gfx, f_ResultsSlide + 232, f_ResultsTop + 88, g_PrintBuffer);
  *displayListHead = gfx;
}

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_EvenScoreFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_ScoreFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_PointsFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_TimeFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_TimeLabel);

#define RESULTS_HEADING_ENV_COLOR 0xFFFF0000
#define RESULTS_HEADING_PRIM_COLOR 0x30FF0000

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
  DrawPlayerPortrait(&gfx, f_ResultsSlide + 40, f_ResultsTop + 88, 0, FALSE);
  DrawResultsLabel(
      &gfx, f_ResultsSlide + 82, f_ResultsTop + 56, RESULTS_LABEL_SCORE);
  DrawResultsLabel(
      &gfx, f_ResultsSlide + 232, f_ResultsTop + 56, RESULTS_LABEL_POINTS);
  if (f_OpenCourseCount == COURSE_COUNT) {
    SetTextSlot(f_ResultsSlide + 32, f_ResultsTop + 192,
        MESSAGE_ALL_COURSES_OPEN, RESULTS_TEXT_FONT, RESULTS_TEXT_SLOT, 0,
        RESULTS_TEXT_RED, RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE,
        RESULTS_TEXT_ALPHA);
  } else {
    DrawResultsNumber(&gfx, f_ResultsSlide + 29, POINTS_TOTAL_Y,
        f_PointsToNextCourse + f_PrizePoints + f_PointsEarned);
    SetTextSlot(f_ResultsSlide + 64, f_ResultsTop + 200,
        MESSAGE_POINTS_TO_NEXT_COURSE, RESULTS_TEXT_FONT, RESULTS_TEXT_SLOT, 0,
        RESULTS_TEXT_RED, RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE,
        RESULTS_TEXT_ALPHA);
  }

  LoadSmallPalette(&gfx, 0, f_ResultsLargeDigitsImage + IMAGE_HEADER_SIZE);
  LoadNibbleTextureBlock(&gfx,
      f_ResultsLargeDigitsImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI,
      LARGE_DIGIT_WIDTH, LARGE_DIGITS_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  if (f_PlayerResults[0] == 0) {
    sprintf(g_PrintBuffer, f_EvenScoreFormat, 0);
  } else {
    sprintf(g_PrintBuffer, f_ScoreFormat, f_PlayerResults[0]);
  }

  DrawDigitText(&gfx, f_ResultsSlide + 90, f_ResultsTop + 132, g_PrintBuffer);
  seconds = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND;
  hundredths = (s32)(g_GameState.stopwatchFrameCount *
                   ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
      HUNDREDTHS_PER_SECOND;
  sprintf(g_PrintBuffer, f_TimeFormat,
      seconds / SECONDS_PER_MINUTE % SHOWN_MINUTES_LIMIT,
      seconds % SECONDS_PER_MINUTE, hundredths);
  DrawDigitText(&gfx, f_ResultsSlide + 90, f_ResultsTop + 94, g_PrintBuffer);
  sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerPoints[0]);
  DrawDigitText(&gfx, f_ResultsSlide + 228, f_ResultsTop + 88, g_PrintBuffer);
  SetUpGradientTextDrawing(&gfx, 0);
  sprintf(g_PrintBuffer, f_TimeLabel);
  DrawLargeTextWithShadow(&gfx, f_ResultsSlide + 86, f_ResultsTop + 76,
      g_PrintBuffer, RESULTS_HEADING_ENV_COLOR, RESULTS_HEADING_PRIM_COLOR, 0);
  sprintf(g_PrintBuffer, "Stroke");
  DrawLargeTextWithShadow(&gfx, f_ResultsSlide + 86, f_ResultsTop + 114,
      g_PrintBuffer, RESULTS_HEADING_ENV_COLOR, RESULTS_HEADING_PRIM_COLOR, 0);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

#define RESULTS_ROW_HEIGHT 32
#define RESULTS_ICON_ROW_Y 68
#define RESULTS_TEXT_ROW_Y 80

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
    DrawResultsLabel(&gfx, f_ResultsSlide + 88,
        f_ResultsTop + RESULTS_HEADING_Y, RESULTS_LABEL_SCORE);
    DrawResultsLabel(&gfx, f_ResultsSlide + 192,
        f_ResultsTop + RESULTS_HEADING_Y, RESULTS_LABEL_BETS);
    DrawResultsLabel(&gfx, f_ResultsSlide + 248,
        f_ResultsTop + RESULTS_HEADING_Y, RESULTS_LABEL_POINTS);
    for (i = 0; i != g_GameState.playerCount; i++) {
      if (g_GameState.playerCount >= 2) {
        DrawPlaceIcon(&gfx, f_ResultsSlide + 18,
            f_ResultsTop + RESULTS_ICON_ROW_Y + (i * RESULTS_ROW_HEIGHT),
            f_PlayerRanks[i]);
      }

      DrawPlayerPortrait(&gfx, f_ResultsSlide + 50,
          f_ResultsTop + RESULTS_ICON_ROW_Y + (i * RESULTS_ROW_HEIGHT), i,
          FALSE);
    }
  } else {
    DrawResultsLabel(&gfx, f_ResultsSlide + 144,
        f_ResultsTop + RESULTS_HEADING_Y, RESULTS_LABEL_SCORE);
    DrawResultsLabel(&gfx, f_ResultsSlide + 234,
        f_ResultsTop + RESULTS_HEADING_Y, RESULTS_LABEL_POINTS);
    for (i = 0; i != g_GameState.playerCount; i++) {
      if (g_GameState.playerCount >= 2) {
        DrawRank(&gfx, f_ResultsSlide + 32,
            f_ResultsTop + RESULTS_ICON_ROW_Y + (i * RESULTS_ROW_HEIGHT),
            f_PlayerRanks[i]);
      }

      DrawPlayerPortrait(&gfx, f_ResultsSlide + 96,
          f_ResultsTop + RESULTS_ICON_ROW_Y + (i * RESULTS_ROW_HEIGHT), i,
          FALSE);
    }
  }

  LoadSmallPalette(&gfx, 0, f_ResultsLargeDigitsImage + IMAGE_HEADER_SIZE);
  LoadNibbleTextureBlock(&gfx,
      f_ResultsLargeDigitsImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI,
      LARGE_DIGIT_WIDTH, LARGE_DIGITS_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
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

      DrawDigitText(&gfx, f_ResultsSlide + 96,
          f_ResultsTop + RESULTS_TEXT_ROW_Y + (i * RESULTS_ROW_HEIGHT),
          g_PrintBuffer);
      sprintf(g_PrintBuffer, "%5d", f_PlayerBetTotals[i]);
      DrawDigitText(&gfx, f_ResultsSlide + 156 + betShift,
          f_ResultsTop + RESULTS_TEXT_ROW_Y + (i * RESULTS_ROW_HEIGHT),
          g_PrintBuffer);
      if (f_PlayerPoints[i] < 0) {
        sprintf(g_PrintBuffer, f_NoPointsText, f_PlayerPoints[i]);
      } else {
        sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerPoints[i]);
      }

      DrawDigitText(&gfx, f_ResultsSlide + 242,
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

      DrawDigitText(&gfx, f_ResultsSlide + 152,
          f_ResultsTop + RESULTS_TEXT_ROW_Y + (i * RESULTS_ROW_HEIGHT),
          g_PrintBuffer);
      if (f_PlayerPoints[i] < 0) {
        sprintf(g_PrintBuffer, f_NoPointsText, f_PlayerPoints[i]);
      } else {
        sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerPoints[i]);
      }

      DrawDigitText(&gfx, f_ResultsSlide + 232,
          f_ResultsTop + RESULTS_TEXT_ROW_Y + (i * RESULTS_ROW_HEIGHT),
          g_PrintBuffer);
    }
  }

  if (f_OpenCourseCount == COURSE_COUNT) {
    SetTextSlot(f_ResultsSlide + 32, f_ResultsTop + 200,
        MESSAGE_ALL_COURSES_OPEN, RESULTS_TEXT_FONT, RESULTS_TEXT_SLOT, 0,
        RESULTS_TEXT_RED, RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE,
        RESULTS_TEXT_ALPHA);
  } else {
    DrawResultsNumber(&gfx, f_ResultsSlide + 29, POINTS_TOTAL_Y,
        f_PointsToNextCourse + f_PointsEarned);
    SetTextSlot(f_ResultsSlide + 64, f_ResultsTop + 200,
        MESSAGE_POINTS_TO_NEXT_COURSE, RESULTS_TEXT_FONT, RESULTS_TEXT_SLOT, 0,
        RESULTS_TEXT_RED, RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE,
        RESULTS_TEXT_ALPHA);
  }

  *displayListHead = gfx;
}

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", f_NoPointsText);

#define MATCH_PLAYER_COUNT 2
#define MATCH_ROW_HEIGHT 72
/* The standing words are 48x48, holding UP, DOWN and EVEN in rows of 16 */
#define STANDING_WORDS_SIZE 48
#define STANDING_WORD_WIDTH 48
#define STANDING_WORD_HEIGHT 16
#define STANDING_WORD_UP_ROW 0
#define STANDING_WORD_DOWN_ROW 16
#define STANDING_WORD_EVEN_ROW 32
/* Shifts the hole count and its word left, so that the pair stays centered */
#define STANDING_ONE_DIGIT_UP_SHIFT -8
#define STANDING_ONE_DIGIT_DOWN_SHIFT -20
#define STANDING_TWO_DIGIT_DOWN_SHIFT -12

/* The match play results of two players: place, portrait, holes up or down, any
 * bets and Course Points */
#ifdef NON_MATCHING
void DrawMatchResults(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 unused[4];
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
    DrawResultsLabel(&gfx, f_ResultsSlide + 152,
        f_ResultsTop + RESULTS_HEADING_Y + 8, RESULTS_LABEL_BETS);
    DrawResultsLabel(&gfx, f_ResultsSlide + 240,
        f_ResultsTop + RESULTS_HEADING_Y + 8, RESULTS_LABEL_POINTS);
    DrawRank(&gfx, f_ResultsSlide + 40, f_ResultsTop + 48, f_PlayerRanks[0]);
    DrawRank(&gfx, f_ResultsSlide + 40, f_ResultsTop + 48 + MATCH_ROW_HEIGHT,
        f_PlayerRanks[1]);
    DrawPlayerPortrait(&gfx, f_ResultsSlide + 62, f_ResultsTop + 76, 0, FALSE);
    DrawPlayerPortrait(&gfx, f_ResultsSlide + 62,
        f_ResultsTop + 76 + MATCH_ROW_HEIGHT, 1, FALSE);
  } else {
    DrawResultsLabel(&gfx, f_ResultsSlide + 220,
        f_ResultsTop + RESULTS_HEADING_Y + 8, RESULTS_LABEL_POINTS);
    DrawRank(&gfx, f_ResultsSlide + 56, f_ResultsTop + 48, f_PlayerRanks[0]);
    DrawRank(&gfx, f_ResultsSlide + 56, f_ResultsTop + 48 + MATCH_ROW_HEIGHT,
        f_PlayerRanks[1]);
    DrawPlayerPortrait(&gfx, f_ResultsSlide + 78, f_ResultsTop + 76, 0, FALSE);
    DrawPlayerPortrait(&gfx, f_ResultsSlide + 78,
        f_ResultsTop + 76 + MATCH_ROW_HEIGHT, 1, FALSE);
  }

  LoadSmallPalette(&gfx, 0, f_MatchStandingWordsImage + IMAGE_HEADER_SIZE);
  LoadNibbleTextureBlock(&gfx,
      f_MatchStandingWordsImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI,
      STANDING_WORDS_SIZE, STANDING_WORDS_SIZE, 0, 0, 0, 0, 0, 0, 0);
  for (i = 0; i != g_GameState.playerCount; i++) {
    if (f_PlayerResults[i] > 0) {
      if (f_PlayerResults[i] < 10) {
        standingShift[i] = STANDING_ONE_DIGIT_UP_SHIFT;
      } else {
        standingShift[i] = 0;
      }
    } else if (f_PlayerResults[i] < 0) {
      if (f_PlayerResults[i] < -9) {
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
      DrawTextureRect(&gfx, (standingShift[i] + 80 + f_ResultsSlide) << 2,
          (f_ResultsTop + 108 + (i * MATCH_ROW_HEIGHT)) << 2,
          (standingShift[i] + 80 + STANDING_WORD_WIDTH + f_ResultsSlide) << 2,
          (f_ResultsTop + 108 + (i * MATCH_ROW_HEIGHT) + STANDING_WORD_HEIGHT)
              << 2,
          G_TX_RENDERTILE, 0, standingRow << 5, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    } else {
      DrawTextureRect(&gfx, (standingShift[i] + 96 + f_ResultsSlide) << 2,
          (f_ResultsTop + 108 + (i * MATCH_ROW_HEIGHT)) << 2,
          (standingShift[i] + 96 + STANDING_WORD_WIDTH + f_ResultsSlide) << 2,
          (f_ResultsTop + 108 + (i * MATCH_ROW_HEIGHT) + STANDING_WORD_HEIGHT)
              << 2,
          G_TX_RENDERTILE, 0, standingRow << 5, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  LoadSmallPalette(&gfx, 0, f_ResultsLargeDigitsImage + IMAGE_HEADER_SIZE);
  LoadNibbleTextureBlock(&gfx,
      f_ResultsLargeDigitsImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI,
      LARGE_DIGIT_WIDTH, LARGE_DIGITS_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  for (i = 0; i != g_GameState.playerCount; i++) {
    if (f_HasBets) {
      /* An even match draws only the word */
      if (f_PlayerResults[0] != 0) {
        holeCount = f_PlayerResults[i];
        if (holeCount < 0) {
          holeCount = -holeCount;
        }

        sprintf(g_PrintBuffer, "%2d", holeCount);
        DrawDigitText(&gfx, standingShift[i] + f_ResultsSlide + 48,
            f_ResultsTop + 108 + (i * MATCH_ROW_HEIGHT), g_PrintBuffer);
      }

      sprintf(g_PrintBuffer, f_NumberFormat, f_PlayerBetTotals[i]);
      DrawDigitText(&gfx, betShift + 128 + f_ResultsSlide,
          f_ResultsTop + 92 + (i * MATCH_ROW_HEIGHT), g_PrintBuffer);
      if (f_PlayerPoints[i] < 0) {
        sprintf(g_PrintBuffer, f_NoPointsText, f_PlayerPoints[i]);
      } else {
        sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerPoints[i]);
      }

      DrawDigitText(&gfx, f_ResultsSlide + 234,
          f_ResultsTop + 92 + (i * MATCH_ROW_HEIGHT), g_PrintBuffer);
    } else {
      if (f_PlayerResults[0] != 0) {
        holeCount = f_PlayerResults[i];
        if (holeCount < 0) {
          holeCount = -holeCount;
        }

        sprintf(g_PrintBuffer, "%2d", holeCount);
        DrawDigitText(&gfx, standingShift[i] + f_ResultsSlide + 64,
            f_ResultsTop + 108 + (i * MATCH_ROW_HEIGHT), g_PrintBuffer);
      }

      if (f_PlayerPoints[i] < 0) {
        sprintf(g_PrintBuffer, f_NoPointsText, f_PlayerPoints[i]);
      } else {
        sprintf(g_PrintBuffer, f_PointsFormat, f_PlayerPoints[i]);
      }

      DrawDigitText(&gfx, f_ResultsSlide + 218, f_ResultsTop + 92 + (i * 64),
          g_PrintBuffer);
    }
  }

  if (f_OpenCourseCount == COURSE_COUNT) {
    SetTextSlot(f_ResultsSlide + 32, f_ResultsTop + 204,
        MESSAGE_ALL_COURSES_OPEN, RESULTS_TEXT_FONT, RESULTS_TEXT_SLOT, 0,
        RESULTS_TEXT_RED, RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE,
        RESULTS_TEXT_ALPHA);
  } else {
    DrawResultsNumber(&gfx, f_ResultsSlide + 29, POINTS_TOTAL_Y,
        f_PointsToNextCourse + f_PointsEarned);
    SetTextSlot(f_ResultsSlide + 64, f_ResultsTop + 200,
        MESSAGE_POINTS_TO_NEXT_COURSE, RESULTS_TEXT_FONT, RESULTS_TEXT_SLOT, 0,
        RESULTS_TEXT_RED, RESULTS_TEXT_GREEN, RESULTS_TEXT_BLUE,
        RESULTS_TEXT_ALPHA);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", DrawMatchResults);
#endif

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", D_80224E10_ovl_00);

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
/* DrawLargeTextWithShadow ignores this flag */
#define CLUB_TOURNAMENT_LABEL_FLAGS 4
#define CLUB_TOURNAMENT_LABEL_ENV_COLOR 0xFF5B0000
#define CLUB_TOURNAMENT_LABEL_PRIM_COLOR 0xFFFF0000
#define SCORE_PASSWORD_ENV_COLOR 0x00FF0000
#define SCORE_PASSWORD_PRIM_COLOR 0xFFFF0000

/* The results of a club tournament: its course, number, name and mode, the
 * score and the password to send in */
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

  isNintendo = IsNintendoClubTournament();
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gameMode = GetClubTournamentGameMode();
  LoadLargePalette(&gfx, f_ResultsCourseImage + IMAGE_HEADER_SIZE);
  for (i = 0; i < CLUB_TOURNAMENT_COURSE_STRIP_COUNT; i++) {
    LoadTextureBlock(&gfx,
        f_ResultsCourseImage + LARGE_PALETTED_TEXELS_OFFSET +
            (i * CLUB_TOURNAMENT_COURSE_STRIP_WIDTH *
                CLUB_TOURNAMENT_COURSE_STRIP_HEIGHT),
        G_IM_FMT_CI, G_IM_SIZ_8b, CLUB_TOURNAMENT_COURSE_STRIP_WIDTH,
        CLUB_TOURNAMENT_COURSE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    DrawTextureRect(&gfx, (f_ResultsSlide + 28) * 4,
        ((i * CLUB_TOURNAMENT_COURSE_STRIP_HEIGHT) + (f_ResultsTop + 104)) * 4,
        (f_ResultsSlide + 28 + CLUB_TOURNAMENT_COURSE_STRIP_WIDTH) * 4,
        ((i * CLUB_TOURNAMENT_COURSE_STRIP_HEIGHT) +
            (f_ResultsTop + 104 + CLUB_TOURNAMENT_COURSE_STRIP_HEIGHT)) *
            4,
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  detailsTop = CLUB_TOURNAMENT_DETAILS_TOP;
  if (isNintendo) {
    lineHeight = CLUB_TOURNAMENT_LINE_HEIGHT;
    DrawPlayerPortrait(&gfx, f_ResultsSlide + 96, f_ResultsTop + 136, 0, TRUE);
    LoadLargePalette(&gfx, f_NintendoBadgeImage + IMAGE_HEADER_SIZE);
    for (i = 0; i != NINTENDO_BADGE_STRIP_COUNT; i++) {
      LoadTextureBlock(&gfx,
          f_NintendoBadgeImage + LARGE_PALETTED_TEXELS_OFFSET +
              (i * NINTENDO_BADGE_STRIP_WIDTH * NINTENDO_BADGE_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, NINTENDO_BADGE_STRIP_WIDTH,
          NINTENDO_BADGE_STRIP_HEIGHT, 0, G_TX_MIRROR, 0, 0, 0, 0, 0);
      DrawTextureRect(&gfx, (f_ResultsSlide + 88) * 4,
          (f_ResultsTop + 54 + (i * NINTENDO_BADGE_STRIP_HEIGHT)) * 4,
          (f_ResultsSlide + 88 + NINTENDO_BADGE_STRIP_WIDTH) * 4,
          (f_ResultsTop + 54 + (i * NINTENDO_BADGE_STRIP_HEIGHT) +
              NINTENDO_BADGE_STRIP_HEIGHT) *
              4,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  } else {
    detailsTop = CLUB_TOURNAMENT_UNBADGED_DETAILS_TOP;
    lineHeight = CLUB_TOURNAMENT_UNBADGED_LINE_HEIGHT;
    DrawPlayerPortrait(&gfx, f_ResultsSlide + 96, f_ResultsTop + 128, 0, TRUE);
  }

  DrawPlaceNumber(&gfx, f_ResultsSlide + 160, f_ResultsTop + detailsTop,
      GetClubTournamentNumber());
  SetUpTextDrawing(&gfx);
  MeasureMessage(
      GetClubTournamentNameMessage(), FONT_SIZE_LARGE, &width, &height, 0);
  MeasureMessage(GetClubTournamentCourseTitleMessage(), FONT_SIZE_LARGE, &width,
      &height, 0);
  MeasureMessage(
      GetClubTournamentModeMessage(), FONT_SIZE_LARGE, &width, &height, 0);
  SetTextSlot(f_ResultsSlide + 160, f_ResultsTop + detailsTop + lineHeight + 8,
      GetClubTournamentNameMessage(), RESULTS_TEXT_FONT,
      CLUB_TOURNAMENT_NAME_SLOT, CLUB_TOURNAMENT_TEXT_FLAGS, 127, 191, 255,
      255);
  SetTextSlot(f_ResultsSlide + 160,
      f_ResultsTop + detailsTop + (lineHeight * 2) + 8,
      GetClubTournamentCourseTitleMessage(), RESULTS_TEXT_FONT,
      CLUB_TOURNAMENT_COURSE_SLOT, CLUB_TOURNAMENT_TEXT_FLAGS, 43, 255, 75,
      255);
  SetTextSlot(f_ResultsSlide + 160,
      f_ResultsTop + detailsTop + (lineHeight * 3) + 8,
      GetClubTournamentModeMessage(), RESULTS_TEXT_FONT,
      CLUB_TOURNAMENT_MODE_SLOT, CLUB_TOURNAMENT_TEXT_FLAGS, 255, 63, 127, 255);
  for (i = CLUB_TOURNAMENT_NAME_SLOT; i != CLUB_TOURNAMENT_MODE_SLOT + 1; i++) {
    gDPSetPrimColor(gfx++, 0, 0, f_TextSlotColors[i - 1][0],
        f_TextSlotColors[i - 1][1], f_TextSlotColors[i - 1][2], 255);
    DrawLargeTextSlot(&gfx, i);
  }

  SetUpGradientTextDrawing(&gfx, 0);
  sprintf(g_PrintBuffer, "Score");
  DrawLargeTextWithShadow(&gfx, f_ResultsSlide + 144,
      f_ResultsTop + detailsTop + (lineHeight * 4) + 8, g_PrintBuffer,
      CLUB_TOURNAMENT_LABEL_ENV_COLOR, CLUB_TOURNAMENT_LABEL_PRIM_COLOR,
      CLUB_TOURNAMENT_LABEL_FLAGS);
  if (f_PlayerResults[0] == 0) {
    sprintf(g_PrintBuffer, "#0", 0);
  } else {
    sprintf(g_PrintBuffer, "%+d", f_PlayerResults[0]);
  }

  DrawLargeTextWithShadow(&gfx, f_ResultsSlide + 192,
      f_ResultsTop + detailsTop + (lineHeight * 4) + 8, g_PrintBuffer,
      CLUB_TOURNAMENT_LABEL_ENV_COLOR, CLUB_TOURNAMENT_LABEL_PRIM_COLOR,
      TEXT_FLAG_FIXED_WIDTH);
  if (gameMode == GAME_MODE_SPEED_GOLF) {
    hundredths = (s32)(g_GameState.stopwatchFrameCount *
                     ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
        HUNDREDTHS_PER_SECOND;
    seconds = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND %
        SECONDS_PER_MINUTE;
    minutes = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND /
        SECONDS_PER_MINUTE % SHOWN_MINUTES_LIMIT;
    sprintf(g_PrintBuffer, f_TimeLabel);
    DrawLargeTextWithShadow(&gfx, f_ResultsSlide + 144,
        f_ResultsTop + detailsTop + (lineHeight * 5) + 8, g_PrintBuffer,
        CLUB_TOURNAMENT_LABEL_ENV_COLOR, CLUB_TOURNAMENT_LABEL_PRIM_COLOR,
        CLUB_TOURNAMENT_LABEL_FLAGS);
    sprintf(g_PrintBuffer, f_TimeFormat, minutes, seconds, hundredths);
    DrawLargeTextWithShadow(&gfx, f_ResultsSlide + 192,
        f_ResultsTop + detailsTop + (lineHeight * 5) + 8, g_PrintBuffer,
        CLUB_TOURNAMENT_LABEL_ENV_COLOR, CLUB_TOURNAMENT_LABEL_PRIM_COLOR,
        TEXT_FLAG_FIXED_WIDTH);
  }

  for (i = 0; i != SCORE_PASSWORD_LINE_LENGTH; i++) {
    g_PrintBuffer[i] = f_ScorePassword[i];
  }

  g_PrintBuffer[SCORE_PASSWORD_LINE_LENGTH] = '\0';
  DrawLargeTextWithShadow(&gfx, f_ResultsSlide + 116, f_ResultsTop + 182,
      g_PrintBuffer, SCORE_PASSWORD_ENV_COLOR, SCORE_PASSWORD_PRIM_COLOR,
      TEXT_FLAG_FIXED_WIDTH);
  for (i = 0; i != SCORE_PASSWORD_LINE_LENGTH; i++) {
    g_PrintBuffer[i] = f_ScorePassword[SCORE_PASSWORD_LINE_LENGTH + i];
  }

  g_PrintBuffer[SCORE_PASSWORD_LINE_LENGTH] = '\0';
  DrawLargeTextWithShadow(&gfx, f_ResultsSlide + 116, f_ResultsTop + 200,
      g_PrintBuffer, SCORE_PASSWORD_ENV_COLOR, SCORE_PASSWORD_PRIM_COLOR,
      TEXT_FLAG_FIXED_WIDTH);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", DrawClubTournamentResults);
#endif

#define BACKDROP_STRIP_WIDTH 288
#define BACKDROP_STRIP_HEIGHT 5
#define BACKDROP_STRIP_COUNT 40
/* The club tournament backdrop is taller, so it starts higher */
#define CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT 7
#define CLUB_TOURNAMENT_BACKDROP_STRIP_COUNT 32
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

/* The results card, its game mode's results, the bobbing title and the players'
 * portraits */
#ifdef NON_MATCHING
void DrawRoundResults(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  f32 titleX;
  f32 titleY;
  s32 left;
  s32 top;
  s32 right;
  s32 stripHeight;
  s32 textureS;
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
  LoadLargePalette(&gfx, f_ResultsBackdropImage + IMAGE_HEADER_SIZE);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  if (g_GameState.isClubTournament) {
    for (i = 0; i != CLUB_TOURNAMENT_BACKDROP_STRIP_COUNT; i++) {
      LoadTextureBlock(&gfx,
          f_ResultsBackdropImage + LARGE_PALETTED_TEXELS_OFFSET +
              (i * BACKDROP_STRIP_WIDTH *
                  CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_STRIP_WIDTH,
          CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      DrawTextureRect(&gfx, (f_ResultsSlide + 16) * 4,
          (f_ResultsTop + ((i * CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT) + 8)) *
              4,
          (f_ResultsSlide + 16 + BACKDROP_STRIP_WIDTH) * 4,
          (f_ResultsTop +
              ((i * CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT) + 8 +
                  CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT)) *
              4,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  } else {
    for (i = 0; i != BACKDROP_STRIP_COUNT; i++) {
      LoadTextureBlock(&gfx,
          f_ResultsBackdropImage + LARGE_PALETTED_TEXELS_OFFSET +
              (i * BACKDROP_STRIP_WIDTH * BACKDROP_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_STRIP_WIDTH, BACKDROP_STRIP_HEIGHT,
          0, 0, 0, 0, 0, 0, 0);
      DrawTextureRect(&gfx, (f_ResultsSlide + 16) * 4,
          ((i * BACKDROP_STRIP_HEIGHT) + 24 + f_ResultsTop) * 4,
          (f_ResultsSlide + 16 + BACKDROP_STRIP_WIDTH) * 4,
          ((i * BACKDROP_STRIP_HEIGHT) + 24 + BACKDROP_STRIP_HEIGHT +
              f_ResultsTop) *
              4,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  if (f_IsAwaitingConfirm == TRUE) {
    f_HandFrame = (f_HandFrame + 1) % (HAND_FRAME_COUNT * HAND_FRAME_DRAWS);
    LoadLargePalette(&gfx, f_ResultsHandImage + IMAGE_HEADER_SIZE);
    LoadTextureBlock(&gfx,
        f_ResultsHandImage + LARGE_PALETTED_TEXELS_OFFSET +
            (f_HandFrame / HAND_FRAME_DRAWS * HAND_SIZE * HAND_SIZE),
        G_IM_FMT_CI, G_IM_SIZ_8b, HAND_SIZE, HAND_SIZE, 0, 0, 0, 0, 0, 0, 0);
    DrawTextureRect(&gfx, (f_ResultsSlide + 272) * 4, (f_ResultsTop + 192) * 4,
        (f_ResultsSlide + 272 + HAND_SIZE) * 4,
        (f_ResultsTop + 192 + HAND_SIZE) * 4, G_TX_RENDERTILE, 0, 0,
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
    LoadLargePalette(&gfx, f_FinalResultsTitleImage + IMAGE_HEADER_SIZE);
    right = left + FINAL_RESULTS_TITLE_WIDTH;
    for (i = 0; i != FINAL_RESULTS_TITLE_STRIP_COUNT; i++) {
      stripHeight = FINAL_RESULTS_TITLE_STRIP_HEIGHT;
      if (i == FINAL_RESULTS_TITLE_STRIP_COUNT - 1) {
        stripHeight = FINAL_RESULTS_TITLE_LAST_STRIP_HEIGHT;
      }

      LoadTextureBlock(&gfx,
          f_FinalResultsTitleImage + LARGE_PALETTED_TEXELS_OFFSET +
              (i * FINAL_RESULTS_TITLE_WIDTH *
                  FINAL_RESULTS_TITLE_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, FINAL_RESULTS_TITLE_WIDTH,
          FINAL_RESULTS_TITLE_LOADED_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      gSPTextureRectangle(gfx++, (left - f_ResultsSlide) * 4,
          (i * FINAL_RESULTS_TITLE_STRIP_HEIGHT + f_ResultsTop + top) * 4,
          (right - f_ResultsSlide) * 4,
          (i * FINAL_RESULTS_TITLE_STRIP_HEIGHT + stripHeight + f_ResultsTop +
              top) *
              4,
          G_TX_RENDERTILE, textureS, textureT, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  for (i = 0; i != g_GameState.playerCount; i++) {
    AssetFile file;
    s32 unused[4];

    if (f_PortraitFrames[i] != NO_PORTRAIT_FRAME) {
      if (!(f_PortraitTimers[i] & PORTRAIT_FRAME_DRAWS_MASK)) {
        switch (f_PortraitAnimations[i]) {
          case PORTRAIT_ANIMATION_REST:
            OpenEncodedAsset(
                g_CharacterImageAssets[(s8)g_Players[i].characterIndex], &file);
            ReadEncodedAsset(f_PlayerPortraitImages[i], &file);
            f_PortraitFrames[i] = NO_PORTRAIT_FRAME;
            break;
          case PORTRAIT_ANIMATION_UNK1:
            OpenEncodedAsset(
                g_CharacterImageAssets[(s8)g_Players[i].characterIndex] +
                    f_PortraitFrames[i],
                &file);
            ReadEncodedAsset(f_PlayerPortraitImages[i], &file);
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
                &file);
            ReadEncodedAsset(f_PlayerPortraitImages[i], &file);
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
INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", D_80224E3C_ovl_00);

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
