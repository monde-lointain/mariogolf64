#include "character.h"

#include <stdlib.h>
#include <string.h>
#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "camera.h"
#include "character.h"
#include "character_slot.h"
#include "club.h"
#include "compiler.h"
#include "course_space.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "debug_text.h"
#include "game.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "heap.h"
#include "hole_conditions.h"
#include "hole_loader.h"
#include "host_file.h"
#include "language.h"
#include "main.h"
#include "math_util.h"
#include "play.h"
#include "player.h"
#include "reaction.h"
#include "screen.h"
#include "shot_camera.h"
#include "shot_effects.h"
#include "terrain.h"

#define CHARACTER_BUFFER_SIZE 0x23680
#define FRAME_DATA_OFFSET 0x20000
#define FRAME_DATA_COPY_SIZE 0x780
#define DEFAULT_CHARACTER_BUFFER 0x8030F800
#define WARNING_REPEAT_COUNT 10
#define LONG_AGO_FRAME (-4)

static inline CharacterAnimation *ModelAnimations(const CharacterModel *model) {
    u32 address = model->segmentAddresses[CHARACTER_MODEL_ANIMATIONS];

    return (CharacterAnimation *)address;
}

#define ModelJointAnimations(model) \
    ((JointAnimation *)(model)->segmentAddresses[CHARACTER_MODEL_JOINTS])
#define ModelAnimatedTriangles(model) \
    ((AnimatedTriangle **)(model)->segmentAddresses[CHARACTER_MODEL_ANIMATED_TRIANGLES])
#define ModelTextureAnimations(model) \
    ((TextureAnimation *)(model)->segmentAddresses[CHARACTER_MODEL_TEXTURE_ANIMATIONS])
#define ModelAnimatedTextures(model) \
    ((AnimatedTexture *)(model)->segmentAddresses[CHARACTER_MODEL_TEXTURES])

#define FrameDataAddress(slot, address) \
    (SEGMENT_OFFSET(address) + (u32)(slot)->frameData[(slot)->frameDataIndex])

#define SetTriangleTexture(slot, triangle, texture, sCoordinates, tCoordinates, vertexIndex) \
    { \
        gfx = (Gfx *) \
            FrameDataAddress(slot, (triangle)->loadTextureBlock + LOAD_TEXTURE_BLOCK_OFFSET); \
        gDPLoadTextureBlock( \
            gfx++, \
            (texture)->address & ~(TEXTURE_ALIGNMENT - 1), \
            G_IM_FMT_RGBA, \
            G_IM_SIZ_16b, \
            (texture)->width, \
            (texture)->height, \
            0, \
            G_TX_NOMIRROR | G_TX_CLAMP, \
            G_TX_NOMIRROR | G_TX_CLAMP, \
            ANIMATED_TEXTURE_MASK, \
            ANIMATED_TEXTURE_MASK, \
            G_TX_NOLOD, \
            G_TX_NOLOD \
        ); \
        for ((vertexIndex) = 0; (vertexIndex) < TRIANGLE_VERTEX_COUNT; (vertexIndex)++) { \
            vertex = \
                (Vtx *)FrameDataAddress(slot, ((vertexIndex) * VTX_SIZE) + (triangle)->vertices); \
            vertex->v.tc[TEXTURE_AXIS_S] = (sCoordinates)[(vertexIndex)]; \
            vertex->v.tc[TEXTURE_AXIS_T] = (tCoordinates)[(vertexIndex)]; \
        } \
    }

#define VTX_SIZE 0x10
#define LOAD_TEXTURE_BLOCK_OFFSET 0x10
#define TEXTURE_ALIGNMENT 8
#define ANIMATED_TEXTURE_MASK 6

#define MapModelBuffer(slot) \
    SetMultiTlb( \
        0, \
        TLB_PAGE_MODE_4K, \
        CHARACTER_MODEL_PAGE_COUNT, \
        CHARACTER_MODEL_ADDRESS, \
        osVirtualToPhysical((slot)->modelBuffer) \
    )

static inline Boolean FindUsedSlot(u32 slotIndex, CharacterSlot **slot) {
    *slot = GetUsedCharacterSlot(slotIndex);
    return *slot != NULL;
}

#define BALL_DISTANCE_WOOD_APPROACH 4
#define BALL_DISTANCE_COUNT 5

#define WOOD_BALL_DISTANCE 0x3600
#define IRON_BALL_DISTANCE 0x2400
#define PUTTER_BALL_DISTANCE 0x1800

#define WOOD_DISTANCE_POINT 3
#define IRON_DISTANCE_POINT 4
#define WEDGE_DISTANCE_POINT 5
#define PUTTER_DISTANCE_POINT 6

#define POINT_DEPTH_TO_BALL_DISTANCE 1228.8f

#define WOOD_DISTANCE_ANIMATION 0
#define IRON_DISTANCE_ANIMATION 3
#define WEDGE_DISTANCE_ANIMATION 6
#define PUTTER_DISTANCE_ANIMATION 9
#define WOOD_APPROACH_DISTANCE_ANIMATION 2

#define POINT_POSITION_TEXT_COLUMN 2
#define POINT_POSITION_TEXT_ROW 13

#define BALL_DISTANCE_SOURCE_SIZE 0x8000
#define BALL_DISTANCE_LINE_SIZE 0x100
#define WOOD_DISPLAY_LIST_CHOICE 0
#define IRON_DISPLAY_LIST_CHOICE 1
#define WEDGE_DISPLAY_LIST_CHOICE 2
#define PUTTER_DISPLAY_LIST_CHOICE 3

#define IMPACT_EVENT_ANIMATION_INDEX 2

#define VIEW_DISTANCE 61440.0f
#define VIEW_DROP 15360.0f

#define LOADED_ALPHA 0x60
#define LOADED_RANDOM_TEXTURE_ANIMATION_DELAY 27

#define LATE_AMBIENT_LEVEL 120
#define LATE_LIGHT_RED 200
#define LATE_LIGHT_GREEN 80
#define LATE_LIGHT_BLUE 10
#define DAY_AMBIENT_RED 130
#define DAY_AMBIENT_GREEN 130
#define DAY_AMBIENT_BLUE 120
#define DAY_LIGHT_RED 130
#define DAY_LIGHT_GREEN 120
#define DAY_LIGHT_BLUE 110
#define RAIN_AMBIENT_LEVEL 100
#define RAIN_LIGHT_RED 155
#define RAIN_LIGHT_GREEN 120
#define RAIN_LIGHT_BLUE 120
#define RAIN_SECOND_LIGHT_LEVEL 150

static Mtx f_ModelScaleMatrix;
static Mtx f_RollMatrix;
static Mtx f_HeadingMatrix;

static u8 f_UnusedBufferAfterHeadingMatrix[120] UNUSED;

#define MODEL_SCALE 0.01f
#define HALF_TURN_DEGREES 180
#define BLEND_ALPHA 0x80
#define MODEL_RENDER_MODE_TRANSLUCENT (G_RM_ZB_XLU_SURF | Z_UPD)
#define MODEL_RENDER_MODE_TRANSLUCENT2 (G_RM_ZB_XLU_SURF2 | Z_UPD)

static Mtx f_ModelMatrices[CHARACTER_SLOT_COUNT][DRAW_BUFFER_COUNT];
static Mtx f_ShadowMatrices[CHARACTER_SLOT_COUNT][DRAW_BUFFER_COUNT];
static Mtx f_ShadowProjections[CHARACTER_SLOT_COUNT][DRAW_BUFFER_COUNT];
u8 *g_ShadowTexture;
ImportedCharacter g_ImportedCharacters[IMPORTED_CHARACTER_COUNT];

FILE_STATIC s32 g_CurrentAnimationStartFrame;

#define SHADOW_QUAD_HALF_SIZE 1500
#define SHADOW_QUAD_TEXEL_SPAN TexelCoordinate(SHADOW_TEXTURE_SIZE * 2)
#define SECOND_LIGHT_DIRECTION_LENGTH 64.0f
#define SHADOW_CORNER_DISTANCE 30.0f
#define SHADOW_SCALE_UNIT 0.0001f
#define SHADOW_DEPTH_SCALE 0.01f
#define SHADOW_DEPTH_OFFSET 6.0f
#define SHADOW_ALPHA 0x80
#define SHADOW_ALPHA_THRESHOLD 0x10
#define LEVEL_GROUND_NORMAL_Y 0.999f
#define QUARTER_TURN_DEGREES 90.0f

#define MODEL_BUFFER_SEGMENT 0x02
#define JOINT_MATRICES_SEGMENT 0x03
#define RENDER_MODES_SEGMENT 0x04
#define CULLING_SEGMENT 0x05
#define ENV_COLOR_SEGMENT 0x08
#define HIGHLIGHT_SEGMENT 0x09
#define RENDER_MODE_SEGMENT 0x0A
#define COMBINE_SEGMENT 0x0B
#define MIRROR_TEXTURE_SEGMENT 0x0C
#define FLAME_TEXTURE_SEGMENT 0x0D
#define BLENDED_TEXTURE_SEGMENT 0x0E

