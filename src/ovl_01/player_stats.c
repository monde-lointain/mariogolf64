#include "player_stats.h"

#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "bets.h"
#include "character.h"
#include "character_slot.h"
#include "compiler.h"
#include "course.h"
#include "cpu_projection.h"
#include "game.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "main_menu.h"
#include "math_util.h"
#include "menu.h"
#include "menu_arrows.h"
#include "menu_memory.h"
#include "menu_number.h"
#include "play_record.h"
#include "player.h"
#include "save.h"
#include "save_screens.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "sram.h"
#include "text.h"

#define ASSET_JAPANESE_BIRDIE_LABEL 0x82D
#define ASSET_JAPANESE_ALBATROSS_LABEL 0x82E
#define ASSET_JAPANESE_EAGLE_LABEL 0x82F
#define ASSET_JAPANESE_HOLE_IN_ONE_LABEL 0x830

#define PLAYER_STATS_IMAGE_COUNT 6

#define PLAYER_STATS_IMAGE_SIZE 0x258
#define SCORE_LABEL_COUNT 4

#define NAMED_CHARACTER_COUNT 14

#define STATS_GRID_COLUMN_COUNT 5
#define STATS_GRID_ROW_COUNT 4
#define STATS_CELL_EMPTY (-1)

enum BestShotRow {
    BEST_SHOT_ROW_BIRDIE,
    BEST_SHOT_ROW_EAGLE,
    BEST_SHOT_ROW_ALBATROSS,
    BEST_SHOT_ROW_HOLE_IN_ONE
};

enum GatheredBestShots {
    GATHERED_BIRDIES,
    GATHERED_EAGLES,
    GATHERED_HOLES_IN_ONE,
    GATHERED_ALBATROSSES
};

#define STATS_KIND_LIST_SIZE (STATS_GRID_COLUMN_COUNT + 1)

#define BET_ICON_COUNT 8
#define BET_ICON_SLOT_SIZE 0x218
#define ASSET_STAKE_MARK 0x671
#define ASSET_HANDICAP_PLAQUE 0x6C4
#define ASSET_BET_LABELS 0x6E8
#define BET_LABEL_WIDTH 128
#define BET_LABEL_HEIGHT 20
#define BET_LABEL_IMAGE_SIZE (BET_LABEL_WIDTH * BET_LABEL_HEIGHT / 2)
#define ASSET_TRIPLE_PAGE_ARROWS 0x828
#define ASSET_DOUBLE_PAGE_ARROWS 0x829
#define BET_SET_COLUMN_COUNT_MAX 6
#define OFFERING_BET_SET_COUNT BET_SET_MINI_GOLF_FOUR_PLAYER
#define UNUSED_BET_COLUMN (-1)
#define BET_ICON_SIZE 24
#define BET_ICON_FRAME_COUNT 2
#define BET_ICON_FRAME_DURATION 4

#define STATS_TEXT_SLOT_COUNT 4

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
#define MirroredArrowColumn(arrowIndex) (((arrowIndex) == PAGE_ARROW_RIGHT) * PAGE_ARROW_SIZE)
#define TURNED_ARROW_ANGLE 90.0f
#define TURNED_ARROW_Z (-100.0f)
#define TURNED_ARROW_NEAR 10.0f
#define TURNED_ARROW_FAR 2000.0f
#define TURNED_ARROW_VERTEX_COUNT 4

#define STATS_ARROW_COUNT 2
#define STATS_ARROW_BLINK_FRAME_COUNT 4
#define STATS_ARROW_LEFT 152
#define STATS_UP_ARROW_OFFSET (-52)
#define STATS_DOWN_ARROW_OFFSET 36

#define RECORD_LABEL_SIZE 16

#define STATS_CURSOR_COLUMN_WIDTH 34
#define STATS_CURSOR_ROW_HEIGHT 26
#define STATS_CURSOR_LEFT_X 6
#define STATS_CURSOR_TOP_Y 30

enum StatsDirection {
    STATS_DIRECTION_LEFT,
    STATS_DIRECTION_RIGHT,
    STATS_DIRECTION_UP,
    STATS_DIRECTION_DOWN
};

typedef u8 StatsDirection;

#define STATS_DIRECTION_COUNT 4

#define STATS_SLIDE_ROW_COUNT 9
#define STATS_WALL_IMAGE_WIDTH 152
#define STATS_WALL_STRIP_HEIGHT 13
#define STATS_WALL_ROW_HEIGHT 27
#define BET_STAKE_LEVEL_COUNT 6
#define NO_BET (-1)
#define BET_HELP_X 36
#define BET_HELP_Y 186
#define MESSAGE_BET_HELP 0x15

#define HANDICAP_CHOICE_COUNT 37
#define HANDICAP_SLIDE_START (-512)

typedef struct GridStep {
    s8 column;
    s8 row;
} GridStep;

void DrawBrickWallRows(Gfx **displayListHead, u8 slide);

void DrawBestShotsCursor(Gfx **displayListHead);
void DrawBetIcon(Gfx **displayListHead, s32 left, s32 top, s32 size, s32 textureStep);
void DrawBetLabel(Gfx **displayListHead, s32 centerX, s32 top, u8 labelIndex);

static UBoolean8 f_IsBetHelpShown = FALSE;
static UBoolean8 f_IsHandicapPromptShown = FALSE;
static s16 f_GreenArrowFrame = 0;
static s16 f_GreenArrowTimer = 0;
static s16 f_PageArrowFrame = 0;
static s16 f_PageArrowTimer = 0;
static WideAssetId f_BetIconAssets[BET_ICON_COUNT] =
    {0x0674, 0x0678, 0x067A, 0x067E, 0x0680, 0x067C, 0x0676, 0x0672};
static WideAssetId f_BetIconFrameAssets[BET_ICON_COUNT][BET_ICON_FRAME_COUNT] = {
    {0x0674, 0x0675},
    {0x0678, 0x0679},
    {0x067A, 0x067B},
    {0x067E, 0x067F},
    {0x0680, 0x0681},
    {0x067C, 0x067D},
    {0x0676, 0x0677},
    {0x0672, 0x0673},
};
static s16 f_BetSetBets[OFFERING_BET_SET_COUNT][BET_SET_COLUMN_COUNT_MAX] = {
    {0, 1, 2, 3, 5, UNUSED_BET_COLUMN},
    {0, 1, 2, 3, 5, 7},
    {0, 1, 6, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN},
    {0, 1, 5, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN},
    {0, 1, 5, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN},
    {2, 3, 5, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN},
    {2, 3, 5, 7, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN},
};
static s16 f_BetSetStakeSlots[OFFERING_BET_SET_COUNT][BET_SET_COLUMN_COUNT_MAX] = {
    {0, 1, 2, 3, 4, UNUSED_BET_COLUMN},
    {0, 1, 2, 3, 4, 6},
    {0, 1, 5, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN},
    {0, 1, 4, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN},
    {0, 1, 4, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN},
    {2, 3, 4, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN},
    {2, 3, 4, 6, UNUSED_BET_COLUMN, UNUSED_BET_COLUMN},
};
static u8 f_BetSetColumnCounts[OFFERING_BET_SET_COUNT] = {5, 6, 3, 3, 3, 3, 4};
static u8 f_BetPresetStakeLevels[BET_PRESET_COUNT][BET_STAKE_SLOT_COUNT] = {
    {4, 4, 1, 0, 0, NO_BET, NO_BET, NO_BET},
    {4, 4, 0, 0, 0, NO_BET, 0, NO_BET},
    {4, 4, NO_BET, NO_BET, NO_BET, 1, NO_BET, NO_BET},
    {4, 4, NO_BET, NO_BET, 1, NO_BET, NO_BET, NO_BET},
    {4, 4, NO_BET, NO_BET, 1, NO_BET, NO_BET, NO_BET},
    {NO_BET, NO_BET, 1, 0, 0, NO_BET, NO_BET, NO_BET},
    {NO_BET, NO_BET, 0, 0, 0, NO_BET, 0, NO_BET},
};
static u8 f_BetStakeAmounts[BET_STAKE_LEVEL_COUNT] = {0, 1, 2, 3, 5, 10};
static s32 f_BetSetColumnXs[OFFERING_BET_SET_COUNT][BET_SET_COLUMN_COUNT_MAX] = {
    {68, 108, 148, 188, 228, 228},
    {48, 88, 128, 168, 208, 248},
    {114, 148, 184, 216, 256, 256},
    {114, 148, 184, 216, 256, 256},
    {114, 148, 184, 144, 144, 144},
    {114, 148, 184, 216, 256, 256},
    {87, 127, 167, 207, 228, 228},
};
static char f_UnusedGameNames[8][10] UNUSED =
    {"ﾄﾞﾗｺﾝ", "ﾆｱﾋﾟﾝ", "ｽｺｱ", "ﾀﾃ", "ﾖｺ", "ｽｷﾝｽﾞ", "ﾏｯﾁ", "ﾀﾞﾌﾞﾙｽ"};
static f32 f_HandicapGolferAngles[2] = {0.0f, 0.0f};
static s32 f_UnusedWordsAfterGolferAngles[3] UNUSED = {0, 0, 0};
static f32 f_HandicapGolferScales[PLAYER_COUNT_MAX] = {4.2f, 4.0f, 4.2f, 4.0f};
static Vec3 f_HandicapGolferPositions[PLAYER_COUNT_MAX][PLAYER_COUNT_MAX] = {
    {
        {0.0f, -44.0f, -1000.0f},
        {64.0f, -44.0f, -1500.0f},
        {64.0f, -44.0f, -1000.0f},
        {64.0f, -44.0f, -1500.0f},
    },
    {
        {-64.0f, -44.0f, -1500.0f},
        {64.0f, -44.0f, -1000.0f},
        {64.0f, -44.0f, -1500.0f},
        {64.0f, -44.0f, -1000.0f},
    },
    {
        {-96.0f, -44.0f, -1500.0f},
        {0.0f, -54.0f, -1000.0f},
        {96.0f, -44.0f, -1500.0f},
        {96.0f, -44.0f, -1000.0f},
    },
    {
        {-96.0f, -44.0f, -1500.0f},
        {-32.0f, -54.0f, -1000.0f},
        {32.0f, -44.0f, -1500.0f},
        {96.0f, -54.0f, -1000.0f},
    },
};
static s32 f_HandicapColumnLefts[PLAYER_COUNT_MAX][PLAYER_COUNT_MAX] =
    {{128, 0, 0, 0}, {96, 160, 0, 0}, {64, 128, 192, 0}, {32, 96, 160, 224}};
