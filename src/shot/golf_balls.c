#include "shot/golf_balls.h"

#include <ultra64.h>

#include "camera/camera.h"
#include "camera/shot_camera.h"
#include "camera/view_projection.h"
#include "course/course_space.h"
#include "course/hole_loader.h"
#include "course/hole_setup.h"
#include "course/pin.h"
#include "course/surface.h"
#include "course/terrain.h"
#include "debug_text.h"
#include "game.h"
#include "game_flags.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round/game_state.h"
#include "round/player.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "shot/ball_simulation.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"

#define GOLF_BALL_COUNT 32
#define AFTERIMAGE_LENGTH_MAX 60
#define AFTERIMAGE_FAST_LENGTH 10

#define BALL_HIDDEN (-1)
#define BALL_SHOWN 0xFF
#define BALL_NO_MARKER (-1)
#define AFTERIMAGE_UNUSED (-1)
#define AFTERIMAGE_ALPHA 0x80
#define AFTERIMAGE_FAST_ALPHA 0xC0
#define AFTERIMAGE_FAST_FADE 0x10
#define AFTERIMAGE_SLOW_FADE_PERIOD 3

#define HEAP_BALL_IMAGES 2
#define BALL_IMAGE_HEAP_SIZE 0x1800

#define ASSET_SMALL_BALL_IMAGE 0x4C7
#define ASSET_ROLLING_BALL_IMAGE 0x4C8
#define ASSET_BALL_SHADOW_IMAGE 0x4DA
#define ASSET_BALL_MARKER_IMAGE 0x4C6
#define ASSET_BALL_AFTERIMAGE_IMAGE 0x6BB

#define PIKA_TEXTURE_WIDTH 32
#define PIKA_TEXTURE_HEIGHT 8

#define BOO_TEXTURE_WIDTH 48
#define BOO_TEXTURE_HEIGHT 42
#define BOO_TEXTURE_MASK 6

#define BALL_ROLL_DEGREES_PER_UNIT 0.09325484f
#define BALL_MODEL_SCALE 0.05f
#define BALL_RADIUS 0x600
#define BALL_MODEL_DROP_DIVISOR (WORLD_TO_COURSE_SCALE * 5.0)
#define FAST_AFTERIMAGE_SPACING 0.00009765625f
#define AFTERIMAGE_SPACING 0.0005859375f
#define CUP_VIEW_AFTERIMAGE_UNZOOM 0.5f

typedef struct GolfBall {
    Mtx matrices[DRAW_BUFFER_COUNT];
    SurfaceTraits *surface;
    Vec3 position;
    s32 alpha;
    UBoolean8 isInCup;
    BallState state;
    s8 markerPlayerIndex;
    LaunchPurpose8 purpose;
    Boolean8 hasEnteredWater;
} GolfBall;

typedef struct BallAfterimage {
    Vec3 position;
    s32 alpha;
    Boolean isInCup;
} BallAfterimage;

