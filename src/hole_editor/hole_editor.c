#define CALLS_GROUND_POINT_SETTER_UNPROTOTYPED

#include "hole_editor.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>

#include "billboard.h"
#include "camera.h"
#include "controllers/controller.h"
#include "course.h"
#include "course_space.h"
#include "debug_text.h"
#include "display_list.h"
#include "game.h"
#include "game_flags.h"
#include "hole_builder.h"
#include "hole_loader.h"
#include "hole_setup.h"
#include "hole_triangles.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "play.h"
#include "player.h"
#include "scene.h"
#include "screen.h"
#include "sound.h"
#include "surface.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/host_file.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "terrain.h"
#include "text.h"
#include "view_projection.h"
#include "work_area.h"

#define EDGE_POINT_COUNT (GROUND_TILE_POINT_SPAN + 1)
#define SEGMENT_POINT_COUNT 3
#define SEGMENT_START 0
#define SEGMENT_MIDPOINT 1
#define SEGMENT_END 2
#define SLOPE_HALF_COUNT 2
#define SLOPE_BLOCK_COLUMN_COUNT 8
#define SLOPE_BLOCK_ROW_COUNT 16
#define START_TRIANGLE_SIZE 256
#define SLOPE_SHADE_OFFSET 0x40
#define SLOPE_SHADE_MAX 0xFF

#define SHAPE_OFFSET_COUNT 1600
#define SHAPE_OFFSET_SPAN 20
#define HOLE_EDITOR_SHAPE_ASSERT_LINE 2191
#define GREEN_NOISE_MASK 3
#define GROUND_NOISE_MASK 0xF
#define HEIGHT_PER_COURSE_UNIT (-0.00000390625f)
#define BUNKER_INNER_DISTANCE 2
#define BUNKER_INNER_OFFSET 45
#define ROUGH_INNER_DISTANCE 19
#define ROUGH_EDGE_OFFSET 22
#define ROUGH_INNER_OFFSET 120
#define ROUGH_INNER_NOISE_MASK 0x1F
#define SHAPED_POINT_SHADE 0xFF
#define TREE_DISTANCE_MIN 8
#define TREE_DISTANCE_MAX 10
#define TREE_SPACING 0x100
#define TREE_TEXTURE_INDEX 2
#define TREE_NEAR_EDGE 0x100
#define TREE_WIDTH 0x80
#define TREE_HEIGHT 0x100

#define BLEND_DISTANCE_SCALE 512.0f
#define BLENDED_POINT_SHADE 0xFF

#define CELL_CROSS_PRODUCT_MIN 16

#define SLOPE_OFFSET_COUNT 400
#define SLOPE_OFFSET_SPAN 10
#define SLOPE_START_DISTANCE 5
#define SLOPE_NOISE_DISTANCE 6
#define SLOPE_EXTRA_NOISE_DISTANCE 12
#define SLOPE_BASE_DISTANCE 2
#define SLOPE_DROP_PER_DISTANCE (30.0f / OFFSET_DISTANCE_SCALE)
#define SLOPE_NOISE_MASK 0x3FFF
#define SLOPE_NOISE_SCALE 0.001f
#define SLOPE_FAR_DROP 60.0f
#define SLOPE_EXTRA_DROP 15.0f

#define EDITED_POINT_SHADE 0xFF
#define HEIGHT_STEP 4
#define COARSE_NOISE_MASK 0x3F
#define COARSE_NOISE_OFFSET 0x20
#define ROUGH_NOISE_MASK 0x7F
#define ROUGH_NOISE_OFFSET 0x40
#define STEP_NOISE_BIT (HEIGHT_STEP * 2)
#define DIP_NOISE_BIT 0x10

enum HeightEdit {
    HEIGHT_EDIT_NONE,
    HEIGHT_EDIT_POINT,
    HEIGHT_EDIT_MOVE
};

typedef s32 HeightEdit;

enum CursorDirection {
    CURSOR_DIRECTION_NONE,
    CURSOR_DIRECTION_RIGHT = 2,
    CURSOR_DIRECTION_UP_RIGHT,
    CURSOR_DIRECTION_UP,
    CURSOR_DIRECTION_UP_LEFT,
    CURSOR_DIRECTION_LEFT,
    CURSOR_DIRECTION_DOWN_LEFT,
    CURSOR_DIRECTION_DOWN,
    CURSOR_DIRECTION_DOWN_RIGHT
};

typedef s32 CursorDirection;

#define BINARY_ANGLE_EIGHTH_TURN (BINARY_ANGLE_TURN / 8)
#define EDITOR_ANGLE_TO_RADIANS (3.1416 / 32768)
#define CURSOR_STEP_LENGTH 1.5
#define EDITOR_ZOOM_MAX 4.0f
#define STICK_STEP_SIZE 16
#define HEADING_STICK_SPEED 0x100
#define PITCH_STICK_SPEED (1.0f / 180)
#define EDITOR_PITCH_MIN (-80 * 3.1416 / 180)
#define EDITOR_CAMERA_DISTANCE 2000.0
#define EDITOR_CAMERA_START_ANGLE (3.1416f / 4)
#define METERS_PER_HEIGHT_UNIT (1.0f / 15)
#define HEIGHT_TEXT_COLUMN 25
#define HEIGHT_TEXT_ROW 3

enum TriangleEdit {
    TRIANGLE_EDIT_NONE,
    TRIANGLE_EDIT_MOVE,
    TRIANGLE_EDIT_SELECT,
    TRIANGLE_EDIT_KIND = 4
};

typedef s32 TriangleEdit;

#define FIRST_RING_TRIANGLE (HOLE_TRIANGLE_COUNT - RING_COUNT)
#define CORNER_STEP_SCALE 10.0f
#define SAVED_CORNER_COUNT 10
#define TRIANGLE_TEXT_COLUMN 10
#define EDIT_KIND_TEXT_ROW 10
#define CORNER_HEIGHT_TEXT_ROW 11

#define RING_DIRECTION_CORNER 1
#define RING_SCALE_CORNER 2
#define RING_SCALE_STEP 16
#define RING_SCALE_MIN 16
#define RING_SCALE_MAX 0x2000
#define RING_DIRECTION_LENGTH 8192.0f
#define RING_TEXT_COLUMN 19
#define RING_TEXT_ROW 4

#define WALL_TRIANGLE_COUNT 2
#define WALL_BOTTOM_CORNER_COUNT 2
#define WALL_TOP_CORNER 2
#define WALL_HEIGHT 24.0f
#define WALL_SINK_DEPTH (12 << WORLD_TO_COURSE_SHIFT)
#define WALL_LIMIT_SOUND 0xE
#define WALL_LIMIT_SOUND_CHANNEL 11
#define WALL_TEXT_COLUMN 5
#define WALL_TEXT_ROW 3

#define TREE_TEXT_COLUMN 19
#define TREE_KIND_LABEL_ROW 24
#define TREE_KIND_TEXT_ROW 25
#define TREE_INDEX_LABEL_ROW 26
#define TREE_INDEX_TEXT_ROW 27
#define TREE_SIZE_MAX 0x800
#define TREE_X_LIMIT 0x1000
#define TREE_Z_LIMIT 0x2000
#define IsFreeBillboard(billboard) \
    ((billboard).x <= 0 || \
        (billboard).x >= TREE_X_LIMIT || \
        (billboard).z <= 0 || \
        (billboard).z >= TREE_Z_LIMIT)
#define COPIED_TREE_OFFSET 0x100

#define HEX_DUMP_BUFFER_SIZE 0x40000
#define HEX_DUMP_TEXT_SIZE 0x100
#define HEX_DUMP_LINE_WORDS 4
#define HEX_DUMP_LINE_BYTES (HEX_DUMP_LINE_WORDS * (s32)sizeof(u32))
#define HOLE_EDITOR_HEX_DUMP_ASSERT_LINE 3027

enum HoleFileStep {
    HOLE_FILE_STEP_ACTION,
    HOLE_FILE_STEP_COURSE,
    HOLE_FILE_STEP_HOLE,
    HOLE_FILE_STEP_CONFIRM
};

typedef s32 HoleFileStep;

#define HOLE_FILE_BLINK_BIT 4
#define HOLE_FILE_ARROW_COLUMN 7
#define HOLE_FILE_LABEL_COLUMN 10
#define HOLE_FILE_NUMBER_COLUMN 16
#define HOLE_FILE_PROMPT_COLUMN 5
#define HOLE_FILE_LOAD_ROW 10
#define HOLE_FILE_SAVE_ROW 11
#define HOLE_FILE_COURSE_ROW 15
#define HOLE_FILE_HOLE_ROW 16
#define HOLE_FILE_PROMPT_ROW 18

#define CELL_COLUMN_COUNT (GROUND_POINT_COLUMN_COUNT - 1)
#define CELL_ROW_COUNT (GROUND_POINT_ROW_COUNT - 1)

#define POINT_MOVE_COUNT 4
#define POINT_MOVE_STEP 0x10
#define POINT_TEXTURE_STEP 0x40
#define TILE_POINT_MASK (GROUND_TILE_POINT_SPAN - 1)
#define POINT_EDITOR_SETTING_MIN 2
#define POINT_EDITOR_SETTING_MAX 13
#define HEIGHT_UNITS_PER_METER 15
#define POINT_TEXT_COLUMN 2
#define TILE_TEXT_ROW 4
#define TILE_POINT_TEXT_ROW 5
#define X_LIMIT_TEXT_ROW 6
#define Z_LIMIT_TEXT_ROW 7
#define POINT_HEIGHT_TEXT_ROW 8
#define DEAD_TRIANGLE_TEXT_ROW 9

#define HOLE_POINT_MODE_UNSET (-1)
#define HOLE_POINT_MODE_PIN_TEE 0
#define HOLE_POINT_MODE_AIM 1
#define HOLE_POINT_MODE_ROUTE 2
#define HOLE_POINT_MODE_COUNT 3
#define HOLE_POINT_STEP_LENGTH 10.0
#define HEADING_RADIANS ((u16)f_HolePointCameraYawAngle * BINARY_ANGLE_TO_RADIANS)
#define CURSOR_TEXT_COLUMN 3
#define CURSOR_TEXT_ROW 5
#define SURFACE_TEXT_COLUMN 2
#define SURFACE_TEXT_ROW 4
#define HOLE_POINT_TEXT_COLUMN 9
#define HOLE_POINT_MODE_TEXT_ROW 6
#define HOLE_POINT_INDEX_TEXT_ROW 7
#define CURSOR_ZOOM_IN_FACTOR 1.1f
#define CURSOR_ZOOM_OUT_FACTOR 0.9f
#define CURSOR_ZOOM_MAX 10.0f
#define CURSOR_ZOOM_MIN 0.1f

#define DIAGONAL_DISTANCE_MIN 3.0f
#define DIAGONAL_SIDE_COUNT 4
#define SIDE_CODE_COUNT 2
#define HasTwoSideCodes(codes) ((codes)[SIDE_CODE_COUNT - 1] != TERRAIN_CODE_NONE)
#define HOLE_EDITOR_DIAGONAL_ASSERT_LINE 2046
#define VERTEX_UNITS_PER_CODE 16

#define CAPTURE_HEADER_SIZE 54
#define CORNER_CAPTURE_WIDTH 64
#define CORNER_CAPTURE_HEIGHT 128
#define CAPTURE_BYTES_PER_PIXEL 3
#define CORNER_CAPTURE_SIZE \
    (CAPTURE_HEADER_SIZE + (CORNER_CAPTURE_WIDTH * CORNER_CAPTURE_HEIGHT * CAPTURE_BYTES_PER_PIXEL))
#define LITTLE_ENDIAN_16(value) ((value) & 0xFF), (((value) >> 8) & 0xFF)
#define LITTLE_ENDIAN_32(value) LITTLE_ENDIAN_16(value), LITTLE_ENDIAN_16((value) >> 16)
#define BMP_INFO_HEADER_SIZE 40
#define BMP_PLANE_COUNT 1
#define CAPTURE_BITS_PER_PIXEL 24
#define BMP_NO_COMPRESSION 0
#define CAPTURE_PIXELS_PER_METER_X 3790
#define CAPTURE_PIXELS_PER_METER_Y 3780
#define BMP_HEADER(fileSize, width, height, imageSize) \
    {'B', \
        'M', \
        LITTLE_ENDIAN_32(fileSize), \
        LITTLE_ENDIAN_32(0), \
        LITTLE_ENDIAN_32(CAPTURE_HEADER_SIZE), \
        LITTLE_ENDIAN_32(BMP_INFO_HEADER_SIZE), \
        LITTLE_ENDIAN_32(width), \
        LITTLE_ENDIAN_32(height), \
        LITTLE_ENDIAN_16(BMP_PLANE_COUNT), \
        LITTLE_ENDIAN_16(CAPTURE_BITS_PER_PIXEL), \
        LITTLE_ENDIAN_32(BMP_NO_COMPRESSION), \
        LITTLE_ENDIAN_32(imageSize), \
        LITTLE_ENDIAN_32(CAPTURE_PIXELS_PER_METER_X), \
        LITTLE_ENDIAN_32(CAPTURE_PIXELS_PER_METER_Y), \
        LITTLE_ENDIAN_32(0), \
        LITTLE_ENDIAN_32(0)}
#define MAP_CAPTURE_WIDTH (GROUND_TILE_COLUMN_COUNT * GROUND_TILE_TEXEL_SPAN)
#define MAP_CAPTURE_HEIGHT (GROUND_TILE_ROW_COUNT * GROUND_TILE_TEXEL_SPAN)
#define MAP_CAPTURE_SIZE \
    (CAPTURE_HEADER_SIZE + (MAP_CAPTURE_WIDTH * MAP_CAPTURE_HEIGHT * CAPTURE_BYTES_PER_PIXEL))
#define GROUND_TILE_TEXEL_BORDER 1
#define CAPTURE_FILE_NAME_SIZE 0x80
#define SCREEN_CAPTURE_SIZE \
    (CAPTURE_HEADER_SIZE + (SCREEN_WIDTH * SCREEN_HEIGHT * CAPTURE_BYTES_PER_PIXEL))
#define KSEG1_OFFSET 0x20000000
#define UncachedAddress(address) AddressPast((address), KSEG1_OFFSET)

#define AssertInEditor(condition, lineNumber) \
    ((condition) ? (void)0 : ReportFailedAssertion("EX", "c:/usgolf64/hiedit.c", lineNumber))

#define POLE_QUAD_COUNT 2

#define WAVE_TEXTURE_WIDTH 32
#define WAVE_TEXTURE_HEIGHT 32
#define WAVE_ANGLE_STEP 0.19635
#define WAVE_TEXEL_STEP 4
#define WAVE_AMPLITUDE 256.0
#define WAVE_LEVEL_SHIFT 20
#define WAVE_LEVEL_MIN 1
#define WAVE_LEVEL_MAX 15
#define IA8_ALPHA_SHIFT 4

#define EDITOR_GROUND_LIST_LENGTH 15000

#define WHITE_TEXTURE_SIZE 32
#define WHITE_TEXEL (-1)

#define WAVE_TMEM 0x180
#define WAVE_SCROLL_DIVISOR 32
#define WAVE_TILE_SPAN 0x40

#define MAP_BLEND_ALPHA 0xFE
#define SECOND_PASS_BLEND_ALPHA 0xF7

#define FINE_TILE_CELL_SPAN 4
#define FINE_TILE_BLOCK_SPAN 2
#define GROUND_HALF_CELL_COLUMNS (GROUND_TILE_GROUP_SIZE * FINE_TILE_CELL_SPAN)
#define FINE_TILE_POINT_SPAN (FINE_TILE_CELL_SPAN + 1)
#define TILE_TRIANGLE_COUNT (FINE_TILE_CELL_SPAN * FINE_TILE_CELL_SPAN * 2)

enum TileVertexSlot {
    TILE_VERTEX_ROW0_COLUMN0,
    TILE_VERTEX_ROW1_COLUMN0,
    TILE_VERTEX_ROW2_COLUMN0,
    TILE_VERTEX_ROW3_COLUMN0,
    TILE_VERTEX_ROW4_COLUMN0,
    TILE_VERTEX_ROW0_COLUMN1,
    TILE_VERTEX_ROW4_COLUMN1,
    TILE_VERTEX_ROW0_COLUMN2,
    TILE_VERTEX_ROW2_COLUMN2,
    TILE_VERTEX_ROW4_COLUMN2,
    TILE_VERTEX_ROW0_COLUMN3,
    TILE_VERTEX_ROW4_COLUMN3,
    TILE_VERTEX_ROW0_COLUMN4,
    TILE_VERTEX_ROW1_COLUMN4,
    TILE_VERTEX_ROW2_COLUMN4,
    TILE_VERTEX_ROW3_COLUMN4,
    TILE_VERTEX_ROW4_COLUMN4,
    FINE_TILE_BUILT_VERTEX_COUNT
};

#define TILE_FIRST_COLUMN_VERTEX_COUNT TILE_VERTEX_ROW0_COLUMN1
#define TILE_MIDDLE_COLUMN_VERTEX_COUNT (TILE_VERTEX_ROW0_COLUMN4 - TILE_VERTEX_ROW0_COLUMN1)
#define TILE_LAST_COLUMN_VERTEX_COUNT (FINE_TILE_BUILT_VERTEX_COUNT - TILE_VERTEX_ROW0_COLUMN4)
#define TILE_INNER_MIDPOINT_CACHE_SLOT TILE_VERTEX_ROW0_COLUMN4
#define TILE_LAST_COLUMN_CACHE_SLOT (TILE_INNER_MIDPOINT_CACHE_SLOT + TILE_INNER_MIDPOINT_COUNT)

#define BORDERLESS_TILE_FIRST_COORDINATE 4
#define TEST_TILE_SIZE 0x88

#define MIRROR_S_OFFSET 0x100
#define MIRROR_S_SCALE 16
#define MIRRORED_TEST_S 0x2100
#define MIRRORED_TEST_FIRST_S 0x88
#define MIRRORED_TEST_LAST_S 0x8A

#define SHADED_TRIANGLE_LEVEL 0x96
#define CELL_LEVEL 0xDC
#define MIRRORED_CELL_LEVEL 0xA0

enum TilePass {
    TILE_PASS_ONLY,
    TILE_PASS_FIRST,
    TILE_PASS_SECOND
};

typedef s32 TilePass;

enum GroundOverlay {
    GROUND_OVERLAY_NOISE,
    GROUND_OVERLAY_WAVES,
    GROUND_OVERLAY_NONE
};

typedef s32 GroundOverlay;

enum EditorCombineMode {
    EDITOR_COMBINE_NONE = -1,
    EDITOR_COMBINE_WAVES_ONLY,
    EDITOR_COMBINE_NOISE_ONLY,
    EDITOR_COMBINE_WAVES_FIRST,
    EDITOR_COMBINE_NOISE_FIRST,
    EDITOR_COMBINE_WAVES_SECOND,
    EDITOR_COMBINE_NOISE_SECOND,
    EDITOR_COMBINE_TILE_ONLY,
    EDITOR_COMBINE_TILE_TWICE
};

typedef s32 EditorCombineMode;

#define HasTileBorders(tileIndex) \
    (g_WorkArea.hole.tiles[tileIndex] \
            .texels[GROUND_TILE_TEXEL_ROW_COUNT - 1][GROUND_TILE_TEXEL_ROW_LENGTH - 1] != 0)

#define IsNotFine(tileIndex) (g_WorkArea.hole.tiles[tileIndex].detail < GROUND_DETAIL_FINE)

#define LoadWaveTexture() \
    gDPLoadMultiBlock( \
        gfx++, \
        TextureAddress(f_WaveTexels), \
        WAVE_TMEM, \
        G_TX_RENDERTILE, \
        G_IM_FMT_IA, \
        G_IM_SIZ_8b, \
        WAVE_TEXTURE_WIDTH, \
        WAVE_TEXTURE_HEIGHT, \
        0, \
        G_TX_NOMIRROR | G_TX_WRAP, \
        G_TX_NOMIRROR | G_TX_WRAP, \
        TextureMask(WAVE_TEXTURE_WIDTH), \
        TextureMask(WAVE_TEXTURE_HEIGHT), \
        15, \
        15 \
    )

#define LoadWhiteNoiseTexture(shiftS, shiftT) \
    gDPLoadMultiBlock( \
        gfx++, \
        TextureAddress(f_WhiteNoiseTexels), \
        NOISE_TMEM, \
        G_TX_RENDERTILE, \
        G_IM_FMT_I, \
        G_IM_SIZ_8b, \
        WHITE_TEXTURE_SIZE, \
        WHITE_TEXTURE_SIZE, \
        0, \
        G_TX_MIRROR, \
        G_TX_MIRROR, \
        TextureMask(WHITE_TEXTURE_SIZE), \
        TextureMask(WHITE_TEXTURE_SIZE), \
        shiftS, \
        shiftT \
    )

#define LoadEditorTileTexture(tileIndex, firstS, groupColumn, row, pass, overlay) \
    { \
        if (HasTileBorders(tileIndex)) { \
            if ((pass) == 0 || g_WorkArea.hole.tiles[tileIndex].extraPassCount <= 0) { \
                if (g_WorkArea.hole.tiles[tileIndex].extraPassCount != 0) { \
                    SetEditorCombineMode(TILE_PASS_FIRST, overlay); \
                } \
                else { \
                    SetEditorCombineMode(TILE_PASS_ONLY, overlay); \
                } \
                gDPLoadMultiBlock( \
                    gfx++, \
                    TextureAddress(&g_WorkArea.hole.tiles[tileIndex]), \
                    0, \
                    1, \
                    G_IM_FMT_RGBA, \
                    G_IM_SIZ_16b, \
                    GROUND_TILE_TEXEL_ROW_LENGTH, \
                    GROUND_TILE_TEXEL_ROW_COUNT, \
                    0, \
                    G_TX_MIRROR | G_TX_WRAP, \
                    G_TX_NOMIRROR | G_TX_CLAMP, \
                    6, \
                    G_TX_NOMASK, \
                    G_TX_NOLOD, \
                    G_TX_NOLOD \
                ); \
            } \
            else { \
                SetEditorCombineMode(TILE_PASS_SECOND, overlay); \
            } \
\
            if (IsGameFlagSet(GAME_FLAG_TEST_GROUND_TEXTURE)) { \
                gDPLoadMultiBlock( \
                    gfx++, \
                    TextureAddress(&g_WorkArea.hole.tiles[tileIndex]), \
                    0, \
                    1, \
                    G_IM_FMT_RGBA, \
                    G_IM_SIZ_16b, \
                    GROUND_TILE_TEXEL_ROW_LENGTH, \
                    GROUND_TILE_TEXEL_ROW_COUNT, \
                    0, \
                    G_TX_MIRROR | G_TX_WRAP, \
                    G_TX_NOMIRROR | G_TX_CLAMP, \
                    6, \
                    6, \
                    3, \
                    3 \
                ); \
                gDPSetTileSize(gfx++, 1, 0, 0, TEST_TILE_SIZE, TEST_TILE_SIZE); \
            } \
            else { \
                gDPSetTileSize( \
                    gfx++, \
                    1, \
                    (firstS) + ((groupColumn) * TILE_COORDINATE_SPAN) + \
                        BORDERED_TILE_FIRST_COORDINATE, \
                    ((row) * TILE_COORDINATE_SPAN) + BORDERED_TILE_FIRST_COORDINATE, \
                    (firstS) + (((groupColumn) + 1) * TILE_COORDINATE_SPAN) + \
                        BORDERED_TILE_S_PAST_NEXT, \
                    (((row) + 1) * TILE_COORDINATE_SPAN) + BORDERED_TILE_T_PAST_NEXT \
                ); \
            } \
        } \
        else { \
            gDPLoadMultiBlock( \
                gfx++, \
                TextureAddress(&g_WorkArea.hole.tiles[tileIndex]), \
                0, \
                1, \
                G_IM_FMT_RGBA, \
                G_IM_SIZ_16b, \
                GROUND_TILE_TEXEL_SPAN, \
                GROUND_TILE_TEXEL_SPAN, \
                0, \
                G_TX_MIRROR | G_TX_WRAP, \
                G_TX_NOMIRROR | G_TX_CLAMP, \
                TextureMask(GROUND_TILE_TEXEL_SPAN), \
                G_TX_NOMASK, \
                G_TX_NOLOD, \
                G_TX_NOLOD \
            ); \
            gDPSetTileSize( \
                gfx++, \
                1, \
                (firstS) + ((groupColumn) * TILE_COORDINATE_SPAN) + \
                    BORDERLESS_TILE_FIRST_COORDINATE, \
                ((row) * TILE_COORDINATE_SPAN) + BORDERLESS_TILE_FIRST_COORDINATE, \
                (firstS) + (((groupColumn) + 1) * TILE_COORDINATE_SPAN) + \
                    BORDERLESS_TILE_FIRST_COORDINATE, \
                (((row) + 1) * TILE_COORDINATE_SPAN) + BORDERLESS_TILE_FIRST_COORDINATE \
            ); \
        } \
    }

#define LoadEditorAveragedRow(row, slot) \
    { \
        Vtx *even = (row); \
\
        GetMidpointVertex(&even[0].v, &even[1].v, &vertices[0].v); \
        GetMidpointVertex(&even[1].v, &even[2].v, &vertices[1].v); \
        GetMidpointVertex(&even[2].v, &even[3].v, &vertices[2].v); \
        GetMidpointVertex(&even[3].v, &even[4].v, &vertices[3].v); \
        gSPVertex(gfx++, vertices, 4, slot); \
        vertices += 4; \
    }

