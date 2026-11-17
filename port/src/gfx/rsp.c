/*
 * The RSP's half of the interpreter: F3DEX2's display lists, matrices,
 * vertices, lights and triangles.
 *
 * The game builds most display lists natively, with words as wide as the
 * host's pointers; those it loads from ROM hold the N64's 32-bit words in
 * its byte order, as do their vertices and matrices. osPcIsRomOrder tells
 * which is which, list by list and load by load.
 */

#include "rsp.h"

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_endian.h>
#include <SDL3/SDL_stdinc.h>
#include <ultra64.h>

#include "port/os_host.h"
#include "rcp.h"

#define VERTEX_BUFFER_SIZE 64
#define MATRIX_STACK_DEPTH 32
#define DISPLAY_LIST_DEPTH 18
// The lights after the two LookAt directions: up to seven and the ambient
#define LIGHT_COUNT_MAX 8
// The size of a light in the RSP's memory, which numbers them
#define LIGHT_STRIDE 24
#define ROM_COMMAND_SIZE 8
#define DEPTH_SCALE_MAX ((float)G_MAXZ)
// What a texture coordinate's 10.5 fixed point and a texture scale's 0.16
// come to
#define TEXEL_FRACTION 32.0f
#define SCALE_FRACTION 65536.0f
// The range of texture generation's coordinates, before the texture scale
#define TEXGEN_RANGE 512.0f
#define NORMAL_SCALE 128.0f

// Without moving them the RSP clips x and y at twice w
#define CLIP_RATIO_DEFAULT 2.0f
// The integer half of the RSP's reciprocal of w at its largest
#define BEHIND_EYE_RECIPROCAL 32767.0f
// A triangle clipped by each plane gains at most one corner
#define CLIP_POLYGON_MAX (3 + CLIP_PLANE_COUNT)

typedef float Matrix[4][4];

// The planes that the RSP clips a triangle at, in the order it does
typedef enum ClipPlane {
    CLIP_PLANE_NEAR,
    CLIP_PLANE_FAR,
    CLIP_PLANE_TOP,
    CLIP_PLANE_RIGHT,
    CLIP_PLANE_BOTTOM,
    CLIP_PLANE_LEFT,
    CLIP_PLANE_COUNT
} ClipPlane;

typedef struct RspLight {
    float color[3];
    float direction[3]; // As given: normalized, in the space after modelview
} RspLight;

typedef struct DisplayList {
    const uint8_t *cursor;
    bool isRomOrder;
} DisplayList;

typedef struct RspState {
    Matrix modelview[MATRIX_STACK_DEPTH];
    int modelviewTop;
    Matrix projection;
    Matrix combined; // modelview times projection
    bool isCombinedStale;

    RspLight lights[LIGHT_COUNT_MAX];
    float lookAt[2][3];
    int lightCount;
    // The lights' and LookAt's directions in model space, for the current
    // modelview
    float modelLights[LIGHT_COUNT_MAX][3];
    float modelLookAt[2][3];
    bool areModelLightsStale;

    float viewportScale[3];
    float viewportTranslate[3];
    // The multiples of w where the RSP clips x and y, by ClipPlane
    float clipRatios[CLIP_PLANE_COUNT];
    float fogMultiplier;
    float fogOffset;
    float textureScale[2];
    RspDrawState draw;
    uint32_t otherModeHigh;
    uint32_t otherModeLow;
    RcpMemory memory;

    RspVertex vertices[VERTEX_BUFFER_SIZE];
} RspState;

static RspState f_Rsp;

static uint32_t ReadWord32(const uint8_t *bytes, bool isRomOrder) {
    uint32_t word;

    SDL_memcpy(&word, bytes, sizeof(word));
    return isRomOrder ? SDL_Swap32BE(word) : word;
}

static int16_t ReadHalf(const uint8_t *bytes, bool isRomOrder) {
    uint16_t half;

    SDL_memcpy(&half, bytes, sizeof(half));
    return (int16_t)(isRomOrder ? SDL_Swap16BE(half) : half);
}

static void SetIdentity(Matrix matrix) {
    int row;
    int column;

    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            matrix[row][column] = row == column ? 1.0f : 0.0f;
        }
    }
}

// product = first times second, in the row-vector convention, so that
// first applies first
static void MultiplyMatrices(const Matrix first, const Matrix second, Matrix product) {
    Matrix result;
    int row;
    int column;

    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            result[row][column] = (first[row][0] * second[0][column]) +
                (first[row][1] * second[1][column]) + (first[row][2] * second[2][column]) +
                (first[row][3] * second[3][column]);
        }
    }

    SDL_memcpy(product, result, sizeof(result));
}

