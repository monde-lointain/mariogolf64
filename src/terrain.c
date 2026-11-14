#include "terrain.h"

#include <math.h>
#include <ultra64.h>

#include "animated_box.h"
#include "billboard.h"
#include "camera.h"
#include "character_model.h"
#include "club.h"
#include "collision.h"
#include "controllers/controller.h"
#include "course_space.h"
#include "current_shot.h"
#include "debug_text.h"
#include "display_list.h"
#include "game_flags.h"
#include "game_state.h"
#include "geometry.h"
#include "hole_builder.h"
#include "hole_editor.h"
#include "hole_loader.h"
#include "hole_setup.h"
#include "hole_triangles.h"
#include "math_util.h"
#include "menu.h"
#include "play.h"
#include "round_hud.h"
#include "scene.h"
#include "screen.h"
#include "sound.h"
#include "surface.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"
#include "work_area.h"

#define SCENE_MENU_LENGTH 16
#define SCENE_MENU_TOP_ROW 5
#define SCENE_MENU_CURSOR_COLUMN 10
#define SCENE_MENU_LABEL_COLUMN 12
#define SCENE_LABEL_COLUMN 2
#define SCENE_LABEL_ROW 3
#define SURFACE_NAME_COLUMN 2
#define SURFACE_NAME_ROW 4
#define RESET_LIGHT_PITCH 0x3000
#define RESET_LIGHT_YAW 0x2000
#define SCENE_MENU_BOX_LEFT 72
#define SCENE_MENU_BOX_TOP 32
#define SCENE_MENU_BOX_RIGHT 152
#define SCENE_MENU_BOX_BOTTOM 152

#define MAP_OVERVIEW_ADJUSTMENT_MIN 5
#define MAP_OVERVIEW_ADJUSTMENT_MAX 0x200
#define MAP_OVERVIEW_ADJUSTMENT_START 10
#define MAP_OVERVIEW_SELECTION_COUNT 7
#define MAP_OVERVIEW_CAMERA_STEP 8
#define MAP_OVERVIEW_ADJUSTMENT_STEP 2
#define MAP_OVERVIEW_CAMERA_SCALE 16
#define STICK_STEP_SIZE 32

#define PointTile(point) ((point) / GROUND_TILE_POINT_SPAN)
#define PointSlot(point) ((point) & (GROUND_TILE_POINT_SPAN - 1))
#define TILE_POINT_SPAN_SHIFT 2
#define TileIndex(tileColumn, tileRow) (((tileRow) * GROUND_TILE_COLUMN_COUNT) + (tileColumn))

#define IsSharedWithPreviousTileRow(column, row) \
    (((column) & 1) && !PointSlot(row) && (PointTile(row) > 0))
#define IsSharedWithPreviousTileColumn(column, row) \
    (((row) & 1) && !PointSlot(column) && (PointTile(column) > 0))

typedef struct MapOverviewControls {
    s32 cameraX;
    s32 cameraZ;
    s32 selectionIndex;
    s32 adjustment;
    Boolean isToggled;
} MapOverviewControls;

typedef struct FlagPanel {
    s32 firstRow;
    s32 column;
    s32 row;
} FlagPanel;

#define MAP_TEXEL(column, row) \
    g_WorkArea.hole \
        .tiles \
            [((column) / GROUND_TILE_TEXEL_SPAN) + \
                (((row) / GROUND_TILE_TEXEL_SPAN) * GROUND_TILE_COLUMN_COUNT)] \
        .texels[1] \
               [((column) & (GROUND_TILE_TEXEL_SPAN - 1)) + \
                   (((row) & (GROUND_TILE_TEXEL_SPAN - 1)) * GROUND_TILE_TEXEL_ROW_LENGTH) + 1]

#define WATER_PROBE_REACH 3072000
#define MINI_GOLF_WATER_PROBE_REACH 614400
#define WaterProbeHeight(fraction, reach) (((fraction) * (2.0f * (reach))) + -(f32)(reach))
#define WATER_BIAS 0x266
#define LOWERED_WATER_BIAS (-0x600)

#define SURFACE_EDGE_MARGIN ((s32)UNITS_PER_METER)
#define IsInSurfaceEdgeMargin(coordinate, sideSize) \
    ((u32)((coordinate) - SURFACE_EDGE_MARGIN) > (sideSize) - (SURFACE_EDGE_MARGIN * 2) - 1)
#define NEAREST_CODE_COUNT 4
#define GREEN_SURFACE_WEIGHT 1.1f
#define ROUGH_SURFACE_WEIGHT 0.9f
#define BUNKER_SURFACE_WEIGHT 0.7f
#define OUT_OF_BOUNDS_SURFACE_WEIGHT 0.5f
#define MINI_GOLF_OUT_OF_BOUNDS_SURFACE_WEIGHT 0.1f
#define MINI_GOLF_TEXEL_WIDTH (MINI_GOLF_GROUND_SIZE / CODE_GRID_COLUMN_COUNT)
#define MINI_GOLF_TEXEL_LENGTH (MINI_GOLF_GROUND_SIZE / CODE_GRID_ROW_COUNT)

enum TerrainColorIndex {
    TERRAIN_COLOR_FAIRWAY,
    TERRAIN_COLOR_GREEN,
    TERRAIN_COLOR_BUNKER,
    TERRAIN_COLOR_ROUGH,
    TERRAIN_COLOR_NO_DROP_GROUND,
    TERRAIN_COLOR_OUT_OF_BOUNDS,
    TERRAIN_COLOR_CART_WAY,
    TERRAIN_COLOR_OLIVE_FAIRWAY,
    TERRAIN_COLOR_TEE_GROUND,
    TERRAIN_COLOR_ROCK,
    TERRAIN_COLOR_DEEP_ROUGH
};

typedef s32 TerrainColorIndex;

#define TERRAIN_COLOR_COUNT 11

#define SLOT_GRID_COLUMN_SHIFT 2
#define SLOT_GRID_MASK 3
s32 g_CoarseVertexOffsets[GROUND_TILE_COUNT][TILE_VERTEX_SEGMENT_COUNT];
Gfx *g_GroundTileDisplayLists[GROUND_TILE_COUNT];
u16 g_TerrainCodeColors[TERRAIN_CODE_COUNT];
Vtx *g_VertexListHead;

static u8 f_PointSlots[GROUND_TILE_POINT_SPAN + 1][8] = {
    {0x80, 0x00, 0x84, 0x01, 0x88},
    {0x02, 0x08, 0x09, 0x0A, 0x03},
    {0x81, 0x0B, 0x85, 0x0C, 0x89},
    {0x04, 0x0D, 0x0E, 0x0F, 0x05},
    {0x82, 0x06, 0x86, 0x07, 0x8A},
};
HoleSetup g_HoleSetup = {
    .cupPosition = {GROUND_WIDTH / 2, 0, GROUND_LENGTH / 2},
    .teePosition = {GROUND_WIDTH / 2, 0, GROUND_LENGTH / 8},
};
f32 g_GroundTileHeightRanges[GROUND_TILE_COUNT] = {0};
s16 g_TerrainCodeCells[TERRAIN_CODE_CELL_ROW_COUNT][TERRAIN_CODE_CELL_COLUMN_COUNT] = {{0}};
s32 g_TerrainCodeBlockCount = -1;

/* clang-format off */
static u8 f_UnusedBytesAfterTerrainBlockCount[16] UNUSED = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};
/* clang-format on */

Scene g_Scene = SCENE_FLAG_PANEL;
Scene g_NextScene = SCENE_SWAPPING_MODULES;
static Scene f_SceneBeforeFlagPanel = SCENE_BOOT_LOGOS;
ModuleId g_SceneModuleToUnload = MODULE_BOOT_LOGOS;
ModuleId g_SceneModuleToLoad = MODULE_BOOT_LOGOS;
ModuleId g_SubModuleId = MODULE_SCORECARD;
ModuleId g_PendingSubModuleId = MODULE_SCORECARD;
SubModuleState g_SubModuleState = SUB_MODULE_UNLOADED;
static TerrainColor f_TerrainColors[TERRAIN_COLOR_COUNT] = {
    {0x05, 0x19, 0x00, 1},
    {0x05, 0x1E, 0x05, 1},
    {0x1C, 0x1C, 0x1C, 1},
    {0x00, 0x10, 0x00, 1},
    {0x00, 0x00, 0x1F, 0},
    {0x0F, 0x08, 0x00, 1},
    {0x0F, 0x0F, 0x0F, 1},
    {0x08, 0x16, 0x00, 1},
    {0x1F, 0x16, 0x00, 1},
    {0x05, 0x05, 0x05, 1},
    {0x00, 0x1F, 0x1F, 1},
};
static MapOverviewControls f_MapOverviewControls = {0, 0, 0, MAP_OVERVIEW_ADJUSTMENT_START, FALSE};

static FlagPanel f_FlagPanel = {0, 0, 0};
static Boolean f_IsSceneMenuOpen = FALSE;
static AnimatedBox f_SceneMenuBox = {
    SCENE_MENU_BOX_LEFT,
    SCENE_MENU_BOX_TOP,
    SCENE_MENU_BOX_RIGHT,
    SCENE_MENU_BOX_BOTTOM,
    0x00,
    0x00,
    0x00,
    0x9F,
    BOX_CLOSED,
    0,
};
static Scene f_SceneMenuScenes[SCENE_MENU_LENGTH] = {
    SCENE_PLAY_HOLE,
    SCENE_GROUND_CAMERA,
    SCENE_HEIGHTS,
    SCENE_CELL_EDITOR,
    SCENE_POINT_EDITOR,
    SCENE_TRIANGLES,
    SCENE_TREES,
    SCENE_PIN,
    SCENE_FLAG_PANEL,
    SCENE_MENU,
    SCENE_MAP_OVERVIEW,
    SCENE_HOLE_FILES,
    SCENE_PLAY_ROUND,
    SCENE_PLAY_BEST_SHOT_REPLAY,
    SCENE_RINGS,
    SCENE_WALLS,
};
static Boolean f_IsFirstDebugUpdate = TRUE;
s32 g_UnreadGroundQueryMarker = 0;
static s8 f_CellSearchOffsets[GROUND_TILE_POINT_SPAN] = {0, 1, 3, 2};

static Scene f_SceneMenuSelection;

#define FLAG_PANEL_ROW_COUNT 8
#define FLAG_PANEL_COLUMN_COUNT 16
#define FLAG_PANEL_FIRST_ROW_MAX 8
#define BUILD_DATE_LABEL_COLUMN 3
#define BUILD_DATE_COLUMN 8
#define ROM_BUILD_DATE_ROW 4
#define SAVE_BUILD_DATE_ROW 5
#define FLAG_PANEL_HEADER_ROW 12
#define FLAG_PANEL_FIRST_TEXT_ROW 14
#define FLAG_ROW_LABEL_COLUMN 3
#define FIRST_FLAG_TEXT_COLUMN 6
#define FLAG_TEXT_SPACING 2
#define FLAG_PANEL_TEXT_SIZE 0x10

s32 GetTerrainHeight(s32 x, s32 z);

