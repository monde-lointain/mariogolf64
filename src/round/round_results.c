/*
 * The round overlay. It begins with the results that slide in when a
 * round ends: the players' portraits and scores, the course points the
 * round earned and the notices of what those points unlocked.
 */

#include "round/round_results.h"

#include <ultra64.h>

#include "audio/sound.h"
#include "characters/character.h"
#include "controllers/controller.h"
#include "controllers/transfer_pak.h"
#include "course/course.h"
#include "debug/debug_text.h"
#include "debug/game_flags.h"
#include "game/scene.h"
#include "math/math_util.h"
#include "menus/menu.h"
#include "menus/password.h"
#include "round/bets.h"
#include "round/game_state.h"
#include "round/player.h"
#include "round/round_hud.h"
#include "round/round_stats.h"
#include "round/scorecard.h"
#include "round/scoring.h"
#include "round/tournament.h"
#include "save/awards.h"
#include "save/course_stats.h"
#include "save/save.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "screen/screen_layers.h"
#include "shot/particle_effects.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/frame_time.h"
#include "system/heap.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "text/text.h"

// f_ResultsState values
enum ResultsState {
    RESULTS_SLIDING_IN,
    RESULTS_SHOWN,
    RESULTS_CLOSING // Saves and frees them
};

typedef s32 ResultsState;

// f_BannerKind values: what the results banner shows
enum BannerKind {
    BANNER_KIND_NONE, // Set with BANNER_HIDDEN as the results load
    BANNER_KIND_TOURNAMENT_PLACE,
    // The BANNER_KIND_UNUSED_ kinds are never set
    BANNER_KIND_UNUSED_A,
    BANNER_KIND_POINTS,
    BANNER_KIND_UNUSED_B,
    // Would show MESSAGE_UNUSED_BANNER
    BANNER_KIND_UNUSED_MESSAGE
};

typedef s32 BannerKind;

// f_BannerState values
enum BannerState {
    BANNER_UNSET = -1, // Until the results first load
    BANNER_SLIDING_IN,
    BANNER_SHOWN,
    BANNER_HIDDEN = 3,
    BANNER_SLIDING_OUT
};

typedef s32 BannerState;

// f_UnlockNoticeKind values
enum UnlockNoticeKind {
    UNLOCK_NOTICE_KIND_NONE,
    UNLOCK_NOTICE_KIND_COURSE,
    UNLOCK_NOTICE_KIND_CHARACTER
};

typedef s32 UnlockNoticeKind;

enum NoticeStage {
    NOTICE_STAGE_SWINGING_IN,
    NOTICE_STAGE_SWINGING
};

typedef s32 NoticeStage;

// f_TallySpeed normally, and while A is held
#define NORMAL_TALLY_SPEED 1
#define FAST_TALLY_SPEED 10
// At the normal speed, the tally ticks on every fourth call
#define TALLY_SOUND_INTERVAL_MASK 3
// The points that the debug point flags add
#define SMALL_DEBUG_POINTS_BONUS 500
#define LARGE_DEBUG_POINTS_BONUS 9999
// Where the debug unlock choice prints the chosen character
#define DEBUG_UNLOCK_CHOICE_COLUMN 25
#define DEBUG_UNLOCK_CHOICE_ROW 5
#define CONFIRM_SOUND_CHANNEL 15
#define CONFIRM_SOUND_VOLUME 100

// The faces a results portrait plays through: the pleased one, the character's
// faces 1 to 4, is given to winners, to the player shown a score password, and
// to every player when a course opens; the beaten one, faces 5 to 8, to players
// ranked below another.
enum FaceAnimation {
    FACE_ANIMATION_PLEASED = 1,
    FACE_ANIMATION_BEATEN = 4
};

typedef s32 FaceAnimation;

// The course notice drops in on a half sine swing, NOTICE_SWING_STEP an
// update, and is done NOTICE_DONE_FRAME frames after it starts
#define NOTICE_SWING_HEIGHT 160.0f
// Nearly a quarter turn back, so that the swing starts at its full height
#define NOTICE_SWING_START_ANGLE (-1.57f)
#define NOTICE_SWING_STEP (M_PI / 9)
#define NOTICE_SWING_FRAMES 46
#define NOTICE_DONE_FRAME 51

// The character notice's portrait changes face every few frames after
// a pause, then lifts away, faster and faster, until it is off screen
#define NOTICE_FACE_DELAY 10
// Every fourth frame
#define NOTICE_FACE_INTERVAL_MASK 3
#define NOTICE_LAST_FACE_FRAME 3
#define NOTICE_LIFT_DELAY 50
#define NOTICE_LIFT_START_SPEED 8
// Until the lift passes this, it rises NOTICE_LIFT_START_SPEED a frame; after,
// it grows each frame by this fraction of itself
#define NOTICE_LIFT_START_LIMIT (-2)
#define NOTICE_LIFT_GROWTH_NUMERATOR 2
#define NOTICE_LIFT_GROWTH_DENOMINATOR 3
#define NOTICE_LIFT_EXIT (-200)

// The labels image's 16-color strips. Each is stored wider than drawn.
#define RESULTS_LABEL_WIDTH 72
#define RESULTS_LABEL_STORED_WIDTH 80
#define RESULTS_LABEL_HEIGHT 18

// The portrait's mirrored wrap masks its S at its width, 1 << 5
#define FACE_SIZE_LOG2 5

// Course points are clamped there rather than overflow
#define COURSE_POINTS_MAX S32_MAX

// The places of f_TournamentPrizes's columns after the trophy winners'
#define TROPHY_PLACE_COUNT 3
#define PRIZE_TOP_TEN 10
#define PRIZE_TOP_TWENTY 20

// The columns of f_TournamentPrizes: a prize for each trophy place, then one
// for each wider bracket of places
enum PrizeBracket {
    PRIZE_BRACKET_FIRST,
    PRIZE_BRACKET_SECOND,
    PRIZE_BRACKET_THIRD,
    PRIZE_BRACKET_TOP_TEN,
    PRIZE_BRACKET_TOP_TWENTY,
    PRIZE_BRACKET_REST,
    PRIZE_BRACKET_COUNT
};

// The results' images. g_GameState.isClubTournament swaps in another set.
#define ASSET_RESULTS_BACKDROP 0x6A0
#define ASSET_CLUB_TOURNAMENT_BACKDROP 0x947
#define ASSET_FINAL_RESULTS_TITLE 0x69F
#define ASSET_COURSE_NOTICE_TITLE 0x55F
#define ASSET_CHARACTER_NOTICE_TITLE 0x560
// A 72 by 64 white glow fading to yellow, loaded but never drawn
#define ASSET_RESULTS_GLOW_IMAGE 0x72C
#define ASSET_RESULTS_GLOW_PALETTE 0x72D
#define ASSET_MATCH_WIN_BANNER 0x6A2
#define ASSET_MATCH_LOSE_BANNER 0x6A3
// The words UP, DOWN and EVEN, stacked, for match play standings
#define ASSET_MATCH_STANDING_WORDS 0x8FE
// The digits 0 to 9, stacked
#define ASSET_TOURNAMENT_PLACE_DIGITS 0x6A1

// Makes DrawRank draw the rank as a place: 1st, 2nd and so on
#define RANK_AS_PLACE 0x1000
#define NOTICE_TITLE_STRIP_HEIGHT 8
#define NOTICE_SHINE_STEP 18
#define NOTICE_SHINE_START 16
#define NOTICE_SHINE_END 40
// The title's depth, and the shine's behind it, so that the shine only lights
// the title's letters
#define NOTICE_TITLE_DEPTH 0x3FFF
#define NOTICE_SHINE_DEPTH 0x4027
#define NOTICE_SWING_SOUND_CHANNEL 13
#define NOTICE_SWING_SOUND_VOLUME 95
#define NOTICE_SHINE_SOUND_CHANNEL 14
#define NOTICE_SHINE_SOUND_VOLUME 63
#define RESULTS_LABEL_SCORE 2
#define RESULTS_LABEL_POINTS 5
#define MESSAGE_MORE_POINTS_TO_NEXT_COURSE 0x46
#define MESSAGE_ALL_COURSES_OPEN 0x47
#define RESULTS_TEXT_SLOT 0
#define RESULTS_TEXT_RED 230
#define RESULTS_TEXT_GREEN 230
#define RESULTS_TEXT_BLUE 255
#define RESULTS_TEXT_ALPHA OPAQUE_ALPHA
// The Course Points footer: the points total, then the text of the points
// left to the next course, or else the notice that every course is open,
// whose row differs by screen
#define POINTS_TOTAL_X 29
#define POINTS_TOTAL_Y 201
#define POINTS_TO_NEXT_COURSE_X 64
#define POINTS_TO_NEXT_COURSE_Y 200
#define ALL_COURSES_OPEN_X 32
#define SOLO_ALL_COURSES_OPEN_Y 192
#define PLAYERS_ALL_COURSES_OPEN_Y 200
#define MATCH_ALL_COURSES_OPEN_Y 204
// The minutes wrap here, since the time shows two digits
#define SHOWN_MINUTES_LIMIT 99
#define RESULTS_LABEL_BETS 4
#define RESULTS_HEADING_Y 54
// Bet totals of four digits all shift left, so that the column stays centered
#define FOUR_DIGIT_BET_SHIFT (-16)
#define FOUR_DIGIT_BET_MIN 999
#define FOUR_DIGIT_BET_MAX 9999

// f_PlayerRanks of a match's players
#define MATCH_RANK_WON 100
#define MATCH_RANK_LOST 101

void FreeRoundResults(void);
void LoadRoundResults(void);
void DrawResultsBanner(Gfx **displayListHead);
Boolean UpdateTournamentResults(ControllerState *controller);
Boolean ShowScorePassword(ControllerState *controller);
Boolean UpdateCoursePointsResults(ControllerState *controller);
Boolean UpdateMatchResults(ControllerState *controller);

// The score password shows as two lines of equal length
#define SCORE_PASSWORD_LINE_LENGTH (SCORE_PASSWORD_LENGTH / 2)

// The images of the trophy a course's tournament gives its top three
typedef struct TrophyAssets {
    s32 palette; // For first place; second and third follow
    s32 image;
    s32 frameCount; // Of image, as the trophy turns
} TrophyAssets;

// The course points a round earned: earned counts down as the results add
// them to the save, and shown keeps the total for the banner
typedef struct CoursePointsTally {
    s32 earned;
    s32 shown;
} CoursePointsTally;

// Each course's trophy image's sizes, indexed by these
#define TROPHY_WIDTH 0
// TROPHY_PART_COUNT of them
#define TROPHY_PART_HEIGHTS 1
#define TROPHY_SHINE_HEIGHT 3
#define TROPHY_SIZE_COUNT 4

// "StrokeSpeedGolfMini-Golf", which only an unread pointer points to
extern const char f_UnusedModeNamesText[];

static Boolean f_AreResultsLoaded = FALSE;
static ResultsState f_ResultsState = RESULTS_SLIDING_IN;
// The step within the results' state, from each mode's own steps, such as
// TOURNAMENT_STEP_START
static s32 f_ResultsStep = 0;
// Frames the results have run, which some steps reset to time a wait
static s32 f_ResultsStepTime = 0;
static s32 f_ResultsSlide = SCREEN_WIDTH;
static s32 f_ResultsTop = 0;
static s32 f_TallySpeed = NORMAL_TALLY_SPEED;
// The pressing hand shows while the results wait for A
static Boolean f_IsAwaitingConfirm = FALSE;
// Each course's tournament prizes, in course points: first to third
// place, then up to PRIZE_TOP_TEN, PRIZE_TOP_TWENTY and the rest
static s32 f_TournamentPrizes[COURSE_COUNT][PRIZE_BRACKET_COUNT] = {
    {50, 25, 12, 8, 5, 3},
    {200, 100, 50, 20, 10, 6},
    {600, 300, 150, 75, 35, 17},
    {1200, 600, 300, 100, 50, 25},
    {2000, 1000, 500, 200, 100, 50},
    {2000, 1000, 500, 200, 100, 50},
};
static f32 f_CoursePointRates[] = {1.0f, 1.5f, 2.0f, 2.5f, 3.5f, 3.5f, 1.0f, 1.0f};
static s32 f_OpenCourseCount = 1;
static s32 f_TournamentOpenCourseCount = 1;
static s32 f_PointsToNextCourse = 0;
static TrophyAssets f_TrophyAssets[COURSE_COUNT] = {
    {0x88A, 0x88D, 16},
    {0x8AF, 0x8B2, 16},
    {0x877, 0x87A, 16},
    {0x8D5, 0x8D8, 16},
    {0x8C2, 0x8C5, 16},
    {0x89D, 0x8A0, 15},
};
static BannerState f_BannerState = BANNER_UNSET;
static BannerKind f_BannerKind = BANNER_KIND_NONE;
static s32 f_BannerX = SCREEN_WIDTH;
static s32 f_BannerY = 0;
static WideAssetId f_CourseNoticeAssets[] = {0x96C, 0x96D, 0x96E, 0x96F, 0x970};
static s32 f_TallySoundCount = 0;
static s32 f_UnusedPingPongFrames[6] UNUSED = {0, 1, 2, 3, 2, 1};
static s32 f_TrophySizes[COURSE_COUNT][TROPHY_SIZE_COUNT] = {
    {40, 24, 24, 38},
    {40, 40, 11, 40},
    {40, 40, 11, 50},
    {32, 32, 32, 64},
    {48, 22, 20, 21},
    {36, 40, 11, 40},
};
static s32 f_TrophyShineFrame = 0;
static s32 f_TrophyFrame = 0;
static CharacterIndex f_DebugUnlockCharacter = 0;
static const char *f_UnusedModeNamesPointers[2] UNUSED = {NULL, f_UnusedModeNamesText};
// The text color of each text slot, from slot 1
static u8 f_TextSlotColors[][RGB_CHANNEL_COUNT] = {
    {0xFF, 0xFF, 0xFF},
    {0x7F, 0xBF, 0xFF},
    {0x7F, 0xBF, 0xFF},
    {0x7F, 0xFF, 0x4B},
    {0xFF, 0x7F, 0x9F},
};
// Counts up through the pressing hand's frames, HAND_FRAME_DRAWS draws to a
// frame
static s32 f_PressingHandFrame = 0;
static Radians f_TitleBobAngle = 0.0f;

static u8 f_UnusedBufferBeforeBackdrop[0x10] UNUSED;
static ImageAsset *f_ResultsBackdropImage;
static ImageAsset *f_FinalResultsTitleImage;
static ImageAsset *f_ResultsCourseImage;
static ImageAsset *f_FaceImage;
static PaletteColor *f_FacePalette;
static ImageAsset *f_PlayerFaceImages[PLAYER_COUNT_MAX];
static PaletteColor *f_PlayerFacePalettes[PLAYER_COUNT_MAX];
static ImageAsset *f_ResultsCloudImage;
static ImageAsset *f_ResultsLabelsImage;
static ImageAsset *f_ResultsLargeDigitsImage;
static ImageAsset *f_ResultsPlacesImage;
// In a match, the LOSE banner that DrawRank draws for a rank past RANK_BANNER;
// in a tournament, the digits it draws a place from the 4th with
static ImageAsset *f_SecondBannerImage;
static ImageAsset *f_PlaqueImage;
static ImageAsset *f_NoticeTitleImage;
static ImageAsset *f_ResultsShineImage;
// An image and palette that are loaded and freed, but never drawn
static ImageAsset *f_UnusedResultsImage;
static PaletteColor *f_UnusedResultsPalette;
static ImageAsset *f_TrophyImage;
static PaletteColor *f_TrophyPalette;
static ImageAsset *f_ResultsHandImage;
static ImageAsset *f_MatchStandingWordsImage;
static ImageAsset *f_NintendoBadgeImage;
static ImageAsset *f_DigitsImage;
static ImageAsset *f_PlaceDigitsImage;
static UnlockNoticeKind f_UnlockNoticeKind;
static Boolean f_HasBets;
static CharacterIndex f_FaceCharacterIndex;
static NoticeStage f_NoticeStage;
static s32 f_NoticeFrameCount;
static s32 f_NoticeSwing;
// Moves the course notice down; only ever set to 0
static s32 f_CourseNoticeShift;
static Radians f_NoticeAngle;
static Boolean f_IsNoticeHeld;
static s32 f_NoticeTime;
static s32 f_NoticeFaceFrame;
static s32 f_NoticeLift;
// Moves the character notice down; only ever set to 0
static s32 f_CharacterNoticeShift;
// The score password of a round of a tournament entered by password
static char f_ScorePassword[SCORE_PASSWORD_LENGTH];
static s32 f_PlayerResults[PLAYER_COUNT_MAX];
static s32 f_PlayerPoints[PLAYER_COUNT_MAX];
static s32 f_PlayerBetTotals[PLAYER_COUNT_MAX];
static s32 f_PlayerRanks[PLAYER_COUNT_MAX];
static s32 f_PrizePoints;
static s32 f_ShownPrizePoints;
static CoursePointsTally f_CoursePointsTally;
static s32 f_FaceAnimations[PLAYER_COUNT_MAX];
static s32 f_FaceFrames[PLAYER_COUNT_MAX];
static s32 f_FaceFrameTimes[PLAYER_COUNT_MAX];
#define NO_FACE_FRAME (-1)

// The course points that open the courses up to courseIndex, together
s32 SumCoursePointsNeeded(CourseIndex courseIndex) {
    s32 total = 0;
    CourseIndex course;

    for (course = 1; course < courseIndex + 1; course++) {
        total += GetCoursePointsNeeded(course);
    }

    return total;
}

// Loads the results and slides them in, or with CLOSE_ROUND_RESULTS frees them
void OpenRoundResults(RoundResultsRequest request) {
    if (request == CLOSE_ROUND_RESULTS) {
        FreeRoundResults();
        return;
    }

    LoadRoundResults();
    f_ResultsSlide = SCREEN_WIDTH;
    f_ResultsState = RESULTS_SLIDING_IN;
}

// Adds to the save the average points of the players that have them,
// scaled by the course's rate, counting them with index. A macro, since the
// original repeats it for each mode.
#define AddRoundPoints(save, index) \
    { \
        s32 counted_; \
        s32 total_; \
\
        f_OpenCourseCount = CountOpenCourses((save)->coursePoints); \
        if (f_OpenCourseCount < COURSE_COUNT) { \
            counted_ = 0; \
            f_PointsToNextCourse = \
                GetCoursePointsNeeded(f_OpenCourseCount) - (save)->coursePoints; \
            f_CoursePointsTally.earned = 0; \
            for ((index) = 0; (index) != g_GameState.playerCount; (index)++) { \
                if (f_PlayerPoints[(index)] >= 0) { \
                    counted_++; \
                    f_CoursePointsTally.earned += f_PlayerPoints[(index)]; \
                } \
            } \
            if (counted_ == 0) { \
                counted_ = 1; \
            } \
            f_CoursePointsTally.earned = f_CoursePointsTally.earned * \
                f_CoursePointRates[GetCourseRecordIndex(g_GameState.courseIndex)] / counted_; \
            f_CoursePointsTally.shown = f_CoursePointsTally.earned; \
            f_PointsToNextCourse -= f_CoursePointsTally.earned; \
        } \
        total_ = (save)->coursePoints + f_CoursePointsTally.earned; \
        if (total_ > COURSE_POINTS_MAX) { \
            (save)->coursePoints = COURSE_POINTS_MAX; \
        } \
        else { \
            (save)->coursePoints = total_; \
        } \
    }