static Gfx f_UnusedListBeforeBooMouth[] UNUSED = {gsSPEndDisplayList()};
static u16 f_BooMouthTexture[] = {
#include "assets/shot/golf_balls/boo_mouth_texture.rgba16.inc.c"
};
static Gfx f_UnusedListAfterBooMouth[] UNUSED = {gsSPEndDisplayList()};
static u16 f_BooEyeTexture[] = {
#include "assets/shot/golf_balls/boo_eye_texture.rgba16.inc.c"
};
static Vtx f_BooEyeVertices[] = {
    {{{-146, 40, -96}, 0, {0xD80, 0x940}, {0xB9, 0x25, 0x62, 0xFF}}},
    {{{0, 35, -56}, 0, {0x120, 0x940}, {0x00, 0x0B, 0x7E, 0xFF}}},
    {{{0, 148, -107}, 0, {0x1E0, 0x80}, {0x00, 0x4E, 0x63, 0xFF}}},
    {{{-103, 155, -142}, 0, {0xC60, -0x1A0}, {0xC8, 0x5F, 0x3D, 0xFF}}},
    {{{148, 40, -96}, 0, {0xD80, 0x940}, {0x47, 0x25, 0x61, 0xFF}}},
    {{{0, 35, -56}, 0, {0x120, 0x960}, {0x00, 0x0B, 0x7E, 0xFF}}},
    {{{104, 155, -142}, 0, {0xC60, -0x1A0}, {0x38, 0x5F, 0x3D, 0xFF}}},
};
static Gfx f_BooEyeDisplayList[] = {
    gsDPPipeSync(),
    gsDPPipeSync(),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsSPClearGeometryMode(G_SHADE | G_CULL_BOTH | G_FOG | G_LIGHTING | G_SHADING_SMOOTH),
    gsSPSetGeometryMode(G_ZBUFFER | G_CULL_FRONT),
    gsDPPipeSync(),
    gsDPSetTextureFilter(G_TF_BILERP),
    gsDPPipeSync(),
    gsDPSetColorDither(G_CD_BAYER),
    gsDPPipeSync(),
    gsDPSetAlphaDither(G_AD_NOTPATTERN),
    gsDPPipeSync(),
    gsDPSetAlphaCompare(G_AC_NONE),
    gsDPPipeSync(),
    gsDPSetDepthSource(G_ZS_PIXEL),
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2),
    gsSPTexture(0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON),
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_NONE),
    gsDPLoadTextureBlock(
        f_BooEyeTexture,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        BOO_TEXTURE_WIDTH,
        BOO_TEXTURE_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP,
        BOO_TEXTURE_MASK,
        BOO_TEXTURE_MASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    ),
    gsSPSetGeometryMode(G_SHADE | G_LIGHTING | G_SHADING_SMOOTH),
    gsDPSetCombineLERP(
        TEXEL0,
        0,
        SHADE,
        0,
        ENVIRONMENT,
        0,
        TEXEL0,
        0,
        TEXEL0,
        0,
        SHADE,
        0,
        ENVIRONMENT,
        0,
        TEXEL0,
        0
    ),
    gsSPVertex(f_BooEyeVertices, 7, 0),
    gsDPSetPrimColor(0, 0, 0xFF, 0xFF, 0xFF, 0xFF),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(4, 2, 5, 0, 6, 2, 4, 0),
    gsSPEndDisplayList(),
};
static Vtx f_BooMouthVertices[] = {
    {{{0, -144, -101}, 0, {0x5A0, 0x9C0}, {0x00, 0xBB, 0x6A, 0xFF}}},
    {{{104, -155, -142}, 0, {0xB80, 0x9C0}, {0x42, 0xA7, 0x3D, 0xFF}}},
    {{{148, 40, -96}, 0, {0xC60, 0x80}, {0x47, 0x25, 0x61, 0xFF}}},
    {{{0, -144, -101}, 0, {0x5A0, 0x9E0}, {0x00, 0xBB, 0x6A, 0xFF}}},
    {{{-146, 40, -96}, 0, {-0xA0, 0x80}, {0xB9, 0x25, 0x62, 0xFF}}},
    {{{-103, -155, -142}, 0, {0, 0x9E0}, {0xBF, 0xA7, 0x3D, 0xFF}}},
    {{{0, 35, -56}, 0, {0x5A0, 0x100}, {0x00, 0x0B, 0x7E, 0xFF}}},
};
static Gfx f_BooMouthDisplayList[] = {
    gsDPPipeSync(),
    gsDPPipeSync(),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsSPClearGeometryMode(G_SHADE | G_CULL_BOTH | G_FOG | G_LIGHTING | G_SHADING_SMOOTH),
    gsSPSetGeometryMode(G_ZBUFFER | G_CULL_FRONT),
    gsDPPipeSync(),
    gsDPSetTextureFilter(G_TF_BILERP),
    gsDPPipeSync(),
    gsDPSetColorDither(G_CD_BAYER),
    gsDPPipeSync(),
    gsDPSetAlphaDither(G_AD_NOTPATTERN),
    gsDPPipeSync(),
    gsDPSetAlphaCompare(G_AC_NONE),
    gsDPPipeSync(),
    gsDPSetDepthSource(G_ZS_PIXEL),
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2),
    gsSPTexture(0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON),
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_NONE),
    gsDPLoadTextureBlock(
        f_BooMouthTexture,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        BOO_TEXTURE_WIDTH,
        BOO_TEXTURE_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP,
        BOO_TEXTURE_MASK,
        BOO_TEXTURE_MASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    ),
    gsSPSetGeometryMode(G_SHADE | G_LIGHTING | G_SHADING_SMOOTH),
    gsDPSetCombineLERP(
        TEXEL0,
        0,
        SHADE,
        0,
        ENVIRONMENT,
        0,
        TEXEL0,
        0,
        TEXEL0,
        0,
        SHADE,
        0,
        ENVIRONMENT,
        0,
        TEXEL0,
        0
    ),
    gsSPVertex(f_BooMouthVertices, 7, 0),
    gsDPSetPrimColor(0, 0, 0xFF, 0xFF, 0xFF, 0xFF),
    gsSP2Triangles(0, 1, 2, 0, 3, 4, 5, 0),
    gsSP2Triangles(6, 4, 3, 0, 0, 2, 6, 0),
    gsSPEndDisplayList(),
};
static Vtx f_BooBodyVertices[] = {
    {{{100, -120, -345}, 0, {0, 0}, {0x5A, 0xC8, 0xBB, 0xFF}}},
    {{{148, -136, -267}, 0, {0, 0}, {0x52, 0xAE, 0xCE, 0xFF}}},
    {{{0, -201, -251}, 0, {0, 0}, {0x00, 0x82, 0xF1, 0xFF}}},
    {{{193, 24, -267}, 0, {0, 0}, {0x69, 0x1A, 0xBF, 0xFF}}},
    {{{130, 0, -390}, 0, {0, 0}, {0x5F, 0x03, 0xAC, 0xFF}}},
    {{{148, 155, -267}, 0, {0, 0}, {0x65, 0x4B, 0xF5, 0xFF}}},
    {{{104, 155, -345}, 0, {0, 0}, {0x3D, 0x4E, 0xB2, 0xFF}}},
    {{{0, 217, -247}, 0, {0, 0}, {0x00, 0x7E, 0x00, 0xFF}}},
    {{{0, -43, -571}, 0, {0, 0}, {0x00, 0xC2, 0x92, 0xFF}}},
    {{{104, 155, -142}, 0, {0, 0}, {0x38, 0x5F, 0x3D, 0xFF}}},
    {{{0, 0, -455}, 0, {0, 0}, {0x0A, 0x20, 0x86, 0xFF}}},
    {{{0, 155, -390}, 0, {0, 0}, {0xF4, 0x51, 0xA0, 0xFF}}},
    {{{-98, -120, -345}, 0, {0, 0}, {0xAE, 0xC2, 0xB7, 0xFF}}},
    {{{-129, 0, -390}, 0, {0, 0}, {0xAA, 0xFE, 0xA4, 0xFF}}},
    {{{-103, 155, -345}, 0, {0, 0}, {0xB7, 0x4D, 0xBC, 0xFF}}},
    {{{-146, -136, -267}, 0, {0, 0}, {0xAA, 0xB3, 0xCD, 0xFF}}},
    {{{-192, 24, -267}, 0, {0, 0}, {0x9A, 0x2A, 0xC3, 0xFF}}},
    {{{-146, 155, -267}, 0, {0, 0}, {0xA0, 0x52, 0x00, 0xFF}}},
    {{{-103, -155, -142}, 0, {0, 0}, {0xBF, 0xA7, 0x3D, 0xFF}}},
    {{{-283, 42, -238}, 0, {0, 0}, {0x83, 0xFE, 0xED, 0xFF}}},
    {{{-146, 40, -96}, 0, {0, 0}, {0xB9, 0x25, 0x62, 0xFF}}},
    {{{-103, 155, -142}, 0, {0, 0}, {0xC8, 0x5F, 0x3D, 0xFF}}},
    {{{0, -144, -101}, 0, {0, 0}, {0x00, 0xBB, 0x6A, 0xFF}}},
    {{{0, 148, -107}, 0, {0, 0}, {0x00, 0x4E, 0x63, 0xFF}}},
    {{{104, -155, -142}, 0, {0, 0}, {0x42, 0xA7, 0x3D, 0xFF}}},
    {{{276, 42, -238}, 0, {0, 0}, {0x7D, 0xFF, 0xEE, 0xFF}}},
    {{{148, 40, -96}, 0, {0, 0}, {0x47, 0x25, 0x61, 0xFF}}},
};
static Gfx f_BooBodyDisplayList[] = {
    gsDPPipeSync(),
    gsDPPipeSync(),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsSPClearGeometryMode(G_SHADE | G_CULL_BOTH | G_FOG | G_LIGHTING | G_SHADING_SMOOTH),
    gsSPSetGeometryMode(G_ZBUFFER | G_CULL_FRONT),
    gsDPPipeSync(),
    gsDPSetTextureFilter(G_TF_BILERP),
    gsDPPipeSync(),
    gsDPSetColorDither(G_CD_BAYER),
    gsDPPipeSync(),
    gsDPSetAlphaDither(G_AD_NOTPATTERN),
    gsDPPipeSync(),
    gsDPSetAlphaCompare(G_AC_NONE),
    gsDPPipeSync(),
    gsDPSetDepthSource(G_ZS_PIXEL),
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2),
    gsSPSetGeometryMode(G_SHADE | G_LIGHTING | G_SHADING_SMOOTH),
    gsDPSetCombineLERP(
        PRIMITIVE,
        0,
        SHADE,
        0,
        ENVIRONMENT,
        0,
        SHADE,
        0,
        PRIMITIVE,
        0,
        SHADE,
        0,
        ENVIRONMENT,
        0,
        SHADE,
        0
    ),
    gsSPVertex(f_BooBodyVertices, 27, 0),
    gsDPSetPrimColor(0, 0, 0xFE, 0xFE, 0xFE, 0xFF),
    gsSP2Triangles(0, 1, 2, 0, 0, 3, 1, 0),
    gsSP2Triangles(4, 3, 0, 0, 4, 5, 3, 0),
    gsSP2Triangles(6, 5, 4, 0, 6, 7, 5, 0),
    gsSP2Triangles(8, 0, 2, 0, 5, 7, 9, 0),
    gsSP2Triangles(10, 6, 4, 0, 11, 6, 10, 0),
    gsSP2Triangles(11, 7, 6, 0, 12, 8, 2, 0),
    gsSP2Triangles(12, 10, 8, 0, 13, 10, 12, 0),
    gsSP2Triangles(13, 11, 10, 0, 14, 11, 13, 0),
    gsSP2Triangles(14, 7, 11, 0, 15, 12, 2, 0),
    gsSP2Triangles(15, 13, 12, 0, 16, 13, 15, 0),
    gsSP2Triangles(16, 14, 13, 0, 17, 14, 16, 0),
    gsSP2Triangles(17, 7, 14, 0, 18, 15, 2, 0),
    gsSP2Triangles(19, 16, 15, 0, 20, 16, 19, 0),
    gsSP2Triangles(20, 17, 16, 0, 21, 17, 20, 0),
    gsSP2Triangles(21, 7, 17, 0, 22, 18, 2, 0),
    gsSP2Triangles(0, 8, 10, 0, 0, 10, 4, 0),
    gsSP2Triangles(23, 7, 21, 0, 24, 22, 2, 0),
    gsSP2Triangles(9, 7, 23, 0, 1, 24, 2, 0),
    gsSP2Triangles(25, 26, 24, 0, 3, 26, 25, 0),
    gsSP2Triangles(26, 5, 9, 0, 26, 3, 5, 0),
    gsSP2Triangles(20, 19, 18, 0, 18, 19, 15, 0),
    gsSP2Triangles(3, 25, 1, 0, 1, 25, 24, 0),
    gsSPEndDisplayList(),
};
Gfx g_BooDisplayList[] = {
    gsSPDisplayList(f_BooEyeDisplayList),
    gsSPDisplayList(f_BooMouthDisplayList),
    gsSPDisplayList(f_BooBodyDisplayList),
    gsDPPipeSync(),
    gsSPEndDisplayList(),
};
static s32 f_UnusedWordsAfterBooDisplayList[2] UNUSED = {0};
static Vtx f_UnusedSquareVertices[] UNUSED = {
    {{{-7, 0, 0}, 0, {0, 0x7E0}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{7, 0, 0}, 0, {0x800, 0x7E0}, {0x00, 0x00, 0x00, 0xFF}}},
    {{{7, -14, 0}, 0, {0x800, 0}, {0x00, 0x00, 0xFF, 0x00}}},
    {{{-7, -14, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0x00}}},
};
static Vtx f_PikaBallVertices[] = {
    {{{-2, -1, -5}, 0, {0, 0x200}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{2, -1, -5}, 0, {0x800, 0x200}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-2, 1, -5}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{2, 1, -5}, 0, {0x800, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-5, -1, -2}, 0, {0x800, 0x200}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{-5, 1, -2}, 0, {0x800, 0}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{-5, -1, 2}, 0, {0, 0x200}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{-5, 1, 2}, 0, {0, 0}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{5, -1, -2}, 0, {0x800, 0x200}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{5, 1, -2}, 0, {0x800, 0}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{5, -1, 2}, 0, {0, 0x200}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{5, 1, 2}, 0, {0, 0}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{-2, -1, 5}, 0, {0, 0x200}, {0xFF, 0x80, 0x00, 0xFF}}},
    {{{2, -1, 5}, 0, {0x800, 0x200}, {0xFF, 0x80, 0x00, 0xFF}}},
    {{{-2, 1, 5}, 0, {0, 0}, {0xFF, 0x80, 0x00, 0xFF}}},
    {{{2, 1, 5}, 0, {0x800, 0}, {0xFF, 0x80, 0x00, 0xFF}}},
};
static GolfBall f_GolfBalls[GOLF_BALL_COUNT] = {0};
static BallAfterimage f_BallAfterimages[AFTERIMAGE_LENGTH_MAX] = {0};
static s32 f_BallAfterimageHead = 0;
static Boolean f_IsBallAfterimageShown = FALSE;
static s32 f_BallAfterimageFrameCount = 0;
static Boolean f_IsBallAfterimageZoomed = FALSE;
static Boolean f_IsBallAfterimageFast = FALSE;
static s32 f_BallAfterimageLength = AFTERIMAGE_LENGTH_MAX;
static s32 f_UnusedBallFrame = 0;
static s32 f_BallRollFrame = 0;

