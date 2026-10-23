#include "ball_simulation.h"
#include "billboard.h"
#include "common.h"
#include "debug_text.h"
#include "functions.h"
#include "game_flags.h"
#include "hole_triangles.h"
#include "libc.h"
#include "math_util.h"
#include "save.h"
#include "terrain.h"
#include "work_area.h"

/*
 * Each billboard texture has a hit mask: BILLBOARD_MASK_SIZE rows of
 * BILLBOARD_MASK_SIZE digits, from the billboard's foot up and from its
 * middle out to its side. A digit names what a ball hits there; '0' is
 * nothing.
 */
#define BILLBOARD_MASK_SIZE 16
#define BILLBOARD_MASK_COUNT 41
#define BILLBOARD_MASK_INDEX_COUNT 64
#define BILLBOARD_CELL_EMPTY 0
#define BILLBOARD_CELL_UNK1 1
/* Cells the masks mark with this are treated as empty */
#define BILLBOARD_CELL_IGNORED 3
#define BILLBOARD_CELL_LAST 15
/* The centre cell counts this many times as much as each neighbour */
#define BILLBOARD_CENTER_CELL_WEIGHT 4

/* Shifting a course point's coordinate right by this gives whole units, as billboards are placed in */
#define COURSE_UNIT_SHIFT 10
/* The fraction along a path that the hit tests return when nothing is hit: past its end */
#define NO_HIT_FRACTION 1.1f

/* Balls nearer the cup than this, in 1/1024ths of a unit, are moved out to it */
#define CUP_EDGE_DISTANCE 614.4f

/* The flagstick and flag, in 1/1024ths of a unit */
#define PIN_COUNT 1
#define FLAGSTICK_RADIUS 614.4f
#define FLAGSTICK_RADIUS_SQUARED 377487.38f
#define FLAG_REACH 1228.8f
#define FLAGSTICK_HEIGHT 46080.0f
#define FLAG_BOTTOM_HEIGHT 39936.0f
/* A ball that hits the flag keeps only this speed along y, which points down */
#define FLAG_HIT_SPEED_Y -524
/* A ball bounces off the flagstick with between these fractions of its speed */
#define FLAGSTICK_MOST_BOUNCE 0.8f
#define FLAGSTICK_LEAST_BOUNCE 0.2f

/* What FindPinHit returns */
#define PIN_HIT_NONE 0
#define PIN_HIT_FLAGSTICK 1
#define PIN_HIT_FLAG 2

/* Course units in a whole unit */
#define WHOLE_UNIT_SIZE (1 << COURSE_UNIT_SHIFT)
/* FindHoleTriangleHit's paths that rise by more than this many course units hit nothing */
#define PATH_MOST_RISE 153.6f
/* A path that ends this near a triangle's plane, in course units, touches it */
#define PLANE_TOUCH_DISTANCE 15.36f
#define NO_HOLE_TRIANGLE -1
/* Which sides of a triangle's edge a point is on, seen from above */
#define EDGE_SIDE_POSITIVE 1
#define EDGE_SIDE_NEGATIVE 2
#define EDGE_SIDE_ON (EDGE_SIDE_POSITIVE | EDGE_SIDE_NEGATIVE)

/* FindRingHit's use of a ring's corners */
#define RING_CENTER 0
#define RING_FACING 1
#define RING_SCALE 2 /* Only its x is used */
/* A ring's hit radius in units for each unit of its scale's x, which is 1024 at full size */
#define RING_HIT_RADIUS_PER_SCALE 0.1274414f
#define NO_RING_NUMBER -1
/* The fraction along a path that the plane hit tests return when nothing is hit */
#define NO_PLANE_HIT_FRACTION 2.0f
#define COURSE_UNITS_PER_UNIT 1024.0f
#define UNITS_PER_COURSE_UNIT (1.0f / 1024.0f)
#define PATH_DEBUG_COLUMN 2
#define PATH_DEBUG_ROW 8
#define PATH_DEBUG_CLEAR_COLUMN 25
#define PATH_DEBUG_CLEAR_ROW 3

/* Which plane a triangle faces most, as GetTriangleFacing returns it */
#define TRIANGLE_FACING_NONE -1
#define TRIANGLE_FACING_UP 0
#define TRIANGLE_FACING_X 1
#define TRIANGLE_FACING_Z 2

/* The hit mask cell of a billboard texture */
#define BILLBOARD_MASK_CELL(textureIndex, row, column)                                                               \
    g_BillboardMasks[g_BillboardMaskIndices[(textureIndex) & (BILLBOARD_MASK_INDEX_COUNT - 1)]]                      \
                    [((row) * BILLBOARD_MASK_SIZE) | (column)]