static inline s32 GetCellCodeOffset(s32 cellEntry, s32 column, s32 row) {
    return ((column & (TERRAIN_CODE_CELL_SPAN - 1)) >> 1) +
        ((row & (TERRAIN_CODE_CELL_SPAN - 1)) * (TERRAIN_CODE_CELL_SPAN / 2)) +
        ((cellEntry & (TERRAIN_CODE_CELL_BLOCK - 1)) * TERRAIN_CODE_BLOCK_SIZE);
}

TerrainCode GetTerrainCode(s32 column, s32 row) {
    s32 cellEntry;
    s32 cellOffset;

    if (column < 0) {
        column = 0;
    }
    else if (column >= CODE_GRID_COLUMN_COUNT) {
        column = CODE_GRID_COLUMN_COUNT - 1;
    }

    if (row < 0) {
        row = 0;
    }
    else if (row >= CODE_GRID_ROW_COUNT) {
        row = CODE_GRID_ROW_COUNT - 1;
    }

    cellOffset = ((column / TERRAIN_CODE_CELL_SPAN) * TERRAIN_CODE_CELL_SIZE) +
        ((row / TERRAIN_CODE_CELL_SPAN) * TERRAIN_CODE_CELL_ROW_SIZE);
    cellEntry = *(s16 *)AddressPast(g_TerrainCodeCells, cellOffset);
    if (cellEntry == NO_CELL_TERRAIN_CODE) {
        return NO_CELL_TERRAIN_CODE;
    }

    if (cellEntry >= 0) {
        return cellEntry;
    }

    if (!(column & 1)) {
        return g_WorkArea.hole.terrainCodes[GetCellCodeOffset(cellEntry, column, row)] >>
            TERRAIN_CODE_BITS;
    }

    return g_WorkArea.hole.terrainCodes[GetCellCodeOffset(cellEntry, column, row)] &
        TERRAIN_CODE_MASK;
}

#define BEYOND_ANY_COLOR_DISTANCE 1000000

TerrainColorIndex FindNearestTerrainColor(s32 color) {
    s32 nearestDistance = BEYOND_ANY_COLOR_DISTANCE;
    TerrainColorIndex nearestIndex = TERRAIN_COLOR_FAIRWAY;
    s32 colorRed = (color >> RGBA16_RED_SHIFT) & RGBA16_CHANNEL_MASK;
    s32 colorGreen = (color >> RGBA16_GREEN_SHIFT) & RGBA16_CHANNEL_MASK;
    s32 colorBlue = (color >> RGBA16_BLUE_SHIFT) & RGBA16_CHANNEL_MASK;
    TerrainColorIndex colorIndex;
    s32 red;
    s32 green;
    s32 blue;
    s32 distance;

    for (colorIndex = 0; colorIndex < TERRAIN_COLOR_COUNT; colorIndex++) {
        red = colorRed - f_TerrainColors[colorIndex].red;
        if (red < 0) {
            red = -red;
        }

        green = colorGreen - f_TerrainColors[colorIndex].green;
        if (green < 0) {
            green = -green;
        }

        blue = colorBlue - f_TerrainColors[colorIndex].blue;
        distance = red + green + Abs(blue);
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestIndex = colorIndex;
        }
    }

    return nearestIndex;
}

s32 GetTerrainCodeColor(TerrainCode code) {
    TerrainColorIndex colorIndex = TERRAIN_COLOR_FAIRWAY;

    switch (code) {
        case TERRAIN_CODE_FAIRWAY:
            colorIndex = TERRAIN_COLOR_FAIRWAY;
            break;
        case TERRAIN_CODE_GREEN:
            colorIndex = TERRAIN_COLOR_GREEN;
            break;
        case TERRAIN_CODE_BUNKER:
            colorIndex = TERRAIN_COLOR_BUNKER;
            break;
        case TERRAIN_CODE_ROUGH:
            colorIndex = TERRAIN_COLOR_ROUGH;
            break;
        case TERRAIN_CODE_NO_DROP_GROUND:
            colorIndex = TERRAIN_COLOR_NO_DROP_GROUND;
            break;
        case TERRAIN_CODE_OUT_OF_BOUNDS:
            colorIndex = TERRAIN_COLOR_OUT_OF_BOUNDS;
            break;
        case TERRAIN_CODE_CART_WAY:
            colorIndex = TERRAIN_COLOR_CART_WAY;
            break;
        case TERRAIN_CODE_TEE_GROUND:
            colorIndex = TERRAIN_COLOR_TEE_GROUND;
            break;
        case TERRAIN_CODE_ROCK:
            colorIndex = TERRAIN_COLOR_ROCK;
            break;
        case TERRAIN_CODE_DEEP_ROUGH:
            colorIndex = TERRAIN_COLOR_DEEP_ROUGH;
            break;
    }

    return (f_TerrainColors[colorIndex].red << RGBA16_RED_SHIFT) |
        (f_TerrainColors[colorIndex].green << RGBA16_GREEN_SHIFT) |
        (f_TerrainColors[colorIndex].blue << RGBA16_BLUE_SHIFT) | f_TerrainColors[colorIndex].alpha;
}

TerrainCode GetColorTerrainCode(s32 color) {
    switch (FindNearestTerrainColor(color)) {
        case TERRAIN_COLOR_GREEN:
            return TERRAIN_CODE_GREEN;
        case TERRAIN_COLOR_BUNKER:
            return TERRAIN_CODE_BUNKER;
        case TERRAIN_COLOR_ROUGH:
            return TERRAIN_CODE_ROUGH;
        case TERRAIN_COLOR_NO_DROP_GROUND:
            return TERRAIN_CODE_NO_DROP_GROUND;
        case TERRAIN_COLOR_CART_WAY:
            return TERRAIN_CODE_CART_WAY;
        case TERRAIN_COLOR_FAIRWAY:
        case TERRAIN_COLOR_OLIVE_FAIRWAY:
            return TERRAIN_CODE_FAIRWAY;
        case TERRAIN_COLOR_TEE_GROUND:
            return TERRAIN_CODE_TEE_GROUND;
        case TERRAIN_COLOR_ROCK:
            return TERRAIN_CODE_ROCK;
        case TERRAIN_COLOR_DEEP_ROUGH:
            return TERRAIN_CODE_DEEP_ROUGH;
    }

    return TERRAIN_CODE_OUT_OF_BOUNDS;
}

#define SURFACE_PERCENT_MIN 5

void GetSurfacePercentRange(SurfaceTraits *surfaceTraits, Club *club, s32 *range) {
    if (club->surfaceMask & GetSurfaceClubMask(TRUE, surfaceTraits)) {
        range[0] = surfaceTraits->percentMin;
        range[1] = surfaceTraits->percentMax;
    }
    else {
        range[0] = surfaceTraits->percentMin - surfaceTraits->wrongClubPenalty;
        if (range[0] < SURFACE_PERCENT_MIN) {
            range[0] = SURFACE_PERCENT_MIN;
        }

        range[1] = surfaceTraits->percentMax - surfaceTraits->wrongClubPenalty;
        if (range[1] < SURFACE_PERCENT_MIN) {
            range[1] = SURFACE_PERCENT_MIN;
        }
    }
}

#define WATER_RATIO_COLUMN 10
#define WATER_RATIO_ROW 11

Surface GetMiniGolfSurface(s32 x, s32 z, Boolean lowersWaterLevel) {
    s8 surfaces[NEAREST_CODE_COUNT];
    f32 weights[NEAREST_CODE_COUNT] UNUSED;
    PlaneTriangle hit;
    f32 fraction;
    s32 shiftedX;
    s32 shiftedZ;
    s32 waterBias;
    s32 neighborIndex;
    s32 column;
    s32 row;
    s32 stepColumn;
    s32 stepRow;
    f32 weight;
    Surface surface;

    shiftedX = x;
    shiftedZ = z;
    shiftedX -= MINI_GOLF_TEXEL_WIDTH / 2;
    shiftedZ -= MINI_GOLF_TEXEL_LENGTH / 2;
    if (IsInSurfaceEdgeMargin(shiftedX, MINI_GOLF_GROUND_SIZE) ||
        IsInSurfaceEdgeMargin(shiftedZ, MINI_GOLF_GROUND_SIZE)) {
        return SURFACE_OUT_OF_COURSE;
    }

    if (FindHoleTriangleHit(
            shiftedX,
            -MINI_GOLF_WATER_PROBE_REACH,
            shiftedZ,
            shiftedX,
            MINI_GOLF_WATER_PROBE_REACH,
            shiftedZ,
            &hit,
            &fraction
        )) {
        waterBias = WATER_BIAS;
        if (lowersWaterLevel) {
            waterBias = LOWERED_WATER_BIAS;
        }

        if (IsGameFlagSet(GAME_FLAG_RANDOM_FRAMEBUFFER_COUNT)) {
            char text[DEBUG_TEXT_BUFFER_SIZE];

            sprintf(
                text,
                "ratio %f,%x",
                fraction,
                (unsigned int)(s32)(WaterProbeHeight(fraction, MINI_GOLF_WATER_PROBE_REACH) +
                    waterBias)
            );
            PrintDebugText(text, WATER_RATIO_COLUMN, WATER_RATIO_ROW);
        }

        if (hit.tag == HOLE_TRIANGLE_KIND_FLAT) {
            neighborIndex = WaterProbeHeight(fraction, MINI_GOLF_WATER_PROBE_REACH) + waterBias;
            if (neighborIndex < GetTerrainHeight(x, z)) {
                return SURFACE_WATER_HAZARD;
            }
        }
    }

    column = shiftedX / MINI_GOLF_TEXEL_WIDTH;
    row = shiftedZ / MINI_GOLF_TEXEL_LENGTH;
    stepColumn = ((shiftedX & (MINI_GOLF_TEXEL_WIDTH - 1)) < MINI_GOLF_TEXEL_WIDTH / 2) ? -1 : 1;
    stepRow = ((shiftedZ & (MINI_GOLF_TEXEL_LENGTH - 1)) < MINI_GOLF_TEXEL_LENGTH / 2) ? -1 : 1;
    for (neighborIndex = 0; neighborIndex < NEAREST_CODE_COUNT; neighborIndex++) {
        switch (GetTerrainCode(
            (neighborIndex & 1) ? column + stepColumn : column,
            (neighborIndex & 2) ? row + stepRow : row
        )) {
            case TERRAIN_CODE_FAIRWAY:
                weight = 1.0f;
                surface = SURFACE_MINI_GOLF_FAIRWAY;
                break;
            case TERRAIN_CODE_GREEN:
                weight = GREEN_SURFACE_WEIGHT;
                surface = SURFACE_MINI_GOLF_GREEN;
                break;
            case TERRAIN_CODE_BUNKER:
                weight = BUNKER_SURFACE_WEIGHT;
                surface = SURFACE_BUNKER;
                break;
            case TERRAIN_CODE_ROUGH:
                weight = ROUGH_SURFACE_WEIGHT;
                surface = SURFACE_ROUGH;
                break;
            case TERRAIN_CODE_OUT_OF_BOUNDS:
                weight = MINI_GOLF_OUT_OF_BOUNDS_SURFACE_WEIGHT;
                surface = SURFACE_OUT_OF_BOUNDS;
                break;
            case TERRAIN_CODE_CART_WAY:
                weight = 1.0f;
                surface = SURFACE_MINI_GOLF_CART_WAY;
                break;
            case TERRAIN_CODE_TEE_GROUND:
                weight = 1.0f;
                surface = SURFACE_TEE_GROUND;
                break;
            case TERRAIN_CODE_ROCK:
                weight = 1.0f;
                surface = SURFACE_ROCK;
                break;
            case TERRAIN_CODE_GREEN_EDGE:
                weight = 1.0f;
                surface = SURFACE_GREEN_EDGE;
                break;
            case TERRAIN_CODE_SEMI_ROUGH:
                weight = 1.0f;
                surface = SURFACE_SEMI_ROUGH;
                break;
            case TERRAIN_CODE_BARE_GROUND:
                weight = 1.0f;
                surface = SURFACE_BARE_GROUND;
                break;
            case TERRAIN_CODE_NO_DROP_GROUND:
                weight = 1.0f;
                surface = SURFACE_NO_DROP_GROUND;
                break;
            case TERRAIN_CODE_DEEP_ROUGH:
                weight = ROUGH_SURFACE_WEIGHT;
                surface = SURFACE_DEEP_ROUGH;
                break;
            case TERRAIN_CODE_WASTE_AREA:
                weight = ROUGH_SURFACE_WEIGHT;
                surface = SURFACE_WASTE_AREA;
                break;
            default:
                surface = SURFACE_OUT_OF_BOUNDS;
                weight = 0.0f;
                break;
        }

        surfaces[neighborIndex] = surface;
        weights[neighborIndex] = weight;
    }

    return surfaces[0];
}

