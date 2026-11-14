#include "collision.h"

#include <math.h>
#include <ultra64.h>

#include "ball_simulation.h"
#include "billboard.h"
#include "compiler.h"
#include "course_space.h"
#include "debug_text.h"
#include "game_flags.h"
#include "game_state.h"
#include "geometry.h"
#include "hole_triangles.h"
#include "language.h"
#include "main.h"
#include "math_util.h"
#include "pin.h"
#include "terrain.h"
#include "work_area.h"

#define BILLBOARD_MASK_COUNT 41
#define BILLBOARD_MASK_CELL_COUNT (BILLBOARD_MASK_SIZE * BILLBOARD_MASK_SIZE)
#define BILLBOARD_MASK_INDEX_COUNT 64
#define BILLBOARD_CELL_LAST 15
#define BILLBOARD_CENTER_CELL_WEIGHT 4

#define NO_HIT_FRACTION 1.1f

#define CUP_EDGE_DISTANCE 614.4f

#define PIN_COUNT 1
#define FLAGSTICK_RADIUS 614.4f
#define FLAGSTICK_RADIUS_SQUARED 377487.38f
#define FLAG_REACH 1228.8f
#define FLAG_BOTTOM_HEIGHT 39936.0f
#define FLAG_HIT_SPEED_Y (-524)
#define FLAGSTICK_MOST_BOUNCE 0.8f
#define FLAGSTICK_LEAST_BOUNCE 0.2f

#define WHOLE_UNIT_SIZE (1 << WORLD_TO_COURSE_SHIFT)
#define PATH_MOST_RISE 153.6f
#define PLANE_TOUCH_DISTANCE 15.36f
#define NO_HOLE_TRIANGLE (-1)
#define GROUND_TILE_TRIANGLE_COUNT (GROUND_TILE_POINT_SPAN * GROUND_TILE_POINT_SPAN * 2)
#define BARE_CROSSING_DISTANCE 786.0f
#define TILE_SEARCH_STEP_COUNT 10
#define LEVEL_TRIANGLE_SIZE 0x100

#define WALL_TOUCH_DISTANCE 460.80002f
#define WALL_PUSH_DISTANCE 476.16f
#define WALL_HEIGHT_SLACK 0xC00
#define CORNER_LEAST_DISTANCE 1.536f
#define CORNER_DEFAULT_PUSH_X 0.2f

#define WALL_REACH_DISTANCE 384.0f
#define WALL_HIT_DISTANCE 399.36f
#define WALL_END_DISTANCE 307.2f
#define WALL_WIDENING 0.15f
#define WallWidening(axisCorners, farCorner, edgeLength) \
    (((axisCorners)[0] - (axisCorners)[farCorner]) * WALL_WIDENING / (edgeLength))
#define FARTHEST_CORNER_DISTANCE 153600.0f
#define CORNER_HIT_FRACTION (-0.1f)
#define WALL_DEBUG_COLUMN 10
#define WALL_DEBUG_ROW 10

#define EDGE_SIDE_POSITIVE 1
#define EDGE_SIDE_NEGATIVE 2
#define EDGE_SIDE_ON (EDGE_SIDE_POSITIVE | EDGE_SIDE_NEGATIVE)

#define RING_HIT_RADIUS_PER_SCALE 0.1274414f
#define NO_RING_NUMBER (-1)
#define NO_PLANE_HIT_FRACTION 2.0f
#define PATH_DEBUG_COLUMN 2
#define PATH_DEBUG_ROW 8
#define PATH_DEBUG_CLEAR_COLUMN 25
#define PATH_DEBUG_CLEAR_ROW 3

enum TriangleFacing {
    TRIANGLE_FACING_NONE = -1,
    TRIANGLE_FACING_UP,
    TRIANGLE_FACING_X,
    TRIANGLE_FACING_Z
};

typedef s32 TriangleFacing;

#define GetPlaneSide(plane, x, y, z) \
    (((plane).normal[AXIS_X] * (x)) + ((plane).normal[AXIS_Y] * (y)) + \
        ((plane).normal[AXIS_Z] * (z)) + ((plane).distance * WORLD_TO_COURSE_SCALE))

#define IsTriangleBelowPath(isFirstBelow, isSecondBelow, isThirdBelow) \
    ((isFirstBelow) && (isSecondBelow) && (isThirdBelow))
#define IsLessThanCorners(value, corners) \
    (((value) < (corners)[0] * WORLD_TO_COURSE_SCALE) && \
        ((value) < (corners)[1] * WORLD_TO_COURSE_SCALE) && \
        ((value) < (corners)[2] * WORLD_TO_COURSE_SCALE))
#define IsMoreThanCorners(value, corners) \
    (((corners)[0] * WORLD_TO_COURSE_SCALE < (value)) && \
        ((corners)[1] * WORLD_TO_COURSE_SCALE < (value)) && \
        ((corners)[2] * WORLD_TO_COURSE_SCALE < (value)))

#define IsOffGroundSigned(x, z) ((x) < 0 || (x) >= GROUND_WIDTH || (z) < 0 || (z) >= GROUND_LENGTH)

static inline Boolean IsOffGround(s32 x, s32 z) {
    return ((u32)x >= GROUND_WIDTH) || ((u32)z >= GROUND_LENGTH);
}

static inline Boolean IsOffMiniGolfGround(s32 x, s32 z) {
    return ((u32)x >= MINI_GOLF_GROUND_SIZE) || ((u32)z >= MINI_GOLF_GROUND_SIZE);
}

#define HaveSameSign(first, second) \
    ((((first) >= 0) && ((second) >= 0)) || (((first) < 0) && ((second) < 0)))

#define DoesPathMeetPlane(startSide, endSide) \
    ((fabsf(endSide) < PLANE_TOUCH_DISTANCE) || \
        (((startSide) > 0.0f) & ((endSide) < 0.0f)) || \
        (((startSide) < 0.0f) & ((endSide) > 0.0f)))

#define DoesPathReachWall(startSide, endSide) \
    ((fabsf(endSide) < WALL_REACH_DISTANCE) || \
        (((startSide) >= 0.0f) & ((endSide) < 0.0f)) || \
        (((startSide) < 0.0f) & ((endSide) > 0.0f)))

#define DoesPathMeetRingPlane(startSide, endSide) \
    (((endSide) == 0.0f) || \
        (((startSide) > 0.0f) && ((endSide) < 0.0f)) || \
        (((startSide) < 0.0f) && ((endSide) > 0.0f)))

#define IsBareCrossing(simulation, startSide, endSide) \
    (((simulation)->state == BALL_STATE_ROLLING) && \
        (fabsf((f32)((startSide) - (endSide))) < BARE_CROSSING_DISTANCE))

static s8 f_BillboardMasks[BILLBOARD_MASK_COUNT][BILLBOARD_MASK_CELL_COUNT] NONSTRING = {
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "2323232300000000"
        "1122222230000000"
        "1122111123000000"
        "1111112222320000"
        "1111222223222000"
        "1112223232223200"
        "1112322223000000"
        "1110000000000000"
        "1110000000000000"
        "1110000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "2200000000000000"
        "1223230000000000"
        "1122223230000000"
        "1122111123000000"
        "1111112222320000"
        "1111222323222000"
        "1112222222323200"
        "1112232300000000"
        "1110000000000000"
        "1110000000000000"
        "1111000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "1232223220000000"
        "1123222222230000"
        "1132222111223200"
        "1122111122232000"
        "1111112232322200"
        "1111132000000000"
        "1110000000000000"
        "1100000000000000"
        "1100000000000000"
        "1100000000000000"
        "1110000000000000"
        "1111000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "1232323230000000"
        "1122211123232000"
        "1111111222323200"
        "1111222222232000"
        "1112211232323200"
        "1111000000000000"
        "1110000000000000"
        "1110000000000000"
        "1110000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "3223230000000000"
        "2232222232000000"
        "1222111223200000"
        "1111122232322000"
        "1122222323232300"
        "1123223232323200",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "2200000000000000"
        "1222232000000000"
        "1123223230000000"
        "1122222223230000"
        "1122111222223000"
        "1111122232322300"
        "1112223222220000"
        "1122220000000000"
        "1000000000000000"
        "1000000000000000"
        "1000000000000000"
        "1100000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "2300000000000000"
        "1200000000000000"
        "1230000000000000"
        "1123000000000000"
        "1122011230000000"
        "1121112300000000"
        "1111222220000000"
        "1111111232300000"
        "1122221112230000"
        "1123002223000000"
        "1000000000000000"
        "1000000000000000"
        "1000000000000000"
        "1000000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "5555550000000000"
        "5555555555000000"
        "5555555555555500"
        "5555555555000000"
        "5555500000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "5555500000000000"
        "5555555550000000"
        "5555555555000000"
        "5555555555555500"
        "5555555555000000"
        "5555500000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "5555555000000000"
        "5555555555500000"
        "5555555555000000"
        "5555555555555500"
        "5555555555000000"
        "5555500000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "4444444400000000"
        "4444444444000000"
        "4444444444440000"
        "4444444444440000"
        "4444444444440000"
        "4444444444440000"
        "4444444400000000"
        "4444400000000000"
        "4444000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "4400000000000000"
        "4400000000000000"
        "4440000000000000"
        "4440000000000000"
        "4444000000000000"
        "4444400000000000"
        "4444400000000000"
        "4444400000000000"
        "4444400000000000"
        "4444000000000000"
        "4440000000000000"
        "4440000000000000"
        "4440000000000000"
        "4440000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "4400000000000000"
        "4400000000000000"
        "4440000000000000"
        "4440000000000000"
        "4444000000000000"
        "4444400000000000"
        "4444400000000000"
        "4444400000000000"
        "4444400000000000"
        "4444000000000000"
        "4440000000000000"
        "4440000000000000"
        "4440000000000000"
        "4440000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "4444440000000000"
        "4444444400000000"
        "4444444400040000"
        "4444444044444000"
        "4444444444444000"
        "4444444000000000"
        "4444440000000000"
        "4440000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "1111110000000000"
        "1111111110000000"
        "1111111111000000"
        "1111111111111100"
        "1111111111111000"
        "1111111110000000"
        "1111111111100000"
        "1111111111111000",
    },
    {
        "0000000000000000"
        "2000020000000000"
        "3232323232323000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111110000000",
    },
    {
        "0000000000000000"
        "2000020000000000"
        "3232323232323000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111110000000",
    },
    {
        "0000000000000000"
        "2000020000000000"
        "3232323232323000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111111111000"
        "1111111110000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "4444444000000000"
        "4444444444000000"
        "4444444444440000"
        "4444400000000000"
        "4444400000000000"
        "4444400000000000"
        "4444440000000000"
        "4444400000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "4444444000000000"
        "4444444444000000"
        "4444444444440000"
        "4444400000000000"
        "4444400000000000"
        "4444400000000000"
        "4444440000000000"
        "4444400000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0232322323000000"
        "2222222222320000"
        "1223223232232300"
        "1122233222222300"
        "1123300032322200"
        "1100000000332300"
        "1000000000000000"
        "1100000000000000"
        "1100000000000000"
        "1110000000000000"
        "1110000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "3333333300000000"
        "3232323233000000"
        "2222232323233000"
        "2222323323330000"
        "2222000000000000"
        "1112000000000000"
        "1111000000000000"
        "1111000000000000"
        "1111000000000000"
        "1111000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "3333333000000000"
        "2323232330000000"
        "3232323323330000"
        "2222222330333300"
        "1112333230000000"
        "1100000330000000"
        "1110000000000000"
        "1110000000000000"
        "1100000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "4444400000000000"
        "4444444444000000"
        "4444444444444000"
        "4444444444444000"
        "4444444444000000"
        "4444400000000000"
        "4444000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "4444444400000000"
        "4444444444440000"
        "4444444444444000"
        "4444444444444000"
        "4444444444400000"
        "4444440000000000"
        "4444400000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "4444440000000000"
        "4444444444000000"
        "4444444444440000"
        "4444444440000000"
        "4444000000000000"
        "4000000000000000"
        "4000000000000000"
        "4000444444440000"
        "4444444440000000"
        "4444000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "4444444440000000"
        "4444444444000000"
        "4444444444000000"
        "4444444000000000"
        "4400444440000000"
        "4444440000000000"
        "4444000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "4444440000000000"
        "4444444444400000"
        "4444444444000000"
        "4444444400000000"
        "4444440000000000"
        "4000444400000000"
        "4444440000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "4400000000000000"
        "4444444000000000"
        "4444444444400000"
        "4444444444400000"
        "4444444440000000"
        "4444444444400000"
        "4400044440000000"
        "4400000000000000"
        "4400044440000000"
        "4444444000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "3223232300000000"
        "2232222232000000"
        "1222111223200000"
        "1111122232320000"
        "1122222323232000"
        "1123223232323000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "1111111111111100"
        "1111111111111100"
        "1111111111111100"
        "1111111111111100"
        "1000000000000000"
        "1000000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "1111111111111100"
        "1111111111111100"
        "1111111111111100"
        "1111111111111100"
        "1000000000000000"
        "1000000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "1111111111111100"
        "1111111111111100"
        "1111111111111100"
        "1111111111111100"
        "1000000000000000"
        "1000000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "1111111111111100"
        "1111111111111100"
        "1111111111111100"
        "1111111111111100"
        "1000000000000000"
        "1000000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "1111111111111100"
        "1111111111111100"
        "1111111111111100"
        "1111111111111100"
        "1000000000000000"
        "1000000000000000",
    },
    {
        "0000000000000000"
        "1111111111000000"
        "1111111111000000"
        "1111111111000000"
        "1111111111000000"
        "1000000000000000"
        "1000000000000000"
        "1000000000000000"
        "1000000000000000"
        "1000000000000000"
        "1000000000000000"
        "1000000000000000"
        "1000000000000000"
        "1000000000000000"
        "1000000000000000"
        "1000000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "4444000000000000"
        "4444444444000000"
        "4444444444440000"
        "4444444444444400"
        "4444444444444400"
        "4444444444444000"
        "4444444444440000"
        "4444444444444400"
        "0004444444440000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "4444000000000000"
        "4444444444000000"
        "4444444444440000"
        "4444444444444400"
        "4444444444444400"
        "4444444444444000"
        "4444444444440000"
        "4444444444444400"
        "0004444444440000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "3333333000000000"
        "2323232333300000"
        "2222232323233000"
        "2222223232322300"
        "1222232323233300"
        "1122333232333300"
        "1110000333000000"
        "1100000000000000"
        "1111000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "3333333000000000"
        "2323232333300000"
        "2222232323233000"
        "2222223232322300"
        "1222232323233300"
        "1122333232333300"
        "1110000333000000"
        "1100000000000000"
        "1111000000000000",
    },
    {
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "0000000000000000"
        "3333333000000000"
        "2323232333300000"
        "2222232323233000"
        "2222223232322300"
        "1222232323233300"
        "1122333232333300"
        "1110000333000000"
        "1100000000000000"
        "1111000000000000",
    },
};
/* clang-format off */
static u8 f_BillboardMaskIndices[BILLBOARD_MASK_INDEX_COUNT] = {
       0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
      11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
      27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};
