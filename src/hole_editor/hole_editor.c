/*
 * The hole editor's scenes, which edit a hole's ground cells, points, heights,
 * triangles, rings and billboards over its map. They also load and save holes
 * and write captures of the frame to the host.
 */
#include "hole_editor.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>

#include "address.h"
#include "billboard.h"
#include "camera.h"
#include "compiler.h"
#include "controller.h"
#include "course.h"
#include "course_space.h"
#include "cpu_projection.h"
#include "debug_text.h"
#include "game.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "heap.h"
#include "hole_builder.h"
#include "hole_loader.h"
#include "hole_triangles.h"
#include "host_file.h"
#include "include_asm.h"
#include "language.h"
#include "main.h"
#include "math_util.h"
#include "play.h"
#include "player.h"
#include "scene.h"
#include "screen.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "text.h"
#include "work_area.h"

/* The points along a tile's edge, its corners included */
#define EDGE_POINT_COUNT (GROUND_TILE_POINT_SPAN + 1)
/* Marks a grid vertex in f_TileMidpointSlots, numbered column by column */
#define GRID_VERTEX_SLOT_FLAG 0x80
#define GridVertexSlot(index) (GRID_VERTEX_SLOT_FLAG | (index))
/* UpdateTriangleSlopes goes over the ground in two halves of 8 by 16 blocks */
#define SLOPE_HALF_COUNT 2
#define SLOPE_BLOCK_COLUMN_COUNT 8
#define SLOPE_BLOCK_ROW_COUNT 16
/* The triangles' starting corners; every corner is overwritten before it is
 * read */
#define START_TRIANGLE_SIZE 256
#define SLOPE_SHADE_OFFSET 0x40
#define SLOPE_SHADE_MAX 0xFF

/* ShapeGroundFromCodes's ground shaping searches this many nearest offsets */
#define SHAPE_OFFSET_COUNT 1600
#define SHAPE_OFFSET_SPAN 20
#define HOLE_EDITOR_SHAPE_ASSERT_LINE 2191
#define GREEN_NOISE_MASK 3
#define GROUND_NOISE_MASK 0xF
/* A point's y changes by this per course unit between its code and the green's
 * middle */
#define HEIGHT_PER_COURSE_UNIT (-0.00000390625f)
/* Bunker codes with no other code nearer than this get BUNKER_INNER_OFFSET
 * added to y */
#define BUNKER_INNER_DISTANCE 2
#define BUNKER_INNER_OFFSET 45
/* Rough codes take ROUGH_EDGE_OFFSET off y, or more with no other code nearer
 * than this */
#define ROUGH_INNER_DISTANCE 19
#define ROUGH_EDGE_OFFSET 22
#define ROUGH_INNER_OFFSET 120
#define ROUGH_INNER_NOISE_MASK 0x1F
#define SHAPED_POINT_SHADE 0xFF
/* Trees go in rough this many code cells from its edge, at least TREE_SPACING
 * units apart */
#define TREE_MIN_DISTANCE 8
#define TREE_MAX_DISTANCE 10
#define TREE_SPACING 0x100
#define TREE_TEXTURE_INDEX 2
/* PlaceBillboards sets isNearCourseEdge and y again */
#define TREE_NEAR_EDGE 0x100
#define TREE_WIDTH 0x80
#define TREE_HEIGHT 0x100

/* BlendGroundAroundPoint blends heights over distances of this many units per
 * falloff step */
#define BLEND_DISTANCE_SCALE 512.0f
#define BLENDED_POINT_SHADE 0xFF

/* HasDeadTriangleNearPoint rejects cell triangles whose cross product is below
 * this */
#define MIN_CELL_CROSS_PRODUCT 16

/* SlopeOutOfBoundsGround's out-of-bounds slope searches this many nearest
 * offsets */
#define SLOPE_OFFSET_COUNT 400
#define SLOPE_OFFSET_SPAN 10
/* The distances, in grid steps, where the slope starts and turns to noise */
#define SLOPE_START_DISTANCE 5
#define SLOPE_NOISE_DISTANCE 6
#define SLOPE_EXTRA_NOISE_DISTANCE 12
/* The slope's line meets zero at this distance */
#define SLOPE_BASE_DISTANCE 2
#define SLOPE_DROP_PER_DISTANCE (30.0f / OFFSET_DISTANCE_SCALE)
#define SLOPE_NOISE_MASK 0x3FFF
#define SLOPE_NOISE_SCALE 0.001f
#define SLOPE_FAR_DROP 60.0f
#define SLOPE_EXTRA_DROP 15.0f

/* UpdateHeightEditor's ground height editor */
#define EDITOR_PAD 0
#define OPTION_PAD 1
#define SECOND_EDITOR_PAD 2
/* Edited points are lit fully */
#define EDITED_POINT_SHADE 0xFF
#define HEIGHT_STEP 4
/* C-left sets the last height plus or minus up to 0x20 */
#define COARSE_NOISE_MASK 0x3F
#define COARSE_NOISE_OFFSET 0x20
/* A moves the point's height by up to 0x40 either way */
#define ROUGH_NOISE_MASK 0x7F
#define ROUGH_NOISE_OFFSET 0x40
/* B moves it HEIGHT_STEP down or up */
#define STEP_NOISE_BIT (HEIGHT_STEP * 2)
/* The second editor pad's B lowers it by 0 or 0x10 */
#define DIP_NOISE_BIT 0x10
/* What a frame did, kept in f_LastHeightEditKind */
#define HEIGHT_EDIT_NONE 0
#define HEIGHT_EDIT_POINT 1
#define HEIGHT_EDIT_MOVE 2
/* The cursor's moves, in eighths of a turn from the camera's heading */
#define CURSOR_DIRECTION_NONE 0
#define CURSOR_DIRECTION_RIGHT 2
#define CURSOR_DIRECTION_UP_RIGHT 3
#define CURSOR_DIRECTION_UP 4
#define CURSOR_DIRECTION_UP_LEFT 5
#define CURSOR_DIRECTION_LEFT 6
#define CURSOR_DIRECTION_DOWN_LEFT 7
#define CURSOR_DIRECTION_DOWN 8
#define CURSOR_DIRECTION_DOWN_RIGHT 9
#define BINARY_ANGLE_EIGHTH_TURN (BINARY_ANGLE_TURN / 8)
/* BINARY_ANGLE_TO_RADIANS with pi as 3.1416, in double precision */
#define EDITOR_ANGLE_TO_RADIANS (3.1416 / 32768)
/* Truncated, a step moves the cursor one point along each axis it leans to */
#define CURSOR_STEP_LENGTH 1.5
#define EDITOR_ZOOM_MAX 4.0f
/* The stick turns the camera in steps of 16 of its units */
#define STICK_STEP_SIZE 16
#define HEADING_STICK_SPEED 0x100
#define PITCH_STICK_SPEED (1.0f / 180)
#define EDITOR_PITCH_MIN (-80 * 3.1416 / 180)
#define EDITOR_CAMERA_DISTANCE 2000.0
/* The cameras start an eighth of a turn down and around */
#define EDITOR_CAMERA_START_ANGLE (3.1416f / 4)
#define METERS_PER_HEIGHT_UNIT (1.0f / 15)
#define HEIGHT_TEXT_COLUMN 25
#define HEIGHT_TEXT_ROW 3

/* UpdateTriangleEditor's triangle editor */
/* What a frame did, kept in f_LastTriangleEditKind */
enum TriangleEdit {
    TRIANGLE_EDIT_NONE,
    TRIANGLE_EDIT_MOVE,
    TRIANGLE_EDIT_SELECT,
    TRIANGLE_EDIT_KIND = 4
};

/* The last RING_COUNT triangles hold the rings, which UpdateRingEditor
 * edits */
#define FIRST_RING_TRIANGLE (HOLE_TRIANGLE_COUNT - RING_COUNT)
#define HOLE_TRIANGLE_KIND_COUNT 4
/* A step moves the corner this far at zoom 1 */
#define CORNER_STEP_SCALE 10.0f
#define SAVED_CORNER_COUNT 10
#define TRIANGLE_TEXT_COLUMN 10
#define EDIT_KIND_TEXT_ROW 10
#define CORNER_HEIGHT_TEXT_ROW 11

/* UpdateRingEditor's ring editor; its corners hold a ring's position,
 * direction and scale */
#define RING_DIRECTION_CORNER 1
/* In the corner's x, 1024 for the ring's own size */
#define RING_SCALE_CORNER 2
#define RING_SCALE_STEP 16
#define RING_SCALE_MIN 16
#define RING_SCALE_MAX 0x2000
#define RING_DIRECTION_LENGTH 8192.0f
#define RING_TEXT_COLUMN 19
#define RING_TEXT_ROW 4

/* UpdateWallEditor's wall editor */
#define WALL_TRIANGLE_COUNT 2
/* Corners 0 and 1; the others follow them */
#define WALL_BOTTOM_CORNER_COUNT 2
#define WALL_TOP_CORNER 2
#define WALL_HEIGHT 24.0f
/* How far a dropped wall reaches below the ground */
#define WALL_SINK_DEPTH (12 << WORLD_TO_COURSE_SHIFT)
/* Also the ball's flagstick hit */
#define WALL_LIMIT_SOUND 0xE
#define WALL_LIMIT_SOUND_CHANNEL 11
#define WALL_TEXT_COLUMN 5
#define WALL_TEXT_ROW 3

/* UpdateTreeEditor's tree editor */
#define TREE_TEXT_COLUMN 19
#define TREE_KIND_LABEL_ROW 24
#define TREE_KIND_TEXT_ROW 25
#define TREE_INDEX_LABEL_ROW 26
#define TREE_INDEX_TEXT_ROW 27
#define TREE_SIZE_MAX 0x800
/* A billboard from 1 to these limits in x and z is taken */
#define TREE_X_LIMIT 0x1000
#define TREE_Z_LIMIT 0x2000
/* Whether a billboard is free, outside the limits that taken ones are in */
#define IsFreeBillboard(billboard) \
    ((billboard).x <= 0 || \
        (billboard).x >= TREE_X_LIMIT || \
        (billboard).z <= 0 || \
        (billboard).z >= TREE_Z_LIMIT)
/* Along x */
#define COPIED_TREE_OFFSET 0x100

/* WriteHexDumpFile's hex dump */
#define HEX_DUMP_BUFFER_SIZE 0x40000
#define HEX_DUMP_TEXT_SIZE 0x100
#define HEX_DUMP_LINE_WORDS 4
#define HEX_DUMP_LINE_BYTES (HEX_DUMP_LINE_WORDS * (s32)sizeof(u32))
#define HOLE_EDITOR_HEX_DUMP_ASSERT_LINE 3027

/* UpdateHoleFileMenu's steps, in f_HoleFileStep */
#define HOLE_FILE_STEP_ACTION 0
#define HOLE_FILE_STEP_COURSE 1
#define HOLE_FILE_STEP_HOLE 2
#define HOLE_FILE_STEP_CONFIRM 3
/* The prompt shows highlighted while this bit of its frame count is set */
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

/* The ground cells, between neighboring ground points */
#define CELL_COLUMN_COUNT (GROUND_POINT_COLUMN_COUNT - 1)
#define CELL_ROW_COUNT (GROUND_POINT_ROW_COUNT - 1)

/* UpdateGroundPointEditor's ground point editor */
/* One for each C-button */
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

/* UpdateHolePointEditor's hole point editor */
#define HOLE_POINT_MODE_UNSET (-1)
#define HOLE_POINT_MODE_PIN_TEE 0
#define HOLE_POINT_MODE_MESON 1
#define HOLE_POINT_MODE_ROUTE 2
#define HOLE_POINT_MODE_COUNT 3
#define HOLE_POINT_STEP_LENGTH 10.0
#define HEADING_RADIANS ((u16)f_HolePointCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS)
#define CURSOR_TEXT_COLUMN 3
#define CURSOR_TEXT_ROW 5
#define SURFACE_TEXT_COLUMN 2
#define SURFACE_TEXT_ROW 4
#define HOLE_POINT_TEXT_COLUMN 9
#define HOLE_POINT_MODE_TEXT_ROW 6
#define HOLE_POINT_INDEX_TEXT_ROW 7
/* How SCENE_CELL_EDITOR's cursor zooms the map camera each frame R or L is held
 */
#define CURSOR_ZOOM_IN_FACTOR 1.1f
#define CURSOR_ZOOM_OUT_FACTOR 0.9f
#define CURSOR_ZOOM_MAX 10.0f
#define CURSOR_ZOOM_MIN 0.1f

/* ShouldMirrorCell gathers the terrain codes of cells at least this far from a
 * diagonal */
#define DIAGONAL_MIN_DISTANCE 3.0f
/* The sides of the two diagonals that ShouldMirrorCell gathers codes for */
#define DIAGONAL_SIDE_COUNT 4
/* How many different codes each side keeps */
#define SIDE_CODE_COUNT 2
/* Whether a diagonal side has gathered two different terrain codes */
#define HasTwoSideCodes(codes) ((codes)[SIDE_CODE_COUNT - 1] != NO_TERRAIN_CODE)
#define NO_TERRAIN_CODE (-1)
#define HOLE_EDITOR_DIAGONAL_ASSERT_LINE 2046
/* The vertex units in a terrain code column or row */
#define VERTEX_UNITS_PER_CODE 16

/* The screen captures, 24-bit BMPs; the corner capture covers the frame's
 * top-left corner */
#define CAPTURE_HEADER_SIZE 54
#define CORNER_CAPTURE_WIDTH 64
#define CORNER_CAPTURE_HEIGHT 128
#define CAPTURE_BYTES_PER_PIXEL 3
#define CORNER_CAPTURE_SIZE \
    (CAPTURE_HEADER_SIZE + (CORNER_CAPTURE_WIDTH * CORNER_CAPTURE_HEIGHT * CAPTURE_BYTES_PER_PIXEL))
/* A BMP header's fields are little-endian */
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
/* The size of a whole-screen capture */
/* The map capture covers every ground tile's texels, inside their borders */
#define MAP_CAPTURE_WIDTH (GROUND_TILE_COLUMN_COUNT * GROUND_TILE_TEXEL_SPAN)
#define MAP_CAPTURE_HEIGHT (GROUND_TILE_ROW_COUNT * GROUND_TILE_TEXEL_SPAN)
#define MAP_CAPTURE_SIZE \
    (CAPTURE_HEADER_SIZE + (MAP_CAPTURE_WIDTH * MAP_CAPTURE_HEIGHT * CAPTURE_BYTES_PER_PIXEL))
#define GROUND_TILE_TEXEL_BORDER 1
#define CAPTURE_FILE_NAME_SIZE 0x80
#define SCREEN_CAPTURE_SIZE \
    (CAPTURE_HEADER_SIZE + (SCREEN_WIDTH * SCREEN_HEIGHT * CAPTURE_BYTES_PER_PIXEL))
/* Adds to a cached KSEG0 address to read the same memory uncached through KSEG1
 */
#define KSEG1_OFFSET 0x20000000
/* The same memory read uncached, to see what the RDP wrote */
#define UncachedAddress(address) AddressPast((address), KSEG1_OFFSET)

/* A plain if leaves GCC to invert the condition, which the game's code does not
 */
#define AssertInEditor(condition, lineNumber) \
    ((condition) ? (void)0 : \
                   ReportFailedAssertion(f_EditorAssertionText, f_EditorFileName, lineNumber))

/* DrawTargetPole's pole: two crossed quads, one in the x-y plane and
 * one in the z-y plane */
#define POLE_QUAD_COUNT 2