f32 ConvertToFloat(s32 value) {
    return value;
}

Surface FindSurface(s32 x, s32 z, Boolean lowersWaterLevel) {
    s8 surfaces[NEAREST_CODE_COUNT];
    f32 weights[NEAREST_CODE_COUNT];
    PlaneTriangle hit;
    f32 fraction;
    s32 waterBias;
    s32 neighborIndex;
    s32 column;
    s32 row;
    s32 fractionX;
    s32 fractionZ;
    s32 stepColumn;
    s32 stepRow;
    s32 distanceX;
    s32 distanceZ;
    s32 stepDifference;
    f32 weight;
    Surface surface;
    s32 bestIndex;
    s32 weightIndex;
    f32 bestWeight;

    if (g_GameState.isMiniGolf) {
        return GetMiniGolfSurface(x, z, lowersWaterLevel);
    }

    if (IsInSurfaceEdgeMargin(x, GROUND_WIDTH)) {
        return SURFACE_OUT_OF_COURSE;
    }

    if (IsInSurfaceEdgeMargin(z, GROUND_LENGTH)) {
        return SURFACE_OUT_OF_COURSE;
    }

    if (FindHoleTriangleHit(x, -WATER_PROBE_REACH, z, x, WATER_PROBE_REACH, z, &hit, &fraction)) {
        char unusedText[0x80] UNUSED;

        waterBias = WATER_BIAS;
        if (lowersWaterLevel) {
            waterBias = LOWERED_WATER_BIAS;
        }

        if ((s32)(WaterProbeHeight(fraction, WATER_PROBE_REACH) + waterBias) <
            GetTerrainHeight(x, z)) {
            return SURFACE_WATER_HAZARD;
        }
    }

    column = x / GROUND_TEXEL_SIZE;
    row = z / GROUND_TEXEL_SIZE;
    fractionX = x & (GROUND_TEXEL_SIZE - 1);
    stepColumn = (fractionX < GROUND_TEXEL_SIZE / 2) ? -1 : 1;
    fractionZ = z & (GROUND_TEXEL_SIZE - 1);
    stepRow = (fractionZ < GROUND_TEXEL_SIZE / 2) ? -1 : 1;
    for (neighborIndex = 0; neighborIndex < NEAREST_CODE_COUNT; neighborIndex++) {
        switch (GetTerrainCode(
            (neighborIndex & 1) ? column + stepColumn : column,
            (neighborIndex & 2) ? row + stepRow : row
        )) {
            case TERRAIN_CODE_FAIRWAY:
                weight = 1.0f;
                surface = SURFACE_FAIRWAY;
                break;
            case TERRAIN_CODE_GREEN:
                weight = GREEN_SURFACE_WEIGHT;
                surface = SURFACE_GREEN;
                break;
            case TERRAIN_CODE_BUNKER:
                weight = BUNKER_SURFACE_WEIGHT;
                surface = SURFACE_BUNKER;
                break;
            case TERRAIN_CODE_ROUGH:
                weight = ROUGH_SURFACE_WEIGHT;
                surface = SURFACE_ROUGH;
                break;
            case TERRAIN_CODE_OUT_OF_BOUNDS:
                weight = OUT_OF_BOUNDS_SURFACE_WEIGHT;
                surface = SURFACE_OUT_OF_BOUNDS;
                break;
            case TERRAIN_CODE_CART_WAY:
                weight = 1.0f;
                surface = SURFACE_CART_WAY;
                break;
            case TERRAIN_CODE_TEE_GROUND:
                weight = 1.0f;
                surface = SURFACE_TEE_GROUND;
                break;
            case TERRAIN_CODE_ROCK:
                weight = 1.0f;
                surface = SURFACE_ROCK;
                break;
            case TERRAIN_CODE_GREEN_EDGE:
                weight = 1.0f;
                surface = SURFACE_GREEN_EDGE;
                break;
            case TERRAIN_CODE_SEMI_ROUGH:
                weight = 1.0f;
                surface = SURFACE_SEMI_ROUGH;
                break;
            case TERRAIN_CODE_BARE_GROUND:
                weight = 1.0f;
                surface = SURFACE_BARE_GROUND;
                break;
            case TERRAIN_CODE_NO_DROP_GROUND:
                weight = 1.0f;
                surface = SURFACE_NO_DROP_GROUND;
                break;
            case TERRAIN_CODE_DEEP_ROUGH:
                weight = ROUGH_SURFACE_WEIGHT;
                surface = SURFACE_DEEP_ROUGH;
                break;
            case TERRAIN_CODE_WASTE_AREA:
                weight = ROUGH_SURFACE_WEIGHT;
                surface = SURFACE_WASTE_AREA;
                break;
            default:
                surface = SURFACE_OUT_OF_BOUNDS;
                weight = 0.0f;
                break;
        }

        surfaces[neighborIndex] = surface;
        weights[neighborIndex] = weight;
    }

    for (neighborIndex = 1; neighborIndex < NEAREST_CODE_COUNT; neighborIndex++) {
        if (surfaces[neighborIndex] != surfaces[0]) {
            break;
        }
    }

    if (neighborIndex == NEAREST_CODE_COUNT) {
        return surface;
    }

    distanceX = Abs(GROUND_TEXEL_SIZE / 2 - fractionX);
    distanceZ = Abs(GROUND_TEXEL_SIZE / 2 - fractionZ);
    stepDifference = (stepColumn + 1) ^ (stepRow + 1);
    if ((stepDifference < 0 && stepDifference > 0) || stepDifference == 0) {
        weights[0] = weights[0] *
            ((1.0f / ((f32)GROUND_TEXEL_SIZE * GROUND_TEXEL_SIZE)) *
                ((f32)(GROUND_TEXEL_SIZE - distanceX) * (f32)(GROUND_TEXEL_SIZE - distanceZ)));
        weights[1] = weights[1] *
            ((1.0f / ((f32)GROUND_TEXEL_SIZE * GROUND_TEXEL_SIZE)) *
                ((f32)distanceX * (f32)(GROUND_TEXEL_SIZE - distanceZ)));
        weights[2] = weights[2] * ((1.0f / GROUND_TEXEL_SIZE) * (f32)distanceZ);
    }
    else if (distanceX < distanceZ) {
        surfaces[1] = surfaces[3];
        weights[0] = weights[0] *
            ((1.0f / ((f32)GROUND_TEXEL_SIZE * GROUND_TEXEL_SIZE)) *
                ((f32)(GROUND_TEXEL_SIZE - distanceZ) * (f32)(GROUND_TEXEL_SIZE - distanceX)));
        weights[2] = weights[2] *
            ((1.0f / ((f32)GROUND_TEXEL_SIZE * GROUND_TEXEL_SIZE)) *
                ((f32)distanceZ * (f32)(GROUND_TEXEL_SIZE - distanceX)));
        weights[1] = weights[3] * (f32)distanceX * (1.0f / GROUND_TEXEL_SIZE);
    }
    else {
        surfaces[2] = surfaces[3];
        weights[0] = weights[0] *
            ((1.0f / ((f32)GROUND_TEXEL_SIZE * GROUND_TEXEL_SIZE)) *
                ((f32)(GROUND_TEXEL_SIZE - distanceX) * (f32)(GROUND_TEXEL_SIZE - distanceZ)));
        weights[1] = weights[1] *
            ((1.0f / ((f32)GROUND_TEXEL_SIZE * GROUND_TEXEL_SIZE)) *
                ((f32)distanceX * (f32)(GROUND_TEXEL_SIZE - distanceZ)));
        weights[2] = weights[3] * (f32)distanceZ * (1.0f / GROUND_TEXEL_SIZE);
    }

    if (surfaces[0] == surfaces[1]) {
        weights[0] += weights[1];
        weights[1] = 0.0f;
    }

    if (surfaces[0] == surfaces[2]) {
        weights[0] += weights[2];
        weights[2] = 0.0f;
    }

    bestIndex = 0;
    if (surfaces[1] == surfaces[2]) {
        weights[1] += weights[2];
        weights[2] = 0.0f;
    }

    bestWeight = 0.0f;
    for (weightIndex = 0; weightIndex < NEAREST_CODE_COUNT - 1; weightIndex++) {
        if (bestWeight < weights[weightIndex]) {
            bestIndex = weightIndex;
            bestWeight = weights[weightIndex];
        }
    }

    return surfaces[bestIndex];
}

Surface GetSurface(s32 x, s32 z) {
    return FindSurface(x, z, FALSE);
}

SurfaceTraits *GetSurfaceTraitsAt(s32 x, s32 z) {
    return GetSurfaceTraits(GetSurface(x, z));
}

u16 GetMapColor(s32 x, s32 z) {
    u32 column;
    u32 row;

    if ((u32)x >= GROUND_WIDTH) {
        return 0;
    }

    if ((u32)z >= GROUND_LENGTH) {
        return 0;
    }

    column = x / GROUND_TEXEL_SIZE;
    row = z / GROUND_TEXEL_SIZE;
    return MAP_TEXEL(column, row) | 1;
}

