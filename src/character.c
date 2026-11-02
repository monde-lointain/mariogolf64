/*
 * The characters' models: loading them, playing their animations and texture
 * animations, posing, lighting, placing and drawing them. It also measures how
 * far to the side of the ball each character stands for each club.
 */
#include "common.h"

#include "character.h"

#include "camera.h"
#include "character_slot.h"
#include "club.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "debug_text.h"
#include "game.h"
#include "game_flags.h"
#include "heap.h"
#include "hole_loader.h"
#include "host_file.h"
#include "libc.h"
#include "main.h"
#include "math_util.h"
#include "play.h"
#include "reaction.h"
#include "save.h"
#include "screen.h"
#include "shot_camera.h"
#include "shot_effects.h"
#include "terrain.h"

/* Room for a model's files and, FRAME_DATA_OFFSET in, its frame data copies */
#define CHARACTER_BUFFER_SIZE 0x23680
#define FRAME_DATA_OFFSET 0x20000
#define FRAME_DATA_COPY_SIZE 0x780
/* The buffer each slot is given before AssignCharacterBuffers gives it its own
 */
#define DEFAULT_CHARACTER_BUFFER 0x8030F800
/* SetCharacterBuffer's warning is printed this many times, so it stands out */
#define WARNING_REPEAT_COUNT 10
/* Long enough ago that IsCharacterSlotBusy finds a slot not busy */
#define LONG_AGO_FRAME (-4)

static inline CharacterAnimation* ModelAnimations(const CharacterModel* model) {
  u32 address = model->segmentAddresses[CHARACTER_MODEL_ANIMATIONS];

  return (CharacterAnimation*)address;
}

#define ModelJointAnimations(model) \
  ((JointAnimation*)(model)->segmentAddresses[CHARACTER_MODEL_JOINTS])
#define ModelAnimatedTriangles(model) \
  ((AnimatedTriangle**)(model) \
          ->segmentAddresses[CHARACTER_MODEL_ANIMATED_TRIANGLES])
#define ModelTextureAnimations(model) \
  ((TextureAnimation*)(model) \
          ->segmentAddresses[CHARACTER_MODEL_TEXTURE_ANIMATIONS])
#define ModelAnimatedTextures(model) \
  ((AnimatedTexture*)(model)->segmentAddresses[CHARACTER_MODEL_TEXTURES])

/* Where a segment address into the frame data points in the slot's copy being
 * written */
#define FrameDataAddress(slot, address) \
  ((u8*)(slot)->frameData[(slot)->frameDataIndex] + SEGMENT_OFFSET(address))

/*
 * Patches the slot's frame data copy being written so the triangle shows the
 * texture with the texture coordinates. Uses the locals gfx, vertex and i.
 */
#define SetTriangleTexture( \
    slot, triangle, texture, sCoordinates, tCoordinates) \
  gfx = (Gfx*)FrameDataAddress( \
      slot, (triangle)->loadTextureBlock + LOAD_TEXTURE_BLOCK_OFFSET); \
  gDPLoadTextureBlock(gfx++, (texture)->address & ~(TEXTURE_ALIGNMENT - 1), \
      G_IM_FMT_RGBA, G_IM_SIZ_16b, (texture)->width, (texture)->height, 0, \
      G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, \
      ANIMATED_TEXTURE_MASK, ANIMATED_TEXTURE_MASK, G_TX_NOLOD, G_TX_NOLOD); \
  for (i = 0; i < TRIANGLE_VERTEX_COUNT; i++) { \
    vertex = \
        (Vtx*)FrameDataAddress(slot, (triangle)->vertices + (i * VTX_SIZE)); \
    vertex->v.tc[TEXTURE_AXIS_S] = (sCoordinates)[i]; \
    vertex->v.tc[TEXTURE_AXIS_T] = (tCoordinates)[i]; \
  }

/* The size of a Vtx */
#define VTX_SIZE 0x10
/* From AnimatedTriangle.loadTextureBlock to the gDPLoadTextureBlock */
#define LOAD_TEXTURE_BLOCK_OFFSET 0x10
/* The alignment the RDP needs of a texture's address */
#define TEXTURE_ALIGNMENT 8
/* The texture coordinate mask of an animated triangle's tiles, 64 texels each
 * way */
#define ANIMATED_TEXTURE_MASK 6

/* Makes a slot's model readable at CHARACTER_MODEL_ADDRESS, where its segment
 * addresses point */
#define MapModelBuffer(slot) \
  SetMultiTlb(0, TLB_PAGE_MODE_4K, CHARACTER_MODEL_PAGE_COUNT, \
      CHARACTER_MODEL_ADDRESS, osVirtualToPhysical((slot)->modelBuffer))

/* The models with a row of f_BallDistances are the playable characters, below
 * CHARACTER_COUNT */
/* f_BallDistances columns: one per club kind, then this one */
#define BALL_DISTANCE_WOOD_APPROACH 4
#define BALL_DISTANCE_COUNT 5

/* The default ball distance for each club kind */
#define WOOD_BALL_DISTANCE 0x3600
#define IRON_BALL_DISTANCE 0x2400
#define PUTTER_BALL_DISTANCE 0x1800

/* The character points whose depth gives the ball distance for each club kind
 */
#define WOOD_DISTANCE_POINT 3
#define IRON_DISTANCE_POINT 4
#define WEDGE_DISTANCE_POINT 5
#define PUTTER_DISTANCE_POINT 6

/* A ball distance is this times the point's depth in world units */
#define POINT_DEPTH_TO_BALL_DISTANCE 1228.8f

/* The animations whose last frame WriteBallDistances measures for each club
 * kind */
#define WOOD_DISTANCE_ANIMATION 0
#define IRON_DISTANCE_ANIMATION 3
#define WEDGE_DISTANCE_ANIMATION 6
#define PUTTER_DISTANCE_ANIMATION 9
/* The animation whose first frame it measures for the approach column */
#define WOOD_APPROACH_DISTANCE_ANIMATION 2

/* Where GetBallDistance shows the measured point position */
#define POINT_POSITION_TEXT_COLUMN 2
#define POINT_POSITION_TEXT_ROW 13

#define BALL_DISTANCE_SOURCE_SIZE 0x8000
#define BALL_DISTANCE_LINE_SIZE 0x100
/* The display list choices that hold each kind of club */
#define WOOD_DISPLAY_LIST_CHOICE 0
#define IRON_DISPLAY_LIST_CHOICE 1
#define WEDGE_DISPLAY_LIST_CHOICE 2
#define PUTTER_DISPLAY_LIST_CHOICE 3

/* The animation whose impact frame GetImpactEventFrame returns */
#define IMPACT_EVENT_ANIMATION_INDEX 2

/*
 * How far PlaceAndViewCharacter puts the camera from the character, and how
 * far below it the view sits, in course units
 */
#define VIEW_DISTANCE 61440.0f
#define VIEW_DROP 15360.0f

void AdvanceTextureAnimations(
    u32 slotIndex, s32 unused, s32 frameLastDigit, s32 frame);

/* How a slot starts out when LoadCharacter loads it */
#define LOADED_ALPHA 0x60
#define LOADED_RANDOM_TEXTURE_ANIMATION_DELAY 27

/* SetCharacterLights' dry colors near the end of the round */
#define LATE_AMBIENT_LEVEL 120
#define LATE_LIGHT_RED 200
#define LATE_LIGHT_GREEN 80
#define LATE_LIGHT_BLUE 10
/* SetCharacterLights' dry colors through the rest of the round */
#define DAY_AMBIENT_RED 130
#define DAY_AMBIENT_GREEN 130
#define DAY_AMBIENT_BLUE 120
#define DAY_LIGHT_RED 130
#define DAY_LIGHT_GREEN 120
#define DAY_LIGHT_BLUE 110
/* SetCharacterLights' colors in the rain */
#define RAIN_AMBIENT_LEVEL 100
#define RAIN_LIGHT_RED 155
#define RAIN_LIGHT_GREEN 120
#define RAIN_LIGHT_BLUE 120
#define RAIN_SECOND_LIGHT_LEVEL 150

/* The characters are drawn at this scale; the rotations are set up but never
 * drawn */
static Mtx f_ModelScaleMatrix;
static Mtx f_RollMatrix;
static Mtx f_HeadingMatrix;

/* Read by no routine */
static u8 f_UnusedBufferAfterHeadingMatrix[120] UNUSED;

#define MODEL_SCALE 0.01f
#define HALF_TURN_DEGREES 180
#define BLEND_ALPHA 0x80
#define OPAQUE_ALPHA 0xFF
/* The models' translucent mode: like G_RM_ZB_XLU_SURF, but updating the
 * depth buffer */
#define MODEL_RENDER_MODE_TRANSLUCENT (G_RM_ZB_XLU_SURF | Z_UPD)
#define MODEL_RENDER_MODE_TRANSLUCENT2 (G_RM_ZB_XLU_SURF2 | Z_UPD)

/* Written elsewhere */
/* Each slot's matrices, for each draw buffer */
static Mtx f_ModelMatrices[CHARACTER_SLOT_COUNT][DRAW_BUFFER_COUNT];
static Mtx f_ShadowMatrices[CHARACTER_SLOT_COUNT][DRAW_BUFFER_COUNT];
static Mtx f_ShadowProjections[CHARACTER_SLOT_COUNT][DRAW_BUFFER_COUNT];
u8* g_ShadowTexture;
ImportedCharacter g_ImportedCharacters[IMPORTED_CHARACTER_COUNT];

FILE_STATIC s32 g_CurrentAnimationStartFrame;

#define SHADOW_TEXTURE_SIZE 32
#define SHADOW_TEXTURE_SIZE_LOG2 5
/* Half a side of the shadow's square, in model units */
#define SHADOW_QUAD_HALF_SIZE 1500
/* The square's texture coordinates, in s10.5, span the texture and the mirror
 * image its tile wraps to */
#define SHADOW_QUAD_TEXEL_SPAN TexelCoordinate(SHADOW_TEXTURE_SIZE * 2)
#define ShadowTexturePixels(texture) (TextureAddress((u32)(texture) + 8))
/* The camera index DrawCharacterAndShadow always draws for */
#define CAMERA_UNK2 2
#define SECOND_LIGHT_DIRECTION_LENGTH 64.0f
/* How far from the shadow point, in model units, the ground is sampled */
#define SHADOW_CORNER_DISTANCE 30.0f
/* CharacterSlot.shadowScale is in these units */
#define SHADOW_SCALE_UNIT 0.0001f
#define SHADOW_DEPTH_SCALE 0.01f
#define SHADOW_DEPTH_OFFSET 6.0f
#define SHADOW_ALPHA 0x80
#define SHADOW_ALPHA_THRESHOLD 0x10
/* Ground whose normal is nearer vertical than this counts as level */
#define LEVEL_GROUND_NORMAL_Y 0.999f
#define QUARTER_TURN_DEGREES 90.0f

/* The segments the character display lists use */
#define MODEL_BUFFER_SEGMENT 0x02
#define JOINT_MATRICES_SEGMENT 0x03
#define RENDER_MODES_SEGMENT 0x04
#define CULLING_SEGMENT 0x05
#define ENV_COLOR_SEGMENT 0x08
#define HILITE_SEGMENT 0x09
#define RENDER_MODE_SEGMENT 0x0A
#define COMBINE_SEGMENT 0x0B
#define MIRROR_TEXTURE_SEGMENT 0x0C
#define FLAME_TEXTURE_SEGMENT 0x0D
#define BLENDED_TEXTURE_SEGMENT 0x0E

/* The size of a line of debug text */
#define DEBUG_LINE_SIZE 0x80

/* Where AnimateCharacter prints a slot's frame when
 * GAME_FLAG_SHOW_ANIMATION_FRAMES is set */
#define ANIMATION_FRAME_TEXT_COLUMN 10
#define ANIMATION_FRAME_TEXT_FIRST_ROW 5

/* How many steps go by between plays of a slot's random texture animation */
#define RANDOM_TEXTURE_ANIMATION_DELAY_MIN 30
#define RANDOM_TEXTURE_ANIMATION_DELAY_MASK 0x1F

/* The slot that WriteBallDistances loads each character into */
#define MEASURED_SLOT 0
/* The shot mode that WriteBallDistances measures every column with */
#define MEASURED_SHOT_MODE SHOT_MODE_NORMAL