// An Mtx's 16.16 fixed point: the integer halves of the elements, two to a
// word, then their fractions
static void ReadFixedMatrix(const uint8_t *bytes, bool isRomOrder, Matrix matrix) {
    int element;

    for (element = 0; element < 16; element++) {
        int wordOffset = (element / 2) * 4;
        int halfShift = (element & 1) ? 0 : 16;
        uint32_t integerWord = ReadWord32(bytes + wordOffset, isRomOrder);
        uint32_t fractionWord = ReadWord32(bytes + 32 + wordOffset, isRomOrder);
        int32_t integer = (int16_t)(integerWord >> halfShift);
        uint32_t fraction = (fractionWord >> halfShift) & 0xFFFF;

        matrix[element / 4][element % 4] = (float)integer + ((float)fraction / SCALE_FRACTION);
    }
}

static void LoadMatrix(uint32_t parameters, const void *address) {
    const uint8_t *bytes = address;
    Matrix matrix;

    ReadFixedMatrix(bytes, osPcIsRomOrder(bytes) != 0, matrix);
    if (parameters & G_MTX_PROJECTION) {
        if (parameters & G_MTX_LOAD) {
            SDL_memcpy(f_Rsp.projection, matrix, sizeof(matrix));
        }
        else {
            MultiplyMatrices(matrix, f_Rsp.projection, f_Rsp.projection);
        }
    }
    else {
        // gSPMatrix flips the push bit for F3DEX2, so a set bit means none
        if (!(parameters & G_MTX_PUSH) && f_Rsp.modelviewTop < MATRIX_STACK_DEPTH - 1) {
            SDL_memcpy(
                f_Rsp.modelview[f_Rsp.modelviewTop + 1],
                f_Rsp.modelview[f_Rsp.modelviewTop],
                sizeof(Matrix)
            );
            f_Rsp.modelviewTop++;
        }

        if (parameters & G_MTX_LOAD) {
            SDL_memcpy(f_Rsp.modelview[f_Rsp.modelviewTop], matrix, sizeof(matrix));
        }
        else {
            MultiplyMatrices(
                matrix,
                f_Rsp.modelview[f_Rsp.modelviewTop],
                f_Rsp.modelview[f_Rsp.modelviewTop]
            );
        }

        f_Rsp.areModelLightsStale = true;
    }

    f_Rsp.isCombinedStale = true;
}

static void PopMatrices(uint32_t count) {
    f_Rsp.modelviewTop = (int)SDL_max((int64_t)f_Rsp.modelviewTop - count, 0);
    f_Rsp.isCombinedStale = true;
    f_Rsp.areModelLightsStale = true;
}

static void Normalize(float vector[3]) {
    float length =
        SDL_sqrtf((vector[0] * vector[0]) + (vector[1] * vector[1]) + (vector[2] * vector[2]));

    if (length > 0.0f) {
        vector[0] /= length;
        vector[1] /= length;
        vector[2] /= length;
    }
}

// A direction in the space after modelview, as model space sees it: the
// modelview's rotation, transposed
static void ToModelSpace(const float direction[3], float model[3]) {
    const Matrix *modelview = &f_Rsp.modelview[f_Rsp.modelviewTop];
    int axis;

    for (axis = 0; axis < 3; axis++) {
        model[axis] = ((*modelview)[axis][0] * direction[0]) +
            ((*modelview)[axis][1] * direction[1]) + ((*modelview)[axis][2] * direction[2]);
    }

    Normalize(model);
}

static void UpdateModelLights(void) {
    int i;

    for (i = 0; i < f_Rsp.lightCount && i < LIGHT_COUNT_MAX; i++) {
        ToModelSpace(f_Rsp.lights[i].direction, f_Rsp.modelLights[i]);
    }

    ToModelSpace(f_Rsp.lookAt[0], f_Rsp.modelLookAt[0]);
    ToModelSpace(f_Rsp.lookAt[1], f_Rsp.modelLookAt[1]);
    f_Rsp.areModelLightsStale = false;
}

// A Light or LookAt: three bytes of color, its copy, then three of direction
static void ReadLight(const uint8_t *bytes, float color[3], float direction[3]) {
    int axis;

    for (axis = 0; axis < 3; axis++) {
        if (color != NULL) {
            color[axis] = (float)bytes[axis] / 255.0f;
        }

        direction[axis] = (float)(int8_t)bytes[8 + axis] / NORMAL_SCALE;
    }

    Normalize(direction);
}

