/*
 * The clubhouse's Best Shots, screen 12, a grid of stats over a brick wall with
 * ranked records and page arrows. The file also holds the bet settings screen
 * and the handicap screen, where each player chooses a handicap.
 */
#include "common.h"

#include "asset.h"
#include "character.h"
#include "character_slot.h"
#include "course.h"
#include "cpu_projection.h"
#include "game.h"
#include "game_flags.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "libc.h"
#include "main_menu.h"
#include "math_util.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "sram.h"
#include "text.h"

/* Score labels in Japanese */
#define ASSET_JAPANESE_BIRDIE_LABEL 0x82D
#define ASSET_JAPANESE_ALBATROSS_LABEL 0x82E
#define ASSET_JAPANESE_EAGLE_LABEL 0x82F
#define ASSET_JAPANESE_HOLE_IN_ONE_LABEL 0x830

/* The images of g_CourseIconAssets that LoadBestShotsCourseIcons reads */
#define PLAYER_STATS_IMAGE_COUNT 6

/* The bytes of each of the four images that LoadScoreLabelBuffer reads into one
 * buffer
 */
#define PLAYER_STATS_IMAGE_SIZE 0x258
/* They are the ASSET_JAPANESE_*_LABEL score labels */
#define SCORE_LABEL_COUNT 4

/* DrawCharacterFace writes names beside the faces of these first characters
 * only */
#define NAMED_CHARACTER_COUNT 14

/* The grid of stats that the cursor moves over, and the value of its empty
 * cells */
#define STATS_GRID_COLUMN_COUNT 5
#define STATS_GRID_ROW_COUNT 4
#define STATS_CELL_EMPTY (-1)

/* The rows of the Best Shots grid, by score kind */
enum BestShotRow {
  BEST_SHOT_ROW_BIRDIE,
  BEST_SHOT_ROW_EAGLE,
  BEST_SHOT_ROW_ALBATROSS,
  BEST_SHOT_ROW_HOLE_IN_ONE
};

/* The order in which ReadBestShots gathers the kinds before laying them out */
enum GatheredBestShots {
  GATHERED_BIRDIES,
  GATHERED_EAGLES,
  GATHERED_HOLES_IN_ONE,
  GATHERED_ALBATROSSES
};

/* ReadBestShots lists each kind's records in rows one longer than the grid */
#define STATS_KIND_LIST_SIZE (STATS_GRID_COLUMN_COUNT + 1)

/* What LoadPlayerStatsBuffer reads into the buffers it fills */
#define BET_ICON_COUNT 8
/* Each bet icon's room in the buffer, more than its 0x148 bytes */
#define BET_ICON_SLOT_SIZE 0x218
/* An 8x8 orange dot, drawn once per stake level of a bet */
#define ASSET_STAKE_MARK 0x671
/* A 64x44 wooden plaque, drawn behind each player's handicap */
#define ASSET_HANDICAP_PLAQUE 0x6C4
/*
 * The 128x20 names of the bets: Driving Contest, Closest to Pin, Score, Nassau,
 * a blank, Skins, Match and Doubles
 */
#define ASSET_BET_LABELS 0x6E8
#define BET_LABEL_WIDTH 128
#define BET_LABEL_HEIGHT 20
#define BET_LABEL_IMAGE_SIZE (BET_LABEL_WIDTH * BET_LABEL_HEIGHT / 2)
#define ASSET_STATS_BRICK_WALL 0x721
/* Eight 32x32 frames of flashing triple arrows pointing left */
#define ASSET_TRIPLE_PAGE_ARROWS 0x828
/* The same as ASSET_TRIPLE_PAGE_ARROWS, with double arrows */
#define ASSET_DOUBLE_PAGE_ARROWS 0x829
#define BET_SET_MAX_COLUMN_COUNT 6
/*
 * The bet set tables have a row for each set that offers bets, from
 * BET_SET_STROKE on; columns past a set's count in f_BetSetColumnCounts hold
 * UNUSED_BET_COLUMN
 */
#define OFFERING_BET_SET_COUNT BET_SET_MINI_GOLF_FOUR_PLAYER
#define UNUSED_BET_COLUMN (-1)
/* The bet icons are 24x24, and DrawBetIcon shows each of their two frames
 * for 4 video frames */
#define BET_ICON_SIZE 24
#define BET_ICON_FRAME_COUNT 2
#define BET_ICON_FRAME_DURATION 4

/* The text slots that screen 12 clears on leaving */
#define STATS_TEXT_SLOT_COUNT 4

/* The animated arrows that DrawPageArrows draws either side of the page */
#define PAGE_ARROW_COUNT 2
#define PAGE_ARROW_RIGHT 1
#define PAGE_ARROW_SIZE 32
#define PAGE_ARROW_MASK 5
#define PAGE_ARROW_IMAGE_SIZE (PAGE_ARROW_SIZE * PAGE_ARROW_SIZE / 2)
#define PAGE_ARROW_FRAME_COUNT 8
#define PAGE_ARROW_FRAME_DURATION 3
#define PAGE_ARROW_LEFT_X 76
#define PAGE_ARROW_RIGHT_X 212
#define PAGE_ARROW_TOP 122
/*
 * The texel column where the arrow's texture starts. The right arrow's starts
 * a whole image in, where the mirrored wrap reads the left arrow's backward.
 */
#define MirroredArrowColumn(arrowIndex) \
  (((arrowIndex) == PAGE_ARROW_RIGHT) * PAGE_ARROW_SIZE)
/* DrawTurnedPageArrows turns the page arrows a quarter turn each way, and draws
 * them as squares away from the camera */
#define TURNED_ARROW_ANGLE 90.0f
#define TURNED_ARROW_Z (-100.0f)
#define TURNED_ARROW_NEAR 10.0f
#define TURNED_ARROW_FAR 2000.0f
#define TURNED_ARROW_VERTEX_COUNT 4

/* The blinking arrows that DrawUpDownArrows draws above and below a point */
#define STATS_ARROW_COUNT 2
#define STATS_ARROW_BLINK_FRAME_COUNT 4
#define STATS_ARROW_LEFT 152
#define STATS_UP_ARROW_OFFSET (-52)
#define STATS_DOWN_ARROW_OFFSET 36

/* The bytes of each of f_RecordLabels's record labels, such as "Birdie" */
#define RECORD_LABEL_SIZE 16

/* DrawBestShotsCursor puts the cursor on the stats grid's chosen cell, measured
 * from the screen's center */
#define STATS_CURSOR_COLUMN_WIDTH 34
#define STATS_CURSOR_ROW_HEIGHT 26
#define STATS_CURSOR_LEFT_X 6
#define STATS_CURSOR_TOP_Y 30

/* The directions that FindNextBestShotCell moves the cursor in, which index
 * f_BestShotDirectionSteps
 */
enum StatsDirection {
  STATS_DIRECTION_LEFT,
  STATS_DIRECTION_RIGHT,
  STATS_DIRECTION_UP,
  STATS_DIRECTION_DOWN
};

#define STATS_DIRECTION_COUNT 4

/* The rows of the brick wall behind the stats, which slide across together */
#define STATS_SLIDE_ROW_COUNT 9
/*
 * DrawBrickWallRows draws the 152x117 brick wall behind the stats in one strip
 * per row, each stretched across the screen
 */
#define STATS_WALL_IMAGE_WIDTH 152
#define STATS_WALL_STRIP_HEIGHT 13
#define STATS_WALL_ROW_HEIGHT 27
/* Each bet's stake is one of these levels, which f_BetStakeAmounts turns into
 * an amount */
#define BET_STAKE_LEVEL_COUNT 6
/* The betStakeLevels value of a bet that ApplyBetPreset's preset leaves out */
#define NO_BET (-1)
/* Where UpdateBetSettings types the chosen bet's explanation, and its first
 * message
 */
#define BET_HELP_X 36
#define BET_HELP_Y 186
#define BET_HELP_MESSAGE 0x15

/* Each player's choice of handicap, and where the screen's slide starts */
#define HANDICAP_CHOICE_COUNT 37
#define HANDICAP_SLIDE_START (-512)

/*
 * GCC 2.7 emits each string constant once per file, where it is first used,
 * so the original's "%s" belongs to DrawBestShots. While that is asm, the
 * routines after it print with the copy in its rodata.
 */
#ifdef NON_MATCHING
#define f_FaceNameFormat "%s"
#else
extern const char f_FaceNameFormat[];
#endif

/* A step from one grid cell to its neighbor */
typedef struct GridStep {
  /* 0x0 */ s8 column;
  /* 0x1 */ s8 row;
} GridStep; /* size = 0x2 */

void DrawBrickWallRows(Gfx** displayListHead, u8 slide);

s16 GetChosenBestShot(void);
void DrawBestShotsCursor(Gfx** displayListHead);
void DrawCharacterFace(Gfx** displayListHead, void* texels, void* palette,
    s32 left, s32 top, s16 character, u8 drawMode);
void DrawOneDigitMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void DrawUnalignedMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void DrawBetIcon(
    Gfx** displayListHead, s32 left, s32 top, s32 size, s32 textureStep);
void DrawBetLabel(Gfx** displayListHead, s32 centerX, s32 top, u8 labelIndex);

/* Set once the chosen bet's help or the handicap prompt has been typed */
static u8 f_IsBetHelpShown = FALSE;
static u8 f_IsHandicapPromptShown = FALSE;
/* The blinking green arrows' frame and its time shown */
static s16 f_GreenArrowFrame = 0;
static s16 f_GreenArrowTimer = 0;
/* The page arrows' animation frame and its time shown */
static s16 f_PageArrowFrame = 0;
static s16 f_PageArrowTimer = 0;
/* Each bet icon's first frame, read for the bet settings' rows */
static u32 f_BetIconAssets[BET_ICON_COUNT] = {
    0x0674, 0x0678, 0x067A, 0x067E, 0x0680, 0x067C, 0x0676, 0x0672};
/* Both frames of each bet icon, for the chosen bet's animation */
// clang-format off
static u32 f_BetIconFrameAssets[BET_ICON_COUNT]
                             [BET_ICON_FRAME_COUNT] = {
  { 0x0674, 0x0675 },
  { 0x0678, 0x0679 },
  { 0x067A, 0x067B },
  { 0x067E, 0x067F },
  { 0x0680, 0x0681 },
  { 0x067C, 0x067D },
  { 0x0676, 0x0677 },
  { 0x0672, 0x0673 }
};
// clang-format on
/* The bet whose label and icon each column of a bet set shows */
// clang-format off
static s16 f_BetSetBets[OFFERING_BET_SET_COUNT]
                             [BET_SET_MAX_COLUMN_COUNT] = {
  { 0, 1, 2, 3, 5, UNUSED_BET_COLUMN },
  { 0, 1, 2, 3, 5, 7 },
  { 0, 1, 6, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN },
  { 0, 1, 5, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN },
  { 0, 1, 5, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN },
  { 2, 3, 5, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN },
  { 2, 3, 5, 7, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN }
};
// clang-format on
// clang-format off
/* Which of betStakeLevels' stakes each column of a bet set sets */
static s16 f_BetSetStakeSlots[OFFERING_BET_SET_COUNT]
                             [BET_SET_MAX_COLUMN_COUNT] = {
  { 0, 1, 2, 3, 4, UNUSED_BET_COLUMN },
  { 0, 1, 2, 3, 4, 6 },
  { 0, 1, 5, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN },
  { 0, 1, 4, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN },
  { 0, 1, 4, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN },
  { 2, 3, 4, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN },
  { 2, 3, 4, 6, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN }
};
// clang-format on
/* How many columns each bet set uses */
static u8 f_BetSetColumnCounts[OFFERING_BET_SET_COUNT] = {5, 6, 3, 3, 3, 3, 4};
/* Each BET_PRESET_*'s stake level for each bet */
// clang-format off
static u8 f_BetPresetStakeLevels[BET_PRESET_COUNT]
                            [BET_STAKE_SLOT_COUNT] = {
  { 4, 4, 1, 0, 0, NO_BET, NO_BET, NO_BET },
  { 4, 4, 0, 0, 0, NO_BET, 0, NO_BET },
  { 4, 4, NO_BET, NO_BET, NO_BET, 1, NO_BET, NO_BET },
  { 4, 4, NO_BET, NO_BET, 1, NO_BET, NO_BET, NO_BET },
  { 4, 4, NO_BET, NO_BET, 1, NO_BET, NO_BET, NO_BET },
  { NO_BET, NO_BET, 1, 0, 0, NO_BET, NO_BET, NO_BET },
  { NO_BET, NO_BET, 0, 0, 0, NO_BET, 0, NO_BET }
};
// clang-format on
static u8 f_BetStakeAmounts[BET_STAKE_LEVEL_COUNT] = {0, 1, 2, 3, 5, 10};
// clang-format off
/* The x of each column of a bet set */
static s32 f_BetSetColumnXs[OFFERING_BET_SET_COUNT]
                             [BET_SET_MAX_COLUMN_COUNT] = {
  { 68, 108, 148, 188, 228, 228 },
  { 48, 88, 128, 168, 208, 248 },
  { 114, 148, 184, 216, 256, 256 },
  { 114, 148, 184, 216, 256, 256 },
  { 114, 148, 184, 144, 144, 144 },
  { 114, 148, 184, 216, 256, 256 },
  { 87, 127, 167, 207, 228, 228 }
};
// clang-format on
/* Read by no routine; katakana for "Drive contest", "Near pin", "Score",
 * "Vertical", "Horizontal", "Skins", "Match" and "Doubles" */
static char f_UnusedGameNames[8][10] UNUSED = {
    "ﾄﾞﾗｺﾝ", "ﾆｱﾋﾟﾝ", "ｽｺｱ", "ﾀﾃ", "ﾖｺ", "ｽｷﾝｽﾞ", "ﾏｯﾁ", "ﾀﾞﾌﾞﾙｽ"};