static u8 f_TileMidpointSlots[EDGE_POINT_COUNT][EDGE_POINT_COUNT] = {
    {GridVertexSlot(0), 0, GridVertexSlot(4), 1, GridVertexSlot(8)},
    {2, 8, 9, 10, 3},
    {GridVertexSlot(1), 11, GridVertexSlot(5), 12, GridVertexSlot(9)},
    {4, 13, 14, 15, 5},
    {GridVertexSlot(2), 6, GridVertexSlot(6), 7, GridVertexSlot(10)},
};
static s32 f_HolePointCursor[PLANE_AXIS_COUNT] = {0, 0};
static s32 f_UnusedHolePointValue UNUSED = -1;
static s32 f_CellCursorColumn = 0;
static s32 f_CellCursorRow = 0;
static s32 f_PointCursorColumn = 0;
static s32 f_PointCursorRow = 0;
static s32 f_PointEditorFirstSetting = 0;
static s32 f_PointEditorSecondSetting = 0;
static Radians f_HolePointCameraPitch = -EDITOR_CAMERA_START_ANGLE;
static Radians f_HolePointCameraYaw = EDITOR_CAMERA_START_ANGLE;
static s32 f_UnusedHolePointAngle UNUSED = BINARY_ANGLE_EIGHTH_TURN;
static WideBinaryAngle f_HolePointCameraYawAngle = 0;
static s32 f_HolePointEditMode = -1;
static s32 f_PinTeeIndex = 0;
static s32 f_AimPointIndex = 0;
static s32 f_RoutePointIndex = 0;

static Radians f_TriangleCameraPitch = -EDITOR_CAMERA_START_ANGLE;
static Radians f_TriangleCameraYaw = EDITOR_CAMERA_START_ANGLE;
static s32 f_UnusedTriangleAngle UNUSED = BINARY_ANGLE_EIGHTH_TURN;
static WideBinaryAngle f_TriangleCameraYawAngle = 0;
static s32 f_EditedTriangleIndex = 0;
static s32 f_EditedCornerIndex = 0;
static s32 f_SavedCornerIndex = 0;
static TriangleEdit f_LastTriangleEditKind = TRIANGLE_EDIT_NONE;

static Radians f_RingCameraPitch = -EDITOR_CAMERA_START_ANGLE;
static Radians f_RingCameraYaw = EDITOR_CAMERA_START_ANGLE;
static s32 f_UnusedRingAngle UNUSED = BINARY_ANGLE_EIGHTH_TURN;
static WideBinaryAngle f_RingCameraYawAngle = 0;
static s32 f_RingTriangleIndex = 0x9D;
static s32 f_RingCornerIndex = 0;
static s32 f_UnusedRingIndex UNUSED = 0;
static TriangleEdit f_LastRingEditKind = TRIANGLE_EDIT_NONE;

static Radians f_WallCameraPitch = -EDITOR_CAMERA_START_ANGLE;
static Radians f_WallCameraYaw = EDITOR_CAMERA_START_ANGLE;
static s32 f_UnusedWallAngle UNUSED = BINARY_ANGLE_EIGHTH_TURN;
static WideBinaryAngle f_WallCameraYawAngle = 0;
static s32 f_FirstWallTriangle = 0x19;
static s32 f_WallTriangleIndex = 0x19;
static s32 f_WallCornerIndex = 0;
static s32 f_SavedWallCornerIndex = 0;
static TriangleEdit f_LastWallEditKind = TRIANGLE_EDIT_NONE;

void ToggleCellMirroring(s32 column, s32 row);

typedef struct TriangleSlope {
    s16 waveS;
    s16 waveT;
    s16 normalX;
    s16 normalZ;
    u8 shade;
} TriangleSlope;

#define EDITOR_BUFFER_SIZE 0x40000

static CoursePoint f_SavedCorners[SAVED_CORNER_COUNT];
static CoursePoint f_UnusedSavedCorners[SAVED_CORNER_COUNT] UNUSED;
static CoursePoint f_SavedWallCorners[SAVED_CORNER_COUNT];
static Mtx f_PoleMatrix;
static s32 f_UnusedEditorWords[2] UNUSED;
static u8 f_WhiteNoiseTexels[WHITE_TEXTURE_SIZE * WHITE_TEXTURE_SIZE];
static TriangleSlope f_TriangleSlopes[CELL_COLUMN_COUNT * CELL_ROW_COUNT * 2];
static Gfx f_EditorGroundDisplayLists[DRAW_BUFFER_COUNT][EDITOR_GROUND_LIST_LENGTH];
static u8 f_EditorBuffer[EDITOR_BUFFER_SIZE];
static u8 f_WaveTexels[WAVE_TEXTURE_HEIGHT * WAVE_TEXTURE_WIDTH];

static s32 f_HeightCursorColumn = 0;
static s32 f_HeightCursorRow = 0;
static s32 f_UnusedHeightSettings[] UNUSED = {6, 0};
static Radians f_HeightCameraPitch = -EDITOR_CAMERA_START_ANGLE;
static Radians f_HeightCameraYaw = EDITOR_CAMERA_START_ANGLE;
static s32 f_UnusedHeightAngle UNUSED = BINARY_ANGLE_EIGHTH_TURN;
static WideBinaryAngle f_HeightCameraYawAngle = 0;
static s32 f_LastSetHeight = 0;
static Boolean f_ShowsGroundWaves = TRUE;
static s32 f_CLeftPickedHeight = 0;
static s32 f_CUpPickedHeight = 0;
static HeightEdit f_LastHeightEditKind = HEIGHT_EDIT_NONE;

static s32 f_UnusedHoleFileSetting UNUSED = 1;
static Boolean f_IsSavingHoleFiles = FALSE;
static HoleFileStep f_HoleFileStep = HOLE_FILE_STEP_ACTION;
static s32 f_HoleFileHoleNumber = 1;
static s32 f_HoleFileCourseNumber = 1;
static s32 f_HoleFilePromptFrames = 0;

static Radians f_TreeCameraPitch = -EDITOR_CAMERA_START_ANGLE;
static Radians f_TreeCameraYaw = EDITOR_CAMERA_START_ANGLE;
static s32 f_UnusedTreeAngle UNUSED = BINARY_ANGLE_EIGHTH_TURN;
static WideBinaryAngle f_TreeCameraYawAngle = 0;
static s32 f_EditedTreeIndex = 0;
static s32 f_LastTreeEditKind = 0;

static s32 f_EditorClipRatio = 1;
static s32 f_UnusedClipRatioValue UNUSED = 0;
static s8 f_FineTilePointSlots[FINE_TILE_POINT_SPAN][FINE_TILE_POINT_SPAN] = {
    {0, 5, 7, 10, 20},
    {1, 12, 13, 14, 21},
    {2, 15, 8, 16, 22},
    {3, 17, 18, 19, 23},
    {4, 6, 9, 11, 24},
};
static s32 f_EditorGroundTimingRow = 0;