s32 BlendSurfaceMapColor(s32 x, s32 z, Surface surface) {
    u16 groundColor = GetMapColor(x, z);
    TerrainCode code = TERRAIN_CODE_NONE;
    s32 groundRed;
    s32 groundGreen;
    s32 groundBlue;
    s32 surfaceRed;
    s32 surfaceGreen;
    s32 surfaceBlue;
    u16 surfaceColor;

    switch (GetSurfaceTraits(surface)->kind) {
        case SURFACE_KIND_FAIRWAY:
        case SURFACE_KIND_FAIRWAY_FAIR_LIE:
        case SURFACE_KIND_FAIRWAY_POOR_LIE:
        case SURFACE_KIND_SEMI_ROUGH:
            code = TERRAIN_CODE_FAIRWAY;
            break;
        case SURFACE_KIND_ROUGH:
        case SURFACE_KIND_ROUGH_POOR_LIE:
            code = TERRAIN_CODE_ROUGH;
            break;
        case SURFACE_KIND_BUNKER:
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
        case SURFACE_KIND_BUNKER_UNUSED_LIE:
            code = TERRAIN_CODE_BUNKER;
            break;
        case SURFACE_KIND_DEEP_ROUGH:
            code = TERRAIN_CODE_DEEP_ROUGH;
            break;
        case SURFACE_KIND_OUT_OF_BOUNDS:
            code = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
        case SURFACE_KIND_GREEN:
        case SURFACE_KIND_GREEN_EDGE:
            code = TERRAIN_CODE_GREEN;
            break;
        case SURFACE_KIND_ROCK:
            code = TERRAIN_CODE_ROCK;
            break;
        case SURFACE_KIND_BARE_GROUND:
        case SURFACE_KIND_NO_DROP_GROUND:
            code = TERRAIN_CODE_BARE_GROUND;
            break;
        case SURFACE_KIND_TEE_GROUND:
            code = TERRAIN_CODE_TEE_GROUND;
            break;
        case SURFACE_KIND_CART_WAY:
            code = TERRAIN_CODE_CART_WAY;
            break;
    }

    if (code > TERRAIN_CODE_NONE) {
        groundRed = groundColor >> RGBA16_RED_SHIFT;
        groundGreen = (groundColor >> RGBA16_GREEN_SHIFT) & RGBA16_CHANNEL_MASK;
        surfaceColor = g_TerrainCodeColors[code];

        groundBlue = (surfaceColor >> RGBA16_BLUE_SHIFT) & RGBA16_CHANNEL_MASK;
#ifdef AVOID_UB
        surfaceBlue = groundBlue;
#endif
        surfaceRed = surfaceColor >> RGBA16_RED_SHIFT;
        surfaceGreen = (surfaceColor >> RGBA16_GREEN_SHIFT) & RGBA16_CHANNEL_MASK;
        osSyncPrintf(
            "c1(%d,%d,%d) c2(%d,%d,%d)\n",
            groundRed,
            groundGreen,
            groundBlue,
            surfaceRed,
            surfaceGreen,
            surfaceBlue
        );
        groundRed = (groundRed + surfaceRed) >> 1;
        groundGreen = (groundGreen + surfaceGreen) >> 1;
        groundBlue = (groundBlue + surfaceBlue) / 2;
        return (groundRed << RGBA16_RED_SHIFT) | (groundGreen << RGBA16_GREEN_SHIFT) |
            (groundBlue << RGBA16_BLUE_SHIFT) | 1;
    }

    return groundColor;
}

Boolean IsInWater(s32 x, s32 y UNUSED, s32 z) {
    return GetSurface(x, z) == SURFACE_WATER_HAZARD;
}

void SetVertexMidpoint(Vtx *first, Vtx *second, Vtx *midpoint) {
    midpoint->v.ob[AXIS_X] = (first->v.ob[AXIS_X] + second->v.ob[AXIS_X]) / 2;
    midpoint->v.ob[AXIS_Y] = (first->v.ob[AXIS_Y] + second->v.ob[AXIS_Y]) / 2;
    midpoint->v.ob[AXIS_Z] = (first->v.ob[AXIS_Z] + second->v.ob[AXIS_Z]) / 2;
    midpoint->v.flag = 0;
    midpoint->v.tc[TEXTURE_AXIS_S] =
        (first->v.tc[TEXTURE_AXIS_S] + second->v.tc[TEXTURE_AXIS_S]) / 2;
    midpoint->v.tc[TEXTURE_AXIS_T] =
        (first->v.tc[TEXTURE_AXIS_T] + second->v.tc[TEXTURE_AXIS_T]) / 2;
    midpoint->v.cn[RED_CHANNEL] = (first->v.cn[RED_CHANNEL] + second->v.cn[RED_CHANNEL]) / 2;
    midpoint->v.cn[GREEN_CHANNEL] = (first->v.cn[GREEN_CHANNEL] + second->v.cn[GREEN_CHANNEL]) / 2;
    midpoint->v.cn[BLUE_CHANNEL] = (first->v.cn[BLUE_CHANNEL] + second->v.cn[BLUE_CHANNEL]) / 2;
    midpoint->v.cn[ALPHA_CHANNEL] = (first->v.cn[ALPHA_CHANNEL] + second->v.cn[ALPHA_CHANNEL]) / 2;
}

void BlendVertices(Vtx *first, Vtx *second, Vtx *result, s32 weight) {
    s32 firstWeight = EDGE_WEIGHT_MAX - weight;

    result->v.ob[AXIS_X] =
        ((first->v.ob[AXIS_X] * firstWeight) + (second->v.ob[AXIS_X] * weight)) / EDGE_WEIGHT_MAX;
    result->v.ob[AXIS_Y] =
        ((first->v.ob[AXIS_Y] * firstWeight) + (second->v.ob[AXIS_Y] * weight)) / EDGE_WEIGHT_MAX;
    result->v.ob[AXIS_Z] =
        ((first->v.ob[AXIS_Z] * firstWeight) + (second->v.ob[AXIS_Z] * weight)) / EDGE_WEIGHT_MAX;
    result->v.flag = 0;
    result->v.tc[TEXTURE_AXIS_S] =
        ((first->v.tc[TEXTURE_AXIS_S] * firstWeight) + (second->v.tc[TEXTURE_AXIS_S] * weight)) /
        EDGE_WEIGHT_MAX;
    result->v.tc[TEXTURE_AXIS_T] =
        ((first->v.tc[TEXTURE_AXIS_T] * firstWeight) + (second->v.tc[TEXTURE_AXIS_T] * weight)) /
        EDGE_WEIGHT_MAX;
    result->v.cn[RED_CHANNEL] =
        ((first->v.cn[RED_CHANNEL] * firstWeight) + (second->v.cn[RED_CHANNEL] * weight)) /
        EDGE_WEIGHT_MAX;
    result->v.cn[GREEN_CHANNEL] =
        ((first->v.cn[GREEN_CHANNEL] * firstWeight) + (second->v.cn[GREEN_CHANNEL] * weight)) /
        EDGE_WEIGHT_MAX;
    result->v.cn[BLUE_CHANNEL] =
        ((first->v.cn[BLUE_CHANNEL] * firstWeight) + (second->v.cn[BLUE_CHANNEL] * weight)) /
        EDGE_WEIGHT_MAX;
    result->v.cn[ALPHA_CHANNEL] =
        ((first->v.cn[ALPHA_CHANNEL] * firstWeight) + (second->v.cn[ALPHA_CHANNEL] * weight)) /
        EDGE_WEIGHT_MAX;
}

Vtx *GetGridVertex(u32 column, u32 row) {
    Vtx *vertex;

    if (column >= GROUND_GRID_VERTEX_COLUMN_COUNT || row >= GROUND_GRID_VERTEX_ROW_COUNT) {
        vertex = &g_WorkArea.hole.gridVertices[0];
    }
    else {
        vertex = &g_WorkArea.hole.gridVertices
                      [(row * GROUND_GRID_COLUMN_COUNT) + (column >> 1) +
                          ((column & 1) * GROUND_GRID_EVEN_COLUMN_COUNT)];
    }

    return vertex;
}

s16 GetGridVertexHeight(s32 column, s32 row) {
    return GetGridVertex(column, row)->v.ob[AXIS_Y];
}

#define GRID_VERTEX_WORLD_SHIFT ((GROUND_TILE_SHIFT - WORLD_TO_COURSE_SHIFT) - 1)
#define GRID_VERTEX_TEXEL_SPAN 32
#define TEXTURE_COORDINATE_MAX 0x7FFF

void InitGridVertex(s32 column, s32 row, s32 height, u8 shade) {
    Vtx *vertex = GetGridVertex(column, row);

    vertex->v.ob[AXIS_X] = column << GRID_VERTEX_WORLD_SHIFT;
    vertex->v.ob[AXIS_Z] = row << GRID_VERTEX_WORLD_SHIFT;
    vertex->v.ob[AXIS_Y] = height & ~1;
    vertex->v.flag = 0;
    vertex->v.tc[TEXTURE_AXIS_S] =
        TexelCoordinate(column * GRID_VERTEX_TEXEL_SPAN) + TexelCoordinate(1);
    vertex->v.tc[TEXTURE_AXIS_T] =
        TexelCoordinate(row * GRID_VERTEX_TEXEL_SPAN) + TexelCoordinate(1);
    if (row == GROUND_GRID_ROW_COUNT - 1) {
        vertex->v.tc[TEXTURE_AXIS_T] = TEXTURE_COORDINATE_MAX;
    }

    vertex->v.cn[RED_CHANNEL] = shade;
    vertex->v.cn[GREEN_CHANNEL] = shade;
    vertex->v.cn[BLUE_CHANNEL] = shade;
    vertex->v.cn[ALPHA_CHANNEL] = OPAQUE_ALPHA;
}

void SetGridVertexHeight(s32 column, s32 row, s32 height) {
    GetGridVertex(column, row)->v.ob[AXIS_Y] = height & ~1;
}

void SetGridVertexShade(s32 column, s32 row, s8 shade) {
    Vtx *vertex = GetGridVertex(column, row);

    vertex->v.cn[RED_CHANNEL] = shade;
    vertex->v.cn[GREEN_CHANNEL] = shade;
    vertex->v.cn[BLUE_CHANNEL] = shade;
}

void SetGridVertex(s32 column, s32 row, Vtx_t *point) {
    GetGridVertex(column, row)->v = *point;
}

Vtx *GetGroundPoint(u32 column, u32 row) {
    u32 tileRow;
    u32 tileColumn;
    s32 slotRow;
    s32 slotColumn;
    s32 tileIndex;
    Vtx *midpoints;
    Vtx *point;

    if ((column | row) & 1) {
        tileRow = PointTile(row);
        if (row == GROUND_POINT_ROW_COUNT - 1) {
            tileRow = GROUND_TILE_ROW_COUNT - 1;
            slotRow = GROUND_TILE_POINT_SPAN;
        }
        else {
            slotRow = PointSlot(row);
        }

        if (column == GROUND_POINT_COLUMN_COUNT - 1) {
            tileColumn = GROUND_TILE_COLUMN_COUNT - 1;
            slotColumn = GROUND_TILE_POINT_SPAN;
        }
        else {
            tileColumn = PointTile(column);
            slotColumn = PointSlot(column);
        }

        tileIndex = TileIndex(tileColumn, tileRow);
        midpoints = g_WorkArea.hole.midpoints[tileIndex];
        point = &midpoints[f_PointSlots[slotRow][slotColumn]];
    }
    else {
        point = GetGridVertex(column >> 1, row >> 1);
    }

    return point;
}

