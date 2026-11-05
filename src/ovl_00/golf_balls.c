/*
 * The golf balls on the course, one for each player, and the fading
 * afterimages that the ball in play leaves behind it.
 */
#include "common.h"

#include "golf_balls.h"

#include "asset.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "camera.h"
#include "cpu_projection.h"
#include "debug_text.h"
#include "game.h"
#include "game_flags.h"
#include "heap.h"
#include "hole_loader.h"
#include "main.h"
#include "overlay_manager.h"
#include "pin.h"
#include "save.h"
#include "screen.h"
#include "shot_camera.h"
#include "surface.h"
#include "terrain.h"

#define GOLF_BALL_COUNT 32
#define AFTERIMAGE_MAX_LENGTH 60
#define AFTERIMAGE_FAST_LENGTH 10

/* A ball's alpha while it is hidden */
#define BALL_HIDDEN (-1)
#define BALL_SHOWN 0xFF
/* A ball's markerPlayerIndex while no number is shown over it */
#define BALL_NO_MARKER (-1)
/* An afterimage's alpha while it is unused */
#define AFTERIMAGE_UNUSED (-1)
#define AFTERIMAGE_ALPHA 0x80
#define AFTERIMAGE_FAST_ALPHA 0xC0
#define AFTERIMAGE_FAST_FADE 0x10
/* Slow afterimages fade by one on every this many frames */
#define AFTERIMAGE_SLOW_FADE_PERIOD 3

/* The heap that the ball images are loaded into, and its size */
#define HEAP_BALL_IMAGES 2
#define BALL_IMAGE_HEAP_SIZE 0x1800

/* The texture that "PIKA" is written into for Pikachu's ball */
#define PIKA_TEXTURE_WIDTH 32
#define PIKA_TEXTURE_HEIGHT 8

/*
 * A model that no routine draws. Rendered from its vertices, it is a round
 * white body with stubby arms and a tail, eyes and a fanged mouth: a Boo
 */
#define BOO_TEXTURE_WIDTH 48
#define BOO_TEXTURE_HEIGHT 42
/* The textures wrap at 64 texels, the power of two above both sizes */
#define BOO_TEXTURE_MASK 6

/* The ball turns this many degrees as it rolls one course unit */
#define BALL_ROLL_DEGREES_PER_UNIT 0.09325484f
#define BALL_MODEL_SCALE 0.05f
/* The ball's radius in course units, at a pin scale of one */
#define BALL_RADIUS 0x600

typedef struct GolfBall {
  /* 0x00 */ Mtx matrices[DRAW_BUFFER_COUNT];
  /* 0x80 */ SurfaceInfo* surface;
  /* 0x84 */ f32 position[3];
  /* 0x90 */ s32 alpha; /* Or BALL_HIDDEN */
  /* 0x94 */ u8 isInCup;
  /* 0x98 */ s32 state; /* A BALL_STATE_* value */
  /* 0x9C */ s8 markerPlayerIndex; /* Or BALL_NO_MARKER */
  /* 0x9D */ s8 purpose; /* A LAUNCH_PURPOSE_* value */
  /* 0x9E */ s8 unk9E;
} GolfBall; /* size = 0xA0 */

typedef struct BallAfterimage {
  /* 0x00 */ f32 position[3];
  /* 0x0C */ s32 alpha; /* Or AFTERIMAGE_UNUSED */
  /* 0x10 */ Boolean isInCup;
} BallAfterimage; /* size = 0x14 */

/* A ball's screen pixel and depth, as the start of a ScreenPoint gives them */
typedef struct BallScreenPoint {
  /* 0x0 */ s32 x;
  /* 0x4 */ s32 y;
  /* 0x8 */ s32 depth;
} BallScreenPoint; /* size = 0xC */

/* Read by no routine */
static Gfx f_UnusedListBeforeBooMouth[] UNUSED = {gsSPEndDisplayList()};
static u16 f_BooMouthTexture[] = {
#include "assets/ovl_00/golf_balls/boo_mouth_texture.rgba16.inc.c"
};
/* Read by no routine */
static Gfx f_UnusedListAfterBooMouth[] UNUSED = {gsSPEndDisplayList()};
static u16 f_BooEyeTexture[] = {
#include "assets/ovl_00/golf_balls/boo_eye_texture.rgba16.inc.c"
};
static Vtx f_BooEyeVertices[] = {
    {{{-146, 40, -96}, 0, {0xD80, 0x940}, {0xB9, 0x25, 0x62, 0xFF}}},
    {{{0, 35, -56}, 0, {0x120, 0x940}, {0x00, 0x0B, 0x7E, 0xFF}}},
    {{{0, 148, -107}, 0, {0x1E0, 0x80}, {0x00, 0x4E, 0x63, 0xFF}}},
    {{{-103, 155, -142}, 0, {0xC60, -0x1A0}, {0xC8, 0x5F, 0x3D, 0xFF}}},
    {{{148, 40, -96}, 0, {0xD80, 0x940}, {0x47, 0x25, 0x61, 0xFF}}},
    {{{0, 35, -56}, 0, {0x120, 0x960}, {0x00, 0x0B, 0x7E, 0xFF}}},
    {{{104, 155, -142}, 0, {0xC60, -0x1A0}, {0x38, 0x5F, 0x3D, 0xFF}}}};
static Gfx f_BooEyeDisplayList[] = {
    gsDPPipeSync(),
    gsDPPipeSync(),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsSPClearGeometryMode(
        G_SHADE | G_CULL_BOTH | G_FOG | G_LIGHTING | G_SHADING_SMOOTH),
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
    gsDPLoadTextureBlock(f_BooEyeTexture, G_IM_FMT_RGBA, G_IM_SIZ_16b,
        BOO_TEXTURE_WIDTH, BOO_TEXTURE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP, BOO_TEXTURE_MASK, BOO_TEXTURE_MASK,
        G_TX_NOLOD, G_TX_NOLOD),
    gsSPSetGeometryMode(G_SHADE | G_LIGHTING | G_SHADING_SMOOTH),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, ENVIRONMENT, 0, TEXEL0, 0, TEXEL0,
        0, SHADE, 0, ENVIRONMENT, 0, TEXEL0, 0),
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
    {{{0, 35, -56}, 0, {0x5A0, 0x100}, {0x00, 0x0B, 0x7E, 0xFF}}}};