/* clang-format on */
static s32 f_UnusedWordsAfterBillboardMasks[4] UNUSED = {0};

static inline s8 GetBillboardMaskCell(s32 textureIndex, s32 row, s32 column) {
    return f_BillboardMasks[f_BillboardMaskIndices[textureIndex & (BILLBOARD_MASK_INDEX_COUNT - 1)]]
                           [(row * BILLBOARD_MASK_SIZE) | column];
}

const char g_PathDebugBlankText[] = "          ";

Boolean FindMiniGolfGroundHit(
    BallSimulation *simulation,
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hit,
    f32 *fraction
) {
    Vtx *points[GROUND_TILE_POINT_SPAN + 1][GROUND_TILE_POINT_SPAN + 1];
    Boolean8 isBelowPath[GROUND_TILE_POINT_SPAN + 1][GROUND_TILE_POINT_SPAN + 1];
    PlaneTriangle triangles[GROUND_TILE_TRIANGLE_COUNT];
    TrianglePlane plane;
    s32 tileX;
    s32 tileZ;
    s32 endTileX;
    s32 endTileZ;
    s32 nextTileX;
    s32 nextTileZ;
    s32 tileDistance;
    s32 pathBottomY;
    s32 row;
    s32 firstColumn;
    s32 firstRow;
    s32 column;
    s32 triangleCount;
    s32 i;
    s32 hitIndex;
    s32 startSide;
    s32 endSide;
    s32 hitX;
    s32 hitZ;
    f32 planeOffset;
    f32 nearestFraction;
    f32 hitFraction;
    f32 tileFraction;
    f32 step;

    if (IsGameFlagSet(GAME_FLAG_PRINT_HIT_PATHS)) {
        PrintDebugPaddedHex(startX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW);
        PrintDebugPaddedHex(startY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 1);
        PrintDebugPaddedHex(startZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 2);
        PrintDebugPaddedHex(endX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 3);
        PrintDebugPaddedHex(endY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 4);
        PrintDebugPaddedHex(endZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 5);
        PrintDebugText(g_PathDebugBlankText, PATH_DEBUG_CLEAR_COLUMN, PATH_DEBUG_CLEAR_ROW);
    }

    if (IsOffMiniGolfGround(startX, startZ) || IsOffMiniGolfGround(endX, endZ)) {
        if (endY >= GetGroundHeight(endX, endZ)) {
            *fraction = 1.0f;
            hit->tag = HOLE_TRIANGLE_KIND_FLAT;
            hit->corners[AXIS_X][0] = 0;
            hit->corners[AXIS_Y][0] = 0;
            hit->corners[AXIS_Z][0] = 0;
            hit->corners[AXIS_X][1] = LEVEL_TRIANGLE_SIZE;
            hit->corners[AXIS_Y][1] = 0;
            hit->corners[AXIS_Z][1] = 0;
            hit->corners[AXIS_X][2] = 0;
            hit->corners[AXIS_Y][2] = 0;
            hit->corners[AXIS_Z][2] = LEVEL_TRIANGLE_SIZE;
            return TRUE;
        }

        return FALSE;
    }

    tileX = startX >> MINI_GOLF_TILE_X_SHIFT;
    tileZ = startZ >> MINI_GOLF_TILE_Z_SHIFT;
    endTileX = endX >> MINI_GOLF_TILE_X_SHIFT;
    endTileZ = endZ >> MINI_GOLF_TILE_Z_SHIFT;
    tileFraction = 0.0f;
    while (TRUE) {
        hitIndex = NO_HOLE_TRIANGLE;
        firstColumn = tileX * GROUND_TILE_POINT_SPAN;
        firstRow = tileZ * GROUND_TILE_POINT_SPAN;
        if (endY < startY) {
            pathBottomY = (startY / WHOLE_UNIT_SIZE) + 1;
        }
        else {
            pathBottomY = (endY / WHOLE_UNIT_SIZE) + 1;
        }

        for (row = 0; row < GROUND_TILE_POINT_SPAN + 1; row++) {
            for (column = 0; column < GROUND_TILE_POINT_SPAN + 1; column++) {
                points[row][column] = GetGroundPoint(firstColumn + column, firstRow + row);
                if (pathBottomY < points[row][column]->v.ob[AXIS_Y]) {
                    isBelowPath[row][column] = TRUE;
                }
                else {
                    isBelowPath[row][column] = FALSE;
                }
            }
        }

        triangleCount = 0;
        for (row = 0; row < GROUND_TILE_POINT_SPAN; row++) {
            for (column = 0; column < GROUND_TILE_POINT_SPAN; column++) {
                if (!IsTriangleBelowPath(
                        isBelowPath[row][column],
                        isBelowPath[row][column + 1],
                        isBelowPath[row + 1][column]
                    )) {
                    triangles[triangleCount].corners[AXIS_X][0] = points[row][column]->v.ob[AXIS_X];
                    triangles[triangleCount].corners[AXIS_Y][0] = points[row][column]->v.ob[AXIS_Y];
                    triangles[triangleCount].corners[AXIS_Z][0] = points[row][column]->v.ob[AXIS_Z];
                    triangles[triangleCount].corners[AXIS_X][1] =
                        points[row][column + 1]->v.ob[AXIS_X];
                    triangles[triangleCount].corners[AXIS_Y][1] =
                        points[row][column + 1]->v.ob[AXIS_Y];
                    triangles[triangleCount].corners[AXIS_Z][1] =
                        points[row][column + 1]->v.ob[AXIS_Z];
                    triangles[triangleCount].corners[AXIS_X][2] =
                        points[row + 1][column]->v.ob[AXIS_X];
                    triangles[triangleCount].corners[AXIS_Y][2] =
                        points[row + 1][column]->v.ob[AXIS_Y];
                    triangles[triangleCount].corners[AXIS_Z][2] =
                        points[row + 1][column]->v.ob[AXIS_Z];
                    triangleCount++;
                }

                if (!IsTriangleBelowPath(
                        isBelowPath[row][column + 1],
                        isBelowPath[row + 1][column + 1],
                        isBelowPath[row + 1][column]
                    )) {
                    triangles[triangleCount].corners[AXIS_X][0] =
                        points[row][column + 1]->v.ob[AXIS_X];
                    triangles[triangleCount].corners[AXIS_Y][0] =
                        points[row][column + 1]->v.ob[AXIS_Y];
                    triangles[triangleCount].corners[AXIS_Z][0] =
                        points[row][column + 1]->v.ob[AXIS_Z];
                    triangles[triangleCount].corners[AXIS_X][1] =
                        points[row + 1][column + 1]->v.ob[AXIS_X];
                    triangles[triangleCount].corners[AXIS_Y][1] =
                        points[row + 1][column + 1]->v.ob[AXIS_Y];
                    triangles[triangleCount].corners[AXIS_Z][1] =
                        points[row + 1][column + 1]->v.ob[AXIS_Z];
                    triangles[triangleCount].corners[AXIS_X][2] =
                        points[row + 1][column]->v.ob[AXIS_X];
                    triangles[triangleCount].corners[AXIS_Y][2] =
                        points[row + 1][column]->v.ob[AXIS_Y];
                    triangles[triangleCount].corners[AXIS_Z][2] =
                        points[row + 1][column]->v.ob[AXIS_Z];
                    triangleCount++;
                }
            }
        }

        nearestFraction = NO_PLANE_HIT_FRACTION;
        for (i = 0; i < triangleCount; i++) {
            GetTrianglePlane(&triangles[i], &plane);
            planeOffset = plane.distance * WORLD_TO_COURSE_SCALE;
            startSide = (plane.normal[AXIS_X] * startX) + (plane.normal[AXIS_Y] * startY) +
                (plane.normal[AXIS_Z] * startZ) + planeOffset;
            endSide = (plane.normal[AXIS_X] * endX) + (plane.normal[AXIS_Y] * endY) +
                (plane.normal[AXIS_Z] * endZ) + planeOffset;
            if ((endSide >= 0) & (startSide <= 0)) {
                if (startSide < 0 &&
                    !IsBareCrossing(simulation, startSide, endSide) &&
                    startSide != endSide) {
                    hitFraction = fabsf((f32)startSide) / fabsf((f32)(startSide - endSide));
                }
                else {
                    hitFraction = 1.0f;
                }

                if (hitFraction < nearestFraction) {
                    hitX = (s32)((endX - startX) * hitFraction) + startX;
                    hitZ = (s32)((endZ - startZ) * hitFraction) + startZ;
                    if (((triangles[i].corners[AXIS_X][1] - triangles[i].corners[AXIS_X][0]) *
                            (hitZ - (triangles[i].corners[AXIS_Z][0] << WORLD_TO_COURSE_SHIFT))) -
                            ((triangles[i].corners[AXIS_Z][1] - triangles[i].corners[AXIS_Z][0]) *
                                (hitX -
                                    (triangles[i].corners[AXIS_X][0] << WORLD_TO_COURSE_SHIFT))) >=
                        0) {
                        if (((triangles[i].corners[AXIS_X][2] - triangles[i].corners[AXIS_X][1]) *
                                (hitZ -
                                    (triangles[i].corners[AXIS_Z][1] << WORLD_TO_COURSE_SHIFT))) -
                                ((triangles[i].corners[AXIS_Z][2] -
                                     triangles[i].corners[AXIS_Z][1]) *
                                    (hitX -
                                        (triangles[i].corners[AXIS_X][1]
                                            << WORLD_TO_COURSE_SHIFT))) >=
                            0) {
                            if (((triangles[i].corners[AXIS_X][0] -
                                     triangles[i].corners[AXIS_X][2]) *
                                    (hitZ -
                                        (triangles[i].corners[AXIS_Z][2]
                                            << WORLD_TO_COURSE_SHIFT))) -
                                    ((triangles[i].corners[AXIS_Z][0] -
                                         triangles[i].corners[AXIS_Z][2]) *
                                        (hitX -
                                            (triangles[i].corners[AXIS_X][2]
                                                << WORLD_TO_COURSE_SHIFT))) >=
                                0) {
                                nearestFraction = hitFraction;
                                hitIndex = i;
                            }
                        }
                    }
                }
            }
        }

        if (hitIndex >= 0) {
            *hit = triangles[hitIndex];
            hit->tag = HOLE_TRIANGLE_KIND_FLAT;
            *fraction = nearestFraction;
            return TRUE;
        }

        if ((tileX == endTileX) & (tileZ == endTileZ)) {
            break;
        }

        if ((tileX != endTileX) | (tileZ != endTileZ)) {
            step = (1.0f - tileFraction) * 0.5f;
            hitFraction = tileFraction + step;
            for (i = 0; i < TILE_SEARCH_STEP_COUNT; i++) {
                nextTileX =
                    (s32)(((endX - startX) * hitFraction) + startX) >> MINI_GOLF_TILE_X_SHIFT;
                nextTileZ =
                    (s32)(((endZ - startZ) * hitFraction) + startZ) >> MINI_GOLF_TILE_Z_SHIFT;
                tileDistance = Abs(nextTileX - tileX) + Abs(nextTileZ - tileZ);
                if (tileDistance == 1) {
                    break;
                }

                step *= 0.5f;
                if (tileDistance == 0) {
                    hitFraction += step;
                }
                else {
                    hitFraction -= step;
                }
            }

            tileX = nextTileX;
            if (i != TILE_SEARCH_STEP_COUNT) {
                tileZ = nextTileZ;
                tileFraction = hitFraction;
            }
            else {
                osSyncPrintf("CTerm\n");
                tileX = endTileX;
                tileZ = endTileZ;
            }
        }
    }

    return FALSE;
}