static void MoveMemory(uint32_t w0, const void *address) {
    uint32_t index = w0 & 0xFF;
    uint32_t offset = ((w0 >> 8) & 0xFF) * 8;
    const uint8_t *bytes = address;
    bool isRomOrder = osPcIsRomOrder(bytes) != 0;
    int axis;

    switch (index) {
        case G_MV_VIEWPORT:
            for (axis = 0; axis < 3; axis++) {
                f_Rsp.viewportScale[axis] = (float)ReadHalf(bytes + (axis * 2), isRomOrder);
                f_Rsp.viewportTranslate[axis] = (float)ReadHalf(bytes + 8 + (axis * 2), isRomOrder);
            }

            // x and y are in quarter pixels
            f_Rsp.viewportScale[0] /= 4.0f;
            f_Rsp.viewportScale[1] /= 4.0f;
            f_Rsp.viewportTranslate[0] /= 4.0f;
            f_Rsp.viewportTranslate[1] /= 4.0f;
            break;
        case G_MV_LIGHT: {
            uint32_t slot = offset / LIGHT_STRIDE;

            if (slot < 2) {
                ReadLight(bytes, NULL, f_Rsp.lookAt[slot]);
            }
            else if (slot - 2 < LIGHT_COUNT_MAX) {
                RspLight *light = &f_Rsp.lights[slot - 2];

                ReadLight(bytes, light->color, light->direction);
            }

            f_Rsp.areModelLightsStale = true;
            break;
        }
        default:
            // Forced matrices, which the game doesn't use
            break;
    }
}

static void MoveWord(uint32_t w0, uintptr_t w1) {
    uint32_t index = (w0 >> 16) & 0xFF;
    uint32_t offset = w0 & 0xFFFF;

    switch (index) {
        case G_MW_SEGMENT:
            f_Rsp.memory.segments[(offset / 4) & 0xF] = w1;
            break;
        case G_MW_NUMLIGHT:
            f_Rsp.lightCount = (int)SDL_min((uint32_t)w1 / LIGHT_STRIDE, LIGHT_COUNT_MAX - 1);
            f_Rsp.areModelLightsStale = true;
            break;
        case G_MW_FOG:
            f_Rsp.fogMultiplier = (float)(int16_t)(w1 >> 16);
            f_Rsp.fogOffset = (float)(int16_t)w1;
            break;
        case G_MW_LIGHTCOL: {
            uint32_t light = offset / LIGHT_STRIDE;

            if ((offset % LIGHT_STRIDE) == 0 && light < LIGHT_COUNT_MAX) {
                f_Rsp.lights[light].color[0] = (float)((w1 >> 24) & 0xFF) / 255.0f;
                f_Rsp.lights[light].color[1] = (float)((w1 >> 16) & 0xFF) / 255.0f;
                f_Rsp.lights[light].color[2] = (float)((w1 >> 8) & 0xFF) / 255.0f;
            }
            break;
        }
        case G_MW_CLIP: {
            // The RSP keeps the positive sides' ratios negated
            float ratio = (float)(int16_t)w1;

            switch (offset) {
                case G_MWO_CLIP_RNX:
                    f_Rsp.clipRatios[CLIP_PLANE_LEFT] = ratio;
                    break;
                case G_MWO_CLIP_RNY:
                    f_Rsp.clipRatios[CLIP_PLANE_BOTTOM] = ratio;
                    break;
                case G_MWO_CLIP_RPX:
                    f_Rsp.clipRatios[CLIP_PLANE_RIGHT] = -ratio;
                    break;
                case G_MWO_CLIP_RPY:
                    f_Rsp.clipRatios[CLIP_PLANE_TOP] = -ratio;
                    break;
                default:
                    break;
            }
            break;
        }
        default:
            // Perspective normalization and forced matrix elements, which
            // floats don't need
            break;
    }
}

// How far inside a clipping plane a point in clip space is, in a measure
// that changes linearly between points: negative outside
static float FindPlaneDistance(const float clip[4], ClipPlane plane) {
    float w = clip[3];

    switch (plane) {
        case CLIP_PLANE_NEAR:
            return clip[2] + w;
        case CLIP_PLANE_FAR:
            return w - clip[2];
        case CLIP_PLANE_TOP:
            return (f_Rsp.clipRatios[plane] * w) - clip[1];
        case CLIP_PLANE_RIGHT:
            return (f_Rsp.clipRatios[plane] * w) - clip[0];
        case CLIP_PLANE_BOTTOM:
            return clip[1] + (f_Rsp.clipRatios[plane] * w);
        case CLIP_PLANE_LEFT:
            return clip[0] + (f_Rsp.clipRatios[plane] * w);
        default:
            SDL_assert(!"Unknown clipping plane");
            return 0.0f;
    }
}

