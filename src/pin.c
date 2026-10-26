#include "common.h"
#include "asset.h"
#include "billboard.h"
#include "camera.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "functions.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "pin.h"
#include "round_hud.h"
#include "save.h"
#include "surface.h"
#include "variables.h"

/* In 1/1024ths of a unit */
#define CUP_DISTANCE_OFFSET 0x699

/*
 * The shadow is a strip of rows of two vertices, running from the cup along
 * the light's yaw and narrowing row by row
 */
#define PIN_SHADOW_ROW_COUNT 9
#define PIN_SHADOW_VERTEX_COUNT (PIN_SHADOW_ROW_COUNT * 2)
#define PIN_SHADOW_ROW_SPACING 8533.0f /* In 1/1024ths of a unit */
#define PIN_SHADOW_WIDTH_STEP 46.079998f
/* The shadow's vertices are in 1/256ths of a unit */
#define COURSE_UNITS_PER_VERTEX_UNIT 4

#define ASSET_SMALL_CUP_TEXTURE 0x4C9
#define ASSET_CUP_TEXTURE 0x4CA
#define ASSET_FLAGSTICK_TEXTURE 0x703
#define ASSET_PIN_SHADOW_TEXTURE 0x704

#define PIN_SCALE 0.1f
/* Turns the pin from facing away from the camera to facing it */
#define PIN_HEADING_OFFSET 3.14f

#define PIN_SHADOW_SCALE (4.0f / 1024.0f)
#define LARGE_PIN_SHADOW_SCALE (6.0f / 1024.0f)
#define PIN_SHADOW_TEXTURE_WIDTH 16
#define PIN_SHADOW_TEXTURE_HEIGHT 1
#define CUP_TEXTURE_WIDTH 32
#define CUP_TEXTURE_HEIGHT 64
#define SMALL_CUP_TEXTURE_SIZE 8
#define FLAGSTICK_TEXTURE_SIZE 16

#define CUP_MAX_HALF_HEIGHT 5.0f
#define CUP_MIN_HALF_HEIGHT 1.0f
/* Half the distance in front of the cup, toward the camera, of the point projected as the flagstick's foot */
#define STICK_FOOT_HALF_DISTANCE 614.4f
#define FLAGSTICK_HEIGHT 0xB400 /* In 1/1024ths of a unit */

/* Each part's prim depth is the cup's, moved by a height in vertex units and an offset */
#define PART_DEPTH_BIAS 120
#define CUP_DEPTH_HEIGHT 1.5f
#define CUP_DEPTH_OFFSET 60.0
#define CUP_WALL_DEPTH_HEIGHT 10.5f
#define CUP_WALL_DEPTH_OFFSET 20.0f
#define FLAGSTICK_DEPTH_HEIGHT 7
#define FLAGSTICK_DEPTH_OFFSET 40.0f

/*
 * When the cup or flagstick would be drawn smaller than these screen sizes,
 * its vertices are placed in screen space instead
 */
#define CUP_SCREEN_RADIUS 1.7999999999999998
#define SCREEN_CUP_MAX_SIZE 10.0f
#define FLAGSTICK_SCREEN_RADIUS 0.6000000000000001
#define SCREEN_FLAGSTICK_MAX_SIZE 2.0f
/* The screen-space cup's corners, in pixels from the cup's center */
#define SCREEN_CUP_LEFT -4
#define SCREEN_CUP_RIGHT 5
#define SCREEN_CUP_WALL_HEIGHT 6

/* Two of the cup's corners get lighter as the cup gets farther from the camera */
#define CUP_BASE_SHADE 0x5C
#define MIN_CUP_SHADE 0x7F
#define MAX_CUP_SHADE 0xFF
#define CUP_SHADE_ALPHA 0xBF

#define FLAG_SCALE 0.01f
#define FLAG_WAVE_STEP_DEGREES 5
#define FLAG_WIND_SPEED_UNIT 0x10000
#define FLAG_WAVE_PER_WIND 0.02f
#define MIN_FLAG_WAVE 0.05f