/* The source lines of pcsub.c that ReportFailedAssertion reports */
#define BALL_DISTANCE_SOURCE_ASSERT_LINE 1525
#define TEXTURE_ANIMATION_ASSERT_LINE 1956

s32 g_ImpactFlashState = IMPACT_FLASH_NONE;
s32 g_ImpactFlashLevel = 0;
/* The render modes that f_OpaqueRenderModes branches to */
static Gfx f_SurfaceRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2),
    gsSPEndDisplayList(),
};
static Gfx f_BlendedRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(
        MODEL_RENDER_MODE_TRANSLUCENT, MODEL_RENDER_MODE_TRANSLUCENT2),
    gsSPEndDisplayList(),
};
static Gfx f_DecalRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_AA_ZB_OPA_DECAL, G_RM_AA_ZB_OPA_DECAL2),
    gsSPEndDisplayList(),
};
static Gfx f_TextureEdgeRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2),
    gsSPEndDisplayList(),
};
/* The render modes that f_TranslucentRenderModes branches to, all blended */
static Gfx f_TranslucentSurfaceRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(
        MODEL_RENDER_MODE_TRANSLUCENT, MODEL_RENDER_MODE_TRANSLUCENT2),
    gsSPEndDisplayList(),
};
static Gfx f_TranslucentBlendedRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(
        MODEL_RENDER_MODE_TRANSLUCENT, MODEL_RENDER_MODE_TRANSLUCENT2),
    gsSPEndDisplayList(),
};
static Gfx f_TranslucentDecalRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(
        MODEL_RENDER_MODE_TRANSLUCENT, MODEL_RENDER_MODE_TRANSLUCENT2),
    gsSPEndDisplayList(),
};
static Gfx f_TranslucentTextureEdgeRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(
        MODEL_RENDER_MODE_TRANSLUCENT, MODEL_RENDER_MODE_TRANSLUCENT2),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};
/* Segment branch tables to the render modes; the opaque ones, and the
 * translucent ones */
static Gfx f_OpaqueRenderModes[] = {
    gsSPBranchList(f_SurfaceRenderMode),
    gsSPBranchList(f_DecalRenderMode),
    gsSPBranchList(f_TextureEdgeRenderMode),
    gsSPBranchList(f_BlendedRenderMode),
    gsSPEndDisplayList(),
};
static Gfx f_TranslucentRenderModes[] = {
    gsSPBranchList(f_TranslucentSurfaceRenderMode),
    gsSPBranchList(f_TranslucentDecalRenderMode),
    gsSPBranchList(f_TranslucentTextureEdgeRenderMode),
    gsSPBranchList(f_TranslucentBlendedRenderMode),
    gsSPEndDisplayList(),
};
/* Segment tables that clear or set face culling; front-face culling, and
 * back-face culling */
static Gfx f_FrontCulling[] = {
    gsSPClearGeometryMode(G_CULL_FRONT),
    gsSPEndDisplayList(),
    gsSPSetGeometryMode(G_CULL_FRONT),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};
static Gfx f_BackCulling[] = {
    gsSPClearGeometryMode(G_CULL_BACK),
    gsSPEndDisplayList(),
    gsSPSetGeometryMode(G_CULL_BACK),
    gsSPEndDisplayList(),
};
Gfx g_CharacterRenderModeDisplayList[3] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_SURF2),
    gsSPEndDisplayList(),
};
Gfx g_CharacterCombineDisplayList[2] = {
    gsDPSetCombineLERP(PRIMITIVE, 0, SHADE, 0, ENVIRONMENT, 0, SHADE, 0,
        ENVIRONMENT, COMBINED, TEXEL0, COMBINED, 0, 0, 0, COMBINED),
    gsSPEndDisplayList(),
};
/* Starts like g_CharacterRenderModeDisplayList, until SetCharacterHilite
 * fills it */
Gfx g_CharacterHiliteDisplayList[3] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_SURF2),
    gsSPEndDisplayList(),
};
/* Read by no routine */
static Gfx f_UnusedDoubleEndDisplayList[] UNUSED = {
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};
u8 g_CharacterEnvColors[ENV_COLOR_COUNT][3] = {
    {0x80, 0x40, 0x1E},
    {0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0x7F},
    {0xC8, 0xC8, 0xDC},
    {0x00, 0xDC, 0x00},
    {0xFF, 0x7F, 0x7F},
    {0xFF, 0xC8, 0x40},
    {0xFF, 0xFF, 0xFF},
    {0xFF, 0xFA, 0xE6},
    {0x40, 0x9F, 0xFF},
};
Gfx g_CharacterEnvColorDisplayLists[ENV_COLOR_COUNT][2] = {
    {gsDPSetEnvColor(0, 0, 0, OPAQUE_ALPHA), gsSPEndDisplayList()},
    {gsDPSetEnvColor(0, 0, 0, OPAQUE_ALPHA), gsSPEndDisplayList()},
    {gsDPSetEnvColor(0, 0, 0, OPAQUE_ALPHA), gsSPEndDisplayList()},
    {gsDPSetEnvColor(0, 0, 0, OPAQUE_ALPHA), gsSPEndDisplayList()},
    {gsDPSetEnvColor(0, 0, 0, OPAQUE_ALPHA), gsSPEndDisplayList()},
    {gsDPSetEnvColor(0, 0, 0, OPAQUE_ALPHA), gsSPEndDisplayList()},
    {gsDPSetEnvColor(0, 0, 0, OPAQUE_ALPHA), gsSPEndDisplayList()},
    {gsDPSetEnvColor(0, 0, 0, OPAQUE_ALPHA), gsSPEndDisplayList()},
    {gsDPSetEnvColor(0, 0, 0, OPAQUE_ALPHA), gsSPEndDisplayList()},
    {gsDPSetEnvColor(0, 0, 0, OPAQUE_ALPHA), gsSPEndDisplayList()},
};
/* Read by no routine */
static s32 f_UnusedWordBeforeModelIndex UNUSED = 1;
s32 g_CharacterModelIndex = 1;
/* The animation LoadCharacter starts */
static s32 f_StartedAnimationIndex = 0;
/* Set to the model buffer of the slot last set up or animated */
static void* f_CurrentModelBuffer = NULL;
/* Read by no routine */
static Gfx f_UnusedCharacterEndDisplayList[] UNUSED = {
    gsSPEndDisplayList(),
};
/* For CHARACTER_LIGHTING_FIXED and CHARACTER_LIGHTING_BLACK */
static Lights2 f_FixedLights = gdSPDefLights2(0x6E, 0x6E, 0x6E, 0x9B, 0x78,
    0x78, 0, 100, 100, 0x96, 0x96, 0x96, 120, 0, 0);