static s32 f_MapCaptureCount = 0;
static u8 f_MapCaptureHeader[CAPTURE_HEADER_SIZE] = BMP_HEADER(
    SCREEN_CAPTURE_SIZE,
    MAP_CAPTURE_WIDTH,
    MAP_CAPTURE_HEIGHT,
    SCREEN_CAPTURE_SIZE - CAPTURE_HEADER_SIZE
);
static s32 f_ScreenCaptureCount = 0;
static u8 f_ScreenCaptureHeader[CAPTURE_HEADER_SIZE] = BMP_HEADER(
    SCREEN_CAPTURE_SIZE,
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    SCREEN_CAPTURE_SIZE - CAPTURE_HEADER_SIZE
);
static u8 f_CornerCaptureHeader[CAPTURE_HEADER_SIZE] = BMP_HEADER(
    CORNER_CAPTURE_SIZE,
    CORNER_CAPTURE_WIDTH,
    CORNER_CAPTURE_HEIGHT,
    CORNER_CAPTURE_SIZE - CAPTURE_HEADER_SIZE
);
static Vtx f_PoleVertices[POLE_QUAD_COUNT][QUAD_VERTEX_COUNT] = {
    {
        {{{-8, -560, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
        {{{8, -560, 0}, 0, {0x800, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
        {{{-8, 0, 0}, 0, {0, 0x800}, {0x00, 0x00, 0xFF, 0xFF}}},
        {{{8, 0, 0}, 0, {0x800, 0x800}, {0x00, 0x00, 0xFF, 0xFF}}},
    },
    {
        {{{0, -560, -8}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
        {{{0, -560, 8}, 0, {0x800, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
        {{{0, 0, -8}, 0, {0, 0x800}, {0x00, 0x00, 0xFF, 0xFF}}},
        {{{0, 0, 8}, 0, {0x800, 0x800}, {0x00, 0x00, 0xFF, 0xFF}}},
    },
};

void *GetEditorBuffer(void) {
    return f_EditorBuffer;
}

void UpdateCellEditor(void) {
    Vtx *corner;
    Vtx *oppositeCorner;

    if (g_MenuControllerData[0].button & R_JPAD) {
        f_CellCursorColumn++;
    }

    if (g_MenuControllerData[0].button & L_JPAD) {
        f_CellCursorColumn--;
    }

    if (g_MenuControllerData[0].button & D_JPAD) {
        f_CellCursorRow--;
    }

    if (g_MenuControllerData[0].button & U_JPAD) {
        f_CellCursorRow++;
    }

    if (f_CellCursorColumn < 0) {
        f_CellCursorColumn = 0;
    }

    if (f_CellCursorColumn >= CELL_COLUMN_COUNT) {
        f_CellCursorColumn = CELL_COLUMN_COUNT - 1;
    }

    if (f_CellCursorRow < 0) {
        f_CellCursorRow = 0;
    }

    if (f_CellCursorRow >= CELL_ROW_COUNT) {
        f_CellCursorRow = CELL_ROW_COUNT - 1;
    }

    if (g_ControllerData[0].button & R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= CURSOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MAX;
        }
    }

    if (g_ControllerData[0].button & L_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_MenuControllerData[0].button & A_BUTTON) {
        ToggleCellMirroring(f_CellCursorColumn, f_CellCursorRow);
    }

    if (g_MenuControllerData[0].trigger & Z_TRIG) {
        f_PointCursorColumn = f_CellCursorColumn;
        f_PointCursorRow = f_CellCursorRow;
        ClearDebugText();
        g_Scene = SCENE_POINT_EDITOR;
    }

    corner = GetGroundPoint(f_CellCursorColumn, f_CellCursorRow);
    oppositeCorner = GetGroundPoint(f_CellCursorColumn + 1, f_CellCursorRow + 1);
    g_MapCamera.view.eye[AXIS_X] = (corner->v.ob[AXIS_X] + oppositeCorner->v.ob[AXIS_X]) / 2;
    g_MapCamera.view.eye[AXIS_Z] = (corner->v.ob[AXIS_Z] + oppositeCorner->v.ob[AXIS_Z]) / 2;
}

Boolean HasDeadTriangleNearPoint(s32 column, s32 row);

void UpdateGroundPointEditor(void) {
    Vtx_t points[POINT_MOVE_COUNT + 1];
    char text[DEBUG_TEXT_BUFFER_SIZE];
    Vtx_t unusedPoint UNUSED;
    Boolean madeDeadTriangle;

    madeDeadTriangle = FALSE;
    if (g_MenuControllerData[EDITOR_CONTROLLER].button & R_JPAD) {
        f_PointCursorColumn++;
    }

    if (g_MenuControllerData[EDITOR_CONTROLLER].button & L_JPAD) {
        f_PointCursorColumn--;
    }

    if (g_MenuControllerData[EDITOR_CONTROLLER].button & D_JPAD) {
        f_PointCursorRow--;
    }

    if (g_MenuControllerData[EDITOR_CONTROLLER].button & U_JPAD) {
        f_PointCursorRow++;
    }

    if (f_PointCursorColumn < 0) {
        f_PointCursorColumn = 0;
    }

    if (f_PointCursorColumn >= CELL_COLUMN_COUNT) {
        f_PointCursorColumn = CELL_COLUMN_COUNT - 1;
    }

    if (f_PointCursorRow < 0) {
        f_PointCursorRow = 0;
    }

    if (f_PointCursorRow >= CELL_ROW_COUNT) {
        f_PointCursorRow = CELL_ROW_COUNT - 1;
    }

    if (!(g_MenuControllerData[EDITOR_CONTROLLER].button & B_BUTTON)) {
        if ((g_MenuControllerData[EDITOR_CONTROLLER].button & D_CBUTTONS) &&
            (f_PointCursorRow & TILE_POINT_MASK)) {
            points[0] = GetGroundPoint(f_PointCursorColumn, f_PointCursorRow)->v;
            points[1] = points[0];
            points[0].ob[AXIS_Z] -= POINT_MOVE_STEP;
            points[0].tc[TEXTURE_AXIS_T] -= POINT_TEXTURE_STEP;
            SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[0]);
            if (HasDeadTriangleNearPoint(f_PointCursorColumn, f_PointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[1]);
            }
        }

        if ((g_MenuControllerData[EDITOR_CONTROLLER].button & U_CBUTTONS) &&
            (f_PointCursorRow & TILE_POINT_MASK)) {
            points[1] = GetGroundPoint(f_PointCursorColumn, f_PointCursorRow)->v;
            points[2] = points[1];
            points[1].ob[AXIS_Z] += POINT_MOVE_STEP;
            points[1].tc[TEXTURE_AXIS_T] += POINT_TEXTURE_STEP;
            SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[1]);
            if (HasDeadTriangleNearPoint(f_PointCursorColumn, f_PointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[2]);
            }
        }

        if ((g_MenuControllerData[EDITOR_CONTROLLER].button & L_CBUTTONS) &&
            (f_PointCursorColumn & TILE_POINT_MASK)) {
            points[2] = GetGroundPoint(f_PointCursorColumn, f_PointCursorRow)->v;
            points[3] = points[2];
            points[2].ob[AXIS_X] -= POINT_MOVE_STEP;
            points[2].tc[TEXTURE_AXIS_S] -= POINT_TEXTURE_STEP;
            SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[2]);
            if (HasDeadTriangleNearPoint(f_PointCursorColumn, f_PointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[3]);
            }
        }

        if ((g_MenuControllerData[EDITOR_CONTROLLER].button & R_CBUTTONS) &&
            (f_PointCursorColumn & TILE_POINT_MASK)) {
            points[3] = GetGroundPoint(f_PointCursorColumn, f_PointCursorRow)->v;
            points[4] = points[3];
            points[3].ob[AXIS_X] += POINT_MOVE_STEP;
            points[3].tc[TEXTURE_AXIS_S] += POINT_TEXTURE_STEP;
            SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[3]);
            if (HasDeadTriangleNearPoint(f_PointCursorColumn, f_PointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[4]);
            }
        }
    }
    else {
        if ((g_MenuControllerData[EDITOR_CONTROLLER].button & D_CBUTTONS) &&
            (f_PointCursorRow & TILE_POINT_MASK)) {
            points[0] = GetGroundPoint(f_PointCursorColumn, f_PointCursorRow)->v;
            points[1] = points[0];
            points[0].tc[TEXTURE_AXIS_T] -= POINT_TEXTURE_STEP;
            SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[0]);
            if (HasDeadTriangleNearPoint(f_PointCursorColumn, f_PointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[1]);
            }
        }

        if ((g_MenuControllerData[EDITOR_CONTROLLER].button & U_CBUTTONS) &&
            (f_PointCursorRow & TILE_POINT_MASK)) {
            points[1] = GetGroundPoint(f_PointCursorColumn, f_PointCursorRow)->v;
            points[2] = points[1];
            points[1].tc[TEXTURE_AXIS_T] += POINT_TEXTURE_STEP;
            SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[1]);
            if (HasDeadTriangleNearPoint(f_PointCursorColumn, f_PointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[2]);
            }
        }

        if ((g_MenuControllerData[EDITOR_CONTROLLER].button & L_CBUTTONS) &&
            (f_PointCursorColumn & TILE_POINT_MASK)) {
            points[2] = GetGroundPoint(f_PointCursorColumn, f_PointCursorRow)->v;
            points[3] = points[2];
            points[2].tc[TEXTURE_AXIS_S] -= POINT_TEXTURE_STEP;
            SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[2]);
            if (HasDeadTriangleNearPoint(f_PointCursorColumn, f_PointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[3]);
            }
        }

        if ((g_MenuControllerData[EDITOR_CONTROLLER].button & R_CBUTTONS) &&
            (f_PointCursorColumn & TILE_POINT_MASK)) {
            points[3] = GetGroundPoint(f_PointCursorColumn, f_PointCursorRow)->v;
            points[4] = points[3];
            points[3].tc[TEXTURE_AXIS_S] += POINT_TEXTURE_STEP;
            SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[3]);
            if (HasDeadTriangleNearPoint(f_PointCursorColumn, f_PointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(f_PointCursorColumn, f_PointCursorRow, &points[4]);
            }
        }
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= CURSOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MAX;
        }
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & L_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_MenuControllerData[EDITOR_CONTROLLER].trigger & Z_TRIG) {
        f_CellCursorColumn = f_PointCursorColumn;
        f_CellCursorRow = f_PointCursorRow;
        ClearDebugText();
        g_Scene = SCENE_CELL_EDITOR;
    }

    if (f_PointEditorFirstSetting < POINT_EDITOR_SETTING_MIN) {
        f_PointEditorFirstSetting = POINT_EDITOR_SETTING_MIN;
    }

    if (f_PointEditorFirstSetting > POINT_EDITOR_SETTING_MAX) {
        f_PointEditorFirstSetting = POINT_EDITOR_SETTING_MAX;
    }

    if (f_PointEditorSecondSetting < POINT_EDITOR_SETTING_MIN) {
        f_PointEditorSecondSetting = POINT_EDITOR_SETTING_MIN;
    }

    if (f_PointEditorSecondSetting > POINT_EDITOR_SETTING_MAX) {
        f_PointEditorSecondSetting = POINT_EDITOR_SETTING_MAX;
    }
    {
        Vtx *point = GetGroundPoint(f_PointCursorColumn, f_PointCursorRow);

        g_MapCamera.view.eye[AXIS_X] = point->v.ob[AXIS_X];
        g_MapCamera.view.eye[AXIS_Z] = point->v.ob[AXIS_Z];
    }

    sprintf(
        text,
        "SuperMacro X:%2d Z:%2d",
        (int)(f_PointCursorColumn / GROUND_TILE_POINT_SPAN),
        (int)(f_PointCursorRow / GROUND_TILE_POINT_SPAN)
    );
    PrintDebugText(text, POINT_TEXT_COLUMN, TILE_TEXT_ROW);
    sprintf(
        text,
        "SubX:%d SubZ:%d",
        (int)(f_PointCursorColumn & TILE_POINT_MASK),
        (int)(f_PointCursorRow & TILE_POINT_MASK)
    );
    PrintDebugText(text, POINT_TEXT_COLUMN, TILE_POINT_TEXT_ROW);
    SetDebugTextHighlight(TRUE);
    if (!(f_PointCursorColumn & TILE_POINT_MASK)) {
        sprintf(text, "Can't Move X-Axis");
    }
    else {
        sprintf(text, "                 ");
    }

    PrintDebugText(text, POINT_TEXT_COLUMN, X_LIMIT_TEXT_ROW);
    SetDebugTextHighlight(TRUE);
    if (!(f_PointCursorRow & TILE_POINT_MASK)) {
        sprintf(text, "Can't Move Z-Axis");
    }
    else {
        sprintf(text, "                 ");
    }

    PrintDebugText(text, POINT_TEXT_COLUMN, Z_LIMIT_TEXT_ROW);
    sprintf(
        text,
        "Height:%dM   ",
        -GetGroundPoint(f_PointCursorColumn, f_PointCursorRow)->v.ob[AXIS_Y] /
            HEIGHT_UNITS_PER_METER
    );
    PrintDebugText(text, POINT_TEXT_COLUMN, POINT_HEIGHT_TEXT_ROW);
    if (madeDeadTriangle) {
        sprintf(text, "DeadTriangle");
    }
    else {
        sprintf(text, "             ");
    }

    PrintDebugText(text, POINT_TEXT_COLUMN, DEAD_TRIANGLE_TEXT_ROW);
}

void UpdateHolePointEditor(void) {
    char text[DEBUG_TEXT_BUFFER_SIZE];
    s32 stepX;
    s32 stepZ;
    Boolean isStepping;
    f64 cameraDistance;

    sprintf(
        text,
        "X:%d Z:%d             ",
        (int)f_HolePointCursor[GROUND_AXIS_X],
        (int)f_HolePointCursor[GROUND_AXIS_Z]
    );
    PrintDebugText(text, CURSOR_TEXT_COLUMN, CURSOR_TEXT_ROW);
    g_ShowsGroundWaves = FALSE;
    isStepping = FALSE;
    if (f_HolePointEditMode == HOLE_POINT_MODE_UNSET) {
        f_HolePointEditMode = HOLE_POINT_MODE_PIN_TEE;
        f_HolePointCursor[GROUND_AXIS_X] =
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
        f_HolePointCursor[GROUND_AXIS_Z] =
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
    }

    if (g_MenuControllerData[EDITOR_CONTROLLER].button & R_CBUTTONS) {
        ClearDebugText();
        f_HolePointEditMode++;
        if (f_HolePointEditMode == HOLE_POINT_MODE_COUNT) {
            f_HolePointEditMode = HOLE_POINT_MODE_COUNT - 1;
        }

        if (f_HolePointEditMode == HOLE_POINT_MODE_PIN_TEE) {
            f_HolePointCursor[GROUND_AXIS_X] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[GROUND_AXIS_Z] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
            f_AimPointIndex = 0;
            f_RoutePointIndex = 0;
        }

        if (f_HolePointEditMode == HOLE_POINT_MODE_AIM) {
            f_HolePointCursor[GROUND_AXIS_X] =
                g_HolePoints.aimPoints[f_AimPointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[GROUND_AXIS_Z] =
                g_HolePoints.aimPoints[f_AimPointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        if (f_HolePointEditMode == HOLE_POINT_MODE_ROUTE) {
            f_HolePointCursor[GROUND_AXIS_X] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[GROUND_AXIS_Z] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }
    }

    if (g_MenuControllerData[EDITOR_CONTROLLER].button & L_CBUTTONS) {
        ClearDebugText();
        f_HolePointEditMode--;
        if (f_HolePointEditMode < 0) {
            f_HolePointEditMode = HOLE_POINT_MODE_PIN_TEE;
        }

        if (f_HolePointEditMode == HOLE_POINT_MODE_PIN_TEE) {
            f_HolePointCursor[GROUND_AXIS_X] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[GROUND_AXIS_Z] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
            f_AimPointIndex = 0;
            f_RoutePointIndex = 0;
        }

        if (f_HolePointEditMode == HOLE_POINT_MODE_AIM) {
            f_HolePointCursor[GROUND_AXIS_X] =
                g_HolePoints.aimPoints[f_AimPointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[GROUND_AXIS_Z] =
                g_HolePoints.aimPoints[f_AimPointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        if (f_HolePointEditMode == HOLE_POINT_MODE_ROUTE) {
            f_HolePointCursor[GROUND_AXIS_X] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[GROUND_AXIS_Z] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }
    }

    if (f_HolePointEditMode == HOLE_POINT_MODE_PIN_TEE) {
        if (g_MenuControllerData[EDITOR_CONTROLLER].button & A_BUTTON) {
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] = f_HolePointCursor[GROUND_AXIS_X]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] = f_HolePointCursor[GROUND_AXIS_Z]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Y] = GetGroundHeight(
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X],
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z]
            );
            PlaySound(SOUND_MENU_CONFIRM, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
            PrintDebugText(
                GetSurfaceTraitsAt(
                    g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X],
                    g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z]
                )
                    ->name,
                SURFACE_TEXT_COLUMN,
                SURFACE_TEXT_ROW
            );
            if (f_PinTeeIndex == DEFAULT_CUP) {
                g_HoleSetup.cupPosition[AXIS_X] = g_HolePoints.cupPositions[DEFAULT_CUP][AXIS_X];
                g_HoleSetup.cupPosition[AXIS_Y] = g_HolePoints.cupPositions[DEFAULT_CUP][AXIS_Y];
                g_HoleSetup.cupPosition[AXIS_Z] = g_HolePoints.cupPositions[DEFAULT_CUP][AXIS_Z];
            }
        }

        if (g_MenuControllerData[EDITOR_CONTROLLER].button & B_BUTTON) {
            g_HolePoints.teePositions[f_PinTeeIndex][AXIS_X] = f_HolePointCursor[GROUND_AXIS_X]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Z] = f_HolePointCursor[GROUND_AXIS_Z]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Y] = GetGroundHeight(
                g_HolePoints.teePositions[f_PinTeeIndex][AXIS_X],
                g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Z]
            );
            PlaySound(SOUND_MENU_CONFIRM, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
            PrintDebugText(
                GetSurfaceTraitsAt(
                    g_HolePoints.teePositions[f_PinTeeIndex][AXIS_X],
                    g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Z]
                )
                    ->name,
                SURFACE_TEXT_COLUMN,
                SURFACE_TEXT_ROW
            );
            if (f_PinTeeIndex == PLAYED_TEE) {
                g_HoleSetup.teePosition[AXIS_X] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_X];
                g_HoleSetup.teePosition[AXIS_Y] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_Y];
                g_HoleSetup.teePosition[AXIS_Z] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_Z];
            }
        }

        if (g_MenuControllerData[OPTION_CONTROLLER].button & B_BUTTON) {
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] = 0;
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Y] = 0;
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] = 0;
            g_HolePoints.teePositions[f_PinTeeIndex][AXIS_X] = 0;
            g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Y] = 0;
            g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Z] = 0;
            f_HolePointCursor[GROUND_AXIS_X] = 0;
            f_HolePointCursor[GROUND_AXIS_Z] = 0;
        }

        if (g_MenuControllerData[EDITOR_CONTROLLER].button & U_CBUTTONS) {
            f_PinTeeIndex++;
            if (f_PinTeeIndex == CUP_POSITION_COUNT) {
                f_PinTeeIndex = 0;
            }

            f_HolePointCursor[GROUND_AXIS_X] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[GROUND_AXIS_Z] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        if (g_MenuControllerData[EDITOR_CONTROLLER].button & D_CBUTTONS) {
            f_PinTeeIndex--;
            if (f_PinTeeIndex < 0) {
                f_PinTeeIndex = CUP_POSITION_COUNT - 1;
            }

            f_HolePointCursor[GROUND_AXIS_X] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[GROUND_AXIS_Z] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        if (g_MenuControllerData[EDITOR_CONTROLLER].button & START_BUTTON) {
            f_HolePointCursor[GROUND_AXIS_X] =
                g_HolePoints.teePositions[f_PinTeeIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[GROUND_AXIS_Z] =
                g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        PrintDebugText("Pin Tee Edit Mode", HOLE_POINT_TEXT_COLUMN, HOLE_POINT_MODE_TEXT_ROW);
        PrintDebugHex(f_PinTeeIndex, HOLE_POINT_TEXT_COLUMN, HOLE_POINT_INDEX_TEXT_ROW);
    }

    if (f_HolePointEditMode == HOLE_POINT_MODE_AIM) {
        if (g_MenuControllerData[EDITOR_CONTROLLER].button & A_BUTTON) {
            g_HolePoints.aimPoints[f_AimPointIndex][AXIS_X] = f_HolePointCursor[GROUND_AXIS_X]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.aimPoints[f_AimPointIndex][AXIS_Z] = f_HolePointCursor[GROUND_AXIS_Z]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.aimPoints[f_AimPointIndex][AXIS_Y] = GetGroundHeight(
                g_HolePoints.aimPoints[f_AimPointIndex][AXIS_X],
                g_HolePoints.aimPoints[f_AimPointIndex][AXIS_Z]
            );
            PlaySound(SOUND_MENU_CONFIRM, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
            PrintDebugText(
                GetSurfaceTraitsAt(
                    g_HolePoints.aimPoints[f_AimPointIndex][AXIS_X],
                    g_HolePoints.aimPoints[f_AimPointIndex][AXIS_Z]
                )
                    ->name,
                SURFACE_TEXT_COLUMN,
                SURFACE_TEXT_ROW
            );
        }

        if (g_MenuControllerData[OPTION_CONTROLLER].button & B_BUTTON) {
            g_HolePoints.aimPoints[f_AimPointIndex][AXIS_X] = 0;
            g_HolePoints.aimPoints[f_AimPointIndex][AXIS_Y] = 0;
            g_HolePoints.aimPoints[f_AimPointIndex][AXIS_Z] = 0;
            f_HolePointCursor[GROUND_AXIS_X] = 0;
            f_HolePointCursor[GROUND_AXIS_Z] = 0;
        }

        if (g_MenuControllerData[EDITOR_CONTROLLER].button & U_CBUTTONS) {
            f_AimPointIndex++;
            if (f_AimPointIndex == ARRAY_COUNT(g_HolePoints.aimPoints)) {
                f_AimPointIndex = 0;
            }

            f_HolePointCursor[GROUND_AXIS_X] =
                g_HolePoints.aimPoints[f_AimPointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[GROUND_AXIS_Z] =
                g_HolePoints.aimPoints[f_AimPointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        if (g_MenuControllerData[EDITOR_CONTROLLER].button & D_CBUTTONS) {
            f_AimPointIndex--;
            if (f_AimPointIndex < 0) {
                f_AimPointIndex = ARRAY_COUNT(g_HolePoints.aimPoints) - 1;
            }

            f_HolePointCursor[GROUND_AXIS_X] =
                g_HolePoints.aimPoints[f_AimPointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[GROUND_AXIS_Z] =
                g_HolePoints.aimPoints[f_AimPointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        PrintDebugText("Meson Edit Mode", HOLE_POINT_TEXT_COLUMN, HOLE_POINT_MODE_TEXT_ROW);
        PrintDebugHex(f_AimPointIndex, HOLE_POINT_TEXT_COLUMN, HOLE_POINT_INDEX_TEXT_ROW);
    }

    if (f_HolePointEditMode == HOLE_POINT_MODE_ROUTE) {
        if (g_MenuControllerData[EDITOR_CONTROLLER].button & A_BUTTON) {
            g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X] = f_HolePointCursor[GROUND_AXIS_X]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z] = f_HolePointCursor[GROUND_AXIS_Z]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Y] = GetGroundHeight(
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X],
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z]
            );
            PlaySound(SOUND_MENU_CONFIRM, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
            PrintDebugText(
                GetSurfaceTraitsAt(
                    g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X],
                    g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z]
                )
                    ->name,
                SURFACE_TEXT_COLUMN,
                SURFACE_TEXT_ROW
            );
        }

        if (g_MenuControllerData[OPTION_CONTROLLER].button & B_BUTTON) {
            g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X] = 0;
            g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Y] = 0;
            g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z] = 0;
            f_HolePointCursor[GROUND_AXIS_X] = 0;
            f_HolePointCursor[GROUND_AXIS_Z] = 0;
        }

        if (g_MenuControllerData[EDITOR_CONTROLLER].button & U_CBUTTONS) {
            f_RoutePointIndex++;
            if (f_RoutePointIndex == ROUTE_POINT_COUNT) {
                f_RoutePointIndex = 0;
            }

            f_HolePointCursor[GROUND_AXIS_X] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[GROUND_AXIS_Z] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        if (g_MenuControllerData[EDITOR_CONTROLLER].button & D_CBUTTONS) {
            f_RoutePointIndex--;
            if (f_RoutePointIndex < 0) {
                f_RoutePointIndex = ROUTE_POINT_COUNT - 1;
            }

            f_HolePointCursor[GROUND_AXIS_X] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[GROUND_AXIS_Z] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        PrintDebugText("AI Edit Mode", HOLE_POINT_TEXT_COLUMN, HOLE_POINT_MODE_TEXT_ROW);
        PrintDebugHex(f_RoutePointIndex, HOLE_POINT_TEXT_COLUMN, HOLE_POINT_INDEX_TEXT_ROW);
    }

    if (!(g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG)) {
        if (*(u16 *)&g_MenuControllerData[EDITOR_CONTROLLER].stick_x != 0) {
            stepX = (-cos(HEADING_RADIANS) * g_MenuControllerData[EDITOR_CONTROLLER].stick_y) -
                (sin(HEADING_RADIANS) * g_MenuControllerData[EDITOR_CONTROLLER].stick_x);
            stepZ = (-sin(HEADING_RADIANS) * g_MenuControllerData[EDITOR_CONTROLLER].stick_y) +
                (cos(HEADING_RADIANS) * g_MenuControllerData[EDITOR_CONTROLLER].stick_x);
            f_HolePointCursor[GROUND_AXIS_X] += stepX;
            f_HolePointCursor[GROUND_AXIS_Z] += stepZ;
        }
        else {
            if (g_MenuControllerData[EDITOR_CONTROLLER].button & U_JPAD) {
                stepX = -cos(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
                isStepping = TRUE;
                stepZ = -sin(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
            }

            if (g_MenuControllerData[EDITOR_CONTROLLER].button & D_JPAD) {
                stepX = cos(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
                isStepping = TRUE;
                stepZ = sin(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
            }

            if (g_MenuControllerData[EDITOR_CONTROLLER].button & R_JPAD) {
                stepX = -sin(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
                isStepping = TRUE;
                stepZ = cos(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
            }

            if (g_MenuControllerData[EDITOR_CONTROLLER].button & L_JPAD) {
                stepX = sin(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
                isStepping = TRUE;
                stepZ = -cos(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
            }

            if (isStepping) {
                f_HolePointCursor[GROUND_AXIS_X] += stepX;
                f_HolePointCursor[GROUND_AXIS_Z] += stepZ;
            }
        }
    }

    if ((g_ControllerData[EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[EDITOR_CONTROLLER].button & L_TRIG) ||
        (g_ControllerData[EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG) {
        f_HolePointCameraYawAngle -=
            g_MenuControllerData[EDITOR_CONTROLLER].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_HolePointCameraPitch -=
            g_MenuControllerData[EDITOR_CONTROLLER].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }

    if (f_HolePointCameraPitch > 0.0f) {
        f_HolePointCameraPitch = 0.0f;
    }

    if (f_HolePointCameraPitch < EDITOR_PITCH_MIN) {
        f_HolePointCameraPitch = EDITOR_PITCH_MIN;
    }

    f_HolePointCameraYaw = f_HolePointCameraYawAngle * EDITOR_ANGLE_TO_RADIANS;
    g_MapCamera.view.lookAt[AXIS_X] = f_HolePointCursor[GROUND_AXIS_X];
    g_MapCamera.view.lookAt[AXIS_Z] = f_HolePointCursor[GROUND_AXIS_Z];
    g_MapCamera.view.lookAt[AXIS_Y] = GetGroundHeight(
                                          f_HolePointCursor[GROUND_AXIS_X] << WORLD_TO_COURSE_SHIFT,
                                          f_HolePointCursor[GROUND_AXIS_Z] << WORLD_TO_COURSE_SHIFT
                                      ) *
        (1.0f / (1 << WORLD_TO_COURSE_SHIFT));
    cameraDistance = EDITOR_CAMERA_DISTANCE;
    g_MapCamera.view.eye[AXIS_X] = (cos(f_HolePointCameraPitch) * cos(f_HolePointCameraYaw) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = (cos(f_HolePointCameraPitch) * sin(f_HolePointCameraYaw) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] =
        (sin(f_HolePointCameraPitch) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Y];
}

void UpdateTriangleSlopes(void);

void UpdateTriangleEditor(void) {
    Mat4 unusedMatrices[2] UNUSED;
    f32 stepRight;
    f32 stepUp;
    f32 stepY;
    f32 stepX;
    f32 stepZ;
    f32 moveY;
    TriangleEdit editKind;
    s32 corner;
    f64 cameraDistance;
    Vtx_t *lookAtVertex;

    editKind = TRIANGLE_EDIT_NONE;
    g_ShowsGroundWaves = FALSE;
    stepY = 0.0f;
    stepRight = stepY;
    stepUp = stepRight;
    if (g_ControllerData[EDITOR_CONTROLLER].button & U_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepUp = 1.0f;
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & D_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepUp = -1.0f;
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & R_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepRight = 1.0f;
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & L_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepRight = -1.0f;
    }

    if (g_ControllerData[OPTION_CONTROLLER].button & D_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepY = 1.0f;
    }

    if (g_ControllerData[OPTION_CONTROLLER].button & U_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepY = -1.0f;
    }

    if (g_MenuControllerData[EDITOR_CONTROLLER].button & B_BUTTON) {
        f_EditedCornerIndex = (f_EditedCornerIndex + 1) % TRIANGLE_VERTEX_COUNT;
    }

    PrintDebugHex(editKind, TRIANGLE_TEXT_COLUMN, EDIT_KIND_TEXT_ROW);
    if (editKind == TRIANGLE_EDIT_MOVE) {
        stepX = (-cos((u16)f_TriangleCameraYawAngle * BINARY_ANGLE_TO_RADIANS) * stepUp) -
            (sin((u16)f_TriangleCameraYawAngle * BINARY_ANGLE_TO_RADIANS) * stepRight);
        stepZ = (-sin((u16)f_TriangleCameraYawAngle * BINARY_ANGLE_TO_RADIANS) * stepUp) +
            (cos((u16)f_TriangleCameraYawAngle * BINARY_ANGLE_TO_RADIANS) * stepRight);
        {
            f32 scaledX = stepX * CORNER_STEP_SCALE;
            f32 scaledY = stepY * CORNER_STEP_SCALE;
            f32 scaledZ = stepZ * CORNER_STEP_SCALE;

            stepX = scaledX * g_MapCamera.view.zoom;
            moveY = scaledY * g_MapCamera.view.zoom;
            stepZ = scaledZ * g_MapCamera.view.zoom;
        }

        for (corner = 0; corner < TRIANGLE_VERTEX_COUNT; corner++) {
            if (!(g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG) ||
                f_EditedCornerIndex == corner) {
                Vtx_t *vertex;

                vertex = &g_WorkArea.hole.triangles[f_EditedTriangleIndex].vertices[corner].v;
                vertex->ob[AXIS_X] = vertex->ob[AXIS_X] + stepX;
                vertex->ob[AXIS_Y] = vertex->ob[AXIS_Y] + moveY;
                vertex->ob[AXIS_Z] = vertex->ob[AXIS_Z] + stepZ;
                PrintDebugHex(vertex->ob[AXIS_Y], TRIANGLE_TEXT_COLUMN, CORNER_HEIGHT_TEXT_ROW);
            }
        }

        PrepareHoleTriangles();
    }

    if (g_MenuControllerData[OPTION_CONTROLLER].button & A_BUTTON) {
        Vtx_t *vertex;

        vertex = &g_WorkArea.hole.triangles[f_EditedTriangleIndex].vertices[f_EditedCornerIndex].v;
        f_SavedCornerIndex = (f_SavedCornerIndex + 1) % SAVED_CORNER_COUNT;
        f_SavedCorners[f_SavedCornerIndex][AXIS_X] = vertex->ob[AXIS_X];
        f_SavedCorners[f_SavedCornerIndex][AXIS_Y] = vertex->ob[AXIS_Y];
        f_SavedCorners[f_SavedCornerIndex][AXIS_Z] = vertex->ob[AXIS_Z];
        PrepareHoleTriangles();
    }

    if (g_MenuControllerData[OPTION_CONTROLLER].button & B_BUTTON) {
        Vtx_t *vertex;

        vertex = &g_WorkArea.hole.triangles[f_EditedTriangleIndex].vertices[f_EditedCornerIndex].v;
        vertex->ob[AXIS_X] = f_SavedCorners[f_SavedCornerIndex][AXIS_X];
        vertex->ob[AXIS_Y] = f_SavedCorners[f_SavedCornerIndex][AXIS_Y];
        vertex->ob[AXIS_Z] = f_SavedCorners[f_SavedCornerIndex][AXIS_Z];
        f_SavedCornerIndex = (f_SavedCornerIndex + SAVED_CORNER_COUNT - 1) % SAVED_CORNER_COUNT;
        PrepareHoleTriangles();
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & R_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        f_EditedTriangleIndex++;
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & L_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        f_EditedTriangleIndex--;
    }

    if (f_EditedTriangleIndex < 0) {
        f_EditedTriangleIndex = FIRST_RING_TRIANGLE - 1;
    }

    if (f_EditedTriangleIndex >= FIRST_RING_TRIANGLE) {
        f_EditedTriangleIndex = 0;
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & U_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        g_WorkArea.hole.triangles[f_EditedTriangleIndex].kind++;
        editKind = TRIANGLE_EDIT_KIND;
        if (g_WorkArea.hole.triangles[f_EditedTriangleIndex].kind >= HOLE_TRIANGLE_KIND_COUNT) {
            g_WorkArea.hole.triangles[f_EditedTriangleIndex].kind = HOLE_TRIANGLE_KIND_FLAT;
        }

        PrepareHoleTriangles();
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & D_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        g_WorkArea.hole.triangles[f_EditedTriangleIndex].kind--;
        editKind = TRIANGLE_EDIT_KIND;
        PrepareHoleTriangles();
    }

    if ((g_ControllerData[EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[EDITOR_CONTROLLER].button & L_TRIG) ||
        (g_ControllerData[EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG) {
        f_TriangleCameraYawAngle -=
            g_MenuControllerData[EDITOR_CONTROLLER].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_TriangleCameraPitch -=
            g_MenuControllerData[EDITOR_CONTROLLER].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }

    if (f_TriangleCameraPitch > 0.0f) {
        f_TriangleCameraPitch = 0.0f;
    }

    if (f_TriangleCameraPitch < EDITOR_PITCH_MIN) {
        f_TriangleCameraPitch = EDITOR_PITCH_MIN;
    }

    f_TriangleCameraYaw = f_TriangleCameraYawAngle * EDITOR_ANGLE_TO_RADIANS;
    if ((f_LastTriangleEditKind != TRIANGLE_EDIT_NONE) & (editKind == TRIANGLE_EDIT_NONE)) {
        UpdateTriangleSlopes();
    }

    cameraDistance = EDITOR_CAMERA_DISTANCE;
    lookAtVertex =
        &g_WorkArea.hole.triangles[f_EditedTriangleIndex].vertices[f_EditedCornerIndex].v;
    g_MapCamera.view.lookAt[AXIS_X] = lookAtVertex->ob[AXIS_X];
    g_MapCamera.view.lookAt[AXIS_Y] = lookAtVertex->ob[AXIS_Y];
    f_LastTriangleEditKind = editKind;
    g_MapCamera.view.lookAt[AXIS_Z] = lookAtVertex->ob[AXIS_Z];
    g_MapCamera.view.eye[AXIS_X] = (cos(f_TriangleCameraPitch) * cos(f_TriangleCameraYaw) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = (cos(f_TriangleCameraPitch) * sin(f_TriangleCameraYaw) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] =
        (sin(f_TriangleCameraPitch) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Y];
}

void UpdateRingEditor(void) {
    Vec3 direction;
    f32 stepRight;
    f32 stepUp;
    f32 stepY;
    f32 stepX;
    f32 stepZ;
    f32 moveY;
    TriangleEdit editKind;
    s32 corner;
    f64 cameraDistance;
    Vtx_t *lookAtVertex;

    editKind = TRIANGLE_EDIT_NONE;
    stepY = 0.0f;
    g_ShowsGroundWaves = FALSE;
    sprintf(g_PrintBuffer, "EditNo : %d   ", (int)f_RingTriangleIndex);
    PrintDebugText(g_PrintBuffer, RING_TEXT_COLUMN, RING_TEXT_ROW);
    stepRight = 0.0f;
    stepUp = stepRight;
    if (!(g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG)) {
        if (g_ControllerData[EDITOR_CONTROLLER].button & U_CBUTTONS) {
            editKind = TRIANGLE_EDIT_MOVE;
            stepUp = 1.0f;
        }

        if (g_ControllerData[EDITOR_CONTROLLER].button & D_CBUTTONS) {
            editKind = TRIANGLE_EDIT_MOVE;
            stepUp = -1.0f;
        }

        if (g_ControllerData[EDITOR_CONTROLLER].button & R_CBUTTONS) {
            editKind = TRIANGLE_EDIT_MOVE;
            stepRight = 1.0f;
        }

        if (g_ControllerData[EDITOR_CONTROLLER].button & L_CBUTTONS) {
            editKind = TRIANGLE_EDIT_MOVE;
            stepRight = -1.0f;
        }
    }
    else {
        Vtx_t *scaleVertex =
            &g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[RING_SCALE_CORNER].v;

        if (g_ControllerData[EDITOR_CONTROLLER].button & U_CBUTTONS) {
            scaleVertex->ob[AXIS_X] += RING_SCALE_STEP;
            if (scaleVertex->ob[AXIS_X] >= RING_SCALE_MAX) {
                scaleVertex->ob[AXIS_X] = RING_SCALE_MAX;
            }

            PrepareHoleTriangles();
        }

        if (g_ControllerData[EDITOR_CONTROLLER].button & D_CBUTTONS) {
            scaleVertex->ob[AXIS_X] -= RING_SCALE_STEP;
            if (scaleVertex->ob[AXIS_X] < RING_SCALE_MIN) {
                scaleVertex->ob[AXIS_X] = RING_SCALE_MIN;
            }

            PrepareHoleTriangles();
        }
    }

    if (g_ControllerData[OPTION_CONTROLLER].button & D_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepY = 1.0f;
    }

    if (g_ControllerData[OPTION_CONTROLLER].button & U_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepY = -1.0f;
    }

    PrintDebugHex(editKind, TRIANGLE_TEXT_COLUMN, EDIT_KIND_TEXT_ROW);
    if (editKind == TRIANGLE_EDIT_MOVE) {
        stepX = (-cos((u16)f_RingCameraYawAngle * BINARY_ANGLE_TO_RADIANS) * stepUp) -
            (sin((u16)f_RingCameraYawAngle * BINARY_ANGLE_TO_RADIANS) * stepRight);
        stepZ = (-sin((u16)f_RingCameraYawAngle * BINARY_ANGLE_TO_RADIANS) * stepUp) +
            (cos((u16)f_RingCameraYawAngle * BINARY_ANGLE_TO_RADIANS) * stepRight);
        {
            f32 scaledX = stepX * CORNER_STEP_SCALE;
            f32 scaledY = stepY * CORNER_STEP_SCALE;
            f32 scaledZ = stepZ * CORNER_STEP_SCALE;

            stepX = scaledX * g_MapCamera.view.zoom;
            moveY = scaledY * g_MapCamera.view.zoom;
            stepZ = scaledZ * g_MapCamera.view.zoom;
        }

        for (corner = 0; corner < TRIANGLE_VERTEX_COUNT; corner++) {
            if (f_RingCornerIndex == corner) {
                Vtx_t *vertex;

                vertex = &g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[corner].v;
                vertex->ob[AXIS_X] = vertex->ob[AXIS_X] + stepX;
                vertex->ob[AXIS_Y] = vertex->ob[AXIS_Y] + moveY;
                vertex->ob[AXIS_Z] = vertex->ob[AXIS_Z] + stepZ;
            }
        }

        PrepareHoleTriangles();
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & A_BUTTON) {
        Vtx_t *directionVertex;

        g_WorkArea.hole.triangles[f_RingTriangleIndex].kind = HOLE_TRIANGLE_KIND_RING;
        directionVertex =
            &g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[RING_DIRECTION_CORNER].v;
        direction[AXIS_X] = g_MapCamera.view.lookAt[AXIS_X] - g_MapCamera.view.eye[AXIS_X];
        direction[AXIS_Y] = g_MapCamera.view.lookAt[AXIS_Y] - g_MapCamera.view.eye[AXIS_Y];
        direction[AXIS_Z] = g_MapCamera.view.lookAt[AXIS_Z] - g_MapCamera.view.eye[AXIS_Z];
        guNormalize(&direction[AXIS_X], &direction[AXIS_Y], &direction[AXIS_Z]);
        directionVertex->ob[AXIS_X] = direction[AXIS_X] * RING_DIRECTION_LENGTH;
        directionVertex->ob[AXIS_Y] = direction[AXIS_Y] * RING_DIRECTION_LENGTH;
        directionVertex->ob[AXIS_Z] = direction[AXIS_Z] * RING_DIRECTION_LENGTH;
        PrepareHoleTriangles();
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & R_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        f_RingTriangleIndex++;
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & L_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        f_RingTriangleIndex--;
    }

    if (f_RingTriangleIndex < FIRST_RING_TRIANGLE) {
        f_RingTriangleIndex = HOLE_TRIANGLE_COUNT - 1;
    }

    if (f_RingTriangleIndex >= HOLE_TRIANGLE_COUNT) {
        f_RingTriangleIndex = FIRST_RING_TRIANGLE;
    }

    if (g_ControllerData[SECOND_EDITOR_CONTROLLER].button & B_BUTTON) {
        g_WorkArea.hole.triangles[f_RingTriangleIndex].kind = HOLE_TRIANGLE_KIND_FLAT;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.ob[AXIS_X] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.ob[AXIS_Y] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.ob[AXIS_Z] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.flag = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.tc[TEXTURE_AXIS_S] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.tc[TEXTURE_AXIS_T] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.cn[RED_CHANNEL] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.cn[GREEN_CHANNEL] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.cn[BLUE_CHANNEL] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.cn[ALPHA_CHANNEL] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.ob[AXIS_X] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.ob[AXIS_Y] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.ob[AXIS_Z] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.flag = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.tc[TEXTURE_AXIS_S] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.tc[TEXTURE_AXIS_T] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.cn[RED_CHANNEL] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.cn[GREEN_CHANNEL] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.cn[BLUE_CHANNEL] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.cn[ALPHA_CHANNEL] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.ob[AXIS_X] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.ob[AXIS_Y] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.ob[AXIS_Z] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.flag = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.tc[TEXTURE_AXIS_S] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.tc[TEXTURE_AXIS_T] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.cn[RED_CHANNEL] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.cn[GREEN_CHANNEL] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.cn[BLUE_CHANNEL] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.cn[ALPHA_CHANNEL] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].bounds[AXIS_X][BOUND_LOWEST] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].bounds[AXIS_X][BOUND_HIGHEST] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].bounds[AXIS_Y][BOUND_LOWEST] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].bounds[AXIS_Y][BOUND_HIGHEST] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].bounds[AXIS_Z][BOUND_LOWEST] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].bounds[AXIS_Z][BOUND_HIGHEST] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].ignoredCenter[AXIS_X] = 0.0f;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].ignoredCenter[AXIS_Y] = 0.0f;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].ignoredCenter[AXIS_Z] = 0.0f;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].unusedEditorValues[0] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].unusedEditorValues[1] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].unusedEditorValues[2] = 0;
    }

    if ((g_ControllerData[EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[EDITOR_CONTROLLER].button & L_TRIG) ||
        (g_ControllerData[EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG) {
        f_RingCameraYawAngle -=
            g_MenuControllerData[EDITOR_CONTROLLER].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_RingCameraPitch -=
            g_MenuControllerData[EDITOR_CONTROLLER].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }

    if (f_RingCameraPitch > 0.0f) {
        f_RingCameraPitch = 0.0f;
    }

    if (f_RingCameraPitch < EDITOR_PITCH_MIN) {
        f_RingCameraPitch = EDITOR_PITCH_MIN;
    }

    f_RingCameraYaw = f_RingCameraYawAngle * EDITOR_ANGLE_TO_RADIANS;

    if ((f_LastRingEditKind != TRIANGLE_EDIT_NONE) & (editKind == TRIANGLE_EDIT_NONE)) {
        UpdateTriangleSlopes();
    }

    cameraDistance = EDITOR_CAMERA_DISTANCE;
    lookAtVertex = &g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[f_RingCornerIndex].v;
    g_MapCamera.view.lookAt[AXIS_X] = lookAtVertex->ob[AXIS_X];
    g_MapCamera.view.lookAt[AXIS_Y] = lookAtVertex->ob[AXIS_Y];
    f_LastRingEditKind = editKind;
    g_MapCamera.view.lookAt[AXIS_Z] = lookAtVertex->ob[AXIS_Z];
    g_MapCamera.view.eye[AXIS_X] =
        (cos(f_RingCameraPitch) * cos(f_RingCameraYaw) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] =
        (cos(f_RingCameraPitch) * sin(f_RingCameraYaw) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] =
        (sin(f_RingCameraPitch) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Y];
}

void UpdateWallEditor(void) {
    char text[DEBUG_TEXT_BUFFER_SIZE];
    Vtx_t *corners[TRIANGLE_VERTEX_COUNT];
    Vtx_t *nextCorners[TRIANGLE_VERTEX_COUNT];
    Mat4 unusedMatrices[2] UNUSED;
    f32 stepRight;
    f32 stepUp;
    f32 stepY;
    f32 stepX;
    f32 stepZ;
    f32 moveY;
    TriangleEdit editKind;
    Boolean isDropping;
    s32 corner;
    f64 cameraDistance;
    Vtx_t *lookAtVertex;

    editKind = TRIANGLE_EDIT_NONE;
    isDropping = FALSE;
    g_ShowsGroundWaves = FALSE;
    stepY = 0.0f;
    stepRight = stepY;
    stepUp = stepRight;
    if (g_ControllerData[EDITOR_CONTROLLER].button & U_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepUp = 1.0f;
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & D_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepUp = -1.0f;
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & R_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepRight = 1.0f;
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & L_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepRight = -1.0f;
    }

    if (g_ControllerData[OPTION_CONTROLLER].button & D_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepY = 1.0f;
    }

    if (g_ControllerData[OPTION_CONTROLLER].button & U_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepY = -1.0f;
    }

    if (g_ControllerData[OPTION_CONTROLLER].button & L_CBUTTONS) {
        isDropping = TRUE;
    }

    if (g_MenuControllerData[EDITOR_CONTROLLER].button & B_BUTTON) {
        f_WallCornerIndex = (f_WallCornerIndex + 1) % WALL_BOTTOM_CORNER_COUNT;
    }

    PrintDebugHex(editKind, TRIANGLE_TEXT_COLUMN, EDIT_KIND_TEXT_ROW);
    sprintf(text, "EditNo%d   ", f_WallTriangleIndex);
    PrintDebugText(text, WALL_TEXT_COLUMN, WALL_TEXT_ROW);
    if ((editKind == TRIANGLE_EDIT_MOVE) | (isDropping == TRUE)) {
        stepZ = stepUp;
        stepX = (-cos((u16)f_WallCameraYawAngle * BINARY_ANGLE_TO_RADIANS) * stepUp) -
            (sin((u16)f_WallCameraYawAngle * BINARY_ANGLE_TO_RADIANS) * stepRight);
        stepX = (-sin((u16)f_WallCameraYawAngle * BINARY_ANGLE_TO_RADIANS) * stepUp) +
            (cos((u16)f_WallCameraYawAngle * BINARY_ANGLE_TO_RADIANS) * stepRight);

        {
            f32 scaledX = stepRight * CORNER_STEP_SCALE;
            f32 scaledY = stepY * CORNER_STEP_SCALE;
            f32 scaledZ = stepZ * CORNER_STEP_SCALE;

            stepX = scaledX * g_MapCamera.view.zoom;
            moveY = scaledY * g_MapCamera.view.zoom;
            stepZ = scaledZ * g_MapCamera.view.zoom;
        }

        for (corner = 0; corner < TRIANGLE_VERTEX_COUNT; corner++) {
            if (!(g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG) ||
                f_WallCornerIndex == corner) {
                Vtx_t *vertex;

                vertex = &g_WorkArea.hole.triangles[f_WallTriangleIndex].vertices[corner].v;
                if (isDropping == TRUE) {
                    s32 height = (GetGroundHeight(
                                      vertex->ob[AXIS_X] << WORLD_TO_COURSE_SHIFT,
                                      vertex->ob[AXIS_Z] << WORLD_TO_COURSE_SHIFT
                                  ) - WALL_SINK_DEPTH) *
                        (1.0f / (1 << WORLD_TO_COURSE_SHIFT));

                    if (corner == WALL_TOP_CORNER) {
                        height += WALL_HEIGHT;
                        vertex->ob[AXIS_Y] = height;
                    }
                    else {
                        vertex->ob[AXIS_Y] = height;
                    }
                }
                else {
                    vertex->ob[AXIS_X] = vertex->ob[AXIS_X] + stepX;
                    vertex->ob[AXIS_Y] = vertex->ob[AXIS_Y] + moveY;
                    vertex->ob[AXIS_Z] = vertex->ob[AXIS_Z] + stepZ;
                }

                PrintDebugHex(vertex->ob[AXIS_Y], TRIANGLE_TEXT_COLUMN, CORNER_HEIGHT_TEXT_ROW);
            }
        }

        for (corner = 0; corner < TRIANGLE_VERTEX_COUNT; corner++) {
            corners[corner] = &g_WorkArea.hole.triangles[f_WallTriangleIndex].vertices[corner].v;
            nextCorners[corner] =
                &g_WorkArea.hole.triangles[f_WallTriangleIndex + 1].vertices[corner].v;
        }

        corners[WALL_TOP_CORNER]->ob[AXIS_X] = corners[0]->ob[AXIS_X];
        corners[WALL_TOP_CORNER]->ob[AXIS_Z] = corners[0]->ob[AXIS_Z];
        corners[WALL_TOP_CORNER]->ob[AXIS_Y] = corners[0]->ob[AXIS_Y] + WALL_HEIGHT;
        nextCorners[0]->ob[AXIS_X] = corners[1]->ob[AXIS_X];
        nextCorners[0]->ob[AXIS_Y] = corners[1]->ob[AXIS_Y];
        nextCorners[0]->ob[AXIS_Z] = corners[1]->ob[AXIS_Z];
        nextCorners[1]->ob[AXIS_X] = corners[1]->ob[AXIS_X];
        nextCorners[1]->ob[AXIS_Y] = corners[1]->ob[AXIS_Y] + WALL_HEIGHT;
        nextCorners[1]->ob[AXIS_Z] = corners[1]->ob[AXIS_Z];
        nextCorners[2]->ob[AXIS_X] = corners[WALL_TOP_CORNER]->ob[AXIS_X];
        nextCorners[2]->ob[AXIS_Y] = corners[WALL_TOP_CORNER]->ob[AXIS_Y];
        nextCorners[2]->ob[AXIS_Z] = corners[WALL_TOP_CORNER]->ob[AXIS_Z];
        PrepareHoleTriangles();
    }

    if (g_MenuControllerData[OPTION_CONTROLLER].button & A_BUTTON) {
        Vtx_t *vertex;

        vertex = &g_WorkArea.hole.triangles[f_WallTriangleIndex].vertices[f_WallCornerIndex].v;
        f_SavedWallCornerIndex = (f_SavedWallCornerIndex + 1) % SAVED_CORNER_COUNT;
        f_SavedWallCorners[f_SavedWallCornerIndex][AXIS_X] = vertex->ob[AXIS_X];
        f_SavedWallCorners[f_SavedWallCornerIndex][AXIS_Y] = vertex->ob[AXIS_Y];
        f_SavedWallCorners[f_SavedWallCornerIndex][AXIS_Z] = vertex->ob[AXIS_Z];
        PrepareHoleTriangles();
    }

    if (g_MenuControllerData[OPTION_CONTROLLER].button & B_BUTTON) {
        Vtx_t *vertex;

        vertex = &g_WorkArea.hole.triangles[f_WallTriangleIndex].vertices[f_WallCornerIndex].v;
        vertex->ob[AXIS_X] = f_SavedWallCorners[f_SavedWallCornerIndex][AXIS_X];
        vertex->ob[AXIS_Y] = f_SavedWallCorners[f_SavedWallCornerIndex][AXIS_Y];
        vertex->ob[AXIS_Z] = f_SavedWallCorners[f_SavedWallCornerIndex][AXIS_Z];
        f_SavedWallCornerIndex =
            (f_SavedWallCornerIndex + SAVED_CORNER_COUNT - 1) % SAVED_CORNER_COUNT;
        PrepareHoleTriangles();
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & R_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        f_WallTriangleIndex += WALL_TRIANGLE_COUNT;
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & L_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        f_WallTriangleIndex -= WALL_TRIANGLE_COUNT;
    }

    if (f_WallTriangleIndex < f_FirstWallTriangle) {
        f_WallTriangleIndex = f_FirstWallTriangle;
        PlaySound(WALL_LIMIT_SOUND, WALL_LIMIT_SOUND_CHANNEL, SOUND_FULL_SCALE);
    }

    if (f_WallTriangleIndex >= FIRST_RING_TRIANGLE) {
        f_WallTriangleIndex -= WALL_TRIANGLE_COUNT;
        PlaySound(WALL_LIMIT_SOUND, WALL_LIMIT_SOUND_CHANNEL, SOUND_FULL_SCALE);
    }

    g_WorkArea.hole.triangles[f_WallTriangleIndex].kind = HOLE_TRIANGLE_KIND_SHADED;
    g_WorkArea.hole.triangles[f_WallTriangleIndex + 1].kind = HOLE_TRIANGLE_KIND_SHADED;
    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & U_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_KIND;

        if (g_WorkArea.hole.triangles[f_WallTriangleIndex].kind >= HOLE_TRIANGLE_KIND_COUNT) {
            g_WorkArea.hole.triangles[f_WallTriangleIndex].kind = HOLE_TRIANGLE_KIND_FLAT;
        }

        PrepareHoleTriangles();
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & D_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_KIND;
        PrepareHoleTriangles();
    }

    if ((g_ControllerData[EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[EDITOR_CONTROLLER].button & L_TRIG) ||
        (g_ControllerData[EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG) {
        f_WallCameraYawAngle -=
            g_MenuControllerData[EDITOR_CONTROLLER].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_WallCameraPitch -=
            g_MenuControllerData[EDITOR_CONTROLLER].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }

    if (f_WallCameraPitch > 0.0f) {
        f_WallCameraPitch = 0.0f;
    }

    if (f_WallCameraPitch < EDITOR_PITCH_MIN) {
        f_WallCameraPitch = EDITOR_PITCH_MIN;
    }

    f_WallCameraYaw = f_WallCameraYawAngle * EDITOR_ANGLE_TO_RADIANS;

    if ((f_LastWallEditKind != TRIANGLE_EDIT_NONE) & (editKind == TRIANGLE_EDIT_NONE)) {
        UpdateTriangleSlopes();
    }

    cameraDistance = EDITOR_CAMERA_DISTANCE;
    lookAtVertex = &g_WorkArea.hole.triangles[f_WallTriangleIndex].vertices[f_WallCornerIndex].v;
    g_MapCamera.view.lookAt[AXIS_X] = lookAtVertex->ob[AXIS_X];
    g_MapCamera.view.lookAt[AXIS_Y] = lookAtVertex->ob[AXIS_Y];
    f_LastWallEditKind = editKind;
    g_MapCamera.view.lookAt[AXIS_Z] = lookAtVertex->ob[AXIS_Z];
    g_MapCamera.view.eye[AXIS_X] =
        (cos(f_WallCameraPitch) * cos(f_WallCameraYaw) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] =
        (cos(f_WallCameraPitch) * sin(f_WallCameraYaw) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] =
        (sin(f_WallCameraPitch) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Y];
}

void PutObjectsOnGround(void) {
    Mat4 translation;
    Mat4 unusedMatrix UNUSED;
    s32 height;
    s32 i;

    PlaySound(SOUND_MENU_CONFIRM, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
    for (i = 0; i < BILLBOARD_COUNT; i++) {
        if (!g_Billboards[i].isNearCourseEdge) {
            g_Billboards[i].y =
                GetGroundHeight(g_Billboards[i].x, g_Billboards[i].z) * COURSE_TO_WORLD_SCALE;
            height = GetGroundHeight(
                         g_Billboards[i].x << WORLD_TO_COURSE_SHIFT,
                         g_Billboards[i].z << WORLD_TO_COURSE_SHIFT
                     ) /
                (1 << WORLD_TO_COURSE_SHIFT);
            guTranslateF(translation, g_Billboards[i].x, height + 1, g_Billboards[i].z);
            PlaceBillboards();
        }
    }

    for (i = 0; i < CUP_POSITION_COUNT; i++) {
        if (g_HolePoints.cupPositions[i][AXIS_X] != 0 &&
            g_HolePoints.cupPositions[i][AXIS_Z] != 0) {
            g_HolePoints.cupPositions[i][AXIS_Y] = GetGroundHeight(
                g_HolePoints.cupPositions[i][AXIS_X],
                g_HolePoints.cupPositions[i][AXIS_Z]
            );
            g_HolePoints.teePositions[i][AXIS_Y] = GetGroundHeight(
                g_HolePoints.teePositions[i][AXIS_X],
                g_HolePoints.teePositions[i][AXIS_Z]
            );
        }
    }

    g_HoleSetup.cupPosition[AXIS_X] = g_HolePoints.cupPositions[DEFAULT_CUP][AXIS_X];
    g_HoleSetup.cupPosition[AXIS_Y] = g_HolePoints.cupPositions[DEFAULT_CUP][AXIS_Y];
    g_HoleSetup.cupPosition[AXIS_Z] = g_HolePoints.cupPositions[DEFAULT_CUP][AXIS_Z];
    g_HoleSetup.teePosition[AXIS_X] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_X];
    g_HoleSetup.teePosition[AXIS_Y] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_Y];
    g_HoleSetup.teePosition[AXIS_Z] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_Z];
    for (i = 0; i < ROUTE_POINT_COUNT; i++) {
        if (g_HolePoints.aimPoints[i][AXIS_X] != 0 && g_HolePoints.aimPoints[i][AXIS_Z] != 0) {
            g_HolePoints.aimPoints[i][AXIS_Y] = GetGroundHeight(
                g_HolePoints.aimPoints[i][AXIS_X],
                g_HolePoints.aimPoints[i][AXIS_Z]
            );
        }

        if (g_HolePoints.routePoints[i][AXIS_X] != 0 && g_HolePoints.routePoints[i][AXIS_Z] != 0) {
            g_HolePoints.routePoints[i][AXIS_Y] = GetGroundHeight(
                g_HolePoints.routePoints[i][AXIS_X],
                g_HolePoints.routePoints[i][AXIS_Z]
            );
        }
    }
}

void SlopeOutOfBoundsGround(void) {
    Vtx_t point;
    GridOffset *offsets;
    s32 offsetCount;
    s32 offsetIndex;
    s32 column;
    s32 row;
    Vtx *groundPoint;
    Vtx *neighbor;

    offsets = AllocMemory(SLOPE_OFFSET_COUNT * sizeof(GridOffset));
    offsetCount = ListOffsetsByDistance(SLOPE_OFFSET_SPAN, offsets, SLOPE_OFFSET_COUNT);
    for (row = 0; row < GROUND_POINT_ROW_COUNT; row++) {
        for (column = 0; column < GROUND_POINT_COLUMN_COUNT; column++) {
            groundPoint = GetGroundPoint(column, row);
            if (GetTerrainCode(
                    groundPoint->v.ob[AXIS_X] / VERTEX_UNITS_PER_CODE,
                    groundPoint->v.ob[AXIS_Z] / VERTEX_UNITS_PER_CODE
                ) != TERRAIN_CODE_OUT_OF_BOUNDS) {
                continue;
            }

            for (offsetIndex = 1; offsetIndex < offsetCount; offsetIndex++) {
                if (offsets[offsetIndex].distance >= SLOPE_NOISE_DISTANCE * OFFSET_DISTANCE_SCALE) {
                    break;
                }

                neighbor = GetGroundPoint(
                    column + offsets[offsetIndex].column,
                    row + offsets[offsetIndex].row
                );
                if (GetTerrainCode(
                        neighbor->v.ob[AXIS_X] / VERTEX_UNITS_PER_CODE,
                        neighbor->v.ob[AXIS_Z] / VERTEX_UNITS_PER_CODE
                    ) != TERRAIN_CODE_OUT_OF_BOUNDS) {
                    break;
                }
            }

            point = groundPoint->v;

            if (offsets[offsetIndex].distance / OFFSET_DISTANCE_SCALE >=
                SLOPE_EXTRA_NOISE_DISTANCE) {
                point.ob[AXIS_Y] +=
                    ((rand() & SLOPE_NOISE_MASK) * SLOPE_NOISE_SCALE) + SLOPE_EXTRA_DROP;
            }

            if (offsets[offsetIndex].distance / OFFSET_DISTANCE_SCALE >= SLOPE_NOISE_DISTANCE) {
                point.ob[AXIS_Y] +=
                    ((rand() & SLOPE_NOISE_MASK) * SLOPE_NOISE_SCALE) + SLOPE_FAR_DROP;
            }
            else if (offsets[offsetIndex].distance / OFFSET_DISTANCE_SCALE >=
                SLOPE_START_DISTANCE) {
                point.ob[AXIS_Y] += (offsets[offsetIndex].distance -
                                        (SLOPE_BASE_DISTANCE * OFFSET_DISTANCE_SCALE)) *
                    SLOPE_DROP_PER_DISTANCE;
            }

            SetGroundPoint(column, row, &point);
        }
    }

    FreeMemory(&offsets);
}

void UpdateHeightEditor(void) {
    char text[DEBUG_TEXT_BUFFER_SIZE];
    s32 baseHeight;
    HeightEdit editKind;
    CursorDirection direction;
    s32 columnStep;
    s32 rowStep;
    f64 cameraDistance;

    void ChangeGroundHeight(s32 change) {
        s32 column;
        s32 row;

        for (row = 0; row < GROUND_POINT_ROW_COUNT; row++) {
            for (column = 0; column < GROUND_POINT_COLUMN_COUNT; column++) {
                s32 height = GetGroundPoint(column, row)->v.ob[AXIS_Y];

                height += change;
                SetGroundPointHeightAndShade(column, row, height, EDITED_POINT_SHADE);
            }
        }
    }

    editKind = HEIGHT_EDIT_NONE;
    direction = CURSOR_DIRECTION_NONE;
    if (f_ShowsGroundWaves) {
        g_ShowsGroundWaves = TRUE;
    }
    else {
        g_ShowsGroundWaves = FALSE;
    }

    if (g_MenuControllerData[OPTION_CONTROLLER].button & D_CBUTTONS) {
        SlopeOutOfBoundsGround();
    }

    if (g_MenuControllerData[SECOND_EDITOR_CONTROLLER].button & L_CBUTTONS) {
        f_CLeftPickedHeight = GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Y];
    }

    if (g_MenuControllerData[SECOND_EDITOR_CONTROLLER].button & U_CBUTTONS) {
        f_CUpPickedHeight = GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Y];
    }

    if (g_MenuControllerData[SECOND_EDITOR_CONTROLLER].button & R_CBUTTONS) {
        s32 change = f_CUpPickedHeight - f_CLeftPickedHeight;

        editKind = HEIGHT_EDIT_POINT;
        SetGroundPointHeightAndShade(
            f_HeightCursorColumn,
            f_HeightCursorRow,
            GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Y] + change,
            EDITED_POINT_SHADE
        );
        MeasureGroundTiles();
    }

    if (g_MenuControllerData[SECOND_EDITOR_CONTROLLER].button & D_CBUTTONS) {
        s32 change = f_CLeftPickedHeight - f_CUpPickedHeight;

        editKind = HEIGHT_EDIT_POINT;
        SetGroundPointHeightAndShade(
            f_HeightCursorColumn,
            f_HeightCursorRow,
            GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Y] + change,
            EDITED_POINT_SHADE
        );
        MeasureGroundTiles();
    }

    if (g_MenuControllerData[OPTION_CONTROLLER].button & A_BUTTON) {
        if (f_ShowsGroundWaves) {
            f_ShowsGroundWaves = FALSE;
        }
        else {
            f_ShowsGroundWaves = TRUE;
        }
    }

    if (g_MenuControllerData[OPTION_CONTROLLER].button & B_BUTTON) {
        PutObjectsOnGround();
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & U_CBUTTONS) {
        if (g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG) {
            ChangeGroundHeight(-HEIGHT_STEP);
        }
        else {
            s32 height = GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Y];

            height -= HEIGHT_STEP;
            f_LastSetHeight = height;
            SetGroundPointHeightAndShade(
                f_HeightCursorColumn,
                f_HeightCursorRow,
                height,
                EDITED_POINT_SHADE
            );
        }

        editKind = HEIGHT_EDIT_POINT;
        MeasureGroundTiles();
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & D_CBUTTONS) {
        if (g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG) {
            ChangeGroundHeight(HEIGHT_STEP);
        }
        else {
            s32 height = GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Y];

            height += HEIGHT_STEP;
            f_LastSetHeight = height;
            SetGroundPointHeightAndShade(
                f_HeightCursorColumn,
                f_HeightCursorRow,
                height,
                EDITED_POINT_SHADE
            );
        }

        editKind = HEIGHT_EDIT_POINT;
        MeasureGroundTiles();
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & R_CBUTTONS) {
        SetGroundPointHeightAndShade(
            f_HeightCursorColumn,
            f_HeightCursorRow,
            f_LastSetHeight,
            EDITED_POINT_SHADE
        );
        editKind = HEIGHT_EDIT_POINT;
        MeasureGroundTiles();
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & L_CBUTTONS) {
        SetGroundPointHeightAndShade(
            f_HeightCursorColumn,
            f_HeightCursorRow,
            f_LastSetHeight + (rand() & COARSE_NOISE_MASK) - COARSE_NOISE_OFFSET,
            EDITED_POINT_SHADE
        );
        editKind = HEIGHT_EDIT_POINT;
        MeasureGroundTiles();
    }

    if (g_MenuControllerData[EDITOR_CONTROLLER].button & A_BUTTON) {
        editKind = HEIGHT_EDIT_POINT;
        baseHeight = GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Y];
        baseHeight -= ROUGH_NOISE_OFFSET;
        SetGroundPointHeightAndShade(
            f_HeightCursorColumn,
            f_HeightCursorRow,
            baseHeight + (rand() & ROUGH_NOISE_MASK),
            EDITED_POINT_SHADE
        );
        MeasureGroundTiles();
    }

    if (g_MenuControllerData[EDITOR_CONTROLLER].button & B_BUTTON) {
        editKind = HEIGHT_EDIT_POINT;
        baseHeight = GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Y];
        baseHeight -= HEIGHT_STEP;
        SetGroundPointHeightAndShade(
            f_HeightCursorColumn,
            f_HeightCursorRow,
            baseHeight + (rand() & STEP_NOISE_BIT),
            EDITED_POINT_SHADE
        );
        MeasureGroundTiles();
    }

    if (g_MenuControllerData[SECOND_EDITOR_CONTROLLER].button & B_BUTTON) {
        editKind = HEIGHT_EDIT_POINT;
        baseHeight = GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Y];
        SetGroundPointHeightAndShade(
            f_HeightCursorColumn,
            f_HeightCursorRow,
            baseHeight - (rand() & DIP_NOISE_BIT),
            EDITED_POINT_SHADE
        );
        MeasureGroundTiles();
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & R_JPAD) &&
        !(g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        direction = CURSOR_DIRECTION_RIGHT;
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & L_JPAD) &&
        !(g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        direction = CURSOR_DIRECTION_LEFT;
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & D_JPAD) &&
        !(g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        if (direction == CURSOR_DIRECTION_RIGHT) {
            direction = CURSOR_DIRECTION_DOWN_RIGHT;
        }
        else if (direction == CURSOR_DIRECTION_LEFT) {
            direction = CURSOR_DIRECTION_DOWN_LEFT;
        }
        else {
            direction = CURSOR_DIRECTION_DOWN;
        }
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & U_JPAD) &&
        !(g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        if (direction == CURSOR_DIRECTION_RIGHT) {
            direction = CURSOR_DIRECTION_UP_RIGHT;
        }
        else if (direction == CURSOR_DIRECTION_LEFT) {
            direction = CURSOR_DIRECTION_UP_LEFT;
        }
        else {
            direction = CURSOR_DIRECTION_UP;
        }
    }

    if ((g_MenuControllerData[SECOND_EDITOR_CONTROLLER].button & R_JPAD) &&
        !(g_ControllerData[SECOND_EDITOR_CONTROLLER].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        direction = CURSOR_DIRECTION_RIGHT;
    }

    if ((g_MenuControllerData[SECOND_EDITOR_CONTROLLER].button & L_JPAD) &&
        !(g_ControllerData[SECOND_EDITOR_CONTROLLER].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        direction = CURSOR_DIRECTION_LEFT;
    }

    if ((g_MenuControllerData[SECOND_EDITOR_CONTROLLER].button & D_JPAD) &&
        !(g_ControllerData[SECOND_EDITOR_CONTROLLER].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        if (direction == CURSOR_DIRECTION_RIGHT) {
            direction = CURSOR_DIRECTION_DOWN_RIGHT;
        }
        else if (direction == CURSOR_DIRECTION_LEFT) {
            direction = CURSOR_DIRECTION_DOWN_LEFT;
        }
        else {
            direction = CURSOR_DIRECTION_DOWN;
        }
    }

    if ((g_MenuControllerData[SECOND_EDITOR_CONTROLLER].button & U_JPAD) &&
        !(g_ControllerData[SECOND_EDITOR_CONTROLLER].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        if (direction == CURSOR_DIRECTION_RIGHT) {
            direction = CURSOR_DIRECTION_UP_RIGHT;
        }
        else if (direction == CURSOR_DIRECTION_LEFT) {
            direction = CURSOR_DIRECTION_UP_LEFT;
        }
        else {
            direction = CURSOR_DIRECTION_UP;
        }
    }

    if (direction != CURSOR_DIRECTION_NONE) {
        columnStep = cos(((direction * BINARY_ANGLE_EIGHTH_TURN) + f_HeightCameraYawAngle) *
                         EDITOR_ANGLE_TO_RADIANS) *
            CURSOR_STEP_LENGTH;
        rowStep = sin(((direction * BINARY_ANGLE_EIGHTH_TURN) + f_HeightCameraYawAngle) *
                      EDITOR_ANGLE_TO_RADIANS) *
            CURSOR_STEP_LENGTH;
        f_HeightCursorColumn += columnStep;
        f_HeightCursorRow += rowStep;
        if (f_HeightCursorColumn < 0) {
            f_HeightCursorColumn = 0;
        }

        if (f_HeightCursorRow < 0) {
            f_HeightCursorRow = 0;
        }

        if (f_HeightCursorColumn >= GROUND_POINT_COLUMN_COUNT) {
            f_HeightCursorColumn = GROUND_POINT_COLUMN_COUNT - 1;
        }

        if (f_HeightCursorRow >= GROUND_POINT_ROW_COUNT) {
            f_HeightCursorRow = GROUND_POINT_ROW_COUNT - 1;
        }
    }

    if ((g_ControllerData[EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[EDITOR_CONTROLLER].button & L_TRIG) ||
        (g_ControllerData[EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG) {
        f_HeightCameraYawAngle -=
            g_MenuControllerData[EDITOR_CONTROLLER].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_HeightCameraPitch -=
            g_MenuControllerData[EDITOR_CONTROLLER].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }

    if ((g_ControllerData[SECOND_EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[SECOND_EDITOR_CONTROLLER].button & L_TRIG) ||
        (g_ControllerData[SECOND_EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) ==
            (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[SECOND_EDITOR_CONTROLLER].button & Z_TRIG) {
        f_HeightCameraYawAngle -= g_MenuControllerData[SECOND_EDITOR_CONTROLLER].stick_x /
            STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_HeightCameraPitch -= g_MenuControllerData[SECOND_EDITOR_CONTROLLER].stick_y /
            STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }

    if (f_HeightCameraPitch > 0.0f) {
        f_HeightCameraPitch = 0.0f;
    }

    if (f_HeightCameraPitch < EDITOR_PITCH_MIN) {
        f_HeightCameraPitch = EDITOR_PITCH_MIN;
    }

    f_HeightCameraYaw = f_HeightCameraYawAngle * EDITOR_ANGLE_TO_RADIANS;
    if (f_LastHeightEditKind != HEIGHT_EDIT_NONE && editKind == HEIGHT_EDIT_NONE) {
        UpdateTriangleSlopes();
    }

    f_LastHeightEditKind = editKind;
    cameraDistance = EDITOR_CAMERA_DISTANCE;
    g_MapCamera.view.lookAt[AXIS_X] =
        GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_X];
    g_MapCamera.view.lookAt[AXIS_Y] =
        GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Y];
    g_MapCamera.view.lookAt[AXIS_Z] =
        GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Z];
    g_MapCamera.view.eye[AXIS_X] = (cos(f_HeightCameraPitch) * cos(f_HeightCameraYaw) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = (cos(f_HeightCameraPitch) * sin(f_HeightCameraYaw) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] =
        (sin(f_HeightCameraPitch) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Y];
    sprintf(
        text,
        "%fm  ",
        -(f32)GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Y] *
            METERS_PER_HEIGHT_UNIT
    );
    PrintDebugText(text, HEIGHT_TEXT_COLUMN, HEIGHT_TEXT_ROW);
}

Boolean ShouldMirrorCell(s32 column, s32 row, s32 endColumn, s32 endRow) {
    TerrainCode sideCodes[DIAGONAL_SIDE_COUNT][SIDE_CODE_COUNT];
    s32 width;
    s32 height;
    s32 codeIndex;
    s32 rowOffset;
    s32 columnOffset;
    TerrainCode code;
    f32 inverseLength;
    f32 distance;

    void AddSideCode(TerrainCode * codes, TerrainCode code) {
        s32 codeIndex;

        for (codeIndex = 0; codeIndex < SIDE_CODE_COUNT; codeIndex++) {
            if (codes[codeIndex] == code) {
                return;
            }

            if (codes[codeIndex] == TERRAIN_CODE_NONE) {
                codes[codeIndex] = code;
                return;
            }
        }
    }

    width = endColumn - column;
    height = endRow - row;
    AssertInEditor(width > 0 && height > 0, HOLE_EDITOR_DIAGONAL_ASSERT_LINE);
    for (codeIndex = 0; codeIndex < DIAGONAL_SIDE_COUNT; codeIndex++) {
        sideCodes[codeIndex][0] = TERRAIN_CODE_NONE;
        sideCodes[codeIndex][1] = TERRAIN_CODE_NONE;
    }

    inverseLength = 1.0f / sqrtf((width * width) + (height * height));
    for (rowOffset = 0; rowOffset < height; rowOffset++) {
        for (columnOffset = 0; columnOffset < width; columnOffset++) {
            code = GetTerrainCode(column + columnOffset, row + rowOffset);
            distance = ((width * (rowOffset - height)) + (columnOffset * height)) * inverseLength;
            if (AbsFloat(distance) >= DIAGONAL_DISTANCE_MIN) {
                if (distance >= 0.0f) {
                    AddSideCode(sideCodes[2], code);
                }
                else {
                    AddSideCode(sideCodes[3], code);
                }
            }

            distance = ((-height * columnOffset) + (rowOffset * width)) * inverseLength;
            if (AbsFloat(distance) >= DIAGONAL_DISTANCE_MIN) {
                if (distance >= 0.0f) {
                    AddSideCode(sideCodes[0], code);
                }
                else {
                    AddSideCode(sideCodes[1], code);
                }
            }
        }
    }

    if (HasTwoSideCodes(sideCodes[0]) &&
        HasTwoSideCodes(sideCodes[1]) &&
        (!HasTwoSideCodes(sideCodes[2]) || !HasTwoSideCodes(sideCodes[3]))) {
        return TRUE;
    }

    return FALSE;
}

#define IsOffGroundPoints(column, row) \
    ((column) < 0 || \
        (column) >= GROUND_TILE_COLUMN_COUNT * GROUND_TILE_POINT_SPAN || \
        (row) >= GROUND_TILE_ROW_COUNT * GROUND_TILE_POINT_SPAN || \
        (row) < 0)

void ToggleCellMirroring(s32 column, s32 row) {
    s32 tileIndex;
    s32 cellBits;

    if (IsOffGroundPoints(column, row)) {
        return;
    }

    tileIndex = ((row / GROUND_TILE_POINT_SPAN) * GROUND_TILE_COLUMN_COUNT) +
        (column / GROUND_TILE_POINT_SPAN);
    cellBits = g_WorkArea.hole.tiles[tileIndex].mirroredCells;
    g_WorkArea.hole.tiles[tileIndex].mirroredCells = cellBits ^
        (1
            << (((row & (GROUND_TILE_POINT_SPAN - 1)) * GROUND_TILE_POINT_SPAN) +
                   (column & (GROUND_TILE_POINT_SPAN - 1))));
}

void InterpolateColumnPoints(Vtx *vertices, s32 column, s32 row, s32 weight) {
    Vtx *ends[2];
    s32 firstWeight;
    s32 pointIndex;

    firstWeight = EDGE_WEIGHT_MAX - weight;
    ends[0] = GetGridVertex(column, row);
    ends[1] = GetGridVertex(column + 1, row);
    for (pointIndex = 0; pointIndex < EDGE_POINT_COUNT; pointIndex++) {
        Vtx *vertex = &vertices[f_TileMidpointSlots[pointIndex][((column & 1) * 2) + 1]];

        vertex->v.ob[AXIS_X] =
            ((ends[0]->v.ob[AXIS_X] * firstWeight) + (ends[1]->v.ob[AXIS_X] * weight)) /
            EDGE_WEIGHT_MAX;
        vertex->v.tc[TEXTURE_AXIS_S] = ((ends[0]->v.tc[TEXTURE_AXIS_S] * firstWeight) +
                                           (ends[1]->v.tc[TEXTURE_AXIS_S] * weight)) /
            EDGE_WEIGHT_MAX;
    }
}

void InterpolateRowPoints(Vtx *vertices, s32 column, s32 row, s32 weight) {
    Vtx *ends[2];
    s32 firstWeight;
    s32 pointIndex;

    firstWeight = EDGE_WEIGHT_MAX - weight;
    ends[0] = GetGridVertex(column, row);
    ends[1] = GetGridVertex(column, row + 1);
    for (pointIndex = 0; pointIndex < EDGE_POINT_COUNT; pointIndex++) {
        Vtx *vertex = &vertices[f_TileMidpointSlots[((row & 1) * 2) + 1][pointIndex]];

        vertex->v.ob[AXIS_Z] =
            ((ends[0]->v.ob[AXIS_Z] * firstWeight) + (ends[1]->v.ob[AXIS_Z] * weight)) /
            EDGE_WEIGHT_MAX;
        vertex->v.tc[TEXTURE_AXIS_T] = ((ends[0]->v.tc[TEXTURE_AXIS_T] * firstWeight) +
                                           (ends[1]->v.tc[TEXTURE_AXIS_T] * weight)) /
            EDGE_WEIGHT_MAX;
    }
}

void ShapeGroundFromCodes(void) {
    Vtx *segmentPoints[SEGMENT_POINT_COUNT];
    s32 treePositions[BILLBOARD_COUNT][PLANE_AXIS_COUNT];
    GridOffset *offsets;
    s32 offsetCount;
    s32 offsetIndex;
    s32 greenColumn;
    s32 greenRow;
    s32 greenCount;
    s32 column;
    s32 row;
    s32 codeColumn;
    s32 codeRow;
    s32 edgeRow;
    s32 edgeColumn;
    TerrainCode code;
    TerrainCode neighborCode;
    s32 height;
    s32 tileIndex;
    s32 treeCount;
    s32 treeIndex;
    s32 billboardIndex;
    s32 x;
    s32 z;
    Vtx *point;

    offsets = AllocMemory(SHAPE_OFFSET_COUNT * sizeof(GridOffset));
    AssertInEditor(offsets != NULL, HOLE_EDITOR_SHAPE_ASSERT_LINE);
    offsetCount = ListOffsetsByDistance(SHAPE_OFFSET_SPAN, offsets, SHAPE_OFFSET_COUNT);
    greenCount = 0;
    greenRow = 0;
    greenColumn = 0;
    for (row = 0; row < CODE_GRID_ROW_COUNT; row++) {
        for (column = 0; column < CODE_GRID_COLUMN_COUNT; column++) {
            if (GetTerrainCode(column, row) == TERRAIN_CODE_GREEN) {
                greenColumn += column;
                greenRow += row;
                greenCount++;
            }
        }
    }

    if (greenCount != 0) {
        greenColumn /= greenCount;
        greenRow /= greenCount;
    }

    BuildAllTileMidpoints();
    for (row = 0; row < GROUND_POINT_ROW_COUNT; row++) {
        for (column = 0; column < GROUND_POINT_COLUMN_COUNT; column++) {
            if ((row & 1) != (column & 1)) {
                continue;
            }

            point = GetGroundPoint(column, row);
            codeColumn = point->v.ob[AXIS_X] / VERTEX_UNITS_PER_CODE;
            codeRow = point->v.ob[AXIS_Z] / VERTEX_UNITS_PER_CODE;
            code = GetTerrainCode(codeColumn, codeRow);
            if (code == TERRAIN_CODE_GREEN) {
                height = rand() & GREEN_NOISE_MASK;
            }
            else {
                height = (f32)((s32)IntLength2D(codeColumn - greenColumn, codeRow - greenRow)
                             << GROUND_TEXEL_SHIFT) *
                    HEIGHT_PER_COURSE_UNIT;
                height -= rand() & GROUND_NOISE_MASK;
                if (code == TERRAIN_CODE_BUNKER) {
                    neighborCode = code;
                    for (offsetIndex = 0; offsetIndex < offsetCount; offsetIndex++) {
                        neighborCode = GetTerrainCode(
                            codeColumn + offsets[offsetIndex].column,
                            codeRow + offsets[offsetIndex].row
                        );
                        if (neighborCode != TERRAIN_CODE_BUNKER ||
                            offsets[offsetIndex].distance >=
                                BUNKER_INNER_DISTANCE * OFFSET_DISTANCE_SCALE) {
                            break;
                        }
                    }

                    if (neighborCode == TERRAIN_CODE_BUNKER) {
                        height += BUNKER_INNER_OFFSET;
                    }
                }
                else if (code == TERRAIN_CODE_ROUGH) {
                    for (offsetIndex = 0; offsetIndex < offsetCount; offsetIndex++) {
                        if (GetTerrainCode(
                                codeColumn + offsets[offsetIndex].column,
                                codeRow + offsets[offsetIndex].row
                            ) != TERRAIN_CODE_ROUGH ||
                            offsets[offsetIndex].distance >=
                                ROUGH_INNER_DISTANCE * OFFSET_DISTANCE_SCALE) {
                            break;
                        }
                    }

                    if (offsets[offsetIndex].distance >=
                        ROUGH_INNER_DISTANCE * OFFSET_DISTANCE_SCALE) {
                        height -= (rand() & ROUGH_INNER_NOISE_MASK) + ROUGH_INNER_OFFSET;
                    }
                    else {
                        height -= ROUGH_EDGE_OFFSET;
                    }
                }
            }

            SetGroundPointHeightAndShade(column, row, height, SHAPED_POINT_SHADE);
        }
    }

    for (row = 0; row < GROUND_TILE_ROW_COUNT; row++) {
        for (column = 0; column < GROUND_TILE_COLUMN_COUNT; column++) {
            tileIndex = (row * GROUND_TILE_COLUMN_COUNT) + column;
            if (g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex < 0) {
                continue;
            }

            g_WorkArea.hole.tiles[tileIndex].extraPassCount = 0;
            for (edgeRow = 0; edgeRow < EDGE_POINT_COUNT; edgeRow++) {
                for (edgeColumn = 0; edgeColumn < EDGE_POINT_COUNT; edgeColumn++) {
                    if ((edgeRow & 1) == (edgeColumn & 1)) {
                        continue;
                    }

                    if ((edgeRow & 1) == 0) {
                        segmentPoints[SEGMENT_START] = GetGroundPoint(
                            (column * GROUND_TILE_POINT_SPAN) + edgeColumn - 1,
                            (row * GROUND_TILE_POINT_SPAN) + edgeRow
                        );
                        segmentPoints[SEGMENT_MIDPOINT] =
                            &g_WorkArea.hole.midpoints[g_WorkArea.hole.tiles[tileIndex]
                                    .midpointBlockIndex][f_TileMidpointSlots[edgeRow][edgeColumn]];
                        segmentPoints[SEGMENT_END] = GetGroundPoint(
                            (column * GROUND_TILE_POINT_SPAN) + edgeColumn + 1,
                            (row * GROUND_TILE_POINT_SPAN) + edgeRow
                        );
                        segmentPoints[SEGMENT_MIDPOINT]->v.ob[AXIS_Y] =
                            segmentPoints[SEGMENT_START]->v.ob[AXIS_Y] +
                            ((segmentPoints[SEGMENT_END]->v.ob[AXIS_Y] -
                                 segmentPoints[SEGMENT_START]->v.ob[AXIS_Y]) *
                                (segmentPoints[SEGMENT_MIDPOINT]->v.ob[AXIS_X] -
                                    segmentPoints[SEGMENT_START]->v.ob[AXIS_X]) /
                                (segmentPoints[SEGMENT_END]->v.ob[AXIS_X] -
                                    segmentPoints[SEGMENT_START]->v.ob[AXIS_X]));
                    }
                    else {
                        segmentPoints[SEGMENT_START] = GetGroundPoint(
                            (column * GROUND_TILE_POINT_SPAN) + edgeColumn,
                            (row * GROUND_TILE_POINT_SPAN) + edgeRow - 1
                        );
                        segmentPoints[SEGMENT_MIDPOINT] =
                            &g_WorkArea.hole.midpoints[g_WorkArea.hole.tiles[tileIndex]
                                    .midpointBlockIndex][f_TileMidpointSlots[edgeRow][edgeColumn]];
                        segmentPoints[SEGMENT_END] = GetGroundPoint(
                            (column * GROUND_TILE_POINT_SPAN) + edgeColumn,
                            (row * GROUND_TILE_POINT_SPAN) + edgeRow + 1
                        );
                        segmentPoints[SEGMENT_MIDPOINT]->v.ob[AXIS_Y] =
                            segmentPoints[SEGMENT_START]->v.ob[AXIS_Y] +
                            ((segmentPoints[SEGMENT_END]->v.ob[AXIS_Y] -
                                 segmentPoints[SEGMENT_START]->v.ob[AXIS_Y]) *
                                (segmentPoints[SEGMENT_MIDPOINT]->v.ob[AXIS_Z] -
                                    segmentPoints[SEGMENT_START]->v.ob[AXIS_Z]) /
                                (segmentPoints[SEGMENT_END]->v.ob[AXIS_Z] -
                                    segmentPoints[SEGMENT_START]->v.ob[AXIS_Z]));
                    }
                }
            }
        }
    }

    treeCount = 0;
    for (row = 0; row < CODE_GRID_ROW_COUNT; row++) {
        for (column = 0; column < CODE_GRID_COLUMN_COUNT; column++) {
            if (GetTerrainCode(column, row) == TERRAIN_CODE_ROUGH) {
                for (offsetIndex = 0; offsetIndex < offsetCount; offsetIndex++) {
                    if (GetTerrainCode(
                            column + offsets[offsetIndex].column,
                            row + offsets[offsetIndex].row
                        ) != TERRAIN_CODE_ROUGH ||
                        offsets[offsetIndex].distance >=
                            TREE_DISTANCE_MAX * OFFSET_DISTANCE_SCALE) {
                        break;
                    }
                }

                if (offsets[offsetIndex].distance >= TREE_DISTANCE_MIN * OFFSET_DISTANCE_SCALE &&
                    offsets[offsetIndex].distance < TREE_DISTANCE_MAX * OFFSET_DISTANCE_SCALE) {
                    x = column * VERTEX_UNITS_PER_CODE;
                    z = row * VERTEX_UNITS_PER_CODE;
                    for (treeIndex = 0; treeIndex < treeCount; treeIndex++) {
                        if (IntLength2D(
                                x - treePositions[treeIndex][GROUND_AXIS_X],
                                z - treePositions[treeIndex][GROUND_AXIS_Z]
                            ) < TREE_SPACING) {
                            break;
                        }
                    }

                    if (treeIndex == treeCount) {
                        treePositions[treeCount][GROUND_AXIS_X] = x;
                        treePositions[treeCount][GROUND_AXIS_Z] = z;
                        treeCount++;

                        if (treeCount == BILLBOARD_COUNT) {
                            goto PLANTED;
                        }
                    }

                    if (treeCount == BILLBOARD_COUNT) {
                        goto PLANTED;
                    }
                }

                if (treeCount == BILLBOARD_COUNT) {
                    goto PLANTED;
                }
            }

            if (treeCount == BILLBOARD_COUNT) {
                goto PLANTED;
            }
        }
    }

PLANTED:

    for (billboardIndex = 0; billboardIndex < treeCount; billboardIndex++) {
        g_Billboards[billboardIndex].y =
            (GetGroundHeight(
                 treePositions[billboardIndex][GROUND_AXIS_X] << WORLD_TO_COURSE_SHIFT,
                 treePositions[billboardIndex][GROUND_AXIS_Z] << WORLD_TO_COURSE_SHIFT
             ) / (1 << WORLD_TO_COURSE_SHIFT)) +
            1;
        g_Billboards[billboardIndex].x = treePositions[billboardIndex][GROUND_AXIS_X];
        g_Billboards[billboardIndex].z = treePositions[billboardIndex][GROUND_AXIS_Z];
        g_Billboards[billboardIndex].textureIndex = TREE_TEXTURE_INDEX;
        g_Billboards[billboardIndex].shapeIndex = 0;
        g_Billboards[billboardIndex].isNearCourseEdge = TREE_NEAR_EDGE;
        g_Billboards[billboardIndex].width = TREE_WIDTH;
        g_Billboards[billboardIndex].height = TREE_HEIGHT;
    }

    for (billboardIndex = treeCount; billboardIndex < BILLBOARD_COUNT; billboardIndex++) {
        g_Billboards[billboardIndex].textureIndex = -1;
    }

    PlaceBillboards();
    FreeMemory(&offsets);
}

void ChooseMirroredCells(void) {
    s32 tileRow;
    s32 tileColumn;
    s32 cellRow;
    s32 cellColumn;
    s32 tileIndex;
    s32 cellBits;
    Vtx *corners[2];

    for (tileRow = 0; tileRow < GROUND_TILE_ROW_COUNT; tileRow++) {
        for (tileColumn = 0; tileColumn < GROUND_TILE_COLUMN_COUNT; tileColumn++) {
            cellBits = 0;
            tileIndex = (tileRow * GROUND_TILE_COLUMN_COUNT) + tileColumn;
            for (cellRow = 0; cellRow < GROUND_TILE_POINT_SPAN; cellRow++) {
                for (cellColumn = 0; cellColumn < GROUND_TILE_POINT_SPAN; cellColumn++) {
                    corners[0] = GetGroundPoint(
                        (tileColumn * GROUND_TILE_POINT_SPAN) + cellColumn,
                        (tileRow * GROUND_TILE_POINT_SPAN) + cellRow
                    );
                    corners[1] = GetGroundPoint(
                        (tileColumn * GROUND_TILE_POINT_SPAN) + cellColumn + 1,
                        (tileRow * GROUND_TILE_POINT_SPAN) + cellRow + 1
                    );
                    cellBits |= ShouldMirrorCell(
                                    corners[0]->v.ob[AXIS_X] / VERTEX_UNITS_PER_CODE,
                                    corners[0]->v.ob[AXIS_Z] / VERTEX_UNITS_PER_CODE,
                                    corners[1]->v.ob[AXIS_X] / VERTEX_UNITS_PER_CODE,
                                    corners[1]->v.ob[AXIS_Z] / VERTEX_UNITS_PER_CODE
                                )
                        << ((cellRow * GROUND_TILE_POINT_SPAN) + cellColumn);
                }
            }

            g_WorkArea.hole.tiles[tileIndex].mirroredCells = cellBits;
        }
    }
}

Boolean HasDeadTriangleNearPoint(s32 column, s32 row) {
    Vtx *corners[4];
    s32 rowOffset;
    s32 columnOffset;

    for (rowOffset = -1; rowOffset <= 0; rowOffset++) {
        columnOffset = -1;
        while (columnOffset <= 0) {
            corners[0] = GetGroundPoint(column + columnOffset, row + rowOffset);
            corners[1] = GetGroundPoint(column + columnOffset + 1, row + rowOffset);
            corners[2] = GetGroundPoint(column + columnOffset, row + rowOffset + 1);
            corners[3] = GetGroundPoint(column + columnOffset + 1, row + rowOffset + 1);
            if (CrossProductXZ(
                    corners[0]->v.ob[AXIS_X],
                    corners[0]->v.ob[AXIS_Z],
                    corners[1]->v.ob[AXIS_X],
                    corners[1]->v.ob[AXIS_Z],
                    corners[2]->v.ob[AXIS_X],
                    corners[2]->v.ob[AXIS_Z]
                ) < CELL_CROSS_PRODUCT_MIN ||
                CrossProductXZ(
                    corners[1]->v.ob[AXIS_X],
                    corners[1]->v.ob[AXIS_Z],
                    corners[3]->v.ob[AXIS_X],
                    corners[3]->v.ob[AXIS_Z],
                    corners[2]->v.ob[AXIS_X],
                    corners[2]->v.ob[AXIS_Z]
                ) < CELL_CROSS_PRODUCT_MIN) {
                return -TRUE;
            }

            columnOffset++;
        }
    }
    return 0;
}

void UpdateHoleFileMenu(void) {
    char courseText[DEBUG_TEXT_BUFFER_SIZE];
    char holeText[DEBUG_TEXT_BUFFER_SIZE];

    ClearDebugText();
    if (f_HoleFileStep == HOLE_FILE_STEP_ACTION) {
        if (g_MenuControllerData[0].button & (U_JPAD | D_JPAD)) {
            f_IsSavingHoleFiles ^= TRUE;
        }

        if (g_MenuControllerData[0].button & A_BUTTON) {
            f_HoleFileStep = HOLE_FILE_STEP_COURSE;
        }

        if (!f_IsSavingHoleFiles) {
            SetDebugTextHighlight(TRUE);
            PrintDebugText("->", HOLE_FILE_ARROW_COLUMN, HOLE_FILE_LOAD_ROW);
        }

        if (f_IsSavingHoleFiles) {
            SetDebugTextHighlight(TRUE);
            PrintDebugText("->", HOLE_FILE_ARROW_COLUMN, HOLE_FILE_SAVE_ROW);
        }
    }
    else if (f_HoleFileStep == HOLE_FILE_STEP_COURSE) {
        if (g_MenuControllerData[0].button & (L_JPAD | D_JPAD)) {
            f_HoleFileCourseNumber--;
        }

        if (g_MenuControllerData[0].button & (R_JPAD | U_JPAD)) {
            f_HoleFileCourseNumber++;
        }

        f_HoleFileCourseNumber =
            ((f_HoleFileCourseNumber + HOLE_FILE_COURSE_COUNT - 1) % HOLE_FILE_COURSE_COUNT) + 1;
        if (g_MenuControllerData[0].trigger & A_BUTTON) {
            f_HoleFileStep = HOLE_FILE_STEP_HOLE;
        }

        if (g_MenuControllerData[0].trigger & B_BUTTON) {
            f_HoleFileStep = HOLE_FILE_STEP_ACTION;
        }
    }
    else if (f_HoleFileStep == HOLE_FILE_STEP_HOLE) {
        if (g_MenuControllerData[0].button & (L_JPAD | D_JPAD)) {
            f_HoleFileHoleNumber--;
        }

        if (g_MenuControllerData[0].button & (R_JPAD | U_JPAD)) {
            f_HoleFileHoleNumber++;
        }

        f_HoleFileHoleNumber =
            ((f_HoleFileHoleNumber + COURSE_HOLE_COUNT - 1) % COURSE_HOLE_COUNT) + 1;
        if (g_MenuControllerData[0].trigger & A_BUTTON) {
            f_HoleFileStep = HOLE_FILE_STEP_CONFIRM;
        }

        if (g_MenuControllerData[0].trigger & B_BUTTON) {
            f_HoleFileStep = HOLE_FILE_STEP_COURSE;
        }
    }
    else if (f_HoleFileStep == HOLE_FILE_STEP_CONFIRM) {
        if (f_HoleFilePromptFrames & HOLE_FILE_BLINK_BIT) {
            SetDebugTextHighlight(TRUE);
        }

        PrintDebugText(
            "Are you sure? (Exec -> A_BUTTON)",
            HOLE_FILE_PROMPT_COLUMN,
            HOLE_FILE_PROMPT_ROW
        );
        f_HoleFilePromptFrames++;
        if (g_MenuControllerData[0].trigger & A_BUTTON) {
            if (f_IsSavingHoleFiles) {
                if (g_WorkArea.hole.tiles[0].texels[GROUND_TILE_TEXEL_ROW_COUNT - 1]
                                                   [GROUND_TILE_TEXEL_ROW_LENGTH - 1]) {
                    ToggleGroundTileBorders();
                }

                SaveHoleFiles(f_HoleFileCourseNumber, f_HoleFileHoleNumber);
            }
            else {
                BuildHole(f_HoleFileCourseNumber, f_HoleFileHoleNumber);
            }

            f_HoleFileStep = HOLE_FILE_STEP_ACTION;
        }

        if (g_MenuControllerData[0].trigger & B_BUTTON) {
            f_HoleFileStep = HOLE_FILE_STEP_HOLE;
        }

        if (g_MenuControllerData[0].trigger & START_BUTTON) {
            f_HoleFileStep = HOLE_FILE_STEP_ACTION;
        }
    }

    if (!f_IsSavingHoleFiles) {
        SetDebugTextHighlight(TRUE);
    }

    PrintDebugText("Load HoleData", HOLE_FILE_LABEL_COLUMN, HOLE_FILE_LOAD_ROW);
    if (f_IsSavingHoleFiles) {
        SetDebugTextHighlight(TRUE);
    }

    PrintDebugText("Save HoleData", HOLE_FILE_LABEL_COLUMN, HOLE_FILE_SAVE_ROW);
    if (f_HoleFileStep >= HOLE_FILE_STEP_COURSE) {
        PrintDebugText("Course : ", HOLE_FILE_ARROW_COLUMN, HOLE_FILE_COURSE_ROW);
        sprintf(courseText, "%d", (int)f_HoleFileCourseNumber);
        PrintDebugText(courseText, HOLE_FILE_NUMBER_COLUMN, HOLE_FILE_COURSE_ROW);
    }

    if (f_HoleFileStep >= HOLE_FILE_STEP_HOLE) {
        PrintDebugText("Hole   : ", HOLE_FILE_ARROW_COLUMN, HOLE_FILE_HOLE_ROW);
        sprintf(holeText, "%d", (int)f_HoleFileHoleNumber);
        PrintDebugText(holeText, HOLE_FILE_NUMBER_COLUMN, HOLE_FILE_HOLE_ROW);
    }
}

void UpdateTreeEditor(void) {
    Mat4 translation;
    Mat4 unusedMatrix UNUSED;
    TriangleEdit editKind;
    s32 billboardIndex;
    s32 height;
    f64 cameraDistance;

    PrintDebugText("KIND", TREE_TEXT_COLUMN, TREE_KIND_LABEL_ROW);
    PrintDebugText("        ", TREE_TEXT_COLUMN, TREE_KIND_TEXT_ROW);
    PrintDebugHex(
        g_Billboards[f_EditedTreeIndex].textureIndex,
        TREE_TEXT_COLUMN,
        TREE_KIND_TEXT_ROW
    );
    PrintDebugText("EDIT NUM", TREE_TEXT_COLUMN, TREE_INDEX_LABEL_ROW);
    PrintDebugText("        ", TREE_TEXT_COLUMN, TREE_INDEX_TEXT_ROW);
    PrintDebugHex(f_EditedTreeIndex, TREE_TEXT_COLUMN, TREE_INDEX_TEXT_ROW);
    g_ShowsGroundWaves = FALSE;
    editKind = TRIANGLE_EDIT_NONE;
    if (g_ControllerData[EDITOR_CONTROLLER].button & U_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[f_EditedTreeIndex].height++;
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & D_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[f_EditedTreeIndex].height--;
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & R_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[f_EditedTreeIndex].width++;
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & L_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[f_EditedTreeIndex].width--;
    }

    if (g_Billboards[f_EditedTreeIndex].height > TREE_SIZE_MAX) {
        g_Billboards[f_EditedTreeIndex].height = TREE_SIZE_MAX;
    }

    if (g_Billboards[f_EditedTreeIndex].height == 0) {
        g_Billboards[f_EditedTreeIndex].height = 1;
    }

    if (g_Billboards[f_EditedTreeIndex].width > TREE_SIZE_MAX) {
        g_Billboards[f_EditedTreeIndex].width = TREE_SIZE_MAX;
    }

    if (g_Billboards[f_EditedTreeIndex].width == 0) {
        g_Billboards[f_EditedTreeIndex].width = 1;
    }

    if (g_MenuControllerData[EDITOR_CONTROLLER].button & A_BUTTON) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[f_EditedTreeIndex].isIgnoredEditorMark =
            !g_Billboards[f_EditedTreeIndex].isIgnoredEditorMark;
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & R_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        g_Billboards[f_EditedTreeIndex].isNearCourseEdge = FALSE;
        f_EditedTreeIndex++;
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & L_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        g_Billboards[f_EditedTreeIndex].isNearCourseEdge = FALSE;
        f_EditedTreeIndex--;
    }

    if (f_EditedTreeIndex < 0) {
        f_EditedTreeIndex = BILLBOARD_COUNT - 1;
    }

    if (f_EditedTreeIndex >= BILLBOARD_COUNT) {
        f_EditedTreeIndex = 0;
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & U_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_KIND;
        g_Billboards[f_EditedTreeIndex].textureIndex++;
    }

    if ((g_MenuControllerData[EDITOR_CONTROLLER].button & D_JPAD) &&
        (g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionX |
            g_MenuRepeatStates[EDITOR_CONTROLLER].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_KIND;
        g_Billboards[f_EditedTreeIndex].textureIndex--;
    }

    if (g_Billboards[f_EditedTreeIndex].textureIndex >= BILLBOARD_TEXTURE_COUNT) {
        g_Billboards[f_EditedTreeIndex].textureIndex = 0;
    }

    if (g_Billboards[f_EditedTreeIndex].textureIndex < 0) {
        g_Billboards[f_EditedTreeIndex].textureIndex = BILLBOARD_TEXTURE_COUNT - 1;
    }

    billboardIndex = 0;
    if (g_MenuControllerData[EDITOR_CONTROLLER].button & B_BUTTON) {
        for (; billboardIndex < BILLBOARD_COUNT; billboardIndex++) {
            if (IsFreeBillboard(g_Billboards[billboardIndex])) {
                break;
            }
        }

        if (billboardIndex != BILLBOARD_COUNT) {
            g_Billboards[billboardIndex] = g_Billboards[f_EditedTreeIndex];
            g_Billboards[billboardIndex].x += COPIED_TREE_OFFSET;
            editKind = TRIANGLE_EDIT_SELECT;
            g_Billboards[f_EditedTreeIndex].isNearCourseEdge = FALSE;
            f_EditedTreeIndex = billboardIndex;
        }
    }

    if (!(g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG) &&
        *(u16 *)&g_MenuControllerData[EDITOR_CONTROLLER].stick_x != 0) {
        s32 moveX = (-cos((u16)f_TreeCameraYawAngle * BINARY_ANGLE_TO_RADIANS) *
                        g_MenuControllerData[EDITOR_CONTROLLER].stick_y) -
            (sin((u16)f_TreeCameraYawAngle * BINARY_ANGLE_TO_RADIANS) *
                g_MenuControllerData[EDITOR_CONTROLLER].stick_x);
        s32 moveZ = (-sin((u16)f_TreeCameraYawAngle * BINARY_ANGLE_TO_RADIANS) *
                        g_MenuControllerData[EDITOR_CONTROLLER].stick_y) +
            (cos((u16)f_TreeCameraYawAngle * BINARY_ANGLE_TO_RADIANS) *
                g_MenuControllerData[EDITOR_CONTROLLER].stick_x);

        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[f_EditedTreeIndex].x += moveX;
        g_Billboards[f_EditedTreeIndex].z += moveZ;
    }

    if (editKind != TRIANGLE_EDIT_NONE) {
        height = GetGroundHeight(
                     g_Billboards[f_EditedTreeIndex].x << WORLD_TO_COURSE_SHIFT,
                     g_Billboards[f_EditedTreeIndex].z << WORLD_TO_COURSE_SHIFT
                 ) /
            (1 << WORLD_TO_COURSE_SHIFT);
        guTranslateF(
            translation,
            g_Billboards[f_EditedTreeIndex].x,
            height + 1,
            g_Billboards[f_EditedTreeIndex].z
        );
        RevertBillboardManager();
        PlaceBillboards();
    }
    else {
        g_Billboards[f_EditedTreeIndex].isNearCourseEdge =
            !g_Billboards[f_EditedTreeIndex].isNearCourseEdge;
    }

    if ((g_ControllerData[EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[EDITOR_CONTROLLER].button & L_TRIG) ||
        (g_ControllerData[EDITOR_CONTROLLER].button & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[EDITOR_CONTROLLER].button & Z_TRIG) {
        f_TreeCameraYawAngle -=
            g_MenuControllerData[EDITOR_CONTROLLER].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_TreeCameraPitch -=
            g_MenuControllerData[EDITOR_CONTROLLER].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }

    if (f_TreeCameraPitch > 0.0f) {
        f_TreeCameraPitch = 0.0f;
    }

    if (f_TreeCameraPitch < EDITOR_PITCH_MIN) {
        f_TreeCameraPitch = EDITOR_PITCH_MIN;
    }

    cameraDistance = EDITOR_CAMERA_DISTANCE;
    g_MapCamera.view.lookAt[AXIS_X] = g_Billboards[f_EditedTreeIndex].x;
    g_MapCamera.view.lookAt[AXIS_Y] = g_Billboards[f_EditedTreeIndex].y;
    f_LastTreeEditKind = editKind;
    g_MapCamera.view.lookAt[AXIS_Z] = g_Billboards[f_EditedTreeIndex].z;
    f_TreeCameraYaw = f_TreeCameraYawAngle * EDITOR_ANGLE_TO_RADIANS;
    g_MapCamera.view.eye[AXIS_X] =
        (cos(f_TreeCameraPitch) * cos(f_TreeCameraYaw) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] =
        (cos(f_TreeCameraPitch) * sin(f_TreeCameraYaw) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] =
        (sin(f_TreeCameraPitch) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Y];
}

void BlendGroundAroundPoint(u32 column, u32 row, s32 radius, s32 falloff) {
    Vtx *center;
    Vtx *point;
    s32 rowOffset;
    s32 columnOffset;
    s32 halfWidth;
    s32 radiusSquared;
    f32 distance;
    f32 weight;

    center = GetGroundPoint(column, row);
    radiusSquared = radius * radius;
    for (rowOffset = -radius; rowOffset <= radius; rowOffset++) {
        if (row + rowOffset < GROUND_POINT_ROW_COUNT) {
            halfWidth = radiusSquared - (rowOffset * rowOffset);
            if (halfWidth < 0) {
                halfWidth = 0;
            }

            halfWidth = sqrt(halfWidth);
            for (columnOffset = -halfWidth; columnOffset <= halfWidth; columnOffset++) {
                if (column + columnOffset < GROUND_POINT_COLUMN_COUNT) {
                    point = GetGroundPoint(column + columnOffset, row + rowOffset);
                    distance = (f32)IntLength2D(
                                   center->v.ob[AXIS_X] - point->v.ob[AXIS_X],
                                   center->v.ob[AXIS_Z] - point->v.ob[AXIS_Z]
                               ) /
                        BLEND_DISTANCE_SCALE;
                    weight = distance * falloff;
                    if (weight > 1.0f) {
                        weight = 1.0f;
                    }

                    weight *= weight;
                    SetGroundPointHeightAndShade(
                        column + columnOffset,
                        row + rowOffset,
                        (point->v.ob[AXIS_Y] * weight) + (center->v.ob[AXIS_Y] * (1.0f - weight)),
                        BLENDED_POINT_SHADE
                    );
                }
            }
        }
    }
}

void UpdateTriangleSlopes(void) {
    PlaneTriangle firstTriangle;
    PlaneTriangle secondTriangle;
    IntegerPlane plane;
    s32 blockRow;
    s32 half;
    s32 blockColumn;
    s32 block;
    s32 pairRow;
    s32 cellRowInPair;
    s32 cellRow;
    s32 cellColumnInBlock;
    s32 row;
    s32 slopeIndex;
    s32 column;
    Vtx *corner;
    Vtx *nextColumnCorner;
    Vtx *nextRowCorner;
    Vtx *oppositeCorner;

    slopeIndex = 0;
    firstTriangle.corners[AXIS_X][0] = 0;
    firstTriangle.corners[AXIS_X][1] = START_TRIANGLE_SIZE;
    firstTriangle.corners[AXIS_X][2] = 0;
    firstTriangle.corners[AXIS_Z][0] = 0;
    firstTriangle.corners[AXIS_Z][1] = 0;
    firstTriangle.corners[AXIS_Z][2] = START_TRIANGLE_SIZE;
    secondTriangle.corners[AXIS_X][0] = START_TRIANGLE_SIZE;
    secondTriangle.corners[AXIS_X][1] = 0;
    secondTriangle.corners[AXIS_X][2] = START_TRIANGLE_SIZE;
    secondTriangle.corners[AXIS_Z][0] = 0;
    secondTriangle.corners[AXIS_Z][1] = START_TRIANGLE_SIZE;
    secondTriangle.corners[AXIS_Z][2] = START_TRIANGLE_SIZE;
    for (half = 0; half < SLOPE_HALF_COUNT; half++) {
        for (blockRow = 0; blockRow < SLOPE_BLOCK_ROW_COUNT; blockRow++) {
            for (blockColumn = 0; blockColumn < SLOPE_BLOCK_COLUMN_COUNT; blockColumn++) {
                block = (half * SLOPE_BLOCK_COLUMN_COUNT) + blockColumn;
                for (pairRow = 0; pairRow < 2; pairRow++) {
                    cellRow = ((blockRow * 2) + pairRow) * 2;
                    for (cellRowInPair = 0; cellRowInPair < 2; cellRowInPair++) {
                        for (cellColumnInBlock = 0; cellColumnInBlock < 2; cellColumnInBlock++) {
                            row = cellRow + cellRowInPair;
                            column = (block * 2) + cellColumnInBlock;
                            corner = GetGroundPoint(column, row);
                            nextColumnCorner = GetGroundPoint(column + 1, row);
                            nextRowCorner = GetGroundPoint(column, row + 1);
                            oppositeCorner = GetGroundPoint(column + 1, row + 1);
                            firstTriangle.corners[AXIS_Y][0] = corner->v.ob[AXIS_Y];
                            firstTriangle.corners[AXIS_Y][1] = nextColumnCorner->v.ob[AXIS_Y];
                            secondTriangle.corners[AXIS_Y][0] = firstTriangle.corners[AXIS_Y][1];
                            firstTriangle.corners[AXIS_Y][2] = nextRowCorner->v.ob[AXIS_Y];
                            secondTriangle.corners[AXIS_Y][2] = firstTriangle.corners[AXIS_Y][2];
                            secondTriangle.corners[AXIS_Y][1] = oppositeCorner->v.ob[AXIS_Y];
                            firstTriangle.corners[AXIS_X][0] = corner->v.ob[AXIS_X];
                            firstTriangle.corners[AXIS_X][1] = nextColumnCorner->v.ob[AXIS_X];
                            firstTriangle.corners[AXIS_X][2] = nextRowCorner->v.ob[AXIS_X];
                            firstTriangle.corners[AXIS_Z][0] = corner->v.ob[AXIS_Z];
                            firstTriangle.corners[AXIS_Z][1] = nextColumnCorner->v.ob[AXIS_Z];
                            firstTriangle.corners[AXIS_Z][2] = nextRowCorner->v.ob[AXIS_Z];
                            secondTriangle.corners[AXIS_X][0] = nextColumnCorner->v.ob[AXIS_X];
                            secondTriangle.corners[AXIS_X][1] = oppositeCorner->v.ob[AXIS_X];
                            secondTriangle.corners[AXIS_X][2] = nextRowCorner->v.ob[AXIS_X];
                            secondTriangle.corners[AXIS_Z][0] = nextColumnCorner->v.ob[AXIS_Z];
                            secondTriangle.corners[AXIS_Z][1] = oppositeCorner->v.ob[AXIS_Z];
                            secondTriangle.corners[AXIS_Z][2] = nextRowCorner->v.ob[AXIS_Z];
                            firstTriangle.corners[AXIS_X][1] -= firstTriangle.corners[AXIS_X][0];
                            firstTriangle.corners[AXIS_X][2] -= firstTriangle.corners[AXIS_X][0];
                            firstTriangle.corners[AXIS_X][0] = 0;
                            firstTriangle.corners[AXIS_Z][1] -= firstTriangle.corners[AXIS_Z][0];
                            firstTriangle.corners[AXIS_Z][2] -= firstTriangle.corners[AXIS_Z][0];
                            firstTriangle.corners[AXIS_Z][0] = 0;
                            secondTriangle.corners[AXIS_X][1] -= secondTriangle.corners[AXIS_X][0];
                            secondTriangle.corners[AXIS_X][2] -= secondTriangle.corners[AXIS_X][0];
                            secondTriangle.corners[AXIS_X][0] = 0;
                            secondTriangle.corners[AXIS_Z][1] -= secondTriangle.corners[AXIS_Z][0];
                            secondTriangle.corners[AXIS_Z][2] -= secondTriangle.corners[AXIS_Z][0];
                            secondTriangle.corners[AXIS_Z][0] = 0;
                            plane.normal[AXIS_Z] = 0;
                            plane.normal[AXIS_X] = 0;
                            GetTriangleIntegerPlane(&firstTriangle, &plane);
                            f_TriangleSlopes[slopeIndex].normalX = plane.normal[AXIS_X] * 2;
                            f_TriangleSlopes[slopeIndex].normalZ = plane.normal[AXIS_Z] * 2;
                            f_TriangleSlopes[slopeIndex].shade =
                                IntLength2D(plane.normal[AXIS_X], plane.normal[AXIS_Z]);
                            if (f_TriangleSlopes[slopeIndex].shade != 0) {
                                if (f_TriangleSlopes[slopeIndex].shade <
                                    SLOPE_SHADE_MAX - SLOPE_SHADE_OFFSET) {
                                    f_TriangleSlopes[slopeIndex].shade += SLOPE_SHADE_OFFSET;
                                }
                                else {
                                    f_TriangleSlopes[slopeIndex].shade = SLOPE_SHADE_MAX;
                                }
                            }

                            slopeIndex++;
                            GetTriangleIntegerPlane(&secondTriangle, &plane);
                            f_TriangleSlopes[slopeIndex].normalX = plane.normal[AXIS_X] * 2;
                            f_TriangleSlopes[slopeIndex].normalZ = plane.normal[AXIS_Z] * 2;
                            f_TriangleSlopes[slopeIndex].shade =
                                IntLength2D(plane.normal[AXIS_X], plane.normal[AXIS_Z]);
                            if (f_TriangleSlopes[slopeIndex].shade != 0) {
                                if (f_TriangleSlopes[slopeIndex].shade <
                                    SLOPE_SHADE_MAX - SLOPE_SHADE_OFFSET) {
                                    f_TriangleSlopes[slopeIndex].shade += SLOPE_SHADE_OFFSET;
                                }
                                else {
                                    f_TriangleSlopes[slopeIndex].shade = SLOPE_SHADE_MAX;
                                }
                            }

                            slopeIndex++;
                        }
                    }
                }
            }
        }
    }
}

void FillWaveTexels(void) {
    s32 row;
    s32 column;
    s32 texelIndex;
    s32 level;

    texelIndex = 0;
    for (row = 0; row < WAVE_TEXTURE_HEIGHT; row++) {
        for (column = 0; column < WAVE_TEXTURE_WIDTH; column++) {
            level = cos((f32)(column * WAVE_TEXEL_STEP) * WAVE_ANGLE_STEP) *
                sin((f32)(row * WAVE_TEXEL_STEP) * WAVE_ANGLE_STEP) * WAVE_AMPLITUDE;
            level *= level * level;
            level /= 1 << WAVE_LEVEL_SHIFT;
            if (level <= 0) {
                level = WAVE_LEVEL_MIN;
            }

            if (level > WAVE_LEVEL_MAX) {
                level = WAVE_LEVEL_MAX;
            }

            f_WaveTexels[texelIndex++] = (level << IA8_ALPHA_SHIFT) | level;
        }
    }
}

void WriteHexDumpFile(u32 *words, s32 byteCount, char *fileName) {
    char text[HEX_DUMP_TEXT_SIZE];
    char *cursor;
    char *end;
    char *buffer;
    s32 line;
    s32 wordIndex;
    u32 *lineWords;

    void AppendText(char *source) {
        char character;

        while (TRUE) {
            if ((character = *source++) == '\0') {
                return;
            }

            if (cursor < end) {
                *cursor++ = character;
            }
        }
    }

    buffer = AllocMemory(HEX_DUMP_BUFFER_SIZE);
    cursor = buffer;
    end = buffer + HEX_DUMP_BUFFER_SIZE;
    AssertInEditor(buffer != NULL, HOLE_EDITOR_HEX_DUMP_ASSERT_LINE);
    for (line = 0; line < (byteCount + HEX_DUMP_LINE_BYTES - 1) / HEX_DUMP_LINE_BYTES; line++) {
        lineWords = words;
        sprintf(text, "%4.4X: ", (unsigned int)(line * HEX_DUMP_LINE_BYTES));
        AppendText(text);
        for (wordIndex = 0; wordIndex < HEX_DUMP_LINE_WORDS; wordIndex++) {
            sprintf(
                text,
                "%8.8X ",
                (unsigned int)(lineWords[(line * HEX_DUMP_LINE_WORDS) + wordIndex])
            );
            AppendText(text);
        }

        sprintf(text, "\n");
        AppendText(text);
    }

    text[0] = END_OF_FILE_CHARACTER;
    text[1] = '\0';
    AppendText(text);
    sprintf(text, fileName);
    WriteHostFile(text, cursor - buffer);
    FreeMemory(&buffer);
}

void DrawHoleEditorGround(CameraMatrices *matrices UNUSED, TerrainStyle terrainStyle) {
    UBoolean8 snapsEdge[2][TILE_VERTEX_SEGMENT_COUNT];
    Boolean keepsCombineMode;
    EditorCombineMode combineMode;
    Gfx *gfx;
    s32 half;
    Vtx *vertices;
    u32 start;
    u8 currentSnapSet;
    s32 firstTileS;
    s32 visibility;
    GroundDetail detail;
    GroundDetail previousDetail;
    s32 slopeIndex;
    s32 row;
    s32 detailTileIndex;
    s32 groupTileIndex;
    s32 edgeSegment;
    s32 segmentIndex;
    s32 edge;
    s32 pass;
    s32 mediumPass;
    s32 coarsePass;
    s32 tileIndex;
    s32 topSlot;
    s32 middleSlot;
    s32 bottomSlot;
    s32 spareSlot;
    s32 blockColumn;
    s32 blockRow;
    s32 cellColumn;
    s32 cellRow;
    s32 vertexColumn;
    Boolean isMirrored;
    Vtx *midpoints;
    GroundTile *tile;
    s32 groupOffset;

    void GetMidpointVertex(Vtx_t * first, Vtx_t * second, Vtx_t * midpoint) {
        midpoint->ob[AXIS_X] = (first->ob[AXIS_X] + second->ob[AXIS_X]) / 2;
        midpoint->ob[AXIS_Y] = (first->ob[AXIS_Y] + second->ob[AXIS_Y]) / 2;
        midpoint->ob[AXIS_Z] = (first->ob[AXIS_Z] + second->ob[AXIS_Z]) / 2;
        midpoint->flag = 0;
        midpoint->tc[TEXTURE_AXIS_S] = (first->tc[TEXTURE_AXIS_S] + second->tc[TEXTURE_AXIS_S]) / 2;
        midpoint->tc[TEXTURE_AXIS_T] = (first->tc[TEXTURE_AXIS_T] + second->tc[TEXTURE_AXIS_T]) / 2;
        midpoint->cn[0] = (first->cn[0] + second->cn[0]) / 2;
        midpoint->cn[1] = (first->cn[1] + second->cn[1]) / 2;
        midpoint->cn[2] = (first->cn[2] + second->cn[2]) / 2;
        midpoint->cn[3] = (first->cn[3] + second->cn[3]) / 2;
    }

    void SetEditorCombineMode(TilePass tilePass, GroundOverlay overlay) {
        if (!keepsCombineMode) {
            switch (tilePass) {
                case TILE_PASS_ONLY:
                    if (overlay == GROUND_OVERLAY_NONE) {
                        if (combineMode != EDITOR_COMBINE_TILE_ONLY) {
                            combineMode = EDITOR_COMBINE_TILE_ONLY;
                            gDPSetCombineLERP(
                                gfx++,
                                0,
                                0,
                                0,
                                TEXEL1,
                                0,
                                0,
                                0,
                                TEXEL1,
                                0,
                                0,
                                0,
                                COMBINED,
                                0,
                                0,
                                0,
                                1
                            );
                        }
                    }
                    else if (overlay == GROUND_OVERLAY_WAVES) {
                        if (combineMode != EDITOR_COMBINE_WAVES_ONLY) {
                            combineMode = EDITOR_COMBINE_WAVES_ONLY;
                            gDPSetCombineLERP(
                                gfx++,
                                0,
                                0,
                                0,
                                TEXEL1,
                                PRIMITIVE,
                                0,
                                TEXEL0,
                                0,
                                PRIMITIVE,
                                COMBINED,
                                COMBINED_ALPHA,
                                COMBINED,
                                0,
                                0,
                                0,
                                1
                            );
                        }
                    }
                    else if (combineMode != EDITOR_COMBINE_NOISE_ONLY) {
                        combineMode = EDITOR_COMBINE_NOISE_ONLY;
                        gDPPipeSync(gfx++);
                        gDPSetAlphaCompare(gfx++, G_AC_NONE);
                        gDPSetCombineLERP(
                            gfx++,
                            TEXEL0,
                            0,
                            TEXEL1,
                            0,
                            ENVIRONMENT,
                            SHADE,
                            TEXEL1,
                            ENVIRONMENT,
                            COMBINED,
                            TEXEL0,
                            COMBINED_ALPHA,
                            TEXEL0,
                            0,
                            0,
                            0,
                            1
                        );
                    }
                    break;

                case TILE_PASS_FIRST:
                    if (overlay == GROUND_OVERLAY_NONE) {
                        if (combineMode != EDITOR_COMBINE_TILE_TWICE) {
                            combineMode = EDITOR_COMBINE_TILE_TWICE;
                            gDPSetCombineLERP(
                                gfx++,
                                0,
                                0,
                                0,
                                TEXEL1,
                                0,
                                0,
                                0,
                                TEXEL1,
                                0,
                                0,
                                0,
                                COMBINED,
                                0,
                                0,
                                0,
                                COMBINED
                            );
                        }
                    }
                    else if (overlay == GROUND_OVERLAY_WAVES) {
                        if (combineMode != EDITOR_COMBINE_WAVES_FIRST) {
                            combineMode = EDITOR_COMBINE_WAVES_FIRST;
                            gDPSetCombineLERP(
                                gfx++,
                                0,
                                0,
                                0,
                                TEXEL1,
                                PRIMITIVE,
                                0,
                                TEXEL0,
                                0,
                                PRIMITIVE,
                                COMBINED,
                                COMBINED_ALPHA,
                                COMBINED,
                                0,
                                0,
                                0,
                                TEXEL0
                            );
                        }
                    }
                    else if (combineMode != EDITOR_COMBINE_NOISE_FIRST) {
                        combineMode = EDITOR_COMBINE_NOISE_FIRST;
                        gDPPipeSync(gfx++);
                        gDPSetAlphaCompare(gfx++, G_AC_NONE);
                        gDPSetCombineLERP(
                            gfx++,
                            TEXEL0,
                            0,
                            TEXEL1,
                            0,
                            1,
                            SHADE,
                            ENVIRONMENT,
                            ENVIRONMENT,
                            COMBINED,
                            TEXEL0,
                            COMBINED_ALPHA,
                            TEXEL0,
                            0,
                            0,
                            0,
                            TEXEL0
                        );
                    }
                    break;

                case TILE_PASS_SECOND:
                default:
                    if (overlay == GROUND_OVERLAY_NONE) {
                        if (combineMode != EDITOR_COMBINE_TILE_TWICE) {
                            combineMode = EDITOR_COMBINE_TILE_TWICE;
                            gDPSetCombineLERP(
                                gfx++,
                                0,
                                0,
                                0,
                                TEXEL1,
                                0,
                                0,
                                0,
                                TEXEL1,
                                0,
                                0,
                                0,
                                COMBINED,
                                0,
                                0,
                                0,
                                COMBINED
                            );
                        }
                    }
                    else if (overlay == GROUND_OVERLAY_WAVES) {
                        if (combineMode != EDITOR_COMBINE_WAVES_SECOND) {
                            combineMode = EDITOR_COMBINE_WAVES_SECOND;
                            gDPSetCombineLERP(
                                gfx++,
                                0,
                                0,
                                0,
                                TEXEL1,
                                PRIMITIVE,
                                0,
                                TEXEL0,
                                0,
                                PRIMITIVE,
                                COMBINED,
                                COMBINED_ALPHA,
                                COMBINED,
                                0,
                                0,
                                0,
                                TEXEL0
                            );
                        }
                    }
                    else if (combineMode != EDITOR_COMBINE_NOISE_SECOND) {
                        combineMode = EDITOR_COMBINE_NOISE_SECOND;
                        gDPPipeSync(gfx++);
                        gDPSetAlphaCompare(gfx++, G_AC_NONE);
                        gDPSetCombineLERP(
                            gfx++,
                            TEXEL0,
                            0,
                            TEXEL1,
                            0,
                            0,
                            0,
                            0,
                            ENVIRONMENT,
                            COMBINED,
                            TEXEL0,
                            COMBINED_ALPHA,
                            TEXEL0,
                            0,
                            0,
                            0,
                            TEXEL0
                        );
                        gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, SECOND_PASS_BLEND_ALPHA);
                    }
                    break;
            }
        }
    }

    currentSnapSet = 0;
    vertices = g_VertexListHead;
    gfx = f_EditorGroundDisplayLists[g_DrawBufferIndex];
    start = osGetCount();
    combineMode = EDITOR_COMBINE_NONE;
    keepsCombineMode = FALSE;
    if (!g_HasLoaderFinished) {
        return;
    }

    memset(f_WhiteNoiseTexels, WHITE_TEXEL, sizeof(f_WhiteNoiseTexels));
    if (IsGameFlagSet(GAME_FLAG_TEST_GROUND_TEXTURE)) {
        RebuildGroundTextureCoordinates();
    }

    if (IsGameFlagSet(GAME_FLAG_CYCLE_CLIP_RATIO)) {
        char text[CLIP_RATIO_TEXT_SIZE];

        sprintf(text, "CLIPRATIO=%d", f_EditorClipRatio);
        PrintDebugText(text, CLIP_RATIO_TEXT_COLUMN, CLIP_RATIO_TEXT_ROW);
        switch (f_EditorClipRatio) {
            case 1:
                gSPClipRatio(gfx++, FRUSTRATIO_1);
                break;
            case 2:
                gSPClipRatio(gfx++, FRUSTRATIO_2);
                break;
            case 3:
                gSPClipRatio(gfx++, FRUSTRATIO_3);
                break;
            case 4:
                gSPClipRatio(gfx++, FRUSTRATIO_4);
                break;
            case 5:
                gSPClipRatio(gfx++, FRUSTRATIO_5);
                break;
            case 6:
                gSPClipRatio(gfx++, FRUSTRATIO_6);
                break;
        }

        if (g_ControllerData[1].trigger & A_BUTTON) {
            f_EditorClipRatio = (f_EditorClipRatio % CLIP_RATIO_COUNT) + 1;
        }
    }
    else {
        gSPClipRatio(gfx++, FRUSTRATIO_1);
    }

    if (IsGameFlagSet(GAME_FLAG_EDITOR_GROUND_NPRIMITIVE)) {
        gDPPipeSync(gfx++);
        gDPPipelineMode(gfx++, G_PM_NPRIMITIVE);
    }
    else {
        gDPPipeSync(gfx++);
        gDPPipelineMode(gfx++, G_PM_1PRIMITIVE);
    }

    if (IsGameFlagSet(GAME_FLAG_HIDE_GROUND)) {
        return;
    }

    gDPPipeSync(gfx++);
    gSPLoadGeometryMode(gfx++, 0);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureLOD(gfx++, G_TL_TILE);
    gDPPipeSync(gfx++);
    gDPSetTextureDetail(gfx++, G_TD_CLAMP);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    if (IsMapTerrainStyle(terrainStyle)) {
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_2CYCLE);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        gSPLoadGeometryMode(gfx++, 0);
        gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
        gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, MAP_BLEND_ALPHA);
        if (g_ShowsGroundWaves) {
            LoadWaveTexture();
            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                TEXEL1,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                PRIMITIVE,
                COMBINED,
                COMBINED_ALPHA,
                COMBINED,
                0,
                0,
                0,
                1
            );
            gDPPipeSync(gfx++);
            gDPSetAlphaCompare(gfx++, G_AC_NONE);
        }
        else {
            gDPLoadMultiBlock(
                gfx++,
                TextureAddress(f_WhiteNoiseTexels),
                NOISE_TMEM,
                G_TX_RENDERTILE,
                G_IM_FMT_I,
                G_IM_SIZ_8b,
                WHITE_TEXTURE_SIZE,
                WHITE_TEXTURE_SIZE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                TextureMask(WHITE_TEXTURE_SIZE),
                TextureMask(WHITE_TEXTURE_SIZE),
                12,
                12
            );
            gDPSetCombineLERP(
                gfx++,
                TEXEL1,
                ENVIRONMENT,
                PRIMITIVE,
                ENVIRONMENT,
                0,
                0,
                0,
                TEXEL1,
                0,
                0,
                0,
                COMBINED,
                0,
                0,
                0,
                COMBINED
            );
            gDPPipeSync(gfx++);
            gDPSetAlphaCompare(gfx++, G_AC_NONE);
            keepsCombineMode = TRUE;
        }
    }

    if (terrainStyle == TERRAIN_STYLE_NORMAL) {
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_2CYCLE);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_TEX_EDGE2);
        gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_FOG | G_SHADING_SMOOTH);
        SetSceneFog(&gfx);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        if (g_ShowsGroundWaves) {
            LoadWaveTexture();
            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                TEXEL1,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                PRIMITIVE,
                COMBINED,
                COMBINED_ALPHA,
                COMBINED,
                0,
                0,
                0,
                COMBINED
            );
            gDPPipeSync(gfx++);
            gDPSetAlphaCompare(gfx++, G_AC_NONE);
            SetEditorCombineMode(TILE_PASS_ONLY, GROUND_OVERLAY_WAVES);
        }
        else {
            if (IsGameFlagSet(GAME_FLAG_TEST_GROUND_TEXTURE)) {
                LoadWhiteNoiseTexture(13, 13);
            }
            else {
                LoadWhiteNoiseTexture(13, 15);
            }

            SetEditorCombineMode(TILE_PASS_ONLY, GROUND_OVERLAY_NOISE);
            gDPSetEnvColor(gfx++, 0, 0, 0, NOISE_BLEND_ALPHA);
        }

        gDPPipeSync(gfx++);
    }

    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    g_GroundFarthestDepth = 0.0f;
    g_GroundNearDepthEighth = NO_NEAR_DEPTH;
    for (tileIndex = 0; tileIndex < GROUND_TILE_COUNT; tileIndex++) {
        tile = &g_WorkArea.hole.tiles[tileIndex];
        visibility = GetSphereVisibility(
            tile->center[AXIS_X],
            tile->center[AXIS_Y],
            tile->center[AXIS_Z],
            GROUND_TILE_RADIUS_MIN
        );
        detail = GROUND_DETAIL_HIDDEN;
        if (visibility < 0) {
            if (tile->midpointBlockIndex >= 0 &&
                visibility * FINE_DEPTH_SCALE < g_GroundTileHeightRanges[tileIndex]) {
                detail = GROUND_DETAIL_FINE;
            }
            else if (visibility * MEDIUM_DEPTH_SCALE < g_GroundTileHeightRanges[tileIndex]) {
                detail = GROUND_DETAIL_MEDIUM;
            }
            else {
                detail = GROUND_DETAIL_COARSE;
            }
        }

        if (IsGroundEditorScene(g_Scene) &&
            IsMapTerrainStyle(terrainStyle) &&
            detail != GROUND_DETAIL_HIDDEN) {
            detail = GROUND_DETAIL_FINE;
        }

        tile->detail = detail;
        if (detail > GROUND_DETAIL_HIDDEN) {
            if (visibility < g_GroundFarthestDepth) {
                g_GroundFarthestDepth = visibility;
            }

            if (g_GroundNearDepthEighth < visibility / NEAR_DEPTH_DIVISOR) {
                g_GroundNearDepthEighth = visibility / NEAR_DEPTH_DIVISOR;
            }
        }
    }

    if (IsGameFlagSet(GAME_FLAG_TEST_GROUND_TEXTURE)) {
        osSyncPrintf("%f\n", g_GroundFarthestDepth);
    }

    if (g_ShowsGroundWaves) {
        for (slopeIndex = 0; slopeIndex < ARRAY_COUNT(f_TriangleSlopes); slopeIndex++) {
            f_TriangleSlopes[slopeIndex].waveS -= f_TriangleSlopes[slopeIndex].normalX;
            f_TriangleSlopes[slopeIndex].waveT -= f_TriangleSlopes[slopeIndex].normalZ;
        }
    }

    slopeIndex = 0;
    for (edge = 0; edge < ARRAY_COUNT(snapsEdge); edge++) {
        for (segmentIndex = 0; segmentIndex < TILE_VERTEX_SEGMENT_COUNT; segmentIndex++) {
            snapsEdge[edge][segmentIndex] = FALSE;
        }
    }

    gSPSegment(gfx++, 1, 0);
    gSPSegment(gfx++, 2, 0);
    gSPSegment(gfx++, 3, 0);
    gSPSegment(gfx++, 4, 0);
    gSPSegment(gfx++, 5, 0);
    gSPSegment(gfx++, 6, 0);
    gSPSegment(gfx++, 7, 0);
    gSPSegment(gfx++, 8, 0);
    for (half = 0; half < GROUND_HALF_COUNT; half++) {
        previousDetail = GROUND_DETAIL_HIDDEN;
        tileIndex = half * GROUND_TILE_GROUP_SIZE;
        topSlot = FIRST_TOP_SLOT;
        firstTileS = tileIndex * TILE_COORDINATE_SPAN;
        middleSlot = FIRST_MIDDLE_SLOT;
        bottomSlot = FIRST_BOTTOM_SLOT;
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0, f_TriangleSlopes[slopeIndex].shade);
        gDPSetTileSize(
            gfx++,
            G_TX_RENDERTILE,
            f_TriangleSlopes[slopeIndex].waveS / WAVE_SCROLL_DIVISOR,
            f_TriangleSlopes[slopeIndex].waveT / WAVE_SCROLL_DIVISOR,
            (f_TriangleSlopes[slopeIndex].waveS / WAVE_SCROLL_DIVISOR) + WAVE_TILE_SPAN,
            (f_TriangleSlopes[slopeIndex].waveT / WAVE_SCROLL_DIVISOR) + WAVE_TILE_SPAN
        );
        gDPPipeSync(gfx++);

        for (row = 0; row < GROUND_TILE_ROW_COUNT; row++) {
            GroundDetail groupDetail;

            groupDetail = GROUND_DETAIL_COARSE;
            for (detailTileIndex = 0; detailTileIndex < GROUND_TILE_GROUP_SIZE; detailTileIndex++) {
                if (groupDetail < g_WorkArea.hole.tiles[tileIndex + detailTileIndex].detail) {
                    groupDetail = g_WorkArea.hole.tiles[tileIndex + detailTileIndex].detail;
                }
            }

            if (groupDetail >= GROUND_DETAIL_FINE) {
                s32 groupTile;

                for (groupTile = 0; groupTile < GROUND_TILE_GROUP_SIZE; groupTile++) {
                    s32 mirroredS;
                    s32 finePass;

                    midpoints = g_WorkArea.hole
                                    .midpoints[g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex];
                    mirroredS =
                        ((firstTileS + (groupTile * TILE_COORDINATE_SPAN)) + MIRROR_S_OFFSET) *
                        MIRROR_S_SCALE;

                    if (IsMapTerrainStyle(terrainStyle)) {
                        Boolean isOdd = (row + groupTile) & 1;
                        s32 red = isOdd * COLOR_CHANNEL_MAX;
                        s32 blue = COLOR_CHANNEL_MAX - red;

                        gDPSetEnvColor(gfx++, red, 0, blue, OPAQUE_ALPHA);
                    }

                    if (g_WorkArea.hole.tiles[tileIndex].detail == GROUND_DETAIL_FINE &&
                        g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex >= 0) {
                        if (IsGameFlagSet(GAME_FLAG_HOLE_LOADED)) {
                            if ((half * GROUND_TILE_GROUP_SIZE) + groupTile != 0) {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_LEFT] =
                                    IsCoarse(tileIndex - 1);
                                snapsEdge[currentSnapSet][TILE_EDGE_LEFT] =
                                    IsNotFine(tileIndex - 1);
                            }
                            else {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_LEFT] = FALSE;
                                snapsEdge[currentSnapSet][TILE_EDGE_LEFT] = FALSE;
                            }

                            if (row != 0) {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_TOP] =
                                    IsCoarse(tileIndex - GROUND_TILE_COLUMN_COUNT);
                                snapsEdge[currentSnapSet][TILE_EDGE_TOP] =
                                    IsNotFine(tileIndex - GROUND_TILE_COLUMN_COUNT);
                            }
                            else {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_TOP] = FALSE;
                                snapsEdge[currentSnapSet][TILE_EDGE_TOP] = FALSE;
                            }

                            if (row != GROUND_TILE_ROW_COUNT - 1) {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_BOTTOM] =
                                    IsCoarse(tileIndex + GROUND_TILE_COLUMN_COUNT);
                                snapsEdge[currentSnapSet][TILE_EDGE_BOTTOM] =
                                    IsNotFine(tileIndex + GROUND_TILE_COLUMN_COUNT);
                            }
                            else {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_BOTTOM] =
                                    FALSE;
                                snapsEdge[currentSnapSet][TILE_EDGE_BOTTOM] = FALSE;
                            }

                            if ((half * GROUND_TILE_GROUP_SIZE) + groupTile !=
                                GROUND_TILE_COLUMN_COUNT - 1) {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_RIGHT] =
                                    IsCoarse(tileIndex + 1);
                                snapsEdge[currentSnapSet][TILE_EDGE_RIGHT] =
                                    IsNotFine(tileIndex + 1);
                            }
                            else {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_RIGHT] =
                                    FALSE;
                                snapsEdge[currentSnapSet][TILE_EDGE_RIGHT] = FALSE;
                            }
                            for (edgeSegment = 0; edgeSegment < TILE_VERTEX_SEGMENT_COUNT;
                                edgeSegment++) {
                                if (snapsEdge[currentSnapSet][edgeSegment] ||
                                    snapsEdge[currentSnapSet ^ 1][edgeSegment]) {
                                    s32 offset;

                                    offset = snapsEdge[currentSnapSet][edgeSegment] ?
                                        g_CoarseVertexOffsets[tileIndex][edgeSegment] :
                                        0;
                                    gSPSegment(gfx++, edgeSegment + 1, offset);
                                }
                            }

                            currentSnapSet ^= 1;
                            gSPDisplayList(gfx++, g_GroundTileDisplayLists[tileIndex]);
                            tileIndex++;
                            continue;
                        }
                        else {
                            vertexColumn = (groupTile * 2) + HalfFirstColumn(half);
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn, row * 2),
                                (f64 *)&vertices[TILE_VERTEX_ROW0_COLUMN0]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn, (row * 2) + 1),
                                (f64 *)&vertices[TILE_VERTEX_ROW2_COLUMN0]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn, (row * 2) + 2),
                                (f64 *)&vertices[TILE_VERTEX_ROW4_COLUMN0]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn + 1, row * 2),
                                (f64 *)&vertices[TILE_VERTEX_ROW0_COLUMN2]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn + 1, (row * 2) + 1),
                                (f64 *)&vertices[TILE_VERTEX_ROW2_COLUMN2]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn + 1, (row * 2) + 2),
                                (f64 *)&vertices[TILE_VERTEX_ROW4_COLUMN2]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn + 2, row * 2),
                                (f64 *)&vertices[TILE_VERTEX_ROW0_COLUMN4]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn + 2, (row * 2) + 1),
                                (f64 *)&vertices[TILE_VERTEX_ROW2_COLUMN4]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn + 2, (row * 2) + 2),
                                (f64 *)&vertices[TILE_VERTEX_ROW4_COLUMN4]
                            );
                            if ((half * GROUND_TILE_GROUP_SIZE) + groupTile == 0 ||
                                g_WorkArea.hole.tiles[tileIndex - 1].detail !=
                                    GROUND_DETAIL_MEDIUM) {
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW1_COLUMN0],
                                    (f64 *)&vertices[TILE_VERTEX_ROW1_COLUMN0]
                                );
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW3_COLUMN0],
                                    (f64 *)&vertices[TILE_VERTEX_ROW3_COLUMN0]
                                );
                            }
                            else {
                                GetMidpointVertex(
                                    &vertices[TILE_VERTEX_ROW0_COLUMN0].v,
                                    &vertices[TILE_VERTEX_ROW2_COLUMN0].v,
                                    &vertices[TILE_VERTEX_ROW1_COLUMN0].v
                                );
                                GetMidpointVertex(
                                    &vertices[TILE_VERTEX_ROW2_COLUMN0].v,
                                    &vertices[TILE_VERTEX_ROW4_COLUMN0].v,
                                    &vertices[TILE_VERTEX_ROW3_COLUMN0].v
                                );
                            }

                            if (row == 0 ||
                                g_WorkArea.hole.tiles[tileIndex - GROUND_TILE_COLUMN_COUNT]
                                        .detail != GROUND_DETAIL_MEDIUM) {
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW0_COLUMN1],
                                    (f64 *)&vertices[TILE_VERTEX_ROW0_COLUMN1]
                                );
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW0_COLUMN3],
                                    (f64 *)&vertices[TILE_VERTEX_ROW0_COLUMN3]
                                );
                            }
                            else {
                                GetMidpointVertex(
                                    &vertices[TILE_VERTEX_ROW0_COLUMN0].v,
                                    &vertices[TILE_VERTEX_ROW0_COLUMN2].v,
                                    &vertices[TILE_VERTEX_ROW0_COLUMN1].v
                                );
                                GetMidpointVertex(
                                    &vertices[TILE_VERTEX_ROW0_COLUMN2].v,
                                    &vertices[TILE_VERTEX_ROW0_COLUMN4].v,
                                    &vertices[TILE_VERTEX_ROW0_COLUMN3].v
                                );
                            }

                            if (row == GROUND_TILE_ROW_COUNT - 1 ||
                                g_WorkArea.hole.tiles[tileIndex + GROUND_TILE_COLUMN_COUNT]
                                        .detail != GROUND_DETAIL_MEDIUM) {
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW4_COLUMN1],
                                    (f64 *)&vertices[TILE_VERTEX_ROW4_COLUMN1]
                                );
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW4_COLUMN3],
                                    (f64 *)&vertices[TILE_VERTEX_ROW4_COLUMN3]
                                );
                            }
                            else {
                                GetMidpointVertex(
                                    &vertices[TILE_VERTEX_ROW4_COLUMN0].v,
                                    &vertices[TILE_VERTEX_ROW4_COLUMN2].v,
                                    &vertices[TILE_VERTEX_ROW4_COLUMN1].v
                                );
                                GetMidpointVertex(
                                    &vertices[TILE_VERTEX_ROW4_COLUMN2].v,
                                    &vertices[TILE_VERTEX_ROW4_COLUMN4].v,
                                    &vertices[TILE_VERTEX_ROW4_COLUMN3].v
                                );
                            }

                            if ((half * GROUND_TILE_GROUP_SIZE) + groupTile ==
                                    GROUND_TILE_COLUMN_COUNT - 1 ||
                                g_WorkArea.hole.tiles[tileIndex + 1].detail !=
                                    GROUND_DETAIL_MEDIUM) {
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW1_COLUMN4],
                                    (f64 *)&vertices[TILE_VERTEX_ROW1_COLUMN4]
                                );
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW3_COLUMN4],
                                    (f64 *)&vertices[TILE_VERTEX_ROW3_COLUMN4]
                                );
                            }
                            else {
                                GetMidpointVertex(
                                    &vertices[TILE_VERTEX_ROW0_COLUMN4].v,
                                    &vertices[TILE_VERTEX_ROW2_COLUMN4].v,
                                    &vertices[TILE_VERTEX_ROW1_COLUMN4].v
                                );
                                GetMidpointVertex(
                                    &vertices[TILE_VERTEX_ROW2_COLUMN4].v,
                                    &vertices[TILE_VERTEX_ROW4_COLUMN4].v,
                                    &vertices[TILE_VERTEX_ROW3_COLUMN4].v
                                );
                            }

                            gSPVertex(
                                gfx++,
                                &vertices[TILE_VERTEX_ROW0_COLUMN0],
                                TILE_FIRST_COLUMN_VERTEX_COUNT,
                                TILE_VERTEX_ROW0_COLUMN0
                            );
                            gSPVertex(
                                gfx++,
                                &vertices[TILE_VERTEX_ROW0_COLUMN1],
                                TILE_MIDDLE_COLUMN_VERTEX_COUNT,
                                TILE_VERTEX_ROW0_COLUMN1
                            );
                            gSPVertex(
                                gfx++,
                                &midpoints[TILE_FIRST_INNER_MIDPOINT],
                                TILE_INNER_MIDPOINT_COUNT,
                                TILE_INNER_MIDPOINT_CACHE_SLOT
                            );
                            gSPVertex(
                                gfx++,
                                &vertices[TILE_VERTEX_ROW0_COLUMN4],
                                TILE_LAST_COLUMN_VERTEX_COUNT,
                                TILE_LAST_COLUMN_CACHE_SLOT
                            );
                            vertices += FINE_TILE_BUILT_VERTEX_COUNT;
                            for (finePass = 0; finePass < GROUND_TILE_PASSES_MAX; finePass++) {
                                LoadEditorTileTexture(
                                    tileIndex,
                                    firstTileS,
                                    groupTile,
                                    row,
                                    finePass,
                                    g_ShowsGroundWaves
                                );

                                for (blockColumn = 0; blockColumn < FINE_TILE_CELL_SPAN;
                                    blockColumn += FINE_TILE_BLOCK_SPAN) {
                                    for (blockRow = 0; blockRow < FINE_TILE_CELL_SPAN;
                                        blockRow += FINE_TILE_BLOCK_SPAN) {
                                        for (cellRow = blockRow;
                                            cellRow < blockRow + FINE_TILE_BLOCK_SPAN;
                                            cellRow++) {
                                            for (cellColumn = blockColumn;
                                                cellColumn < blockColumn + FINE_TILE_BLOCK_SPAN;
                                                cellColumn++) {
                                                if (IsGameFlagSet(GAME_FLAG_TEST_GROUND_TEXTURE)) {
                                                    isMirrored = g_WorkArea.hole.tiles[tileIndex]
                                                                     .mirroredCells &
                                                        (1
                                                            << (cellColumn +
                                                                   (cellRow *
                                                                       FINE_TILE_CELL_SPAN)));
                                                }
                                                else {
                                                    isMirrored = g_WorkArea.hole.tiles[tileIndex]
                                                                     .mirroredCells &
                                                        (1
                                                            << (cellColumn +
                                                                   (cellRow *
                                                                       FINE_TILE_CELL_SPAN)));
                                                }
                                                if (isMirrored) {
                                                    s32 pointColumn;
                                                    s32 pointRow;

                                                    for (pointRow = 0;
                                                        pointRow < FINE_TILE_BLOCK_SPAN;
                                                        pointRow++) {
                                                        for (pointColumn = 0;
                                                            pointColumn < FINE_TILE_BLOCK_SPAN;
                                                            pointColumn++) {
                                                            Vtx *point = GetGroundPoint(
                                                                (half * GROUND_HALF_CELL_COLUMNS) +
                                                                    (groupTile *
                                                                        FINE_TILE_CELL_SPAN) +
                                                                    cellColumn + pointColumn,
                                                                (row * FINE_TILE_CELL_SPAN) +
                                                                    cellRow + pointRow
                                                            );
                                                            s32 packedTextureCoordinates;

                                                            if (IsGameFlagSet(
                                                                    GAME_FLAG_TEST_GROUND_TEXTURE
                                                                )) {
                                                                packedTextureCoordinates =
                                                                    ((
                                                                         MIRRORED_TEST_S -
                                                                         (point->v.tc
                                                                                 [TEXTURE_AXIS_S] /
                                                                             2)
                                                                     ) << 16) |
                                                                    ((point->v.tc[TEXTURE_AXIS_T] /
                                                                         2) &
                                                                        0xFFFF);
                                                                gDPSetTileSize(
                                                                    gfx++,
                                                                    1,
                                                                    MIRRORED_TEST_FIRST_S,
                                                                    0,
                                                                    MIRRORED_TEST_LAST_S,
                                                                    TEST_TILE_SIZE
                                                                );
                                                            }
                                                            else {
                                                                packedTextureCoordinates =
                                                                    ((
                                                                         mirroredS -
                                                                         (point->v.tc
                                                                                 [TEXTURE_AXIS_S] /
                                                                             2)
                                                                     ) << 16) |
                                                                    (point->v.tc[TEXTURE_AXIS_T] /
                                                                        2);
                                                            }

                                                            gSPModifyVertex(
                                                                gfx++,
                                                                f_FineTilePointSlots
                                                                    [cellRow + pointRow]
                                                                    [cellColumn + pointColumn],
                                                                G_MWO_POINT_ST,
                                                                packedTextureCoordinates
                                                            );
                                                        }
                                                    }
                                                }

                                                if ((terrainStyle != TERRAIN_STYLE_TOP_DOWN_MAP &&
                                                        terrainStyle == TERRAIN_STYLE_MAP) ||
                                                    g_ShowsGroundWaves) {
                                                    if (!g_ShowsGroundWaves) {
                                                        if (terrainStyle != TERRAIN_STYLE_NORMAL) {
                                                            gDPSetPrimColor(
                                                                gfx++,
                                                                0,
                                                                0,
                                                                0xFF,
                                                                0xFF,
                                                                0xFF,
                                                                OPAQUE_ALPHA
                                                            );
                                                        }
                                                    }

                                                    gSP1Triangle(
                                                        gfx++,
                                                        f_FineTilePointSlots[cellRow][cellColumn],
                                                        f_FineTilePointSlots[cellRow]
                                                                            [cellColumn + 1],
                                                        f_FineTilePointSlots[cellRow + 1]
                                                                            [cellColumn],
                                                        0
                                                    );
                                                    slopeIndex++;
                                                    if (!g_ShowsGroundWaves) {
                                                        if (terrainStyle != TERRAIN_STYLE_NORMAL) {
                                                            gDPSetPrimColor(
                                                                gfx++,
                                                                0,
                                                                0,
                                                                SHADED_TRIANGLE_LEVEL,
                                                                SHADED_TRIANGLE_LEVEL,
                                                                SHADED_TRIANGLE_LEVEL,
                                                                OPAQUE_ALPHA
                                                            );
                                                        }
                                                    }

                                                    gSP1Triangle(
                                                        gfx++,
                                                        f_FineTilePointSlots[cellRow + 1]
                                                                            [cellColumn + 1],
                                                        f_FineTilePointSlots[cellRow + 1]
                                                                            [cellColumn],
                                                        f_FineTilePointSlots[cellRow]
                                                                            [cellColumn + 1],
                                                        0
                                                    );
                                                    slopeIndex++;
                                                }
                                                else {
                                                    if (terrainStyle != TERRAIN_STYLE_NORMAL) {
                                                        if ((cellColumn ^ cellRow) & 1) {
                                                            gDPSetEnvColor(
                                                                gfx++,
                                                                0xFF,
                                                                0,
                                                                0,
                                                                OPAQUE_ALPHA
                                                            );
                                                        }
                                                        else {
                                                            gDPSetEnvColor(
                                                                gfx++,
                                                                0,
                                                                0,
                                                                0xFF,
                                                                OPAQUE_ALPHA
                                                            );
                                                        }

                                                        if (isMirrored) {
                                                            gDPSetPrimColor(
                                                                gfx++,
                                                                0,
                                                                0,
                                                                MIRRORED_CELL_LEVEL,
                                                                MIRRORED_CELL_LEVEL,
                                                                MIRRORED_CELL_LEVEL,
                                                                OPAQUE_ALPHA
                                                            );
                                                        }
                                                        else {
                                                            gDPSetPrimColor(
                                                                gfx++,
                                                                0,
                                                                0,
                                                                CELL_LEVEL,
                                                                CELL_LEVEL,
                                                                CELL_LEVEL,
                                                                OPAQUE_ALPHA
                                                            );
                                                        }
                                                    }

                                                    gSP2Triangles(
                                                        gfx++,
                                                        f_FineTilePointSlots[cellRow][cellColumn],
                                                        f_FineTilePointSlots[cellRow]
                                                                            [cellColumn + 1],
                                                        f_FineTilePointSlots[cellRow + 1]
                                                                            [cellColumn],
                                                        0,
                                                        f_FineTilePointSlots[cellRow]
                                                                            [cellColumn + 1],
                                                        f_FineTilePointSlots[cellRow + 1]
                                                                            [cellColumn + 1],
                                                        f_FineTilePointSlots[cellRow + 1]
                                                                            [cellColumn],
                                                        0
                                                    );
                                                    slopeIndex += 2;
                                                }

                                                if (isMirrored) {
                                                    s32 pointColumn;
                                                    s32 pointRow;

                                                    for (pointRow = 0;
                                                        pointRow < FINE_TILE_BLOCK_SPAN;
                                                        pointRow++) {
                                                        for (pointColumn = 0;
                                                            pointColumn < FINE_TILE_BLOCK_SPAN;
                                                            pointColumn++) {
                                                            Vtx *point = GetGroundPoint(
                                                                (half * GROUND_HALF_CELL_COLUMNS) +
                                                                    (groupTile *
                                                                        FINE_TILE_CELL_SPAN) +
                                                                    cellColumn + pointColumn,
                                                                (row * FINE_TILE_CELL_SPAN) +
                                                                    cellRow + pointRow
                                                            );
                                                            s32 packedTextureCoordinates =
                                                                ((point->v.tc[TEXTURE_AXIS_S] / 2)
                                                                    << 16) |
                                                                (point->v.tc[TEXTURE_AXIS_T] / 2);

                                                            if (IsGameFlagSet(
                                                                    GAME_FLAG_TEST_GROUND_TEXTURE
                                                                )) {
                                                                gDPSetTileSize(
                                                                    gfx++,
                                                                    1,
                                                                    0,
                                                                    0,
                                                                    TEST_TILE_SIZE,
                                                                    TEST_TILE_SIZE
                                                                );
                                                            }

                                                            gSPModifyVertex(
                                                                gfx++,
                                                                f_FineTilePointSlots
                                                                    [cellRow + pointRow]
                                                                    [cellColumn + pointColumn],
                                                                G_MWO_POINT_ST,
                                                                packedTextureCoordinates
                                                            );
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                if (g_WorkArea.hole.tiles[tileIndex].extraPassCount <= 0) {
                                    break;
                                }

                                slopeIndex -= TILE_TRIANGLE_COUNT;
                            }
                        }
                    }
                    else if (g_WorkArea.hole.tiles[tileIndex].detail > GROUND_DETAIL_HIDDEN) {
                        gSPVertex(
                            gfx++,
                            GridVertex(HalfFirstColumn(half) + (groupTile * 2), row * 2),
                            2,
                            0
                        );
                        gSPVertex(
                            gfx++,
                            GridVertex(HalfFirstColumn(half) + (groupTile * 2) + 1, row * 2),
                            1,
                            2
                        );
                        gSPVertex(
                            gfx++,
                            GridVertex(HalfFirstColumn(half) + (groupTile * 2), (row * 2) + 1),
                            2,
                            3
                        );
                        gSPVertex(
                            gfx++,
                            GridVertex(HalfFirstColumn(half) + (groupTile * 2) + 1, (row * 2) + 1),
                            1,
                            5
                        );
                        gSPVertex(
                            gfx++,
                            GridVertex(HalfFirstColumn(half) + (groupTile * 2), (row * 2) + 2),
                            2,
                            6
                        );
                        gSPVertex(
                            gfx++,
                            GridVertex(HalfFirstColumn(half) + (groupTile * 2) + 1, (row * 2) + 2),
                            1,
                            8
                        );
                        for (mediumPass = 0; mediumPass < GROUND_TILE_PASSES_MAX; mediumPass++) {
                            LoadEditorTileTexture(
                                tileIndex,
                                firstTileS,
                                groupTile,
                                row,
                                mediumPass,
                                GROUND_OVERLAY_NOISE
                            );
                            gSP2Triangles(gfx++, 0, 2, 3, 0, 2, 5, 3, 0);
                            gSP2Triangles(gfx++, 2, 1, 5, 0, 1, 4, 5, 0);
                            gSP2Triangles(gfx++, 3, 5, 6, 0, 5, 8, 6, 0);
                            gSP2Triangles(gfx++, 5, 4, 8, 0, 4, 7, 8, 0);
                            if (g_WorkArea.hole.tiles[tileIndex].extraPassCount <= 0) {
                                break;
                            }
                        }

                        slopeIndex += TILE_TRIANGLE_COUNT;
                    }
                    else {
                        slopeIndex += TILE_TRIANGLE_COUNT;
                    }

                    tileIndex++;
                }
            }
            else if (groupDetail > GROUND_DETAIL_COARSE) {
                spareSlot = middleSlot;
                middleSlot = topSlot;
                topSlot = bottomSlot;
                bottomSlot = spareSlot;
                gDPPipeSync(gfx++);

                if (previousDetail <= GROUND_DETAIL_HIDDEN ||
                    previousDetail == GROUND_DETAIL_FINE) {
                    gSPVertex(
                        gfx++,
                        GridVertex(HalfFirstColumn(half), row * 2),
                        GROUND_TILE_GROUP_SIZE + 1,
                        topSlot
                    );
                    gSPVertex(
                        gfx++,
                        GridVertex(HalfFirstColumn(half) + 1, row * 2),
                        GROUND_TILE_GROUP_SIZE,
                        topSlot + ODD_SLOT
                    );
                }
                else if (previousDetail == GROUND_DETAIL_COARSE) {
                    LoadEditorAveragedRow(
                        GridVertex(HalfFirstColumn(half), row * 2),
                        topSlot + ODD_SLOT
                    );
                }

                gSPVertex(
                    gfx++,
                    GridVertex(HalfFirstColumn(half), (row * 2) + 1),
                    GROUND_TILE_GROUP_SIZE + 1,
                    middleSlot
                );
                gSPVertex(
                    gfx++,
                    GridVertex(HalfFirstColumn(half) + 1, (row * 2) + 1),
                    GROUND_TILE_GROUP_SIZE,
                    middleSlot + ODD_SLOT
                );
                gSPVertex(
                    gfx++,
                    GridVertex(HalfFirstColumn(half), (row * 2) + 2),
                    GROUND_TILE_GROUP_SIZE + 1,
                    bottomSlot
                );
                if (row != GROUND_TILE_ROW_COUNT - 1 &&
                    g_WorkArea.hole.tiles[tileIndex + GROUND_TILE_COLUMN_COUNT].detail ==
                        GROUND_DETAIL_COARSE) {
                    LoadEditorAveragedRow(
                        GridVertex(HalfFirstColumn(half), (row * 2) + 2),
                        bottomSlot + ODD_SLOT
                    );
                }
                else {
                    gSPVertex(
                        gfx++,
                        GridVertex(HalfFirstColumn(half) + 1, (row * 2) + 2),
                        GROUND_TILE_GROUP_SIZE,
                        bottomSlot + ODD_SLOT
                    );
                }

                for (groupTileIndex = 0; groupTileIndex < GROUND_TILE_GROUP_SIZE;
                    groupTileIndex++) {
                    if (g_WorkArea.hole.tiles[tileIndex].detail > GROUND_DETAIL_HIDDEN) {
                        for (pass = 0; pass < GROUND_TILE_PASSES_MAX; pass++) {
                            LoadEditorTileTexture(
                                tileIndex,
                                firstTileS,
                                groupTileIndex,
                                row,
                                pass,
                                GROUND_OVERLAY_NOISE
                            );
                            DrawTileQuads(
                                topSlot,
                                topSlot + ODD_SLOT,
                                topSlot + 1,
                                middleSlot,
                                middleSlot + ODD_SLOT,
                                middleSlot + 1,
                                bottomSlot,
                                bottomSlot + ODD_SLOT,
                                bottomSlot + 1
                            );
                            if (g_WorkArea.hole.tiles[tileIndex].extraPassCount <= 0) {
                                break;
                            }
                        }
                    }

                    topSlot++;
                    middleSlot++;
                    bottomSlot++;
                    tileIndex++;
                    slopeIndex += TILE_TRIANGLE_COUNT;
                }

                topSlot -= GROUND_TILE_GROUP_SIZE;
                middleSlot -= GROUND_TILE_GROUP_SIZE;
                bottomSlot -= GROUND_TILE_GROUP_SIZE;
            }
            else if (groupDetail >= GROUND_DETAIL_COARSE) {
                spareSlot = middleSlot;
                middleSlot = topSlot;
                topSlot = bottomSlot;
                bottomSlot = spareSlot;
                if (previousDetail <= GROUND_DETAIL_HIDDEN ||
                    previousDetail == GROUND_DETAIL_FINE) {
                    gSPVertex(
                        gfx++,
                        GridVertex(HalfFirstColumn(half), row * 2),
                        GROUND_TILE_GROUP_SIZE + 1,
                        topSlot
                    );
                }

                gSPVertex(
                    gfx++,
                    GridVertex(HalfFirstColumn(half), (row * 2) + 2),
                    GROUND_TILE_GROUP_SIZE + 1,
                    bottomSlot
                );
                for (groupOffset = 0; groupOffset < GROUND_TILE_GROUP_SIZE; groupOffset++) {
                    if (g_WorkArea.hole.tiles[tileIndex].detail > GROUND_DETAIL_HIDDEN) {
                        for (coarsePass = 0; coarsePass < GROUND_TILE_PASSES_MAX; coarsePass++) {
                            LoadEditorTileTexture(
                                tileIndex,
                                firstTileS,
                                groupOffset,
                                row,
                                coarsePass,
                                GROUND_OVERLAY_NOISE
                            );
                            gSP2Triangles(
                                gfx++,
                                topSlot,
                                topSlot + 1,
                                bottomSlot,
                                0,
                                topSlot + 1,
                                bottomSlot + 1,
                                bottomSlot,
                                0
                            );
                            if (g_WorkArea.hole.tiles[tileIndex].extraPassCount <= 0) {
                                break;
                            }
                        }
                    }

                    topSlot++;
                    bottomSlot++;
                    tileIndex++;
                    slopeIndex += TILE_TRIANGLE_COUNT;
                }

                topSlot -= GROUND_TILE_GROUP_SIZE;
                bottomSlot -= GROUND_TILE_GROUP_SIZE;
            }
            else {
                slopeIndex += GROUND_TILE_GROUP_SIZE * TILE_TRIANGLE_COUNT;
                tileIndex += GROUND_TILE_GROUP_SIZE;
            }

            previousDetail = groupDetail;

            tileIndex += GROUND_TILE_GROUP_SIZE;
        }
    }

    g_VertexListHead = vertices;
    gDPPipeSync(gfx++);
    gSPEndDisplayList(gfx++);
    gSPDisplayList(g_DisplayListHead++, f_EditorGroundDisplayLists[g_DrawBufferIndex]);
    if (IsGameFlagSet(GAME_FLAG_TIME_GROUND)) {
        start = osGetCount() - start;
        PrintDebugPaddedHex(
            start / GROUND_TIMING_TICKS,
            GROUND_TIMING_COLUMN,
            f_EditorGroundTimingRow + GROUND_TIMING_ROW
        );
        f_EditorGroundTimingRow = 1 - f_EditorGroundTimingRow;
    }
}