#define PIN_VERTEX_COUNT 12
#define PIN_VERTEX_SCALE 2
#define LARGE_PIN_VERTEX_SCALE 4
#define SMALL_CUP_DEPTH -200
#define FAR_FLAGSTICK_DEPTH -100
#define LARGE_CUP_PRIM_DEPTH 0x403B
#define LARGE_CUP_WALL_PRIM_DEPTH 0x4010
#define LARGE_FLAGSTICK_PRIM_DEPTH 0x4027

/* Texture coordinates in texels, packed as s10.5 */
#define SMALL_CUP_TEXCOORDS(s, t) ((((s) << 5) << 16) | ((t) << 5))
/* Screen coordinates are in quarter pixels */
#define SCREEN_SUBPIXEL_COUNT 4
#define SCREEN_XY(x, y) ((((x) * SCREEN_SUBPIXEL_COUNT) << 16) + (s32)((y) * SCREEN_SUBPIXEL_COUNT))
/* Half the screen-space flagstick's width, in quarter pixels */
#define SCREEN_FLAGSTICK_HALF_WIDTH 2
/* How far above its projected foot the screen-space flagstick starts, in pixels */
#define SCREEN_FLAGSTICK_FOOT_RISE 1
/* Prim depths are kept to multiples of 4 */
#define PRIM_DEPTH_MASK (~3)

/* GameState.unk0 values */
#define GAME_STATE_UNK6 6
#define GAME_STATE_UNK7 7

extern Vtx sPinShadowVertices[PIN_SHADOW_VERTEX_COUNT];
/* Set by TakePinOut and cleared when the pin is next drawn */
extern s8 sIsPinOut;
/* Set by SkipNextPinDraw and cleared by the draw it skips */
extern s8 sSkipsNextPinDraw;

extern u8 *sSmallCupTexture;
extern u8 *sCupTexture;
extern u8 *sFlagstickTexture;
extern u8 *sPinShadowTexture;
extern Mtx sPinMatrices[][DRAW_BUFFER_COUNT];
extern Mtx sPinShadowMatrix;
extern Mtx sFlagMatrices[DRAW_BUFFER_COUNT];
extern Vtx sPinVertices[PIN_VERTEX_COUNT];
extern Vtx sLargePinVertices[PIN_VERTEX_COUNT];
extern f32 sFlagWavePhase;
extern s32 g_CupViewDepth;
extern s32 D_800BE69C;


/*
 * Returns the length of (x, z), less CUP_DISTANCE_OFFSET but at least 0.
 * Callers pass a point's offset from the cup.
 */
s32 GetCupDistance(s32 x, s32 z) {
    s32 distance = IntLength2D(x, z);

    if ((u32)distance < CUP_DISTANCE_OFFSET) {
        distance = 0;
    } else {
        distance -= CUP_DISTANCE_OFFSET;
    }
    return distance;
}