// Loads the results screen's images and each player's portrait, works out the
// players' results and ranks for the game mode, adds the round's course points
// to the save and starts the results' song. It does nothing if the results are
// already loaded.
//
// The club tournament's results loop sits in a block of its own, as a macro's
// do-while would make it, to match. The original keeps k in $s2 and the loop's
// stepped result address in $s1. flow weights a reference by its loop depth,
// and a loop that runs once still counts, so the block lifts the address's 7
// references to 11 over a live length of 12, priority 27500 in global-alloc,
// where k has 59 over 121, priority 24380: the address takes $s1 first.
// Without the block, or with plain braces, the address's priority is 11666 and
// k's 22314, so k takes $s1 and 54 words differ, none in shape. A block inside
// the loop, around its statement, gives the same 54. In the block, counting
// the results with i, with j or with a counter of their own gives 159, 191 or
// 175 words, 7 in shape. With the stroke results' loop in a block as well, the
// function still matches.
//
// k also counts the tally, the tournament's standings and the skins' totals
// and ranks. None of the other choices of counter differs in shape: counting
// the Game Boy experience loop or the stroke results with k gives 70 words;
// the bet loop, the portrait loop or both with k 62, 60 or 68; the tally with
// a counter of its own 90, all three tallies with i or j 158 or 128, and any
// one of them 110 or 58; the tournament's standings or the skins' totals with
// i 110, and the skins' ranks 112.
//
// The match game reads the last hole into holeEnd before clearing the results,
// as the matched scorecard_drawing.c reads its countEnd, and the original loads
// it in GetFirstHoleIndex's delay slot; testing g_GameState.holeIndex in the
// loop gives 17 words (3 in shape). Its clear is written as
// f_PlayerResults[0] = f_PlayerResults[1] = 0, which stores the second result
// first, as the original does; two statements in that order give 10 words (8
// in shape), and in the other order 6 (6). The tournament's cap adds the prize
// before the earned points, as the subtraction before it does; the other
// order in its test gives 8 words (2 in shape), and in its sum as well 10 (2).
//
// The original keeps the earned points' address in $a1 across the tally and
// builds it again in $a0 after GetCourseRecordIndex, the address forms GCC
// gives a struct's fields; as two separate s32s they are addressed through $at
// each time, and 93 words differ, 25 in shape.
//
// The tournament's open course count is stored first, as the original does, and
// the ranks are counted in f_PlayerRanks itself. The tournament finds the
// player's standing through a pointer to the standing it tests. AddRoundPoints
// passes g_GameState.courseIndex to GetCourseRecordIndex, which ignores it, as
// the original loads it into $a0 before both calls; play.c passes the same
// field where it opens the scorecard.
void LoadRoundResults(void) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save;
    s32 i;
    s32 j;
    s32 k;
    s32 holeEnd;
    PaletteColor *palette;
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

    if (g_GameState.isClubTournament) {
        f_ResultsBackdropImage =
            AllocMemory(OpenEncodedAsset(ASSET_CLUB_TOURNAMENT_BACKDROP, &asset));
        ReadEncodedAsset(f_ResultsBackdropImage, &asset);
    }
    else {
        f_ResultsBackdropImage = AllocMemory(OpenEncodedAsset(ASSET_RESULTS_BACKDROP, &asset));
        ReadEncodedAsset(f_ResultsBackdropImage, &asset);
    }
    f_ResultsHandImage = AllocMemory(OpenEncodedAsset(ASSET_PRESSING_HAND, &asset));
    ReadEncodedAsset(f_ResultsHandImage, &asset);
    f_DigitsImage = AllocMemory(OpenEncodedAsset(ASSET_DISTANCE_DIGITS, &asset));
    ReadEncodedAsset(f_DigitsImage, &asset);
    f_PlaceDigitsImage = AllocMemory(OpenEncodedAsset(ASSET_PLACE_DIGITS, &asset));
    ReadEncodedAsset(f_PlaceDigitsImage, &asset);
    if (g_GameState.isClubTournament) {
        f_NintendoBadgeImage = AllocMemory(OpenEncodedAsset(ASSET_NINTENDO_BADGE, &asset));
        ReadEncodedAsset(f_NintendoBadgeImage, &asset);
        f_ResultsCourseImage = AllocMemory(
            OpenEncodedAsset(g_TournamentIconAssets[GetClubTournamentCourse()], &asset)
        );
        ReadEncodedAsset(f_ResultsCourseImage, &asset);
    }
    else {
        f_FinalResultsTitleImage = AllocMemory(OpenEncodedAsset(ASSET_FINAL_RESULTS_TITLE, &asset));
        ReadEncodedAsset(f_FinalResultsTitleImage, &asset);
        f_ResultsLabelsImage = AllocMemory(OpenEncodedAsset(ASSET_SCORE_LABELS, &asset));
        ReadEncodedAsset(f_ResultsLabelsImage, &asset);
        f_ResultsLargeDigitsImage = AllocMemory(OpenEncodedAsset(ASSET_LARGE_DIGITS, &asset));
        ReadEncodedAsset(f_ResultsLargeDigitsImage, &asset);
        f_ResultsCloudImage = AllocMemory(OpenEncodedAsset(ASSET_WIDE_CLOUD, &asset));
        ReadEncodedAsset(f_ResultsCloudImage, &asset);
        f_ResultsCourseImage = AllocMemory(OpenEncodedAsset(g_TournamentIconAssets[0], &asset));
        ReadEncodedAsset(f_ResultsCourseImage, &asset);
        f_NoticeTitleImage = AllocMemory(OpenEncodedAsset(ASSET_COURSE_NOTICE_TITLE, &asset));
        ReadEncodedAsset(f_NoticeTitleImage, &asset);
        f_ResultsShineImage = AllocMemory(OpenEncodedAsset(ASSET_SHINE_IMAGE, &asset));
        ReadEncodedAsset(f_ResultsShineImage, &asset);
        f_UnusedResultsImage = AllocMemory(OpenEncodedAsset(ASSET_RESULTS_GLOW_IMAGE, &asset));
        ReadEncodedAsset(f_UnusedResultsImage, &asset);
        OpenAssetFile(ASSET_RESULTS_GLOW_PALETTE, &file);
        size = GetAssetFileSize(&file);
        f_UnusedResultsPalette = AllocMemory(size);
        ReadAssetFile(f_UnusedResultsPalette, size, &file);
    }

    for (i = 0; i != g_GameState.playerCount; i++) {
        f_PlayerFaceImages[i] = AllocMemory(
            OpenEncodedAsset(g_CharacterFirstFaceAssets[g_Players[i].characterIndex], &asset)
        );
        ReadEncodedAsset(f_PlayerFaceImages[i], &asset);
        OpenAssetFile(
            g_CharacterFirstPaletteFiles[g_Players[i].characterIndex] + g_Players[i].paletteIndex,
            &file
        );
        size = GetAssetFileSize(&file);
        palette = AllocMemory(size);
        f_PlayerFacePalettes[i] = palette;
        ReadAssetFile(palette, size, &file);
        if (g_Players[i].isComputer) {
            f_PlayerPoints[i] = -1;
        }
        else {
            f_PlayerPoints[i] = g_Players[i].points;
        }
        f_PlayerBetTotals[i] = 0;
        for (j = 0; j != BET_COUNT; j++) {
            f_PlayerBetTotals[i] += g_Players[i].betTallies[j];
        }
    }

    if (!g_GameState.isClubTournament) {
        if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
            f_ResultsPlacesImage = AllocMemory(OpenEncodedAsset(ASSET_MATCH_WIN_BANNER, &asset));
            ReadEncodedAsset(f_ResultsPlacesImage, &asset);
            f_SecondBannerImage = AllocMemory(OpenEncodedAsset(ASSET_MATCH_LOSE_BANNER, &asset));
            ReadEncodedAsset(f_SecondBannerImage, &asset);
            f_PlaqueImage = AllocMemory(OpenEncodedAsset(ASSET_WOODEN_PLAQUE, &asset));
            ReadEncodedAsset(f_PlaqueImage, &asset);
            f_MatchStandingWordsImage =
                AllocMemory(OpenEncodedAsset(ASSET_MATCH_STANDING_WORDS, &asset));
            ReadEncodedAsset(f_MatchStandingWordsImage, &asset);
            f_FaceCharacterIndex = g_Players[1].characterIndex;
        }
        else {
            f_ResultsPlacesImage = AllocMemory(OpenEncodedAsset(ASSET_PLACES, &asset));
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
        f_FaceAnimations[i] = 0;
        f_FaceFrames[i] = NO_FACE_FRAME;
        f_FaceFrameTimes[i] = 0;
    }

    if (g_GameState.isClubTournament) {
        // In a block of its own to match, as the header comment explains
        IN_OWN_BLOCK(for (k = 0; k != g_GameState.playerCount; k++) {
            f_PlayerResults[k] = GetNetScoreToPar(GetFirstHoleIndex() + GetRoundHoleCount(), k);
        });
    }
    else {
        switch (g_GameState.gameMode) {
            case GAME_MODE_STROKE:
            case GAME_MODE_SPEED_GOLF:
                for (i = 0; i != g_GameState.playerCount; i++) {
                    f_PlayerResults[i] =
                        GetNetScoreToPar(GetFirstHoleIndex() + GetRoundHoleCount(), i);
                }

                for (i = 0; i != g_GameState.playerCount; i++) {
                    f_PlayerRanks[i] = 0;
                    for (j = 0; j != g_GameState.playerCount; j++) {
                        if (i != j && f_PlayerResults[i] > f_PlayerResults[j]) {
                            f_PlayerRanks[i]++;
                        }
                    }
                }

                AddRoundPoints(save, k);
                break;
            case GAME_MODE_MATCH_GAME:
            case GAME_MODE_GET_CHARACTER:
                holeEnd = g_GameState.holeIndex;
                f_PlayerResults[0] = f_PlayerResults[1] = 0;
                for (i = GetFirstHoleIndex(); i != holeEnd; i++) {
                    if (g_Players[0].holes[i].strokeCount < g_Players[1].holes[i].strokeCount) {
                        f_PlayerResults[0]++;
                        f_PlayerResults[1]--;
                    }
                    else if (g_Players[1].holes[i].strokeCount <
                        g_Players[0].holes[i].strokeCount) {
                        f_PlayerResults[0]--;
                        f_PlayerResults[1]++;
                    }
                }

                if (f_PlayerResults[0] > f_PlayerResults[1]) {
                    f_PlayerRanks[0] = MATCH_RANK_WON;
                    f_PlayerRanks[1] = MATCH_RANK_LOST;
                }
                else {
                    f_PlayerRanks[0] = MATCH_RANK_LOST;
                    f_PlayerRanks[1] = MATCH_RANK_WON;
                }

                f_FaceImage = AllocMemory(OpenEncodedAsset(
                    g_CharacterFirstFaceAssets[g_Players[1].characterIndex],
                    &asset
                ));
                ReadEncodedAsset(f_FaceImage, &asset);
                OpenAssetFile(
                    g_CharacterFirstPaletteFiles[g_Players[1].characterIndex] +
                        g_Players[1].paletteIndex,
                    &file
                );
                size = GetAssetFileSize(&file);
                f_FacePalette = AllocMemory(size);
                ReadAssetFile(f_FacePalette, size, &file);
                AddRoundPoints(save, k);
                break;
            case GAME_MODE_UNLISTED_STROKE:
            case GAME_MODE_TRAINING:
            case GAME_MODE_UNRECORDED_STROKE:
            case GAME_MODE_RECORD_REPLAY:
            case GAME_MODE_RING_SHOT:
            case GAME_MODE_SKINS_MATCH:
            case GAME_MODE_CLUB_SLOTS:
                TallySkins(GetFirstHoleIndex() + GetRoundHoleCount());
                for (k = 0; k != g_GameState.playerCount; k++) {
                    f_PlayerResults[k] = g_RoundHud.skinsTotals[k];
                }

                for (k = 0; k != g_GameState.playerCount; k++) {
                    f_PlayerRanks[k] = 0;
                    for (j = 0; j != g_GameState.playerCount; j++) {
                        if (k != j && f_PlayerResults[k] < f_PlayerResults[j]) {
                            f_PlayerRanks[k]++;
                        }
                    }
                }

                AddRoundPoints(save, k);
                break;
            case GAME_MODE_TOURNAMENT:
                f_SecondBannerImage =
                    AllocMemory(OpenEncodedAsset(ASSET_TOURNAMENT_PLACE_DIGITS, &asset));
                ReadEncodedAsset(f_SecondBannerImage, &asset);
                f_PlayerResults[0] = GetNetScoreToPar(GetFirstHoleIndex() + GetRoundHoleCount(), 0);
                RankTournament(COURSE_HOLE_COUNT, 0, f_PlayerResults[0], g_GameState.roundSeed);
                for (k = 0; k != TOURNAMENT_CONTESTANT_COUNT; k++) {
                    TournamentStanding *standing = &g_TournamentStandings[k];

                    if (standing->contestantId == -1) {
                        f_PlayerRanks[0] = standing->place & PLACE_MASK;
                        break;
                    }
                }

                if (f_PlayerRanks[0] <= TROPHY_PLACE_COUNT) {
                    f_PrizePoints =
                        f_TournamentPrizes[g_GameState.courseIndex][f_PlayerRanks[0] - 1];
                }
                else if (f_PlayerRanks[0] <= PRIZE_TOP_TEN) {
                    f_PrizePoints =
                        f_TournamentPrizes[g_GameState.courseIndex][PRIZE_BRACKET_TOP_TEN];
                }
                else if (f_PlayerRanks[0] <= PRIZE_TOP_TWENTY) {
                    f_PrizePoints =
                        f_TournamentPrizes[g_GameState.courseIndex][PRIZE_BRACKET_TOP_TWENTY];
                }
                else {
                    f_PrizePoints = f_TournamentPrizes[g_GameState.courseIndex][PRIZE_BRACKET_REST];
                }

                f_ShownPrizePoints = f_PrizePoints;
                if (f_PlayerRanks[0] <= TROPHY_PLACE_COUNT) {
                    f_TrophyImage = AllocMemory(
                        OpenEncodedAsset(f_TrophyAssets[g_GameState.courseIndex].image, &asset)
                    );
                    ReadEncodedAsset(f_TrophyImage, &asset);
                    OpenAssetFile(
                        f_TrophyAssets[g_GameState.courseIndex].palette + f_PlayerRanks[0] - 1,
                        &file
                    );
                    size = GetAssetFileSize(&file);
                    f_TrophyPalette = AllocMemory(size);
                    ReadAssetFile(f_TrophyPalette, size, &file);
                }

                f_CoursePointsTally.earned =
                    f_PlayerPoints[0] * f_CoursePointRates[g_GameState.courseIndex];
                f_CoursePointsTally.shown = f_CoursePointsTally.earned;
                f_TournamentOpenCourseCount = CountOpenCourses(save->coursePoints);
                f_OpenCourseCount = f_TournamentOpenCourseCount;
                if (f_OpenCourseCount < COURSE_COUNT) {
                    f_PointsToNextCourse = GetCoursePointsNeeded(f_OpenCourseCount) -
                        save->coursePoints - f_PrizePoints - f_CoursePointsTally.earned;
                }

                if (save->coursePoints + f_PrizePoints + f_CoursePointsTally.earned >
                    COURSE_POINTS_MAX) {
                    save->coursePoints = COURSE_POINTS_MAX;
                }
                else {
                    save->coursePoints += f_PrizePoints + f_CoursePointsTally.earned;
                }

                break;
        }
    }

    for (i = 0; i != g_GameState.playerCount; i++) {
        if (g_Players[i].characterIndex >= CHARACTER_KID && f_PlayerPoints[i] > 0) {
            AddGameBoyExperience(g_Players[i].gameBoyCharacterIndex, f_PlayerPoints[i]);
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

// Loads the notice of what the round unlocked: one of the characters for
// the notices below UNLOCK_NOTICE_COURSES, or else the course from
// UNLOCK_NOTICE_COURSES on.
void LoadUnlockNotice(UnlockNotice unlock) {
    EncodedAsset asset;
    AssetFile file;
    CourseIndex courseIndex;
    u32 size;

    GetSaveData();
    f_IsNoticeHeld = FALSE;
    f_NoticeTime = 0;
    f_NoticeFaceFrame = 0;
    f_NoticeLift = 0;
    f_CharacterNoticeShift = 0;
    f_NoticeFrameCount = 0;
    f_NoticeStage = NOTICE_STAGE_SWINGING_IN;
    f_NoticeSwing = 0;
    f_CourseNoticeShift = 0;
    f_NoticeAngle = 0;
    courseIndex = 0;
    if (unlock == UNLOCK_NOTICE_DONKEY_KONG) {
        f_FaceCharacterIndex = CHARACTER_DONKEY_KONG;
        f_UnlockNoticeKind = UNLOCK_NOTICE_KIND_CHARACTER;
    }
    else if (unlock == UNLOCK_NOTICE_MAPLE) {
        f_FaceCharacterIndex = CHARACTER_MAPLE;
        f_UnlockNoticeKind = UNLOCK_NOTICE_KIND_CHARACTER;
    }
    else if (unlock == UNLOCK_NOTICE_METAL_MARIO) {
        f_FaceCharacterIndex = CHARACTER_METAL_MARIO;
        f_UnlockNoticeKind = UNLOCK_NOTICE_KIND_CHARACTER;
    }
    else {
        courseIndex = unlock - UNLOCK_NOTICE_COURSES;
        f_UnlockNoticeKind = UNLOCK_NOTICE_KIND_COURSE;
    }

    f_ResultsShineImage = AllocMemory(OpenEncodedAsset(ASSET_SHINE_IMAGE, &asset));
    ReadEncodedAsset(f_ResultsShineImage, &asset);
    if (f_UnlockNoticeKind == UNLOCK_NOTICE_KIND_COURSE) {
        f_NoticeTitleImage = AllocMemory(OpenEncodedAsset(ASSET_COURSE_NOTICE_TITLE, &asset));
        ReadEncodedAsset(f_NoticeTitleImage, &asset);
        f_ResultsCourseImage =
            AllocMemory(OpenEncodedAsset(f_CourseNoticeAssets[courseIndex], &asset));
        ReadEncodedAsset(f_ResultsCourseImage, &asset);
    }

    if (f_UnlockNoticeKind == UNLOCK_NOTICE_KIND_CHARACTER) {
        f_NoticeTitleImage = AllocMemory(OpenEncodedAsset(ASSET_CHARACTER_NOTICE_TITLE, &asset));
        ReadEncodedAsset(f_NoticeTitleImage, &asset);
        f_PlaqueImage = AllocMemory(OpenEncodedAsset(ASSET_WOODEN_PLAQUE, &asset));
        ReadEncodedAsset(f_PlaqueImage, &asset);
        f_FaceImage =
            AllocMemory(OpenEncodedAsset(g_CharacterFirstFaceAssets[f_FaceCharacterIndex], &asset));
        ReadEncodedAsset(f_FaceImage, &asset);
        OpenAssetFile(g_CharacterFirstPaletteFiles[f_FaceCharacterIndex], &file);
        size = GetAssetFileSize(&file);
        f_FacePalette = AllocMemory(size);
        ReadAssetFile(f_FacePalette, size, &file);
    }
}

void FreeUnlockNotice(void) {
    FreeMemory(&f_NoticeTitleImage);
    FreeMemory(&f_ResultsShineImage);
    if (f_UnlockNoticeKind == UNLOCK_NOTICE_KIND_COURSE) {
        FreeMemory(&f_ResultsCourseImage);
    }

    if (f_UnlockNoticeKind == UNLOCK_NOTICE_KIND_CHARACTER) {
        FreeMemory(&f_PlaqueImage);
        FreeMemory(&f_FaceImage);
        FreeMemory(&f_FacePalette);
    }

    f_UnlockNoticeKind = UNLOCK_NOTICE_KIND_NONE;
}

// Swings the course in, or raises the character's face; returns whether the
// notice is done
Boolean UpdateUnlockNotice(void) {
    EncodedAsset asset;

    if (f_UnlockNoticeKind == UNLOCK_NOTICE_KIND_COURSE) {
        f_NoticeFrameCount++;
        switch (f_NoticeStage) {
            case NOTICE_STAGE_SWINGING_IN:
                f_NoticeSwing = -sinf(f_NoticeAngle) * NOTICE_SWING_HEIGHT;
                f_CourseNoticeShift = 0;
                f_NoticeAngle += NOTICE_SWING_STEP;
                if (f_NoticeAngle > M_PI) {
                    f_NoticeAngle = M_PI;
                }

                if (f_NoticeFrameCount >= NOTICE_SWING_FRAMES) {
                    f_NoticeStage = NOTICE_STAGE_SWINGING;
                }

                break;
            case NOTICE_STAGE_SWINGING:
                f_NoticeSwing = -sinf(f_NoticeAngle) * NOTICE_SWING_HEIGHT;
                f_NoticeAngle += NOTICE_SWING_STEP;
                if (f_NoticeFrameCount < NOTICE_DONE_FRAME) {
                    break;
                }

                return TRUE;
        }
    }
    else {
        f_NoticeTime++;
        if (f_IsNoticeHeld) {
            return FALSE;
        }

        if (f_NoticeTime > NOTICE_FACE_DELAY &&
            !(f_NoticeTime & NOTICE_FACE_INTERVAL_MASK) &&
            f_NoticeFaceFrame < NOTICE_LAST_FACE_FRAME) {
            f_NoticeFaceFrame++;
            OpenEncodedAsset(
                g_CharacterFirstFaceAssets[f_FaceCharacterIndex] + f_NoticeFaceFrame,
                &asset
            );
            ReadEncodedAsset(f_FaceImage, &asset);
        }

        if (f_NoticeTime > NOTICE_LIFT_DELAY) {
            if (f_NoticeLift > NOTICE_LIFT_START_LIMIT) {
                f_NoticeLift -= NOTICE_LIFT_START_SPEED;
            }
            else {
                f_NoticeLift +=
                    f_NoticeLift * NOTICE_LIFT_GROWTH_NUMERATOR / NOTICE_LIFT_GROWTH_DENOMINATOR;
            }

            if (f_NoticeLift < NOTICE_LIFT_EXIT) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

void FreeRoundResults(void) {
    s32 playerIndex;

    if (f_AreResultsLoaded) {
        f_AreResultsLoaded = FALSE;
        FreeMemory(&f_ResultsBackdropImage);
        FreeMemory(&f_ResultsHandImage);
        FreeMemory(&f_DigitsImage);
        FreeMemory(&f_PlaceDigitsImage);
        if (g_GameState.isClubTournament) {
            FreeMemory(&f_NintendoBadgeImage);
            FreeMemory(&f_ResultsCourseImage);
        }
        else {
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

        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            FreeMemory(&f_PlayerFaceImages[playerIndex]);
            FreeMemory(&f_PlayerFacePalettes[playerIndex]);
        }

        if (!g_GameState.isClubTournament) {
            if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
                FreeMemory(&f_ResultsPlacesImage);
                FreeMemory(&f_SecondBannerImage);
                FreeMemory(&f_PlaqueImage);
                FreeMemory(&f_MatchStandingWordsImage);
            }
            else {
                FreeMemory(&f_ResultsPlacesImage);
            }

            switch (g_GameState.gameMode) {
                case GAME_MODE_STROKE:
                case GAME_MODE_UNLISTED_STROKE:
                case GAME_MODE_TRAINING:
                case GAME_MODE_UNRECORDED_STROKE:
                case GAME_MODE_SPEED_GOLF:
                case GAME_MODE_RECORD_REPLAY:
                case GAME_MODE_RING_SHOT:
                case GAME_MODE_SKINS_MATCH:
                case GAME_MODE_CLUB_SLOTS:
                    break;
                case GAME_MODE_MATCH_GAME:
                case GAME_MODE_GET_CHARACTER:
                    FreeMemory(&f_FaceImage);
                    FreeMemory(&f_FacePalette);
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

void StartFaceAnimation(FaceAnimation animation, s32 playerIndex) {
    f_FaceAnimations[playerIndex] = animation;
    f_FaceFrames[playerIndex] = 0;
    f_FaceFrameTimes[playerIndex] = 0;
}

// Ticks as the points count up, on every fourth call at the normal speed
void PlayTallySound(void) {
    f_TallySoundCount++;
    if (f_TallySpeed != NORMAL_TALLY_SPEED || !(f_TallySoundCount & TALLY_SOUND_INTERVAL_MASK)) {
        PlaySound(SOUND_TICK, g_NextSoundChannel, SOUND_FULL_SCALE);
    }
}

// Draws value right-aligned in four of the 8 by 12 digits of f_DigitsImage
void DrawResultsNumber(Gfx **displayListHead, s32 x, s32 y, s32 number) {
    Gfx *gfx = *displayListHead;
    u8 *text;
    u8 character;
    s32 offset;

    sprintf(g_PrintBuffer, "%4d", number);
    LoadSmallPalette(&gfx, 0, ImagePalette(f_DigitsImage));
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(f_DigitsImage)),
        G_IM_FMT_CI,
        DISTANCE_FONT_WIDTH,
        DISTANCE_GLYPH_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    offset = 0;
    for (text = (u8 *)g_PrintBuffer; (character = *text) != '\0';
        text++, offset += DISTANCE_DIGIT_WIDTH) {
        if (character != ' ') {
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(x + offset),
                QuarterPixels(y),
                QuarterPixels(x + offset + DISTANCE_DIGIT_WIDTH),
                QuarterPixels(y + DISTANCE_GLYPH_HEIGHT),
                G_TX_RENDERTILE,
                TexelCoordinate((character - '0') * DISTANCE_DIGIT_WIDTH),
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    *displayListHead = gfx;
}

#define PLACE_NUMBER_X_OFFSET (-12)

// Loads glyph from f_PlaceDigitsImage and draws it at x, y
#define DrawPlaceGlyph(gfx, x, y, glyph) \
    { \
        gDPLoadTextureTile( \
            (gfx)++, \
            TextureAddress(LargeImageTexels(f_PlaceDigitsImage)), \
            G_IM_FMT_CI, \
            G_IM_SIZ_8b, \
            PLACE_GLYPH_SIZE, \
            0, \
            0, \
            (glyph) * PLACE_GLYPH_SIZE, \
            PLACE_GLYPH_SIZE - 1, \
            ((glyph) * PLACE_GLYPH_SIZE) + PLACE_GLYPH_SIZE - 1, \
            0, \
            G_TX_NOMIRROR | G_TX_WRAP, \
            G_TX_NOMIRROR | G_TX_WRAP, \
            G_TX_NOMASK, \
            G_TX_NOMASK, \
            G_TX_NOLOD, \
            G_TX_NOLOD \
        ); \
        gSPScisTextureRectangle( \
            (gfx)++, \
            QuarterPixels(x), \
            QuarterPixels(y), \
            QuarterPixels((x) + PLACE_GLYPH_SIZE), \
            QuarterPixels((y) + PLACE_GLYPH_SIZE), \
            G_TX_RENDERTILE, \
            0, \
            TexelCoordinate((glyph) * PLACE_GLYPH_SIZE), \
            TEXEL_STEP_ONE, \
            TEXEL_STEP_ONE \
        ); \
    }

// Draws place, as in "21st", centered on x
void DrawPlaceNumber(Gfx **displayListHead, s32 x, s32 y, s32 place) {
    Gfx *gfx = *displayListHead;
    char *digit;
    s32 width;
    PlaceSuffix suffix;
    s32 lastDigit;
    u8 character;
    s32 glyph;

    sprintf(g_PrintBuffer, "%d", place);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadLargePalette(&gfx, ImagePalette(f_PlaceDigitsImage));
    width = 0;
    for (digit = g_PrintBuffer; *digit != '\0'; digit++) {
        width += PLACE_GLYPH_SPACING;
    }

    x += PLACE_NUMBER_X_OFFSET - (width / 2);
    width = 0;
    for (digit = g_PrintBuffer; *digit != '\0'; digit++, width += PLACE_GLYPH_SPACING) {
        character = *digit;
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
        }
        else if (suffix < PLACES_PER_HUNDRED) {
            lastDigit = suffix % DECIMAL_BASE;
            if (lastDigit < PLACE_SUFFIX_TH) {
                suffix = lastDigit;
            }
            else {
                suffix = PLACE_SUFFIX_TH;
            }
        }
    }

    DrawPlaceGlyph(gfx, x + width, y, suffix + PLACE_SUFFIX_GLYPH_FIRST);
    *displayListHead = gfx;
}

void DrawPlaceIcon(Gfx **displayListHead, s32 x, s32 y, s32 place) {
    Gfx *gfx = *displayListHead;

    LoadLargePalette(&gfx, ImagePalette(f_ResultsPlacesImage));
    LoadTextureBlock(
        &gfx,
        f_ResultsPlacesImage +
            ((place * PLACE_PART_SIZE * PLACE_PART_SIZE) + LARGE_PALETTED_TEXELS_OFFSET),
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        PLACE_PART_SIZE,
        PLACE_PART_SIZE,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(x),
        QuarterPixels(y),
        QuarterPixels(x + PLACE_PART_SIZE),
        QuarterPixels(y + PLACE_PART_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    *displayListHead = gfx;
}

// Ranks from this one draw a banner instead
#define RANK_BANNER 100
#define RANK_BANNER_WIDTH 96
#define RANK_BANNER_STRIP_HEIGHT 16
#define RANK_BANNER_STRIP_COUNT 3
// DrawRank's ranks count from 0 for the 1st place. From the 4th, it draws
// the place as digits and a suffix icon: st, nd, rd or th.
#define RANK_FOURTH 3
#define RANK_TWENTY_FIRST 20
#define RANK_TWENTY_FOURTH 23
#define PLACE_DIGIT_SPACING 20
#define PLACE_DIGIT_X 4
#define PLACE_DIGIT_Y 10
// Two-digit places start further left, from the 10th
#define RANK_TENTH 9
#define PLACE_TWO_DIGIT_SHIFT 16
#define RANK_BANNER_X_OFFSET (-12)

// Draws the place icon for a rank. With RANK_AS_PLACE, places past 3rd
// are written as digits followed by a suffix.
void DrawRank(Gfx **displayListHead, s32 x, s32 y, s32 rank) {
    Gfx *gfx = *displayListHead;
    PlaceSuffix suffix;
    s32 digitX;
    s32 digit;
    u8 *image;
    s32 i;
    s32 stripY;
    s32 offset;

    if (rank & RANK_AS_PLACE) {
        rank &= ~RANK_AS_PLACE;
        suffix = rank;
        if (rank >= RANK_FOURTH) {
            if (rank < RANK_TWENTY_FIRST) {
                suffix = PLACE_SUFFIX_TH;
            }
            else if (rank < RANK_TWENTY_FOURTH) {
                suffix = rank - RANK_TWENTY_FIRST;
            }
            else {
                suffix = PLACE_SUFFIX_TH;
            }
        }

        LoadLargePalette(&gfx, ImagePalette(f_ResultsPlacesImage));
        LoadTextureBlock(
            &gfx,
            f_ResultsPlacesImage +
                ((suffix * PLACE_PART_SIZE * PLACE_PART_SIZE) + LARGE_PALETTED_TEXELS_OFFSET +
                    (PLACE_NUMBER_PART_COUNT * PLACE_PART_SIZE * PLACE_PART_SIZE)),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            PLACE_PART_SIZE,
            PLACE_PART_SIZE,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(x + PLACE_GLYPH_SIZE),
            QuarterPixels(y),
            QuarterPixels(x + PLACE_GLYPH_SIZE + PLACE_PART_SIZE),
            QuarterPixels(y + PLACE_PART_SIZE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        if (rank >= RANK_FOURTH) {
            sprintf(g_PrintBuffer, "%d", rank + 1);
            LoadSmallPalette(&gfx, 0, ImagePalette(f_SecondBannerImage));
            if (rank >= RANK_TENTH) {
                x -= PLACE_TWO_DIGIT_SHIFT;
            }

            for (i = 0; g_PrintBuffer[i] != '\0'; i++) {
                digit = g_PrintBuffer[i] - '0';
                digitX = (i * PLACE_DIGIT_SPACING) + PLACE_DIGIT_X;
                gDPLoadTextureTile_4b(
                    gfx++,
                    TextureAddress(ImageTexels(f_SecondBannerImage)),
                    G_IM_FMT_CI,
                    PLACE_GLYPH_SIZE,
                    0,
                    0,
                    digit * PLACE_GLYPH_SIZE,
                    PLACE_GLYPH_SIZE - 1,
                    digit * PLACE_GLYPH_SIZE + PLACE_GLYPH_SIZE - 1,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(x + digitX),
                    QuarterPixels(y + PLACE_DIGIT_Y),
                    QuarterPixels(x + digitX + PLACE_GLYPH_SIZE),
                    QuarterPixels(y + PLACE_DIGIT_Y + PLACE_GLYPH_SIZE),
                    G_TX_RENDERTILE,
                    0,
                    TexelCoordinate(digit * PLACE_GLYPH_SIZE),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }
        }
        else {
            DrawPlaceIcon(&gfx, x, y, rank);
        }
    }
    else if (rank >= RANK_BANNER) {
        if (rank == RANK_BANNER) {
            image = f_ResultsPlacesImage;
        }
        else {
            image = f_SecondBannerImage;
        }

        LoadLargePalette(&gfx, ImagePalette(image));
        for (i = 0; i != RANK_BANNER_STRIP_COUNT; i++) {
            offset =
                LARGE_PALETTED_TEXELS_OFFSET + (i * RANK_BANNER_WIDTH * RANK_BANNER_STRIP_HEIGHT);
            stripY = y + (i * RANK_BANNER_STRIP_HEIGHT);
            LoadTextureBlock(
                &gfx,
                image + offset,
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                RANK_BANNER_WIDTH,
                RANK_BANNER_STRIP_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(x + RANK_BANNER_X_OFFSET),
                QuarterPixels(stripY),
                QuarterPixels(x + RANK_BANNER_X_OFFSET + RANK_BANNER_WIDTH),
                QuarterPixels(stripY + RANK_BANNER_STRIP_HEIGHT),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }
    else {
        LoadLargePalette(&gfx, ImagePalette(f_ResultsPlacesImage));
        LoadTextureBlock(
            &gfx,
            f_ResultsPlacesImage +
                ((rank * PLACE_PART_SIZE * PLACE_PART_SIZE) + LARGE_PALETTED_TEXELS_OFFSET +
                    (PLACE_NUMBER_PART_COUNT * PLACE_PART_SIZE * PLACE_PART_SIZE)),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            PLACE_PART_SIZE,
            PLACE_PART_SIZE,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(x + PLACE_GLYPH_SIZE),
            QuarterPixels(y),
            QuarterPixels(x + PLACE_GLYPH_SIZE + PLACE_PART_SIZE),
            QuarterPixels(y + PLACE_PART_SIZE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        DrawPlaceIcon(&gfx, x, y, rank);
    }

    *displayListHead = gfx;
}

void DrawResultsLabel(Gfx **displayListHead, s32 x, s32 y, s32 label) {
    Gfx *gfx = *displayListHead;

    LoadSmallPalette(&gfx, 0, ImagePalette(f_ResultsLabelsImage));
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(
            f_ResultsLabelsImage +
            ((label * (RESULTS_LABEL_STORED_WIDTH * RESULTS_LABEL_HEIGHT / 2)) +
                PALETTED_TEXELS_OFFSET)
        ),
        G_IM_FMT_CI,
        RESULTS_LABEL_STORED_WIDTH,
        0,
        0,
        0,
        RESULTS_LABEL_WIDTH - 1,
        RESULTS_LABEL_HEIGHT - 1,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(x),
        QuarterPixels(y),
        QuarterPixels(x + RESULTS_LABEL_WIDTH),
        QuarterPixels(y + RESULTS_LABEL_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    *displayListHead = gfx;
}

// isMirrored starts the texture a width in (S10.5 texels), where its
// mirrored wrap flips it
void DrawPlayerFace(Gfx **displayListHead, s32 x, s32 y, s32 playerIndex, Boolean isMirrored) {
    Gfx *gfx = *displayListHead;

    LoadLargePalette(&gfx, f_PlayerFacePalettes[playerIndex]);
    LoadTextureBlock(
        &gfx,
        f_PlayerFaceImages[playerIndex],
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        FACE_FRAME_SIZE,
        FACE_FRAME_SIZE,
        0,
        G_TX_MIRROR,
        G_TX_NOMIRROR,
        FACE_SIZE_LOG2,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(x),
        QuarterPixels(y),
        QuarterPixels(x + FACE_FRAME_SIZE),
        QuarterPixels(y + FACE_FRAME_SIZE),
        G_TX_RENDERTILE,
        isMirrored * TexelCoordinate(FACE_FRAME_SIZE),
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    *displayListHead = gfx;
}

void StartResultsNotice(UnlockNoticeKind kind) {
    f_UnlockNoticeKind = kind;
    switch (kind) {
        case UNLOCK_NOTICE_KIND_NONE:
            break;
        case UNLOCK_NOTICE_KIND_COURSE:
            f_NoticeStage = NOTICE_STAGE_SWINGING_IN;
            f_NoticeFrameCount = 0;
            f_NoticeSwing = SCREEN_WIDTH;
            f_CourseNoticeShift = 0;
            f_NoticeAngle = NOTICE_SWING_START_ANGLE;
            break;
        case UNLOCK_NOTICE_KIND_CHARACTER:
            f_IsNoticeHeld = FALSE;
            f_NoticeTime = 0;
            f_NoticeFaceFrame = 0;
            f_NoticeLift = 0;
            f_CharacterNoticeShift = 0;
            break;
    }
}

// Swings the notice in, then counts the course it opened
void UpdateCourseResultsNotice(void) {
    SaveData *save = GetSaveData();

    f_NoticeFrameCount++;
    switch (f_NoticeStage) {
        case NOTICE_STAGE_SWINGING_IN:
            f_NoticeSwing = -sinf(f_NoticeAngle) * NOTICE_SWING_HEIGHT;
            f_CourseNoticeShift = 0;
            f_NoticeAngle += NOTICE_SWING_STEP;
            if (f_NoticeAngle > M_PI) {
                f_NoticeAngle = M_PI;
            }

            if (f_NoticeFrameCount >= NOTICE_SWING_FRAMES) {
                f_NoticeStage = NOTICE_STAGE_SWINGING;
            }

            break;
        case NOTICE_STAGE_SWINGING:
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

// The course image is two strips of 64x32 that shrink from twice their size to
// full size as the notice swings
#define NOTICE_COURSE_STRIP_WIDTH 64
#define NOTICE_COURSE_STRIP_HEIGHT 32
#define NOTICE_COURSE_STRIP_COUNT 2
#define NOTICE_CENTER_X (SCREEN_WIDTH / 2)
#define NOTICE_CENTER_Y 128
#define NOTICE_COURSE_SCALE_BIAS 2.0f
#define NOTICE_TITLE_STRIP_WIDTH 160
#define NOTICE_TITLE_STRIP_COUNT 8
#define NOTICE_TITLE_LEFT 80
#define NOTICE_TITLE_TOP 56
// The top of the title's strip I, lifted by LIFT
#define TitleStripTop(lift, i) ((lift) + NOTICE_TITLE_TOP + ((i) * NOTICE_TITLE_STRIP_HEIGHT))
#define NOTICE_TITLE_LIFT 96.0f
// Wider than the shine texture, so that it sweeps across the title
#define NOTICE_SHINE_DSDX TexelRunStep(SHINE_SIZE, NOTICE_TITLE_STRIP_WIDTH)
#define NOTICE_PAN_START 3
#define NOTICE_PAN_END 60
#define NOTICE_PAN_RANGE 126
#define NOTICE_CHEER_SOUND_CHANNEL 3

// The notice that a course has opened: its picture and title swing in, then a
// shine sweeps the title
void DrawCourseResultsNotice(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 size;
    s32 texelStep;
    s32 lift;
    s32 shineLift;
    s32 depthLift;
    SoundPan pan;
    s32 centerX;
    s32 i;
    s32 shineTexelX;
    s32 stripBottomOffset;

    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

    // Once the swing settles the picture is drawn at full size, where point
    // sampling is sharper
    if (f_NoticeAngle == M_PI) {
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_POINT);
    }
    else {
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
    }

    LoadLargePalette(&gfx, ImagePalette(f_ResultsCourseImage));
    for (i = 0; i != NOTICE_COURSE_STRIP_COUNT; i++) {
        s32 top;
        size = (cosf(f_NoticeAngle) + NOTICE_COURSE_SCALE_BIAS) * NOTICE_COURSE_STRIP_HEIGHT;
        texelStep = TexelRunStep(NOTICE_COURSE_STRIP_HEIGHT, size);
        LoadTextureBlock(
            &gfx,
            LargeImageTexels(f_ResultsCourseImage) +
                (i * NOTICE_COURSE_STRIP_WIDTH * NOTICE_COURSE_STRIP_HEIGHT),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NOTICE_COURSE_STRIP_WIDTH,
            NOTICE_COURSE_STRIP_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        top = NOTICE_CENTER_Y - size + (i * size);
        centerX = f_NoticeSwing + NOTICE_CENTER_X;
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(centerX - size),
            QuarterPixels(top + f_CourseNoticeShift),
            QuarterPixels(size + centerX),
            QuarterPixels(top + size + f_CourseNoticeShift),
            G_TX_RENDERTILE,
            0,
            0,
            texelStep,
            texelStep
        );
    }

    LoadLargePalette(&gfx, ImagePalette(f_NoticeTitleImage));
    for (i = 0; i != NOTICE_TITLE_STRIP_COUNT; i++) {
        s32 top;
        lift = -(s32)(cosf(f_NoticeAngle) * NOTICE_TITLE_LIFT) - (s32)NOTICE_TITLE_LIFT;
        LoadTextureBlock(
            &gfx,
            LargeImageTexels(f_NoticeTitleImage) +
                (i * NOTICE_TITLE_STRIP_WIDTH * NOTICE_TITLE_STRIP_HEIGHT),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NOTICE_TITLE_STRIP_WIDTH,
            NOTICE_TITLE_STRIP_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        top = TitleStripTop(lift, i);
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(f_NoticeSwing + NOTICE_TITLE_LEFT),
            QuarterPixels(top + f_CourseNoticeShift),
            QuarterPixels(f_NoticeSwing + NOTICE_TITLE_LEFT + NOTICE_TITLE_STRIP_WIDTH),
            QuarterPixels(top + NOTICE_TITLE_STRIP_HEIGHT + f_CourseNoticeShift),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    // The title again, into the depth buffer alone
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPSetPrimDepth(gfx++, NOTICE_TITLE_DEPTH, 0);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 1, TEXEL0, 1, 0, 0, 0, TEXEL0, 0, 1, TEXEL0, 1);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(
        gfx++,
        GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
        G_RM_AA_ZB_TEX_EDGE2
    );
    LoadLargePalette(&gfx, ImagePalette(f_NoticeTitleImage));
    for (i = 0; i != NOTICE_TITLE_STRIP_COUNT; i++) {
        depthLift = -(s32)(cosf(f_NoticeAngle) * NOTICE_TITLE_LIFT) - (s32)NOTICE_TITLE_LIFT;
        LoadTextureBlock(
            &gfx,
            LargeImageTexels(f_NoticeTitleImage) +
                (i * NOTICE_TITLE_STRIP_WIDTH * NOTICE_TITLE_STRIP_HEIGHT),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NOTICE_TITLE_STRIP_WIDTH,
            NOTICE_TITLE_STRIP_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        // The top is written out and the bottom's offset kept in a local, unlike
        // the colored title's, to match: writing the bottom out too gives 35
        // words differing, 5 in shape
        stripBottomOffset = f_CourseNoticeShift + NOTICE_TITLE_STRIP_HEIGHT;
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(f_NoticeSwing + NOTICE_TITLE_LEFT),
            QuarterPixels(TitleStripTop(depthLift, i) + f_CourseNoticeShift),
            QuarterPixels(f_NoticeSwing + NOTICE_TITLE_LEFT + NOTICE_TITLE_STRIP_WIDTH),
            QuarterPixels(TitleStripTop(depthLift, i) + stripBottomOffset),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    gDPSetPrimDepth(gfx++, NOTICE_SHINE_DEPTH, 0);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    LoadTextureBlock(
        &gfx,
        ImageBody(f_ResultsShineImage),
        G_IM_FMT_I,
        G_IM_SIZ_8b,
        SHINE_SIZE,
        SHINE_SIZE,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    if (f_NoticeFrameCount == NOTICE_SHINE_START) {
        PlaySoundWithEffects(
            SOUND_SHRINKING_IMAGE,
            NOTICE_SHINE_SOUND_CHANNEL,
            NOTICE_SHINE_SOUND_VOLUME,
            NO_REVERB,
            SOUND_FULL_SCALE
        );
    }

    if (f_NoticeFrameCount == 2) {
        PlaySoundWithEffects(
            SOUND_UNLOCK_EVERYTHING,
            NOTICE_SWING_SOUND_CHANNEL,
            NOTICE_SWING_SOUND_VOLUME,
            NO_REVERB,
            SOUND_PAN_RIGHT
        );
        PlaySound(SOUND_COURSE_NOTICE_CHEER, NOTICE_CHEER_SOUND_CHANNEL, SOUND_FULL_SCALE);
    }

    // The swing sound follows the notice across the screen
    if (f_NoticeFrameCount >= NOTICE_PAN_START && f_NoticeFrameCount < NOTICE_PAN_END) {
        pan = (f_NoticeSwing + NOTICE_CENTER_X) * NOTICE_PAN_RANGE / NOTICE_CENTER_X;
        if (pan < SOUND_PAN_LEFT) {
            pan = SOUND_PAN_LEFT;
        }

        if (pan > SOUND_PAN_RIGHT) {
            pan = SOUND_PAN_RIGHT;
        }

        SetSoundPan(NOTICE_SWING_SOUND_CHANNEL, pan);
    }

    shineLift = -(s32)(cosf(f_NoticeAngle) * NOTICE_TITLE_LIFT) - (s32)NOTICE_TITLE_LIFT;
    if (f_NoticeFrameCount >= NOTICE_SHINE_START) {
        if (f_NoticeFrameCount < NOTICE_SHINE_END) {
            shineTexelX = TexelCoordinate(
                SHINE_SIZE - ((f_NoticeFrameCount - NOTICE_SHINE_START) * NOTICE_SHINE_STEP)
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(f_NoticeSwing + NOTICE_TITLE_LEFT),
                QuarterPixels(NOTICE_TITLE_TOP + shineLift + f_CourseNoticeShift),
                QuarterPixels(f_NoticeSwing + NOTICE_TITLE_LEFT + NOTICE_TITLE_STRIP_WIDTH),
                QuarterPixels(NOTICE_TITLE_TOP + shineLift + SHINE_SIZE + f_CourseNoticeShift),
                G_TX_RENDERTILE,
                shineTexelX,
                0,
                NOTICE_SHINE_DSDX,
                TEXEL_STEP_ONE
            );
        }
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    *displayListHead = gfx;
}

// Changes the beaten character's face, then lifts the notice away
void UpdateCharacterResultsNotice(void) {
    EncodedAsset asset;

    GetSaveData();
    f_NoticeTime++;
    if (f_IsNoticeHeld) {
        return;
    }

    if (f_NoticeTime > NOTICE_FACE_DELAY &&
        !(f_NoticeTime & NOTICE_FACE_INTERVAL_MASK) &&
        f_NoticeFaceFrame < NOTICE_LAST_FACE_FRAME) {
        f_NoticeFaceFrame++;
        OpenEncodedAsset(
            g_CharacterFirstFaceAssets[g_Players[1].characterIndex] + f_NoticeFaceFrame,
            &asset
        );
        ReadEncodedAsset(f_FaceImage, &asset);
    }

    if (f_NoticeTime > NOTICE_LIFT_DELAY) {
        if (f_NoticeLift > NOTICE_LIFT_START_LIMIT) {
            f_NoticeLift -= NOTICE_LIFT_START_SPEED;
        }
        else {
            f_NoticeLift +=
                f_NoticeLift * NOTICE_LIFT_GROWTH_NUMERATOR / NOTICE_LIFT_GROWTH_DENOMINATOR;
        }

        if (f_NoticeLift < NOTICE_LIFT_EXIT) {
            StartResultsNotice(UNLOCK_NOTICE_KIND_NONE);
        }
    }
}

// The character notice pops open past its full size, then settles at
// sin(NOTICE_POP_SETTLE_ANGLE)
#define NOTICE_POP_FRAMES 10
#define NOTICE_POP_STEP 0.15f
#define NOTICE_POP_SETTLE_ANGLE (3 * M_PI / 4)
#define NOTICE_PLAQUE_POP_SIZE 36.0f
#define NOTICE_FACE_SIZE 32
#define NOTICE_FACE_POP_SIZE 24.0f
#define NOTICE_CHARACTER_TITLE_WIDTH 144
#define NOTICE_CHARACTER_TITLE_TOP 80
#define NOTICE_CHARACTER_TITLE_POP_WIDTH 108.0f
#define NOTICE_CHARACTER_TITLE_POP_HEIGHT 12.0f
#define NOTICE_CHARACTER_TITLE_STRIP_COUNT 7
// The top of a title strip, while the strips are height tall. It isn't kept
// in a variable, which would change the loops' code.
#define CharacterTitleStripTop(strip, height) \
    (NOTICE_CHARACTER_TITLE_TOP - ((height) * NOTICE_CHARACTER_TITLE_STRIP_COUNT / 2) + \
        ((strip) * (height)))
#define NOTICE_CHARACTER_SHINE_LEFT 88
#define NOTICE_CHARACTER_SHINE_TOP 52
#define NOTICE_CHARACTER_SHINE_HEIGHT 56
#define NOTICE_CHARACTER_SHINE_DSDX TexelRunStep(SHINE_SIZE, 128)
#define NOTICE_CHARACTER_SHINE_DTDY TexelRunStep(SHINE_SIZE, NOTICE_CHARACTER_SHINE_HEIGHT)
#define NOTICE_CHARACTER_VOICE_FRAME 14
#define NOTICE_CHARACTER_VOICE_SLOT 1

// The notice that a character has been beaten: their nameplate, face and title
// pop open, then a shine sweeps the title
void DrawCharacterResultsNotice(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    Radians angle;
    f32 popScale;
    s32 size;
    s32 texelStep;
    s32 width;
    s32 height;
    s32 titleCenterX;
    s32 stripIndex;
    s32 liftFromCenter;
    s32 shineTexelX;

    if (f_NoticeTime < NOTICE_POP_FRAMES) {
        angle = f_NoticeTime * NOTICE_POP_STEP * (M_PI / 2);
    }
    else {
        angle = NOTICE_POP_SETTLE_ANGLE;
    }

    popScale = sinf(angle);
    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    size = popScale * NOTICE_PLAQUE_POP_SIZE;
    if (size > 0) {
        s32 centerX;
        s32 centerY;

        LoadSmallPalette(&gfx, 0, ImagePalette(f_PlaqueImage));
        LoadNibbleTextureBlock(
            &gfx,
            ImageTexels(f_PlaqueImage),
            G_IM_FMT_CI,
            WOODEN_PLAQUE_SIZE,
            WOODEN_PLAQUE_SIZE,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        centerX = f_NoticeLift + NOTICE_CENTER_X;
        centerY = f_CharacterNoticeShift + NOTICE_CENTER_Y;
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(centerX - size),
            QuarterPixels(centerY - size),
            QuarterPixels(size + centerX),
            QuarterPixels(size + centerY),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(WOODEN_PLAQUE_SIZE / 2, size),
            TexelRunStep(WOODEN_PLAQUE_SIZE / 2, size)
        );
    }

    size = popScale * NOTICE_FACE_POP_SIZE;
    if (size > 0) {
        s32 centerX;
        s32 centerY;

        LoadLargePalette(&gfx, f_FacePalette);
        LoadTextureBlock(
            &gfx,
            f_FaceImage,
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NOTICE_FACE_SIZE,
            NOTICE_FACE_SIZE,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        texelStep = TexelRunStep(NOTICE_FACE_SIZE / 2, size);
        centerX = f_NoticeLift + NOTICE_CENTER_X;
        centerY = f_CharacterNoticeShift + NOTICE_CENTER_Y;
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(centerX - size),
            QuarterPixels(centerY - size),
            QuarterPixels(size + centerX),
            QuarterPixels(size + centerY),
            G_TX_RENDERTILE,
            0,
            0,
            texelStep,
            texelStep
        );
    }

    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    height = popScale * NOTICE_CHARACTER_TITLE_POP_HEIGHT;
    width = popScale * NOTICE_CHARACTER_TITLE_POP_WIDTH;
    if (width > 0 && height > 0) {
        LoadLargePalette(&gfx, ImagePalette(f_NoticeTitleImage));
        for (stripIndex = 0; stripIndex != NOTICE_CHARACTER_TITLE_STRIP_COUNT; stripIndex++) {
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(f_NoticeTitleImage) +
                    (stripIndex * NOTICE_CHARACTER_TITLE_WIDTH * NOTICE_TITLE_STRIP_HEIGHT),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                NOTICE_CHARACTER_TITLE_WIDTH,
                NOTICE_TITLE_STRIP_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            // This pass mirrors the slide, unlike the depth pass and the shine
            liftFromCenter = f_NoticeLift - NOTICE_CENTER_X;
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(NOTICE_CENTER_X - width - f_NoticeLift),
                QuarterPixels(CharacterTitleStripTop(stripIndex, height) + f_CharacterNoticeShift),
                QuarterPixels(width - liftFromCenter),
                QuarterPixels(
                    CharacterTitleStripTop(stripIndex, height) + height + f_CharacterNoticeShift
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep((NOTICE_CHARACTER_TITLE_WIDTH / 2), width),
                TexelRunStep(NOTICE_TITLE_STRIP_HEIGHT, height)
            );
        }

        // The title again, into the depth buffer alone, so that the shine behind it
        // only lights the letters
        gDPPipeSync(gfx++);
        gDPSetDepthSource(gfx++, G_ZS_PRIM);
        gDPSetPrimDepth(gfx++, NOTICE_TITLE_DEPTH, 0);
        gDPSetCombineLERP(
            gfx++,
            0,
            0,
            0,
            TEXEL0,
            0,
            1,
            TEXEL0,
            1,
            0,
            0,
            0,
            TEXEL0,
            0,
            1,
            TEXEL0,
            1
        );
        gDPPipeSync(gfx++);
        gDPSetRenderMode(
            gfx++,
            GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
            G_RM_AA_ZB_TEX_EDGE2
        );
        for (stripIndex = 0; stripIndex != NOTICE_CHARACTER_TITLE_STRIP_COUNT; stripIndex++) {
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(f_NoticeTitleImage) +
                    (stripIndex * NOTICE_CHARACTER_TITLE_WIDTH * NOTICE_TITLE_STRIP_HEIGHT),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                NOTICE_CHARACTER_TITLE_WIDTH,
                NOTICE_TITLE_STRIP_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            titleCenterX = f_NoticeLift + NOTICE_CENTER_X;
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(titleCenterX - width),
                QuarterPixels(CharacterTitleStripTop(stripIndex, height) + f_CharacterNoticeShift),
                QuarterPixels(width + titleCenterX),
                QuarterPixels(
                    CharacterTitleStripTop(stripIndex, height) + height + f_CharacterNoticeShift
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep((NOTICE_CHARACTER_TITLE_WIDTH / 2), width),
                TexelRunStep(NOTICE_TITLE_STRIP_HEIGHT, height)
            );
        }

        gDPSetPrimDepth(gfx++, NOTICE_SHINE_DEPTH, 0);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        gDPSetCombineLERP(
            gfx++,
            0,
            0,
            0,
            PRIMITIVE,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            0,
            0,
            0,
            PRIMITIVE,
            PRIMITIVE,
            0,
            TEXEL0,
            0
        );
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_NONE);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        LoadTextureBlock(
            &gfx,
            ImageBody(f_ResultsShineImage),
            G_IM_FMT_I,
            G_IM_SIZ_8b,
            SHINE_SIZE,
            SHINE_SIZE,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        if (f_NoticeTime == NOTICE_CHARACTER_VOICE_FRAME) {
            PlayCharacterSound(
                f_FaceCharacterIndex,
                NOTICE_CHARACTER_VOICE_SLOT,
                CHARACTER_SOUND_NOTICE,
                SOUND_FULL_SCALE,
                SOUND_FULL_SCALE
            );
        }

        if (f_NoticeTime == NOTICE_SHINE_START) {
            PlaySoundWithEffects(
                SOUND_SHRINKING_IMAGE,
                NOTICE_SHINE_SOUND_CHANNEL,
                NOTICE_SHINE_SOUND_VOLUME,
                NO_REVERB,
                SOUND_FULL_SCALE
            );
        }

        if (f_NoticeTime == 2) {
            PlaySoundWithEffects(
                SOUND_UNLOCK_EVERYTHING,
                NOTICE_SWING_SOUND_CHANNEL,
                NOTICE_SWING_SOUND_VOLUME,
                NO_REVERB,
                SOUND_FULL_SCALE
            );
        }

        if (f_NoticeTime >= NOTICE_SHINE_START) {
            if (f_NoticeTime < NOTICE_SHINE_END) {
                shineTexelX = TexelCoordinate(
                    SHINE_SIZE - ((f_NoticeTime - NOTICE_SHINE_START) * NOTICE_SHINE_STEP)
                );
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(f_NoticeLift + NOTICE_CHARACTER_SHINE_LEFT),
                    QuarterPixels(f_CharacterNoticeShift + NOTICE_CHARACTER_SHINE_TOP),
                    QuarterPixels(
                        f_NoticeLift + NOTICE_CHARACTER_SHINE_LEFT + NOTICE_CHARACTER_TITLE_WIDTH
                    ),
                    QuarterPixels(
                        f_CharacterNoticeShift + NOTICE_CHARACTER_SHINE_TOP +
                        NOTICE_CHARACTER_SHINE_HEIGHT
                    ),
                    G_TX_RENDERTILE,
                    shineTexelX,
                    0,
                    NOTICE_CHARACTER_SHINE_DSDX,
                    NOTICE_CHARACTER_SHINE_DTDY
                );
            }
        }

        gDPPipeSync(gfx++);
        gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    }

    *displayListHead = gfx;
}

// Returns TRUE once the results have been saved and freed
Boolean UpdateRoundResults(ControllerState *controller) {
    Boolean isDone;

    switch (f_ResultsState) {
        case RESULTS_SLIDING_IN:
            if (f_ResultsSlide == 1) {
                f_ResultsState = RESULTS_SHOWN;
                f_ResultsSlide = 0;
                f_ResultsStep = 0;
                f_ResultsStepTime = 0;
                break;
            }

            f_ResultsSlide += -f_ResultsSlide / 2;
            break;
        case RESULTS_SHOWN:
            if (g_GameState.isClubTournament) {
                isDone = ShowScorePassword(controller);
            }
            else {
                switch (g_GameState.gameMode) {
                    case GAME_MODE_STROKE:
                    case GAME_MODE_UNRECORDED_STROKE:
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

void SetResultsBanner(BannerState state, BannerKind kind) {
    f_BannerState = state;
    f_BannerKind = kind;
    if (state == BANNER_SLIDING_IN) {
        f_BannerX = SCREEN_WIDTH;
        f_BannerY = 0;
    }
}

// The trophy is drawn in two parts, as each must fit in texture memory
#define TROPHY_PART_COUNT 2

// Off the screen's left
#define BANNER_GONE_X (-SCREEN_WIDTH)
#define BANNER_SLIDE_OUT_SPEED 4

// Plus the place
#define MESSAGE_GOLD_TROPHY_PRIZE 0x3D
#define MESSAGE_PLACE_PRIZE 0x40
// Plus the place
#define MESSAGE_GOLD_TROPHY 0x41
#define MESSAGE_UNUSED_BANNER 0x48
#define MESSAGE_PLACE 0x49
#define MESSAGE_POINTS_EARNED 0x1F3

#define BANNER_TEXT_SLOT 1
#define BANNER_TEXT_RED 32
#define BANNER_TEXT_GREEN 64
#define BANNER_TEXT_BLUE 255
#define BANNER_TEXT_ALPHA OPAQUE_ALPHA
#define PRIZE_TEXT_X 66
#define PRIZE_TEXT_Y 128
#define BANNER_TEXT_X 168
#define BANNER_TEXT_Y 140

// The places that the ordinal suffixes differ for
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
// The points banner is taller, its clouds stretched
#define TALL_BANNER_CLOUD_TOP 104
#define TALL_BANNER_CLOUD_HEIGHT 40
#define BANNER_CLOUD_DSDX 0x3C3
#define TALL_BANNER_CLOUD_DTDY 0x333

// The trophy turns, a frame each TROPHY_FRAME_TICKS, as the shine sweeps it
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

// Draws the banner of a round's result, and the trophy above a top three
// tournament place
//
// The trophy's frame advances in two arms, one for the fast tally and one on
// the shine's tick, which cross-jumping merges into one copy, as the
// original has it. Each arm reads course twice, which lifts its global-alloc
// priority above the trophy loops' row values (342 and 340), so it takes $s6
// as in the original. Joining the two tests with ||, course's priority is
// 266 and it gets $fp: 32 words differ, none in shape. Against the joined
// test, assigning course later or before the kind test, narrower or
// unsigned types, reading frameCount through g_GameState, regrouping the
// trophy heights and other forms of the frame's wrap gave 32 to 74 words; a
// struct for each course's trophy sizes 52, and a helper for the two
// identical trophy-part loops 207 taking the course, or 295 taking its
// sizes' address.
void DrawResultsBanner(Gfx **displayListHead) {
    Gfx *gfx;
    s32 i;
    s32 rank;
    s32 place;
    CourseIndex courseIndex;
    s32 top;
    s32 trophyHeight;

    gfx = *displayListHead;

    switch (f_BannerState) {
        case BANNER_SHOWN:
        case BANNER_HIDDEN:
            break;
        case BANNER_SLIDING_IN:
            if (f_BannerX == 1) {
                f_BannerState = BANNER_SHOWN;
            }
            else {
                f_BannerX += -f_BannerX / 2;
            }

            break;
        case BANNER_SLIDING_OUT:
            if (f_BannerX >= -1) {
                f_BannerX -= BANNER_SLIDE_OUT_SPEED;
            }
            else {
                f_BannerX += f_BannerX / 2;
            }

            if (f_BannerX < BANNER_GONE_X) {
                f_BannerState = BANNER_HIDDEN;
            }

            break;
    }

    switch (f_BannerKind) {
        case BANNER_KIND_UNUSED_A:
        case BANNER_KIND_UNUSED_B:
            break;
        case BANNER_KIND_POINTS:
            SetTextSlotValue(f_CoursePointsTally.shown, BANNER_TEXT_SLOT, 0);
            SetTextSlot(
                f_BannerX + BANNER_TEXT_X,
                f_BannerY + BANNER_TEXT_Y,
                MESSAGE_POINTS_EARNED,
                TEXT_SLOT_LARGE_FONT,
                BANNER_TEXT_SLOT,
                TEXT_FLAG_DRAWN_BY_OWNER | TEXT_FLAG_CENTERED,
                BANNER_TEXT_RED,
                BANNER_TEXT_GREEN,
                BANNER_TEXT_BLUE,
                BANNER_TEXT_ALPHA
            );
            break;
        case BANNER_KIND_TOURNAMENT_PLACE:
            rank = f_PlayerRanks[0];
            place = rank - 1;
            if (f_TournamentOpenCourseCount < COURSE_COUNT) {
                if (place < TROPHY_PLACE_COUNT) {
                    SetTextSlotValue(f_ShownPrizePoints, BANNER_TEXT_SLOT, 0);
                    SetTextSlot(
                        f_BannerX + PRIZE_TEXT_X,
                        f_BannerY + PRIZE_TEXT_Y,
                        place + MESSAGE_GOLD_TROPHY_PRIZE,
                        TEXT_SLOT_LARGE_FONT,
                        BANNER_TEXT_SLOT,
                        TEXT_FLAG_DRAWN_BY_OWNER,
                        BANNER_TEXT_RED,
                        BANNER_TEXT_GREEN,
                        BANNER_TEXT_BLUE,
                        BANNER_TEXT_ALPHA
                    );
                }
                else {
                    if (rank < PLACE_TWENTY_FIRST || rank > PLACE_TWENTY_THIRD) {
                        sprintf(g_PrintBuffer, "%dth", rank);
                    }
                    else if (rank == PLACE_TWENTY_FIRST) {
                        sprintf(g_PrintBuffer, "%dst", rank);
                    }
                    else if (rank == PLACE_TWENTY_SECOND) {
                        sprintf(g_PrintBuffer, "%dnd", rank);
                    }
                    else if (rank == PLACE_TWENTY_THIRD) {
                        sprintf(g_PrintBuffer, "%drd", rank);
                    }

                    SetTextName(g_PrintBuffer);
                    SetTextSlotValue(f_ShownPrizePoints, BANNER_TEXT_SLOT, 1);
                    SetTextSlot(
                        f_BannerX + PRIZE_TEXT_X,
                        f_BannerY + PRIZE_TEXT_Y,
                        MESSAGE_PLACE_PRIZE,
                        TEXT_SLOT_LARGE_FONT,
                        BANNER_TEXT_SLOT,
                        TEXT_FLAG_DRAWN_BY_OWNER,
                        BANNER_TEXT_RED,
                        BANNER_TEXT_GREEN,
                        BANNER_TEXT_BLUE,
                        BANNER_TEXT_ALPHA
                    );
                }
            }
            else if (place < TROPHY_PLACE_COUNT) {
                SetTextSlot(
                    f_BannerX + BANNER_TEXT_X,
                    f_BannerY + BANNER_TEXT_Y,
                    place + MESSAGE_GOLD_TROPHY,
                    TEXT_SLOT_LARGE_FONT,
                    BANNER_TEXT_SLOT,
                    TEXT_FLAG_DRAWN_BY_OWNER | TEXT_FLAG_CENTERED,
                    BANNER_TEXT_RED,
                    BANNER_TEXT_GREEN,
                    BANNER_TEXT_BLUE,
                    BANNER_TEXT_ALPHA
                );
            }
            else {
                SetTextSlotValue(rank, BANNER_TEXT_SLOT, 0);
                SetTextSlot(
                    f_BannerX + BANNER_TEXT_X,
                    f_BannerY + BANNER_TEXT_Y,
                    MESSAGE_PLACE,
                    TEXT_SLOT_LARGE_FONT,
                    BANNER_TEXT_SLOT,
                    TEXT_FLAG_DRAWN_BY_OWNER | TEXT_FLAG_CENTERED,
                    BANNER_TEXT_RED,
                    BANNER_TEXT_GREEN,
                    BANNER_TEXT_BLUE,
                    BANNER_TEXT_ALPHA
                );
            }

            break;
        case BANNER_KIND_UNUSED_MESSAGE:
            SetTextSlot(
                f_BannerX + BANNER_TEXT_X,
                f_BannerY + BANNER_TEXT_Y,
                MESSAGE_UNUSED_BANNER,
                TEXT_SLOT_LARGE_FONT,
                BANNER_TEXT_SLOT,
                TEXT_FLAG_DRAWN_BY_OWNER | TEXT_FLAG_CENTERED,
                BANNER_TEXT_RED,
                BANNER_TEXT_GREEN,
                BANNER_TEXT_BLUE,
                BANNER_TEXT_ALPHA
            );
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
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, BANNER_CLOUD_ALPHA);
    for (i = 0; i != BANNER_CLOUD_STRIP_COUNT; i++) {
        LoadNibbleTextureBlock(
            &gfx,
            ImageBody(f_ResultsCloudImage) + (i * BANNER_CLOUD_STRIP_SIZE),
            G_IM_FMT_I,
            BANNER_CLOUD_WIDTH,
            BANNER_CLOUD_HEIGHT,
            0,
            G_TX_CLAMP,
            G_TX_CLAMP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        if (f_BannerKind == BANNER_KIND_POINTS) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(f_BannerX + BANNER_CLOUD_LEFT),
                QuarterPixels(f_BannerY + TALL_BANNER_CLOUD_TOP + (i * TALL_BANNER_CLOUD_HEIGHT)),
                QuarterPixels(f_BannerX + BANNER_CLOUD_RIGHT),
                QuarterPixels(
                    f_BannerY + TALL_BANNER_CLOUD_TOP + (i * TALL_BANNER_CLOUD_HEIGHT) +
                    TALL_BANNER_CLOUD_HEIGHT
                ),
                G_TX_RENDERTILE,
                0,
                0,
                BANNER_CLOUD_DSDX,
                TALL_BANNER_CLOUD_DTDY
            );
        }
        else {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(f_BannerX + BANNER_CLOUD_LEFT),
                QuarterPixels(f_BannerY + BANNER_CLOUD_TOP + (i * BANNER_CLOUD_HEIGHT)),
                QuarterPixels(f_BannerX + BANNER_CLOUD_RIGHT),
                QuarterPixels(
                    f_BannerY + BANNER_CLOUD_TOP + (i * BANNER_CLOUD_HEIGHT) + BANNER_CLOUD_HEIGHT
                ),
                G_TX_RENDERTILE,
                0,
                0,
                BANNER_CLOUD_DSDX,
                TEXEL_STEP_ONE
            );
        }
    }

    SetUpTextDrawing(&gfx);
    gDPSetPrimColor(
        gfx++,
        0,
        0,
        BANNER_TEXT_RED,
        BANNER_TEXT_GREEN,
        BANNER_TEXT_BLUE,
        BANNER_TEXT_ALPHA
    );
    DrawLargeTextSlot(&gfx, BANNER_TEXT_SLOT);
    if (f_BannerKind == BANNER_KIND_TOURNAMENT_PLACE) {
        courseIndex = g_GameState.courseIndex;
        if (f_PlayerRanks[0] <= TROPHY_PLACE_COUNT) {
            EncodedAsset asset;

            f_TrophyShineFrame = (f_TrophyShineFrame + 1) % TROPHY_SHINE_CYCLE;
            if (f_TallySpeed == FAST_TALLY_SPEED) {
                f_TrophyFrame++;
                if (f_TrophyFrame == f_TrophyAssets[courseIndex].frameCount) {
                    f_TrophyFrame = 0;
                }

                OpenEncodedAsset(f_TrophyAssets[courseIndex].image + f_TrophyFrame, &asset);
                ReadEncodedAsset(f_TrophyImage, &asset);
            }
            else if (f_TrophyShineFrame % TROPHY_FRAME_TICKS == 0) {
                f_TrophyFrame++;
                if (f_TrophyFrame == f_TrophyAssets[courseIndex].frameCount) {
                    f_TrophyFrame = 0;
                }

                OpenEncodedAsset(f_TrophyAssets[courseIndex].image + f_TrophyFrame, &asset);
                ReadEncodedAsset(f_TrophyImage, &asset);
            }

            gSPDisplayList(gfx++, g_DecalTextureSetup);
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            gDPPipeSync(gfx++);
            gDPSetTextureFilter(gfx++, G_TF_POINT);
            LoadLargePalette(&gfx, f_TrophyPalette);
            top = -f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS] -
                f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS + 1];
            for (i = 0; i != TROPHY_PART_COUNT; i++) {
                gDPLoadTextureTile(
                    gfx++,
                    TextureAddress(
                        AddressNumber(f_TrophyImage) +
                        (i * f_TrophySizes[courseIndex][TROPHY_WIDTH] *
                            f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS])
                    ),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    f_TrophySizes[courseIndex][TROPHY_WIDTH],
                    f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS + i],
                    0,
                    0,
                    f_TrophySizes[courseIndex][TROPHY_WIDTH] - 1,
                    f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS + i] - 1,
                    0,
                    0,
                    0,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(
                        TROPHY_CENTER_X + f_BannerX - (f_TrophySizes[courseIndex][TROPHY_WIDTH] / 2)
                    ),
                    QuarterPixels(top + TROPHY_BOTTOM),
                    QuarterPixels(
                        TROPHY_CENTER_X + f_BannerX + (f_TrophySizes[courseIndex][TROPHY_WIDTH] / 2)
                    ),
                    QuarterPixels(
                        TROPHY_BOTTOM + top + f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS + i]
                    ),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                top += f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS + i];
            }

            // The trophy again, into the depth buffer alone
            gDPPipeSync(gfx++);
            gDPSetDepthSource(gfx++, G_ZS_PRIM);
            gDPSetPrimDepth(gfx++, NOTICE_TITLE_DEPTH, 0);
            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                TEXEL0,
                0,
                1,
                TEXEL0,
                1,
                0,
                0,
                0,
                TEXEL0,
                0,
                1,
                TEXEL0,
                1
            );
            gDPPipeSync(gfx++);
            gDPSetRenderMode(
                gfx++,
                GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
                G_RM_AA_ZB_TEX_EDGE2
            );
            top = -f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS] -
                f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS + 1];
            for (i = 0; i != TROPHY_PART_COUNT; i++) {
                gDPLoadTextureTile(
                    gfx++,
                    TextureAddress(
                        AddressNumber(f_TrophyImage) +
                        (i * f_TrophySizes[courseIndex][TROPHY_WIDTH] *
                            f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS])
                    ),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    f_TrophySizes[courseIndex][TROPHY_WIDTH],
                    f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS + i],
                    0,
                    0,
                    f_TrophySizes[courseIndex][TROPHY_WIDTH] - 1,
                    f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS + i] - 1,
                    0,
                    0,
                    0,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(
                        TROPHY_CENTER_X + f_BannerX - (f_TrophySizes[courseIndex][TROPHY_WIDTH] / 2)
                    ),
                    QuarterPixels(top + TROPHY_BOTTOM),
                    QuarterPixels(
                        TROPHY_CENTER_X + f_BannerX + (f_TrophySizes[courseIndex][TROPHY_WIDTH] / 2)
                    ),
                    QuarterPixels(
                        TROPHY_BOTTOM + top + f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS + i]
                    ),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                top += f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS + i];
            }

            gDPSetPrimDepth(gfx++, NOTICE_SHINE_DEPTH, 0);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_NONE);
            gDPPipeSync(gfx++);
            gDPSetAlphaDither(gfx++, G_AD_PATTERN);
            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                PRIMITIVE,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                0,
                0,
                0,
                PRIMITIVE,
                PRIMITIVE,
                0,
                TEXEL0,
                0
            );
            gDPPipeSync(gfx++);
            gDPSetAlphaCompare(gfx++, G_AC_NONE);
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, TROPHY_SHINE_TEXT_ALPHA);
            LoadTextureBlock(
                &gfx,
                ImageBody(f_ResultsShineImage),
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                SHINE_SIZE,
                SHINE_SIZE,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            if (f_TrophyShineFrame == TROPHY_SHINE_SOUND_FRAME) {
                PlaySoundWithEffects(
                    SOUND_SHRINKING_IMAGE,
                    TROPHY_SHINE_SOUND_CHANNEL,
                    TROPHY_SHINE_SOUND_VOLUME,
                    TROPHY_SHINE_SOUND_REVERB,
                    TROPHY_SHINE_SOUND_PAN
                );
            }

            if (f_TrophyShineFrame >= TROPHY_SHINE_START) {
                if (f_TrophyShineFrame < TROPHY_SHINE_CYCLE) {
                    // The trophy's height, its two parts stacked
                    trophyHeight = f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS] +
                        f_TrophySizes[courseIndex][TROPHY_PART_HEIGHTS + 1];
                    DrawTextureRectangle(
                        &gfx,
                        QuarterPixels(
                            TROPHY_CENTER_X + f_BannerX -
                            (f_TrophySizes[courseIndex][TROPHY_WIDTH] / 2)
                        ),
                        QuarterPixels(TROPHY_BOTTOM - trophyHeight),
                        QuarterPixels(
                            TROPHY_CENTER_X + f_BannerX +
                            (f_TrophySizes[courseIndex][TROPHY_WIDTH] / 2)
                        ),
                        QuarterPixels(
                            TROPHY_BOTTOM - trophyHeight +
                            f_TrophySizes[courseIndex][TROPHY_SHINE_HEIGHT]
                        ),
                        G_TX_RENDERTILE,
                        TexelCoordinate(
                            SHINE_SIZE -
                            ((f_TrophyShineFrame - TROPHY_SHINE_START) * TROPHY_SHINE_SPEED)
                        ),
                        0,
                        TROPHY_SHINE_DSDX,
                        TEXEL_STEP_ONE
                    );
                }
            }

            gDPPipeSync(gfx++);
            gDPSetDepthSource(gfx++, G_ZS_PIXEL);
        }
    }

    *displayListHead = gfx;
}

// f_ResultsStep values while UpdateTournamentResults runs
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

// Counts the tournament prize, then the earned course points, into the save,
// showing a notice when they open a course, then returns TRUE once A is pressed
Boolean UpdateTournamentResults(ControllerState *controller) {
    EncodedAsset asset;
    CourseStats *courseStats;
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
                f_PointsToNextCourse + f_PrizePoints + f_CoursePointsTally.earned == 0) {
                StartFaceAnimation(FACE_ANIMATION_PLEASED, 0);
                StartResultsNotice(UNLOCK_NOTICE_KIND_COURSE);
                OpenEncodedAsset(g_TournamentIconAssets[f_OpenCourseCount], &asset);
                ReadEncodedAsset(f_ResultsCourseImage, &asset);
                OpenEncodedAsset(ASSET_COURSE_NOTICE_TITLE, &asset);
                ReadEncodedAsset(f_NoticeTitleImage, &asset);
                return FALSE;
            }

            switch (f_ResultsStep) {
                case TOURNAMENT_STEP_START:
                    courseStats = &GetSaveData()->records.courseStats[g_GameState.courseIndex];
                    f_ResultsStep = TOURNAMENT_STEP_SHOWN;
                    if (courseStats->roundCount <= 0 ||
                        courseStats->bestScore >= f_PlayerResults[0]) {
                        PlaySound(
                            SOUND_NEW_BEST_SCORE,
                            NEW_BEST_SCORE_SOUND_CHANNEL,
                            SOUND_FULL_SCALE
                        );
                    }

                    f_IsAwaitingConfirm = TRUE;
                    break;
                case TOURNAMENT_STEP_SHOWN:
                    if (controller->buttons & A_BUTTON) {
                        f_IsAwaitingConfirm = FALSE;
                        SetResultsBanner(BANNER_SLIDING_IN, BANNER_KIND_TOURNAMENT_PLACE);
                        PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL, CONFIRM_SOUND_VOLUME);
                        f_ResultsStep = TOURNAMENT_STEP_SLIDING_IN_PRIZE;
                    }

                    break;
                case TOURNAMENT_STEP_SLIDING_IN_PRIZE:
                    if (GetBannerState() == BANNER_SHOWN) {
                        f_ResultsStep = TOURNAMENT_STEP_TALLYING_PRIZE;
                        if (f_PlayerRanks[0] == 1) {
                            StartFaceAnimation(FACE_ANIMATION_PLEASED, 0);
                            PlayCharacterSound(
                                g_Players[0].characterIndex,
                                0,
                                CHARACTER_SOUND_WIN,
                                SOUND_FULL_SCALE,
                                WIN_SOUND_PAN
                            );
                        }
                    }

                    break;
                case TOURNAMENT_STEP_TALLYING_PRIZE:
                    if (controller->buttons & A_BUTTON) {
                        f_TallySpeed = FAST_TALLY_SPEED;
                    }
                    {
                        // Through a pointer, to match: naming f_PrizePoints directly
                        // gives 117 words differing, 7 in shape
                        s32 *points = &f_PrizePoints;

                        *points -= f_TallySpeed;
                        if (*points > 0) {
                            PlayTallySound();
                        }

                        if (f_OpenCourseCount == COURSE_COUNT) {
                            *points = 0;
                            f_ResultsStep = TOURNAMENT_STEP_PRIZE_TALLIED;
                            if (f_TournamentOpenCourseCount < COURSE_COUNT) {
                                PlaySound(SOUND_TALLY_DONE, g_NextSoundChannel, SOUND_FULL_SCALE);
                            }

                            break;
                        }

                        // Stop on the points that open the next course, so that its notice
                        // shows
                        overshoot = f_PointsToNextCourse + *points + f_CoursePointsTally.earned;
                        if (overshoot < 0) {
                            *points -= overshoot;
                        }

                        if (*points < 0) {
                            f_ResultsStep = TOURNAMENT_STEP_PRIZE_TALLIED;
                            *points = 0;
                            f_IsAwaitingConfirm = TRUE;
                            if (f_TournamentOpenCourseCount < COURSE_COUNT) {
                                PlaySound(SOUND_TALLY_DONE, g_NextSoundChannel, SOUND_FULL_SCALE);
                            }
                        }
                    }

                    break;
                case TOURNAMENT_STEP_PRIZE_TALLIED:
                    if (controller->buttons & A_BUTTON) {
                        PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL, CONFIRM_SOUND_VOLUME);
                        f_TallySpeed = NORMAL_TALLY_SPEED;
                        f_IsAwaitingConfirm = FALSE;
                        f_ResultsStep = TOURNAMENT_STEP_SLIDE_OUT_PRIZE;
                    }

                    // A debugging shortcut to the course notice
                    if ((controller->pressedButtons & B_BUTTON) && g_IsDebugEnabled) {
                        StartResultsNotice(UNLOCK_NOTICE_KIND_COURSE);
                    }

                    break;
                case TOURNAMENT_STEP_SLIDE_OUT_PRIZE:
                    SetResultsBanner(BANNER_SLIDING_OUT, BANNER_KIND_TOURNAMENT_PLACE);
                    f_ResultsStep = TOURNAMENT_STEP_SLIDING_OUT_PRIZE;
                    break;
                case TOURNAMENT_STEP_SLIDING_OUT_PRIZE:
                    if (GetBannerState() == BANNER_HIDDEN) {
                        if (f_CoursePointsTally.earned > 0 && f_OpenCourseCount < COURSE_COUNT) {
                            f_ResultsStep = TOURNAMENT_STEP_SLIDING_IN_POINTS;
                            SetResultsBanner(BANNER_SLIDING_IN, BANNER_KIND_POINTS);
                        }
                        else {
                            f_ResultsStep = TOURNAMENT_STEP_WAITING;
                        }
                    }

                    break;
                case TOURNAMENT_STEP_SLIDING_IN_POINTS:
                    if (GetBannerState() == BANNER_SHOWN) {
                        f_ResultsStep = TOURNAMENT_STEP_TALLYING_POINTS;
                    }

                    break;
                case TOURNAMENT_STEP_TALLYING_POINTS:
                    if (controller->buttons & A_BUTTON) {
                        f_TallySpeed = FAST_TALLY_SPEED;
                    }
                    if (f_CoursePointsTally.earned > 0) {
                        PlayTallySound();
                    }

                    f_CoursePointsTally.earned -= f_TallySpeed;
                    if (f_CoursePointsTally.earned < 0) {
                        f_CoursePointsTally.earned = 0;
                    }

                    if (f_OpenCourseCount == COURSE_COUNT) {
                        f_CoursePointsTally.earned = 0;
                    }
                    else {
                        overshoot =
                            f_PointsToNextCourse + f_PrizePoints + f_CoursePointsTally.earned;
                        if (overshoot < 0) {
                            f_CoursePointsTally.earned -= overshoot;
                        }
                    }

                    if (f_CoursePointsTally.earned == 0) {
                        PlaySound(SOUND_TALLY_DONE, g_NextSoundChannel, SOUND_FULL_SCALE);
                        f_ResultsStep = TOURNAMENT_STEP_POINTS_TALLIED;
                        f_IsAwaitingConfirm = TRUE;
                    }

                    break;
                case TOURNAMENT_STEP_POINTS_TALLIED:
                    if (controller->buttons & A_BUTTON) {
                        PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL, CONFIRM_SOUND_VOLUME);
                        SetResultsBanner(BANNER_SLIDING_OUT, BANNER_KIND_POINTS);
                        f_ResultsStep = TOURNAMENT_STEP_SLIDING_OUT_POINTS;
                    }

                    break;
                case TOURNAMENT_STEP_SLIDING_OUT_POINTS:
                    if (GetBannerState() == BANNER_HIDDEN) {
                        f_IsAwaitingConfirm = TRUE;
                        f_ResultsStep = TOURNAMENT_STEP_WAITING;
                    }

                    break;
                case TOURNAMENT_STEP_WAITING:
                    f_IsAwaitingConfirm = TRUE;
                    if (controller->buttons & A_BUTTON) {
                        PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL, CONFIRM_SOUND_VOLUME);
                        return TRUE;
                    }

                    break;
            }

            break;
    }

    return FALSE;
}

// The steps of ShowScorePassword, in f_ResultsStep
enum ScorePasswordStep {
    SCORE_PASSWORD_STEP_START,
    SCORE_PASSWORD_STEP_SHOWN,
    SCORE_PASSWORD_STEP_ASKING,
    SCORE_PASSWORD_STEP_DONE
};

// Shows the round's score password until the player says they have it. Returns
// TRUE then.
Boolean ShowScorePassword(ControllerState *controller) {
    ScoreReport report;
    // Pads the frame to match; without it 16 words differ
    s32 unused[2] UNUSED;

    GetSaveData();
    f_ResultsStepTime++;
    switch (f_ResultsStep) {
        case SCORE_PASSWORD_STEP_START:
            f_ResultsStep = SCORE_PASSWORD_STEP_SHOWN;
            StartFaceAnimation(FACE_ANIMATION_PLEASED, 0);
            CopyTournamentCode(report.tournamentCode);
            report.characterIndex = g_Players[0].characterIndex;
            report.score = f_PlayerResults[0];
            report.time = g_GameState.stopwatchFrameCount;
            report.gameBoyLevel = g_GameBoyCharacters[g_Players[0].gameBoyCharacterIndex].level;
            EncodeScorePassword(&report, f_ScorePassword);
            f_IsAwaitingConfirm = TRUE;
            break;
        case SCORE_PASSWORD_STEP_SHOWN:
            if (controller->buttons & A_BUTTON) {
                f_IsAwaitingConfirm = FALSE;
                f_ResultsStep = SCORE_PASSWORD_STEP_ASKING;
                OpenPrompt(PROMPT_SCORE_PASSWORD);
                PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL, CONFIRM_SOUND_VOLUME);
            }

            break;
        case SCORE_PASSWORD_STEP_ASKING:
            switch (UpdatePrompt(controller)) {
                case PROMPT_RESULT_YES:
                    f_ResultsStep = SCORE_PASSWORD_STEP_DONE;
                    break;
                case PROMPT_RESULT_NO:
                    f_ResultsStep = SCORE_PASSWORD_STEP_SHOWN;
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

// f_ResultsStep values while UpdateCoursePointsResults runs
enum PointsStep {
    POINTS_STEP_SHOWN,
    POINTS_STEP_SLIDING_IN_TALLY,
    POINTS_STEP_TALLIED,
    POINTS_STEP_SLIDING_OUT_TALLY,
    POINTS_STEP_WAITING,
    POINTS_STEP_TALLYING,
    POINTS_STEP_START_TALLY
};

// Counts the earned course points into the save, showing a notice when they
// open a course, then returns TRUE once A is pressed
//
// When starting the tally, the original reads the player count for the
// portrait loop's entry test through the address of g_GameState.gameMode,
// kept in $a0, though nothing else uses that address. A test of the game mode
// that changes nothing leaves that behind, and the do-while (0) before the
// loop is one. What the original's test was for can't be recovered. Without
// it the count is read from its own address, and 5 words differ, 1 in shape.
// Each part of it is needed:
//
// - The first store to j is dead, but only flow finds that out, so the
//   branch around it lasts until the final jump pass. That pass deletes it as
//   a branch to the next instruction, with the loads of the mode and the
//   constant that feed it (jump.c, delete_computation), but not the address,
//   which the count's read uses by then. Until then the mode and the constant
//   hold $v1 and $v0, so global allocation gives the address $a0. A test
//   against GAME_MODE_STROKE, which needs no constant loaded, or against both
//   GAME_MODE_SKINS_MATCH and GAME_MODE_CLUB_SLOTS, which folds into one range
//   test, leaves the address in $v1: 6 words differ, none in shape.
// - The loop's end note ends the first cse pass's block but not the second's
//   (cse.c, cse_end_of_basic_block), so only the second pass finds the mode's
//   address in a register at the entry test and rewrites the count's as that
//   register plus 4 (cse.c, use_related_value). Where the first pass does the
//   rewriting, the second folds the sum back into the count's own address:
//   in plain braces the same statements give the 5 words.
// - The second store keeps the branch's label inside the loop. Without it, or
//   with it after the loop, jump points the branch past the loop's end note
//   and the first pass follows it there: the 5 words again.
//
// Each of the other 12 modes after GAME_MODE_STROKE matches too, as do a !=
// test, a switch with the one case, stores of the player count, stores to
// overshoot, stores to i with the loop's own left out, and a for (;;) loop
// ending in a break. Storing 1 or 0 in an if and else, equal values in both
// stores, or a conditional expression's value all give the 5 words. Setting
// a pointer to &g_GameState.gameMode in both arms of the if before the loop
// and reading the count as its second word, with no test, gives 6 words,
// none in shape: the address stays in $v0.
//
// Ending the loops on < rather than != gives 37 words (5 in shape) for i's
// loop, 67 (5) for j's and 70 (10) for both.
Boolean UpdateCoursePointsResults(ControllerState *controller) {
    EncodedAsset asset;
    s32 playerIndex;
    s32 otherIndex;
    s32 overshoot;

    GetSaveData();
    f_ResultsStepTime++;
    switch (f_UnlockNoticeKind) {
        case UNLOCK_NOTICE_KIND_COURSE:
            UpdateCourseResultsNotice();
            break;
        case UNLOCK_NOTICE_KIND_CHARACTER:
            break;

        case UNLOCK_NOTICE_KIND_NONE:
            if (f_OpenCourseCount < COURSE_COUNT &&
                f_PointsToNextCourse + f_CoursePointsTally.earned == 0) {
                for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                    StartFaceAnimation(FACE_ANIMATION_PLEASED, playerIndex);
                }

                StartResultsNotice(UNLOCK_NOTICE_KIND_COURSE);
                OpenEncodedAsset(g_TournamentIconAssets[f_OpenCourseCount], &asset);
                ReadEncodedAsset(f_ResultsCourseImage, &asset);
                OpenEncodedAsset(ASSET_COURSE_NOTICE_TITLE, &asset);
                ReadEncodedAsset(f_NoticeTitleImage, &asset);
                return FALSE;
            }

            switch (f_ResultsStep) {
                case POINTS_STEP_SHOWN:
                    f_IsAwaitingConfirm = TRUE;
                    if (controller->buttons & A_BUTTON) {
                        f_IsAwaitingConfirm = FALSE;
                        f_ResultsStep = POINTS_STEP_START_TALLY;
                        PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL, CONFIRM_SOUND_VOLUME);
                    }

                    break;
                case POINTS_STEP_START_TALLY:
                    if (f_CoursePointsTally.earned > 0 && f_OpenCourseCount < COURSE_COUNT) {
                        f_ResultsStep = POINTS_STEP_SLIDING_IN_TALLY;
                        SetResultsBanner(BANNER_SLIDING_IN, BANNER_KIND_POINTS);
                    }
                    else {
                        f_ResultsStep = POINTS_STEP_WAITING;
                    }

                    // Changes nothing, but leaves the game mode's address behind
                    // for the loop's entry test, as the header comment explains
                    IN_OWN_BLOCK({
                        if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH) {
                            otherIndex = 1;
                        }

                        otherIndex = 0;
                    });
                    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                        StartFaceAnimation(FACE_ANIMATION_PLEASED, playerIndex);
                        for (otherIndex = 0; otherIndex != g_GameState.playerCount; otherIndex++) {
                            if (playerIndex != otherIndex &&
                                f_PlayerRanks[playerIndex] > f_PlayerRanks[otherIndex]) {
                                StartFaceAnimation(FACE_ANIMATION_BEATEN, playerIndex);
                            }
                        }
                    }

                    break;
                case POINTS_STEP_SLIDING_IN_TALLY:
                    if (GetBannerState() == BANNER_SHOWN) {
                        f_ResultsStep = POINTS_STEP_TALLYING;
                    }

                    break;
                case POINTS_STEP_TALLYING:
                    if (controller->buttons & A_BUTTON) {
                        f_TallySpeed = FAST_TALLY_SPEED;
                    }
                    if (f_CoursePointsTally.earned > 0) {
                        PlayTallySound();
                    }

                    f_CoursePointsTally.earned -= f_TallySpeed;
                    if (f_CoursePointsTally.earned < 0) {
                        f_CoursePointsTally.earned = 0;
                    }

                    if (f_OpenCourseCount == COURSE_COUNT) {
                        f_CoursePointsTally.earned = 0;
                    }
                    else {
                        // Stop on the points that open the next course, so that its
                        // notice shows
                        overshoot = f_PointsToNextCourse + f_CoursePointsTally.earned;
                        if (overshoot < 0) {
                            f_CoursePointsTally.earned -= overshoot;
                        }
                    }

                    if (f_CoursePointsTally.earned == 0) {
                        f_ResultsStepTime = 0;
                        PlaySound(SOUND_TALLY_DONE, g_NextSoundChannel, SOUND_FULL_SCALE);
                        f_ResultsStep = POINTS_STEP_TALLIED;
                        f_IsAwaitingConfirm = TRUE;
                    }

                    break;
                case POINTS_STEP_TALLIED:
                    if (controller->buttons & A_BUTTON) {
                        SetResultsBanner(BANNER_SLIDING_OUT, BANNER_KIND_POINTS);
                        f_ResultsStep = POINTS_STEP_SLIDING_OUT_TALLY;
                        f_IsAwaitingConfirm = FALSE;
                    }

                    break;
                case POINTS_STEP_SLIDING_OUT_TALLY:
                    if (GetBannerState() == BANNER_HIDDEN) {
                        f_ResultsStep = POINTS_STEP_WAITING;
                    }

                    break;
                case POINTS_STEP_WAITING:
                    f_IsAwaitingConfirm = TRUE;
                    if (controller->pressedButtons & A_BUTTON) {
                        PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL, CONFIRM_SOUND_VOLUME);
                        f_IsAwaitingConfirm = FALSE;
                        return TRUE;
                    }

                    break;
            }

            break;
    }

    return FALSE;
}

// f_ResultsStep values while UpdateMatchResults runs
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
// Whether beating a character in Get Character unlocks them: other feats
// unlock the rest
#define IsUnlockedByBeating(character) \
    ((character) != CHARACTER_PLUM && \
        (character) != CHARACTER_CHARLIE && \
        (character) != CHARACTER_MAPLE && \
        (character) != CHARACTER_BABY_MARIO && \
        (character) != CHARACTER_PEACH && \
        (character) != CHARACTER_DONKEY_KONG && \
        (character) < CHARACTER_METAL_MARIO)

// Counts the earned course points into the save, then in Get Character
// records the match and unlocks a beaten opponent, returning TRUE once
// A is pressed
Boolean UpdateMatchResults(ControllerState *controller) {
    EncodedAsset asset;
    s32 playerIndex;
    s32 overshoot;

    GetSaveData();
    f_ResultsStepTime++;
    switch (f_UnlockNoticeKind) {
        case UNLOCK_NOTICE_KIND_COURSE:
            UpdateCourseResultsNotice();
            break;
        case UNLOCK_NOTICE_KIND_CHARACTER:
            UpdateCharacterResultsNotice();
            break;
        case UNLOCK_NOTICE_KIND_NONE:
            if (f_OpenCourseCount < COURSE_COUNT &&
                f_PointsToNextCourse + f_CoursePointsTally.earned == 0) {
                for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                    StartFaceAnimation(FACE_ANIMATION_PLEASED, playerIndex);
                }

                StartResultsNotice(UNLOCK_NOTICE_KIND_COURSE);
                OpenEncodedAsset(g_TournamentIconAssets[f_OpenCourseCount], &asset);
                ReadEncodedAsset(f_ResultsCourseImage, &asset);
                OpenEncodedAsset(ASSET_COURSE_NOTICE_TITLE, &asset);
                ReadEncodedAsset(f_NoticeTitleImage, &asset);
                return FALSE;
            }

            switch (f_ResultsStep) {
                case MATCH_STEP_SHOWN:
                    f_IsAwaitingConfirm = TRUE;
                    if (controller->buttons & A_BUTTON) {
                        f_IsAwaitingConfirm = FALSE;
                        f_ResultsStep = MATCH_STEP_START_TALLY;
                        PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL, CONFIRM_SOUND_VOLUME);
                    }

                    break;
                case MATCH_STEP_START_TALLY:
                    if (f_CoursePointsTally.earned > 0 && f_OpenCourseCount < COURSE_COUNT) {
                        f_ResultsStep = MATCH_STEP_SLIDING_IN_TALLY;
                        SetResultsBanner(BANNER_SLIDING_IN, BANNER_KIND_POINTS);
                    }
                    else {
                        f_ResultsStep = MATCH_STEP_RECORD_RESULT;
                    }

                    if (f_PlayerRanks[0] == MATCH_RANK_WON) {
                        StartFaceAnimation(FACE_ANIMATION_PLEASED, 0);
                        StartFaceAnimation(FACE_ANIMATION_BEATEN, 1);
                    }
                    else {
                        StartFaceAnimation(FACE_ANIMATION_PLEASED, 1);
                        StartFaceAnimation(FACE_ANIMATION_BEATEN, 0);
                    }

                    break;
                case MATCH_STEP_SLIDING_IN_TALLY:
                    if (GetBannerState() == BANNER_SHOWN) {
                        f_ResultsStep = MATCH_STEP_TALLYING;
                    }

                    break;
                case MATCH_STEP_TALLYING:
                    if (controller->buttons & A_BUTTON) {
                        f_TallySpeed = FAST_TALLY_SPEED;
                    }
                    if (f_CoursePointsTally.earned > 0) {
                        PlayTallySound();
                    }

                    f_CoursePointsTally.earned -= f_TallySpeed;
                    if (f_CoursePointsTally.earned < 0) {
                        f_CoursePointsTally.earned = 0;
                    }

                    if (f_OpenCourseCount == COURSE_COUNT) {
                        f_CoursePointsTally.earned = 0;
                    }
                    else {
                        // Stop on the points that open the next course, so that its
                        // notice shows
                        overshoot = f_PointsToNextCourse + f_CoursePointsTally.earned;
                        if (overshoot < 0) {
                            f_CoursePointsTally.earned -= overshoot;
                        }
                    }

                    if (f_CoursePointsTally.earned == 0) {
                        f_ResultsStep = MATCH_STEP_TALLIED;
                        PlaySound(SOUND_TALLY_DONE, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }

                    break;
                case MATCH_STEP_TALLIED:
                    f_IsAwaitingConfirm = TRUE;
                    if (controller->buttons & A_BUTTON) {
                        SetResultsBanner(BANNER_SLIDING_OUT, BANNER_KIND_POINTS);
                        f_ResultsStep = MATCH_STEP_SLIDING_OUT_TALLY;
                        f_IsAwaitingConfirm = FALSE;
                    }

                    break;
                case MATCH_STEP_SLIDING_OUT_TALLY:
                    if (GetBannerState() == BANNER_HIDDEN) {
                        f_ResultsStep = MATCH_STEP_RECORD_RESULT;
                    }

                    break;
                case MATCH_STEP_RECORD_RESULT:
                    f_ResultsStep = MATCH_STEP_WAITING;
                    f_ResultsStepTime = 0;
                    if (g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
                        // Beating a still locked opponent unlocks them
                        if (f_PlayerRanks[0] == MATCH_RANK_WON &&
                            FindCharacterOnGrid(g_Players[1].characterIndex) ==
                                CHARACTER_OFF_GRID &&
                            IsUnlockedByBeating(g_Players[1].characterIndex)) {
                            f_ResultsStep = MATCH_STEP_SHOW_CHARACTER_NOTICE;
                        }

                        RecordComputerMatchupResult(
                            g_Players[0].characterIndex,
                            g_Players[1].characterIndex,
                            GetMatchWinner(0, 1) == MATCH_SIDE_PLAYER
                        );
                    }

                    break;
                case MATCH_STEP_SHOW_CHARACTER_NOTICE: {
                    EncodedAsset titleAsset;

                    StartResultsNotice(UNLOCK_NOTICE_KIND_CHARACTER);
                    OpenEncodedAsset(ASSET_CHARACTER_NOTICE_TITLE, &titleAsset);
                    ReadEncodedAsset(f_NoticeTitleImage, &titleAsset);
                    f_ResultsStep = MATCH_STEP_START_NOTICE_WAIT;
                    break;
                }
                case MATCH_STEP_START_NOTICE_WAIT:
                    f_ResultsStep = MATCH_STEP_NOTICE_WAIT;
                    f_ResultsStepTime = 0;
                    break;
                case MATCH_STEP_NOTICE_WAIT:
                    if (f_ResultsStepTime == CHARACTER_NOTICE_WAIT_FRAMES) {
                        f_ResultsStep = MATCH_STEP_WAITING;
                    }

                    break;
                case MATCH_STEP_WAITING:
                    f_IsAwaitingConfirm = TRUE;

                    // A debugging aid: up and down choose a character, and B shows its
                    // notice
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
                        PrintDebugText(
                            g_PrintBuffer,
                            DEBUG_UNLOCK_CHOICE_COLUMN,
                            DEBUG_UNLOCK_CHOICE_ROW
                        );
                        if (controller->pressedButtons & B_BUTTON) {
                            EncodedAsset faceAsset;
                            AssetFile file;

                            StartResultsNotice(UNLOCK_NOTICE_KIND_CHARACTER);
                            g_Players[1].characterIndex = f_DebugUnlockCharacter;
                            f_FaceCharacterIndex = (s8)f_DebugUnlockCharacter;
                            OpenEncodedAsset(
                                g_CharacterFirstFaceAssets[(s8)f_DebugUnlockCharacter],
                                &faceAsset
                            );
                            ReadEncodedAsset(f_FaceImage, &faceAsset);
                            OpenAssetFile(
                                g_CharacterFirstPaletteFiles[g_Players[1].characterIndex] +
                                    g_Players[1].paletteIndex,
                                &file
                            );
                            ReadAssetFile(f_FacePalette, GetAssetFileSize(&file), &file);
                            OpenEncodedAsset(ASSET_CHARACTER_NOTICE_TITLE, &faceAsset);
                            ReadEncodedAsset(f_NoticeTitleImage, &faceAsset);
                            f_ResultsStep = MATCH_STEP_SLIDING_OUT_TALLY;
                        }
                    }

                    if (controller->buttons & A_BUTTON) {
                        PlaySound(SOUND_MENU_CONFIRM, CONFIRM_SOUND_CHANNEL, CONFIRM_SOUND_VOLUME);
                        f_IsAwaitingConfirm = FALSE;
                        return TRUE;
                    }

                    break;
            }

            break;
    }

    return FALSE;
}