static Gfx f_BooMouthDisplayList[] = {
    gsDPPipeSync(),
    gsDPPipeSync(),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsSPClearGeometryMode(
        G_SHADE | G_CULL_BOTH | G_FOG | G_LIGHTING | G_SHADING_SMOOTH),
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
    gsDPLoadTextureBlock(f_BooMouthTexture, G_IM_FMT_RGBA, G_IM_SIZ_16b,
        BOO_TEXTURE_WIDTH, BOO_TEXTURE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP, BOO_TEXTURE_MASK, BOO_TEXTURE_MASK,
        G_TX_NOLOD, G_TX_NOLOD),
    gsSPSetGeometryMode(G_SHADE | G_LIGHTING | G_SHADING_SMOOTH),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, ENVIRONMENT, 0, TEXEL0, 0, TEXEL0,
        0, SHADE, 0, ENVIRONMENT, 0, TEXEL0, 0),
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
    {{{148, 40, -96}, 0, {0, 0}, {0x47, 0x25, 0x61, 0xFF}}}};
static Gfx f_BooBodyDisplayList[] = {
    gsDPPipeSync(),
    gsDPPipeSync(),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsSPClearGeometryMode(
        G_SHADE | G_CULL_BOTH | G_FOG | G_LIGHTING | G_SHADING_SMOOTH),
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
    gsDPSetCombineLERP(PRIMITIVE, 0, SHADE, 0, ENVIRONMENT, 0, SHADE, 0,
        PRIMITIVE, 0, SHADE, 0, ENVIRONMENT, 0, SHADE, 0),
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
/* Read by no routine */
static s32 f_UnusedWordsAfterBooDisplayList[2] UNUSED = {0};
/* Read by no routine */
static Vtx f_UnusedSquareVertices[] UNUSED = {
    {{{-7, 0, 0}, 0, {0, 0x7E0}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{7, 0, 0}, 0, {0x800, 0x7E0}, {0x00, 0x00, 0x00, 0xFF}}},
    {{{7, -14, 0}, 0, {0x800, 0}, {0x00, 0x00, 0xFF, 0x00}}},
    {{{-7, -14, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0x00}}}};
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
    {{{2, 1, 5}, 0, {0x800, 0}, {0xFF, 0x80, 0x00, 0xFF}}}};
static GolfBall f_GolfBalls[GOLF_BALL_COUNT] = {0};
static BallAfterimage f_BallAfterimages[AFTERIMAGE_MAX_LENGTH] = {0};
/* The point that the next one is written over */
static s32 f_BallAfterimageHead = 0;
static Boolean f_IsBallAfterimageShown = FALSE;
static s32 f_BallAfterimageFrameCount = 0;
/* Whether the points are scaled for the close view of the cup */
static Boolean f_IsBallAfterimageZoomed = FALSE;
/* Set for the few afterimages that fade quickly */
static Boolean f_IsBallAfterimageFast = FALSE;
static s32 f_BallAfterimageLength = AFTERIMAGE_MAX_LENGTH;
/* f_BallRollFrame cycles through BALL_ROLL_FRAME_COUNT frames, this through
 * nine unused ones */
static s32 f_UnusedBallFrame = 0;
static s32 f_BallRollFrame = 0;

/* Left empty in the original, and nothing calls it */
void UnusedGolfBallStub(void) {}

/* Where the ball of the player whose turn it is was last drawn */
static BallScreenPoint f_CurrentBallScreenPoint;
static u8* f_BallShadowImage;
static u8* f_RollingBallImage;
static u8* f_BallSparkleImage;
static u8* f_BallMarkerImage;
static u8* f_BallAfterimageImage;

/* Read by no routine */
static u8 f_UnusedBytesAfterAfterimageImage[16] UNUSED;

static u8 f_BallImageHeap[BALL_IMAGE_HEAP_SIZE];
/* "PIKA" in 4-bit texels, four to a u16 */
static u16 f_PikaBallTexture[PIKA_TEXTURE_WIDTH * PIKA_TEXTURE_HEIGHT / 4];
/* How the ball has turned as it rolled */
static Mat4 f_BallRotation;
static s32 f_GolfBallUnk100;
/* The ball's position when last rolled */
static s32 f_LastRolledBallPosition[3];

void DrawPikaBallTexture(void) {
  DrawTextIntoTexture("PIKA", 4, f_PikaBallTexture);
}

void LoadGolfBallImages(void) {
  EncodedAsset asset;

  ResetHeap(HEAP_BALL_IMAGES);
  AddHeapRegion(HEAP_BALL_IMAGES, f_BallImageHeap,
      f_BallImageHeap + BALL_IMAGE_HEAP_SIZE);
  f_BallShadowImage =
      AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x4C7, &asset));
  ReadEncodedAsset(f_BallShadowImage, &asset);
  f_RollingBallImage =
      AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x4C8, &asset));
  ReadEncodedAsset(f_RollingBallImage, &asset);
  f_BallSparkleImage =
      AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x4DA, &asset));
  ReadEncodedAsset(f_BallSparkleImage, &asset);
  f_BallMarkerImage =
      AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x4C6, &asset));
  ReadEncodedAsset(f_BallMarkerImage, &asset);
  f_BallAfterimageImage =
      AllocFromHeap(HEAP_BALL_IMAGES, OpenEncodedAsset(0x6BB, &asset));
  ReadEncodedAsset(f_BallAfterimageImage, &asset);
  osSyncPrintf("HEAP_BALLTEX LEFT%d\n", GetHeapFreeSize(HEAP_BALL_IMAGES));
  DrawPikaBallTexture();
  ResetGolfBalls();
  ResetBallRotation();
}

/* Turns the ball about the horizontal axis across the way that it moved */
void RollGolfBall(GolfBall* ball UNUSED, BallSimulation* simulation) {
  Mat4 rotation;
  s32 movement[3];
  f32 distance;

  f32 GetLength(f32 x, f32 y, f32 z) {
    return sqrtf(x * x + y * y + z * z);
  }

  if (simulation->state == BALL_STATE_FLYING ||
      simulation->state == BALL_STATE_ROLLING) {
    movement[0] =
        simulation->position[AXIS_X] - f_LastRolledBallPosition[AXIS_X];
    movement[1] =
        simulation->position[AXIS_Y] - f_LastRolledBallPosition[AXIS_Y];
    movement[2] =
        simulation->position[AXIS_Z] - f_LastRolledBallPosition[AXIS_Z];
    f_LastRolledBallPosition[AXIS_X] = simulation->position[AXIS_X];
    f_LastRolledBallPosition[AXIS_Y] = simulation->position[AXIS_Y];
    f_LastRolledBallPosition[AXIS_Z] = simulation->position[AXIS_Z];
  } else {
    movement[0] = simulation->velocity[0];
    movement[1] = simulation->velocity[1];
    movement[2] = simulation->velocity[2];
  }

  if (simulation->state != BALL_STATE_STOPPED) {
    distance = GetLength(movement[0], movement[1], movement[2]);
    if (movement[2] != 0 || movement[0] != 0) {
      guRotateF(rotation, distance * BALL_ROLL_DEGREES_PER_UNIT, -movement[2],
          0.0f, movement[0]);
      guMtxCatF(f_BallRotation, rotation, f_BallRotation);
    }
  }
}

