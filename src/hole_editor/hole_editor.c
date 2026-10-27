#include "common.h"
#include "billboard.h"
#include "camera.h"
#include "controller.h"
#include "debug_text.h"
#include "functions.h"
#include "heap.h"
#include "hole_builder.h"
#include "hole_loader.h"
#include "host_file.h"
#include "libc.h"
#include "main.h"
#include "math_util.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "sound.h"
#include "terrain.h"
#include "variables.h"
#include "work_area.h"

/* The points along a tile's edge, its corners included */
#define EDGE_POINT_COUNT (GROUND_TILE_POINT_SPAN + 1)
/* An interpolation weight runs from 0, all the first end, to this, all the second */
#define EDGE_WEIGHT_MAX 16

#define EDITOR_SOUND_CHANNEL 15
#define EDITOR_SOUND_VOLUME 127
/* WORLD_TO_COURSE_SCALE as a shift */
#define WORLD_TO_COURSE_SHIFT 10

/* func_804079B8 goes over the ground in two halves of 8 by 16 blocks */
#define SLOPE_HALF_COUNT 2
#define SLOPE_BLOCK_COLUMN_COUNT 8
#define SLOPE_BLOCK_ROW_COUNT 16
/* The triangles' starting corners; every corner is overwritten before it is read */
#define START_TRIANGLE_SIZE 256
#define SLOPE_SHADE_OFFSET 0x40
#define SLOPE_SHADE_MAX 0xFF

/* func_80405DB0's ground shaping searches this many nearest offsets */
#define SHAPE_OFFSET_COUNT 1600
#define SHAPE_OFFSET_SPAN 20
#define HOLE_EDITOR_SHAPE_ASSERT_LINE 2191
/* The terrain code grid's columns and rows */
#define CODE_GRID_COLUMN_COUNT (TERRAIN_CODE_CELL_COLUMN_COUNT * TERRAIN_CODE_CELL_SPAN)
#define CODE_GRID_ROW_COUNT (TERRAIN_CODE_CELL_ROW_COUNT * TERRAIN_CODE_CELL_SPAN)
#define GREEN_NOISE_MASK 3
#define GROUND_NOISE_MASK 0xF
/* A point's y changes by this per course unit between its code and the green's middle */
#define HEIGHT_PER_COURSE_UNIT -0.00000390625f
/* Bunker codes with no other code nearer than this get BUNKER_INNER_OFFSET added to y */
#define BUNKER_INNER_DISTANCE 2
#define BUNKER_INNER_OFFSET 45
/* Rough codes take ROUGH_EDGE_OFFSET off y, or more with no other code nearer than this */
#define ROUGH_INNER_DISTANCE 19
#define ROUGH_EDGE_OFFSET 22
#define ROUGH_INNER_OFFSET 120
#define ROUGH_INNER_NOISE_MASK 0x1F
#define SHAPED_POINT_SHADE 0xFF
/* Trees go in rough this many code cells from its edge, at least TREE_SPACING units apart */
#define TREE_MIN_DISTANCE 8
#define TREE_MAX_DISTANCE 10
#define TREE_SPACING 0x100
#define TREE_TEXTURE_INDEX 2
/* PlaceBillboards sets isNearCourseEdge and y again */
#define TREE_NEAR_EDGE 0x100
#define TREE_WIDTH 0x80
#define TREE_HEIGHT 0x100

/* func_804077B0 blends heights over distances of this many units per falloff step */
#define BLEND_DISTANCE_SCALE 512.0f
#define BLENDED_POINT_SHADE 0xFF

/* func_804066C8 rejects cell triangles whose cross product is below this */
#define MIN_CELL_CROSS_PRODUCT 16

/* func_80404850's out-of-bounds slope searches this many nearest offsets */
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

/* func_80404B84's ground height editor */
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
/* What a frame did, kept in sLastHeightEditKind */
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
#define METERS_PER_HEIGHT_UNIT (1.0f / 15)
#define HEIGHT_TEXT_COLUMN 25
#define HEIGHT_TEXT_ROW 3

/* func_80402428's triangle editor */
/* What a frame did, kept in sLastTriangleEditKind */
#define TRIANGLE_EDIT_NONE 0
#define TRIANGLE_EDIT_MOVE 1
#define TRIANGLE_EDIT_SELECT 2
#define TRIANGLE_EDIT_KIND 4
/* The last RING_COUNT triangles hold the rings, which func_80402E1C edits */
#define FIRST_RING_TRIANGLE (HOLE_TRIANGLE_COUNT - RING_COUNT)
#define HOLE_TRIANGLE_KIND_COUNT 4
/* A step moves the corner this far at zoom 1 */
#define CORNER_STEP_SCALE 10.0f
#define SAVED_CORNER_COUNT 10
#define TRIANGLE_TEXT_COLUMN 10
#define EDIT_KIND_TEXT_ROW 10
#define CORNER_HEIGHT_TEXT_ROW 11

/* func_80402E1C's ring editor; its corners hold a ring's position, direction and scale */
#define RING_DIRECTION_CORNER 1
#define RING_SCALE_CORNER 2 /* In the corner's x, 1024 for the ring's own size */
#define RING_SCALE_STEP 16
#define RING_SCALE_MIN 16
#define RING_SCALE_MAX 0x2000
#define RING_DIRECTION_LENGTH 8192.0f
#define RING_TEXT_COLUMN 19
#define RING_TEXT_ROW 4

/* func_80403A04's wall editor */
#define WALL_TRIANGLE_COUNT 2
#define WALL_BOTTOM_CORNER_COUNT 2 /* Corners 0 and 1; the others follow them */
#define WALL_TOP_CORNER 2
#define WALL_HEIGHT 24.0f
#define WALL_SINK_DEPTH (12 << WORLD_TO_COURSE_SHIFT) /* How far a dropped wall reaches below the ground */
#define WALL_LIMIT_SOUND 0xE /* Also the ball's flagstick hit */
#define WALL_LIMIT_SOUND_CHANNEL 11
#define WALL_TEXT_COLUMN 5
#define WALL_TEXT_ROW 3

/* func_80406C3C's tree editor */
#define TREE_TEXT_COLUMN 19
#define TREE_KIND_LABEL_ROW 24
#define TREE_KIND_TEXT_ROW 25
#define TREE_INDEX_LABEL_ROW 26
#define TREE_INDEX_TEXT_ROW 27
#define TREE_SIZE_MAX 0x800
/* A billboard from 1 to these limits in x and z is taken */
#define TREE_X_LIMIT 0x1000
#define TREE_Z_LIMIT 0x2000
#define COPIED_TREE_OFFSET 0x100 /* In x */

/* func_80407F08's hex dump */
#define HEX_DUMP_BUFFER_SIZE 0x40000
#define HEX_DUMP_TEXT_SIZE 0x100
#define HEX_DUMP_LINE_WORDS 4
#define HEX_DUMP_LINE_BYTES (HEX_DUMP_LINE_WORDS * (s32)sizeof(u32))
#define HOLE_EDITOR_HEX_DUMP_ASSERT_LINE 3027

/* func_80406818's steps, in sHoleFileStep */
#define HOLE_FILE_STEP_ACTION 0
#define HOLE_FILE_STEP_COURSE 1
#define HOLE_FILE_STEP_HOLE 2
#define HOLE_FILE_STEP_CONFIRM 3
/* The course numbers it offers run from 1 to this */
#define HOLE_FILE_COURSE_COUNT 9
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
#define DEBUG_TEXT_BUFFER_SIZE 0x80

/* The ground cells, between neighbouring ground points */
#define CELL_COLUMN_COUNT (GROUND_POINT_COLUMN_COUNT - 1)
#define CELL_ROW_COUNT (GROUND_POINT_ROW_COUNT - 1)

/* func_804002C0's ground point editor */
#define POINT_MOVE_COUNT 4 /* One for each C-button */
#define POINT_MOVE_STEP 0x10
#define POINT_TEXTURE_STEP 0x40
#define TILE_POINT_MASK (GROUND_TILE_POINT_SPAN - 1)
#define UNKNOWN_LIMIT_MIN 2
#define UNKNOWN_LIMIT_MAX 13
#define HEIGHT_UNITS_PER_METER 15
#define POINT_TEXT_COLUMN 2
#define TILE_TEXT_ROW 4
#define TILE_POINT_TEXT_ROW 5
#define X_LIMIT_TEXT_ROW 6
#define Z_LIMIT_TEXT_ROW 7
#define POINT_HEIGHT_TEXT_ROW 8
#define DEAD_TRIANGLE_TEXT_ROW 9
/* How scene 9's cursor zooms the map camera each frame R or L is held */
#define CURSOR_ZOOM_IN_FACTOR 1.1f
#define CURSOR_ZOOM_OUT_FACTOR 0.9f
#define CURSOR_ZOOM_MAX 10.0f
#define CURSOR_ZOOM_MIN 0.1f

/* func_80405870 gathers the terrain codes of cells at least this far from a diagonal */
#define DIAGONAL_MIN_DISTANCE 3.0f
/* The sides of the two diagonals that func_80405870 gathers codes for */
#define DIAGONAL_SIDE_COUNT 4
/* How many different codes each side keeps */
#define SIDE_CODE_COUNT 2
#define NO_TERRAIN_CODE (-1)
#define HOLE_EDITOR_DIAGONAL_ASSERT_LINE 2046
/* The vertex units in a terrain code column or row */
#define VERTEX_UNITS_PER_CODE 16

/* The screen captures, 24-bit BMPs; the corner capture covers the frame's top-left corner */
#define CAPTURE_HEADER_SIZE 54
#define CORNER_CAPTURE_WIDTH 64
#define CORNER_CAPTURE_HEIGHT 128
#define CAPTURE_BYTES_PER_PIXEL 3
/* The size of a whole-screen capture */
/* The map capture covers every ground tile's texels, inside their borders */
#define MAP_CAPTURE_WIDTH (GROUND_TILE_COLUMN_COUNT * GROUND_TILE_TEXEL_SPAN)
#define MAP_CAPTURE_HEIGHT (GROUND_TILE_ROW_COUNT * GROUND_TILE_TEXEL_SPAN)
#define MAP_CAPTURE_SIZE (CAPTURE_HEADER_SIZE + MAP_CAPTURE_WIDTH * MAP_CAPTURE_HEIGHT * CAPTURE_BYTES_PER_PIXEL)
#define GROUND_TILE_TEXEL_BORDER 1
#define CAPTURE_FILE_NAME_SIZE 0x80
#define SCREEN_CAPTURE_SIZE (CAPTURE_HEADER_SIZE + SCREEN_WIDTH * SCREEN_HEIGHT * CAPTURE_BYTES_PER_PIXEL)
/* Where the 5-bit channels sit in an RGBA16 pixel, and the shift that widens one to 8 bits */
#define RGBA16_RED_SHIFT 11
#define RGBA16_GREEN_SHIFT 6
#define RGBA16_BLUE_SHIFT 1
#define RGBA16_CHANNEL_MASK 0x1F
#define RGBA16_CHANNEL_WIDEN_SHIFT 3
/* Adds to a cached KSEG0 address to read the same memory uncached through KSEG1 */
#define KSEG1_OFFSET 0x20000000

extern u8 D_80451A60[];

/* func_8040C360's pole: two crossed quads, one in the x-y plane and one in the z-y plane */
#define POLE_QUAD_COUNT 2
#define POLE_QUAD_VERTEX_COUNT 4
extern Vtx sPoleVertices[POLE_QUAD_COUNT][POLE_QUAD_VERTEX_COUNT];
extern Mtx D_8040CC98;

/* func_80407D64's 32x32 IA8 wave pattern */
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
extern u8 D_80491A60[WAVE_TEXTURE_HEIGHT * WAVE_TEXTURE_WIDTH];

s32 GetTerrainCode(s32 column, s32 row);
void BuildAllTileMidpoints(void);
s32 CrossProductXZ(s32 originX, s32 originZ, s32 firstX, s32 firstZ, s32 secondX, s32 secondZ);
void MarkBillboardsOnMap(void);
extern u8 D_8040C580_hole_editor[EDGE_POINT_COUNT][EDGE_POINT_COUNT];
extern s32 sScreenCaptureNumber;
/* The cell that scene 9's cursor is on */
extern s32 sCellCursorColumn;
extern s32 sCellCursorRow;
/* The ground point that scene 10's cursor is on, starting at scene 9's cell */
extern s32 sPointCursorColumn;
extern s32 sPointCursorRow;
extern s32 D_8040C5B8_hole_editor;
extern s32 D_8040C5BC_hole_editor;

/* func_80402428's state */
extern f32 sTriangleCameraPitch;   /* In radians */
extern f32 sTriangleCameraHeading; /* In radians, from sTriangleCameraHeadingAngle */
extern s32 sTriangleCameraHeadingAngle;
extern s32 sEditedTriangleIndex;
extern s32 sEditedCornerIndex;
extern s32 sSavedCornerIndex;
extern s32 sLastTriangleEditKind; /* A TRIANGLE_EDIT_* */
extern CoursePoint sSavedCorners[SAVED_CORNER_COUNT];

/* func_80402E1C's state */
extern f32 sRingCameraPitch;   /* In radians */
extern f32 sRingCameraHeading; /* In radians, from sRingCameraHeadingAngle */
extern s32 sRingCameraHeadingAngle;
extern s32 sRingTriangleIndex;
extern s32 sRingCornerIndex; /* Always 0 */
extern s32 sLastRingEditKind; /* A TRIANGLE_EDIT_* */