#define DEBUG_LINE_SIZE 0x80

#define ANIMATION_FRAME_TEXT_COLUMN 10
#define ANIMATION_FRAME_TEXT_FIRST_ROW 5

#define RANDOM_TEXTURE_ANIMATION_DELAY_MIN 30
#define RANDOM_TEXTURE_ANIMATION_DELAY_MASK 0x1F

#define MEASURED_SLOT 0
#define MEASURED_SHOT_MODE SHOT_MODE_NORMAL

#define BALL_DISTANCE_SOURCE_ASSERT_LINE 1525
#define TEXTURE_ANIMATION_ASSERT_LINE 1956

ImpactFlashState g_ImpactFlashState = IMPACT_FLASH_NONE;
s32 g_ImpactFlashLevel = 0;
static Gfx f_SurfaceRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2),
    gsSPEndDisplayList(),
};
static Gfx f_BlendedRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(MODEL_RENDER_MODE_TRANSLUCENT, MODEL_RENDER_MODE_TRANSLUCENT2),
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
static Gfx f_TranslucentSurfaceRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(MODEL_RENDER_MODE_TRANSLUCENT, MODEL_RENDER_MODE_TRANSLUCENT2),
    gsSPEndDisplayList(),
};
static Gfx f_TranslucentBlendedRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(MODEL_RENDER_MODE_TRANSLUCENT, MODEL_RENDER_MODE_TRANSLUCENT2),
    gsSPEndDisplayList(),
};
static Gfx f_TranslucentDecalRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(MODEL_RENDER_MODE_TRANSLUCENT, MODEL_RENDER_MODE_TRANSLUCENT2),
    gsSPEndDisplayList(),
};
static Gfx f_TranslucentTextureEdgeRenderMode[] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(MODEL_RENDER_MODE_TRANSLUCENT, MODEL_RENDER_MODE_TRANSLUCENT2),
    gsSPEndDisplayList(),
    gsSPEndDisplayList(),
};
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
    gsDPSetCombineLERP(
        PRIMITIVE,
        0,
        SHADE,
        0,
        ENVIRONMENT,
        0,
        SHADE,
        0,
        ENVIRONMENT,
        COMBINED,
        TEXEL0,
        COMBINED,
        0,
        0,
        0,
        COMBINED
    ),
    gsSPEndDisplayList(),
};
Gfx g_CharacterHighlightDisplayList[3] = {
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_PASS, G_RM_AA_ZB_OPA_SURF2),
    gsSPEndDisplayList(),
};
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
static s32 f_UnusedWordBeforeModelIndex UNUSED = 1;
s32 g_CharacterModelIndex = 1;
static s32 f_StartedAnimationIndex = 0;
static void *f_CurrentModelBuffer = NULL;
static Gfx f_UnusedCharacterEndDisplayList[] UNUSED = {
    gsSPEndDisplayList(),
};
static Lights2 f_FixedLights =
    gdSPDefLights2(0x6E, 0x6E, 0x6E, 0x9B, 0x78, 0x78, 0, 100, 100, 0x96, 0x96, 0x96, 120, 0, 0);