/* The handicap screen's golfers' turn, in degrees about X and Y */
static f32 f_HandicapGolferAngles[2] = {0.0f, 0.0f};
/* Read by no routine */
static s32 f_UnusedWordsAfterGolferAngles[3] UNUSED = {0, 0, 0};
/* Each golfer's scale on the handicap screen */
static f32 f_HandicapGolferScales[MAX_PLAYER_COUNT] = {4.2f, 4.0f, 4.2f, 4.0f};
/* Where DrawHandicaps stands each golfer, by the player count */
// clang-format off
static Vec3 f_HandicapGolferPositions[MAX_PLAYER_COUNT]
                              [MAX_PLAYER_COUNT] = {
  {
    { 0.0f, -44.0f, -1000.0f },
    { 64.0f, -44.0f, -1500.0f },
    { 64.0f, -44.0f, -1000.0f },
    { 64.0f, -44.0f, -1500.0f }
  },
  {
    { -64.0f, -44.0f, -1500.0f },
    { 64.0f, -44.0f, -1000.0f },
    { 64.0f, -44.0f, -1500.0f },
    { 64.0f, -44.0f, -1000.0f }
  },
  {
    { -96.0f, -44.0f, -1500.0f },
    { 0.0f, -54.0f, -1000.0f },
    { 96.0f, -44.0f, -1500.0f },
    { 96.0f, -44.0f, -1000.0f }
  },
  {
    { -96.0f, -44.0f, -1500.0f },
    { -32.0f, -54.0f, -1000.0f },
    { 32.0f, -44.0f, -1500.0f },
    { 96.0f, -54.0f, -1000.0f }
  }
};
// clang-format on
/* The left of each player's column, by the player count */
static s32 f_HandicapColumnLefts[MAX_PLAYER_COUNT][MAX_PLAYER_COUNT] = {
    {128, 0, 0, 0}, {96, 160, 0, 0}, {64, 128, 192, 0}, {32, 96, 160, 224}};
/* The frames each brick wall row waits before it moves, from a slide's start */
static s32 f_BrickRowStartDelays[STATS_SLIDE_ROW_COUNT] = {
    0, 2, 4, 6, 8, 10, 12, 14, 16};
/* The frames each row has still to wait */
static s32 f_BrickRowDelays[STATS_SLIDE_ROW_COUNT] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0};
/* How far each row moves a frame */
static s32 f_BrickRowSpeeds[STATS_SLIDE_ROW_COUNT] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64};
/* How far across the screen each row is drawn */
static s32 f_BrickRowOffsets[STATS_SLIDE_ROW_COUNT] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0};
/* Where each slide but STATS_SLIDE_CURRENT starts the rows */
// clang-format off
static s32 f_BrickSlideStartOffsets[STATS_SLIDE_COUNT - 1]
                             [STATS_SLIDE_ROW_COUNT] = {
  { 320, 320, 320, 320, 320, 320, 320, 320, 320 },
  { -320, -320, -320, -320, -320, -320, -320, -320, -320 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
// clang-format on
/* The slide that the rows were last started on */
static u8 f_BrickWallSlide = 0;
/* The corners of each page arrow that DrawTurnedPageArrows tilts */
static Vtx f_TurnedArrowVertices[TURNED_ARROW_VERTEX_COUNT] = {
    {{{-16, 16, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-16, -16, 0}, 0, {0, 2048}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{16, -16, 0}, 0, {2048, 2048}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{16, 16, 0}, 0, {2048, 0}, {0xFF, 0xFF, 0x00, 0xFF}}}};
/* Read by no routine */
static s32 f_UnusedWordsAfterArrowVertices[2] UNUSED = {0, 0};
/* Read by no routine; the values of f_BestShotsCursorPhases */
static f32 f_UnusedCursorPhases[4] UNUSED = {0.0f, 2.0f, 4.0f, 6.0f};
/* Read by no routine */
static s32 f_UnusedWordsAfterCursorPhases[2] UNUSED = {0, 0};
/* Whether ReadBestShots found any recorded best shot */
static u8 f_HasBestShots = FALSE;
AssetId g_CharacterFaceAssets[CHARACTER_COUNT] = {0x07C5, 0x0755, 0x07A9,
    0x0747, 0x078D, 0x07B7, 0x07D3, 0x07E1, 0x0771, 0x079B, 0x07EF, 0x0763,
    0x077F, 0x098D, 0x07FD, 0x080B, 0x0819, 0x0827};
// clang-format off
s16 g_CharacterPaletteFiles[CHARACTER_COUNT]
                            [CHARACTER_PALETTE_COUNT] = {
  { 0x07B8, 0x07B9, 0x07BA, 0x07BB },
  { 0x0748, 0x0749, 0x074A, 0x074B },
  { 0x079C, 0x079D, 0x079E, 0x079F },
  { 0x073A, 0x073B, 0x073C, 0x073D },
  { 0x0780, 0x0781, 0x0782, 0x0783 },
  { 0x07AA, 0x07AB, 0x07AC, 0x07AD },
  { 0x07C6, 0x07C7, 0x07C8, 0x07C9 },
  { 0x07D4, 0x07D5, 0x07D6, 0x07D7 },
  { 0x0764, 0x0765, 0x0766, 0x0767 },
  { 0x078E, 0x078F, 0x0790, 0x0791 },
  { 0x07E2, 0x07E3, 0x07E4, 0x07E5 },
  { 0x0756, 0x0757, 0x0758, 0x0759 },
  { 0x0772, 0x0773, 0x0774, 0x0775 },
  { 0x0980, 0x0981, 0x0982, 0x0983 },
  { 0x07F0, 0x07F1, 0x07F2, 0x07F3 },
  { 0x07FE, 0x07FF, 0x0800, 0x0801 },
  { 0x080C, 0x080D, 0x080E, 0x080F },
  { 0x081A, 0x081B, 0x081C, 0x081D }
};
// clang-format on
/* Read by no routine; a row like the six that follow the characters' rows of
 * g_CharacterFacePaletteFiles */
static s16 f_UnusedPaletteFileRow[4] UNUSED = {0x078E, 0x078E, 0x078E, 0x078E};
/* Read by no routine */
static s16 f_UnusedCharacterOrder[18] UNUSED = {
    0, 0, 1, 4, 5, 6, 3, 2, 7, 9, 12, 10, 13, 15, 14, 11, 8, 0};
/* The labels of the Best Shots rows */
static char f_RecordLabels[STATS_GRID_ROW_COUNT][RECORD_LABEL_SIZE] = {
    "Birdie", "Eagle", "Albatross", "Hole-in-One"};
/* The column and row step of each way the Best Shots cursor moves */
static GridStep f_BestShotDirectionSteps[STATS_DIRECTION_COUNT] = {
    {-1, 0}, {1, 0}, {0, -1}, {0, 1}};
/* Read by no routine; four widths, each followed by a height of 12 */
static s32 f_UnusedLabelSizes[8] UNUSED = {64, 12, 48, 12, 80, 12, 96, 12};
/* The Best Shots cursor's corners, whose colors DrawBestShotsCursor cycles */
static Vtx f_BestShotsCursorVertices[MENU_CURSOR_VERTEX_COUNT] = {
    {{{-48, 48, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-48, 0, 0}, 0, {0, 3072}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{0, 0, 0}, 0, {3072, 3072}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{0, 48, 0}, 0, {3072, 0}, {0xFF, 0xFF, 0x00, 0xFF}}}};
/* Read by no routine */
static s32 f_UnusedWordsAfterBestShotsCursor[2] UNUSED = {0, 0};
/* The angle, in radians, of each corner's color cycle */
static f32 f_BestShotsCursorPhases[MENU_CURSOR_VERTEX_COUNT] = {
    0.0f, 2.0f, 4.0f, 6.0f};
/* The CharacterChoice of each CharacterId */
static s32 f_StatsCharacterChoices[CHARACTER_COUNT] = {CHARACTER_CHOICE_PLUM,
    CHARACTER_CHOICE_CHARLIE, CHARACTER_CHOICE_MAPLE,
    CHARACTER_CHOICE_BABY_MARIO, CHARACTER_CHOICE_LUIGI, CHARACTER_CHOICE_PEACH,
    CHARACTER_CHOICE_SONNY, CHARACTER_CHOICE_WARIO, CHARACTER_CHOICE_HARRY,
    CHARACTER_CHOICE_MARIO, CHARACTER_CHOICE_YOSHI,
    CHARACTER_CHOICE_DONKEY_KONG, CHARACTER_CHOICE_BOWSER,
    CHARACTER_CHOICE_METAL_MARIO, CHARACTER_CHOICE_KID, CHARACTER_CHOICE_SHERRY,
    CHARACTER_CHOICE_AZALEA, CHARACTER_CHOICE_JOE};

/* The camera and placement that DrawHandicaps draws each golfer with */
static Mtx f_HandicapGolferTransforms[MAX_PLAYER_COUNT][DRAW_BUFFER_COUNT];
static Mtx f_HandicapGolfersProjection;
static LookAt f_HandicapGolfersLookAts[DRAW_BUFFER_COUNT];

/* Read by no routine */
static u8 f_UnusedBufferAfterGolferLookAts[0x100] UNUSED;

/* The camera and placement of each arrow that DrawTurnedPageArrows draws */
static Mtx f_TurnedArrowProjections[PAGE_ARROW_COUNT];
static Mtx f_TurnedArrowTranslations[PAGE_ARROW_COUNT];
static Mtx f_TurnedArrowRotations[PAGE_ARROW_COUNT];

/* Read by no routine */
static u8 f_UnusedBufferAfterArrowRotations[0x100] UNUSED;

/* The scale of each arrow that DrawTurnedPageArrows draws */
static Mtx f_TurnedArrowScales[PAGE_ARROW_COUNT];
/* Every bet icon, BET_ICON_SLOT_SIZE apart */
static void* f_BetIcons;
/* Set by LoadPlayerStatsBuffer, and read by no routine */
static void* f_UnusedStatsBuffer;
static void* f_StakeMark;
static void* f_StatsNamePlate;
/* The two frames of the bet icon that DrawBetIcon animates */
static void* f_ChosenBetIconFrames[BET_ICON_FRAME_COUNT];
static void* f_StatsGreenArrows;
static void* f_PageArrows;
static void* f_HandicapPlaque;
static void* f_BetLabels;
static void* f_StatsPlayerLabels;
static void* f_BrickWall;

/* Read by no routine */
static u8 f_UnusedBufferAfterBrickWall[0xA0] UNUSED;

/* The camera and placement that DrawBestShotsCursor draws the cursor with */
static Mtx f_BestShotsCursorProjection;
static Mtx f_BestShotsCursorTranslation;
static Mtx f_BestShotsCursorRotation;

/* Read by no routine */
static u8 f_UnusedBufferBeforeBestShotsScale[0x80] UNUSED;

static Mtx f_BestShotsCursorScale;

/* Read by no routine */
static u8 f_UnusedBufferAfterBestShotsScale[0x50] UNUSED;

/* The Best Shots screen's images: each cell's face, then the course icons,
 * the name plate, the cursor box and the score labels */
static void* f_BestShotFaces[STATS_GRID_ROW_COUNT][STATS_GRID_COLUMN_COUNT];
static void** f_BestShotsCourseIcons;
static void* f_BestShotsNamePlate;
static void* f_BestShotsCursorBox;
static u8* f_ScoreLabels;
/* Each Best Shots cell's ranked record, or STATS_CELL_EMPTY; a row per kind */
static s16 f_BestShotRecords[STATS_GRID_ROW_COUNT][STATS_GRID_COLUMN_COUNT];

/* Not static: the original left it to the linker's common block */
s16 g_ChosenHandicaps[MAX_PLAYER_COUNT];

void LoadPlayerStatsBuffer(void* buffer, s16 bufferKind) {
  EncodedAsset asset;
  MenuMemory* memory = GetMenuMemory();
  s16 iconIndex;
  BetSet betSet;
  AssetId assetId;
  void** imageSlot;

  switch (bufferKind) {
    case PLAYER_STATS_BUFFER_BET_ICONS:
      f_BetIcons = buffer;
      for (iconIndex = 0; iconIndex != BET_ICON_COUNT; iconIndex++) {
        OpenEncodedAsset(f_BetIconAssets[iconIndex], &asset);
        ReadEncodedAsset(
            ImageAt(f_BetIcons, iconIndex * BET_ICON_SLOT_SIZE), &asset);
      }

      break;
    case PLAYER_STATS_BUFFER_STAKE_MARK:
      f_StakeMark = buffer;
      OpenEncodedAsset(ASSET_STAKE_MARK, &asset);
      ReadEncodedAsset(f_StakeMark, &asset);
      break;
    case PLAYER_STATS_BUFFER_NAME_PLATE:
      f_StatsNamePlate = buffer;
      break;
    case PLAYER_STATS_BUFFER_BET_ICON_FIRST_FRAME:
      betSet = g_BetSet;
      imageSlot = &f_ChosenBetIconFrames[0];
      *imageSlot = buffer;
      OpenEncodedAsset(
          f_BetIconFrameAssets[f_BetSetBets[betSet - 1][memory->betColumn]][0],
          &asset);
      ReadEncodedAsset(*imageSlot, &asset);
      break;
    case PLAYER_STATS_BUFFER_BET_ICON_SECOND_FRAME:
      betSet = g_BetSet;
      imageSlot = &f_ChosenBetIconFrames[1];
      *imageSlot = buffer;
      OpenEncodedAsset(
          f_BetIconFrameAssets[f_BetSetBets[betSet - 1][memory->betColumn]][1],
          &asset);
      ReadEncodedAsset(*imageSlot, &asset);
      break;
    case PLAYER_STATS_BUFFER_UNUSED:
      f_UnusedStatsBuffer = buffer;
      break;
    case PLAYER_STATS_BUFFER_GREEN_ARROWS:
      f_StatsGreenArrows = buffer;
      OpenEncodedAsset(ASSET_GREEN_ARROWS, &asset);
      ReadEncodedAsset(f_StatsGreenArrows, &asset);
      break;
    case PLAYER_STATS_BUFFER_HANDICAP_PLAQUE:
      f_HandicapPlaque = buffer;
      OpenEncodedAsset(ASSET_HANDICAP_PLAQUE, &asset);
      ReadEncodedAsset(f_HandicapPlaque, &asset);
      break;
    case PLAYER_STATS_BUFFER_BET_LABELS:
      f_BetLabels = buffer;
      OpenEncodedAsset(ASSET_BET_LABELS, &asset);
      ReadEncodedAsset(f_BetLabels, &asset);
      break;
    case PLAYER_STATS_BUFFER_PLAYER_LABELS:
      f_StatsPlayerLabels = buffer;
      OpenEncodedAsset(ASSET_PLAYER_LABELS, &asset);
      ReadEncodedAsset(f_StatsPlayerLabels, &asset);
      break;
    case PLAYER_STATS_BUFFER_BRICK_WALL:
      f_BrickWall = buffer;
      OpenEncodedAsset(ASSET_STATS_BRICK_WALL, &asset);
      ReadEncodedAsset(f_BrickWall, &asset);
      break;
    case PLAYER_STATS_BUFFER_TRIPLE_PAGE_ARROWS:
      assetId = ASSET_TRIPLE_PAGE_ARROWS;
      f_PageArrows = buffer;
      OpenEncodedAsset(assetId, &asset);
      ReadEncodedAsset(f_PageArrows, &asset);
      break;
    case PLAYER_STATS_BUFFER_DOUBLE_PAGE_ARROWS:
      assetId = ASSET_DOUBLE_PAGE_ARROWS;
      f_PageArrows = buffer;
      OpenEncodedAsset(assetId, &asset);
      ReadEncodedAsset(f_PageArrows, &asset);
      break;
  }

  g_AnimatedIconFrameTime = 0;
  g_AnimatedIconFrameIndex = 0;
  g_AnimatedIconBufferIndex = 0;
  f_IsBetHelpShown = FALSE;
  f_IsHandicapPromptShown = FALSE;
}

/*
 * Sets each bet's stake level and rate from the mode's preset, or none, and
 * resets the handicaps
 */
void ApplyBetPreset(s32 requestedPreset) {
  s8 presetIndex = requestedPreset;
  MenuMemory* memory = GetMenuMemory();
  s16 index;

  if ((u8)requestedPreset < BET_PRESET_COUNT) {
    if ((s8)requestedPreset == BET_PRESET_STROKE &&
        g_ChosenPlayerCount == MAX_PLAYER_COUNT) {
      presetIndex = BET_PRESET_STROKE_FOUR_PLAYER;
    }

    if (presetIndex == BET_PRESET_MINI_GOLF &&
        g_ChosenPlayerCount == MAX_PLAYER_COUNT) {
      presetIndex = BET_PRESET_MINI_GOLF_FOUR_PLAYER;
    }

    for (index = 0; index != BET_STAKE_SLOT_COUNT; index++) {
      memory->betStakeLevels[index] =
          f_BetPresetStakeLevels[presetIndex][index];
      g_GameState.betRates[index] =
          f_BetStakeAmounts[memory->betStakeLevels[index]];
    }
  } else {
    for (index = 0; index != BET_STAKE_SLOT_COUNT; index++) {
      memory->betStakeLevels[index] = NO_BET;
      g_GameState.betRates[index] = UNSET_BET_RATE;
    }
  }

  memory->betColumn = 0;
  for (index = 0; index != MAX_PLAYER_COUNT; index++) {
    g_ChosenHandicaps[index] =
        index < g_ChosenPlayerCount ? 0 : ABSENT_PLAYER_HANDICAP;
  }

  memory->handicapPlayer = 0;
  memory->unk2A = 0;
}

/* The asset of the given frame of the chosen bet column's icon */
static inline AssetId ChosenBetIconFrame(MenuMemory* memory, s32 frame) {
  return f_BetIconFrameAssets[f_BetSetBets[g_BetSet - 1][memory->betColumn]]
                             [frame];
}

/*
 * Handles the bet settings screen's buttons. Up and down raise and lower the
 * chosen column's stake, and left and right move between columns, showing
 * the new column's help text and icon. A confirms the stakes and goes on to
 * the handicaps; B cancels. Either way the stakes become the game's bet
 * rates.
 */
void UpdateBetSettings(void) {
  EncodedAsset asset;
  MenuMemory* memory;
  s16 index;

  GetSaveData();
  memory = GetMenuMemory();
  if (g_BetPagesPhase == MENU_PHASE_START) {
    PlaySong(SONG_TITLE, 0);
    g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
    g_BetPagesWallSlide = STATS_SLIDE_IN_FROM_RIGHT;
  }

  if (g_BetPagesPhase != BET_PHASE_CHOOSING) {
    return;
  }

  if (g_MenuPressedButtons & A_BUTTON) {
    PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
    g_MenuFidgetDelays[0] = guRandom() % FIDGET_DELAY_RANGE;
    g_MenuFidgetDelays[1] = guRandom() % FIDGET_DELAY_RANGE;
    g_MenuFidgetDelays[2] = guRandom() % FIDGET_DELAY_RANGE;
    g_MenuFidgetDelays[3] = guRandom() % FIDGET_DELAY_RANGE;
    for (index = 0; index != MAXCONTROLLERS; index++) {
      g_IsMenuCharacterLoaded[index] = 0;
    }

    g_BetPagesPhase = BET_PHASE_CONFIRMED;
    FreeAllCharacterSlots();
    g_MenuScreen = MENU_SCREEN_HANDICAPS;
    f_IsBetHelpShown = FALSE;
    SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED, 255,
        255, 255, 255);
    for (index = 0; index != BET_STAKE_SLOT_COUNT; index++) {
      if (memory->betStakeLevels[index] != NO_BET) {
        g_GameState.betRates[index] =
            f_BetStakeAmounts[memory->betStakeLevels[index]];
      } else {
        g_GameState.betRates[index] = UNSET_BET_RATE;
      }
    }

    return;
  }

  if (g_MenuPressedButtons & B_BUTTON) {
    PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    g_BetPagesPhase = BET_PHASE_CANCELED;
    f_IsBetHelpShown = FALSE;
    SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED, 255,
        255, 255, 255);
    for (index = 0; index != BET_STAKE_SLOT_COUNT; index++) {
      if (memory->betStakeLevels[index] != NO_BET) {
        g_GameState.betRates[index] =
            f_BetStakeAmounts[memory->betStakeLevels[index]];
      } else {
        g_GameState.betRates[index] = UNSET_BET_RATE;
      }
    }

    return;
  }

  if (g_MenuRepeatButtons & U_JPAD) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    index = f_BetSetStakeSlots[g_BetSet - 1][memory->betColumn];
    if (memory->betStakeLevels[index] < BET_STAKE_LEVEL_COUNT - 1) {
      memory->betStakeLevels[index]++;
    }
  } else if (g_MenuRepeatButtons & D_JPAD) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    index = f_BetSetStakeSlots[g_BetSet - 1][memory->betColumn];
    if (memory->betStakeLevels[index] > 0) {
      memory->betStakeLevels[index]--;
    }
  } else if (g_MenuRepeatButtons & L_JPAD) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    memory->betColumn--;
    if (memory->betColumn < 0) {
      memory->betColumn = f_BetSetColumnCounts[g_BetSet - 1] - 1;
    }

    SetTextSlot(BET_HELP_X, BET_HELP_Y,
        f_BetSetBets[g_BetSet - 1][memory->betColumn] + BET_HELP_MESSAGE,
        TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED, 255, 255, 255, 255);
    OpenEncodedAsset(
        ChosenBetIconFrame(memory, g_AnimatedIconFrameIndex), &asset);
    ReadEncodedAsset(
        f_ChosenBetIconFrames[g_AnimatedIconBufferIndex ^ 1], &asset);
    g_AnimatedIconBufferIndex ^= 1;
  } else if (g_MenuRepeatButtons & R_JPAD) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    memory->betColumn++;
    if (memory->betColumn > f_BetSetColumnCounts[g_BetSet - 1] - 1) {
      memory->betColumn = 0;
    }

    SetTextSlot(BET_HELP_X, BET_HELP_Y,
        f_BetSetBets[g_BetSet - 1][memory->betColumn] + BET_HELP_MESSAGE,
        TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED, 255, 255, 255, 255);
    OpenEncodedAsset(
        ChosenBetIconFrame(memory, g_AnimatedIconFrameIndex), &asset);
    ReadEncodedAsset(
        f_ChosenBetIconFrames[g_AnimatedIconBufferIndex ^ 1], &asset);
    g_AnimatedIconBufferIndex ^= 1;
  }
}