static uint16_t FindPlaneFlag(ClipPlane plane) {
    static const uint16_t flags[CLIP_PLANE_COUNT] = {
        CLIP_NEAR,
        CLIP_FAR,
        CLIP_RATIO_TOP,
        CLIP_RATIO_RIGHT,
        CLIP_RATIO_BOTTOM,
        CLIP_RATIO_LEFT,
    };

    return flags[plane];
}

static uint16_t ClipFlags(const float clip[4]) {
    uint16_t flags = 0;
    float w = clip[3];
    int plane;

    flags |= clip[0] < -w ? CLIP_LEFT : 0;
    flags |= clip[0] > w ? CLIP_RIGHT : 0;
    flags |= clip[1] > w ? CLIP_TOP : 0;
    flags |= clip[1] < -w ? CLIP_BOTTOM : 0;
    for (plane = 0; plane < CLIP_PLANE_COUNT; plane++) {
        if (FindPlaneDistance(clip, (ClipPlane)plane) < 0.0f) {
            flags |= FindPlaneFlag((ClipPlane)plane);
        }
    }

    return flags;
}

static uint8_t ClampToByte(float value) {
    if (value <= 0.0f) {
        return 0;
    }

    if (value >= 255.0f) {
        return 255;
    }

    return (uint8_t)value;
}

static void LightVertex(const int8_t normal[3], RspVertex *vertex) {
    float model[3];
    float color[3];
    int i;

    model[0] = (float)normal[0] / NORMAL_SCALE;
    model[1] = (float)normal[1] / NORMAL_SCALE;
    model[2] = (float)normal[2] / NORMAL_SCALE;
    // The ambient light follows the directional ones
    SDL_memcpy(color, f_Rsp.lights[f_Rsp.lightCount].color, sizeof(color));
    for (i = 0; i < f_Rsp.lightCount; i++) {
        const float *direction = f_Rsp.modelLights[i];
        float intensity =
            (model[0] * direction[0]) + (model[1] * direction[1]) + (model[2] * direction[2]);

        if (intensity > 0.0f) {
            color[0] += f_Rsp.lights[i].color[0] * intensity;
            color[1] += f_Rsp.lights[i].color[1] * intensity;
            color[2] += f_Rsp.lights[i].color[2] * intensity;
        }
    }

    vertex->color[0] = ClampToByte(color[0] * 255.0f);
    vertex->color[1] = ClampToByte(color[1] * 255.0f);
    vertex->color[2] = ClampToByte(color[2] * 255.0f);

    if (f_Rsp.draw.geometryMode & G_TEXTURE_GEN) {
        int axis;

        for (axis = 0; axis < 2; axis++) {
            const float *direction = f_Rsp.modelLookAt[axis];
            float dot =
                (model[0] * direction[0]) + (model[1] * direction[1]) + (model[2] * direction[2]);
            float coordinate;

            if (f_Rsp.draw.geometryMode & G_TEXTURE_GEN_LINEAR) {
                coordinate =
                    SDL_acosf(SDL_clamp(dot, -1.0f, 1.0f)) * (2.0f * TEXGEN_RANGE) / SDL_PI_F;
            }
            else {
                coordinate = (dot + 1.0f) * TEXGEN_RANGE;
            }

            vertex->texCoord[axis] = coordinate * f_Rsp.textureScale[axis];
        }
    }
}

static void UpdateCombinedMatrix(void) {
    MultiplyMatrices(f_Rsp.modelview[f_Rsp.modelviewTop], f_Rsp.projection, f_Rsp.combined);
    f_Rsp.isCombinedStale = false;
}

// Transforms one Vtx into the RSP's vertex buffer
// Fills in the vertex's screen position and flags from its clip space one
static void ProjectVertex(RspVertex *vertex) {
    const float *clip = vertex->clip;

    vertex->screen[0] = (clip[0] * f_Rsp.viewportScale[0]) + (clip[3] * f_Rsp.viewportTranslate[0]);
    vertex->screen[1] = (clip[3] * f_Rsp.viewportTranslate[1]) - (clip[1] * f_Rsp.viewportScale[1]);
    vertex->screen[2] =
        ((clip[2] * f_Rsp.viewportScale[2]) + (clip[3] * f_Rsp.viewportTranslate[2])) /
        DEPTH_SCALE_MAX;
    vertex->screen[3] = clip[3];
    vertex->clipFlags = ClipFlags(clip);
}