/* The hit masks, and which mask each billboard texture has */
extern s8 g_BillboardMasks[BILLBOARD_MASK_COUNT][BILLBOARD_MASK_SIZE * BILLBOARD_MASK_SIZE];
extern u8 g_BillboardMaskIndices[BILLBOARD_MASK_INDEX_COUNT];

INCLUDE_RODATA("asm/nonmatchings/collision", D_800CA710);

INCLUDE_ASM("asm/nonmatchings/collision", func_80038EB0);

INCLUDE_ASM("asm/nonmatchings/collision", func_80039748);

/*
 * Returns the cell of a billboard texture's hit mask at fractions of the
 * billboard's width out from its middle and of its height down from its top
 */
s32 GetBillboardCell(s32 textureIndex, f32 across, f32 down) {
    s32 column = across * BILLBOARD_MASK_SIZE;
    s32 row = down * BILLBOARD_MASK_SIZE;
    s32 cell;

    if (column < 0) {
        column = 0;
    } else if (column >= BILLBOARD_MASK_SIZE) {
        column = BILLBOARD_MASK_SIZE - 1;
    }
    if (row < 0) {
        row = 0;
    } else if (row >= BILLBOARD_MASK_SIZE) {
        row = BILLBOARD_MASK_SIZE - 1;
    }
    row = BILLBOARD_MASK_SIZE - 1 - row;
    cell = BILLBOARD_MASK_CELL(textureIndex, row, column);
    cell -= '0';
    if (cell < 0) {
        cell = 0;
    }
    if (cell > BILLBOARD_CELL_LAST) {
        cell = BILLBOARD_CELL_LAST;
    }
    if (cell == BILLBOARD_CELL_IGNORED) {
        cell = BILLBOARD_CELL_EMPTY;
    }
    if (IsGameFlagSet(GAME_FLAG_FILL_BILLBOARD_MASKS)) {
        cell = BILLBOARD_CELL_UNK1;
    }
    return cell;
}

/*
 * Finds the nearest billboard that a ball moving from one point to another
 * hits, treating each billboard as standing across the ball's path. It
 * sets the hit mask cell there and the fraction of the way along the path,
 * which stays above 1 when nothing is hit, and returns whether something is.
 */
#ifdef NON_MATCHING
s32 FindBillboardHit(s32 startX, s32 startY, s32 startZ, s32 endX, s32 endY, s32 endZ, s32 *cell, f32 *fraction) {
    s32 fromY = startY >> COURSE_UNIT_SHIFT;
    f32 nearestFraction = NO_HIT_FRACTION;
    s32 fromX = startX >> COURSE_UNIT_SHIFT;
    s32 toX = endX >> COURSE_UNIT_SHIFT;
    s32 deltaX = toX - fromX;
    s32 i = 0;
    s32 fromZ = startZ >> COURSE_UNIT_SHIFT;
    s32 toZ = endZ >> COURSE_UNIT_SHIFT;
    s32 toY = endY >> COURSE_UNIT_SHIFT;
    s32 isHit = FALSE;
    s32 deltaZ = toZ - fromZ;
    Billboard *billboard;
    s32 startSide;
    s32 endSide;
    f32 hitFraction;
    s32 hitX;
    s32 hitY;
    s32 hitZ;
    s32 depth;
    s32 distance;
    s32 hitCell;

    for (; i < BILLBOARD_COUNT; i++) {
        billboard = &g_Billboards[i];
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
        startSide = deltaX * (billboard->x - fromX) + deltaZ * (billboard->z - fromZ);
        endSide = deltaX * (billboard->x - toX) + deltaZ * (billboard->z - toZ);
        if ((startSide ^ endSide) >= 0) {
            continue;
        }
        if (ABS(endSide) + ABS(startSide) != 0) {
            hitFraction = fabsf(startSide) / (ABS(endSide) + ABS(startSide));
        } else {
            hitFraction = 0.0f;
        }
        if (hitFraction > nearestFraction) {
            continue;
        }
        hitX = fromX + deltaX * hitFraction;
        hitY = fromY + (toY - fromY) * hitFraction;
        hitZ = fromZ + deltaZ * hitFraction;
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
        hitCell = GetBillboardCell(billboard->textureIndex, (f32)distance / billboard->width,
                                   (f32)depth / billboard->height);
        if (hitCell != BILLBOARD_CELL_EMPTY) {
            nearestFraction = hitFraction;
            isHit = TRUE;
            *cell = hitCell;
        }
    }
    *fraction = nearestFraction;
    return isHit;
}