/* Lays the pin's shadow vertices over the ground around the cup */
#ifdef NON_MATCHING
void BuildPinShadow(void) {
    s32 points[PIN_SHADOW_VERTEX_COUNT][AXIS_COUNT];
    s32 i;
    f32 heading = D_800BA9D8.conditions.lightYaw * BINARY_ANGLE_TO_RADIANS + M_PI / 2;

    for (i = 0; i != PIN_SHADOW_ROW_COUNT; i++) {
        points[i * 2][AXIS_X] = sinf(heading) * i * PIN_SHADOW_ROW_SPACING +
                                sinf(heading - M_PI / 2) * (PIN_SHADOW_ROW_COUNT - i) * PIN_SHADOW_WIDTH_STEP;
        points[i * 2][AXIS_Z] = -cosf(heading) * i * PIN_SHADOW_ROW_SPACING -
                                cosf(heading - M_PI / 2) * (PIN_SHADOW_ROW_COUNT - i) * PIN_SHADOW_WIDTH_STEP;
        points[i * 2 + 1][AXIS_X] = sinf(heading) * i * PIN_SHADOW_ROW_SPACING +
                                    sinf(heading + M_PI / 2) * (PIN_SHADOW_ROW_COUNT - i) * PIN_SHADOW_WIDTH_STEP;
        points[i * 2 + 1][AXIS_Z] = -cosf(heading) * i * PIN_SHADOW_ROW_SPACING -
                                    cosf(heading + M_PI / 2) * (PIN_SHADOW_ROW_COUNT - i) * PIN_SHADOW_WIDTH_STEP;
        points[i * 2][AXIS_Y] = GetGroundHeight(D_800BA9D8.cupPosition[AXIS_X] + points[i * 2][AXIS_X],
                                                D_800BA9D8.cupPosition[AXIS_Z] + points[i * 2][AXIS_Z]) -
                                D_800BA9D8.cupPosition[AXIS_Y];
        points[i * 2 + 1][AXIS_Y] = GetGroundHeight(D_800BA9D8.cupPosition[AXIS_X] + points[i * 2 + 1][AXIS_X],
                                                    D_800BA9D8.cupPosition[AXIS_Z] + points[i * 2 + 1][AXIS_Z]) -
                                    D_800BA9D8.cupPosition[AXIS_Y];
    }
    for (i = 0; i != PIN_SHADOW_VERTEX_COUNT; i++) {
        sPinShadowVertices[i].v.ob[AXIS_X] = points[i][AXIS_X] / COURSE_UNITS_PER_VERTEX_UNIT;
        sPinShadowVertices[i].v.ob[AXIS_Y] = points[i][AXIS_Y] / COURSE_UNITS_PER_VERTEX_UNIT;
        sPinShadowVertices[i].v.ob[AXIS_Z] = points[i][AXIS_Z] / COURSE_UNITS_PER_VERTEX_UNIT;
    }
}

#else
INCLUDE_ASM("asm/nonmatchings/pin", BuildPinShadow);
#endif

/* Draws the pin's shadow, with its texture already loaded */
void DrawPinShadow(void) {
    s32 i;

    gDPSetCombineLERP(g_DisplayListHead++, 0, 0, 0, SHADE, SHADE, 0, TEXEL0, 0, 0, 0, 0, SHADE, SHADE, 0, TEXEL0, 0);
    gDPPipeSync(g_DisplayListHead++);
    gSPVertex(g_DisplayListHead++, sPinShadowVertices, PIN_SHADOW_VERTEX_COUNT, 0);
    for (i = 0; i != PIN_SHADOW_ROW_COUNT - 1; i++) {
        gSP2Triangles(g_DisplayListHead++, i * 2, i * 2 + 2, i * 2 + 1, 0, i * 2 + 1, i * 2 + 2, i * 2 + 3, 0);
    }
}

void LoadPinTextures(void) {
    EncodedAsset asset;

    sCupTexture = AllocMemory(OpenEncodedAsset(ASSET_CUP_TEXTURE, &asset));
    ReadEncodedAsset(sCupTexture, &asset);
    sSmallCupTexture = AllocMemory(OpenEncodedAsset(ASSET_SMALL_CUP_TEXTURE, &asset));
    ReadEncodedAsset(sSmallCupTexture, &asset);
    sFlagstickTexture = AllocMemory(OpenEncodedAsset(ASSET_FLAGSTICK_TEXTURE, &asset));
    ReadEncodedAsset(sFlagstickTexture, &asset);
    sPinShadowTexture = AllocMemory(OpenEncodedAsset(ASSET_PIN_SHADOW_TEXTURE, &asset));
    ReadEncodedAsset(sPinShadowTexture, &asset);
}

void FreePinTextures(void) {
    FreeMemory(&sSmallCupTexture);
    FreeMemory(&sCupTexture);
    FreeMemory(&sFlagstickTexture);
    FreeMemory(&sPinShadowTexture);
}