Boolean FindGroundHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hit,
    f32 *fraction
) {
    Vtx *points[GROUND_TILE_POINT_SPAN + 1][GROUND_TILE_POINT_SPAN + 1];
    Boolean8 isBelowPath[GROUND_TILE_POINT_SPAN + 1][GROUND_TILE_POINT_SPAN + 1];
    PlaneTriangle triangles[GROUND_TILE_TRIANGLE_COUNT];
    TrianglePlane plane;
    s32 tileX;
    s32 tileZ;
    s32 endTileX;
    s32 endTileZ;
    s32 nextTileX;
    s32 nextTileZ;
    s32 tileDistance;
    s32 pathBottomY;
    s32 row;
    s32 firstColumn;
    s32 firstRow;
    s32 column;
    s32 triangleCount;
    s32 i;
    s32 hitIndex;
    s32 startSide;
    s32 endSide;
    s32 hitX;
    s32 hitZ;
    f32 planeOffset;
    f32 nearestFraction;
    f32 hitFraction;
    f32 tileFraction;
    f32 step;

    if (IsGameFlagSet(GAME_FLAG_PRINT_HIT_PATHS)) {
        PrintDebugPaddedHex(startX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW);
        PrintDebugPaddedHex(startY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 1);
        PrintDebugPaddedHex(startZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 2);
        PrintDebugPaddedHex(endX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 3);
        PrintDebugPaddedHex(endY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 4);
        PrintDebugPaddedHex(endZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 5);
        PrintDebugText(g_PathDebugBlankText, PATH_DEBUG_CLEAR_COLUMN, PATH_DEBUG_CLEAR_ROW);
    }

    if (IsOffGround(startX, startZ) || IsOffGround(endX, endZ)) {
        if (endY >= GetGroundHeight(endX, endZ)) {
            *fraction = 1.0f;
            hit->tag = HOLE_TRIANGLE_KIND_FLAT;
            hit->corners[AXIS_X][0] = 0;
            hit->corners[AXIS_Y][0] = 0;
            hit->corners[AXIS_Z][0] = 0;
            hit->corners[AXIS_X][1] = LEVEL_TRIANGLE_SIZE;
            hit->corners[AXIS_Y][1] = 0;
            hit->corners[AXIS_Z][1] = 0;
            hit->corners[AXIS_X][2] = 0;
            hit->corners[AXIS_Y][2] = 0;
            hit->corners[AXIS_Z][2] = LEVEL_TRIANGLE_SIZE;
            return TRUE;
        }

        return FALSE;
    }

    tileX = startX >> GROUND_TILE_SHIFT;
    tileZ = startZ >> GROUND_TILE_SHIFT;
    endTileX = endX >> GROUND_TILE_SHIFT;
    endTileZ = endZ >> GROUND_TILE_SHIFT;
    tileFraction = 0.0f;
    while (TRUE) {
        hitIndex = NO_HOLE_TRIANGLE;
        firstColumn = tileX * GROUND_TILE_POINT_SPAN;
        firstRow = tileZ * GROUND_TILE_POINT_SPAN;
        if (endY < startY) {
            pathBottomY = startY / WHOLE_UNIT_SIZE;
        }
        else {
            pathBottomY = endY / WHOLE_UNIT_SIZE;
        }

        for (row = 0; row < GROUND_TILE_POINT_SPAN + 1; row++) {
            for (column = 0; column < GROUND_TILE_POINT_SPAN + 1; column++) {
                points[row][column] = GetGroundPoint(firstColumn + column, firstRow + row);
                if (pathBottomY < points[row][column]->v.ob[AXIS_Y]) {
                    isBelowPath[row][column] = TRUE;
                }
                else {
                    isBelowPath[row][column] = FALSE;
                }
            }
        }

        triangleCount = 0;
        for (row = 0; row < GROUND_TILE_POINT_SPAN; row++) {
            for (column = 0; column < GROUND_TILE_POINT_SPAN; column++) {
                if (!IsTriangleBelowPath(
                        isBelowPath[row][column],
                        isBelowPath[row][column + 1],
                        isBelowPath[row + 1][column]
                    )) {
                    triangles[triangleCount].corners[AXIS_X][0] = points[row][column]->v.ob[AXIS_X];
                    triangles[triangleCount].corners[AXIS_Y][0] = points[row][column]->v.ob[AXIS_Y];
                    triangles[triangleCount].corners[AXIS_Z][0] = points[row][column]->v.ob[AXIS_Z];
                    triangles[triangleCount].corners[AXIS_X][1] =
                        points[row][column + 1]->v.ob[AXIS_X];
                    triangles[triangleCount].corners[AXIS_Y][1] =
                        points[row][column + 1]->v.ob[AXIS_Y];
                    triangles[triangleCount].corners[AXIS_Z][1] =
                        points[row][column + 1]->v.ob[AXIS_Z];
                    triangles[triangleCount].corners[AXIS_X][2] =
                        points[row + 1][column]->v.ob[AXIS_X];
                    triangles[triangleCount].corners[AXIS_Y][2] =
                        points[row + 1][column]->v.ob[AXIS_Y];
                    triangles[triangleCount].corners[AXIS_Z][2] =
                        points[row + 1][column]->v.ob[AXIS_Z];
                    triangleCount++;
                }

                if (!IsTriangleBelowPath(
                        isBelowPath[row][column + 1],
                        isBelowPath[row + 1][column + 1],
                        isBelowPath[row + 1][column]
                    )) {
                    triangles[triangleCount].corners[AXIS_X][0] =
                        points[row][column + 1]->v.ob[AXIS_X];
                    triangles[triangleCount].corners[AXIS_Y][0] =
                        points[row][column + 1]->v.ob[AXIS_Y];
                    triangles[triangleCount].corners[AXIS_Z][0] =
                        points[row][column + 1]->v.ob[AXIS_Z];
                    triangles[triangleCount].corners[AXIS_X][1] =
                        points[row + 1][column + 1]->v.ob[AXIS_X];
                    triangles[triangleCount].corners[AXIS_Y][1] =
                        points[row + 1][column + 1]->v.ob[AXIS_Y];
                    triangles[triangleCount].corners[AXIS_Z][1] =
                        points[row + 1][column + 1]->v.ob[AXIS_Z];
                    triangles[triangleCount].corners[AXIS_X][2] =
                        points[row + 1][column]->v.ob[AXIS_X];
                    triangles[triangleCount].corners[AXIS_Y][2] =
                        points[row + 1][column]->v.ob[AXIS_Y];
                    triangles[triangleCount].corners[AXIS_Z][2] =
                        points[row + 1][column]->v.ob[AXIS_Z];
                    triangleCount++;
                }
            }
        }

        nearestFraction = NO_PLANE_HIT_FRACTION;
        for (i = 0; i < triangleCount; i++) {
            GetTrianglePlane(&triangles[i], &plane);
            planeOffset = plane.distance * WORLD_TO_COURSE_SCALE;
            endSide = (plane.normal[AXIS_X] * endX) + (plane.normal[AXIS_Y] * endY) +
                (plane.normal[AXIS_Z] * endZ) + planeOffset;
            if (endSide >= 0) {
                startSide = (plane.normal[AXIS_X] * startX) + (plane.normal[AXIS_Y] * startY) +
                    (plane.normal[AXIS_Z] * startZ) + planeOffset;
                if (startSide != endSide) {
                    hitFraction = fabsf((f32)startSide) / fabsf((f32)(startSide - endSide));
                }
                else {
                    hitFraction = 0.0f;
                }

                if (hitFraction < nearestFraction) {
                    hitX = ((endX - startX) * hitFraction) + startX;
                    hitZ = ((endZ - startZ) * hitFraction) + startZ;
                    if (((triangles[i].corners[AXIS_X][1] - triangles[i].corners[AXIS_X][0]) *
                            (hitZ - (triangles[i].corners[AXIS_Z][0] << WORLD_TO_COURSE_SHIFT))) -
                            ((triangles[i].corners[AXIS_Z][1] - triangles[i].corners[AXIS_Z][0]) *
                                (hitX -
                                    (triangles[i].corners[AXIS_X][0] << WORLD_TO_COURSE_SHIFT))) >=
                        0) {
                        if (((triangles[i].corners[AXIS_X][2] - triangles[i].corners[AXIS_X][1]) *
                                (hitZ -
                                    (triangles[i].corners[AXIS_Z][1] << WORLD_TO_COURSE_SHIFT))) -
                                ((triangles[i].corners[AXIS_Z][2] -
                                     triangles[i].corners[AXIS_Z][1]) *
                                    (hitX -
                                        (triangles[i].corners[AXIS_X][1]
                                            << WORLD_TO_COURSE_SHIFT))) >=
                            0) {
                            if (((triangles[i].corners[AXIS_X][0] -
                                     triangles[i].corners[AXIS_X][2]) *
                                    (hitZ -
                                        (triangles[i].corners[AXIS_Z][2]
                                            << WORLD_TO_COURSE_SHIFT))) -
                                    ((triangles[i].corners[AXIS_Z][0] -
                                         triangles[i].corners[AXIS_Z][2]) *
                                        (hitX -
                                            (triangles[i].corners[AXIS_X][2]
                                                << WORLD_TO_COURSE_SHIFT))) >=
                                0) {
                                nearestFraction = hitFraction;
                                hitIndex = i;
                            }
                        }
                    }
                }
            }
        }

        if (hitIndex >= 0) {
            *hit = triangles[hitIndex];
            hit->tag = HOLE_TRIANGLE_KIND_FLAT;
            *fraction = nearestFraction;
            return TRUE;
        }

        if ((tileX == endTileX) & (tileZ == endTileZ)) {
            break;
        }

        if ((tileX != endTileX) | (tileZ != endTileZ)) {
            step = (1.0f - tileFraction) * 0.5f;
            hitFraction = tileFraction + step;
            for (i = 0; i < TILE_SEARCH_STEP_COUNT; i++) {
                nextTileX = (s32)(((endX - startX) * hitFraction) + startX) >> GROUND_TILE_SHIFT;
                nextTileZ = (s32)(((endZ - startZ) * hitFraction) + startZ) >> GROUND_TILE_SHIFT;
                tileDistance = Abs(nextTileX - tileX) + Abs(nextTileZ - tileZ);
                if (tileDistance == 1) {
                    break;
                }

                step *= 0.5f;
                if (tileDistance == 0) {
                    hitFraction += step;
                }
                else {
                    hitFraction -= step;
                }
            }

            tileX = nextTileX;
            if (i != TILE_SEARCH_STEP_COUNT) {
                tileZ = nextTileZ;
                tileFraction = hitFraction;
            }
            else {
                tileX = endTileX;
                tileZ = endTileZ;
            }
        }
    }

    return FALSE;
}

BillboardCell GetBillboardCell(s32 textureIndex, f32 across, f32 down) {
    s32 column = across * BILLBOARD_MASK_SIZE;
    s32 row = down * BILLBOARD_MASK_SIZE;
    BillboardCell cell;

    if (column < 0) {
        column = 0;
    }
    else if (column >= BILLBOARD_MASK_SIZE) {
        column = BILLBOARD_MASK_SIZE - 1;
    }

    if (row < 0) {
        row = 0;
    }
    else if (row >= BILLBOARD_MASK_SIZE) {
        row = BILLBOARD_MASK_SIZE - 1;
    }

    row = BILLBOARD_MASK_SIZE - 1 - row;
    cell = GetBillboardMaskCell(textureIndex, row, column);
    cell -= '0';
    if (cell < BILLBOARD_CELL_EMPTY) {
        cell = BILLBOARD_CELL_EMPTY;
    }

    if (cell > BILLBOARD_CELL_LAST) {
        cell = BILLBOARD_CELL_LAST;
    }

    if (cell == BILLBOARD_CELL_IGNORED) {
        cell = BILLBOARD_CELL_EMPTY;
    }

    if (IsGameFlagSet(GAME_FLAG_FILL_BILLBOARD_MASKS)) {
        cell = BILLBOARD_CELL_TRUNK;
    }

    return cell;
}

