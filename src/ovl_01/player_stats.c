/*
 * The clubhouse's screen 12, a grid of stats over a brick wall with ranked
 * records and page arrows. The file also holds the bet settings screen and
 * screen 20, where each player makes a choice.
 */
#include "common.h"

#include "asset.h"
#include "character.h"
#include "character_slot.h"
#include "course.h"
#include "cpu_projection.h"
#include "functions.h"
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
#include "variables.h"

/* The images of g_CourseIconAssets that func_8021CF10 reads */
#define PLAYER_STATS_IMAGE_COUNT 6

/* The bytes of each of the four images that func_8021CFAC reads into one buffer
 */
#define PLAYER_STATS_IMAGE_SIZE 0x258
/* They are Japanese score labels (assets 0x82D, 0x82F, 0x82E and 0x830), such
 * as birdie and albatross */
#define SCORE_LABEL_COUNT 4

/* func_8021F124 writes names beside the faces of these first characters only */
#define NAMED_CHARACTER_COUNT 14

/* The most bytes of each ranked record holder's palette file that func_8021EF64
 * keeps */
#define PLAYER_STATS_FILE_SIZE 0x200

/* The grid of stats that the cursor moves over, and the value of its empty
 * cells */
#define STATS_GRID_COLUMN_COUNT 5
#define STATS_GRID_ROW_COUNT 4
#define STATS_CELL_EMPTY -1
/* The bit and the kind field in a ranked record's unk3A */
#define RANKED_RECORD_SAVED 0x80
#define RANKED_RECORD_KIND_MASK 0xF
/* func_8021E01C lists each kind's records in rows one longer than the grid */
#define STATS_KIND_LIST_SIZE (STATS_GRID_COLUMN_COUNT + 1)

/* What func_802178B0 reads into the buffers it fills */
#define PLAYER_STATS_ICON_COUNT 8
#define PLAYER_STATS_ICON_SIZE 0x218
#define ASSET_PLAYER_STATS_UNK2 0x671
#define ASSET_PLAYER_STATS_UNK8 0x6C4
#define ASSET_PLAYER_STATS_GAME_LABELS 0x6E8
#define GAME_LABEL_WIDTH 128
#define GAME_LABEL_HEIGHT 20
#define GAME_LABEL_IMAGE_SIZE (GAME_LABEL_WIDTH * GAME_LABEL_HEIGHT / 2)
#define ASSET_STATS_BRICK_WALL 0x721
#define ASSET_PLAYER_STATS_UNK12 0x828
#define ASSET_PLAYER_STATS_UNK13 0x829
#define STATS_INDEX_COLUMN_COUNT 6
/* D_801B5634 counts the index tables' rows from 1; columns past a row's count
 * in D_802304B4 hold STATS_INDEX_UNUSED */
#define STATS_INDEX_ROW_COUNT 7
#define STATS_INDEX_UNUSED -1
/* The stats icons are 24x24, and func_80219460 shows each of their two frames
 * for 4 video frames */
#define STATS_ICON_SIZE 24
#define STATS_ICON_FRAME_COUNT 2
#define STATS_ICON_FRAME_DURATION 4

/* The text slots that screen 12 clears on leaving */
#define STATS_TEXT_SLOT_COUNT 4

/* The animated arrows that func_8021BE88 draws either side of the page */
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
/* func_8021C67C turns the page arrows a quarter turn each way, and draws them
 * as squares away from the camera */
#define TURNED_ARROW_ANGLE 90.0f
#define TURNED_ARROW_Z -100.0f
#define TURNED_ARROW_NEAR 10.0f
#define TURNED_ARROW_FAR 2000.0f
#define TURNED_ARROW_VERTEX_COUNT 4

/* The blinking pair of arrows that func_8021CBDC draws above and below a point
 */
#define STATS_ARROW_COUNT 2
#define STATS_ARROW_SIZE 16
#define STATS_ARROW_IMAGE_SIZE (STATS_ARROW_SIZE * STATS_ARROW_SIZE / 2)
#define STATS_ARROW_BLINK_STRIDE (4 * STATS_ARROW_IMAGE_SIZE)
#define STATS_ARROW_BLINK_FRAME_COUNT 4
#define STATS_ARROW_LEFT 152
#define STATS_UP_ARROW_OFFSET (-52)
#define STATS_DOWN_ARROW_OFFSET 36

/* The bytes of each of D_80230984's record labels, such as "Birdie" */
#define RECORD_LABEL_SIZE 16

/* func_8021E97C puts the cursor on the stats grid's chosen cell, measured from
 * the screen's centre */
#define STATS_CURSOR_COLUMN_WIDTH 34
#define STATS_CURSOR_ROW_HEIGHT 26
#define STATS_CURSOR_LEFT_X 6
#define STATS_CURSOR_TOP_Y 30

/* The directions that func_8021E340 moves the cursor in, which index D_802309C4
 */
enum StatsDirection {
  STATS_DIRECTION_LEFT,
  STATS_DIRECTION_RIGHT,
  STATS_DIRECTION_UP,
  STATS_DIRECTION_DOWN
};

#define STATS_DIRECTION_COUNT 4

/*
 * The rows of the stats panel slide across together: D_80230784 holds their
 * offsets, and D_802307A8 holds where each slide after the first sends them
 */
#define STATS_SLIDE_ROW_COUNT 9
/*
 * func_8021B87C draws the 152x117 brick wall behind the stats in one strip
 * per row, each stretched across the screen; D_80230760 holds each row's
 * slide speed
 */
#define STATS_WALL_IMAGE_WIDTH 152
#define STATS_WALL_STRIP_HEIGHT 13
#define STATS_WALL_ROW_HEIGHT 27
/* Each bet's stake is one of these levels, which D_802304F4 turns into an
 * amount */
#define BET_STAKE_LEVEL_COUNT 6
/* What unkE holds for a bet that func_80217B88's preset leaves out */
#define NO_BET -1
/* Where func_80217D20 types the chosen bet's explanation, and its first message
 */
#define BET_HELP_X 36
#define BET_HELP_Y 186
#define BET_HELP_MESSAGE 0x15

/* Each player's choice on screen 20, and where the screen's slide starts */
#define PLAYER_OPTION_COUNT 37
#define PLAYER_OPTION_SLIDE_START -512

extern void* D_8012D430[CHARACTER_COUNT];
extern s32 g_ChosenPlayerCount;
extern s8 D_801B60BA[MENU_UNKE_COUNT];
extern s8 D_801B70F0[MAXCONTROLLERS];
extern const char D_802319E0_ovl_01[];

extern u8 D_801B5634;
extern s8 D_801052E8;
extern s32 D_800FE334;
extern u8 D_801B9410;

void func_8021B87C_ovl_01(Gfx** displayListHead, u8 slide);
extern u8 D_8023BFF0_ovl_01[STATS_GRID_ROW_COUNT * STATS_GRID_COLUMN_COUNT]
                           [PLAYER_STATS_FILE_SIZE];

s16 func_8021E468_ovl_01(void);
void func_8021E97C_ovl_01(Gfx** displayListHead);
void func_8021F124_ovl_01(Gfx** displayListHead, void* texels, void* palette,
    s32 left, s32 top, s16 character, u8 drawMode);