/* Places the pin at the cup, turned to face the main camera */
void SetPinMatrix(s32 cameraIndex) {
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;
    f32 heading = atan2(D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE - g_Cameras[0].view.eye[AXIS_X],
                        D_800BA9D8.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE - g_Cameras[0].view.eye[AXIS_Z]);

    translation[AXIS_X] = D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    translation[AXIS_Y] = D_800BA9D8.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    translation[AXIS_Z] = D_800BA9D8.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    rotation[AXIS_X] = 0;
    rotation[AXIS_Z] = 0;
    scale[AXIS_X] = PIN_SCALE;
    scale[AXIS_Y] = PIN_SCALE;
    scale[AXIS_Z] = PIN_SCALE;
    rotation[AXIS_Y] = heading + PIN_HEADING_OFFSET;
    BuildTransformMatrix(matrix, rotation, translation, scale);
    ConvertMatrixToFixed(matrix, &sPinMatrices[cameraIndex][g_DrawBufferIndex]);
}

/* Takes the pin out of the cup until it is next drawn */
void TakePinOut(void) {
    sIsPinOut = TRUE;
}

/* Makes the next DrawPin draw nothing */
void SkipNextPinDraw(void) {
    sSkipsNextPinDraw = TRUE;
}

/*
 * Returns whether the pin stands in the cup. During GameState.unk0 6 and 7 it
 * is taken out while the shot is from the green, except in mini-golf.
 */
s32 IsPinInCup(void) {
    if (sIsPinOut || sSkipsNextPinDraw) {
        return FALSE;
    }
    if (g_GameState.unk0 == GAME_STATE_UNK6 || g_GameState.unk0 == GAME_STATE_UNK7) {
        if (g_IsMiniGolf) {
            return TRUE;
        }
        return GetSurfaceInfo(g_CurrentShot.surface)->kind != SURFACE_KIND_GREEN;
    }
    return TRUE;
}

/*
 * Draws the pin's shadow, the cup, the flagstick and the flag, which turns
 * to the wind. While the pin is out of the cup, only the cup is drawn.
 * matrices is unused; DrawGame passes it to its other scene drawers too.
 */