s32 FindBillboardHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    BillboardCell *cell,
    f32 *fraction
) {
    s32 fromY;
    f32 nearestFraction;
    s32 fromX;
    s32 toX;
    s32 deltaX;
    s32 billboardIndex;
    s32 fromZ;
    s32 toZ;
    s32 toY;
    Boolean isHit;
    s32 deltaZ;
    Billboard *billboard;
    s32 startSide;
    s32 endSide;
    f32 hitFraction;
    s32 hitX;
    s32 hitY;
    s32 hitZ;
    s32 depth;
    s32 distance;
    BillboardCell hitCell;

    nearestFraction = NO_HIT_FRACTION;
    toY = endY >> WORLD_TO_COURSE_SHIFT;
    isHit = FALSE;
    fromX = startX >> WORLD_TO_COURSE_SHIFT;
    fromY = startY >> WORLD_TO_COURSE_SHIFT;
    toX = endX >> WORLD_TO_COURSE_SHIFT;
    deltaX = toX - fromX;
    billboardIndex = 0;
    fromZ = startZ >> WORLD_TO_COURSE_SHIFT;
    toZ = endZ >> WORLD_TO_COURSE_SHIFT;
    deltaZ = toZ - fromZ;

    for (; billboardIndex < BILLBOARD_COUNT; billboardIndex++) {
        billboard = &g_Billboards[billboardIndex];
        if (billboard->textureIndex < 0) {
            continue;
        }

        if (fromX < billboard->x - billboard->width) {
            if (toX < billboard->x - billboard->width) {
                continue;
            }
        }

        if (billboard->x + billboard->width < fromX) {
            if (billboard->x + billboard->width < toX) {
                continue;
            }
        }

        if (fromZ < billboard->z - billboard->width) {
            if (toZ < billboard->z - billboard->width) {
                continue;
            }
        }

        if (billboard->z + billboard->width < fromZ) {
            if (billboard->z + billboard->width < toZ) {
                continue;
            }
        }

        if (billboard->width == 0) {
            continue;
        }

        if (billboard->height == 0) {
            continue;
        }

        startSide = (deltaX * (billboard->x - fromX)) + (deltaZ * (billboard->z - fromZ));
        endSide = (deltaX * (billboard->x - toX)) + (deltaZ * (billboard->z - toZ));
        if ((startSide ^ endSide) >= 0) {
            continue;
        }

        if (Abs(endSide) + Abs(startSide) != 0) {
            hitFraction = fabsf(startSide) / (Abs(endSide) + Abs(startSide));
        }
        else {
            hitFraction = 0.0f;
        }

        if (hitFraction > nearestFraction) {
            continue;
        }

        hitX = fromX + (deltaX * hitFraction);
        hitY = fromY + ((toY - fromY) * hitFraction);
        hitZ = fromZ + (deltaZ * hitFraction);
        depth = billboard->y - hitY;
        if (depth < 0) {
            continue;
        }

        if (depth > billboard->height) {
            continue;
        }

        distance = IntLength2D(billboard->x - hitX, billboard->z - hitZ);
        if (distance > billboard->width) {
            continue;
        }

        hitCell = GetBillboardCell(
            billboard->textureIndex,
            (f32)distance / billboard->width,
            (f32)depth / billboard->height
        );
        if (hitCell != BILLBOARD_CELL_EMPTY) {
            nearestFraction = hitFraction;
            isHit = TRUE;
            *cell = hitCell;
        }
    }

    *fraction = nearestFraction;
    return isHit;
}

void MoveBallToCupEdge(BallSimulation *simulation) {
    s32 distance = IntLength2D(
        g_HoleSetup.cupPosition[AXIS_X] - simulation->position[AXIS_X],
        g_HoleSetup.cupPosition[AXIS_Z] - simulation->position[AXIS_Z]
    );
    Vec3 direction;
    f32 offsetX;
    s32 deltaX;
    s32 deltaZ;
    f32 offsetZ;

    if (distance < CUP_EDGE_DISTANCE) {
        if (distance != 0) {
            direction[AXIS_Y] = 0;
            deltaX = simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];
            deltaZ = simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];
            direction[AXIS_X] = deltaX;
            direction[AXIS_Z] = deltaZ;
            guNormalize(&direction[AXIS_X], &direction[AXIS_Y], &direction[AXIS_Z]);
            offsetX = direction[AXIS_X] * CUP_EDGE_DISTANCE;
            offsetZ = direction[AXIS_Z] * CUP_EDGE_DISTANCE;

            simulation->position[AXIS_X] = offsetX + g_HoleSetup.cupPosition[AXIS_X];
            simulation->position[AXIS_Z] = offsetZ + g_HoleSetup.cupPosition[AXIS_Z];
        }
    }
}

PinHit FindPinHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    CoursePoint velocity,
    f32 *fraction,
    Boolean hasHitFlag,
    BallSimulation *simulation
) {
    s32 pinIndex;
    s32 fromX = startX;
    s32 toX = endX;
    s32 fromZ = startZ;
    s32 toZ = endZ;
    s32 fromY = startY;
    s32 toY = endY;
    s32 deltaX = toX - fromX;
    s32 deltaY = toY - fromY;
    s32 deltaZ = toZ - fromZ;
    f32 nearestFraction = NO_HIT_FRACTION;
    PinHit hitKind = PIN_HIT_NONE;
    s32 startSide;
    s32 endSide;
    s32 endToCupX;
    s32 endToCupZ;
    f32 hitFraction;
    f32 height;
    Boolean isFlagHit;
    s32 distance;
    s32 speed;
    f32 halfChord;
    Vec3 direction;
    s32 contactX;
    s32 contactY;
    s32 contactZ;
    Radians angle;
    f32 bounceSpeed;

    for (pinIndex = 0; pinIndex < PIN_COUNT; pinIndex++) {
        if (fromX < (s32)(g_HoleSetup.cupPosition[AXIS_X] - FLAG_REACH)) {
            if (toX < (s32)(g_HoleSetup.cupPosition[AXIS_X] - FLAG_REACH)) {
                continue;
            }
        }

        if ((s32)(g_HoleSetup.cupPosition[AXIS_X] + FLAG_REACH) < fromX) {
            if ((s32)(g_HoleSetup.cupPosition[AXIS_X] + FLAG_REACH) < toX) {
                continue;
            }
        }

        if (fromZ < (s32)(g_HoleSetup.cupPosition[AXIS_Z] - FLAG_REACH)) {
            if (toZ < (s32)(g_HoleSetup.cupPosition[AXIS_Z] - FLAG_REACH)) {
                continue;
            }
        }

        if ((s32)(g_HoleSetup.cupPosition[AXIS_Z] + FLAG_REACH) < fromZ) {
            if ((s32)(g_HoleSetup.cupPosition[AXIS_Z] + FLAG_REACH) < toZ) {
                continue;
            }
        }

        endToCupX = g_HoleSetup.cupPosition[AXIS_X] - toX;
        endToCupZ = g_HoleSetup.cupPosition[AXIS_Z] - toZ;
        startSide = (deltaX * (g_HoleSetup.cupPosition[AXIS_X] - fromX)) +
            (deltaZ * (g_HoleSetup.cupPosition[AXIS_Z] - fromZ));
        endSide = (deltaX * endToCupX) + (deltaZ * endToCupZ);
        if (HaveSameSign(startSide, endSide)) {
            if ((s32)IntLength2D(endToCupX, endToCupZ) <= FLAGSTICK_RADIUS) {
                hitFraction = 1.0f;
            }
            else {
                continue;
            }
        }
        else {
            if (Abs(endSide) + Abs(startSide) != 0) {
                hitFraction = fabsf(startSide) / (Abs(endSide) + Abs(startSide));
            }
            else {
                hitFraction = 0.0f;
            }

            if (hitFraction > nearestFraction) {
                continue;
            }
        }

        contactX = fromX + (deltaX * hitFraction);
        contactY = fromY + (deltaY * hitFraction);
        contactZ = fromZ + (deltaZ * hitFraction);
        height = g_HoleSetup.cupPosition[AXIS_Y] - contactY;
        isFlagHit = FALSE;
        if (height > FLAGSTICK_HEIGHT) {
            continue;
        }

        if (!hasHitFlag) {
            if (height > FLAG_BOTTOM_HEIGHT) {
                isFlagHit = TRUE;
            }
        }

        if ((deltaY < 0) && !IsGameFlagSet(GAME_FLAG_RISING_FLAG_HITS)) {
            isFlagHit = FALSE;
        }

        distance = IntLength2D(
            g_HoleSetup.cupPosition[AXIS_X] - contactX,
            g_HoleSetup.cupPosition[AXIS_Z] - contactZ
        );
        if (distance > FLAG_REACH) {
            continue;
        }

        if ((distance > FLAGSTICK_RADIUS) && !isFlagHit) {
            continue;
        }

        speed = IntLength2D(deltaX, deltaZ);
        nearestFraction = hitFraction;
        hitKind = PIN_HIT_FLAGSTICK;
        if (isFlagHit) {
            hitKind = PIN_HIT_FLAG;
        }

        halfChord = FLAGSTICK_RADIUS_SQUARED - (distance * distance);
        if (halfChord >= 0.0f) {
            halfChord = sqrtf(halfChord);
        }
        else {
            halfChord = FLAGSTICK_RADIUS;
        }

        direction[AXIS_X] = fromX - contactX;
        ASSIGN_FOR_VALUE(direction[AXIS_Y] = 0.0f);
        direction[AXIS_Z] = fromZ - contactZ;
        if ((direction[AXIS_X] != 0.0f) | (direction[AXIS_Z] != 0.0f)) {
            guNormalize(&direction[AXIS_X], &direction[AXIS_Y], &direction[AXIS_Z]);
        }

        direction[AXIS_X] *= halfChord;
        direction[AXIS_Y] *= halfChord;
        direction[AXIS_Z] *= halfChord;
        contactX += direction[AXIS_X];
        contactZ += direction[AXIS_Z];
        contactX -= g_HoleSetup.cupPosition[AXIS_X];
        contactZ -= g_HoleSetup.cupPosition[AXIS_Z];
        angle = 0.0f;
        if ((contactX | contactZ) != 0) {
            angle = atan2(contactZ, contactX);
        }

        if (isFlagHit) {
            velocity[AXIS_X] = 0;
            velocity[AXIS_Y] = FLAG_HIT_SPEED_Y;
            velocity[AXIS_Z] = 0;
        }
        else {
            bounceSpeed = (speed * FLAGSTICK_MOST_BOUNCE) - (Abs(deltaY) * 2);
            if (simulation->state == BALL_STATE_FLYING) {
                bounceSpeed *= 0.5f;
            }

            if (bounceSpeed > speed * FLAGSTICK_MOST_BOUNCE) {
                bounceSpeed = speed * FLAGSTICK_MOST_BOUNCE;
            }

            if (bounceSpeed < speed * FLAGSTICK_LEAST_BOUNCE) {
                bounceSpeed = speed * FLAGSTICK_LEAST_BOUNCE;
            }

            velocity[AXIS_X] = cosf(angle) * bounceSpeed;
            velocity[AXIS_Y] = deltaY;
            velocity[AXIS_Z] = sinf(angle) * bounceSpeed;
        }
    }

    *fraction = nearestFraction;
    return hitKind;
}

TriangleFacing GetTriangleFacing(
    s32 firstX,
    s32 firstY,
    s32 firstZ,
    s32 secondX,
    s32 secondY,
    s32 secondZ,
    s32 thirdX,
    s32 thirdY,
    s32 thirdZ
) {
    s32 secondDeltaX;
    s32 secondDeltaY;
    s32 secondDeltaZ;
    s32 thirdDeltaX;
    s32 thirdDeltaY;
    s32 thirdDeltaZ;
    s32 largest;
    s32 normalX;
    s32 normalY;
    s32 normalZ;
    TriangleFacing facing;

    secondDeltaZ = secondZ - firstZ;
    thirdDeltaX = thirdX - firstX;
    thirdDeltaZ = thirdZ - firstZ;
    secondDeltaX = secondX - firstX;
    largest = 0;
    normalY = (secondDeltaX * thirdDeltaZ) - (secondDeltaZ * thirdDeltaX);
    facing = TRIANGLE_FACING_NONE;
    if (normalY < 0) {
        normalY = -normalY;
    }

    if (normalY > largest) {
        facing = TRIANGLE_FACING_UP;
        largest = normalY;
    }

    secondDeltaY = secondY - firstY;
    thirdDeltaY = thirdY - firstY;
    normalX = (secondDeltaY * thirdDeltaZ) - (secondDeltaZ * thirdDeltaY);
    if (normalX < 0) {
        normalX = -normalX;
    }

    if (normalX > largest) {
        facing = TRIANGLE_FACING_X;
        largest = normalX;
    }

    normalZ = (secondDeltaX * thirdDeltaY) - (secondDeltaY * thirdDeltaX);
    if (normalZ < 0) {
        normalZ = -normalZ;
    }

    if (normalZ > largest) {
        facing = TRIANGLE_FACING_Z;
    }

    return facing;
}