/*
 * Each bet's column: its icon at the bottom, the stake's amount above it, and
 * above that a stack of five stake marks, those up to the stake lit
 */
#define BET_ICON_TOP 120
#define CHOSEN_BET_ICON_TOP 124
#define BET_AMOUNT_OFFSET_X (-4)
#define BET_AMOUNT_Y 106
#define STAKE_MARK_SIZE 8
#define STAKE_MARK_OFFSET_X 8
#define STAKE_MARK_BOTTOM 102
#define STAKE_MARK_SPACING 8
/* The marks of the top two levels are larger, and the top one is raised */
#define LARGE_STAKE_LEVEL 3
#define LARGE_STAKE_GROWTH 4
#define LARGEST_STAKE_LEVEL 4
#define LARGEST_STAKE_GROWTH 6
#define LARGEST_STAKE_RAISE 3
#define STAKE_MARK_COUNT 5
/*
 * The banner of f_StatsNamePlate is drawn twice: stretched to 270 columns in
 * overlapping 20-row strips, then narrowed to 160 columns at its own height
 */
#define WIDE_BANNER_LEFT 26
#define WIDE_BANNER_RIGHT 296
#define WIDE_BANNER_TOP 180
#define WIDE_BANNER_RAISE 6
#define WIDE_BANNER_STRIP_HEIGHT 20
#define WIDE_BANNER_STRIP_SPACING 16
#define BET_BANNER_LEFT 80
#define BET_BANNER_RIGHT 240
#define BET_BANNER_TOP 152
/* The chosen bet's label is centered on the banner */
#define BET_LABEL_CENTER_X 160
#define BET_LABEL_TOP 156

/*
 * Draws the bet settings screen: each bet's icon and stake marks shifted right
 * by slide, and the banner and chosen bet's name moved down by slide (or up,
 * when isSlideReversed is set). Once the screen has settled it types the
 * chosen bet's explanation
 */
/*
 * Doesn't match: the original keeps its g_BetSet loads in other registers
 * and schedules the stake marks' coordinates differently
 */