#else
INCLUDE_ASM("asm/nonmatchings/collision", FindBillboardHit);
#endif

/* Moves a ball that is nearer the cup than CUP_EDGE_DISTANCE out to that distance */
void MoveBallToCupEdge(BallSimulation *simulation) {
    s32 distance = IntLength2D(D_800BA9D8.cupPosition[AXIS_X] - simulation->position[AXIS_X],
                               g_CupPositionZ - simulation->position[AXIS_Z]);
    Vec3 direction;
    f32 offsetX;
    s32 deltaX;
    s32 deltaZ;
    f32 offsetZ;
    s32 *position;

    if (distance < CUP_EDGE_DISTANCE) {
        if (distance != 0) {
            direction[AXIS_Y] = 0;
            deltaX = simulation->position[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X];
            deltaZ = simulation->position[AXIS_Z] - g_CupPositionZ;
            direction[AXIS_X] = deltaX;
            direction[AXIS_Z] = deltaZ;
            guNormalize(&direction[AXIS_X], &direction[AXIS_Y], &direction[AXIS_Z]);
            offsetX = direction[AXIS_X] * CUP_EDGE_DISTANCE;
            offsetZ = direction[AXIS_Z] * CUP_EDGE_DISTANCE;
            /* The pointer is there to match; GCC keeps its stores ahead of the later cup load */
            position = simulation->position;
            position[AXIS_X] = offsetX + D_800BA9D8.cupPosition[AXIS_X];
            position[AXIS_Z] = offsetZ + g_CupPositionZ;
        }
    }
}

/*
 * Finds whether a ball moving from one point to another hits the
 * flagstick or the flag. It sets the ball's new velocity and the fraction
 * of the way along the path, and returns which it hits as a PIN_HIT_*.
 */