// The fog factor from z over w. At or behind the eye the RSP's reciprocal
// of w saturates at its largest positive value, so the fog goes to
// whichever end z's sign points it to.
static uint8_t FindFog(const float clip[4]) {
    float w = clip[3];
    float reciprocal = w > 0.0f ? 1.0f / w : BEHIND_EYE_RECIPROCAL;

    return ClampToByte((clip[2] * reciprocal * f_Rsp.fogMultiplier) + f_Rsp.fogOffset);
}

static void LoadVertex(const uint8_t *bytes, bool isRomOrder, RspVertex *vertex) {
    const Matrix *combined = &f_Rsp.combined;
    float position[3];
    float *clip = vertex->clip;
    int axis;

    for (axis = 0; axis < 3; axis++) {
        position[axis] = (float)ReadHalf(bytes + (axis * 2), isRomOrder);
    }

    for (axis = 0; axis < 4; axis++) {
        clip[axis] = (position[0] * (*combined)[0][axis]) + (position[1] * (*combined)[1][axis]) +
            (position[2] * (*combined)[2][axis]) + (*combined)[3][axis];
    }

    ProjectVertex(vertex);

    vertex->texCoord[0] =
        (float)ReadHalf(bytes + 8, isRomOrder) * f_Rsp.textureScale[0] / TEXEL_FRACTION;
    vertex->texCoord[1] =
        (float)ReadHalf(bytes + 10, isRomOrder) * f_Rsp.textureScale[1] / TEXEL_FRACTION;
    SDL_memcpy(vertex->color, bytes + 12, 4);

    if (f_Rsp.draw.geometryMode & G_LIGHTING) {
        int8_t normal[3] = {(int8_t)bytes[12], (int8_t)bytes[13], (int8_t)bytes[14]};

        LightVertex(normal, vertex);
    }

    if (f_Rsp.draw.geometryMode & G_FOG) {
        vertex->color[3] = FindFog(clip);
    }
}

static void LoadVertices(uint32_t w0, const void *address) {
    uint32_t count = (w0 >> 12) & 0xFF;
    uint32_t end = (w0 >> 1) & 0x7F;
    const uint8_t *bytes = address;
    bool isRomOrder = osPcIsRomOrder(bytes) != 0;
    uint32_t i;

    if (count > end || end > VERTEX_BUFFER_SIZE) {
        return;
    }

    if (f_Rsp.isCombinedStale) {
        UpdateCombinedMatrix();
    }

    if ((f_Rsp.draw.geometryMode & G_LIGHTING) && f_Rsp.areModelLightsStale) {
        UpdateModelLights();
    }

    for (i = 0; i < count; i++) {
        LoadVertex(bytes + (i * sizeof(Vtx)), isRomOrder, &f_Rsp.vertices[end - count + i]);
    }
}

static void ModifyVertex(uint32_t w0, uint32_t value) {
    uint32_t index = (w0 & 0xFFFF) / 2;
    uint32_t where = (w0 >> 16) & 0xFF;
    RspVertex *vertex;

    if (index >= VERTEX_BUFFER_SIZE) {
        return;
    }

    vertex = &f_Rsp.vertices[index];
    switch (where) {
        case G_MWO_POINT_RGBA:
            vertex->color[0] = (uint8_t)(value >> 24);
            vertex->color[1] = (uint8_t)(value >> 16);
            vertex->color[2] = (uint8_t)(value >> 8);
            vertex->color[3] = (uint8_t)value;
            break;
        case G_MWO_POINT_ST:
            // The RSP's vertices hold their coordinates already scaled
            vertex->texCoord[0] = (float)(int16_t)(value >> 16) / TEXEL_FRACTION;
            vertex->texCoord[1] = (float)(int16_t)value / TEXEL_FRACTION;
            break;
        case G_MWO_POINT_XYSCREEN:
            vertex->screen[0] = (float)(int16_t)(value >> 16) / 4.0f * vertex->screen[3];
            vertex->screen[1] = (float)(int16_t)value / 4.0f * vertex->screen[3];
            break;
        case G_MWO_POINT_ZSCREEN:
            vertex->screen[2] =
                (float)(value >> 16) / (DEPTH_SCALE_MAX * 32.0f) * vertex->screen[3];
            break;
        default:
            break;
    }
}