void UnusedGolfBallStub(void) {
}

static ScreenPosition f_CurrentBallScreenPoint;
static ImageAsset *f_SmallBallImage;
static ImageAsset *f_RollingBallImage;
static ImageAsset *f_BallShadowImage;
static ImageAsset *f_BallMarkerImage;
static ImageAsset *f_BallAfterimageImage;

static u8 f_UnusedBytesAfterAfterimageImage[16] UNUSED;

static u8 f_BallImageHeap[BALL_IMAGE_HEAP_SIZE];
static u16 f_PikaBallTexture[PIKA_TEXTURE_WIDTH * PIKA_TEXTURE_HEIGHT / 4];
static Mat4 f_BallRotation;
static s32 f_UnusedGolfBallResetValue;
static s32 f_LastRolledBallPosition[AXIS_COUNT];

void DrawPikaBallTexture(void) {
    DrawTextIntoTexture("PIKA", PIKA_TEXTURE_WIDTH / DEBUG_GLYPH_SIZE, f_PikaBallTexture);
}

void LoadGolfBallImages(void) {
    EncodedAsset asset;

    ResetHeap(HEAP_BALL_IMAGES);
    AddHeapRegion(HEAP_BALL_IMAGES, f_BallImageHeap, f_BallImageHeap + BALL_IMAGE_HEAP_SIZE);
    f_SmallBallImage =
        AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(ASSET_SMALL_BALL_IMAGE, &asset));
    ReadEncodedAsset(f_SmallBallImage, &asset);
    f_RollingBallImage =
        AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(ASSET_ROLLING_BALL_IMAGE, &asset));
    ReadEncodedAsset(f_RollingBallImage, &asset);
    f_BallShadowImage =
        AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(ASSET_BALL_SHADOW_IMAGE, &asset));
    ReadEncodedAsset(f_BallShadowImage, &asset);
    f_BallMarkerImage =
        AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(ASSET_BALL_MARKER_IMAGE, &asset));
    ReadEncodedAsset(f_BallMarkerImage, &asset);
    f_BallAfterimageImage =
        AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(ASSET_BALL_AFTERIMAGE_IMAGE, &asset));
    ReadEncodedAsset(f_BallAfterimageImage, &asset);
    osSyncPrintf("HEAP_BALLTEX LEFT%d\n", GetHeapFreeSize(HEAP_BALL_IMAGES));
    DrawPikaBallTexture();
    ResetGolfBalls();
    ResetBallRotation();
}

void RollGolfBall(GolfBall *ball UNUSED, BallSimulation *simulation) {
    Mat4 rotation;
    s32 movement[AXIS_COUNT];
    f32 distance;

    f32 GetLength(f32 x, f32 y, f32 z) {
        return sqrtf(x * x + y * y + z * z);
    }

    if (simulation->state == BALL_STATE_FLYING || simulation->state == BALL_STATE_ROLLING) {
        movement[AXIS_X] = simulation->position[AXIS_X] - f_LastRolledBallPosition[AXIS_X];
        movement[AXIS_Y] = simulation->position[AXIS_Y] - f_LastRolledBallPosition[AXIS_Y];
        movement[AXIS_Z] = simulation->position[AXIS_Z] - f_LastRolledBallPosition[AXIS_Z];
        f_LastRolledBallPosition[AXIS_X] = simulation->position[AXIS_X];
        f_LastRolledBallPosition[AXIS_Y] = simulation->position[AXIS_Y];
        f_LastRolledBallPosition[AXIS_Z] = simulation->position[AXIS_Z];
    }
    else {
        movement[AXIS_X] = simulation->velocity[AXIS_X];
        movement[AXIS_Y] = simulation->velocity[AXIS_Y];
        movement[AXIS_Z] = simulation->velocity[AXIS_Z];
    }

    if (simulation->state != BALL_STATE_STOPPED) {
        distance = GetLength(movement[AXIS_X], movement[AXIS_Y], movement[AXIS_Z]);
        if (movement[AXIS_Z] != 0 || movement[AXIS_X] != 0) {
            guRotateF(
                rotation,
                distance * BALL_ROLL_DEGREES_PER_UNIT,
                -movement[AXIS_Z],
                0.0f,
                movement[AXIS_X]
            );
            guMtxCatF(f_BallRotation, rotation, f_BallRotation);
        }
    }
}

void ResetBallAfterimage(void) {
    s32 afterimageIndex;

    for (afterimageIndex = 0; afterimageIndex != AFTERIMAGE_LENGTH_MAX; afterimageIndex++) {
        f_BallAfterimages[afterimageIndex].alpha = AFTERIMAGE_UNUSED;
        f_BallAfterimages[afterimageIndex].isInCup = FALSE;
    }

    f_BallAfterimages[0].position[AXIS_X] =
        f_GolfBalls[g_GameState.currentPlayerIndex].position[AXIS_X];
    f_BallAfterimages[0].position[AXIS_Y] =
        f_GolfBalls[g_GameState.currentPlayerIndex].position[AXIS_Y];
    f_BallAfterimages[0].position[AXIS_Z] =
        f_GolfBalls[g_GameState.currentPlayerIndex].position[AXIS_Z];
    f_BallAfterimages[AFTERIMAGE_LENGTH_MAX - 1].position[AXIS_X] =
        f_GolfBalls[g_GameState.currentPlayerIndex].position[AXIS_X];
    f_BallAfterimages[AFTERIMAGE_LENGTH_MAX - 1].position[AXIS_Y] =
        f_GolfBalls[g_GameState.currentPlayerIndex].position[AXIS_Y];
    f_BallAfterimageHead = 0;
    f_IsBallAfterimageShown = FALSE;
    f_BallAfterimageFrameCount = 0;
    f_IsBallAfterimageZoomed = FALSE;
    f_BallAfterimages[AFTERIMAGE_LENGTH_MAX - 1].position[AXIS_Z] =
        f_GolfBalls[g_GameState.currentPlayerIndex].position[AXIS_Z];
}

void StartFastBallAfterimage(void) {
    f_IsBallAfterimageShown = TRUE;
    f_IsBallAfterimageFast = TRUE;
    f_BallAfterimageLength = AFTERIMAGE_FAST_LENGTH;
}

void ResetBallRotation(void) {
    guMtxIdentF(f_BallRotation);
}

void ResetGolfBalls(void) {
    s32 ballIndex;

    for (ballIndex = 0; ballIndex < GOLF_BALL_COUNT; ballIndex++) {
        f_GolfBalls[ballIndex].alpha = BALL_HIDDEN;
        f_GolfBalls[ballIndex].state = BALL_STATE_STOPPED;
    }

    ResetBallAfterimage();
    ResetBallRotation();
    f_UnusedGolfBallResetValue = 0;
}