/* func_80403A04's state */
extern f32 sWallCameraPitch;   /* In radians */
extern f32 sWallCameraHeading; /* In radians, from sWallCameraHeadingAngle */
extern s32 sWallCameraHeadingAngle;
extern s32 sFirstWallTriangle;
extern s32 sWallTriangleIndex; /* The first of the wall's two triangles */
extern s32 sWallCornerIndex;
extern s32 sSavedWallCornerIndex;
extern s32 sLastWallEditKind; /* A TRIANGLE_EDIT_* */
extern CoursePoint sSavedWallCorners[SAVED_CORNER_COUNT];

void func_80405AF4_hole_editor(s32 column, s32 row);

/* A ground triangle's slope, from func_804079B8 */
typedef struct TriangleSlope {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 unk2;
    /* 0x4 */ s16 normalX; /* Twice the normal's x, of a normal 256 long */
    /* 0x6 */ s16 normalZ;
    /* 0x8 */ u8 shade; /* 0 when flat, from SLOPE_SHADE_OFFSET up for slopes */
    /* 0x9 */ u8 unk9;
} TriangleSlope; /* size = 0xA */

extern TriangleSlope sTriangleSlopes[CELL_COLUMN_COUNT * CELL_ROW_COUNT * 2];

/* func_80404B84's state */
extern s32 sHeightCursorColumn;
extern s32 sHeightCursorRow;
extern f32 sHeightCameraPitch;   /* In radians */
extern f32 sHeightCameraHeading; /* In radians, from sHeightCameraHeadingAngle */
extern s32 sHeightCameraHeadingAngle;
extern s32 sLastSetHeight; /* The height C-up or C-down last set */
extern s32 D_8040C668_hole_editor; /* Copied to D_800C30D1 */
extern s32 sCLeftPickedHeight; /* Heights picked up by the second editor pad */
extern s32 sCUpPickedHeight;
extern s32 sLastHeightEditKind; /* The last frame's HEIGHT_EDIT_* */

extern s8 D_800C30D1;

/*
 * Declared without a prototype, as func_804077B0 passes it a float height,
 * which goes as a double
 */
s32 SetGroundPointHeightAndShade();

/* func_80406818's menu */
extern s32 sIsSavingHoleFiles;
extern s32 sHoleFileStep; /* A HOLE_FILE_STEP_* */
extern s32 sHoleFileHoleNumber; /* From 1 */
extern s32 sHoleFileCourseNumber; /* From 1 */
extern s32 sHoleFilePromptFrames;

/* func_80406C3C's state */
extern f32 sTreeCameraPitch;   /* In radians */
extern f32 sTreeCameraHeading; /* In radians, from sTreeCameraHeadingAngle */
extern s32 sTreeCameraHeadingAngle;
extern s32 sEditedTreeIndex;
extern s32 sLastTreeEditKind; /* A TRIANGLE_EDIT_*; nothing reads it */

extern s32 sMapCaptureNumber;
extern u8 sMapCaptureHeader[CAPTURE_HEADER_SIZE];
extern u8 sScreenCaptureHeader[CAPTURE_HEADER_SIZE];
extern u8 sCornerCaptureHeader[CAPTURE_HEADER_SIZE];

void *func_80400000_hole_editor(void) {
    return D_80451A60;
}

/*
 * Runs scene 9's cell cursor: the D-pad moves it over the ground cells,
 * R and L zoom the map camera, A toggles the cell's bit and Z hands the
 * cell to scene 10. The map camera's eye stays over the cell's middle.
 */
void func_8040000C_hole_editor(void) {
    Vtx *corner;
    Vtx *oppositeCorner;

    if (g_MenuControllerData[0].button & R_JPAD) {
        sCellCursorColumn++;
    }
    if (g_MenuControllerData[0].button & L_JPAD) {
        sCellCursorColumn--;
    }
    if (g_MenuControllerData[0].button & D_JPAD) {
        sCellCursorRow--;
    }
    if (g_MenuControllerData[0].button & U_JPAD) {
        sCellCursorRow++;
    }
    if (sCellCursorColumn < 0) {
        sCellCursorColumn = 0;
    }
    if (sCellCursorColumn >= CELL_COLUMN_COUNT) {
        sCellCursorColumn = CELL_COLUMN_COUNT - 1;
    }
    if (sCellCursorRow < 0) {
        sCellCursorRow = 0;
    }
    if (sCellCursorRow >= CELL_ROW_COUNT) {
        sCellCursorRow = CELL_ROW_COUNT - 1;
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
        func_80405AF4_hole_editor(sCellCursorColumn, sCellCursorRow);
    }
    if (g_MenuControllerData[0].trigger & Z_TRIG) {
        sPointCursorColumn = sCellCursorColumn;
        sPointCursorRow = sCellCursorRow;
        ClearDebugText();
        g_Scene = SCENE_UNK10;
    }
    corner = GetGroundPoint(sCellCursorColumn, sCellCursorRow);
    oppositeCorner = GetGroundPoint(sCellCursorColumn + 1, sCellCursorRow + 1);
    g_MapCamera.view.eye[0] = (corner->v.ob[0] + oppositeCorner->v.ob[0]) / 2;
    g_MapCamera.view.eye[2] = (corner->v.ob[2] + oppositeCorner->v.ob[2]) / 2;
}

/*
 * Scene 10's ground point editor. The control pad moves the cursor over the
 * ground points, and the C-buttons move the point and its texture coordinates
 * within its tile; a point on a tile's edge stays on that edge. A move
 * that would make a dead triangle is undone. R and L zoom the map camera, and Z
 * hands the point back to scene 9.
 */