// Whether a triangle faces away as the geometry mode culls, from its
// winding on the screen, where y goes down
static bool IsCulled(const RspVertex *first, const RspVertex *second, const RspVertex *third) {
    uint32_t cullMode = f_Rsp.draw.geometryMode & G_CULL_BOTH;
    float x[3];
    float y[3];
    float area;
    const RspVertex *corners[3] = {first, second, third};
    int i;

    if (cullMode == 0) {
        return false;
    }

    for (i = 0; i < 3; i++) {
        float w = corners[i]->screen[3];

        // A triangle crossing the eye's plane can't be judged on the screen
        if (w <= 0.0f) {
            return false;
        }

        x[i] = corners[i]->screen[0] / w;
        y[i] = corners[i]->screen[1] / w;
    }

    area = ((x[1] - x[0]) * (y[2] - y[0])) - ((y[1] - y[0]) * (x[2] - x[0]));
    if (cullMode == G_CULL_BOTH) {
        return true;
    }

    return (cullMode & G_CULL_BACK) ? area > 0.0f : area < 0.0f;
}

static void DrawUnclippedTriangle(
    const RspVertex *first,
    const RspVertex *second,
    const RspVertex *third
) {
    if (!IsCulled(first, second, third)) {
        DrawRdpTriangle(first, second, third, &f_Rsp.draw);
    }
}

// The vertex where the edge from inside a plane to outside it crosses it.
// The RSP steps every attribute along the edge in clip space, the color's
// too, so that fog stays as the corners had it, and truncates the color.
static void CutEdge(
    const RspVertex *inside,
    const RspVertex *outside,
    ClipPlane plane,
    RspVertex *vertex
) {
    float insideDistance = FindPlaneDistance(inside->clip, plane);
    float outsideDistance = FindPlaneDistance(outside->clip, plane);
    float t = SDL_clamp(insideDistance / (insideDistance - outsideDistance), 0.0f, 1.0f);
    int i;

    for (i = 0; i < 4; i++) {
        vertex->clip[i] = inside->clip[i] + ((outside->clip[i] - inside->clip[i]) * t);
        vertex->color[i] = (uint8_t)((float)inside->color[i] +
            (((float)outside->color[i] - (float)inside->color[i]) * t));
    }

    for (i = 0; i < 2; i++) {
        vertex->texCoord[i] =
            inside->texCoord[i] + ((outside->texCoord[i] - inside->texCoord[i]) * t);
    }

    ProjectVertex(vertex);
}

// Cuts the polygon down to the side of the plane that's inside, adding the
// vertices it makes to made. Returns the corners' new count.
static int ClipPolygon(
    const RspVertex *corners[CLIP_POLYGON_MAX],
    int count,
    ClipPlane plane,
    RspVertex *made,
    int *madeCount
) {
    const RspVertex *kept[CLIP_POLYGON_MAX];
    uint16_t flag = FindPlaneFlag(plane);
    const RspVertex *previous = corners[count - 1];
    int keptCount = 0;
    int i;

    for (i = 0; i < count; i++) {
        const RspVertex *corner = corners[i];
        bool isOutside = (corner->clipFlags & flag) != 0;

        if (isOutside != ((previous->clipFlags & flag) != 0)) {
            RspVertex *vertex = &made[(*madeCount)++];

            CutEdge(isOutside ? previous : corner, isOutside ? corner : previous, plane, vertex);
            kept[keptCount++] = vertex;
        }

        if (!isOutside) {
            kept[keptCount++] = corner;
        }

        previous = corner;
    }

    SDL_assert(keptCount <= CLIP_POLYGON_MAX);
    SDL_memcpy(corners, kept, sizeof(kept[0]) * (size_t)keptCount);
    return keptCount;
}

// Clips the triangle at each plane in turn, as the RSP does, and draws
// what's left as a fan of triangles about its last corner
static void DrawClippedTriangle(const RspVertex *triangle[3]) {
    const RspVertex *corners[CLIP_POLYGON_MAX];
    // Each plane makes at most two vertices
    RspVertex made[CLIP_PLANE_COUNT * 2];
    int madeCount = 0;
    int count = 3;
    int plane;
    int i;

    SDL_memcpy(corners, triangle, sizeof(triangle[0]) * 3);
    for (plane = 0; plane < CLIP_PLANE_COUNT && count >= 3; plane++) {
        count = ClipPolygon(corners, count, (ClipPlane)plane, made, &madeCount);
    }

    for (i = 0; i + 2 < count; i++) {
        DrawUnclippedTriangle(corners[i], corners[i + 1], corners[count - 1]);
    }
}