void UpdateGolfBalls(void) {
    s32 currentIndex;
    s32 i;
    Player *player;
    GolfBall *ball;
    BallSimulation *simulation;
    BallAfterimage *newestAfterimage;
    BallAfterimage *afterimage;
    BallAfterimage *pulledAfterimage;
    BallAfterimage *previous;
    Mat4 scaling;
    Mat4 rotated;
    Mat4 translation;
    f32 scale;
    f32 distance;

    if (g_GameState.step == GAME_STATE_TAKING_TURNS) {
        currentIndex = g_GameState.currentPlayerIndex;
    }
    else {
        currentIndex = 0;
    }

    for (i = 0; i < g_GameState.playerCount; i++) {
        GolfBall *ball = &f_GolfBalls[i];

        player = &g_Players[i];
        if (player->holeStatus != HOLE_STATUS_PLAYING || player->strokeCount == 0) {
            ball->alpha = BALL_HIDDEN;
        }

        if (currentIndex != i) {
            ball->state = BALL_STATE_STOPPED;
            if (GetSurfaceTraits(player->ballSurface)->kind != SURFACE_KIND_GREEN &&
                !g_GameState.isMiniGolf) {
                ball->markerPlayerIndex = BALL_NO_MARKER;
            }
            else {
                ball->markerPlayerIndex = i;
            }
        }
    }

    if (IsTurnTakingStep(g_GameState.step)) {
        GolfBall *ball;

        for (i = 0, ball = &f_GolfBalls[currentIndex]; i < BALL_SIMULATION_COUNT; i++) {
            simulation = &g_BallSimulations[i];
            ball->isInCup = FALSE;
            if (simulation->playerIndex >= 0 &&
                g_Players[currentIndex].holeStatus == HOLE_STATUS_PLAYING) {
                if (g_IsCupViewShown == TRUE) {
                    ball->position[AXIS_X] =
                        (simulation->position[AXIS_X] -
                            (f32)(g_HoleSetup.cupPosition[AXIS_X] - simulation->position[AXIS_X])) *
                        COURSE_TO_WORLD_SCALE;
                    ball->position[AXIS_Y] =
                        (simulation->position[AXIS_Y] -
                            (f32)(g_HoleSetup.cupPosition[AXIS_Y] - simulation->position[AXIS_Y])) *
                        COURSE_TO_WORLD_SCALE;
                    ball->position[AXIS_Z] =
                        (simulation->position[AXIS_Z] -
                            (f32)(g_HoleSetup.cupPosition[AXIS_Z] - simulation->position[AXIS_Z])) *
                        COURSE_TO_WORLD_SCALE;
                }
                else {
                    ball->position[AXIS_X] = simulation->position[AXIS_X] * COURSE_TO_WORLD_SCALE;
                    ball->position[AXIS_Y] = simulation->position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                    ball->position[AXIS_Z] = simulation->position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                }

                ball->alpha = BALL_SHOWN;
                ball->surface = GetSurfaceTraits(simulation->surface);
                if (simulation->state == BALL_STATE_IN_CUP) {
                    ball->isInCup = TRUE;
                }
                else {
                    ball->isInCup = FALSE;
                }

                if (GetCupDistance(
                        simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
                        simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
                    ) == 0 ||
                    simulation->state == BALL_STATE_IN_CUP ||
                    simulation->isHoledOut) {
                    ball->isInCup = TRUE;
                }
                else {
                    ball->isInCup = FALSE;
                }

                RollGolfBall(ball, simulation);
                ball->state = simulation->state;
                ball->hasEnteredWater = simulation->hasEnteredWater;
                ball->purpose = simulation->purpose;
                if (currentIndex == simulation->playerIndex ||
                    (GetSurfaceTraits(simulation->surface)->kind != SURFACE_KIND_GREEN &&
                        !g_GameState.isMiniGolf)) {
                    ball->markerPlayerIndex = BALL_NO_MARKER;
                }
                else {
                    ball->markerPlayerIndex = simulation->playerIndex;
                }

                scale = g_PinVertexScale * BALL_MODEL_SCALE;
                guScaleF(scaling, scale, scale, scale);
                guMtxCatF(f_BallRotation, scaling, rotated);
                guTranslateF(
                    translation,
                    ball->position[AXIS_X],
                    ball->position[AXIS_Y] -
                        ((g_PinVertexScale * BALL_RADIUS) / BALL_MODEL_DROP_DIVISOR),
                    ball->position[AXIS_Z]
                );
                MultiplyMatrices(rotated, translation, scaling);
                ConvertMatrixToFixed(scaling, &ball->matrices[g_DrawBufferIndex]);
            }
        }
    }

    if (f_IsBallAfterimageShown == TRUE) {
        if (g_IsCupViewShown == TRUE) {
            if (!f_IsBallAfterimageZoomed) {
                f_IsBallAfterimageZoomed = TRUE;
                for (i = 0; i != f_BallAfterimageLength; i++) {
                    afterimage = &f_BallAfterimages[i];
                    if (afterimage->alpha != AFTERIMAGE_UNUSED) {
                        s32 coursePosition[AXIS_COUNT] UNUSED;

                        coursePosition[AXIS_X] =
                            afterimage->position[AXIS_X] * WORLD_TO_COURSE_SCALE;
                        coursePosition[AXIS_Y] =
                            afterimage->position[AXIS_Y] * WORLD_TO_COURSE_SCALE;
                        coursePosition[AXIS_Z] =
                            afterimage->position[AXIS_Z] * WORLD_TO_COURSE_SCALE;
                        afterimage->position[AXIS_X] += afterimage->position[AXIS_X] -
                            (g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE);
                        afterimage->position[AXIS_Y] += afterimage->position[AXIS_Y] -
                            (g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE);
                        afterimage->position[AXIS_Z] += afterimage->position[AXIS_Z] -
                            (g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE);
                    }
                }
            }
        }
        else if (f_IsBallAfterimageZoomed == TRUE) {
            f_IsBallAfterimageZoomed = FALSE;
            for (i = 0; i != f_BallAfterimageLength; i++) {
                pulledAfterimage = &f_BallAfterimages[i];
                if (pulledAfterimage->alpha != AFTERIMAGE_UNUSED) {
                    pulledAfterimage->position[AXIS_X] -=
                        (pulledAfterimage->position[AXIS_X] -
                            (g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE)) *
                        CUP_VIEW_AFTERIMAGE_UNZOOM;
                    pulledAfterimage->position[AXIS_Y] -=
                        (pulledAfterimage->position[AXIS_Y] -
                            (g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE)) *
                        CUP_VIEW_AFTERIMAGE_UNZOOM;
                    pulledAfterimage->position[AXIS_Z] -=
                        (pulledAfterimage->position[AXIS_Z] -
                            (g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE)) *
                        CUP_VIEW_AFTERIMAGE_UNZOOM;
                }
            }
        }

        newestAfterimage = &f_BallAfterimages[f_BallAfterimageHead];
        ball = &f_GolfBalls[currentIndex];
        if (f_BallAfterimageHead == 0) {
            previous = &f_BallAfterimages[f_BallAfterimageLength - 1];
        }
        else {
            previous = &f_BallAfterimages[f_BallAfterimageHead - 1];
        }
        {
            Vec3 offset;

            offset[0] = ball->position[AXIS_X] - previous->position[AXIS_X];
            offset[1] = ball->position[AXIS_Y] - previous->position[AXIS_Y];
            offset[2] = ball->position[AXIS_Z] - previous->position[AXIS_Z];
            distance =
                sqrtf((offset[0] * offset[0]) + (offset[1] * offset[1]) + (offset[2] * offset[2]));
        }

        if (f_IsBallAfterimageFast == TRUE) {
            if ((f32)(g_PinVertexScale * BALL_RADIUS) * FAST_AFTERIMAGE_SPACING < distance &&
                g_BallSimulations[0].state == BALL_STATE_ROLLING) {
                newestAfterimage->alpha = AFTERIMAGE_FAST_ALPHA;
                newestAfterimage->position[AXIS_X] = ball->position[AXIS_X];
                newestAfterimage->position[AXIS_Y] = ball->position[AXIS_Y];
                newestAfterimage->position[AXIS_Z] = ball->position[AXIS_Z];
                if (ball->isInCup == TRUE) {
                    newestAfterimage->isInCup = TRUE;
                }
                else {
                    newestAfterimage->isInCup = FALSE;
                }

                f_BallAfterimageHead++;
                if (f_BallAfterimageHead == f_BallAfterimageLength) {
                    f_BallAfterimageHead = 0;
                }
            }

            for (i = 0; i != f_BallAfterimageLength; i++) {
                if (f_BallAfterimages[i].alpha > 0) {
                    f_BallAfterimages[i].alpha -= AFTERIMAGE_FAST_FADE;
                }
            }
        }
        else {
            if ((f32)(g_PinVertexScale * BALL_RADIUS) * AFTERIMAGE_SPACING < distance) {
                newestAfterimage->alpha = AFTERIMAGE_ALPHA;
                newestAfterimage->position[AXIS_X] = ball->position[AXIS_X];
                newestAfterimage->position[AXIS_Y] = ball->position[AXIS_Y];
                newestAfterimage->position[AXIS_Z] = ball->position[AXIS_Z];
                if (ball->isInCup == TRUE) {
                    newestAfterimage->isInCup = ball->isInCup;
                }
                else {
                    newestAfterimage->isInCup = FALSE;
                }

                f_BallAfterimageHead++;
                if (f_BallAfterimageHead == f_BallAfterimageLength) {
                    f_BallAfterimageHead = 0;
                }
            }

            if (f_BallAfterimageFrameCount % AFTERIMAGE_SLOW_FADE_PERIOD == 0) {
                for (i = 0; i != f_BallAfterimageLength; i++) {
                    if (f_BallAfterimages[i].alpha > 0) {
                        f_BallAfterimages[i].alpha -= 1;
                    }
                }
            }
        }

        f_BallAfterimageFrameCount++;
    }
}