#ifdef NON_MATCHING
void DrawBetSettings(
    Gfx** displayListHead, s32 slide, u8 isSlideReversed, u8 isSettled) {
  Gfx* gfx = *displayListHead;
  MenuMemory* memory = GetMenuMemory();
  /* One index for the bet columns and the banner strips, to match */
  s16 i;
  s16 stakeLevel;
  s16 betIndex;
  s16 markGrowth;
  s32 markRaise;
  s16 bannerShift;
  s32 columnX;
  u8* icon;

  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);

  for (i = 0; i != f_BetSetColumnCounts[g_BetSet - 1]; i++) {
    betIndex = f_BetSetBets[g_BetSet - 1][i];
    if (betIndex == -1) {
      break;
    }

    if (i != memory->betColumn) {
      icon = ImageAt(f_BetIcons, betIndex * BET_ICON_SLOT_SIZE);
      gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(icon)));
      /* Bets without a stake are dimmed */
      if (memory->betStakeLevels[f_BetSetStakeSlots[g_BetSet - 1][i]] == 0) {
        gDPSetCombineMode(
            gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, 127, 127, 127, 255);
      } else {
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      }

      gDPLoadTextureTile_4b(gfx++, TextureAddress(ImageTexels(icon)),
          G_IM_FMT_CI, BET_ICON_SIZE, BET_ICON_SIZE, 0, 0, BET_ICON_SIZE - 1,
          BET_ICON_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++,
          QuarterPixels(f_BetSetColumnXs[g_BetSet - 1][i] + slide),
          QuarterPixels(BET_ICON_TOP),
          QuarterPixels(
              f_BetSetColumnXs[g_BetSet - 1][i] + slide + BET_ICON_SIZE),
          QuarterPixels(BET_ICON_TOP + BET_ICON_SIZE), G_TX_RENDERTILE, 0, 0,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }

    DrawUnalignedMenuNumber(&gfx,
        f_BetSetColumnXs[g_BetSet - 1][i] + slide + BET_AMOUNT_OFFSET_X,
        BET_AMOUNT_Y,
        (s8)f_BetStakeAmounts[memory
                ->betStakeLevels[f_BetSetStakeSlots[g_BetSet - 1][i]]],
        0);
  }

  /* The chosen bet's icon is animated */
  DrawBetIcon(&gfx, f_BetSetColumnXs[g_BetSet - 1][memory->betColumn] + slide,
      CHOSEN_BET_ICON_TOP, BET_ICON_SIZE, TEXEL_STEP_ONE);

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTextureTile_4b(gfx++, TextureAddress(ImageTexels(f_StakeMark)),
      G_IM_FMT_CI, STAKE_MARK_SIZE, STAKE_MARK_SIZE, 0, 0, STAKE_MARK_SIZE - 1,
      STAKE_MARK_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_StakeMark)));
  gDPPipeSync(gfx++);

  markGrowth = 0;
  markRaise = 0;
  for (stakeLevel = 0; stakeLevel != STAKE_MARK_COUNT; stakeLevel++) {
    if (stakeLevel == LARGE_STAKE_LEVEL) {
      markGrowth = LARGE_STAKE_GROWTH;
      markRaise = 0;
    }

    if (stakeLevel == LARGEST_STAKE_LEVEL) {
      markGrowth = LARGEST_STAKE_GROWTH;
      markRaise = LARGEST_STAKE_RAISE;
    }

    for (i = 0; i != f_BetSetColumnCounts[g_BetSet - 1]; i++) {
      /* Marks above the bet's stake are dimmed */
      if (stakeLevel >=
          memory->betStakeLevels[f_BetSetStakeSlots[g_BetSet - 1][i]]) {
        gDPSetCombineMode(
            gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, 127, 127, 127, 255);
      } else {
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      }

      IN_OWN_BLOCK(gDPPipeSync(gfx++));

      columnX = f_BetSetColumnXs[g_BetSet - 1][i] + slide + STAKE_MARK_OFFSET_X;
      gSPScisTextureRectangle(gfx++, QuarterPixels(columnX - (markGrowth >> 1)),
          QuarterPixels((STAKE_MARK_BOTTOM - STAKE_MARK_SIZE) - markGrowth -
              (stakeLevel * STAKE_MARK_SPACING) - markRaise),
          QuarterPixels(columnX + ((markGrowth >> 1) + STAKE_MARK_SIZE)),
          QuarterPixels(STAKE_MARK_BOTTOM - markRaise -
              (stakeLevel * STAKE_MARK_SPACING)),
          G_TX_RENDERTILE, 0, 0,
          (STAKE_MARK_SIZE << 10) / (markGrowth + STAKE_MARK_SIZE),
          (STAKE_MARK_SIZE << 10) / (markGrowth + STAKE_MARK_SIZE));
      gDPPipeSync(gfx++);
    }
  }

  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_StatsNamePlate)));

  bannerShift = slide;
  if (isSlideReversed) {
    bannerShift = -slide;
  }

  for (i = 0; i != MENU_BANNER_STRIP_COUNT; i++) {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(f_StatsNamePlate) +
            (i * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)),
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, QuarterPixels(WIDE_BANNER_LEFT),
        QuarterPixels(bannerShift +
            ((i * WIDE_BANNER_STRIP_SPACING) + WIDE_BANNER_TOP) -
            WIDE_BANNER_RAISE),
        QuarterPixels(WIDE_BANNER_RIGHT),
        QuarterPixels(bannerShift +
            ((i * WIDE_BANNER_STRIP_SPACING) + WIDE_BANNER_TOP) -
            WIDE_BANNER_RAISE + WIDE_BANNER_STRIP_HEIGHT),
        G_TX_RENDERTILE, 0, 0,
        (MENU_BANNER_WIDTH << 10) / (WIDE_BANNER_RIGHT - WIDE_BANNER_LEFT),
        (MENU_BANNER_STRIP_HEIGHT << 10) / WIDE_BANNER_STRIP_HEIGHT);
    gDPPipeSync(gfx++);
  }

  for (i = 0; i != MENU_BANNER_STRIP_COUNT; i++) {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(f_StatsNamePlate) +
            (i * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)),
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, QuarterPixels(BET_BANNER_LEFT),
        QuarterPixels(
            bannerShift + ((i * MENU_BANNER_STRIP_HEIGHT) + BET_BANNER_TOP)),
        QuarterPixels(BET_BANNER_RIGHT),
        QuarterPixels(bannerShift +
            ((i * MENU_BANNER_STRIP_HEIGHT) + BET_BANNER_TOP) +
            MENU_BANNER_STRIP_HEIGHT),
        G_TX_RENDERTILE, 0, 0,
        (MENU_BANNER_WIDTH << 10) / (BET_BANNER_RIGHT - BET_BANNER_LEFT),
        TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  betIndex = f_BetSetBets[g_BetSet - 1][memory->betColumn];
  DrawBetLabel(&gfx, BET_LABEL_CENTER_X, bannerShift + BET_LABEL_TOP, betIndex);
  if (!f_IsBetHelpShown && isSettled == TRUE) {
    SetTextSlot(BET_HELP_X, BET_HELP_Y, betIndex + BET_HELP_MESSAGE,
        TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED, 255, 255, 255, 255);
    f_IsBetHelpShown = TRUE;
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", DrawBetSettings);
#endif

/*
 * Draws the chosen bet's icon size pixels square at (left, top), and reads its
 * next frame once the current one has shown long enough
 */
void DrawBetIcon(
    Gfx** displayListHead, s32 left, s32 top, s32 size, s32 textureStep) {
  Gfx* gfx = *displayListHead;
  MenuMemory* memory = GetMenuMemory();
  EncodedAsset asset;
  void* image;
  u8 iconIndex;

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  image = f_ChosenBetIconFrames[g_AnimatedIconBufferIndex];
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(image)));
  gDPLoadTextureTile_4b(gfx++, TextureAddress(ImageTexels(image)), G_IM_FMT_CI,
      BET_ICON_SIZE, BET_ICON_SIZE, 0, 0, BET_ICON_SIZE - 1, BET_ICON_SIZE - 1,
      0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gSPScisTextureRectangle(gfx++, QuarterPixels(left), QuarterPixels(top),
      QuarterPixels(left + size), QuarterPixels(top + size), G_TX_RENDERTILE, 0,
      0, textureStep, textureStep);
  g_AnimatedIconFrameTime++;
  gDPPipeSync(gfx++);
  if (g_AnimatedIconFrameTime == BET_ICON_FRAME_DURATION) {
    g_AnimatedIconFrameTime = 0;
    g_AnimatedIconFrameIndex++;
    if (g_AnimatedIconFrameIndex >= BET_ICON_FRAME_COUNT) {
      g_AnimatedIconFrameIndex = 0;
    }

    g_AnimatedIconBufferIndex = !g_AnimatedIconBufferIndex;
    iconIndex = f_BetSetBets[g_BetSet - 1][memory->betColumn];
    OpenEncodedAsset(
        f_BetIconFrameAssets[iconIndex][g_AnimatedIconFrameIndex], &asset);
    ReadEncodedAsset(f_ChosenBetIconFrames[g_AnimatedIconBufferIndex], &asset);
  }

  *displayListHead = gfx;
}

/*
 * Runs the handicap screen. UpdateMenus passes it the first player and the
 * player count, as it does UpdateCharacterSelect, but it ignores them.
 */
NO_RESULT UpdateHandicaps(s32 firstPlayerIndex UNUSED, s32 playerCount UNUSED) {
  MenuMemory* memory = GetMenuMemory();

  if (g_BetPagesPhase == MENU_PHASE_START) {
    PlaySong(SONG_TITLE, 0);
    g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
    g_BetPagesWallSlide = STATS_SLIDE_IN_FROM_LEFT;
    FreeAllCharacterSlots();
  }

  if (g_BetPagesPhase == MENU_PHASE_CHOOSING) {
    if (g_MenuPressedButtons & A_BUTTON) {
      PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED,
          0xFF, 0xFF, 0xFF, 0xFF);
      g_CourseBannersSlide = HANDICAP_SLIDE_START;
      f_IsHandicapPromptShown = FALSE;
      g_BetPagesPhase = MENU_PHASE_CONFIRMED;
      return;
    }

    if (g_MenuPressedButtons & B_BUTTON) {
      PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      g_MenuScreen = MENU_SCREEN_BET_SETTINGS;
      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED,
          0xFF, 0xFF, 0xFF, 0xFF);
      f_IsHandicapPromptShown = FALSE;
      g_BetPagesPhase = MENU_PHASE_CANCELED;
      return;
    }

    if (g_MenuRepeatButtons & L_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      memory->handicapPlayer--;
      if (memory->handicapPlayer < 0) {
        memory->handicapPlayer = g_ChosenPlayerCount - 1;
      }

      f_IsHandicapPromptShown = FALSE;
      return;
    }

    if (g_MenuRepeatButtons & R_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      memory->handicapPlayer++;
      if (memory->handicapPlayer > g_ChosenPlayerCount - 1) {
        memory->handicapPlayer = 0;
      }

      f_IsHandicapPromptShown = FALSE;
      return;
    }

    if (g_MenuRepeatButtons & U_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      g_ChosenHandicaps[memory->handicapPlayer]++;
      if (g_ChosenHandicaps[memory->handicapPlayer] >= HANDICAP_CHOICE_COUNT) {
        g_ChosenHandicaps[memory->handicapPlayer] = 0;
      }

      return;
    }

    if (g_MenuRepeatButtons & D_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      g_ChosenHandicaps[memory->handicapPlayer]--;
      if (g_ChosenHandicaps[memory->handicapPlayer] < 0) {
        g_ChosenHandicaps[memory->handicapPlayer] = HANDICAP_CHOICE_COUNT - 1;
      }
    }
  }
}

/* The 200x30 CI8 banner along the handicap screen's foot, drawn in strips of 10
 * rows, each stretched across the screen */
#define HANDICAP_BANNER_WIDTH 200
#define HANDICAP_BANNER_STRIP_HEIGHT 10
#define HANDICAP_BANNER_STRIP_COUNT 3
#define HANDICAP_BANNER_STRIP_SIZE \
  (HANDICAP_BANNER_WIDTH * HANDICAP_BANNER_STRIP_HEIGHT)
#define HANDICAP_BANNER_LEFT 26
#define HANDICAP_BANNER_RIGHT 296
#define HANDICAP_BANNER_TOP 190
#define HANDICAP_BANNER_STEP_X \
  ((HANDICAP_BANNER_WIDTH << 10) / \
      (HANDICAP_BANNER_RIGHT - HANDICAP_BANNER_LEFT))

/* Each player's 64x44 CI4 column, drawn in strips of 22 rows, with the
 * player's label and choice on it */
#define HANDICAP_COLUMN_WIDTH 64
#define HANDICAP_COLUMN_STRIP_HEIGHT 22
#define HANDICAP_COLUMN_STRIP_COUNT 2
#define HANDICAP_COLUMN_STRIP_SIZE \
  (HANDICAP_COLUMN_WIDTH * HANDICAP_COLUMN_STRIP_HEIGHT / 2)
#define HANDICAP_COLUMN_TOP 150
/* The columns of the players other than the chosen one are dimmed */
#define UNCHOSEN_COLUMN_SHADE 200
#define HANDICAP_LABEL_OFFSET_X 4
#define HANDICAP_LABEL_TOP 154
#define HANDICAP_LABEL_TMEM 0x80
#define HANDICAP_LABEL_TILE 1
#define HANDICAP_LABEL_PALETTE 1
#define HANDICAP_NUMBER_OFFSET_X 16
#define HANDICAP_NUMBER_Y 166

/* The arrows around the chosen column, each pushed outward while its D-Pad
 * button is held. The left arrow is the right one's image mirrored. */
#define HANDICAP_ARROW_MASK 4
#define HANDICAP_ARROW_PUSH 6
#define HANDICAP_ARROW_MIDDLE_X 24
#define HANDICAP_ARROW_MIDDLE_Y 15
#define HANDICAP_ARROW_UP_Y 0
#define HANDICAP_ARROW_DOWN_Y 30
#define HANDICAP_ARROW_RIGHT_X 48
#define HANDICAP_ARROW_LEFT_X (-1)

/* The prompt, which names the chosen player */
#define HANDICAP_PROMPT_X 36
#define HANDICAP_PROMPT_Y 200
#define HANDICAP_PROMPT_MESSAGE 0x1D

void DrawTwoDigitMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);

/*
 * Draws the handicap screen: the players' characters, a column for each player
 * with the player's label and handicap, the banner below them, and the
 * arrows around the column of the player that memory->handicapPlayer picks. All
 * but the banner slide across by slide, and the banner and arrows down by it,
 * or up when reverseVerticalSlide is set. With canShowPrompt set, the prompt
 * naming that player is typed once each time f_IsHandicapPromptShown is
 * cleared.
 */
/*
 * Doesn't match: the original adds the label's offset to the slide, and the
 * columns' top to the vertical slide, once before their loops, and adds the
 * texel offset to the blink frame's offset before the arrow's.
 */