static Lights2 f_BlackLights =
    gdSPDefLights2(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
/* SetCharacterLights' lights, in colors for rain that never change */
static Lights2 f_RainLights[DRAW_BUFFER_COUNT] = {
    gdSPDefLights2(0x64, 0x64, 0x64, 0x9B, 0x78, 0x78, 0, -80, 80, 0x96, 0x96,
        0x96, 80, -80, 0),
    gdSPDefLights2(0x64, 0x64, 0x64, 0x9B, 0x78, 0x78, 0, -80, 80, 0x96, 0x96,
        0x96, 80, -80, 0),
};
/* SetCharacterLights' lights, in dry weather colors that change near the end of
 * the round */
static Lights2 f_DryLights[DRAW_BUFFER_COUNT] = {
    gdSPDefLights2(0x64, 0x64, 0x64, 0x9B, 0x78, 0x78, 0, -120, 120, 0x96, 0x96,
        0x96, 120, -120, 0),
    gdSPDefLights2(0x64, 0x64, 0x64, 0x9B, 0x78, 0x78, 0, -120, 120, 0x96, 0x96,
        0x96, 120, -120, 0),
};
/* Read by no routine */
static Lights2 f_UnusedCharacterLights[DRAW_BUFFER_COUNT] UNUSED = {
    gdSPDefLights2(0x64, 0x64, 0x64, 0x9B, 0x78, 0x78, 0, -120, 20, 0x96, 0x96,
        0x96, 20, -120, 0),
    gdSPDefLights2(0x78, 0x5A, 0x5A, 0xB9, 0x78, 0x78, 0, -120, 120, 0xB4, 0x96,
        0x96, 120, -120, 0),
};
/*
 * The ambient and first light last drawn with, used instead during the impact
 * flash; DrawCharacterAndShadow then sets their second light to the flash level
 */
static Lights2 f_SavedLights[DRAW_BUFFER_COUNT] = {
    gdSPDefLights2(0x20, 0x20, 0x20, 0xFF, 0xDC, 0xDC, 0, -127, 0, 0xFF, 0xDC,
        0xDC, 126, 0, 1),
    gdSPDefLights2(0x20, 0x20, 0x20, 0xFF, 0xDC, 0xDC, 0, -127, 0, 0xFF, 0xDC,
        0xDC, 126, 0, 1),
};
/* The direction of each set's second light, across the main camera's view */
static s32 f_SecondLightDirection[AXIS_COUNT] = {0, 0, 0};
/* Set until the first draw sets up the matrices */
static Boolean f_IsMatrixSetupPending = TRUE;
/* Flips between 0 and 1 on each draw */
static s32 f_DrawParity = 0;
/* Read by no routine */
static s32 f_UnusedWordAfterDrawParity UNUSED = 0;
/* A square, textured with g_ShadowTexture, laid on the ground plane by
 * f_ShadowMatrices */
static Vtx f_ShadowVertices[4] = {
    {{{-SHADOW_QUAD_HALF_SIZE, SHADOW_QUAD_HALF_SIZE, 0}, 0,
        {SHADOW_QUAD_TEXEL_SPAN, SHADOW_QUAD_TEXEL_SPAN},
        {0x96, 0x96, 0x96, 0xFF}}},
    {{{SHADOW_QUAD_HALF_SIZE, SHADOW_QUAD_HALF_SIZE, 0}, 0,
        {0, SHADOW_QUAD_TEXEL_SPAN}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{SHADOW_QUAD_HALF_SIZE, -SHADOW_QUAD_HALF_SIZE, 0}, 0, {0, 0},
        {0xFF, 0xFF, 0xFF, 0x00}}},
    {{{-SHADOW_QUAD_HALF_SIZE, -SHADOW_QUAD_HALF_SIZE, 0}, 0,
        {SHADOW_QUAD_TEXEL_SPAN, 0}, {0x96, 0x96, 0x96, 0x00}}},
};

/*
 * How far to the side of the ball each playable character stands, in course
 * units, for each club kind and then for an approach shot with a wood.
 * WriteBallDistances measures the models and writes a table like this as
 * pcdist.c.
 */
static const s32 f_BallDistances[CHARACTER_COUNT][BALL_DISTANCE_COUNT] = {
    {0x2BDD, 0x2070, 0x1903, 0x114D, 0x28F4},
    {0x3502, 0x2910, 0x21DD, 0x1A86, 0x2DAA},
    {0x2B74, 0x1EDA, 0x17DA, 0x10B1, 0x287A},
    {0x2F33, 0x279F, 0x2578, 0x1F5D, 0x2F33},
    {0x3967, 0x258A, 0x1FDF, 0x15C7, 0x2B1C},
    {0x3017, 0x24B5, 0x2229, 0x17C7, 0x249B},
    {0x3624, 0x2B1D, 0x24D4, 0x1891, 0x33E9},
    {0x39E2, 0x3049, 0x2D05, 0x20E4, 0x3279},
    {0x4248, 0x2E12, 0x27B9, 0x1B35, 0x32A7},
    {0x34EC, 0x2A0B, 0x2311, 0x18AB, 0x341D},
    {0x2BCD, 0x1EF7, 0x197B, 0x1636, 0x2807},
    {0x5D47, 0x5119, 0x4DB7, 0x37C5, 0x5D47},
    {0x5418, 0x4540, 0x3EC1, 0x3AF8, 0x5418},
    {0x34EC, 0x2A0B, 0x2311, 0x18AB, 0x341D},
    {0x2CD0, 0x1DAC, 0x183A, 0x1321, 0x224E},
    {0x271C, 0x198F, 0x161D, 0x1433, 0x271C},
    {0x334B, 0x276E, 0x1F84, 0x17E4, 0x3017},
    {0x3295, 0x2776, 0x1FD8, 0x1931, 0x2BC9},
};

/*
 * Returns how far to the side of the ball the character in the slot stands
 * for the club and shot mode. Playable characters' distances come from
 * f_BallDistances unless GAME_FLAG_GENERATE_BALL_DISTANCES is set; otherwise
 * the model is measured, with a default for the club kind if that fails.
 */
s32 GetBallDistance(
    u32 slotIndex, s32 club, Boolean isClubKind, ShotMode shotMode) {
  f32 position[3];
  char text[DEBUG_LINE_SIZE];
  s32 distance;
  s32 pointId = WOOD_DISTANCE_POINT;
  s32 column;
  s32 modelIndex;
  CharacterSlot* slot;
  ClubKind kind;
  f32 depth;

  slot = GetUsedCharacterSlot(slotIndex);
  column = CLUB_KIND_WOOD;
  if (!isClubKind) {
    kind = GetCharacterClubKind(slotIndex, club);
  } else {
    kind = club;
  }

  switch (kind) {
#ifdef AVOID_UB
    default:
#endif
    case CLUB_KIND_WOOD:
      distance = WOOD_BALL_DISTANCE;
      pointId = WOOD_DISTANCE_POINT;
      column = (shotMode == SHOT_MODE_APPROACH) * BALL_DISTANCE_WOOD_APPROACH;
      break;
    case CLUB_KIND_IRON:
      distance = IRON_BALL_DISTANCE;
      pointId = IRON_DISTANCE_POINT;
      column = CLUB_KIND_IRON;
      break;
    case CLUB_KIND_WEDGE:
      distance = IRON_BALL_DISTANCE;
      pointId = WEDGE_DISTANCE_POINT;
      column = CLUB_KIND_WEDGE;
      break;
    case CLUB_KIND_PUTTER:
      distance = PUTTER_BALL_DISTANCE;
      pointId = PUTTER_DISTANCE_POINT;
      column = CLUB_KIND_PUTTER;
      break;
  }

  if (!IsGameFlagSet(GAME_FLAG_GENERATE_BALL_DISTANCES) && slot != NULL &&
      (modelIndex = slot->modelIndex) >= 0 &&
      (modelIndex %= CHARACTER_MODELS_PER_KIND) < CHARACTER_COUNT) {
    distance = f_BallDistances[modelIndex][column];
  } else if (GetCharacterPointModelPosition(slotIndex, pointId, position) ==
      0) {
    sprintf(text, "pres %f,%f,%f", position[AXIS_X], position[AXIS_Y],
        position[AXIS_Z]);
    PrintDebugText(text, POINT_POSITION_TEXT_COLUMN, POINT_POSITION_TEXT_ROW);
    depth = position[AXIS_Z] * POINT_DEPTH_TO_BALL_DISTANCE;
    if (depth < 0.0f) {
      depth = -depth;
    }

    distance = depth;
    osSyncPrintf(":");
  } else {
    osSyncPrintf("]");
  }

  {
    /*
     * An unused nested function, a GNU C extension. Defined here rather than
     * among the declarations, it leaves GetBallDistance's registers as they
     * are in the original.
     */
    UNUSED_FUNCTION f32 GetVectorLength(f32 x, f32 y, f32 z) {
      return sqrtf(x * x + y * y + z * z);
    }
  }

  return distance;
}

/* Assertion strings that WriteBallDistances and PlayTextureAnimation share */
static const char f_AssertionExpression[] = "EX";
static const char f_FileName[] = "c:/usgolf64/polychara/pcsub.c";

/*
 * Loads each playable character, measures its ball distance in each column of
 * f_BallDistances and writes the source of that table to the host as pcdist.c.
 * AppendDistanceText is a nested function, a GNU C extension.
 */
void WriteBallDistances(void) {
  char line[BALL_DISTANCE_LINE_SIZE];
  char* cursor;
  char* end;
  char* source;
  s32 modelIndex;
  s32 column;
  s32 animationId;
  ClubKind kind;

  void AppendDistanceText(char* text) {
    char character;

    while (TRUE) {
      character = *text++;
      if (character == '\0') {
        break;
      }

      if (cursor < end) {
        *cursor++ = character;
      }
    }
  }

  source = AllocMemory(BALL_DISTANCE_SOURCE_SIZE);
  cursor = source;
  end = source + BALL_DISTANCE_SOURCE_SIZE;
  if (source == NULL) {
    ReportFailedAssertion(
        f_AssertionExpression, f_FileName, BALL_DISTANCE_SOURCE_ASSERT_LINE);
  }

  sprintf(line,
      "/* Caution! This file is automatically generated by pcsub.c*/\n"
      "static const DISTPARA distp[PLAYABLE_NUM][5] = {\n");
  AppendDistanceText(line);
  for (modelIndex = 0; modelIndex < CHARACTER_COUNT; modelIndex++) {
    s32 distances[BALL_DISTANCE_COUNT];

    LoadCharacter(MEASURED_SLOT, modelIndex, 0);
    g_CharacterModelIndex = g_CharacterSlots[MEASURED_SLOT].modelIndex;
    for (column = 0; column < BALL_DISTANCE_COUNT; column++) {
      switch (column) {
        case CLUB_KIND_WOOD:
          animationId = WOOD_DISTANCE_ANIMATION;
          kind = CLUB_KIND_WOOD;
          break;
        case CLUB_KIND_IRON:
          animationId = IRON_DISTANCE_ANIMATION;
          kind = CLUB_KIND_IRON;
          break;
        case CLUB_KIND_WEDGE:
          animationId = WEDGE_DISTANCE_ANIMATION;
          kind = CLUB_KIND_WEDGE;
          break;
        case CLUB_KIND_PUTTER:
          animationId = PUTTER_DISTANCE_ANIMATION;
          kind = CLUB_KIND_PUTTER;
          break;
        case BALL_DISTANCE_WOOD_APPROACH:
          animationId = WOOD_APPROACH_DISTANCE_ANIMATION;
          kind = CLUB_KIND_WOOD;
          break;
      }

      SetAnimationSpeed(MEASURED_SLOT, 0.0f);
      SetCharacterAnimation(MEASURED_SLOT, animationId);
      if (column < BALL_DISTANCE_WOOD_APPROACH) {
        SkipToAnimationEnd(MEASURED_SLOT);
      }

      AnimateCharacter(MEASURED_SLOT);
      distances[column] =
          GetBallDistance(MEASURED_SLOT, kind, TRUE, MEASURED_SHOT_MODE);
    }

    osSyncPrintf("\n");
    sprintf(line, "{%d, %d, %d, %d, %d},\n", (int)distances[0],
        (int)distances[1], (int)distances[2], (int)distances[3],
        (int)distances[4]);
    osSyncPrintf(line);
    AppendDistanceText(line);
    FreeCharacterSlot(MEASURED_SLOT);
  }

  sprintf(line, "};\n");
  AppendDistanceText(line);
  line[0] = END_OF_FILE_CHARACTER;
  line[1] = '\0';
  AppendDistanceText(line);
  sprintf(line, "pcdist.c");
  WriteHostFile(line, cursor - source);
  FreeMemory(&source);
}

/* As above, so the data below lands in .rodata */
RESUME_RODATA_SECTION();

/* Unreferenced; left by debug output that the routines below no longer have */
static const char f_UnusedFrameFormat[] = "TA %d : FRAME %d    ";

void WriteBallDistancesIfEnabled(void) {
  if (IsGameFlagSet(GAME_FLAG_GENERATE_BALL_DISTANCES)) {
    WriteBallDistances();
  }
}

/* Has the character in the slot hold the club, by choosing the display list for
 * its kind */
void SetCharacterClub(u32 slotIndex, s32 clubIndex) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  s32 choice;

  if (slot != NULL) {
    g_CharacterModelIndex = slot->modelIndex;
    switch (GetCharacterClubKind(slotIndex, clubIndex)) {
      case CLUB_KIND_WOOD:
        choice = WOOD_DISPLAY_LIST_CHOICE;
        break;
      case CLUB_KIND_IRON:
        choice = IRON_DISPLAY_LIST_CHOICE;
        break;
      case CLUB_KIND_WEDGE:
        choice = WEDGE_DISPLAY_LIST_CHOICE;
        break;
      case CLUB_KIND_PUTTER:
        choice = PUTTER_DISPLAY_LIST_CHOICE;
        break;
      default:
        choice = NO_DISPLAY_LIST_CHOICE;
        break;
    }

    MapModelBuffer(slot);
    SelectCharacterDisplayList(slotIndex, choice);
  }
}

/*
 * Copies the kind and value of up to EVENT_ACTION_COUNT_MAX of the
 * animation's events on the frame into actions, and returns how many it
 * copied
 */
s32 GetAnimationEventActions(u32 slotIndex, s32 animationIndex, s32 frame,
    AnimationEventAction* actions) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  s32 count = 0;
  AnimationEvent* event;
  s32 i;

  if (slot != NULL && animationIndex < slot->animationCount) {
    event = ModelAnimations(slot->model)[animationIndex].events;
    if (event != NULL) {
      for (i = 0; event[i].frame != END_OF_ANIMATION_EVENTS; i++) {
        if (event[i].frame == frame) {
          if (count < EVENT_ACTION_COUNT_MAX) {
            actions[count].kind = event[i].kind;
            actions[count].value = event[i].value;
            count++;
          }
        }
      }
    }
  }

  return count;
}

/* Returns the frame of the animation's first event of the kind, from its start,
 * or -1 if none */
s32 GetAnimationEventFrame(u32 slotIndex, s32 animationIndex, s32 kind) {
  s32 frame = -1;
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  AnimationEvent* event;
  s32 i;

  if (slot != NULL && animationIndex < slot->animationCount) {
    event = ModelAnimations(slot->model)[animationIndex].events;
    if (event != NULL) {
      for (i = 0; event[i].frame != END_OF_ANIMATION_EVENTS; i++) {
        if (event[i].kind == kind) {
          frame = event[i].frame -
              ModelAnimations(slot->model)[animationIndex].startFrame;
          break;
        }
      }
    }
  }

  return frame;
}

/* Unreferenced */
s32 GetImpactEventFrame(u32 slotIndex) {
  return GetAnimationEventFrame(
      slotIndex, IMPACT_EVENT_ANIMATION_INDEX, ANIMATION_EVENT_IMPACT);
}

/*
 * Gives each of the slot's animated triangles its default texture.
 *
 * Doesn't match: 75 words differ. The original's frame has 0x40 unused bytes
 * from 0x18 and keeps the slot at 0x58, reloading it at the top of the
 * triangle loop, where GCC keeps the slot in a saved register. Making
 * SetTriangleTexture a static inline function left the frame as it is, but
 * 137 words differed where 87 did without it. Making it an inline function
 * nested here keeps the slot on the stack and reloads it as the original
 * does, but leaves out the unused bytes and computes the slot's address for
 * the static chain: 133 words differ.
 */
#ifdef NON_MATCHING
void ResetTriangleTextures(u32 slotIndex) {
  s32 triangleIndex;
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  AnimatedTriangle* triangle;
  AnimatedTexture* texture;
  Gfx* gfx;
  Vtx* vertex;
  s32 i;

  if (slot != NULL) {
    MapModelBuffer(slot);
    triangleIndex = 0;
    for (triangle = ModelAnimatedTriangles(slot->model)[0]; triangle != NULL;
        triangle = ModelAnimatedTriangles(slot->model)[++triangleIndex]) {
      texture = ModelAnimatedTextures(slot->model);
      if (triangle->textureIndex >= 0) {
        texture += triangle->textureIndex;
      }

      SetTriangleTexture(slot, triangle, texture, triangle->s, triangle->t);
    }
  }
}

#else
INCLUDE_ASM("asm/nonmatchings/character", ResetTriangleTextures);
#endif

/*
 * Shows the keyed textures of the slot's playing texture animations, then
 * advances each one a frame. The arguments after the slot are unused.
 *
 * Doesn't match: the original's frame leaves 0x18 to 0x57 and 0x5C to 0xF7
 * unused and keeps the slot at 0x58, reloading it at the top of the triangle
 * loop, where GCC keeps the slot in a saved register. 161 words differ.
 */