void func_80210C74_ovl_01(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void func_80210DF4_ovl_01(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void func_80219460_ovl_01(
    Gfx** displayListHead, s32 left, s32 top, s32 size, s32 textureStep);
void func_8021B1E0_ovl_01(
    Gfx** displayListHead, s32 centerX, s32 top, u8 labelIndex);

static u8 D_802303A0_ovl_01 = 0;
static u8 D_802303A1_ovl_01 = FALSE;
static s16 D_802303A2_ovl_01 = 0;
static s16 D_802303A4_ovl_01 = 0;
static s16 D_802303A6_ovl_01 = 0;
static s16 D_802303A8_ovl_01 = 0;
static u32 D_802303AC_ovl_01[PLAYER_STATS_ICON_COUNT] = {
    0x0674, 0x0678, 0x067A, 0x067E, 0x0680, 0x067C, 0x0676, 0x0672};
// clang-format off
static u32 D_802303CC_ovl_01[PLAYER_STATS_ICON_COUNT]
                             [STATS_ICON_FRAME_COUNT] = {
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
// clang-format off
static s16 D_8023040C_ovl_01[STATS_INDEX_ROW_COUNT]
                             [STATS_INDEX_COLUMN_COUNT] = {
  { 0, 1, 2, 3, 5, STATS_INDEX_UNUSED },
  { 0, 1, 2, 3, 5, 7 },
  { 0, 1, 6, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED },
  { 0, 1, 5, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED },
  { 0, 1, 5, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED },
  { 2, 3, 5, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED },
  { 2, 3, 5, 7, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED }
};
// clang-format on
// clang-format off
/* Which of unkE's bets each column of D_8023040C sets */
static s16 D_80230460_ovl_01[STATS_INDEX_ROW_COUNT]
                             [STATS_INDEX_COLUMN_COUNT] = {
  { 0, 1, 2, 3, 4, STATS_INDEX_UNUSED },
  { 0, 1, 2, 3, 4, 6 },
  { 0, 1, 5, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED },
  { 0, 1, 4, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED },
  { 0, 1, 4, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED },
  { 2, 3, 4, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED },
  { 2, 3, 4, 6, STATS_INDEX_UNUSED, STATS_INDEX_UNUSED }
};
// clang-format on
/* How many columns of D_8023040C each row uses */
static u8 D_802304B4_ovl_01[STATS_INDEX_ROW_COUNT] = {5, 6, 3, 3, 3, 3, 4};
// clang-format off
static u8 D_802304BC_ovl_01[MENU_UNKE_PRESET_COUNT]
                            [MENU_UNKE_COUNT] = {
  { 4, 4, 1, 0, 0, NO_BET, NO_BET, NO_BET },
  { 4, 4, 0, 0, 0, NO_BET, 0, NO_BET },
  { 4, 4, NO_BET, NO_BET, NO_BET, 1, NO_BET, NO_BET },
  { 4, 4, NO_BET, NO_BET, 1, NO_BET, NO_BET, NO_BET },
  { 4, 4, NO_BET, NO_BET, 1, NO_BET, NO_BET, NO_BET },
  { NO_BET, NO_BET, 1, 0, 0, NO_BET, NO_BET, NO_BET },
  { NO_BET, NO_BET, 0, 0, 0, NO_BET, 0, NO_BET }
};
// clang-format on
static u8 D_802304F4_ovl_01[BET_STAKE_LEVEL_COUNT] = {0, 1, 2, 3, 5, 10};
// clang-format off
/* The x of each column of D_8023040C */
static s32 D_802304FC_ovl_01[STATS_INDEX_ROW_COUNT]
                             [STATS_INDEX_COLUMN_COUNT] = {
  { 68, 108, 148, 188, 228, 228 },
  { 48, 88, 128, 168, 208, 248 },
  { 114, 148, 184, 216, 256, 256 },
  { 114, 148, 184, 216, 256, 256 },
  { 114, 148, 184, 144, 144, 144 },
  { 114, 148, 184, 216, 256, 256 },
  { 87, 127, 167, 207, 228, 228 }
};
// clang-format on
/* Read by no routine */
static char D_802305A4_ovl_01[8][10] = {
    "ﾄﾞﾗｺﾝ", "ﾆｱﾋﾟﾝ", "ｽｺｱ", "ﾀﾃ", "ﾖｺ", "ｽｷﾝｽﾞ", "ﾏｯﾁ", "ﾀﾞﾌﾞﾙｽ"};
/* The view of the players' characters */
static f32 D_802305F4_ovl_01[2] = {0.0f, 0.0f};
/* Read by no routine */
static s32 D_802305FC_ovl_01[3] = {0, 0, 0};
static f32 D_80230608_ovl_01[MAX_PLAYER_COUNT] = {4.2f, 4.0f, 4.2f, 4.0f};
// clang-format off
static Vec3 D_80230618_ovl_01[MAX_PLAYER_COUNT]
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
static s32 D_802306D8_ovl_01[MAX_PLAYER_COUNT][MAX_PLAYER_COUNT] = {
    {128, 0, 0, 0}, {96, 160, 0, 0}, {64, 128, 192, 0}, {32, 96, 160, 224}};
static s32 D_80230718_ovl_01[STATS_SLIDE_ROW_COUNT] = {
    0, 2, 4, 6, 8, 10, 12, 14, 16};
static s32 D_8023073C_ovl_01[STATS_SLIDE_ROW_COUNT] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0};
static s32 D_80230760_ovl_01[STATS_SLIDE_ROW_COUNT] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64};
static s32 D_80230784_ovl_01[STATS_SLIDE_ROW_COUNT] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0};
// clang-format off
static s32 D_802307A8_ovl_01[STATS_SLIDE_COUNT - 1]
                             [STATS_SLIDE_ROW_COUNT] = {
  { 320, 320, 320, 320, 320, 320, 320, 320, 320 },
  { -320, -320, -320, -320, -320, -320, -320, -320, -320 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
// clang-format on
static u8 D_80230838_ovl_01 = 0;
static Vtx D_80230840_ovl_01[TURNED_ARROW_VERTEX_COUNT] = {
    {{{-16, 16, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-16, -16, 0}, 0, {0, 2048}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{16, -16, 0}, 0, {2048, 2048}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{16, 16, 0}, 0, {2048, 0}, {0xFF, 0xFF, 0x00, 0xFF}}}};
/* Read by no routine */
static s32 D_80230880_ovl_01[2] = {0, 0};
/* Read by no routine; the values of D_80230A38 */
static f32 D_80230888_ovl_01[4] = {0.0f, 2.0f, 4.0f, 6.0f};
/* Read by no routine */
static s32 D_80230898_ovl_01[2] = {0, 0};
static u8 D_802308A0_ovl_01 = 0;
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
static s16 D_80230958_ovl_01[4] = {0x078E, 0x078E, 0x078E, 0x078E};
/* Read by no routine */
static s16 D_80230960_ovl_01[18] = {
    0, 0, 1, 4, 5, 6, 3, 2, 7, 9, 12, 10, 13, 15, 14, 11, 8, 0};
static char D_80230984_ovl_01[STATS_GRID_ROW_COUNT][RECORD_LABEL_SIZE] = {
    "Birdie", "Eagle", "Albatross", "Hole-in-One"};
static s8 D_802309C4_ovl_01[STATS_DIRECTION_COUNT][2] = {
    {-1, 0}, {1, 0}, {0, -1}, {0, 1}};
/* Read by no routine */
static s32 D_802309CC_ovl_01[8] = {64, 12, 48, 12, 80, 12, 96, 12};
static Vtx D_802309F0_ovl_01[MENU_CURSOR_VERTEX_COUNT] = {
    {{{-48, 48, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-48, 0, 0}, 0, {0, 3072}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{0, 0, 0}, 0, {3072, 3072}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{0, 48, 0}, 0, {3072, 0}, {0xFF, 0xFF, 0x00, 0xFF}}}};
/* Read by no routine */
static s32 D_80230A30_ovl_01[2] = {0, 0};
static f32 D_80230A38_ovl_01[MENU_CURSOR_VERTEX_COUNT] = {
    0.0f, 2.0f, 4.0f, 6.0f};
static s32 D_80230A48_ovl_01[CHARACTER_COUNT] = {
    0, 1, 10, 3, 4, 2, 6, 7, 8, 9, 5, 11, 12, 13, 14, 15, 16, 17};

static Mtx D_8023ACC0_ovl_01[MAX_PLAYER_COUNT][2];
static Mtx D_8023AEC0_ovl_01;
static LookAt D_8023AF00_ovl_01[2];

/* Read by no routine */
static u8 D_8023AF40_ovl_01[0x100];

static Mtx D_8023B040_ovl_01[PAGE_ARROW_COUNT];
static Mtx D_8023B0C0_ovl_01[PAGE_ARROW_COUNT];
static Mtx D_8023B140_ovl_01[PAGE_ARROW_COUNT];

/* Read by no routine */
static u8 D_8023B1C0_ovl_01[0x100];

static Mtx D_8023B2C0_ovl_01[PAGE_ARROW_COUNT];
static void* D_8023B340_ovl_01;
static void* D_8023B344_ovl_01;
static void* D_8023B348_ovl_01;
static void* D_8023B34C_ovl_01;
/* The two frames of the stats icon that func_80219460 animates */
static void* D_8023B350_ovl_01[STATS_ICON_FRAME_COUNT];
static void* D_8023B358_ovl_01;
static void* D_8023B35C_ovl_01;
static void* D_8023B360_ovl_01;
static void* D_8023B364_ovl_01;
static void* D_8023B368_ovl_01;
static void* D_8023B36C_ovl_01;

/* Read by no routine */
static u8 D_8023B370_ovl_01[0xA0];

static Mtx D_8023B410_ovl_01;
static Mtx D_8023B450_ovl_01;
static Mtx D_8023B490_ovl_01;

/* Read by no routine */
static u8 D_8023B4D0_ovl_01[0x80];

static Mtx D_8023B550_ovl_01;

/* Read by no routine */
static u8 D_8023B590_ovl_01[0x50];

static void* D_8023B5E0_ovl_01[STATS_GRID_ROW_COUNT][STATS_GRID_COLUMN_COUNT];
static void** D_8023B630_ovl_01;
static void* D_8023B634_ovl_01;
static void* D_8023B638_ovl_01;
static u8* D_8023B63C_ovl_01;
static s16 D_8023B640_ovl_01[STATS_GRID_ROW_COUNT][STATS_GRID_COLUMN_COUNT];

void func_802178B0_ovl_01(void* buffer, s16 bufferKind) {
  EncodedAsset asset;
  MenuMemory* memory = func_8005AF74();
  s16 iconIndex;
  u8 rowNumber; /* D_801B5634 counts the index tables' rows from 1 */
  AssetId assetId;
  void** imageSlot;

  switch (bufferKind) {
    case PLAYER_STATS_BUFFER_ICONS:
      D_8023B340_ovl_01 = buffer;
      for (iconIndex = 0; iconIndex != PLAYER_STATS_ICON_COUNT; iconIndex++) {
        OpenEncodedAsset(D_802303AC_ovl_01[iconIndex], &asset);
        ReadEncodedAsset(
            (u8*)D_8023B340_ovl_01 + (iconIndex * PLAYER_STATS_ICON_SIZE),
            &asset);
      }

      break;
    case PLAYER_STATS_BUFFER_UNK2:
      D_8023B348_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_PLAYER_STATS_UNK2, &asset);
      ReadEncodedAsset(D_8023B348_ovl_01, &asset);
      break;
    case PLAYER_STATS_BUFFER_UNK3:
      D_8023B34C_ovl_01 = buffer;
      break;
    case PLAYER_STATS_BUFFER_UNK4:
      rowNumber = D_801B5634;
      imageSlot = &D_8023B350_ovl_01[0];
      *imageSlot = buffer;
      OpenEncodedAsset(
          D_802303CC_ovl_01[D_8023040C_ovl_01[rowNumber - 1][memory->unkD]][0],
          &asset);
      ReadEncodedAsset(*imageSlot, &asset);
      break;
    case PLAYER_STATS_BUFFER_UNK5:
      rowNumber = D_801B5634;
      imageSlot = &D_8023B350_ovl_01[1];
      *imageSlot = buffer;
      OpenEncodedAsset(
          D_802303CC_ovl_01[D_8023040C_ovl_01[rowNumber - 1][memory->unkD]][1],
          &asset);
      ReadEncodedAsset(*imageSlot, &asset);
      break;
    case PLAYER_STATS_BUFFER_UNK6:
      D_8023B344_ovl_01 = buffer;
      break;
    case PLAYER_STATS_BUFFER_UNK7:
      D_8023B358_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_GREEN_ARROWS, &asset);
      ReadEncodedAsset(D_8023B358_ovl_01, &asset);
      break;
    case PLAYER_STATS_BUFFER_UNK8:
      D_8023B360_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_PLAYER_STATS_UNK8, &asset);
      ReadEncodedAsset(D_8023B360_ovl_01, &asset);
      break;
    case PLAYER_STATS_BUFFER_GAME_LABELS:
      D_8023B364_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_PLAYER_STATS_GAME_LABELS, &asset);
      ReadEncodedAsset(D_8023B364_ovl_01, &asset);
      break;
    case PLAYER_STATS_BUFFER_PLAYER_LABELS:
      D_8023B368_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_PLAYER_LABELS, &asset);
      ReadEncodedAsset(D_8023B368_ovl_01, &asset);
      break;
    case PLAYER_STATS_BUFFER_BRICK_WALL:
      D_8023B36C_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_STATS_BRICK_WALL, &asset);
      ReadEncodedAsset(D_8023B36C_ovl_01, &asset);
      break;
    case PLAYER_STATS_BUFFER_UNK12:
      assetId = ASSET_PLAYER_STATS_UNK12;
      D_8023B35C_ovl_01 = buffer;
      OpenEncodedAsset(assetId, &asset);
      ReadEncodedAsset(D_8023B35C_ovl_01, &asset);
      break;
    case PLAYER_STATS_BUFFER_UNK13:
      assetId = ASSET_PLAYER_STATS_UNK13;
      D_8023B35C_ovl_01 = buffer;
      OpenEncodedAsset(assetId, &asset);
      ReadEncodedAsset(D_8023B35C_ovl_01, &asset);
      break;
  }

  D_8022F8C5_ovl_01 = 0;
  D_8022F8C0_ovl_01 = 0;
  D_8022F8C4_ovl_01 = 0;
  D_802303A0_ovl_01 = 0;
  D_802303A1_ovl_01 = 0;
}

void func_80217B88_ovl_01(s32 requestedPreset) {
  s8 presetIndex = requestedPreset;
  MenuMemory* memory = func_8005AF74();
  s16 index;

  if ((u8)requestedPreset < MENU_UNKE_PRESET_COUNT) {
    if ((s8)requestedPreset == MENU_UNKE_PRESET_STROKE &&
        g_ChosenPlayerCount == MAX_PLAYER_COUNT) {
      presetIndex = MENU_UNKE_PRESET_STROKE_FOUR_PLAYER;
    }

    if (presetIndex == MENU_UNKE_PRESET_MINI_GOLF &&
        g_ChosenPlayerCount == MAX_PLAYER_COUNT) {
      presetIndex = MENU_UNKE_PRESET_MINI_GOLF_FOUR_PLAYER;
    }

    for (index = 0; index != MENU_UNKE_COUNT; index++) {
      memory->unkE[index] = D_802304BC_ovl_01[presetIndex][index];
      D_801B60BA[index] = D_802304F4_ovl_01[memory->unkE[index]];
    }
  } else {
    for (index = 0; index != MENU_UNKE_COUNT; index++) {
      memory->unkE[index] = NO_BET;
      D_801B60BA[index] = -1;
    }
  }

  memory->unkD = 0;
  for (index = 0; index != MAX_PLAYER_COUNT; index++) {
    D_8023BFDC_ovl_01[index] = index < g_ChosenPlayerCount ? 0 : -1;
  }

  memory->unk29 = 0;
  memory->unk2A = 0;
}