static void DrawTriangle(uint32_t first, uint32_t second, uint32_t third) {
    const RspVertex *corners[3];

    first /= 2;
    second /= 2;
    third /= 2;
    if (first >= VERTEX_BUFFER_SIZE ||
        second >= VERTEX_BUFFER_SIZE ||
        third >= VERTEX_BUFFER_SIZE) {
        return;
    }

    corners[0] = &f_Rsp.vertices[first];
    corners[1] = &f_Rsp.vertices[second];
    corners[2] = &f_Rsp.vertices[third];
    // Wholly outside one of the screen's planes
    if ((corners[0]->clipFlags & corners[1]->clipFlags & corners[2]->clipFlags & CLIP_SCREEN) !=
        0) {
        return;
    }

    // The RSP culls the pieces of a clipped triangle, whose corners have
    // places on the screen
    if (((corners[0]->clipFlags | corners[1]->clipFlags | corners[2]->clipFlags) & CLIP_CLIPPED) !=
        0) {
        DrawClippedTriangle(corners);
        return;
    }

    DrawUnclippedTriangle(corners[0], corners[1], corners[2]);
}

static bool IsDisplayListCulled(uint32_t w0, uint32_t w1) {
    uint32_t first = (w0 & 0xFFFF) / 2;
    uint32_t last = (w1 & 0xFFFF) / 2;
    uint16_t flags = CLIP_SCREEN;
    uint32_t i;

    if (last >= VERTEX_BUFFER_SIZE || first > last) {
        return false;
    }

    for (i = first; i <= last; i++) {
        flags &= f_Rsp.vertices[i].clipFlags;
    }

    return flags != 0;
}

static void SetOtherMode(uint32_t *mode, uint32_t w0, uint32_t w1) {
    uint32_t length = (w0 & 0xFF) + 1;
    uint32_t shift = 32 - ((w0 >> 8) & 0xFF) - length;
    uint32_t mask;

    if (length >= 32) {
        mask = 0xFFFFFFFF;
        shift = 0;
    }
    else {
        mask = ((1u << length) - 1) << shift;
    }

    *mode = (*mode & ~mask) | (w1 & mask);
}

static void SendOtherModes(void) {
    RcpMemory none;

    SDL_zero(none);
    RunRdpCommand(
        ((uint32_t)G_RDPSETOTHERMODE << 24) | (f_Rsp.otherModeHigh & 0xFFFFFF),
        f_Rsp.otherModeLow,
        &none
    );
}

static void SetTexture(uint32_t w0, uint32_t w1) {
    f_Rsp.textureScale[0] = (float)((w1 >> 16) & 0xFFFF) / SCALE_FRACTION;
    f_Rsp.textureScale[1] = (float)(w1 & 0xFFFF) / SCALE_FRACTION;
    f_Rsp.draw.textureTile = (int)((w0 >> 8) & 7);
    f_Rsp.draw.isTextureOn = ((w0 >> 1) & 0x7F) != 0;
}

static void ResetRsp(void) {
    int plane;

    SDL_zero(f_Rsp);
    SetIdentity(f_Rsp.modelview[0]);
    SetIdentity(f_Rsp.projection);
    f_Rsp.isCombinedStale = true;
    f_Rsp.areModelLightsStale = true;
    f_Rsp.textureScale[0] = 1.0f;
    f_Rsp.textureScale[1] = 1.0f;
    for (plane = 0; plane < CLIP_PLANE_COUNT; plane++) {
        f_Rsp.clipRatios[plane] = CLIP_RATIO_DEFAULT;
    }
}

void InitRsp(void) {
    ResetRsp();
}

// Reads a command and moves the cursor past it
static void ReadCommand(DisplayList *list, uint32_t *w0, uintptr_t *w1) {
    if (list->isRomOrder) {
        *w0 = ReadWord32(list->cursor, true);
        *w1 = ReadWord32(list->cursor + 4, true);
        list->cursor += ROM_COMMAND_SIZE;
    }
    else {
        const Gfx *command = (const Gfx *)list->cursor;

        *w0 = (uint32_t)command->words.w0;
        *w1 = command->words.w1;
        list->cursor += sizeof(Gfx);
    }
}

static DisplayList OpenDisplayList(const void *address) {
    DisplayList list;

    list.cursor = address;
    list.isRomOrder = osPcIsRomOrder(address) != 0;
    return list;
}