Boolean IsAnyBallInCup(void) {
    Boolean isInCup = FALSE;
    s32 ballIndex;

    for (ballIndex = 0; ballIndex != GOLF_BALL_COUNT; ballIndex++) {
        if (f_GolfBalls[ballIndex].isInCup == TRUE && f_GolfBalls[ballIndex].alpha != BALL_HIDDEN) {
            isInCup = TRUE;
        }
    }

    return isInCup;
}

#define BALL_ROLL_FRAME_COUNT 16
#define BALL_ROLL_FRAME_SIZE 0x400
#define BALL_TILE_ROLLING G_TX_RENDERTILE
#define BALL_TILE_STILL 1
#define BALL_TILE_SHADOW 2
#define BALL_TILE_MARKER 3
#define BALL_TILE_SMALL 4
#define BALL_TILE_AFTERIMAGE 5
#define BALL_STILL_TMEM 0x80
#define BALL_SHADOW_TMEM 0x100
#define BALL_MARKER_TMEM 0x140
#define BALL_SMALL_TMEM 0x150
#define BALL_AFTERIMAGE_TMEM 0x190
#define BALL_TEXTURE_SIZE 32
#define BALL_AFTERIMAGE_FRAME_COUNT 3
#define BALL_AFTERIMAGE_FRAME_SIZE 16
#define SMALL_BALL_TEXTURE_SIZE 8
#define MARKER_TEXTURE_SIZE 16

enum BallLie {
    BALL_BURIED = -1,
    BALL_HALF_SUNK,
    BALL_EXPOSED
};

typedef s32 BallLie;

enum BallSprite {
    BALL_SPRITE_FULL,
    BALL_SPRITE_SMALL,
    BALL_SPRITE_DOT,
    BALL_SPRITE_FAR_DOT,
    BALL_SPRITE_DOT_SHADOWED,
    BALL_SPRITE_FAR_DOT_SHADOWED
};

typedef s32 BallSprite;

#define BALL_NEAREST_DEPTH (-5)
#define BALL_FULL_SPRITE_DEPTH (-86)
#define BALL_NEAR_DEPTH (-200)
#define BALL_DOT_DEPTH (-40000)
#define BALL_DEPTH_BIAS 60.0f
#define BALL_DEPTH_PULL 10.5f
#define NEAR_BALL_DEPTH_OFFSET 48.0f
#define FAR_BALL_DEPTH_OFFSET 8.0f
#define NEAR_SHADOW_DEPTH_OFFSET 56.0f
#define SUNK_SHADOW_DEPTH_OFFSET 40.0f
#define FAR_SHADOW_DEPTH_OFFSET 16.0f
#define PulledBallDepth(point, offset) \
    ((point).depth - ((point).depthStep * (BALL_DEPTH_PULL * g_PinVertexScale)) + (offset) - \
        BALL_DEPTH_BIAS)
#define FULL_SHADOW_SHADE 0x7F
#define SHADOW_FADE_DEPTH 512
#define SHADOW_SHADE_MIN 0x5F
#define CUP_VIEW_SHADOW_FADE_DEPTH 128
#define CUP_VIEW_SHADOW_SHADE_MIN 0x3F
#define SHADOW_DEPTH_GAP_MIN 8
#define AFTERIMAGE_HALF_SIZE_MIN QuarterPixels(1)
#define UNDERGROUND_BALL_DEPTH 153.6f
#define SHADOWLESS_CUP_BALL_HEIGHT 768.0f
#define PIKA_BALL_DEPTH 20000
#define CUP_VIEW_NEAR_BALL_DEPTH 0x402F
#define CUP_VIEW_SUNK_BALL_DEPTH 0x401F
#define CUP_VIEW_FAR_BALL_DEPTH 0x4007
#define CUP_VIEW_SHADOW_DEPTH_STEP 4

#define RM_ZB_XLU_SURF_UPDATE(clk) \
    (Z_CMP | Z_UPD | IM_RD | CVG_DST_FULL | ZMODE_XLU | FORCE_BL | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA))
#define RM_AA_ZB_XLU_SURF_UPDATE(clk) \
    (AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL | \
        GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA))

#define TexelStep(texelCount, from, to) \
    ((to) - (from) != 0 ? ((((texelCount) << 13) / ((to) - (from))) + 1) >> 1 : 0)
#define gSPStretchedTextureRectangle(pkt, xl, yl, xh, yh, tile, s, t, width, height) \
    gSPScisTextureRectangle( \
        pkt, \
        xl, \
        yl, \
        xh, \
        yh, \
        tile, \
        s, \
        t, \
        TexelStep(width, xl, xh), \
        TexelStep(height, yl, yh) \
    )