#ifdef NON_MATCHING
void AdvanceTextureAnimations(u32 slotIndex, s32 unused UNUSED,
    s32 frameLastDigit UNUSED, s32 frame UNUSED) {
  s32 triangleIndex = 0;
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  AnimatedTriangle* triangle;
  TriangleTextureKeys* keySet;
  TriangleTextureKey* key;
  TextureAnimationState* state;
  AnimatedTexture* texture;
  TextureAnimation* animation;
  Gfx* gfx;
  Vtx* vertex;
  s32 keySetIndex;
  s32 keyIndex;
  s32 i;

  if (slot != NULL) {
    MapModelBuffer(slot);
    for (triangle = ModelAnimatedTriangles(slot->model)[0]; triangle != NULL;
        triangle = ModelAnimatedTriangles(slot->model)[++triangleIndex]) {
      for (keySetIndex = 0; keySetIndex < triangle->keySetCount;
          keySetIndex++) {
        keySet = &triangle->keySets[keySetIndex];
        state = &slot->textureAnimations[keySet->textureAnimationIndex];
        if (state->pendingCopyCount != 0) {
          for (keyIndex = 0; keyIndex < keySet->count; keyIndex++) {
            key = &keySet->keys[keyIndex];
            if (state->frame >= key->frame) {
              texture = &ModelAnimatedTextures(slot->model)[key->textureIndex];
              SetTriangleTexture(slot, triangle, texture, key->s, key->t);
            }
          }
        }
      }
    }

    for (i = 0; i < TEXTURE_ANIMATION_COUNT; i++) {
      animation = &ModelTextureAnimations(slot->model)[i];
      if (animation->index < 0) {
        break;
      }

      state = &slot->textureAnimations[animation->index];
      if (state->pendingCopyCount >= TEXTURE_ANIMATION_PLAYING) {
        if (++state->frame >= animation->frameCount) {
          if (animation->index == RANDOM_TEXTURE_ANIMATION) {
            state->pendingCopyCount = FRAME_DATA_COPY_COUNT;
            slot->textureAnimations[RANDOM_TEXTURE_ANIMATION].frame = 0;
          } else {
            state->pendingCopyCount = FRAME_DATA_COPY_COUNT;
            state->frame = animation->frameCount;
          }
        }
      } else if (state->pendingCopyCount != 0) {
        state->pendingCopyCount--;
      }
    }
  }
}

#else
INCLUDE_ASM("asm/nonmatchings/character", AdvanceTextureAnimations);
#endif

NO_RESULT SetTextureAnimationFrame(u32 slotIndex, s32 index, s32 frame) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    slot->textureAnimations[index].frame = frame;
    slot->textureAnimations[index].pendingCopyCount = FRAME_DATA_COPY_COUNT;
  }
}

void StopTextureAnimations(u32 slotIndex) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  s32 i;

  for (i = TEXTURE_ANIMATION_COUNT - 1; i >= 0; i--) {
    slot->textureAnimations[i].pendingCopyCount = 0;
  }
}

NO_RESULT PlayTextureAnimation(u32 slotIndex, s32 index) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (index >= TEXTURE_ANIMATION_COUNT) {
    ReportFailedAssertion(
        f_AssertionExpression, f_FileName, TEXTURE_ANIMATION_ASSERT_LINE);
  }

  if (slot != NULL) {
    slot->textureAnimations[index].pendingCopyCount = TEXTURE_ANIMATION_PLAYING;
    slot->textureAnimations[index].frame = 0;
  }
}

/*
 * Gives the slot's triangles that the texture animation changes their default
 * texture. Nothing calls it.
 *
 * Doesn't match: the original's frame has 0x40 unused bytes from 0x18 and
 * keeps the slot at 0x58, reloading it at the top of the triangle loop, where
 * GCC keeps the slot in a saved register. 87 words differ.
 */
#ifdef NON_MATCHING
void ResetAnimationTriangleTextures(u32 slotIndex, s32 textureAnimationIndex) {
  s32 triangleIndex;
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  AnimatedTriangle* triangle;
  AnimatedTexture* texture;
  Gfx* gfx;
  Vtx* vertex;
  s32 keySetIndex;
  s32 i;

  if (slot != NULL) {
    MapModelBuffer(slot);
    triangleIndex = 0;
    for (triangle = ModelAnimatedTriangles(slot->model)[0]; triangle != NULL;
        triangle = ModelAnimatedTriangles(slot->model)[++triangleIndex]) {
      texture = &ModelAnimatedTextures(slot->model)[triangle->textureIndex];
      for (keySetIndex = 0; keySetIndex < triangle->keySetCount;
          keySetIndex++) {
        if (triangle->keySets[keySetIndex].textureAnimationIndex ==
            textureAnimationIndex) {
          SetTriangleTexture(slot, triangle, texture, triangle->s, triangle->t);
        }
      }
    }
  }
}

#else
INCLUDE_ASM("asm/nonmatchings/character", ResetAnimationTriangleTextures);
#endif

/* As above, so the data below lands in .rodata */
RESUME_RODATA_SECTION();

static const char f_NotDisposedFormat[] =
    "Polychara slot %d not disposed yet!!!!!!!\n";
/* Unreferenced; left by debug output that LoadCharacter no longer has */
static const char f_UnusedReadRomFormat[] =
    "nuPiReadRom(%x, %x, %x) [bufsiz %x]\n";
static const char f_UnusedCompleteText[] = "complete\n";

/*
 * Loads the model of the character of the kind into the slot, sets up its
 * frame data copies, and starts its first animation
 */
void LoadCharacter(u32 slotIndex, s32 character, s32 kind) {
  Boolean isPatched = FALSE;
  CharacterSlot* slot = &g_CharacterSlots[slotIndex];
  JointAnimation* joint;
  CharacterAnimation* animation;
  u32 frameDataSize;
  s32 count;

  if (kind < CHARACTER_MODEL_KIND_COUNT) {
    g_CharacterModelIndex = ((kind & (CHARACTER_MODEL_KIND_COUNT - 1)) *
                                CHARACTER_MODELS_PER_KIND) +
        (character % CHARACTER_MODELS_PER_KIND);
    slot->model = &g_CharacterModels[0][g_CharacterModelIndex];
  } else {
    g_CharacterModelIndex = (u32)character % OTHER_CHARACTER_MODEL_COUNT;
    slot->model = &g_OtherCharacterModels[g_CharacterModelIndex];
  }

  if (IsCharacterSlotBusy(slotIndex)) {
    /* Printed five times, so it stands out */
    osSyncPrintf(f_NotDisposedFormat, slotIndex);
    osSyncPrintf(f_NotDisposedFormat, slotIndex);
    osSyncPrintf(f_NotDisposedFormat, slotIndex);
    osSyncPrintf(f_NotDisposedFormat, slotIndex);
    osSyncPrintf(f_NotDisposedFormat, slotIndex);
  } else if (slot->modelBuffer != NULL) {
    if (slot->model->romStart != slot->model->baseRomStart) {
      isPatched = TRUE;
    }

    slot->modelIndex = g_CharacterModelIndex;
    slot->alpha = LOADED_ALPHA;
    slot->frameDataIndex = 0;
    slot->animationIndex = 0;
    slot->isTranslucent = FALSE;
    slot->lighting = CHARACTER_LIGHTING_FIXED;
    slot->isLooping = TRUE;
    slot->nextAnimationId = NO_NEXT_ANIMATION;
    slot->playsRandomTextureAnimation = TRUE;
    slot->randomTextureAnimationDelay = LOADED_RANDOM_TEXTURE_ANIMATION_DELAY;
    slot->unk188 = 0;
    slot->reusesLastMatrices = 0;
    f_CurrentModelBuffer = slot->modelBuffer;
    if (isPatched) {
      LoadLzData(slot->model->baseRomStart, f_CurrentModelBuffer,
          slot->model->baseRomEnd - slot->model->baseRomStart);
      LoadPatchedLzData(slot->model->romStart, f_CurrentModelBuffer,
          slot->model->romEnd - slot->model->romStart);
    } else {
      LoadLzData(slot->model->romStart, f_CurrentModelBuffer,
          slot->model->romEnd - slot->model->romStart);
    }

    SetMultiTlb(0, TLB_PAGE_MODE_4K, CHARACTER_MODEL_PAGE_COUNT,
        CHARACTER_MODEL_ADDRESS, osVirtualToPhysical(f_CurrentModelBuffer));
    frameDataSize = slot->model->frameDataEnd - slot->model->frameDataStart;
    if (frameDataSize < FRAME_DATA_COPY_SIZE) {
      frameDataSize = FRAME_DATA_COPY_SIZE;
    }

    slot->frameData[0] = OS_PHYSICAL_TO_K0(
        osVirtualToPhysical((void*)slot->model->frameDataStart));
    /* Added as integers, the size first, to match */
    slot->frameData[1] = (void*)(frameDataSize + (u32)slot->frameData[0]);
    slot->frameData[2] = (void*)(frameDataSize + (u32)slot->frameData[1]);
    memcpy(slot->frameData[1], slot->frameData[0], frameDataSize);
    memcpy(slot->frameData[2], slot->frameData[0], frameDataSize);
    g_CurrentJointMatrices = slot->frameData[slot->frameDataIndex];
    f_StartedAnimationIndex = slot->animationIndex;
    if (IsGameFlagSet(GAME_FLAG_LOG_CHARACTER_SETUP)) {
      osSyncPrintf("complete2 anims_addr %x\n", ModelAnimations(slot->model));
    }

    slot->animationStartFrame =
        ModelAnimations(slot->model)[f_StartedAnimationIndex].startFrame;
    slot->animationFrame = slot->animationStartFrame;
    slot->animationEndFrame = slot->animationStartFrame +
        ModelAnimations(slot->model)[f_StartedAnimationIndex].frameCount;
    g_CurrentAnimationFrame = slot->animationFrame;
    g_CurrentAnimationStartFrame = slot->animationStartFrame;
    g_CurrentAnimationEndFrame = slot->animationEndFrame;
    if (IsGameFlagSet(GAME_FLAG_LOG_CHARACTER_SETUP)) {
      osSyncPrintf("loop %d, loop_min = %d, loop_max = %d\n",
          f_StartedAnimationIndex, g_CurrentAnimationStartFrame,
          g_CurrentAnimationEndFrame);
    }

    ResetTriangleTextures(slotIndex);
    slot->frameDataIndex = 1;
    ResetTriangleTextures(slotIndex);
    slot->frameDataIndex = 2;
    ResetTriangleTextures(slotIndex);
    slot->frameDataIndex = 0;
    StopTextureAnimations(slotIndex);
    AnimateCharacterJoints(slotIndex);
    joint = ModelJointAnimations(slot->model);
    for (; joint->translationKeyCount != END_OF_JOINT_ANIMATIONS; joint++) {
      joint->translationSpan.endFrame = UNSET_KEYFRAME_SPAN;
      joint->translationSpan.startFrame = UNSET_KEYFRAME_SPAN;
      joint->rotationSpan.endFrame = UNSET_KEYFRAME_SPAN;
      joint->rotationSpan.startFrame = UNSET_KEYFRAME_SPAN;
      joint->scaleSpan.endFrame = UNSET_KEYFRAME_SPAN;
      joint->scaleSpan.startFrame = UNSET_KEYFRAME_SPAN;
    }

    slot->jointMatrices = g_CurrentJointMatrices;
    slot->displayList =
        (Gfx*)slot->model->segmentAddresses[CHARACTER_MODEL_DISPLAY_LIST];
    slot->unk68[0] =
        *(f32*)slot->model->segmentAddresses[CHARACTER_MODEL_UNK68];
    slot->unk68[1] =
        *(f32*)slot->model->segmentAddresses[CHARACTER_MODEL_UNK68 + 1];
    slot->unk68[2] =
        *(f32*)slot->model->segmentAddresses[CHARACTER_MODEL_UNK68 + 2];
    slot->unk68[3] =
        *(f32*)slot->model->segmentAddresses[CHARACTER_MODEL_UNK68 + 3];
    slot->unk68[4] =
        *(f32*)slot->model->segmentAddresses[CHARACTER_MODEL_UNK68 + 4];
    slot->unk68[5] =
        *(f32*)slot->model->segmentAddresses[CHARACTER_MODEL_UNK68 + 5];
    slot->shadowScale = slot->model->shadowScale;
    count = 0;
    for (animation = ModelAnimations(slot->model); animation->events != NULL;
        animation++) {
      count++;
    }

    slot->animationCount = count;
    slot->animationSpeed = 1.0f;
    if (IsGameFlagSet(GAME_FLAG_LOG_CHARACTER_SETUP)) {
      osSyncPrintf("stage01 inited, char %d.\n", g_CharacterModelIndex);
    }
  }
}