static Lights2 f_BlackLights = gdSPDefLights2(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
static Lights2 f_RainLights[DRAW_BUFFER_COUNT] = {
    gdSPDefLights2(0x64, 0x64, 0x64, 0x9B, 0x78, 0x78, 0, -80, 80, 0x96, 0x96, 0x96, 80, -80, 0),
    gdSPDefLights2(0x64, 0x64, 0x64, 0x9B, 0x78, 0x78, 0, -80, 80, 0x96, 0x96, 0x96, 80, -80, 0),
};
static Lights2 f_DryLights[DRAW_BUFFER_COUNT] = {
    gdSPDefLights2(
        0x64,
        0x64,
        0x64,
        0x9B,
        0x78,
        0x78,
        0,
        -120,
        120,
        0x96,
        0x96,
        0x96,
        120,
        -120,
        0
    ),
    gdSPDefLights2(
        0x64,
        0x64,
        0x64,
        0x9B,
        0x78,
        0x78,
        0,
        -120,
        120,
        0x96,
        0x96,
        0x96,
        120,
        -120,
        0
    ),
};
static Lights2 f_UnusedCharacterLights[DRAW_BUFFER_COUNT] UNUSED = {
    gdSPDefLights2(0x64, 0x64, 0x64, 0x9B, 0x78, 0x78, 0, -120, 20, 0x96, 0x96, 0x96, 20, -120, 0),
    gdSPDefLights2(
        0x78,
        0x5A,
        0x5A,
        0xB9,
        0x78,
        0x78,
        0,
        -120,
        120,
        0xB4,
        0x96,
        0x96,
        120,
        -120,
        0
    ),
};
static Lights2 f_SavedLights[DRAW_BUFFER_COUNT] = {
    gdSPDefLights2(0x20, 0x20, 0x20, 0xFF, 0xDC, 0xDC, 0, -127, 0, 0xFF, 0xDC, 0xDC, 126, 0, 1),
    gdSPDefLights2(0x20, 0x20, 0x20, 0xFF, 0xDC, 0xDC, 0, -127, 0, 0xFF, 0xDC, 0xDC, 126, 0, 1),
};
static s32 f_SecondLightDirection[AXIS_COUNT] = {0, 0, 0};
static Boolean f_IsMatrixSetupPending = TRUE;
static s32 f_DrawParity = 0;
static s32 f_UnusedWordAfterDrawParity UNUSED = 0;
static Vtx f_ShadowVertices[4] = {
    {
        {
            {-SHADOW_QUAD_HALF_SIZE, SHADOW_QUAD_HALF_SIZE, 0},
            0,
            {SHADOW_QUAD_TEXEL_SPAN, SHADOW_QUAD_TEXEL_SPAN},
            {0x96, 0x96, 0x96, 0xFF},
        },
    },
    {
        {
            {SHADOW_QUAD_HALF_SIZE, SHADOW_QUAD_HALF_SIZE, 0},
            0,
            {0, SHADOW_QUAD_TEXEL_SPAN},
            {0xFF, 0xFF, 0xFF, 0xFF},
        },
    },
    {{{SHADOW_QUAD_HALF_SIZE, -SHADOW_QUAD_HALF_SIZE, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x00}}},
    {
        {
            {-SHADOW_QUAD_HALF_SIZE, -SHADOW_QUAD_HALF_SIZE, 0},
            0,
            {SHADOW_QUAD_TEXEL_SPAN, 0},
            {0x96, 0x96, 0x96, 0x00},
        },
    },
};

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

s32 GetBallDistance(u32 slotIndex, s32 club, Boolean isClubCategory, ShotMode shotMode) {
    f32 position[3];
    char text[DEBUG_LINE_SIZE];
    s32 distance;
    s32 pointId = WOOD_DISTANCE_POINT;
    ClubCategory column;
    CharacterSlot *slot;
    ClubCategory clubCategory;
    f32 depth;

    slot = GetUsedCharacterSlot(slotIndex);
    column = CLUB_CATEGORY_WOOD;
    if (!isClubCategory) {
        clubCategory = GetCharacterClubCategory(slotIndex, club);
    }
    else {
        clubCategory = club;
    }

    switch (clubCategory) {
#ifdef AVOID_UB
        default:
#endif
        case CLUB_CATEGORY_WOOD:
            distance = WOOD_BALL_DISTANCE;
            pointId = WOOD_DISTANCE_POINT;
            column = (shotMode == SHOT_MODE_APPROACH) * BALL_DISTANCE_WOOD_APPROACH;
            break;
        case CLUB_CATEGORY_IRON:
            distance = IRON_BALL_DISTANCE;
            pointId = IRON_DISTANCE_POINT;
            column = CLUB_CATEGORY_IRON;
            break;
        case CLUB_CATEGORY_WEDGE:
            distance = IRON_BALL_DISTANCE;
            pointId = WEDGE_DISTANCE_POINT;
            column = CLUB_CATEGORY_WEDGE;
            break;
        case CLUB_CATEGORY_PUTTER:
            distance = PUTTER_BALL_DISTANCE;
            pointId = PUTTER_DISTANCE_POINT;
            column = CLUB_CATEGORY_PUTTER;
            break;
    }

    if (!IsGameFlagSet(GAME_FLAG_GENERATE_BALL_DISTANCES) &&
        slot != NULL &&
        slot->modelIndex >= 0 &&
        ModelCharacter(slot->modelIndex) < CHARACTER_COUNT) {
        distance = f_BallDistances[ModelCharacter(slot->modelIndex)][column];
    }
    else if (GetCharacterPointModelPosition(slotIndex, pointId, position) == 0) {
        sprintf(text, "pres %f,%f,%f", position[AXIS_X], position[AXIS_Y], position[AXIS_Z]);
        PrintDebugText(text, POINT_POSITION_TEXT_COLUMN, POINT_POSITION_TEXT_ROW);
        depth = position[AXIS_Z] * POINT_DEPTH_TO_BALL_DISTANCE;
        if (depth < 0.0f) {
            depth = -depth;
        }

        distance = depth;
        osSyncPrintf(":");
    }
    else {
        osSyncPrintf("]");
    }

    {
        UNUSED_FUNCTION f32 GetVectorLength(f32 x, f32 y, f32 z) {
            return sqrtf(x * x + y * y + z * z);
        }
    }

    return distance;
}

static const char f_AssertionExpression[] = "EX";
static const char f_FileName[] = "c:/usgolf64/polychara/pcsub.c";

void WriteBallDistances(void) {
    char line[BALL_DISTANCE_LINE_SIZE];
    char *cursor;
    char *end;
    char *source;
    s32 modelIndex;
    s32 column;
    s32 animationId;
    ClubCategory clubCategory;

    void AppendDistanceText(char *text) {
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
        ReportFailedAssertion(f_AssertionExpression, f_FileName, BALL_DISTANCE_SOURCE_ASSERT_LINE);
    }

    sprintf(
        line,
        "/* Caution! This file is automatically generated by pcsub.c*/\n"
        "static const DISTPARA distp[PLAYABLE_NUM][5] = {\n"
    );
    AppendDistanceText(line);
    for (modelIndex = 0; modelIndex < CHARACTER_COUNT; modelIndex++) {
        s32 distances[BALL_DISTANCE_COUNT];

        LoadCharacter(MEASURED_SLOT, modelIndex, 0);
        g_CharacterModelIndex = g_CharacterSlots[MEASURED_SLOT].modelIndex;
        for (column = 0; column < BALL_DISTANCE_COUNT; column++) {
            switch (column) {
                case CLUB_CATEGORY_WOOD:
                    animationId = WOOD_DISTANCE_ANIMATION;
                    clubCategory = CLUB_CATEGORY_WOOD;
                    break;
                case CLUB_CATEGORY_IRON:
                    animationId = IRON_DISTANCE_ANIMATION;
                    clubCategory = CLUB_CATEGORY_IRON;
                    break;
                case CLUB_CATEGORY_WEDGE:
                    animationId = WEDGE_DISTANCE_ANIMATION;
                    clubCategory = CLUB_CATEGORY_WEDGE;
                    break;
                case CLUB_CATEGORY_PUTTER:
                    animationId = PUTTER_DISTANCE_ANIMATION;
                    clubCategory = CLUB_CATEGORY_PUTTER;
                    break;
                case BALL_DISTANCE_WOOD_APPROACH:
                    animationId = WOOD_APPROACH_DISTANCE_ANIMATION;
                    clubCategory = CLUB_CATEGORY_WOOD;
                    break;
            }

            SetAnimationSpeed(MEASURED_SLOT, 0.0f);
            SetCharacterAnimation(MEASURED_SLOT, animationId);
            if (column < BALL_DISTANCE_WOOD_APPROACH) {
                SkipToAnimationEnd(MEASURED_SLOT);
            }

            AnimateCharacter(MEASURED_SLOT);
            distances[column] =
                GetBallDistance(MEASURED_SLOT, clubCategory, TRUE, MEASURED_SHOT_MODE);
        }

        osSyncPrintf("\n");
        sprintf(
            line,
            "{%d, %d, %d, %d, %d},\n",
            (int)distances[0],
            (int)distances[1],
            (int)distances[2],
            (int)distances[3],
            (int)distances[4]
        );
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

static const char f_UnusedFrameFormat[] = "TA %d : FRAME %d    ";

void WriteBallDistancesIfEnabled(void) {
    if (IsGameFlagSet(GAME_FLAG_GENERATE_BALL_DISTANCES)) {
        WriteBallDistances();
    }
}

void SetCharacterClub(u32 slotIndex, s32 clubIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 choiceIndex;

    if (slot != NULL) {
        g_CharacterModelIndex = slot->modelIndex;
        switch (GetCharacterClubCategory(slotIndex, clubIndex)) {
            case CLUB_CATEGORY_WOOD:
                choiceIndex = WOOD_DISPLAY_LIST_CHOICE;
                break;
            case CLUB_CATEGORY_IRON:
                choiceIndex = IRON_DISPLAY_LIST_CHOICE;
                break;
            case CLUB_CATEGORY_WEDGE:
                choiceIndex = WEDGE_DISPLAY_LIST_CHOICE;
                break;
            case CLUB_CATEGORY_PUTTER:
                choiceIndex = PUTTER_DISPLAY_LIST_CHOICE;
                break;
            default:
                choiceIndex = NO_DISPLAY_LIST_CHOICE;
                break;
        }

        MapModelBuffer(slot);
        SelectCharacterDisplayList(slotIndex, choiceIndex);
    }
}

s32 GetAnimationEventActions(
    u32 slotIndex,
    s32 animationIndex,
    s32 frame,
    AnimationEventAction *actions
) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 count = 0;
    AnimationEvent *event;
    s32 eventIndex;

    if (slot != NULL && animationIndex < slot->animationCount) {
        event = ModelAnimations(slot->model)[animationIndex].events;
        if (event != NULL) {
            for (eventIndex = 0; event[eventIndex].frame != END_OF_ANIMATION_EVENTS; eventIndex++) {
                if (event[eventIndex].frame == frame) {
                    if (count < EVENT_ACTION_COUNT_MAX) {
                        actions[count].kind = event[eventIndex].kind;
                        actions[count].value = event[eventIndex].value;
                        count++;
                    }
                }
            }
        }
    }

    return count;
}

s32 GetAnimationEventFrame(u32 slotIndex, s32 animationIndex, s32 kind) {
    s32 frame = -1;
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    AnimationEvent *event;
    s32 eventIndex;

    if (slot != NULL && animationIndex < slot->animationCount) {
        event = ModelAnimations(slot->model)[animationIndex].events;
        if (event != NULL) {
            for (eventIndex = 0; event[eventIndex].frame != END_OF_ANIMATION_EVENTS; eventIndex++) {
                if (event[eventIndex].kind == kind) {
                    frame = event[eventIndex].frame -
                        ModelAnimations(slot->model)[animationIndex].startFrame;
                    break;
                }
            }
        }
    }

    return frame;
}

s32 GetImpactEventFrame(u32 slotIndex) {
    return GetAnimationEventFrame(slotIndex, IMPACT_EVENT_ANIMATION_INDEX, ANIMATION_EVENT_IMPACT);
}

void ResetTriangleTextures(u32 slotIndex) {
    u8 unused[0x40] UNUSED;
    s32 triangleIndex = 0;
    CharacterSlot *slot;
    AnimatedTriangle *triangle;
    AnimatedTexture *texture;
    Gfx *gfx;
    Vtx *vertex;
    s32 vertexIndex;

    if (FindUsedSlot(slotIndex, &slot)) {
        MapModelBuffer(slot);
        while ((triangle = ModelAnimatedTriangles(slot->model)[triangleIndex]) != NULL) {
            texture = ModelAnimatedTextures(slot->model);
            if (triangle->textureIndex >= 0) {
                texture += triangle->textureIndex;
            }

            SetTriangleTexture(slot, triangle, texture, triangle->s, triangle->t, vertexIndex);
            triangleIndex++;
        }
    }
}

void AdvanceTextureAnimations(
    u32 slotIndex,
    s32 unused UNUSED,
    s32 frameLastDigit UNUSED,
    s32 frame UNUSED
) {
    u8 unusedHead[0x40] UNUSED;
    s32 triangleIndex = 0;
    CharacterSlot *slot;
    AnimatedTriangle *triangle;
    s32 keySetIndex;
    TriangleTextureKeys *keySet;
    s32 keyIndex;
    TriangleTextureKey *key;
    AnimatedTexture *texture;
    Gfx *gfx;
    Vtx *vertex;
    s32 stateFrame;
    s32 animationIndex;
    /* Counts both triangle vertices and animation slots; separate
       variables allocate registers differently. */
    s32 sharedIndex;

    if (FindUsedSlot(slotIndex, &slot)) {
        u8 unusedTail[0x80] UNUSED;

        MapModelBuffer(slot);
        while ((triangle = ModelAnimatedTriangles(slot->model)[triangleIndex]) != NULL) {
            for (keySetIndex = 0; keySetIndex < triangle->keySetCount; keySetIndex++) {
                keySet = &triangle->keySets[keySetIndex];
                if (slot->textureAnimations[keySet->textureAnimationIndex].pendingCopyCount != 0) {
                    stateFrame = slot->textureAnimations[keySet->textureAnimationIndex].frame;
                    for (keyIndex = 0; keyIndex < keySet->count; keyIndex++) {
                        key = &keySet->keys[keyIndex];
                        if (stateFrame >= key->frame) {
                            texture = &ModelAnimatedTextures(slot->model)[key->textureIndex];
                            SetTriangleTexture(
                                slot,
                                triangle,
                                texture,
                                key->s,
                                key->t,
                                sharedIndex
                            );
                        }
                    }
                }
            }

            triangleIndex++;
        }

        for (sharedIndex = 0; sharedIndex < TEXTURE_ANIMATION_COUNT; sharedIndex++) {
            animationIndex = ModelTextureAnimations(slot->model)[sharedIndex].index;
            if (animationIndex < 0) {
                break;
            }

            if (slot->textureAnimations[animationIndex].pendingCopyCount >=
                TEXTURE_ANIMATION_PLAYING) {
                if (++slot->textureAnimations[animationIndex].frame >=
                    ModelTextureAnimations(slot->model)[sharedIndex].frameCount) {
                    slot->textureAnimations[animationIndex].pendingCopyCount =
                        FRAME_DATA_COPY_COUNT;
                    if (animationIndex == RANDOM_TEXTURE_ANIMATION) {
                        slot->textureAnimations[RANDOM_TEXTURE_ANIMATION].frame = 0;
                    }
                    else {
                        slot->textureAnimations[animationIndex].frame =
                            ModelTextureAnimations(slot->model)[sharedIndex].frameCount;
                    }
                }
            }
            else if (slot->textureAnimations[animationIndex].pendingCopyCount != 0) {
                slot->textureAnimations[animationIndex].pendingCopyCount--;
            }
        }
    }
}

NO_RESULT SetTextureAnimationFrame(u32 slotIndex, s32 index, s32 frame) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->textureAnimations[index].frame = frame;
        slot->textureAnimations[index].pendingCopyCount = FRAME_DATA_COPY_COUNT;
    }
}