#ifdef NON_MATCHING
void DrawHandicaps(Gfx** displayListHead, s32 slide, u8 reverseVerticalSlide,
    u8 canShowPrompt) {
  Gfx* gfx = *displayListHead;
  MenuMemory* memory = GetMenuMemory();
  u16 playerIndex;
  s32 columnIndex;
  s32 stripIndex;
  s32 arrowIndex;
  s32 labelIndex;
  s32 offsetY;
  s32 arrowX;
  s32 arrowY = 0;
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
  u8 unusedStack[0x30] UNUSED;
  Mat4 transform;

  BuildDynamicTextures(&gfx);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_MAGICSQ);
  gDPPipeSync(gfx++);
  guOrtho(&f_HandicapGolfersProjection, -(SCREEN_WIDTH / 2), SCREEN_WIDTH / 2,
      -(SCREEN_HEIGHT / 2), SCREEN_HEIGHT / 2, MENU_CHARACTERS_NEAR,
      MENU_CHARACTERS_FAR, MENU_CHARACTERS_ORTHO_SCALE);
  {
    Mat4 viewing;

    guLookAtReflectF(viewing, &f_HandicapGolfersLookAts[g_DrawBufferIndex],
        0.0f, MENU_CHARACTERS_EYE_Y, MENU_CHARACTERS_EYE_Z, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f);
  }

  gSPLookAt(gfx++, &f_HandicapGolfersLookAts[g_DrawBufferIndex]);
  arrowX = 0;
  for (playerIndex = 0; playerIndex != g_ChosenPlayerCount; playerIndex++) {
    if (g_MenuFidgetDelays[playerIndex] > 0) {
      g_MenuFidgetDelays[playerIndex]--;
    }

    if (g_MenuFidgetDelays[playerIndex] == 0) {
      g_MenuFidgetDelays[playerIndex] = FIDGETING;
      SetAnimationLooping(playerIndex, FALSE);
      SetNextAnimation(playerIndex, MENU_ANIMATION_FIDGET);
    }

    if (g_MenuFidgetDelays[playerIndex] == FIDGETING) {
      if (GetAnimationEndFrame(playerIndex, MENU_ANIMATION_FIDGET) ==
          GetAnimationFrame(playerIndex)) {
        SetCharacterAnimation(playerIndex, MENU_ANIMATION_STANDING);
        SetAnimationLooping(playerIndex, TRUE);
        g_MenuFidgetDelays[playerIndex] = guRandom() % FIDGET_DELAY_RANGE;
      }
    }

    if (!g_IsMenuCharacterLoaded[playerIndex]) {
      if (IsCharacterSlotBusy(playerIndex)) {
        FreeCharacterSlot(playerIndex);
      } else {
        LoadCharacter(playerIndex,
            (s8)g_Players[playerIndex].characterIndex + CHARACTER_COUNT,
            (s8)g_Players[playerIndex].paletteIndex);
        g_IsMenuCharacterLoaded[playerIndex] = TRUE;
        SetCharacterAnimation(playerIndex, MENU_ANIMATION_STANDING);
        SetAnimationSpeed(playerIndex, 1.0f);
      }
    }

    rotation[0] = DegreesToRadians(f_HandicapGolferAngles[0]);
    rotation[1] = DegreesToRadians(f_HandicapGolferAngles[1]);
    rotation[2] = 0.0f;
    translation[0] =
        f_HandicapGolferPositions[g_ChosenPlayerCount - 1][playerIndex][0] +
        slide;
    translation[1] =
        f_HandicapGolferPositions[g_ChosenPlayerCount - 1][playerIndex][1];
    translation[2] =
        f_HandicapGolferPositions[g_ChosenPlayerCount - 1][playerIndex][2];
    scale[0] = f_HandicapGolferScales[playerIndex];
    scale[1] = f_HandicapGolferScales[playerIndex];
    scale[2] = f_HandicapGolferScales[playerIndex];

    BuildTransformMatrix(transform, rotation, translation, scale);
    ConvertMatrixToFixed(
        transform, &f_HandicapGolferTransforms[playerIndex][g_DrawBufferIndex]);

    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, PhysicalAddress(&f_HandicapGolfersProjection),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++,
        PhysicalAddress(
            &f_HandicapGolferTransforms[playerIndex][g_DrawBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    /* DrawCharacter draws through g_DisplayListHead */
    g_DisplayListHead = gfx;
    StopLoaderThread();
    DrawCharacter(playerIndex);
    *displayListHead = g_DisplayListHead;
    gfx = *displayListHead;
    AnimateCharacter(playerIndex);
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_StatsNamePlate)));
  offsetY = slide;
  if (reverseVerticalSlide) {
    offsetY = -slide;
  }

  for (stripIndex = 0; stripIndex != HANDICAP_BANNER_STRIP_COUNT;
      stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(f_StatsNamePlate) +
            (stripIndex * HANDICAP_BANNER_STRIP_SIZE)),
        G_IM_FMT_CI, G_IM_SIZ_8b, HANDICAP_BANNER_WIDTH,
        HANDICAP_BANNER_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gSPScisTextureRectangle(gfx++, QuarterPixels(HANDICAP_BANNER_LEFT),
        QuarterPixels(offsetY + HANDICAP_BANNER_TOP +
            (stripIndex * HANDICAP_BANNER_STRIP_HEIGHT)),
        QuarterPixels(HANDICAP_BANNER_RIGHT),
        QuarterPixels(offsetY + HANDICAP_BANNER_TOP +
            (stripIndex * HANDICAP_BANNER_STRIP_HEIGHT) +
            HANDICAP_BANNER_STRIP_HEIGHT),
        G_TX_RENDERTILE, 0, 0, HANDICAP_BANNER_STEP_X, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_HandicapPlaque)));
  gDPLoadTLUT_pal16(gfx++, HANDICAP_LABEL_PALETTE,
      TextureAddress(ImageBody(f_StatsPlayerLabels)));
  for (columnIndex = 0; columnIndex < g_ChosenPlayerCount; columnIndex++) {
    if (columnIndex == memory->handicapPlayer) {
      gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    } else {
      gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
      gDPSetPrimColor(gfx++, 0, 0, UNCHOSEN_COLUMN_SHADE, UNCHOSEN_COLUMN_SHADE,
          UNCHOSEN_COLUMN_SHADE, 255);
    }

    for (stripIndex = 0; stripIndex != HANDICAP_COLUMN_STRIP_COUNT;
        stripIndex++) {
      gDPLoadTextureBlock_4b(gfx++,
          TextureAddress(ImageTexels(f_HandicapPlaque) +
              (stripIndex * HANDICAP_COLUMN_STRIP_SIZE)),
          G_IM_FMT_CI, HANDICAP_COLUMN_WIDTH, HANDICAP_COLUMN_STRIP_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++,
          QuarterPixels(
              f_HandicapColumnLefts[g_ChosenPlayerCount - 1][columnIndex] +
              slide),
          QuarterPixels(HANDICAP_COLUMN_TOP +
              (stripIndex * HANDICAP_COLUMN_STRIP_HEIGHT)),
          QuarterPixels(
              f_HandicapColumnLefts[g_ChosenPlayerCount - 1][columnIndex] +
              slide + HANDICAP_COLUMN_WIDTH),
          QuarterPixels(HANDICAP_COLUMN_TOP +
              (stripIndex * HANDICAP_COLUMN_STRIP_HEIGHT) +
              HANDICAP_COLUMN_STRIP_HEIGHT),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }

    labelIndex = columnIndex;
    if (g_PlayerControllerIndices[columnIndex] == NO_CONTROLLER_INDEX) {
      labelIndex = columnIndex + PLAYER_LABEL_COMPUTER_FIRST;
    }

    gDPLoadMultiBlock_4b(gfx++,
        TextureAddress(ImageTexelsAt(
            f_StatsPlayerLabels, labelIndex * PLAYER_LABEL_IMAGE_SIZE)),
        HANDICAP_LABEL_TMEM, HANDICAP_LABEL_TILE, G_IM_FMT_CI,
        PLAYER_LABEL_WIDTH, PLAYER_LABEL_HEIGHT, HANDICAP_LABEL_PALETTE,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++,
        QuarterPixels(
            f_HandicapColumnLefts[g_ChosenPlayerCount - 1][columnIndex] +
            (slide + HANDICAP_LABEL_OFFSET_X)),
        QuarterPixels(HANDICAP_LABEL_TOP),
        QuarterPixels(
            f_HandicapColumnLefts[g_ChosenPlayerCount - 1][columnIndex] +
            (slide + HANDICAP_LABEL_OFFSET_X) + PLAYER_LABEL_WIDTH),
        QuarterPixels(HANDICAP_LABEL_TOP + PLAYER_LABEL_HEIGHT),
        HANDICAP_LABEL_TILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  f_GreenArrowTimer++;
  if (f_GreenArrowTimer == STATS_ARROW_BLINK_FRAME_COUNT) {
    f_GreenArrowTimer = 0;
    f_GreenArrowFrame ^= 1;
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_StatsGreenArrows)));
  for (arrowIndex = 0; arrowIndex != MENU_ARROW_COUNT; arrowIndex++) {
    if (arrowIndex == MENU_ARROW_LEFT) {
      IN_OWN_BLOCK(gDPLoadTextureTile_4b(gfx++,
          TextureAddress((u32)f_StatsGreenArrows +
              ((f_GreenArrowFrame * GREEN_ARROW_FRAME_SIZE) +
                  PALETTED_TEXELS_OFFSET +
                  (MENU_ARROW_RIGHT * GREEN_ARROW_IMAGE_SIZE))),
          G_IM_FMT_CI, GREEN_ARROW_SIZE, GREEN_ARROW_SIZE, 0, 0,
          GREEN_ARROW_SIZE - 1, GREEN_ARROW_SIZE - 1, 0,
          G_TX_MIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP, HANDICAP_ARROW_MASK,
          HANDICAP_ARROW_MASK, G_TX_NOLOD, G_TX_NOLOD));
    } else {
      gDPLoadTextureTile_4b(gfx++,
          TextureAddress((u32)f_StatsGreenArrows +
              ((f_GreenArrowFrame * GREEN_ARROW_FRAME_SIZE) +
                  PALETTED_TEXELS_OFFSET +
                  (arrowIndex * GREEN_ARROW_IMAGE_SIZE))),
          G_IM_FMT_CI, GREEN_ARROW_SIZE, GREEN_ARROW_SIZE, 0, 0,
          GREEN_ARROW_SIZE - 1, GREEN_ARROW_SIZE - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    }

    gDPPipeSync(gfx++);
    if (arrowIndex == MENU_ARROW_UP) {
      arrowX = HANDICAP_ARROW_MIDDLE_X;
      arrowY = (g_MenuRepeatButtons & U_JPAD)
          ? (HANDICAP_ARROW_UP_Y - HANDICAP_ARROW_PUSH)
          : HANDICAP_ARROW_UP_Y;
    } else if (arrowIndex == MENU_ARROW_DOWN) {
      arrowX = HANDICAP_ARROW_MIDDLE_X;
      arrowY = (g_MenuRepeatButtons & D_JPAD)
          ? (HANDICAP_ARROW_DOWN_Y + HANDICAP_ARROW_PUSH)
          : HANDICAP_ARROW_DOWN_Y;
    } else if (arrowIndex == MENU_ARROW_RIGHT) {
      arrowY = HANDICAP_ARROW_MIDDLE_Y;
      arrowX = (g_MenuRepeatButtons & R_JPAD)
          ? (HANDICAP_ARROW_RIGHT_X + HANDICAP_ARROW_PUSH)
          : HANDICAP_ARROW_RIGHT_X;
    } else if (arrowIndex == MENU_ARROW_LEFT) {
      arrowY = HANDICAP_ARROW_MIDDLE_Y;
      arrowX = (g_MenuRepeatButtons & L_JPAD)
          ? (HANDICAP_ARROW_LEFT_X - HANDICAP_ARROW_PUSH)
          : HANDICAP_ARROW_LEFT_X;
    }

    /* The left arrow starts at the right edge of its mirrored image */
    gSPScisTextureRectangle(gfx++,
        QuarterPixels(f_HandicapColumnLefts[g_ChosenPlayerCount - 1]
                                           [memory->handicapPlayer] +
            arrowX + slide),
        QuarterPixels(offsetY + HANDICAP_COLUMN_TOP + arrowY),
        QuarterPixels(f_HandicapColumnLefts[g_ChosenPlayerCount - 1]
                                           [memory->handicapPlayer] +
            arrowX + slide + GREEN_ARROW_SIZE),
        QuarterPixels(
            offsetY + HANDICAP_COLUMN_TOP + arrowY + GREEN_ARROW_SIZE),
        G_TX_RENDERTILE,
        (arrowIndex == MENU_ARROW_LEFT) ? TexelCoordinate(GREEN_ARROW_SIZE) : 0,
        0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  for (columnIndex = 0; columnIndex != g_ChosenPlayerCount; columnIndex++) {
    DrawTwoDigitMenuNumber(&gfx,
        f_HandicapColumnLefts[g_ChosenPlayerCount - 1][columnIndex] +
            (slide + HANDICAP_NUMBER_OFFSET_X),
        HANDICAP_NUMBER_Y, g_ChosenHandicaps[columnIndex], MENU_NUMBER_PLAIN);
  }

  if ((f_IsHandicapPromptShown == FALSE) & (canShowPrompt == TRUE)) {
    SetTextSlotValue(memory->handicapPlayer + 1, 0, 0);

    /* The prompt names the player in the player's own color */
    if (memory->handicapPlayer == 0) {
      SetTextPaletteColor(255, 127, 127, 0);
    }

    if (memory->handicapPlayer == 1) {
      SetTextPaletteColor(0, 255, 255, 0);
    }

    if (memory->handicapPlayer == 2) {
      SetTextPaletteColor(255, 255, 0, 0);
    }

    if (memory->handicapPlayer == 3) {
      SetTextPaletteColor(63, 255, 63, 0);
    }

    SetTextSlot(HANDICAP_PROMPT_X, HANDICAP_PROMPT_Y, HANDICAP_PROMPT_MESSAGE,
        TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED, 255, 255, 255, 255);
    f_IsHandicapPromptShown = TRUE;
  }

  *displayListHead = gfx;
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", DrawHandicaps);
#endif

/* Draws a bet's name centered on centerX */
void DrawBetLabel(Gfx** displayListHead, s32 centerX, s32 top, u8 labelIndex) {
  Gfx* gfx = *displayListHead;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_BetLabels)));
  gDPLoadTextureBlock_4b(gfx++,
      TextureAddress(
          ImageTexels(f_BetLabels) + (labelIndex * BET_LABEL_IMAGE_SIZE)),
      G_IM_FMT_CI, BET_LABEL_WIDTH, BET_LABEL_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, QuarterPixels(centerX - (BET_LABEL_WIDTH / 2)),
      QuarterPixels(top), QuarterPixels(centerX + (BET_LABEL_WIDTH / 2)),
      QuarterPixels(top + BET_LABEL_HEIGHT), G_TX_RENDERTILE, 0, 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/*
 * Starts the brick wall's rows on slide once the last slide has ended, then
 * draws them; returns whether the rows have yet to line up
 */
s32 SlideBrickWall(Gfx** displayListHead, u8 slide) {
  Gfx* gfx = *displayListHead;
  s32 index;
  s32 rowIndex;
  s32 offset; /* The slide's first target, then the first row's offset */
  s32 first;

  if ((slide != STATS_SLIDE_CURRENT && slide < STATS_SLIDE_COUNT) == FALSE) {
    return -1;
  }

  offset = f_BrickSlideStartOffsets[slide - 1][0];
  first = f_BrickRowOffsets[0];
  for (index = 0; index != STATS_SLIDE_ROW_COUNT; index++) {
    if (f_BrickRowOffsets[index] != offset &&
        first != f_BrickRowOffsets[index]) {
      break;
    }
  }

  if (index == STATS_SLIDE_ROW_COUNT && f_BrickWallSlide != slide) {
    f_BrickWallSlide = slide;
    switch (slide) {
      case STATS_SLIDE_OUT_RIGHT:
        for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
          f_BrickRowOffsets[rowIndex] =
              f_BrickSlideStartOffsets[STATS_SLIDE_OUT_RIGHT - 1][rowIndex];
          f_BrickRowDelays[rowIndex] = f_BrickRowStartDelays[rowIndex];
        }

        break;
      case STATS_SLIDE_OUT_LEFT:
        for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
          f_BrickRowOffsets[rowIndex] =
              f_BrickSlideStartOffsets[STATS_SLIDE_OUT_LEFT - 1][rowIndex];
          f_BrickRowDelays[rowIndex] = f_BrickRowStartDelays[rowIndex];
        }

        break;
      case STATS_SLIDE_IN_FROM_RIGHT:
      case STATS_SLIDE_IN_FROM_LEFT:
        for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
          f_BrickRowOffsets[rowIndex] =
              f_BrickSlideStartOffsets[f_BrickWallSlide - 1][rowIndex];
          f_BrickRowDelays[rowIndex] = f_BrickRowStartDelays[rowIndex];
        }

        break;
    }
  }

  DrawBrickWallRows(&gfx, f_BrickWallSlide);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
  for (index = 0; index != STATS_SLIDE_ROW_COUNT; index++) {
    if ((offset = f_BrickRowOffsets[0]) != f_BrickRowOffsets[index]) {
      break;
    }
  }

  return index != STATS_SLIDE_ROW_COUNT;
}