void CaptureMapImage(void) {
    char fileName[CAPTURE_FILE_NAME_SIZE];
    u8 *capture;
    s32 byteIndex;
    s32 row;
    s32 column;

    u16 GetMapTexel(u32 texelIndex) {
        u32 column = texelIndex % MAP_CAPTURE_WIDTH;
        u32 row = texelIndex / MAP_CAPTURE_WIDTH % MAP_CAPTURE_HEIGHT;

        return g_WorkArea.hole
            .tiles
                [column / GROUND_TILE_TEXEL_SPAN +
                    row / GROUND_TILE_TEXEL_SPAN * GROUND_TILE_COLUMN_COUNT]
            .texels[GROUND_TILE_TEXEL_BORDER]
                   [GROUND_TILE_TEXEL_BORDER + column % GROUND_TILE_TEXEL_SPAN +
                       row % GROUND_TILE_TEXEL_SPAN * GROUND_TILE_TEXEL_ROW_LENGTH];
    }

    MarkBillboardsOnMap();
    capture = AllocMemory(MAP_CAPTURE_SIZE);
    for (byteIndex = 0; byteIndex != CAPTURE_HEADER_SIZE; byteIndex++) {
        capture[byteIndex] = f_MapCaptureHeader[byteIndex];
    }

    for (row = MAP_CAPTURE_HEIGHT - 1; row != -1; row--) {
        for (column = 0; column != MAP_CAPTURE_WIDTH; column++) {
            capture[byteIndex++] =
                ((GetMapTexel((row * MAP_CAPTURE_WIDTH) + column) >> RGBA16_BLUE_SHIFT) &
                    RGBA16_CHANNEL_MASK)
                << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] =
                ((GetMapTexel((row * MAP_CAPTURE_WIDTH) + column) >> RGBA16_GREEN_SHIFT) &
                    RGBA16_CHANNEL_MASK)
                << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] =
                ((GetMapTexel((row * MAP_CAPTURE_WIDTH) + column) >> RGBA16_RED_SHIFT) &
                    RGBA16_CHANNEL_MASK)
                << RGBA16_CHANNEL_WIDEN_SHIFT;
        }
    }

    sprintf(fileName, "x:\\usgolf64\\capture\\temp%02d.bmp", (int)f_MapCaptureCount);
    WriteHostFile(fileName, MAP_CAPTURE_SIZE);
    FreeMemory(&capture);
    f_MapCaptureCount++;
}