s32 GetAnimationIndex(u32 slotIndex, s32 id) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  s32 foundIndex = 0;
  CharacterAnimation* animation;
  s32 i;

  if (slot != NULL) {
    animation = ModelAnimations(slot->model);
    for (i = 0; animation->events != NULL; i++, animation++) {
      if (animation->id == id) {
        foundIndex = i;
        break;
      }
    }
  }

  return foundIndex;
}

/*
 * Returns the slot's animation with the given id, or its first if it has
 * none with that id. Returns NULL if the slot is unused or its table, which
 * ends at an animation without events, is empty.
 */
CharacterAnimation* FindAnimation(u32 slotIndex, s32 id) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    MapModelBuffer(slot);
    id = GetAnimationIndex(slotIndex, id);
    if (ModelAnimations(slot->model)[id].events == NULL) {
      return NULL;
    }

    return &ModelAnimations(slot->model)[id];
  }

  return NULL;
}

void SetCharacterAnimation(u32 slotIndex, s32 id) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  JointAnimation* joint;

  if (slot != NULL) {
    MapModelBuffer(slot);
    slot->animationIndex = GetAnimationIndex(slotIndex, id);
    if (ModelAnimations(slot->model)[slot->animationIndex].events == NULL) {
      slot->animationIndex = 0;
    }

    slot->animationStartFrame =
        ModelAnimations(slot->model)[slot->animationIndex].startFrame;
    slot->animationFrame = slot->animationStartFrame;
    slot->animationEndFrame = slot->animationStartFrame +
        ModelAnimations(slot->model)[slot->animationIndex].frameCount;
    joint = ModelJointAnimations(slot->model);
    for (; joint->translationKeyCount != END_OF_JOINT_ANIMATIONS; joint++) {
      joint->translationSpan.endFrame = UNSET_KEYFRAME_SPAN;
      joint->translationSpan.startFrame = UNSET_KEYFRAME_SPAN;
      joint->rotationSpan.endFrame = UNSET_KEYFRAME_SPAN;
      joint->rotationSpan.startFrame = UNSET_KEYFRAME_SPAN;
      joint->scaleSpan.endFrame = UNSET_KEYFRAME_SPAN;
      joint->scaleSpan.startFrame = UNSET_KEYFRAME_SPAN;
    }
  }
}

void SetPlaysRandomTextureAnimation(u32 slotIndex, s32 value) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    slot->playsRandomTextureAnimation = value;
  }
}

void SkipToAnimationEnd(u32 slotIndex) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    slot->animationFrame = slot->animationEndFrame;
  }
}

void AdvanceAnimation(u32 slotIndex, s32 frameCount) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    slot->animationFrame += frameCount;
    if (slot->animationEndFrame <= slot->animationFrame) {
      slot->animationFrame = slot->animationEndFrame;
    }
  }
}

void SetAnimationSpeed(u32 slotIndex, f32 speed) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    slot->animationSpeed = speed;
  }
}

void SetAnimationLooping(u32 slotIndex, s32 value) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    slot->isLooping = value;
  }
}

void SetNextAnimation(u32 slotIndex, s32 value) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    slot->nextAnimationId = value;
  }
}

void SetCharacterTranslucent(u32 slotIndex, s32 value) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    slot->isTranslucent = value;
  }
}

void SetCharacterLighting(u32 slotIndex, s32 value) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    slot->lighting = value;
  }
}

void SetCharacterSlotAlpha(u32 slotIndex, s32 value) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    slot->alpha = value;
  }
}

s32 GetCurrentAnimationEndFrame(u32 slotIndex) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  s32 endFrame = -1;

  if (slot != NULL) {
    endFrame = slot->animationEndFrame;
  }

  return endFrame;
}

s32 GetAnimationEndFrame(u32 slotIndex, s32 id) {
  CharacterAnimation* animation = FindAnimation(slotIndex, id);

  if (animation != NULL) {
    return animation->startFrame + animation->frameCount;
  }

  return -1;
}

s32 GetAnimationStartFrame(u32 slotIndex, s32 id) {
  CharacterAnimation* animation = FindAnimation(slotIndex, id);

  if (animation != NULL) {
    return animation->startFrame;
  }

  return -1;
}

s32 GetAnimationFrameCount(u32 slotIndex, s32 id) {
  CharacterAnimation* animation = FindAnimation(slotIndex, id);

  if (animation != NULL) {
    return animation->frameCount;
  }

  return -1;
}

void SetCharacterUnk188(u32 slotIndex, s32 value) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    slot->unk188 = value;
  }
}

NO_RESULT ReuseLastCharacterMatrices(u32 slotIndex) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    slot->reusesLastMatrices = TRUE;
  }
}

s32 GetAnimationFrame(u32 slotIndex) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  s32 frame = -1;

  if (slot != NULL) {
    frame = slot->animationFrame;
  }

  return frame;
}

/*
 * Steps the slot's animation: carries out the events of the frame, advances
 * the frame, and poses the joints into the next frame data copy
 */
void AnimateCharacter(u32 slotIndex) {
  AnimationEventAction actions[EVENT_ACTION_COUNT_MAX];
  char text[DEBUG_LINE_SIZE];
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  s32 i = 0;
  s32 actionCount;
  Boolean hasLooped;

  if (slot != NULL) {
    hasLooped = FALSE;
    MapModelBuffer(slot);
    actionCount = GetAnimationEventActions(
        slotIndex, slot->animationIndex, slot->animationFrame, actions);
    for (; i < actionCount; i++) {
      switch (actions[i].kind) {
        case ANIMATION_EVENT_LOOP_START:
          break;
        case ANIMATION_EVENT_LOOP_END:
          if (slot->isLooping) {
            hasLooped = TRUE;
            slot->animationFrame =
                GetAnimationEventFrame(slotIndex, slot->animationIndex,
                    ANIMATION_EVENT_LOOP_START) +
                ModelAnimations(slot->model)[slot->animationIndex].startFrame;
          }

          break;
        case ANIMATION_EVENT_TEXTURE_ANIMATION:
          if (actions[i].value == TEXTURE_ANIMATION_EVENT_START_RANDOM) {
            slot->playsRandomTextureAnimation = TRUE;
          } else if (actions[i].value == TEXTURE_ANIMATION_EVENT_STOP_RANDOM) {
            slot->playsRandomTextureAnimation = FALSE;
          } else {
            PlayTextureAnimation(slotIndex, actions[i].value);
          }

          break;
      }
    }

    if (!hasLooped) {
      slot->animationFrame += slot->animationSpeed;
    }

    if (slot->animationFrame > slot->animationEndFrame) {
      if (slot->nextAnimationId != NO_NEXT_ANIMATION) {
        SetCharacterAnimation(slotIndex, slot->nextAnimationId);
        slot->nextAnimationId = NO_NEXT_ANIMATION;
      } else {
        slot->animationFrame = slot->animationEndFrame;
      }
    }

    if (slot->animationFrame < slot->animationStartFrame) {
      slot->animationFrame = slot->animationStartFrame;
    }

    g_CharacterModelIndex = slot->modelIndex;
    g_CurrentAnimationFrame = slot->animationFrame;
    g_CurrentAnimationEndFrame = slot->animationEndFrame;
    g_CurrentAnimationStartFrame = slot->animationStartFrame;
    if (IsGameFlagSet(GAME_FLAG_SHOW_ANIMATION_FRAMES)) {
      sprintf(text, "[PC%d Frame%d]         ", (int)slotIndex,
          (int)(g_CurrentAnimationFrame - slot->animationStartFrame));
      PrintDebugText(text, ANIMATION_FRAME_TEXT_COLUMN,
          slotIndex + ANIMATION_FRAME_TEXT_FIRST_ROW);
    }

    if (slot->playsRandomTextureAnimation) {
      if (slot->randomTextureAnimationDelay == 0) {
        PlayTextureAnimation(slotIndex, RANDOM_TEXTURE_ANIMATION);
        slot->randomTextureAnimationDelay =
            (rand() & RANDOM_TEXTURE_ANIMATION_DELAY_MASK) +
            RANDOM_TEXTURE_ANIMATION_DELAY_MIN;
      } else {
        slot->randomTextureAnimationDelay--;
      }
    }

    AdvanceTextureAnimations(slotIndex, 0, (s32)g_CurrentAnimationFrame % 10,
        g_CurrentAnimationFrame);
    g_CurrentJointMatrices = slot->frameData[slot->frameDataIndex];
    f_CurrentModelBuffer = slot->modelBuffer;
    slot->frameDataIndex = (slot->frameDataIndex + 1) % FRAME_DATA_COPY_COUNT;
    AnimateCharacterJoints(slotIndex);
    slot->jointMatrices = g_CurrentJointMatrices;
  }
}

/*
 * Moves each joint's keyframe spans on to g_CurrentAnimationFrame as
 * AnimateCharacterJoints does, working out the joint transforms but leaving
 * them unused. Nothing calls it.
 *
 * Doesn't match: GCC keeps other values in saved registers across the joint
 * loop. The original keeps the two words of a double constant, where GCC
 * keeps 1.0f and a stack address. 184 words differ, 78 in shape.
 */