void func_804002C0_hole_editor(void) {
    Vtx_t points[POINT_MOVE_COUNT + 1]; /* Each move saves its point into the next move's slot */
    char text[DEBUG_TEXT_BUFFER_SIZE];
    Vtx_t unusedPoint; /* The game's frame has room for another point */
    s32 madeDeadTriangle;

    madeDeadTriangle = FALSE;
    if (g_MenuControllerData[EDITOR_PAD].button & R_JPAD) {
        sPointCursorColumn++;
    }
    if (g_MenuControllerData[EDITOR_PAD].button & L_JPAD) {
        sPointCursorColumn--;
    }
    if (g_MenuControllerData[EDITOR_PAD].button & D_JPAD) {
        sPointCursorRow--;
    }
    if (g_MenuControllerData[EDITOR_PAD].button & U_JPAD) {
        sPointCursorRow++;
    }
    if (sPointCursorColumn < 0) {
        sPointCursorColumn = 0;
    }
    if (sPointCursorColumn >= CELL_COLUMN_COUNT) {
        sPointCursorColumn = CELL_COLUMN_COUNT - 1;
    }
    if (sPointCursorRow < 0) {
        sPointCursorRow = 0;
    }
    if (sPointCursorRow >= CELL_ROW_COUNT) {
        sPointCursorRow = CELL_ROW_COUNT - 1;
    }
    /* B held moves only the texture */
    if (!(g_MenuControllerData[EDITOR_PAD].button & B_BUTTON)) {
        if ((g_MenuControllerData[EDITOR_PAD].button & D_CBUTTONS) && (sPointCursorRow & TILE_POINT_MASK)) {
            points[0] = GetGroundPoint(sPointCursorColumn, sPointCursorRow)->v;
            points[1] = points[0];
            points[0].ob[AXIS_Z] -= POINT_MOVE_STEP;
            points[0].tc[1] -= POINT_TEXTURE_STEP;
            SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[0]);
            if (func_804066C8_hole_editor(sPointCursorColumn, sPointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[1]);
            }
        }
        if ((g_MenuControllerData[EDITOR_PAD].button & U_CBUTTONS) && (sPointCursorRow & TILE_POINT_MASK)) {
            points[1] = GetGroundPoint(sPointCursorColumn, sPointCursorRow)->v;
            points[2] = points[1];
            points[1].ob[AXIS_Z] += POINT_MOVE_STEP;
            points[1].tc[1] += POINT_TEXTURE_STEP;
            SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[1]);
            if (func_804066C8_hole_editor(sPointCursorColumn, sPointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[2]);
            }
        }
        if ((g_MenuControllerData[EDITOR_PAD].button & L_CBUTTONS) && (sPointCursorColumn & TILE_POINT_MASK)) {
            points[2] = GetGroundPoint(sPointCursorColumn, sPointCursorRow)->v;
            points[3] = points[2];
            points[2].ob[AXIS_X] -= POINT_MOVE_STEP;
            points[2].tc[0] -= POINT_TEXTURE_STEP;
            SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[2]);
            if (func_804066C8_hole_editor(sPointCursorColumn, sPointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[3]);
            }
        }
        if ((g_MenuControllerData[EDITOR_PAD].button & R_CBUTTONS) && (sPointCursorColumn & TILE_POINT_MASK)) {
            points[3] = GetGroundPoint(sPointCursorColumn, sPointCursorRow)->v;
            points[4] = points[3];
            points[3].ob[AXIS_X] += POINT_MOVE_STEP;
            points[3].tc[0] += POINT_TEXTURE_STEP;
            SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[3]);
            if (func_804066C8_hole_editor(sPointCursorColumn, sPointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[4]);
            }
        }
    } else {
        if ((g_MenuControllerData[EDITOR_PAD].button & D_CBUTTONS) && (sPointCursorRow & TILE_POINT_MASK)) {
            points[0] = GetGroundPoint(sPointCursorColumn, sPointCursorRow)->v;
            points[1] = points[0];
            points[0].tc[1] -= POINT_TEXTURE_STEP;
            SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[0]);
            if (func_804066C8_hole_editor(sPointCursorColumn, sPointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[1]);
            }
        }
        if ((g_MenuControllerData[EDITOR_PAD].button & U_CBUTTONS) && (sPointCursorRow & TILE_POINT_MASK)) {
            points[1] = GetGroundPoint(sPointCursorColumn, sPointCursorRow)->v;
            points[2] = points[1];
            points[1].tc[1] += POINT_TEXTURE_STEP;
            SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[1]);
            if (func_804066C8_hole_editor(sPointCursorColumn, sPointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[2]);
            }
        }
        if ((g_MenuControllerData[EDITOR_PAD].button & L_CBUTTONS) && (sPointCursorColumn & TILE_POINT_MASK)) {
            points[2] = GetGroundPoint(sPointCursorColumn, sPointCursorRow)->v;
            points[3] = points[2];
            points[2].tc[0] -= POINT_TEXTURE_STEP;
            SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[2]);
            if (func_804066C8_hole_editor(sPointCursorColumn, sPointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[3]);
            }
        }
        if ((g_MenuControllerData[EDITOR_PAD].button & R_CBUTTONS) && (sPointCursorColumn & TILE_POINT_MASK)) {
            points[3] = GetGroundPoint(sPointCursorColumn, sPointCursorRow)->v;
            points[4] = points[3];
            points[3].tc[0] += POINT_TEXTURE_STEP;
            SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[3]);
            if (func_804066C8_hole_editor(sPointCursorColumn, sPointCursorRow)) {
                madeDeadTriangle = TRUE;
                SetGroundPoint(sPointCursorColumn, sPointCursorRow, &points[4]);
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
        sCellCursorColumn = sPointCursorColumn;
        sCellCursorRow = sPointCursorRow;
        ClearDebugText();
        g_Scene = SCENE_UNK9;
    }
    /* Nothing else uses these two */
    if (D_8040C5B8_hole_editor < UNKNOWN_LIMIT_MIN) {
        D_8040C5B8_hole_editor = UNKNOWN_LIMIT_MIN;
    }
    if (D_8040C5B8_hole_editor > UNKNOWN_LIMIT_MAX) {
        D_8040C5B8_hole_editor = UNKNOWN_LIMIT_MAX;
    }
    if (D_8040C5BC_hole_editor < UNKNOWN_LIMIT_MIN) {
        D_8040C5BC_hole_editor = UNKNOWN_LIMIT_MIN;
    }
    if (D_8040C5BC_hole_editor > UNKNOWN_LIMIT_MAX) {
        D_8040C5BC_hole_editor = UNKNOWN_LIMIT_MAX;
    }
    {
        Vtx *point = GetGroundPoint(sPointCursorColumn, sPointCursorRow);

        g_MapCamera.view.eye[AXIS_X] = point->v.ob[AXIS_X];
        g_MapCamera.view.eye[AXIS_Z] = point->v.ob[AXIS_Z];
    }
    sprintf(text, "SuperMacro X:%2d Z:%2d", sPointCursorColumn / GROUND_TILE_POINT_SPAN,
            sPointCursorRow / GROUND_TILE_POINT_SPAN);
    PrintDebugText(text, POINT_TEXT_COLUMN, TILE_TEXT_ROW);
    sprintf(text, "SubX:%d SubZ:%d", sPointCursorColumn & TILE_POINT_MASK, sPointCursorRow & TILE_POINT_MASK);
    PrintDebugText(text, POINT_TEXT_COLUMN, TILE_POINT_TEXT_ROW);
    SetDebugTextHighlight(TRUE);
    if (!(sPointCursorColumn & TILE_POINT_MASK)) {
        sprintf(text, "Can't Move X-Axis");
    } else {
        sprintf(text, "                 ");
    }
    PrintDebugText(text, POINT_TEXT_COLUMN, X_LIMIT_TEXT_ROW);
    SetDebugTextHighlight(TRUE);
    if (!(sPointCursorRow & TILE_POINT_MASK)) {
        sprintf(text, "Can't Move Z-Axis");
    } else {
        sprintf(text, "                 ");
    }
    PrintDebugText(text, POINT_TEXT_COLUMN, Z_LIMIT_TEXT_ROW);
    sprintf(text, "Height:%dM   ",
            -GetGroundPoint(sPointCursorColumn, sPointCursorRow)->v.ob[AXIS_Y] / HEIGHT_UNITS_PER_METER);
    PrintDebugText(text, POINT_TEXT_COLUMN, POINT_HEIGHT_TEXT_ROW);
    if (madeDeadTriangle) {
        sprintf(text, "DeadTriangle");
    } else {
        sprintf(text, "             ");
    }
    PrintDebugText(text, POINT_TEXT_COLUMN, DEAD_TRIANGLE_TEXT_ROW);
}

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80400FBC_hole_editor);

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
void func_80402428_hole_editor(void) {
    Mat4 unusedMatrices[2]; /* The game's frame has room for two matrices */
    f32 stepRight;
    f32 stepUp;
    f32 stepY;
    f32 stepX;
    f32 stepZ;
    f32 moveY; /* The scaled y step; stepY itself would compile differently */
    s32 editKind;
    s32 corner;
    f64 cameraDistance; /* As a literal, GCC would fold it into the angles before the zoom */
    Vtx_t *lookAtVertex;

    editKind = TRIANGLE_EDIT_NONE;
    D_800C30D1 = FALSE;
    stepUp = stepRight = stepY = 0.0f;
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
        sEditedCornerIndex = (sEditedCornerIndex + 1) % HOLE_TRIANGLE_VERTEX_COUNT;
    }
    PrintDebugHex(editKind, TRIANGLE_TEXT_COLUMN, EDIT_KIND_TEXT_ROW);
    if (editKind == TRIANGLE_EDIT_MOVE) {
        stepX = -cos((u16)sTriangleCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepUp -
                sin((u16)sTriangleCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepRight;
        stepZ = -sin((u16)sTriangleCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepUp +
                cos((u16)sTriangleCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepRight;
        /* Scaled in two steps; in one expression GCC would multiply 10 by the zoom first */
        {
            f32 scaledX = stepX * CORNER_STEP_SCALE;
            f32 scaledY = stepY * CORNER_STEP_SCALE;
            f32 scaledZ = stepZ * CORNER_STEP_SCALE;

            stepX = scaledX * g_MapCamera.view.zoom;
            moveY = scaledY * g_MapCamera.view.zoom;
            stepZ = scaledZ * g_MapCamera.view.zoom;
        }
        for (corner = 0; corner < HOLE_TRIANGLE_VERTEX_COUNT; corner++) {
            if (!(g_ControllerData[EDITOR_PAD].button & Z_TRIG) || sEditedCornerIndex == corner) {
                Vtx_t *vertex;

                vertex = &g_WorkArea.hole.triangles[sEditedTriangleIndex].vertices[corner].v;
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

        vertex = &g_WorkArea.hole.triangles[sEditedTriangleIndex].vertices[sEditedCornerIndex].v;
        sSavedCornerIndex = (sSavedCornerIndex + 1) % SAVED_CORNER_COUNT;
        sSavedCorners[sSavedCornerIndex][AXIS_X] = vertex->ob[AXIS_X];
        sSavedCorners[sSavedCornerIndex][AXIS_Y] = vertex->ob[AXIS_Y];
        sSavedCorners[sSavedCornerIndex][AXIS_Z] = vertex->ob[AXIS_Z];
        PrepareHoleTriangles();
    }
    if (g_MenuControllerData[OPTION_PAD].button & B_BUTTON) {
        Vtx_t *vertex;

        vertex = &g_WorkArea.hole.triangles[sEditedTriangleIndex].vertices[sEditedCornerIndex].v;
        vertex->ob[AXIS_X] = sSavedCorners[sSavedCornerIndex][AXIS_X];
        vertex->ob[AXIS_Y] = sSavedCorners[sSavedCornerIndex][AXIS_Y];
        vertex->ob[AXIS_Z] = sSavedCorners[sSavedCornerIndex][AXIS_Z];
        sSavedCornerIndex = (sSavedCornerIndex + SAVED_CORNER_COUNT - 1) % SAVED_CORNER_COUNT;
        PrepareHoleTriangles();
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & R_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        sEditedTriangleIndex++;
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & L_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        sEditedTriangleIndex--;
    }
    if (sEditedTriangleIndex < 0) {
        sEditedTriangleIndex = FIRST_RING_TRIANGLE - 1;
    }
    if (sEditedTriangleIndex >= FIRST_RING_TRIANGLE) {
        sEditedTriangleIndex = 0;
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & U_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        g_WorkArea.hole.triangles[sEditedTriangleIndex].kind++;
        editKind = TRIANGLE_EDIT_KIND;
        if (g_WorkArea.hole.triangles[sEditedTriangleIndex].kind >= HOLE_TRIANGLE_KIND_COUNT) {
            g_WorkArea.hole.triangles[sEditedTriangleIndex].kind = 0;
        }
        PrepareHoleTriangles();
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & D_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        g_WorkArea.hole.triangles[sEditedTriangleIndex].kind--;
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
        sTriangleCameraHeadingAngle -= g_MenuControllerData[EDITOR_PAD].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        sTriangleCameraPitch -= g_MenuControllerData[EDITOR_PAD].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }
    if (sTriangleCameraPitch > 0.0f) {
        sTriangleCameraPitch = 0.0f;
    }
    if (sTriangleCameraPitch < EDITOR_PITCH_MIN) {
        sTriangleCameraPitch = EDITOR_PITCH_MIN;
    }
    sTriangleCameraHeading = sTriangleCameraHeadingAngle * EDITOR_ANGLE_TO_RADIANS;
    /* The first frame without an edit rebuilds the slopes */
    if ((sLastTriangleEditKind != TRIANGLE_EDIT_NONE) & (editKind == TRIANGLE_EDIT_NONE)) {
        func_804079B8_hole_editor();
    }
    cameraDistance = EDITOR_CAMERA_DISTANCE;
    lookAtVertex = &g_WorkArea.hole.triangles[sEditedTriangleIndex].vertices[sEditedCornerIndex].v;
    g_MapCamera.view.lookAt[AXIS_X] = lookAtVertex->ob[AXIS_X];
    g_MapCamera.view.lookAt[AXIS_Y] = lookAtVertex->ob[AXIS_Y];
    sLastTriangleEditKind = editKind;
    g_MapCamera.view.lookAt[AXIS_Z] = lookAtVertex->ob[AXIS_Z];
    g_MapCamera.view.eye[AXIS_X] = cos(sTriangleCameraPitch) * cos(sTriangleCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance) +
                                   g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = cos(sTriangleCameraPitch) * sin(sTriangleCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance) +
                                   g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] = sin(sTriangleCameraPitch) *
                                       (g_MapCamera.view.zoom * cameraDistance) +
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
void func_80402E1C_hole_editor(void) {
    Vec3 direction;
    f32 stepRight;
    f32 stepUp;
    f32 stepY;
    f32 stepX;
    f32 stepZ;
    f32 moveY; /* The scaled y step; stepY itself would compile differently */
    s32 editKind;
    s32 corner;
    f64 cameraDistance; /* As a literal, GCC would fold it into the angles before the zoom */
    Vtx_t *lookAtVertex;

    editKind = TRIANGLE_EDIT_NONE;
    stepY = 0.0f;
    D_800C30D1 = FALSE;
    sprintf(g_PrintBuffer, "EditNo : %d   ", sRingTriangleIndex);
    PrintDebugText(g_PrintBuffer, RING_TEXT_COLUMN, RING_TEXT_ROW);
    stepUp = stepRight = 0.0f;
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
    } else {
        Vtx_t *scaleVertex = &g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[RING_SCALE_CORNER].v;

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
        stepX = -cos((u16)sRingCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepUp -
                sin((u16)sRingCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepRight;
        stepZ = -sin((u16)sRingCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepUp +
                cos((u16)sRingCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepRight;
        /* Scaled in two steps; in one expression GCC would multiply 10 by the zoom first */
        {
            f32 scaledX = stepX * CORNER_STEP_SCALE;
            f32 scaledY = stepY * CORNER_STEP_SCALE;
            f32 scaledZ = stepZ * CORNER_STEP_SCALE;

            stepX = scaledX * g_MapCamera.view.zoom;
            moveY = scaledY * g_MapCamera.view.zoom;
            stepZ = scaledZ * g_MapCamera.view.zoom;
        }
        for (corner = 0; corner < HOLE_TRIANGLE_VERTEX_COUNT; corner++) {
            if (sRingCornerIndex == corner) {
                Vtx_t *vertex;

                vertex = &g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[corner].v;
                vertex->ob[AXIS_X] = vertex->ob[AXIS_X] + stepX;
                vertex->ob[AXIS_Y] = vertex->ob[AXIS_Y] + moveY;
                vertex->ob[AXIS_Z] = vertex->ob[AXIS_Z] + stepZ;
            }
        }
        PrepareHoleTriangles();
    }
    if (g_ControllerData[EDITOR_PAD].button & A_BUTTON) {
        Vtx_t *directionVertex;

        g_WorkArea.hole.triangles[sRingTriangleIndex].kind = HOLE_TRIANGLE_KIND_RING;
        directionVertex = &g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[RING_DIRECTION_CORNER].v;
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
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        sRingTriangleIndex++;
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & L_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        sRingTriangleIndex--;
    }
    if (sRingTriangleIndex < FIRST_RING_TRIANGLE) {
        sRingTriangleIndex = HOLE_TRIANGLE_COUNT - 1;
    }
    if (sRingTriangleIndex >= HOLE_TRIANGLE_COUNT) {
        sRingTriangleIndex = FIRST_RING_TRIANGLE;
    }
    if (g_ControllerData[SECOND_EDITOR_PAD].button & B_BUTTON) {
        /* Clears every field but unk4 and unk5C */
        g_WorkArea.hole.triangles[sRingTriangleIndex].kind = HOLE_TRIANGLE_KIND_FLAT;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[0].v.ob[AXIS_X] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[0].v.ob[AXIS_Y] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[0].v.ob[AXIS_Z] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[0].v.flag = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[0].v.tc[0] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[0].v.tc[1] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[0].v.cn[0] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[0].v.cn[1] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[0].v.cn[2] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[0].v.cn[3] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[1].v.ob[AXIS_X] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[1].v.ob[AXIS_Y] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[1].v.ob[AXIS_Z] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[1].v.flag = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[1].v.tc[0] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[1].v.tc[1] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[1].v.cn[0] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[1].v.cn[1] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[1].v.cn[2] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[1].v.cn[3] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[2].v.ob[AXIS_X] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[2].v.ob[AXIS_Y] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[2].v.ob[AXIS_Z] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[2].v.flag = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[2].v.tc[0] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[2].v.tc[1] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[2].v.cn[0] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[2].v.cn[1] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[2].v.cn[2] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[2].v.cn[3] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].bounds[AXIS_X][0] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].bounds[AXIS_X][1] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].bounds[AXIS_Y][0] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].bounds[AXIS_Y][1] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].bounds[AXIS_Z][0] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].bounds[AXIS_Z][1] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].center[AXIS_X] = 0.0f;
        g_WorkArea.hole.triangles[sRingTriangleIndex].center[AXIS_Y] = 0.0f;
        g_WorkArea.hole.triangles[sRingTriangleIndex].center[AXIS_Z] = 0.0f;
        g_WorkArea.hole.triangles[sRingTriangleIndex].unk50[0] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].unk50[1] = 0;
        g_WorkArea.hole.triangles[sRingTriangleIndex].unk50[2] = 0;
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
        sRingCameraHeadingAngle -= g_MenuControllerData[EDITOR_PAD].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        sRingCameraPitch -= g_MenuControllerData[EDITOR_PAD].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }
    if (sRingCameraPitch > 0.0f) {
        sRingCameraPitch = 0.0f;
    }
    if (sRingCameraPitch < EDITOR_PITCH_MIN) {
        sRingCameraPitch = EDITOR_PITCH_MIN;
    }
    sRingCameraHeading = sRingCameraHeadingAngle * EDITOR_ANGLE_TO_RADIANS;
    /* The first frame without an edit rebuilds the slopes */
    if ((sLastRingEditKind != TRIANGLE_EDIT_NONE) & (editKind == TRIANGLE_EDIT_NONE)) {
        func_804079B8_hole_editor();
    }
    cameraDistance = EDITOR_CAMERA_DISTANCE;
    lookAtVertex = &g_WorkArea.hole.triangles[sRingTriangleIndex].vertices[sRingCornerIndex].v;
    g_MapCamera.view.lookAt[AXIS_X] = lookAtVertex->ob[AXIS_X];
    g_MapCamera.view.lookAt[AXIS_Y] = lookAtVertex->ob[AXIS_Y];
    sLastRingEditKind = editKind;
    g_MapCamera.view.lookAt[AXIS_Z] = lookAtVertex->ob[AXIS_Z];
    g_MapCamera.view.eye[AXIS_X] = cos(sRingCameraPitch) * cos(sRingCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance) +
                                   g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = cos(sRingCameraPitch) * sin(sRingCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance) +
                                   g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] = sin(sRingCameraPitch) *
                                       (g_MapCamera.view.zoom * cameraDistance) +
                                   g_MapCamera.view.lookAt[AXIS_Y];
}

/*
 * Scene SCENE_WALLS's editor for walls, each a pair of shaded hole triangles
 * from sFirstWallTriangle up to the rings. The editor pad's C-buttons move
 * the wall along x and z, the option pad's C-down and C-up along y, and the
 * option pad's C-left drops it onto the ground. Z limits an edit to the
 * corner that the editor pad's B picks. The option pad's A saves that corner
 * and its B restores the last saved one. Right and left on the control pad
 * pick the wall. R zooms in, L or Z with R zooms out, and Z with the stick
 * orbits the camera around the corner.
 */