void CaptureFrame(void) {
    u16 *frame;
    u8 *capture;
    s32 byteIndex;
    s32 row;
    s32 column;

    frame = (u16 *)UncachedAddress(nuGfxCfb_ptr);
    capture = AllocMemory(SCREEN_CAPTURE_SIZE);
    for (byteIndex = 0; byteIndex != CAPTURE_HEADER_SIZE; byteIndex++) {
        capture[byteIndex] = f_ScreenCaptureHeader[byteIndex];
    }

    for (row = SCREEN_HEIGHT - 1; row != -1; row--) {
        for (column = 0; column != SCREEN_WIDTH; column++) {
            capture[byteIndex++] =
                ((frame[(row * SCREEN_WIDTH) + column] >> RGBA16_BLUE_SHIFT) & RGBA16_CHANNEL_MASK)
                << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] =
                ((frame[(row * SCREEN_WIDTH) + column] >> RGBA16_GREEN_SHIFT) & RGBA16_CHANNEL_MASK)
                << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] =
                ((frame[(row * SCREEN_WIDTH) + column] >> RGBA16_RED_SHIFT) & RGBA16_CHANNEL_MASK)
                << RGBA16_CHANNEL_WIDEN_SHIFT;
        }
    }

    sprintf(g_PrintBuffer, "x:/usgolf64/capture/cap%02d.bmp", (int)f_ScreenCaptureCount);
    WriteHostFile(g_PrintBuffer, SCREEN_CAPTURE_SIZE);
    FreeMemory(&capture);
    f_ScreenCaptureCount++;
}