// Where the tournament results draw their parts
#define TOURNAMENT_PLACE_X 40
#define TOURNAMENT_FACE_X 100
#define TOURNAMENT_ICON_Y 80
#define TOURNAMENT_SCORE_LABEL_X 144
#define TOURNAMENT_POINTS_LABEL_X 232
#define TOURNAMENT_LABEL_Y 60
#define TOURNAMENT_SCORE_X 152
#define TOURNAMENT_POINTS_X 232
#define TOURNAMENT_NUMBER_Y 88

// The tournament results: place, portrait, score, Course Points and the points
// to the next course
void DrawTournamentResults(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    DrawRank(
        &gfx,
        f_ResultsSlide + TOURNAMENT_PLACE_X,
        f_ResultsTop + TOURNAMENT_ICON_Y,
        (f_PlayerRanks[0] - 1) | RANK_AS_PLACE
    );
    DrawPlayerFace(
        &gfx,
        f_ResultsSlide + TOURNAMENT_FACE_X,
        f_ResultsTop + TOURNAMENT_ICON_Y,
        0,
        FALSE
    );
    DrawResultsLabel(
        &gfx,
        f_ResultsSlide + TOURNAMENT_SCORE_LABEL_X,
        f_ResultsTop + TOURNAMENT_LABEL_Y,
        RESULTS_LABEL_SCORE
    );
    DrawResultsLabel(
        &gfx,
        f_ResultsSlide + TOURNAMENT_POINTS_LABEL_X,
        f_ResultsTop + TOURNAMENT_LABEL_Y,
        RESULTS_LABEL_POINTS
    );
    if (f_OpenCourseCount == COURSE_COUNT) {
        SetTextSlot(
            f_ResultsSlide + ALL_COURSES_OPEN_X,
            f_ResultsTop + SOLO_ALL_COURSES_OPEN_Y,
            MESSAGE_ALL_COURSES_OPEN,
            TEXT_SLOT_LARGE_FONT,
            RESULTS_TEXT_SLOT,
            0,
            RESULTS_TEXT_RED,
            RESULTS_TEXT_GREEN,
            RESULTS_TEXT_BLUE,
            RESULTS_TEXT_ALPHA
        );
    }
    else {
        DrawResultsNumber(
            &gfx,
            f_ResultsSlide + POINTS_TOTAL_X,
            POINTS_TOTAL_Y,
            f_PointsToNextCourse + f_PrizePoints + f_CoursePointsTally.earned
        );
        SetTextSlot(
            f_ResultsSlide + POINTS_TO_NEXT_COURSE_X,
            f_ResultsTop + POINTS_TO_NEXT_COURSE_Y,
            MESSAGE_MORE_POINTS_TO_NEXT_COURSE,
            TEXT_SLOT_LARGE_FONT,
            RESULTS_TEXT_SLOT,
            0,
            RESULTS_TEXT_RED,
            RESULTS_TEXT_GREEN,
            RESULTS_TEXT_BLUE,
            RESULTS_TEXT_ALPHA
        );
    }

    LoadSmallPalette(&gfx, 0, ImagePalette(f_ResultsLargeDigitsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(f_ResultsLargeDigitsImage),
        G_IM_FMT_CI,
        DIGIT_GLYPH_SIZE,
        DIGIT_FONT_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    if (f_PlayerResults[0] == 0) {
        sprintf(g_PrintBuffer, " #0", 0);
    }
    else {
        sprintf(g_PrintBuffer, "%+3d", f_PlayerResults[0]);
    }

    DrawDigitText(
        &gfx,
        f_ResultsSlide + TOURNAMENT_SCORE_X,
        f_ResultsTop + TOURNAMENT_NUMBER_Y,
        g_PrintBuffer
    );
    sprintf(g_PrintBuffer, "%3d", f_PlayerPoints[0]);
    DrawDigitText(
        &gfx,
        f_ResultsSlide + TOURNAMENT_POINTS_X,
        f_ResultsTop + TOURNAMENT_NUMBER_Y,
        g_PrintBuffer
    );
    *displayListHead = gfx;
}

#define RESULTS_HEADING_ENV_COLOR 0xFFFF0000
#define RESULTS_HEADING_PRIM_COLOR 0x30FF0000

// Where the Speed Golf results draw their parts: the time and the strokes
// each sit under a heading
#define SPEED_GOLF_FACE_X 40
#define SPEED_GOLF_FACE_Y 88
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

// The Speed Golf results: portrait, time, strokes, Course Points and the points
// to the next course
void DrawSpeedGolfResults(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 seconds;
    s32 hundredths;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    DrawPlayerFace(
        &gfx,
        f_ResultsSlide + SPEED_GOLF_FACE_X,
        f_ResultsTop + SPEED_GOLF_FACE_Y,
        0,
        FALSE
    );
    DrawResultsLabel(
        &gfx,
        f_ResultsSlide + SPEED_GOLF_SCORE_LABEL_X,
        f_ResultsTop + SPEED_GOLF_LABEL_Y,
        RESULTS_LABEL_SCORE
    );
    DrawResultsLabel(
        &gfx,
        f_ResultsSlide + SPEED_GOLF_POINTS_LABEL_X,
        f_ResultsTop + SPEED_GOLF_LABEL_Y,
        RESULTS_LABEL_POINTS
    );
    if (f_OpenCourseCount == COURSE_COUNT) {
        SetTextSlot(
            f_ResultsSlide + ALL_COURSES_OPEN_X,
            f_ResultsTop + SOLO_ALL_COURSES_OPEN_Y,
            MESSAGE_ALL_COURSES_OPEN,
            TEXT_SLOT_LARGE_FONT,
            RESULTS_TEXT_SLOT,
            0,
            RESULTS_TEXT_RED,
            RESULTS_TEXT_GREEN,
            RESULTS_TEXT_BLUE,
            RESULTS_TEXT_ALPHA
        );
    }
    else {
        DrawResultsNumber(
            &gfx,
            f_ResultsSlide + POINTS_TOTAL_X,
            POINTS_TOTAL_Y,
            f_PointsToNextCourse + f_PrizePoints + f_CoursePointsTally.earned
        );
        SetTextSlot(
            f_ResultsSlide + POINTS_TO_NEXT_COURSE_X,
            f_ResultsTop + POINTS_TO_NEXT_COURSE_Y,
            MESSAGE_MORE_POINTS_TO_NEXT_COURSE,
            TEXT_SLOT_LARGE_FONT,
            RESULTS_TEXT_SLOT,
            0,
            RESULTS_TEXT_RED,
            RESULTS_TEXT_GREEN,
            RESULTS_TEXT_BLUE,
            RESULTS_TEXT_ALPHA
        );
    }

    LoadSmallPalette(&gfx, 0, ImagePalette(f_ResultsLargeDigitsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(f_ResultsLargeDigitsImage),
        G_IM_FMT_CI,
        DIGIT_GLYPH_SIZE,
        DIGIT_FONT_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    if (f_PlayerResults[0] == 0) {
        sprintf(g_PrintBuffer, " #0", 0);
    }
    else {
        sprintf(g_PrintBuffer, "%+3d", f_PlayerResults[0]);
    }

    DrawDigitText(
        &gfx,
        f_ResultsSlide + SPEED_GOLF_VALUE_X,
        f_ResultsTop + SPEED_GOLF_SCORE_Y,
        g_PrintBuffer
    );
    seconds = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND;
    hundredths =
        (s32)(g_GameState.stopwatchFrameCount * ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
        HUNDREDTHS_PER_SECOND;
    sprintf(
        g_PrintBuffer,
        "%02d:%02d:%02d",
        seconds / SECONDS_PER_MINUTE % SHOWN_MINUTES_LIMIT,
        seconds % SECONDS_PER_MINUTE,
        hundredths
    );
    DrawDigitText(
        &gfx,
        f_ResultsSlide + SPEED_GOLF_VALUE_X,
        f_ResultsTop + SPEED_GOLF_TIME_Y,
        g_PrintBuffer
    );
    sprintf(g_PrintBuffer, "%3d", f_PlayerPoints[0]);
    DrawDigitText(
        &gfx,
        f_ResultsSlide + SPEED_GOLF_POINTS_X,
        f_ResultsTop + SPEED_GOLF_POINTS_Y,
        g_PrintBuffer
    );
    SetUpGradientTextDrawing(&gfx, 0);
    sprintf(g_PrintBuffer, "Time");
    DrawLargeTextWithShadow(
        &gfx,
        f_ResultsSlide + SPEED_GOLF_HEADING_X,
        f_ResultsTop + SPEED_GOLF_TIME_HEADING_Y,
        g_PrintBuffer,
        RESULTS_HEADING_ENV_COLOR,
        RESULTS_HEADING_PRIM_COLOR,
        0
    );
    sprintf(g_PrintBuffer, "Stroke");
    DrawLargeTextWithShadow(
        &gfx,
        f_ResultsSlide + SPEED_GOLF_HEADING_X,
        f_ResultsTop + SPEED_GOLF_STROKE_HEADING_Y,
        g_PrintBuffer,
        RESULTS_HEADING_ENV_COLOR,
        RESULTS_HEADING_PRIM_COLOR,
        0
    );
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#define RESULTS_ROW_HEIGHT 32
#define RESULTS_ICON_ROW_Y 68
#define RESULTS_TEXT_ROW_Y 80
// The players' results columns. With bets, places and a bet totals column
// squeeze the others left; without them, ranks lead the rows.
#define BETTING_RESULTS_SCORE_LABEL_X 88
#define BETTING_RESULTS_BETS_LABEL_X 192
#define BETTING_RESULTS_POINTS_LABEL_X 248
#define BETTING_RESULTS_PLACE_X 18
#define BETTING_RESULTS_FACE_X 50
#define BETTING_RESULTS_SCORE_X 96
#define BETTING_RESULTS_BET_TOTAL_X 156
#define BETTING_RESULTS_POINTS_X 242
#define PLAIN_RESULTS_SCORE_LABEL_X 144
#define PLAIN_RESULTS_POINTS_LABEL_X 234
#define PLAIN_RESULTS_RANK_X 32
#define PLAIN_RESULTS_FACE_X 96
#define PLAIN_RESULTS_SCORE_X 152
#define PLAIN_RESULTS_POINTS_X 232

// The results of several players: place, portrait, score, any bet totals and
// Course Points
void DrawPlayersResults(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 betShift = FOUR_DIGIT_BET_SHIFT;
    s32 playerIndex;

    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        if (f_PlayerBetTotals[playerIndex] > FOUR_DIGIT_BET_MAX) {
            betShift = 0;
        }

        if (f_PlayerBetTotals[playerIndex] < FOUR_DIGIT_BET_MIN) {
            betShift = 0;
        }
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    if (f_HasBets) {
        DrawResultsLabel(
            &gfx,
            f_ResultsSlide + BETTING_RESULTS_SCORE_LABEL_X,
            f_ResultsTop + RESULTS_HEADING_Y,
            RESULTS_LABEL_SCORE
        );
        DrawResultsLabel(
            &gfx,
            f_ResultsSlide + BETTING_RESULTS_BETS_LABEL_X,
            f_ResultsTop + RESULTS_HEADING_Y,
            RESULTS_LABEL_BETS
        );
        DrawResultsLabel(
            &gfx,
            f_ResultsSlide + BETTING_RESULTS_POINTS_LABEL_X,
            f_ResultsTop + RESULTS_HEADING_Y,
            RESULTS_LABEL_POINTS
        );
        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            if (g_GameState.playerCount >= 2) {
                DrawPlaceIcon(
                    &gfx,
                    f_ResultsSlide + BETTING_RESULTS_PLACE_X,
                    f_ResultsTop + RESULTS_ICON_ROW_Y + (playerIndex * RESULTS_ROW_HEIGHT),
                    f_PlayerRanks[playerIndex]
                );
            }

            DrawPlayerFace(
                &gfx,
                f_ResultsSlide + BETTING_RESULTS_FACE_X,
                f_ResultsTop + RESULTS_ICON_ROW_Y + (playerIndex * RESULTS_ROW_HEIGHT),
                playerIndex,
                FALSE
            );
        }
    }
    else {
        DrawResultsLabel(
            &gfx,
            f_ResultsSlide + PLAIN_RESULTS_SCORE_LABEL_X,
            f_ResultsTop + RESULTS_HEADING_Y,
            RESULTS_LABEL_SCORE
        );
        DrawResultsLabel(
            &gfx,
            f_ResultsSlide + PLAIN_RESULTS_POINTS_LABEL_X,
            f_ResultsTop + RESULTS_HEADING_Y,
            RESULTS_LABEL_POINTS
        );
        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            if (g_GameState.playerCount >= 2) {
                DrawRank(
                    &gfx,
                    f_ResultsSlide + PLAIN_RESULTS_RANK_X,
                    f_ResultsTop + RESULTS_ICON_ROW_Y + (playerIndex * RESULTS_ROW_HEIGHT),
                    f_PlayerRanks[playerIndex]
                );
            }

            DrawPlayerFace(
                &gfx,
                f_ResultsSlide + PLAIN_RESULTS_FACE_X,
                f_ResultsTop + RESULTS_ICON_ROW_Y + (playerIndex * RESULTS_ROW_HEIGHT),
                playerIndex,
                FALSE
            );
        }
    }

    LoadSmallPalette(&gfx, 0, ImagePalette(f_ResultsLargeDigitsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(f_ResultsLargeDigitsImage),
        G_IM_FMT_CI,
        DIGIT_GLYPH_SIZE,
        DIGIT_FONT_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    if (f_HasBets) {
        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
                sprintf(g_PrintBuffer, "%3d", f_PlayerResults[playerIndex]);
            }
            else if (f_PlayerResults[playerIndex] == 0) {
                sprintf(g_PrintBuffer, " #0", 0);
            }
            else {
                sprintf(g_PrintBuffer, "%+3d", f_PlayerResults[playerIndex]);
            }

            DrawDigitText(
                &gfx,
                f_ResultsSlide + BETTING_RESULTS_SCORE_X,
                f_ResultsTop + RESULTS_TEXT_ROW_Y + (playerIndex * RESULTS_ROW_HEIGHT),
                g_PrintBuffer
            );
            sprintf(g_PrintBuffer, "%5d", (int)f_PlayerBetTotals[playerIndex]);
            DrawDigitText(
                &gfx,
                f_ResultsSlide + BETTING_RESULTS_BET_TOTAL_X + betShift,
                f_ResultsTop + RESULTS_TEXT_ROW_Y + (playerIndex * RESULTS_ROW_HEIGHT),
                g_PrintBuffer
            );
            if (f_PlayerPoints[playerIndex] < 0) {
                sprintf(g_PrintBuffer, "  -", f_PlayerPoints[playerIndex]);
            }
            else {
                sprintf(g_PrintBuffer, "%3d", f_PlayerPoints[playerIndex]);
            }

            DrawDigitText(
                &gfx,
                f_ResultsSlide + BETTING_RESULTS_POINTS_X,
                f_ResultsTop + RESULTS_TEXT_ROW_Y + (playerIndex * RESULTS_ROW_HEIGHT),
                g_PrintBuffer
            );
        }
    }
    else {
        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
                sprintf(g_PrintBuffer, "%3d", f_PlayerResults[playerIndex]);
            }
            else if (f_PlayerResults[playerIndex] == 0) {
                sprintf(g_PrintBuffer, " #0", 0);
            }
            else {
                sprintf(g_PrintBuffer, "%+3d", f_PlayerResults[playerIndex]);
            }

            DrawDigitText(
                &gfx,
                f_ResultsSlide + PLAIN_RESULTS_SCORE_X,
                f_ResultsTop + RESULTS_TEXT_ROW_Y + (playerIndex * RESULTS_ROW_HEIGHT),
                g_PrintBuffer
            );
            if (f_PlayerPoints[playerIndex] < 0) {
                sprintf(g_PrintBuffer, "  -", f_PlayerPoints[playerIndex]);
            }
            else {
                sprintf(g_PrintBuffer, "%3d", f_PlayerPoints[playerIndex]);
            }

            DrawDigitText(
                &gfx,
                f_ResultsSlide + PLAIN_RESULTS_POINTS_X,
                f_ResultsTop + RESULTS_TEXT_ROW_Y + (playerIndex * RESULTS_ROW_HEIGHT),
                g_PrintBuffer
            );
        }
    }

    if (f_OpenCourseCount == COURSE_COUNT) {
        SetTextSlot(
            f_ResultsSlide + ALL_COURSES_OPEN_X,
            f_ResultsTop + PLAYERS_ALL_COURSES_OPEN_Y,
            MESSAGE_ALL_COURSES_OPEN,
            TEXT_SLOT_LARGE_FONT,
            RESULTS_TEXT_SLOT,
            0,
            RESULTS_TEXT_RED,
            RESULTS_TEXT_GREEN,
            RESULTS_TEXT_BLUE,
            RESULTS_TEXT_ALPHA
        );
    }
    else {
        DrawResultsNumber(
            &gfx,
            f_ResultsSlide + POINTS_TOTAL_X,
            POINTS_TOTAL_Y,
            f_PointsToNextCourse + f_CoursePointsTally.earned
        );
        SetTextSlot(
            f_ResultsSlide + POINTS_TO_NEXT_COURSE_X,
            f_ResultsTop + POINTS_TO_NEXT_COURSE_Y,
            MESSAGE_MORE_POINTS_TO_NEXT_COURSE,
            TEXT_SLOT_LARGE_FONT,
            RESULTS_TEXT_SLOT,
            0,
            RESULTS_TEXT_RED,
            RESULTS_TEXT_GREEN,
            RESULTS_TEXT_BLUE,
            RESULTS_TEXT_ALPHA
        );
    }

    *displayListHead = gfx;
}

// Where the match results draw their parts, a row of MATCH_ROW_HEIGHT for
// each player. With bets, the columns sit further left to make room for the
// bet totals. Without them, the points step down a shorter row.
#define MATCH_ROW_HEIGHT 72
#define MATCH_HEADING_Y 62
#define MATCH_RANK_Y 48
#define MATCH_FACE_Y 76
#define MATCH_NUMBER_Y 92
#define MATCH_STANDING_Y 108
#define BETTING_MATCH_BETS_LABEL_X 152
#define BETTING_MATCH_POINTS_LABEL_X 240
#define BETTING_MATCH_RANK_X 40
#define BETTING_MATCH_FACE_X 62
#define BETTING_MATCH_HOLE_COUNT_X 48
#define BETTING_MATCH_STANDING_X 80
#define BETTING_MATCH_BET_TOTAL_X 128
#define BETTING_MATCH_POINTS_X 234
#define PLAIN_MATCH_POINTS_LABEL_X 220
#define PLAIN_MATCH_RANK_X 56
#define PLAIN_MATCH_FACE_X 78
#define PLAIN_MATCH_HOLE_COUNT_X 64
#define PLAIN_MATCH_STANDING_X 96
#define PLAIN_MATCH_POINTS_X 218
#define PLAIN_MATCH_POINTS_ROW_HEIGHT 64
// Hole counts from this one take two digits
#define TWO_DIGIT_HOLE_COUNT_MIN 10
// The standing words are 48x48, holding UP, DOWN and EVEN in rows of 16
#define STANDING_WORDS_SIZE 48
#define STANDING_WORD_WIDTH 48
#define STANDING_WORD_HEIGHT 16
#define STANDING_WORD_UP_ROW 0
#define STANDING_WORD_DOWN_ROW 16
#define STANDING_WORD_EVEN_ROW 32
// Shifts the hole count and its word left, so that the pair stays centered
#define STANDING_ONE_DIGIT_UP_SHIFT (-8)
#define STANDING_ONE_DIGIT_DOWN_SHIFT (-20)
#define STANDING_TWO_DIGIT_DOWN_SHIFT (-12)

// The match play results of two players: place, portrait, holes up or down,
// any bets and Course Points.
void DrawMatchResults(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    // Pads the frame to match; without it 110 words differ
    s32 unused[4] UNUSED;
    s32 standingShift[MATCH_PLAYER_COUNT];
    s32 betShift = FOUR_DIGIT_BET_SHIFT;
    s32 standingRow;
    s32 holeCount;
    s32 playerIndex;

    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        if (f_PlayerBetTotals[playerIndex] > FOUR_DIGIT_BET_MAX) {
            betShift = 0;
        }

        if (f_PlayerBetTotals[playerIndex] < FOUR_DIGIT_BET_MIN) {
            betShift = 0;
        }
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    if (f_HasBets) {
        DrawResultsLabel(
            &gfx,
            f_ResultsSlide + BETTING_MATCH_BETS_LABEL_X,
            f_ResultsTop + MATCH_HEADING_Y,
            RESULTS_LABEL_BETS
        );
        DrawResultsLabel(
            &gfx,
            f_ResultsSlide + BETTING_MATCH_POINTS_LABEL_X,
            f_ResultsTop + MATCH_HEADING_Y,
            RESULTS_LABEL_POINTS
        );
        DrawRank(
            &gfx,
            f_ResultsSlide + BETTING_MATCH_RANK_X,
            f_ResultsTop + MATCH_RANK_Y,
            f_PlayerRanks[0]
        );
        DrawRank(
            &gfx,
            f_ResultsSlide + BETTING_MATCH_RANK_X,
            f_ResultsTop + MATCH_RANK_Y + MATCH_ROW_HEIGHT,
            f_PlayerRanks[1]
        );
        DrawPlayerFace(
            &gfx,
            f_ResultsSlide + BETTING_MATCH_FACE_X,
            f_ResultsTop + MATCH_FACE_Y,
            0,
            FALSE
        );
        DrawPlayerFace(
            &gfx,
            f_ResultsSlide + BETTING_MATCH_FACE_X,
            f_ResultsTop + MATCH_FACE_Y + MATCH_ROW_HEIGHT,
            1,
            FALSE
        );
    }
    else {
        DrawResultsLabel(
            &gfx,
            f_ResultsSlide + PLAIN_MATCH_POINTS_LABEL_X,
            f_ResultsTop + MATCH_HEADING_Y,
            RESULTS_LABEL_POINTS
        );
        DrawRank(
            &gfx,
            f_ResultsSlide + PLAIN_MATCH_RANK_X,
            f_ResultsTop + MATCH_RANK_Y,
            f_PlayerRanks[0]
        );
        DrawRank(
            &gfx,
            f_ResultsSlide + PLAIN_MATCH_RANK_X,
            f_ResultsTop + MATCH_RANK_Y + MATCH_ROW_HEIGHT,
            f_PlayerRanks[1]
        );
        DrawPlayerFace(
            &gfx,
            f_ResultsSlide + PLAIN_MATCH_FACE_X,
            f_ResultsTop + MATCH_FACE_Y,
            0,
            FALSE
        );
        DrawPlayerFace(
            &gfx,
            f_ResultsSlide + PLAIN_MATCH_FACE_X,
            f_ResultsTop + MATCH_FACE_Y + MATCH_ROW_HEIGHT,
            1,
            FALSE
        );
    }

    LoadSmallPalette(&gfx, 0, ImagePalette(f_MatchStandingWordsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(f_MatchStandingWordsImage),
        G_IM_FMT_CI,
        STANDING_WORDS_SIZE,
        STANDING_WORDS_SIZE,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        if (f_PlayerResults[playerIndex] > 0) {
            if (f_PlayerResults[playerIndex] >= TWO_DIGIT_HOLE_COUNT_MIN) {
                standingShift[playerIndex] = 0;
            }
            else {
                standingShift[playerIndex] = STANDING_ONE_DIGIT_UP_SHIFT;
            }
        }
        else if (f_PlayerResults[playerIndex] < 0) {
            if (f_PlayerResults[playerIndex] <= -TWO_DIGIT_HOLE_COUNT_MIN) {
                standingShift[playerIndex] = STANDING_TWO_DIGIT_DOWN_SHIFT;
            }
            else {
                standingShift[playerIndex] = STANDING_ONE_DIGIT_DOWN_SHIFT;
            }
        }
    }

    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        if (f_PlayerResults[playerIndex] > 0) {
            standingRow = STANDING_WORD_UP_ROW;
        }
        else if (f_PlayerResults[playerIndex] < 0) {
            standingRow = STANDING_WORD_DOWN_ROW;
        }
        else {
            standingRow = STANDING_WORD_EVEN_ROW;
        }

        if (f_HasBets) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(
                    BETTING_MATCH_STANDING_X + f_ResultsSlide + standingShift[playerIndex]
                ),
                QuarterPixels(f_ResultsTop + MATCH_STANDING_Y + (playerIndex * MATCH_ROW_HEIGHT)),
                QuarterPixels(
                    BETTING_MATCH_STANDING_X + (f_ResultsSlide + STANDING_WORD_WIDTH) +
                    standingShift[playerIndex]
                ),
                QuarterPixels(
                    f_ResultsTop + MATCH_STANDING_Y + (playerIndex * MATCH_ROW_HEIGHT) +
                    STANDING_WORD_HEIGHT
                ),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(standingRow),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
        else {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(PLAIN_MATCH_STANDING_X + f_ResultsSlide + standingShift[playerIndex]),
                QuarterPixels(f_ResultsTop + MATCH_STANDING_Y + (playerIndex * MATCH_ROW_HEIGHT)),
                QuarterPixels(
                    PLAIN_MATCH_STANDING_X + (f_ResultsSlide + STANDING_WORD_WIDTH) +
                    standingShift[playerIndex]
                ),
                QuarterPixels(
                    f_ResultsTop + MATCH_STANDING_Y + (playerIndex * MATCH_ROW_HEIGHT) +
                    STANDING_WORD_HEIGHT
                ),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(standingRow),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    LoadSmallPalette(&gfx, 0, ImagePalette(f_ResultsLargeDigitsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(f_ResultsLargeDigitsImage),
        G_IM_FMT_CI,
        DIGIT_GLYPH_SIZE,
        DIGIT_FONT_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        if (f_HasBets) {
            // An even match draws only the word
            if (f_PlayerResults[0] != 0) {
                holeCount = f_PlayerResults[playerIndex];
                if (holeCount < 0) {
                    holeCount = -holeCount;
                }

                sprintf(g_PrintBuffer, "%2d", holeCount);
                DrawDigitText(
                    &gfx,
                    (f_ResultsSlide + BETTING_MATCH_HOLE_COUNT_X) + standingShift[playerIndex],
                    f_ResultsTop + MATCH_STANDING_Y + (playerIndex * MATCH_ROW_HEIGHT),
                    g_PrintBuffer
                );
            }

            sprintf(g_PrintBuffer, "%4d", f_PlayerBetTotals[playerIndex]);
            DrawDigitText(
                &gfx,
                (BETTING_MATCH_BET_TOTAL_X + f_ResultsSlide) + betShift,
                f_ResultsTop + MATCH_NUMBER_Y + (playerIndex * MATCH_ROW_HEIGHT),
                g_PrintBuffer
            );
            if (f_PlayerPoints[playerIndex] < 0) {
                sprintf(g_PrintBuffer, "  -", f_PlayerPoints[playerIndex]);
            }
            else {
                sprintf(g_PrintBuffer, "%3d", f_PlayerPoints[playerIndex]);
            }

            DrawDigitText(
                &gfx,
                f_ResultsSlide + BETTING_MATCH_POINTS_X,
                f_ResultsTop + MATCH_NUMBER_Y + (playerIndex * MATCH_ROW_HEIGHT),
                g_PrintBuffer
            );
        }
        else {
            if (f_PlayerResults[0] != 0) {
                holeCount = f_PlayerResults[playerIndex];
                if (holeCount < 0) {
                    holeCount = -holeCount;
                }

                sprintf(g_PrintBuffer, "%2d", holeCount);
                DrawDigitText(
                    &gfx,
                    (f_ResultsSlide + PLAIN_MATCH_HOLE_COUNT_X) + standingShift[playerIndex],
                    f_ResultsTop + MATCH_STANDING_Y + (playerIndex * MATCH_ROW_HEIGHT),
                    g_PrintBuffer
                );
            }

            if (f_PlayerPoints[playerIndex] < 0) {
                sprintf(g_PrintBuffer, "  -", f_PlayerPoints[playerIndex]);
            }
            else {
                sprintf(g_PrintBuffer, "%3d", f_PlayerPoints[playerIndex]);
            }

            DrawDigitText(
                &gfx,
                f_ResultsSlide + PLAIN_MATCH_POINTS_X,
                f_ResultsTop + MATCH_NUMBER_Y + (playerIndex * PLAIN_MATCH_POINTS_ROW_HEIGHT),
                g_PrintBuffer
            );
        }
    }

    if (f_OpenCourseCount == COURSE_COUNT) {
        SetTextSlot(
            f_ResultsSlide + ALL_COURSES_OPEN_X,
            f_ResultsTop + MATCH_ALL_COURSES_OPEN_Y,
            MESSAGE_ALL_COURSES_OPEN,
            TEXT_SLOT_LARGE_FONT,
            RESULTS_TEXT_SLOT,
            0,
            RESULTS_TEXT_RED,
            RESULTS_TEXT_GREEN,
            RESULTS_TEXT_BLUE,
            RESULTS_TEXT_ALPHA
        );
    }
    else {
        DrawResultsNumber(
            &gfx,
            f_ResultsSlide + POINTS_TOTAL_X,
            POINTS_TOTAL_Y,
            f_PointsToNextCourse + f_CoursePointsTally.earned
        );
        SetTextSlot(
            f_ResultsSlide + POINTS_TO_NEXT_COURSE_X,
            f_ResultsTop + POINTS_TO_NEXT_COURSE_Y,
            MESSAGE_MORE_POINTS_TO_NEXT_COURSE,
            TEXT_SLOT_LARGE_FONT,
            RESULTS_TEXT_SLOT,
            0,
            RESULTS_TEXT_RED,
            RESULTS_TEXT_GREEN,
            RESULTS_TEXT_BLUE,
            RESULTS_TEXT_ALPHA
        );
    }

    *displayListHead = gfx;
}

const char f_UnusedModeNamesText[] = "StrokeSpeedGolfMini-Golf";

// Without the badge the details move up into its place and spread out
#define CLUB_TOURNAMENT_DETAILS_TOP 74
#define CLUB_TOURNAMENT_LINE_HEIGHT 16
#define CLUB_TOURNAMENT_UNBADGED_DETAILS_TOP 58
#define CLUB_TOURNAMENT_UNBADGED_LINE_HEIGHT 18
#define CLUB_TOURNAMENT_NAME_SLOT 3
#define CLUB_TOURNAMENT_COURSE_SLOT 4
#define CLUB_TOURNAMENT_MODE_SLOT 5
#define CLUB_TOURNAMENT_TEXT_FLAGS (TEXT_FLAG_CENTERED | TEXT_FLAG_DRAWN_BY_OWNER)
// Where the club tournament results draw their parts
#define CLUB_TOURNAMENT_COURSE_X 28
#define CLUB_TOURNAMENT_COURSE_Y 104
#define CLUB_TOURNAMENT_FACE_X 96
#define BADGED_FACE_Y 136
#define UNBADGED_FACE_Y 128
#define NINTENDO_BADGE_X 88
#define NINTENDO_BADGE_Y 54
#define CLUB_TOURNAMENT_DETAILS_X (SCREEN_WIDTH / 2)
#define CLUB_TOURNAMENT_LABEL_X 144
#define CLUB_TOURNAMENT_VALUE_X 192
// The text lines sit this far below their line's top
#define CLUB_TOURNAMENT_TEXT_DROP 8
#define SCORE_PASSWORD_X 116
#define SCORE_PASSWORD_FIRST_Y 182
#define SCORE_PASSWORD_SECOND_Y 200

// The club tournament details' lines, from the tournament's number down
enum ClubTournamentLine {
    CLUB_TOURNAMENT_LINE_NUMBER,
    CLUB_TOURNAMENT_LINE_NAME,
    CLUB_TOURNAMENT_LINE_COURSE,
    CLUB_TOURNAMENT_LINE_MODE,
    CLUB_TOURNAMENT_LINE_SCORE,
    CLUB_TOURNAMENT_LINE_TIME
};

// The top of a club tournament details' text line
#define ClubTournamentTextY(detailsTop, lineHeight, line) \
    (f_ResultsTop + (detailsTop) + ((lineHeight) * (line)) + CLUB_TOURNAMENT_TEXT_DROP)

// DrawLargeTextWithShadow ignores this flag
#define CLUB_TOURNAMENT_LABEL_FLAGS 4
#define CLUB_TOURNAMENT_LABEL_ENV_COLOR 0xFF5B0000
#define CLUB_TOURNAMENT_LABEL_PRIM_COLOR 0xFFFF0000
#define SCORE_PASSWORD_ENV_COLOR 0x00FF0000
#define SCORE_PASSWORD_PRIM_COLOR 0xFFFF0000

// The results of a club tournament: its course, number, name and mode, the
// score and the password to send in.
void DrawClubTournamentResults(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 width;
    s32 height;
    GameMode gameMode;
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
    LoadLargePalette(&gfx, ImagePalette(f_ResultsCourseImage));
    for (i = 0; i != COURSE_ICON_STRIP_COUNT; i++) {
        LoadTextureBlock(
            &gfx,
            LargeImageTexels(f_ResultsCourseImage) +
                (i * COURSE_ICON_WIDTH * COURSE_ICON_STRIP_HEIGHT),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            COURSE_ICON_WIDTH,
            COURSE_ICON_STRIP_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        firstStripBottom = f_ResultsTop + CLUB_TOURNAMENT_COURSE_Y + COURSE_ICON_STRIP_HEIGHT;
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(f_ResultsSlide + CLUB_TOURNAMENT_COURSE_X),
            QuarterPixels((i * COURSE_ICON_STRIP_HEIGHT) + CLUB_TOURNAMENT_COURSE_Y + f_ResultsTop),
            QuarterPixels(f_ResultsSlide + CLUB_TOURNAMENT_COURSE_X + COURSE_ICON_WIDTH),
            QuarterPixels((i * COURSE_ICON_STRIP_HEIGHT) + firstStripBottom),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    detailsTop = CLUB_TOURNAMENT_DETAILS_TOP;
    if (isNintendo) {
        lineHeight = CLUB_TOURNAMENT_LINE_HEIGHT;
        DrawPlayerFace(
            &gfx,
            f_ResultsSlide + CLUB_TOURNAMENT_FACE_X,
            f_ResultsTop + BADGED_FACE_Y,
            0,
            TRUE
        );
        LoadLargePalette(&gfx, ImagePalette(f_NintendoBadgeImage));
        for (i = 0; i != NINTENDO_BADGE_STRIP_COUNT; i++) {
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(f_NintendoBadgeImage) +
                    (i * NINTENDO_BADGE_WIDTH * NINTENDO_BADGE_STRIP_HEIGHT),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                NINTENDO_BADGE_WIDTH,
                NINTENDO_BADGE_STRIP_HEIGHT,
                0,
                G_TX_MIRROR,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(f_ResultsSlide + NINTENDO_BADGE_X),
                QuarterPixels(f_ResultsTop + NINTENDO_BADGE_Y + (i * NINTENDO_BADGE_STRIP_HEIGHT)),
                QuarterPixels(f_ResultsSlide + NINTENDO_BADGE_X + NINTENDO_BADGE_WIDTH),
                QuarterPixels(
                    f_ResultsTop + NINTENDO_BADGE_Y + (i * NINTENDO_BADGE_STRIP_HEIGHT) +
                    NINTENDO_BADGE_STRIP_HEIGHT
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }
    else {
        detailsTop = CLUB_TOURNAMENT_UNBADGED_DETAILS_TOP;
        lineHeight = CLUB_TOURNAMENT_UNBADGED_LINE_HEIGHT;
        DrawPlayerFace(
            &gfx,
            f_ResultsSlide + CLUB_TOURNAMENT_FACE_X,
            f_ResultsTop + UNBADGED_FACE_Y,
            0,
            TRUE
        );
    }

    DrawPlaceNumber(
        &gfx,
        f_ResultsSlide + CLUB_TOURNAMENT_DETAILS_X,
        f_ResultsTop + detailsTop,
        GetClubTournamentNumber()
    );
    SetUpTextDrawing(&gfx);
    MeasureMessage(GetClubTournamentNameMessage(), FONT_SIZE_LARGE, &width, &height, 0);
    MeasureMessage(GetClubTournamentCourseTitleMessage(), FONT_SIZE_LARGE, &width, &height, 0);
    MeasureMessage(GetClubTournamentModeMessage(), FONT_SIZE_LARGE, &width, &height, 0);
    SetTextSlot(
        f_ResultsSlide + CLUB_TOURNAMENT_DETAILS_X,
        ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_NAME),
        GetClubTournamentNameMessage(),
        TEXT_SLOT_LARGE_FONT,
        CLUB_TOURNAMENT_NAME_SLOT,
        CLUB_TOURNAMENT_TEXT_FLAGS,
        127,
        191,
        255,
        OPAQUE_ALPHA
    );
    SetTextSlot(
        f_ResultsSlide + CLUB_TOURNAMENT_DETAILS_X,
        ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_COURSE),
        GetClubTournamentCourseTitleMessage(),
        TEXT_SLOT_LARGE_FONT,
        CLUB_TOURNAMENT_COURSE_SLOT,
        CLUB_TOURNAMENT_TEXT_FLAGS,
        43,
        255,
        75,
        OPAQUE_ALPHA
    );
    SetTextSlot(
        f_ResultsSlide + CLUB_TOURNAMENT_DETAILS_X,
        ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_MODE),
        GetClubTournamentModeMessage(),
        TEXT_SLOT_LARGE_FONT,
        CLUB_TOURNAMENT_MODE_SLOT,
        CLUB_TOURNAMENT_TEXT_FLAGS,
        255,
        63,
        127,
        OPAQUE_ALPHA
    );
    for (i = CLUB_TOURNAMENT_NAME_SLOT; i != CLUB_TOURNAMENT_MODE_SLOT + 1; i++) {
        gDPSetPrimColor(
            gfx++,
            0,
            0,
            f_TextSlotColors[i - 1][RED_CHANNEL],
            f_TextSlotColors[i - 1][GREEN_CHANNEL],
            f_TextSlotColors[i - 1][BLUE_CHANNEL],
            OPAQUE_ALPHA
        );
        DrawLargeTextSlot(&gfx, i);
    }

    SetUpGradientTextDrawing(&gfx, 0);
    sprintf(g_PrintBuffer, "Score");
    DrawLargeTextWithShadow(
        &gfx,
        f_ResultsSlide + CLUB_TOURNAMENT_LABEL_X,
        ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_SCORE),
        g_PrintBuffer,
        CLUB_TOURNAMENT_LABEL_ENV_COLOR,
        CLUB_TOURNAMENT_LABEL_PRIM_COLOR,
        CLUB_TOURNAMENT_LABEL_FLAGS
    );
    if (f_PlayerResults[0] == 0) {
        // The digit font draws '#' as a plus-minus sign, for an even score
        sprintf(g_PrintBuffer, "#0", 0);
    }
    else {
        sprintf(g_PrintBuffer, "%+d", f_PlayerResults[0]);
    }

    DrawLargeTextWithShadow(
        &gfx,
        f_ResultsSlide + CLUB_TOURNAMENT_VALUE_X,
        ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_SCORE),
        g_PrintBuffer,
        CLUB_TOURNAMENT_LABEL_ENV_COLOR,
        CLUB_TOURNAMENT_LABEL_PRIM_COLOR,
        TEXT_FLAG_FIXED_WIDTH
    );
    if (gameMode == GAME_MODE_SPEED_GOLF) {
        hundredths = (s32)(g_GameState.stopwatchFrameCount *
                         ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
            HUNDREDTHS_PER_SECOND;
        seconds = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND % SECONDS_PER_MINUTE;
        minutes = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND / SECONDS_PER_MINUTE %
            SHOWN_MINUTES_LIMIT;
        sprintf(g_PrintBuffer, "Time");
        DrawLargeTextWithShadow(
            &gfx,
            f_ResultsSlide + CLUB_TOURNAMENT_LABEL_X,
            ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_TIME),
            g_PrintBuffer,
            CLUB_TOURNAMENT_LABEL_ENV_COLOR,
            CLUB_TOURNAMENT_LABEL_PRIM_COLOR,
            CLUB_TOURNAMENT_LABEL_FLAGS
        );
        sprintf(g_PrintBuffer, "%02d:%02d:%02d", minutes, seconds, hundredths);
        DrawLargeTextWithShadow(
            &gfx,
            f_ResultsSlide + CLUB_TOURNAMENT_VALUE_X,
            ClubTournamentTextY(detailsTop, lineHeight, CLUB_TOURNAMENT_LINE_TIME),
            g_PrintBuffer,
            CLUB_TOURNAMENT_LABEL_ENV_COLOR,
            CLUB_TOURNAMENT_LABEL_PRIM_COLOR,
            TEXT_FLAG_FIXED_WIDTH
        );
    }

    for (i = 0; i != SCORE_PASSWORD_LINE_LENGTH; i++) {
        g_PrintBuffer[i] = f_ScorePassword[i];
    }

    g_PrintBuffer[SCORE_PASSWORD_LINE_LENGTH] = '\0';
    DrawLargeTextWithShadow(
        &gfx,
        f_ResultsSlide + SCORE_PASSWORD_X,
        f_ResultsTop + SCORE_PASSWORD_FIRST_Y,
        g_PrintBuffer,
        SCORE_PASSWORD_ENV_COLOR,
        SCORE_PASSWORD_PRIM_COLOR,
        TEXT_FLAG_FIXED_WIDTH
    );
    for (i = 0; i != SCORE_PASSWORD_LINE_LENGTH; i++) {
        g_PrintBuffer[i] = f_ScorePassword[SCORE_PASSWORD_LINE_LENGTH + i];
    }

    g_PrintBuffer[SCORE_PASSWORD_LINE_LENGTH] = '\0';
    DrawLargeTextWithShadow(
        &gfx,
        f_ResultsSlide + SCORE_PASSWORD_X,
        f_ResultsTop + SCORE_PASSWORD_SECOND_Y,
        g_PrintBuffer,
        SCORE_PASSWORD_ENV_COLOR,
        SCORE_PASSWORD_PRIM_COLOR,
        TEXT_FLAG_FIXED_WIDTH
    );
    *displayListHead = gfx;
}

#define BACKDROP_X 16
#define BACKDROP_Y 24
#define BACKDROP_STRIP_WIDTH 288
#define BACKDROP_STRIP_HEIGHT 5
#define BACKDROP_STRIP_COUNT 40
// The club tournament backdrop is taller, so it starts higher
#define CLUB_TOURNAMENT_BACKDROP_Y 8
#define CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT 7
#define CLUB_TOURNAMENT_BACKDROP_STRIP_COUNT 32
#define FINAL_RESULTS_TITLE_WIDTH 200
#define FINAL_RESULTS_TITLE_STRIP_HEIGHT 8
// Each strip loads a row more than it shows so that filtering has a row below
// it
#define FINAL_RESULTS_TITLE_LOADED_HEIGHT 10
#define FINAL_RESULTS_TITLE_LAST_STRIP_HEIGHT 6
#define FINAL_RESULTS_TITLE_STRIP_COUNT 4
#define FINAL_RESULTS_TITLE_X 24.0f
#define FINAL_RESULTS_TITLE_Y 20.0f
#define FINAL_RESULTS_TITLE_BOB 5.0f
#define FINAL_RESULTS_TITLE_BOB_STEP (M_PI / 18)
// A texture coordinate of this many units steps one texel
#define TEXEL_UNITS 32
#define SUBTEXEL_STEPS 16.0f
// The portrait animations load their faces only every fourth draw
#define FACE_FRAME_DRAWS_MASK 3
#define RESULTS_FACE_ANIMATION_FRAME_COUNT 5
#define FACE_ANIMATION_REST 0

// The results card, its game mode's results, the bobbing title and the players'
// portraits.
//
// The title's sub-texel offsets are both worked out before either is
// doubled, and the rectangle adds the texel row to the doubled T offset:
// converting and doubling each offset in one statement, or doubling the S
// offset before converting the T one, gives 42 words differing, and adding
// the row before the rectangle 4, where the original doubles the T offset in
// a call's delay slot.
void DrawRoundResults(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    f32 titleX;
    f32 titleY;
    s32 left;
    s32 top;
    s32 right;
    s32 stripHeight;
    s32 titleSubtexelS;
    s32 titleSubtexelT;
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
    LoadLargePalette(&gfx, ImagePalette(f_ResultsBackdropImage));
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    if (g_GameState.isClubTournament) {
        for (i = 0; i != CLUB_TOURNAMENT_BACKDROP_STRIP_COUNT; i++) {
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(f_ResultsBackdropImage) +
                    (i * BACKDROP_STRIP_WIDTH * CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                BACKDROP_STRIP_WIDTH,
                CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(f_ResultsSlide + BACKDROP_X),
                QuarterPixels(
                    (i * CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT) + CLUB_TOURNAMENT_BACKDROP_Y +
                    f_ResultsTop
                ),
                QuarterPixels(f_ResultsSlide + BACKDROP_X + BACKDROP_STRIP_WIDTH),
                QuarterPixels(
                    (i * CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT) + CLUB_TOURNAMENT_BACKDROP_Y +
                    CLUB_TOURNAMENT_BACKDROP_STRIP_HEIGHT + f_ResultsTop
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }
    else {
        for (i = 0; i != BACKDROP_STRIP_COUNT; i++) {
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(f_ResultsBackdropImage) +
                    (i * BACKDROP_STRIP_WIDTH * BACKDROP_STRIP_HEIGHT),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                BACKDROP_STRIP_WIDTH,
                BACKDROP_STRIP_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(f_ResultsSlide + BACKDROP_X),
                QuarterPixels((i * BACKDROP_STRIP_HEIGHT) + BACKDROP_Y + f_ResultsTop),
                QuarterPixels(f_ResultsSlide + BACKDROP_X + BACKDROP_STRIP_WIDTH),
                QuarterPixels(
                    (i * BACKDROP_STRIP_HEIGHT) + BACKDROP_Y + BACKDROP_STRIP_HEIGHT + f_ResultsTop
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    if (f_IsAwaitingConfirm == TRUE) {
        f_PressingHandFrame = (f_PressingHandFrame + 1) % PRESSING_HAND_FRAME_COUNT;
        LoadLargePalette(&gfx, ImagePalette(f_ResultsHandImage));
        LoadTextureBlock(
            &gfx,
            LargeImageTexels(f_ResultsHandImage) +
                (f_PressingHandFrame / PRESSING_HAND_FRAMES_PER_IMAGE * PRESSING_HAND_IMAGE_SIZE),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            PRESSING_HAND_SIZE,
            PRESSING_HAND_SIZE,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(f_ResultsSlide + CORNER_PRESSING_HAND_LEFT),
            QuarterPixels(f_ResultsTop + CORNER_PRESSING_HAND_TOP),
            QuarterPixels(f_ResultsSlide + CORNER_PRESSING_HAND_LEFT + PRESSING_HAND_SIZE),
            QuarterPixels(f_ResultsTop + CORNER_PRESSING_HAND_TOP + PRESSING_HAND_SIZE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    if (g_GameState.isClubTournament) {
        DrawClubTournamentResults(&gfx);
    }
    else {
        switch (g_GameState.gameMode) {
            case GAME_MODE_SPEED_GOLF:
                DrawSpeedGolfResults(&gfx);
                break;
            case GAME_MODE_STROKE:
            case GAME_MODE_UNLISTED_STROKE:
            case GAME_MODE_TRAINING:
            case GAME_MODE_UNRECORDED_STROKE:
            case GAME_MODE_RECORD_REPLAY:
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
        titleX = (cosf(f_TitleBobAngle * 0.5f) * FINAL_RESULTS_TITLE_BOB) + FINAL_RESULTS_TITLE_X;
        titleY = (sinf(f_TitleBobAngle) * FINAL_RESULTS_TITLE_BOB) + FINAL_RESULTS_TITLE_Y;
        left = HalfTowardZero(titleX);
        top = HalfTowardZero(titleY);
        gSPDisplayList(gfx++, g_DecalTextureSetup);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_AVERAGE);
        titleSubtexelS = (s32)(SUBTEXEL_STEPS - ((titleX - left) * SUBTEXEL_STEPS));
        titleSubtexelT = (s32)(SUBTEXEL_STEPS - ((titleY - top) * SUBTEXEL_STEPS));
        textureS = titleSubtexelS * 2;
        textureT = titleSubtexelT * 2;
        f_TitleBobAngle += FINAL_RESULTS_TITLE_BOB_STEP;
        LoadLargePalette(&gfx, ImagePalette(f_FinalResultsTitleImage));
        for (i = 0; i != FINAL_RESULTS_TITLE_STRIP_COUNT; i++) {
            // Setting the usual height first and the last strip's in an if, or
            // testing for not the last strip, gives 10 words differing, 6 in shape
            if (i == FINAL_RESULTS_TITLE_STRIP_COUNT - 1) {
                stripHeight = FINAL_RESULTS_TITLE_LAST_STRIP_HEIGHT;
            }
            else {
                stripHeight = FINAL_RESULTS_TITLE_STRIP_HEIGHT;
            }

            right = left + FINAL_RESULTS_TITLE_WIDTH;
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(f_FinalResultsTitleImage) +
                    (i * FINAL_RESULTS_TITLE_WIDTH * FINAL_RESULTS_TITLE_STRIP_HEIGHT),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                FINAL_RESULTS_TITLE_WIDTH,
                FINAL_RESULTS_TITLE_LOADED_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gSPTextureRectangle(
                gfx++,
                QuarterPixels(left - f_ResultsSlide),
                QuarterPixels(i * FINAL_RESULTS_TITLE_STRIP_HEIGHT + f_ResultsTop + top),
                QuarterPixels(right - f_ResultsSlide),
                QuarterPixels(
                    i * FINAL_RESULTS_TITLE_STRIP_HEIGHT + stripHeight + f_ResultsTop + top
                ),
                G_TX_RENDERTILE,
                textureS,
                textureT + TEXEL_UNITS,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    for (i = 0; i != g_GameState.playerCount; i++) {
        EncodedAsset asset;

        if (f_FaceFrames[i] != NO_FACE_FRAME) {
            if (!(f_FaceFrameTimes[i] & FACE_FRAME_DRAWS_MASK)) {
                switch (f_FaceAnimations[i]) {
                    case FACE_ANIMATION_REST:
                        OpenEncodedAsset(
                            g_CharacterFirstFaceAssets[g_Players[i].characterIndex],
                            &asset
                        );
                        ReadEncodedAsset(f_PlayerFaceImages[i], &asset);
                        f_FaceFrames[i] = NO_FACE_FRAME;
                        break;
                    case FACE_ANIMATION_PLEASED:
                        OpenEncodedAsset(
                            g_CharacterFirstFaceAssets[g_Players[i].characterIndex] +
                                f_FaceFrames[i],
                            &asset
                        );
                        ReadEncodedAsset(f_PlayerFaceImages[i], &asset);
                        f_FaceFrames[i]++;
                        if (f_FaceFrames[i] == RESULTS_FACE_ANIMATION_FRAME_COUNT) {
                            f_FaceFrames[i] = NO_FACE_FRAME;
                        }

                        break;
                    case FACE_ANIMATION_BEATEN: {
                        s32 beatenFrames[RESULTS_FACE_ANIMATION_FRAME_COUNT] = {0, 5, 6, 7, 8};

                        OpenEncodedAsset(
                            g_CharacterFirstFaceAssets[g_Players[i].characterIndex] +
                                beatenFrames[f_FaceFrames[i]],
                            &asset
                        );
                        ReadEncodedAsset(f_PlayerFaceImages[i], &asset);
                        f_FaceFrames[i]++;
                        if (f_FaceFrames[i] == RESULTS_FACE_ANIMATION_FRAME_COUNT) {
                            f_FaceFrames[i] = NO_FACE_FRAME;
                        }

                        break;
                    }
                }
            }

            f_FaceFrameTimes[i]++;
        }
    }

    DrawResultsBanner(&gfx);
    *displayListHead = gfx;
}

void DrawResultsNotice(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

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