/* Moves each brick wall row whose delay has run out a step along slide, and
 * draws the rows */
void DrawBrickWallRows(Gfx** displayListHead, u8 slide) {
  Gfx* gfx = *displayListHead;
  s32 rowIndex;
  s32 stripHeight = STATS_WALL_STRIP_HEIGHT;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_BrickWall)));
  for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(f_BrickWall) +
            (stripHeight * STATS_WALL_IMAGE_WIDTH * rowIndex)),
        G_IM_FMT_CI, G_IM_SIZ_8b, STATS_WALL_IMAGE_WIDTH, stripHeight, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    if (f_BrickRowDelays[rowIndex] > 0) {
      f_BrickRowDelays[rowIndex]--;
    } else {
      switch (slide) {
        case STATS_SLIDE_OUT_RIGHT:
          f_BrickRowOffsets[rowIndex] -= f_BrickRowSpeeds[rowIndex];
          if (f_BrickRowOffsets[rowIndex] <= 0) {
            f_BrickRowOffsets[rowIndex] = 0;
          }

          break;
        case STATS_SLIDE_OUT_LEFT:
          f_BrickRowOffsets[rowIndex] += f_BrickRowSpeeds[rowIndex];
          if (f_BrickRowOffsets[rowIndex] > 0) {
            f_BrickRowOffsets[rowIndex] = 0;
          }

          break;
        case STATS_SLIDE_IN_FROM_RIGHT:
          f_BrickRowOffsets[rowIndex] += f_BrickRowSpeeds[rowIndex];
          if (f_BrickRowOffsets[rowIndex] > SCREEN_WIDTH) {
            f_BrickRowOffsets[rowIndex] = SCREEN_WIDTH;
          }

          break;
        case STATS_SLIDE_IN_FROM_LEFT:
          f_BrickRowOffsets[rowIndex] -= f_BrickRowSpeeds[rowIndex];
          if (f_BrickRowOffsets[rowIndex] < -SCREEN_WIDTH) {
            f_BrickRowOffsets[rowIndex] = -SCREEN_WIDTH;
          }

          break;
      }
    }

    gSPScisTextureRectangle(gfx++, QuarterPixels(f_BrickRowOffsets[rowIndex]),
        QuarterPixels(rowIndex * STATS_WALL_ROW_HEIGHT),
        QuarterPixels(f_BrickRowOffsets[rowIndex] + SCREEN_WIDTH),
        QuarterPixels(
            (rowIndex * STATS_WALL_ROW_HEIGHT) + STATS_WALL_ROW_HEIGHT),
        G_TX_RENDERTILE, 0, 0, (STATS_WALL_IMAGE_WIDTH << 10) / SCREEN_WIDTH,
        (stripHeight << 10) / STATS_WALL_ROW_HEIGHT);
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* Draws the animated page arrows either side of the course select screen */
/*
 * Doesn't match: the original computes the mirrored arrow's start without a
 * branch, and keeps the display list head in its argument slot.
 */