#ifdef NON_MATCHING
void UpdateJointKeyframeSpans(u32 slotIndex) {
  f32 unusedMatrices[3][4][4] UNUSED;
  f32 translationMatrix[4][4];
  f32 jointMatrix[4][4];
  f32 scaleMatrix[4][4] UNUSED;
  f32 rotation[4];
  f32(*rotationMatrix)[4];
  JointAnimation* joint;
  f32 frame;
  f32 x;
  f32 y;
  f32 z;
  f32 progress;
  f32 remaining;
  f32 dot;
  Radians angle;
  f32 nextWeight;
  f32 spanWeight;
  f32 sinAngle;
  f32 lengthSquared;
  f32 scale;
  f32 xs;
  f32 ys;
  f32 zs;
  f32 wx;
  f32 wy;
  f32 wz;
  f32 xx;
  f32 yy;
  f32 xy;
  f32 yz;
  f32 xz;
  s16 isInterpolated;
  s32 keyIndex;
  s16 spanIndex;
  s16 nextSpanIndex;

  joint = (JointAnimation*)GetUsedCharacterSlot(slotIndex)
              ->model->segmentAddresses[CHARACTER_MODEL_JOINTS];
  if (joint->translationKeyCount == END_OF_JOINT_ANIMATIONS) {
    return;
  }

  do {
    frame = g_CurrentAnimationFrame;
    isInterpolated = TRUE;
    if (joint->translationSpan.endFrame < frame ||
        frame < joint->translationSpan.startFrame) {
      keyIndex = 1;
      if (frame < joint->translationFrames[0]) {
        isInterpolated = FALSE;
        x = joint->translations[0][0];
        y = joint->translations[0][1];
        z = joint->translations[0][2];
      } else {
        for (; keyIndex < joint->translationKeyCount; keyIndex++) {
          if (frame < joint->translationFrames[keyIndex]) {
            joint->translationSpan.index = keyIndex - 1;
            joint->translationSpan.startFrame =
                joint->translationFrames[keyIndex - 1];
            joint->translationSpan.endFrame =
                joint->translationFrames[keyIndex];
            break;
          }
        }

        if (keyIndex == joint->translationKeyCount) {
          keyIndex--;
          isInterpolated = FALSE;
          x = joint->translations[keyIndex][0];
          y = joint->translations[keyIndex][1];
          z = joint->translations[keyIndex][2];
        }
      }
    }

    if (isInterpolated) {
      if (g_CurrentAnimationEndFrame < joint->translationSpan.endFrame) {
        x = joint->translations[joint->translationSpan.index][0];
        y = joint->translations[joint->translationSpan.index][1];
        z = joint->translations[joint->translationSpan.index][2];
      } else {
        /*
         * This and the other two progress fractions read the global again,
         * though frame holds it, to match
         */
        progress =
            (g_CurrentAnimationFrame - joint->translationSpan.startFrame) /
            (joint->translationSpan.endFrame -
                joint->translationSpan.startFrame);
        spanIndex = joint->translationSpan.index;
        nextSpanIndex = spanIndex + 1;
        remaining = 1.0 - progress;
        x = (remaining * joint->translations[spanIndex][0]) +
            (progress * joint->translations[nextSpanIndex][0]);
        y = (remaining * joint->translations[spanIndex][1]) +
            (progress * joint->translations[nextSpanIndex][1]);
        z = (remaining * joint->translations[spanIndex][2]) +
            (progress * joint->translations[nextSpanIndex][2]);
      }
    }

    guTranslateF(translationMatrix, x, y, z);

    frame = g_CurrentAnimationFrame;
    isInterpolated = TRUE;
    if (joint->rotationSpan.endFrame < frame ||
        frame < joint->rotationSpan.startFrame) {
      keyIndex = 1;
      if (frame < joint->rotationFrames[0]) {
        isInterpolated = FALSE;
        rotation[0] = joint->rotations[0][0] * QUATERNION_SCALE;
        rotation[1] = joint->rotations[0][1] * QUATERNION_SCALE;
        rotation[2] = joint->rotations[0][2] * QUATERNION_SCALE;
        rotation[3] = joint->rotations[0][3] * QUATERNION_SCALE;
      } else {
        for (; keyIndex < joint->rotationKeyCount; keyIndex++) {
          if (frame < joint->rotationFrames[keyIndex]) {
            joint->rotationSpan.index = keyIndex - 1;
            joint->rotationSpan.startFrame =
                joint->rotationFrames[keyIndex - 1];
            joint->rotationSpan.endFrame = joint->rotationFrames[keyIndex];
            break;
          }
        }

        if (keyIndex == joint->rotationKeyCount) {
          keyIndex--;
          isInterpolated = FALSE;
          rotation[0] = joint->rotations[keyIndex][0] * QUATERNION_SCALE;
          rotation[1] = joint->rotations[keyIndex][1] * QUATERNION_SCALE;
          rotation[2] = joint->rotations[keyIndex][2] * QUATERNION_SCALE;
          rotation[3] = joint->rotations[keyIndex][3] * QUATERNION_SCALE;
        }
      }
    }

    if (isInterpolated) {
      if (g_CurrentAnimationEndFrame < joint->rotationSpan.endFrame) {
        rotation[0] =
            joint->rotations[joint->rotationSpan.index][0] * QUATERNION_SCALE;
        rotation[1] =
            joint->rotations[joint->rotationSpan.index][1] * QUATERNION_SCALE;
        rotation[2] =
            joint->rotations[joint->rotationSpan.index][2] * QUATERNION_SCALE;
        rotation[3] =
            joint->rotations[joint->rotationSpan.index][3] * QUATERNION_SCALE;
      } else {
        /* Its own, as the original allocates it within the branch */
        f32 rotationProgress;

        spanIndex = joint->rotationSpan.index;
        nextSpanIndex = spanIndex + 1;
        dot = ((joint->rotations[nextSpanIndex][0] * QUATERNION_PRODUCT_SCALE) *
                  joint->rotations[spanIndex][0]) +
            ((joint->rotations[nextSpanIndex][1] * QUATERNION_PRODUCT_SCALE) *
                joint->rotations[spanIndex][1]) +
            ((joint->rotations[nextSpanIndex][2] * QUATERNION_PRODUCT_SCALE) *
                joint->rotations[spanIndex][2]) +
            ((joint->rotations[nextSpanIndex][3] * QUATERNION_PRODUCT_SCALE) *
                joint->rotations[spanIndex][3]);
        rotationProgress =
            (g_CurrentAnimationFrame - joint->rotationSpan.startFrame) /
            (joint->rotationSpan.endFrame - joint->rotationSpan.startFrame);
        /* Nearly equal rotations are blended linearly, where the sines below
         * would be near zero */
        if (1.0 - dot < SLERP_THRESHOLD) {
          nextWeight = rotationProgress;
          spanWeight = 1.0 - nextWeight;
        } else {
          angle = acosf(dot);
          rotationProgress *= angle;
          sinAngle = sinf(angle);
          nextWeight = sinf(rotationProgress) / sinAngle;
          spanWeight = sinf(angle - rotationProgress) / sinAngle;
        }

        spanWeight *= QUATERNION_SCALE;
        nextWeight *= QUATERNION_SCALE;
        rotation[0] = (spanWeight * joint->rotations[spanIndex][0]) +
            (nextWeight * joint->rotations[nextSpanIndex][0]);
        rotation[1] = (spanWeight * joint->rotations[spanIndex][1]) +
            (nextWeight * joint->rotations[nextSpanIndex][1]);
        rotation[2] = (spanWeight * joint->rotations[spanIndex][2]) +
            (nextWeight * joint->rotations[nextSpanIndex][2]);
        rotation[3] = (spanWeight * joint->rotations[spanIndex][3]) +
            (nextWeight * joint->rotations[nextSpanIndex][3]);
      }
    }

    /* The rotation quaternion (x, y, z, w) as a matrix */
    lengthSquared = (rotation[0] * rotation[0]) + (rotation[1] * rotation[1]) +
        (rotation[2] * rotation[2]) + (rotation[3] * rotation[3]);
    if (lengthSquared > 0.0f) {
      scale = 2.0f / lengthSquared;
      rotationMatrix = jointMatrix;
    } else {
      scale = 0.0f;
      rotationMatrix = jointMatrix;
    }

    xs = rotation[0] * scale;
    ys = rotation[1] * scale;
    zs = rotation[2] * scale;
    wx = rotation[3] * xs;
    wy = rotation[3] * ys;
    wz = rotation[3] * zs;
    xx = rotation[0] * xs;
    yy = rotation[1] * ys;
    /* dot holds the z axis's square term from here, to match */
    dot = rotation[2] * zs;
    xy = rotation[0] * ys;
    yz = rotation[1] * zs;
    xz = rotation[0] * zs;
    rotationMatrix[0][0] = 1.0f - (yy + dot);
    rotationMatrix[0][1] = xy + wz;
    rotationMatrix[0][2] = xz - wy;
    rotationMatrix[0][3] = 0.0f;
    rotationMatrix[1][0] = xy - wz;
    rotationMatrix[1][1] = 1.0f - (xx + dot);
    rotationMatrix[1][2] = yz + wx;
    rotationMatrix[1][3] = 0.0f;
    rotationMatrix[2][0] = xz + wy;
    rotationMatrix[2][1] = yz - wx;
    IN_OWN_BLOCK(rotationMatrix[2][2] = 1.0f - (xx + yy));
    rotationMatrix[2][3] = 0.0f;
    rotationMatrix[3][0] = 0.0f;
    rotationMatrix[3][1] = 0.0f;
    rotationMatrix[3][2] = 0.0f;
    rotationMatrix[3][3] = 1.0f;

    frame = g_CurrentAnimationFrame;
    isInterpolated = TRUE;
    if (joint->scaleSpan.endFrame < frame ||
        frame < joint->scaleSpan.startFrame) {
      keyIndex = 1;
      if (frame < joint->scaleFrames[0]) {
        isInterpolated = FALSE;
        x = joint->scales[0][0];
        y = joint->scales[0][1];
        z = joint->scales[0][2];
      } else {
        for (; keyIndex < joint->scaleKeyCount; keyIndex++) {
          if (frame < joint->scaleFrames[keyIndex]) {
            joint->scaleSpan.index = keyIndex - 1;
            joint->scaleSpan.startFrame = joint->scaleFrames[keyIndex - 1];
            joint->scaleSpan.endFrame = joint->scaleFrames[keyIndex];
            break;
          }
        }

        if (keyIndex == joint->scaleKeyCount) {
          keyIndex--;
          isInterpolated = FALSE;
          x = joint->scales[keyIndex][0];
          y = joint->scales[keyIndex][1];
          z = joint->scales[keyIndex][2];
        }
      }
    }

    if (isInterpolated) {
      if (g_CurrentAnimationEndFrame < joint->scaleSpan.endFrame) {
        x = joint->scales[joint->scaleSpan.index][0];
        y = joint->scales[joint->scaleSpan.index][1];
        z = joint->scales[joint->scaleSpan.index][2];
      } else {
        progress = (g_CurrentAnimationFrame - joint->scaleSpan.startFrame) /
            (joint->scaleSpan.endFrame - joint->scaleSpan.startFrame);
        spanIndex = joint->scaleSpan.index;
        nextSpanIndex = spanIndex + 1;
        remaining = 1.0 - progress;
        x = (remaining * joint->scales[spanIndex][0]) +
            (progress * joint->scales[nextSpanIndex][0]);
        y = (remaining * joint->scales[spanIndex][1]) +
            (progress * joint->scales[nextSpanIndex][1]);
        z = (remaining * joint->scales[spanIndex][2]) +
            (progress * joint->scales[nextSpanIndex][2]);
      }
    }

    joint++;
  } while (joint->translationKeyCount != END_OF_JOINT_ANIMATIONS);
}
#else
/* As above, so the data below lands in .rodata */
RESUME_RODATA_SECTION();

/* The double 1.0, which UpdateJointKeyframeSpans loads as two words, aligned as
 * a double is */
const u32 f_KeyframeOneHighWord __attribute__((aligned(8))) = 0x3FF00000;
const u32 f_KeyframeOneLowWord = 0;

INCLUDE_ASM("asm/nonmatchings/character", UpdateJointKeyframeSpans);
#endif

/*
 * Steps the slot's animation as AnimateCharacter does, without the debug text,
 * but poses the joints into the frame data copy the next frame writes, without
 * moving on from it
 */
void AnimateCharacterInSameCopy(u32 slotIndex) {
  AnimationEventAction actions[EVENT_ACTION_COUNT_MAX];
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  s32 i = 0;
  s32 actionCount;
  Boolean hasLooped;

  if (slot != NULL) {
    hasLooped = FALSE;
    MapModelBuffer(slot);
    actionCount = GetAnimationEventActions(
        slotIndex, slot->animationIndex, slot->animationFrame, actions);
    for (; i < actionCount; i++) {
      switch (actions[i].kind) {
        case ANIMATION_EVENT_LOOP_START:
          break;
        case ANIMATION_EVENT_LOOP_END:
          if (slot->isLooping) {
            hasLooped = TRUE;
            slot->animationFrame =
                GetAnimationEventFrame(slotIndex, slot->animationIndex,
                    ANIMATION_EVENT_LOOP_START) +
                ModelAnimations(slot->model)[slot->animationIndex].startFrame;
          }

          break;
        case ANIMATION_EVENT_TEXTURE_ANIMATION:
          if (actions[i].value == TEXTURE_ANIMATION_EVENT_START_RANDOM) {
            slot->playsRandomTextureAnimation = TRUE;
          } else if (actions[i].value == TEXTURE_ANIMATION_EVENT_STOP_RANDOM) {
            slot->playsRandomTextureAnimation = FALSE;
          } else {
            PlayTextureAnimation(slotIndex, actions[i].value);
          }

          break;
      }
    }

    if (!hasLooped) {
      slot->animationFrame += slot->animationSpeed;
    }

    if (slot->animationFrame > slot->animationEndFrame) {
      if (slot->nextAnimationId != NO_NEXT_ANIMATION) {
        SetCharacterAnimation(slotIndex, slot->nextAnimationId);
        slot->nextAnimationId = NO_NEXT_ANIMATION;
      } else {
        slot->animationFrame = slot->animationEndFrame;
      }
    }

    if (slot->animationFrame < slot->animationStartFrame) {
      slot->animationFrame = slot->animationStartFrame;
    }

    g_CharacterModelIndex = slot->modelIndex;
    g_CurrentAnimationFrame = slot->animationFrame;
    g_CurrentAnimationEndFrame = slot->animationEndFrame;
    g_CurrentAnimationStartFrame = slot->animationStartFrame;
    if (slot->playsRandomTextureAnimation) {
      if (slot->randomTextureAnimationDelay == 0) {
        PlayTextureAnimation(slotIndex, RANDOM_TEXTURE_ANIMATION);
        slot->randomTextureAnimationDelay =
            (rand() & RANDOM_TEXTURE_ANIMATION_DELAY_MASK) +
            RANDOM_TEXTURE_ANIMATION_DELAY_MIN;
      } else {
        slot->randomTextureAnimationDelay--;
      }
    }

    AdvanceTextureAnimations(slotIndex, 0, (s32)g_CurrentAnimationFrame % 10,
        g_CurrentAnimationFrame);
    g_CurrentJointMatrices = slot->frameData[slot->frameDataIndex];
    f_CurrentModelBuffer = slot->modelBuffer;
    AnimateCharacterJoints(slotIndex);
    slot->jointMatrices = g_CurrentJointMatrices;
  }
}

