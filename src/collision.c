#include "ball_simulation.h"
#include "common.h"
#include "debug_text.h"
#include "functions.h"
#include "game_flags.h"
#include "math_util.h"
#include "save.h"

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

/* Balls nearer the cup than this, in 1/1024ths of a unit, are moved out to it */
#define CUP_EDGE_DISTANCE 614.4f

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

INCLUDE_ASM("asm/nonmatchings/collision", func_8003A078);

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

INCLUDE_ASM("asm/nonmatchings/collision", func_8003A4EC);

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

INCLUDE_ASM("asm/nonmatchings/collision", func_8003AE50);

INCLUDE_ASM("asm/nonmatchings/collision", func_8003B584);

INCLUDE_ASM("asm/nonmatchings/collision", func_8003D1A8);

INCLUDE_ASM("asm/nonmatchings/collision", func_8003D8EC);

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