void DrawPin(CameraMatrices *matrices, CameraView *view, s32 cameraIndex) {
    ScreenPoint top;
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
    f32 heading;
    f32 footOffsetX;
    f32 footOffsetZ;
    f32 cupScreenScale;
    f32 stickScreenScale;
    f32 wave;
    s32 isPinOut;
    s32 cupDepth;
    s32 primDepth;
    s32 shade;
    u32 color;

    SetPinMatrix(cameraIndex);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gDPPipeSync(g_DisplayListHead++);
    offset[AXIS_X] = D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE - view->eye[AXIS_X];
    offset[AXIS_Y] = D_800BA9D8.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE - view->eye[AXIS_Y];
    offset[AXIS_Z] = D_800BA9D8.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE - view->eye[AXIS_Z];
    cupHalfHeight =
        sinf(atan2(offset[AXIS_Y], sqrtf(offset[AXIS_X] * offset[AXIS_X] + offset[AXIS_Z] * offset[AXIS_Z]))) *
        CUP_MAX_HALF_HEIGHT;
    if (cupHalfHeight < CUP_MIN_HALF_HEIGHT) {
        cupHalfHeight = CUP_MIN_HALF_HEIGHT;
    }
    offset[AXIS_X] = view->lookAt[AXIS_X] - view->eye[AXIS_X];
    offset[AXIS_Y] = view->lookAt[AXIS_Y] - view->eye[AXIS_Y];
    offset[AXIS_Z] = view->lookAt[AXIS_Z] - view->eye[AXIS_Z];
    heading = atan2(offset[AXIS_X], offset[AXIS_Z]);
    footOffsetX = 2.0f * (sinf(heading) * STICK_FOOT_HALF_DISTANCE);
    footOffsetZ = 2.0f * (cosf(heading) * STICK_FOOT_HALF_DISTANCE);
    isPinOut = !IsPinInCup();
    sIsPinOut = FALSE;
    if (sSkipsNextPinDraw) {
        sSkipsNextPinDraw = FALSE;
        return;
    }
    if (!isPinOut) {
        shadowRotation[AXIS_X] = 0;
        shadowRotation[AXIS_Y] = 0;
        shadowRotation[AXIS_Z] = 0;
        shadowTranslation[AXIS_X] = D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        shadowTranslation[AXIS_Y] = D_800BA9D8.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        shadowTranslation[AXIS_Z] = D_800BA9D8.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        if (D_800BE69C == 1) {
            shadowScale[AXIS_X] = LARGE_PIN_SHADOW_SCALE;
            shadowScale[AXIS_Y] = LARGE_PIN_SHADOW_SCALE;
            shadowScale[AXIS_Z] = LARGE_PIN_SHADOW_SCALE;
        } else {
            shadowScale[AXIS_X] = PIN_SHADOW_SCALE;
            shadowScale[AXIS_Y] = PIN_SHADOW_SCALE;
            shadowScale[AXIS_Z] = PIN_SHADOW_SCALE;
        }
        BuildTransformMatrix(shadowMatrix, shadowRotation, shadowTranslation, shadowScale);
        ConvertMatrixToFixed(shadowMatrix, &sPinShadowMatrix);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetAlphaCompare(g_DisplayListHead++, G_AC_NONE);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureLUT(g_DisplayListHead++, G_TT_NONE);
        gSPTexture(g_DisplayListHead++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
        gSPLoadGeometryMode(g_DisplayListHead++, 0);
        gSPSetGeometryMode(g_DisplayListHead++, G_SHADE | G_SHADING_SMOOTH);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
        gDPLoadTextureBlock_4b(g_DisplayListHead++, ((u32)sPinShadowTexture + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_I,
                               PIN_SHADOW_TEXTURE_WIDTH, PIN_SHADOW_TEXTURE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                               G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPMatrix(g_DisplayListHead++, OS_K0_TO_PHYSICAL(&sPinShadowMatrix),
                  G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        DrawPinShadow();
        gSPPopMatrix(g_DisplayListHead++, G_MTX_MODELVIEW);
        gDPPipeSync(g_DisplayListHead++);
    }
    ProjectPoint(D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                 (D_800BA9D8.cupPosition[AXIS_Y] - FLAGSTICK_HEIGHT) * COURSE_TO_WORLD_SCALE,
                 D_800BA9D8.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE, &top);
    cupDepth = ProjectPoint(D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                            D_800BA9D8.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE,
                            D_800BA9D8.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE, &cup);
    ProjectPoint((D_800BA9D8.cupPosition[AXIS_X] - footOffsetX) * COURSE_TO_WORLD_SCALE,
                 D_800BA9D8.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE,
                 (D_800BA9D8.cupPosition[AXIS_Z] - footOffsetZ) * COURSE_TO_WORLD_SCALE, &stickFoot);
    g_CupViewDepth = cupDepth;
    if (D_800BE69C == 1) {
        g_PinVertexScale = LARGE_PIN_VERTEX_SCALE;
    } else {
        g_PinVertexScale = PIN_VERTEX_SCALE;
    }
    if (cupDepth < 0) {
        if (D_800BE69C == 1) {
            primDepth = LARGE_CUP_PRIM_DEPTH;
        } else {
            primDepth = cup.depth - cup.depthStep * (f64)(g_PinVertexScale * CUP_DEPTH_HEIGHT) + CUP_DEPTH_OFFSET -
                        PART_DEPTH_BIAS;
        }
        gDPSetPrimDepth(g_DisplayListHead++, primDepth & PRIM_DEPTH_MASK, 0);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetDepthSource(g_DisplayListHead++, G_ZS_PRIM);
        gSPLoadGeometryMode(g_DisplayListHead++, 0);
        gSPSetGeometryMode(g_DisplayListHead++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
        gDPPipeSync(g_DisplayListHead++);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
        gSPTexture(g_DisplayListHead++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
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
        gSPMatrix(g_DisplayListHead++, OS_K0_TO_PHYSICAL(&sPinMatrices[cameraIndex][g_DrawBufferIndex]),
                  G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        if (D_800BE69C == 1) {
            gSPVertex(g_DisplayListHead++, sLargePinVertices, PIN_VERTEX_COUNT, 0);
        } else {
            gSPVertex(g_DisplayListHead++, sPinVertices, PIN_VERTEX_COUNT, 0);
        }
        if (cupDepth < SMALL_CUP_DEPTH) {
            gDPLoadTLUT_pal16(g_DisplayListHead++, 0, ((u32)sSmallCupTexture + IMAGE_HEADER_SIZE) & ~7);
            gDPPipeSync(g_DisplayListHead++);
            gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
            gDPLoadTextureBlock_4b(g_DisplayListHead++, ((u32)sSmallCupTexture + PALETTED_TEXELS_OFFSET) & ~7,
                                   G_IM_FMT_CI, SMALL_CUP_TEXTURE_SIZE, SMALL_CUP_TEXTURE_SIZE, 0,
                                   G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD,
                                   G_TX_NOLOD);
            gSPModifyVertex(g_DisplayListHead++, 1, G_MWO_POINT_ST, SMALL_CUP_TEXCOORDS(0, 4));
            gSPModifyVertex(g_DisplayListHead++, 2, G_MWO_POINT_ST, SMALL_CUP_TEXCOORDS(8, 4));
            gSPModifyVertex(g_DisplayListHead++, 3, G_MWO_POINT_ST, SMALL_CUP_TEXCOORDS(8, 0));
        } else {
            gDPLoadTextureBlock(g_DisplayListHead++, ((u32)sCupTexture + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_RGBA,
                                G_IM_SIZ_16b, CUP_TEXTURE_WIDTH, CUP_TEXTURE_HEIGHT, 0, G_TX_MIRROR | G_TX_WRAP,
                                G_TX_NOMIRROR | G_TX_CLAMP, 5, 6, G_TX_NOLOD, G_TX_NOLOD);
        }
        if (isPinOut) {
            cupScreenScale = cup.scaleX;
            if (cupScreenScale < 0.0f) {
                cupScreenScale = -cupScreenScale;
            }
            if ((f32)(cupScreenScale * (CUP_SCREEN_RADIUS * g_PinVertexScale)) < SCREEN_CUP_MAX_SIZE) {
                gSPModifyVertex(g_DisplayListHead++, 0, G_MWO_POINT_XYSCREEN,
                                SCREEN_XY(cup.x + SCREEN_CUP_RIGHT, cup.y - cupHalfHeight));
                gSPModifyVertex(g_DisplayListHead++, 1, G_MWO_POINT_XYSCREEN,
                                SCREEN_XY(cup.x + SCREEN_CUP_RIGHT, cup.y + cupHalfHeight));
                gSPModifyVertex(g_DisplayListHead++, 2, G_MWO_POINT_XYSCREEN,
                                SCREEN_XY(cup.x + SCREEN_CUP_LEFT, cup.y + cupHalfHeight));
                gSPModifyVertex(g_DisplayListHead++, 3, G_MWO_POINT_XYSCREEN,
                                SCREEN_XY(cup.x + SCREEN_CUP_LEFT, cup.y - cupHalfHeight));
                gSPModifyVertex(g_DisplayListHead++, 4, G_MWO_POINT_XYSCREEN,
                                SCREEN_XY(cup.x + SCREEN_CUP_RIGHT, cup.y));
                gSPModifyVertex(g_DisplayListHead++, 5, G_MWO_POINT_XYSCREEN,
                                SCREEN_XY(cup.x + SCREEN_CUP_RIGHT, cup.y + SCREEN_CUP_WALL_HEIGHT));
                gSPModifyVertex(g_DisplayListHead++, 6, G_MWO_POINT_XYSCREEN,
                                SCREEN_XY(cup.x + SCREEN_CUP_LEFT, cup.y + SCREEN_CUP_WALL_HEIGHT));
                gSPModifyVertex(g_DisplayListHead++, 7, G_MWO_POINT_XYSCREEN,
                                SCREEN_XY(cup.x + SCREEN_CUP_LEFT, cup.y));
            }
        }
        if (g_Cameras[cameraIndex].view.mode != CAMERA_MODE_UNK2) {
            gSP2Triangles(g_DisplayListHead++, 2, 1, 0, 0, 3, 2, 0, 0);
        }
        gDPPipeSync(g_DisplayListHead++);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureLUT(g_DisplayListHead++, G_TT_NONE);
        if (D_800BE69C == 1) {
            primDepth = LARGE_CUP_WALL_PRIM_DEPTH;
        } else {
            primDepth = cup.depth - cup.depthStep * (CUP_WALL_DEPTH_HEIGHT * g_PinVertexScale) +
                        CUP_WALL_DEPTH_OFFSET - PART_DEPTH_BIAS;
            primDepth &= PRIM_DEPTH_MASK;
        }
        if (D_800BE69C == 1) {
            gDPSetPrimDepth(g_DisplayListHead++, primDepth, 0);
            gDPSetCombineLERP(g_DisplayListHead++, 0, 0, 0, TEXEL0, 0, 1, TEXEL0, 1, 0, 0, 0, TEXEL0, 0, 1, TEXEL0,
                              1);
            gDPPipeSync(g_DisplayListHead++);
            gDPSetRenderMode(g_DisplayListHead++, GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
                             G_RM_AA_ZB_TEX_EDGE2);
            gDPPipeSync(g_DisplayListHead++);
            gDPSetTextureFilter(g_DisplayListHead++, G_TF_POINT);
            gDPPipeSync(g_DisplayListHead++);
            if (g_Cameras[cameraIndex].view.mode != CAMERA_MODE_UNK2) {
                gSP2Triangles(g_DisplayListHead++, 6, 5, 4, 0, 7, 6, 4, 0);
            }
        }
        if (!isPinOut) {
            shade = CUP_BASE_SHADE - cupDepth;
            if (shade < MIN_CUP_SHADE) {
                shade = MIN_CUP_SHADE;
            }
            if (shade > MAX_CUP_SHADE) {
                shade = MAX_CUP_SHADE;
            }
            color = (shade << 24) | (shade << 16) | (shade << 8) | CUP_SHADE_ALPHA;
            gSPModifyVertex(g_DisplayListHead++, 2, G_MWO_POINT_RGBA, color);
            gSPModifyVertex(g_DisplayListHead++, 3, G_MWO_POINT_RGBA, color);
            stickScreenScale = cup.scaleX;
            if (stickScreenScale < 0.0f) {
                stickScreenScale = -stickScreenScale;
            }
            if ((f32)(stickScreenScale * FLAGSTICK_SCREEN_RADIUS) < SCREEN_FLAGSTICK_MAX_SIZE) {
                gSPModifyVertex(g_DisplayListHead++, 11, G_MWO_POINT_XYSCREEN,
                                (((top.x * SCREEN_SUBPIXEL_COUNT) - SCREEN_FLAGSTICK_HALF_WIDTH) << 16) +
                                    top.y * SCREEN_SUBPIXEL_COUNT);
                gSPModifyVertex(g_DisplayListHead++, 10, G_MWO_POINT_XYSCREEN,
                                (((stickFoot.x * SCREEN_SUBPIXEL_COUNT) - SCREEN_FLAGSTICK_HALF_WIDTH) << 16) +
                                    (stickFoot.y - SCREEN_FLAGSTICK_FOOT_RISE) * SCREEN_SUBPIXEL_COUNT);
                gSPModifyVertex(g_DisplayListHead++, 9, G_MWO_POINT_XYSCREEN,
                                (((stickFoot.x * SCREEN_SUBPIXEL_COUNT) + SCREEN_FLAGSTICK_HALF_WIDTH) << 16) +
                                    (stickFoot.y - SCREEN_FLAGSTICK_FOOT_RISE) * SCREEN_SUBPIXEL_COUNT);
                gSPModifyVertex(g_DisplayListHead++, 8, G_MWO_POINT_XYSCREEN,
                                (((top.x * SCREEN_SUBPIXEL_COUNT) + SCREEN_FLAGSTICK_HALF_WIDTH) << 16) +
                                    top.y * SCREEN_SUBPIXEL_COUNT);
            }
            if (D_800BE69C == 1) {
                primDepth = LARGE_FLAGSTICK_PRIM_DEPTH;
            } else if (cupDepth < SMALL_CUP_DEPTH) {
                primDepth = cup.depth;
            } else {
                primDepth = cup.depth - cup.depthStep * (g_PinVertexScale * FLAGSTICK_DEPTH_HEIGHT) +
                            FLAGSTICK_DEPTH_OFFSET - PART_DEPTH_BIAS;
            }
            if (cupDepth < FAR_FLAGSTICK_DEPTH) {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetRenderMode(g_DisplayListHead++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
            } else {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
            }
            gDPPipeSync(g_DisplayListHead++);
            gDPSetPrimDepth(g_DisplayListHead++, primDepth & PRIM_DEPTH_MASK, 0);
            if (cupDepth < FAR_FLAGSTICK_DEPTH) {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetTextureFilter(g_DisplayListHead++, G_TF_POINT);
            } else {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
            }
            gDPPipeSync(g_DisplayListHead++);
            gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEI, G_CC_MODULATEI);
            gDPPipeSync(g_DisplayListHead++);
            gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
            gDPLoadTLUT_pal16(g_DisplayListHead++, 0, ((u32)sFlagstickTexture + IMAGE_HEADER_SIZE) & ~7);
            gDPLoadTextureBlock_4b(g_DisplayListHead++, ((u32)sFlagstickTexture + PALETTED_TEXELS_OFFSET) & ~7,
                                   G_IM_FMT_CI, FLAGSTICK_TEXTURE_SIZE, FLAGSTICK_TEXTURE_SIZE, 0,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, 4, G_TX_NOLOD,
                                   G_TX_NOLOD);
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
        if (g_HoleConditions.windSpeed != 0) {
            sFlagWavePhase += DEGREES_TO_RADIANS(FLAG_WAVE_STEP_DEGREES);
            if (sFlagWavePhase > 2 * M_PI) {
                sFlagWavePhase -= 2 * M_PI;
            }
            if (sFlagWavePhase < 0.0f) {
                sFlagWavePhase += 2 * M_PI;
            }
            wave = ((g_HoleConditions.windSpeed / FLAG_WIND_SPEED_UNIT) * FLAG_WAVE_PER_WIND + MIN_FLAG_WAVE) *
                   sinf(sFlagWavePhase);
        } else {
            wave = 0.0f;
        }
        flagRotation[AXIS_X] = M_PI;
        flagRotation[AXIS_Z] = 0;
        flagScale[AXIS_X] = FLAG_SCALE;
        flagScale[AXIS_Y] = FLAG_SCALE;
        flagScale[AXIS_Z] = FLAG_SCALE;
        flagTranslation[AXIS_X] = D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        flagTranslation[AXIS_Y] = (D_800BA9D8.cupPosition[AXIS_Y] - FLAGSTICK_HEIGHT) * COURSE_TO_WORLD_SCALE;
        flagTranslation[AXIS_Z] = D_800BA9D8.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        flagRotation[AXIS_Y] = wave - g_HoleConditions.windDirection * BINARY_ANGLE_TO_RADIANS;
        BuildTransformMatrix(flagMatrix, flagRotation, flagTranslation, flagScale);
        ConvertMatrixToFixed(flagMatrix, &sFlagMatrices[g_DrawBufferIndex]);
        gDPPipeSync(g_DisplayListHead++);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
        gSPMatrix(g_DisplayListHead++, OS_K0_TO_PHYSICAL(&sFlagMatrices[g_DrawBufferIndex]),
                  G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        DrawHoleFlag(TRUE);
        gDPPipeSync(g_DisplayListHead++);
        gSPPopMatrix(g_DisplayListHead++, G_MTX_MODELVIEW);
    }
    gDPPipeSync(g_DisplayListHead++);
    gDPSetAlphaCompare(g_DisplayListHead++, G_AC_NONE);
}