void CaptureFrameCorner(s32 captureIndex) {
    u16 *frame;
    u8 *capture;
    s32 byteIndex;
    s32 row;
    s32 column;

    capture = g_WorkArea.bytes;
    frame = (u16 *)UncachedAddress(nuGfxCfb_ptr);
    for (byteIndex = 0; byteIndex != CAPTURE_HEADER_SIZE; byteIndex++) {
        capture[byteIndex] = f_CornerCaptureHeader[byteIndex];
    }

    for (row = CORNER_CAPTURE_HEIGHT - 1; row != -1; row--) {
        for (column = 0; column != CORNER_CAPTURE_WIDTH; column++) {
            capture[byteIndex++] =
                ((frame[(row * SCREEN_WIDTH) + column] >> RGBA16_BLUE_SHIFT) & RGBA16_CHANNEL_MASK)
                << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] =
                ((frame[(row * SCREEN_WIDTH) + column] >> RGBA16_GREEN_SHIFT) & RGBA16_CHANNEL_MASK)
                << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] =
                ((frame[(row * SCREEN_WIDTH) + column] >> RGBA16_RED_SHIFT) & RGBA16_CHANNEL_MASK)
                << RGBA16_CHANNEL_WIDEN_SHIFT;
        }
    }

    sprintf(g_PrintBuffer, "c:/golf/capture/temp%02d.bmp", (int)captureIndex);
    WriteHostFile(g_PrintBuffer, SCREEN_CAPTURE_SIZE);
}

void DrawTargetPole(CameraMatrices *matrices UNUSED, Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    guTranslate(
        &f_PoleMatrix,
        g_MapCamera.view.lookAt[AXIS_X],
        g_MapCamera.view.lookAt[AXIS_Y],
        g_MapCamera.view.lookAt[AXIS_Z]
    );
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    gSPMatrix(gfx++, PhysicalAddress(&f_PoleMatrix), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(gfx++, f_PoleVertices[0], QUAD_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 1, 3, 2, 0);
    gSPVertex(gfx++, f_PoleVertices[1], QUAD_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 1, 3, 2, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