/* Doesn't match: the bet icon's index and the frame swap registers ($v0 and
 * $v1) before OpenEncodedAsset. */
#ifdef NON_MATCHING
void func_80217D20_ovl_01(void) {
  EncodedAsset asset;
  MenuMemory* memory;
  s16 index;

  GetSaveData();
  memory = func_8005AF74();
  if (D_8022BE88_ovl_01 == MENU_PHASE_START) {
    PlaySong(SONG_TITLE, 0);
    g_MenuScreen = MENU_SCREEN_UNK6;
    D_8022BE89_ovl_01 = MENU_UNK89_UNK3;
  }

  if (D_8022BE88_ovl_01 != BET_PHASE_CHOOSING) {
    return;
  }

  if (g_MenuPressedButtons & A_BUTTON) {
    PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
    D_8023BFC0_ovl_01[0] = guRandom() % FIDGET_DELAY_RANGE;
    D_8023BFC0_ovl_01[1] = guRandom() % FIDGET_DELAY_RANGE;
    D_8023BFC0_ovl_01[2] = guRandom() % FIDGET_DELAY_RANGE;
    D_8023BFC0_ovl_01[3] = guRandom() % FIDGET_DELAY_RANGE;
    for (index = 0; index != MAXCONTROLLERS; index++) {
      D_801B70F0[index] = 0;
    }

    D_8022BE88_ovl_01 = BET_PHASE_CONFIRMED;
    FreeAllCharacterSlots();
    g_MenuScreen = MENU_SCREEN_UNK20;
    D_802303A0_ovl_01 = 0;
    SetTextSlot(0, 0, NO_MESSAGE, 1, 0, TEXT_FLAG_TYPED, 255, 255, 255, 255);
    for (index = 0; index != MENU_UNKE_COUNT; index++) {
      if (memory->unkE[index] != NO_BET) {
        D_801B60BA[index] = D_802304F4_ovl_01[memory->unkE[index]];
      } else {
        D_801B60BA[index] = -1;
      }
    }

    return;
  }

  if (g_MenuPressedButtons & B_BUTTON) {
    PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    D_8022BE88_ovl_01 = BET_PHASE_CANCELLED;
    D_802303A0_ovl_01 = 0;
    SetTextSlot(0, 0, NO_MESSAGE, 1, 0, TEXT_FLAG_TYPED, 255, 255, 255, 255);
    for (index = 0; index != MENU_UNKE_COUNT; index++) {
      if (memory->unkE[index] != NO_BET) {
        D_801B60BA[index] = D_802304F4_ovl_01[memory->unkE[index]];
      } else {
        D_801B60BA[index] = -1;
      }
    }

    return;
  }

  if (g_MenuRepeatButtons & U_JPAD) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    index = D_80230460_ovl_01[D_801B5634 - 1][memory->unkD];
    if (memory->unkE[index] < BET_STAKE_LEVEL_COUNT - 1) {
      memory->unkE[index]++;
    }
  } else if (g_MenuRepeatButtons & D_JPAD) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    index = D_80230460_ovl_01[D_801B5634 - 1][memory->unkD];
    if (memory->unkE[index] > 0) {
      memory->unkE[index]--;
    }
  } else if (g_MenuRepeatButtons & L_JPAD) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    memory->unkD--;
    if (memory->unkD < 0) {
      memory->unkD = D_802304B4_ovl_01[D_801B5634 - 1] - 1;
    }

    SetTextSlot(BET_HELP_X, BET_HELP_Y,
        D_8023040C_ovl_01[D_801B5634 - 1][memory->unkD] + BET_HELP_MESSAGE, 1,
        0, TEXT_FLAG_TYPED, 255, 255, 255, 255);
    OpenEncodedAsset(
        D_802303CC_ovl_01[D_8023040C_ovl_01[D_801B5634 - 1][memory->unkD]]
                         [D_8022F8C0_ovl_01],
        &asset);
    ReadEncodedAsset(D_8023B350_ovl_01[D_8022F8C4_ovl_01 ^ 1], &asset);
    D_8022F8C4_ovl_01 ^= 1;
  } else if (g_MenuRepeatButtons & R_JPAD) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    memory->unkD++;
    if (memory->unkD > D_802304B4_ovl_01[D_801B5634 - 1] - 1) {
      memory->unkD = 0;
    }

    SetTextSlot(BET_HELP_X, BET_HELP_Y,
        D_8023040C_ovl_01[D_801B5634 - 1][memory->unkD] + BET_HELP_MESSAGE, 1,
        0, TEXT_FLAG_TYPED, 255, 255, 255, 255);
    OpenEncodedAsset(
        D_802303CC_ovl_01[D_8023040C_ovl_01[D_801B5634 - 1][memory->unkD]]
                         [D_8022F8C0_ovl_01],
        &asset);
    ReadEncodedAsset(D_8023B350_ovl_01[D_8022F8C4_ovl_01 ^ 1], &asset);
    D_8022F8C4_ovl_01 ^= 1;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80217D20_ovl_01);
#endif

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
 * The banner of D_8023B34C is drawn twice: stretched to 270 columns in
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
/* The chosen bet's label is centred on the banner */
#define BET_LABEL_CENTER_X 160
#define BET_LABEL_TOP 156

/*
 * Draws the bet settings screen: each bet's icon and stake marks shifted right
 * by slide, and the banner and chosen bet's name moved down by slide (or up,
 * when isSlideReversed is set). Once the screen has settled it types the
 * chosen bet's explanation
 */
/*
 * Doesn't match: the original keeps its D_801B5634 loads in other registers
 * and schedules the stake marks' coordinates differently
 */