void AnimateAllCharacters(void) {
  s32 i;

  for (i = 0; i < CHARACTER_SLOT_COUNT; i++) {
    AnimateCharacter(i);
  }
}

s32 GetCharacterBufferSize(void) {
  return CHARACTER_BUFFER_SIZE;
}

void SetCharacterBuffer(s32 slotIndex, u8* buffer) {
  /* Aligned for SetMultiTlb */
  u32 alignedBuffer = RoundUpTo((u32)buffer, SMALLEST_PAGE_SIZE);
  s32 warningCount;
  s32 i;

  if (g_CharacterSlots[slotIndex].modelIndex != NO_CHARACTER_MODEL) {
    for (warningCount = 0; warningCount < WARNING_REPEAT_COUNT;
        warningCount++) {
      osSyncPrintf(
          "Polychara %d still alive. You can't change buffer.\n", slotIndex);
    }
  } else {
    g_CharacterSlots[slotIndex].modelBuffer = (void*)alignedBuffer;
    for (i = 0; i < FRAME_DATA_COPY_COUNT; i++) {
      g_CharacterSlots[slotIndex].frameData[i] =
          AddressPast(
              g_CharacterSlots[slotIndex].modelBuffer, FRAME_DATA_OFFSET) +
          (i * FRAME_DATA_COPY_SIZE);
    }
  }
}

void ResetCharacterSlots(void) {
  s32 i;

  FreeAllCharacterSlots();
  for (i = 0; i < CHARACTER_SLOT_COUNT; i++) {
    g_CharacterSlots[i].lastDrawnFrame = LONG_AGO_FRAME;
    SetCharacterBuffer(i, (u8*)DEFAULT_CHARACTER_BUFFER);
  }
}

/*
 * Sets the dry and rain character lights for the frame being drawn, both lit
 * along the scene's light direction. The dry colors change near the end of
 * the round.
 */
void SetCharacterLights(void) {
  u8 ambientColor[3];
  u8 lightColor[3];
  s8 lightDirection[3];
  Radians lightPitch;
  Radians lightYaw;

  switch (g_HoleSetup.conditions.sceneColorIndex) {
    case SCENE_COLOR_INDEX_ROUND_LATE:
    case SCENE_COLOR_INDEX_ROUND_END:
      ambientColor[0] = LATE_AMBIENT_LEVEL;
      ambientColor[1] = LATE_AMBIENT_LEVEL;
      ambientColor[2] = LATE_AMBIENT_LEVEL;
      lightColor[0] = LATE_LIGHT_RED;
      lightColor[1] = LATE_LIGHT_GREEN;
      lightColor[2] = LATE_LIGHT_BLUE;
      break;
    default:
      ambientColor[0] = DAY_AMBIENT_RED;
      ambientColor[1] = DAY_AMBIENT_GREEN;
      ambientColor[2] = DAY_AMBIENT_BLUE;
      lightColor[0] = DAY_LIGHT_RED;
      lightColor[1] = DAY_LIGHT_GREEN;
      lightColor[2] = DAY_LIGHT_BLUE;
      break;
  }

  lightPitch = g_HoleSetup.conditions.lightPitch * BINARY_ANGLE_TO_RADIANS;
  lightYaw = g_HoleSetup.conditions.lightYaw * BINARY_ANGLE_TO_RADIANS;
  lightDirection[0] =
      -sinf(lightPitch) * cosf(lightYaw) * LIGHT_DIRECTION_SCALE;
  lightDirection[1] = -cosf(lightPitch) * LIGHT_DIRECTION_SCALE;
  lightDirection[2] =
      -sinf(lightPitch) * sinf(lightYaw) * LIGHT_DIRECTION_SCALE;
  f_DryLights[g_DrawBufferIndex] =
      (Lights2)gdSPDefLights2(ambientColor[0], ambientColor[1], ambientColor[2],
          lightColor[0], lightColor[1], lightColor[2], lightDirection[0],
          lightDirection[1], lightDirection[2], lightColor[0], lightColor[1],
          lightColor[2], f_SecondLightDirection[AXIS_X],
          f_SecondLightDirection[AXIS_Y], f_SecondLightDirection[AXIS_Z]);
  f_RainLights[g_DrawBufferIndex] = (Lights2)gdSPDefLights2(RAIN_AMBIENT_LEVEL,
      RAIN_AMBIENT_LEVEL, RAIN_AMBIENT_LEVEL, RAIN_LIGHT_RED, RAIN_LIGHT_GREEN,
      RAIN_LIGHT_BLUE, lightDirection[0], lightDirection[1], lightDirection[2],
      RAIN_SECOND_LIGHT_LEVEL, RAIN_SECOND_LIGHT_LEVEL, RAIN_SECOND_LIGHT_LEVEL,
      f_SecondLightDirection[AXIS_X], f_SecondLightDirection[AXIS_Y],
      f_SecondLightDirection[AXIS_Z]);
}

/* Copies the ambient and first light into both of f_SavedLights */
#define SaveLights(lights) \
  f_SavedLights[0].l[0].l = (lights).l[0].l; \
  f_SavedLights[1].l[0].l = (lights).l[0].l; \
  f_SavedLights[0].a.l = (lights).a.l; \
  f_SavedLights[1].a.l = (lights).a.l

/*
 * Draws the character in the slot with its lights, alpha and culling
 *
 * Doesn't match: 113 words differ.
 */
#ifdef NON_MATCHING
void DrawCharacter(u32 slotIndex) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);
  Gfx* gfx = g_DisplayListHead;
  Lights2* lights;

  f_DrawParity ^= 1;
  if (f_IsMatrixSetupPending) {
    f_IsMatrixSetupPending = FALSE;
    guScale(&f_ModelScaleMatrix, MODEL_SCALE, MODEL_SCALE, MODEL_SCALE);
    guRotate(&f_RollMatrix, HALF_TURN_DEGREES, 0.0f, 0.0f, 1.0f);
    guRotate(&f_HeadingMatrix,
        (f_DrawParity * DEGREES_PER_TURN / BINARY_ANGLE_TURN) +
            HALF_TURN_DEGREES,
        0.0f, 1.0f, 0.0f);
  }

  if (slot != NULL) {
    if (g_ImpactFlashState == IMPACT_FLASH_NONE) {
      switch (slot->lighting) {
        case CHARACTER_LIGHTING_FIXED:
          gSPSetLights2(gfx++, f_FixedLights);
          SaveLights(f_FixedLights);
          break;
        default:
        case CHARACTER_LIGHTING_SCENE:
          if (g_HoleSetup.conditions.isRaining) {
            gSPSetLights2(gfx++, f_RainLights[g_DrawBufferIndex]);
            SaveLights(f_RainLights[g_DrawBufferIndex]);
          } else {
            gSPSetLights2(gfx++, f_DryLights[g_DrawBufferIndex]);
            SaveLights(f_DryLights[g_DrawBufferIndex]);
          }

          break;
        case CHARACTER_LIGHTING_SCENE_THIRD:
          lights =
              g_HoleSetup.conditions.isRaining ? f_RainLights : f_DryLights;
          if (!AddReactionLight(&gfx, &lights[g_DrawBufferIndex])) {
            IN_OWN_BLOCK(gSPSetLights2(gfx++, lights[g_DrawBufferIndex]));
            SaveLights(lights[g_DrawBufferIndex]);
          }

          break;
        case CHARACTER_LIGHTING_BLACK:
          gSPSetLights2(gfx++, f_BlackLights);
          SaveLights(f_BlackLights);
          break;
      }
    } else {
      gSPSetLights2(gfx++, f_SavedLights[g_DrawBufferIndex]);
    }

    MapModelBuffer(slot);
    gSPMatrix(
        gfx++, &f_ModelScaleMatrix, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
    gSPSegment(
        gfx++, MODEL_BUFFER_SEGMENT, osVirtualToPhysical(slot->modelBuffer));
    gSPSegment(gfx++, JOINT_MATRICES_SEGMENT,
        osVirtualToPhysical(slot->jointMatrices));
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_DISABLE);
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, BLEND_ALPHA);
    gSPSegment(gfx++, ENV_COLOR_SEGMENT,
        osVirtualToPhysical(g_CharacterEnvColorDisplayLists));
    gSPSegment(gfx++, HILITE_SEGMENT,
        osVirtualToPhysical(g_CharacterHiliteDisplayList));
    gSPSegment(gfx++, RENDER_MODE_SEGMENT,
        osVirtualToPhysical(g_CharacterRenderModeDisplayList));
    gSPSegment(gfx++, COMBINE_SEGMENT,
        osVirtualToPhysical(g_CharacterCombineDisplayList));
    gSPSegment(
        gfx++, MIRROR_TEXTURE_SEGMENT, osVirtualToPhysical(g_MirrorTexture));
    gSPSegment(
        gfx++, FLAME_TEXTURE_SEGMENT, osVirtualToPhysical(g_FlameTexture));
    gSPSegment(
        gfx++, BLENDED_TEXTURE_SEGMENT, osVirtualToPhysical(g_BlendedTexture));
    if (slot->isTranslucent) {
      gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0xFF, slot->alpha);
      SetCharacterAlpha(slot->alpha);
      gSPSegment(gfx++, RENDER_MODES_SEGMENT,
          osVirtualToPhysical(f_TranslucentRenderModes));
    } else {
      gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0xFF, BLEND_ALPHA);
      SetCharacterAlpha(OPAQUE_ALPHA);
      gSPSegment(gfx++, RENDER_MODES_SEGMENT,
          osVirtualToPhysical(f_OpaqueRenderModes));
    }

    gSPSegment(gfx++, CULLING_SEGMENT,
        osVirtualToPhysical(slot->unk188 ? f_FrontCulling : f_BackCulling));
    gSPDisplayList(gfx++, slot->displayList);
    slot->lastDrawnFrame = g_FrameCount;
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
  }

  g_DisplayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/character", DrawCharacter);
#endif

/* Places the character in the slot with PlaceGolferAtBall, turned to the
 * current shot's direction */
void PlaceCharacterForShot(u32 slotIndex) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    PlaceGolferAtBall(slot->position, slot->rotation, slot->scale);
    slot->rotation[AXIS_Y] = ((-g_CurrentShot.heading) & BINARY_ANGLE_MASK) *
        BINARY_ANGLE_TO_RADIANS;
  }
}

/*
 * Places the character in the slot with PlaceAtReactionSpot, then points the
 * first camera at it from VIEW_DISTANCE away along its facing
 */
void PlaceAndViewCharacter(u32 slotIndex) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    PlaceAtReactionSpot(slot->position, slot->rotation, slot->scale);
    g_Cameras[0].view.lookAt[AXIS_X] =
        slot->position[AXIS_X] * COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.lookAt[AXIS_Y] =
        (slot->position[AXIS_Y] - VIEW_DROP) * COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.lookAt[AXIS_Z] =
        slot->position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.eye[AXIS_X] =
        (slot->position[AXIS_X] +
            (cosf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) *
                VIEW_DISTANCE)) *
        COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.eye[AXIS_Y] =
        (slot->position[AXIS_Y] - VIEW_DROP) * COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.eye[AXIS_Z] =
        (slot->position[AXIS_Z] +
            (sinf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) *
                VIEW_DISTANCE)) *
        COURSE_TO_WORLD_SCALE;
  }
}

/* Stands the character in the slot on the ground at the point, facing the
 * heading, at full size */
