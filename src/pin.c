#include "pin.h"

#include <math.h>
#include <ultra64.h>

#include "camera.h"
#include "course_space.h"
#include "current_shot.h"
#include "game.h"
#include "game_state.h"
#include "hole_conditions.h"
#include "hole_setup.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round_hud.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "shot_camera.h"
#include "surface.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/memory_address.h"
#include "terrain.h"
#include "view_projection.h"

#define CUP_DISTANCE_OFFSET 0x699

#define PIN_SHADOW_ROW_COUNT 9
#define PIN_SHADOW_VERTEX_COUNT (PIN_SHADOW_ROW_COUNT * 2)
#define PIN_SHADOW_ROW_SPACING 8533.0f
#define PIN_SHADOW_WIDTH_STEP 46.079998f
#define COURSE_UNITS_PER_VERTEX_UNIT 4

#define ASSET_SMALL_CUP_TEXTURE 0x4C9
#define ASSET_CUP_TEXTURE 0x4CA
#define ASSET_FLAGSTICK_TEXTURE 0x703
#define ASSET_PIN_SHADOW_TEXTURE 0x704

#define PIN_SCALE 0.1f
#define PIN_HEADING_OFFSET 3.14f

#define PIN_SHADOW_SCALE (4.0f / 1024.0f)
#define LARGE_PIN_SHADOW_SCALE (6.0f / 1024.0f)
#define PIN_SHADOW_TEXTURE_WIDTH 16
#define PIN_SHADOW_TEXTURE_HEIGHT 1
#define CUP_TEXTURE_WIDTH 32
#define CUP_TEXTURE_HEIGHT 64
#define SMALL_CUP_TEXTURE_SIZE 8
#define FLAGSTICK_TEXTURE_SIZE 16

#define CUP_HALF_HEIGHT_MAX 5.0f
#define CUP_HALF_HEIGHT_MIN 1.0f
#define STICK_FOOT_HALF_DISTANCE 614.4f

#define PART_DEPTH_BIAS 120
#define CUP_DEPTH_HEIGHT 1.5f
#define CUP_DEPTH_OFFSET 60.0
#define CUP_WALL_DEPTH_HEIGHT 10.5f
#define CUP_WALL_DEPTH_OFFSET 20.0f
#define FLAGSTICK_DEPTH_HEIGHT 7
#define FLAGSTICK_DEPTH_OFFSET 40.0f

#define CUP_SCREEN_RADIUS 1.7999999999999998
#define SCREEN_CUP_SIZE_MAX 10.0f
#define FLAGSTICK_SCREEN_RADIUS 0.6000000000000001
#define SCREEN_FLAGSTICK_SIZE_MAX 2.0f
#define SCREEN_CUP_LEFT (-4)
#define SCREEN_CUP_RIGHT 5
#define SCREEN_CUP_WALL_HEIGHT 6

#define CUP_BASE_SHADE 0x5C
#define CUP_SHADE_MIN 0x7F
#define CUP_SHADE_MAX 0xFF
#define CUP_SHADE_ALPHA 0xBF

#define FLAG_SCALE 0.01f
#define FLAG_WAVE_STEP_DEGREES 5
#define FLAG_WAVE_PER_WIND 0.02f
#define FLAG_WAVE_MIN 0.05f

#define PIN_VERTEX_COUNT 12
#define PIN_VERTEX_SCALE 2
#define LARGE_PIN_VERTEX_SCALE 4
#define SMALL_CUP_DEPTH (-200)
#define FAR_FLAGSTICK_DEPTH (-100)
#define LARGE_CUP_PRIM_DEPTH 0x403B
#define LARGE_CUP_WALL_PRIM_DEPTH 0x4010
#define LARGE_FLAGSTICK_PRIM_DEPTH 0x4027

#define SmallCupTextureCoordinates(s, t) \
    TextureCoordinateWord(TexelCoordinate(s), TexelCoordinate(t))
#define ScreenXY(x, y) \
    ScreenPointWord((x) * QUARTER_PIXELS_PER_PIXEL, (s32)((y) * QUARTER_PIXELS_PER_PIXEL))
#define SCREEN_FLAGSTICK_HALF_WIDTH 2
#define SCREEN_FLAGSTICK_FOOT_RISE 1
#define PRIM_DEPTH_MASK (~3)

s32 g_CupViewDepth;

s32 g_PinVertexScale = PIN_VERTEX_SCALE;

static u8 f_UnusedBytesAfterVertexScale[4] UNUSED = {0};