void StopTextureAnimations(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 animationIndex;

    for (animationIndex = TEXTURE_ANIMATION_COUNT - 1; animationIndex >= 0; animationIndex--) {
        slot->textureAnimations[animationIndex].pendingCopyCount = 0;
    }
}

NO_RESULT PlayTextureAnimation(u32 slotIndex, s32 index) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (index >= TEXTURE_ANIMATION_COUNT) {
        ReportFailedAssertion(f_AssertionExpression, f_FileName, TEXTURE_ANIMATION_ASSERT_LINE);
    }

    if (slot != NULL) {
        slot->textureAnimations[index].pendingCopyCount = TEXTURE_ANIMATION_PLAYING;
        slot->textureAnimations[index].frame = 0;
    }
}

void ResetAnimationTriangleTextures(u32 slotIndex, s32 textureAnimationIndex) {
    u8 unused[0x40] UNUSED;
    s32 triangleIndex = 0;
    CharacterSlot *slot;
    AnimatedTriangle *triangle;
    AnimatedTexture *texture;
    Gfx *gfx;
    Vtx *vertex;
    TriangleTextureKeys *keySet;
    s32 keySetIndex;
    s32 vertexIndex;

    if (FindUsedSlot(slotIndex, &slot)) {
        MapModelBuffer(slot);
        while ((triangle = ModelAnimatedTriangles(slot->model)[triangleIndex]) != NULL) {
            texture = &ModelAnimatedTextures(slot->model)[triangle->textureIndex];
            for (keySetIndex = 0; keySetIndex < triangle->keySetCount; keySetIndex++) {
                keySet = &triangle->keySets[keySetIndex];
                if (keySet->textureAnimationIndex == textureAnimationIndex) {
                    SetTriangleTexture(
                        slot,
                        triangle,
                        texture,
                        triangle->s,
                        triangle->t,
                        vertexIndex
                    );
                }
            }

            triangleIndex++;
        }
    }
}

static const char f_NotDisposedFormat[] = "Polychara slot %d not disposed yet!!!!!!!\n";
static const char f_UnusedReadRomFormat[] = "nuPiReadRom(%x, %x, %x) [bufsiz %x]\n";
static const char f_UnusedCompleteText[] = "complete\n";

void LoadCharacter(u32 slotIndex, s32 characterIndex, s32 modelKind) {
    Boolean isPatched = FALSE;
    CharacterSlot *slot = &g_CharacterSlots[slotIndex];
    JointAnimation *joint;
    CharacterAnimation *animation;
    u32 frameDataSize;
    s32 animationCount;

    if (modelKind < CHARACTER_MODEL_KIND_COUNT) {
        g_CharacterModelIndex =
            ((modelKind & (CHARACTER_MODEL_KIND_COUNT - 1)) * CHARACTER_MODELS_PER_KIND) +
            (characterIndex % CHARACTER_MODELS_PER_KIND);
        slot->model = &g_CharacterModels[0][g_CharacterModelIndex];
    }
    else {
        g_CharacterModelIndex = (u32)characterIndex % OPENING_CHARACTER_MODEL_COUNT;
        slot->model = &g_OpeningCharacterModels[g_CharacterModelIndex];
    }

    if (IsCharacterSlotBusy(slotIndex)) {
        osSyncPrintf(f_NotDisposedFormat, slotIndex);
        osSyncPrintf(f_NotDisposedFormat, slotIndex);
        osSyncPrintf(f_NotDisposedFormat, slotIndex);
        osSyncPrintf(f_NotDisposedFormat, slotIndex);
        osSyncPrintf(f_NotDisposedFormat, slotIndex);
    }
    else if (slot->modelBuffer != NULL) {
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
        slot->usesFrontCulling = 0;
        slot->reusesLastMatrices = 0;
        f_CurrentModelBuffer = slot->modelBuffer;
        if (isPatched) {
            LoadLzData(
                slot->model->baseRomStart,
                f_CurrentModelBuffer,
                slot->model->baseRomEnd - slot->model->baseRomStart
            );
            LoadPatchedLzData(
                slot->model->romStart,
                f_CurrentModelBuffer,
                slot->model->romEnd - slot->model->romStart
            );
        }
        else {
            LoadLzData(
                slot->model->romStart,
                f_CurrentModelBuffer,
                slot->model->romEnd - slot->model->romStart
            );
        }

        SetMultiTlb(
            0,
            TLB_PAGE_MODE_4K,
            CHARACTER_MODEL_PAGE_COUNT,
            CHARACTER_MODEL_ADDRESS,
            osVirtualToPhysical(f_CurrentModelBuffer)
        );
        frameDataSize = slot->model->frameDataEnd - slot->model->frameDataStart;
        if (frameDataSize < FRAME_DATA_COPY_SIZE) {
            frameDataSize = FRAME_DATA_COPY_SIZE;
        }

        slot->frameData[0] =
            OS_PHYSICAL_TO_K0(osVirtualToPhysical((void *)slot->model->frameDataStart));
        slot->frameData[1] = (void *)(frameDataSize + (u32)slot->frameData[0]);
        slot->frameData[2] = (void *)(frameDataSize + (u32)slot->frameData[1]);
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
            osSyncPrintf(
                "loop %d, loop_min = %d, loop_max = %d\n",
                f_StartedAnimationIndex,
                g_CurrentAnimationStartFrame,
                g_CurrentAnimationEndFrame
            );
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
        slot->displayList = (Gfx *)slot->model->segmentAddresses[CHARACTER_MODEL_DISPLAY_LIST];
        slot->portraitCamera[0] =
            *(f32 *)slot->model->segmentAddresses[CHARACTER_MODEL_PORTRAIT_CAMERA];
        slot->portraitCamera[1] =
            *(f32 *)slot->model->segmentAddresses[CHARACTER_MODEL_PORTRAIT_CAMERA + 1];
        slot->portraitCamera[2] =
            *(f32 *)slot->model->segmentAddresses[CHARACTER_MODEL_PORTRAIT_CAMERA + 2];
        slot->portraitCamera[3] =
            *(f32 *)slot->model->segmentAddresses[CHARACTER_MODEL_PORTRAIT_CAMERA + 3];
        slot->portraitCamera[4] =
            *(f32 *)slot->model->segmentAddresses[CHARACTER_MODEL_PORTRAIT_CAMERA + 4];
        slot->portraitCamera[5] =
            *(f32 *)slot->model->segmentAddresses[CHARACTER_MODEL_PORTRAIT_CAMERA + 5];
        slot->shadowScale = slot->model->shadowScale;
        animationCount = 0;
        for (animation = ModelAnimations(slot->model); animation->events != NULL; animation++) {
            animationCount++;
        }

        slot->animationCount = animationCount;
        slot->animationSpeed = 1.0f;
        if (IsGameFlagSet(GAME_FLAG_LOG_CHARACTER_SETUP)) {
            osSyncPrintf("stage01 inited, char %d.\n", g_CharacterModelIndex);
        }
    }
}

s32 GetAnimationIndex(u32 slotIndex, s32 animationId) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 foundIndex = 0;
    CharacterAnimation *animation;
    s32 i;

    if (slot != NULL) {
        i = 0;
        for (animation = ModelAnimations(slot->model); animation->events != NULL; animation++) {
            if (animation->id == animationId) {
                foundIndex = i;
                break;
            }

            i++;
        }
    }

    return foundIndex;
}

CharacterAnimation *FindAnimation(u32 slotIndex, s32 animationId) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        MapModelBuffer(slot);
        // animationId is reused for the animation's index; a separate
        // variable breaks the match.
        animationId = GetAnimationIndex(slotIndex, animationId);
        if (ModelAnimations(slot->model)[animationId].events == NULL) {
            return NULL;
        }

        return &ModelAnimations(slot->model)[animationId];
    }

    return NULL;
}