NO_RESULT SetGroundPoint(s32 column, s32 row, Vtx_t *point) {
    s32 blockIndex;
    s32 tileIndex;
    Vtx *midpoint;
    Vtx *midpoints;

    if ((column | row) & 1) {
        if (row < GROUND_POINT_ROW_COUNT - 1 && column < GROUND_POINT_COLUMN_COUNT - 1) {
            tileIndex = TileIndex(PointTile(column), PointTile(row));
            blockIndex = g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex;
            if (blockIndex >= 0) {
                midpoints = g_WorkArea.hole.midpoints[blockIndex];
                midpoint = &midpoints[f_PointSlots[PointSlot(row)][PointSlot(column)]];
                midpoint->v = *point;
            }
        }

        if (IsSharedWithPreviousTileRow(column, row)) {
            tileIndex = TileIndex(PointTile(column), PointTile(row) - 1);
            blockIndex = g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex;
            if (blockIndex >= 0) {
                midpoints = g_WorkArea.hole.midpoints[blockIndex];
                midpoint = &midpoints[f_PointSlots[GROUND_TILE_POINT_SPAN][PointSlot(column)]];
                midpoint->v = *point;
            }
        }

        if (IsSharedWithPreviousTileColumn(column, row)) {
            tileIndex = TileIndex(PointTile(column) - 1, PointTile(row));
            blockIndex = g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex;
            if (blockIndex >= 0) {
                midpoints = g_WorkArea.hole.midpoints[blockIndex];
                midpoint = &midpoints[f_PointSlots[PointSlot(row)][GROUND_TILE_POINT_SPAN]];
                midpoint->v = *point;
            }
        }
    }
    else {
        SetGridVertex(column / 2, row / 2, point);
    }
}

NO_RESULT SetGroundPointHeightAndShade(s32 column, s32 row, s32 height, s32 shade) {
    s32 blockIndex;
    s32 tileIndex;
    Vtx *point;
    Vtx *midpoints;

    if ((column | row) & 1) {
        if (row < GROUND_POINT_ROW_COUNT - 1 && column < GROUND_POINT_COLUMN_COUNT - 1) {
            tileIndex = TileIndex(PointTile(column), PointTile(row));
            blockIndex = g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex;
            if (blockIndex >= 0) {
                midpoints = g_WorkArea.hole.midpoints[blockIndex];
                point = &midpoints[f_PointSlots[PointSlot(row)][PointSlot(column)]];
                point->v.ob[AXIS_Y] = height;

                point->v.cn[RED_CHANNEL] = point->v.cn[GREEN_CHANNEL] = point->v.cn[BLUE_CHANNEL] =
                    shade;
            }
        }

        if (IsSharedWithPreviousTileRow(column, row)) {
            tileIndex = TileIndex(PointTile(column), PointTile(row) - 1);
            blockIndex = g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex;
            if (blockIndex >= 0) {
                midpoints = g_WorkArea.hole.midpoints[blockIndex];
                point = &midpoints[f_PointSlots[GROUND_TILE_POINT_SPAN][PointSlot(column)]];
                point->v.ob[AXIS_Y] = height;

                point->v.cn[RED_CHANNEL] = point->v.cn[GREEN_CHANNEL] = point->v.cn[BLUE_CHANNEL] =
                    shade;
            }
        }

        if (IsSharedWithPreviousTileColumn(column, row)) {
            tileIndex = TileIndex(PointTile(column) - 1, PointTile(row));
            blockIndex = g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex;
            if (blockIndex >= 0) {
                midpoints = g_WorkArea.hole.midpoints[blockIndex];
                point = &midpoints[f_PointSlots[PointSlot(row)][GROUND_TILE_POINT_SPAN]];
                point->v.ob[AXIS_Y] = height;

                point->v.cn[RED_CHANNEL] = point->v.cn[GREEN_CHANNEL] = point->v.cn[BLUE_CHANNEL] =
                    shade;
            }
        }
    }
    else {
        SetGridVertexHeight(column / 2, row / 2, height);
    }
}

void InitTerrainTextures(void) {
    GroundTile *tile;
    s32 tileIndex;
    s32 texelRow;
    s32 texelColumn;
    s32 row;
    s32 column;

    for (tileIndex = 0; tileIndex < GROUND_TILE_COUNT; tileIndex++) {
        tile = &g_WorkArea.hole.tiles[tileIndex];
        g_GroundTileHeightRanges[tileIndex] = 1.0f;
        tile->detail = GROUND_DETAIL_MEDIUM;
        tile->midpointBlockIndex = -1;
        tile->extraPassCount = 0;
        for (texelRow = 0; texelRow < GROUND_TILE_TEXEL_SPAN; texelRow++) {
            for (texelColumn = 0; texelColumn < GROUND_TILE_TEXEL_SPAN; texelColumn++) {
                tile->texels[0][(texelRow * GROUND_TILE_TEXEL_SPAN) + texelColumn] = 0x7C1;
            }
        }
    }

    for (row = 0; row < GROUND_GRID_ROW_COUNT; row++) {
        for (column = 0; column < GROUND_GRID_COLUMN_COUNT; column++) {
            InitGridVertex(column, row, 0, 0x80);
        }
    }

    BuildAllTileMidpoints();
}

#define TILE_CORNER_SPAN (GROUND_TILE_GRID_SPAN + 1)
#define CORNER(row, column) corners[((row) * TILE_CORNER_SPAN) + (column)]

void BuildTileMidpoints(Vtx *midpoints, s32 tileColumn, s32 tileRow) {
    Vtx *corners[TILE_CORNER_SPAN * TILE_CORNER_SPAN];
    s32 cornerIndex = 0;
    s32 row;
    s32 column;

    for (row = 0; row < TILE_CORNER_SPAN; row++) {
        for (column = 0; column < TILE_CORNER_SPAN; column++) {
            corners[cornerIndex++] = TileGridVertex(tileColumn, tileRow, column, row);
        }
    }

    SetVertexMidpoint(CORNER(0, 0), CORNER(0, 1), &midpoints[TILE_MIDPOINT_ROW0_COLUMN1]);
    SetVertexMidpoint(CORNER(0, 1), CORNER(0, 2), &midpoints[TILE_MIDPOINT_ROW0_COLUMN3]);
    SetVertexMidpoint(CORNER(0, 0), CORNER(1, 0), &midpoints[TILE_MIDPOINT_ROW1_COLUMN0]);
    SetVertexMidpoint(CORNER(0, 1), CORNER(1, 1), &midpoints[TILE_MIDPOINT_ROW1_COLUMN2]);
    SetVertexMidpoint(CORNER(0, 2), CORNER(1, 2), &midpoints[TILE_MIDPOINT_ROW1_COLUMN4]);
    SetVertexMidpoint(CORNER(1, 0), CORNER(1, 1), &midpoints[TILE_MIDPOINT_ROW2_COLUMN1]);
    SetVertexMidpoint(CORNER(1, 1), CORNER(1, 2), &midpoints[TILE_MIDPOINT_ROW2_COLUMN3]);
    SetVertexMidpoint(CORNER(1, 0), CORNER(2, 0), &midpoints[TILE_MIDPOINT_ROW3_COLUMN0]);
    SetVertexMidpoint(CORNER(1, 1), CORNER(2, 1), &midpoints[TILE_MIDPOINT_ROW3_COLUMN2]);
    SetVertexMidpoint(CORNER(1, 2), CORNER(2, 2), &midpoints[TILE_MIDPOINT_ROW3_COLUMN4]);
    SetVertexMidpoint(CORNER(2, 0), CORNER(2, 1), &midpoints[TILE_MIDPOINT_ROW4_COLUMN1]);
    SetVertexMidpoint(CORNER(2, 1), CORNER(2, 2), &midpoints[TILE_MIDPOINT_ROW4_COLUMN3]);
    SetVertexMidpoint(CORNER(0, 1), CORNER(1, 0), &midpoints[TILE_MIDPOINT_ROW1_COLUMN1]);
    SetVertexMidpoint(CORNER(0, 2), CORNER(1, 1), &midpoints[TILE_MIDPOINT_ROW1_COLUMN3]);
    SetVertexMidpoint(CORNER(1, 1), CORNER(2, 0), &midpoints[TILE_MIDPOINT_ROW3_COLUMN1]);
    SetVertexMidpoint(CORNER(1, 2), CORNER(2, 1), &midpoints[TILE_MIDPOINT_ROW3_COLUMN3]);
}

void BuildAllTileMidpoints(void) {
    s32 tileIndex = 0;
    s32 tileRow;
    s32 tileColumn;
    Vtx *midpoints;
    Vtx *center;

    for (tileRow = 0; tileRow < GROUND_TILE_ROW_COUNT; tileRow++) {
        for (tileColumn = 0; tileColumn < GROUND_TILE_COLUMN_COUNT; tileColumn++) {
            g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex = tileIndex;
            midpoints = g_WorkArea.hole.midpoints[tileIndex];
            center = TileGridVertex(tileColumn, tileRow, 1, 1);
            g_WorkArea.hole.tiles[tileIndex].center[AXIS_X] = center->v.ob[AXIS_X];
            g_WorkArea.hole.tiles[tileIndex].center[AXIS_Y] = center->v.ob[AXIS_Y];
            g_WorkArea.hole.tiles[tileIndex].center[AXIS_Z] = center->v.ob[AXIS_Z];
            if (g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex >= 0) {
                BuildTileMidpoints(midpoints, tileColumn, tileRow);
            }

            tileIndex++;
        }
    }
}