void ResetBallAfterimage(void) {
  s32 i;

  for (i = 0; i != AFTERIMAGE_MAX_LENGTH; i++) {
    f_BallAfterimages[i].alpha = AFTERIMAGE_UNUSED;
    f_BallAfterimages[i].isInCup = FALSE;
  }

  f_BallAfterimages[0].position[AXIS_X] =
      f_GolfBalls[g_GameState.currentPlayerIndex].position[AXIS_X];
  f_BallAfterimages[0].position[AXIS_Y] =
      f_GolfBalls[g_GameState.currentPlayerIndex].position[AXIS_Y];
  f_BallAfterimages[0].position[AXIS_Z] =
      f_GolfBalls[g_GameState.currentPlayerIndex].position[AXIS_Z];
  /* The head's previous point wraps to the last one while the afterimage has
   * its full length */
  f_BallAfterimages[AFTERIMAGE_MAX_LENGTH - 1].position[AXIS_X] =
      f_GolfBalls[g_GameState.currentPlayerIndex].position[AXIS_X];
  f_BallAfterimages[AFTERIMAGE_MAX_LENGTH - 1].position[AXIS_Y] =
      f_GolfBalls[g_GameState.currentPlayerIndex].position[AXIS_Y];
  f_BallAfterimageHead = 0;
  f_IsBallAfterimageShown = FALSE;
  f_BallAfterimageFrameCount = 0;
  f_IsBallAfterimageZoomed = FALSE;
  f_BallAfterimages[AFTERIMAGE_MAX_LENGTH - 1].position[AXIS_Z] =
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
  s32 i;

  for (i = 0; i < GOLF_BALL_COUNT; i++) {
    f_GolfBalls[i].alpha = BALL_HIDDEN;
    f_GolfBalls[i].state = BALL_STATE_STOPPED;
  }

  ResetBallAfterimage();
  ResetBallRotation();
  f_GolfBallUnk100 = 0;
}

/*
 * Updates each player's ball for the frame: hides the balls of players out
 * of the hole, places the current player's ball from the ball simulations,
 * and moves and fades the ball's afterimage.
 */