void DrawGolfBalls(CameraMatrices *matrices UNUSED) {
    Gfx *gfx = g_DisplayListHead;
    ScreenPoint ballPoint;
    ScreenPoint shadowPoint;
    s32 i;
    Boolean isCurrentBallAboveGround = FALSE;
    GolfBall *ball;
    BallAfterimage *afterimage;
    SurfaceTraits *surface;
    SurfaceTraits *pikaBallSurface;
    s32 groundHeight;
    BallSprite spriteKind;
    Boolean isAboveGround;
    BallLie exposure;
    Boolean isAboveCup;
    s32 ballDepth;
    s32 afterimageProjectedDepth;
    s32 shadowDepth;
    s32 routePointDepth;
#ifdef AVOID_UB
    s32 primDepth = 0;
#endif
    s32 shadowPrimDepth;
    s32 size;
    s32 waterShade;
    s32 shade;
    f32 shadowScale;
    s32 halfWidth;
    s32 halfHeight;
    u32 afterimageDepth;

    if (!IsTurnTakingStep(g_GameState.step)) {
        return;
    }

    f_UnusedBallFrame = (f_UnusedBallFrame + 1) % 9;
    f_BallRollFrame = (f_BallRollFrame + 1) & (BALL_ROLL_FRAME_COUNT - 1);
    g_ModuleRdpRequestFrames[MODULE_GOLF_BALLS] = g_FrameCount;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, RM_ZB_XLU_SURF_UPDATE(1), RM_ZB_XLU_SURF_UPDATE(2));
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0x01);
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(ImageBodyAt(f_RollingBallImage, f_BallRollFrame / 4 * BALL_ROLL_FRAME_SIZE)),
        G_IM_FMT_I,
        G_IM_SIZ_8b,
        BALL_TEXTURE_SIZE,
        BALL_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP,
        6,
        6,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPLoadMultiBlock(
        gfx++,
        TextureAddress(ImageBody(f_RollingBallImage)),
        BALL_STILL_TMEM,
        BALL_TILE_STILL,
        G_IM_FMT_I,
        G_IM_SIZ_8b,
        BALL_TEXTURE_SIZE,
        BALL_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP,
        6,
        6,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPLoadMultiBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_BallShadowImage)),
        BALL_SHADOW_TMEM,
        BALL_TILE_SHADOW,
        G_IM_FMT_I,
        16,
        16,
        0,
        G_TX_MIRROR | G_TX_WRAP,
        G_TX_MIRROR | G_TX_WRAP,
        4,
        4,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPLoadMultiBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_BallMarkerImage)),
        BALL_MARKER_TMEM,
        BALL_TILE_MARKER,
        G_IM_FMT_I,
        MARKER_TEXTURE_SIZE,
        MARKER_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP,
        6,
        6,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPLoadMultiBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_SmallBallImage)),
        BALL_SMALL_TMEM,
        BALL_TILE_SMALL,
        G_IM_FMT_I,
        16,
        16,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP,
        6,
        6,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPLoadMultiBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_BallAfterimageImage)),
        BALL_AFTERIMAGE_TMEM,
        BALL_TILE_AFTERIMAGE,
        G_IM_FMT_I,
        16,
        64,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP,
        6,
        6,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_AVERAGE);
    gDPPipeSync(gfx++);
    for (i = 0; i < GOLF_BALL_COUNT; i++) {
        ball = &f_GolfBalls[i];
        if (ball->alpha > 0) {
#ifndef AVOID_UB
            s32 primDepth;
#endif

            surface = ball->surface;
            groundHeight = GetGroundHeight(
                ball->position[AXIS_X] * WORLD_TO_COURSE_SCALE,
                ball->position[AXIS_Z] * WORLD_TO_COURSE_SCALE
            );
            spriteKind = BALL_SPRITE_FULL;
            isAboveGround = TRUE;
            if (ball->position[AXIS_Y] * WORLD_TO_COURSE_SCALE <
                groundHeight - UNDERGROUND_BALL_DEPTH) {
                isAboveGround = FALSE;
            }

            if (isAboveGround) {
                if (surface->sinkDepth == SINK_DEPTH_BURIED) {
                    exposure = BALL_BURIED;
                }
                else if (surface->sinkDepth == SINK_DEPTH_HALF) {
                    exposure = BALL_HALF_SUNK;
                }
                else {
                    exposure = BALL_EXPOSED;
                }
            }
            else {
                exposure = BALL_EXPOSED;
            }

            isAboveCup = FALSE;
            if (groundHeight + SHADOWLESS_CUP_BALL_HEIGHT <
                ball->position[AXIS_Y] * WORLD_TO_COURSE_SCALE) {
                isAboveCup = ball->isInCup == TRUE;
            }

            ballDepth = ProjectPoint(
                ball->position[AXIS_X],
                ball->position[AXIS_Y] - (g_PinVertexScale * 0.3f),
                ball->position[AXIS_Z],
                &ballPoint
            );
            shadowDepth = ProjectPoint(
                ball->position[AXIS_X],
                (groundHeight * COURSE_TO_WORLD_SCALE) - (g_PinVertexScale * 0.06f),
                ball->position[AXIS_Z],
                &shadowPoint
            );
            if (ball->markerPlayerIndex == BALL_NO_MARKER) {
                if (ballDepth < BALL_NEAREST_DEPTH && exposure >= BALL_HALF_SUNK) {
                    gDPPipeSync(gfx++);
                    if (ball->hasEnteredWater) {
                        groundHeight = GetGroundOrWaterHeight(
                            ball->position[AXIS_X] * WORLD_TO_COURSE_SCALE,
                            ball->position[AXIS_Z] * WORLD_TO_COURSE_SCALE
                        );
                        waterShade = (s32)(((ball->position[AXIS_Y] * WORLD_TO_COURSE_SCALE) -
                                               groundHeight) *
                            0.0032552085f);
                        waterShade = 100 - waterShade;
                        gDPPipeSync(gfx++);
                        gDPSetRenderMode(
                            gfx++,
                            RM_AA_ZB_XLU_SURF_UPDATE(1),
                            RM_AA_ZB_XLU_SURF_UPDATE(2)
                        );
                        if (waterShade < 0) {
                            waterShade = 0;
                        }

                        gDPSetPrimColor(gfx++, 0, 0, 0xEF, 0xEF, 0xFF, waterShade);
                        gDPPipeSync(gfx++);
                        gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
                    }
                    else {
                        gDPPipeSync(gfx++);
                        gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
                        gDPSetPrimColor(gfx++, 0, 0, 0xEF, 0xEF, 0xFF, OPAQUE_ALPHA);
                    }

                    if (g_IsCupViewShown == TRUE) {
                        if (ballDepth < g_CupViewDepth) {
                            primDepth = CUP_VIEW_NEAR_BALL_DEPTH;
                        }
                        else {
                            primDepth =
                                isAboveCup ? CUP_VIEW_SUNK_BALL_DEPTH : CUP_VIEW_FAR_BALL_DEPTH;
                        }
                    }
                    else if (ball->purpose == LAUNCH_PURPOSE_PLACED_AT_REST) {
                        primDepth = ballPoint.depth;
                    }
                    else if (ballDepth < g_CupViewDepth) {
                        if (isAboveCup == TRUE) {
                            continue;
                        }

                        if (ballDepth < BALL_NEAR_DEPTH) {
                            primDepth = ballPoint.depth;
                        }
                        else {
                            primDepth = PulledBallDepth(ballPoint, NEAR_BALL_DEPTH_OFFSET);
                        }
                    }
                    else {
                        if (isAboveCup == TRUE) {
                            continue;
                        }

                        if (ballDepth < BALL_NEAR_DEPTH) {
                            primDepth = ballPoint.depth;
                        }
                        else {
                            primDepth = PulledBallDepth(ballPoint, FAR_BALL_DEPTH_OFFSET);
                        }
                    }

                    primDepth &= ~3;
                    if (g_GameState.currentPlayerIndex == i) {
                        isCurrentBallAboveGround = isAboveGround;
                        f_CurrentBallScreenPoint.depth = primDepth;
                        f_CurrentBallScreenPoint.x = ballPoint.x;
                        f_CurrentBallScreenPoint.y = ballPoint.y;
                    }

                    if (primDepth < 0) {
                        primDepth = 0;
                    }

                    gDPSetPrimDepth(gfx++, primDepth, 0);
                    gDPPipeSync(gfx++);
                    size = (s32)(AbsFloat(ballPoint.scaleX) * (0.6f * (2.0f * g_PinVertexScale)) *
                        0.25f);
                    if (size <= 0) {
                        size = 1;
                    }

                    if (g_IsCupViewShown == TRUE || ballDepth >= BALL_FULL_SPRITE_DEPTH) {
                        spriteKind = BALL_SPRITE_FULL;
                        if (ball->state == BALL_STATE_STOPPED) {
                            gSPStretchedTextureRectangle(
                                gfx++,
                                QuarterPixels(ballPoint.x - size),
                                QuarterPixels(ballPoint.y - size),
                                QuarterPixels(ballPoint.x + size),
                                QuarterPixels(ballPoint.y + (exposure * size)),
                                BALL_TILE_STILL,
                                0,
                                0,
                                BALL_TEXTURE_SIZE,
                                (BALL_TEXTURE_SIZE / 2) + (exposure * (BALL_TEXTURE_SIZE / 2))
                            );
                        }
                        else {
                            gSPStretchedTextureRectangle(
                                gfx++,
                                QuarterPixels(ballPoint.x - size),
                                QuarterPixels(ballPoint.y - size),
                                QuarterPixels(ballPoint.x + size),
                                QuarterPixels(ballPoint.y + (exposure * size)),
                                BALL_TILE_ROLLING,
                                0,
                                0,
                                BALL_TEXTURE_SIZE,
                                (BALL_TEXTURE_SIZE / 2) + (exposure * (BALL_TEXTURE_SIZE / 2))
                            );
                        }
                    }
                    else if (ballDepth >= BALL_FULL_SPRITE_DEPTH) {
                        spriteKind = BALL_SPRITE_SMALL;
                        gSPStretchedTextureRectangle(
                            gfx++,
                            QuarterPixels(ballPoint.x - size),
                            QuarterPixels(ballPoint.y - size),
                            QuarterPixels(ballPoint.x + size),
                            QuarterPixels(ballPoint.y + (exposure * size)),
                            BALL_TILE_SMALL,
                            0,
                            0,
                            SMALL_BALL_TEXTURE_SIZE,
                            (SMALL_BALL_TEXTURE_SIZE / 2) +
                                (exposure * (SMALL_BALL_TEXTURE_SIZE / 2))
                        );
                    }
                    else if (ballDepth > BALL_DOT_DEPTH) {
                        gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
                        gDPPipeSync(gfx++);
                        gSPStretchedTextureRectangle(
                            gfx++,
                            QuarterPixels(ballPoint.x - 1),
                            QuarterPixels(ballPoint.y - 1),
                            QuarterPixels(ballPoint.x + 1),
                            QuarterPixels(ballPoint.y),
                            BALL_TILE_SMALL,
                            0,
                            0,
                            SMALL_BALL_TEXTURE_SIZE,
                            SMALL_BALL_TEXTURE_SIZE
                        );
                        gDPPipeSync(gfx++);
                        gDPSetPrimColor(gfx++, 0, 0, 0xBF, 0xBF, 0xBF, OPAQUE_ALPHA);
                        gDPPipeSync(gfx++);
                        gDPPipeSync(gfx++);
                        gDPPipeSync(gfx++);
                        gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
                        gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 0x3F);
                        gDPPipeSync(gfx++);
                        if (isAboveGround == TRUE) {
                            spriteKind = BALL_SPRITE_DOT_SHADOWED;
                            gSPStretchedTextureRectangle(
                                gfx++,
                                QuarterPixels(ballPoint.x - 1),
                                QuarterPixels(ballPoint.y + 1),
                                QuarterPixels(ballPoint.x + 1),
                                QuarterPixels(ballPoint.y + 2),
                                BALL_TILE_SMALL,
                                0,
                                0,
                                SMALL_BALL_TEXTURE_SIZE,
                                SMALL_BALL_TEXTURE_SIZE
                            );
                        }
                        else {
                            spriteKind = BALL_SPRITE_DOT;
                        }
                    }
                    else {
                        gDPPipeSync(gfx++);
                        gDPSetRenderMode(gfx++, RM_ZB_XLU_SURF_UPDATE(1), RM_ZB_XLU_SURF_UPDATE(2));
                        gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
                        gDPPipeSync(gfx++);
                        gSPStretchedTextureRectangle(
                            gfx++,
                            QuarterPixels(ballPoint.x) - 4,
                            QuarterPixels(ballPoint.y - 1) - 4,
                            QuarterPixels(ballPoint.x),
                            QuarterPixels(ballPoint.y),
                            BALL_TILE_SMALL,
                            0,
                            0,
                            SMALL_BALL_TEXTURE_SIZE,
                            SMALL_BALL_TEXTURE_SIZE
                        );
                        gDPPipeSync(gfx++);
                        gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, OPAQUE_ALPHA);
                        gDPPipeSync(gfx++);
                        spriteKind = BALL_SPRITE_FAR_DOT;
                        if (isAboveGround == TRUE) {
                            spriteKind = BALL_SPRITE_FAR_DOT_SHADOWED;
                        }
                    }

                    gDPPipeSync(gfx++);
                    gDPPipeSync(gfx++);
                    gDPSetRenderMode(gfx++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
                    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                }

                if (exposure == BALL_EXPOSED &&
                    shadowDepth < BALL_NEAREST_DEPTH &&
                    !isAboveCup &&
                    !ball->hasEnteredWater) {
                    size = (s32)(AbsFloat(shadowPoint.scaleX) * (0.6f * (2.0f * g_PinVertexScale)) *
                        0.25f);
                    if (g_IsCupViewShown == TRUE) {
                        if (shadowDepth < g_CupViewDepth) {
                            shadowPrimDepth = CUP_VIEW_NEAR_BALL_DEPTH + CUP_VIEW_SHADOW_DEPTH_STEP;
                        }
                        else if (ball->isInCup == TRUE) {
                            shadowPrimDepth = CUP_VIEW_SUNK_BALL_DEPTH + CUP_VIEW_SHADOW_DEPTH_STEP;
                        }
                        else {
                            shadowPrimDepth = CUP_VIEW_FAR_BALL_DEPTH + CUP_VIEW_SHADOW_DEPTH_STEP;
                        }

                        shade = FULL_SHADOW_SHADE -
                            ((s32)(groundHeight -
                                 (ball->position[AXIS_Y] * WORLD_TO_COURSE_SCALE)) /
                                CUP_VIEW_SHADOW_FADE_DEPTH);
                        if (shade < CUP_VIEW_SHADOW_SHADE_MIN) {
                            shade = CUP_VIEW_SHADOW_SHADE_MIN;
                        }
                    }
                    else {
                        if (ball->purpose == LAUNCH_PURPOSE_PLACED_AT_REST) {
                            shadowPrimDepth = ballPoint.depth;
                        }
                        else {
                            if (shadowDepth < g_CupViewDepth) {
                                shadowPrimDepth =
                                    PulledBallDepth(shadowPoint, NEAR_SHADOW_DEPTH_OFFSET);
                            }
                            else if (ball->isInCup == TRUE) {
                                shadowPrimDepth =
                                    PulledBallDepth(shadowPoint, SUNK_SHADOW_DEPTH_OFFSET);
                            }
                            else {
                                shadowPrimDepth =
                                    PulledBallDepth(shadowPoint, FAR_SHADOW_DEPTH_OFFSET);
                            }
                        }

                        shade = FULL_SHADOW_SHADE -
                            ((s32)(groundHeight -
                                 (ball->position[AXIS_Y] * WORLD_TO_COURSE_SCALE)) /
                                SHADOW_FADE_DEPTH);
                        if (shade < SHADOW_SHADE_MIN) {
                            shade = SHADOW_SHADE_MIN;
                        }
                    }

                    if (shade > FULL_SHADOW_SHADE) {
                        shade = FULL_SHADOW_SHADE;
                    }

                    shadowScale = 1.0f / FULL_SHADOW_SHADE;
                    shadowScale *= shade;
                    size *= shadowScale;
                    if (size <= 0) {
                        size = 1;
                    }

                    shadowPrimDepth &= ~3;
                    if (shadowPrimDepth < 0) {
                        shadowPrimDepth = 0;
                    }

                    if (shadowPrimDepth < primDepth + SHADOW_DEPTH_GAP_MIN) {
                        shadowPrimDepth = primDepth + SHADOW_DEPTH_GAP_MIN;
                    }

                    gDPSetPrimDepth(gfx++, shadowPrimDepth, 0);
                    gDPPipeSync(gfx++);
                    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, shade);
                    if (spriteKind < BALL_SPRITE_DOT_SHADOWED) {
                        gDPPipeSync(gfx++);
                        gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
                        gDPPipeSync(gfx++);
                        gSPStretchedTextureRectangle(
                            gfx++,
                            QuarterPixels(shadowPoint.x - size),
                            QuarterPixels(shadowPoint.y - size),
                            QuarterPixels(shadowPoint.x + size),
                            QuarterPixels(shadowPoint.y + size),
                            BALL_TILE_SHADOW,
                            0,
                            0,
                            BALL_TEXTURE_SIZE,
                            BALL_TEXTURE_SIZE
                        );
                    }
                }
            }
            else if (shadowDepth < BALL_NEAREST_DEPTH) {
                size = AbsFloat(shadowPoint.scaleX) * (0.6f * (2.0f * g_PinVertexScale)) * 0.25f;
                if (g_IsCupViewShown != TRUE) {
                    if (shadowDepth < g_CupViewDepth) {
                        shadowPrimDepth = PulledBallDepth(shadowPoint, NEAR_SHADOW_DEPTH_OFFSET);
                    }
                    else if (ball->isInCup == TRUE) {
                        shadowPrimDepth = PulledBallDepth(shadowPoint, SUNK_SHADOW_DEPTH_OFFSET);
                    }
                    else {
                        shadowPrimDepth = PulledBallDepth(shadowPoint, FAR_SHADOW_DEPTH_OFFSET);
                    }

                    if (size <= 0) {
                        size = 1;
                    }

                    gDPSetPrimDepth(gfx++, shadowPrimDepth & ~3, 0);
                    gDPPipeSync(gfx++);
                    switch (ball->markerPlayerIndex) {
                        case BALL_NO_MARKER:
                            gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 0);
                            break;
                        case 0:
                            gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0x7F, OPAQUE_ALPHA);
                            break;
                        case 1:
                            gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0x7F, 0xFF, OPAQUE_ALPHA);
                            break;
                        case 2:
                            gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0xFF, OPAQUE_ALPHA);
                            break;
                        case 3:
                            gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xBF, 0x7F, OPAQUE_ALPHA);
                            break;
                    }

                    gDPPipeSync(gfx++);
                    gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
                    gDPPipeSync(gfx++);
                    gSPStretchedTextureRectangle(
                        gfx++,
                        QuarterPixels(ballPoint.x - size),
                        QuarterPixels(ballPoint.y - size),
                        QuarterPixels(ballPoint.x + size),
                        QuarterPixels(ballPoint.y + size),
                        BALL_TILE_MARKER,
                        0,
                        0,
                        MARKER_TEXTURE_SIZE,
                        MARKER_TEXTURE_SIZE
                    );
                }
            }
        }
    }

    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2);
    gDPSetCombineLERP(
        gfx++,
        PRIMITIVE,
        0,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        PRIMITIVE,
        0,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    gDPSetEnvColor(gfx++, 0x00, 0x00, 0x80, 0x1E);
    for (i = 0; i != f_BallAfterimageLength; i++) {
        BallAfterimage *afterimage = &f_BallAfterimages[i];

        if (afterimage->alpha > 0) {
            afterimageProjectedDepth = ProjectPoint(
                afterimage->position[AXIS_X],
                afterimage->position[AXIS_Y] - (g_PinVertexScale * 0.27f),
                afterimage->position[AXIS_Z],
                &ballPoint
            );
            if (afterimageProjectedDepth < BALL_NEAREST_DEPTH) {
                if (g_IsCupViewShown == TRUE) {
                    afterimageDepth = CUP_VIEW_NEAR_BALL_DEPTH;
                    if (afterimageProjectedDepth >= g_CupViewDepth) {
                        afterimageDepth = afterimage->isInCup == TRUE ? CUP_VIEW_SUNK_BALL_DEPTH :
                                                                        CUP_VIEW_FAR_BALL_DEPTH;
                    }

                    gDPSetPrimColor(gfx++, 0, 0, 0xA0, 0xA0, 0xFF, afterimage->alpha / 2);
                }
                else {
                    if (afterimageProjectedDepth < g_CupViewDepth) {
                        afterimageDepth = PulledBallDepth(ballPoint, NEAR_BALL_DEPTH_OFFSET);
                    }
                    else if (afterimage->isInCup == FALSE) {
                        afterimageDepth = PulledBallDepth(ballPoint, FAR_BALL_DEPTH_OFFSET);
                    }
                    else {
                        continue;
                    }

                    if (f_IsBallAfterimageFast == TRUE) {
                        gDPSetPrimColor(gfx++, 0, 0, 0xA0, 0xA0, 0xFF, afterimage->alpha / 2);
                    }
                    else {
                        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, afterimage->alpha / 2);
                    }
                }

                gDPSetPrimDepth(gfx++, afterimageDepth & ~3, 0);
                gDPPipeSync(gfx++);
                halfWidth =
                    (AbsFloat(ballPoint.scaleX) *
                        ((0.3 * COURSE_TO_WORLD_SCALE) * (g_PinVertexScale * BALL_RADIUS * 2))) -
                    1.0;
                halfHeight =
                    (AbsFloat(ballPoint.scaleY) *
                        ((0.3 * COURSE_TO_WORLD_SCALE) * (g_PinVertexScale * BALL_RADIUS * 2))) -
                    1.0;
                if (halfWidth < AFTERIMAGE_HALF_SIZE_MIN) {
                    halfWidth = AFTERIMAGE_HALF_SIZE_MIN;
                }

                if (halfHeight < AFTERIMAGE_HALF_SIZE_MIN) {
                    halfHeight = AFTERIMAGE_HALF_SIZE_MIN;
                }

                if (f_IsBallAfterimageFast == TRUE) {
                    gSPStretchedTextureRectangle(
                        gfx++,
                        QuarterPixels(ballPoint.x) - halfWidth - 1,
                        QuarterPixels(ballPoint.y) - halfHeight - 1,
                        QuarterPixels(ballPoint.x) + halfWidth + 2,
                        QuarterPixels(ballPoint.y) + halfHeight + 2,
                        BALL_TILE_AFTERIMAGE,
                        0,
                        TexelCoordinate(
                            (guRandom() % BALL_AFTERIMAGE_FRAME_COUNT) * BALL_AFTERIMAGE_FRAME_SIZE
                        ),
                        15,
                        15
                    );
                }
                else {
                    gSPStretchedTextureRectangle(
                        gfx++,
                        QuarterPixels(ballPoint.x) - halfWidth - 1,
                        QuarterPixels(ballPoint.y) - halfHeight - 1,
                        QuarterPixels(ballPoint.x) + halfWidth + 2,
                        QuarterPixels(ballPoint.y) + halfHeight + 2,
                        BALL_TILE_STILL,
                        0,
                        0,
                        BALL_TEXTURE_SIZE,
                        BALL_TEXTURE_SIZE
                    );
                }

                gDPPipeSync(gfx++);
            }
        }
    }

    if (IsGameFlagSet(GAME_FLAG_SHOW_ROUTE_POINTS)) {
        for (i = 0; i != ROUTE_POINT_COUNT; i++) {
            afterimage = &f_BallAfterimages[i];
            routePointDepth = ProjectPoint(
                g_HolePoints.routePoints[i][0] * COURSE_TO_WORLD_SCALE,
                (g_HolePoints.routePoints[i][1] * COURSE_TO_WORLD_SCALE) -
                    (g_PinVertexScale * 0.27f),
                g_HolePoints.routePoints[i][2] * COURSE_TO_WORLD_SCALE,
                &shadowPoint
            );
            if (routePointDepth < BALL_NEAREST_DEPTH) {
                if (routePointDepth < g_CupViewDepth) {
                    afterimageDepth = PulledBallDepth(shadowPoint, NEAR_BALL_DEPTH_OFFSET);
                }
                else if (afterimage->isInCup == FALSE) {
                    afterimageDepth = PulledBallDepth(shadowPoint, FAR_BALL_DEPTH_OFFSET);
                }
                else {
                    continue;
                }

                if (f_IsBallAfterimageFast == TRUE) {
                    gDPSetPrimColor(gfx++, 0, 0, 0xA0, 0xA0, 0xFF, afterimage->alpha / 2);
                }
                else {
                    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, afterimage->alpha / 2);
                }

                gDPSetPrimDepth(gfx++, afterimageDepth & ~3, 0);
                gDPPipeSync(gfx++);
                halfWidth =
                    (AbsFloat(shadowPoint.scaleX) *
                        ((0.3 * COURSE_TO_WORLD_SCALE) * (g_PinVertexScale * BALL_RADIUS * 2))) -
                    1.0;
                halfHeight =
                    (AbsFloat(shadowPoint.scaleY) *
                        ((0.3 * COURSE_TO_WORLD_SCALE) * (g_PinVertexScale * BALL_RADIUS * 2))) -
                    1.0;
                if (halfWidth < AFTERIMAGE_HALF_SIZE_MIN) {
                    halfWidth = AFTERIMAGE_HALF_SIZE_MIN;
                }

                if (halfHeight < AFTERIMAGE_HALF_SIZE_MIN) {
                    halfHeight = AFTERIMAGE_HALF_SIZE_MIN;
                }

                if (f_IsBallAfterimageFast == TRUE) {
                    gSPStretchedTextureRectangle(
                        gfx++,
                        QuarterPixels(shadowPoint.x) - halfWidth - 1,
                        QuarterPixels(shadowPoint.y) - halfHeight - 1,
                        QuarterPixels(shadowPoint.x) + halfWidth + 2,
                        QuarterPixels(shadowPoint.y) + halfHeight + 2,
                        BALL_TILE_AFTERIMAGE,
                        0,
                        TexelCoordinate(
                            (guRandom() % BALL_AFTERIMAGE_FRAME_COUNT) * BALL_AFTERIMAGE_FRAME_SIZE
                        ),
                        15,
                        15
                    );
                }
                else {
                    gSPStretchedTextureRectangle(
                        gfx++,
                        QuarterPixels(shadowPoint.x) - halfWidth - 1,
                        QuarterPixels(shadowPoint.y) - halfHeight - 1,
                        QuarterPixels(shadowPoint.x) + halfWidth + 2,
                        QuarterPixels(shadowPoint.y) + halfHeight + 2,
                        BALL_TILE_STILL,
                        0,
                        0,
                        BALL_TEXTURE_SIZE,
                        BALL_TEXTURE_SIZE
                    );
                }

                gDPPipeSync(gfx++);
            }
        }
    }

    gDPPipeSync(gfx++);
    AppendSyncedRdpState(gfx, gDPSetCycleType, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, RM_AA_ZB_XLU_SURF_UPDATE(1), RM_AA_ZB_XLU_SURF_UPDATE(2));
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        SHADE,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        SHADE,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0x00, 0x60);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(f_PikaBallTexture),
        G_IM_FMT_IA,
        PIKA_TEXTURE_WIDTH,
        PIKA_TEXTURE_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetPrimDepth(gfx++, f_CurrentBallScreenPoint.depth - 4, 0);
    if (f_CurrentBallScreenPoint.depth < PIKA_BALL_DEPTH) {
        for (i = 0; i < GOLF_BALL_COUNT; i++) {
            ball = &f_GolfBalls[i];
            if (ball->markerPlayerIndex == BALL_NO_MARKER &&
                f_GolfBalls[i].alpha == BALL_SHOWN &&
                (g_IsCupViewShown || ball->isInCup != TRUE)) {
                pikaBallSurface = ball->surface;
                if ((!isCurrentBallAboveGround || pikaBallSurface->sinkDepth == SINK_DEPTH_NONE) &&
                    ball->alpha > 0) {
                    gDPPipeSync(gfx++);
                    gSPMatrix(
                        gfx++,
                        &ball->matrices[g_DrawBufferIndex],
                        G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
                    );
                    gSPVertex(gfx++, f_PikaBallVertices, 16, 0);
                    gSP2Triangles(gfx++, 0, 2, 1, 0, 1, 2, 3, 0);
                    gSP2Triangles(gfx++, 4, 6, 5, 0, 5, 6, 7, 0);
                    gSP2Triangles(gfx++, 8, 9, 10, 0, 9, 11, 10, 0);
                    gSP2Triangles(gfx++, 12, 13, 14, 0, 13, 15, 14, 0);
                    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
                    gDPPipeSync(gfx++);
                }
            }
        }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPPipeSync(gfx++);
    g_DisplayListHead = gfx;
}