void UpdateFlagPanel(void) {
    char text[FLAG_PANEL_TEXT_SIZE];
    s32 row;
    s32 column;
    s32 textColumn;

    s32 GetSelectedFlag(void) {
        return (f_FlagPanel.firstRow + f_FlagPanel.row) * FLAG_PANEL_COLUMN_COUNT +
            f_FlagPanel.column;
    }

    ClearDebugText();
    PrintDebugText("ROM :", BUILD_DATE_LABEL_COLUMN, ROM_BUILD_DATE_ROW);
    PrintDebugText(g_RomBuildDate, BUILD_DATE_COLUMN, ROM_BUILD_DATE_ROW);
    PrintDebugText("SRAM:", BUILD_DATE_LABEL_COLUMN, SAVE_BUILD_DATE_ROW);
    PrintDebugText(g_SaveBuildDate, BUILD_DATE_COLUMN, SAVE_BUILD_DATE_ROW);
    for (row = 0; row < FLAG_PANEL_ROW_COUNT; row++) {
        sprintf(text, "%x0|", (unsigned int)(row + f_FlagPanel.firstRow));
        PrintDebugText(text, FLAG_ROW_LABEL_COLUMN, row + FLAG_PANEL_FIRST_TEXT_ROW);
        for (column = 0, textColumn = FIRST_FLAG_TEXT_COLUMN; column < FLAG_PANEL_COLUMN_COUNT;
            column++, textColumn += FLAG_TEXT_SPACING) {
            if (row == 0) {
                sprintf(text, "%X", (unsigned int)column);
                PrintDebugText(text, textColumn, FLAG_PANEL_HEADER_ROW);
            }

            sprintf(
                text,
                "%X",
                (unsigned int)(IsGameFlagSet(
                    ((row + f_FlagPanel.firstRow) * FLAG_PANEL_COLUMN_COUNT) + column
                ))
            );
            if (row == f_FlagPanel.row && column == f_FlagPanel.column) {
                SetDebugTextHighlight(TRUE);
            }

            PrintDebugText(text, textColumn, row + FLAG_PANEL_FIRST_TEXT_ROW);
        }
    }

    if (g_MenuControllerData[0].button & R_JPAD) {
        f_FlagPanel.column++;
    }

    if (g_MenuControllerData[0].button & L_JPAD) {
        f_FlagPanel.column--;
    }

    if (f_FlagPanel.column < 0) {
        f_FlagPanel.column = FLAG_PANEL_COLUMN_COUNT - 1;
        f_FlagPanel.row--;
    }

    if (f_FlagPanel.column >= FLAG_PANEL_COLUMN_COUNT) {
        f_FlagPanel.column = 0;
        f_FlagPanel.row++;
    }

    if (g_MenuControllerData[0].button & U_JPAD) {
        f_FlagPanel.row--;
    }

    if (g_MenuControllerData[0].button & D_JPAD) {
        f_FlagPanel.row++;
    }

    if (g_MenuControllerData[0].button & R_TRIG) {
        f_FlagPanel.firstRow += FLAG_PANEL_ROW_COUNT;
    }

    if (g_MenuControllerData[0].button & L_TRIG) {
        f_FlagPanel.firstRow -= FLAG_PANEL_ROW_COUNT;
    }
    while (TRUE) {
        if (f_FlagPanel.row < 0) {
            f_FlagPanel.row++;
            f_FlagPanel.firstRow--;
        }
        else if (f_FlagPanel.row >= FLAG_PANEL_ROW_COUNT) {
            f_FlagPanel.row--;
            f_FlagPanel.firstRow++;
        }
        else {
            break;
        }
    }

    if (f_FlagPanel.firstRow > FLAG_PANEL_FIRST_ROW_MAX) {
        f_FlagPanel.firstRow = FLAG_PANEL_FIRST_ROW_MAX;
    }

    if (f_FlagPanel.firstRow < 0) {
        f_FlagPanel.firstRow = 0;
    }

    if (g_MenuControllerData[0].button & A_BUTTON) {
        SetGameFlag(GetSelectedFlag());
    }

    if (g_MenuControllerData[0].button & B_BUTTON) {
        ClearGameFlag(GetSelectedFlag());
    }
}

const char *GetDebugSceneLabel(Scene scene) {
    switch (scene) {
        case SCENE_FLAG_PANEL:
            return "FLAGS ";
        case SCENE_MAP_OVERVIEW:
            return "ABOUT ";
        case SCENE_GROUND_CAMERA:
            return " VIEW ";
        case SCENE_PLAY_HOLE:
            return " PLAY ";
        case SCENE_HEIGHTS:
            return "HEIGHT";
        case SCENE_TREES:
            return " TREE ";
        case SCENE_PIN:
            return " PIN  ";
        case SCENE_CELL_EDITOR:
            return "DIVINF";
        case SCENE_TRIANGLES:
            return "TRIANG";
        case SCENE_MENU:
            return "MENU";
        case SCENE_POINT_EDITOR:
            return "XZ-DIV";
        case SCENE_HOLE_FILES:
            return " FILE ";
        case SCENE_PLAY_ROUND:
            return "STROKE";
        case SCENE_PLAY_BEST_SHOT_REPLAY:
            return "S-SHOT";
        case SCENE_RINGS:
            return "RING  ";
        case SCENE_WALLS:
            return "WALL  ";
        default:
            return "      ";
    }
}

NO_RESULT UpdateDebugSceneMenu(void) {
    u32 selectedIndex;
    u32 entryIndex;
    s32 row;

    g_CurrentShot.surface = SURFACE_OUT_OF_BOUNDS;
    ClearGameFlag(GAME_FLAG_HOLE_LOADED);
    g_ShowsGroundWaves = FALSE;
    if (g_MenuControllerData[0].button & START_BUTTON) {
        ClearDebugText();
        f_IsSceneMenuOpen = FALSE;
        f_SceneMenuBox.state = BOX_CLOSING;
        f_SceneMenuBox.frame = 0;
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        return;
    }

    for (selectedIndex = 0; selectedIndex < SCENE_MENU_LENGTH; selectedIndex++) {
        if (f_SceneMenuScenes[selectedIndex] == f_SceneMenuSelection) {
            break;
        }
    }

    for (entryIndex = 0, row = SCENE_MENU_TOP_ROW; entryIndex < SCENE_MENU_LENGTH;
        entryIndex++, row++) {
        if (entryIndex == selectedIndex) {
            SetDebugTextHighlight(TRUE);
            PrintDebugText("->", SCENE_MENU_CURSOR_COLUMN, selectedIndex + SCENE_MENU_TOP_ROW);
        }
        else {
            PrintDebugText("  ", SCENE_MENU_CURSOR_COLUMN, row);
        }

        PrintDebugText(
            GetDebugSceneLabel(f_SceneMenuScenes[entryIndex]),
            SCENE_MENU_LABEL_COLUMN,
            row
        );
    }

    if (g_MenuControllerData[0].button & U_JPAD) {
        selectedIndex = (selectedIndex + SCENE_MENU_LENGTH - 1) % SCENE_MENU_LENGTH;
        f_SceneMenuSelection = f_SceneMenuScenes[selectedIndex];
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    }

    if (g_MenuControllerData[0].button & D_JPAD) {
        selectedIndex = (selectedIndex + SCENE_MENU_LENGTH + 1) % SCENE_MENU_LENGTH;
        f_SceneMenuSelection = f_SceneMenuScenes[selectedIndex];
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    }

    if (g_MenuControllerData[0].button & A_BUTTON) {
        ClearDebugText();
        f_IsSceneMenuOpen = FALSE;
        f_SceneMenuBox.state = BOX_CLOSING;
        f_SceneMenuBox.frame = 0;
        g_Scene = f_SceneMenuSelection;
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        if (f_SceneMenuSelection == SCENE_MENU) {
            ResetMenuChoices();
        }
        else {
            PlaySong(SONG_HOLE_OUT_BIRDIE, 0);
        }

        switch ((u32)g_Scene) {
            case SCENE_PLAY_HOLE:
                InitPlay(PLAY_KIND_HOLE);
                ChooseConditionsAndInitHole();
                InitRoundHud();
                break;
            case SCENE_CELL_EDITOR:
                break;
            case SCENE_PLAY_ROUND:
                InitPlay(PLAY_KIND_FLAGGED_ROUND);
                InitRoundHud();
                break;
            case SCENE_PLAY_BEST_SHOT_REPLAY:
                InitPlay(PLAY_KIND_BEST_SHOT_REPLAY);
                InitRoundHud();
                break;
        }
    }
}

#define BEYOND_ANY_COORDINATE 0x20000000

void PrepareHoleTriangles(void) {
    Vtx_t *vertex;
    s32 triangleIndex;
    s32 corner;
    s32 lowest[AXIS_COUNT];
    s32 highest[AXIS_COUNT];

    for (triangleIndex = 0; triangleIndex < HOLE_TRIANGLE_COUNT; triangleIndex++) {
        lowest[AXIS_Z] = BEYOND_ANY_COORDINATE;
        lowest[AXIS_Y] = lowest[AXIS_Z];
        lowest[AXIS_X] = lowest[AXIS_Y];
        highest[AXIS_Z] = -BEYOND_ANY_COORDINATE;
        highest[AXIS_Y] = highest[AXIS_Z];
        highest[AXIS_X] = highest[AXIS_Y];
        for (corner = 0; corner < TRIANGLE_VERTEX_COUNT; corner++) {
            vertex = &g_WorkArea.hole.triangles[triangleIndex].vertices[corner].v;
            if (vertex->ob[AXIS_X] < lowest[AXIS_X]) {
                lowest[AXIS_X] = vertex->ob[AXIS_X];
            }

            if (vertex->ob[AXIS_X] > highest[AXIS_X]) {
                highest[AXIS_X] = vertex->ob[AXIS_X];
            }

            if (vertex->ob[AXIS_Y] < lowest[AXIS_Y]) {
                lowest[AXIS_Y] = vertex->ob[AXIS_Y];
            }

            if (vertex->ob[AXIS_Y] > highest[AXIS_Y]) {
                highest[AXIS_Y] = vertex->ob[AXIS_Y];
            }

            if (vertex->ob[AXIS_Z] < lowest[AXIS_Z]) {
                lowest[AXIS_Z] = vertex->ob[AXIS_Z];
            }

            if (vertex->ob[AXIS_Z] > highest[AXIS_Z]) {
                highest[AXIS_Z] = vertex->ob[AXIS_Z];
            }

            vertex->tc[TEXTURE_AXIS_S] = vertex->ob[AXIS_X];
            vertex->tc[TEXTURE_AXIS_T] = vertex->ob[AXIS_Z];
        }

        g_WorkArea.hole.triangles[triangleIndex].bounds[AXIS_X][BOUND_LOWEST] = lowest[AXIS_X];
        g_WorkArea.hole.triangles[triangleIndex].bounds[AXIS_Y][BOUND_LOWEST] = lowest[AXIS_Y];
        g_WorkArea.hole.triangles[triangleIndex].bounds[AXIS_Z][BOUND_LOWEST] = lowest[AXIS_Z];
        g_WorkArea.hole.triangles[triangleIndex].bounds[AXIS_X][BOUND_HIGHEST] = highest[AXIS_X];
        g_WorkArea.hole.triangles[triangleIndex].bounds[AXIS_Y][BOUND_HIGHEST] = highest[AXIS_Y];
        g_WorkArea.hole.triangles[triangleIndex].bounds[AXIS_Z][BOUND_HIGHEST] = highest[AXIS_Z];
    }

    InitRings();
}

Boolean IsOverlayInExpansionMemory(void) {
    return (u32)BuildHole > BASE_RDRAM_END - 1;
}

#define GROUND_TEXEL_WORLD_SIZE (GROUND_TEXEL_SIZE >> WORLD_TO_COURSE_SHIFT)

void MarkBillboardsOnMap(void) {
    s32 billboardIndex;
    u16 color = 0xF801;
    u32 column;
    u32 row;

    for (billboardIndex = 0; billboardIndex < BILLBOARD_COUNT; billboardIndex++) {
        if (g_Billboards[billboardIndex].textureIndex >= 0) {
            column = g_Billboards[billboardIndex].x / GROUND_TEXEL_WORLD_SIZE;
            row = g_Billboards[billboardIndex].z / GROUND_TEXEL_WORLD_SIZE;
            MAP_TEXEL(column, row) = color;
        }
    }
}

#define IsSceneMenuRequested() \
    (g_IsDebugEnabled && \
        (g_MenuControllerData[0].button & START_BUTTON) && \
        ((g_ControllerData[0].button & (START_BUTTON | L_TRIG)) == (START_BUTTON | L_TRIG)))