static Vtx f_PinVertices[PIN_VERTEX_COUNT] = {
    {{{-16, 0, -16}, 0, {0, 0}, {0xBF, 0xBF, 0xBF, 0xFF}}},
    {{{-16, 0, 16}, 0, {0, 4096}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{16, 0, 16}, 0, {4096, 4096}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{16, 0, -16}, 0, {4096, 0}, {0xBF, 0xBF, 0xBF, 0xFF}}},
    {{{-16, 0, 0}, 0, {64, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-16, 0, 99}, 0, {64, 10240}, {0xDF, 0xDF, 0xDF, 0xFF}}},
    {{{16, 0, 99}, 0, {4096, 10240}, {0x70, 0x70, 0x70, 0xFF}}},
    {{{16, 0, 0}, 0, {4096, 2048}, {0x80, 0x80, 0x80, 0xFF}}},
    {{{-3, -450, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-3, 8, 0}, 0, {0, 3072}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{3, 8, 0}, 0, {1024, 3072}, {0x7F, 0x7F, 0x7F, 0xFF}}},
    {{{3, -450, 0}, 0, {1024, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
};
static Vtx f_LargePinVertices[PIN_VERTEX_COUNT] = {
    {{{-33, 0, -33}, 0, {0, 0}, {0xBF, 0xBF, 0xBF, 0xFF}}},
    {{{-33, 0, 33}, 0, {0, 4096}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{33, 0, 33}, 0, {4096, 4096}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{33, 0, -33}, 0, {4096, 0}, {0xBF, 0xBF, 0xBF, 0xFF}}},
    {{{-33, 0, 0}, 0, {64, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-33, 0, 198}, 0, {64, 14336}, {0xDF, 0xDF, 0xDF, 0xFF}}},
    {{{33, 0, 198}, 0, {4096, 14336}, {0x70, 0x70, 0x70, 0xFF}}},
    {{{33, 0, 0}, 0, {4096, 2048}, {0x80, 0x80, 0x80, 0xFF}}},
    {{{-6, -450, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-6, 33, 0}, 0, {0, 3072}, {0xDF, 0xDF, 0xDF, 0xFF}}},
    {{{6, 33, 0}, 0, {1024, 3072}, {0x70, 0x70, 0x70, 0xFF}}},
    {{{6, -450, 0}, 0, {1024, 0}, {0x80, 0x80, 0x80, 0xFF}}},
};
static Vtx f_PinShadowVertices[PIN_SHADOW_VERTEX_COUNT] = {
    {{{-40, 0, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x60}}},
    {{{40, 0, 0}, 0, {1024, 0}, {0x00, 0x00, 0x00, 0x60}}},
    {{{-30, 0, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x5C}}},
    {{{30, 0, 0}, 0, {1024, 0}, {0x00, 0x00, 0x00, 0x5C}}},
    {{{-30, 0, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x58}}},
    {{{30, 0, 0}, 0, {1024, 0}, {0x00, 0x00, 0x00, 0x58}}},
    {{{-30, 0, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x54}}},
    {{{30, 0, 0}, 0, {1024, 0}, {0x00, 0x00, 0x00, 0x54}}},
    {{{-30, 0, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x50}}},
    {{{30, 0, 0}, 0, {1024, 0}, {0x00, 0x00, 0x00, 0x50}}},
    {{{-30, 0, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x4C}}},
    {{{30, 0, 0}, 0, {1024, 0}, {0x00, 0x00, 0x00, 0x4C}}},
    {{{-30, 0, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x48}}},
    {{{30, 0, 0}, 0, {1024, 0}, {0x00, 0x00, 0x00, 0x48}}},
    {{{-30, 0, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x44}}},
    {{{30, 0, 0}, 0, {1024, 0}, {0x00, 0x00, 0x00, 0x44}}},
    {{{-30, 0, 0}, 0, {0, 0}, {0x00, 0x00, 0x00, 0x00}}},
    {{{30, 0, 0}, 0, {1024, 0}, {0x00, 0x00, 0x00, 0x00}}},
};
static Boolean8 f_IsPinOut = FALSE;
static Boolean8 f_SkipsNextPinDraw = FALSE;
static f32 f_FlagWavePhase = 0.0f;

static Mtx f_PinShadowMatrix;
static Mtx f_FlagMatrices[DRAW_BUFFER_COUNT];
static u8 *f_SmallCupTexture;
static u8 *f_CupTexture;
static u8 *f_FlagstickTexture;
static u8 *f_PinShadowTexture;
static Mtx f_PinMatrices[CAMERA_COUNT][DRAW_BUFFER_COUNT];
static u8 f_UnusedBufferAfterPinMatrices[0xC0] UNUSED;

s32 GetCupDistance(s32 x, s32 z) {
    s32 distance = IntLength2D(x, z);

    if ((u32)distance < CUP_DISTANCE_OFFSET) {
        distance = 0;
    }
    else {
        distance -= CUP_DISTANCE_OFFSET;
    }

    return distance;
}

void BuildPinShadow(void) {
    s32 points[PIN_SHADOW_VERTEX_COUNT][AXIS_COUNT];
    s32 i;
    Radians heading = (g_HoleSetup.conditions.lightYaw * BINARY_ANGLE_TO_RADIANS) + (M_PI / 2);
    f32 rowSpacing = PIN_SHADOW_ROW_SPACING;

    for (i = 0; i != PIN_SHADOW_ROW_COUNT; i++) {
        points[i * 2][AXIS_X] = (sinf(heading) * i * rowSpacing) +
            (sinf(heading - (M_PI / 2)) * (PIN_SHADOW_ROW_COUNT - i) * PIN_SHADOW_WIDTH_STEP);
        points[i * 2][AXIS_Z] = (-cosf(heading) * i * rowSpacing) -
            (cosf(heading - (M_PI / 2)) * (PIN_SHADOW_ROW_COUNT - i) * PIN_SHADOW_WIDTH_STEP);
        points[(i * 2) + 1][AXIS_X] = (sinf(heading) * i * rowSpacing) +
            (sinf(heading + (M_PI / 2)) * (PIN_SHADOW_ROW_COUNT - i) * PIN_SHADOW_WIDTH_STEP);
        points[(i * 2) + 1][AXIS_Z] = (-cosf(heading) * i * rowSpacing) -
            (cosf(heading + (M_PI / 2)) * (PIN_SHADOW_ROW_COUNT - i) * PIN_SHADOW_WIDTH_STEP);
        points[i * 2][AXIS_Y] = GetGroundHeight(
                                    g_HoleSetup.cupPosition[AXIS_X] + points[i * 2][AXIS_X],
                                    g_HoleSetup.cupPosition[AXIS_Z] + points[i * 2][AXIS_Z]
                                ) -
            g_HoleSetup.cupPosition[AXIS_Y];
        points[(i * 2) + 1][AXIS_Y] =
            GetGroundHeight(
                g_HoleSetup.cupPosition[AXIS_X] + points[(i * 2) + 1][AXIS_X],
                g_HoleSetup.cupPosition[AXIS_Z] + points[(i * 2) + 1][AXIS_Z]
            ) -
            g_HoleSetup.cupPosition[AXIS_Y];
    }

    for (i = 0; i != PIN_SHADOW_VERTEX_COUNT; i++) {
        f_PinShadowVertices[i].v.ob[AXIS_X] = points[i][AXIS_X] / COURSE_UNITS_PER_VERTEX_UNIT;
        f_PinShadowVertices[i].v.ob[AXIS_Y] = points[i][AXIS_Y] / COURSE_UNITS_PER_VERTEX_UNIT;
        f_PinShadowVertices[i].v.ob[AXIS_Z] = points[i][AXIS_Z] / COURSE_UNITS_PER_VERTEX_UNIT;
    }
}

void DrawPinShadow(void) {
    s32 row;

    gDPSetCombineLERP(
        g_DisplayListHead++,
        0,
        0,
        0,
        SHADE,
        SHADE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        SHADE,
        SHADE,
        0,
        TEXEL0,
        0
    );
    gDPPipeSync(g_DisplayListHead++);
    gSPVertex(g_DisplayListHead++, f_PinShadowVertices, PIN_SHADOW_VERTEX_COUNT, 0);
    for (row = 0; row != PIN_SHADOW_ROW_COUNT - 1; row++) {
        gSP2Triangles(
            g_DisplayListHead++,
            row * 2,
            (row * 2) + 2,
            (row * 2) + 1,
            0,
            (row * 2) + 1,
            (row * 2) + 2,
            (row * 2) + 3,
            0
        );
    }
}

void LoadPinTextures(void) {
    EncodedAsset asset;

    f_CupTexture = AllocMemory(OpenEncodedAsset(ASSET_CUP_TEXTURE, &asset));
    ReadEncodedAsset(f_CupTexture, &asset);
    f_SmallCupTexture = AllocMemory(OpenEncodedAsset(ASSET_SMALL_CUP_TEXTURE, &asset));
    ReadEncodedAsset(f_SmallCupTexture, &asset);
    f_FlagstickTexture = AllocMemory(OpenEncodedAsset(ASSET_FLAGSTICK_TEXTURE, &asset));
    ReadEncodedAsset(f_FlagstickTexture, &asset);
    f_PinShadowTexture = AllocMemory(OpenEncodedAsset(ASSET_PIN_SHADOW_TEXTURE, &asset));
    ReadEncodedAsset(f_PinShadowTexture, &asset);
}

void FreePinTextures(void) {
    FreeMemory(&f_SmallCupTexture);
    FreeMemory(&f_CupTexture);
    FreeMemory(&f_FlagstickTexture);
    FreeMemory(&f_PinShadowTexture);
}

void SetPinMatrix(s32 cameraIndex) {
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;
    Radians heading = atan2(
        (g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE) -
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_X],
        (g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE) -
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z]
    );

    translation[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    translation[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    translation[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    rotation[AXIS_X] = 0;
    rotation[AXIS_Z] = 0;
    scale[AXIS_X] = PIN_SCALE;
    scale[AXIS_Y] = PIN_SCALE;
    scale[AXIS_Z] = PIN_SCALE;
    rotation[AXIS_Y] = heading + PIN_HEADING_OFFSET;
    BuildTransformMatrix(matrix, rotation, translation, scale);
    ConvertMatrixToFixed(matrix, &f_PinMatrices[cameraIndex][g_DrawBufferIndex]);
}

void TakePinOut(void) {
    f_IsPinOut = TRUE;
}

void SkipNextPinDraw(void) {
    f_SkipsNextPinDraw = TRUE;
}

Boolean IsPinInCup(void) {
    if (f_IsPinOut || f_SkipsNextPinDraw) {
        return FALSE;
    }

    if (g_GameState.step == GAME_STATE_TAKING_TURNS || g_GameState.step == GAME_STATE_HOLE_END) {
        if (g_GameState.isMiniGolf) {
            return TRUE;
        }

        return GetSurfaceTraits(g_CurrentShot.surface)->kind != SURFACE_KIND_GREEN;
    }

    return TRUE;
}

void DrawPin(CameraMatrices *matrices UNUSED, CameraView *view, s32 cameraIndex) {
    ScreenPoint stickTop;
    ScreenPoint cup;
    ScreenPoint stickFoot;
    Vec3 offset;
    Vec3 shadowTranslation;
    Vec3 shadowRotation;
    Vec3 shadowScale;
    Mat4 shadowMatrix;
    Vec3 flagTranslation;
    Vec3 flagRotation;
    Vec3 flagScale;
    Mat4 flagMatrix;
    f32 cupHalfHeight;
    Radians heading;
    f32 footOffsetX;
    f32 footOffsetZ;
    f32 cupScreenScale;
    f32 stickScreenScale;
    f32 wave;
    Boolean isPinOut;
    s32 cupDepth;
    s32 primDepth;
    s32 shade;
    u32 color;

    SetPinMatrix(cameraIndex);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gDPPipeSync(g_DisplayListHead++);
    offset[AXIS_X] = (g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE) - view->eye[AXIS_X];
    offset[AXIS_Y] = (g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE) - view->eye[AXIS_Y];
    offset[AXIS_Z] = (g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE) - view->eye[AXIS_Z];
    cupHalfHeight = sinf(atan2(
                        offset[AXIS_Y],
                        sqrtf((offset[AXIS_X] * offset[AXIS_X]) + (offset[AXIS_Z] * offset[AXIS_Z]))
                    )) *
        CUP_HALF_HEIGHT_MAX;
    if (cupHalfHeight < CUP_HALF_HEIGHT_MIN) {
        cupHalfHeight = CUP_HALF_HEIGHT_MIN;
    }

    offset[AXIS_X] = view->lookAt[AXIS_X] - view->eye[AXIS_X];
    offset[AXIS_Y] = view->lookAt[AXIS_Y] - view->eye[AXIS_Y];
    offset[AXIS_Z] = view->lookAt[AXIS_Z] - view->eye[AXIS_Z];
    heading = atan2(offset[AXIS_X], offset[AXIS_Z]);
    footOffsetX = 2.0f * (sinf(heading) * STICK_FOOT_HALF_DISTANCE);
    footOffsetZ = 2.0f * (cosf(heading) * STICK_FOOT_HALF_DISTANCE);
    isPinOut = !IsPinInCup();
    f_IsPinOut = FALSE;
    if (f_SkipsNextPinDraw) {
        f_SkipsNextPinDraw = FALSE;
        return;
    }

    if (!isPinOut) {
        shadowRotation[AXIS_X] = 0;
        shadowRotation[AXIS_Y] = 0;
        shadowRotation[AXIS_Z] = 0;
        shadowTranslation[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        shadowTranslation[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        shadowTranslation[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        if (g_IsCupViewShown == TRUE) {
            shadowScale[AXIS_X] = LARGE_PIN_SHADOW_SCALE;
            shadowScale[AXIS_Y] = LARGE_PIN_SHADOW_SCALE;
            shadowScale[AXIS_Z] = LARGE_PIN_SHADOW_SCALE;
        }
        else {
            shadowScale[AXIS_X] = PIN_SHADOW_SCALE;
            shadowScale[AXIS_Y] = PIN_SHADOW_SCALE;
            shadowScale[AXIS_Z] = PIN_SHADOW_SCALE;
        }

        BuildTransformMatrix(shadowMatrix, shadowRotation, shadowTranslation, shadowScale);
        ConvertMatrixToFixed(shadowMatrix, &f_PinShadowMatrix);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetAlphaCompare(g_DisplayListHead++, G_AC_NONE);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureLUT(g_DisplayListHead++, G_TT_NONE);
        gSPTexture(
            g_DisplayListHead++,
            TEXTURE_SCALE_HALF,
            TEXTURE_SCALE_HALF,
            0,
            G_TX_RENDERTILE,
            G_ON
        );
        gSPLoadGeometryMode(g_DisplayListHead++, 0);
        gSPSetGeometryMode(g_DisplayListHead++, G_SHADE | G_SHADING_SMOOTH);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
        gDPLoadTextureBlock_4b(
            g_DisplayListHead++,
            TextureAddress(ImageBody(f_PinShadowTexture)),
            G_IM_FMT_I,
            PIN_SHADOW_TEXTURE_WIDTH,
            PIN_SHADOW_TEXTURE_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_CLAMP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gSPMatrix(
            g_DisplayListHead++,
            PhysicalAddress(&f_PinShadowMatrix),
            G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
        );
        DrawPinShadow();
        gSPPopMatrix(g_DisplayListHead++, G_MTX_MODELVIEW);
        gDPPipeSync(g_DisplayListHead++);
    }

    ProjectPoint(
        g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
        (g_HoleSetup.cupPosition[AXIS_Y] - FLAGSTICK_HEIGHT) * COURSE_TO_WORLD_SCALE,
        g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE,
        &stickTop
    );
    cupDepth = ProjectPoint(
        g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
        g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE,
        g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE,
        &cup
    );
    ProjectPoint(
        (g_HoleSetup.cupPosition[AXIS_X] - footOffsetX) * COURSE_TO_WORLD_SCALE,
        g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE,
        (g_HoleSetup.cupPosition[AXIS_Z] - footOffsetZ) * COURSE_TO_WORLD_SCALE,
        &stickFoot
    );
    g_CupViewDepth = cupDepth;
    if (g_IsCupViewShown == TRUE) {
        g_PinVertexScale = LARGE_PIN_VERTEX_SCALE;
    }
    else {
        g_PinVertexScale = PIN_VERTEX_SCALE;
    }

    if (cupDepth < 0) {
        if (g_IsCupViewShown == TRUE) {
            primDepth = LARGE_CUP_PRIM_DEPTH;
        }
        else {
            primDepth = cup.depth - (cup.depthStep * (f64)(g_PinVertexScale * CUP_DEPTH_HEIGHT)) +
                CUP_DEPTH_OFFSET - PART_DEPTH_BIAS;
        }

        gDPSetPrimDepth(g_DisplayListHead++, primDepth & PRIM_DEPTH_MASK, 0);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetDepthSource(g_DisplayListHead++, G_ZS_PRIM);
        gSPLoadGeometryMode(g_DisplayListHead++, 0);
        gSPSetGeometryMode(
            g_DisplayListHead++,
            G_ZBUFFER | G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH
        );
        gDPPipeSync(g_DisplayListHead++);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
        gSPTexture(
            g_DisplayListHead++,
            TEXTURE_SCALE_HALF,
            TEXTURE_SCALE_HALF,
            0,
            G_TX_RENDERTILE,
            G_ON
        );
        gDPPipeSync(g_DisplayListHead++);
        gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
        gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEIA, G_CC_MODULATEIA);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
        gDPPipeSync(g_DisplayListHead++);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetAlphaCompare(g_DisplayListHead++, G_AC_THRESHOLD);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetAlphaCompare(g_DisplayListHead++, G_AC_NONE);
        gDPSetBlendColor(g_DisplayListHead++, 0xFF, 0xFF, 0xFF, 0x01);
        gSPMatrix(
            g_DisplayListHead++,
            PhysicalAddress(&f_PinMatrices[cameraIndex][g_DrawBufferIndex]),
            G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
        );
        if (g_IsCupViewShown == TRUE) {
            gSPVertex(g_DisplayListHead++, f_LargePinVertices, PIN_VERTEX_COUNT, 0);
        }
        else {
            gSPVertex(g_DisplayListHead++, f_PinVertices, PIN_VERTEX_COUNT, 0);
        }

        if (cupDepth < SMALL_CUP_DEPTH) {
            gDPLoadTLUT_pal16(
                g_DisplayListHead++,
                0,
                TextureAddress(ImagePalette(f_SmallCupTexture))
            );
            gDPPipeSync(g_DisplayListHead++);
            gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
            gDPLoadTextureBlock_4b(
                g_DisplayListHead++,
                TextureAddress(ImageTexels(f_SmallCupTexture)),
                G_IM_FMT_CI,
                SMALL_CUP_TEXTURE_SIZE,
                SMALL_CUP_TEXTURE_SIZE,
                0,
                G_TX_NOMIRROR | G_TX_CLAMP,
                G_TX_NOMIRROR | G_TX_CLAMP,
                6,
                6,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gSPModifyVertex(
                g_DisplayListHead++,
                1,
                G_MWO_POINT_ST,
                SmallCupTextureCoordinates(0, 4)
            );
            gSPModifyVertex(
                g_DisplayListHead++,
                2,
                G_MWO_POINT_ST,
                SmallCupTextureCoordinates(8, 4)
            );
            gSPModifyVertex(
                g_DisplayListHead++,
                3,
                G_MWO_POINT_ST,
                SmallCupTextureCoordinates(8, 0)
            );
        }
        else {
            gDPLoadTextureBlock(
                g_DisplayListHead++,
                TextureAddress(ImageBody(f_CupTexture)),
                G_IM_FMT_RGBA,
                G_IM_SIZ_16b,
                CUP_TEXTURE_WIDTH,
                CUP_TEXTURE_HEIGHT,
                0,
                G_TX_MIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_CLAMP,
                TextureMask(CUP_TEXTURE_WIDTH),
                TextureMask(CUP_TEXTURE_HEIGHT),
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }

        if (isPinOut) {
            cupScreenScale = cup.scaleX;
            if (cupScreenScale < 0.0f) {
                cupScreenScale = -cupScreenScale;
            }

            if ((f32)(cupScreenScale * (CUP_SCREEN_RADIUS * g_PinVertexScale)) <
                SCREEN_CUP_SIZE_MAX) {
                gSPModifyVertex(
                    g_DisplayListHead++,
                    0,
                    G_MWO_POINT_XYSCREEN,
                    ScreenXY(cup.x + SCREEN_CUP_RIGHT, cup.y - cupHalfHeight)
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    1,
                    G_MWO_POINT_XYSCREEN,
                    ScreenXY(cup.x + SCREEN_CUP_RIGHT, cup.y + cupHalfHeight)
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    2,
                    G_MWO_POINT_XYSCREEN,
                    ScreenXY(cup.x + SCREEN_CUP_LEFT, cup.y + cupHalfHeight)
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    3,
                    G_MWO_POINT_XYSCREEN,
                    ScreenXY(cup.x + SCREEN_CUP_LEFT, cup.y - cupHalfHeight)
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    4,
                    G_MWO_POINT_XYSCREEN,
                    ScreenXY(cup.x + SCREEN_CUP_RIGHT, cup.y)
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    5,
                    G_MWO_POINT_XYSCREEN,
                    ScreenXY(cup.x + SCREEN_CUP_RIGHT, cup.y + SCREEN_CUP_WALL_HEIGHT)
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    6,
                    G_MWO_POINT_XYSCREEN,
                    ScreenXY(cup.x + SCREEN_CUP_LEFT, cup.y + SCREEN_CUP_WALL_HEIGHT)
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    7,
                    G_MWO_POINT_XYSCREEN,
                    ScreenXY(cup.x + SCREEN_CUP_LEFT, cup.y)
                );
            }
        }

        if (g_Cameras[cameraIndex].mode != CAMERA_MODE_FREE_LOOK) {
            gSP2Triangles(g_DisplayListHead++, 2, 1, 0, 0, 3, 2, 0, 0);
        }

        gDPPipeSync(g_DisplayListHead++);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureLUT(g_DisplayListHead++, G_TT_NONE);
        if (g_IsCupViewShown == TRUE) {
            primDepth = LARGE_CUP_WALL_PRIM_DEPTH;
        }
        else {
            primDepth = cup.depth - (cup.depthStep * (CUP_WALL_DEPTH_HEIGHT * g_PinVertexScale)) +
                CUP_WALL_DEPTH_OFFSET - PART_DEPTH_BIAS;
            primDepth &= PRIM_DEPTH_MASK;
        }

        if (g_IsCupViewShown == TRUE) {
            gDPSetPrimDepth(g_DisplayListHead++, primDepth, 0);
            gDPSetCombineLERP(
                g_DisplayListHead++,
                0,
                0,
                0,
                TEXEL0,
                0,
                1,
                TEXEL0,
                1,
                0,
                0,
                0,
                TEXEL0,
                0,
                1,
                TEXEL0,
                1
            );
            gDPPipeSync(g_DisplayListHead++);
            gDPSetRenderMode(
                g_DisplayListHead++,
                GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
                G_RM_AA_ZB_TEX_EDGE2
            );
            gDPPipeSync(g_DisplayListHead++);
            gDPSetTextureFilter(g_DisplayListHead++, G_TF_POINT);
            gDPPipeSync(g_DisplayListHead++);
            if (g_Cameras[cameraIndex].mode != CAMERA_MODE_FREE_LOOK) {
                gSP2Triangles(g_DisplayListHead++, 6, 5, 4, 0, 7, 6, 4, 0);
            }
        }

        if (!isPinOut) {
            shade = CUP_BASE_SHADE - cupDepth;
            if (shade < CUP_SHADE_MIN) {
                shade = CUP_SHADE_MIN;
            }

            if (shade > CUP_SHADE_MAX) {
                shade = CUP_SHADE_MAX;
            }

            color = RgbaColor(shade, shade, shade, CUP_SHADE_ALPHA);
            gSPModifyVertex(g_DisplayListHead++, 2, G_MWO_POINT_RGBA, color);
            gSPModifyVertex(g_DisplayListHead++, 3, G_MWO_POINT_RGBA, color);
            stickScreenScale = cup.scaleX;
            if (stickScreenScale < 0.0f) {
                stickScreenScale = -stickScreenScale;
            }

            if ((f32)(stickScreenScale * FLAGSTICK_SCREEN_RADIUS) < SCREEN_FLAGSTICK_SIZE_MAX) {
                gSPModifyVertex(
                    g_DisplayListHead++,
                    11,
                    G_MWO_POINT_XYSCREEN,
                    ScreenPointWord(
                        (stickTop.x * QUARTER_PIXELS_PER_PIXEL) - SCREEN_FLAGSTICK_HALF_WIDTH,
                        stickTop.y * QUARTER_PIXELS_PER_PIXEL
                    )
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    10,
                    G_MWO_POINT_XYSCREEN,
                    ScreenPointWord(
                        (stickFoot.x * QUARTER_PIXELS_PER_PIXEL) - SCREEN_FLAGSTICK_HALF_WIDTH,
                        (stickFoot.y - SCREEN_FLAGSTICK_FOOT_RISE) * QUARTER_PIXELS_PER_PIXEL
                    )
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    9,
                    G_MWO_POINT_XYSCREEN,
                    ScreenPointWord(
                        (stickFoot.x * QUARTER_PIXELS_PER_PIXEL) + SCREEN_FLAGSTICK_HALF_WIDTH,
                        (stickFoot.y - SCREEN_FLAGSTICK_FOOT_RISE) * QUARTER_PIXELS_PER_PIXEL
                    )
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    8,
                    G_MWO_POINT_XYSCREEN,
                    ScreenPointWord(
                        (stickTop.x * QUARTER_PIXELS_PER_PIXEL) + SCREEN_FLAGSTICK_HALF_WIDTH,
                        stickTop.y * QUARTER_PIXELS_PER_PIXEL
                    )
                );
            }

            if (g_IsCupViewShown == TRUE) {
                primDepth = LARGE_FLAGSTICK_PRIM_DEPTH;
            }
            else if (cupDepth < SMALL_CUP_DEPTH) {
                primDepth = cup.depth;
            }
            else {
                primDepth = cup.depth -
                    (cup.depthStep * (g_PinVertexScale * FLAGSTICK_DEPTH_HEIGHT)) +
                    FLAGSTICK_DEPTH_OFFSET - PART_DEPTH_BIAS;
            }

            if (cupDepth < FAR_FLAGSTICK_DEPTH) {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetRenderMode(g_DisplayListHead++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
            }
            else {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
            }

            gDPPipeSync(g_DisplayListHead++);
            gDPSetPrimDepth(g_DisplayListHead++, primDepth & PRIM_DEPTH_MASK, 0);
            if (cupDepth < FAR_FLAGSTICK_DEPTH) {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetTextureFilter(g_DisplayListHead++, G_TF_POINT);
            }
            else {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
            }

            gDPPipeSync(g_DisplayListHead++);
            gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEI, G_CC_MODULATEI);
            gDPPipeSync(g_DisplayListHead++);
            gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
            gDPLoadTLUT_pal16(
                g_DisplayListHead++,
                0,
                TextureAddress(ImagePalette(f_FlagstickTexture))
            );
            gDPLoadTextureBlock_4b(
                g_DisplayListHead++,
                TextureAddress(ImageTexels(f_FlagstickTexture)),
                G_IM_FMT_CI,
                FLAGSTICK_TEXTURE_SIZE,
                FLAGSTICK_TEXTURE_SIZE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                TextureMask(FLAGSTICK_TEXTURE_SIZE),
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(g_DisplayListHead++);
            gSP2Triangles(g_DisplayListHead++, 10, 9, 8, 0, 11, 10, 8, 0);
        }

        gDPPipeSync(g_DisplayListHead++);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetDepthSource(g_DisplayListHead++, G_ZS_PIXEL);
        gSPPopMatrix(g_DisplayListHead++, G_MTX_MODELVIEW);
        gDPPipeSync(g_DisplayListHead++);
    }

    if (!isPinOut) {
        if (g_HoleSetup.conditions.windSpeed != 0) {
            f_FlagWavePhase += DegreesToRadians(FLAG_WAVE_STEP_DEGREES);
            if (f_FlagWavePhase > FULL_TURN_RADIANS) {
                f_FlagWavePhase -= FULL_TURN_RADIANS;
            }

            if (f_FlagWavePhase < 0.0f) {
                f_FlagWavePhase += FULL_TURN_RADIANS;
            }

            wave = (((g_HoleSetup.conditions.windSpeed / WIND_SPEED_UNIT) * FLAG_WAVE_PER_WIND) +
                       FLAG_WAVE_MIN) *
                sinf(f_FlagWavePhase);
        }
        else {
            wave = 0.0f;
        }

        flagRotation[AXIS_X] = M_PI;
        flagRotation[AXIS_Z] = 0;
        flagScale[AXIS_X] = FLAG_SCALE;
        flagScale[AXIS_Y] = FLAG_SCALE;
        flagScale[AXIS_Z] = FLAG_SCALE;
        flagTranslation[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        flagTranslation[AXIS_Y] =
            (g_HoleSetup.cupPosition[AXIS_Y] - FLAGSTICK_HEIGHT) * COURSE_TO_WORLD_SCALE;
        flagTranslation[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        flagRotation[AXIS_Y] =
            wave - (g_HoleSetup.conditions.windDirection * BINARY_ANGLE_TO_RADIANS);
        BuildTransformMatrix(flagMatrix, flagRotation, flagTranslation, flagScale);
        ConvertMatrixToFixed(flagMatrix, &f_FlagMatrices[g_DrawBufferIndex]);
        gDPPipeSync(g_DisplayListHead++);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
        gSPMatrix(
            g_DisplayListHead++,
            PhysicalAddress(&f_FlagMatrices[g_DrawBufferIndex]),
            G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
        );
        DrawHoleFlag(TRUE);
        gDPPipeSync(g_DisplayListHead++);
        gSPPopMatrix(g_DisplayListHead++, G_MTX_MODELVIEW);
    }

    gDPPipeSync(g_DisplayListHead++);
    gDPSetAlphaCompare(g_DisplayListHead++, G_AC_NONE);
}