#define NORMAL_ERROR_TEXT_COLUMN 3
#define NORMAL_ERROR_TEXT_ROW 15

Outcome GetTrianglePlaneCopy(PlaneTriangle *triangle, TrianglePlane *plane) {
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
        PrintDebugText("NormalFError", NORMAL_ERROR_TEXT_COLUMN, NORMAL_ERROR_TEXT_ROW);
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

Boolean FindHoleTriangleHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hit,
    f32 *fraction
) {
    PlaneTriangle triangle;
    TrianglePlane plane;
    s32 edgeCrosses[TRIANGLE_CORNER_COUNT];
    s32 lowestX;
    s32 highestX;
    s32 lowestY;
    s32 highestY;
    s32 lowestZ;
    s32 highestZ;
    s32 hitIndex;
    s32 triangleIndex;
    HoleTriangle *candidate;
    f32 nearestFraction;
    f32 startSide;
    f32 endSide;
    f32 sideChange;
    f32 hitFraction;
    s32 hitX;
    s32 hitZ;
    s32 sides;
    Boolean isDebug = IsGameFlagSet(GAME_FLAG_PRINT_HIT_PATHS);

    hitIndex = NO_HOLE_TRIANGLE;
    if (isDebug) {
        PrintDebugPaddedHex(startX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW);
        PrintDebugPaddedHex(startY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 1);
        PrintDebugPaddedHex(startZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 2);
        PrintDebugPaddedHex(endX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 3);
        PrintDebugPaddedHex(endY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 4);
        PrintDebugPaddedHex(endZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 5);
        PrintDebugText(g_PathDebugBlankText, PATH_DEBUG_CLEAR_COLUMN, PATH_DEBUG_CLEAR_ROW);
    }

    if (endY + PATH_MOST_RISE < startY) {
        return FALSE;
    }

    if (IsOffGround(startX, startZ)) {
        return FALSE;
    }

    if (IsOffGround(endX, endZ)) {
        return FALSE;
    }

    if (startX < endX) {
        lowestX = startX / WHOLE_UNIT_SIZE;
        highestX = endX / WHOLE_UNIT_SIZE;
    }
    else {
        lowestX = endX / WHOLE_UNIT_SIZE;
        highestX = startX / WHOLE_UNIT_SIZE;
    }

    if (startY < endY) {
        lowestY = startY / WHOLE_UNIT_SIZE;
        highestY = endY / WHOLE_UNIT_SIZE;
    }
    else {
        lowestY = endY / WHOLE_UNIT_SIZE;
        highestY = startY / WHOLE_UNIT_SIZE;
    }

    if (startZ < endZ) {
        lowestZ = startZ / WHOLE_UNIT_SIZE;
        highestZ = endZ / WHOLE_UNIT_SIZE;
    }
    else {
        lowestZ = endZ / WHOLE_UNIT_SIZE;
        highestZ = startZ / WHOLE_UNIT_SIZE;
    }

    nearestFraction = NO_PLANE_HIT_FRACTION;
    for (triangleIndex = 0; triangleIndex < HOLE_TRIANGLE_COUNT; triangleIndex++) {
        candidate = &g_WorkArea.hole.triangles[triangleIndex];
        if (candidate->kind == HOLE_TRIANGLE_KIND_RING) {
            continue;
        }

        if (candidate->kind == HOLE_TRIANGLE_KIND_SHADED) {
            continue;
        }

        if (highestY < candidate->bounds[AXIS_Y][BOUND_LOWEST] ||
            candidate->bounds[AXIS_Y][BOUND_HIGHEST] < lowestY) {
            continue;
        }

        if (highestX < candidate->bounds[AXIS_X][BOUND_LOWEST] ||
            candidate->bounds[AXIS_X][BOUND_HIGHEST] < lowestX) {
            continue;
        }

        if (highestZ < candidate->bounds[AXIS_Z][BOUND_LOWEST] ||
            candidate->bounds[AXIS_Z][BOUND_HIGHEST] < lowestZ) {
            continue;
        }

        triangle.corners[AXIS_X][0] = candidate->vertices[0].v.ob[AXIS_X];
        triangle.corners[AXIS_X][1] = candidate->vertices[1].v.ob[AXIS_X];
        triangle.corners[AXIS_X][2] = candidate->vertices[2].v.ob[AXIS_X];
        triangle.corners[AXIS_Y][0] = candidate->vertices[0].v.ob[AXIS_Y];
        triangle.corners[AXIS_Y][1] = candidate->vertices[1].v.ob[AXIS_Y];
        triangle.corners[AXIS_Y][2] = candidate->vertices[2].v.ob[AXIS_Y];
        triangle.corners[AXIS_Z][0] = candidate->vertices[0].v.ob[AXIS_Z];
        triangle.corners[AXIS_Z][1] = candidate->vertices[1].v.ob[AXIS_Z];
        triangle.corners[AXIS_Z][2] = candidate->vertices[2].v.ob[AXIS_Z];
        GetTrianglePlane(&triangle, &plane);
        startSide = (plane.normal[AXIS_X] * startX) + (plane.normal[AXIS_Y] * startY) +
            (plane.normal[AXIS_Z] * startZ) + (plane.distance * WORLD_TO_COURSE_SCALE);
        endSide = (plane.normal[AXIS_X] * endX) + (plane.normal[AXIS_Y] * endY) +
            (plane.normal[AXIS_Z] * endZ) + (plane.distance * WORLD_TO_COURSE_SCALE);
        if (DoesPathMeetPlane(startSide, endSide)) {
            sideChange = startSide - endSide;
            if ((s32)sideChange != 0) {
                hitFraction = fabsf(startSide) / fabsf(sideChange);
            }
            else {
                hitFraction = 0.0f;
            }

            if (hitFraction < nearestFraction) {
                hitX = ((endX - startX) * hitFraction) + startX;
                hitZ = ((endZ - startZ) * hitFraction) + startZ;

                edgeCrosses[0] =
                    ((triangle.corners[AXIS_X][1] - triangle.corners[AXIS_X][0]) *
                        (hitZ - (triangle.corners[AXIS_Z][0] << WORLD_TO_COURSE_SHIFT))) -
                    ((triangle.corners[AXIS_Z][1] - triangle.corners[AXIS_Z][0]) *
                        (hitX - (triangle.corners[AXIS_X][0] << WORLD_TO_COURSE_SHIFT)));
                if (edgeCrosses[0] < 0) {
                    sides = EDGE_SIDE_NEGATIVE;
                }
                else {
                    sides = (edgeCrosses[0] == 0) ? EDGE_SIDE_ON : EDGE_SIDE_POSITIVE;
                }

                edgeCrosses[1] =
                    ((triangle.corners[AXIS_X][2] - triangle.corners[AXIS_X][1]) *
                        (hitZ - (triangle.corners[AXIS_Z][1] << WORLD_TO_COURSE_SHIFT))) -
                    ((triangle.corners[AXIS_Z][2] - triangle.corners[AXIS_Z][1]) *
                        (hitX - (triangle.corners[AXIS_X][1] << WORLD_TO_COURSE_SHIFT)));
                if (edgeCrosses[1] < 0) {
                    sides &= EDGE_SIDE_NEGATIVE;
                }
                else if (edgeCrosses[1] == 0) {
                    sides &= EDGE_SIDE_ON;
                }
                else {
                    sides &= EDGE_SIDE_POSITIVE;
                }

                if (sides != 0) {
                    edgeCrosses[2] =
                        ((triangle.corners[AXIS_X][1] - triangle.corners[AXIS_X][0]) *
                            (hitZ - (triangle.corners[AXIS_Z][0] << WORLD_TO_COURSE_SHIFT))) -
                        ((triangle.corners[AXIS_Z][1] - triangle.corners[AXIS_Z][0]) *
                            (hitX - (triangle.corners[AXIS_X][0] << WORLD_TO_COURSE_SHIFT)));
                    if (edgeCrosses[2] < 0) {
                        sides &= EDGE_SIDE_NEGATIVE;
                    }
                    else if (edgeCrosses[2] == 0) {
                        sides &= EDGE_SIDE_ON;
                    }
                    else {
                        sides &= EDGE_SIDE_POSITIVE;
                    }

                    if (sides != 0) {
                        nearestFraction = hitFraction;
                        hitIndex = triangleIndex;
                    }
                }
            }
        }
    }

    if (hitIndex >= 0) {
        HoleTriangle *hitTriangle = &g_WorkArea.hole.triangles[hitIndex];

        hit->corners[AXIS_X][0] = hitTriangle->vertices[0].v.ob[AXIS_X];
        hit->corners[AXIS_X][1] = hitTriangle->vertices[1].v.ob[AXIS_X];
        hit->corners[AXIS_X][2] = hitTriangle->vertices[2].v.ob[AXIS_X];
        hit->corners[AXIS_Y][0] = hitTriangle->vertices[0].v.ob[AXIS_Y];
        hit->corners[AXIS_Y][1] = hitTriangle->vertices[1].v.ob[AXIS_Y];
        hit->corners[AXIS_Y][2] = hitTriangle->vertices[2].v.ob[AXIS_Y];
        hit->corners[AXIS_Z][0] = hitTriangle->vertices[0].v.ob[AXIS_Z];
        hit->corners[AXIS_Z][1] = hitTriangle->vertices[1].v.ob[AXIS_Z];
        hit->corners[AXIS_Z][2] = hitTriangle->vertices[2].v.ob[AXIS_Z];

        hit->tag = g_WorkArea.hole.triangles[hitIndex].kind;
        *fraction = nearestFraction;
        return TRUE;
    }

    return FALSE;
}