void UpdateDebugScenes(void) {
    AnimatedBoxState openState = BOX_OPEN;
    s32 stickStep;

    if (f_IsFirstDebugUpdate) {
        f_IsFirstDebugUpdate = FALSE;
        if (g_ControllerData[0].button & L_TRIG) {
            g_IsDebugEnabled = FALSE;
        }

        g_Scene = SCENE_BOOT_LOGOS;
        return;
    }

    if (IsGameFlagSet(GAME_FLAG_SHOW_SURFACE_NAME)) {
        PrintDebugText(
            GetSurfaceTraitsAt(g_CurrentShot.targetPoint[AXIS_X], g_CurrentShot.targetPoint[AXIS_Z])
                ->name,
            SURFACE_NAME_COLUMN,
            SURFACE_NAME_ROW
        );
    }

    if (f_IsSceneMenuOpen && f_SceneMenuBox.state == openState) {
        UpdateDebugSceneMenu();
        QueueAnimatedBox(&f_SceneMenuBox);
        return;
    }

    if (IsSceneMenuRequested()) {
        if (IsOverlayInExpansionMemory()) {
            f_IsSceneMenuOpen = TRUE;
            ClearGameFlag(GAME_FLAG_HIDE_DEBUG_TEXT);
            FreeAllCharacterSlots();
            f_SceneMenuBox.state = BOX_OPENING;
            f_SceneMenuBox.frame = 0;
            QueueAnimatedBox(&f_SceneMenuBox);
            PlaySound(SOUND_PAUSE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            MarkBillboardsOnMap();
            return;
        }

        if (g_Scene == SCENE_FLAG_PANEL) {
            ClearDebugText();
            g_Scene = f_SceneBeforeFlagPanel;
            InitClubs();
            if (IsGameFlagSet(GAME_FLAG_LOG_GROUND_ROWS)) {
                nuContRmbMgrRemove();
            }
        }
        else {
            f_SceneBeforeFlagPanel = g_Scene;
            g_Scene = SCENE_FLAG_PANEL;
        }

        return;
    }

    QueueAnimatedBox(&f_SceneMenuBox);
    if ((g_Scene != SCENE_PLAY_ROUND) & (g_Scene != SCENE_MENU)) {
        SetDebugTextHighlight(TRUE);
        PrintDebugText(GetDebugSceneLabel(g_Scene), SCENE_LABEL_COLUMN, SCENE_LABEL_ROW);
    }

    if (g_Scene == SCENE_HOLE_FILES) {
        UpdateHoleFileMenu();
    }

    if (g_Scene == SCENE_FLAG_PANEL) {
        UpdateFlagPanel();
    }

    if (g_Scene == SCENE_CELL_EDITOR) {
        UpdateCellEditor();
    }
    else if (g_Scene == SCENE_POINT_EDITOR) {
        UpdateGroundPointEditor();
    }

    if (g_Scene == SCENE_TREES) {
        UpdateTreeEditor();
    }

    if (g_Scene == SCENE_TRIANGLES) {
        UpdateTriangleEditor();
    }

    if (g_Scene == SCENE_RINGS) {
        UpdateRingEditor();
    }

    if (g_Scene == SCENE_WALLS) {
        UpdateWallEditor();
    }

    if (g_Scene == SCENE_MAP_OVERVIEW) {
        if (g_MenuControllerData[0].button & R_JPAD) {
            f_MapOverviewControls.cameraZ -= MAP_OVERVIEW_CAMERA_STEP;
        }

        if (g_MenuControllerData[0].button & L_JPAD) {
            f_MapOverviewControls.cameraZ += MAP_OVERVIEW_CAMERA_STEP;
        }

        if (g_MenuControllerData[0].button & D_JPAD) {
            f_MapOverviewControls.cameraX -= MAP_OVERVIEW_CAMERA_STEP;
        }

        if (g_MenuControllerData[0].button & U_JPAD) {
            f_MapOverviewControls.cameraX += MAP_OVERVIEW_CAMERA_STEP;
        }

        stickStep = g_MenuControllerData[0].stick_y / STICK_STEP_SIZE;
        if (stickStep != 0) {
            f_MapOverviewControls.cameraX += stickStep;
        }

        stickStep = g_MenuControllerData[0].stick_x / STICK_STEP_SIZE;
        if (stickStep != 0) {
            f_MapOverviewControls.cameraZ -= stickStep;
        }

        if (g_MenuControllerData[0].button & L_TRIG) {
            f_MapOverviewControls.adjustment -= MAP_OVERVIEW_ADJUSTMENT_STEP;
            if (f_MapOverviewControls.adjustment < MAP_OVERVIEW_ADJUSTMENT_MIN) {
                f_MapOverviewControls.adjustment = MAP_OVERVIEW_ADJUSTMENT_MIN;
            }
        }

        if (g_MenuControllerData[0].button & R_TRIG) {
            f_MapOverviewControls.adjustment += MAP_OVERVIEW_ADJUSTMENT_STEP;
            if (f_MapOverviewControls.adjustment > MAP_OVERVIEW_ADJUSTMENT_MAX) {
                f_MapOverviewControls.adjustment = MAP_OVERVIEW_ADJUSTMENT_MAX;
            }
        }

        if (g_MenuControllerData[0].button & Z_TRIG) {
            f_MapOverviewControls.isToggled ^= TRUE;
        }

        if (g_MenuControllerData[0].button & A_BUTTON) {
            f_MapOverviewControls.selectionIndex++;
            if (f_MapOverviewControls.selectionIndex >= MAP_OVERVIEW_SELECTION_COUNT) {
                f_MapOverviewControls.selectionIndex = 0;
            }

            f_MapOverviewControls.isToggled = FALSE;
        }

        if (g_MenuControllerData[0].button & B_BUTTON) {
            f_MapOverviewControls.selectionIndex--;
            if (f_MapOverviewControls.selectionIndex < 0) {
                f_MapOverviewControls.selectionIndex = MAP_OVERVIEW_SELECTION_COUNT - 1;
            }

            f_MapOverviewControls.isToggled = FALSE;
        }

        if (g_MenuControllerData[0].trigger & D_CBUTTONS) {
            LoadToolCodeMap();
        }

        if (g_MenuControllerData[0].trigger & R_CBUTTONS) {
            ToggleCodeMapCleanup();
        }

        if (g_MenuControllerData[0].trigger & U_CBUTTONS) {
            g_HoleSetup.conditions.lightPitch = RESET_LIGHT_PITCH;
            g_HoleSetup.conditions.lightYaw = RESET_LIGHT_YAW;
            ShadeGround(1);
        }

        if (g_MenuControllerData[0].trigger & L_CBUTTONS) {
            ToggleGroundTileBorders();
        }

        g_MapCamera.view.eye[AXIS_X] = f_MapOverviewControls.cameraX * MAP_OVERVIEW_CAMERA_SCALE;
        g_MapCamera.view.eye[AXIS_Z] = f_MapOverviewControls.cameraZ * MAP_OVERVIEW_CAMERA_SCALE;
    }

    if (g_Scene == SCENE_HEIGHTS) {
        UpdateHeightEditor();
    }

    if (g_Scene == SCENE_PIN) {
        UpdateHolePointEditor();
    }
}

Vtx *GetTileCellPoint(Vtx *midpoints, s32 tileColumn, s32 tileRow, s32 slot) {
    Vtx *vertex;

    if (slot & GRID_VERTEX_SLOT_FLAG) {
        vertex = TileGridVertex(
            tileColumn,
            tileRow,
            (slot >> SLOT_GRID_COLUMN_SHIFT) & SLOT_GRID_MASK,
            slot & SLOT_GRID_MASK
        );
    }
    else {
        vertex = &midpoints[slot];
    }

    return vertex;
}

s32 CrossProductXZ(s32 originX, s32 originZ, s32 firstX, s32 firstZ, s32 secondX, s32 secondZ) {
    return ((firstX - originX) * (secondZ - originZ)) - ((firstZ - originZ) * (secondX - originX));
}

#define GROUND_ERROR_COLUMN 10
#define GROUND_ERROR_ROW 11

s32 FindGroundTriangle(s32 x, s32 z, PlaneTriangle *triangle) {
    s32 baseColumn;
    s32 baseRow;
    s32 slotColumn;
    s32 slotRow;
    s32 rowSearchIndex;
    s32 columnSearchIndex;
    s32 column;
    s32 row;

    inline Boolean IsOutsideEdge(Vtx * start, Vtx * end) {
        return (end->v.ob[AXIS_X] - start->v.ob[AXIS_X]) *
                (z - (start->v.ob[AXIS_Z] << WORLD_TO_COURSE_SHIFT)) -
            (end->v.ob[AXIS_Z] - start->v.ob[AXIS_Z]) *
                (x - (start->v.ob[AXIS_X] << WORLD_TO_COURSE_SHIFT)) <
            0;
    }

    inline s32 ContainsPoint(Vtx * first, Vtx * second, Vtx * third) {
        if (IsOutsideEdge(first, second) ||
            IsOutsideEdge(second, third) ||
            IsOutsideEdge(third, first)) {
            return FALSE;
        }

        triangle->corners[AXIS_X][0] = first->v.ob[AXIS_X];
        triangle->corners[AXIS_X][1] = second->v.ob[AXIS_X];
        triangle->corners[AXIS_X][2] = third->v.ob[AXIS_X];
        triangle->corners[AXIS_Y][0] = first->v.ob[AXIS_Y];
        triangle->corners[AXIS_Y][1] = second->v.ob[AXIS_Y];
        triangle->corners[AXIS_Y][2] = third->v.ob[AXIS_Y];
        triangle->corners[AXIS_Z][0] = first->v.ob[AXIS_Z];
        triangle->corners[AXIS_Z][1] = second->v.ob[AXIS_Z];
        triangle->corners[AXIS_Z][2] = third->v.ob[AXIS_Z];
        return TRUE;
    }

    if (g_GameState.isMiniGolf) {
        if ((u32)x > MINI_GOLF_GROUND_SIZE - 1) {
            return -1;
        }

        if ((u32)z > MINI_GOLF_GROUND_SIZE - 1) {
            return -1;
        }
    }
    else if ((u32)x > GROUND_WIDTH - 1 || (u32)z > GROUND_LENGTH - 1) {
        return -1;
    }

    if (g_GameState.isMiniGolf) {
        baseColumn = x >> MINI_GOLF_TILE_X_SHIFT;
        baseRow = z >> MINI_GOLF_TILE_Z_SHIFT;
        baseColumn *= GROUND_TILE_POINT_SPAN;
        baseRow *= GROUND_TILE_POINT_SPAN;
        slotColumn = PointSlot(x >> (MINI_GOLF_TILE_X_SHIFT - TILE_POINT_SPAN_SHIFT));
        slotRow = PointSlot(z >> (MINI_GOLF_TILE_Z_SHIFT - TILE_POINT_SPAN_SHIFT));
    }
    else {
        baseColumn = x >> GROUND_TILE_SHIFT;
        baseRow = z >> GROUND_TILE_SHIFT;
        baseColumn *= GROUND_TILE_POINT_SPAN;
        baseRow *= GROUND_TILE_POINT_SPAN;
        slotColumn = PointSlot(x >> (GROUND_TILE_SHIFT - TILE_POINT_SPAN_SHIFT));
        slotRow = PointSlot(z >> (GROUND_TILE_SHIFT - TILE_POINT_SPAN_SHIFT));
    }

    for (rowSearchIndex = 0; rowSearchIndex < GROUND_TILE_POINT_SPAN; rowSearchIndex++) {
        Vtx *corners[4];

        row = baseRow + PointSlot(slotRow + f_CellSearchOffsets[rowSearchIndex]);
        for (columnSearchIndex = 0; columnSearchIndex < GROUND_TILE_POINT_SPAN;
            columnSearchIndex++) {
            column = baseColumn + PointSlot(slotColumn + f_CellSearchOffsets[columnSearchIndex]);
            corners[0] = GetGroundPoint(column, row);
            corners[1] = GetGroundPoint(column + 1, row);
            corners[2] = GetGroundPoint(column, row + 1);
            if (ContainsPoint(corners[0], corners[1], corners[2])) {
                return 0;
            }

            corners[3] = GetGroundPoint(column + 1, row + 1);
            if (ContainsPoint(corners[1], corners[3], corners[2])) {
                return 0;
            }
        }
    }

    PrintDebugText("GetHeightInfo", GROUND_ERROR_COLUMN, GROUND_ERROR_ROW);
    return -1;
}

Boolean AreTrianglesDifferent(PlaneTriangle *first, PlaneTriangle *second) {
    s32 cornerIndex;

    for (cornerIndex = 0; cornerIndex < TRIANGLE_CORNER_COUNT; cornerIndex++) {
        if (first->corners[AXIS_X][cornerIndex] != second->corners[AXIS_X][cornerIndex] ||
            first->corners[AXIS_Y][cornerIndex] != second->corners[AXIS_Y][cornerIndex] ||
            first->corners[AXIS_Z][cornerIndex] != second->corners[AXIS_Z][cornerIndex]) {
            return TRUE;
        }
    }

    return FALSE;
}

#define NORMAL_PART_MAX 0x4000
#define NORMAL_REDUCTION 256

#define IsNormalTooLong(normalX, normalY, normalZ) \
    ((Abs(normalX) > NORMAL_PART_MAX) || \
        (Abs(normalY) > NORMAL_PART_MAX) || \
        (Abs(normalZ) > NORMAL_PART_MAX))

Outcome GetTriangleIntegerPlane(PlaneTriangle *triangle, IntegerPlane *plane) {
    s32 firstX;
    s32 secondX;
    s32 firstY;
    s32 secondY;
    s32 firstZ;
    s32 secondZ;
    s32 normalX;
    s32 normalY;
    s32 normalZ;
    s32 distance;
    s32 length;

    secondZ = triangle->corners[AXIS_Z][2] - triangle->corners[AXIS_Z][0];
    firstY = triangle->corners[AXIS_Y][1];
    firstY -= triangle->corners[AXIS_Y][0];
    secondY = triangle->corners[AXIS_Y][2] - triangle->corners[AXIS_Y][0];
    firstZ = triangle->corners[AXIS_Z][1] - triangle->corners[AXIS_Z][0];
    firstX = triangle->corners[AXIS_X][1] - triangle->corners[AXIS_X][0];
    secondX = triangle->corners[AXIS_X][2] - triangle->corners[AXIS_X][0];
    normalX = (firstY * secondZ) - (secondY * firstZ);
    normalY = (-firstX * secondZ) + (secondX * firstZ);
    normalZ = (-firstY * secondX) + (secondY * firstX);
    distance =
        -((normalX * triangle->corners[AXIS_X][0]) + (normalY * triangle->corners[AXIS_Y][0]) +
            (normalZ * triangle->corners[AXIS_Z][0]));
    while (IsNormalTooLong(normalX, normalY, normalZ)) {
        normalX /= NORMAL_REDUCTION;
        normalY /= NORMAL_REDUCTION;
        normalZ /= NORMAL_REDUCTION;
        distance /= NORMAL_REDUCTION;
    }

    length = sqrt((normalX * normalX) + (normalY * normalY) + (normalZ * normalZ));
    if (length == 0) {
        PrintDebugText("NormalError", NORMAL_ERROR_TEXT_COLUMN, NORMAL_ERROR_TEXT_ROW);
        plane->normal[AXIS_X] = 0;
        plane->normal[AXIS_Y] = INTEGER_PLANE_UNIT;
        plane->normal[AXIS_Z] = 0;
        plane->distance = 0;
        plane->normalLength = INTEGER_PLANE_UNIT;
        return OUTCOME_FAILED;
    }

    if (normalY < 0) {
        length = -length;
    }

    plane->normalLength = INTEGER_PLANE_UNIT;
    plane->normal[AXIS_X] = (normalX << INTEGER_PLANE_SHIFT) / length;
    plane->normal[AXIS_Y] = (normalY << INTEGER_PLANE_SHIFT) / length;
    plane->normal[AXIS_Z] = (normalZ << INTEGER_PLANE_SHIFT) / length;
    plane->distance = (distance << INTEGER_PLANE_SHIFT) / length;
    return OUTCOME_OK;
}

Outcome GetTrianglePlane(PlaneTriangle *triangle, TrianglePlane *plane) {
    s32 firstX;
    s32 secondX;
    s32 firstY;
    s32 secondY;
    s32 firstZ;
    s32 secondZ;
    f32 normalX;
    f32 normalY;
    f32 normalZ;
    f32 length;
    f32 distance;
    f32 scale;

    secondZ = triangle->corners[AXIS_Z][2] - triangle->corners[AXIS_Z][0];
    firstY = triangle->corners[AXIS_Y][1];
    firstY -= triangle->corners[AXIS_Y][0];
    secondY = triangle->corners[AXIS_Y][2] - triangle->corners[AXIS_Y][0];
    firstZ = triangle->corners[AXIS_Z][1] - triangle->corners[AXIS_Z][0];
    firstX = triangle->corners[AXIS_X][1] - triangle->corners[AXIS_X][0];
    secondX = triangle->corners[AXIS_X][2] - triangle->corners[AXIS_X][0];
    normalX = (firstY * secondZ) - (secondY * firstZ);
    normalY = (-firstX * secondZ) + (secondX * firstZ);
    normalZ = (-firstY * secondX) + (secondY * firstX);
    length = sqrtf((normalX * normalX) + (normalY * normalY) + (normalZ * normalZ));
    distance =
        -((normalX * triangle->corners[AXIS_X][0]) + (normalY * triangle->corners[AXIS_Y][0]) +
            (normalZ * triangle->corners[AXIS_Z][0]));
    if (length == 0.0f) {
        if (g_IsDebugEnabled) {
            PrintDebugText("NormalFError", NORMAL_ERROR_TEXT_COLUMN, NORMAL_ERROR_TEXT_ROW);
        }

        plane->normal[AXIS_X] = 0.0f;
        plane->normal[AXIS_Z] = 0.0f;
        plane->distance = 0.0f;
        plane->normal[AXIS_Y] = 1.0f;
        plane->normalLength = 1.0f;
        return OUTCOME_FAILED;
    }

    if (normalY < 0.0f) {
        length = -length;
    }

    scale = 1.0f / length;
    plane->normalLength = 1.0f;
    plane->normal[AXIS_X] = normalX * scale;
    plane->normal[AXIS_Y] = normalY * scale;
    plane->normal[AXIS_Z] = normalZ * scale;
    plane->distance = distance * scale;
    return OUTCOME_OK;
}

#define NO_GROUND_HEIGHT (-300)

s32 GetTerrainHeight(s32 x, s32 z) {
    PlaneTriangle triangle;
    s32 firstX;
    s32 secondX;
    s32 firstY;
    s32 secondY;
    s32 firstZ;
    s32 secondZ;
    f32 normalX;
    f32 normalY;
    f32 normalZ;
    s32 originX;
    s32 originZ;
    s32 offsetX;
    s32 offsetZ;
    s32 height;
    s32 firstEdgeEndY;

    if (g_GameState.isMiniGolf) {
        if ((u32)x > MINI_GOLF_GROUND_SIZE - 1) {
            if (x < 0) {
                x = 0;
            }
            else {
                x = MINI_GOLF_GROUND_SIZE - 1;
            }
        }

        if ((u32)z > MINI_GOLF_GROUND_SIZE - 1) {
            if (z < 0) {
                z = 0;
            }
            else {
                z = MINI_GOLF_GROUND_SIZE - 1;
            }
        }
    }
    else {
        if ((u32)x > GROUND_WIDTH - 1) {
            if (x < 0) {
                x = 0;
            }
            else {
                x = GROUND_WIDTH - 1;
            }
        }

        if ((u32)z > GROUND_LENGTH - 1) {
            if (z < 0) {
                z = 0;
            }
            else {
                z = GROUND_LENGTH - 1;
            }
        }
    }

    if (FindGroundTriangle(x, z, &triangle) < 0) {
        return NO_GROUND_HEIGHT;
    }

    firstEdgeEndY = triangle.corners[AXIS_Y][1];
    secondZ = triangle.corners[AXIS_Z][2] - triangle.corners[AXIS_Z][0];
    firstY = firstEdgeEndY - triangle.corners[AXIS_Y][0];
    firstZ = triangle.corners[AXIS_Z][1] - triangle.corners[AXIS_Z][0];
    firstX = triangle.corners[AXIS_X][1] - triangle.corners[AXIS_X][0];
    secondY = triangle.corners[AXIS_Y][2] - triangle.corners[AXIS_Y][0];
    secondX = triangle.corners[AXIS_X][2] - triangle.corners[AXIS_X][0];
    height = triangle.corners[AXIS_Y][0] << WORLD_TO_COURSE_SHIFT;
    originX = triangle.corners[AXIS_X][0] << WORLD_TO_COURSE_SHIFT;
    originZ = triangle.corners[AXIS_Z][0] << WORLD_TO_COURSE_SHIFT;
    offsetX = x - originX;
    offsetZ = z - originZ;
    normalX = (firstY * secondZ) + (secondY * -firstZ);
    normalY = (firstX * -secondZ) + (secondX * firstZ);
    normalZ = (firstY * -secondX) + (secondY * firstX);
    if ((s32)normalY != 0) {
        return height + (s32)(((-normalX * offsetX) - (normalZ * offsetZ)) / normalY);
    }

    return 0;
}

s32 GetGroundHeight(s32 x, s32 z) {
    return GetTerrainHeight(x, z);
}

s32 GetGroundOrWaterHeight(s32 x, s32 z) {
    PlaneTriangle hit;
    f32 fraction;
    s32 height = GetTerrainHeight(x, z);
    s32 waterHeight;

    if (FindHoleTriangleHit(x, -WATER_PROBE_REACH, z, x, WATER_PROBE_REACH, z, &hit, &fraction)) {
        char unusedText[0x80] UNUSED;

        waterHeight = WaterProbeHeight(fraction, WATER_PROBE_REACH);
        if (waterHeight < height) {
            height = waterHeight;
        }
    }

    return height;
}

static const char f_UnreferencedRodata[8] = {0};