void UpdateGolfBalls(void) {
  s32 currentIndex;
  s32 i;
  Player* player;
  GolfBall* ball;
  BallSimulation* simulation;
  BallAfterimage* head;
  BallAfterimage* afterimage;
  BallAfterimage* pulledAfterimage;
  BallAfterimage* previous;
  Mat4 scaling;
  Mat4 rotated;
  Mat4 translation;
  f32 scale;
  f32 distance;

  if (g_GameState.step == GAME_STATE_UNK6) {
    currentIndex = g_GameState.currentPlayerIndex;
  } else {
    currentIndex = 0;
  }

  for (i = 0; i < g_GameState.playerCount; i++) {
    GolfBall* ball = &f_GolfBalls[i];

    player = &g_Players[i];
    if (player->holeStatus != HOLE_STATUS_PLAYING || player->strokeCount == 0) {
      ball->alpha = BALL_HIDDEN;
    }

    if (currentIndex != i) {
      ball->state = BALL_STATE_STOPPED;
      if (GetSurfaceInfo(player->ballSurface)->kind != SURFACE_KIND_GREEN &&
          !g_GameState.isMiniGolf) {
        ball->markerPlayerIndex = BALL_NO_MARKER;
      } else {
        ball->markerPlayerIndex = i;
      }
    }
  }

  if (g_GameState.step == GAME_STATE_UNK6 ||
      g_GameState.step == GAME_STATE_UNK11) {
    GolfBall* ball;

    for (i = 0, ball = &f_GolfBalls[currentIndex]; i < BALL_SIMULATION_COUNT;
        i++) {
      simulation = &g_BallSimulations[i];
      ball->isInCup = FALSE;
      if (simulation->playerIndex >= 0 &&
          g_Players[currentIndex].holeStatus == HOLE_STATUS_PLAYING) {
        if (g_IsCupViewShown == TRUE) {
          ball->position[AXIS_X] = (simulation->position[AXIS_X] -
                                       (f32)(g_HoleSetup.cupPosition[AXIS_X] -
                                           simulation->position[AXIS_X])) *
              COURSE_TO_WORLD_SCALE;
          ball->position[AXIS_Y] = (simulation->position[AXIS_Y] -
                                       (f32)(g_HoleSetup.cupPosition[AXIS_Y] -
                                           simulation->position[AXIS_Y])) *
              COURSE_TO_WORLD_SCALE;
          ball->position[AXIS_Z] = (simulation->position[AXIS_Z] -
                                       (f32)(g_HoleSetup.cupPosition[AXIS_Z] -
                                           simulation->position[AXIS_Z])) *
              COURSE_TO_WORLD_SCALE;
        } else {
          ball->position[AXIS_X] =
              simulation->position[AXIS_X] * COURSE_TO_WORLD_SCALE;
          ball->position[AXIS_Y] =
              simulation->position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
          ball->position[AXIS_Z] =
              simulation->position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        }

        ball->alpha = BALL_SHOWN;
        ball->surface = GetSurfaceInfo(simulation->surface);
        if (simulation->state == BALL_STATE_IN_CUP) {
          ball->isInCup = TRUE;
        } else {
          ball->isInCup = FALSE;
        }

        if (GetCupDistance(
                simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
                simulation->position[AXIS_Z] -
                    g_HoleSetup.cupPosition[AXIS_Z]) == 0 ||
            simulation->state == BALL_STATE_IN_CUP ||
            simulation->isHoledOut != 0) {
          ball->isInCup = TRUE;
        } else {
          ball->isInCup = FALSE;
        }

        RollGolfBall(ball, simulation);
        ball->state = simulation->state;
        ball->unk9E = simulation->hasEnteredWater;
        ball->purpose = simulation->purpose;
        if (currentIndex == simulation->playerIndex ||
            (GetSurfaceInfo(simulation->surface)->kind != SURFACE_KIND_GREEN &&
                !g_GameState.isMiniGolf)) {
          ball->markerPlayerIndex = BALL_NO_MARKER;
        } else {
          ball->markerPlayerIndex = simulation->playerIndex;
        }

        scale = g_PinVertexScale * BALL_MODEL_SCALE;
        guScaleF(scaling, scale, scale, scale);
        guMtxCatF(f_BallRotation, scaling, rotated);
        guTranslateF(translation, ball->position[AXIS_X],
            ball->position[AXIS_Y] -
                ((g_PinVertexScale * BALL_RADIUS) / 5120.0),
            ball->position[AXIS_Z]);
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
            s32 coursePosition[3] UNUSED;

            coursePosition[AXIS_X] = afterimage->position[AXIS_X] * 1024.0f;
            coursePosition[AXIS_Y] = afterimage->position[AXIS_Y] * 1024.0f;
            coursePosition[AXIS_Z] = afterimage->position[AXIS_Z] * 1024.0f;
            afterimage->position[AXIS_X] += afterimage->position[AXIS_X] -
                (g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE);
            afterimage->position[AXIS_Y] += afterimage->position[AXIS_Y] -
                (g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE);
            afterimage->position[AXIS_Z] += afterimage->position[AXIS_Z] -
                (g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE);
          }
        }
      }
    } else if (f_IsBallAfterimageZoomed == TRUE) {
      f_IsBallAfterimageZoomed = FALSE;
      for (i = 0; i != f_BallAfterimageLength; i++) {
        pulledAfterimage = &f_BallAfterimages[i];
        if (pulledAfterimage->alpha != AFTERIMAGE_UNUSED) {
          pulledAfterimage->position[AXIS_X] -=
              (pulledAfterimage->position[AXIS_X] -
                  (g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE)) *
              0.5f;
          pulledAfterimage->position[AXIS_Y] -=
              (pulledAfterimage->position[AXIS_Y] -
                  (g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE)) *
              0.5f;
          pulledAfterimage->position[AXIS_Z] -=
              (pulledAfterimage->position[AXIS_Z] -
                  (g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE)) *
              0.5f;
        }
      }
    }

    head = &f_BallAfterimages[f_BallAfterimageHead];
    ball = &f_GolfBalls[currentIndex];
    if (f_BallAfterimageHead == 0) {
      previous = &f_BallAfterimages[f_BallAfterimageLength - 1];
    } else {
      previous = &f_BallAfterimages[f_BallAfterimageHead - 1];
    }
    {
      f32 offset[3];

      offset[0] = ball->position[AXIS_X] - previous->position[AXIS_X];
      offset[1] = ball->position[AXIS_Y] - previous->position[AXIS_Y];
      offset[2] = ball->position[AXIS_Z] - previous->position[AXIS_Z];
      distance = sqrtf((offset[0] * offset[0]) + (offset[1] * offset[1]) +
          (offset[2] * offset[2]));
    }

    if (f_IsBallAfterimageFast == TRUE) {
      if ((f32)(g_PinVertexScale * BALL_RADIUS) * 0.00009765625f < distance &&
          g_BallSimulations[0].state == BALL_STATE_ROLLING) {
        head->alpha = AFTERIMAGE_FAST_ALPHA;
        head->position[AXIS_X] = ball->position[AXIS_X];
        head->position[AXIS_Y] = ball->position[AXIS_Y];
        head->position[AXIS_Z] = ball->position[AXIS_Z];
        if (ball->isInCup == TRUE) {
          head->isInCup = TRUE;
        } else {
          head->isInCup = FALSE;
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
    } else {
      if ((f32)(g_PinVertexScale * BALL_RADIUS) * 0.0005859375f < distance) {
        head->alpha = AFTERIMAGE_ALPHA;
        head->position[AXIS_X] = ball->position[AXIS_X];
        head->position[AXIS_Y] = ball->position[AXIS_Y];
        head->position[AXIS_Z] = ball->position[AXIS_Z];
        if (ball->isInCup == TRUE) {
          head->isInCup = ball->isInCup;
        } else {
          head->isInCup = FALSE;
        }

        f_BallAfterimageHead++;
        if (f_BallAfterimageHead == f_BallAfterimageLength) {
          f_BallAfterimageHead = 0;
        }
      }

      if (f_BallAfterimageFrameCount % AFTERIMAGE_SLOW_FADE_PERIOD == 0) {
        for (i = 0; i != f_BallAfterimageLength; i++) {
          /* -= rather than --, which loop.c reduces with another address */
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
  s32 i;

  for (i = 0; i != GOLF_BALL_COUNT; i++) {
    if (f_GolfBalls[i].isInCup == TRUE && f_GolfBalls[i].alpha != BALL_HIDDEN) {
      isInCup = TRUE;
    }
  }

  return isInCup;
}

/* The rolling ball turns through this many frames, each shown for four draws */
#define BALL_ROLL_FRAME_COUNT 16
#define BALL_ROLL_FRAME_SIZE 0x400
/* The tiles that the ball images are loaded into */
#define BALL_TILE_ROLLING G_TX_RENDERTILE
#define BALL_TILE_STILL 1
#define BALL_TILE_SHADOW 2
#define BALL_TILE_MARKER 3
#define BALL_TILE_SMALL 4
#define BALL_TILE_AFTERIMAGE 5
#define BALL_TEXTURE_SIZE 32
#define SMALL_BALL_TEXTURE_SIZE 8

/* How a ball sits in its surface, from SurfaceInfo.unk14 */
enum BallLie {
  BALL_BURIED = -1,
  BALL_HALF_SUNK,
  BALL_EXPOSED
};

/* How each ball was drawn, which decides whether it gets a shadow */
enum BallSprite {
  BALL_SPRITE_FULL,
  BALL_SPRITE_SMALL,
  BALL_SPRITE_DOT,
  BALL_SPRITE_FAR_DOT,
  BALL_SPRITE_DOT_SHADOWED,
  BALL_SPRITE_FAR_DOT_SHADOWED
};

/* Balls nearer than this are not drawn */
#define BALL_NEAREST_DEPTH (-5)
#define BALL_FULL_SPRITE_DEPTH (-86)
#define BALL_NEAR_DEPTH (-200)
#define BALL_DOT_DEPTH (-40000)
#define BALL_DEPTH_BIAS 60.0f
/* The pika balls are drawn as models only while the current ball is nearer than
 * this */
#define PIKA_BALL_DEPTH 20000

/* Like G_RM_ZB_XLU_SURF and G_RM_AA_ZB_XLU_SURF, but also updating the depth
 * buffer */
#define RM_ZB_XLU_SURF_UPDATE(clk) \
  (Z_CMP | Z_UPD | IM_RD | CVG_DST_FULL | ZMODE_XLU | FORCE_BL | \
      GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA))
#define RM_AA_ZB_XLU_SURF_UPDATE(clk) \
  (AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | \
      FORCE_BL | GBL_c##clk(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA))

/* The texture coordinate step that stretches texelCount texels from from to to,
 * in quarter pixels */
#define TexelStep(texelCount, from, to) \
  ((to) - (from) != 0 ? ((((texelCount) << 13) / ((to) - (from))) + 1) >> 1 : 0)
#define gSPStretchedTextureRectangle( \
    pkt, xl, yl, xh, yh, tile, s, t, width, height) \
  gSPScisTextureRectangle(pkt, xl, yl, xh, yh, tile, s, t, \
      TexelStep(width, xl, xh), TexelStep(height, yl, yh))

/*
 * Doesn't match: the original's reload spills the divide by 9's constant first,
 * to 0x54($sp), and the depth in pseudo 87 later, so its spill slots come in
 * another order, its frame is 8 bytes larger and the display list pointers take
 * other registers. The sprite branches share one sync after the chain, which
 * the original reaches by jumping from each branch; with a sync ending each
 * branch instead, which emits the same commands, 911 words differed, 137 in
 * shape. Before that change, updating f_UnusedBallFrame ahead of the display
 * list, rather than after f_BallRollFrame, cut 1220 words differing to 834, and
 * updating f_BallRollFrame anywhere else gave 822 to 1633 words, 158 or more in
 * shape. The original aligns the Pika ball texture's address to 8 bytes, as
 * TextureAddress does; without that, 894 words differed, 152 in shape. The
 * afterimages' loop tests i != f_BallAfterimageLength, as the original's bne
 * against the reloaded length shows; < compiles to slt and bnez. The register
 * oracle's closest result has no register forced, so the code itself differs.
 * 654 words differ, 90 in shape.
 */
#ifdef NON_MATCHING
void DrawGolfBalls(CameraMatrices* matrices UNUSED) {
  Gfx* gfx = g_DisplayListHead;
  ScreenPoint ballPoint;
  ScreenPoint shadowPoint;
  s32 i;
  Boolean isCurrentBallAboveGround;
  GolfBall* ball;
  BallAfterimage* afterimage;
  SurfaceInfo* surface;
  s32 groundHeight;
  s32 spriteKind;
  Boolean isAboveGround;
  s32 exposure;
  Boolean isAboveCup;
  s32 ballDepth;
  s32 afterimageProjectedDepth;
  s32 shadowDepth;
  s32 primDepth;
  s32 shadowPrimDepth;
  s32 size;
  s32 shade;
  s32 halfWidth;
  s32 halfHeight;
  u32 afterimageDepth;

  /*
   * A shadow stays behind the last ball drawn, which the original leaves
   * unset until a ball is drawn
   */
#ifdef AVOID_UB
  primDepth = 0;
#endif
  if (g_GameState.step != GAME_STATE_UNK6 &&
      g_GameState.step != GAME_STATE_UNK11) {
    return;
  }

  f_UnusedBallFrame = (f_UnusedBallFrame + 1) % 9;
  g_ModuleRdpRequestFrames[MODULE_GOLF_BALLS] = g_FrameCount;
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
  f_BallRollFrame = (f_BallRollFrame + 1) & (BALL_ROLL_FRAME_COUNT - 1);
  gDPLoadTextureBlock(gfx++,
      TextureAddress(ImageBodyAt(
          f_RollingBallImage, f_BallRollFrame / 4 * BALL_ROLL_FRAME_SIZE)),
      G_IM_FMT_I, G_IM_SIZ_8b, BALL_TEXTURE_SIZE, BALL_TEXTURE_SIZE, 0,
      G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPLoadMultiBlock(gfx++, TextureAddress(ImageBody(f_RollingBallImage)), 0x80,
      BALL_TILE_STILL, G_IM_FMT_I, G_IM_SIZ_8b, BALL_TEXTURE_SIZE,
      BALL_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_CLAMP,
      G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
  gDPLoadMultiBlock_4b(gfx++, TextureAddress(ImageBody(f_BallSparkleImage)),
      0x100, BALL_TILE_SHADOW, G_IM_FMT_I, 16, 16, 0, G_TX_MIRROR | G_TX_WRAP,
      G_TX_MIRROR | G_TX_WRAP, 4, 4, G_TX_NOLOD, G_TX_NOLOD);
  gDPLoadMultiBlock_4b(gfx++, TextureAddress(ImageBody(f_BallMarkerImage)),
      0x140, BALL_TILE_MARKER, G_IM_FMT_I, 16, 16, 0,
      G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPLoadMultiBlock_4b(gfx++, TextureAddress(ImageBody(f_BallShadowImage)),
      0x150, BALL_TILE_SMALL, G_IM_FMT_I, 16, 16, 0, G_TX_NOMIRROR | G_TX_CLAMP,
      G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
  gDPLoadMultiBlock_4b(gfx++, TextureAddress(ImageBody(f_BallAfterimageImage)),
      0x190, BALL_TILE_AFTERIMAGE, G_IM_FMT_I, 16, 64, 0,
      G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  isCurrentBallAboveGround = FALSE;
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_AVERAGE);
  gDPPipeSync(gfx++);
  for (i = 0; i < GOLF_BALL_COUNT; i++) {
    ball = &f_GolfBalls[i];
    if (ball->alpha > 0) {
      surface = ball->surface;
      groundHeight =
          GetGroundHeight(ball->position[AXIS_X] * WORLD_TO_COURSE_SCALE,
              ball->position[AXIS_Z] * WORLD_TO_COURSE_SCALE);
      spriteKind = BALL_SPRITE_FULL;
      isAboveGround = TRUE;
      if (ball->position[AXIS_Y] * WORLD_TO_COURSE_SCALE <
          groundHeight - 153.6f) {
        isAboveGround = FALSE;
      }

      if (isAboveGround) {
        if (surface->unk14 == 2) {
          exposure = BALL_BURIED;
        } else if (surface->unk14 == 1) {
          exposure = BALL_HALF_SUNK;
        } else {
          exposure = BALL_EXPOSED;
        }
      } else {
        exposure = BALL_EXPOSED;
      }

      isAboveCup = FALSE;
      if (groundHeight + 768.0f <
          ball->position[AXIS_Y] * WORLD_TO_COURSE_SCALE) {
        isAboveCup = ball->isInCup == TRUE;
      }

      ballDepth = ProjectPoint(ball->position[AXIS_X],
          ball->position[AXIS_Y] - (g_PinVertexScale * 0.3f),
          ball->position[AXIS_Z], &ballPoint);
      shadowDepth = ProjectPoint(ball->position[AXIS_X],
          (groundHeight * COURSE_TO_WORLD_SCALE) - (g_PinVertexScale * 0.06f),
          ball->position[AXIS_Z], &shadowPoint);
      if (ball->markerPlayerIndex == BALL_NO_MARKER) {
        if (ballDepth < BALL_NEAREST_DEPTH && exposure >= BALL_HALF_SUNK) {
          gDPPipeSync(gfx++);
          if (ball->unk9E) {
            groundHeight = GetGroundOrWaterHeight(
                ball->position[AXIS_X] * WORLD_TO_COURSE_SCALE,
                ball->position[AXIS_Z] * WORLD_TO_COURSE_SCALE);
            shade = 100 -
                (s32)(((ball->position[AXIS_Y] * WORLD_TO_COURSE_SCALE) -
                          groundHeight) *
                    0.0032552085f);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, RM_AA_ZB_XLU_SURF_UPDATE(1),
                RM_AA_ZB_XLU_SURF_UPDATE(2));
            if (shade < 0) {
              shade = 0;
            }

            gDPSetPrimColor(gfx++, 0, 0, 0xEF, 0xEF, 0xFF, shade);
            gDPPipeSync(gfx++);
            gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
          } else {
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
            gDPSetPrimColor(gfx++, 0, 0, 0xEF, 0xEF, 0xFF, 0xFF);
          }

          if (g_IsCupViewShown == TRUE) {
            if (ballDepth < g_CupViewDepth) {
              primDepth = 0x402F;
            } else {
              primDepth = isAboveCup ? 0x401F : 0x4007;
            }
          } else if (ball->purpose == LAUNCH_PURPOSE_UNK3) {
            primDepth = ballPoint.depth;
          } else if (ballDepth < g_CupViewDepth) {
            if (isAboveCup == TRUE) {
              continue;
            }

            if (ballDepth < BALL_NEAR_DEPTH) {
              primDepth = ballPoint.depth;
            } else {
              primDepth = ballPoint.depth -
                  (ballPoint.depthStep * (10.5f * g_PinVertexScale)) + 48.0f -
                  BALL_DEPTH_BIAS;
            }
          } else {
            if (isAboveCup == TRUE) {
              continue;
            }

            if (ballDepth < BALL_NEAR_DEPTH) {
              primDepth = ballPoint.depth;
            } else {
              primDepth = ballPoint.depth -
                  (ballPoint.depthStep * (10.5f * g_PinVertexScale)) + 8.0f -
                  BALL_DEPTH_BIAS;
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
          size = (s32)(AbsFloat(ballPoint.scaleX) *
              (0.6f * (2.0f * g_PinVertexScale)) * 0.25f);
          if (size <= 0) {
            size = 1;
          }

          if (g_IsCupViewShown == TRUE || ballDepth >= BALL_FULL_SPRITE_DEPTH) {
            spriteKind = BALL_SPRITE_FULL;
            if (ball->state == BALL_STATE_STOPPED) {
              gSPStretchedTextureRectangle(gfx++,
                  QuarterPixels(ballPoint.x - size),
                  QuarterPixels(ballPoint.y - size),
                  QuarterPixels(ballPoint.x + size),
                  QuarterPixels(ballPoint.y + (exposure * size)),
                  BALL_TILE_STILL, 0, 0, BALL_TEXTURE_SIZE,
                  (BALL_TEXTURE_SIZE / 2) +
                      (exposure * (BALL_TEXTURE_SIZE / 2)));
            } else {
              gSPStretchedTextureRectangle(gfx++,
                  QuarterPixels(ballPoint.x - size),
                  QuarterPixels(ballPoint.y - size),
                  QuarterPixels(ballPoint.x + size),
                  QuarterPixels(ballPoint.y + (exposure * size)),
                  BALL_TILE_ROLLING, 0, 0, BALL_TEXTURE_SIZE,
                  (BALL_TEXTURE_SIZE / 2) +
                      (exposure * (BALL_TEXTURE_SIZE / 2)));
            }
          } else if (ballDepth >= BALL_FULL_SPRITE_DEPTH) {
            spriteKind = BALL_SPRITE_SMALL;
            gSPStretchedTextureRectangle(gfx++,
                QuarterPixels(ballPoint.x - size),
                QuarterPixels(ballPoint.y - size),
                QuarterPixels(ballPoint.x + size),
                QuarterPixels(ballPoint.y + (exposure * size)), BALL_TILE_SMALL,
                0, 0, SMALL_BALL_TEXTURE_SIZE,
                (SMALL_BALL_TEXTURE_SIZE / 2) +
                    (exposure * (SMALL_BALL_TEXTURE_SIZE / 2)));
          } else if (ballDepth > BALL_DOT_DEPTH) {
            gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            gDPPipeSync(gfx++);
            gSPStretchedTextureRectangle(gfx++, QuarterPixels(ballPoint.x - 1),
                QuarterPixels(ballPoint.y - 1), QuarterPixels(ballPoint.x + 1),
                QuarterPixels(ballPoint.y), BALL_TILE_SMALL, 0, 0,
                SMALL_BALL_TEXTURE_SIZE, SMALL_BALL_TEXTURE_SIZE);
            gDPPipeSync(gfx++);
            gDPSetPrimColor(gfx++, 0, 0, 0xBF, 0xBF, 0xBF, 0xFF);
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
            gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 0x3F);
            gDPPipeSync(gfx++);
            if (isAboveGround == TRUE) {
              spriteKind = BALL_SPRITE_DOT_SHADOWED;
              gSPStretchedTextureRectangle(gfx++,
                  QuarterPixels(ballPoint.x - 1),
                  QuarterPixels(ballPoint.y + 1),
                  QuarterPixels(ballPoint.x + 1),
                  QuarterPixels(ballPoint.y + 2), BALL_TILE_SMALL, 0, 0,
                  SMALL_BALL_TEXTURE_SIZE, SMALL_BALL_TEXTURE_SIZE);
            } else {
              spriteKind = BALL_SPRITE_DOT;
            }
          } else {
            gDPPipeSync(gfx++);
            gDPSetRenderMode(
                gfx++, RM_ZB_XLU_SURF_UPDATE(1), RM_ZB_XLU_SURF_UPDATE(2));
            gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(gfx++, (ballPoint.x << 2) - 4,
                (ballPoint.y << 2) - 8, QuarterPixels(ballPoint.x),
                QuarterPixels(ballPoint.y), BALL_TILE_SMALL, 0, 0,
                TexelStep(SMALL_BALL_TEXTURE_SIZE, (ballPoint.x << 2) - 4,
                    ballPoint.x << 2),
                TexelStep(SMALL_BALL_TEXTURE_SIZE, (ballPoint.y - 1) << 2,
                    (ballPoint.y + 1) << 2));
            gDPPipeSync(gfx++);
            gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 0xFF);
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

        if (exposure == BALL_EXPOSED && shadowDepth < BALL_NEAREST_DEPTH &&
            !isAboveCup && !ball->unk9E) {
          size = (s32)(AbsFloat(shadowPoint.scaleX) *
              (0.6f * (2.0f * g_PinVertexScale)) * 0.25f);
          if (g_IsCupViewShown == TRUE) {
            if (shadowDepth < g_CupViewDepth) {
              shadowPrimDepth = 0x4033;
            } else if (ball->isInCup == TRUE) {
              shadowPrimDepth = 0x4023;
            } else {
              shadowPrimDepth = 0x400B;
            }

            shade = 0x7F -
                ((s32)(groundHeight -
                     (ball->position[AXIS_Y] * WORLD_TO_COURSE_SCALE)) /
                    128);
            if (shade < 0x3F) {
              shade = 0x3F;
            }
          } else {
            if (ball->purpose == LAUNCH_PURPOSE_UNK3) {
              shadowPrimDepth = ballPoint.depth;
            } else {
              f32 depth;

              if (shadowDepth < g_CupViewDepth) {
                depth = shadowPoint.depth -
                    (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) +
                    56.0f;
              } else if (ball->isInCup == TRUE) {
                depth = shadowPoint.depth -
                    (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) +
                    40.0f;
              } else {
                depth = shadowPoint.depth -
                    (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) +
                    16.0f;
              }

              shadowPrimDepth = depth - BALL_DEPTH_BIAS;
            }

            shade = 0x7F -
                ((s32)(groundHeight -
                     (ball->position[AXIS_Y] * WORLD_TO_COURSE_SCALE)) /
                    512);
            if (shade < 0x5F) {
              shade = 0x5F;
            }
          }

          if (shade > 0x7F) {
            shade = 0x7F;
          }

          size = size * (shade * 0.007874016f);
          if (size <= 0) {
            size = 1;
          }

          shadowPrimDepth &= ~3;
          if (shadowPrimDepth < 0) {
            shadowPrimDepth = 0;
          }

          if (shadowPrimDepth < primDepth + 8) {
            shadowPrimDepth = primDepth + 8;
          }

          gDPSetPrimDepth(gfx++, shadowPrimDepth, 0);
          gDPPipeSync(gfx++);
          gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, shade);
          if (spriteKind < BALL_SPRITE_DOT_SHADOWED) {
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
            gDPPipeSync(gfx++);
            gSPStretchedTextureRectangle(gfx++,
                QuarterPixels(shadowPoint.x - size),
                QuarterPixels(shadowPoint.y - size),
                QuarterPixels(shadowPoint.x + size),
                QuarterPixels(shadowPoint.y + size), BALL_TILE_SHADOW, 0, 0,
                BALL_TEXTURE_SIZE, BALL_TEXTURE_SIZE);
          }
        }
      } else if (shadowDepth < BALL_NEAREST_DEPTH) {
        f32 depth;

        size = AbsFloat(shadowPoint.scaleX) *
            (0.6f * (2.0f * g_PinVertexScale)) * 0.25f;
        if (g_IsCupViewShown != TRUE) {
          if (shadowDepth < g_CupViewDepth) {
            depth = shadowPoint.depth -
                (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) + 56.0f;
          } else if (ball->isInCup == TRUE) {
            depth = shadowPoint.depth -
                (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) + 40.0f;
          } else {
            depth = shadowPoint.depth -
                (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) + 16.0f;
          }

          shadowPrimDepth = depth - BALL_DEPTH_BIAS;
          if (size <= 0) {
            size = 1;
          }

          gDPSetPrimDepth(gfx++, shadowPrimDepth & ~3, 0);
          gDPPipeSync(gfx++);
          switch (ball->markerPlayerIndex) {
            case -1:
              gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 0);
              break;
            case 0:
              gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0x7F, 0xFF);
              break;
            case 1:
              gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0x7F, 0xFF, 0xFF);
              break;
            case 2:
              gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0xFF, 0xFF);
              break;
            case 3:
              gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xBF, 0x7F, 0xFF);
              break;
          }

          gDPPipeSync(gfx++);
          gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
          gDPPipeSync(gfx++);
          gSPStretchedTextureRectangle(gfx++, QuarterPixels(ballPoint.x - size),
              QuarterPixels(ballPoint.y - size),
              QuarterPixels(ballPoint.x + size),
              QuarterPixels(ballPoint.y + size), BALL_TILE_MARKER, 0, 0, 16,
              16);
        }
      }
    }
  }

  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2);
  gDPSetCombineLERP(gfx++, PRIMITIVE, 0, TEXEL0, ENVIRONMENT, PRIMITIVE, 0,
      TEXEL0, 0, PRIMITIVE, 0, TEXEL0, ENVIRONMENT, PRIMITIVE, 0, TEXEL0, 0);
  gDPSetEnvColor(gfx++, 0x00, 0x00, 0x80, 0x1E);
  for (i = 0; i != f_BallAfterimageLength; i++) {
    BallAfterimage* afterimage = &f_BallAfterimages[i];

    if (afterimage->alpha > 0) {
      afterimageProjectedDepth = ProjectPoint(afterimage->position[AXIS_X],
          afterimage->position[AXIS_Y] - (g_PinVertexScale * 0.27f),
          afterimage->position[AXIS_Z], &ballPoint);
      if (afterimageProjectedDepth < BALL_NEAREST_DEPTH) {
        if (g_IsCupViewShown == TRUE) {
          afterimageDepth = 0x402F;
          if (afterimageProjectedDepth >= g_CupViewDepth) {
            afterimageDepth = afterimage->isInCup == TRUE ? 0x401F : 0x4007;
          }

          gDPSetPrimColor(gfx++, 0, 0, 0xA0, 0xA0, 0xFF, afterimage->alpha / 2);
        } else {
          if (afterimageProjectedDepth < g_CupViewDepth) {
            afterimageDepth = ballPoint.depth -
                (ballPoint.depthStep * (10.5f * g_PinVertexScale)) + 48.0f -
                BALL_DEPTH_BIAS;
          } else if (afterimage->isInCup == FALSE) {
            afterimageDepth = ballPoint.depth -
                (ballPoint.depthStep * (10.5f * g_PinVertexScale)) + 8.0f -
                BALL_DEPTH_BIAS;
          } else {
            continue;
          }

          if (f_IsBallAfterimageFast == TRUE) {
            gDPSetPrimColor(
                gfx++, 0, 0, 0xA0, 0xA0, 0xFF, afterimage->alpha / 2);
          } else {
            gDPSetPrimColor(
                gfx++, 0, 0, 0xFF, 0xFF, 0xFF, afterimage->alpha / 2);
          }
        }

        gDPSetPrimDepth(gfx++, afterimageDepth & ~3, 0);
        gDPPipeSync(gfx++);
        halfWidth = (AbsFloat(ballPoint.scaleX) *
                        ((0.3 / 1024) * (g_PinVertexScale * BALL_RADIUS * 2))) -
            1.0;
        halfHeight =
            (AbsFloat(ballPoint.scaleY) *
                ((0.3 / 1024) * (g_PinVertexScale * BALL_RADIUS * 2))) -
            1.0;
        if (halfWidth < 4) {
          halfWidth = 4;
        }

        if (halfHeight < 4) {
          halfHeight = 4;
        }

        if (f_IsBallAfterimageFast == TRUE) {
          gSPStretchedTextureRectangle(gfx++,
              (ballPoint.x << 2) - halfWidth - 1,
              (ballPoint.y << 2) - halfHeight - 1,
              (ballPoint.x << 2) + halfWidth + 2,
              (ballPoint.y << 2) + halfHeight + 2, BALL_TILE_AFTERIMAGE, 0,
              (guRandom() % 3) << 9, 15, 15);
        } else {
          gSPStretchedTextureRectangle(gfx++,
              (ballPoint.x << 2) - halfWidth - 1,
              (ballPoint.y << 2) - halfHeight - 1,
              (ballPoint.x << 2) + halfWidth + 2,
              (ballPoint.y << 2) + halfHeight + 2, BALL_TILE_STILL, 0, 0,
              BALL_TEXTURE_SIZE, BALL_TEXTURE_SIZE);
        }

        gDPPipeSync(gfx++);
      }
    }
  }

  if (IsGameFlagSet(GAME_FLAG_SHOW_ROUTE_POINTS)) {
    for (i = 0; i != ROUTE_POINT_COUNT; i++) {
      afterimage = &f_BallAfterimages[i];
      shadowDepth = ProjectPoint(
          g_HolePoints.routePoints[i][0] * COURSE_TO_WORLD_SCALE,
          (g_HolePoints.routePoints[i][1] * COURSE_TO_WORLD_SCALE) -
              (g_PinVertexScale * 0.27f),
          g_HolePoints.routePoints[i][2] * COURSE_TO_WORLD_SCALE, &shadowPoint);
      if (shadowDepth < BALL_NEAREST_DEPTH) {
        if (shadowDepth < g_CupViewDepth) {
          afterimageDepth = shadowPoint.depth -
              (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) + 48.0f -
              BALL_DEPTH_BIAS;
        } else if (afterimage->isInCup == FALSE) {
          afterimageDepth = shadowPoint.depth -
              (shadowPoint.depthStep * (10.5f * g_PinVertexScale)) + 8.0f -
              BALL_DEPTH_BIAS;
        } else {
          continue;
        }

        if (f_IsBallAfterimageFast == TRUE) {
          gDPSetPrimColor(gfx++, 0, 0, 0xA0, 0xA0, 0xFF, afterimage->alpha / 2);
        } else {
          gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, afterimage->alpha / 2);
        }

        gDPSetPrimDepth(gfx++, afterimageDepth & ~3, 0);
        gDPPipeSync(gfx++);
        halfWidth = (AbsFloat(shadowPoint.scaleX) *
                        ((0.3 / 1024) * (g_PinVertexScale * BALL_RADIUS * 2))) -
            1.0;
        halfHeight =
            (AbsFloat(shadowPoint.scaleY) *
                ((0.3 / 1024) * (g_PinVertexScale * BALL_RADIUS * 2))) -
            1.0;
        if (halfWidth < 4) {
          halfWidth = 4;
        }

        if (halfHeight < 4) {
          halfHeight = 4;
        }

        if (f_IsBallAfterimageFast == TRUE) {
          gSPStretchedTextureRectangle(gfx++,
              (shadowPoint.x << 2) - halfWidth - 1,
              (shadowPoint.y << 2) - halfHeight - 1,
              (shadowPoint.x << 2) + halfWidth + 2,
              (shadowPoint.y << 2) + halfHeight + 2, BALL_TILE_AFTERIMAGE, 0,
              (guRandom() % 3) << 9, 15, 15);
        } else {
          gSPStretchedTextureRectangle(gfx++,
              (shadowPoint.x << 2) - halfWidth - 1,
              (shadowPoint.y << 2) - halfHeight - 1,
              (shadowPoint.x << 2) + halfWidth + 2,
              (shadowPoint.y << 2) + halfHeight + 2, BALL_TILE_STILL, 0, 0,
              BALL_TEXTURE_SIZE, BALL_TEXTURE_SIZE);
        }

        gDPPipeSync(gfx++);
      }
    }
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(
      gfx++, RM_AA_ZB_XLU_SURF_UPDATE(1), RM_AA_ZB_XLU_SURF_UPDATE(2));
  gSPLoadGeometryMode(gfx++, 0);
  gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
  gDPSetCombineLERP(gfx++, 0, 0, 0, SHADE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
      SHADE, PRIMITIVE, 0, TEXEL0, 0);
  gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0x00, 0x60);
  gDPLoadTextureBlock_4b(gfx++, TextureAddress(f_PikaBallTexture), G_IM_FMT_IA,
      PIKA_TEXTURE_WIDTH, PIKA_TEXTURE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_CLAMP,
      G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPSetPrimDepth(gfx++, f_CurrentBallScreenPoint.depth - 4, 0);
  if (f_CurrentBallScreenPoint.depth < PIKA_BALL_DEPTH) {
    for (i = 0; i < GOLF_BALL_COUNT; i++) {
      ball = &f_GolfBalls[i];
      if (ball->markerPlayerIndex == BALL_NO_MARKER &&
          f_GolfBalls[i].alpha == BALL_SHOWN &&
          (g_IsCupViewShown || ball->isInCup != TRUE) &&
          (!isCurrentBallAboveGround || ball->surface->unk14 == 0) &&
          ball->alpha > 0) {
        gDPPipeSync(gfx++);
        gSPMatrix(gfx++, &ball->matrices[g_DrawBufferIndex],
            G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
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

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  gDPPipeSync(gfx++);
  g_DisplayListHead = gfx;
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/golf_balls", DrawGolfBalls);
#endif