Boolean FindWallHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hit,
    f32 *fraction
) {
    PlaneTriangle triangle;
    TrianglePlane plane;
    s32 edgeCrosses[TRIANGLE_CORNER_COUNT];
    f32 cornersU[TRIANGLE_CORNER_COUNT];
    f32 cornersV[TRIANGLE_CORNER_COUNT];
    s32 lowestX;
    s32 highestX;
    s32 lowestY;
    s32 highestY;
    s32 lowestZ;
    s32 highestZ;
    s32 hitIndex;
    s32 hitX;
    s32 hitY;
    s32 hitZ;
    s32 triangleIndex;
    f32 nearestFraction;
    f32 startSide;
    f32 endSide;
    f32 sideChange;
    f32 hitFraction;
    f32 endFraction;
    f32 wallEnd;
    f32 nearestCorner;
    f32 cornerDistance;
    s32 hitPointU;
    s32 hitPointV;
    TriangleFacing facing;
    s32 cornerIndex;
    Boolean isDebug = IsGameFlagSet(GAME_FLAG_PRINT_HIT_PATHS);

    hitIndex = NO_HOLE_TRIANGLE;
    if (isDebug) {
        PrintDebugPaddedHex(startX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW);
        PrintDebugPaddedHex(startY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 1);
        PrintDebugPaddedHex(startZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 2);
        PrintDebugPaddedHex(endX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 3);
        PrintDebugPaddedHex(endY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 4);
        PrintDebugPaddedHex(endZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 5);
        PrintDebugText(g_PathDebugBlankText, PATH_DEBUG_CLEAR_COLUMN, PATH_DEBUG_CLEAR_ROW);
    }

    if (IsOffGround(startX, startZ)) {
        return FALSE;
    }

    if (IsOffGround(endX, endZ)) {
        return FALSE;
    }

    if (startX < endX) {
        lowestX = startX / WHOLE_UNIT_SIZE;
        highestX = endX / WHOLE_UNIT_SIZE;
    }
    else {
        lowestX = endX / WHOLE_UNIT_SIZE;
        highestX = startX / WHOLE_UNIT_SIZE;
    }

    if (startY < endY) {
        lowestY = startY / WHOLE_UNIT_SIZE;
        highestY = endY / WHOLE_UNIT_SIZE;
    }
    else {
        lowestY = endY / WHOLE_UNIT_SIZE;
        highestY = startY / WHOLE_UNIT_SIZE;
    }

    if (startZ < endZ) {
        lowestZ = startZ / WHOLE_UNIT_SIZE;
        highestZ = endZ / WHOLE_UNIT_SIZE;
    }
    else {
        lowestZ = endZ / WHOLE_UNIT_SIZE;
        highestZ = startZ / WHOLE_UNIT_SIZE;
    }

    nearestFraction = NO_PLANE_HIT_FRACTION;
    for (triangleIndex = 0; triangleIndex < HOLE_TRIANGLE_COUNT; triangleIndex++) {
        HoleTriangle *candidate = &g_WorkArea.hole.triangles[triangleIndex];

        if (candidate->kind != HOLE_TRIANGLE_KIND_SHADED) {
            continue;
        }

        if (highestY < candidate->bounds[AXIS_Y][BOUND_LOWEST] ||
            candidate->bounds[AXIS_Y][BOUND_HIGHEST] < lowestY) {
            continue;
        }

        if (highestX < candidate->bounds[AXIS_X][BOUND_LOWEST] ||
            candidate->bounds[AXIS_X][BOUND_HIGHEST] < lowestX) {
            continue;
        }

        if (highestZ < candidate->bounds[AXIS_Z][BOUND_LOWEST] ||
            candidate->bounds[AXIS_Z][BOUND_HIGHEST] < lowestZ) {
            continue;
        }

        triangle.corners[AXIS_X][0] = candidate->vertices[0].v.ob[AXIS_X];
        triangle.corners[AXIS_X][1] = candidate->vertices[1].v.ob[AXIS_X];
        triangle.corners[AXIS_X][2] = candidate->vertices[2].v.ob[AXIS_X];
        triangle.corners[AXIS_Y][0] = candidate->vertices[0].v.ob[AXIS_Y];
        triangle.corners[AXIS_Y][1] = candidate->vertices[1].v.ob[AXIS_Y];
        triangle.corners[AXIS_Y][2] = candidate->vertices[2].v.ob[AXIS_Y];
        triangle.corners[AXIS_Z][0] = candidate->vertices[0].v.ob[AXIS_Z];
        triangle.corners[AXIS_Z][1] = candidate->vertices[1].v.ob[AXIS_Z];
        triangle.corners[AXIS_Z][2] = candidate->vertices[2].v.ob[AXIS_Z];
        GetTrianglePlane(&triangle, &plane);
        startSide = GetPlaneSide(plane, startX, startY, startZ);
        endSide = GetPlaneSide(plane, endX, endY, endZ);
        if (DoesPathReachWall(startSide, endSide) == FALSE) {
            continue;
        }

        sideChange = startSide - endSide;
        if ((s32)sideChange != 0) {
            hitFraction = (fabsf(startSide) - WALL_HIT_DISTANCE) / fabsf(sideChange);
        }
        else {
            hitFraction = 0.0f;
        }

        if (hitFraction < 0.0f) {
            hitFraction = 0.0f;
        }

        if (!(hitFraction < nearestFraction)) {
            continue;
        }

        cornersU[0] = triangle.corners[AXIS_X][0];
        cornersU[1] = triangle.corners[AXIS_X][1];
        cornersU[2] = triangle.corners[AXIS_X][2];
        cornersV[0] = triangle.corners[AXIS_Z][0];
        cornersV[1] = triangle.corners[AXIS_Z][1];
        cornersV[2] = triangle.corners[AXIS_Z][2];
        if (candidate->kind == HOLE_TRIANGLE_KIND_SHADED) {
            facing = GetTriangleFacing(
                triangle.corners[AXIS_X][0],
                triangle.corners[AXIS_Y][0],
                triangle.corners[AXIS_Z][0],
                triangle.corners[AXIS_X][1],
                triangle.corners[AXIS_Y][1],
                triangle.corners[AXIS_Z][1],
                triangle.corners[AXIS_X][2],
                triangle.corners[AXIS_Y][2],
                triangle.corners[AXIS_Z][2]
            );

            if (facing == TRIANGLE_FACING_Z) {
                hitPointU = ((endX - startX) * hitFraction) + startX;
                hitPointV = ((endY - startY) * hitFraction) + startY;
                if (triangle.corners[AXIS_X][0] == triangle.corners[AXIS_X][1] &&
                    triangle.corners[AXIS_X][0] != triangle.corners[AXIS_X][2]) {
                    f32 edgeLength;
                    f32 farEnd;
                    f32 nearEnd;
                    endFraction = 0.0f;
                    edgeLength = Length2D(
                        triangle.corners[AXIS_X][0] - triangle.corners[AXIS_X][2],
                        triangle.corners[AXIS_Z][0] - triangle.corners[AXIS_Z][2]
                    );
                    if (triangle.corners[AXIS_X][0] < triangle.corners[AXIS_X][2]) {
                        farEnd = triangle.corners[AXIS_X][2] << WORLD_TO_COURSE_SHIFT;
                        nearEnd = triangle.corners[AXIS_X][0] << WORLD_TO_COURSE_SHIFT;
                    }
                    else {
                        farEnd = triangle.corners[AXIS_X][0] << WORLD_TO_COURSE_SHIFT;
                        nearEnd = triangle.corners[AXIS_X][2] << WORLD_TO_COURSE_SHIFT;
                    }

                    if (endX < startX) {
                        wallEnd = farEnd + WALL_END_DISTANCE;
                        if (endX <= wallEnd && wallEnd < startX) {
                            endFraction = (startX - wallEnd) / (startX - endX);
                        }
                    }

                    if (startX < endX) {
                        wallEnd = nearEnd - WALL_END_DISTANCE;
                        if (wallEnd <= endX && startX < wallEnd) {
                            endFraction = (startX - wallEnd) / (startX - endX);
                        }
                    }

                    if (endFraction != 0.0f) {
                        osSyncPrintf("X Adjust? %f,%f\n", hitFraction, endFraction);
                    }

                    if ((hitFraction >= 0.0f) & (hitFraction < endFraction)) {
                        if (fabsf(GetPlaneSide(
                                plane,
                                ((endX - startX) * endFraction) + startX,
                                ((endY - startY) * endFraction) + startY,
                                ((endZ - startZ) * endFraction) + startZ
                            )) < WALL_END_DISTANCE) {
                            osSyncPrintf("X Adjust Logic %f,%f\n", hitFraction, endFraction);
                            hitFraction = endFraction;
                            hitPointU = ((endX - startX) * endFraction) + startX;
                            hitPointV = ((endY - startY) * endFraction) + startY;
                            osSyncPrintf(
                                "x0[%f]x1[%f]x2[%f] y0[%f]y1[%f]y2[%f]\n",
                                cornersU[0] * WORLD_TO_COURSE_SCALE,
                                cornersU[1] * WORLD_TO_COURSE_SCALE,
                                cornersU[2] * WORLD_TO_COURSE_SCALE,
                                cornersV[0] * WORLD_TO_COURSE_SCALE,
                                cornersV[1] * WORLD_TO_COURSE_SCALE,
                                cornersV[2] * WORLD_TO_COURSE_SCALE
                            );
                            osSyncPrintf("xxc[%d]zzc[%d]\n", hitPointU, hitPointV);
                            if (nearestFraction < hitFraction) {
                                continue;
                            }
                        }
                    }

                    cornersU[0] += WallWidening(triangle.corners[AXIS_X], 2, edgeLength);
                    cornersU[1] += WallWidening(triangle.corners[AXIS_X], 2, edgeLength);
                    cornersU[2] -= WallWidening(triangle.corners[AXIS_X], 2, edgeLength);
                    osSyncPrintf(
                        "X2To0 %f\n",
                        WallWidening(triangle.corners[AXIS_X], 2, edgeLength)
                    );
                }
                else if (triangle.corners[AXIS_X][0] == triangle.corners[AXIS_X][2] &&
                    triangle.corners[AXIS_X][0] != triangle.corners[AXIS_X][1]) {
                    f32 edgeLength;
                    f32 farEnd;
                    f32 nearEnd;
                    endFraction = 0.0f;
                    edgeLength = Length2D(
                        triangle.corners[AXIS_X][0] - triangle.corners[AXIS_X][1],
                        triangle.corners[AXIS_Z][0] - triangle.corners[AXIS_Z][1]
                    );
                    if (triangle.corners[AXIS_X][0] < triangle.corners[AXIS_X][1]) {
                        farEnd = triangle.corners[AXIS_X][1] << WORLD_TO_COURSE_SHIFT;
                        nearEnd = triangle.corners[AXIS_X][0] << WORLD_TO_COURSE_SHIFT;
                    }
                    else {
                        farEnd = triangle.corners[AXIS_X][0] << WORLD_TO_COURSE_SHIFT;
                        nearEnd = triangle.corners[AXIS_X][1] << WORLD_TO_COURSE_SHIFT;
                    }

                    if (endX < startX) {
                        wallEnd = farEnd + WALL_END_DISTANCE;
                        if (endX <= wallEnd && wallEnd < startX) {
                            endFraction = (startX - wallEnd) / (startX - endX);
                        }
                    }

                    if (startX < endX) {
                        wallEnd = nearEnd - WALL_END_DISTANCE;
                        if (wallEnd <= endX && startX < wallEnd) {
                            endFraction = (startX - wallEnd) / (startX - endX);
                        }
                    }

                    if (endFraction != 0.0f) {
                        osSyncPrintf("X Adjust? %f,%f\n", hitFraction, endFraction);
                    }

                    if ((hitFraction >= 0.0f) & (hitFraction < endFraction)) {
                        if (fabsf(GetPlaneSide(
                                plane,
                                ((endX - startX) * endFraction) + startX,
                                ((endY - startY) * endFraction) + startY,
                                ((endZ - startZ) * endFraction) + startZ
                            )) < WALL_END_DISTANCE) {
                            osSyncPrintf("X Adjust Logic %f,%f\n", hitFraction, endFraction);
                            hitFraction = endFraction;
                            hitPointU = ((endX - startX) * endFraction) + startX;
                            hitPointV = ((endY - startY) * endFraction) + startY;
                            osSyncPrintf(
                                "x0[%f]x1[%f]x2[%f] y0[%f]y1[%f]y2[%f]\n",
                                cornersU[0] * WORLD_TO_COURSE_SCALE,
                                cornersU[1] * WORLD_TO_COURSE_SCALE,
                                cornersU[2] * WORLD_TO_COURSE_SCALE,
                                cornersV[0] * WORLD_TO_COURSE_SCALE,
                                cornersV[1] * WORLD_TO_COURSE_SCALE,
                                cornersV[2] * WORLD_TO_COURSE_SCALE
                            );
                            osSyncPrintf("xxc[%d]zzc[%d]\n", hitPointU, hitPointV);
                            if (nearestFraction < hitFraction) {
                                continue;
                            }
                        }
                    }

                    cornersU[0] += WallWidening(triangle.corners[AXIS_X], 1, edgeLength);
                    cornersU[1] -= WallWidening(triangle.corners[AXIS_X], 1, edgeLength);
                    cornersU[2] += WallWidening(triangle.corners[AXIS_X], 1, edgeLength);
                    osSyncPrintf(
                        "X1To0 %f\n",
                        WallWidening(triangle.corners[AXIS_X], 1, edgeLength)
                    );
                }
                else {
                    osSyncPrintf("Illegal Wall!!!!!!!!!\n");
                }

                if (hitFraction == 0.0f) {
                    nearestCorner = FARTHEST_CORNER_DISTANCE;
                    for (cornerIndex = 0; cornerIndex < TRIANGLE_CORNER_COUNT; cornerIndex++) {
                        cornerDistance = fabsf(
                            hitPointU -
                            (triangle.corners[AXIS_X][cornerIndex] << WORLD_TO_COURSE_SHIFT)
                        );
                        if (cornerDistance < nearestCorner) {
                            nearestCorner = cornerDistance;
                        }
                    }

                    if (nearestCorner < WALL_END_DISTANCE) {
                        hitFraction = CORNER_HIT_FRACTION;
                    }
                }

                cornersV[0] = triangle.corners[AXIS_Y][0];
                cornersV[1] = triangle.corners[AXIS_Y][1];
                cornersV[2] = triangle.corners[AXIS_Y][2];
                PrintDebugText("CheckXY axis\n", WALL_DEBUG_COLUMN, WALL_DEBUG_ROW);
                osSyncPrintf(
                    "x0[%f]x1[%f]x2[%f] y0[%f]y1[%f]y2[%f]\n",
                    cornersU[0] * WORLD_TO_COURSE_SCALE,
                    cornersU[1] * WORLD_TO_COURSE_SCALE,
                    cornersU[2] * WORLD_TO_COURSE_SCALE,
                    cornersV[0] * WORLD_TO_COURSE_SCALE,
                    cornersV[1] * WORLD_TO_COURSE_SCALE,
                    cornersV[2] * WORLD_TO_COURSE_SCALE
                );
                osSyncPrintf("xxc[%d]zzc[%d]\n", hitPointU, hitPointV);
                if (IsLessThanCorners(hitPointU, cornersU)) {
                    continue;
                }

                if (IsMoreThanCorners(hitPointU, cornersU)) {
                    continue;
                }
            }
            else if (facing == TRIANGLE_FACING_X) {
                hitPointU = ((endY - startY) * hitFraction) + startY;
                hitPointV = ((endZ - startZ) * hitFraction) + startZ;
                if (triangle.corners[AXIS_Z][0] == triangle.corners[AXIS_Z][1] &&
                    triangle.corners[AXIS_Z][0] != triangle.corners[AXIS_Z][2]) {
                    f32 edgeLength;
                    f32 farEnd;
                    f32 nearEnd;
                    endFraction = 0.0f;
                    edgeLength = Length2D(
                        triangle.corners[AXIS_Z][0] - triangle.corners[AXIS_Z][2],
                        triangle.corners[AXIS_X][0] - triangle.corners[AXIS_X][2]
                    );
                    if (triangle.corners[AXIS_Z][0] < triangle.corners[AXIS_Z][2]) {
                        farEnd = triangle.corners[AXIS_Z][2] << WORLD_TO_COURSE_SHIFT;
                        nearEnd = triangle.corners[AXIS_Z][0] << WORLD_TO_COURSE_SHIFT;
                    }
                    else {
                        farEnd = triangle.corners[AXIS_Z][0] << WORLD_TO_COURSE_SHIFT;
                        nearEnd = triangle.corners[AXIS_Z][2] << WORLD_TO_COURSE_SHIFT;
                    }

                    if (endZ < startZ) {
                        wallEnd = farEnd + WALL_END_DISTANCE;
                        if (endZ <= wallEnd && wallEnd < startZ) {
                            endFraction = (startZ - farEnd) / (startZ - endZ);
                        }
                    }

                    if (startZ < endZ) {
                        wallEnd = nearEnd - WALL_END_DISTANCE;
                        if (wallEnd <= endZ && startZ < wallEnd) {
                            endFraction = (startZ - nearEnd) / (startZ - endZ);
                        }
                    }

                    if (endFraction != 0.0f) {
                        osSyncPrintf("Z Adjust? %f,%f\n", hitFraction, endFraction);
                    }

                    if ((hitFraction >= 0.0f) & (hitFraction < endFraction)) {
                        if (fabsf(GetPlaneSide(
                                plane,
                                ((endX - startX) * endFraction) + startX,
                                ((endY - startY) * endFraction) + startY,
                                ((endZ - startZ) * endFraction) + startZ
                            )) < WALL_END_DISTANCE) {
                            osSyncPrintf("Z Adjust Logic %f,%f\n", hitFraction, endFraction);
                            hitFraction = endFraction;
                            hitPointU = ((endY - startY) * endFraction) + startY;
                            hitPointV = ((endZ - startZ) * endFraction) + startZ;
                            if (nearestFraction < hitFraction) {
                                continue;
                            }
                        }
                    }

                    cornersV[0] += WallWidening(triangle.corners[AXIS_Z], 2, edgeLength);
                    cornersV[1] += WallWidening(triangle.corners[AXIS_Z], 2, edgeLength);
                    cornersV[2] -= WallWidening(triangle.corners[AXIS_Z], 2, edgeLength);
                    osSyncPrintf(
                        "Z2To0 %f\n",
                        WallWidening(triangle.corners[AXIS_Z], 2, edgeLength)
                    );
                }
                else if (triangle.corners[AXIS_Z][0] == triangle.corners[AXIS_Z][2] &&
                    triangle.corners[AXIS_Z][0] != triangle.corners[AXIS_Z][1]) {
                    f32 edgeLength;
                    f32 farEnd;
                    f32 nearEnd;
                    endFraction = 0.0f;
                    edgeLength = Length2D(
                        triangle.corners[AXIS_Z][0] - triangle.corners[AXIS_Z][1],
                        triangle.corners[AXIS_X][0] - triangle.corners[AXIS_X][1]
                    );
                    if (triangle.corners[AXIS_Z][0] < triangle.corners[AXIS_Z][1]) {
                        farEnd = triangle.corners[AXIS_Z][1] << WORLD_TO_COURSE_SHIFT;
                        nearEnd = triangle.corners[AXIS_Z][0] << WORLD_TO_COURSE_SHIFT;
                    }
                    else {
                        farEnd = triangle.corners[AXIS_Z][0] << WORLD_TO_COURSE_SHIFT;
                        nearEnd = triangle.corners[AXIS_Z][1] << WORLD_TO_COURSE_SHIFT;
                    }

                    if (endZ < startZ) {
                        wallEnd = farEnd + WALL_END_DISTANCE;
                        if (endZ <= wallEnd && wallEnd < startZ) {
                            endFraction = (startZ - farEnd) / (startZ - endZ);
                        }
                    }

                    if (startZ < endZ) {
                        wallEnd = nearEnd - WALL_END_DISTANCE;
                        if (wallEnd <= endZ && startZ < wallEnd) {
                            endFraction = (startZ - nearEnd) / (startZ - endZ);
                        }
                    }

                    if (endFraction != 0.0f) {
                        osSyncPrintf("Z Adjust? %f,%f\n", hitFraction, endFraction);
                    }

                    if ((hitFraction >= 0.0f) & (hitFraction < endFraction)) {
                        if (fabsf(GetPlaneSide(
                                plane,
                                ((endX - startX) * endFraction) + startX,
                                ((endY - startY) * endFraction) + startY,
                                ((endZ - startZ) * endFraction) + startZ
                            )) < WALL_END_DISTANCE) {
                            osSyncPrintf("Z Adjust Logic %f,%f\n", hitFraction, endFraction);
                            hitFraction = endFraction;
                            hitPointU = ((endY - startY) * endFraction) + startY;
                            hitPointV = ((endZ - startZ) * endFraction) + startZ;
                            if (nearestFraction < hitFraction) {
                                continue;
                            }
                        }
                    }

                    cornersV[0] += WallWidening(triangle.corners[AXIS_Z], 1, edgeLength);
                    cornersV[1] -= WallWidening(triangle.corners[AXIS_Z], 1, edgeLength);
                    cornersV[2] += WallWidening(triangle.corners[AXIS_Z], 1, edgeLength);
                    osSyncPrintf(
                        "Z1To0 %f\n",
                        WallWidening(triangle.corners[AXIS_Z], 1, edgeLength)
                    );
                }
                else {
                    osSyncPrintf("Illegal Wall!!!!!!!!!\n");
                }

                if (hitFraction == 0.0f) {
                    nearestCorner = FARTHEST_CORNER_DISTANCE;
                    for (cornerIndex = 0; cornerIndex < TRIANGLE_CORNER_COUNT; cornerIndex++) {
                        cornerDistance = fabsf(
                            hitPointV -
                            (triangle.corners[AXIS_Z][cornerIndex] << WORLD_TO_COURSE_SHIFT)
                        );
                        if (cornerDistance < nearestCorner) {
                            nearestCorner = cornerDistance;
                        }
                    }

                    if (nearestCorner < WALL_END_DISTANCE) {
                        hitFraction = CORNER_HIT_FRACTION;
                    }
                }

                cornersU[0] = triangle.corners[AXIS_Y][0];
                cornersU[1] = triangle.corners[AXIS_Y][1];
                cornersU[2] = triangle.corners[AXIS_Y][2];
                osSyncPrintf(
                    "z0[%f]z1[%f]z2[%f] x0[%f]x1[%f]x2[%f]\n",
                    cornersV[0] * WORLD_TO_COURSE_SCALE,
                    cornersV[1] * WORLD_TO_COURSE_SCALE,
                    cornersV[2] * WORLD_TO_COURSE_SCALE,
                    cornersU[0] * WORLD_TO_COURSE_SCALE,
                    cornersU[1] * WORLD_TO_COURSE_SCALE,
                    cornersU[2] * WORLD_TO_COURSE_SCALE
                );
                osSyncPrintf("xxc[%d]zzc[%d]\n", hitPointU, hitPointV);
                if (IsLessThanCorners(hitPointV, cornersV)) {
                    continue;
                }

                if (IsMoreThanCorners(hitPointV, cornersV)) {
                    continue;
                }
            }
            else {
                hitPointU = ((endX - startX) * hitFraction) + startX;
                hitPointV = ((endZ - startZ) * hitFraction) + startZ;
            }
        }
        else {
            hitPointU = ((endX - startX) * hitFraction) + startX;
            hitPointV = ((endZ - startZ) * hitFraction) + startZ;
        }

        hitX = ((endX - startX) * hitFraction) + startX;
        hitY = ((endY - startY) * hitFraction) + startY;
        hitZ = ((endZ - startZ) * hitFraction) + startZ;
        if (hitY + WALL_HEIGHT_SLACK > GetGroundHeight(hitX, hitZ)) {
            nearestFraction = hitFraction;
            if (hitIndex != triangleIndex) {
                osSyncPrintf("New Logic attemped!!!!! ratio %f\n", nearestFraction);
            }

            hitIndex = triangleIndex;
        }

        edgeCrosses[0] =
            ((cornersU[1] - cornersU[0]) * (hitPointV - (cornersV[0] * WORLD_TO_COURSE_SCALE))) -
            ((cornersV[1] - cornersV[0]) * (hitPointU - (cornersU[0] * WORLD_TO_COURSE_SCALE)));
        if (edgeCrosses[0] < 0) {
            cornerIndex = EDGE_SIDE_NEGATIVE;
        }
        else {
            cornerIndex = (edgeCrosses[0] == 0) ? EDGE_SIDE_ON : EDGE_SIDE_POSITIVE;
        }

        edgeCrosses[1] =
            ((cornersU[2] - cornersU[1]) * (hitPointV - (cornersV[1] * WORLD_TO_COURSE_SCALE))) -
            ((cornersV[2] - cornersV[1]) * (hitPointU - (cornersU[1] * WORLD_TO_COURSE_SCALE)));
        if (edgeCrosses[1] < 0) {
            cornerIndex &= EDGE_SIDE_NEGATIVE;
        }
        else if (edgeCrosses[1] == 0) {
            cornerIndex &= EDGE_SIDE_ON;
        }
        else {
            cornerIndex &= EDGE_SIDE_POSITIVE;
        }

        if (cornerIndex != 0) {
            edgeCrosses[2] = ((cornersU[1] - cornersU[0]) *
                                 (hitPointV - (cornersV[0] * WORLD_TO_COURSE_SCALE))) -
                ((cornersV[1] - cornersV[0]) * (hitPointU - (cornersU[0] * WORLD_TO_COURSE_SCALE)));
            if (edgeCrosses[2] < 0) {
                cornerIndex &= EDGE_SIDE_NEGATIVE;
            }
            else if (edgeCrosses[2] == 0) {
                cornerIndex &= EDGE_SIDE_ON;
            }
            else {
                cornerIndex &= EDGE_SIDE_POSITIVE;
            }

            if (cornerIndex != 0) {
                nearestFraction = hitFraction;
                hitIndex = triangleIndex;
                if (g_IsDebugEnabled) {
                    PrintDebugText("Hit\n", WALL_DEBUG_COLUMN, WALL_DEBUG_ROW + 2);
                    osSyncPrintf("Hit Triangle %d  ratio %f\n", hitIndex, nearestFraction);
                }
            }
        }
    }

    if (hitIndex >= 0) {
        HoleTriangle *candidate = &g_WorkArea.hole.triangles[hitIndex];

        hit->corners[AXIS_X][0] = candidate->vertices[0].v.ob[AXIS_X];
        hit->corners[AXIS_X][1] = candidate->vertices[1].v.ob[AXIS_X];
        hit->corners[AXIS_X][2] = candidate->vertices[2].v.ob[AXIS_X];
        hit->corners[AXIS_Y][0] = candidate->vertices[0].v.ob[AXIS_Y];
        hit->corners[AXIS_Y][1] = candidate->vertices[1].v.ob[AXIS_Y];
        hit->corners[AXIS_Y][2] = candidate->vertices[2].v.ob[AXIS_Y];
        hit->corners[AXIS_Z][0] = candidate->vertices[0].v.ob[AXIS_Z];
        hit->corners[AXIS_Z][1] = candidate->vertices[1].v.ob[AXIS_Z];
        hit->corners[AXIS_Z][2] = candidate->vertices[2].v.ob[AXIS_Z];
        hit->tag = g_WorkArea.hole.triangles[hitIndex].kind;
        *fraction = nearestFraction;
        return TRUE;
    }

    return FALSE;
}