#ifdef NON_MATCHING
s32 FindPinHit(s32 startX, s32 startY, s32 startZ, s32 endX, s32 endY, s32 endZ, CoursePoint velocity, f32 *fraction,
               s32 unkB0, BallSimulation *simulation) {
    s32 i = 0;
    s32 deltaX = endX - startX;
    f32 nearestFraction = NO_HIT_FRACTION;
    s32 deltaZ = endZ - startZ;
    s32 hitKind = PIN_HIT_NONE;
    s32 deltaY = endY - startY;
    s32 startSide;
    s32 endSide;
    s32 endToCupX;
    s32 endToCupZ;
    f32 hitFraction;
    f32 height;
    s32 hitX;
    s32 hitZ;
    s32 isFlagHit;
    s32 distance;
    s32 speed;
    f32 chordSquared;
    f32 halfChord;
    Vec3 direction;
    s32 contactX;
    s32 contactZ;
    f32 angle;
    f32 bounceSpeed;
    s32 unused[2];

    for (; i < PIN_COUNT; i++) {
        if (startX < (s32)(D_800BA9D8.cupPosition[AXIS_X] - FLAG_REACH)) {
            if (endX < (s32)(D_800BA9D8.cupPosition[AXIS_X] - FLAG_REACH)) {
                continue;
            }
        }
        if ((s32)(D_800BA9D8.cupPosition[AXIS_X] + FLAG_REACH) < startX) {
            if ((s32)(D_800BA9D8.cupPosition[AXIS_X] + FLAG_REACH) < endX) {
                continue;
            }
        }
        if (startZ < (s32)(g_CupPositionZ - FLAG_REACH)) {
            if (endZ < (s32)(g_CupPositionZ - FLAG_REACH)) {
                continue;
            }
        }
        if ((s32)(g_CupPositionZ + FLAG_REACH) < startZ) {
            if ((s32)(g_CupPositionZ + FLAG_REACH) < endZ) {
                continue;
            }
        }
        endToCupX = D_800BA9D8.cupPosition[AXIS_X] - endX;
        endToCupZ = g_CupPositionZ - endZ;
        startSide = deltaX * (D_800BA9D8.cupPosition[AXIS_X] - startX) + deltaZ * (g_CupPositionZ - startZ);
        endSide = deltaX * endToCupX + deltaZ * endToCupZ;
        if ((startSide >= 0 && endSide >= 0) || (startSide < 0 && endSide < 0)) {
            if ((s32)IntLength2D(endToCupX, endToCupZ) <= FLAGSTICK_RADIUS) {
                hitFraction = 1.0f;
            } else {
                continue;
            }
        } else {
            if (ABS(endSide) + ABS(startSide) != 0) {
                hitFraction = fabsf(startSide) / (ABS(endSide) + ABS(startSide));
            } else {
                hitFraction = 0.0f;
            }
            if (hitFraction > nearestFraction) {
                continue;
            }
        }
        height = g_CupPositionY - (s32)(startY + deltaY * hitFraction);
        hitX = startX + deltaX * hitFraction;
        hitZ = startZ + deltaZ * hitFraction;
        isFlagHit = FALSE;
        if (height > FLAGSTICK_HEIGHT) {
            continue;
        }
        if (unkB0 == 0) {
            if (height > FLAG_BOTTOM_HEIGHT) {
                isFlagHit = TRUE;
            }
        }
        if (deltaY < 0) {
            isFlagHit &= -(IsGameFlagSet(GAME_FLAG_RISING_FLAG_HITS) != 0);
        }
        distance = IntLength2D(D_800BA9D8.cupPosition[AXIS_X] - hitX, g_CupPositionZ - hitZ);
        if (distance > FLAG_REACH) {
            continue;
        }
        if ((distance > FLAGSTICK_RADIUS) & !isFlagHit) {
            continue;
        }
        nearestFraction = hitFraction;
        speed = IntLength2D(deltaX, deltaZ);
        hitKind = PIN_HIT_FLAGSTICK;
        if (isFlagHit) {
            hitKind = PIN_HIT_FLAG;
        }
        chordSquared = FLAGSTICK_RADIUS_SQUARED - distance * distance;
        if (chordSquared >= 0.0f) {
            halfChord = sqrtf(chordSquared);
        } else {
            halfChord = FLAGSTICK_RADIUS;
        }
        direction[AXIS_X] = startX - hitX;
        direction[AXIS_Y] = 0.0f;
        direction[AXIS_Z] = startZ - hitZ;
        if (direction[AXIS_X] != 0.0f || direction[AXIS_Z] != 0.0f) {
            guNormalize(&direction[AXIS_X], &direction[AXIS_Y], &direction[AXIS_Z]);
        }
        direction[AXIS_X] *= halfChord;
        direction[AXIS_Y] *= halfChord;
        direction[AXIS_Z] *= halfChord;
        contactZ = (s32)(hitZ + direction[AXIS_Z]) - g_CupPositionZ;
        contactX = (s32)(hitX + direction[AXIS_X]) - D_800BA9D8.cupPosition[AXIS_X];
        angle = 0.0f;
        if (contactX != 0 || contactZ != 0) {
            angle = atan2(contactZ, contactX);
        }
        if (isFlagHit) {
            velocity[AXIS_X] = 0;
            velocity[AXIS_Y] = FLAG_HIT_SPEED_Y;
            velocity[AXIS_Z] = 0;
        } else {
            bounceSpeed = speed * FLAGSTICK_MOST_BOUNCE - ABS(deltaY) * 2;
            if (simulation->isRunning == 1) {
                bounceSpeed *= 0.5f;
            }
            if (bounceSpeed > speed * FLAGSTICK_MOST_BOUNCE) {
                bounceSpeed = speed * FLAGSTICK_MOST_BOUNCE;
            }
            if (bounceSpeed < speed * FLAGSTICK_LEAST_BOUNCE) {
                bounceSpeed = speed * FLAGSTICK_LEAST_BOUNCE;
            }
            velocity[AXIS_Y] = deltaY;
            velocity[AXIS_X] = cosf(angle) * bounceSpeed;
            velocity[AXIS_Z] = sinf(angle) * bounceSpeed;
        }
    }
    *fraction = nearestFraction;
    return hitKind;
}

#else
INCLUDE_ASM("asm/nonmatchings/collision", FindPinHit);
#endif

/*
 * Returns which plane a triangle faces most: the one its normal's largest
 * component is across, or TRIANGLE_FACING_NONE for a triangle without area
 */
s32 GetTriangleFacing(s32 firstX, s32 firstY, s32 firstZ, s32 secondX, s32 secondY, s32 secondZ, s32 thirdX,
                      s32 thirdY, s32 thirdZ) {
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
    s32 facing;

    secondDeltaZ = secondZ - firstZ;
    thirdDeltaX = thirdX - firstX;
    thirdDeltaZ = thirdZ - firstZ;
    secondDeltaX = secondX - firstX;
    largest = 0;
    normalY = secondDeltaX * thirdDeltaZ - secondDeltaZ * thirdDeltaX;
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
    normalX = secondDeltaY * thirdDeltaZ - secondDeltaZ * thirdDeltaY;
    if (normalX < 0) {
        normalX = -normalX;
    }
    if (normalX > largest) {
        facing = TRIANGLE_FACING_X;
        largest = normalX;
    }
    normalZ = secondDeltaX * thirdDeltaY - secondDeltaY * thirdDeltaX;
    if (normalZ < 0) {
        normalZ = -normalZ;
    }
    if (normalZ > largest) {
        facing = TRIANGLE_FACING_Z;
    }
    return facing;
}