void RunRspTask(const Gfx *displayList) {
    DisplayList stack[DISPLAY_LIST_DEPTH];
    int depth = 0;
    // A display list longer than any the game builds has run off its end
    uint32_t commandBudget = 4u * 1024 * 1024;

    ResetRsp();
    ResetRdpForTask();
    stack[0] = OpenDisplayList(displayList);
    while (depth >= 0 && commandBudget-- != 0) {
        DisplayList *list = &stack[depth];
        uint32_t w0;
        uintptr_t w1;
        uint8_t command;

        ReadCommand(list, &w0, &w1);
        command = (uint8_t)(w0 >> 24);
        switch (command) {
            case G_VTX:
                LoadVertices(w0, ResolveRcpAddress(&f_Rsp.memory, w1));
                break;
            case G_MODIFYVTX:
                ModifyVertex(w0, (uint32_t)w1);
                break;
            case G_CULLDL:
                if (IsDisplayListCulled(w0, (uint32_t)w1)) {
                    depth--;
                }
                break;
            case G_TRI1:
                DrawTriangle((w0 >> 16) & 0xFF, (w0 >> 8) & 0xFF, w0 & 0xFF);
                break;
            case G_TRI2:
            case G_QUAD:
                DrawTriangle((w0 >> 16) & 0xFF, (w0 >> 8) & 0xFF, w0 & 0xFF);
                DrawTriangle((w1 >> 16) & 0xFF, (w1 >> 8) & 0xFF, w1 & 0xFF);
                break;
            case G_MTX:
                LoadMatrix(w0 & 0xFF, ResolveRcpAddress(&f_Rsp.memory, w1));
                break;
            case G_POPMTX:
                PopMatrices((uint32_t)w1 / sizeof(Mtx));
                break;
            case G_GEOMETRYMODE:
                f_Rsp.draw.geometryMode =
                    (f_Rsp.draw.geometryMode & (w0 & 0xFFFFFF)) | (uint32_t)w1;
                f_Rsp.areModelLightsStale = true;
                break;
            case G_TEXTURE:
                SetTexture(w0, (uint32_t)w1);
                break;
            case G_MOVEWORD:
                MoveWord(w0, w1);
                break;
            case G_MOVEMEM:
                MoveMemory(w0, ResolveRcpAddress(&f_Rsp.memory, w1));
                break;
            case G_DL: {
                DisplayList next = OpenDisplayList(ResolveRcpAddress(&f_Rsp.memory, w1));

                if (((w0 >> 16) & 0xFF) == G_DL_NOPUSH) {
                    *list = next;
                }
                else if (depth < DISPLAY_LIST_DEPTH - 1) {
                    stack[++depth] = next;
                }
                break;
            }
            case G_ENDDL:
                depth--;
                break;
            case G_SETOTHERMODE_H:
                SetOtherMode(&f_Rsp.otherModeHigh, w0, (uint32_t)w1);
                SendOtherModes();
                break;
            case G_SETOTHERMODE_L:
                SetOtherMode(&f_Rsp.otherModeLow, w0, (uint32_t)w1);
                SendOtherModes();
                break;
            case G_RDPSETOTHERMODE:
                f_Rsp.otherModeHigh = w0 & 0xFFFFFF;
                f_Rsp.otherModeLow = (uint32_t)w1;
                SendOtherModes();
                break;
            case G_TEXRECT:
            case G_TEXRECTFLIP: {
                uint32_t half0;
                uint32_t half1;
                uintptr_t texCoords;
                uintptr_t texSteps;

                // F3DEX2 sends the coordinates and steps in the two
                // RDPHALF commands that follow
                ReadCommand(list, &half0, &texCoords);
                ReadCommand(list, &half1, &texSteps);
                DrawRdpTextureRectangle(
                    w0,
                    (uint32_t)w1,
                    (uint32_t)texCoords,
                    (uint32_t)texSteps,
                    command == G_TEXRECTFLIP
                );
                break;
            }
            case G_BRANCH_Z:
            case G_RDPHALF_1:
            case G_RDPHALF_2:
            case G_SPNOOP:
            case G_NOOP:
            case G_LOAD_UCODE:
            case G_DMA_IO:
                // The game neither branches on depth nor loads microcode
                break;
            default:
                if (command >= G_RDPLOADSYNC) {
                    RunRdpCommand(w0, w1, &f_Rsp.memory);
                }
                break;
        }
    }

    FlushRdpFrame();
}