s32 PushBallOutOfWalls(BallSimulation *simulation, s32 checksHeight) {
    PlaneTriangle wall;
    TrianglePlane plane;
    f32 cornerXs[TRIANGLE_CORNER_COUNT];
    f32 cornerZs[TRIANGLE_CORNER_COUNT];
    Vec3 push;
    s32 x;
    s32 y;
    s32 z;
    Boolean isPushed;
    s32 lowestX;
    s32 highestX;
    s32 lowestY;
    s32 highestY;
    s32 lowestZ;
    s32 highestZ;
    s32 triangleIndex;
    s32 corner;
    HoleTriangle *candidate;
    f32 side;
    f32 cornerDistance;

    IsGameFlagSet(GAME_FLAG_PRINT_HIT_PATHS);
    x = simulation->position[AXIS_X];
    z = simulation->position[AXIS_Z];
    y = simulation->position[AXIS_Y];
    isPushed = TRUE;
    if (IsOffGround(x, z) ||
        (checksHeight == TRUE && y + WALL_HEIGHT_SLACK < GetGroundHeight(x, z))) {
        return FALSE;
    }
    while (isPushed) {
        isPushed = FALSE;
        lowestX = (x / WHOLE_UNIT_SIZE) - 1;
        highestX = (x / WHOLE_UNIT_SIZE) + 1;
        lowestY = (y / WHOLE_UNIT_SIZE) - 1;
        highestY = (y / WHOLE_UNIT_SIZE) + 1;
        lowestZ = (z / WHOLE_UNIT_SIZE) - 1;
        highestZ = (z / WHOLE_UNIT_SIZE) + 1;
        for (triangleIndex = 0; triangleIndex < HOLE_TRIANGLE_COUNT; triangleIndex++) {
            candidate = &g_WorkArea.hole.triangles[triangleIndex];
            if (candidate->kind != HOLE_TRIANGLE_KIND_SHADED) {
                continue;
            }

            if (checksHeight == TRUE) {
                if (highestY < candidate->bounds[AXIS_Y][BOUND_LOWEST]) {
                    continue;
                }

                if (candidate->bounds[AXIS_Y][BOUND_HIGHEST] < lowestY) {
                    continue;
                }
            }

            if (highestX < candidate->bounds[AXIS_X][BOUND_LOWEST]) {
                continue;
            }

            if (candidate->bounds[AXIS_X][BOUND_HIGHEST] < lowestX) {
                continue;
            }

            if (highestZ < candidate->bounds[AXIS_Z][BOUND_LOWEST]) {
                continue;
            }

            if (candidate->bounds[AXIS_Z][BOUND_HIGHEST] < lowestZ) {
                continue;
            }

            wall.corners[AXIS_X][0] = candidate->vertices[0].v.ob[AXIS_X];
            wall.corners[AXIS_X][1] = candidate->vertices[1].v.ob[AXIS_X];
            wall.corners[AXIS_X][2] = candidate->vertices[2].v.ob[AXIS_X];
            wall.corners[AXIS_Y][0] = candidate->vertices[0].v.ob[AXIS_Y];
            wall.corners[AXIS_Y][1] = candidate->vertices[1].v.ob[AXIS_Y];
            wall.corners[AXIS_Y][2] = candidate->vertices[2].v.ob[AXIS_Y];
            wall.corners[AXIS_Z][0] = candidate->vertices[0].v.ob[AXIS_Z];
            wall.corners[AXIS_Z][1] = candidate->vertices[1].v.ob[AXIS_Z];
            wall.corners[AXIS_Z][2] = candidate->vertices[2].v.ob[AXIS_Z];
            GetTrianglePlane(&wall, &plane);
            side = (plane.normal[AXIS_X] * x) + (plane.normal[AXIS_Y] * y) +
                (plane.normal[AXIS_Z] * z) + (plane.distance * WORLD_TO_COURSE_SCALE);
            osSyncPrintf("?");
            if (!(fabsf(side) < WALL_TOUCH_DISTANCE)) {
                continue;
            }

            osSyncPrintf("うっ\n");
            for (corner = 0; corner < TRIANGLE_CORNER_COUNT; corner++) {
                cornerDistance = Length2D(
                    (wall.corners[AXIS_X][corner] << WORLD_TO_COURSE_SHIFT) - x,
                    (wall.corners[AXIS_Z][corner] << WORLD_TO_COURSE_SHIFT) - z
                );
                if (cornerDistance < WALL_TOUCH_DISTANCE) {
                    push[AXIS_X] = x - (wall.corners[AXIS_X][corner] << WORLD_TO_COURSE_SHIFT);
                    push[AXIS_Y] = 0.0f;
                    push[AXIS_Z] = z - (wall.corners[AXIS_Z][corner] << WORLD_TO_COURSE_SHIFT);
                    if (cornerDistance > CORNER_LEAST_DISTANCE) {
                        guNormalize(&push[AXIS_X], &push[AXIS_Y], &push[AXIS_Z]);
                    }
                    else {
                        push[AXIS_X] = CORNER_DEFAULT_PUSH_X;
                    }

                    push[AXIS_X] *= WALL_PUSH_DISTANCE;
                    push[AXIS_Z] *= WALL_PUSH_DISTANCE;
                    x = push[AXIS_X] + (wall.corners[AXIS_X][corner] << WORLD_TO_COURSE_SHIFT);
                    z = push[AXIS_Z] + (wall.corners[AXIS_Z][corner] << WORLD_TO_COURSE_SHIFT);
                    isPushed = TRUE;
                    osSyncPrintf("WallInsideOutCorner!\n");
                    break;
                }
            }

            if (isPushed) {
                break;
            }

            if (GetTriangleFacing(
                    wall.corners[AXIS_X][0],
                    wall.corners[AXIS_Y][0],
                    wall.corners[AXIS_Z][0],
                    wall.corners[AXIS_X][1],
                    wall.corners[AXIS_Y][1],
                    wall.corners[AXIS_Z][1],
                    wall.corners[AXIS_X][2],
                    wall.corners[AXIS_Y][2],
                    wall.corners[AXIS_Z][2]
                ) == TRIANGLE_FACING_Z) {
                cornerXs[0] = wall.corners[AXIS_X][0];
                cornerXs[1] = wall.corners[AXIS_X][1];
                cornerXs[2] = wall.corners[AXIS_X][2];
                if (IsLessThanCorners((f32)x, cornerXs)) {
                    continue;
                }

                if (IsMoreThanCorners((f32)x, cornerXs)) {
                    continue;
                }
            }
            else {
                cornerZs[0] = wall.corners[AXIS_Z][0];
                cornerZs[1] = wall.corners[AXIS_Z][1];
                cornerZs[2] = wall.corners[AXIS_Z][2];
                if (IsLessThanCorners((f32)z, cornerZs)) {
                    continue;
                }

                if (IsMoreThanCorners((f32)z, cornerZs)) {
                    continue;
                }
            }

            if (side < 0.0f) {
                isPushed = TRUE;
                x += plane.normal[AXIS_X] * (-WALL_PUSH_DISTANCE - side);
                z += plane.normal[AXIS_Z] * (-WALL_PUSH_DISTANCE - side);
            }
            else {
                isPushed = TRUE;
                x += plane.normal[AXIS_X] * (WALL_PUSH_DISTANCE - side);
                z += plane.normal[AXIS_Z] * (WALL_PUSH_DISTANCE - side);
            }

            if (isPushed) {
                osSyncPrintf("WallInsideOut2!\n");
                break;
            }
        }

        if (isPushed) {
            osSyncPrintf("WallInsideOut!\n");
        }

        simulation->position[AXIS_X] = x;
        simulation->position[AXIS_Z] = z;
    }

    return UNSET_RESULT;
}