/* An unreferenced copy of GetTrianglePlane that prints its error even without debugging */
s32 GetTrianglePlaneCopy(PlaneTriangle *triangle, TrianglePlane *plane) {
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
    firstY = triangle->corners[AXIS_Y][1]; /* Split from the subtraction to match */
    firstY -= triangle->corners[AXIS_Y][0];
    secondY = triangle->corners[AXIS_Y][2] - triangle->corners[AXIS_Y][0];
    firstZ = triangle->corners[AXIS_Z][1] - triangle->corners[AXIS_Z][0];
    firstX = triangle->corners[AXIS_X][1] - triangle->corners[AXIS_X][0];
    secondX = triangle->corners[AXIS_X][2] - triangle->corners[AXIS_X][0];
    normalX = firstY * secondZ - secondY * firstZ;
    normalY = -firstX * secondZ + secondX * firstZ;
    normalZ = -firstY * secondX + secondY * firstX;
    length = sqrtf(normalX * normalX + normalY * normalY + normalZ * normalZ);
    distance = -(normalX * triangle->corners[AXIS_X][0] + normalY * triangle->corners[AXIS_Y][0] +
                 normalZ * triangle->corners[AXIS_Z][0]);
    if (length == 0.0f) {
        PrintDebugText("NormalFError", 3, 15);
        plane->normal[AXIS_X] = 0.0f;
        plane->normal[AXIS_Z] = 0.0f;
        plane->distance = 0.0f;
        plane->normal[AXIS_Y] = 1.0f;
        plane->unk10 = 1.0f;
        return -1;
    }
    if (normalY < 0.0f) {
        length = -length;
    }
    scale = 1.0f / length;
    plane->unk10 = 1.0f;
    plane->normal[AXIS_X] = normalX * scale;
    plane->normal[AXIS_Y] = normalY * scale;
    plane->normal[AXIS_Z] = normalZ * scale;
    plane->distance = distance * scale;
    return 0;
}

/*
 * Finds the nearest hole triangle, other than the rings and the shaded
 * triangles, that a ball moving from one point to another hits. It sets
 * a copy of the triangle's corners, holding its kind, and the fraction of
 * the way along the path, and returns whether one is hit. Paths that rise
 * by more than PATH_MOST_RISE, as course y points down, or that start or
 * end off the ground hit nothing.
 */