void PlaceCharacter(u32 slotIndex, CoursePoint point, Radians heading) {
  CharacterSlot* slot = GetUsedCharacterSlot(slotIndex);

  if (slot != NULL) {
    slot->position[AXIS_X] = point[AXIS_X];
    slot->position[AXIS_Z] = point[AXIS_Z];
    slot->position[AXIS_Y] =
        GetGroundHeight(slot->position[AXIS_X], slot->position[AXIS_Z]);
    slot->rotation[AXIS_X] = 0.0f;
    slot->rotation[AXIS_Z] = 0.0f;
    slot->rotation[AXIS_Y] = -heading;
    slot->scale[AXIS_X] = 1.0f;
    slot->scale[AXIS_Y] = 1.0f;
    slot->scale[AXIS_Z] = 1.0f;
  }
}

/*
 * Whether DrawCharacterAndShadow draws for a camera: CAMERA_UNK2, or any
 * but the inset while g_AreCharactersDrawn is set. It is a macro, as an inline
 * function compiles differently from the original.
 */
#define IsCharacterDrawnForCamera(cameraIndex) \
  (((cameraIndex) == CAMERA_UNK2) || \
      (((cameraIndex) != INSET_CAMERA) && g_AreCharactersDrawn))

/*
 * Draws the character in the slot and its shadow, for cameraIndex's view.
 * The shadow lies on the ground plane under three points around the
 * character's shadow point.
 *
 * Doesn't match: 37 words differ.
 */
#ifdef NON_MATCHING
void DrawCharacterAndShadow(
    CameraMatrices* matrices, s32 cameraIndex, s32 slotIndex) {
  f32 shadowPoint[3];
  f32 corners[TRIANGLE_CORNER_COUNT][3];
  f32 viewDirection[3];
  s32 lightDirection[3];
  Mat4 transform;
  Mat4 rotation;
  Mat4 translation;
  Mat4 alignment;
  Mat4 shadow;
  PlaneTriangle triangle;
  TrianglePlane plane;
  Mat4 projection;
  Mat4 unusedMatrix UNUSED;
  Gfx* gfx;
  CharacterSlot* slot;
  f32 shadowScale;

  gfx = g_DisplayListHead;
  if (IsCharacterDrawnForCamera(cameraIndex)) {
    slot = GetUsedCharacterSlot(slotIndex);
    if (slot == NULL) {
      return;
    }

    viewDirection[AXIS_X] =
        g_Cameras[0].view.lookAt[AXIS_X] - g_Cameras[0].view.eye[AXIS_X];
    viewDirection[AXIS_Y] = 0.0f;
    viewDirection[AXIS_Z] =
        g_Cameras[0].view.lookAt[AXIS_Z] - g_Cameras[0].view.eye[AXIS_Z];
    if (fabsf(viewDirection[AXIS_X]) + fabsf(viewDirection[AXIS_Z]) >= 1.0f) {
      guNormalize(&viewDirection[AXIS_X], &viewDirection[AXIS_Y],
          &viewDirection[AXIS_Z]);
      viewDirection[AXIS_X] *= SECOND_LIGHT_DIRECTION_LENGTH;
      viewDirection[AXIS_Y] *= SECOND_LIGHT_DIRECTION_LENGTH;
      viewDirection[AXIS_Z] *= SECOND_LIGHT_DIRECTION_LENGTH;
      lightDirection[AXIS_X] = -viewDirection[AXIS_Z];
      f_SecondLightDirection[AXIS_X] = lightDirection[AXIS_X];
      f_SecondLightDirection[AXIS_Y] = 0;
      lightDirection[AXIS_Z] = viewDirection[AXIS_X];
      f_SecondLightDirection[AXIS_Z] = lightDirection[AXIS_Z];
      SetCharacterLights();
    }

    if (g_ImpactFlashState != IMPACT_FLASH_NONE) {
      f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.col[0] = g_ImpactFlashLevel;
      f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.col[1] = g_ImpactFlashLevel;
      f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.col[2] =
          g_ImpactFlashLevel / 2;
      f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.colc[0] = g_ImpactFlashLevel;
      f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.colc[1] = g_ImpactFlashLevel;
      f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.colc[2] =
          g_ImpactFlashLevel / 2;
      f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.dir[0] =
          sinf(g_Players[g_CurrentShot.playerIndex].unk22
                  ? slot->rotation[1]
                  : -slot->rotation[1]) *
          LIGHT_DIRECTION_SCALE;
      f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.dir[1] = 0;
      f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.dir[2] =
          -cosf(g_Players[g_CurrentShot.playerIndex].unk22
                  ? slot->rotation[1]
                  : -slot->rotation[1]) *
          LIGHT_DIRECTION_SCALE;
    }

    if (slot->reusesLastMatrices) {
      f_ModelMatrices[slotIndex][g_DrawBufferIndex] =
          f_ModelMatrices[slotIndex][g_DrawBufferIndex ^ 1];
    } else {
      guTranslateF(transform, slot->position[AXIS_X] * COURSE_TO_WORLD_SCALE,
          slot->position[AXIS_Y] * COURSE_TO_WORLD_SCALE,
          slot->position[AXIS_Z] * COURSE_TO_WORLD_SCALE);
      guRotateRPYF(rotation,
          (s32)(ROLL_ORIGIN_DEGREES -
              (s32)(slot->rotation[0] * RADIANS_TO_DEGREES)),
          (s32)(slot->rotation[1] * RADIANS_TO_DEGREES),
          (s32)(slot->rotation[2] * RADIANS_TO_DEGREES));
      guMtxCatF(rotation, transform, transform);
      guScaleF(rotation, slot->scale[0], slot->scale[1], slot->scale[2]);
      guMtxCatF(rotation, transform, transform);
      guMtxF2L(transform, &f_ModelMatrices[slotIndex][g_DrawBufferIndex]);
      GetCharacterShadowPoint(slotIndex, shadowPoint);
      guMtxXFMF(transform, shadowPoint[AXIS_X] + SHADOW_CORNER_DISTANCE, 0.0f,
          shadowPoint[AXIS_Z], &corners[0][AXIS_X], &corners[0][AXIS_Y],
          &corners[0][AXIS_Z]);
      guMtxXFMF(transform, shadowPoint[AXIS_X] - SHADOW_CORNER_DISTANCE, 0.0f,
          shadowPoint[AXIS_Z] - SHADOW_CORNER_DISTANCE, &corners[1][AXIS_X],
          &corners[1][AXIS_Y], &corners[1][AXIS_Z]);
      guMtxXFMF(transform, shadowPoint[AXIS_X] - SHADOW_CORNER_DISTANCE, 0.0f,
          shadowPoint[AXIS_Z] + SHADOW_CORNER_DISTANCE, &corners[2][AXIS_X],
          &corners[2][AXIS_Y], &corners[2][AXIS_Z]);
      guMtxXFMF(transform, shadowPoint[AXIS_X], 0.0f, shadowPoint[AXIS_Z],
          &shadowPoint[AXIS_X], &shadowPoint[AXIS_Y], &shadowPoint[AXIS_Z]);
    }

    if (slot->reusesLastMatrices) {
      f_ShadowMatrices[slotIndex][g_DrawBufferIndex] =
          f_ShadowMatrices[slotIndex][g_DrawBufferIndex ^ 1];
    } else {
      shadowPoint[AXIS_X] *= WORLD_TO_COURSE_SCALE;
      shadowPoint[AXIS_Z] *= WORLD_TO_COURSE_SCALE;
      shadowPoint[AXIS_Y] =
          GetGroundHeight(shadowPoint[AXIS_X], shadowPoint[AXIS_Z]);
      shadowScale = slot->shadowScale * SHADOW_SCALE_UNIT;
      triangle.corners[AXIS_X][0] = corners[0][AXIS_X];
      triangle.corners[AXIS_Y][0] =
          GetGroundHeight(corners[0][AXIS_X] * WORLD_TO_COURSE_SCALE,
              corners[0][AXIS_Z] * WORLD_TO_COURSE_SCALE) *
          COURSE_TO_WORLD_SCALE;
      triangle.corners[AXIS_Z][0] = corners[0][AXIS_Z];
      triangle.corners[AXIS_X][1] = corners[1][AXIS_X];
      triangle.corners[AXIS_Y][1] =
          GetGroundHeight(corners[1][AXIS_X] * WORLD_TO_COURSE_SCALE,
              corners[1][AXIS_Z] * WORLD_TO_COURSE_SCALE) *
          COURSE_TO_WORLD_SCALE;
      triangle.corners[AXIS_Z][1] = corners[1][AXIS_Z];
      triangle.corners[AXIS_X][2] = corners[2][AXIS_X];
      triangle.corners[AXIS_Y][2] =
          GetGroundHeight(corners[2][AXIS_X] * WORLD_TO_COURSE_SCALE,
              corners[2][AXIS_Z] * WORLD_TO_COURSE_SCALE) *
          COURSE_TO_WORLD_SCALE;
      triangle.corners[AXIS_Z][2] = corners[2][AXIS_Z];
      GetTrianglePlane(&triangle, &plane);
      guTranslateF(translation, shadowPoint[AXIS_X] * COURSE_TO_WORLD_SCALE,
          shadowPoint[AXIS_Y] * COURSE_TO_WORLD_SCALE,
          shadowPoint[AXIS_Z] * COURSE_TO_WORLD_SCALE);
      /* guAlignF leaves the identity matrix for a vertical direction */
      if (fabsf(plane.normal[AXIS_Y]) > LEVEL_GROUND_NORMAL_Y) {
        guRotateF(alignment, QUARTER_TURN_DEGREES, 1.0f, 0.0f, 0.0f);
      } else {
        guAlignF(alignment, 0.0f, plane.normal[AXIS_X], plane.normal[AXIS_Y],
            plane.normal[AXIS_Z]);
      }

      MultiplyMatrices(alignment, translation, shadow);
      guScaleF(alignment, shadowScale, shadowScale, SHADOW_DEPTH_SCALE);
      MultiplyMatrices(alignment, shadow, shadow);
      ConvertMatrixToFixed(
          shadow, &f_ShadowMatrices[slotIndex][g_DrawBufferIndex]);
    }

    /* Draws the shadow a little nearer the camera than the ground */
    guMtxL2F(projection, &matrices->projection);
    projection[3][2] -= SHADOW_DEPTH_OFFSET;
    guMtxF2L(projection, &f_ShadowProjections[slotIndex][g_DrawBufferIndex]);
    gSPMatrix(gfx++, &f_ShadowProjections[slotIndex][g_DrawBufferIndex],
        G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(
        gfx++, G_ZBUFFER | G_SHADE | G_CULL_FRONT | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, SHADOW_ALPHA_THRESHOLD);
    SetSceneFog(&gfx);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_PASS, G_RM_ZB_CLD_SURF2);
    gDPSetEnvColor(gfx++, 0, 0, 0, SHADOW_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_FOG | G_SHADING_SMOOTH);
    gDPLoadTextureBlock(gfx++, ShadowTexturePixels(g_ShadowTexture), G_IM_FMT_I,
        G_IM_SIZ_8b, SHADOW_TEXTURE_SIZE, SHADOW_TEXTURE_SIZE, 0,
        G_TX_MIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP,
        SHADOW_TEXTURE_SIZE_LOG2, SHADOW_TEXTURE_SIZE_LOG2, G_TX_NOLOD,
        G_TX_NOLOD);
    gDPSetCombineLERP(gfx++, 0, 0, 0, 0, ENVIRONMENT, 0, TEXEL0, 0, 0, 0, 0, 0,
        0, 0, 0, COMBINED);
    gDPPipeSync(gfx++);
    gDPSetTextureLOD(gfx++, G_TL_TILE);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gSPMatrix(gfx++, &f_ShadowMatrices[slotIndex][g_DrawBufferIndex],
        G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
    gSPVertex(gfx++, f_ShadowVertices, ARRAY_COUNT(f_ShadowVertices), 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gSPMatrix(gfx++, &matrices->projection,
        G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gfx++, &f_ModelMatrices[slotIndex][g_DrawBufferIndex],
        G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
    g_DisplayListHead = gfx;
    DrawCharacter(slotIndex);
    gfx = g_DisplayListHead;
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    slot->reusesLastMatrices = FALSE;
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    g_DisplayListHead = gfx;
  }
}

#else
INCLUDE_ASM("asm/nonmatchings/character", DrawCharacterAndShadow);
#endif