s32 FindRingHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hitRing,
    f32 *fraction
) {
    PlaneTriangle ring;
    TrianglePlane plane;
    s32 hitRingNumber;
    s32 ringNumber;
    s32 triangleIndex;
    HoleTriangle *triangle;
    f32 nearestFraction;
    f32 startSide;
    f32 endSide;
    f32 sideChange;
    f32 hitFraction;
    s32 unused[4] UNUSED;
    s32 hitX;
    s32 hitY;
    s32 hitZ;
    Boolean isDebug = IsGameFlagSet(GAME_FLAG_PRINT_HIT_PATHS);

    hitRingNumber = NO_RING_NUMBER;
    if (isDebug) {
        PrintDebugPaddedHex(startX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW);
        PrintDebugPaddedHex(startY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 1);
        PrintDebugPaddedHex(startZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 2);
        PrintDebugPaddedHex(endX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 3);
        PrintDebugPaddedHex(endY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 4);
        PrintDebugPaddedHex(endZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 5);
        PrintDebugText(g_PathDebugBlankText, PATH_DEBUG_CLEAR_COLUMN, PATH_DEBUG_CLEAR_ROW);
    }

    if (IsOffGroundSigned(startX, startZ)) {
        return -1;
    }

    if (IsOffGroundSigned(endX, endZ)) {
        return -1;
    }

    ringNumber = 0;
    nearestFraction = NO_PLANE_HIT_FRACTION;
    for (triangleIndex = 0; triangleIndex < HOLE_TRIANGLE_COUNT; triangleIndex++) {
        triangle = &g_WorkArea.hole.triangles[triangleIndex];
        if (triangle->kind != HOLE_TRIANGLE_KIND_RING) {
            continue;
        }

        ring.corners[AXIS_X][RING_CENTER] = triangle->vertices[RING_CENTER].v.ob[AXIS_X];
        ring.corners[AXIS_X][RING_FACING] = triangle->vertices[RING_FACING].v.ob[AXIS_X];
        ring.corners[AXIS_X][RING_SCALE] = triangle->vertices[RING_SCALE].v.ob[AXIS_X];
        ring.corners[AXIS_Y][RING_CENTER] = triangle->vertices[RING_CENTER].v.ob[AXIS_Y];
        ring.corners[AXIS_Y][RING_FACING] = triangle->vertices[RING_FACING].v.ob[AXIS_Y];
        ring.corners[AXIS_Y][RING_SCALE] = triangle->vertices[RING_SCALE].v.ob[AXIS_Y];
        ring.corners[AXIS_Z][RING_CENTER] = triangle->vertices[RING_CENTER].v.ob[AXIS_Z];
        ring.corners[AXIS_Z][RING_FACING] = triangle->vertices[RING_FACING].v.ob[AXIS_Z];
        ring.corners[AXIS_Z][RING_SCALE] = triangle->vertices[RING_SCALE].v.ob[AXIS_Z];
        plane.normal[AXIS_X] = ring.corners[AXIS_X][RING_FACING];
        plane.normal[AXIS_Y] = ring.corners[AXIS_Y][RING_FACING];
        plane.normal[AXIS_Z] = ring.corners[AXIS_Z][RING_FACING];
        guNormalize(&plane.normal[AXIS_X], &plane.normal[AXIS_Y], &plane.normal[AXIS_Z]);
        plane.distance =
            -((plane.normal[AXIS_X] * ring.corners[AXIS_X][RING_CENTER]) +
                (plane.normal[AXIS_Y] * ring.corners[AXIS_Y][RING_CENTER]) +
                (plane.normal[AXIS_Z] * ring.corners[AXIS_Z][RING_CENTER]));
        ringNumber++;
        startSide = (plane.normal[AXIS_X] * startX) + (plane.normal[AXIS_Y] * startY) +
            (plane.normal[AXIS_Z] * startZ) + (plane.distance * WORLD_TO_COURSE_SCALE);
        endSide = (plane.normal[AXIS_X] * endX) + (plane.normal[AXIS_Y] * endY) +
            (plane.normal[AXIS_Z] * endZ) + (plane.distance * WORLD_TO_COURSE_SCALE);
        if (DoesPathMeetRingPlane(startSide, endSide)) {
            sideChange = startSide - endSide;
            if (fabsf(sideChange) > 1.0f) {
                f32 startDistance = fabsf(startSide);

                if (sideChange < 0.0f) {
                    sideChange = -sideChange;
                }

                hitFraction = startDistance / sideChange;
            }
            else {
                hitFraction = 1.0f;
            }

            if (hitFraction < nearestFraction) {
                hitX = ((endX - startX) * hitFraction) + startX;
                hitY = ((endY - startY) * hitFraction) + startY;
                hitZ = ((endZ - startZ) * hitFraction) + startZ;
                if (Length3D(
                        (hitX * COURSE_TO_WORLD_SCALE) - ring.corners[AXIS_X][RING_CENTER],
                        (hitY * COURSE_TO_WORLD_SCALE) - ring.corners[AXIS_Y][RING_CENTER],
                        (hitZ * COURSE_TO_WORLD_SCALE) - ring.corners[AXIS_Z][RING_CENTER]
                    ) < ring.corners[AXIS_X][RING_SCALE] * RING_HIT_RADIUS_PER_SCALE) {
                    nearestFraction = hitFraction;
                    hitRingNumber = ringNumber;
                }
            }
        }
    }

    if (hitRingNumber >= 0) {
        *hitRing = ring;
        hitRing->tag = hitRingNumber - 1;
        *fraction = nearestFraction;
        return 1;
    }

    return 0;
}

s32 GetBillboardSolidity(s32 textureIndex, f32 across, f32 down) {
    s32 column = across * BILLBOARD_MASK_SIZE;
    s32 row = down * BILLBOARD_MASK_SIZE;
    s32 solidity;

    if (column < 0) {
        column = 0;
    }
    else if (column >= BILLBOARD_MASK_SIZE) {
        column = BILLBOARD_MASK_SIZE - 1;
    }

    if (row < 0) {
        row = 0;
    }
    else if (row >= BILLBOARD_MASK_SIZE) {
        row = BILLBOARD_MASK_SIZE - 1;
    }

    row = BILLBOARD_MASK_SIZE - 1 - row;
    solidity =
        (GetBillboardMaskCell(textureIndex, row, column) != '0') * BILLBOARD_CENTER_CELL_WEIGHT;
    if (row > 0) {
        solidity += GetBillboardMaskCell(textureIndex, row - 1, column) != '0';
    }

    if (row < BILLBOARD_MASK_SIZE - 1) {
        solidity += GetBillboardMaskCell(textureIndex, row + 1, column) != '0';
    }

    if (column > 0) {
        solidity += GetBillboardMaskCell(textureIndex, row, column - 1) != '0';
    }

    if (column < BILLBOARD_MASK_SIZE - 1) {
        solidity += GetBillboardMaskCell(textureIndex, row, column + 1) != '0';
    }

    return solidity;
}