/* Doesn't match: the original copies stepUp into stepZ's register before scaling it, where GCC folds the copy */
#ifdef NON_MATCHING
void func_80403A04_hole_editor(void) {
    char text[DEBUG_TEXT_BUFFER_SIZE];
    Vtx_t *corners[HOLE_TRIANGLE_VERTEX_COUNT];
    Vtx_t *nextCorners[HOLE_TRIANGLE_VERTEX_COUNT];
    Mat4 unusedMatrices[2]; /* The game's frame has room for two matrices */
    f32 stepRight;
    f32 stepUp;
    f32 stepY;
    f32 stepX;
    f32 stepZ;
    f32 moveY; /* The scaled y step; stepY itself would compile differently */
    s32 editKind;
    s32 isDropping;
    s32 corner;
    f64 cameraDistance; /* As a literal, GCC would fold it into the angles before the zoom */
    Vtx_t *lookAtVertex;

    editKind = TRIANGLE_EDIT_NONE;
    isDropping = FALSE;
    D_800C30D1 = FALSE;
    stepUp = stepRight = stepY = 0.0f;
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
        sWallCornerIndex = (sWallCornerIndex + 1) % WALL_BOTTOM_CORNER_COUNT;
    }
    PrintDebugHex(editKind, TRIANGLE_TEXT_COLUMN, EDIT_KIND_TEXT_ROW);
    sprintf(text, "EditNo%d   ", sWallTriangleIndex);
    PrintDebugText(text, WALL_TEXT_COLUMN, WALL_TEXT_ROW);
    if ((editKind == TRIANGLE_EDIT_MOVE) | (isDropping == TRUE)) {
        /* Left over from the triangle editor; walls move along the world axes */
        stepX = -cos((u16)sWallCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepUp -
                sin((u16)sWallCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepRight;
        stepZ = -sin((u16)sWallCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepUp +
                cos((u16)sWallCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) * stepRight;
        stepZ = stepUp;
        /* Scaled in two steps; in one expression GCC would multiply 10 by the zoom first */
        {
            f32 scaledX = stepRight * CORNER_STEP_SCALE;
            f32 scaledY = stepY * CORNER_STEP_SCALE;
            f32 scaledZ = stepZ * CORNER_STEP_SCALE;

            stepX = scaledX * g_MapCamera.view.zoom;
            moveY = scaledY * g_MapCamera.view.zoom;
            stepZ = scaledZ * g_MapCamera.view.zoom;
        }
        for (corner = 0; corner < HOLE_TRIANGLE_VERTEX_COUNT; corner++) {
            if (!(g_ControllerData[EDITOR_PAD].button & Z_TRIG) || sWallCornerIndex == corner) {
                Vtx_t *vertex;

                vertex = &g_WorkArea.hole.triangles[sWallTriangleIndex].vertices[corner].v;
                if (isDropping == TRUE) {
                    s32 height = (GetGroundHeight(vertex->ob[AXIS_X] << WORLD_TO_COURSE_SHIFT,
                                                  vertex->ob[AXIS_Z] << WORLD_TO_COURSE_SHIFT) -
                                  WALL_SINK_DEPTH) *
                                 (1.0f / (1 << WORLD_TO_COURSE_SHIFT));

                    if (corner == WALL_TOP_CORNER) {
                        height += WALL_HEIGHT;
                        vertex->ob[AXIS_Y] = height;
                    } else {
                        vertex->ob[AXIS_Y] = height;
                    }
                } else {
                    vertex->ob[AXIS_X] = vertex->ob[AXIS_X] + stepX;
                    vertex->ob[AXIS_Y] = vertex->ob[AXIS_Y] + moveY;
                    vertex->ob[AXIS_Z] = vertex->ob[AXIS_Z] + stepZ;
                }
                PrintDebugHex(vertex->ob[AXIS_Y], TRIANGLE_TEXT_COLUMN, CORNER_HEIGHT_TEXT_ROW);
            }
        }
        for (corner = 0; corner < HOLE_TRIANGLE_VERTEX_COUNT; corner++) {
            corners[corner] = &g_WorkArea.hole.triangles[sWallTriangleIndex].vertices[corner].v;
            nextCorners[corner] = &g_WorkArea.hole.triangles[sWallTriangleIndex + 1].vertices[corner].v;
        }
        /* The first triangle's top corner stands over its first corner; the second fills in the rest */
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

        vertex = &g_WorkArea.hole.triangles[sWallTriangleIndex].vertices[sWallCornerIndex].v;
        sSavedWallCornerIndex = (sSavedWallCornerIndex + 1) % SAVED_CORNER_COUNT;
        sSavedWallCorners[sSavedWallCornerIndex][AXIS_X] = vertex->ob[AXIS_X];
        sSavedWallCorners[sSavedWallCornerIndex][AXIS_Y] = vertex->ob[AXIS_Y];
        sSavedWallCorners[sSavedWallCornerIndex][AXIS_Z] = vertex->ob[AXIS_Z];
        PrepareHoleTriangles();
    }
    if (g_MenuControllerData[OPTION_PAD].button & B_BUTTON) {
        Vtx_t *vertex;

        vertex = &g_WorkArea.hole.triangles[sWallTriangleIndex].vertices[sWallCornerIndex].v;
        vertex->ob[AXIS_X] = sSavedWallCorners[sSavedWallCornerIndex][AXIS_X];
        vertex->ob[AXIS_Y] = sSavedWallCorners[sSavedWallCornerIndex][AXIS_Y];
        vertex->ob[AXIS_Z] = sSavedWallCorners[sSavedWallCornerIndex][AXIS_Z];
        sSavedWallCornerIndex = (sSavedWallCornerIndex + SAVED_CORNER_COUNT - 1) % SAVED_CORNER_COUNT;
        PrepareHoleTriangles();
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & R_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        sWallTriangleIndex += WALL_TRIANGLE_COUNT;
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & L_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        sWallTriangleIndex -= WALL_TRIANGLE_COUNT;
    }
    if (sWallTriangleIndex < sFirstWallTriangle) {
        sWallTriangleIndex = sFirstWallTriangle;
        PlaySound(WALL_LIMIT_SOUND, WALL_LIMIT_SOUND_CHANNEL, EDITOR_SOUND_VOLUME);
    }
    if (sWallTriangleIndex >= FIRST_RING_TRIANGLE) {
        sWallTriangleIndex -= WALL_TRIANGLE_COUNT;
        PlaySound(WALL_LIMIT_SOUND, WALL_LIMIT_SOUND_CHANNEL, EDITOR_SOUND_VOLUME);
    }
    /* Both of the wall's triangles are kept shaded */
    g_WorkArea.hole.triangles[sWallTriangleIndex].kind = HOLE_TRIANGLE_KIND_SHADED;
    g_WorkArea.hole.triangles[sWallTriangleIndex + 1].kind = HOLE_TRIANGLE_KIND_SHADED;
    if ((g_MenuControllerData[EDITOR_PAD].button & U_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_KIND;
        /* Never true; the triangle editor's kind cycling without the step */
        if (g_WorkArea.hole.triangles[sWallTriangleIndex].kind >= HOLE_TRIANGLE_KIND_COUNT) {
            g_WorkArea.hole.triangles[sWallTriangleIndex].kind = HOLE_TRIANGLE_KIND_FLAT;
        }
        PrepareHoleTriangles();
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & D_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
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
        sWallCameraHeadingAngle -= g_MenuControllerData[EDITOR_PAD].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        sWallCameraPitch -= g_MenuControllerData[EDITOR_PAD].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }
    if (sWallCameraPitch > 0.0f) {
        sWallCameraPitch = 0.0f;
    }
    if (sWallCameraPitch < EDITOR_PITCH_MIN) {
        sWallCameraPitch = EDITOR_PITCH_MIN;
    }
    sWallCameraHeading = sWallCameraHeadingAngle * EDITOR_ANGLE_TO_RADIANS;
    /* The first frame without an edit rebuilds the slopes */
    if ((sLastWallEditKind != TRIANGLE_EDIT_NONE) & (editKind == TRIANGLE_EDIT_NONE)) {
        func_804079B8_hole_editor();
    }
    cameraDistance = EDITOR_CAMERA_DISTANCE;
    lookAtVertex = &g_WorkArea.hole.triangles[sWallTriangleIndex].vertices[sWallCornerIndex].v;
    g_MapCamera.view.lookAt[AXIS_X] = lookAtVertex->ob[AXIS_X];
    g_MapCamera.view.lookAt[AXIS_Y] = lookAtVertex->ob[AXIS_Y];
    sLastWallEditKind = editKind;
    g_MapCamera.view.lookAt[AXIS_Z] = lookAtVertex->ob[AXIS_Z];
    g_MapCamera.view.eye[AXIS_X] = cos(sWallCameraPitch) * cos(sWallCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance) +
                                   g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = cos(sWallCameraPitch) * sin(sWallCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance) +
                                   g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] = sin(sWallCameraPitch) *
                                       (g_MapCamera.view.zoom * cameraDistance) +
                                   g_MapCamera.view.lookAt[AXIS_Y];
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80403A04_hole_editor);
#endif

/*
 * Sets the billboards and the hole's points back on the ground, and
 * copies the first cup and tee positions to D_800BA9D8. The billboards'
 * first height comes from their x and z unscaled. The translation each
 * billboard builds goes unused, and PlaceBillboards places them all again
 * for each one.
 */
void func_804045C0_hole_editor(void) {
    Mat4 translation;
    Mat4 unusedMatrix; /* The game's frame has room for a second matrix */
    s32 height;
    s32 i;

    PlaySound(SOUND_CONFIRM, EDITOR_SOUND_CHANNEL, EDITOR_SOUND_VOLUME);
    for (i = 0; i < BILLBOARD_COUNT; i++) {
        if (!g_Billboards[i].isNearCourseEdge) {
            g_Billboards[i].y = GetGroundHeight(g_Billboards[i].x, g_Billboards[i].z) * COURSE_TO_WORLD_SCALE;
            height = GetGroundHeight(g_Billboards[i].x << WORLD_TO_COURSE_SHIFT,
                                     g_Billboards[i].z << WORLD_TO_COURSE_SHIFT) /
                     (1 << WORLD_TO_COURSE_SHIFT);
            guTranslateF(translation, g_Billboards[i].x, height + 1, g_Billboards[i].z);
            PlaceBillboards();
        }
    }
    for (i = 0; i < CUP_POSITION_COUNT; i++) {
        if (g_HolePoints.cupPositions[i][AXIS_X] != 0 && g_HolePoints.cupPositions[i][AXIS_Z] != 0) {
            g_HolePoints.cupPositions[i][AXIS_Y] =
                GetGroundHeight(g_HolePoints.cupPositions[i][AXIS_X], g_HolePoints.cupPositions[i][AXIS_Z]);
            g_HolePoints.teePositions[i][AXIS_Y] =
                GetGroundHeight(g_HolePoints.teePositions[i][AXIS_X], g_HolePoints.teePositions[i][AXIS_Z]);
        }
    }
    D_800BA9D8.cupPosition[AXIS_X] = g_HolePoints.cupPositions[0][AXIS_X];
    g_CupPositionY = g_HolePoints.cupPositions[0][AXIS_Y];
    g_CupPositionZ = g_HolePoints.cupPositions[0][AXIS_Z];
    g_TeePositionX = g_HolePoints.teePositions[0][AXIS_X];
    g_TeePositionY = g_HolePoints.teePositions[0][AXIS_Y];
    g_TeePositionZ = g_HolePoints.teePositions[0][AXIS_Z];
    for (i = 0; i < ROUTE_POINT_COUNT; i++) {
        if (g_HolePoints.unk60[i][AXIS_X] != 0 && g_HolePoints.unk60[i][AXIS_Z] != 0) {
            g_HolePoints.unk60[i][AXIS_Y] =
                GetGroundHeight(g_HolePoints.unk60[i][AXIS_X], g_HolePoints.unk60[i][AXIS_Z]);
        }
        if (g_HolePoints.routePoints[i][AXIS_X] != 0 && g_HolePoints.routePoints[i][AXIS_Z] != 0) {
            g_HolePoints.routePoints[i][AXIS_Y] =
                GetGroundHeight(g_HolePoints.routePoints[i][AXIS_X], g_HolePoints.routePoints[i][AXIS_Z]);
        }
    }
}

/*
 * Slopes the out-of-bounds ground: each out-of-bounds ground point moves
 * along y by how far it is from the nearest point with another code, with
 * noise from SLOPE_NOISE_DISTANCE on. The height editor's option pad calls
 * it with C-down.
 */
void func_80404850_hole_editor(void) {
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
            if (GetTerrainCode(groundPoint->v.ob[AXIS_X] / VERTEX_UNITS_PER_CODE,
                               groundPoint->v.ob[AXIS_Z] / VERTEX_UNITS_PER_CODE) != TERRAIN_CODE_OUT_OF_BOUNDS) {
                continue;
            }
            for (offsetIndex = 1; offsetIndex < offsetCount; offsetIndex++) {
                if (offsets[offsetIndex].distance >= SLOPE_NOISE_DISTANCE * OFFSET_DISTANCE_SCALE) {
                    break;
                }
                neighbor = GetGroundPoint(column + offsets[offsetIndex].column, row + offsets[offsetIndex].row);
                if (GetTerrainCode(neighbor->v.ob[AXIS_X] / VERTEX_UNITS_PER_CODE,
                                   neighbor->v.ob[AXIS_Z] / VERTEX_UNITS_PER_CODE) != TERRAIN_CODE_OUT_OF_BOUNDS) {
                    break;
                }
            }
            point = groundPoint->v;
            /* Never true, as the search stops at SLOPE_NOISE_DISTANCE */
            if (offsets[offsetIndex].distance / OFFSET_DISTANCE_SCALE >= SLOPE_EXTRA_NOISE_DISTANCE) {
                point.ob[AXIS_Y] += (rand() & SLOPE_NOISE_MASK) * SLOPE_NOISE_SCALE + SLOPE_EXTRA_DROP;
            }
            if (offsets[offsetIndex].distance / OFFSET_DISTANCE_SCALE >= SLOPE_NOISE_DISTANCE) {
                point.ob[AXIS_Y] += (rand() & SLOPE_NOISE_MASK) * SLOPE_NOISE_SCALE + SLOPE_FAR_DROP;
            } else if (offsets[offsetIndex].distance / OFFSET_DISTANCE_SCALE >= SLOPE_START_DISTANCE) {
                point.ob[AXIS_Y] += (offsets[offsetIndex].distance - SLOPE_BASE_DISTANCE * OFFSET_DISTANCE_SCALE) *
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
 * option pad toggles D_8040C668 and calls func_804045C0 and func_80404850.
 * After a frame of edits, the next frame without one rebuilds the slopes.
 */
void func_80404B84_hole_editor(void) {
    char text[DEBUG_TEXT_BUFFER_SIZE];
    s32 baseHeight;
    s32 editKind;
    s32 direction;
    s32 columnStep;
    s32 rowStep;
    f64 cameraDistance; /* As a literal, GCC would fold it into the angles before the zoom */

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
    if (D_8040C668_hole_editor) {
        D_800C30D1 = TRUE;
    } else {
        D_800C30D1 = FALSE;
    }
    if (g_MenuControllerData[OPTION_PAD].button & D_CBUTTONS) {
        func_80404850_hole_editor();
    }
    if (g_MenuControllerData[SECOND_EDITOR_PAD].button & L_CBUTTONS) {
        sCLeftPickedHeight = GetGroundPoint(sHeightCursorColumn, sHeightCursorRow)->v.ob[AXIS_Y];
    }
    if (g_MenuControllerData[SECOND_EDITOR_PAD].button & U_CBUTTONS) {
        sCUpPickedHeight = GetGroundPoint(sHeightCursorColumn, sHeightCursorRow)->v.ob[AXIS_Y];
    }
    if (g_MenuControllerData[SECOND_EDITOR_PAD].button & R_CBUTTONS) {
        s32 change = sCUpPickedHeight - sCLeftPickedHeight;

        editKind = HEIGHT_EDIT_POINT;
        SetGroundPointHeightAndShade(
            sHeightCursorColumn, sHeightCursorRow,
            GetGroundPoint(sHeightCursorColumn, sHeightCursorRow)->v.ob[AXIS_Y] + change,
            EDITED_POINT_SHADE);
        MeasureGroundTiles();
    }
    if (g_MenuControllerData[SECOND_EDITOR_PAD].button & D_CBUTTONS) {
        s32 change = sCLeftPickedHeight - sCUpPickedHeight;

        editKind = HEIGHT_EDIT_POINT;
        SetGroundPointHeightAndShade(
            sHeightCursorColumn, sHeightCursorRow,
            GetGroundPoint(sHeightCursorColumn, sHeightCursorRow)->v.ob[AXIS_Y] + change,
            EDITED_POINT_SHADE);
        MeasureGroundTiles();
    }
    if (g_MenuControllerData[OPTION_PAD].button & A_BUTTON) {
        if (D_8040C668_hole_editor) {
            D_8040C668_hole_editor = FALSE;
        } else {
            D_8040C668_hole_editor = TRUE;
        }
    }
    if (g_MenuControllerData[OPTION_PAD].button & B_BUTTON) {
        func_804045C0_hole_editor();
    }
    if (g_ControllerData[EDITOR_PAD].button & U_CBUTTONS) {
        if (g_ControllerData[EDITOR_PAD].button & Z_TRIG) {
            ChangeGroundHeight(-HEIGHT_STEP);
        } else {
            s32 height = GetGroundPoint(sHeightCursorColumn, sHeightCursorRow)->v.ob[AXIS_Y];

            height -= HEIGHT_STEP;
            sLastSetHeight = height;
            SetGroundPointHeightAndShade(sHeightCursorColumn, sHeightCursorRow, height, EDITED_POINT_SHADE);
        }
        editKind = HEIGHT_EDIT_POINT;
        MeasureGroundTiles();
    }
    if (g_ControllerData[EDITOR_PAD].button & D_CBUTTONS) {
        if (g_ControllerData[EDITOR_PAD].button & Z_TRIG) {
            ChangeGroundHeight(HEIGHT_STEP);
        } else {
            s32 height = GetGroundPoint(sHeightCursorColumn, sHeightCursorRow)->v.ob[AXIS_Y];

            height += HEIGHT_STEP;
            sLastSetHeight = height;
            SetGroundPointHeightAndShade(sHeightCursorColumn, sHeightCursorRow, height, EDITED_POINT_SHADE);
        }
        editKind = HEIGHT_EDIT_POINT;
        MeasureGroundTiles();
    }
    if (g_ControllerData[EDITOR_PAD].button & R_CBUTTONS) {
        SetGroundPointHeightAndShade(sHeightCursorColumn, sHeightCursorRow, sLastSetHeight,
                                     EDITED_POINT_SHADE);
        editKind = HEIGHT_EDIT_POINT;
        MeasureGroundTiles();
    }
    if (g_ControllerData[EDITOR_PAD].button & L_CBUTTONS) {
        SetGroundPointHeightAndShade(sHeightCursorColumn, sHeightCursorRow,
                                     sLastSetHeight + (rand() & COARSE_NOISE_MASK) - COARSE_NOISE_OFFSET,
                                     EDITED_POINT_SHADE);
        editKind = HEIGHT_EDIT_POINT;
        MeasureGroundTiles();
    }
    if (g_MenuControllerData[EDITOR_PAD].button & A_BUTTON) {
        editKind = HEIGHT_EDIT_POINT;
        baseHeight = GetGroundPoint(sHeightCursorColumn, sHeightCursorRow)->v.ob[AXIS_Y];
        baseHeight -= ROUGH_NOISE_OFFSET;
        SetGroundPointHeightAndShade(sHeightCursorColumn, sHeightCursorRow,
                                     baseHeight + (rand() & ROUGH_NOISE_MASK), EDITED_POINT_SHADE);
        MeasureGroundTiles();
    }
    if (g_MenuControllerData[EDITOR_PAD].button & B_BUTTON) {
        editKind = HEIGHT_EDIT_POINT;
        baseHeight = GetGroundPoint(sHeightCursorColumn, sHeightCursorRow)->v.ob[AXIS_Y];
        baseHeight -= HEIGHT_STEP;
        SetGroundPointHeightAndShade(sHeightCursorColumn, sHeightCursorRow,
                                     baseHeight + (rand() & STEP_NOISE_BIT), EDITED_POINT_SHADE);
        MeasureGroundTiles();
    }
    if (g_MenuControllerData[SECOND_EDITOR_PAD].button & B_BUTTON) {
        editKind = HEIGHT_EDIT_POINT;
        baseHeight = GetGroundPoint(sHeightCursorColumn, sHeightCursorRow)->v.ob[AXIS_Y];
        SetGroundPointHeightAndShade(sHeightCursorColumn, sHeightCursorRow,
                                     baseHeight - (rand() & DIP_NOISE_BIT), EDITED_POINT_SHADE);
        MeasureGroundTiles();
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & R_JPAD) && !(g_ControllerData[EDITOR_PAD].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        direction = CURSOR_DIRECTION_RIGHT;
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & L_JPAD) && !(g_ControllerData[EDITOR_PAD].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        direction = CURSOR_DIRECTION_LEFT;
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & D_JPAD) && !(g_ControllerData[EDITOR_PAD].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        if (direction == CURSOR_DIRECTION_RIGHT) {
            direction = CURSOR_DIRECTION_DOWN_RIGHT;
        } else if (direction == CURSOR_DIRECTION_LEFT) {
            direction = CURSOR_DIRECTION_DOWN_LEFT;
        } else {
            direction = CURSOR_DIRECTION_DOWN;
        }
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & U_JPAD) && !(g_ControllerData[EDITOR_PAD].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        if (direction == CURSOR_DIRECTION_RIGHT) {
            direction = CURSOR_DIRECTION_UP_RIGHT;
        } else if (direction == CURSOR_DIRECTION_LEFT) {
            direction = CURSOR_DIRECTION_UP_LEFT;
        } else {
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
        } else if (direction == CURSOR_DIRECTION_LEFT) {
            direction = CURSOR_DIRECTION_DOWN_LEFT;
        } else {
            direction = CURSOR_DIRECTION_DOWN;
        }
    }
    if ((g_MenuControllerData[SECOND_EDITOR_PAD].button & U_JPAD) &&
        !(g_ControllerData[SECOND_EDITOR_PAD].button & Z_TRIG)) {
        editKind = HEIGHT_EDIT_MOVE;
        if (direction == CURSOR_DIRECTION_RIGHT) {
            direction = CURSOR_DIRECTION_UP_RIGHT;
        } else if (direction == CURSOR_DIRECTION_LEFT) {
            direction = CURSOR_DIRECTION_UP_LEFT;
        } else {
            direction = CURSOR_DIRECTION_UP;
        }
    }
    if (direction != CURSOR_DIRECTION_NONE) {
        columnStep = cos((direction * BINARY_ANGLE_EIGHTH_TURN + sHeightCameraHeadingAngle) * EDITOR_ANGLE_TO_RADIANS) *
                     CURSOR_STEP_LENGTH;
        rowStep = sin((direction * BINARY_ANGLE_EIGHTH_TURN + sHeightCameraHeadingAngle) * EDITOR_ANGLE_TO_RADIANS) *
                  CURSOR_STEP_LENGTH;
        sHeightCursorColumn += columnStep;
        sHeightCursorRow += rowStep;
        if (sHeightCursorColumn < 0) {
            sHeightCursorColumn = 0;
        }
        if (sHeightCursorRow < 0) {
            sHeightCursorRow = 0;
        }
        if (sHeightCursorColumn >= GROUND_POINT_COLUMN_COUNT) {
            sHeightCursorColumn = GROUND_POINT_COLUMN_COUNT - 1;
        }
        if (sHeightCursorRow >= GROUND_POINT_ROW_COUNT) {
            sHeightCursorRow = GROUND_POINT_ROW_COUNT - 1;
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
        sHeightCameraHeadingAngle -= g_MenuControllerData[EDITOR_PAD].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        sHeightCameraPitch -= g_MenuControllerData[EDITOR_PAD].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
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
        sHeightCameraHeadingAngle -=
            g_MenuControllerData[SECOND_EDITOR_PAD].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        sHeightCameraPitch -=
            g_MenuControllerData[SECOND_EDITOR_PAD].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }
    if (sHeightCameraPitch > 0.0f) {
        sHeightCameraPitch = 0.0f;
    }
    if (sHeightCameraPitch < EDITOR_PITCH_MIN) {
        sHeightCameraPitch = EDITOR_PITCH_MIN;
    }
    sHeightCameraHeading = sHeightCameraHeadingAngle * EDITOR_ANGLE_TO_RADIANS;
    if (sLastHeightEditKind && editKind == HEIGHT_EDIT_NONE) {
        func_804079B8_hole_editor();
    }
    sLastHeightEditKind = editKind;
    cameraDistance = EDITOR_CAMERA_DISTANCE;
    g_MapCamera.view.lookAt[AXIS_X] = GetGroundPoint(sHeightCursorColumn, sHeightCursorRow)->v.ob[AXIS_X];
    g_MapCamera.view.lookAt[AXIS_Y] = GetGroundPoint(sHeightCursorColumn, sHeightCursorRow)->v.ob[AXIS_Y];
    g_MapCamera.view.lookAt[AXIS_Z] = GetGroundPoint(sHeightCursorColumn, sHeightCursorRow)->v.ob[AXIS_Z];
    g_MapCamera.view.eye[AXIS_X] = cos(sHeightCameraPitch) * cos(sHeightCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance) +
                                   g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = cos(sHeightCameraPitch) * sin(sHeightCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance) +
                                   g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] =
        sin(sHeightCameraPitch) * (g_MapCamera.view.zoom * cameraDistance) + g_MapCamera.view.lookAt[AXIS_Y];
    sprintf(text, "%fm  ",
            -(f32)GetGroundPoint(sHeightCursorColumn, sHeightCursorRow)->v.ob[AXIS_Y] *
                METERS_PER_HEIGHT_UNIT);
    PrintDebugText(text, HEIGHT_TEXT_COLUMN, HEIGHT_TEXT_ROW);
}

static const char sEditorAssertionText[] = "EX";
static const char sEditorFileName[] = "c:/usgolf64/hiedit.c";

/* A plain if leaves GCC to invert the condition, which the game's code does not */
#define EDITOR_ASSERT(condition, lineNumber) \
    ((condition) ? (void)0 : ReportFailedAssertion(sEditorAssertionText, sEditorFileName, lineNumber))

/*
 * Gathers the terrain codes on each side of the two diagonals of the
 * rectangle from code column and row (column, row) to (endColumn,
 * endRow), keeping up to two different codes a side and skipping cells
 * within DIAGONAL_MIN_DISTANCE of the diagonal. Sides 0 and 1 are those
 * of the diagonal through offsets (0, 0) and (width, height), and sides 2
 * and 3 those of the one through (0, height) and (width, 0). Returns
 * whether sides 0 and 1 both have two codes while side 2 or 3 does not.
 */
s32 func_80405870_hole_editor(s32 column, s32 row, s32 endColumn, s32 endRow) {
    s32 sideCodes[DIAGONAL_SIDE_COUNT][SIDE_CODE_COUNT];
    s32 width;
    s32 height;
    s32 i;
    s32 rowOffset;
    s32 columnOffset;
    s32 code;
    f32 inverseLength;
    f32 distance;
    f32 absDistance;

    void AddSideCode(s32 *codes, s32 code) {
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
    EDITOR_ASSERT(width > 0 && height > 0, HOLE_EDITOR_DIAGONAL_ASSERT_LINE);
    for (i = 0; i < DIAGONAL_SIDE_COUNT; i++) {
        sideCodes[i][0] = NO_TERRAIN_CODE;
        sideCodes[i][1] = NO_TERRAIN_CODE;
    }
    inverseLength = 1.0f / sqrtf(width * width + height * height);
    for (rowOffset = 0; rowOffset < height; rowOffset++) {
        for (columnOffset = 0; columnOffset < width; columnOffset++) {
            code = GetTerrainCode(column + columnOffset, row + rowOffset);
            distance = (width * (rowOffset - height) + columnOffset * height) * inverseLength;
            absDistance = distance < 0.0f ? -distance : distance;
            if (absDistance >= DIAGONAL_MIN_DISTANCE) {
                if (distance >= 0.0f) {
                    AddSideCode(sideCodes[2], code);
                } else {
                    AddSideCode(sideCodes[3], code);
                }
            }
            distance = (-height * columnOffset + rowOffset * width) * inverseLength;
            absDistance = distance < 0.0f ? -distance : distance;
            if (absDistance >= DIAGONAL_MIN_DISTANCE) {
                if (distance >= 0.0f) {
                    AddSideCode(sideCodes[0], code);
                } else {
                    AddSideCode(sideCodes[1], code);
                }
            }
        }
    }
    if (sideCodes[0][1] != NO_TERRAIN_CODE && sideCodes[1][1] != NO_TERRAIN_CODE &&
        (sideCodes[2][1] == NO_TERRAIN_CODE || sideCodes[3][1] == NO_TERRAIN_CODE)) {
        return TRUE;
    }
    return FALSE;
}

/*
 * Toggles the unk992 bit of one cell of the ground. The cells are
 * GROUND_TILE_POINT_SPAN a side in each tile.
 */
void func_80405AF4_hole_editor(s32 column, s32 row) {
    s32 tileIndex;
    s32 cellBits;

    if (column < 0 || column >= GROUND_TILE_COLUMN_COUNT * GROUND_TILE_POINT_SPAN
        || row >= GROUND_TILE_ROW_COUNT * GROUND_TILE_POINT_SPAN || row < 0) {
        return;
    }
    tileIndex = (row >> 2) * GROUND_TILE_COLUMN_COUNT + column / GROUND_TILE_POINT_SPAN;
    cellBits = g_WorkArea.hole.tiles[tileIndex].unk992;
    g_WorkArea.hole.tiles[tileIndex].unk992 = cellBits ^ (1 << ((row & 3) * GROUND_TILE_POINT_SPAN + (column & 3)));
}

/* Sets the x and s of a column of points between grid vertices (column, row) and (column + 1, row) */
void func_80405B80_hole_editor(Vtx *vertices, s32 column, s32 row, s32 weight) {
    Vtx *ends[2];
    s32 firstWeight;
    s32 i;

    firstWeight = EDGE_WEIGHT_MAX - weight;
    ends[0] = GetGridVertex(column, row);
    ends[1] = GetGridVertex(column + 1, row);
    for (i = 0; i < EDGE_POINT_COUNT; i++) {
        Vtx *vertex = &vertices[D_8040C580_hole_editor[i][(column & 1) * 2 + 1]];

        vertex->v.ob[0] = (ends[0]->v.ob[0] * firstWeight + ends[1]->v.ob[0] * weight) / EDGE_WEIGHT_MAX;
        vertex->v.tc[0] = (ends[0]->v.tc[0] * firstWeight + ends[1]->v.tc[0] * weight) / EDGE_WEIGHT_MAX;
    }
}

/* Sets the z and t of a row of points between grid vertices (column, row) and (column, row + 1) */
void func_80405C94_hole_editor(Vtx *vertices, s32 column, s32 row, s32 weight) {
    Vtx *ends[2];
    s32 firstWeight;
    s32 i;

    firstWeight = EDGE_WEIGHT_MAX - weight;
    ends[0] = GetGridVertex(column, row);
    ends[1] = GetGridVertex(column, row + 1);
    for (i = 0; i < EDGE_POINT_COUNT; i++) {
        Vtx *vertex = &vertices[D_8040C580_hole_editor[(row & 1) * 2 + 1][i]];

        vertex->v.ob[2] = (ends[0]->v.ob[2] * firstWeight + ends[1]->v.ob[2] * weight) / EDGE_WEIGHT_MAX;
        vertex->v.tc[1] = (ends[0]->v.tc[1] * firstWeight + ends[1]->v.tc[1] * weight) / EDGE_WEIGHT_MAX;
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
void func_80405DB0_hole_editor(void) {
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
    EDITOR_ASSERT(offsets != NULL, HOLE_EDITOR_SHAPE_ASSERT_LINE);
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
            codeColumn = (point = GetGroundPoint(column, row))->v.ob[AXIS_X] / VERTEX_UNITS_PER_CODE;
            codeRow = point->v.ob[AXIS_Z] / VERTEX_UNITS_PER_CODE;
            code = GetTerrainCode(codeColumn, codeRow);
            if (code == TERRAIN_CODE_GREEN) {
                height = rand() & GREEN_NOISE_MASK;
            } else {
                height = (f32)((s32)IntLength2D(codeColumn - greenColumn, codeRow - greenRow) << GROUND_TEXEL_SHIFT) *
                         HEIGHT_PER_COURSE_UNIT;
                height -= rand() & GROUND_NOISE_MASK;
                if (code == TERRAIN_CODE_BUNKER) {
                    neighborCode = code;
                    for (offsetIndex = 0; offsetIndex < offsetCount; offsetIndex++) {
                        neighborCode = GetTerrainCode(codeColumn + offsets[offsetIndex].column,
                                                      codeRow + offsets[offsetIndex].row);
                        if (neighborCode != TERRAIN_CODE_BUNKER ||
                            offsets[offsetIndex].distance >= BUNKER_INNER_DISTANCE * OFFSET_DISTANCE_SCALE) {
                            break;
                        }
                    }
                    if (neighborCode == TERRAIN_CODE_BUNKER) {
                        height += BUNKER_INNER_OFFSET;
                    }
                } else if (code == TERRAIN_CODE_ROUGH) {
                    for (offsetIndex = 0; offsetIndex < offsetCount; offsetIndex++) {
                        if (GetTerrainCode(codeColumn + offsets[offsetIndex].column,
                                           codeRow + offsets[offsetIndex].row) != TERRAIN_CODE_ROUGH ||
                            offsets[offsetIndex].distance >= ROUGH_INNER_DISTANCE * OFFSET_DISTANCE_SCALE) {
                            break;
                        }
                    }
                    if (offsets[offsetIndex].distance >= ROUGH_INNER_DISTANCE * OFFSET_DISTANCE_SCALE) {
                        height -= (rand() & ROUGH_INNER_NOISE_MASK) + ROUGH_INNER_OFFSET;
                    } else {
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
            tileIndex = row * GROUND_TILE_COLUMN_COUNT + column;
            if (g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex < 0) {
                continue;
            }
            g_WorkArea.hole.tiles[tileIndex].unk990 = 0;
            for (edgeRow = 0; edgeRow < EDGE_POINT_COUNT; edgeRow++) {
                for (edgeColumn = 0; edgeColumn < EDGE_POINT_COUNT; edgeColumn++) {
                    if ((edgeRow & 1) == (edgeColumn & 1)) {
                        continue;
                    }
                    if ((edgeRow & 1) == 0) {
                        edgePoints[0] = GetGroundPoint(column * GROUND_TILE_POINT_SPAN + edgeColumn - 1,
                                                       row * GROUND_TILE_POINT_SPAN + edgeRow);
                        edgePoints[1] = &g_WorkArea.hole.midpoints[g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex]
                                                                  [D_8040C580_hole_editor[edgeRow][edgeColumn]];
                        edgePoints[2] = GetGroundPoint(column * GROUND_TILE_POINT_SPAN + edgeColumn + 1,
                                                       row * GROUND_TILE_POINT_SPAN + edgeRow);
                        edgePoints[1]->v.ob[AXIS_Y] =
                            edgePoints[0]->v.ob[AXIS_Y] +
                            (edgePoints[2]->v.ob[AXIS_Y] - edgePoints[0]->v.ob[AXIS_Y]) *
                                (edgePoints[1]->v.ob[AXIS_X] - edgePoints[0]->v.ob[AXIS_X]) /
                                (edgePoints[2]->v.ob[AXIS_X] - edgePoints[0]->v.ob[AXIS_X]);
                    } else {
                        edgePoints[0] = GetGroundPoint(column * GROUND_TILE_POINT_SPAN + edgeColumn,
                                                       row * GROUND_TILE_POINT_SPAN + edgeRow - 1);
                        edgePoints[1] = &g_WorkArea.hole.midpoints[g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex]
                                                                  [D_8040C580_hole_editor[edgeRow][edgeColumn]];
                        edgePoints[2] = GetGroundPoint(column * GROUND_TILE_POINT_SPAN + edgeColumn,
                                                       row * GROUND_TILE_POINT_SPAN + edgeRow + 1);
                        edgePoints[1]->v.ob[AXIS_Y] =
                            edgePoints[0]->v.ob[AXIS_Y] +
                            (edgePoints[2]->v.ob[AXIS_Y] - edgePoints[0]->v.ob[AXIS_Y]) *
                                (edgePoints[1]->v.ob[AXIS_Z] - edgePoints[0]->v.ob[AXIS_Z]) /
                                (edgePoints[2]->v.ob[AXIS_Z] - edgePoints[0]->v.ob[AXIS_Z]);
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
                    if (GetTerrainCode(column + offsets[offsetIndex].column, row + offsets[offsetIndex].row) !=
                            TERRAIN_CODE_ROUGH ||
                        offsets[offsetIndex].distance >= TREE_MAX_DISTANCE * OFFSET_DISTANCE_SCALE) {
                        break;
                    }
                }
                if (offsets[offsetIndex].distance >= TREE_MIN_DISTANCE * OFFSET_DISTANCE_SCALE &&
                    offsets[offsetIndex].distance < TREE_MAX_DISTANCE * OFFSET_DISTANCE_SCALE) {
                    x = column * VERTEX_UNITS_PER_CODE;
                    z = row * VERTEX_UNITS_PER_CODE;
                    for (treeIndex = 0; treeIndex < treeCount; treeIndex++) {
                        if (IntLength2D(x - treePositions[treeIndex][0], z - treePositions[treeIndex][1]) <
                            TREE_SPACING) {
                            break;
                        }
                    }
                    if (treeIndex == treeCount) {
                        treePositions[treeCount][0] = x;
                        treePositions[treeCount][1] = z;
                        treeCount++;
                        /* The limit is checked at the end of each block; one check compiles differently */
                        if (treeCount == BILLBOARD_COUNT) {
                            goto planted;
                        }
                    }
                    if (treeCount == BILLBOARD_COUNT) {
                        goto planted;
                    }
                }
                if (treeCount == BILLBOARD_COUNT) {
                    goto planted;
                }
            }
            if (treeCount == BILLBOARD_COUNT) {
                goto planted;
            }
        }
    }
planted:
    for (billboardIndex = 0; billboardIndex < treeCount; billboardIndex++) {
        g_Billboards[billboardIndex].y = GetGroundHeight(treePositions[billboardIndex][0] << WORLD_TO_COURSE_SHIFT,
                                                    treePositions[billboardIndex][1] << WORLD_TO_COURSE_SHIFT) /
                                        (1 << WORLD_TO_COURSE_SHIFT) +
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
 * Sets each ground tile's unk992 bits, one for each of its cells, to
 * func_80405870 of the codes between the cell's opposite corners
 */
void func_80406570_hole_editor(void) {
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
            tileIndex = tileRow * GROUND_TILE_COLUMN_COUNT + tileColumn;
            for (cellRow = 0; cellRow < GROUND_TILE_POINT_SPAN; cellRow++) {
                for (cellColumn = 0; cellColumn < GROUND_TILE_POINT_SPAN; cellColumn++) {
                    corners[0] = GetGroundPoint(tileColumn * GROUND_TILE_POINT_SPAN + cellColumn,
                                                tileRow * GROUND_TILE_POINT_SPAN + cellRow);
                    corners[1] = GetGroundPoint(tileColumn * GROUND_TILE_POINT_SPAN + cellColumn + 1,
                                                tileRow * GROUND_TILE_POINT_SPAN + cellRow + 1);
                    cellBits |= func_80405870_hole_editor(corners[0]->v.ob[0] / VERTEX_UNITS_PER_CODE,
                                                          corners[0]->v.ob[2] / VERTEX_UNITS_PER_CODE,
                                                          corners[1]->v.ob[0] / VERTEX_UNITS_PER_CODE,
                                                          corners[1]->v.ob[2] / VERTEX_UNITS_PER_CODE)
                                << (cellRow * GROUND_TILE_POINT_SPAN + cellColumn);
                }
            }
            g_WorkArea.hole.tiles[tileIndex].unk992 = cellBits;
        }
    }
}

/*
 * Returns -1 if either triangle of any of the four ground cells around a
 * ground point has a cross product below MIN_CELL_CROSS_PRODUCT, so is
 * too thin or turned over, or 0 if none does
 */
s32 func_804066C8_hole_editor(s32 column, s32 row) {
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
            if (CrossProductXZ(corners[0]->v.ob[AXIS_X], corners[0]->v.ob[AXIS_Z], corners[1]->v.ob[AXIS_X],
                               corners[1]->v.ob[AXIS_Z], corners[2]->v.ob[AXIS_X],
                               corners[2]->v.ob[AXIS_Z]) < MIN_CELL_CROSS_PRODUCT ||
                CrossProductXZ(corners[1]->v.ob[AXIS_X], corners[1]->v.ob[AXIS_Z], corners[3]->v.ob[AXIS_X],
                               corners[3]->v.ob[AXIS_Z], corners[2]->v.ob[AXIS_X],
                               corners[2]->v.ob[AXIS_Z]) < MIN_CELL_CROSS_PRODUCT) {
                return -1;
            }
            columnOffset++;
        } while (columnOffset <= 0);
        rowOffset++;
    } while (rowOffset <= 0);
    return 0;
}

/*
 * Runs scene 11's menu, which loads a hole with BuildHole or saves the
 * one in memory with SaveHoleFiles. The D-pad picks the action, then the
 * course and the hole; A goes on to the next step and B goes back.
 */
void func_80406818_hole_editor(void) {
    char courseText[DEBUG_TEXT_BUFFER_SIZE];
    char holeText[DEBUG_TEXT_BUFFER_SIZE];

    ClearDebugText();
    if (sHoleFileStep == HOLE_FILE_STEP_ACTION) {
        if (g_MenuControllerData[0].button & (U_JPAD | D_JPAD)) {
            sIsSavingHoleFiles ^= 1;
        }
        if (g_MenuControllerData[0].button & A_BUTTON) {
            sHoleFileStep = HOLE_FILE_STEP_COURSE;
        }
        if (!sIsSavingHoleFiles) {
            SetDebugTextHighlight(TRUE);
            PrintDebugText("->", HOLE_FILE_ARROW_COLUMN, HOLE_FILE_LOAD_ROW);
        }
        if (sIsSavingHoleFiles) {
            SetDebugTextHighlight(TRUE);
            PrintDebugText("->", HOLE_FILE_ARROW_COLUMN, HOLE_FILE_SAVE_ROW);
        }
    } else if (sHoleFileStep == HOLE_FILE_STEP_COURSE) {
        if (g_MenuControllerData[0].button & (L_JPAD | D_JPAD)) {
            sHoleFileCourseNumber--;
        }
        if (g_MenuControllerData[0].button & (R_JPAD | U_JPAD)) {
            sHoleFileCourseNumber++;
        }
        sHoleFileCourseNumber = (sHoleFileCourseNumber + HOLE_FILE_COURSE_COUNT - 1) % HOLE_FILE_COURSE_COUNT + 1;
        if (g_MenuControllerData[0].trigger & A_BUTTON) {
            sHoleFileStep = HOLE_FILE_STEP_HOLE;
        }
        if (g_MenuControllerData[0].trigger & B_BUTTON) {
            sHoleFileStep = HOLE_FILE_STEP_ACTION;
        }
    } else if (sHoleFileStep == HOLE_FILE_STEP_HOLE) {
        if (g_MenuControllerData[0].button & (L_JPAD | D_JPAD)) {
            sHoleFileHoleNumber--;
        }
        if (g_MenuControllerData[0].button & (R_JPAD | U_JPAD)) {
            sHoleFileHoleNumber++;
        }
        sHoleFileHoleNumber = (sHoleFileHoleNumber + COURSE_HOLE_COUNT - 1) % COURSE_HOLE_COUNT + 1;
        if (g_MenuControllerData[0].trigger & A_BUTTON) {
            sHoleFileStep = HOLE_FILE_STEP_CONFIRM;
        }
        if (g_MenuControllerData[0].trigger & B_BUTTON) {
            sHoleFileStep = HOLE_FILE_STEP_COURSE;
        }
    } else if (sHoleFileStep == HOLE_FILE_STEP_CONFIRM) {
        if (sHoleFilePromptFrames & HOLE_FILE_BLINK_BIT) {
            SetDebugTextHighlight(TRUE);
        }
        PrintDebugText("Are you sure? (Exec -> A_BUTTON)", HOLE_FILE_PROMPT_COLUMN, HOLE_FILE_PROMPT_ROW);
        sHoleFilePromptFrames++;
        if (g_MenuControllerData[0].trigger & A_BUTTON) {
            if (sIsSavingHoleFiles) {
                if (g_WorkArea.hole.tiles[0]
                        .texels[GROUND_TILE_TEXEL_ROW_COUNT - 1][GROUND_TILE_TEXEL_ROW_LENGTH - 1]) {
                    ToggleGroundTileBorders();
                }
                SaveHoleFiles(sHoleFileCourseNumber, sHoleFileHoleNumber);
            } else {
                BuildHole(sHoleFileCourseNumber, sHoleFileHoleNumber);
            }
            sHoleFileStep = HOLE_FILE_STEP_ACTION;
        }
        if (g_MenuControllerData[0].trigger & B_BUTTON) {
            sHoleFileStep = HOLE_FILE_STEP_HOLE;
        }
        if (g_MenuControllerData[0].trigger & START_BUTTON) {
            sHoleFileStep = HOLE_FILE_STEP_ACTION;
        }
    }
    if (!sIsSavingHoleFiles) {
        SetDebugTextHighlight(TRUE);
    }
    PrintDebugText("Load HoleData", HOLE_FILE_LABEL_COLUMN, HOLE_FILE_LOAD_ROW);
    if (sIsSavingHoleFiles) {
        SetDebugTextHighlight(TRUE);
    }
    PrintDebugText("Save HoleData", HOLE_FILE_LABEL_COLUMN, HOLE_FILE_SAVE_ROW);
    if (sHoleFileStep >= HOLE_FILE_STEP_COURSE) {
        PrintDebugText("Course : ", HOLE_FILE_ARROW_COLUMN, HOLE_FILE_COURSE_ROW);
        sprintf(courseText, "%d", sHoleFileCourseNumber);
        PrintDebugText(courseText, HOLE_FILE_NUMBER_COLUMN, HOLE_FILE_COURSE_ROW);
    }
    if (sHoleFileStep >= HOLE_FILE_STEP_HOLE) {
        PrintDebugText("Hole   : ", HOLE_FILE_ARROW_COLUMN, HOLE_FILE_HOLE_ROW);
        sprintf(holeText, "%d", sHoleFileHoleNumber);
        PrintDebugText(holeText, HOLE_FILE_NUMBER_COLUMN, HOLE_FILE_HOLE_ROW);
    }
}

/*
 * Scene SCENE_TREES's editor for the billboards that stand for a hole's
 * trees. The editor pad's C-up and C-down change the edited tree's height,
 * C-right and C-left its width, A toggles unk8, up and down on the control
 * pad its texture, and the stick moves it. Right and left on the control pad
 * pick the tree, and B copies it. R zooms in, L or Z with R zooms out, and Z
 * with the stick orbits the camera around the tree.
 */
void func_80406C3C_hole_editor(void) {
    Mat4 translation;
    Mat4 unusedMatrix; /* The game's frame has room for a second matrix */
    s32 editKind;
    s32 i;
    s32 height;
    f64 cameraDistance; /* As a literal, GCC would fold it into the angles before the zoom */

    PrintDebugText("KIND", TREE_TEXT_COLUMN, TREE_KIND_LABEL_ROW);
    PrintDebugText("        ", TREE_TEXT_COLUMN, TREE_KIND_TEXT_ROW);
    PrintDebugHex(g_Billboards[sEditedTreeIndex].textureIndex, TREE_TEXT_COLUMN, TREE_KIND_TEXT_ROW);
    PrintDebugText("EDIT NUM", TREE_TEXT_COLUMN, TREE_INDEX_LABEL_ROW);
    PrintDebugText("        ", TREE_TEXT_COLUMN, TREE_INDEX_TEXT_ROW);
    PrintDebugHex(sEditedTreeIndex, TREE_TEXT_COLUMN, TREE_INDEX_TEXT_ROW);
    D_800C30D1 = FALSE;
    editKind = TRIANGLE_EDIT_NONE;
    if (g_ControllerData[EDITOR_PAD].button & U_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[sEditedTreeIndex].height++;
    }
    if (g_ControllerData[EDITOR_PAD].button & D_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[sEditedTreeIndex].height--;
    }
    if (g_ControllerData[EDITOR_PAD].button & R_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[sEditedTreeIndex].width++;
    }
    if (g_ControllerData[EDITOR_PAD].button & L_CBUTTONS) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[sEditedTreeIndex].width--;
    }
    if (g_Billboards[sEditedTreeIndex].height > TREE_SIZE_MAX) {
        g_Billboards[sEditedTreeIndex].height = TREE_SIZE_MAX;
    }
    if (g_Billboards[sEditedTreeIndex].height == 0) {
        g_Billboards[sEditedTreeIndex].height = 1;
    }
    if (g_Billboards[sEditedTreeIndex].width > TREE_SIZE_MAX) {
        g_Billboards[sEditedTreeIndex].width = TREE_SIZE_MAX;
    }
    if (g_Billboards[sEditedTreeIndex].width == 0) {
        g_Billboards[sEditedTreeIndex].width = 1;
    }
    if (g_MenuControllerData[EDITOR_PAD].button & A_BUTTON) {
        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[sEditedTreeIndex].unk8 = !g_Billboards[sEditedTreeIndex].unk8;
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & R_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        g_Billboards[sEditedTreeIndex].isNearCourseEdge = FALSE;
        sEditedTreeIndex++;
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & L_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_SELECT;
        g_Billboards[sEditedTreeIndex].isNearCourseEdge = FALSE;
        sEditedTreeIndex--;
    }
    if (sEditedTreeIndex < 0) {
        sEditedTreeIndex = BILLBOARD_COUNT - 1;
    }
    if (sEditedTreeIndex >= BILLBOARD_COUNT) {
        sEditedTreeIndex = 0;
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & U_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_KIND;
        g_Billboards[sEditedTreeIndex].textureIndex++;
    }
    if ((g_MenuControllerData[EDITOR_PAD].button & D_JPAD) &&
        (g_MenuRepeatStates[EDITOR_PAD].stickDirectionX | g_MenuRepeatStates[EDITOR_PAD].stickDirectionY) == 0) {
        editKind = TRIANGLE_EDIT_KIND;
        g_Billboards[sEditedTreeIndex].textureIndex--;
    }
    if (g_Billboards[sEditedTreeIndex].textureIndex >= BILLBOARD_TEXTURE_COUNT) {
        g_Billboards[sEditedTreeIndex].textureIndex = 0;
    }
    if (g_Billboards[sEditedTreeIndex].textureIndex < 0) {
        g_Billboards[sEditedTreeIndex].textureIndex = BILLBOARD_TEXTURE_COUNT - 1;
    }
    /* B copies the tree into the first free billboard and edits the copy */
    i = 0;
    if (g_MenuControllerData[EDITOR_PAD].button & B_BUTTON) {
        for (; i < BILLBOARD_COUNT; i++) {
            if (g_Billboards[i].x <= 0 || g_Billboards[i].x >= TREE_X_LIMIT || g_Billboards[i].z <= 0 ||
                g_Billboards[i].z >= TREE_Z_LIMIT) {
                break;
            }
        }
        if (i != BILLBOARD_COUNT) {
            g_Billboards[i] = g_Billboards[sEditedTreeIndex];
            g_Billboards[i].x += COPIED_TREE_OFFSET;
            editKind = TRIANGLE_EDIT_SELECT;
            g_Billboards[sEditedTreeIndex].isNearCourseEdge = FALSE;
            sEditedTreeIndex = i;
        }
    }
    /* The u16 holds stick_x and stick_y */
    if (!(g_ControllerData[EDITOR_PAD].button & Z_TRIG) && *(u16 *)&g_MenuControllerData[EDITOR_PAD].stick_x != 0) {
        s32 moveX = -cos((u16)sTreeCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) *
                        g_MenuControllerData[EDITOR_PAD].stick_y -
                    sin((u16)sTreeCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) *
                        g_MenuControllerData[EDITOR_PAD].stick_x;
        s32 moveZ = -sin((u16)sTreeCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) *
                        g_MenuControllerData[EDITOR_PAD].stick_y +
                    cos((u16)sTreeCameraHeadingAngle * BINARY_ANGLE_TO_RADIANS) *
                        g_MenuControllerData[EDITOR_PAD].stick_x;

        editKind = TRIANGLE_EDIT_MOVE;
        g_Billboards[sEditedTreeIndex].x += moveX;
        g_Billboards[sEditedTreeIndex].z += moveZ;
    }
    /* The translation goes unused; PlaceBillboards places every tree again */
    if (editKind != TRIANGLE_EDIT_NONE) {
        height = GetGroundHeight(g_Billboards[sEditedTreeIndex].x << WORLD_TO_COURSE_SHIFT,
                                 g_Billboards[sEditedTreeIndex].z << WORLD_TO_COURSE_SHIFT) /
                 (1 << WORLD_TO_COURSE_SHIFT);
        guTranslateF(translation, g_Billboards[sEditedTreeIndex].x, height + 1,
                     g_Billboards[sEditedTreeIndex].z);
        RevertBillboardManager();
        PlaceBillboards();
    } else {
        /* Blinks the tree, as drawing skips billboards near the course edge */
        g_Billboards[sEditedTreeIndex].isNearCourseEdge = !g_Billboards[sEditedTreeIndex].isNearCourseEdge;
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
        sTreeCameraHeadingAngle -= g_MenuControllerData[EDITOR_PAD].stick_x / STICK_STEP_SIZE * HEADING_STICK_SPEED;
        sTreeCameraPitch -= g_MenuControllerData[EDITOR_PAD].stick_y / STICK_STEP_SIZE * PITCH_STICK_SPEED;
    }
    if (sTreeCameraPitch > 0.0f) {
        sTreeCameraPitch = 0.0f;
    }
    if (sTreeCameraPitch < EDITOR_PITCH_MIN) {
        sTreeCameraPitch = EDITOR_PITCH_MIN;
    }
    cameraDistance = EDITOR_CAMERA_DISTANCE;
    g_MapCamera.view.lookAt[AXIS_X] = g_Billboards[sEditedTreeIndex].x;
    g_MapCamera.view.lookAt[AXIS_Y] = g_Billboards[sEditedTreeIndex].y;
    sLastTreeEditKind = editKind;
    g_MapCamera.view.lookAt[AXIS_Z] = g_Billboards[sEditedTreeIndex].z;
    sTreeCameraHeading = sTreeCameraHeadingAngle * EDITOR_ANGLE_TO_RADIANS;
    g_MapCamera.view.eye[AXIS_X] = cos(sTreeCameraPitch) * cos(sTreeCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance) +
                                   g_MapCamera.view.lookAt[AXIS_X];
    g_MapCamera.view.eye[AXIS_Z] = cos(sTreeCameraPitch) * sin(sTreeCameraHeading) *
                                       (g_MapCamera.view.zoom * cameraDistance) +
                                   g_MapCamera.view.lookAt[AXIS_Z];
    g_MapCamera.view.eye[AXIS_Y] = sin(sTreeCameraPitch) * (g_MapCamera.view.zoom * cameraDistance) +
                                   g_MapCamera.view.lookAt[AXIS_Y];
}

/*
 * Blends the ground points within a radius of a ground point toward its
 * height: the farther a point, the more it keeps its own height, fully
 * from BLEND_DISTANCE_SCALE / falloff away. Nothing calls it.
 */
void func_804077B0_hole_editor(u32 column, u32 row, s32 radius, s32 falloff) {
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
            halfWidth = radiusSquared - rowOffset * rowOffset;
            if (halfWidth < 0) {
                halfWidth = 0;
            }
            halfWidth = sqrt(halfWidth);
            for (columnOffset = -halfWidth; columnOffset <= halfWidth; columnOffset++) {
                if (column + columnOffset < GROUND_POINT_COLUMN_COUNT) {
                    point = GetGroundPoint(column + columnOffset, row + rowOffset);
                    distance = (f32)IntLength2D(center->v.ob[AXIS_X] - point->v.ob[AXIS_X],
                                                center->v.ob[AXIS_Z] - point->v.ob[AXIS_Z]) /
                               BLEND_DISTANCE_SCALE;
                    weight = distance * falloff;
                    if (weight > 1.0f) {
                        weight = 1.0f;
                    }
                    weight *= weight;
                    SetGroundPointHeightAndShade(column + columnOffset, row + rowOffset,
                                                 point->v.ob[AXIS_Y] * weight + center->v.ob[AXIS_Y] * (1.0f - weight),
                                                 BLENDED_POINT_SHADE);
                }
            }
        }
    }
}

/*
 * Fills sTriangleSlopes from the ground. It goes over the cells in blocks
 * of 2 columns by 4 rows; each cell's two triangles take an entry each.
 */
void func_804079B8_hole_editor(void) {
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
                block = half * SLOPE_BLOCK_COLUMN_COUNT + blockColumn;
                for (pairRow = 0; pairRow < 2; pairRow++) {
                    cellRow = (blockRow * 2 + pairRow) * 2;
                    for (cellRowInPair = 0; cellRowInPair < 2; cellRowInPair++) {
                        for (cellColumnInBlock = 0; cellColumnInBlock < 2; cellColumnInBlock++) {
                            row = cellRow + cellRowInPair;
                            column = block * 2 + cellColumnInBlock;
                            corner = GetGroundPoint(column, row);
                            nextColumnCorner = GetGroundPoint(column + 1, row);
                            nextRowCorner = GetGroundPoint(column, row + 1);
                            oppositeCorner = GetGroundPoint(column + 1, row + 1);
                            firstTriangle.corners[AXIS_Y][0] = corner->v.ob[AXIS_Y];
                            secondTriangle.corners[AXIS_Y][0] = firstTriangle.corners[AXIS_Y][1] =
                                nextColumnCorner->v.ob[AXIS_Y];
                            secondTriangle.corners[AXIS_Y][2] = firstTriangle.corners[AXIS_Y][2] =
                                nextRowCorner->v.ob[AXIS_Y];
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
                            sTriangleSlopes[slopeIndex].normalX = plane.normal[AXIS_X] * 2;
                            sTriangleSlopes[slopeIndex].normalZ = plane.normal[AXIS_Z] * 2;
                            sTriangleSlopes[slopeIndex].shade = IntLength2D(plane.normal[AXIS_X], plane.normal[AXIS_Z]);
                            if (sTriangleSlopes[slopeIndex].shade != 0) {
                                if (sTriangleSlopes[slopeIndex].shade < SLOPE_SHADE_MAX - SLOPE_SHADE_OFFSET) {
                                    sTriangleSlopes[slopeIndex].shade += SLOPE_SHADE_OFFSET;
                                } else {
                                    sTriangleSlopes[slopeIndex].shade = SLOPE_SHADE_MAX;
                                }
                            }
                            slopeIndex++;
                            GetTriangleIntegerPlane(&secondTriangle, &plane);
                            sTriangleSlopes[slopeIndex].normalX = plane.normal[AXIS_X] * 2;
                            sTriangleSlopes[slopeIndex].normalZ = plane.normal[AXIS_Z] * 2;
                            sTriangleSlopes[slopeIndex].shade = IntLength2D(plane.normal[AXIS_X], plane.normal[AXIS_Z]);
                            if (sTriangleSlopes[slopeIndex].shade != 0) {
                                if (sTriangleSlopes[slopeIndex].shade < SLOPE_SHADE_MAX - SLOPE_SHADE_OFFSET) {
                                    sTriangleSlopes[slopeIndex].shade += SLOPE_SHADE_OFFSET;
                                } else {
                                    sTriangleSlopes[slopeIndex].shade = SLOPE_SHADE_MAX;
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
 * Fills D_80491A60 with the cube of cos(column * pi/4) * sin(row * pi/4),
 * as matching IA8 intensity and alpha levels from 1 to 15
 */
void func_80407D64_hole_editor(void) {
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
            D_80491A60[texelIndex++] = (level << IA8_ALPHA_SHIFT) | level;
        }
    }
}

/*
 * Writes a hex dump of words to the host as a text file: lines of four
 * words, each line led by its byte offset. Nothing calls it.
 */
void func_80407F08_hole_editor(u32 *words, s32 byteCount, char *fileName) {
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
    EDITOR_ASSERT(buffer != NULL, HOLE_EDITOR_HEX_DUMP_ASSERT_LINE);
    for (line = 0; line < (byteCount + HEX_DUMP_LINE_BYTES - 1) / HEX_DUMP_LINE_BYTES; line++) {
        lineWords = words;
        sprintf(text, "%4.4X: ", line * HEX_DUMP_LINE_BYTES);
        AppendText(text);
        for (wordIndex = 0; wordIndex < HEX_DUMP_LINE_WORDS; wordIndex++) {
            sprintf(text, "%8.8X ", lineWords[line * HEX_DUMP_LINE_WORDS + wordIndex]);
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

/*
 * Doesn't match: the original opens with an 8-byte frame and stores an unset
 * $v0 into it, and it clears the flag only after loading ob[2].
 */
#ifdef NON_MATCHING
void GetMidpointVertex(Vtx_t *first, Vtx_t *second, Vtx_t *midpoint) {
    s32 unused;

    midpoint->ob[0] = (first->ob[0] + second->ob[0]) / 2;
    midpoint->ob[1] = (first->ob[1] + second->ob[1]) / 2;
    midpoint->flag = 0;
    midpoint->ob[2] = (first->ob[2] + second->ob[2]) / 2;
    midpoint->tc[0] = (first->tc[0] + second->tc[0]) / 2;
    midpoint->tc[1] = (first->tc[1] + second->tc[1]) / 2;
    midpoint->cn[0] = (first->cn[0] + second->cn[0]) / 2;
    midpoint->cn[1] = (first->cn[1] + second->cn[1]) / 2;
    midpoint->cn[2] = (first->cn[2] + second->cn[2]) / 2;
    midpoint->cn[3] = (first->cn[3] + second->cn[3]) / 2;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", GetMidpointVertex);
#endif

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80408170_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804083E0_hole_editor);

/*
 * Marks the billboards on the map, then writes the whole map image to
 * the host as x:\usgolf64\capture\tempNN.bmp
 */
void func_8040BFC8_hole_editor(void) {
    char fileName[CAPTURE_FILE_NAME_SIZE];
    u8 *capture;
    s32 byteIndex;
    s32 row;
    s32 column;

    u16 GetMapTexel(u32 texelIndex) {
        u32 column = texelIndex % MAP_CAPTURE_WIDTH;
        u32 row = texelIndex / MAP_CAPTURE_WIDTH % MAP_CAPTURE_HEIGHT;

        /* The game indexes the texels past the border as one run of rows */
        return g_WorkArea.hole.tiles[column / GROUND_TILE_TEXEL_SPAN +
                                     row / GROUND_TILE_TEXEL_SPAN * GROUND_TILE_COLUMN_COUNT]
            .texels[GROUND_TILE_TEXEL_BORDER][GROUND_TILE_TEXEL_BORDER + column % GROUND_TILE_TEXEL_SPAN +
                                              row % GROUND_TILE_TEXEL_SPAN * GROUND_TILE_TEXEL_ROW_LENGTH];
    }

    MarkBillboardsOnMap();
    capture = AllocMemory(MAP_CAPTURE_SIZE);
    for (byteIndex = 0; byteIndex != CAPTURE_HEADER_SIZE; byteIndex++) {
        capture[byteIndex] = sMapCaptureHeader[byteIndex];
    }
    for (row = MAP_CAPTURE_HEIGHT - 1; row != -1; row--) {
        for (column = 0; column != MAP_CAPTURE_WIDTH; column++) {
            capture[byteIndex++] = ((GetMapTexel(row * MAP_CAPTURE_WIDTH + column) >> RGBA16_BLUE_SHIFT) &
                                    RGBA16_CHANNEL_MASK)
                                   << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] = ((GetMapTexel(row * MAP_CAPTURE_WIDTH + column) >> RGBA16_GREEN_SHIFT) &
                                    RGBA16_CHANNEL_MASK)
                                   << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] = ((GetMapTexel(row * MAP_CAPTURE_WIDTH + column) >> RGBA16_RED_SHIFT) &
                                    RGBA16_CHANNEL_MASK)
                                   << RGBA16_CHANNEL_WIDEN_SHIFT;
        }
    }
    sprintf(fileName, "x:\\usgolf64\\capture\\temp%02d.bmp", sMapCaptureNumber);
    WriteHostFile(fileName, MAP_CAPTURE_SIZE);
    FreeMemory(&capture);
    sMapCaptureNumber++;
}

/* Writes the whole frame to the host as the next numbered capture file */
void func_8040C124_hole_editor(void) {
    u16 *frame;
    u8 *capture;
    s32 byteIndex;
    s32 row;
    s32 column;

    frame = (u16 *)((u8 *)nuGfxCfb_ptr + KSEG1_OFFSET);
    capture = AllocMemory(SCREEN_CAPTURE_SIZE);
    for (byteIndex = 0; byteIndex != CAPTURE_HEADER_SIZE; byteIndex++) {
        capture[byteIndex] = sScreenCaptureHeader[byteIndex];
    }
    for (row = SCREEN_HEIGHT - 1; row != -1; row--) {
        for (column = 0; column != SCREEN_WIDTH; column++) {
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_BLUE_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_GREEN_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_RED_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
        }
    }
    sprintf(g_PrintBuffer, "x:/usgolf64/capture/cap%02d.bmp", sScreenCaptureNumber);
    WriteHostFile(g_PrintBuffer, SCREEN_CAPTURE_SIZE);
    FreeMemory(&capture);
    sScreenCaptureNumber++;
}

/*
 * Writes the frame's top-left corner to the host as capture file
 * captureIndex. The size written is that of a whole-screen capture.
 */
void func_8040C260_hole_editor(s32 captureIndex) {
    u16 *frame;
    u8 *capture;
    s32 byteIndex;
    s32 row;
    s32 column;

    capture = g_WorkArea.bytes;
    frame = (u16 *)((u8 *)nuGfxCfb_ptr + KSEG1_OFFSET);
    for (byteIndex = 0; byteIndex != CAPTURE_HEADER_SIZE; byteIndex++) {
        capture[byteIndex] = sCornerCaptureHeader[byteIndex];
    }
    /* BMP rows run from the bottom up, and each pixel is blue, green, red */
    for (row = CORNER_CAPTURE_HEIGHT - 1; row != -1; row--) {
        for (column = 0; column != CORNER_CAPTURE_WIDTH; column++) {
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_BLUE_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_GREEN_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_RED_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
        }
    }
    sprintf(g_PrintBuffer, "c:/golf/capture/temp%02d.bmp", captureIndex);
    WriteHostFile(g_PrintBuffer, SCREEN_CAPTURE_SIZE);
}

/*
 * Draws a pole, red at the foot and blue at the top, at the map camera's
 * look-at point
 */
void func_8040C360_hole_editor(CameraMatrices *matrices, Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    guTranslate(&D_8040CC98, g_MapCamera.view.lookAt[0], g_MapCamera.view.lookAt[1], g_MapCamera.view.lookAt[2]);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8040CC98), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(gfx++, sPoleVertices[0], POLE_QUAD_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 1, 3, 2, 0);
    gSPVertex(gfx++, sPoleVertices[1], POLE_QUAD_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 1, 3, 2, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