void SetCharacterAnimation(u32 slotIndex, s32 animationId) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    JointAnimation *joint;

    if (slot != NULL) {
        MapModelBuffer(slot);
        slot->animationIndex = GetAnimationIndex(slotIndex, animationId);
        if (ModelAnimations(slot->model)[slot->animationIndex].events == NULL) {
            slot->animationIndex = 0;
        }

        slot->animationStartFrame = ModelAnimations(slot->model)[slot->animationIndex].startFrame;
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

void SetPlaysRandomTextureAnimation(u32 slotIndex, s32 playsRandomTextureAnimation) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->playsRandomTextureAnimation = playsRandomTextureAnimation;
    }
}

void SkipToAnimationEnd(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->animationFrame = slot->animationEndFrame;
    }
}

void AdvanceAnimation(u32 slotIndex, s32 frameCount) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->animationFrame += frameCount;
        if (slot->animationEndFrame <= slot->animationFrame) {
            slot->animationFrame = slot->animationEndFrame;
        }
    }
}

void SetAnimationSpeed(u32 slotIndex, f32 speed) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->animationSpeed = speed;
    }
}

void SetAnimationLooping(u32 slotIndex, Boolean isLooping) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->isLooping = isLooping;
    }
}

void SetNextAnimation(u32 slotIndex, s32 nextAnimationId) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->nextAnimationId = nextAnimationId;
    }
}

void SetCharacterTranslucent(u32 slotIndex, Boolean isTranslucent) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->isTranslucent = isTranslucent;
    }
}

void SetCharacterLighting(u32 slotIndex, CharacterLighting lighting) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->lighting = lighting;
    }
}

void SetCharacterSlotAlpha(u32 slotIndex, s32 alpha) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->alpha = alpha;
    }
}

s32 GetCurrentAnimationEndFrame(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 endFrame = -1;

    if (slot != NULL) {
        endFrame = slot->animationEndFrame;
    }

    return endFrame;
}

s32 GetAnimationEndFrame(u32 slotIndex, s32 animationId) {
    CharacterAnimation *animation = FindAnimation(slotIndex, animationId);

    if (animation != NULL) {
        return animation->startFrame + animation->frameCount;
    }

    return -1;
}

s32 GetAnimationStartFrame(u32 slotIndex, s32 animationId) {
    CharacterAnimation *animation = FindAnimation(slotIndex, animationId);

    if (animation != NULL) {
        return animation->startFrame;
    }

    return -1;
}

s32 GetAnimationFrameCount(u32 slotIndex, s32 animationId) {
    CharacterAnimation *animation = FindAnimation(slotIndex, animationId);

    if (animation != NULL) {
        return animation->frameCount;
    }

    return -1;
}

void SetCharacterFrontCulling(u32 slotIndex, Boolean usesFrontCulling) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->usesFrontCulling = usesFrontCulling;
    }
}

NO_RESULT ReuseLastCharacterMatrices(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->reusesLastMatrices = TRUE;
    }
}

s32 GetAnimationFrame(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 frame = -1;

    if (slot != NULL) {
        frame = slot->animationFrame;
    }

    return frame;
}