/* FillWaveTexels's 32x32 IA8 wave pattern */
#define WAVE_TEXTURE_WIDTH 32
#define WAVE_TEXTURE_HEIGHT 32
/* A texel step's angle, pi/16, times the texel steps between samples */
#define WAVE_ANGLE_STEP 0.19635
#define WAVE_TEXEL_STEP 4
#define WAVE_AMPLITUDE 256.0
/* Cubing the amplitude-256 wave gives 24 bits; the top four are the level */
#define WAVE_LEVEL_SHIFT 20
#define WAVE_LEVEL_MIN 1
#define WAVE_LEVEL_MAX 15
#define IA8_ALPHA_SHIFT 4

/* DrawHoleEditorGround's display list, one for each draw buffer */
#define EDITOR_GROUND_LIST_LENGTH 15000

/* An I8 texture as large as DrawGround's noise, all white */
#define WHITE_TEXTURE_SIZE 32
/* Every bit set, so each texel is white */
#define WHITE_TEXEL (-1)
/* DrawGround's clip ratio text buffer is as large */
#define CLIP_RATIO_TEXT_SIZE 0x98

/* Where the wave texture goes in TMEM, past a tile's texels */
#define WAVE_TMEM 0x180
/* A half's wave tile starts at its first triangle's scroll over this */
#define WAVE_SCROLL_DIVISOR 32
#define WAVE_TILE_SPAN 0x40

/* The maps' blend colors' alphas, and that of a tile's second pass */
#define MAP_BLEND_ALPHA 0xFE
#define SECOND_PASS_BLEND_ALPHA 0xF7

/*
 * A fine tile is 4 by 4 cells of two triangles each, drawn in blocks of 2 by
 * 2 cells. When the editor builds a tile, 17 of its 5 by 5 points go in the
 * vertex list and 8 come from its midpoints, and f_FineTilePointSlots gives
 * each point's vertex cache slot.
 */
#define FINE_TILE_CELL_SPAN 4
#define FINE_TILE_BLOCK_SPAN 2
#define FINE_TILE_POINT_SPAN (FINE_TILE_CELL_SPAN + 1)
#define FINE_TILE_BUILT_VERTEX_COUNT 17
#define TILE_TRIANGLE_COUNT (FINE_TILE_CELL_SPAN * FINE_TILE_CELL_SPAN * 2)

/*
 * A borderless tile's texture starts a texel into its place in the tile size
 * coordinates, and ends as far past the start of the next tile's place.
 */
#define BORDERLESS_TILE_FIRST_COORDINATE 4
/* GAME_FLAG_TEST_GROUND_TEXTURE's fixed tile, 34 texels square */
#define TEST_TILE_SIZE 0x88

/*
 * A mirrored cell's points have their S reflected about a line, in the
 * halved texture coordinates that its vertex changes write
 */
#define MIRROR_S_OFFSET 0x100
#define MIRROR_S_SCALE 16
#define MIRRORED_TEST_S 0x2100
#define MIRRORED_TEST_FIRST_S 0x88
#define MIRRORED_TEST_LAST_S 0x8A

/* The maps' cell and triangle shades */
#define SHADED_TRIANGLE_LEVEL 0x96
#define CELL_LEVEL 0xDC
#define MIRRORED_CELL_LEVEL 0xA0

/* SetEditorCombineMode's pass of a tile drawn once or twice */
enum TilePass {
    TILE_PASS_ONLY,
    TILE_PASS_FIRST,
    TILE_PASS_SECOND
};

/* What SetEditorCombineMode blends over a tile; g_ShowsGroundWaves picks
 * the noise or waves */
enum GroundOverlay {
    GROUND_OVERLAY_NOISE,
    GROUND_OVERLAY_WAVES,
    GROUND_OVERLAY_NONE
};

/* The combiner mode that DrawHoleEditorGround last set */
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

/* Whether ToggleGroundTileBorders has given a tile its bordered layout */
#define HasTileBorders(tileIndex) \
    (g_WorkArea.hole.tiles[tileIndex] \
            .texels[GROUND_TILE_TEXEL_ROW_COUNT - 1][GROUND_TILE_TEXEL_ROW_LENGTH - 1] != 0)

/* Whether a fine tile's neighbor makes it snap the vertices of their shared
 * edge; unlike DrawGround's test, a hidden neighbor counts */
#define IsNotFine(tileIndex) (g_WorkArea.hole.tiles[tileIndex].detail < GROUND_DETAIL_FINE)

/* Loads the wave texture where DrawGround loads its noise texture */
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

/* Loads the white texture where DrawGround loads its noise texture */
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

/*
 * Loads a tile's texture in either layout. A bordered tile picks its combiner
 * mode for the pass, and its second pass reuses the loaded texels. The tile is
 * the groupColumn-th from the one whose S starts at firstS.
 */
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

/* Loads a row's odd vertices as midpoints of its even ones, so that they line
 * up with a coarse row */
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

/*
 * Each point of a tile's EDGE_POINT_COUNT by EDGE_POINT_COUNT grid's slot among
 * the tile's midpoints. Entries from GRID_VERTEX_SLOT_FLAG up are its grid
 * vertices, which have no midpoint slot.
 */
static u8 f_TileMidpointSlots[EDGE_POINT_COUNT][EDGE_POINT_COUNT] = {
    {GridVertexSlot(0), 0, GridVertexSlot(4), 1, GridVertexSlot(8)},
    {2, 8, 9, 10, 3},
    {GridVertexSlot(1), 11, GridVertexSlot(5), 12, GridVertexSlot(9)},
    {4, 13, 14, 15, 5},
    {GridVertexSlot(2), 6, GridVertexSlot(6), 7, GridVertexSlot(10)},
};
/* UpdateHolePointEditor's cursor */
static s32 f_HolePointCursor[2] = {0, 0}; /* x, z */
/* Read by no routine */
static s32 f_UnusedHolePointValue UNUSED = -1;
/* The cell that SCENE_CELL_EDITOR's cursor is on */
static s32 f_CellCursorColumn = 0;
static s32 f_CellCursorRow = 0;
/* The ground point that SCENE_POINT_EDITOR's cursor is on, starting at
 * SCENE_CELL_EDITOR's cell */
static s32 f_PointCursorColumn = 0;
static s32 f_PointCursorRow = 0;
/* UpdateGroundPointEditor keeps these between POINT_EDITOR_SETTING_MIN and
 * POINT_EDITOR_SETTING_MAX, and nothing else uses them */
static s32 f_PointEditorFirstSetting = 0;
static s32 f_PointEditorSecondSetting = 0;
/* UpdateHolePointEditor's state */
static Radians f_HolePointCameraPitch = -EDITOR_CAMERA_START_ANGLE;
static Radians f_HolePointCameraHeading = EDITOR_CAMERA_START_ANGLE;
/* Read by no routine */
static s32 f_UnusedHolePointAngle UNUSED = BINARY_ANGLE_EIGHTH_TURN;
static s32 f_HolePointCameraHeadingAngle = 0;
static s32 f_HolePointEditMode = -1;
static s32 f_PinTeeIndex = 0;
static s32 f_MesonPointIndex = 0;
static s32 f_RoutePointIndex = 0;

/* UpdateTriangleEditor's state */
static Radians f_TriangleCameraPitch = -EDITOR_CAMERA_START_ANGLE;
/* In radians, from f_TriangleCameraHeadingAngle */
static Radians f_TriangleCameraHeading = EDITOR_CAMERA_START_ANGLE;
/* Read by no routine */
static s32 f_UnusedTriangleAngle UNUSED = BINARY_ANGLE_EIGHTH_TURN;
static s32 f_TriangleCameraHeadingAngle = 0;
static s32 f_EditedTriangleIndex = 0;
static s32 f_EditedCornerIndex = 0;
static s32 f_SavedCornerIndex = 0;
static s32 f_LastTriangleEditKind = 0; /* A TRIANGLE_EDIT_* */

/* UpdateRingEditor's state */
static Radians f_RingCameraPitch = -EDITOR_CAMERA_START_ANGLE;
/* In radians, from f_RingCameraHeadingAngle */
static Radians f_RingCameraHeading = EDITOR_CAMERA_START_ANGLE;
/* Read by no routine */
static s32 f_UnusedRingAngle UNUSED = BINARY_ANGLE_EIGHTH_TURN;
static s32 f_RingCameraHeadingAngle = 0;
static s32 f_RingTriangleIndex = 0x9D;
static s32 f_RingCornerIndex = 0; /* Always 0 */
/* Read by no routine */
static s32 f_UnusedRingIndex UNUSED = 0;
static s32 f_LastRingEditKind = 0; /* A TRIANGLE_EDIT_* */

/* UpdateWallEditor's state */
static Radians f_WallCameraPitch = -EDITOR_CAMERA_START_ANGLE;
/* In radians, from f_WallCameraHeadingAngle */
static Radians f_WallCameraHeading = EDITOR_CAMERA_START_ANGLE;
/* Read by no routine */
static s32 f_UnusedWallAngle UNUSED = BINARY_ANGLE_EIGHTH_TURN;
static s32 f_WallCameraHeadingAngle = 0;
static s32 f_FirstWallTriangle = 0x19;
static s32 f_WallTriangleIndex = 0x19; /* The first of the wall's two triangles */
static s32 f_WallCornerIndex = 0;
static s32 f_SavedWallCornerIndex = 0;
static s32 f_LastWallEditKind = 0; /* A TRIANGLE_EDIT_* */

void ToggleCellMirroring(s32 column, s32 row);

/* A ground triangle's slope, from UpdateTriangleSlopes */
typedef struct TriangleSlope {
    /* 0x0 */ s16 waveS; /* The wave texture's scroll, which moves by the
                           normal each frame */
    /* 0x2 */ s16 waveT;
    /* 0x4 */ s16 normalX; /* Twice the normal's x, of a normal 256 long */
    /* 0x6 */ s16 normalZ;
    /* 0x8 */ u8 shade; /* 0 when flat, from SLOPE_SHADE_OFFSET up for slopes */
} TriangleSlope; /* size = 0xA */

/* The size of the buffer that GetEditorBuffer returns */
#define EDITOR_BUFFER_SIZE 0x40000

/*
 * The editor's bss, in the original's address order: KMC allocates a file's
 * uninitialized statics in the order they are defined.
 */
static CoursePoint f_SavedCorners[SAVED_CORNER_COUNT];
/* Read by no routine */
static CoursePoint f_UnusedSavedCorners[SAVED_CORNER_COUNT] UNUSED;
static CoursePoint f_SavedWallCorners[SAVED_CORNER_COUNT];
/* Places DrawTargetPole's pole at the map camera's target */
static Mtx f_PoleMatrix;
/* Read by no routine */
static s32 f_UnusedEditorWords[2] UNUSED;
static u8 f_WhiteNoiseTexels[WHITE_TEXTURE_SIZE * WHITE_TEXTURE_SIZE];
static TriangleSlope f_TriangleSlopes[CELL_COLUMN_COUNT * CELL_ROW_COUNT * 2];
static Gfx f_EditorGroundDisplayLists[DRAW_BUFFER_COUNT][EDITOR_GROUND_LIST_LENGTH];
/* Reached only through GetEditorBuffer, which nothing calls */
static u8 f_EditorBuffer[EDITOR_BUFFER_SIZE];
/* FillWaveTexels's wave pattern */
static u8 f_WaveTexels[WAVE_TEXTURE_HEIGHT * WAVE_TEXTURE_WIDTH];

/* UpdateHeightEditor's state */
static s32 f_HeightCursorColumn = 0;
static s32 f_HeightCursorRow = 0;
/* Read by no routine */
static s32 f_UnusedHeightSettings[] UNUSED = {6, 0};
static Radians f_HeightCameraPitch = -EDITOR_CAMERA_START_ANGLE;
/* In radians, from f_HeightCameraHeadingAngle */
static Radians f_HeightCameraHeading = EDITOR_CAMERA_START_ANGLE;
/* Read by no routine */
static s32 f_UnusedHeightAngle UNUSED = BINARY_ANGLE_EIGHTH_TURN;
static s32 f_HeightCameraHeadingAngle = 0;
static s32 f_LastSetHeight = 0; /* The height C-up or C-down last set */
static Boolean f_ShowsGroundWaves = TRUE; /* Copied to g_ShowsGroundWaves */
static s32 f_CLeftPickedHeight = 0; /* Heights picked up by the second editor pad */
static s32 f_CUpPickedHeight = 0;
static s32 f_LastHeightEditKind = 0; /* The last frame's HEIGHT_EDIT_* */

/*
 * The original declares this without a prototype, so BlendGroundAroundPoint's
 * float height goes as a double, in the registers of both the height and the
 * shade: the routine takes the double's high word as the height and its low
 * word as the shade, and BLENDED_POINT_SHADE is lost. AVOID_UB gives it its
 * prototype, so that the height arrives as an s32.
 */
#ifdef AVOID_UB
NO_RESULT SetGroundPointHeightAndShade(s32 column, s32 row, s32 height, s32 shade);
#else
NO_RESULT SetGroundPointHeightAndShade();
#endif

/* UpdateHoleFileMenu's menu */
/* Read by no routine */
static s32 f_UnusedHoleFileSetting UNUSED = 1;
static Boolean f_IsSavingHoleFiles = FALSE;
static s32 f_HoleFileStep = 0; /* A HOLE_FILE_STEP_* */
static s32 f_HoleFileHoleNumber = 1; /* From 1 */
static s32 f_HoleFileCourseNumber = 1; /* From 1 */
static s32 f_HoleFilePromptFrames = 0;

/* UpdateTreeEditor's state */
static Radians f_TreeCameraPitch = -EDITOR_CAMERA_START_ANGLE;
/* In radians, from f_TreeCameraHeadingAngle */
static Radians f_TreeCameraHeading = EDITOR_CAMERA_START_ANGLE;
/* Read by no routine */
static s32 f_UnusedTreeAngle UNUSED = BINARY_ANGLE_EIGHTH_TURN;
static s32 f_TreeCameraHeadingAngle = 0;
static s32 f_EditedTreeIndex = 0;
static s32 f_LastTreeEditKind = 0; /* A TRIANGLE_EDIT_*; nothing reads it */

static s32 f_EditorClipRatio = 1;
/* Read by no routine */
static s32 f_UnusedClipRatioValue UNUSED = 0;
static s8 f_FineTilePointSlots[FINE_TILE_POINT_SPAN][FINE_TILE_POINT_SPAN] = {
    {0, 5, 7, 10, 20},
    {1, 12, 13, 14, 21},
    {2, 15, 8, 16, 22},
    {3, 17, 18, 19, 23},
    {4, 6, 9, 11, 24},
};
static s32 f_EditorGroundTimingRow = 0;

static s32 f_MapCaptureNumber = 0;
/* The map's header gives its own width and height but a screen capture's
 * sizes */
static u8 f_MapCaptureHeader[CAPTURE_HEADER_SIZE] = BMP_HEADER(
    SCREEN_CAPTURE_SIZE,
    MAP_CAPTURE_WIDTH,
    MAP_CAPTURE_HEIGHT,
    SCREEN_CAPTURE_SIZE - CAPTURE_HEADER_SIZE
);
static s32 f_ScreenCaptureNumber = 0;
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

/*
 * Runs SCENE_CELL_EDITOR's cell cursor: the D-pad moves it over the ground
 * cells, R and L zoom the map camera, A toggles the cell's bit and Z hands the
 * cell to SCENE_POINT_EDITOR. The map camera's eye stays over the cell's
 * middle.
 */
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
    g_MapCamera.view.eye[0] = (corner->v.ob[AXIS_X] + oppositeCorner->v.ob[AXIS_X]) / 2;
    g_MapCamera.view.eye[2] = (corner->v.ob[AXIS_Z] + oppositeCorner->v.ob[AXIS_Z]) / 2;
}

s32 HasDeadTriangleNearPoint(s32 column, s32 row);

/*
 * SCENE_POINT_EDITOR's ground point editor. The control pad moves the cursor
 * over the ground points, and the C-buttons move the point and its texture
 * coordinates within its tile; a point on a tile's edge stays on that edge. A
 * move that would make a dead triangle is undone. R and L zoom the map camera,
 * and Z hands the point back to SCENE_CELL_EDITOR.
 */