#ifdef NON_MATCHING
void func_8021826C_ovl_01(
    Gfx** displayListHead, s32 slide, u8 isSlideReversed, u8 isSettled) {
  Gfx* gfx = *displayListHead;
  MenuMemory* memory = func_8005AF74();
  s16 columnIndex;
  s16 stakeLevel;
  s16 betIndex;
  s32 markGrowth;
  s32 markRaise;
  s32 bannerShift;
  s16 stripIndex;
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

  for (columnIndex = 0; columnIndex != D_802304B4_ovl_01[D_801B5634 - 1];
      columnIndex++) {
    betIndex = D_8023040C_ovl_01[D_801B5634 - 1][columnIndex];
    if (betIndex == -1) {
      break;
    }

    if (columnIndex != memory->unkD) {
      icon = (u8*)D_8023B340_ovl_01 + (betIndex * PLAYER_STATS_ICON_SIZE);
      gDPLoadTLUT_pal16(gfx++, 0, ((u32)icon + IMAGE_HEADER_SIZE) & ~7);
      /* Bets without a stake are dimmed */
      if (memory->unkE[D_80230460_ovl_01[D_801B5634 - 1][columnIndex]] == 0) {
        gDPSetCombineMode(
            gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, 127, 127, 127, 255);
      } else {
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      }

      gDPLoadTextureTile_4b(gfx++, ((u32)icon + PALETTED_TEXELS_OFFSET) & ~7,
          G_IM_FMT_CI, STATS_ICON_SIZE, STATS_ICON_SIZE, 0, 0,
          STATS_ICON_SIZE - 1, STATS_ICON_SIZE - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++,
          (D_802304FC_ovl_01[D_801B5634 - 1][columnIndex] + slide) << 2,
          BET_ICON_TOP << 2,
          (D_802304FC_ovl_01[D_801B5634 - 1][columnIndex] + slide +
              STATS_ICON_SIZE)
              << 2,
          (BET_ICON_TOP + STATS_ICON_SIZE) << 2, G_TX_RENDERTILE, 0, 0,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }

    func_80210DF4_ovl_01(&gfx,
        D_802304FC_ovl_01[D_801B5634 - 1][columnIndex] + slide +
            BET_AMOUNT_OFFSET_X,
        BET_AMOUNT_Y,
        (s8)D_802304F4_ovl_01[memory
                ->unkE[D_80230460_ovl_01[D_801B5634 - 1][columnIndex]]],
        0);
  }

  /* The chosen bet's icon is animated */
  func_80219460_ovl_01(&gfx,
      D_802304FC_ovl_01[D_801B5634 - 1][memory->unkD] + slide,
      CHOSEN_BET_ICON_TOP, STATS_ICON_SIZE, TEXEL_STEP_ONE);

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTextureTile_4b(gfx++,
      ((u32)D_8023B348_ovl_01 + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
      STAKE_MARK_SIZE, STAKE_MARK_SIZE, 0, 0, STAKE_MARK_SIZE - 1,
      STAKE_MARK_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8023B348_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
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

    for (columnIndex = 0; columnIndex != D_802304B4_ovl_01[D_801B5634 - 1];
        columnIndex++) {
      /* Marks above the bet's stake are dimmed */
      if (stakeLevel >=
          memory->unkE[D_80230460_ovl_01[D_801B5634 - 1][columnIndex]]) {
        gDPSetCombineMode(
            gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, 127, 127, 127, 255);
      } else {
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      }

      gDPPipeSync(gfx++);

      columnX = D_802304FC_ovl_01[D_801B5634 - 1][columnIndex] + slide +
          STAKE_MARK_OFFSET_X;
      gSPScisTextureRectangle(gfx++, (columnX - (markGrowth >> 1)) << 2,
          ((STAKE_MARK_BOTTOM - STAKE_MARK_SIZE) - markGrowth -
              (stakeLevel * STAKE_MARK_SPACING) - markRaise)
              << 2,
          (columnX + ((markGrowth >> 1) + STAKE_MARK_SIZE)) << 2,
          (STAKE_MARK_BOTTOM - markRaise - (stakeLevel * STAKE_MARK_SPACING))
              << 2,
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
  gDPLoadTLUT_pal256(gfx++, ((u32)D_8023B34C_ovl_01 + IMAGE_HEADER_SIZE) & ~7);

  bannerShift = slide;
  if (isSlideReversed) {
    bannerShift = -slide;
  }

  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)D_8023B34C_ovl_01 + LARGE_PALETTED_TEXELS_OFFSET +
            (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, WIDE_BANNER_LEFT << 2,
        (bannerShift +
            ((stripIndex * WIDE_BANNER_STRIP_SPACING) + WIDE_BANNER_TOP) -
            WIDE_BANNER_RAISE)
            << 2,
        WIDE_BANNER_RIGHT << 2,
        (bannerShift +
            ((stripIndex * WIDE_BANNER_STRIP_SPACING) + WIDE_BANNER_TOP) -
            WIDE_BANNER_RAISE + WIDE_BANNER_STRIP_HEIGHT)
            << 2,
        G_TX_RENDERTILE, 0, 0,
        (MENU_BANNER_WIDTH << 10) / (WIDE_BANNER_RIGHT - WIDE_BANNER_LEFT),
        (MENU_BANNER_STRIP_HEIGHT << 10) / WIDE_BANNER_STRIP_HEIGHT);
    gDPPipeSync(gfx++);
  }

  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)D_8023B34C_ovl_01 + LARGE_PALETTED_TEXELS_OFFSET +
            (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, BET_BANNER_LEFT << 2,
        (bannerShift +
            ((stripIndex * MENU_BANNER_STRIP_HEIGHT) + BET_BANNER_TOP))
            << 2,
        BET_BANNER_RIGHT << 2,
        (bannerShift +
            ((stripIndex * MENU_BANNER_STRIP_HEIGHT) + BET_BANNER_TOP) +
            MENU_BANNER_STRIP_HEIGHT)
            << 2,
        G_TX_RENDERTILE, 0, 0,
        (MENU_BANNER_WIDTH << 10) / (BET_BANNER_RIGHT - BET_BANNER_LEFT),
        TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  betIndex = D_8023040C_ovl_01[D_801B5634 - 1][memory->unkD];
  func_8021B1E0_ovl_01(
      &gfx, BET_LABEL_CENTER_X, bannerShift + BET_LABEL_TOP, betIndex);
  if (D_802303A0_ovl_01 == 0 && isSettled == TRUE) {
    SetTextSlot(BET_HELP_X, BET_HELP_Y, betIndex + BET_HELP_MESSAGE, 1, 0,
        TEXT_FLAG_TYPED, 255, 255, 255, 255);
    D_802303A0_ovl_01 = 1;
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021826C_ovl_01);
#endif

void func_80219460_ovl_01(
    Gfx** displayListHead, s32 left, s32 top, s32 size, s32 textureStep) {
  Gfx* gfx = *displayListHead;
  MenuMemory* memory = func_8005AF74();
  EncodedAsset asset;
  void* image;
  u8 iconIndex;

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  image = D_8023B350_ovl_01[D_8022F8C4_ovl_01];
  gDPLoadTLUT_pal16(gfx++, 0, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  gDPLoadTextureTile_4b(gfx++, ((u32)image + PALETTED_TEXELS_OFFSET) & ~7,
      G_IM_FMT_CI, STATS_ICON_SIZE, STATS_ICON_SIZE, 0, 0, STATS_ICON_SIZE - 1,
      STATS_ICON_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gSPScisTextureRectangle(gfx++, left << 2, top << 2, (left + size) << 2,
      (top + size) << 2, G_TX_RENDERTILE, 0, 0, textureStep, textureStep);
  D_8022F8C5_ovl_01++;
  gDPPipeSync(gfx++);
  if (D_8022F8C5_ovl_01 == STATS_ICON_FRAME_DURATION) {
    D_8022F8C5_ovl_01 = 0;
    D_8022F8C0_ovl_01++;
    if (D_8022F8C0_ovl_01 >= STATS_ICON_FRAME_COUNT) {
      D_8022F8C0_ovl_01 = 0;
    }

    D_8022F8C4_ovl_01 = !D_8022F8C4_ovl_01;
    iconIndex = D_8023040C_ovl_01[D_801B5634 - 1][memory->unkD];
    OpenEncodedAsset(D_802303CC_ovl_01[iconIndex][D_8022F8C0_ovl_01], &asset);
    ReadEncodedAsset(D_8023B350_ovl_01[D_8022F8C4_ovl_01], &asset);
  }

  *displayListHead = gfx;
}

/* Returns nothing, but is declared to return a value, which leaves v0 live at
 * its end */
s32 func_80219948_ovl_01(void) {
  MenuMemory* memory = func_8005AF74();

  if (D_8022BE88_ovl_01 == MENU_PHASE_START) {
    PlaySong(SONG_TITLE, 0);
    g_MenuScreen = MENU_SCREEN_UNK6;
    D_8022BE89_ovl_01 = MENU_UNK89_UNK4;
    FreeAllCharacterSlots();
  }

  if (D_8022BE88_ovl_01 == MENU_PHASE_CHOOSING) {
    if (g_MenuPressedButtons & A_BUTTON) {
      PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      SetTextSlot(0, 0, NO_MESSAGE, 1, 0, 1, 0xFF, 0xFF, 0xFF, 0xFF);
      D_8023BFD0_ovl_01 = PLAYER_OPTION_SLIDE_START;
      D_802303A1_ovl_01 = 0;
      D_8022BE88_ovl_01 = MENU_PHASE_CONFIRMED;
      return;
    }

    if (g_MenuPressedButtons & B_BUTTON) {
      PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      g_MenuScreen = MENU_SCREEN_UNK22;
      SetTextSlot(0, 0, NO_MESSAGE, 1, 0, 1, 0xFF, 0xFF, 0xFF, 0xFF);
      D_802303A1_ovl_01 = 0;
      D_8022BE88_ovl_01 = MENU_PHASE_CANCELLED;
      return;
    }

    if (g_MenuRepeatButtons & L_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      memory->unk29--;
      if (memory->unk29 < 0) {
        memory->unk29 = g_ChosenPlayerCount - 1;
      }

      D_802303A1_ovl_01 = 0;
      return;
    }

    if (g_MenuRepeatButtons & R_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      memory->unk29++;
      if (memory->unk29 > g_ChosenPlayerCount - 1) {
        memory->unk29 = 0;
      }

      D_802303A1_ovl_01 = 0;
      return;
    }

    if (g_MenuRepeatButtons & U_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      D_8023BFDC_ovl_01[memory->unk29]++;
      if (D_8023BFDC_ovl_01[memory->unk29] >= PLAYER_OPTION_COUNT) {
        D_8023BFDC_ovl_01[memory->unk29] = 0;
      }

      return;
    }

    if (g_MenuRepeatButtons & D_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      D_8023BFDC_ovl_01[memory->unk29]--;
      if (D_8023BFDC_ovl_01[memory->unk29] < 0) {
        D_8023BFDC_ovl_01[memory->unk29] = PLAYER_OPTION_COUNT - 1;
      }
    }
  }
}

/* The 200x30 CI8 banner along screen 20's foot, drawn in strips of 10 rows,
 * each stretched across the screen */
#define OPTION_BANNER_WIDTH 200
#define OPTION_BANNER_STRIP_HEIGHT 10
#define OPTION_BANNER_STRIP_COUNT 3
#define OPTION_BANNER_STRIP_SIZE \
  (OPTION_BANNER_WIDTH * OPTION_BANNER_STRIP_HEIGHT)
#define OPTION_BANNER_LEFT 26
#define OPTION_BANNER_RIGHT 296
#define OPTION_BANNER_TOP 190
#define OPTION_BANNER_STEP_X \
  ((OPTION_BANNER_WIDTH << 10) / (OPTION_BANNER_RIGHT - OPTION_BANNER_LEFT))

/* Each player's 64x44 CI4 column, drawn in strips of 22 rows, with the
 * player's label and choice on it */
#define OPTION_COLUMN_WIDTH 64
#define OPTION_COLUMN_STRIP_HEIGHT 22
#define OPTION_COLUMN_STRIP_COUNT 2
#define OPTION_COLUMN_STRIP_SIZE \
  (OPTION_COLUMN_WIDTH * OPTION_COLUMN_STRIP_HEIGHT / 2)
#define OPTION_COLUMN_TOP 150
/* The columns of the players other than the chosen one are dimmed */
#define UNCHOSEN_COLUMN_SHADE 200
#define OPTION_LABEL_OFFSET_X 4
#define OPTION_LABEL_TOP 154
#define OPTION_LABEL_TMEM 0x80
#define OPTION_LABEL_TILE 1
#define OPTION_LABEL_PALETTE 1
#define OPTION_NUMBER_OFFSET_X 16
#define OPTION_NUMBER_Y 166

/* The arrows around the chosen column, each pushed outward while its D-Pad
 * button is held. The left arrow is the right one's image mirrored. */
#define OPTION_ARROW_UP 0
#define OPTION_ARROW_DOWN 1
#define OPTION_ARROW_RIGHT 2
#define OPTION_ARROW_LEFT 3
#define OPTION_ARROW_COUNT 4
#define OPTION_ARROW_MASK 4
#define OPTION_ARROW_PUSH 6
#define OPTION_ARROW_MIDDLE_X 24
#define OPTION_ARROW_MIDDLE_Y 15
#define OPTION_ARROW_UP_Y 0
#define OPTION_ARROW_DOWN_Y 30
#define OPTION_ARROW_RIGHT_X 48
#define OPTION_ARROW_LEFT_X (-1)

/* The prompt, which names the chosen player */
#define OPTION_PROMPT_X 36
#define OPTION_PROMPT_Y 200
#define OPTION_PROMPT_MESSAGE 0x1D

void func_80210CD4_ovl_01(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);

/*
 * Draws screen 20: the players' characters, a column for each player with
 * the player's label and D_8023BFDC value, the banner below them, and the
 * arrows around the column of the player that memory->unk29 picks. All but
 * the banner slide across by slide, and the banner and arrows down by it, or
 * up when reverseVerticalSlide is set. With canShowPrompt set, the prompt
 * naming that player is typed once each time D_802303A1 is cleared.
 */
/*
 * Doesn't match: the original adds the label's offset to the slide, and the
 * columns' top to the vertical slide, once before their loops, and adds the
 * texel offset to the blink frame's offset before the arrow's.
 */
#ifdef NON_MATCHING
void func_80219BD4_ovl_01(Gfx** displayListHead, s32 slide,
    u8 reverseVerticalSlide, u8 canShowPrompt) {
  Gfx* gfx = *displayListHead;
  MenuMemory* memory = func_8005AF74();
  u16 playerIndex;
  s32 columnIndex;
  s32 stripIndex;
  s32 arrowIndex;
  s32 labelIndex;
  s32 offsetY;
  s32 arrowX = 0;
  s32 arrowY = 0;
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
  u8 unusedStack[0x30];
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
  guOrtho(&D_8023AEC0_ovl_01, -(SCREEN_WIDTH / 2), SCREEN_WIDTH / 2,
      -(SCREEN_HEIGHT / 2), SCREEN_HEIGHT / 2, MENU_CHARACTERS_NEAR,
      MENU_CHARACTERS_FAR, MENU_CHARACTERS_ORTHO_SCALE);
  {
    Mat4 viewing;

    guLookAtReflectF(viewing, &D_8023AF00_ovl_01[g_DrawBufferIndex], 0.0f,
        MENU_CHARACTERS_EYE_Y, MENU_CHARACTERS_EYE_Z, 0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f);
  }

  gSPLookAt(gfx++, &D_8023AF00_ovl_01[g_DrawBufferIndex]);
  for (playerIndex = 0; playerIndex != g_ChosenPlayerCount; playerIndex++) {
    if (D_8023BFC0_ovl_01[playerIndex] > 0) {
      D_8023BFC0_ovl_01[playerIndex]--;
    }

    if (D_8023BFC0_ovl_01[playerIndex] == 0) {
      D_8023BFC0_ovl_01[playerIndex] = -1;
      SetAnimationLooping(playerIndex, FALSE);
      SetNextAnimation(playerIndex, MENU_ANIMATION_FIDGET);
    }

    if (D_8023BFC0_ovl_01[playerIndex] == -1) {
      if (GetAnimationEndFrame(playerIndex, MENU_ANIMATION_FIDGET) ==
          GetAnimationFrame(playerIndex)) {
        SetCharacterAnimation(playerIndex, MENU_ANIMATION_STANDING);
        SetAnimationLooping(playerIndex, TRUE);
        D_8023BFC0_ovl_01[playerIndex] = guRandom() % FIDGET_DELAY_RANGE;
      }
    }

    if ((u8)D_801B70F0[playerIndex] == FALSE) {
      if (IsCharacterSlotBusy(playerIndex)) {
        FreeCharacterSlot(playerIndex);
      } else {
        LoadCharacter(playerIndex,
            (s8)g_Players[playerIndex].characterIndex + CHARACTER_COUNT,
            (s8)g_Players[playerIndex].paletteIndex);
        D_801B70F0[playerIndex] = TRUE;
        SetCharacterAnimation(playerIndex, MENU_ANIMATION_STANDING);
        SetAnimationSpeed(playerIndex, 1.0f);
      }
    }

    rotation[0] = DEGREES_TO_RADIANS(D_802305F4_ovl_01[0]);
    rotation[1] = DEGREES_TO_RADIANS(D_802305F4_ovl_01[1]);
    rotation[2] = 0.0f;
    translation[0] =
        D_80230618_ovl_01[g_ChosenPlayerCount - 1][playerIndex][0] + slide;
    translation[1] = D_80230618_ovl_01[g_ChosenPlayerCount - 1][playerIndex][1];
    translation[2] = D_80230618_ovl_01[g_ChosenPlayerCount - 1][playerIndex][2];
    scale[0] = D_80230608_ovl_01[playerIndex];
    scale[1] = D_80230608_ovl_01[playerIndex];
    scale[2] = D_80230608_ovl_01[playerIndex];

    BuildTransformMatrix(transform, rotation, translation, scale);
    ConvertMatrixToFixed(
        transform, &D_8023ACC0_ovl_01[playerIndex][g_DrawBufferIndex]);

    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023AEC0_ovl_01),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++,
        OS_K0_TO_PHYSICAL(&D_8023ACC0_ovl_01[playerIndex][g_DrawBufferIndex]),
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
  gDPLoadTLUT_pal256(gfx++, ((u32)D_8023B34C_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  offsetY = slide;
  if (reverseVerticalSlide) {
    offsetY = -slide;
  }

  for (stripIndex = 0; stripIndex != OPTION_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)D_8023B34C_ovl_01 + LARGE_PALETTED_TEXELS_OFFSET +
            (stripIndex * OPTION_BANNER_STRIP_SIZE)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, OPTION_BANNER_WIDTH,
        OPTION_BANNER_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gSPScisTextureRectangle(gfx++, OPTION_BANNER_LEFT << 2,
        (offsetY + OPTION_BANNER_TOP +
            (stripIndex * OPTION_BANNER_STRIP_HEIGHT))
            << 2,
        OPTION_BANNER_RIGHT << 2,
        (offsetY + OPTION_BANNER_TOP +
            (stripIndex * OPTION_BANNER_STRIP_HEIGHT) +
            OPTION_BANNER_STRIP_HEIGHT)
            << 2,
        G_TX_RENDERTILE, 0, 0, OPTION_BANNER_STEP_X, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8023B360_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  gDPLoadTLUT_pal16(gfx++, OPTION_LABEL_PALETTE,
      ((u32)D_8023B368_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  for (columnIndex = 0; columnIndex != g_ChosenPlayerCount; columnIndex++) {
    if (columnIndex == memory->unk29) {
      gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    } else {
      gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
      gDPSetPrimColor(gfx++, 0, 0, UNCHOSEN_COLUMN_SHADE, UNCHOSEN_COLUMN_SHADE,
          UNCHOSEN_COLUMN_SHADE, 255);
    }

    for (stripIndex = 0; stripIndex != OPTION_COLUMN_STRIP_COUNT;
        stripIndex++) {
      gDPLoadTextureBlock_4b(gfx++,
          ((u32)D_8023B360_ovl_01 + PALETTED_TEXELS_OFFSET +
              (stripIndex * OPTION_COLUMN_STRIP_SIZE)) &
              ~7,
          G_IM_FMT_CI, OPTION_COLUMN_WIDTH, OPTION_COLUMN_STRIP_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++,
          (D_802306D8_ovl_01[g_ChosenPlayerCount - 1][columnIndex] + slide)
              << 2,
          (OPTION_COLUMN_TOP + (stripIndex * OPTION_COLUMN_STRIP_HEIGHT)) << 2,
          (D_802306D8_ovl_01[g_ChosenPlayerCount - 1][columnIndex] + slide +
              OPTION_COLUMN_WIDTH)
              << 2,
          (OPTION_COLUMN_TOP + (stripIndex * OPTION_COLUMN_STRIP_HEIGHT) +
              OPTION_COLUMN_STRIP_HEIGHT)
              << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }

    labelIndex = columnIndex;
    if (g_PlayerControllerIndices[columnIndex] == NO_CONTROLLER_INDEX) {
      labelIndex = columnIndex + PLAYER_LABEL_COMPUTER_FIRST;
    }

    gDPLoadMultiBlock_4b(gfx++,
        ((u32)D_8023B368_ovl_01 + (labelIndex * PLAYER_LABEL_IMAGE_SIZE) +
            PALETTED_TEXELS_OFFSET) &
            ~7,
        OPTION_LABEL_TMEM, OPTION_LABEL_TILE, G_IM_FMT_CI, PLAYER_LABEL_WIDTH,
        PLAYER_LABEL_HEIGHT, OPTION_LABEL_PALETTE, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++,
        (D_802306D8_ovl_01[g_ChosenPlayerCount - 1][columnIndex] +
            (slide + OPTION_LABEL_OFFSET_X))
            << 2,
        OPTION_LABEL_TOP << 2,
        (D_802306D8_ovl_01[g_ChosenPlayerCount - 1][columnIndex] +
            (slide + OPTION_LABEL_OFFSET_X) + PLAYER_LABEL_WIDTH)
            << 2,
        (OPTION_LABEL_TOP + PLAYER_LABEL_HEIGHT) << 2, OPTION_LABEL_TILE, 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  D_802303A4_ovl_01++;
  if (D_802303A4_ovl_01 == STATS_ARROW_BLINK_FRAME_COUNT) {
    D_802303A4_ovl_01 = 0;
    D_802303A2_ovl_01 ^= 1;
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8023B358_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  for (arrowIndex = 0; arrowIndex != OPTION_ARROW_COUNT; arrowIndex++) {
    if (arrowIndex == OPTION_ARROW_LEFT) {
      gDPLoadTextureTile_4b(gfx++,
          ((u32)D_8023B358_ovl_01 +
              ((D_802303A2_ovl_01 * STATS_ARROW_BLINK_STRIDE) +
                  PALETTED_TEXELS_OFFSET +
                  (OPTION_ARROW_RIGHT * STATS_ARROW_IMAGE_SIZE))) &
              ~7,
          G_IM_FMT_CI, STATS_ARROW_SIZE, STATS_ARROW_SIZE, 0, 0,
          STATS_ARROW_SIZE - 1, STATS_ARROW_SIZE - 1, 0,
          G_TX_MIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP, OPTION_ARROW_MASK,
          OPTION_ARROW_MASK, G_TX_NOLOD, G_TX_NOLOD);
    } else {
      gDPLoadTextureTile_4b(gfx++,
          ((u32)D_8023B358_ovl_01 +
              ((D_802303A2_ovl_01 * STATS_ARROW_BLINK_STRIDE) +
                  PALETTED_TEXELS_OFFSET +
                  (arrowIndex * STATS_ARROW_IMAGE_SIZE))) &
              ~7,
          G_IM_FMT_CI, STATS_ARROW_SIZE, STATS_ARROW_SIZE, 0, 0,
          STATS_ARROW_SIZE - 1, STATS_ARROW_SIZE - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    }

    gDPPipeSync(gfx++);
    if (arrowIndex == OPTION_ARROW_UP) {
      arrowX = OPTION_ARROW_MIDDLE_X;
      arrowY = (g_MenuRepeatButtons & U_JPAD)
          ? (OPTION_ARROW_UP_Y - OPTION_ARROW_PUSH)
          : OPTION_ARROW_UP_Y;
    } else if (arrowIndex == OPTION_ARROW_DOWN) {
      arrowX = OPTION_ARROW_MIDDLE_X;
      arrowY = (g_MenuRepeatButtons & D_JPAD)
          ? (OPTION_ARROW_DOWN_Y + OPTION_ARROW_PUSH)
          : OPTION_ARROW_DOWN_Y;
    } else if (arrowIndex == OPTION_ARROW_RIGHT) {
      arrowY = OPTION_ARROW_MIDDLE_Y;
      arrowX = (g_MenuRepeatButtons & R_JPAD)
          ? (OPTION_ARROW_RIGHT_X + OPTION_ARROW_PUSH)
          : OPTION_ARROW_RIGHT_X;
    } else if (arrowIndex == OPTION_ARROW_LEFT) {
      arrowY = OPTION_ARROW_MIDDLE_Y;
      arrowX = (g_MenuRepeatButtons & L_JPAD)
          ? (OPTION_ARROW_LEFT_X - OPTION_ARROW_PUSH)
          : OPTION_ARROW_LEFT_X;
    }

    /* The left arrow starts at the right edge of its mirrored image */
    gSPScisTextureRectangle(gfx++,
        (D_802306D8_ovl_01[g_ChosenPlayerCount - 1][memory->unk29] + arrowX +
            slide)
            << 2,
        (offsetY + OPTION_COLUMN_TOP + arrowY) << 2,
        (D_802306D8_ovl_01[g_ChosenPlayerCount - 1][memory->unk29] + arrowX +
            slide + STATS_ARROW_SIZE)
            << 2,
        (offsetY + OPTION_COLUMN_TOP + arrowY + STATS_ARROW_SIZE) << 2,
        G_TX_RENDERTILE,
        (arrowIndex == OPTION_ARROW_LEFT) ? (STATS_ARROW_SIZE << 5) : 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  for (columnIndex = 0; columnIndex != g_ChosenPlayerCount; columnIndex++) {
    func_80210CD4_ovl_01(&gfx,
        D_802306D8_ovl_01[g_ChosenPlayerCount - 1][columnIndex] +
            (slide + OPTION_NUMBER_OFFSET_X),
        OPTION_NUMBER_Y, D_8023BFDC_ovl_01[columnIndex], MENU_NUMBER_PLAIN);
  }

  if ((D_802303A1_ovl_01 == FALSE) & (canShowPrompt == TRUE)) {
    SetTextSlotValue(memory->unk29 + 1, 0, 0);

    /* The prompt names the player in the player's own colour */
    if (memory->unk29 == 0) {
      SetTextPaletteColor(255, 127, 127, 0);
    }

    if (memory->unk29 == 1) {
      SetTextPaletteColor(0, 255, 255, 0);
    }

    if (memory->unk29 == 2) {
      SetTextPaletteColor(255, 255, 0, 0);
    }

    if (memory->unk29 == 3) {
      SetTextPaletteColor(63, 255, 63, 0);
    }

    SetTextSlot(OPTION_PROMPT_X, OPTION_PROMPT_Y, OPTION_PROMPT_MESSAGE, 1, 0,
        TEXT_FLAG_TYPED, 255, 255, 255, 255);
    D_802303A1_ovl_01 = TRUE;
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80219BD4_ovl_01);
#endif

void func_8021B1E0_ovl_01(
    Gfx** displayListHead, s32 centerX, s32 top, u8 labelIndex) {
  Gfx* gfx = *displayListHead;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8023B364_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  gDPLoadTextureBlock_4b(gfx++,
      ((u32)D_8023B364_ovl_01 + PALETTED_TEXELS_OFFSET +
          (labelIndex * GAME_LABEL_IMAGE_SIZE)) &
          ~7,
      G_IM_FMT_CI, GAME_LABEL_WIDTH, GAME_LABEL_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, (centerX - (GAME_LABEL_WIDTH / 2)) << 2,
      top << 2, (centerX + (GAME_LABEL_WIDTH / 2)) << 2,
      (top + GAME_LABEL_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
      TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

s32 func_8021B618_ovl_01(Gfx** displayListHead, u8 slide) {
  Gfx* gfx = *displayListHead;
  s32 index;
  s32 rowIndex;
  s32 offset; /* The slide's first target, then the first row's offset */
  s32 first;

  if ((slide != STATS_SLIDE_CURRENT && slide < STATS_SLIDE_COUNT) == FALSE) {
    return -1;
  }

  offset = D_802307A8_ovl_01[slide - 1][0];
  first = D_80230784_ovl_01[0];
  for (index = 0; index != STATS_SLIDE_ROW_COUNT; index++) {
    if (D_80230784_ovl_01[index] != offset &&
        first != D_80230784_ovl_01[index]) {
      break;
    }
  }

  if (index == STATS_SLIDE_ROW_COUNT && D_80230838_ovl_01 != slide) {
    D_80230838_ovl_01 = slide;
    switch (slide) {
      case STATS_SLIDE_OUT_RIGHT:
        for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
          D_80230784_ovl_01[rowIndex] =
              D_802307A8_ovl_01[STATS_SLIDE_OUT_RIGHT - 1][rowIndex];
          D_8023073C_ovl_01[rowIndex] = D_80230718_ovl_01[rowIndex];
        }

        break;
      case STATS_SLIDE_OUT_LEFT:
        for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
          D_80230784_ovl_01[rowIndex] =
              D_802307A8_ovl_01[STATS_SLIDE_OUT_LEFT - 1][rowIndex];
          D_8023073C_ovl_01[rowIndex] = D_80230718_ovl_01[rowIndex];
        }

        break;
      case STATS_SLIDE_IN_FROM_RIGHT:
      case STATS_SLIDE_IN_FROM_LEFT:
        for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
          D_80230784_ovl_01[rowIndex] =
              D_802307A8_ovl_01[D_80230838_ovl_01 - 1][rowIndex];
          D_8023073C_ovl_01[rowIndex] = D_80230718_ovl_01[rowIndex];
        }

        break;
    }
  }

  func_8021B87C_ovl_01(&gfx, D_80230838_ovl_01);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
  for (index = 0; index != STATS_SLIDE_ROW_COUNT; index++) {
    if ((offset = D_80230784_ovl_01[0]) != D_80230784_ovl_01[index]) {
      break;
    }
  }

  return index != STATS_SLIDE_ROW_COUNT;
}

void func_8021B87C_ovl_01(Gfx** displayListHead, u8 slide) {
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
  gDPLoadTLUT_pal256(gfx++, ((u32)D_8023B36C_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock(gfx++,
        ((u32)D_8023B36C_ovl_01 + LARGE_PALETTED_TEXELS_OFFSET +
            (stripHeight * STATS_WALL_IMAGE_WIDTH * rowIndex)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, STATS_WALL_IMAGE_WIDTH, stripHeight, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    if (D_8023073C_ovl_01[rowIndex] > 0) {
      D_8023073C_ovl_01[rowIndex]--;
    } else {
      switch (slide) {
        case STATS_SLIDE_OUT_RIGHT:
          D_80230784_ovl_01[rowIndex] -= D_80230760_ovl_01[rowIndex];
          if (D_80230784_ovl_01[rowIndex] <= 0) {
            D_80230784_ovl_01[rowIndex] = 0;
          }

          break;
        case STATS_SLIDE_OUT_LEFT:
          D_80230784_ovl_01[rowIndex] += D_80230760_ovl_01[rowIndex];
          if (D_80230784_ovl_01[rowIndex] > 0) {
            D_80230784_ovl_01[rowIndex] = 0;
          }

          break;
        case STATS_SLIDE_IN_FROM_RIGHT:
          D_80230784_ovl_01[rowIndex] += D_80230760_ovl_01[rowIndex];
          if (D_80230784_ovl_01[rowIndex] > SCREEN_WIDTH) {
            D_80230784_ovl_01[rowIndex] = SCREEN_WIDTH;
          }

          break;
        case STATS_SLIDE_IN_FROM_LEFT:
          D_80230784_ovl_01[rowIndex] -= D_80230760_ovl_01[rowIndex];
          if (D_80230784_ovl_01[rowIndex] < -SCREEN_WIDTH) {
            D_80230784_ovl_01[rowIndex] = -SCREEN_WIDTH;
          }

          break;
      }
    }

    gSPScisTextureRectangle(gfx++, D_80230784_ovl_01[rowIndex] << 2,
        (rowIndex * STATS_WALL_ROW_HEIGHT) << 2,
        (D_80230784_ovl_01[rowIndex] + SCREEN_WIDTH) << 2,
        ((rowIndex * STATS_WALL_ROW_HEIGHT) + STATS_WALL_ROW_HEIGHT) << 2,
        G_TX_RENDERTILE, 0, 0, (STATS_WALL_IMAGE_WIDTH << 10) / SCREEN_WIDTH,
        (stripHeight << 10) / STATS_WALL_ROW_HEIGHT);
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/*
 * Doesn't match: the original computes the mirrored arrow's start without a
 * branch, and keeps the display list head in its argument slot.
 */
#ifdef NON_MATCHING
void func_8021BE88_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 arrowIndex;
  s32 left;
  s32 unused[4];

  D_802303A8_ovl_01++;
  if (D_802303A8_ovl_01 >= PAGE_ARROW_FRAME_DURATION) {
    D_802303A6_ovl_01++;
    if (D_802303A6_ovl_01 >= PAGE_ARROW_FRAME_COUNT) {
      D_802303A6_ovl_01 = 0;
    }

    D_802303A8_ovl_01 = 0;
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8023B35C_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  gDPPipeSync(gfx++);
  for (arrowIndex = 0; arrowIndex != PAGE_ARROW_COUNT; arrowIndex++) {
    if (arrowIndex == PAGE_ARROW_RIGHT) {
      /* The right arrow is the left one's image, mirrored */
      gDPLoadTextureTile_4b(gfx++,
          ((u32)D_8023B35C_ovl_01 + PALETTED_TEXELS_OFFSET +
              D_802303A6_ovl_01 * PAGE_ARROW_IMAGE_SIZE) &
              ~7,
          G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0,
          PAGE_ARROW_SIZE - 1, PAGE_ARROW_SIZE - 1, 0, G_TX_MIRROR | G_TX_WRAP,
          G_TX_MIRROR | G_TX_WRAP, PAGE_ARROW_MASK, PAGE_ARROW_MASK, G_TX_NOLOD,
          G_TX_NOLOD);
      left = PAGE_ARROW_RIGHT_X;
    } else {
      gDPLoadTextureTile_4b(gfx++,
          ((u32)D_8023B35C_ovl_01 + PALETTED_TEXELS_OFFSET +
              D_802303A6_ovl_01 * PAGE_ARROW_IMAGE_SIZE) &
              ~7,
          G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0,
          PAGE_ARROW_SIZE - 1, PAGE_ARROW_SIZE - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      left = PAGE_ARROW_LEFT_X;
    }

    gDPPipeSync(gfx++);
    gSPTextureRectangle(gfx++, left << 2, PAGE_ARROW_TOP << 2,
        (left + PAGE_ARROW_SIZE) << 2, (PAGE_ARROW_TOP + PAGE_ARROW_SIZE) << 2,
        G_TX_RENDERTILE, (arrowIndex == PAGE_ARROW_RIGHT) << 10, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021BE88_ovl_01);
#endif

/*
 * Doesn't match: the original keeps the mirrored arrow's texture start in
 * the register it computes it in, where this moves it to another.
 */
#ifdef NON_MATCHING
void func_8021C200_ovl_01(Gfx** displayListHead, s32 gap, s32 centerY) {
  Gfx* gfx = *displayListHead;
  s32 arrowIndex;
  s32 left;

  D_802303A8_ovl_01++;
  if (D_802303A8_ovl_01 >= PAGE_ARROW_FRAME_DURATION) {
    D_802303A6_ovl_01++;
    if (D_802303A6_ovl_01 >= PAGE_ARROW_FRAME_COUNT) {
      D_802303A6_ovl_01 = 0;
    }

    D_802303A8_ovl_01 = 0;
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8023B35C_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  gDPPipeSync(gfx++);
  for (arrowIndex = 0; arrowIndex != PAGE_ARROW_COUNT; arrowIndex++) {
    if (arrowIndex == PAGE_ARROW_RIGHT) {
      /* The right arrow is the left one's image, mirrored */
      gDPLoadTextureTile_4b(gfx++,
          ((u32)D_8023B35C_ovl_01 + PALETTED_TEXELS_OFFSET +
              D_802303A6_ovl_01 * PAGE_ARROW_IMAGE_SIZE) &
              ~7,
          G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0,
          PAGE_ARROW_SIZE - 1, PAGE_ARROW_SIZE - 1, 0, G_TX_MIRROR | G_TX_WRAP,
          G_TX_MIRROR | G_TX_WRAP, PAGE_ARROW_MASK, PAGE_ARROW_MASK, G_TX_NOLOD,
          G_TX_NOLOD);
      left = (SCREEN_WIDTH / 2) + (gap / 2);
    } else {
      gDPLoadTextureTile_4b(gfx++,
          ((u32)D_8023B35C_ovl_01 + PALETTED_TEXELS_OFFSET +
              D_802303A6_ovl_01 * PAGE_ARROW_IMAGE_SIZE) &
              ~7,
          G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0,
          PAGE_ARROW_SIZE - 1, PAGE_ARROW_SIZE - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      left = (SCREEN_WIDTH / 2) - PAGE_ARROW_SIZE - (gap / 2);
    }

    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, left << 2,
        (centerY - (PAGE_ARROW_SIZE / 2)) << 2, (left + PAGE_ARROW_SIZE) << 2,
        (centerY + (PAGE_ARROW_SIZE / 2)) << 2, G_TX_RENDERTILE,
        (arrowIndex == PAGE_ARROW_RIGHT) << 10, 0, TEXEL_STEP_ONE,
        TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021C200_ovl_01);
#endif

/* Nothing calls this */
void func_8021C67C_ovl_01(Gfx** displayListHead, s32 spacing, s32 y) {
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
    guOrtho(&D_8023B040_ovl_01[arrowIndex], -(SCREEN_WIDTH / 2),
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

    guTranslate(&D_8023B0C0_ovl_01[arrowIndex], x, y, TURNED_ARROW_Z);
    guRotate(&D_8023B140_ovl_01[arrowIndex], angle, 0.0f, 0.0f, 1.0f);
    guScale(&D_8023B2C0_ovl_01[arrowIndex], 1.0f, 1.0f, 1.0f);
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(
        gfx++, 0, ((u32)D_8023B35C_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureTile_4b(gfx++,
        ((u32)D_8023B35C_ovl_01 + PALETTED_TEXELS_OFFSET +
            (D_802303A6_ovl_01 * PAGE_ARROW_IMAGE_SIZE)) &
            ~7,
        G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0,
        PAGE_ARROW_SIZE - 1, PAGE_ARROW_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023B040_ovl_01[arrowIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023B0C0_ovl_01[arrowIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023B140_ovl_01[arrowIndex]),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023B2C0_ovl_01[arrowIndex]),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, D_80230840_ovl_01, TURNED_ARROW_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

/*
 * Doesn't match: the original adds the texel offset to the blink frame's
 * offset before the arrow's, and adds the arrow size to the centre first.
 */
#ifdef NON_MATCHING
void func_8021CBDC_ovl_01(Gfx** displayListHead, s32 centerY) {
  Gfx* gfx = *displayListHead;
  s32 left = 0;
  s32 offsetY = 0;
  s32 arrowIndex;

  D_802303A4_ovl_01++;
  if (D_802303A4_ovl_01 == STATS_ARROW_BLINK_FRAME_COUNT) {
    D_802303A4_ovl_01 = 0;
    D_802303A2_ovl_01 ^= 1;
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8023B358_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  for (arrowIndex = 0; arrowIndex != STATS_ARROW_COUNT; arrowIndex++) {
    gDPLoadTextureTile_4b(gfx++,
        ((u32)D_8023B358_ovl_01 + PALETTED_TEXELS_OFFSET +
            (D_802303A2_ovl_01 * STATS_ARROW_BLINK_STRIDE +
                arrowIndex * STATS_ARROW_IMAGE_SIZE)) &
            ~7,
        G_IM_FMT_CI, STATS_ARROW_SIZE, STATS_ARROW_SIZE, 0, 0,
        STATS_ARROW_SIZE - 1, STATS_ARROW_SIZE - 1, 0,
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

    gSPScisTextureRectangle(gfx++, left << 2, (offsetY + centerY) << 2,
        (left + STATS_ARROW_SIZE) << 2,
        (offsetY + (centerY + STATS_ARROW_SIZE)) << 2, G_TX_RENDERTILE, 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021CBDC_ovl_01);
#endif

void func_8021CF10_ovl_01(void** buffers) {
  EncodedAsset asset;
  s16 imageIndex;

  D_8023B630_ovl_01 = buffers;
  for (imageIndex = 0; imageIndex != PLAYER_STATS_IMAGE_COUNT; imageIndex++) {
    OpenEncodedAsset(g_CourseIconAssets[imageIndex], &asset);
    ReadEncodedAsset(D_8023B630_ovl_01[imageIndex], &asset);
  }
}

void func_8021CFAC_ovl_01(void* buffer, s16 bufferKind) {
  EncodedAsset asset;

  switch (bufferKind) {
    case 1:
      D_8023B634_ovl_01 = buffer;
      break;
    case 2:
      D_8023B638_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_CURSOR_BOX, &asset);
      ReadEncodedAsset(D_8023B638_ovl_01, &asset);
      break;
    case 3:
      D_8023B63C_ovl_01 = buffer;
      OpenEncodedAsset(0x82D, &asset);
      ReadEncodedAsset(D_8023B63C_ovl_01, &asset);
      OpenEncodedAsset(0x82F, &asset);
      ReadEncodedAsset(D_8023B63C_ovl_01 + PLAYER_STATS_IMAGE_SIZE, &asset);
      OpenEncodedAsset(0x82E, &asset);
      ReadEncodedAsset(
          D_8023B63C_ovl_01 + (PLAYER_STATS_IMAGE_SIZE * 2), &asset);
      OpenEncodedAsset(0x830, &asset);
      ReadEncodedAsset(
          D_8023B63C_ovl_01 + (PLAYER_STATS_IMAGE_SIZE * 3), &asset);
      break;
  }
}

/* func_8021E340 is called without a prototype; the original extends its result
 * from 16 bits, not 8 */
void func_8021D0B8_ovl_01(void) {
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  s32 slot;
  s32 moved;
  s32 column;

  if (IsHudAnimationDone() == TRUE) {
    if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
      if (GetWipeProgress() == 0) {
        g_MenuScreen = MENU_SCREEN_EXIT;
        D_800FE334 = 0;
        g_GameState.unk31 = 1;
        SetGameFlag(GAME_FLAG_UNK1E);
        for (slot = 0; slot != STATS_TEXT_SLOT_COUNT; slot++) {
          SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        }
      }
    } else if (GetWipeKind() == WIPE_NONE || D_801B9410 != 1) {
      if (g_MenuPressedButtons & A_BUTTON) {
        if (func_8021E468_ovl_01() != STATS_CELL_EMPTY) {
          PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
          D_801B9410 = 0;
          column = memory->unk39;
          LoadWipeShape(
              (s8)save->rankedRecords[D_8023B640_ovl_01[memory->unk3A][column]]
                  .unk38);
          StartWipe(WIPE_CLOSING);
          D_801052E8 = 0;
        }
      } else if (g_MenuPressedButtons & B_BUTTON) {
        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
        for (slot = 0; slot != STATS_TEXT_SLOT_COUNT; slot++) {
          SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        }
      } else if (g_MenuRepeatButtons & U_JPAD) {
        moved = func_8021E340_ovl_01(
            memory->unk39, memory->unk3A, STATS_DIRECTION_UP);
        if ((s16)moved != STATS_CELL_EMPTY) {
          PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          memory->unk3A = moved;
        }
      } else if (g_MenuRepeatButtons & D_JPAD) {
        moved = func_8021E340_ovl_01(
            memory->unk39, memory->unk3A, STATS_DIRECTION_DOWN);
        if ((s16)moved != STATS_CELL_EMPTY) {
          PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          memory->unk3A = moved;
        }
      } else if (g_MenuRepeatButtons & L_JPAD) {
        moved = func_8021E340_ovl_01(
            memory->unk39, memory->unk3A, STATS_DIRECTION_LEFT);
        if ((s16)moved != STATS_CELL_EMPTY) {
          PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          memory->unk39 = moved;
        }
      } else if (g_MenuRepeatButtons & R_JPAD) {
        moved = func_8021E340_ovl_01(
            memory->unk39, memory->unk3A, STATS_DIRECTION_RIGHT);
        if ((s16)moved != STATS_CELL_EMPTY) {
          PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          memory->unk39 = moved;
        }
      }
    }
  }
}

/*
 * func_8021D3AC draws the records page: a shaded panel, the image in
 * D_8023B634 in strips below it, and a row of up to five ranked faces for each
 * label of D_80230984. The chosen record also shows its course's image and
 * name.
 */
#define RECORDS_BOARD_KIND 11
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
void func_8021D3AC_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s16 index = 0;
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  s16 row;
  s16 column;
  s16 character;
  s32 chosenRow;
  s32 chosenColumn;

  func_801FB54C_ovl_01(&gfx, RECORDS_BOARD_KIND);
  gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
  gDPPipeSync(gfx++);
  gSPTextureRectangle(gfx++, RECORDS_PANEL_LEFT << 2, RECORDS_PANEL_TOP << 2,
      RECORDS_PANEL_RIGHT << 2, RECORDS_PANEL_BOTTOM << 2, G_TX_RENDERTILE, 0,
      0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
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
  gDPLoadTLUT_pal256(gfx++, ((u32)D_8023B634_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  do {
    gDPLoadTextureBlock(gfx++,
        ((u32)D_8023B634_ovl_01 + LARGE_PALETTED_TEXELS_OFFSET +
            (index * RECORDS_STRIP_WIDTH * RECORDS_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, RECORDS_STRIP_WIDTH, RECORDS_STRIP_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, RECORDS_STRIP_LEFT << 2,
        (RECORDS_STRIP_TOP + (index * RECORDS_STRIP_SPACING)) << 2,
        RECORDS_STRIP_RIGHT << 2,
        (RECORDS_STRIP_TOP + RECORDS_STRIP_SPACING +
            (index * RECORDS_STRIP_SPACING))
            << 2,
        G_TX_RENDERTILE, 0, 0,
        (RECORDS_STRIP_WIDTH << 10) /
            (RECORDS_STRIP_RIGHT - RECORDS_STRIP_LEFT),
        (RECORDS_STRIP_HEIGHT << 10) / RECORDS_STRIP_SPACING);
    gDPPipeSync(gfx++);
    index++;
  } while (index != RECORDS_STRIP_COUNT);

  for (row = 0; row != STATS_GRID_ROW_COUNT; row++) {
    for (column = 0; column != STATS_GRID_COLUMN_COUNT; column++) {
      if (D_8023B640_ovl_01[row][column] != STATS_CELL_EMPTY) {
        character =
            (s8)save->rankedRecords[D_8023B640_ovl_01[row][column]].unk3B;
        func_8021F124_ovl_01(&gfx, D_8012D430[character],
            D_8023B5E0_ovl_01[row][column],
            RECORD_FACE_X + (column * STATS_CURSOR_COLUMN_WIDTH),
            RECORD_FACE_Y + (row * STATS_CURSOR_ROW_HEIGHT), character,
            FACE_PLAIN);
      }

      func_80210C74_ovl_01(&gfx,
          RECORD_PLACE_X + (column * STATS_CURSOR_COLUMN_WIDTH),
          RECORD_PLACE_Y + (row * STATS_CURSOR_ROW_HEIGHT), column + 1,
          MENU_NUMBER_PLAIN);
    }
  }

  if (D_8023B640_ovl_01[memory->unk3A][memory->unk39] != STATS_CELL_EMPTY) {
    func_8021E97C_ovl_01(&gfx);
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
  chosenRow = memory->unk3A;
  chosenColumn = memory->unk39;
  if (D_8023B640_ovl_01[chosenRow][chosenColumn] != STATS_CELL_EMPTY) {
    gDPLoadTLUT_pal256(gfx++,
        ((u32)D_8023B630_ovl_01[(
             s8)save->rankedRecords[D_8023B640_ovl_01[chosenRow][chosenColumn]]
                 .unk38] +
            IMAGE_HEADER_SIZE) &
            ~7);
    for (index = 0; index != RECORD_COURSE_STRIP_COUNT; index++) {
      gDPLoadTextureBlock(gfx++,
          ((u32)D_8023B630_ovl_01[(s8)save
                   ->rankedRecords[D_8023B640_ovl_01[chosenRow][chosenColumn]]
                   .unk38] +
              LARGE_PALETTED_TEXELS_OFFSET +
              (index * RECORD_COURSE_IMAGE_WIDTH *
                  RECORD_COURSE_STRIP_HEIGHT)) &
              ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, RECORD_COURSE_IMAGE_WIDTH,
          RECORD_COURSE_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, RECORD_COURSE_LEFT << 2,
          (RECORD_COURSE_TOP + (index * RECORD_COURSE_STRIP_SPACING)) << 2,
          RECORD_COURSE_RIGHT << 2,
          (RECORD_COURSE_TOP + RECORD_COURSE_STRIP_SPACING +
              (index * RECORD_COURSE_STRIP_SPACING))
              << 2,
          G_TX_RENDERTILE, 0, 0,
          (RECORD_COURSE_IMAGE_WIDTH << 10) /
              (RECORD_COURSE_RIGHT - RECORD_COURSE_LEFT),
          (RECORD_COURSE_STRIP_HEIGHT << 10) / RECORD_COURSE_STRIP_SPACING);
      gDPPipeSync(gfx++);
    }
  }

  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  for (row = 0; row != STATS_GRID_ROW_COUNT; row++) {
    sprintf(g_PrintBuffer, D_802319E0_ovl_01, D_80230984_ovl_01[row]);
    DrawLargeText(&gfx, RECORD_LABEL_X,
        (s16)(RECORD_PLACE_Y + (row * STATS_CURSOR_ROW_HEIGHT)), g_PrintBuffer);
  }

  chosenRow = memory->unk3A;
  chosenColumn = memory->unk39;
  if (D_8023B640_ovl_01[chosenRow][chosenColumn] != STATS_CELL_EMPTY) {
    sprintf(g_PrintBuffer, D_802319E0_ovl_01,
        g_CourseRecords
            [(s8)save->rankedRecords[D_8023B640_ovl_01[chosenRow][chosenColumn]]
                    .unk38]
                .name);
    DrawLargeText(
        &gfx, RECORD_COURSE_NAME_X, RECORD_COURSE_NAME_Y, g_PrintBuffer);
    SetTextSlotValue(
        (s8)save->rankedRecords[D_8023B640_ovl_01[chosenRow][chosenColumn]]
                .unk39 +
            1,
        1, 0);
    SetTextSlot(RECORD_DETAIL_X, RECORD_DETAIL_Y, MESSAGE_RECORD_DETAIL, 1, 1,
        0, 255, 255, 255, 255);
  }

  if (D_802308A0_ovl_01 == 0) {
    SetTextSlot(NO_RECORDS_X, NO_RECORDS_Y, MESSAGE_NO_RECORDS, 1, 0,
        TEXT_FLAG_CENTERED, 255, 255, 255, 255);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021D3AC_ovl_01);
#endif

/*
 * Doesn't match: the original copies the chosen row's zero into the column
 * rather than loading zero, and so reads the last row into a1 instead of v1.
 */
#ifdef NON_MATCHING
void func_8021E01C_ovl_01(void) {
  SaveData* save = GetSaveData();
  s32 kindRecords[STATS_GRID_ROW_COUNT][STATS_KIND_LIST_SIZE];
  s32 index;
  s32 recordIndex;
  s32 kindThreeCount;
  s32 kindTwoCount;
  s32 kindOneCount;
  s32 kindZeroCount;
  s32 flags;
  u8 row;
  s32 column;
  MenuMemory* memory;

  InitSram();
  TransferSaveData(save, FALSE);
  for (recordIndex = 0; recordIndex != STATS_GRID_COLUMN_COUNT; recordIndex++) {
    kindRecords[0][recordIndex] = STATS_CELL_EMPTY;
    kindRecords[1][recordIndex] = STATS_CELL_EMPTY;
    kindRecords[2][recordIndex] = STATS_CELL_EMPTY;
    kindRecords[3][recordIndex] = STATS_CELL_EMPTY;
  }

  kindThreeCount = 0;
  kindTwoCount = 0;
  kindZeroCount = 0;
  kindOneCount = 0;
  for (recordIndex = 0; recordIndex != RANKED_RECORD_COUNT; recordIndex++) {
    flags = (s8)save->rankedRecords[recordIndex].unk3A;
    if (flags & RANKED_RECORD_SAVED) {
      switch (flags & RANKED_RECORD_KIND_MASK) {
        case 3:
          if (kindThreeCount < STATS_GRID_COLUMN_COUNT) {
            kindRecords[0][kindThreeCount++] = recordIndex;
          }

          break;
        case 2:
          if (kindTwoCount < STATS_GRID_COLUMN_COUNT) {
            kindRecords[1][kindTwoCount++] = recordIndex;
          }

          break;
        case 1:
          if (kindOneCount < STATS_GRID_COLUMN_COUNT) {
            kindRecords[3][kindOneCount++] = recordIndex;
          }

          break;
        case 0:
          if (kindZeroCount < STATS_GRID_COLUMN_COUNT) {
            kindRecords[2][kindZeroCount++] = recordIndex;
          }

          break;
      }
    }
  }

  row = 0;
  memory = func_8005AF74();
  column = row;
  if (kindThreeCount == 0) {
    if (kindTwoCount != 0) {
      row = 1;
    } else if (kindOneCount != 0) {
      row = 2;
    } else if (kindZeroCount != 0) {
      row = 3;
    }
  }

  switch (memory->unk3A) {
    case 0:
      if (kindThreeCount != 0) {
        row = 0;
        if (memory->unk39 < kindThreeCount && memory->unk39 >= 0) {
          column = memory->unk39;
        }
      }

      break;
    case 1:
      if (kindTwoCount != 0) {
        row = 1;
        if (memory->unk39 < kindTwoCount && memory->unk39 >= 0) {
          column = memory->unk39;
        }
      }

      break;
    case 2:
      if (kindOneCount != 0) {
        row = 2;
        if (memory->unk39 < kindOneCount && memory->unk39 >= 0) {
          column = memory->unk39;
        }
      }

      break;
    case 3:
      if (kindZeroCount != 0) {
        row = 3;
        if (memory->unk39 < kindZeroCount && memory->unk39 >= 0) {
          column = memory->unk39;
        }
      }

      break;
  }

  memory->unk39 = column;
  memory->unk3A = row;
  for (recordIndex = 0; recordIndex != STATS_GRID_COLUMN_COUNT; recordIndex++) {
    D_8023B640_ovl_01[0][recordIndex] = kindRecords[0][recordIndex];
    D_8023B640_ovl_01[1][recordIndex] = kindRecords[1][recordIndex];
    D_8023B640_ovl_01[2][recordIndex] = kindRecords[3][recordIndex];
    D_8023B640_ovl_01[3][recordIndex] = kindRecords[2][recordIndex];
  }

  recordIndex = 0;
  D_802308A0_ovl_01 = 0;
  for (; recordIndex != STATS_GRID_ROW_COUNT; recordIndex++) {
    for (index = 0; index != STATS_GRID_COLUMN_COUNT; index++) {
      if (D_8023B640_ovl_01[recordIndex][index] != STATS_CELL_EMPTY) {
        D_802308A0_ovl_01++;
        break;
      }
    }

    if (D_802308A0_ovl_01 != 0) {
      break;
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E01C_ovl_01);
#endif

s8 func_8021E340_ovl_01(s8 column, s8 row, u8 direction) {
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
    column += D_802309C4_ovl_01[direction][0];
    row += D_802309C4_ovl_01[direction][1];
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

    if (D_8023B640_ovl_01[row][column] != STATS_CELL_EMPTY) {
      break;
    }
  }

  if (stepIndex == stepCount) {
    return -1;
  }

  /* No return follows for other directions, which step no cells and so returned
   * above */
  if (direction == STATS_DIRECTION_LEFT || direction == STATS_DIRECTION_RIGHT) {
    return column;
  } else if (direction == STATS_DIRECTION_UP ||
      direction == STATS_DIRECTION_DOWN) {
    return row;
  }
}

s16 func_8021E468_ovl_01(void) {
  MenuMemory* memory = func_8005AF74();

  return D_8023B640_ovl_01[memory->unk3A][memory->unk39];
}

void func_8021E4B0_ovl_01(
    Gfx** displayListHead, s32 left, s32 top, s32 labelIndex) {
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

  image = D_8023B63C_ovl_01 + (labelIndex * PLAYER_STATS_IMAGE_SIZE);
  width = image[IMAGE_WIDTH_LOW_BYTE];
  height = image[IMAGE_HEIGHT_LOW_BYTE];

  /* The original passes the palette's first byte, not its address, so the
   * colors come from low memory */
  gDPLoadTLUT_pal16(gfx++, 0, image[IMAGE_HEADER_SIZE] & ~7);
  gDPPipeSync(gfx++);
  gDPLoadTextureBlock_4b(gfx++, ((u32)image + PALETTED_TEXELS_OFFSET) & ~7,
      G_IM_FMT_CI, width, height, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, left << 2, top << 2, (left + width) << 2,
      (top + height) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
      TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/*
 * Doesn't match: the stack slots of the pointers to the second pipe sync
 * after the image and the cycle type are swapped.
 */
#ifdef NON_MATCHING
void func_8021E97C_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  MenuMemory* memory = func_8005AF74();
  s32 vertexIndex;
  s32 red;
  s32 blue;
  s32 green;

  for (vertexIndex = 0; vertexIndex != MENU_CURSOR_VERTEX_COUNT;
      vertexIndex++) {
    D_80230A38_ovl_01[vertexIndex] += MENU_CURSOR_PHASE_STEP;
    red =
        (s32)(sinf(D_80230A38_ovl_01[vertexIndex]) * MENU_CURSOR_COLOR_SWING) +
        MENU_CURSOR_COLOR_MIDDLE;
    blue = (s32)(sinf(D_80230A38_ovl_01[vertexIndex] + (M_PI / 2)) *
               MENU_CURSOR_COLOR_SWING) +
        MENU_CURSOR_COLOR_MIDDLE;
    green = (s32)(sinf(D_80230A38_ovl_01[vertexIndex] + M_PI) *
                MENU_CURSOR_COLOR_SWING) +
        MENU_CURSOR_COLOR_MIDDLE;
    D_802309F0_ovl_01[vertexIndex].v.cn[0] = red;
    D_802309F0_ovl_01[vertexIndex].v.cn[2] = blue;
    D_802309F0_ovl_01[vertexIndex].v.cn[1] = green;
  }

  guOrtho(&D_8023B410_ovl_01, -(SCREEN_WIDTH / 2), SCREEN_WIDTH / 2,
      -(SCREEN_HEIGHT / 2), SCREEN_HEIGHT / 2, MENU_CURSOR_NEAR,
      MENU_CURSOR_FAR, 1.0f);
  guTranslate(&D_8023B450_ovl_01,
      (memory->unk39 * STATS_CURSOR_COLUMN_WIDTH) + STATS_CURSOR_LEFT_X,
      STATS_CURSOR_TOP_Y - (memory->unk3A * STATS_CURSOR_ROW_HEIGHT),
      MENU_CURSOR_Z);
  guRotate(&D_8023B490_ovl_01, 0.0f, 0.0f, 1.0f, 0.0f);
  guScale(&D_8023B550_ovl_01, 1.0f, MENU_CURSOR_SCALE_Y, 1.0f);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPLoadTextureBlock_4b(gfx++,
      ((u32)D_8023B638_ovl_01 + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_IA,
      MENU_CURSOR_SIZE, MENU_CURSOR_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
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
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023B410_ovl_01),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023B450_ovl_01),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023B490_ovl_01),
      G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023B550_ovl_01),
      G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
  gDPPipeSync(gfx++);
  gSPVertex(gfx++, D_802309F0_ovl_01, MENU_CURSOR_VERTEX_COUNT, 0);
  gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E97C_ovl_01);
#endif

/* Doesn't match: the original keeps the row and column counters in other saved
 * registers */
#ifdef NON_MATCHING
void func_8021EF64_ovl_01(void) {
  EncodedAsset asset;
  AssetFile file;
  SaveData* save = GetSaveData();
  s32 index;
  s32 row;
  s32 column;
  s32 character;
  s32 palette;
  s32 cell;

  for (index = 0; index != CHARACTER_COUNT; index++) {
    OpenEncodedAsset(g_CharacterFaceAssets[index], &asset);
    ReadEncodedAsset(D_8012D430[index], &asset);
  }

  for (row = 0; row != STATS_GRID_ROW_COUNT; row++) {
    cell = row * STATS_GRID_COLUMN_COUNT;
    for (column = 0; column != STATS_GRID_COLUMN_COUNT; column++, cell++) {
      if (D_8023B640_ovl_01[row][column] != STATS_CELL_EMPTY) {
        character =
            (s8)save->rankedRecords[D_8023B640_ovl_01[row][column]].unk3B;
        palette = (s8)save->rankedRecords[D_8023B640_ovl_01[row][column]].unk3D;
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
        palette = 0;
      }

      D_8023B5E0_ovl_01[row][column] = D_8023BFF0_ovl_01[cell];
      OpenAssetFile(g_CharacterPaletteFiles[character][palette], &file);
      ReadAssetFile(
          D_8023B5E0_ovl_01[row][column], GetAssetFileSize(&file), &file);
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021EF64_ovl_01);
#endif

void func_8021F124_ovl_01(Gfx** displayListHead, void* texels, void* palette,
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

  gDPLoadTLUT_pal256(gfx++, (u32)palette & ~7);
  gDPLoadTextureBlock(gfx++, (u32)texels & ~7, G_IM_FMT_CI, G_IM_SIZ_8b,
      FACE_WIDTH, FACE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, left << 2, top << 2, (left + FACE_WIDTH) << 2,
      (top + FACE_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
      TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  if (drawMode == FACE_WITH_NAME) {
    if (character < NAMED_CHARACTER_COUNT) {
      SetUpTextDrawing(&gfx);
      SetTextColor(&gfx, 255, 255, 255);
      sprintf(g_PrintBuffer, D_802319E0_ovl_01,
          D_8022BF70_ovl_01[D_80230A48_ovl_01[character]]);
      DrawLargeText(&gfx, left + FACE_NAME_OFFSET_X, top + FACE_NAME_OFFSET_Y,
          g_PrintBuffer);
      gDPPipeSync(gfx++);
    }
  }

  *displayListHead = gfx;
}