#ifdef NON_MATCHING
s32 FindHoleTriangleHit(s32 startX, s32 startY, s32 startZ, s32 endX, s32 endY, s32 endZ, PlaneTriangle *hit,
                        f32 *fraction) {
    PlaneTriangle triangle;
    TrianglePlane plane;
    s32 edgeCrosses[TRIANGLE_CORNER_COUNT];
    s32 lowestX;
    s32 highestX;
    s32 lowestY;
    s32 highestY;
    s32 lowestZ;
    s32 highestZ;
    s32 hitIndex = NO_HOLE_TRIANGLE;
    s32 i;
    HoleTriangle *candidate;
    f32 nearestFraction;
    f32 startSide;
    f32 endSide;
    f32 sideChange;
    f32 hitFraction;
    s32 hitX;
    s32 hitZ;
    s32 sides;

    if (IsGameFlagSet(GAME_FLAG_UNK0)) {
        PrintDebugPaddedHex(startX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW);
        PrintDebugPaddedHex(startY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 1);
        PrintDebugPaddedHex(startZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 2);
        PrintDebugPaddedHex(endX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 3);
        PrintDebugPaddedHex(endY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 4);
        PrintDebugPaddedHex(endZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 5);
        PrintDebugText("          ", PATH_DEBUG_CLEAR_COLUMN, PATH_DEBUG_CLEAR_ROW);
    }
    if (endY + PATH_MOST_RISE < startY) {
        return FALSE;
    }
    if ((u32)startX >= GROUND_WIDTH || (u32)startZ >= GROUND_LENGTH) {
        return FALSE;
    }
    if ((u32)endX >= GROUND_WIDTH || (u32)endZ >= GROUND_LENGTH) {
        return FALSE;
    }
    if (startX < endX) {
        lowestX = startX / WHOLE_UNIT_SIZE;
        highestX = endX / WHOLE_UNIT_SIZE;
    } else {
        lowestX = endX / WHOLE_UNIT_SIZE;
        highestX = startX / WHOLE_UNIT_SIZE;
    }
    if (startY < endY) {
        lowestY = startY / WHOLE_UNIT_SIZE;
        highestY = endY / WHOLE_UNIT_SIZE;
    } else {
        lowestY = endY / WHOLE_UNIT_SIZE;
        highestY = startY / WHOLE_UNIT_SIZE;
    }
    if (startZ < endZ) {
        lowestZ = startZ / WHOLE_UNIT_SIZE;
        highestZ = endZ / WHOLE_UNIT_SIZE;
    } else {
        lowestZ = endZ / WHOLE_UNIT_SIZE;
        highestZ = startZ / WHOLE_UNIT_SIZE;
    }
    nearestFraction = NO_PLANE_HIT_FRACTION;
    for (i = 0; i < HOLE_TRIANGLE_COUNT; i++) {
        candidate = &g_WorkArea.hole.triangles[i];
        if (candidate->kind == HOLE_TRIANGLE_KIND_RING) {
            continue;
        }
        if (candidate->kind == HOLE_TRIANGLE_KIND_SHADED) {
            continue;
        }
        if (highestY < candidate->bounds[AXIS_Y][0] || candidate->bounds[AXIS_Y][1] < lowestY) {
            continue;
        }
        if (highestX < candidate->bounds[AXIS_X][0] || candidate->bounds[AXIS_X][1] < lowestX) {
            continue;
        }
        if (highestZ < candidate->bounds[AXIS_Z][0] || candidate->bounds[AXIS_Z][1] < lowestZ) {
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
        startSide = plane.normal[AXIS_X] * startX + plane.normal[AXIS_Y] * startY + plane.normal[AXIS_Z] * startZ +
                    plane.distance * COURSE_UNITS_PER_UNIT;
        endSide = plane.normal[AXIS_X] * endX + plane.normal[AXIS_Y] * endY + plane.normal[AXIS_Z] * endZ +
                  plane.distance * COURSE_UNITS_PER_UNIT;
        if (fabsf(endSide) < PLANE_TOUCH_DISTANCE || ((startSide > 0.0f) & (endSide < 0.0f)) ||
            ((startSide < 0.0f) & (endSide > 0.0f))) {
            sideChange = startSide - endSide;
            if ((s32)sideChange != 0) {
                hitFraction = fabsf(startSide) / fabsf(sideChange);
            } else {
                hitFraction = 0.0f;
            }
            if (hitFraction < nearestFraction) {
                hitZ = (endZ - startZ) * hitFraction + startZ;
                hitX = (endX - startX) * hitFraction + startX;
                /*
                 * Each cross product tells which side of an edge the hit is on,
                 * seen from above. The third repeats the first edge, so the edge
                 * from the third corner back to the first is never tested.
                 */
                edgeCrosses[0] =
                    (triangle.corners[AXIS_X][1] - triangle.corners[AXIS_X][0]) *
                        (hitZ - (triangle.corners[AXIS_Z][0] << COURSE_UNIT_SHIFT)) -
                    (triangle.corners[AXIS_Z][1] - triangle.corners[AXIS_Z][0]) *
                        (hitX - (triangle.corners[AXIS_X][0] << COURSE_UNIT_SHIFT));
                if (edgeCrosses[0] < 0) {
                    sides = EDGE_SIDE_NEGATIVE;
                } else {
                    sides = (edgeCrosses[0] == 0) ? EDGE_SIDE_ON : EDGE_SIDE_POSITIVE;
                }
                edgeCrosses[1] =
                    (triangle.corners[AXIS_X][2] - triangle.corners[AXIS_X][1]) *
                        (hitZ - (triangle.corners[AXIS_Z][1] << COURSE_UNIT_SHIFT)) -
                    (triangle.corners[AXIS_Z][2] - triangle.corners[AXIS_Z][1]) *
                        (hitX - (triangle.corners[AXIS_X][1] << COURSE_UNIT_SHIFT));
                if (edgeCrosses[1] < 0) {
                    sides &= EDGE_SIDE_NEGATIVE;
                } else if (edgeCrosses[1] == 0) {
                    sides &= EDGE_SIDE_ON;
                } else {
                    sides &= EDGE_SIDE_POSITIVE;
                }
                if (sides != 0) {
                    edgeCrosses[2] =
                        (triangle.corners[AXIS_X][1] - triangle.corners[AXIS_X][0]) *
                            (hitZ - (triangle.corners[AXIS_Z][0] << COURSE_UNIT_SHIFT)) -
                        (triangle.corners[AXIS_Z][1] - triangle.corners[AXIS_Z][0]) *
                            (hitX - (triangle.corners[AXIS_X][0] << COURSE_UNIT_SHIFT));
                    if (edgeCrosses[2] < 0) {
                        sides &= EDGE_SIDE_NEGATIVE;
                    } else if (edgeCrosses[2] == 0) {
                        sides &= EDGE_SIDE_ON;
                    } else {
                        sides &= EDGE_SIDE_POSITIVE;
                    }
                    if (sides != 0) {
                        nearestFraction = hitFraction;
                        hitIndex = i;
                    }
                }
            }
        }
    }
    if (hitIndex >= 0) {
        hit->corners[AXIS_X][0] = g_WorkArea.hole.triangles[hitIndex].vertices[0].v.ob[AXIS_X];
        hit->corners[AXIS_X][1] = g_WorkArea.hole.triangles[hitIndex].vertices[1].v.ob[AXIS_X];
        hit->corners[AXIS_X][2] = g_WorkArea.hole.triangles[hitIndex].vertices[2].v.ob[AXIS_X];
        hit->corners[AXIS_Y][0] = g_WorkArea.hole.triangles[hitIndex].vertices[0].v.ob[AXIS_Y];
        hit->corners[AXIS_Y][1] = g_WorkArea.hole.triangles[hitIndex].vertices[1].v.ob[AXIS_Y];
        hit->corners[AXIS_Y][2] = g_WorkArea.hole.triangles[hitIndex].vertices[2].v.ob[AXIS_Y];
        hit->corners[AXIS_Z][0] = g_WorkArea.hole.triangles[hitIndex].vertices[0].v.ob[AXIS_Z];
        hit->corners[AXIS_Z][1] = g_WorkArea.hole.triangles[hitIndex].vertices[1].v.ob[AXIS_Z];
        hit->corners[AXIS_Z][2] = g_WorkArea.hole.triangles[hitIndex].vertices[2].v.ob[AXIS_Z];
        hit->unk0 = g_WorkArea.hole.triangles[hitIndex].kind;
        *fraction = nearestFraction;
        return TRUE;
    }
    return FALSE;
}

#else
INCLUDE_ASM("asm/nonmatchings/collision", FindHoleTriangleHit);
#endif

INCLUDE_ASM("asm/nonmatchings/collision", func_8003B584);

INCLUDE_ASM("asm/nonmatchings/collision", func_8003D1A8);

/*
 * Finds the nearest ring that a ball moving from one point to another
 * passes through. It sets a copy of the last ring's corners, holding the
 * hit ring's index, and the fraction of the way along the path. It returns
 * 1 for a hit, 0 for none and -1 for a start or end off the ground.
 */
#ifdef NON_MATCHING
s32 FindRingHit(s32 startX, s32 startY, s32 startZ, s32 endX, s32 endY, s32 endZ, PlaneTriangle *hitRing,
                f32 *fraction) {
    PlaneTriangle ring;
    TrianglePlane plane;
    s32 hitNumber = NO_RING_NUMBER;
    s32 ringNumber;
    s32 i;
    HoleTriangle *triangle;
    f32 nearestFraction;
    f32 startSide;
    f32 endSide;
    f32 sideChange;
    f32 hitFraction;
    s32 unused[4];

    if (IsGameFlagSet(GAME_FLAG_UNK0)) {
        PrintDebugPaddedHex(startX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW);
        PrintDebugPaddedHex(startY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 1);
        PrintDebugPaddedHex(startZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 2);
        PrintDebugPaddedHex(endX, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 3);
        PrintDebugPaddedHex(endY, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 4);
        PrintDebugPaddedHex(endZ, PATH_DEBUG_COLUMN, PATH_DEBUG_ROW + 5);
        PrintDebugText("          ", PATH_DEBUG_CLEAR_COLUMN, PATH_DEBUG_CLEAR_ROW);
    }
    if (startX < 0 || startX >= GROUND_WIDTH || startZ < 0 || startZ >= GROUND_LENGTH) {
        return -1;
    }
    if (endX < 0 || endX >= GROUND_WIDTH || endZ < 0 || endZ >= GROUND_LENGTH) {
        return -1;
    }
    ringNumber = 0;
    nearestFraction = NO_PLANE_HIT_FRACTION;
    for (i = 0; i < HOLE_TRIANGLE_COUNT; i++) {
        triangle = &g_WorkArea.hole.triangles[i];
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
        plane.distance = -(plane.normal[AXIS_X] * ring.corners[AXIS_X][RING_CENTER] +
                           plane.normal[AXIS_Y] * ring.corners[AXIS_Y][RING_CENTER] +
                           plane.normal[AXIS_Z] * ring.corners[AXIS_Z][RING_CENTER]);
        ringNumber++;
        startSide = plane.normal[AXIS_X] * startX + plane.normal[AXIS_Y] * startY + plane.normal[AXIS_Z] * startZ +
                    plane.distance * COURSE_UNITS_PER_UNIT;
        endSide = plane.normal[AXIS_X] * endX + plane.normal[AXIS_Y] * endY + plane.normal[AXIS_Z] * endZ +
                  plane.distance * COURSE_UNITS_PER_UNIT;
        if (endSide == 0.0f || (startSide > 0.0f && endSide < 0.0f) || (startSide < 0.0f && endSide > 0.0f)) {
            sideChange = startSide - endSide;
            if (fabsf(sideChange) > 1.0f) {
                if (sideChange < 0.0f) {
                    sideChange = -sideChange;
                }
                startSide = fabsf(startSide);
                hitFraction = startSide / sideChange;
            } else {
                hitFraction = 1.0f;
            }
            if (hitFraction < nearestFraction) {
                if (Length3D((s32)((endX - startX) * hitFraction + startX) * UNITS_PER_COURSE_UNIT -
                                 ring.corners[AXIS_X][RING_CENTER],
                             (s32)((endY - startY) * hitFraction + startY) * UNITS_PER_COURSE_UNIT -
                                 ring.corners[AXIS_Y][RING_CENTER],
                             (s32)((endZ - startZ) * hitFraction + startZ) * UNITS_PER_COURSE_UNIT -
                                 ring.corners[AXIS_Z][RING_CENTER]) <
                    ring.corners[AXIS_X][RING_SCALE] * RING_HIT_RADIUS_PER_SCALE) {
                    nearestFraction = hitFraction;
                    hitNumber = ringNumber;
                }
            }
        }
    }
    if (hitNumber >= 0) {
        *hitRing = ring;
        hitRing->unk0 = hitNumber - 1;
        *fraction = nearestFraction;
        return 1;
    }
    return 0;
}

#else
INCLUDE_ASM("asm/nonmatchings/collision", FindRingHit);
#endif

/*
 * Returns how solid a billboard's hit mask is around a point: the cell
 * there counted BILLBOARD_CENTER_CELL_WEIGHT times, plus each of its four
 * neighbours that is not empty
 */
s32 GetBillboardSolidity(s32 textureIndex, f32 across, f32 down) {
    s32 column = across * BILLBOARD_MASK_SIZE;
    s32 row = down * BILLBOARD_MASK_SIZE;
    s32 solidity;

    if (column < 0) {
        column = 0;
    } else if (column >= BILLBOARD_MASK_SIZE) {
        column = BILLBOARD_MASK_SIZE - 1;
    }
    if (row < 0) {
        row = 0;
    } else if (row >= BILLBOARD_MASK_SIZE) {
        row = BILLBOARD_MASK_SIZE - 1;
    }
    row = BILLBOARD_MASK_SIZE - 1 - row;
    solidity = (BILLBOARD_MASK_CELL(textureIndex, row, column) != '0') * BILLBOARD_CENTER_CELL_WEIGHT;
    if (row > 0) {
        solidity += BILLBOARD_MASK_CELL(textureIndex, row - 1, column) != '0';
    }
    if (row < BILLBOARD_MASK_SIZE - 1) {
        solidity += BILLBOARD_MASK_CELL(textureIndex, row + 1, column) != '0';
    }
    if (column > 0) {
        solidity += BILLBOARD_MASK_CELL(textureIndex, row, column - 1) != '0';
    }
    if (column < BILLBOARD_MASK_SIZE - 1) {
        solidity += BILLBOARD_MASK_CELL(textureIndex, row, column + 1) != '0';
    }
    return solidity;
}

INCLUDE_RODATA("asm/nonmatchings/collision", D_800CA890);

INCLUDE_RODATA("asm/nonmatchings/collision", D_800CA898);

INCLUDE_RODATA("asm/nonmatchings/collision", D_800CA8B0);

INCLUDE_RODATA("asm/nonmatchings/collision", D_800CA8C4);