void AnimateCharacter(u32 slotIndex) {
    AnimationEventAction actions[EVENT_ACTION_COUNT_MAX];
    char text[DEBUG_LINE_SIZE];
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 actionIndex = 0;
    s32 actionCount;
    Boolean hasLooped;

    if (slot != NULL) {
        hasLooped = FALSE;
        MapModelBuffer(slot);
        actionCount = GetAnimationEventActions(
            slotIndex,
            slot->animationIndex,
            slot->animationFrame,
            actions
        );
        for (; actionIndex < actionCount; actionIndex++) {
            switch (actions[actionIndex].kind) {
                case ANIMATION_EVENT_LOOP_START:
                    break;
                case ANIMATION_EVENT_LOOP_END:
                    if (slot->isLooping) {
                        hasLooped = TRUE;
                        slot->animationFrame = GetAnimationEventFrame(
                                                   slotIndex,
                                                   slot->animationIndex,
                                                   ANIMATION_EVENT_LOOP_START
                                               ) +
                            ModelAnimations(slot->model)[slot->animationIndex].startFrame;
                    }

                    break;
                case ANIMATION_EVENT_TEXTURE_ANIMATION:
                    if (actions[actionIndex].value == TEXTURE_ANIMATION_EVENT_START_RANDOM) {
                        slot->playsRandomTextureAnimation = TRUE;
                    }
                    else if (actions[actionIndex].value == TEXTURE_ANIMATION_EVENT_STOP_RANDOM) {
                        slot->playsRandomTextureAnimation = FALSE;
                    }
                    else {
                        PlayTextureAnimation(slotIndex, actions[actionIndex].value);
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
            }
            else {
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
            sprintf(
                text,
                "[PC%d Frame%d]         ",
                (int)slotIndex,
                (int)(g_CurrentAnimationFrame - slot->animationStartFrame)
            );
            PrintDebugText(
                text,
                ANIMATION_FRAME_TEXT_COLUMN,
                slotIndex + ANIMATION_FRAME_TEXT_FIRST_ROW
            );
        }

        if (slot->playsRandomTextureAnimation) {
            if (slot->randomTextureAnimationDelay == 0) {
                PlayTextureAnimation(slotIndex, RANDOM_TEXTURE_ANIMATION);
                slot->randomTextureAnimationDelay = (rand() & RANDOM_TEXTURE_ANIMATION_DELAY_MASK) +
                    RANDOM_TEXTURE_ANIMATION_DELAY_MIN;
            }
            else {
                slot->randomTextureAnimationDelay--;
            }
        }

        AdvanceTextureAnimations(
            slotIndex,
            0,
            (s32)g_CurrentAnimationFrame % 10,
            g_CurrentAnimationFrame
        );
        g_CurrentJointMatrices = slot->frameData[slot->frameDataIndex];
        f_CurrentModelBuffer = slot->modelBuffer;
        slot->frameDataIndex = (slot->frameDataIndex + 1) % FRAME_DATA_COPY_COUNT;
        AnimateCharacterJoints(slotIndex);
        slot->jointMatrices = g_CurrentJointMatrices;
    }
}

void UpdateJointKeyframeSpans(u32 slotIndex) {
    f32 unusedMatrices[3][4][4] UNUSED;
    f32 translationMatrix[4][4];
    f32 jointMatrix[4][4];
    f32 scaleMatrix[4][4] UNUSED;
    f32 rotation[4];
    JointAnimation *joint;
    f32 x;
    f32 y;
    f32 z;
    f32 progress;
    f32 remaining;
    f32 dot;
    Radians angle;
    f32 spanWeight;
    f32 spanFactor;
    f32 nextFactor;
    f32 sinAngle;
    s16 isInterpolated;
    s32 keyIndex;
    s16 spanIndex;
    s16 nextSpanIndex;

    joint = (JointAnimation *)GetUsedCharacterSlot(slotIndex)
                ->model->segmentAddresses[CHARACTER_MODEL_JOINTS];
    if (joint->translationKeyCount == END_OF_JOINT_ANIMATIONS) {
        return;
    }

    do {
        isInterpolated = TRUE;
        if (joint->translationSpan.endFrame < g_CurrentAnimationFrame ||
            g_CurrentAnimationFrame < joint->translationSpan.startFrame) {
            keyIndex = 1;
            if (g_CurrentAnimationFrame < joint->translationFrames[0]) {
                isInterpolated = FALSE;
                x = joint->translations[0][0];
                y = joint->translations[0][1];
                z = joint->translations[0][2];
            }
            else {
                for (; keyIndex < joint->translationKeyCount; keyIndex++) {
                    if (g_CurrentAnimationFrame < joint->translationFrames[keyIndex]) {
                        joint->translationSpan.startKeyIndex = keyIndex - 1;
                        joint->translationSpan.startFrame = joint->translationFrames[keyIndex - 1];
                        joint->translationSpan.endFrame = joint->translationFrames[keyIndex];
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
                x = joint->translations[joint->translationSpan.startKeyIndex][0];
                y = joint->translations[joint->translationSpan.startKeyIndex][1];
                z = joint->translations[joint->translationSpan.startKeyIndex][2];
            }
            else {
                progress = (g_CurrentAnimationFrame - joint->translationSpan.startFrame) /
                    (joint->translationSpan.endFrame - joint->translationSpan.startFrame);
                spanIndex = joint->translationSpan.startKeyIndex;
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

        isInterpolated = TRUE;
        if (joint->rotationSpan.endFrame < g_CurrentAnimationFrame ||
            g_CurrentAnimationFrame < joint->rotationSpan.startFrame) {
            keyIndex = 1;
            if (g_CurrentAnimationFrame < joint->rotationFrames[0]) {
                isInterpolated = FALSE;
                rotation[0] = joint->rotations[0][0] * QUATERNION_SCALE;
                rotation[1] = joint->rotations[0][1] * QUATERNION_SCALE;
                rotation[2] = joint->rotations[0][2] * QUATERNION_SCALE;
                rotation[3] = joint->rotations[0][3] * QUATERNION_SCALE;
            }
            else {
                for (; keyIndex < joint->rotationKeyCount; keyIndex++) {
                    if (g_CurrentAnimationFrame < joint->rotationFrames[keyIndex]) {
                        joint->rotationSpan.startKeyIndex = keyIndex - 1;
                        joint->rotationSpan.startFrame = joint->rotationFrames[keyIndex - 1];
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
                    joint->rotations[joint->rotationSpan.startKeyIndex][0] * QUATERNION_SCALE;
                rotation[1] =
                    joint->rotations[joint->rotationSpan.startKeyIndex][1] * QUATERNION_SCALE;
                rotation[2] =
                    joint->rotations[joint->rotationSpan.startKeyIndex][2] * QUATERNION_SCALE;
                rotation[3] =
                    joint->rotations[joint->rotationSpan.startKeyIndex][3] * QUATERNION_SCALE;
            }
            else {
                f32 rotationProgress;
                Radians coveredAngle;

                spanIndex = joint->rotationSpan.startKeyIndex;
                nextSpanIndex = spanIndex + 1;
                dot = ((joint->rotations[spanIndex][0] * QUATERNION_PRODUCT_SCALE) *
                          joint->rotations[nextSpanIndex][0]) +
                    ((joint->rotations[spanIndex][1] * QUATERNION_PRODUCT_SCALE) *
                        joint->rotations[nextSpanIndex][1]) +
                    ((joint->rotations[spanIndex][2] * QUATERNION_PRODUCT_SCALE) *
                        joint->rotations[nextSpanIndex][2]) +
                    ((joint->rotations[spanIndex][3] * QUATERNION_PRODUCT_SCALE) *
                        joint->rotations[nextSpanIndex][3]);
                rotationProgress = (g_CurrentAnimationFrame - joint->rotationSpan.startFrame) /
                    (joint->rotationSpan.endFrame - joint->rotationSpan.startFrame);
                if (1.0 - dot < SLERP_THRESHOLD) {
                    progress = rotationProgress;
                    spanWeight = 1.0 - progress;
                }
                else {
                    angle = acosf(dot);
                    coveredAngle = rotationProgress * angle;
                    sinAngle = sinf(angle);
                    progress = sinf(coveredAngle) / sinAngle;
                    spanWeight = sinf(angle - coveredAngle) / sinAngle;
                }

                spanFactor = spanWeight * QUATERNION_SCALE;
                nextFactor = progress * QUATERNION_SCALE;
                rotation[0] = (spanFactor * joint->rotations[spanIndex][0]) +
                    (nextFactor * joint->rotations[nextSpanIndex][0]);
                rotation[1] = (spanFactor * joint->rotations[spanIndex][1]) +
                    (nextFactor * joint->rotations[nextSpanIndex][1]);
                rotation[2] = (spanFactor * joint->rotations[spanIndex][2]) +
                    (nextFactor * joint->rotations[nextSpanIndex][2]);
                rotation[3] = (spanFactor * joint->rotations[spanIndex][3]) +
                    (nextFactor * joint->rotations[nextSpanIndex][3]);
            }
        }

        QuaternionToMatrix(rotation, jointMatrix);

        isInterpolated = TRUE;
        if (joint->scaleSpan.endFrame < g_CurrentAnimationFrame ||
            g_CurrentAnimationFrame < joint->scaleSpan.startFrame) {
            keyIndex = 1;
            if (g_CurrentAnimationFrame < joint->scaleFrames[0]) {
                isInterpolated = FALSE;
                x = joint->scales[0][0];
                y = joint->scales[0][1];
                z = joint->scales[0][2];
            }
            else {
                for (; keyIndex < joint->scaleKeyCount; keyIndex++) {
                    if (g_CurrentAnimationFrame < joint->scaleFrames[keyIndex]) {
                        joint->scaleSpan.startKeyIndex = keyIndex - 1;
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
                x = joint->scales[joint->scaleSpan.startKeyIndex][0];
                y = joint->scales[joint->scaleSpan.startKeyIndex][1];
                z = joint->scales[joint->scaleSpan.startKeyIndex][2];
            }
            else {
                progress = (g_CurrentAnimationFrame - joint->scaleSpan.startFrame) /
                    (joint->scaleSpan.endFrame - joint->scaleSpan.startFrame);
                spanIndex = joint->scaleSpan.startKeyIndex;
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
    }
    while (joint->translationKeyCount != END_OF_JOINT_ANIMATIONS);
}

void AnimateCharacterInSameCopy(u32 slotIndex) {
    AnimationEventAction actions[EVENT_ACTION_COUNT_MAX];
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    s32 actionIndex = 0;
    s32 actionCount;
    Boolean hasLooped;

    if (slot != NULL) {
        hasLooped = FALSE;
        MapModelBuffer(slot);
        actionCount = GetAnimationEventActions(
            slotIndex,
            slot->animationIndex,
            slot->animationFrame,
            actions
        );
        for (; actionIndex < actionCount; actionIndex++) {
            switch (actions[actionIndex].kind) {
                case ANIMATION_EVENT_LOOP_START:
                    break;
                case ANIMATION_EVENT_LOOP_END:
                    if (slot->isLooping) {
                        hasLooped = TRUE;
                        slot->animationFrame = GetAnimationEventFrame(
                                                   slotIndex,
                                                   slot->animationIndex,
                                                   ANIMATION_EVENT_LOOP_START
                                               ) +
                            ModelAnimations(slot->model)[slot->animationIndex].startFrame;
                    }

                    break;
                case ANIMATION_EVENT_TEXTURE_ANIMATION:
                    if (actions[actionIndex].value == TEXTURE_ANIMATION_EVENT_START_RANDOM) {
                        slot->playsRandomTextureAnimation = TRUE;
                    }
                    else if (actions[actionIndex].value == TEXTURE_ANIMATION_EVENT_STOP_RANDOM) {
                        slot->playsRandomTextureAnimation = FALSE;
                    }
                    else {
                        PlayTextureAnimation(slotIndex, actions[actionIndex].value);
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
            }
            else {
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
                slot->randomTextureAnimationDelay = (rand() & RANDOM_TEXTURE_ANIMATION_DELAY_MASK) +
                    RANDOM_TEXTURE_ANIMATION_DELAY_MIN;
            }
            else {
                slot->randomTextureAnimationDelay--;
            }
        }

        AdvanceTextureAnimations(
            slotIndex,
            0,
            (s32)g_CurrentAnimationFrame % 10,
            g_CurrentAnimationFrame
        );
        g_CurrentJointMatrices = slot->frameData[slot->frameDataIndex];
        f_CurrentModelBuffer = slot->modelBuffer;
        AnimateCharacterJoints(slotIndex);
        slot->jointMatrices = g_CurrentJointMatrices;
    }
}

void AnimateAllCharacters(void) {
    s32 slotIndex;

    for (slotIndex = 0; slotIndex < CHARACTER_SLOT_COUNT; slotIndex++) {
        AnimateCharacter(slotIndex);
    }
}

s32 GetCharacterBufferSize(void) {
    return CHARACTER_BUFFER_SIZE;
}

void SetCharacterBuffer(s32 slotIndex, u8 *buffer) {
    u32 alignedBuffer = RoundUpTo((u32)buffer, SMALLEST_PAGE_SIZE);
    s32 warningCount;
    s32 copyIndex;

    if (g_CharacterSlots[slotIndex].modelIndex != NO_CHARACTER_MODEL) {
        for (warningCount = 0; warningCount < WARNING_REPEAT_COUNT; warningCount++) {
            osSyncPrintf("Polychara %d still alive. You can't change buffer.\n", slotIndex);
        }
    }
    else {
        g_CharacterSlots[slotIndex].modelBuffer = (void *)alignedBuffer;
        for (copyIndex = 0; copyIndex < FRAME_DATA_COPY_COUNT; copyIndex++) {
            g_CharacterSlots[slotIndex].frameData[copyIndex] =
                AddressPast(g_CharacterSlots[slotIndex].modelBuffer, FRAME_DATA_OFFSET) +
                (copyIndex * FRAME_DATA_COPY_SIZE);
        }
    }
}

void ResetCharacterSlots(void) {
    s32 slotIndex;

    FreeAllCharacterSlots();
    for (slotIndex = 0; slotIndex < CHARACTER_SLOT_COUNT; slotIndex++) {
        g_CharacterSlots[slotIndex].lastDrawnFrame = LONG_AGO_FRAME;
        SetCharacterBuffer(slotIndex, (u8 *)DEFAULT_CHARACTER_BUFFER);
    }
}

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
    lightDirection[0] = -sinf(lightPitch) * cosf(lightYaw) * LIGHT_DIRECTION_SCALE;
    lightDirection[1] = -cosf(lightPitch) * LIGHT_DIRECTION_SCALE;
    lightDirection[2] = -sinf(lightPitch) * sinf(lightYaw) * LIGHT_DIRECTION_SCALE;
    f_DryLights[g_DrawBufferIndex] = (Lights2)gdSPDefLights2(
        ambientColor[0],
        ambientColor[1],
        ambientColor[2],
        lightColor[0],
        lightColor[1],
        lightColor[2],
        lightDirection[0],
        lightDirection[1],
        lightDirection[2],
        lightColor[0],
        lightColor[1],
        lightColor[2],
        f_SecondLightDirection[AXIS_X],
        f_SecondLightDirection[AXIS_Y],
        f_SecondLightDirection[AXIS_Z]
    );
    f_RainLights[g_DrawBufferIndex] = (Lights2)gdSPDefLights2(
        RAIN_AMBIENT_LEVEL,
        RAIN_AMBIENT_LEVEL,
        RAIN_AMBIENT_LEVEL,
        RAIN_LIGHT_RED,
        RAIN_LIGHT_GREEN,
        RAIN_LIGHT_BLUE,
        lightDirection[0],
        lightDirection[1],
        lightDirection[2],
        RAIN_SECOND_LIGHT_LEVEL,
        RAIN_SECOND_LIGHT_LEVEL,
        RAIN_SECOND_LIGHT_LEVEL,
        f_SecondLightDirection[AXIS_X],
        f_SecondLightDirection[AXIS_Y],
        f_SecondLightDirection[AXIS_Z]
    );
}

#define SaveLights(lights) \
    { \
        f_SavedLights[0].l[0].l = (lights).l[0].l; \
        f_SavedLights[1].l[0].l = (lights).l[0].l; \
        f_SavedLights[0].a.l = (lights).a.l; \
        f_SavedLights[1].a.l = (lights).a.l; \
    }

void DrawCharacter(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);
    Gfx *gfx = g_DisplayListHead;

    f_DrawParity ^= 1;
    if (f_IsMatrixSetupPending) {
        f_IsMatrixSetupPending = FALSE;
        guScale(&f_ModelScaleMatrix, MODEL_SCALE, MODEL_SCALE, MODEL_SCALE);
        guRotate(&f_RollMatrix, HALF_TURN_DEGREES, 0.0f, 0.0f, 1.0f);
        guRotate(
            &f_HeadingMatrix,
            (f_DrawParity * DEGREES_PER_TURN / BINARY_ANGLE_TURN) + HALF_TURN_DEGREES,
            0.0f,
            1.0f,
            0.0f
        );
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
                    }
                    else {
                        gSPSetLights2(gfx++, f_DryLights[g_DrawBufferIndex]);
                        SaveLights(f_DryLights[g_DrawBufferIndex]);
                    }

                    break;
                case CHARACTER_LIGHTING_SCENE_THIRD:
                    if (g_HoleSetup.conditions.isRaining) {
                        if (!AddReactionLight(&gfx, &f_RainLights[g_DrawBufferIndex])) {
                            gSPSetLights2(gfx++, f_RainLights[g_DrawBufferIndex]);
                            SaveLights(f_RainLights[g_DrawBufferIndex]);
                        }
                    }
                    else {
                        if (!AddReactionLight(&gfx, &f_DryLights[g_DrawBufferIndex])) {
                            gSPSetLights2(gfx++, f_DryLights[g_DrawBufferIndex]);
                            SaveLights(f_DryLights[g_DrawBufferIndex]);
                        }
                    }

                    break;
                case CHARACTER_LIGHTING_BLACK:
                    gSPSetLights2(gfx++, f_BlackLights);
                    SaveLights(f_BlackLights);
                    break;
            }
        }
        else {
            gSPSetLights2(gfx++, f_SavedLights[g_DrawBufferIndex]);
        }

        MapModelBuffer(slot);
        gSPMatrix(gfx++, &f_ModelScaleMatrix, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
        gSPSegment(gfx++, MODEL_BUFFER_SEGMENT, osVirtualToPhysical(slot->modelBuffer));
        gSPSegment(gfx++, JOINT_MATRICES_SEGMENT, osVirtualToPhysical(slot->jointMatrices));
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_PERSP);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_DISABLE);
        gDPPipeSync(gfx++);
        gDPSetDepthSource(gfx++, G_ZS_PIXEL);
        gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, BLEND_ALPHA);
        gSPSegment(gfx++, ENV_COLOR_SEGMENT, osVirtualToPhysical(g_CharacterEnvColorDisplayLists));
        gSPSegment(gfx++, HIGHLIGHT_SEGMENT, osVirtualToPhysical(g_CharacterHighlightDisplayList));
        gSPSegment(
            gfx++,
            RENDER_MODE_SEGMENT,
            osVirtualToPhysical(g_CharacterRenderModeDisplayList)
        );
        gSPSegment(gfx++, COMBINE_SEGMENT, osVirtualToPhysical(g_CharacterCombineDisplayList));
        gSPSegment(gfx++, MIRROR_TEXTURE_SEGMENT, osVirtualToPhysical(g_MirrorTexture));
        gSPSegment(gfx++, FLAME_TEXTURE_SEGMENT, osVirtualToPhysical(g_FlameTexture));
        gSPSegment(gfx++, BLENDED_TEXTURE_SEGMENT, osVirtualToPhysical(g_BlendedTexture));
        if (slot->isTranslucent) {
            gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0xFF, slot->alpha);
            SetCharacterAlpha(slot->alpha);
            gSPSegment(gfx++, RENDER_MODES_SEGMENT, osVirtualToPhysical(f_TranslucentRenderModes));
        }
        else {
            gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0xFF, BLEND_ALPHA);
            SetCharacterAlpha(OPAQUE_ALPHA);
            gSPSegment(gfx++, RENDER_MODES_SEGMENT, osVirtualToPhysical(f_OpaqueRenderModes));
        }

        if (slot->usesFrontCulling) {
            gSPSegment(gfx++, CULLING_SEGMENT, osVirtualToPhysical(f_FrontCulling));
        }
        else {
            gSPSegment(gfx++, CULLING_SEGMENT, osVirtualToPhysical(f_BackCulling));
        }
        gSPDisplayList(gfx++, slot->displayList);
        slot->lastDrawnFrame = g_FrameCount;
        gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
        gDPPipeSync(gfx++);
    }

    g_DisplayListHead = gfx;
}

void PlaceCharacterForShot(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        PlaceGolferAtBall(slot->position, slot->rotation, slot->scale);
        slot->rotation[AXIS_Y] =
            ((-g_CurrentShot.heading) & BINARY_ANGLE_MASK) * BINARY_ANGLE_TO_RADIANS;
    }
}

void PlaceAndViewCharacter(u32 slotIndex) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        PlaceAtReactionSpot(slot->position, slot->rotation, slot->scale);
        g_Cameras[0].view.lookAt[AXIS_X] = slot->position[AXIS_X] * COURSE_TO_WORLD_SCALE;
        g_Cameras[0].view.lookAt[AXIS_Y] =
            (slot->position[AXIS_Y] - VIEW_DROP) * COURSE_TO_WORLD_SCALE;
        g_Cameras[0].view.lookAt[AXIS_Z] = slot->position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        g_Cameras[0].view.eye[AXIS_X] =
            (slot->position[AXIS_X] +
                (cosf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * VIEW_DISTANCE)) *
            COURSE_TO_WORLD_SCALE;
        g_Cameras[0].view.eye[AXIS_Y] =
            (slot->position[AXIS_Y] - VIEW_DROP) * COURSE_TO_WORLD_SCALE;
        g_Cameras[0].view.eye[AXIS_Z] =
            (slot->position[AXIS_Z] +
                (sinf(-slot->rotation[AXIS_Y] - QUARTER_TURN_RADIANS) * VIEW_DISTANCE)) *
            COURSE_TO_WORLD_SCALE;
    }
}

void PlaceCharacter(u32 slotIndex, CoursePoint point, Radians heading) {
    CharacterSlot *slot = GetUsedCharacterSlot(slotIndex);

    if (slot != NULL) {
        slot->position[AXIS_X] = point[AXIS_X];
        slot->position[AXIS_Z] = point[AXIS_Z];
        slot->position[AXIS_Y] = GetGroundHeight(slot->position[AXIS_X], slot->position[AXIS_Z]);
        slot->rotation[AXIS_X] = 0.0f;
        slot->rotation[AXIS_Z] = 0.0f;
        slot->rotation[AXIS_Y] = -heading;
        slot->scale[AXIS_X] = 1.0f;
        slot->scale[AXIS_Y] = 1.0f;
        slot->scale[AXIS_Z] = 1.0f;
    }
}

#define IsCharacterDrawnForCamera(cameraIndex) \
    (((cameraIndex) == OPENING_CAMERA) || (((cameraIndex) != INSET_CAMERA) && g_AreCharactersDrawn))

static inline f32 GetImpactFlashYaw(f32 yaw, Boolean isLefty) {
    f32 flashYaw;

    flashYaw = -yaw;
    if (isLefty) {
        flashYaw = yaw;
    }

    return flashYaw;
}

void DrawCharacterAndShadow(CameraMatrices *matrices, s32 cameraIndex, s32 slotIndex) {
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
    Gfx *gfx;
    CharacterSlot *slot;
    f32 shadowScale;

    gfx = g_DisplayListHead;
    if (IsCharacterDrawnForCamera(cameraIndex)) {
        slot = GetUsedCharacterSlot(slotIndex);
        if (slot == NULL) {
            return;
        }

        viewDirection[AXIS_X] = g_Cameras[0].view.lookAt[AXIS_X] - g_Cameras[0].view.eye[AXIS_X];
        viewDirection[AXIS_Y] = 0.0f;
        viewDirection[AXIS_Z] = g_Cameras[0].view.lookAt[AXIS_Z] - g_Cameras[0].view.eye[AXIS_Z];
        if (fabsf(viewDirection[AXIS_X]) + fabsf(viewDirection[AXIS_Z]) >= 1.0f) {
            guNormalize(&viewDirection[AXIS_X], &viewDirection[AXIS_Y], &viewDirection[AXIS_Z]);
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
            f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.col[2] = g_ImpactFlashLevel / 2;
            f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.colc[0] = g_ImpactFlashLevel;
            f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.colc[1] = g_ImpactFlashLevel;
            f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.colc[2] = g_ImpactFlashLevel / 2;
            f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.dir[0] =
                sinf(GetImpactFlashYaw(
                    slot->rotation[1],
                    g_Players[g_CurrentShot.playerIndex].isLefty
                )) *
                LIGHT_DIRECTION_SCALE;
            f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.dir[1] = 0;
            f_SavedLights[g_DrawBufferIndex ^ 1].l[1].l.dir[2] =
                -cosf(GetImpactFlashYaw(
                    slot->rotation[1],
                    g_Players[g_CurrentShot.playerIndex].isLefty
                )) *
                LIGHT_DIRECTION_SCALE;
        }

        if (slot->reusesLastMatrices) {
            f_ModelMatrices[slotIndex][g_DrawBufferIndex] =
                f_ModelMatrices[slotIndex][g_DrawBufferIndex ^ 1];
        }
        else {
            guTranslateF(
                transform,
                slot->position[AXIS_X] * COURSE_TO_WORLD_SCALE,
                slot->position[AXIS_Y] * COURSE_TO_WORLD_SCALE,
                slot->position[AXIS_Z] * COURSE_TO_WORLD_SCALE
            );
            guRotateRPYF(
                rotation,
                (s32)(ROLL_ORIGIN_DEGREES - (s32)(slot->rotation[0] * RADIANS_TO_DEGREES)),
                (s32)(slot->rotation[1] * RADIANS_TO_DEGREES),
                (s32)(slot->rotation[2] * RADIANS_TO_DEGREES)
            );
            guMtxCatF(rotation, transform, transform);
            guScaleF(rotation, slot->scale[0], slot->scale[1], slot->scale[2]);
            guMtxCatF(rotation, transform, transform);
            guMtxF2L(transform, &f_ModelMatrices[slotIndex][g_DrawBufferIndex]);
            GetCharacterShadowPoint(slotIndex, shadowPoint);
            guMtxXFMF(
                transform,
                shadowPoint[AXIS_X] + SHADOW_CORNER_DISTANCE,
                0.0f,
                shadowPoint[AXIS_Z],
                &corners[0][AXIS_X],
                &corners[0][AXIS_Y],
                &corners[0][AXIS_Z]
            );
            guMtxXFMF(
                transform,
                shadowPoint[AXIS_X] - SHADOW_CORNER_DISTANCE,
                0.0f,
                shadowPoint[AXIS_Z] - SHADOW_CORNER_DISTANCE,
                &corners[1][AXIS_X],
                &corners[1][AXIS_Y],
                &corners[1][AXIS_Z]
            );
            guMtxXFMF(
                transform,
                shadowPoint[AXIS_X] - SHADOW_CORNER_DISTANCE,
                0.0f,
                shadowPoint[AXIS_Z] + SHADOW_CORNER_DISTANCE,
                &corners[2][AXIS_X],
                &corners[2][AXIS_Y],
                &corners[2][AXIS_Z]
            );
            guMtxXFMF(
                transform,
                shadowPoint[AXIS_X],
                0.0f,
                shadowPoint[AXIS_Z],
                &shadowPoint[AXIS_X],
                &shadowPoint[AXIS_Y],
                &shadowPoint[AXIS_Z]
            );
        }

        if (slot->reusesLastMatrices) {
            f_ShadowMatrices[slotIndex][g_DrawBufferIndex] =
                f_ShadowMatrices[slotIndex][g_DrawBufferIndex ^ 1];
        }
        else {
            shadowPoint[AXIS_X] *= WORLD_TO_COURSE_SCALE;
            shadowPoint[AXIS_Z] *= WORLD_TO_COURSE_SCALE;
            shadowPoint[AXIS_Y] = GetGroundHeight(shadowPoint[AXIS_X], shadowPoint[AXIS_Z]);
            shadowScale = slot->shadowScale * SHADOW_SCALE_UNIT;
            triangle.corners[AXIS_X][0] = corners[0][AXIS_X];
            triangle.corners[AXIS_Y][0] = GetGroundHeight(
                                              corners[0][AXIS_X] * WORLD_TO_COURSE_SCALE,
                                              corners[0][AXIS_Z] * WORLD_TO_COURSE_SCALE
                                          ) *
                COURSE_TO_WORLD_SCALE;
            triangle.corners[AXIS_Z][0] = corners[0][AXIS_Z];
            triangle.corners[AXIS_X][1] = corners[1][AXIS_X];
            triangle.corners[AXIS_Y][1] = GetGroundHeight(
                                              corners[1][AXIS_X] * WORLD_TO_COURSE_SCALE,
                                              corners[1][AXIS_Z] * WORLD_TO_COURSE_SCALE
                                          ) *
                COURSE_TO_WORLD_SCALE;
            triangle.corners[AXIS_Z][1] = corners[1][AXIS_Z];
            triangle.corners[AXIS_X][2] = corners[2][AXIS_X];
            triangle.corners[AXIS_Y][2] = GetGroundHeight(
                                              corners[2][AXIS_X] * WORLD_TO_COURSE_SCALE,
                                              corners[2][AXIS_Z] * WORLD_TO_COURSE_SCALE
                                          ) *
                COURSE_TO_WORLD_SCALE;
            triangle.corners[AXIS_Z][2] = corners[2][AXIS_Z];
            GetTrianglePlane(&triangle, &plane);
            guTranslateF(
                translation,
                shadowPoint[AXIS_X] * COURSE_TO_WORLD_SCALE,
                shadowPoint[AXIS_Y] * COURSE_TO_WORLD_SCALE,
                shadowPoint[AXIS_Z] * COURSE_TO_WORLD_SCALE
            );
            if (fabsf(plane.normal[AXIS_Y]) > LEVEL_GROUND_NORMAL_Y) {
                guRotateF(alignment, QUARTER_TURN_DEGREES, 1.0f, 0.0f, 0.0f);
            }
            else {
                guAlignF(
                    alignment,
                    0.0f,
                    plane.normal[AXIS_X],
                    plane.normal[AXIS_Y],
                    plane.normal[AXIS_Z]
                );
            }

            MultiplyMatrices(alignment, translation, shadow);
            guScaleF(alignment, shadowScale, shadowScale, SHADOW_DEPTH_SCALE);
            MultiplyMatrices(alignment, shadow, shadow);
            ConvertMatrixToFixed(shadow, &f_ShadowMatrices[slotIndex][g_DrawBufferIndex]);
        }

        guMtxL2F(projection, &matrices->projection);
        projection[3][2] -= SHADOW_DEPTH_OFFSET;
        guMtxF2L(projection, &f_ShadowProjections[slotIndex][g_DrawBufferIndex]);
        gSPMatrix(
            gfx++,
            &f_ShadowProjections[slotIndex][g_DrawBufferIndex],
            G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
        );
        gSPLoadGeometryMode(gfx++, 0);
        gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_CULL_FRONT | G_SHADING_SMOOTH);
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
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(ImageBody(g_ShadowTexture)),
            G_IM_FMT_I,
            G_IM_SIZ_8b,
            SHADOW_TEXTURE_SIZE,
            SHADOW_TEXTURE_SIZE,
            0,
            G_TX_MIRROR | G_TX_WRAP,
            G_TX_MIRROR | G_TX_WRAP,
            SHADOW_TEXTURE_SIZE_LOG2,
            SHADOW_TEXTURE_SIZE_LOG2,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPSetCombineLERP(
            gfx++,
            0,
            0,
            0,
            0,
            ENVIRONMENT,
            0,
            TEXEL0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            COMBINED
        );
        gDPPipeSync(gfx++);
        gDPSetTextureLOD(gfx++, G_TL_TILE);
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        gSPMatrix(
            gfx++,
            &f_ShadowMatrices[slotIndex][g_DrawBufferIndex],
            G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH
        );
        gSPVertex(gfx++, f_ShadowVertices, ARRAY_COUNT(f_ShadowVertices), 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        gDPPipeSync(gfx++);
        gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_NONE);
        gSPMatrix(gfx++, &matrices->projection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        gSPMatrix(
            gfx++,
            &f_ModelMatrices[slotIndex][g_DrawBufferIndex],
            G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH
        );
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