void UpdateGroundPointEditor(void) {
    Vtx_t points[POINT_MOVE_COUNT + 1]; /* Each move saves its point into the next move's slot */
    char text[DEBUG_TEXT_BUFFER_SIZE];
    /* The game's frame has room for another point; without it 16 words differ */
    Vtx_t unusedPoint UNUSED;
    s32 madeDeadTriangle;

    madeDeadTriangle = FALSE;
    if (g_MenuControllerData[EDITOR_PAD].button & R_JPAD) {
        f_PointCursorColumn++;
    }

    if (g_MenuControllerData[EDITOR_PAD].button & L_JPAD) {
        f_PointCursorColumn--;
    }

    if (g_MenuControllerData[EDITOR_PAD].button & D_JPAD) {
        f_PointCursorRow--;
    }

    if (g_MenuControllerData[EDITOR_PAD].button & U_JPAD) {
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

    /* B held moves only the texture */
    if (!(g_MenuControllerData[EDITOR_PAD].button & B_BUTTON)) {
        if ((g_MenuControllerData[EDITOR_PAD].button & D_CBUTTONS) &&
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

        if ((g_MenuControllerData[EDITOR_PAD].button & U_CBUTTONS) &&
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

        if ((g_MenuControllerData[EDITOR_PAD].button & L_CBUTTONS) &&
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

        if ((g_MenuControllerData[EDITOR_PAD].button & R_CBUTTONS) &&
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
        if ((g_MenuControllerData[EDITOR_PAD].button & D_CBUTTONS) &&
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

        if ((g_MenuControllerData[EDITOR_PAD].button & U_CBUTTONS) &&
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

        if ((g_MenuControllerData[EDITOR_PAD].button & L_CBUTTONS) &&
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

        if ((g_MenuControllerData[EDITOR_PAD].button & R_CBUTTONS) &&
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

    if (g_ControllerData[EDITOR_PAD].button & R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= CURSOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MAX;
        }
    }

    if (g_ControllerData[EDITOR_PAD].button & L_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_MenuControllerData[EDITOR_PAD].trigger & Z_TRIG) {
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

/*
 * Edits the hole's points over the map: the pins and tees, the "Meson" points
 * and the computer golfer's route. The C-buttons pick the list and the point,
 * the stick and control pad move the cursor, and A places the point on the
 * ground under the cursor (B places the tee). The first pin and tee also become
 * the hole's own.
 */
void UpdateHolePointEditor(void) {
    char text[DEBUG_TEXT_BUFFER_SIZE];
    s32 stepX;
    s32 stepZ;
    Boolean isStepping;
    f64 cameraDistance;

    sprintf(text, "X:%d Z:%d             ", (int)f_HolePointCursor[0], (int)f_HolePointCursor[1]);
    PrintDebugText(text, CURSOR_TEXT_COLUMN, CURSOR_TEXT_ROW);
    g_ShowsGroundWaves = FALSE;
    isStepping = FALSE;
    if (f_HolePointEditMode == HOLE_POINT_MODE_UNSET) {
        f_HolePointEditMode = HOLE_POINT_MODE_PIN_TEE;
        f_HolePointCursor[0] =
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
        f_HolePointCursor[1] =
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
    }

    if (g_MenuControllerData[EDITOR_PAD].button & R_CBUTTONS) {
        ClearDebugText();
        f_HolePointEditMode++;
        if (f_HolePointEditMode == HOLE_POINT_MODE_COUNT) {
            f_HolePointEditMode = HOLE_POINT_MODE_COUNT - 1;
        }

        if (f_HolePointEditMode == HOLE_POINT_MODE_PIN_TEE) {
            f_HolePointCursor[0] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[1] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
            f_MesonPointIndex = 0;
            f_RoutePointIndex = 0;
        }

        if (f_HolePointEditMode == HOLE_POINT_MODE_MESON) {
            f_HolePointCursor[0] =
                g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[1] =
                g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        if (f_HolePointEditMode == HOLE_POINT_MODE_ROUTE) {
            f_HolePointCursor[0] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[1] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }
    }

    if (g_MenuControllerData[EDITOR_PAD].button & L_CBUTTONS) {
        ClearDebugText();
        f_HolePointEditMode--;
        if (f_HolePointEditMode < 0) {
            f_HolePointEditMode = HOLE_POINT_MODE_PIN_TEE;
        }

        if (f_HolePointEditMode == HOLE_POINT_MODE_PIN_TEE) {
            f_HolePointCursor[0] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[1] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
            f_MesonPointIndex = 0;
            f_RoutePointIndex = 0;
        }

        if (f_HolePointEditMode == HOLE_POINT_MODE_MESON) {
            f_HolePointCursor[0] =
                g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[1] =
                g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        if (f_HolePointEditMode == HOLE_POINT_MODE_ROUTE) {
            f_HolePointCursor[0] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[1] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }
    }

    if (f_HolePointEditMode == HOLE_POINT_MODE_PIN_TEE) {
        if (g_MenuControllerData[EDITOR_PAD].button & A_BUTTON) {
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] = f_HolePointCursor[0]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] = f_HolePointCursor[1]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Y] = GetGroundHeight(
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X],
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z]
            );
            PlaySound(SOUND_MENU_CONFIRM, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
            PrintDebugText(
                GetSurfaceInfoAt(
                    g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X],
                    g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z]
                )
                    ->name,
                SURFACE_TEXT_COLUMN,
                SURFACE_TEXT_ROW
            );
            if (f_PinTeeIndex == 0) {
                g_HoleSetup.cupPosition[AXIS_X] = g_HolePoints.cupPositions[0][AXIS_X];
                g_HoleSetup.cupPosition[AXIS_Y] = g_HolePoints.cupPositions[0][AXIS_Y];
                g_HoleSetup.cupPosition[AXIS_Z] = g_HolePoints.cupPositions[0][AXIS_Z];
            }
        }

        if (g_MenuControllerData[EDITOR_PAD].button & B_BUTTON) {
            g_HolePoints.teePositions[f_PinTeeIndex][AXIS_X] = f_HolePointCursor[0]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Z] = f_HolePointCursor[1]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Y] = GetGroundHeight(
                g_HolePoints.teePositions[f_PinTeeIndex][AXIS_X],
                g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Z]
            );
            PlaySound(SOUND_MENU_CONFIRM, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
            PrintDebugText(
                GetSurfaceInfoAt(
                    g_HolePoints.teePositions[f_PinTeeIndex][AXIS_X],
                    g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Z]
                )
                    ->name,
                SURFACE_TEXT_COLUMN,
                SURFACE_TEXT_ROW
            );
            if (f_PinTeeIndex == 0) {
                g_HoleSetup.teePosition[AXIS_X] = g_HolePoints.teePositions[0][AXIS_X];
                g_HoleSetup.teePosition[AXIS_Y] = g_HolePoints.teePositions[0][AXIS_Y];
                g_HoleSetup.teePosition[AXIS_Z] = g_HolePoints.teePositions[0][AXIS_Z];
            }
        }

        /* The second pad's B clears the point */
        if (g_MenuControllerData[OPTION_PAD].button & B_BUTTON) {
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] = 0;
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Y] = 0;
            g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] = 0;
            g_HolePoints.teePositions[f_PinTeeIndex][AXIS_X] = 0;
            g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Y] = 0;
            g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Z] = 0;
            f_HolePointCursor[0] = 0;
            f_HolePointCursor[1] = 0;
        }

        if (g_MenuControllerData[EDITOR_PAD].button & U_CBUTTONS) {
            f_PinTeeIndex++;
            if (f_PinTeeIndex == CUP_POSITION_COUNT) {
                f_PinTeeIndex = 0;
            }

            f_HolePointCursor[0] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[1] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        if (g_MenuControllerData[EDITOR_PAD].button & D_CBUTTONS) {
            f_PinTeeIndex--;
            if (f_PinTeeIndex < 0) {
                f_PinTeeIndex = CUP_POSITION_COUNT - 1;
            }

            f_HolePointCursor[0] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[1] =
                g_HolePoints.cupPositions[f_PinTeeIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        /* Start jumps to the tee */
        if (g_MenuControllerData[EDITOR_PAD].button & START_BUTTON) {
            f_HolePointCursor[0] =
                g_HolePoints.teePositions[f_PinTeeIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[1] =
                g_HolePoints.teePositions[f_PinTeeIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        PrintDebugText("Pin Tee Edit Mode", HOLE_POINT_TEXT_COLUMN, HOLE_POINT_MODE_TEXT_ROW);
        PrintDebugHex(f_PinTeeIndex, HOLE_POINT_TEXT_COLUMN, HOLE_POINT_INDEX_TEXT_ROW);
    }

    if (f_HolePointEditMode == HOLE_POINT_MODE_MESON) {
        if (g_MenuControllerData[EDITOR_PAD].button & A_BUTTON) {
            g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_X] = f_HolePointCursor[0]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_Z] = f_HolePointCursor[1]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_Y] = GetGroundHeight(
                g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_X],
                g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_Z]
            );
            PlaySound(SOUND_MENU_CONFIRM, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
            PrintDebugText(
                GetSurfaceInfoAt(
                    g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_X],
                    g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_Z]
                )
                    ->name,
                SURFACE_TEXT_COLUMN,
                SURFACE_TEXT_ROW
            );
        }

        /* The second pad's B clears the point */
        if (g_MenuControllerData[OPTION_PAD].button & B_BUTTON) {
            g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_X] = 0;
            g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_Y] = 0;
            g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_Z] = 0;
            f_HolePointCursor[0] = 0;
            f_HolePointCursor[1] = 0;
        }

        if (g_MenuControllerData[EDITOR_PAD].button & U_CBUTTONS) {
            f_MesonPointIndex++;
            if (f_MesonPointIndex == ARRAY_COUNT(g_HolePoints.aimPoints)) {
                f_MesonPointIndex = 0;
            }

            f_HolePointCursor[0] =
                g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[1] =
                g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        if (g_MenuControllerData[EDITOR_PAD].button & D_CBUTTONS) {
            f_MesonPointIndex--;
            if (f_MesonPointIndex < 0) {
                f_MesonPointIndex = ARRAY_COUNT(g_HolePoints.aimPoints) - 1;
            }

            f_HolePointCursor[0] =
                g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[1] =
                g_HolePoints.aimPoints[f_MesonPointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        PrintDebugText("Meson Edit Mode", HOLE_POINT_TEXT_COLUMN, HOLE_POINT_MODE_TEXT_ROW);
        PrintDebugHex(f_MesonPointIndex, HOLE_POINT_TEXT_COLUMN, HOLE_POINT_INDEX_TEXT_ROW);
    }

    if (f_HolePointEditMode == HOLE_POINT_MODE_ROUTE) {
        if (g_MenuControllerData[EDITOR_PAD].button & A_BUTTON) {
            g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X] = f_HolePointCursor[0]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z] = f_HolePointCursor[1]
                << WORLD_TO_COURSE_SHIFT;
            g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Y] = GetGroundHeight(
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X],
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z]
            );
            PlaySound(SOUND_MENU_CONFIRM, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
            PrintDebugText(
                GetSurfaceInfoAt(
                    g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X],
                    g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z]
                )
                    ->name,
                SURFACE_TEXT_COLUMN,
                SURFACE_TEXT_ROW
            );
        }

        /* The second pad's B clears the point */
        if (g_MenuControllerData[OPTION_PAD].button & B_BUTTON) {
            g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X] = 0;
            g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Y] = 0;
            g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z] = 0;
            f_HolePointCursor[0] = 0;
            f_HolePointCursor[1] = 0;
        }

        if (g_MenuControllerData[EDITOR_PAD].button & U_CBUTTONS) {
            f_RoutePointIndex++;
            if (f_RoutePointIndex == ROUTE_POINT_COUNT) {
                f_RoutePointIndex = 0;
            }

            f_HolePointCursor[0] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[1] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        if (g_MenuControllerData[EDITOR_PAD].button & D_CBUTTONS) {
            f_RoutePointIndex--;
            if (f_RoutePointIndex < 0) {
                f_RoutePointIndex = ROUTE_POINT_COUNT - 1;
            }

            f_HolePointCursor[0] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_X] / (1 << WORLD_TO_COURSE_SHIFT);
            f_HolePointCursor[1] =
                g_HolePoints.routePoints[f_RoutePointIndex][AXIS_Z] / (1 << WORLD_TO_COURSE_SHIFT);
        }

        PrintDebugText("AI Edit Mode", HOLE_POINT_TEXT_COLUMN, HOLE_POINT_MODE_TEXT_ROW);
        PrintDebugHex(f_RoutePointIndex, HOLE_POINT_TEXT_COLUMN, HOLE_POINT_INDEX_TEXT_ROW);
    }

    /* Z held turns the camera instead of moving the cursor */
    if (!(g_ControllerData[EDITOR_PAD].button & Z_TRIG)) {
        if (*(u16 *)&g_MenuControllerData[EDITOR_PAD].stick_x != 0) {
            stepX = (-cos(HEADING_RADIANS) * g_MenuControllerData[EDITOR_PAD].stick_y) -
                (sin(HEADING_RADIANS) * g_MenuControllerData[EDITOR_PAD].stick_x);
            stepZ = (-sin(HEADING_RADIANS) * g_MenuControllerData[EDITOR_PAD].stick_y) +
                (cos(HEADING_RADIANS) * g_MenuControllerData[EDITOR_PAD].stick_x);
            f_HolePointCursor[0] += stepX;
            f_HolePointCursor[1] += stepZ;
        }
        else {
            if (g_MenuControllerData[EDITOR_PAD].button & U_JPAD) {
                stepX = -cos(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
                isStepping = TRUE;
                stepZ = -sin(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
            }

            if (g_MenuControllerData[EDITOR_PAD].button & D_JPAD) {
                stepX = cos(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
                isStepping = TRUE;
                stepZ = sin(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
            }

            if (g_MenuControllerData[EDITOR_PAD].button & R_JPAD) {
                stepX = -sin(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
                isStepping = TRUE;
                stepZ = cos(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
            }

            if (g_MenuControllerData[EDITOR_PAD].button & L_JPAD) {
                stepX = sin(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
                isStepping = TRUE;
                stepZ = -cos(HEADING_RADIANS) * HOLE_POINT_STEP_LENGTH;
            }

            if (isStepping) {
                f_HolePointCursor[0] += stepX;
                f_HolePointCursor[1] += stepZ;
            }
        }
    }

    if ((g_ControllerData[EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[EDITOR_PAD].button & L_TRIG) ||
        (g_ControllerData[EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[EDITOR_PAD].button & Z_TRIG) {
        f_HolePointCameraHeadingAngle -=
            g_MenuControllerData[EDITOR_PAD].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_HolePointCameraPitch -=
            g_MenuControllerData[EDITOR_PAD].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }

    if (f_HolePointCameraPitch > 0.0f) {
        f_HolePointCameraPitch = 0.0f;
    }

    if (f_HolePointCameraPitch < EDITOR_PITCH_MIN) {
        f_HolePointCameraPitch = EDITOR_PITCH_MIN;
    }

    f_HolePointCameraHeading = f_HolePointCameraHeadingAngle * EDITOR_ANGLE_TO_RADIANS;
    g_MapCamera.view.lookAt[AXIS_X] = f_HolePointCursor[0];
    g_MapCamera.view.lookAt[AXIS_Z] = f_HolePointCursor[1];
    g_MapCamera.view.lookAt[AXIS_Y] = GetGroundHeight(
                                          f_HolePointCursor[0] << WORLD_TO_COURSE_SHIFT,
                                          f_HolePointCursor[1] << WORLD_TO_COURSE_SHIFT
                                      ) *
        (1.0f / (1 << WORLD_TO_COURSE_SHIFT));
    cameraDistance = EDITOR_CAMERA_DISTANCE;
    g_MapCamera.view.eye[AXIS_X] = (cos(f_HolePointCameraPitch) * cos(f_HolePointCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = (cos(f_HolePointCameraPitch) * sin(f_HolePointCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] =
        (sin(f_HolePointCameraPitch) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Y];
}

void UpdateTriangleSlopes(void);

/*
 * Scene SCENE_TRIANGLES's editor for the hole triangles. On the editor pad,
 * the C-buttons move the picked triangle across the ground, relative to the
 * camera's heading, and B picks its next corner. With Z held only that
 * corner moves. The option pad's
 * C-down and C-up move it along y, A saves the corner in a ring of
 * positions and B restores the last one saved. Right and left on the
 * control pad pick the triangle, and up and down change its kind. R
 * zooms in, L or Z with R zooms out, and Z with the stick orbits the
 * camera around the corner.
 */
void UpdateTriangleEditor(void) {
    Mat4 unusedMatrices[2] UNUSED; /* The game's frame has room for two matrices */
    f32 stepRight;
    f32 stepUp;
    f32 stepY;
    f32 stepX;
    f32 stepZ;
    f32 moveY; /* The scaled y step; stepY itself would compile differently */
    s32 editKind;
    s32 corner;
    f64 cameraDistance; /* As a literal, GCC would fold it into the angles before
                           the zoom */
    Vtx_t *lookAtVertex;

    editKind = TRIANGLE_EDIT_NONE;
    g_ShowsGroundWaves = FALSE;
    stepY = 0.0f;
    stepRight = stepY;
    stepUp = stepRight;
    if (g_ControllerData[EDITOR_PAD].button & U_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepUp = 1.0f;
    }

    if (g_ControllerData[EDITOR_PAD].button & D_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepUp = -1.0f;
    }

    if (g_ControllerData[EDITOR_PAD].button & R_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepRight = 1.0f;
    }

    if (g_ControllerData[EDITOR_PAD].button & L_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepRight = -1.0f;
    }

    if (g_ControllerData[OPTION_PAD].button & D_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepY = 1.0f;
    }

    if (g_ControllerData[OPTION_PAD].button & U_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepY = -1.0f;
    }

    if (g_MenuControllerData[EDITOR_PAD].button & B_BUTTON) {
        f_EditedCornerIndex = (f_EditedCornerIndex + 1) % HOLE_TRIANGLE_VERTEX_COUNT;
    }

    PrintDebugHex(editKind, TRIANGLE_TEXT_COLUMN, EDIT_KIND_TEXT_ROW);
    if (editKind == TRIANGLE_EDIT_MOVE) {
        stepX = (-cos((u16)f_TriangleCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepUp) -
            (sin((u16)f_TriangleCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepRight);
        stepZ = (-sin((u16)f_TriangleCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepUp) +
            (cos((u16)f_TriangleCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepRight);
        /* Scaled in two steps; in one expression GCC would multiply 10 by the zoom
         * first */
        {
            f32 scaledX = stepX * CORNER_STEP_SCALE;
            f32 scaledY = stepY * CORNER_STEP_SCALE;
            f32 scaledZ = stepZ * CORNER_STEP_SCALE;

            stepX = scaledX * g_MapCamera.view.zoom;
            moveY = scaledY * g_MapCamera.view.zoom;
            stepZ = scaledZ * g_MapCamera.view.zoom;
        }

        for (corner = 0; corner < HOLE_TRIANGLE_VERTEX_COUNT; corner++) {
            if (!(g_ControllerData[EDITOR_PAD].button & Z_TRIG) || f_EditedCornerIndex == corner) {
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

    if (g_MenuControllerData[OPTION_PAD].button & A_BUTTON) {
        Vtx_t *vertex;

        vertex = &g_WorkArea.hole.triangles[f_EditedTriangleIndex].vertices[f_EditedCornerIndex].v;
        f_SavedCornerIndex = (f_SavedCornerIndex + 1) % SAVED_CORNER_COUNT;
        f_SavedCorners[f_SavedCornerIndex][AXIS_X] = vertex->ob[AXIS_X];
        f_SavedCorners[f_SavedCornerIndex][AXIS_Y] = vertex->ob[AXIS_Y];
        f_SavedCorners[f_SavedCornerIndex][AXIS_Z] = vertex->ob[AXIS_Z];
        PrepareHoleTriangles();
    }

    if (g_MenuControllerData[OPTION_PAD].button & B_BUTTON) {
        Vtx_t *vertex;

        vertex = &g_WorkArea.hole.triangles[f_EditedTriangleIndex].vertices[f_EditedCornerIndex].v;
        vertex->ob[AXIS_X] = f_SavedCorners[f_SavedCornerIndex][AXIS_X];
        vertex->ob[AXIS_Y] = f_SavedCorners[f_SavedCornerIndex][AXIS_Y];
        vertex->ob[AXIS_Z] = f_SavedCorners[f_SavedCornerIndex][AXIS_Z];
        f_SavedCornerIndex = (f_SavedCornerIndex + SAVED_CORNER_COUNT - 1) % SAVED_CORNER_COUNT;
        PrepareHoleTriangles();
    }

    if ((g_MenuControllerData[EDITOR_PAD].button & R_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        f_EditedTriangleIndex++;
    }

    if ((g_MenuControllerData[EDITOR_PAD].button & L_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        f_EditedTriangleIndex--;
    }

    if (f_EditedTriangleIndex < 0) {
        f_EditedTriangleIndex = FIRST_RING_TRIANGLE - 1;
    }

    if (f_EditedTriangleIndex >= FIRST_RING_TRIANGLE) {
        f_EditedTriangleIndex = 0;
    }

    if ((g_MenuControllerData[EDITOR_PAD].button & U_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        g_WorkArea.hole.triangles[f_EditedTriangleIndex].kind++;
        editKind = TRIANGLE_EDIT_KIND;
        if (g_WorkArea.hole.triangles[f_EditedTriangleIndex].kind >= HOLE_TRIANGLE_KIND_COUNT) {
            g_WorkArea.hole.triangles[f_EditedTriangleIndex].kind = 0;
        }

        PrepareHoleTriangles();
    }

    if ((g_MenuControllerData[EDITOR_PAD].button & D_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        g_WorkArea.hole.triangles[f_EditedTriangleIndex].kind--;
        editKind = TRIANGLE_EDIT_KIND;
        PrepareHoleTriangles();
    }

    if ((g_ControllerData[EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[EDITOR_PAD].button & L_TRIG) ||
        (g_ControllerData[EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[EDITOR_PAD].button & Z_TRIG) {
        f_TriangleCameraHeadingAngle -=
            g_MenuControllerData[EDITOR_PAD].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_TriangleCameraPitch -=
            g_MenuControllerData[EDITOR_PAD].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }

    if (f_TriangleCameraPitch > 0.0f) {
        f_TriangleCameraPitch = 0.0f;
    }

    if (f_TriangleCameraPitch < EDITOR_PITCH_MIN) {
        f_TriangleCameraPitch = EDITOR_PITCH_MIN;
    }

    f_TriangleCameraHeading = f_TriangleCameraHeadingAngle * EDITOR_ANGLE_TO_RADIANS;
    /* The first frame without an edit rebuilds the slopes */
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
    g_MapCamera.view.eye[AXIS_X] = (cos(f_TriangleCameraPitch) * cos(f_TriangleCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = (cos(f_TriangleCameraPitch) * sin(f_TriangleCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] =
        (sin(f_TriangleCameraPitch) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Y];
}

/*
 * Scene SCENE_RINGS's editor for the rings of Ring Shot, which are kept in
 * the last RING_COUNT hole triangles. On the editor pad, the C-buttons move
 * the ring across the ground, relative to the camera's heading, or with Z
 * held C-up and C-down scale it. A turns the triangle into a ring facing
 * along the camera's view. The option pad's C-down and C-up move it along
 * y, and the second editor pad's B clears the triangle. Right and left on
 * the control pad pick the ring. R zooms in, L or Z with R zooms out, and
 * Z with the stick orbits the camera around the ring.
 */
void UpdateRingEditor(void) {
    Vec3 direction;
    f32 stepRight;
    f32 stepUp;
    f32 stepY;
    f32 stepX;
    f32 stepZ;
    f32 moveY; /* The scaled y step; stepY itself would compile differently */
    s32 editKind;
    s32 corner;
    f64 cameraDistance; /* As a literal, GCC would fold it into the angles before
                           the zoom */
    Vtx_t *lookAtVertex;

    editKind = TRIANGLE_EDIT_NONE;
    stepY = 0.0f;
    g_ShowsGroundWaves = FALSE;
    sprintf(g_PrintBuffer, "EditNo : %d   ", (int)f_RingTriangleIndex);
    PrintDebugText(g_PrintBuffer, RING_TEXT_COLUMN, RING_TEXT_ROW);
    stepRight = 0.0f;
    stepUp = stepRight;
    if (!(g_ControllerData[EDITOR_PAD].button & Z_TRIG)) {
        if (g_ControllerData[EDITOR_PAD].button & U_CBUTTONS) {
            editKind = TRIANGLE_EDIT_MOVE;
            stepUp = 1.0f;
        }

        if (g_ControllerData[EDITOR_PAD].button & D_CBUTTONS) {
            editKind = TRIANGLE_EDIT_MOVE;
            stepUp = -1.0f;
        }

        if (g_ControllerData[EDITOR_PAD].button & R_CBUTTONS) {
            editKind = TRIANGLE_EDIT_MOVE;
            stepRight = 1.0f;
        }

        if (g_ControllerData[EDITOR_PAD].button & L_CBUTTONS) {
            editKind = TRIANGLE_EDIT_MOVE;
            stepRight = -1.0f;
        }
    }
    else {
        Vtx_t *scaleVertex =
            &g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[RING_SCALE_CORNER].v;

        if (g_ControllerData[EDITOR_PAD].button & U_CBUTTONS) {
            scaleVertex->ob[AXIS_X] += RING_SCALE_STEP;
            if (scaleVertex->ob[AXIS_X] >= RING_SCALE_MAX) {
                scaleVertex->ob[AXIS_X] = RING_SCALE_MAX;
            }

            PrepareHoleTriangles();
        }

        if (g_ControllerData[EDITOR_PAD].button & D_CBUTTONS) {
            scaleVertex->ob[AXIS_X] -= RING_SCALE_STEP;
            if (scaleVertex->ob[AXIS_X] < RING_SCALE_MIN) {
                scaleVertex->ob[AXIS_X] = RING_SCALE_MIN;
            }

            PrepareHoleTriangles();
        }
    }

    if (g_ControllerData[OPTION_PAD].button & D_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepY = 1.0f;
    }

    if (g_ControllerData[OPTION_PAD].button & U_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepY = -1.0f;
    }

    PrintDebugHex(editKind, TRIANGLE_TEXT_COLUMN, EDIT_KIND_TEXT_ROW);
    if (editKind == TRIANGLE_EDIT_MOVE) {
        stepX = (-cos((u16)f_RingCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepUp) -
            (sin((u16)f_RingCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepRight);
        stepZ = (-sin((u16)f_RingCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepUp) +
            (cos((u16)f_RingCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepRight);
        /* Scaled in two steps; in one expression GCC would multiply 10 by the zoom
         * first */
        {
            f32 scaledX = stepX * CORNER_STEP_SCALE;
            f32 scaledY = stepY * CORNER_STEP_SCALE;
            f32 scaledZ = stepZ * CORNER_STEP_SCALE;

            stepX = scaledX * g_MapCamera.view.zoom;
            moveY = scaledY * g_MapCamera.view.zoom;
            stepZ = scaledZ * g_MapCamera.view.zoom;
        }

        for (corner = 0; corner < HOLE_TRIANGLE_VERTEX_COUNT; corner++) {
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

    if (g_ControllerData[EDITOR_PAD].button & A_BUTTON) {
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

    if ((g_MenuControllerData[EDITOR_PAD].button & R_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        f_RingTriangleIndex++;
    }

    if ((g_MenuControllerData[EDITOR_PAD].button & L_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        f_RingTriangleIndex--;
    }

    if (f_RingTriangleIndex < FIRST_RING_TRIANGLE) {
        f_RingTriangleIndex = HOLE_TRIANGLE_COUNT - 1;
    }

    if (f_RingTriangleIndex >= HOLE_TRIANGLE_COUNT) {
        f_RingTriangleIndex = FIRST_RING_TRIANGLE;
    }

    if (g_ControllerData[SECOND_EDITOR_PAD].button & B_BUTTON) {
        /* Clears every field */
        g_WorkArea.hole.triangles[f_RingTriangleIndex].kind = HOLE_TRIANGLE_KIND_FLAT;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.ob[AXIS_X] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.ob[AXIS_Y] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.ob[AXIS_Z] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.flag = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.tc[TEXTURE_AXIS_S] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.tc[TEXTURE_AXIS_T] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.cn[0] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.cn[1] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.cn[2] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[0].v.cn[3] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.ob[AXIS_X] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.ob[AXIS_Y] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.ob[AXIS_Z] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.flag = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.tc[TEXTURE_AXIS_S] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.tc[TEXTURE_AXIS_T] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.cn[0] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.cn[1] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.cn[2] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[1].v.cn[3] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.ob[AXIS_X] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.ob[AXIS_Y] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.ob[AXIS_Z] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.flag = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.tc[TEXTURE_AXIS_S] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.tc[TEXTURE_AXIS_T] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.cn[0] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.cn[1] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.cn[2] = 0;
        g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[2].v.cn[3] = 0;
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

    if ((g_ControllerData[EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[EDITOR_PAD].button & L_TRIG) ||
        (g_ControllerData[EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[EDITOR_PAD].button & Z_TRIG) {
        f_RingCameraHeadingAngle -=
            g_MenuControllerData[EDITOR_PAD].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_RingCameraPitch -=
            g_MenuControllerData[EDITOR_PAD].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }

    if (f_RingCameraPitch > 0.0f) {
        f_RingCameraPitch = 0.0f;
    }

    if (f_RingCameraPitch < EDITOR_PITCH_MIN) {
        f_RingCameraPitch = EDITOR_PITCH_MIN;
    }

    f_RingCameraHeading = f_RingCameraHeadingAngle * EDITOR_ANGLE_TO_RADIANS;

    /* The first frame without an edit rebuilds the slopes */
    if ((f_LastRingEditKind != TRIANGLE_EDIT_NONE) & (editKind == TRIANGLE_EDIT_NONE)) {
        UpdateTriangleSlopes();
    }

    cameraDistance = EDITOR_CAMERA_DISTANCE;
    lookAtVertex = &g_WorkArea.hole.triangles[f_RingTriangleIndex].vertices[f_RingCornerIndex].v;
    g_MapCamera.view.lookAt[AXIS_X] = lookAtVertex->ob[AXIS_X];
    g_MapCamera.view.lookAt[AXIS_Y] = lookAtVertex->ob[AXIS_Y];
    f_LastRingEditKind = editKind;
    g_MapCamera.view.lookAt[AXIS_Z] = lookAtVertex->ob[AXIS_Z];
    g_MapCamera.view.eye[AXIS_X] = (cos(f_RingCameraPitch) * cos(f_RingCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = (cos(f_RingCameraPitch) * sin(f_RingCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] =
        (sin(f_RingCameraPitch) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Y];
}

/*
 * Scene SCENE_WALLS's editor for walls, each a pair of shaded hole triangles
 * from f_FirstWallTriangle up to the rings. The editor pad's C-buttons move
 * the wall along x and z, the option pad's C-down and C-up along y, and the
 * option pad's C-left drops it onto the ground. Z limits an edit to the
 * corner that the editor pad's B picks. The option pad's A saves that corner
 * and its B restores the last saved one. Right and left on the control pad
 * pick the wall. R zooms in, L or Z with R zooms out, and Z with the stick
 * orbits the camera around the corner.
 */
void UpdateWallEditor(void) {
    char text[DEBUG_TEXT_BUFFER_SIZE];
    Vtx_t *corners[HOLE_TRIANGLE_VERTEX_COUNT];
    Vtx_t *nextCorners[HOLE_TRIANGLE_VERTEX_COUNT];
    Mat4 unusedMatrices[2] UNUSED; /* The game's frame has room for two matrices */
    f32 stepRight;
    f32 stepUp;
    f32 stepY;
    f32 stepX;
    f32 stepZ;
    f32 moveY; /* The scaled y step; stepY itself would compile differently */
    s32 editKind;
    Boolean isDropping;
    s32 corner;
    f64 cameraDistance; /* As a literal, GCC would fold it into the angles before
                           the zoom */
    Vtx_t *lookAtVertex;

    editKind = TRIANGLE_EDIT_NONE;
    isDropping = FALSE;
    g_ShowsGroundWaves = FALSE;
    stepY = 0.0f;
    stepRight = stepY;
    stepUp = stepRight;
    if (g_ControllerData[EDITOR_PAD].button & U_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepUp = 1.0f;
    }

    if (g_ControllerData[EDITOR_PAD].button & D_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepUp = -1.0f;
    }

    if (g_ControllerData[EDITOR_PAD].button & R_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepRight = 1.0f;
    }

    if (g_ControllerData[EDITOR_PAD].button & L_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepRight = -1.0f;
    }

    if (g_ControllerData[OPTION_PAD].button & D_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepY = 1.0f;
    }

    if (g_ControllerData[OPTION_PAD].button & U_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        stepY = -1.0f;
    }

    if (g_ControllerData[OPTION_PAD].button & L_CBUTTONS) {
        isDropping = TRUE;
    }

    if (g_MenuControllerData[EDITOR_PAD].button & B_BUTTON) {
        f_WallCornerIndex = (f_WallCornerIndex + 1) % WALL_BOTTOM_CORNER_COUNT;
    }

    PrintDebugHex(editKind, TRIANGLE_TEXT_COLUMN, EDIT_KIND_TEXT_ROW);
    sprintf(text, "EditNo%d   ", f_WallTriangleIndex);
    PrintDebugText(text, WALL_TEXT_COLUMN, WALL_TEXT_ROW);
    if ((editKind == TRIANGLE_EDIT_MOVE) | (isDropping == TRUE)) {
        /* Left over from the triangle editor, though walls move along the world
         * axes: both turned steps are overwritten */
        stepZ = stepUp;
        stepX = (-cos((u16)f_WallCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepUp) -
            (sin((u16)f_WallCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepRight);
        stepX = (-sin((u16)f_WallCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepUp) +
            (cos((u16)f_WallCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepRight);

        /* Scaled in two steps; in one expression GCC would multiply 10 by the zoom
         * first */
        {
            f32 scaledX = stepRight * CORNER_STEP_SCALE;
            f32 scaledY = stepY * CORNER_STEP_SCALE;
            f32 scaledZ = stepZ * CORNER_STEP_SCALE;

            stepX = scaledX * g_MapCamera.view.zoom;
            moveY = scaledY * g_MapCamera.view.zoom;
            stepZ = scaledZ * g_MapCamera.view.zoom;
        }

        for (corner = 0; corner < HOLE_TRIANGLE_VERTEX_COUNT; corner++) {
            if (!(g_ControllerData[EDITOR_PAD].button & Z_TRIG) || f_WallCornerIndex == corner) {
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

        for (corner = 0; corner < HOLE_TRIANGLE_VERTEX_COUNT; corner++) {
            corners[corner] = &g_WorkArea.hole.triangles[f_WallTriangleIndex].vertices[corner].v;
            nextCorners[corner] =
                &g_WorkArea.hole.triangles[f_WallTriangleIndex + 1].vertices[corner].v;
        }

        /* The first triangle's top corner stands over its first corner; the second
         * fills in the rest */
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

    if (g_MenuControllerData[OPTION_PAD].button & A_BUTTON) {
        Vtx_t *vertex;

        vertex = &g_WorkArea.hole.triangles[f_WallTriangleIndex].vertices[f_WallCornerIndex].v;
        f_SavedWallCornerIndex = (f_SavedWallCornerIndex + 1) % SAVED_CORNER_COUNT;
        f_SavedWallCorners[f_SavedWallCornerIndex][AXIS_X] = vertex->ob[AXIS_X];
        f_SavedWallCorners[f_SavedWallCornerIndex][AXIS_Y] = vertex->ob[AXIS_Y];
        f_SavedWallCorners[f_SavedWallCornerIndex][AXIS_Z] = vertex->ob[AXIS_Z];
        PrepareHoleTriangles();
    }

    if (g_MenuControllerData[OPTION_PAD].button & B_BUTTON) {
        Vtx_t *vertex;

        vertex = &g_WorkArea.hole.triangles[f_WallTriangleIndex].vertices[f_WallCornerIndex].v;
        vertex->ob[AXIS_X] = f_SavedWallCorners[f_SavedWallCornerIndex][AXIS_X];
        vertex->ob[AXIS_Y] = f_SavedWallCorners[f_SavedWallCornerIndex][AXIS_Y];
        vertex->ob[AXIS_Z] = f_SavedWallCorners[f_SavedWallCornerIndex][AXIS_Z];
        f_SavedWallCornerIndex =
            (f_SavedWallCornerIndex + SAVED_CORNER_COUNT - 1) % SAVED_CORNER_COUNT;
        PrepareHoleTriangles();
    }

    if ((g_MenuControllerData[EDITOR_PAD].button & R_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        f_WallTriangleIndex += WALL_TRIANGLE_COUNT;
    }

    if ((g_MenuControllerData[EDITOR_PAD].button & L_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
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

    /* Both of the wall's triangles are kept shaded */
    g_WorkArea.hole.triangles[f_WallTriangleIndex].kind = HOLE_TRIANGLE_KIND_SHADED;
    g_WorkArea.hole.triangles[f_WallTriangleIndex + 1].kind = HOLE_TRIANGLE_KIND_SHADED;
    if ((g_MenuControllerData[EDITOR_PAD].button & U_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_KIND;

        /* Never true; the triangle editor's kind cycling without the step */
        if (g_WorkArea.hole.triangles[f_WallTriangleIndex].kind >= HOLE_TRIANGLE_KIND_COUNT) {
            g_WorkArea.hole.triangles[f_WallTriangleIndex].kind = HOLE_TRIANGLE_KIND_FLAT;
        }

        PrepareHoleTriangles();
    }

    if ((g_MenuControllerData[EDITOR_PAD].button & D_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_KIND;
        PrepareHoleTriangles();
    }

    if ((g_ControllerData[EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[EDITOR_PAD].button & L_TRIG) ||
        (g_ControllerData[EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[EDITOR_PAD].button & Z_TRIG) {
        f_WallCameraHeadingAngle -=
            g_MenuControllerData[EDITOR_PAD].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_WallCameraPitch -=
            g_MenuControllerData[EDITOR_PAD].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }

    if (f_WallCameraPitch > 0.0f) {
        f_WallCameraPitch = 0.0f;
    }

    if (f_WallCameraPitch < EDITOR_PITCH_MIN) {
        f_WallCameraPitch = EDITOR_PITCH_MIN;
    }

    f_WallCameraHeading = f_WallCameraHeadingAngle * EDITOR_ANGLE_TO_RADIANS;

    /* The first frame without an edit rebuilds the slopes */
    if ((f_LastWallEditKind != TRIANGLE_EDIT_NONE) & (editKind == TRIANGLE_EDIT_NONE)) {
        UpdateTriangleSlopes();
    }

    cameraDistance = EDITOR_CAMERA_DISTANCE;
    lookAtVertex = &g_WorkArea.hole.triangles[f_WallTriangleIndex].vertices[f_WallCornerIndex].v;
    g_MapCamera.view.lookAt[AXIS_X] = lookAtVertex->ob[AXIS_X];
    g_MapCamera.view.lookAt[AXIS_Y] = lookAtVertex->ob[AXIS_Y];
    f_LastWallEditKind = editKind;
    g_MapCamera.view.lookAt[AXIS_Z] = lookAtVertex->ob[AXIS_Z];
    g_MapCamera.view.eye[AXIS_X] = (cos(f_WallCameraPitch) * cos(f_WallCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = (cos(f_WallCameraPitch) * sin(f_WallCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] =
        (sin(f_WallCameraPitch) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Y];
}

/*
 * Sets the billboards and the hole's points back on the ground, and
 * copies the first cup and tee positions to g_HoleSetup. The billboards'
 * first height comes from their x and z unscaled. The translation each
 * billboard builds goes unused, and PlaceBillboards places them all again
 * for each one.
 */
void PutObjectsOnGround(void) {
    Mat4 translation;
    /*
     * The game's frame has room for a second matrix; without it 28 words differ
     */
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

    g_HoleSetup.cupPosition[AXIS_X] = g_HolePoints.cupPositions[0][AXIS_X];
    g_HoleSetup.cupPosition[AXIS_Y] = g_HolePoints.cupPositions[0][AXIS_Y];
    g_HoleSetup.cupPosition[AXIS_Z] = g_HolePoints.cupPositions[0][AXIS_Z];
    g_HoleSetup.teePosition[AXIS_X] = g_HolePoints.teePositions[0][AXIS_X];
    g_HoleSetup.teePosition[AXIS_Y] = g_HolePoints.teePositions[0][AXIS_Y];
    g_HoleSetup.teePosition[AXIS_Z] = g_HolePoints.teePositions[0][AXIS_Z];
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

/*
 * Slopes the out-of-bounds ground: each out-of-bounds ground point moves
 * along y by how far it is from the nearest point with another code, with
 * noise from SLOPE_NOISE_DISTANCE on. The height editor's option pad calls
 * it with C-down.
 */
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

            /* Never true, as the search stops at SLOPE_NOISE_DISTANCE */
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

/*
 * Runs the ground height editor. On the editor pads, the D-pad moves a
 * cursor over the ground points, relative to the camera's heading, Z with
 * the stick turns the camera, and R and L zoom it. On the first pad, C-up
 * and C-down lower and raise the point (with Z, every point), C-right
 * repeats the last height and C-left adds noise to it, and A and B add
 * noise to the point. On the second pad, C-left and C-up pick up two
 * heights and C-right and C-down move the point by their difference. The
 * option pad toggles f_ShowsGroundWaves and calls PutObjectsOnGround and
 * SlopeOutOfBoundsGround. After a frame of edits, the next frame without one
 * rebuilds the slopes.
 */
void UpdateHeightEditor(void) {
    char text[DEBUG_TEXT_BUFFER_SIZE];
    s32 baseHeight;
    s32 editKind;
    s32 direction;
    s32 columnStep;
    s32 rowStep;
    f64 cameraDistance; /* As a literal, GCC would fold it into the angles before
                           the zoom */

    /* Moves every ground point's height by change */
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

    if (g_MenuControllerData[OPTION_PAD].button & D_CBUTTONS) {
        SlopeOutOfBoundsGround();
    }

    if (g_MenuControllerData[SECOND_EDITOR_PAD].button & L_CBUTTONS) {
        f_CLeftPickedHeight = GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Y];
    }

    if (g_MenuControllerData[SECOND_EDITOR_PAD].button & U_CBUTTONS) {
        f_CUpPickedHeight = GetGroundPoint(f_HeightCursorColumn, f_HeightCursorRow)->v.ob[AXIS_Y];
    }

    if (g_MenuControllerData[SECOND_EDITOR_PAD].button & R_CBUTTONS) {
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

    if (g_MenuControllerData[SECOND_EDITOR_PAD].button & D_CBUTTONS) {
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

    if (g_MenuControllerData[OPTION_PAD].button & A_BUTTON) {
        if (f_ShowsGroundWaves) {
            f_ShowsGroundWaves = FALSE;
        }
        else {
            f_ShowsGroundWaves = TRUE;
        }
    }

    if (g_MenuControllerData[OPTION_PAD].button & B_BUTTON) {
        PutObjectsOnGround();
    }

    if (g_ControllerData[EDITOR_PAD].button & U_CBUTTONS) {
        if (g_ControllerData[EDITOR_PAD].button & Z_TRIG) {
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

    if (g_ControllerData[EDITOR_PAD].button & D_CBUTTONS) {
        if (g_ControllerData[EDITOR_PAD].button & Z_TRIG) {
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

    if (g_ControllerData[EDITOR_PAD].button & R_CBUTTONS) {
        SetGroundPointHeightAndShade(
            f_HeightCursorColumn,
            f_HeightCursorRow,
            f_LastSetHeight,
            EDITED_POINT_SHADE
        );
        editKind = HEIGHT_EDIT_POINT;
        MeasureGroundTiles();
    }

    if (g_ControllerData[EDITOR_PAD].button & L_CBUTTONS) {
        SetGroundPointHeightAndShade(
            f_HeightCursorColumn,
            f_HeightCursorRow,
            f_LastSetHeight + (rand() & COARSE_NOISE_MASK) - COARSE_NOISE_OFFSET,
            EDITED_POINT_SHADE
        );
        editKind = HEIGHT_EDIT_POINT;
        MeasureGroundTiles();
    }

    if (g_MenuControllerData[EDITOR_PAD].button & A_BUTTON) {
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

    if (g_MenuControllerData[EDITOR_PAD].button & B_BUTTON) {
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

    if (g_MenuControllerData[SECOND_EDITOR_PAD].button & B_BUTTON) {
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

    if ((g_MenuControllerData[EDITOR_PAD].button & R_JPAD) &&
        !(g_ControllerData[EDITOR_PAD].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        direction = CURSOR_DIRECTION_RIGHT;
    }

    if ((g_MenuControllerData[EDITOR_PAD].button & L_JPAD) &&
        !(g_ControllerData[EDITOR_PAD].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        direction = CURSOR_DIRECTION_LEFT;
    }

    if ((g_MenuControllerData[EDITOR_PAD].button & D_JPAD) &&
        !(g_ControllerData[EDITOR_PAD].button & Z_TRIG)) {
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

    if ((g_MenuControllerData[EDITOR_PAD].button & U_JPAD) &&
        !(g_ControllerData[EDITOR_PAD].button & Z_TRIG)) {
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

    if ((g_MenuControllerData[SECOND_EDITOR_PAD].button & R_JPAD) &&
        !(g_ControllerData[SECOND_EDITOR_PAD].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        direction = CURSOR_DIRECTION_RIGHT;
    }

    if ((g_MenuControllerData[SECOND_EDITOR_PAD].button & L_JPAD) &&
        !(g_ControllerData[SECOND_EDITOR_PAD].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        direction = CURSOR_DIRECTION_LEFT;
    }

    if ((g_MenuControllerData[SECOND_EDITOR_PAD].button & D_JPAD) &&
        !(g_ControllerData[SECOND_EDITOR_PAD].button & Z_TRIG)) {
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

    if ((g_MenuControllerData[SECOND_EDITOR_PAD].button & U_JPAD) &&
        !(g_ControllerData[SECOND_EDITOR_PAD].button & Z_TRIG)) {
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
        columnStep = cos(((direction * BINARY_ANGLE_EIGHTH_TURN) + f_HeightCameraHeadingAngle) *
                         EDITOR_ANGLE_TO_RADIANS) *
            CURSOR_STEP_LENGTH;
        rowStep = sin(((direction * BINARY_ANGLE_EIGHTH_TURN) + f_HeightCameraHeadingAngle) *
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

    if ((g_ControllerData[EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[EDITOR_PAD].button & L_TRIG) ||
        (g_ControllerData[EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[EDITOR_PAD].button & Z_TRIG) {
        f_HeightCameraHeadingAngle -=
            g_MenuControllerData[EDITOR_PAD].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_HeightCameraPitch -=
            g_MenuControllerData[EDITOR_PAD].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }

    if ((g_ControllerData[SECOND_EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[SECOND_EDITOR_PAD].button & L_TRIG) ||
        (g_ControllerData[SECOND_EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[SECOND_EDITOR_PAD].button & Z_TRIG) {
        f_HeightCameraHeadingAngle -=
            g_MenuControllerData[SECOND_EDITOR_PAD].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_HeightCameraPitch -=
            g_MenuControllerData[SECOND_EDITOR_PAD].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }

    if (f_HeightCameraPitch > 0.0f) {
        f_HeightCameraPitch = 0.0f;
    }

    if (f_HeightCameraPitch < EDITOR_PITCH_MIN) {
        f_HeightCameraPitch = EDITOR_PITCH_MIN;
    }

    f_HeightCameraHeading = f_HeightCameraHeadingAngle * EDITOR_ANGLE_TO_RADIANS;
    if (f_LastHeightEditKind && editKind == HEIGHT_EDIT_NONE) {
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
    g_MapCamera.view.eye[AXIS_X] = (cos(f_HeightCameraPitch) * cos(f_HeightCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = (cos(f_HeightCameraPitch) * sin(f_HeightCameraHeading) *
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

static const char f_EditorAssertionText[] = "EX";
static const char f_EditorFileName[] = "c:/usgolf64/hiedit.c";

/*
 * Gathers the terrain codes on each side of the two diagonals of the
 * rectangle from code column and row (column, row) to (endColumn,
 * endRow), keeping up to two different codes a side and skipping cells
 * within DIAGONAL_MIN_DISTANCE of the diagonal. Sides 0 and 1 are those
 * of the diagonal through offsets (0, 0) and (width, height), and sides 2
 * and 3 those of the one through (0, height) and (width, 0). Returns
 * whether sides 0 and 1 both have two codes while side 2 or 3 does not.
 */
s32 ShouldMirrorCell(s32 column, s32 row, s32 endColumn, s32 endRow) {
    s32 sideCodes[DIAGONAL_SIDE_COUNT][SIDE_CODE_COUNT];
    s32 width;
    s32 height;
    s32 i;
    s32 rowOffset;
    s32 columnOffset;
    s32 code;
    f32 inverseLength;
    f32 distance;

    void AddSideCode(s32 * codes, s32 code) {
        s32 i;

        for (i = 0; i < SIDE_CODE_COUNT; i++) {
            if (codes[i] == code) {
                return;
            }

            if (codes[i] == NO_TERRAIN_CODE) {
                codes[i] = code;
                return;
            }
        }
    }

    width = endColumn - column;
    height = endRow - row;
    AssertInEditor(width > 0 && height > 0, HOLE_EDITOR_DIAGONAL_ASSERT_LINE);
    for (i = 0; i < DIAGONAL_SIDE_COUNT; i++) {
        sideCodes[i][0] = NO_TERRAIN_CODE;
        sideCodes[i][1] = NO_TERRAIN_CODE;
    }

    inverseLength = 1.0f / sqrtf((width * width) + (height * height));
    for (rowOffset = 0; rowOffset < height; rowOffset++) {
        for (columnOffset = 0; columnOffset < width; columnOffset++) {
            code = GetTerrainCode(column + columnOffset, row + rowOffset);
            distance = ((width * (rowOffset - height)) + (columnOffset * height)) * inverseLength;
            if (AbsFloat(distance) >= DIAGONAL_MIN_DISTANCE) {
                if (distance >= 0.0f) {
                    AddSideCode(sideCodes[2], code);
                }
                else {
                    AddSideCode(sideCodes[3], code);
                }
            }

            distance = ((-height * columnOffset) + (rowOffset * width)) * inverseLength;
            if (AbsFloat(distance) >= DIAGONAL_MIN_DISTANCE) {
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

/* Whether a ground point is off the ground tiles */
#define IsOffGroundPoints(column, row) \
    ((column) < 0 || \
        (column) >= GROUND_TILE_COLUMN_COUNT * GROUND_TILE_POINT_SPAN || \
        (row) >= GROUND_TILE_ROW_COUNT * GROUND_TILE_POINT_SPAN || \
        (row) < 0)

/*
 * Toggles whether one cell of the ground is drawn mirrored. The cells are
 * GROUND_TILE_POINT_SPAN a side in each tile.
 */
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

/* Sets the x and s of a column of points between grid vertices (column, row)
 * and (column + 1, row) */
void InterpolateColumnPoints(Vtx *vertices, s32 column, s32 row, s32 weight) {
    Vtx *ends[2];
    s32 firstWeight;
    s32 i;

    firstWeight = EDGE_WEIGHT_MAX - weight;
    ends[0] = GetGridVertex(column, row);
    ends[1] = GetGridVertex(column + 1, row);
    for (i = 0; i < EDGE_POINT_COUNT; i++) {
        Vtx *vertex = &vertices[f_TileMidpointSlots[i][((column & 1) * 2) + 1]];

        vertex->v.ob[AXIS_X] =
            ((ends[0]->v.ob[AXIS_X] * firstWeight) + (ends[1]->v.ob[AXIS_X] * weight)) /
            EDGE_WEIGHT_MAX;
        vertex->v.tc[TEXTURE_AXIS_S] = ((ends[0]->v.tc[TEXTURE_AXIS_S] * firstWeight) +
                                           (ends[1]->v.tc[TEXTURE_AXIS_S] * weight)) /
            EDGE_WEIGHT_MAX;
    }
}

/* Sets the z and t of a row of points between grid vertices (column, row) and
 * (column, row + 1) */
void InterpolateRowPoints(Vtx *vertices, s32 column, s32 row, s32 weight) {
    Vtx *ends[2];
    s32 firstWeight;
    s32 i;

    firstWeight = EDGE_WEIGHT_MAX - weight;
    ends[0] = GetGridVertex(column, row);
    ends[1] = GetGridVertex(column, row + 1);
    for (i = 0; i < EDGE_POINT_COUNT; i++) {
        Vtx *vertex = &vertices[f_TileMidpointSlots[((row & 1) * 2) + 1][i]];

        vertex->v.ob[AXIS_Z] =
            ((ends[0]->v.ob[AXIS_Z] * firstWeight) + (ends[1]->v.ob[AXIS_Z] * weight)) /
            EDGE_WEIGHT_MAX;
        vertex->v.tc[TEXTURE_AXIS_T] = ((ends[0]->v.tc[TEXTURE_AXIS_T] * firstWeight) +
                                           (ends[1]->v.tc[TEXTURE_AXIS_T] * weight)) /
            EDGE_WEIGHT_MAX;
    }
}

/*
 * Shapes the ground from the terrain codes and plants trees. Each ground
 * point's y follows its distance from the middle of the green, with
 * noise; bunker and rough points move further, more so far inside them.
 * The tile midpoints are then put back on the lines between their
 * neighbors, and trees go in the rough TREE_MIN_DISTANCE to
 * TREE_MAX_DISTANCE code cells from its edge. Nothing calls it.
 */
void ShapeGroundFromCodes(void) {
    Vtx *edgePoints[3];
    s32 treePositions[BILLBOARD_COUNT][2];
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
    s32 code;
    s32 neighborCode;
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

    /* row and column count tiles here */
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
                        edgePoints[0] = GetGroundPoint(
                            (column * GROUND_TILE_POINT_SPAN) + edgeColumn - 1,
                            (row * GROUND_TILE_POINT_SPAN) + edgeRow
                        );
                        edgePoints[1] = &g_WorkArea.hole.midpoints[g_WorkArea.hole.tiles[tileIndex]
                                .midpointBlockIndex][f_TileMidpointSlots[edgeRow][edgeColumn]];
                        edgePoints[2] = GetGroundPoint(
                            (column * GROUND_TILE_POINT_SPAN) + edgeColumn + 1,
                            (row * GROUND_TILE_POINT_SPAN) + edgeRow
                        );
                        edgePoints[1]->v.ob[AXIS_Y] = edgePoints[0]->v.ob[AXIS_Y] +
                            ((edgePoints[2]->v.ob[AXIS_Y] - edgePoints[0]->v.ob[AXIS_Y]) *
                                (edgePoints[1]->v.ob[AXIS_X] - edgePoints[0]->v.ob[AXIS_X]) /
                                (edgePoints[2]->v.ob[AXIS_X] - edgePoints[0]->v.ob[AXIS_X]));
                    }
                    else {
                        edgePoints[0] = GetGroundPoint(
                            (column * GROUND_TILE_POINT_SPAN) + edgeColumn,
                            (row * GROUND_TILE_POINT_SPAN) + edgeRow - 1
                        );
                        edgePoints[1] = &g_WorkArea.hole.midpoints[g_WorkArea.hole.tiles[tileIndex]
                                .midpointBlockIndex][f_TileMidpointSlots[edgeRow][edgeColumn]];
                        edgePoints[2] = GetGroundPoint(
                            (column * GROUND_TILE_POINT_SPAN) + edgeColumn,
                            (row * GROUND_TILE_POINT_SPAN) + edgeRow + 1
                        );
                        edgePoints[1]->v.ob[AXIS_Y] = edgePoints[0]->v.ob[AXIS_Y] +
                            ((edgePoints[2]->v.ob[AXIS_Y] - edgePoints[0]->v.ob[AXIS_Y]) *
                                (edgePoints[1]->v.ob[AXIS_Z] - edgePoints[0]->v.ob[AXIS_Z]) /
                                (edgePoints[2]->v.ob[AXIS_Z] - edgePoints[0]->v.ob[AXIS_Z]));
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
                            TREE_MAX_DISTANCE * OFFSET_DISTANCE_SCALE) {
                        break;
                    }
                }

                if (offsets[offsetIndex].distance >= TREE_MIN_DISTANCE * OFFSET_DISTANCE_SCALE &&
                    offsets[offsetIndex].distance < TREE_MAX_DISTANCE * OFFSET_DISTANCE_SCALE) {
                    x = column * VERTEX_UNITS_PER_CODE;
                    z = row * VERTEX_UNITS_PER_CODE;
                    for (treeIndex = 0; treeIndex < treeCount; treeIndex++) {
                        if (IntLength2D(
                                x - treePositions[treeIndex][0],
                                z - treePositions[treeIndex][1]
                            ) < TREE_SPACING) {
                            break;
                        }
                    }

                    if (treeIndex == treeCount) {
                        treePositions[treeCount][0] = x;
                        treePositions[treeCount][1] = z;
                        treeCount++;

                        /* The limit is checked at the end of each block, to match: one
                         * check here alone gives 178 words differing, 28 in shape, the
                         * limit in the row and column loops' conditions 193, 37 in
                         * shape, and every check but this one 121, 1 in shape */
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
                 treePositions[billboardIndex][0] << WORLD_TO_COURSE_SHIFT,
                 treePositions[billboardIndex][1] << WORLD_TO_COURSE_SHIFT
             ) / (1 << WORLD_TO_COURSE_SHIFT)) +
            1;
        g_Billboards[billboardIndex].x = treePositions[billboardIndex][0];
        g_Billboards[billboardIndex].z = treePositions[billboardIndex][1];
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

/*
 * Sets whether each cell of each ground tile is drawn mirrored, to
 * ShouldMirrorCell of the codes between the cell's opposite corners
 */
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

/*
 * Returns -1 if either triangle of any of the four ground cells around a
 * ground point has a cross product below MIN_CELL_CROSS_PRODUCT, so is
 * too thin or turned over, or 0 if none does
 */
s32 HasDeadTriangleNearPoint(s32 column, s32 row) {
    Vtx *corners[4];
    s32 rowOffset;
    s32 columnOffset;

    rowOffset = -1;
    do {
        columnOffset = -1;
        do {
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
                ) < MIN_CELL_CROSS_PRODUCT ||
                CrossProductXZ(
                    corners[1]->v.ob[AXIS_X],
                    corners[1]->v.ob[AXIS_Z],
                    corners[3]->v.ob[AXIS_X],
                    corners[3]->v.ob[AXIS_Z],
                    corners[2]->v.ob[AXIS_X],
                    corners[2]->v.ob[AXIS_Z]
                ) < MIN_CELL_CROSS_PRODUCT) {
                return -1;
            }

            columnOffset++;
        }
        while (columnOffset <= 0);
        rowOffset++;
    }
    while (rowOffset <= 0);
    return 0;
}

/*
 * Runs SCENE_HOLE_FILES's menu, which loads a hole with BuildHole or saves the
 * one in memory with SaveHoleFiles. The D-pad picks the action, then the
 * course and the hole; A goes on to the next step and B goes back.
 */
void UpdateHoleFileMenu(void) {
    char courseText[DEBUG_TEXT_BUFFER_SIZE];
    char holeText[DEBUG_TEXT_BUFFER_SIZE];

    ClearDebugText();
    if (f_HoleFileStep == HOLE_FILE_STEP_ACTION) {
        if (g_MenuControllerData[0].button & (U_JPAD | D_JPAD)) {
            f_IsSavingHoleFiles ^= 1;
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

/*
 * Scene SCENE_TREES's editor for the billboards that stand for a hole's
 * trees. The editor pad's C-up and C-down change the edited tree's height,
 * C-right and C-left its width, A toggles isIgnoredEditorMark, up and down on
 * the control pad its texture, and the stick moves it. Right and left on the
 * control pad pick the tree, and B copies it. R zooms in, L or Z with R zooms
 * out, and Z with the stick orbits the camera around the tree.
 */
void UpdateTreeEditor(void) {
    Mat4 translation;
    /*
     * The game's frame has room for a second matrix; without it 28 words differ
     */
    Mat4 unusedMatrix UNUSED;
    s32 editKind;
    s32 i;
    s32 height;
    f64 cameraDistance; /* As a literal, GCC would fold it into the angles before
                           the zoom */

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
    if (g_ControllerData[EDITOR_PAD].button & U_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[f_EditedTreeIndex].height++;
    }

    if (g_ControllerData[EDITOR_PAD].button & D_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[f_EditedTreeIndex].height--;
    }

    if (g_ControllerData[EDITOR_PAD].button & R_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[f_EditedTreeIndex].width++;
    }

    if (g_ControllerData[EDITOR_PAD].button & L_CBUTTONS) {
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

    if (g_MenuControllerData[EDITOR_PAD].button & A_BUTTON) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[f_EditedTreeIndex].isIgnoredEditorMark =
            !g_Billboards[f_EditedTreeIndex].isIgnoredEditorMark;
    }

    if ((g_MenuControllerData[EDITOR_PAD].button & R_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        g_Billboards[f_EditedTreeIndex].isNearCourseEdge = FALSE;
        f_EditedTreeIndex++;
    }

    if ((g_MenuControllerData[EDITOR_PAD].button & L_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
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

    if ((g_MenuControllerData[EDITOR_PAD].button & U_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_KIND;
        g_Billboards[f_EditedTreeIndex].textureIndex++;
    }

    if ((g_MenuControllerData[EDITOR_PAD].button & D_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX |
            g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_KIND;
        g_Billboards[f_EditedTreeIndex].textureIndex--;
    }

    if (g_Billboards[f_EditedTreeIndex].textureIndex >= BILLBOARD_TEXTURE_COUNT) {
        g_Billboards[f_EditedTreeIndex].textureIndex = 0;
    }

    if (g_Billboards[f_EditedTreeIndex].textureIndex < 0) {
        g_Billboards[f_EditedTreeIndex].textureIndex = BILLBOARD_TEXTURE_COUNT - 1;
    }

    /* B copies the tree into the first free billboard and edits the copy */
    i = 0;
    if (g_MenuControllerData[EDITOR_PAD].button & B_BUTTON) {
        for (; i < BILLBOARD_COUNT; i++) {
            if (IsFreeBillboard(g_Billboards[i])) {
                break;
            }
        }

        if (i != BILLBOARD_COUNT) {
            g_Billboards[i] = g_Billboards[f_EditedTreeIndex];
            g_Billboards[i].x += COPIED_TREE_OFFSET;
            editKind = TRIANGLE_EDIT_SELECT;
            g_Billboards[f_EditedTreeIndex].isNearCourseEdge = FALSE;
            f_EditedTreeIndex = i;
        }
    }

    /* The u16 holds stick_x and stick_y */
    if (!(g_ControllerData[EDITOR_PAD].button & Z_TRIG) &&
        *(u16 *)&g_MenuControllerData[EDITOR_PAD].stick_x != 0) {
        s32 moveX = (-cos((u16)f_TreeCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) *
                        g_MenuControllerData[EDITOR_PAD].stick_y) -
            (sin((u16)f_TreeCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) *
                g_MenuControllerData[EDITOR_PAD].stick_x);
        s32 moveZ = (-sin((u16)f_TreeCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) *
                        g_MenuControllerData[EDITOR_PAD].stick_y) +
            (cos((u16)f_TreeCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) *
                g_MenuControllerData[EDITOR_PAD].stick_x);

        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[f_EditedTreeIndex].x += moveX;
        g_Billboards[f_EditedTreeIndex].z += moveZ;
    }

    /* The translation goes unused; PlaceBillboards places every tree again */
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
        /* Blinks the tree, as drawing skips billboards near the course edge */
        g_Billboards[f_EditedTreeIndex].isNearCourseEdge =
            !g_Billboards[f_EditedTreeIndex].isNearCourseEdge;
    }

    if ((g_ControllerData[EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == R_TRIG) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_IN_FACTOR;
        if (g_MapCamera.view.zoom >= EDITOR_ZOOM_MAX) {
            g_MapCamera.view.zoom = EDITOR_ZOOM_MAX;
        }
    }

    if ((g_ControllerData[EDITOR_PAD].button & L_TRIG) ||
        (g_ControllerData[EDITOR_PAD].button & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        g_MapCamera.view.zoom *= CURSOR_ZOOM_OUT_FACTOR;
        if (g_MapCamera.view.zoom < CURSOR_ZOOM_MIN) {
            g_MapCamera.view.zoom = CURSOR_ZOOM_MIN;
        }
    }

    if (g_ControllerData[EDITOR_PAD].button & Z_TRIG) {
        f_TreeCameraHeadingAngle -=
            g_MenuControllerData[EDITOR_PAD].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        f_TreeCameraPitch -=
            g_MenuControllerData[EDITOR_PAD].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
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
    f_TreeCameraHeading = f_TreeCameraHeadingAngle * EDITOR_ANGLE_TO_RADIANS;
    g_MapCamera.view.eye[AXIS_X] = (cos(f_TreeCameraPitch) * cos(f_TreeCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = (cos(f_TreeCameraPitch) * sin(f_TreeCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] =
        (sin(f_TreeCameraPitch) * (g_MapCamera.view.zoom * cameraDistance)) +
        g_MapCamera.view.lookAt[AXIS_Y];
}

/*
 * Blends the ground points within a radius of a ground point toward its
 * height: the farther a point, the more it keeps its own height, fully
 * from BLEND_DISTANCE_SCALE / falloff away. Nothing calls it.
 */
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
            /*
             * halfWidth holds its square until the root, to match: a
             * halfWidthSquared local, wherever it is declared, gives 46 words
             * differing (0 in shape)
             */
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

/*
 * Fills f_TriangleSlopes from the ground. It goes over the cells in blocks
 * of 2 columns by 4 rows; each cell's two triangles take an entry each.
 */
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

/*
 * Fills f_WaveTexels with the cube of cos(column * pi/4) * sin(row * pi/4),
 * as matching IA8 intensity and alpha levels from 1 to 15
 */
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

/*
 * Writes a hex dump of words to the host as a text file: lines of four
 * words, each line led by its byte offset. Nothing calls it.
 */
void WriteHexDumpFile(u32 *words, s32 byteCount, char *fileName) {
    char text[HEX_DUMP_TEXT_SIZE];
    char *cursor;
    char *end;
    char *buffer;
    s32 line;
    s32 wordIndex;
    u32 *lineWords;

    /* Copies text to the buffer, dropping what doesn't fit */
    void AppendText(char *source) {
        char character;

        for (;;) {
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

/* The first vertex column of one of the ground's halves */
#define HalfFirstColumn(half) ((half) * GROUND_HALF_VERTEX_COLUMNS)

/*
 * The hole editor's copy of DrawGround, which DrawGround hands off to while
 * GAME_FLAG_READ_HOST_HOLE is set. It draws into a display list of its own,
 * which the frame's list calls. Unless GAME_FLAG_HOLE_LOADED is set, it builds
 * fine tiles from their grid vertices and midpoints itself, and when
 * g_ShowsGroundWaves is set it blends in a scrolling wave texture.
 *
 * GetMidpointVertex and SetEditorCombineMode are nested functions, a GNU C
 * extension.
 *
 * Doesn't match: the frame is the original's size, but the registers and
 * the spill slots differ; see the last paragraph. Stepping the half's first
 * column beside half gives 2403 words differing, 581 in shape. Giving the
 * fine tiles' loop its own counter and setting the middle slot after the
 * wave's tile took it from 2258 words differing, 518 in shape. In the
 * medium tiles' pass loop, loop.c hoists 0xF5100000 in the original, as it
 * does in the coarse tiles' loop, but finds it not worth moving in ours
 * (savings 3, the loop 308 insns against 259); with that constant also live
 * across the calls, the original's two SetEditorCombineMode calls restore
 * it in different orders, where ours have the same tail and cross-jumping
 * merged them, so ours had 11 calls to the original's 12 until the tile S
 * form below. A local for the half's first tile column, which brought
 * DrawGround's shape from 348 to 283, gives
 * 3683 words differing here (505 in shape) when declared after half, and
 * 3709 (505) when declared first. A permuter output with 2050 words (472
 * in shape) held TILE_COORDINATE_SPAN in a variable set before the tiles
 * and multiplied by it in seven of the eight tile coordinates the texture
 * loads work out, leaving the first expansion's S as the constant;
 * through such a local in all of them, 2236 words differed (474 in
 * shape), and in the rows' coordinates only, 2231 (483). The register
 * oracle, forcing 113 pseudos' registers, left 1763 words at best, so the
 * code differs too. Counting the tile column in tileColumn, set before a
 * tile loop and stepped with tileIndex, as DrawGround's fine tiles do,
 * gave 2158 words (480 in shape) in the medium tiles' loop, 2534 (1216)
 * in the coarse tiles', 2268 (1226) in both, 2985 (499) in the fine
 * tiles' and 3002 (1232) in all three, when the draft gave 2128 (488).
 * The mirrored cells' S line is worked out where the vertex change uses
 * it, in a local declared there, rather than once per fine tile beside
 * tileColumn, which took the draft from 2128 words differing to 2037, 488
 * in shape to 491; written straight into the vertex change it gives the
 * same. A u32 groupDetail gives 2026 words (488 in shape), but it would
 * rank a hidden tile's -1 above every detail level when comparing it with
 * the s32 field; it compiles 27 signed and 16 unsigned compares and 4
 * blez where the original, like the s32, has 29, 14 and 5. The medium
 * groups step their slots middle, bottom, tile index and top, and rewind
 * them middle, bottom and top: stepping them top, middle, bottom and tile
 * index, as DrawGround does, gives 2055 words (491 in shape), also with a
 * top, middle and bottom rewind; that rewind alone gives 2074 (494) and
 * DrawGround's top, bottom and middle 2086 (494). 2037 words differ, 491
 * in shape.
 *
 * A search over merges of two variables gains words only by giving one a
 * second meaning: holding previousDetail in keepsCombineMode gives 2015
 * words, but 515 in shape.
 *
 * Each mirrored-cell vertex loop declares its own st beside its point, as
 * the register oracle's split search suggested, where one st at the top
 * served both: that takes the draft from 2037 words to 2033 (491 in shape
 * either way), and the oracle's closest result from 2001 words to 1995,
 * forcing 112 pseudos for each. Declaring it in only one of the two loops
 * gives the same 2033 words.
 *
 * Each mirrored-cell block also declares its own pointRow and pointColumn,
 * which only those loops use. That takes the draft from 2033 words differing
 * to 2009, 491 in shape either way, and the oracle's closest result from
 * 1995 words, forcing 112 pseudos, to 1975, forcing 110. A pointColumn of
 * their own alone gives 2025, in either block or both.
 *
 * The fine tiles' loop declares its own groupTile, and its body its own
 * tileColumn, which nothing else uses. Together they take the draft from
 * 2009 words differing to 2007, 491 in shape either way, and leave the
 * oracle's closest result at 1975 words, forcing 110 pseudos; tileColumn
 * alone gives 2013 and groupTile alone 2015.
 *
 * A map tile's checker color names its red and blue, set before the color
 * command, where both were written into it. That takes the draft from 2007
 * words differing, 491 in shape, to 1993 (489), and the oracle's closest
 * result from 1975 words, forcing 110 pseudos, to 1968, forcing 95. Naming
 * the blue alone gives the same 1993 words, and the red alone leaves 2007;
 * choosing each with ?: gives 2520 (612).
 *
 * As in DrawGround, each tile loop in the original steps the tile's left S
 * and its place's end by 0x80, with the half's first S, half << 9, in a
 * stack slot. So LoadEditorTileTexture adds the tile's place in its group to
 * firstTileS, and takes the right S and the bottom T past the next tile's
 * start. Written straight into the tile size commands, that takes the draft
 * from 1993 words differing (489 in shape) to 1791 (233), with the
 * original's frame and its 12 SetEditorCombineMode calls, and the register
 * oracle's closest result from 1968 words, forcing 95 pseudos, to 1359,
 * forcing 26. Through locals for the S and T set at the top of the macro,
 * loop.c makes no giv of them, and the draft gives 2425 words (579 in
 * shape); taking only the bottom T as the row's plus 0x82 then gives 2471
 * (337), and only a borderless tile's as its own plus 0x84, 2503 (325).
 * Setting firstTileS from half gives the same 1791 words. The original
 * still loads 0xF5100000 6 times, where ours does 18 times.
 *
 * As in DrawGround, the original steps the tile index and its givs at the end
 * of a fine tile drawn from its display list, and goes from there to the next
 * tile, so that arm steps tileIndex itself and continues. That takes the draft
 * from 1791 words differing (233 in shape) to 1751 (187), and the register
 * oracle's closest result from 1359 words to 1345, forcing 26 pseudos either
 * way. The original stores each snapped edge's segment once, its address
 * chosen as DrawGround's ?: chooses it, but that form gives 2017 words here
 * (189 in shape), and the oracle's closest 1924.
 *
 * The original lays out the hidden groups' arm last, so the coarse groups
 * are tested for, with >=, before the hidden ones fall to the last else, as
 * in DrawGround. That takes the draft from 1751 words differing (187 in
 * shape) to 1665 (165), and the oracle's closest from 1345 words to 1189,
 * forcing 26 pseudos either way; testing for them with == gives 1667 (167).
 * Setting each half's bottom slot, tile index, top slot and previous detail
 * in that order, as the original's stores go, gives 1661 words (159 in
 * shape) but leaves the oracle's closest at 1189, and breaks the order
 * DrawGround's copy sets them in. Holding terrainStyle != TERRAIN_STYLE_NORMAL
 * in a flag set after each fine pass's texture load, as a permuter run did,
 * gives 1209 words (141 in shape), but it compiles to an sltu that the
 * original doesn't have: the original reloads terrainStyle and branches on it
 * at each test. Set anywhere else, once or per half, row, tile or pass, the
 * flag gives 1773 to 2312 words.
 *
 * A mirrored point under the test texture works out its texture
 * coordinates before setting the test tile's size, as a permuter run
 * found. That takes the draft from 1665 words differing (165 in shape) to
 * 921 (133), and the register oracle's closest result from 1189 words,
 * forcing 26 pseudos, to these 921 with none forced, so allocation is no
 * longer the cause. The original's scheduler interleaves the two, so its
 * order can't be read from the code.
 *
 * The original keeps no tile column: each edge test reloads half and
 * groupTile and works out half * 4 + groupTile again, and it works out the
 * mirrored S line once per fine tile, from the left S's giv, into a slot of
 * its own. Writing the column out at each use, as a GroupTileColumn macro,
 * gives 1701 words (125 in shape) with the frame 16 bytes smaller, and with
 * the mirrored S line set at the top of each fine tile from firstTileS,
 * 1540 (112) with the frame 8 bytes smaller; the register oracle's closest
 * is then 1432, forcing 21 pseudos. In both, loop.c makes the column a
 * giv of its own, stepped in a slot, where the original recomputes it.
 * Setting the line from the column instead gives 1542 (114), and from it
 * with tileColumn kept, 2265 (147).
 *
 * A permuter run found 795 words, but 193 in shape, by naming the bottom row
 * test in a Boolean set before the hole-loaded test and using it only for
 * the bottom midpoints. Using it in both of the bottom row's tests gives 1829
 * (245), the top and bottom rows' Booleans in all four row tests 1724 (250),
 * and all four edges' Booleans, the columns' too, 986 (348). 921 words
 * differ, 133 in shape.
 *
 * Each half starts its middle slot after the first tile's S, before the
 * wave's tile, as DrawGround now does; after the wave's tile it was 921
 * words (133 in shape), and this gives 917 (131). Set before the first
 * tile's S, it gives 917 (131) too. The bottom slot is set after the
 * tile index, as in DrawGround; before the top slot, it gave those 917
 * words, and this gives 905 (123). That left 905 words, 123 in shape.
 *
 * The fine tiles' column and their checkerboard parity add the row and
 * the half's first column to groupTile, where the draft added groupTile
 * to them; a search regrouping sums found it. That takes the draft from
 * 905 words differing to 899, 123 in shape both, and the register
 * oracle's closest from 905 words to 899, with nothing forced; the
 * column alone gives 901, the parity alone 903. 899 words differ, 123 in
 * shape.
 */
#ifdef NON_MATCHING
void DrawHoleEditorGround(CameraMatrices *matrices UNUSED, s32 terrainStyle) {
    u8 snapsEdge[2][TILE_VERTEX_SEGMENT_COUNT]; /* For the current and the last
                                                   fine tile */
    s32 keepsCombineMode;
    s32 combineMode; /* An EDITOR_COMBINE_* */
    Gfx *gfx;
    u8 current;
    Vtx *vertices;
    u32 start;
    s32 visibility;
    s32 detail;
    s32 groupDetail;
    s32 previousDetail;
    s32 slopeIndex;
    s32 half;
    s32 row;
    s32 detailTileIndex;
    s32 groupTileIndex;
    s32 j;
    s32 segmentIndex;
    s32 edge;
    s32 pass;
    s32 tileIndex;
    /*
     * The fine tiles' own pass counter, to match: using pass gives 2332 words
     * differing (484 in shape) against the draft's 2037 (491), and the register
     * oracle's closest result rises from 2001 words, forcing 112 pseudos, to
     * the unforced 2332
     */
    s32 finePass;
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
    s32 k;

    /* Sets a vertex halfway between two others */
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

    /*
     * Picks how a tile's texture and the overlay in TMEM combine, for a pass
     * of a tile drawn once or twice. It writes nothing when the mode is
     * already set, or when the map's white overlay keeps its own mode.
     */
    void SetEditorCombineMode(s32 tilePass, s32 overlay) {
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

    current = 0;
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
            tile->center[0],
            tile->center[1],
            tile->center[2],
            MIN_GROUND_TILE_RADIUS
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

        /* The map scenes draw every visible tile finely */
        if (IsGroundEditorScene(g_Scene) &&
            IsMapTerrainStyle(terrainStyle) &&
            detail != GROUND_DETAIL_HIDDEN) {
            detail = GROUND_DETAIL_FINE;
        }

        tile->detail = detail;
        if (detail >= 0) {
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

    /* The waves move down each triangle's slope */
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
        s32 topSlot;
        s32 firstTileS;

        previousDetail = GROUND_DETAIL_HIDDEN;
        topSlot = FIRST_TOP_SLOT;
        tileIndex = half * GROUND_TILE_GROUP_SIZE;
        bottomSlot = FIRST_BOTTOM_SLOT;
        firstTileS = tileIndex * TILE_COORDINATE_SPAN;
        middleSlot = FIRST_MIDDLE_SLOT;
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
            groupDetail = GROUND_DETAIL_COARSE;
            for (detailTileIndex = 0; detailTileIndex < GROUND_TILE_GROUP_SIZE; detailTileIndex++) {
                if (groupDetail < g_WorkArea.hole.tiles[tileIndex + detailTileIndex].detail) {
                    groupDetail = g_WorkArea.hole.tiles[tileIndex + detailTileIndex].detail;
                }
            }

            if (groupDetail >= GROUND_DETAIL_FINE) {
                s32 groupTile;

                for (groupTile = 0; groupTile < GROUND_TILE_GROUP_SIZE; groupTile++) {
                    s32 tileColumn;

                    midpoints = g_WorkArea.hole
                                    .midpoints[g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex];
                    tileColumn = groupTile + (half * GROUND_TILE_GROUP_SIZE);

                    /* The maps tell each tile from its neighbors by color */
                    if (IsMapTerrainStyle(terrainStyle)) {
                        Boolean isOdd = (groupTile + row) & 1;
                        s32 red = isOdd * COLOR_CHANNEL_MAX;
                        s32 blue = COLOR_CHANNEL_MAX - red;

                        gDPSetEnvColor(gfx++, red, 0, blue, OPAQUE_ALPHA);
                    }

                    if (g_WorkArea.hole.tiles[tileIndex].detail == GROUND_DETAIL_FINE &&
                        g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex >= 0) {
                        if (IsGameFlagSet(GAME_FLAG_HOLE_LOADED)) {
                            if (tileColumn != 0) {
                                snapsEdge[current][TILE_EDGE_COUNT + TILE_EDGE_LEFT] =
                                    IsCoarse(tileIndex - 1);
                                snapsEdge[current][TILE_EDGE_LEFT] = IsNotFine(tileIndex - 1);
                            }
                            else {
                                snapsEdge[current][TILE_EDGE_COUNT + TILE_EDGE_LEFT] = FALSE;
                                snapsEdge[current][TILE_EDGE_LEFT] = FALSE;
                            }

                            if (row != 0) {
                                snapsEdge[current][TILE_EDGE_COUNT + TILE_EDGE_TOP] =
                                    IsCoarse(tileIndex - GROUND_TILE_COLUMN_COUNT);
                                snapsEdge[current][TILE_EDGE_TOP] =
                                    IsNotFine(tileIndex - GROUND_TILE_COLUMN_COUNT);
                            }
                            else {
                                snapsEdge[current][TILE_EDGE_COUNT + TILE_EDGE_TOP] = FALSE;
                                snapsEdge[current][TILE_EDGE_TOP] = FALSE;
                            }

                            if (row != GROUND_TILE_ROW_COUNT - 1) {
                                snapsEdge[current][TILE_EDGE_COUNT + TILE_EDGE_BOTTOM] =
                                    IsCoarse(tileIndex + GROUND_TILE_COLUMN_COUNT);
                                snapsEdge[current][TILE_EDGE_BOTTOM] =
                                    IsNotFine(tileIndex + GROUND_TILE_COLUMN_COUNT);
                            }
                            else {
                                snapsEdge[current][TILE_EDGE_COUNT + TILE_EDGE_BOTTOM] = FALSE;
                                snapsEdge[current][TILE_EDGE_BOTTOM] = FALSE;
                            }

                            if (tileColumn != GROUND_TILE_COLUMN_COUNT - 1) {
                                snapsEdge[current][TILE_EDGE_COUNT + TILE_EDGE_RIGHT] =
                                    IsCoarse(tileIndex + 1);
                                snapsEdge[current][TILE_EDGE_RIGHT] = IsNotFine(tileIndex + 1);
                            }
                            else {
                                snapsEdge[current][TILE_EDGE_COUNT + TILE_EDGE_RIGHT] = FALSE;
                                snapsEdge[current][TILE_EDGE_RIGHT] = FALSE;
                            }
                            for (j = 0; j < TILE_VERTEX_SEGMENT_COUNT; j++) {
                                /* Moves a snapped edge's vertices, or moves back those snapped
                                 * for the last tile */
                                if (snapsEdge[current][j] || snapsEdge[current ^ 1][j]) {
                                    /* Indexing the tile's offsets as a pointer is closer to the
                                     * original than indexing the array */
                                    if (snapsEdge[current][j]) {
                                        gSPSegment(
                                            gfx++,
                                            j + 1,
                                            ((s32 *)g_CoarseVertexOffsets[tileIndex])[j]
                                        );
                                    }
                                    else {
                                        gSPSegment(gfx++, j + 1, 0);
                                    }
                                }
                            }

                            current ^= 1;
                            gSPDisplayList(gfx++, g_GroundTileDisplayLists[tileIndex]);
                            tileIndex++;
                            continue;
                        }
                        else {
                            /* Builds the tile's 5 by 5 points: its grid vertices, then the
                             * edge midpoints, which line up with a medium neighbor */
                            vertexColumn = (groupTile * 2) + HalfFirstColumn(half);
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn, row * 2),
                                (f64 *)&vertices[0]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn, (row * 2) + 1),
                                (f64 *)&vertices[2]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn, (row * 2) + 2),
                                (f64 *)&vertices[4]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn + 1, row * 2),
                                (f64 *)&vertices[7]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn + 1, (row * 2) + 1),
                                (f64 *)&vertices[8]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn + 1, (row * 2) + 2),
                                (f64 *)&vertices[9]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn + 2, row * 2),
                                (f64 *)&vertices[12]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn + 2, (row * 2) + 1),
                                (f64 *)&vertices[14]
                            );
                            CopyDoublePair(
                                (f64 *)GridVertex(vertexColumn + 2, (row * 2) + 2),
                                (f64 *)&vertices[16]
                            );
                            if (tileColumn == 0 ||
                                g_WorkArea.hole.tiles[tileIndex - 1].detail !=
                                    GROUND_DETAIL_MEDIUM) {
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW1_COLUMN0],
                                    (f64 *)&vertices[1]
                                );
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW3_COLUMN0],
                                    (f64 *)&vertices[3]
                                );
                            }
                            else {
                                GetMidpointVertex(&vertices[0].v, &vertices[2].v, &vertices[1].v);
                                GetMidpointVertex(&vertices[2].v, &vertices[4].v, &vertices[3].v);
                            }

                            if (row == 0 ||
                                g_WorkArea.hole.tiles[tileIndex - GROUND_TILE_COLUMN_COUNT]
                                        .detail != GROUND_DETAIL_MEDIUM) {
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW0_COLUMN1],
                                    (f64 *)&vertices[5]
                                );
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW0_COLUMN3],
                                    (f64 *)&vertices[10]
                                );
                            }
                            else {
                                GetMidpointVertex(&vertices[0].v, &vertices[7].v, &vertices[5].v);
                                GetMidpointVertex(&vertices[7].v, &vertices[12].v, &vertices[10].v);
                            }

                            if (row == GROUND_TILE_ROW_COUNT - 1 ||
                                g_WorkArea.hole.tiles[tileIndex + GROUND_TILE_COLUMN_COUNT]
                                        .detail != GROUND_DETAIL_MEDIUM) {
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW4_COLUMN1],
                                    (f64 *)&vertices[6]
                                );
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW4_COLUMN3],
                                    (f64 *)&vertices[11]
                                );
                            }
                            else {
                                GetMidpointVertex(&vertices[4].v, &vertices[9].v, &vertices[6].v);
                                GetMidpointVertex(&vertices[9].v, &vertices[16].v, &vertices[11].v);
                            }

                            if (tileColumn == GROUND_TILE_COLUMN_COUNT - 1 ||
                                g_WorkArea.hole.tiles[tileIndex + 1].detail !=
                                    GROUND_DETAIL_MEDIUM) {
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW1_COLUMN4],
                                    (f64 *)&vertices[13]
                                );
                                CopyDoublePair(
                                    (f64 *)&midpoints[TILE_MIDPOINT_ROW3_COLUMN4],
                                    (f64 *)&vertices[15]
                                );
                            }
                            else {
                                GetMidpointVertex(
                                    &vertices[12].v,
                                    &vertices[14].v,
                                    &vertices[13].v
                                );
                                GetMidpointVertex(
                                    &vertices[14].v,
                                    &vertices[16].v,
                                    &vertices[15].v
                                );
                            }

                            gSPVertex(gfx++, &vertices[0], 5, 0);
                            gSPVertex(gfx++, &vertices[5], 7, 5);
                            gSPVertex(
                                gfx++,
                                &midpoints[TILE_FIRST_INNER_MIDPOINT],
                                TILE_INNER_MIDPOINT_COUNT,
                                12
                            );
                            gSPVertex(gfx++, &vertices[12], 5, 20);
                            vertices += FINE_TILE_BUILT_VERTEX_COUNT;
                            for (finePass = 0; finePass < GROUND_TILE_MAX_PASSES; finePass++) {
                                LoadEditorTileTexture(
                                    tileIndex,
                                    firstTileS,
                                    groupTile,
                                    row,
                                    finePass,
                                    g_ShowsGroundWaves
                                );

                                /* Goes over the 4 by 4 cells in blocks of 2 by 2 */
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
                                                                     .mirroredCells;
                                                    isMirrored &= 1
                                                        << (cellColumn +
                                                               (cellRow * FINE_TILE_CELL_SPAN));
                                                }
                                                else {
                                                    isMirrored = g_WorkArea.hole.tiles[tileIndex]
                                                                     .mirroredCells;
                                                    isMirrored &= 1
                                                        << (cellColumn +
                                                               (cellRow * FINE_TILE_CELL_SPAN));
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
                                                                (tileColumn * FINE_TILE_CELL_SPAN) +
                                                                    cellColumn + pointColumn,
                                                                (row * FINE_TILE_CELL_SPAN) +
                                                                    cellRow + pointRow
                                                            );
                                                            s32 st;

                                                            if (IsGameFlagSet(
                                                                    GAME_FLAG_TEST_GROUND_TEXTURE
                                                                )) {
                                                                st = ((
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
                                                                s32 mirroredS =
                                                                    ((tileColumn *
                                                                         TILE_COORDINATE_SPAN) +
                                                                        MIRROR_S_OFFSET) *
                                                                    MIRROR_S_SCALE;

                                                                st = ((
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
                                                                st
                                                            );
                                                        }
                                                    }
                                                }

                                                if ((terrainStyle != TERRAIN_STYLE_TOP_DOWN_MAP &&
                                                        terrainStyle == TERRAIN_STYLE_MAP) ||
                                                    g_ShowsGroundWaves) {
                                                    /* Each cell's two triangles are lit differently
                                                     */
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
                                                    /* The other maps show the cells in a
                                                     * checkerboard, the mirrored ones darker */
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

                                                /* Puts back the texture coordinates */
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
                                                                (tileColumn * FINE_TILE_CELL_SPAN) +
                                                                    cellColumn + pointColumn,
                                                                (row * FINE_TILE_CELL_SPAN) +
                                                                    cellRow + pointRow
                                                            );
                                                            s32 st =
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
                                                                st
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
                    else if (g_WorkArea.hole.tiles[tileIndex].detail >= 0) {
                        /* A medium tile among fine ones loads only its own vertices */
                        gSPVertex(
                            gfx++,
                            GridVertex((groupTile * 2) + HalfFirstColumn(half), row * 2),
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
                        for (pass = 0; pass < GROUND_TILE_MAX_PASSES; pass++) {
                            LoadEditorTileTexture(
                                tileIndex,
                                firstTileS,
                                groupTile,
                                row,
                                pass,
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

                /* The last row's bottom vertices are still in the cache, unless it was
                 * fine or skipped */
                if (previousDetail < 0 || previousDetail == GROUND_DETAIL_FINE) {
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
                    if (g_WorkArea.hole.tiles[tileIndex].detail >= 0) {
                        for (pass = 0; pass < GROUND_TILE_MAX_PASSES; pass++) {
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

                    middleSlot++;
                    bottomSlot++;
                    tileIndex++;
                    topSlot++;
                    slopeIndex += TILE_TRIANGLE_COUNT;
                }

                middleSlot -= GROUND_TILE_GROUP_SIZE;
                bottomSlot -= GROUND_TILE_GROUP_SIZE;
                topSlot -= GROUND_TILE_GROUP_SIZE;
            }
            else if (groupDetail >= GROUND_DETAIL_COARSE) {
                spareSlot = middleSlot;
                middleSlot = topSlot;
                topSlot = bottomSlot;
                bottomSlot = spareSlot;
                if (previousDetail < 0 || previousDetail == GROUND_DETAIL_FINE) {
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
                for (k = 0; k < GROUND_TILE_GROUP_SIZE;
                    k++, topSlot++, bottomSlot++, tileIndex++, slopeIndex += TILE_TRIANGLE_COUNT) {
                    if (g_WorkArea.hole.tiles[tileIndex].detail < 0) {
                        continue;
                    }

                    for (pass = 0; pass < GROUND_TILE_MAX_PASSES; pass++) {
                        LoadEditorTileTexture(
                            tileIndex,
                            firstTileS,
                            k,
                            row,
                            pass,
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

                topSlot -= GROUND_TILE_GROUP_SIZE;
                bottomSlot -= GROUND_TILE_GROUP_SIZE;
            }
            else {
                slopeIndex += GROUND_TILE_GROUP_SIZE * TILE_TRIANGLE_COUNT;
                tileIndex += GROUND_TILE_GROUP_SIZE;
            }

            previousDetail = groupDetail;

            /* Skips the other half's group */
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
#else
INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", GetMidpointVertex);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", SetEditorCombineMode);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", DrawHoleEditorGround);
#endif

/*
 * Marks the billboards on the map, then writes the whole map image to
 * the host as x:\usgolf64\capture\tempNN.bmp
 */
void CaptureMapImage(void) {
    char fileName[CAPTURE_FILE_NAME_SIZE];
    u8 *capture;
    s32 byteIndex;
    s32 row;
    s32 column;

    u16 GetMapTexel(u32 texelIndex) {
        u32 column = texelIndex % MAP_CAPTURE_WIDTH;
        u32 row = texelIndex / MAP_CAPTURE_WIDTH % MAP_CAPTURE_HEIGHT;

        /* The game indexes the texels past the border as one run of rows */
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

    sprintf(fileName, "x:\\usgolf64\\capture\\temp%02d.bmp", (int)f_MapCaptureNumber);
    WriteHostFile(fileName, MAP_CAPTURE_SIZE);
    FreeMemory(&capture);
    f_MapCaptureNumber++;
}

/* Writes the whole frame to the host as the next numbered capture file */
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

    sprintf(g_PrintBuffer, "x:/usgolf64/capture/cap%02d.bmp", (int)f_ScreenCaptureNumber);
    WriteHostFile(g_PrintBuffer, SCREEN_CAPTURE_SIZE);
    FreeMemory(&capture);
    f_ScreenCaptureNumber++;
}

/*
 * Writes the frame's top-left corner to the host as capture file
 * captureIndex. The size written is that of a whole-screen capture.
 */
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

    /* BMP rows run from the bottom up, and each pixel is blue, green, red */
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

/*
 * Draws a pole, red at the foot and blue at the top, at the map camera's
 * look-at point
 */
void DrawTargetPole(CameraMatrices *matrices UNUSED, Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    guTranslate(
        &f_PoleMatrix,
        g_MapCamera.view.lookAt[0],
        g_MapCamera.view.lookAt[1],
        g_MapCamera.view.lookAt[2]
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