static s32 f_BrickRowStartDelays[STATS_SLIDE_ROW_COUNT] = {0, 2, 4, 6, 8, 10, 12, 14, 16};
static s32 f_BrickRowDelays[STATS_SLIDE_ROW_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
static s32 f_BrickRowSpeeds[STATS_SLIDE_ROW_COUNT] = {64, 64, 64, 64, 64, 64, 64, 64, 64};
static s32 f_BrickRowOffsets[STATS_SLIDE_ROW_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
static s32 f_BrickSlideStartOffsets[STATS_SLIDE_COUNT - 1][STATS_SLIDE_ROW_COUNT] = {
    {320, 320, 320, 320, 320, 320, 320, 320, 320},
    {-320, -320, -320, -320, -320, -320, -320, -320, -320},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static u8 f_BrickWallSlide = 0;
static Vtx f_TurnedArrowVertices[TURNED_ARROW_VERTEX_COUNT] = {
    {{{-16, 16, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-16, -16, 0}, 0, {0, 2048}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{16, -16, 0}, 0, {2048, 2048}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{16, 16, 0}, 0, {2048, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
};
static s32 f_UnusedWordsAfterArrowVertices[2] UNUSED = {0, 0};
static f32 f_UnusedCursorPhases[4] UNUSED = {0.0f, 2.0f, 4.0f, 6.0f};
static s32 f_UnusedWordsAfterCursorPhases[2] UNUSED = {0, 0};
static u8 f_HasBestShots = FALSE;
AssetId g_CharacterFaceAssets[CHARACTER_COUNT] = {
    0x07C5,
    0x0755,
    0x07A9,
    0x0747,
    0x078D,
    0x07B7,
    0x07D3,
    0x07E1,
    0x0771,
    0x079B,
    0x07EF,
    0x0763,
    0x077F,
    0x098D,
    0x07FD,
    0x080B,
    0x0819,
    0x0827,
};
SignedAssetId g_CharacterPaletteFiles[CHARACTER_COUNT][CHARACTER_PALETTE_COUNT] = {
    {0x07B8, 0x07B9, 0x07BA, 0x07BB},
    {0x0748, 0x0749, 0x074A, 0x074B},
    {0x079C, 0x079D, 0x079E, 0x079F},
    {0x073A, 0x073B, 0x073C, 0x073D},
    {0x0780, 0x0781, 0x0782, 0x0783},
    {0x07AA, 0x07AB, 0x07AC, 0x07AD},
    {0x07C6, 0x07C7, 0x07C8, 0x07C9},
    {0x07D4, 0x07D5, 0x07D6, 0x07D7},
    {0x0764, 0x0765, 0x0766, 0x0767},
    {0x078E, 0x078F, 0x0790, 0x0791},
    {0x07E2, 0x07E3, 0x07E4, 0x07E5},
    {0x0756, 0x0757, 0x0758, 0x0759},
    {0x0772, 0x0773, 0x0774, 0x0775},
    {0x0980, 0x0981, 0x0982, 0x0983},
    {0x07F0, 0x07F1, 0x07F2, 0x07F3},
    {0x07FE, 0x07FF, 0x0800, 0x0801},
    {0x080C, 0x080D, 0x080E, 0x080F},
    {0x081A, 0x081B, 0x081C, 0x081D},
};
static s16 f_UnusedPaletteFileRow[4] UNUSED = {0x078E, 0x078E, 0x078E, 0x078E};
static s16 f_UnusedCharacterOrder[18] UNUSED =
    {0, 0, 1, 4, 5, 6, 3, 2, 7, 9, 12, 10, 13, 15, 14, 11, 8, 0};
static char f_RecordLabels[STATS_GRID_ROW_COUNT][RECORD_LABEL_SIZE] =
    {"Birdie", "Eagle", "Albatross", "Hole-in-One"};
static GridStep f_BestShotDirectionSteps[STATS_DIRECTION_COUNT] =
    {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
static s32 f_UnusedLabelSizes[8] UNUSED = {64, 12, 48, 12, 80, 12, 96, 12};
static Vtx f_BestShotsCursorVertices[MENU_CURSOR_VERTEX_COUNT] = {
    {{{-48, 48, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-48, 0, 0}, 0, {0, 3072}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{0, 0, 0}, 0, {3072, 3072}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{0, 48, 0}, 0, {3072, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
};
static s32 f_UnusedWordsAfterBestShotsCursor[2] UNUSED = {0, 0};
static f32 f_BestShotsCursorPhases[MENU_CURSOR_VERTEX_COUNT] = {0.0f, 2.0f, 4.0f, 6.0f};
static s32 f_CharacterChoices[CHARACTER_COUNT] = {
    CHARACTER_CHOICE_PLUM,
    CHARACTER_CHOICE_CHARLIE,
    CHARACTER_CHOICE_MAPLE,
    CHARACTER_CHOICE_BABY_MARIO,
    CHARACTER_CHOICE_LUIGI,
    CHARACTER_CHOICE_PEACH,
    CHARACTER_CHOICE_SONNY,
    CHARACTER_CHOICE_WARIO,
    CHARACTER_CHOICE_HARRY,
    CHARACTER_CHOICE_MARIO,
    CHARACTER_CHOICE_YOSHI,
    CHARACTER_CHOICE_DONKEY_KONG,
    CHARACTER_CHOICE_BOWSER,
    CHARACTER_CHOICE_METAL_MARIO,
    CHARACTER_CHOICE_KID,
    CHARACTER_CHOICE_SHERRY,
    CHARACTER_CHOICE_AZALEA,
    CHARACTER_CHOICE_JOE,
};

static Mtx f_HandicapGolferTransforms[PLAYER_COUNT_MAX][DRAW_BUFFER_COUNT];
static Mtx f_HandicapGolfersProjection;
static LookAt f_HandicapGolfersLookAts[DRAW_BUFFER_COUNT];

static u8 f_UnusedBufferAfterGolferLookAts[0x100] UNUSED;

static Mtx f_TurnedArrowProjections[PAGE_ARROW_COUNT];
static Mtx f_TurnedArrowTranslations[PAGE_ARROW_COUNT];
static Mtx f_TurnedArrowRotations[PAGE_ARROW_COUNT];

static u8 f_UnusedBufferAfterArrowRotations[0x100] UNUSED;

static Mtx f_TurnedArrowScales[PAGE_ARROW_COUNT];
static ImageAsset *f_BetIcons;
static void *f_UnusedStatsBuffer;
static ImageAsset *f_StakeMark;
static ImageAsset *f_StatsNamePlate;
static void *f_ChosenBetIconFrames[BET_ICON_FRAME_COUNT];
static ImageAsset *f_StatsGreenArrows;
static ImageAsset *f_PageArrows;
static ImageAsset *f_HandicapPlaque;
static ImageAsset *f_BetLabels;
static ImageAsset *f_StatsPlayerLabels;
static ImageAsset *f_BrickWall;

static u8 f_UnusedBufferAfterBrickWall[0xA0] UNUSED;

static Mtx f_BestShotsCursorProjection;
static Mtx f_BestShotsCursorTranslation;
static Mtx f_BestShotsCursorRotation;

static u8 f_UnusedBufferBeforeBestShotsScale[0x80] UNUSED;

static Mtx f_BestShotsCursorScale;

static u8 f_UnusedBufferAfterBestShotsScale[0x50] UNUSED;

static ImageAsset *f_BestShotFaces[STATS_GRID_ROW_COUNT][STATS_GRID_COLUMN_COUNT];
static ImageAsset **f_BestShotsCourseIcons;
static ImageAsset *f_BestShotsNamePlate;
static ImageAsset *f_BestShotsCursorBox;
static ImageAsset *f_ScoreLabels;
static s16 f_BestShotRecords[STATS_GRID_ROW_COUNT][STATS_GRID_COLUMN_COUNT];

s16 g_ChosenHandicaps[PLAYER_COUNT_MAX];

void LoadPlayerStatsBuffer(void *buffer, PlayerStatsBuffer bufferKind) {
    EncodedAsset asset;
    MenuMemory *memory = GetMenuMemory();
    s16 iconIndex;
    BetSet betSet;
    AssetId assetId;
    void **imageSlot;

    switch (bufferKind) {
        case PLAYER_STATS_BUFFER_BET_ICONS:
            f_BetIcons = buffer;
            for (iconIndex = 0; iconIndex != BET_ICON_COUNT; iconIndex++) {
                OpenEncodedAsset(f_BetIconAssets[iconIndex], &asset);
                ReadEncodedAsset(ImageAt(f_BetIcons, iconIndex * BET_ICON_SLOT_SIZE), &asset);
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
                &asset
            );
            ReadEncodedAsset(*imageSlot, &asset);
            break;
        case PLAYER_STATS_BUFFER_BET_ICON_SECOND_FRAME:
            betSet = g_BetSet;
            imageSlot = &f_ChosenBetIconFrames[1];
            *imageSlot = buffer;
            OpenEncodedAsset(
                f_BetIconFrameAssets[f_BetSetBets[betSet - 1][memory->betColumn]][1],
                &asset
            );
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
            OpenEncodedAsset(ASSET_BRICK_WALL, &asset);
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
    g_AnimatedIconFrame = 0;
    g_AnimatedIconBufferIndex = 0;
    f_IsBetHelpShown = FALSE;
    f_IsHandicapPromptShown = FALSE;
}

void ApplyBetPreset(s32 requestedPreset) {
    s8 presetIndex = requestedPreset;
    MenuMemory *memory = GetMenuMemory();
    s16 index;

    if ((u8)requestedPreset < BET_PRESET_COUNT) {
        if ((s8)requestedPreset == BET_PRESET_STROKE && g_ChosenPlayerCount == PLAYER_COUNT_MAX) {
            presetIndex = BET_PRESET_STROKE_FOUR_PLAYER;
        }

        if (presetIndex == BET_PRESET_MINI_GOLF && g_ChosenPlayerCount == PLAYER_COUNT_MAX) {
            presetIndex = BET_PRESET_MINI_GOLF_FOUR_PLAYER;
        }

        for (index = 0; index != BET_STAKE_SLOT_COUNT; index++) {
            memory->betStakeLevels[index] = f_BetPresetStakeLevels[presetIndex][index];
            g_GameState.betRates[index] = f_BetStakeAmounts[memory->betStakeLevels[index]];
        }
    }
    else {
        for (index = 0; index != BET_STAKE_SLOT_COUNT; index++) {
            memory->betStakeLevels[index] = NO_BET;
            g_GameState.betRates[index] = UNSET_BET_RATE;
        }
    }

    memory->betColumn = 0;
    for (index = 0; index != PLAYER_COUNT_MAX; index++) {
        g_ChosenHandicaps[index] = index < g_ChosenPlayerCount ? 0 : ABSENT_PLAYER_HANDICAP;
    }

    memory->handicapPlayer = 0;
    memory->unusedHandicapValue = 0;
}

static inline AssetId ChosenBetIconFrame(MenuMemory *memory, s32 frame) {
    return f_BetIconFrameAssets[f_BetSetBets[g_BetSet - 1][memory->betColumn]][frame];
}

void UpdateBetSettings(void) {
    EncodedAsset asset;
    MenuMemory *memory;
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
        SetTextSlot(
            0,
            0,
            NO_MESSAGE,
            TEXT_SLOT_LARGE_FONT,
            0,
            TEXT_FLAG_TYPED,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
        for (index = 0; index != BET_STAKE_SLOT_COUNT; index++) {
            if (memory->betStakeLevels[index] != NO_BET) {
                g_GameState.betRates[index] = f_BetStakeAmounts[memory->betStakeLevels[index]];
            }
            else {
                g_GameState.betRates[index] = UNSET_BET_RATE;
            }
        }

        return;
    }

    if (g_MenuPressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        g_BetPagesPhase = BET_PHASE_CANCELED;
        f_IsBetHelpShown = FALSE;
        SetTextSlot(
            0,
            0,
            NO_MESSAGE,
            TEXT_SLOT_LARGE_FONT,
            0,
            TEXT_FLAG_TYPED,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
        for (index = 0; index != BET_STAKE_SLOT_COUNT; index++) {
            if (memory->betStakeLevels[index] != NO_BET) {
                g_GameState.betRates[index] = f_BetStakeAmounts[memory->betStakeLevels[index]];
            }
            else {
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
    }
    else if (g_MenuRepeatButtons & D_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        index = f_BetSetStakeSlots[g_BetSet - 1][memory->betColumn];
        if (memory->betStakeLevels[index] > 0) {
            memory->betStakeLevels[index]--;
        }
    }
    else if (g_MenuRepeatButtons & L_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        memory->betColumn--;
        if (memory->betColumn < 0) {
            memory->betColumn = f_BetSetColumnCounts[g_BetSet - 1] - 1;
        }

        SetTextSlot(
            BET_HELP_X,
            BET_HELP_Y,
            f_BetSetBets[g_BetSet - 1][memory->betColumn] + MESSAGE_BET_HELP,
            TEXT_SLOT_LARGE_FONT,
            0,
            TEXT_FLAG_TYPED,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
        OpenEncodedAsset(ChosenBetIconFrame(memory, g_AnimatedIconFrame), &asset);
        ReadEncodedAsset(f_ChosenBetIconFrames[g_AnimatedIconBufferIndex ^ 1], &asset);
        g_AnimatedIconBufferIndex ^= 1;
    }
    else if (g_MenuRepeatButtons & R_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        memory->betColumn++;
        if (memory->betColumn > f_BetSetColumnCounts[g_BetSet - 1] - 1) {
            memory->betColumn = 0;
        }

        SetTextSlot(
            BET_HELP_X,
            BET_HELP_Y,
            f_BetSetBets[g_BetSet - 1][memory->betColumn] + MESSAGE_BET_HELP,
            TEXT_SLOT_LARGE_FONT,
            0,
            TEXT_FLAG_TYPED,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
        OpenEncodedAsset(ChosenBetIconFrame(memory, g_AnimatedIconFrame), &asset);
        ReadEncodedAsset(f_ChosenBetIconFrames[g_AnimatedIconBufferIndex ^ 1], &asset);
        g_AnimatedIconBufferIndex ^= 1;
    }
}

#define BET_ICON_TOP 120
#define CHOSEN_BET_ICON_TOP 124
#define BET_AMOUNT_OFFSET_X (-4)
#define BET_AMOUNT_Y 106
#define STAKE_MARK_SIZE 8
#define STAKE_MARK_OFFSET_X 8
#define STAKE_MARK_BOTTOM 102
#define STAKE_MARK_SPACING 8
#define LARGE_STAKE_LEVEL 3
#define LARGE_STAKE_GROWTH 4
#define LARGEST_STAKE_LEVEL 4
#define LARGEST_STAKE_GROWTH 6
#define LARGEST_STAKE_RAISE 3
#define STAKE_MARK_COUNT 5
#define WIDE_BANNER_LEFT 26
#define WIDE_BANNER_RIGHT 296
#define WIDE_BANNER_TOP 180
#define WIDE_BANNER_RAISE 6
#define WIDE_BANNER_STRIP_HEIGHT 20
#define WIDE_BANNER_STRIP_SPACING 16
#define BET_BANNER_LEFT 80
#define BET_BANNER_RIGHT 240
#define BET_BANNER_TOP 152
#define BET_LABEL_CENTER_X (SCREEN_WIDTH / 2)
#define BET_LABEL_TOP 156

void DrawBetSettings(
    Gfx **displayListHead,
    s32 slide,
    UBoolean8 isSlideReversed,
    UBoolean8 isSettled
) {
    Gfx *gfx = *displayListHead;
    MenuMemory *memory = GetMenuMemory();
    s16 i;
    s16 stakeLevel;
    s16 betIndex;
    s16 markGrowth;
    s16 markRaise;
    s16 bannerShift;
    u8 *icon;
    s32 stripTop;
    s32 wideStripTop;
    s8 stakeAmount;

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
            gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(icon)));
            if (memory->betStakeLevels[f_BetSetStakeSlots[g_BetSet - 1][i]] == 0) {
                gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
                gDPSetPrimColor(gfx++, 0, 0, 127, 127, 127, OPAQUE_ALPHA);
            }
            else {
                gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            }

            gDPLoadTextureTile_4b(
                gfx++,
                TextureAddress(ImageTexels(icon)),
                G_IM_FMT_CI,
                BET_ICON_SIZE,
                BET_ICON_SIZE,
                0,
                0,
                BET_ICON_SIZE - 1,
                BET_ICON_SIZE - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(f_BetSetColumnXs[g_BetSet - 1][i] + slide),
                QuarterPixels(BET_ICON_TOP),
                QuarterPixels(f_BetSetColumnXs[g_BetSet - 1][i] + slide + BET_ICON_SIZE),
                QuarterPixels(BET_ICON_TOP + BET_ICON_SIZE),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }

        stakeAmount =
            f_BetStakeAmounts[memory->betStakeLevels[f_BetSetStakeSlots[g_BetSet - 1][i]]];
        DrawUnalignedMenuNumber(
            &gfx,
            f_BetSetColumnXs[g_BetSet - 1][i] + slide + BET_AMOUNT_OFFSET_X,
            BET_AMOUNT_Y,
            stakeAmount,
            0
        );
    }

    DrawBetIcon(
        &gfx,
        f_BetSetColumnXs[g_BetSet - 1][memory->betColumn] + slide,
        CHOSEN_BET_ICON_TOP,
        BET_ICON_SIZE,
        TEXEL_STEP_ONE
    );

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(ImageTexels(f_StakeMark)),
        G_IM_FMT_CI,
        STAKE_MARK_SIZE,
        STAKE_MARK_SIZE,
        0,
        0,
        STAKE_MARK_SIZE - 1,
        STAKE_MARK_SIZE - 1,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_StakeMark)));
    gDPPipeSync(gfx++);

    markRaise = 0;
    markGrowth = 0;
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
            if (stakeLevel >= memory->betStakeLevels[f_BetSetStakeSlots[g_BetSet - 1][i]]) {
                gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
                gDPSetPrimColor(gfx++, 0, 0, 127, 127, 127, OPAQUE_ALPHA);
            }
            else {
                gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            }

            gDPPipeSync(gfx++);

            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(
                    f_BetSetColumnXs[g_BetSet - 1][i] + STAKE_MARK_OFFSET_X + slide -
                    (markGrowth / 2)
                ),
                QuarterPixels(
                    (STAKE_MARK_BOTTOM - STAKE_MARK_SIZE) - (stakeLevel * STAKE_MARK_SPACING) -
                    markGrowth - markRaise
                ),
                QuarterPixels(
                    f_BetSetColumnXs[g_BetSet - 1][i] + STAKE_MARK_OFFSET_X + slide +
                    STAKE_MARK_SIZE + (markGrowth / 2)
                ),
                QuarterPixels(STAKE_MARK_BOTTOM - (stakeLevel * STAKE_MARK_SPACING) - markRaise),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep(STAKE_MARK_SIZE, markGrowth + STAKE_MARK_SIZE),
                TexelRunStep(STAKE_MARK_SIZE, markGrowth + STAKE_MARK_SIZE)
            );
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
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_StatsNamePlate)));

    bannerShift = slide;
    if (isSlideReversed) {
        bannerShift = -slide;
    }

    for (i = 0; i != NAME_PLATE_STRIP_COUNT; i++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_StatsNamePlate) +
                (i * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NAME_PLATE_WIDTH,
            NAME_PLATE_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        wideStripTop = (i * WIDE_BANNER_STRIP_SPACING) + WIDE_BANNER_TOP;
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(WIDE_BANNER_LEFT),
            QuarterPixels(bannerShift + wideStripTop - WIDE_BANNER_RAISE),
            QuarterPixels(WIDE_BANNER_RIGHT),
            QuarterPixels(
                bannerShift + wideStripTop - WIDE_BANNER_RAISE + WIDE_BANNER_STRIP_HEIGHT
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(NAME_PLATE_WIDTH, WIDE_BANNER_RIGHT - WIDE_BANNER_LEFT),
            TexelRunStep(NAME_PLATE_STRIP_HEIGHT, WIDE_BANNER_STRIP_HEIGHT)
        );
        gDPPipeSync(gfx++);
    }

    for (i = 0; i != NAME_PLATE_STRIP_COUNT; i++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_StatsNamePlate) +
                (i * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NAME_PLATE_WIDTH,
            NAME_PLATE_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        stripTop = (i * NAME_PLATE_STRIP_HEIGHT) + BET_BANNER_TOP;
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(BET_BANNER_LEFT),
            QuarterPixels(bannerShift + stripTop),
            QuarterPixels(BET_BANNER_RIGHT),
            QuarterPixels(bannerShift + stripTop + NAME_PLATE_STRIP_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(NAME_PLATE_WIDTH, BET_BANNER_RIGHT - BET_BANNER_LEFT),
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    betIndex = f_BetSetBets[g_BetSet - 1][memory->betColumn];
    DrawBetLabel(&gfx, BET_LABEL_CENTER_X, bannerShift + BET_LABEL_TOP, betIndex);
    if (!f_IsBetHelpShown && isSettled == TRUE) {
        SetTextSlot(
            BET_HELP_X,
            BET_HELP_Y,
            betIndex + MESSAGE_BET_HELP,
            TEXT_SLOT_LARGE_FONT,
            0,
            TEXT_FLAG_TYPED,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
        f_IsBetHelpShown = TRUE;
    }

    *displayListHead = gfx;
}

void DrawBetIcon(Gfx **displayListHead, s32 left, s32 top, s32 size, s32 textureStep) {
    Gfx *gfx = *displayListHead;
    MenuMemory *memory = GetMenuMemory();
    EncodedAsset asset;
    ImageAsset *image;
    u8 iconIndex;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    image = f_ChosenBetIconFrames[g_AnimatedIconBufferIndex];
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(image)));
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(ImageTexels(image)),
        G_IM_FMT_CI,
        BET_ICON_SIZE,
        BET_ICON_SIZE,
        0,
        0,
        BET_ICON_SIZE - 1,
        BET_ICON_SIZE - 1,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(left),
        QuarterPixels(top),
        QuarterPixels(left + size),
        QuarterPixels(top + size),
        G_TX_RENDERTILE,
        0,
        0,
        textureStep,
        textureStep
    );
    g_AnimatedIconFrameTime++;
    gDPPipeSync(gfx++);
    if (g_AnimatedIconFrameTime == BET_ICON_FRAME_DURATION) {
        g_AnimatedIconFrameTime = 0;
        g_AnimatedIconFrame++;
        if (g_AnimatedIconFrame >= BET_ICON_FRAME_COUNT) {
            g_AnimatedIconFrame = 0;
        }

        g_AnimatedIconBufferIndex = !g_AnimatedIconBufferIndex;
        iconIndex = f_BetSetBets[g_BetSet - 1][memory->betColumn];
        OpenEncodedAsset(f_BetIconFrameAssets[iconIndex][g_AnimatedIconFrame], &asset);
        ReadEncodedAsset(f_ChosenBetIconFrames[g_AnimatedIconBufferIndex], &asset);
    }

    *displayListHead = gfx;
}

NO_RESULT UpdateHandicaps(s32 firstPlayerIndex UNUSED, s32 playerCount UNUSED) {
    MenuMemory *memory = GetMenuMemory();

    if (g_BetPagesPhase == MENU_PHASE_START) {
        PlaySong(SONG_TITLE, 0);
        g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
        g_BetPagesWallSlide = STATS_SLIDE_IN_FROM_LEFT;
        FreeAllCharacterSlots();
    }

    if (g_BetPagesPhase == MENU_PHASE_CHOOSING) {
        if (g_MenuPressedButtons & A_BUTTON) {
            PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                0,
                TEXT_FLAG_TYPED,
                0xFF,
                0xFF,
                0xFF,
                OPAQUE_ALPHA
            );
            g_CourseBannersSlide = HANDICAP_SLIDE_START;
            f_IsHandicapPromptShown = FALSE;
            g_BetPagesPhase = MENU_PHASE_CONFIRMED;
            return;
        }

        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            g_MenuScreen = MENU_SCREEN_BET_SETTINGS;
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                0,
                TEXT_FLAG_TYPED,
                0xFF,
                0xFF,
                0xFF,
                OPAQUE_ALPHA
            );
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

#define HANDICAP_BANNER_WIDTH 200
#define HANDICAP_BANNER_STRIP_HEIGHT 10
#define HANDICAP_BANNER_STRIP_COUNT 3
#define HANDICAP_BANNER_STRIP_SIZE (HANDICAP_BANNER_WIDTH * HANDICAP_BANNER_STRIP_HEIGHT)
#define HANDICAP_BANNER_LEFT 26
#define HANDICAP_BANNER_RIGHT 296
#define HANDICAP_BANNER_TOP 190
#define HANDICAP_BANNER_STEP_X \
    TexelRunStep(HANDICAP_BANNER_WIDTH, HANDICAP_BANNER_RIGHT - HANDICAP_BANNER_LEFT)

#define HANDICAP_COLUMN_WIDTH 64
#define HANDICAP_COLUMN_STRIP_HEIGHT 22
#define HANDICAP_COLUMN_STRIP_COUNT 2
#define HANDICAP_COLUMN_STRIP_SIZE (HANDICAP_COLUMN_WIDTH * HANDICAP_COLUMN_STRIP_HEIGHT / 2)
#define HANDICAP_COLUMN_TOP 150
#define UNCHOSEN_COLUMN_SHADE 200
#define HANDICAP_LABEL_OFFSET_X 4
#define HANDICAP_LABEL_TOP 154
#define HANDICAP_LABEL_TMEM 0x80
#define HANDICAP_LABEL_TILE 1
#define HANDICAP_LABEL_PALETTE 1
#define HANDICAP_NUMBER_OFFSET_X 16
#define HANDICAP_NUMBER_Y 166

#define HANDICAP_ARROW_MASK 4
#define HANDICAP_ARROW_PUSH 6
#define HANDICAP_ARROW_MIDDLE_X 24
#define HANDICAP_ARROW_MIDDLE_Y 15
#define HANDICAP_ARROW_UP_Y 0
#define HANDICAP_ARROW_DOWN_Y 30
#define HANDICAP_ARROW_RIGHT_X 48
#define HANDICAP_ARROW_LEFT_X (-1)

#define HANDICAP_PROMPT_X 36
#define HANDICAP_PROMPT_Y 200
#define MESSAGE_HANDICAP_PROMPT 0x1D

void DrawHandicaps(
    Gfx **displayListHead,
    s32 slide,
    UBoolean8 isVerticalSlideReversed,
    UBoolean8 canShowPrompt
) {
    Gfx *gfx = *displayListHead;
    s32 labelSlide;
    MenuMemory *memory = GetMenuMemory();
    u16 playerIndex;
    s32 column;
    s32 i;
    u16 textureLeft;
    s32 labelIndex;
    s32 offsetY;
    s32 arrowX;
    s32 arrowY;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    u8 unusedStack[0x30] UNUSED;
    Mat4 transform;
    s32 numberOffsetX;
    s32 columnLeft;
    s32 arrowTop;

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
    guOrtho(
        &f_HandicapGolfersProjection,
        -(SCREEN_WIDTH / 2),
        SCREEN_WIDTH / 2,
        -(SCREEN_HEIGHT / 2),
        SCREEN_HEIGHT / 2,
        MENU_CHARACTERS_NEAR,
        MENU_CHARACTERS_FAR,
        MENU_CHARACTERS_ORTHO_SCALE
    );
    {
        Mat4 viewing;

        guLookAtReflectF(
            viewing,
            &f_HandicapGolfersLookAts[g_DrawBufferIndex],
            0.0f,
            MENU_CHARACTERS_EYE_Y,
            MENU_CHARACTERS_EYE_Z,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            0.0f
        );
    }

    gSPLookAt(gfx++, &f_HandicapGolfersLookAts[g_DrawBufferIndex]);
    playerIndex = 0;
    arrowX = 0;
    arrowY = 0;
    for (; playerIndex != g_ChosenPlayerCount; playerIndex++) {
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
            }
            else {
                LoadCharacter(
                    playerIndex,
                    g_Players[playerIndex].characterIndex + CHARACTER_COUNT,
                    g_Players[playerIndex].paletteIndex
                );
                g_IsMenuCharacterLoaded[playerIndex] = TRUE;
                SetCharacterAnimation(playerIndex, MENU_ANIMATION_STANDING);
                SetAnimationSpeed(playerIndex, 1.0f);
            }
        }

        rotation[0] = DegreesToRadians(f_HandicapGolferAngles[0]);
        rotation[1] = DegreesToRadians(f_HandicapGolferAngles[1]);
        rotation[2] = 0.0f;
        translation[0] = f_HandicapGolferPositions[g_ChosenPlayerCount - 1][playerIndex][0] + slide;
        translation[1] = f_HandicapGolferPositions[g_ChosenPlayerCount - 1][playerIndex][1];
        translation[2] = f_HandicapGolferPositions[g_ChosenPlayerCount - 1][playerIndex][2];
        scale[0] = f_HandicapGolferScales[playerIndex];
        scale[1] = f_HandicapGolferScales[playerIndex];
        scale[2] = f_HandicapGolferScales[playerIndex];

        BuildTransformMatrix(transform, rotation, translation, scale);
        ConvertMatrixToFixed(
            transform,
            &f_HandicapGolferTransforms[playerIndex][g_DrawBufferIndex]
        );

        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_PERSP);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_HandicapGolfersProjection),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_HandicapGolferTransforms[playerIndex][g_DrawBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );

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
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_StatsNamePlate)));
    offsetY = slide;
    if (isVerticalSlideReversed) {
        offsetY = -slide;
    }

    for (i = 0; i != HANDICAP_BANNER_STRIP_COUNT; i++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(LargeImageTexels(f_StatsNamePlate) + (i * HANDICAP_BANNER_STRIP_SIZE)),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            HANDICAP_BANNER_WIDTH,
            HANDICAP_BANNER_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(HANDICAP_BANNER_LEFT),
            QuarterPixels(offsetY + HANDICAP_BANNER_TOP + (i * HANDICAP_BANNER_STRIP_HEIGHT)),
            QuarterPixels(HANDICAP_BANNER_RIGHT),
            QuarterPixels(
                offsetY + HANDICAP_BANNER_TOP + (i * HANDICAP_BANNER_STRIP_HEIGHT) +
                HANDICAP_BANNER_STRIP_HEIGHT
            ),
            G_TX_RENDERTILE,
            0,
            0,
            HANDICAP_BANNER_STEP_X,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_HandicapPlaque)));
    gDPLoadTLUT_pal16(
        gfx++,
        HANDICAP_LABEL_PALETTE,
        TextureAddress(ImagePalette(f_StatsPlayerLabels))
    );
    for (column = 0; column != g_ChosenPlayerCount; column++) {
        if (column == memory->handicapPlayer) {
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        }
        else {
            gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
            gDPSetPrimColor(
                gfx++,
                0,
                0,
                UNCHOSEN_COLUMN_SHADE,
                UNCHOSEN_COLUMN_SHADE,
                UNCHOSEN_COLUMN_SHADE,
                OPAQUE_ALPHA
            );
        }

        for (i = 0; i != HANDICAP_COLUMN_STRIP_COUNT; i++) {
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(ImageTexels(f_HandicapPlaque) + (i * HANDICAP_COLUMN_STRIP_SIZE)),
                G_IM_FMT_CI,
                HANDICAP_COLUMN_WIDTH,
                HANDICAP_COLUMN_STRIP_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(f_HandicapColumnLefts[g_ChosenPlayerCount - 1][column] + slide),
                QuarterPixels(HANDICAP_COLUMN_TOP + (i * HANDICAP_COLUMN_STRIP_HEIGHT)),
                QuarterPixels(
                    f_HandicapColumnLefts[g_ChosenPlayerCount - 1][column] + slide +
                    HANDICAP_COLUMN_WIDTH
                ),
                QuarterPixels(
                    HANDICAP_COLUMN_TOP + (i * HANDICAP_COLUMN_STRIP_HEIGHT) +
                    HANDICAP_COLUMN_STRIP_HEIGHT
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }

        IN_OWN_BLOCK(IN_OWN_BLOCK({
            labelIndex = column;
            if (g_PlayerControllerIndices[column] == NO_CONTROLLER_INDEX) {
                labelIndex = column + PLAYER_LABEL_FIRST_COMPUTER;
            }
        }));

        gDPLoadMultiBlock_4b(
            gfx++,
            TextureAddress(
                ImageTexelsAt(f_StatsPlayerLabels, labelIndex * PLAYER_LABEL_IMAGE_SIZE)
            ),
            HANDICAP_LABEL_TMEM,
            HANDICAP_LABEL_TILE,
            G_IM_FMT_CI,
            PLAYER_LABEL_WIDTH,
            PLAYER_LABEL_HEIGHT,
            HANDICAP_LABEL_PALETTE,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        labelSlide = slide + HANDICAP_LABEL_OFFSET_X;
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(f_HandicapColumnLefts[g_ChosenPlayerCount - 1][column] + labelSlide),
            QuarterPixels(HANDICAP_LABEL_TOP),
            QuarterPixels(
                f_HandicapColumnLefts[g_ChosenPlayerCount - 1][column] + labelSlide +
                PLAYER_LABEL_WIDTH
            ),
            QuarterPixels(HANDICAP_LABEL_TOP + PLAYER_LABEL_HEIGHT),
            HANDICAP_LABEL_TILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    f_GreenArrowTimer++;
    if (f_GreenArrowTimer == STATS_ARROW_BLINK_FRAME_COUNT) {
        f_GreenArrowTimer = 0;
        f_GreenArrowFrame ^= 1;
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_StatsGreenArrows)));
    arrowTop = HANDICAP_COLUMN_TOP;
    for (i = 0; i != MENU_ARROW_COUNT; i++) {
        if (i == MENU_ARROW_LEFT) {
            gDPLoadTextureTile_4b(
                gfx++,
                TextureAddress(ImageTexelsAt(
                    f_StatsGreenArrows,
                    (MENU_ARROW_RIGHT * GREEN_ARROW_IMAGE_SIZE) +
                        (f_GreenArrowFrame * GREEN_ARROW_FRAME_SIZE)
                )),
                G_IM_FMT_CI,
                GREEN_ARROW_SIZE,
                GREEN_ARROW_SIZE,
                0,
                0,
                GREEN_ARROW_SIZE - 1,
                GREEN_ARROW_SIZE - 1,
                0,
                G_TX_MIRROR | G_TX_WRAP,
                G_TX_MIRROR | G_TX_WRAP,
                HANDICAP_ARROW_MASK,
                HANDICAP_ARROW_MASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }
        else {
            gDPLoadTextureTile_4b(
                gfx++,
                TextureAddress(
                    (u32)f_StatsGreenArrows +
                    ((i * GREEN_ARROW_IMAGE_SIZE) + PALETTED_TEXELS_OFFSET +
                        (f_GreenArrowFrame * GREEN_ARROW_FRAME_SIZE))
                ),
                G_IM_FMT_CI,
                GREEN_ARROW_SIZE,
                GREEN_ARROW_SIZE,
                0,
                0,
                GREEN_ARROW_SIZE - 1,
                GREEN_ARROW_SIZE - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }

        gDPPipeSync(gfx++);
        if (i == MENU_ARROW_UP) {
            arrowX = HANDICAP_ARROW_MIDDLE_X;
            arrowY = (g_MenuRepeatButtons & U_JPAD) ? (HANDICAP_ARROW_UP_Y - HANDICAP_ARROW_PUSH) :
                                                      HANDICAP_ARROW_UP_Y;
        }
        else if (i == MENU_ARROW_DOWN) {
            arrowX = HANDICAP_ARROW_MIDDLE_X;
            arrowY = (g_MenuRepeatButtons & D_JPAD) ?
                (HANDICAP_ARROW_DOWN_Y + HANDICAP_ARROW_PUSH) :
                HANDICAP_ARROW_DOWN_Y;
        }
        else if (i == MENU_ARROW_RIGHT) {
            arrowY = HANDICAP_ARROW_MIDDLE_Y;
            arrowX = (g_MenuRepeatButtons & R_JPAD) ?
                (HANDICAP_ARROW_RIGHT_X + HANDICAP_ARROW_PUSH) :
                HANDICAP_ARROW_RIGHT_X;
        }
        else if (i == MENU_ARROW_LEFT) {
            arrowY = HANDICAP_ARROW_MIDDLE_Y;
            arrowX = (g_MenuRepeatButtons & L_JPAD) ?
                (HANDICAP_ARROW_LEFT_X - HANDICAP_ARROW_PUSH) :
                HANDICAP_ARROW_LEFT_X;
        }

        textureLeft = (i == MENU_ARROW_LEFT) ? TexelCoordinate(GREEN_ARROW_SIZE) : 0;
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(
                f_HandicapColumnLefts[g_ChosenPlayerCount - 1][memory->handicapPlayer] + arrowX +
                slide
            ),
            QuarterPixels(arrowY + (offsetY + arrowTop)),
            QuarterPixels(
                f_HandicapColumnLefts[g_ChosenPlayerCount - 1][memory->handicapPlayer] + arrowX +
                slide + GREEN_ARROW_SIZE
            ),
            QuarterPixels(arrowY + (offsetY + arrowTop) + GREEN_ARROW_SIZE),
            G_TX_RENDERTILE,
            textureLeft,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    for (i = 0; i != g_ChosenPlayerCount; i++) {
        columnLeft = f_HandicapColumnLefts[g_ChosenPlayerCount - 1][i];
        numberOffsetX = slide + HANDICAP_NUMBER_OFFSET_X;
        DrawTwoDigitMenuNumber(
            &gfx,
            columnLeft + numberOffsetX,
            HANDICAP_NUMBER_Y,
            g_ChosenHandicaps[i],
            MENU_NUMBER_PLAIN
        );
    }

    if ((f_IsHandicapPromptShown == FALSE) & (canShowPrompt == TRUE)) {
        SetTextSlotValue(memory->handicapPlayer + 1, 0, 0);

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

        SetTextSlot(
            HANDICAP_PROMPT_X,
            HANDICAP_PROMPT_Y,
            MESSAGE_HANDICAP_PROMPT,
            TEXT_SLOT_LARGE_FONT,
            0,
            TEXT_FLAG_TYPED,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
        f_IsHandicapPromptShown = TRUE;
    }

    *displayListHead = gfx;
}

void DrawBetLabel(Gfx **displayListHead, s32 centerX, s32 top, u8 labelIndex) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_BetLabels)));
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(f_BetLabels) + (labelIndex * BET_LABEL_IMAGE_SIZE)),
        G_IM_FMT_CI,
        BET_LABEL_WIDTH,
        BET_LABEL_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(centerX - (BET_LABEL_WIDTH / 2)),
        QuarterPixels(top),
        QuarterPixels(centerX + (BET_LABEL_WIDTH / 2)),
        QuarterPixels(top + BET_LABEL_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

s32 SlideBrickWall(Gfx **displayListHead, u8 slide) {
    Gfx *gfx = *displayListHead;
    s32 index;
    s32 row;
    s32 offset;
    s32 firstRowOffset;

    if ((slide != STATS_SLIDE_CURRENT && slide < STATS_SLIDE_COUNT) == FALSE) {
        return -1;
    }

    offset = f_BrickSlideStartOffsets[slide - 1][0];
    firstRowOffset = f_BrickRowOffsets[0];
    for (index = 0; index != STATS_SLIDE_ROW_COUNT; index++) {
        if (f_BrickRowOffsets[index] != offset && firstRowOffset != f_BrickRowOffsets[index]) {
            break;
        }
    }

    if (index == STATS_SLIDE_ROW_COUNT && f_BrickWallSlide != slide) {
        f_BrickWallSlide = slide;
        switch (slide) {
            case STATS_SLIDE_OUT_RIGHT:
                for (row = 0; row != STATS_SLIDE_ROW_COUNT; row++) {
                    f_BrickRowOffsets[row] =
                        f_BrickSlideStartOffsets[STATS_SLIDE_OUT_RIGHT - 1][row];
                    f_BrickRowDelays[row] = f_BrickRowStartDelays[row];
                }

                break;
            case STATS_SLIDE_OUT_LEFT:
                for (row = 0; row != STATS_SLIDE_ROW_COUNT; row++) {
                    f_BrickRowOffsets[row] =
                        f_BrickSlideStartOffsets[STATS_SLIDE_OUT_LEFT - 1][row];
                    f_BrickRowDelays[row] = f_BrickRowStartDelays[row];
                }

                break;
            case STATS_SLIDE_IN_FROM_RIGHT:
            case STATS_SLIDE_IN_FROM_LEFT:
                for (row = 0; row != STATS_SLIDE_ROW_COUNT; row++) {
                    f_BrickRowOffsets[row] = f_BrickSlideStartOffsets[f_BrickWallSlide - 1][row];
                    f_BrickRowDelays[row] = f_BrickRowStartDelays[row];
                }

                break;
        }
    }

    DrawBrickWallRows(&gfx, f_BrickWallSlide);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
    for (index = 0; index != STATS_SLIDE_ROW_COUNT; index++) {
        offset = f_BrickRowOffsets[0];
        if (offset != f_BrickRowOffsets[index]) {
            break;
        }
    }

    return index != STATS_SLIDE_ROW_COUNT;
}

void DrawBrickWallRows(Gfx **displayListHead, u8 slide) {
    Gfx *gfx = *displayListHead;
    s32 row;
    s32 stripHeight = STATS_WALL_STRIP_HEIGHT;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_BrickWall)));
    for (row = 0; row != STATS_SLIDE_ROW_COUNT; row++) {
        gDPPipeSync(gfx++);
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_BrickWall) + (stripHeight * STATS_WALL_IMAGE_WIDTH * row)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            STATS_WALL_IMAGE_WIDTH,
            stripHeight,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        if (f_BrickRowDelays[row] > 0) {
            f_BrickRowDelays[row]--;
        }
        else {
            switch (slide) {
                case STATS_SLIDE_OUT_RIGHT:
                    f_BrickRowOffsets[row] -= f_BrickRowSpeeds[row];
                    if (f_BrickRowOffsets[row] <= 0) {
                        f_BrickRowOffsets[row] = 0;
                    }

                    break;
                case STATS_SLIDE_OUT_LEFT:
                    f_BrickRowOffsets[row] += f_BrickRowSpeeds[row];
                    if (f_BrickRowOffsets[row] > 0) {
                        f_BrickRowOffsets[row] = 0;
                    }

                    break;
                case STATS_SLIDE_IN_FROM_RIGHT:
                    f_BrickRowOffsets[row] += f_BrickRowSpeeds[row];
                    if (f_BrickRowOffsets[row] > SCREEN_WIDTH) {
                        f_BrickRowOffsets[row] = SCREEN_WIDTH;
                    }

                    break;
                case STATS_SLIDE_IN_FROM_LEFT:
                    f_BrickRowOffsets[row] -= f_BrickRowSpeeds[row];
                    if (f_BrickRowOffsets[row] < -SCREEN_WIDTH) {
                        f_BrickRowOffsets[row] = -SCREEN_WIDTH;
                    }

                    break;
            }
        }

        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(f_BrickRowOffsets[row]),
            QuarterPixels(row * STATS_WALL_ROW_HEIGHT),
            QuarterPixels(f_BrickRowOffsets[row] + SCREEN_WIDTH),
            QuarterPixels((row * STATS_WALL_ROW_HEIGHT) + STATS_WALL_ROW_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(STATS_WALL_IMAGE_WIDTH, SCREEN_WIDTH),
            TexelRunStep(stripHeight, STATS_WALL_ROW_HEIGHT)
        );
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawPageArrows(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
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
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_PageArrows)));
    gDPPipeSync(gfx++);
    for (arrowIndex = 0; arrowIndex != PAGE_ARROW_COUNT; arrowIndex++) {
        if (arrowIndex == PAGE_ARROW_RIGHT) {
            gDPLoadTextureTile_4b(
                gfx++,
                TextureAddress(
                    ImageTexels(f_PageArrows) + f_PageArrowFrame * PAGE_ARROW_IMAGE_SIZE
                ),
                G_IM_FMT_CI,
                PAGE_ARROW_SIZE,
                PAGE_ARROW_SIZE,
                0,
                0,
                PAGE_ARROW_SIZE - 1,
                PAGE_ARROW_SIZE - 1,
                0,
                G_TX_MIRROR | G_TX_WRAP,
                G_TX_MIRROR | G_TX_WRAP,
                PAGE_ARROW_MASK,
                PAGE_ARROW_MASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            left = PAGE_ARROW_RIGHT_X;
        }
        else {
            gDPLoadTextureTile_4b(
                gfx++,
                TextureAddress(
                    ImageTexels(f_PageArrows) + f_PageArrowFrame * PAGE_ARROW_IMAGE_SIZE
                ),
                G_IM_FMT_CI,
                PAGE_ARROW_SIZE,
                PAGE_ARROW_SIZE,
                0,
                0,
                PAGE_ARROW_SIZE - 1,
                PAGE_ARROW_SIZE - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            left = PAGE_ARROW_LEFT_X;
        }

        textureColumn = MirroredArrowColumn(arrowIndex);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(left),
            QuarterPixels(PAGE_ARROW_TOP),
            QuarterPixels(left + PAGE_ARROW_SIZE),
            QuarterPixels(PAGE_ARROW_TOP + PAGE_ARROW_SIZE),
            G_TX_RENDERTILE,
            TexelCoordinate(textureColumn),
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawCenteredPageArrows(Gfx **displayListHead, s32 gap, s32 centerY) {
    Gfx *gfx = *displayListHead;
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
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_PageArrows)));
    gDPPipeSync(gfx++);
    for (arrowIndex = 0; arrowIndex != PAGE_ARROW_COUNT; arrowIndex++) {
        if (arrowIndex == PAGE_ARROW_RIGHT) {
            gDPLoadTextureTile_4b(
                gfx++,
                TextureAddress(
                    ImageTexels(f_PageArrows) + f_PageArrowFrame * PAGE_ARROW_IMAGE_SIZE
                ),
                G_IM_FMT_CI,
                PAGE_ARROW_SIZE,
                PAGE_ARROW_SIZE,
                0,
                0,
                PAGE_ARROW_SIZE - 1,
                PAGE_ARROW_SIZE - 1,
                0,
                G_TX_MIRROR | G_TX_WRAP,
                G_TX_MIRROR | G_TX_WRAP,
                PAGE_ARROW_MASK,
                PAGE_ARROW_MASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            left = (SCREEN_WIDTH / 2) + (gap / 2);
        }
        else {
            gDPLoadTextureTile_4b(
                gfx++,
                TextureAddress(
                    ImageTexels(f_PageArrows) + f_PageArrowFrame * PAGE_ARROW_IMAGE_SIZE
                ),
                G_IM_FMT_CI,
                PAGE_ARROW_SIZE,
                PAGE_ARROW_SIZE,
                0,
                0,
                PAGE_ARROW_SIZE - 1,
                PAGE_ARROW_SIZE - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            left = (SCREEN_WIDTH / 2) - PAGE_ARROW_SIZE - (gap / 2);
        }

        gDPPipeSync(gfx++);
        textureColumn = MirroredArrowColumn(arrowIndex);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(left),
            QuarterPixels(centerY - (PAGE_ARROW_SIZE / 2)),
            QuarterPixels(left + PAGE_ARROW_SIZE),
            QuarterPixels(centerY + (PAGE_ARROW_SIZE / 2)),
            G_TX_RENDERTILE,
            TexelCoordinate(textureColumn),
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawTurnedPageArrows(Gfx **displayListHead, s32 spacing, s32 y) {
    Gfx *gfx = *displayListHead;
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
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gDPPipeSync(gfx++);
    for (arrowIndex = 0; arrowIndex != PAGE_ARROW_COUNT; arrowIndex++) {
        guOrtho(
            &f_TurnedArrowProjections[arrowIndex],
            -(SCREEN_WIDTH / 2),
            SCREEN_WIDTH / 2,
            -(SCREEN_HEIGHT / 2),
            SCREEN_HEIGHT / 2,
            TURNED_ARROW_NEAR,
            TURNED_ARROW_FAR,
            1.0f
        );
        if (arrowIndex == 0) {
            x = -spacing / 2;
        }
        else {
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
        gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_PageArrows)));
        gDPLoadTextureTile_4b(
            gfx++,
            TextureAddress(ImageTexels(f_PageArrows) + (f_PageArrowFrame * PAGE_ARROW_IMAGE_SIZE)),
            G_IM_FMT_CI,
            PAGE_ARROW_SIZE,
            PAGE_ARROW_SIZE,
            0,
            0,
            PAGE_ARROW_SIZE - 1,
            PAGE_ARROW_SIZE - 1,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_TurnedArrowProjections[arrowIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_TurnedArrowTranslations[arrowIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_TurnedArrowRotations[arrowIndex]),
            G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_TurnedArrowScales[arrowIndex]),
            G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
        );
        gDPPipeSync(gfx++);
        gSPVertex(gfx++, f_TurnedArrowVertices, TURNED_ARROW_VERTEX_COUNT, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawUpDownArrows(Gfx **displayListHead, s32 centerY) {
    Gfx *gfx = *displayListHead;
    s32 left = 0;
    s32 offsetY = 0;
    s32 arrowIndex;

    f_GreenArrowTimer++;
    if (f_GreenArrowTimer == STATS_ARROW_BLINK_FRAME_COUNT) {
        f_GreenArrowTimer = 0;
        f_GreenArrowFrame ^= 1;
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_StatsGreenArrows)));
    for (arrowIndex = 0; arrowIndex != STATS_ARROW_COUNT; arrowIndex++) {
        gDPLoadTextureTile_4b(
            gfx++,
            TextureAddress(
                (u8 *)f_StatsGreenArrows +
                (PALETTED_TEXELS_OFFSET + arrowIndex * GREEN_ARROW_IMAGE_SIZE +
                    f_GreenArrowFrame * GREEN_ARROW_FRAME_SIZE)
            ),
            G_IM_FMT_CI,
            GREEN_ARROW_SIZE,
            GREEN_ARROW_SIZE,
            0,
            0,
            GREEN_ARROW_SIZE - 1,
            GREEN_ARROW_SIZE - 1,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        if (arrowIndex == 0) {
            left = STATS_ARROW_LEFT;
            offsetY = STATS_UP_ARROW_OFFSET;
        }
        else if (arrowIndex == 1) {
            left = STATS_ARROW_LEFT;
            offsetY = STATS_DOWN_ARROW_OFFSET;
        }

        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(left),
            QuarterPixels(offsetY + centerY),
            QuarterPixels(left + GREEN_ARROW_SIZE),
            QuarterPixels(offsetY + GREEN_ARROW_SIZE + centerY),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void LoadBestShotsCourseIcons(ImageAsset **buffers) {
    EncodedAsset asset;
    s16 imageIndex;

    f_BestShotsCourseIcons = buffers;
    for (imageIndex = 0; imageIndex != PLAYER_STATS_IMAGE_COUNT; imageIndex++) {
        OpenEncodedAsset(g_CourseIconAssets[imageIndex], &asset);
        ReadEncodedAsset(f_BestShotsCourseIcons[imageIndex], &asset);
    }
}

void LoadScoreLabelBuffer(void *buffer, ScoreLabelBuffer bufferKind) {
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

s32 FindNextBestShotCell(s8 column, s8 row, StatsDirection direction);

void UpdateBestShots(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s32 slot;
    s32 nextCell;
    s32 column;

    if (IsHudAnimationDone() == TRUE) {
        if (GetWipeKind() != WIPE_NONE && !g_IsMenuWipeOpening) {
            if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
                g_MenuScreen = MENU_SCREEN_EXIT;
                g_IsChosenHoleLoadStarted = FALSE;
                g_GameState.replayKind = REPLAY_KIND_BEST_SHOT;
                SetGameFlag(GAME_FLAG_LOADED_PLAY);
                for (slot = 0; slot != STATS_TEXT_SLOT_COUNT; slot++) {
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        slot,
                        0,
                        0xFF,
                        0xFF,
                        0xFF,
                        OPAQUE_ALPHA
                    );
                }
            }
        }
        else if (GetWipeKind() == WIPE_NONE || g_IsMenuWipeOpening != TRUE) {
            if (g_MenuPressedButtons & A_BUTTON) {
                if (GetChosenBestShot() != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                    g_IsMenuWipeOpening = FALSE;
                    column = memory->bestShotColumn;
                    LoadWipeShape(save
                            ->rankedRecords[f_BestShotRecords[memory->bestShotRow][column]]
                            .courseIndex);
                    StartWipe(WIPE_CLOSING);
                    g_IsRingShotWipeOpening = FALSE;
                }
            }
            else if (g_MenuPressedButtons & B_BUTTON) {
                StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
                for (slot = 0; slot != STATS_TEXT_SLOT_COUNT; slot++) {
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        slot,
                        0,
                        0xFF,
                        0xFF,
                        0xFF,
                        OPAQUE_ALPHA
                    );
                }
            }
            else if (g_MenuRepeatButtons & U_JPAD) {
                nextCell = FindNextBestShotCell(
                    memory->bestShotColumn,
                    memory->bestShotRow,
                    STATS_DIRECTION_UP
                );
                if ((s16)nextCell != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->bestShotRow = nextCell;
                }
            }
            else if (g_MenuRepeatButtons & D_JPAD) {
                nextCell = FindNextBestShotCell(
                    memory->bestShotColumn,
                    memory->bestShotRow,
                    STATS_DIRECTION_DOWN
                );
                if ((s16)nextCell != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->bestShotRow = nextCell;
                }
            }
            else if (g_MenuRepeatButtons & L_JPAD) {
                nextCell = FindNextBestShotCell(
                    memory->bestShotColumn,
                    memory->bestShotRow,
                    STATS_DIRECTION_LEFT
                );
                if ((s16)nextCell != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->bestShotColumn = nextCell;
                }
            }
            else if (g_MenuRepeatButtons & R_JPAD) {
                nextCell = FindNextBestShotCell(
                    memory->bestShotColumn,
                    memory->bestShotRow,
                    STATS_DIRECTION_RIGHT
                );
                if ((s16)nextCell != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->bestShotColumn = nextCell;
                }
            }
        }
    }
}

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
#define RECORD_COURSE_LEFT 40
#define RECORD_COURSE_RIGHT 96
#define RECORD_COURSE_TOP 176
#define RECORD_COURSE_STRIP_SPACING 20
#define RECORD_COURSE_NAME_X 120
#define RECORD_COURSE_NAME_Y 180
#define RECORD_HOLE_X 200
#define RECORD_HOLE_Y 204
#define NO_RECORDS_X (SCREEN_WIDTH / 2)
#define NO_RECORDS_Y 192
#define MESSAGE_NO_RECORDS 0x16E

void DrawBestShots(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s16 index;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s16 column;
    s16 characterIndex;
    s32 iconRow;
    s32 iconColumn;
    s32 nameRow;
    s32 nameColumn;

    DrawMenuBackdropLayers(&gfx, PLAIN_BACKDROP_LAYERS);
    gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(
        gfx++,
        QuarterPixels(RECORDS_PANEL_LEFT),
        QuarterPixels(RECORDS_PANEL_TOP),
        QuarterPixels(RECORDS_PANEL_RIGHT),
        QuarterPixels(RECORDS_PANEL_BOTTOM),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
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
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_BestShotsNamePlate)));
    for (index = 0; index != RECORDS_STRIP_COUNT; index++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_BestShotsNamePlate) +
                (index * RECORDS_STRIP_WIDTH * RECORDS_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            RECORDS_STRIP_WIDTH,
            RECORDS_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(RECORDS_STRIP_LEFT),
            QuarterPixels(RECORDS_STRIP_TOP + (index * RECORDS_STRIP_SPACING)),
            QuarterPixels(RECORDS_STRIP_RIGHT),
            QuarterPixels(
                RECORDS_STRIP_TOP + RECORDS_STRIP_SPACING + (index * RECORDS_STRIP_SPACING)
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(RECORDS_STRIP_WIDTH, RECORDS_STRIP_RIGHT - RECORDS_STRIP_LEFT),
            TexelRunStep(RECORDS_STRIP_HEIGHT, RECORDS_STRIP_SPACING)
        );
        gDPPipeSync(gfx++);
    }

    for (index = 0; index != STATS_GRID_ROW_COUNT; index++) {
        for (column = 0; column != STATS_GRID_COLUMN_COUNT; column++) {
            if (f_BestShotRecords[index][column] != STATS_CELL_EMPTY) {
                characterIndex =
                    save->rankedRecords[f_BestShotRecords[index][column]].characterIndex;
                DrawCharacterFace(
                    &gfx,
                    g_CharacterFaceImages[characterIndex],
                    f_BestShotFaces[index][column],
                    RECORD_FACE_X + (column * STATS_CURSOR_COLUMN_WIDTH),
                    RECORD_FACE_Y + (index * STATS_CURSOR_ROW_HEIGHT),
                    characterIndex,
                    FACE_PLAIN
                );
            }

            DrawOneDigitMenuNumber(
                &gfx,
                RECORD_PLACE_X + (column * STATS_CURSOR_COLUMN_WIDTH),
                RECORD_PLACE_Y + (index * STATS_CURSOR_ROW_HEIGHT),
                column + 1,
                MENU_NUMBER_PLAIN
            );
        }
    }

    if (f_BestShotRecords[memory->bestShotRow][memory->bestShotColumn] != STATS_CELL_EMPTY) {
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
    iconRow = memory->bestShotRow;
    iconColumn = memory->bestShotColumn;

    if (f_BestShotRecords[(s8)iconRow][(s8)iconColumn] != STATS_CELL_EMPTY) {
        gDPLoadTLUT_pal256(
            gfx++,
            TextureAddress(
                (u32)f_BestShotsCourseIcons
                    [save->rankedRecords[f_BestShotRecords[iconRow][iconColumn]].courseIndex] +
                IMAGE_HEADER_SIZE
            )
        );
        for (index = 0; index != COURSE_ICON_STRIP_COUNT; index++) {
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(
                    (u32)f_BestShotsCourseIcons
                        [save->rankedRecords[f_BestShotRecords[iconRow][iconColumn]].courseIndex] +
                    LARGE_PALETTED_TEXELS_OFFSET +
                    (index * COURSE_ICON_WIDTH * COURSE_ICON_STRIP_HEIGHT)
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                COURSE_ICON_WIDTH,
                COURSE_ICON_STRIP_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(RECORD_COURSE_LEFT),
                QuarterPixels(RECORD_COURSE_TOP + (index * RECORD_COURSE_STRIP_SPACING)),
                QuarterPixels(RECORD_COURSE_RIGHT),
                QuarterPixels(
                    RECORD_COURSE_TOP + RECORD_COURSE_STRIP_SPACING +
                    (index * RECORD_COURSE_STRIP_SPACING)
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep(COURSE_ICON_WIDTH, RECORD_COURSE_RIGHT - RECORD_COURSE_LEFT),
                TexelRunStep(COURSE_ICON_STRIP_HEIGHT, RECORD_COURSE_STRIP_SPACING)
            );
            gDPPipeSync(gfx++);
        }
    }

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    for (index = 0; index != STATS_GRID_ROW_COUNT; index++) {
        sprintf(g_PrintBuffer, "%s", f_RecordLabels[index]);
        DrawLargeText(
            &gfx,
            RECORD_LABEL_X,
            (s16)(RECORD_PLACE_Y + (index * STATS_CURSOR_ROW_HEIGHT)),
            g_PrintBuffer
        );
    }

    nameRow = memory->bestShotRow;
    nameColumn = memory->bestShotColumn;

    if (f_BestShotRecords[(s8)nameRow][(s8)nameColumn] != STATS_CELL_EMPTY) {
        sprintf(
            g_PrintBuffer,
            "%s",
            g_CourseRecords[save->rankedRecords[f_BestShotRecords[nameRow][nameColumn]].courseIndex]
                .name
        );
        DrawLargeText(&gfx, RECORD_COURSE_NAME_X, RECORD_COURSE_NAME_Y, g_PrintBuffer);
        SetTextSlotValue(
            save->rankedRecords[f_BestShotRecords[nameRow][nameColumn]].holeIndex + 1,
            1,
            0
        );
        SetTextSlot(
            RECORD_HOLE_X,
            RECORD_HOLE_Y,
            MESSAGE_HOLE_NUMBER,
            TEXT_SLOT_LARGE_FONT,
            1,
            0,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
    }

    if (!f_HasBestShots) {
        SetTextSlot(
            NO_RECORDS_X,
            NO_RECORDS_Y,
            MESSAGE_NO_RECORDS,
            TEXT_SLOT_LARGE_FONT,
            0,
            TEXT_FLAG_CENTERED,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
    }

    *displayListHead = gfx;
}

void ReadBestShots(void) {
    SaveData *save = GetSaveData();
    s32 kindRecords[STATS_GRID_ROW_COUNT][STATS_KIND_LIST_SIZE];
    s32 index;
    s32 recordIndex;
    s32 birdieCount;
    s32 eagleCount;
    s32 albatrossCount;
    s32 holeInOneCount;
    s32 scoreKind;
    s32 row;
    s32 column;
    MenuMemory *memory;

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
        scoreKind = (s8)save->rankedRecords[recordIndex].scoreKind;
        if (scoreKind & SCORE_KIND_RECORDED) {
            switch (scoreKind & SCORE_KIND_MASK) {
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

    row = column = BEST_SHOT_ROW_BIRDIE;
    memory = GetMenuMemory();
    column = row;
    if (birdieCount == 0) {
        if (eagleCount != 0) {
            row = BEST_SHOT_ROW_EAGLE;
        }
        else if (albatrossCount != 0) {
            row = BEST_SHOT_ROW_ALBATROSS;
        }
        else if (holeInOneCount != 0) {
            row = BEST_SHOT_ROW_HOLE_IN_ONE;
        }
    }

    recordIndex = memory->bestShotRow;
    switch (recordIndex) {
        case BEST_SHOT_ROW_BIRDIE:
            if (birdieCount != 0) {
                row = BEST_SHOT_ROW_BIRDIE;
                if (memory->bestShotColumn < birdieCount && memory->bestShotColumn >= 0) {
                    column = memory->bestShotColumn;
                }
            }

            break;
        case BEST_SHOT_ROW_EAGLE:
            if (eagleCount != 0) {
                row = BEST_SHOT_ROW_EAGLE;
                if (memory->bestShotColumn < eagleCount && memory->bestShotColumn >= 0) {
                    column = memory->bestShotColumn;
                }
            }

            break;
        case BEST_SHOT_ROW_ALBATROSS:
            if (albatrossCount != 0) {
                row = BEST_SHOT_ROW_ALBATROSS;
                if (memory->bestShotColumn < albatrossCount && memory->bestShotColumn >= 0) {
                    column = memory->bestShotColumn;
                }
            }

            break;
        case BEST_SHOT_ROW_HOLE_IN_ONE:
            if (holeInOneCount != 0) {
                row = BEST_SHOT_ROW_HOLE_IN_ONE;
                if (memory->bestShotColumn < holeInOneCount && memory->bestShotColumn >= 0) {
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

s32 FindNextBestShotCell(s8 column, s8 row, StatsDirection direction) {
    s32 stepCount;
    s32 stepIndex;

    if (direction == STATS_DIRECTION_LEFT || direction == STATS_DIRECTION_RIGHT) {
        stepCount = STATS_GRID_COLUMN_COUNT;
    }
    else if (direction == STATS_DIRECTION_UP || direction == STATS_DIRECTION_DOWN) {
        stepCount = STATS_GRID_ROW_COUNT;
    }
    else {
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

    if (direction == STATS_DIRECTION_LEFT || direction == STATS_DIRECTION_RIGHT) {
        return column;
    }
    else if (direction == STATS_DIRECTION_UP || direction == STATS_DIRECTION_DOWN) {
        return row;
    }

    return UNSET_RESULT;
}

s16 GetChosenBestShot(void) {
    MenuMemory *memory = GetMenuMemory();

    return f_BestShotRecords[memory->bestShotRow][memory->bestShotColumn];
}

void DrawScoreLabel(Gfx **displayListHead, s32 left, s32 top, s32 labelIndex) {
    Gfx *gfx = *displayListHead;
    u8 *image;
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
    width = ReadSmallImageWidth(image);
    height = ReadSmallImageHeight(image);

    gDPLoadTLUT_pal16(gfx++, 0, image[IMAGE_HEADER_SIZE] & ~7);
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(image)),
        G_IM_FMT_CI,
        width,
        height,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(left),
        QuarterPixels(top),
        QuarterPixels(left + width),
        QuarterPixels(top + height),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawBestShotsCursor(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    MenuMemory *memory = GetMenuMemory();
    s32 vertexIndex;
    s32 red;
    s32 blue;
    s32 green;

    for (vertexIndex = 0; vertexIndex != MENU_CURSOR_VERTEX_COUNT; vertexIndex++) {
        f_BestShotsCursorPhases[vertexIndex] += MENU_CURSOR_PHASE_STEP;
        red = (s32)(sinf(f_BestShotsCursorPhases[vertexIndex]) * MENU_CURSOR_COLOR_SWING) +
            MENU_CURSOR_COLOR_MIDDLE;
        blue = (s32)(sinf(f_BestShotsCursorPhases[vertexIndex] + (M_PI / 2)) *
                   MENU_CURSOR_COLOR_SWING) +
            MENU_CURSOR_COLOR_MIDDLE;
        green = (s32)(sinf(f_BestShotsCursorPhases[vertexIndex] + M_PI) * MENU_CURSOR_COLOR_SWING) +
            MENU_CURSOR_COLOR_MIDDLE;
        f_BestShotsCursorVertices[vertexIndex].v.cn[0] = red;
        f_BestShotsCursorVertices[vertexIndex].v.cn[2] = blue;
        f_BestShotsCursorVertices[vertexIndex].v.cn[1] = green;
    }

    guOrtho(
        &f_BestShotsCursorProjection,
        -(SCREEN_WIDTH / 2),
        SCREEN_WIDTH / 2,
        -(SCREEN_HEIGHT / 2),
        SCREEN_HEIGHT / 2,
        MENU_CURSOR_NEAR,
        MENU_CURSOR_FAR,
        1.0f
    );
    guTranslate(
        &f_BestShotsCursorTranslation,
        (memory->bestShotColumn * STATS_CURSOR_COLUMN_WIDTH) + STATS_CURSOR_LEFT_X,
        STATS_CURSOR_TOP_Y - (memory->bestShotRow * STATS_CURSOR_ROW_HEIGHT),
        MENU_CURSOR_Z
    );
    guRotate(&f_BestShotsCursorRotation, 0.0f, 0.0f, 1.0f, 0.0f);
    guScale(&f_BestShotsCursorScale, 1.0f, MENU_CURSOR_SCALE_Y, 1.0f);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_BestShotsCursorBox)),
        G_IM_FMT_IA,
        MENU_CURSOR_SIZE,
        MENU_CURSOR_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    AppendSyncedRdpState(gfx, gDPSetCycleType, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    gDPSetCombineMode(gfx++, G_CC_SHADEDECALA, G_CC_SHADEDECALA);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_BestShotsCursorProjection),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_BestShotsCursorTranslation),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_BestShotsCursorRotation),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_BestShotsCursorScale),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
    );
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, f_BestShotsCursorVertices, MENU_CURSOR_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void LoadBestShotFaces(void) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save = GetSaveData();
    s32 index;
    s32 row;
    s32 column;
    s32 paletteIndex;
    CharacterIndex characterIndex;

    for (index = 0; index != CHARACTER_COUNT; index++) {
        OpenEncodedAsset(g_CharacterFaceAssets[index], &asset);
        ReadEncodedAsset(g_CharacterFaceImages[index], &asset);
    }

    for (row = 0; row != STATS_GRID_ROW_COUNT; row++) {
        for (column = 0; column != STATS_GRID_COLUMN_COUNT; column++) {
            if (f_BestShotRecords[row][column] != STATS_CELL_EMPTY) {
                characterIndex = save->rankedRecords[f_BestShotRecords[row][column]].characterIndex;
                paletteIndex = save->rankedRecords[f_BestShotRecords[row][column]].paletteIndex;

                if (characterIndex < 0) {
                    characterIndex = 0;
                }

                if (characterIndex >= CHARACTER_COUNT) {
                    characterIndex = CHARACTER_COUNT - 1;
                }

                if (paletteIndex < 0) {
                    paletteIndex = 0;
                }

                if (paletteIndex >= CHARACTER_PALETTE_COUNT) {
                    paletteIndex = CHARACTER_PALETTE_COUNT - 1;
                }

                f_BestShotFaces[row][column] =
                    g_MenuCharacterPalettes[(row * STATS_GRID_COLUMN_COUNT) + column];
            }
            else {
                characterIndex = 0;
                paletteIndex = 0;
                f_BestShotFaces[row][column] =
                    g_MenuCharacterPalettes[(row * STATS_GRID_COLUMN_COUNT) + column];
            }

            OpenAssetFile(g_CharacterPaletteFiles[characterIndex][paletteIndex], &file);
            ReadAssetFile(f_BestShotFaces[row][column], GetAssetFileSize(&file), &file);
        }
    }
}

void DrawCharacterFace(
    Gfx **displayListHead,
    void *texels,
    void *palette,
    s32 left,
    s32 top,
    s16 characterIndex,
    u8 drawMode
) {
    Gfx *gfx = *displayListHead;

    if (characterIndex < 0) {
        characterIndex = 0;
    }

    if (characterIndex >= CHARACTER_COUNT) {
        characterIndex = CHARACTER_COUNT - 1;
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
        gDPSetPrimColor(
            gfx++,
            0,
            0,
            FACE_DARKENED_SHADE,
            FACE_DARKENED_SHADE,
            FACE_DARKENED_SHADE,
            OPAQUE_ALPHA
        );
    }
    else {
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    }

    gDPLoadTLUT_pal256(gfx++, TextureAddress(palette));
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(texels),
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        FACE_WIDTH,
        FACE_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(left),
        QuarterPixels(top),
        QuarterPixels(left + FACE_WIDTH),
        QuarterPixels(top + FACE_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    if (drawMode == FACE_WITH_NAME) {
        if (characterIndex < NAMED_CHARACTER_COUNT) {
            SetUpTextDrawing(&gfx);
            SetTextColor(&gfx, 255, 255, 255);
            sprintf(g_PrintBuffer, "%s", g_MenuCharacterNames[f_CharacterChoices[characterIndex]]);
            DrawLargeText(&gfx, left + FACE_NAME_OFFSET_X, top + FACE_NAME_OFFSET_Y, g_PrintBuffer);
            gDPPipeSync(gfx++);
        }
    }

    *displayListHead = gfx;
}