#ifdef NON_MATCHING
void DrawPageArrows(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 arrowIndex;
  s32 left;
  s32 unused[4] UNUSED;

  f_PageArrowTimer++;
  if (f_PageArrowTimer >= PAGE_ARROW_FRAME_DURATION) {
    f_PageArrowFrame++;
    if (f_PageArrowFrame >= PAGE_ARROW_FRAME_COUNT) {
      f_PageArrowFrame = 0;
    }

    f_PageArrowTimer = 0;
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_PageArrows)));
  gDPPipeSync(gfx++);
  for (arrowIndex = 0; arrowIndex != PAGE_ARROW_COUNT; arrowIndex++) {
    if (arrowIndex == PAGE_ARROW_RIGHT) {
      /* The right arrow is the left one's image, mirrored */
      gDPLoadTextureTile_4b(gfx++,
          TextureAddress(ImageTexels(f_PageArrows) +
              f_PageArrowFrame * PAGE_ARROW_IMAGE_SIZE),
          G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0,
          PAGE_ARROW_SIZE - 1, PAGE_ARROW_SIZE - 1, 0, G_TX_MIRROR | G_TX_WRAP,
          G_TX_MIRROR | G_TX_WRAP, PAGE_ARROW_MASK, PAGE_ARROW_MASK, G_TX_NOLOD,
          G_TX_NOLOD);
      left = PAGE_ARROW_RIGHT_X;
    } else {
      gDPLoadTextureTile_4b(gfx++,
          TextureAddress(ImageTexels(f_PageArrows) +
              f_PageArrowFrame * PAGE_ARROW_IMAGE_SIZE),
          G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0,
          PAGE_ARROW_SIZE - 1, PAGE_ARROW_SIZE - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      left = PAGE_ARROW_LEFT_X;
    }

    gDPPipeSync(gfx++);
    gSPTextureRectangle(gfx++, QuarterPixels(left),
        QuarterPixels(PAGE_ARROW_TOP), QuarterPixels(left + PAGE_ARROW_SIZE),
        QuarterPixels(PAGE_ARROW_TOP + PAGE_ARROW_SIZE), G_TX_RENDERTILE,
        TexelCoordinate(MirroredArrowColumn(arrowIndex)), 0, TEXEL_STEP_ONE,
        TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", DrawPageArrows);
#endif

/*
 * Draws the animated page arrows gap apart about the screen's middle, centered
 * on centerY
 */
void DrawCenteredPageArrows(Gfx** displayListHead, s32 gap, s32 centerY) {
  Gfx* gfx = *displayListHead;
  s32 arrowIndex;
  s32 left;
  s32 textureColumn;

  f_PageArrowTimer++;
  if (f_PageArrowTimer >= PAGE_ARROW_FRAME_DURATION) {
    f_PageArrowFrame++;
    if (f_PageArrowFrame >= PAGE_ARROW_FRAME_COUNT) {
      f_PageArrowFrame = 0;
    }

    f_PageArrowTimer = 0;
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_PageArrows)));
  gDPPipeSync(gfx++);
  for (arrowIndex = 0; arrowIndex != PAGE_ARROW_COUNT; arrowIndex++) {
    if (arrowIndex == PAGE_ARROW_RIGHT) {
      /* The right arrow is the left one's image, mirrored */
      gDPLoadTextureTile_4b(gfx++,
          TextureAddress(ImageTexels(f_PageArrows) +
              f_PageArrowFrame * PAGE_ARROW_IMAGE_SIZE),
          G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0,
          PAGE_ARROW_SIZE - 1, PAGE_ARROW_SIZE - 1, 0, G_TX_MIRROR | G_TX_WRAP,
          G_TX_MIRROR | G_TX_WRAP, PAGE_ARROW_MASK, PAGE_ARROW_MASK, G_TX_NOLOD,
          G_TX_NOLOD);
      left = (SCREEN_WIDTH / 2) + (gap / 2);
    } else {
      gDPLoadTextureTile_4b(gfx++,
          TextureAddress(ImageTexels(f_PageArrows) +
              f_PageArrowFrame * PAGE_ARROW_IMAGE_SIZE),
          G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0,
          PAGE_ARROW_SIZE - 1, PAGE_ARROW_SIZE - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      left = (SCREEN_WIDTH / 2) - PAGE_ARROW_SIZE - (gap / 2);
    }

    gDPPipeSync(gfx++);
    /* Computed before the rectangle, which keeps its register, to match */
    textureColumn = MirroredArrowColumn(arrowIndex);
    gSPScisTextureRectangle(gfx++, QuarterPixels(left),
        QuarterPixels(centerY - (PAGE_ARROW_SIZE / 2)),
        QuarterPixels(left + PAGE_ARROW_SIZE),
        QuarterPixels(centerY + (PAGE_ARROW_SIZE / 2)), G_TX_RENDERTILE,
        TexelCoordinate(textureColumn), 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

/* Draws the page arrows spacing apart at y, turned; nothing calls it */
void DrawTurnedPageArrows(Gfx** displayListHead, s32 spacing, s32 y) {
  Gfx* gfx = *displayListHead;
  s32 arrowIndex;
  s32 x;
  Degrees angle;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gDPPipeSync(gfx++);
  for (arrowIndex = 0; arrowIndex != PAGE_ARROW_COUNT; arrowIndex++) {
    guOrtho(&f_TurnedArrowProjections[arrowIndex], -(SCREEN_WIDTH / 2),
        SCREEN_WIDTH / 2, -(SCREEN_HEIGHT / 2), SCREEN_HEIGHT / 2,
        TURNED_ARROW_NEAR, TURNED_ARROW_FAR, 1.0f);
    if (arrowIndex == 0) {
      x = -spacing / 2;
    } else {
      x = spacing / 2;
    }

    angle = TURNED_ARROW_ANGLE;
    if (arrowIndex == 0) {
      angle = -TURNED_ARROW_ANGLE;
    }

    guTranslate(&f_TurnedArrowTranslations[arrowIndex], x, y, TURNED_ARROW_Z);
    guRotate(&f_TurnedArrowRotations[arrowIndex], angle, 0.0f, 0.0f, 1.0f);
    guScale(&f_TurnedArrowScales[arrowIndex], 1.0f, 1.0f, 1.0f);
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_PageArrows)));
    gDPLoadTextureTile_4b(gfx++,
        TextureAddress(ImageTexels(f_PageArrows) +
            (f_PageArrowFrame * PAGE_ARROW_IMAGE_SIZE)),
        G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0,
        PAGE_ARROW_SIZE - 1, PAGE_ARROW_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, PhysicalAddress(&f_TurnedArrowProjections[arrowIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++, PhysicalAddress(&f_TurnedArrowTranslations[arrowIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(gfx++, PhysicalAddress(&f_TurnedArrowRotations[arrowIndex]),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPMatrix(gfx++, PhysicalAddress(&f_TurnedArrowScales[arrowIndex]),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, f_TurnedArrowVertices, TURNED_ARROW_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

/* Draws the blinking green up and down arrows about centerY */
/*
 * Doesn't match: the original adds the texel offset to the blink frame's
 * offset before the arrow's.
 */
#ifdef NON_MATCHING
void DrawUpDownArrows(Gfx** displayListHead, s32 centerY) {
  Gfx* gfx = *displayListHead;
  s32 left = 0;
  s32 offsetY = 0;
  s32 arrowIndex;

  f_GreenArrowTimer++;
  if (f_GreenArrowTimer == STATS_ARROW_BLINK_FRAME_COUNT) {
    f_GreenArrowTimer = 0;
    f_GreenArrowFrame ^= 1;
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_StatsGreenArrows)));
  for (arrowIndex = 0; arrowIndex != STATS_ARROW_COUNT; arrowIndex++) {
    gDPLoadTextureTile_4b(gfx++,
        TextureAddress(ImageTexels(f_StatsGreenArrows) +
            (arrowIndex * GREEN_ARROW_IMAGE_SIZE +
                f_GreenArrowFrame * GREEN_ARROW_FRAME_SIZE)),
        G_IM_FMT_CI, GREEN_ARROW_SIZE, GREEN_ARROW_SIZE, 0, 0,
        GREEN_ARROW_SIZE - 1, GREEN_ARROW_SIZE - 1, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    if (arrowIndex == 0) {
      left = STATS_ARROW_LEFT;
      offsetY = STATS_UP_ARROW_OFFSET;
    } else if (arrowIndex == 1) {
      left = STATS_ARROW_LEFT;
      offsetY = STATS_DOWN_ARROW_OFFSET;
    }

    gSPScisTextureRectangle(gfx++, QuarterPixels(left),
        QuarterPixels(offsetY + centerY),
        QuarterPixels(left + GREEN_ARROW_SIZE),
        QuarterPixels(offsetY + GREEN_ARROW_SIZE + centerY), G_TX_RENDERTILE, 0,
        0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", DrawUpDownArrows);
#endif

/* Reads every course's icon into the Best Shots screen's buffers */
void LoadBestShotsCourseIcons(void** buffers) {
  EncodedAsset asset;
  s16 imageIndex;

  f_BestShotsCourseIcons = buffers;
  for (imageIndex = 0; imageIndex != PLAYER_STATS_IMAGE_COUNT; imageIndex++) {
    OpenEncodedAsset(g_CourseIconAssets[imageIndex], &asset);
    ReadEncodedAsset(f_BestShotsCourseIcons[imageIndex], &asset);
  }
}

void LoadScoreLabelBuffer(void* buffer, s16 bufferKind) {
  EncodedAsset asset;

  switch (bufferKind) {
    case SCORE_LABEL_BUFFER_NAME_PLATE:
      f_BestShotsNamePlate = buffer;
      break;
    case SCORE_LABEL_BUFFER_CURSOR_BOX:
      f_BestShotsCursorBox = buffer;
      OpenEncodedAsset(ASSET_CURSOR_BOX, &asset);
      ReadEncodedAsset(f_BestShotsCursorBox, &asset);
      break;
    case SCORE_LABEL_BUFFER_LABELS:
      f_ScoreLabels = buffer;
      OpenEncodedAsset(ASSET_JAPANESE_BIRDIE_LABEL, &asset);
      ReadEncodedAsset(f_ScoreLabels, &asset);
      OpenEncodedAsset(ASSET_JAPANESE_EAGLE_LABEL, &asset);
      ReadEncodedAsset(f_ScoreLabels + PLAYER_STATS_IMAGE_SIZE, &asset);
      OpenEncodedAsset(ASSET_JAPANESE_ALBATROSS_LABEL, &asset);
      ReadEncodedAsset(f_ScoreLabels + (PLAYER_STATS_IMAGE_SIZE * 2), &asset);
      OpenEncodedAsset(ASSET_JAPANESE_HOLE_IN_ONE_LABEL, &asset);
      ReadEncodedAsset(f_ScoreLabels + (PLAYER_STATS_IMAGE_SIZE * 3), &asset);
      break;
  }
}

/* Returns s32 rather than s8: the callers cast the result to s16, and an s8
 * result would make GCC first extend it from 8 bits, which the original does
 * not */
s32 FindNextBestShotCell(s8 column, s8 row, u8 direction);

void UpdateBestShots(void) {
  SaveData* save = GetSaveData();
  MenuMemory* memory = GetMenuMemory();
  s32 slot;
  s32 moved;
  s32 column;

  if (IsHudAnimationDone() == TRUE) {
    if (GetWipeKind() != WIPE_NONE && !g_IsMenuWipeOpening) {
      if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
        g_MenuScreen = MENU_SCREEN_EXIT;
        g_IsChosenHoleLoadStarted = FALSE;
        g_GameState.replayKind = REPLAY_KIND_BEST_SHOT;
        SetGameFlag(GAME_FLAG_UNK1E);
        for (slot = 0; slot != STATS_TEXT_SLOT_COUNT; slot++) {
          SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, slot, 0, 0xFF,
              0xFF, 0xFF, 0xFF);
        }
      }
    } else if (GetWipeKind() == WIPE_NONE || g_IsMenuWipeOpening != TRUE) {
      if (g_MenuPressedButtons & A_BUTTON) {
        if (GetChosenBestShot() != STATS_CELL_EMPTY) {
          PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
          g_IsMenuWipeOpening = FALSE;
          column = memory->bestShotColumn;
          LoadWipeShape((s8)save
                  ->rankedRecords[f_BestShotRecords[memory->bestShotRow]
                                                   [column]]
                  .courseIndex);
          StartWipe(WIPE_CLOSING);
          g_IsRingShotWipeOpening = FALSE;
        }
      } else if (g_MenuPressedButtons & B_BUTTON) {
        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
        for (slot = 0; slot != STATS_TEXT_SLOT_COUNT; slot++) {
          SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, slot, 0, 0xFF,
              0xFF, 0xFF, 0xFF);
        }
      } else if (g_MenuRepeatButtons & U_JPAD) {
        moved = FindNextBestShotCell(
            memory->bestShotColumn, memory->bestShotRow, STATS_DIRECTION_UP);
        if ((s16)moved != STATS_CELL_EMPTY) {
          PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          memory->bestShotRow = moved;
        }
      } else if (g_MenuRepeatButtons & D_JPAD) {
        moved = FindNextBestShotCell(
            memory->bestShotColumn, memory->bestShotRow, STATS_DIRECTION_DOWN);
        if ((s16)moved != STATS_CELL_EMPTY) {
          PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          memory->bestShotRow = moved;
        }
      } else if (g_MenuRepeatButtons & L_JPAD) {
        moved = FindNextBestShotCell(
            memory->bestShotColumn, memory->bestShotRow, STATS_DIRECTION_LEFT);
        if ((s16)moved != STATS_CELL_EMPTY) {
          PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          memory->bestShotColumn = moved;
        }
      } else if (g_MenuRepeatButtons & R_JPAD) {
        moved = FindNextBestShotCell(
            memory->bestShotColumn, memory->bestShotRow, STATS_DIRECTION_RIGHT);
        if ((s16)moved != STATS_CELL_EMPTY) {
          PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          memory->bestShotColumn = moved;
        }
      }
    }
  }
}

/*
 * DrawBestShots draws the records page: a shaded panel, the image in
 * f_BestShotsNamePlate in strips below it, and a row of up to five ranked faces
 * for each label of f_RecordLabels. The chosen record also shows its course's
 * image and name.
 */
#define RECORDS_PANEL_LEFT 24
#define RECORDS_PANEL_TOP 48
#define RECORDS_PANEL_RIGHT 302
#define RECORDS_PANEL_BOTTOM 166
#define RECORDS_STRIP_WIDTH 200
#define RECORDS_STRIP_HEIGHT 10
#define RECORDS_STRIP_COUNT 3
#define RECORDS_STRIP_LEFT 28
#define RECORDS_STRIP_RIGHT 298
#define RECORDS_STRIP_TOP 168
#define RECORDS_STRIP_SPACING 20
#define RECORD_FACE_X 134
#define RECORD_FACE_Y 56
#define RECORD_PLACE_X 124
#define RECORD_PLACE_Y 64
#define RECORD_LABEL_X 32
#define RECORD_COURSE_IMAGE_WIDTH 64
#define RECORD_COURSE_STRIP_HEIGHT 32
#define RECORD_COURSE_STRIP_COUNT 2
#define RECORD_COURSE_LEFT 40
#define RECORD_COURSE_RIGHT 96
#define RECORD_COURSE_TOP 176
#define RECORD_COURSE_STRIP_SPACING 20
#define RECORD_COURSE_NAME_X 120
#define RECORD_COURSE_NAME_Y 180
#define RECORD_DETAIL_X 200
#define RECORD_DETAIL_Y 204
#define MESSAGE_RECORD_DETAIL 0x76
#define NO_RECORDS_X 160
#define NO_RECORDS_Y 192
#define MESSAGE_NO_RECORDS 0x16E

/*
 * Doesn't match: the original works out the chosen cell's address again after
 * checking that it holds a record, before drawing its course's image and name,
 * where GCC reuses the address from the check.
 */
#ifdef NON_MATCHING
void DrawBestShots(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s16 index = 0;
  SaveData* save = GetSaveData();
  MenuMemory* memory = GetMenuMemory();
  s16 row;
  s16 column;
  s16 character;
  s32 chosenRow;
  s32 chosenColumn;

  DrawMenuBackdropLayers(&gfx, PLAIN_BACKDROP_LAYERS);
  gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
  gDPPipeSync(gfx++);
  gSPTextureRectangle(gfx++, QuarterPixels(RECORDS_PANEL_LEFT),
      QuarterPixels(RECORDS_PANEL_TOP), QuarterPixels(RECORDS_PANEL_RIGHT),
      QuarterPixels(RECORDS_PANEL_BOTTOM), G_TX_RENDERTILE, 0, 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_BestShotsNamePlate)));
  do {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(f_BestShotsNamePlate) +
            (index * RECORDS_STRIP_WIDTH * RECORDS_STRIP_HEIGHT)),
        G_IM_FMT_CI, G_IM_SIZ_8b, RECORDS_STRIP_WIDTH, RECORDS_STRIP_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, QuarterPixels(RECORDS_STRIP_LEFT),
        QuarterPixels(RECORDS_STRIP_TOP + (index * RECORDS_STRIP_SPACING)),
        QuarterPixels(RECORDS_STRIP_RIGHT),
        QuarterPixels(RECORDS_STRIP_TOP + RECORDS_STRIP_SPACING +
            (index * RECORDS_STRIP_SPACING)),
        G_TX_RENDERTILE, 0, 0,
        (RECORDS_STRIP_WIDTH << 10) /
            (RECORDS_STRIP_RIGHT - RECORDS_STRIP_LEFT),
        (RECORDS_STRIP_HEIGHT << 10) / RECORDS_STRIP_SPACING);
    gDPPipeSync(gfx++);
    index++;
  } while (index != RECORDS_STRIP_COUNT);

  for (row = 0; row != STATS_GRID_ROW_COUNT; row++) {
    for (column = 0; column != STATS_GRID_COLUMN_COUNT; column++) {
      if (f_BestShotRecords[row][column] != STATS_CELL_EMPTY) {
        character = (s8)save->rankedRecords[f_BestShotRecords[row][column]]
                        .characterIndex;
        DrawCharacterFace(&gfx, g_CharacterFaceImages[character],
            f_BestShotFaces[row][column],
            RECORD_FACE_X + (column * STATS_CURSOR_COLUMN_WIDTH),
            RECORD_FACE_Y + (row * STATS_CURSOR_ROW_HEIGHT), character,
            FACE_PLAIN);
      }

      DrawOneDigitMenuNumber(&gfx,
          RECORD_PLACE_X + (column * STATS_CURSOR_COLUMN_WIDTH),
          RECORD_PLACE_Y + (row * STATS_CURSOR_ROW_HEIGHT), column + 1,
          MENU_NUMBER_PLAIN);
    }
  }

  if (f_BestShotRecords[memory->bestShotRow][memory->bestShotColumn] !=
      STATS_CELL_EMPTY) {
    DrawBestShotsCursor(&gfx);
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  chosenRow = memory->bestShotRow;
  chosenColumn = memory->bestShotColumn;
  if (f_BestShotRecords[chosenRow][chosenColumn] != STATS_CELL_EMPTY) {
    gDPLoadTLUT_pal256(gfx++,
        TextureAddress(
            (u32)f_BestShotsCourseIcons[(s8)save
                    ->rankedRecords[f_BestShotRecords[chosenRow][chosenColumn]]
                    .courseIndex] +
            IMAGE_HEADER_SIZE));
    for (index = 0; index != RECORD_COURSE_STRIP_COUNT; index++) {
      gDPLoadTextureBlock(gfx++,
          TextureAddress(
              (u32)f_BestShotsCourseIcons[(s8)save
                      ->rankedRecords[f_BestShotRecords[chosenRow]
                                                       [chosenColumn]]
                      .courseIndex] +
              LARGE_PALETTED_TEXELS_OFFSET +
              (index * RECORD_COURSE_IMAGE_WIDTH * RECORD_COURSE_STRIP_HEIGHT)),
          G_IM_FMT_CI, G_IM_SIZ_8b, RECORD_COURSE_IMAGE_WIDTH,
          RECORD_COURSE_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, QuarterPixels(RECORD_COURSE_LEFT),
          QuarterPixels(
              RECORD_COURSE_TOP + (index * RECORD_COURSE_STRIP_SPACING)),
          QuarterPixels(RECORD_COURSE_RIGHT),
          QuarterPixels(RECORD_COURSE_TOP + RECORD_COURSE_STRIP_SPACING +
              (index * RECORD_COURSE_STRIP_SPACING)),
          G_TX_RENDERTILE, 0, 0,
          (RECORD_COURSE_IMAGE_WIDTH << 10) /
              (RECORD_COURSE_RIGHT - RECORD_COURSE_LEFT),
          (RECORD_COURSE_STRIP_HEIGHT << 10) / RECORD_COURSE_STRIP_SPACING);
      gDPPipeSync(gfx++);
    }
  }

  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  for (row = 0; row < STATS_GRID_ROW_COUNT; row++) {
    sprintf(g_PrintBuffer, "%s", f_RecordLabels[row]);
    DrawLargeText(&gfx, RECORD_LABEL_X,
        (s16)(RECORD_PLACE_Y + (row * STATS_CURSOR_ROW_HEIGHT)), g_PrintBuffer);
  }

  chosenRow = memory->bestShotRow;
  chosenColumn = memory->bestShotColumn;
  if (f_BestShotRecords[chosenRow][chosenColumn] != STATS_CELL_EMPTY) {
    sprintf(g_PrintBuffer, "%s",
        g_CourseRecords
            [(s8)save->rankedRecords[f_BestShotRecords[chosenRow][chosenColumn]]
                    .courseIndex]
                .name);
    DrawLargeText(
        &gfx, RECORD_COURSE_NAME_X, RECORD_COURSE_NAME_Y, g_PrintBuffer);
    SetTextSlotValue(
        (s8)save->rankedRecords[f_BestShotRecords[chosenRow][chosenColumn]]
                .holeIndex +
            1,
        1, 0);
    SetTextSlot(RECORD_DETAIL_X, RECORD_DETAIL_Y, MESSAGE_RECORD_DETAIL,
        TEXT_SLOT_LARGE_FONT, 1, 0, 255, 255, 255, 255);
  }

  if (!f_HasBestShots) {
    SetTextSlot(NO_RECORDS_X, NO_RECORDS_Y, MESSAGE_NO_RECORDS,
        TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_CENTERED, 255, 255, 255, 255);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", DrawBestShots);
#endif

/*
 * Reads the save data from SRAM and lays the recorded best shots out in a grid,
 * a row for each score kind, keeping the cursor on a shot where it can
 */
void ReadBestShots(void) {
  SaveData* save = GetSaveData();
  s32 kindRecords[STATS_GRID_ROW_COUNT][STATS_KIND_LIST_SIZE];
  s32 index;
  s32 recordIndex;
  s32 birdieCount;
  s32 eagleCount;
  s32 albatrossCount;
  s32 holeInOneCount;
  s32 flags;
  s32 row;
  s32 column;
  MenuMemory* memory;

  InitSram();
  TransferSaveData(save, FALSE);
  for (recordIndex = 0; recordIndex != STATS_GRID_COLUMN_COUNT; recordIndex++) {
    kindRecords[GATHERED_BIRDIES][recordIndex] = STATS_CELL_EMPTY;
    kindRecords[GATHERED_EAGLES][recordIndex] = STATS_CELL_EMPTY;
    kindRecords[GATHERED_HOLES_IN_ONE][recordIndex] = STATS_CELL_EMPTY;
    kindRecords[GATHERED_ALBATROSSES][recordIndex] = STATS_CELL_EMPTY;
  }

  birdieCount = 0;
  eagleCount = 0;
  holeInOneCount = 0;
  albatrossCount = 0;
  for (recordIndex = 0; recordIndex != RANKED_RECORD_COUNT; recordIndex++) {
    flags = (s8)save->rankedRecords[recordIndex].scoreKind;
    if (flags & SCORE_KIND_RECORDED) {
      switch (flags & SCORE_KIND_MASK) {
        case SCORE_KIND_BIRDIE:
          if (birdieCount < STATS_GRID_COLUMN_COUNT) {
            kindRecords[GATHERED_BIRDIES][birdieCount++] = recordIndex;
          }

          break;
        case SCORE_KIND_EAGLE:
          if (eagleCount < STATS_GRID_COLUMN_COUNT) {
            kindRecords[GATHERED_EAGLES][eagleCount++] = recordIndex;
          }

          break;
        case SCORE_KIND_ALBATROSS:
          if (albatrossCount < STATS_GRID_COLUMN_COUNT) {
            kindRecords[GATHERED_ALBATROSSES][albatrossCount++] = recordIndex;
          }

          break;
        case SCORE_KIND_HOLE_IN_ONE:
          if (holeInOneCount < STATS_GRID_COLUMN_COUNT) {
            kindRecords[GATHERED_HOLES_IN_ONE][holeInOneCount++] = recordIndex;
          }

          break;
      }
    }
  }

  /* Setting the column twice is needed to match */
  row = column = BEST_SHOT_ROW_BIRDIE;
  memory = GetMenuMemory();
  column = row;
  if (birdieCount == 0) {
    if (eagleCount != 0) {
      row = BEST_SHOT_ROW_EAGLE;
    } else if (albatrossCount != 0) {
      row = BEST_SHOT_ROW_ALBATROSS;
    } else if (holeInOneCount != 0) {
      row = BEST_SHOT_ROW_HOLE_IN_ONE;
    }
  }

  /* The record index is reused for the saved row, to match */
  recordIndex = memory->bestShotRow;
  switch (recordIndex) {
    case BEST_SHOT_ROW_BIRDIE:
      if (birdieCount != 0) {
        row = BEST_SHOT_ROW_BIRDIE;
        if (memory->bestShotColumn < birdieCount &&
            memory->bestShotColumn >= 0) {
          column = memory->bestShotColumn;
        }
      }

      break;
    case BEST_SHOT_ROW_EAGLE:
      if (eagleCount != 0) {
        row = BEST_SHOT_ROW_EAGLE;
        if (memory->bestShotColumn < eagleCount &&
            memory->bestShotColumn >= 0) {
          column = memory->bestShotColumn;
        }
      }

      break;
    case BEST_SHOT_ROW_ALBATROSS:
      if (albatrossCount != 0) {
        row = BEST_SHOT_ROW_ALBATROSS;
        if (memory->bestShotColumn < albatrossCount &&
            memory->bestShotColumn >= 0) {
          column = memory->bestShotColumn;
        }
      }

      break;
    case BEST_SHOT_ROW_HOLE_IN_ONE:
      if (holeInOneCount != 0) {
        row = BEST_SHOT_ROW_HOLE_IN_ONE;
        if (memory->bestShotColumn < holeInOneCount &&
            memory->bestShotColumn >= 0) {
          column = memory->bestShotColumn;
        }
      }

      break;
  }

  memory->bestShotColumn = column;
  memory->bestShotRow = row;
  for (recordIndex = 0; recordIndex != STATS_GRID_COLUMN_COUNT; recordIndex++) {
    f_BestShotRecords[BEST_SHOT_ROW_BIRDIE][recordIndex] =
        kindRecords[GATHERED_BIRDIES][recordIndex];
    f_BestShotRecords[BEST_SHOT_ROW_EAGLE][recordIndex] =
        kindRecords[GATHERED_EAGLES][recordIndex];
    f_BestShotRecords[BEST_SHOT_ROW_ALBATROSS][recordIndex] =
        kindRecords[GATHERED_ALBATROSSES][recordIndex];
    f_BestShotRecords[BEST_SHOT_ROW_HOLE_IN_ONE][recordIndex] =
        kindRecords[GATHERED_HOLES_IN_ONE][recordIndex];
  }

  recordIndex = 0;
  f_HasBestShots = 0;
  for (; recordIndex != STATS_GRID_ROW_COUNT; recordIndex++) {
    for (index = 0; index != STATS_GRID_COLUMN_COUNT; index++) {
      if (f_BestShotRecords[recordIndex][index] != STATS_CELL_EMPTY) {
        f_HasBestShots++;
        break;
      }
    }

    if (f_HasBestShots) {
      break;
    }
  }
}

/*
 * Returns the column or row of the next shot from (column, row) in direction,
 * wrapping, or -1 if that line holds no shot
 */
s32 FindNextBestShotCell(s8 column, s8 row, u8 direction) {
  s32 stepCount;
  s32 stepIndex;

  if (direction == STATS_DIRECTION_LEFT || direction == STATS_DIRECTION_RIGHT) {
    stepCount = STATS_GRID_COLUMN_COUNT;
  } else if (direction == STATS_DIRECTION_UP ||
      direction == STATS_DIRECTION_DOWN) {
    stepCount = STATS_GRID_ROW_COUNT;
  } else {
    stepCount = 0;
  }

  for (stepIndex = 0; stepIndex != stepCount; stepIndex++) {
    column += f_BestShotDirectionSteps[direction].column;
    row += f_BestShotDirectionSteps[direction].row;
    if (column < 0) {
      column = STATS_GRID_COLUMN_COUNT - 1;
    }

    if (column >= STATS_GRID_COLUMN_COUNT) {
      column = 0;
    }

    if (row < 0) {
      row = STATS_GRID_ROW_COUNT - 1;
    }

    if (row >= STATS_GRID_ROW_COUNT) {
      row = 0;
    }

    if (f_BestShotRecords[row][column] != STATS_CELL_EMPTY) {
      break;
    }
  }

  if (stepIndex == stepCount) {
    return -1;
  }

  /* Other directions step no cells, and so returned above */
  if (direction == STATS_DIRECTION_LEFT || direction == STATS_DIRECTION_RIGHT) {
    return column;
  } else if (direction == STATS_DIRECTION_UP ||
      direction == STATS_DIRECTION_DOWN) {
    return row;
  }

  return UNSET_RESULT;
}

/* Returns the ranked record under the Best Shots cursor */
s16 GetChosenBestShot(void) {
  MenuMemory* memory = GetMenuMemory();

  return f_BestShotRecords[memory->bestShotRow][memory->bestShotColumn];
}

/* Draws one of the Japanese score labels at (left, top); nothing calls it */
void DrawScoreLabel(Gfx** displayListHead, s32 left, s32 top, s32 labelIndex) {
  Gfx* gfx = *displayListHead;
  u8* image;
  s32 width;
  s32 height;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  if (labelIndex < 0) {
    labelIndex = 0;
  }

  if (labelIndex >= SCORE_LABEL_COUNT) {
    labelIndex = SCORE_LABEL_COUNT - 1;
  }

  image = f_ScoreLabels + (labelIndex * PLAYER_STATS_IMAGE_SIZE);
  width = image[IMAGE_WIDTH_LOW_BYTE];
  height = image[IMAGE_HEIGHT_LOW_BYTE];

  /* The original passes the palette's first byte, not its address, so the
   * colors come from low memory */
  gDPLoadTLUT_pal16(gfx++, 0, image[IMAGE_HEADER_SIZE] & ~7);
  gDPPipeSync(gfx++);
  gDPLoadTextureBlock_4b(gfx++, TextureAddress(ImageTexels(image)), G_IM_FMT_CI,
      width, height, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, QuarterPixels(left), QuarterPixels(top),
      QuarterPixels(left + width), QuarterPixels(top + height), G_TX_RENDERTILE,
      0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* Draws the color-cycling cursor on the chosen Best Shots cell */
/*
 * Doesn't match: the stack slots of the pointers to the second pipe sync
 * after the image and the cycle type are swapped.
 */
#ifdef NON_MATCHING
void DrawBestShotsCursor(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  MenuMemory* memory = GetMenuMemory();
  s32 vertexIndex;
  s32 red;
  s32 blue;
  s32 green;

  for (vertexIndex = 0; vertexIndex != MENU_CURSOR_VERTEX_COUNT;
      vertexIndex++) {
    f_BestShotsCursorPhases[vertexIndex] += MENU_CURSOR_PHASE_STEP;
    red = (s32)(sinf(f_BestShotsCursorPhases[vertexIndex]) *
              MENU_CURSOR_COLOR_SWING) +
        MENU_CURSOR_COLOR_MIDDLE;
    blue = (s32)(sinf(f_BestShotsCursorPhases[vertexIndex] + (M_PI / 2)) *
               MENU_CURSOR_COLOR_SWING) +
        MENU_CURSOR_COLOR_MIDDLE;
    green = (s32)(sinf(f_BestShotsCursorPhases[vertexIndex] + M_PI) *
                MENU_CURSOR_COLOR_SWING) +
        MENU_CURSOR_COLOR_MIDDLE;
    f_BestShotsCursorVertices[vertexIndex].v.cn[0] = red;
    f_BestShotsCursorVertices[vertexIndex].v.cn[2] = blue;
    f_BestShotsCursorVertices[vertexIndex].v.cn[1] = green;
  }

  guOrtho(&f_BestShotsCursorProjection, -(SCREEN_WIDTH / 2), SCREEN_WIDTH / 2,
      -(SCREEN_HEIGHT / 2), SCREEN_HEIGHT / 2, MENU_CURSOR_NEAR,
      MENU_CURSOR_FAR, 1.0f);
  guTranslate(&f_BestShotsCursorTranslation,
      (memory->bestShotColumn * STATS_CURSOR_COLUMN_WIDTH) +
          STATS_CURSOR_LEFT_X,
      STATS_CURSOR_TOP_Y - (memory->bestShotRow * STATS_CURSOR_ROW_HEIGHT),
      MENU_CURSOR_Z);
  guRotate(&f_BestShotsCursorRotation, 0.0f, 0.0f, 1.0f, 0.0f);
  guScale(&f_BestShotsCursorScale, 1.0f, MENU_CURSOR_SCALE_Y, 1.0f);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPLoadTextureBlock_4b(gfx++, TextureAddress(ImageBody(f_BestShotsCursorBox)),
      G_IM_FMT_IA, MENU_CURSOR_SIZE, MENU_CURSOR_SIZE, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
  gDPSetCombineMode(gfx++, G_CC_SHADEDECALA, G_CC_SHADEDECALA);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, PhysicalAddress(&f_BestShotsCursorProjection),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPMatrix(gfx++, PhysicalAddress(&f_BestShotsCursorTranslation),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
  gSPMatrix(gfx++, PhysicalAddress(&f_BestShotsCursorRotation),
      G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
  gSPMatrix(gfx++, PhysicalAddress(&f_BestShotsCursorScale),
      G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
  gDPPipeSync(gfx++);
  gSPVertex(gfx++, f_BestShotsCursorVertices, MENU_CURSOR_VERTEX_COUNT, 0);
  gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", DrawBestShotsCursor);
#endif

/*
 * Reads every character's face, then the palette of each best shot's character
 * in the grid, clamped to the known ones
 */
/* Doesn't match: the original keeps the row and column counters in other saved
 * registers */
#ifdef NON_MATCHING
void LoadBestShotFaces(void) {
  EncodedAsset asset;
  AssetFile file;
  SaveData* save = GetSaveData();
  s32 index;
  s32 row;
  s32 column;
  CharacterId character;
  s32 palette;
  s32 cell;

  for (index = 0; index != CHARACTER_COUNT; index++) {
    OpenEncodedAsset(g_CharacterFaceAssets[index], &asset);
    ReadEncodedAsset(g_CharacterFaceImages[index], &asset);
  }

  for (row = 0; row != STATS_GRID_ROW_COUNT; row++) {
    IN_OWN_BLOCK(cell = row * STATS_GRID_COLUMN_COUNT);
    for (column = 0; column != STATS_GRID_COLUMN_COUNT; column++, cell++) {
      if (f_BestShotRecords[row][column] != STATS_CELL_EMPTY) {
        character = (s8)save->rankedRecords[f_BestShotRecords[row][column]]
                        .characterIndex;
        palette = (s8)save->rankedRecords[f_BestShotRecords[row][column]]
                      .paletteIndex;
        if (character < 0) {
          character = 0;
        }

        if (character >= CHARACTER_COUNT) {
          character = CHARACTER_COUNT - 1;
        }

        if (palette < 0) {
          palette = 0;
        }

        if (palette >= CHARACTER_PALETTE_COUNT) {
          palette = CHARACTER_PALETTE_COUNT - 1;
        }
      } else {
        character = 0;
        IN_OWN_BLOCK(palette = 0);
      }

      f_BestShotFaces[row][column] = g_MenuCharacterPalettes[cell];
      IN_OWN_BLOCK(
          OpenAssetFile(g_CharacterPaletteFiles[character][palette], &file));
      ReadAssetFile(
          f_BestShotFaces[row][column], GetAssetFileSize(&file), &file);
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", LoadBestShotFaces);
#endif

/*
 * Draws a character's face from texels and a palette at (left, top), darkened
 * or with the character's name on it as drawMode asks
 */
void DrawCharacterFace(Gfx** displayListHead, void* texels, void* palette,
    s32 left, s32 top, s16 character, u8 drawMode) {
  Gfx* gfx = *displayListHead;

  if (character < 0) {
    character = 0;
  }

  if (character >= CHARACTER_COUNT) {
    character = CHARACTER_COUNT - 1;
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  if (drawMode == FACE_DARKENED) {
    gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
    gDPSetPrimColor(gfx++, 0, 0, FACE_DARKENED_SHADE, FACE_DARKENED_SHADE,
        FACE_DARKENED_SHADE, 255);
  } else {
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  }

  gDPLoadTLUT_pal256(gfx++, TextureAddress(palette));
  gDPLoadTextureBlock(gfx++, TextureAddress(texels), G_IM_FMT_CI, G_IM_SIZ_8b,
      FACE_WIDTH, FACE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, QuarterPixels(left), QuarterPixels(top),
      QuarterPixels(left + FACE_WIDTH), QuarterPixels(top + FACE_HEIGHT),
      G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  if (drawMode == FACE_WITH_NAME) {
    if (character < NAMED_CHARACTER_COUNT) {
      SetUpTextDrawing(&gfx);
      SetTextColor(&gfx, 255, 255, 255);
      sprintf(g_PrintBuffer, f_FaceNameFormat,
          g_MenuCharacterNames[f_StatsCharacterChoices[character]]);
      DrawLargeText(&gfx, left + FACE_NAME_OFFSET_X, top + FACE_NAME_OFFSET_Y,
          g_PrintBuffer);
      gDPPipeSync(gfx++);
    }
  }

  *displayListHead = gfx;
}
