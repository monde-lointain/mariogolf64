#include "hole_objects.h"

#include <math.h>
#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "camera.h"
#include "compiler.h"
#include "course_space.h"
#include "cpu_projection.h"
#include "effects.h"
#include "game.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "heap.h"
#include "hole_display.h"
#include "language.h"
#include "math_util.h"
#include "round_hud.h"
#include "screen_layers.h"
#include "sound.h"
#include "terrain.h"
#include "turn.h"

#define HOLE_CREATURE_COUNT 10

enum HoleCreatureKind {
    HOLE_CREATURE_NONE,
    HOLE_CREATURE_CHEEP_CHEEP,
    HOLE_CREATURE_MONKEY
};

typedef u8 HoleCreatureKind;

#define ASSET_CHEEP_CHEEP 0x6E0
#define ASSET_CHEEP_CHEEP_FIRST_VIEW 0x6DE
#define CHEEP_CHEEP_VIEW_COUNT 5
#define ASSET_GOLD_CHEEP_CHEEP 0x6E5
#define ASSET_MONKEY 0x6F0
#define MONKEY_FRAME_COUNT 4

#define CHEEP_CHEEP_DEPTH 0x7800
#define CHEEP_CHEEP_LEAP_SPEED 9216.0f
#define CHEEP_CHEEP_GRAVITY (GRAVITY * 2)
#define CHEEP_CHEEP_SPEED_MIN 0x600
#define CHEEP_CHEEP_SPEED_RANGE 0x600
#define CHEEP_CHEEP_PITCH_EASING 0.7f
#define CHEEP_CHEEP_FADE_DEPTH 46080
#define MONKEY_DROP_HEIGHT 0x9600
#define MONKEY_SECOND_FRAME 16
#define MONKEY_THIRD_FRAME 20
#define MONKEY_LAST_FRAME 24
#define MONKEY_LIFETIME 46

#define HOLE_OBJECT_SOUND_CHANNEL 15
#define SPLASH_SOUND_VOLUME 0x5A

#define MONKEY_HALF_IMAGE_SIZE 0x1000

typedef struct HoleCreature {
    Mtx matrices[DRAW_BUFFER_COUNT];
    CoursePoint position;
    f32 velocity[AXIS_COUNT];
    s32 groundHeight;
    Radians yaw;
    Radians pitch;
    u8 frameCount;
    UBoolean8 isFacingBack;
    UBoolean8 isGold;
    HoleCreatureKind kind;
} HoleCreature;

static AssetId f_CheepCheepViewAssets[CHEEP_CHEEP_VIEW_COUNT] UNUSED = {
    ASSET_CHEEP_CHEEP_FIRST_VIEW,
    ASSET_CHEEP_CHEEP_FIRST_VIEW + 1,
    ASSET_CHEEP_CHEEP_FIRST_VIEW + 2,
    ASSET_CHEEP_CHEEP_FIRST_VIEW + 3,
    ASSET_CHEEP_CHEEP_FIRST_VIEW + 4,
};
static AssetId f_MonkeyFrameAssets[MONKEY_FRAME_COUNT] =
    {ASSET_MONKEY, ASSET_MONKEY + 1, ASSET_MONKEY + 2, ASSET_MONKEY + 3};
static Boolean f_AreCheepCheepsLeaping = FALSE;
static s32 f_UnusedLeapStartValue = 0;
static s32 f_UnusedLeapValue = 0;
static s32 f_OtherUnusedLeapValue = 0;
static s32 f_UnusedWordBeforeCheepCheepVertices UNUSED = 0;
static Vtx f_CheepCheepVertices[QUAD_VERTEX_COUNT] = {
    {{{-7, -7, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-7, 7, 0}, 0, {0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{7, 7, 0}, 0, {0x7C0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{7, -7, 0}, 0, {0x7C0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Vtx f_MonkeyVertices[8] = {
    {{{-10, -12, 0}, 0, {0xFC0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-10, -6, 0}, 0, {0xFC0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, -6, 0}, 0, {0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, -12, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-10, -6, 0}, 0, {0xFC0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-10, 0, 0}, 0, {0xFC0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, 0, 0}, 0, {0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{1, -6, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};

static u8 *f_CheepCheepImages[2];
static u8 *f_MonkeyImage;

HoleCreature f_HoleCreatures[HOLE_CREATURE_COUNT];

void LoadHoleCreatureImages(void) {
    EncodedAsset asset;

    f_CheepCheepImages[0] = AllocMemory(OpenEncodedAsset(ASSET_CHEEP_CHEEP, &asset));
    ReadEncodedAsset(f_CheepCheepImages[0], &asset);
    f_CheepCheepImages[1] = AllocMemory(OpenEncodedAsset(ASSET_GOLD_CHEEP_CHEEP, &asset));
    ReadEncodedAsset(f_CheepCheepImages[1], &asset);
    f_MonkeyImage = AllocMemory(OpenEncodedAsset(ASSET_MONKEY, &asset));
    ReadEncodedAsset(f_MonkeyImage, &asset);
    f_AreCheepCheepsLeaping = FALSE;
}

void FreeHoleCreatureImages(void) {
    FreeMemory(&f_CheepCheepImages[0]);
    FreeMemory(&f_CheepCheepImages[1]);
    FreeMemory(&f_MonkeyImage);
}

Radians GetCameraYaw(void) {
    f32 x = g_Cameras->view.lookAt[0] - g_Cameras->view.eye[0];
    Radians yaw = 0.0f;
    f32 z = g_Cameras->view.lookAt[2] - g_Cameras->view.eye[2];

    if ((s32)x != 0 || (s32)z != 0) {
        yaw = atan2f(x, z);
    }

    return yaw;
}

Boolean IsMonkeyShown(void) {
    s32 creatureIndex;
    HoleCreature *creature;

    for (creatureIndex = 0; creatureIndex != HOLE_CREATURE_COUNT; creatureIndex++) {
        creature = &f_HoleCreatures[creatureIndex];
        if (creature->kind == HOLE_CREATURE_MONKEY) {
            return TRUE;
        }
    }

    return FALSE;
}

void StartMonkeyDrop(Radians cameraYaw) {
    EncodedAsset asset;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 transform;
    s32 creatureIndex;
    HoleCreature *creature;

    if (IsMonkeyShown()) {
        return;
    }

    OpenEncodedAsset(f_MonkeyFrameAssets[0], &asset);
    ReadEncodedAsset(f_MonkeyImage, &asset);
    for (creatureIndex = 0; creatureIndex != HOLE_CREATURE_COUNT; creatureIndex++) {
        creature = &f_HoleCreatures[creatureIndex];
        if (creature->kind == HOLE_CREATURE_NONE) {
            creature->kind = HOLE_CREATURE_MONKEY;
            creature->isGold = FALSE;
            creature->isFacingBack = FALSE;
            creature->frameCount = 0;
            creature->velocity[1] = 0;
            creature->position[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X];
            creature->position[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y] - MONKEY_DROP_HEIGHT;
            creature->position[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z];
            creature->yaw = -cameraYaw + M_PI;
            translation[AXIS_X] = creature->position[AXIS_X] * COURSE_TO_WORLD_SCALE;
            translation[AXIS_Y] = creature->position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            translation[AXIS_Z] = creature->position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            rotation[AXIS_X] = 0.0f;
            rotation[AXIS_Y] = creature->yaw;
            rotation[AXIS_Z] = 0;
            scale[AXIS_X] = 1.0f;
            scale[AXIS_Y] = 1.0f;
            scale[AXIS_Z] = 1.0f;
            BuildTransformMatrix(transform, rotation, translation, scale);
            ConvertMatrixToFixed(transform, &creature->matrices[0]);
            ConvertMatrixToFixed(transform, &creature->matrices[1]);
            return;
        }
    }
}

void ClearHoleCreatures(void) {
    s32 creatureIndex;
    HoleCreature *creature;

    for (creatureIndex = 0; creatureIndex != HOLE_CREATURE_COUNT; creatureIndex++) {
        creature = &f_HoleCreatures[creatureIndex];
        creature->kind = HOLE_CREATURE_NONE;
    }
}

void StartCheepCheepLeap(s32 startValue) {
    EncodedAsset asset;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 transform;
    s32 creatureIndex;
    HoleCreature *creature;
    f32 speed;

    f_UnusedLeapStartValue = startValue;
    f_AreCheepCheepsLeaping = TRUE;
    f_UnusedLeapValue = 0;
    f_OtherUnusedLeapValue = 0;
    for (creatureIndex = 0; creatureIndex != HOLE_CREATURE_COUNT; creatureIndex++) {
        creature = &f_HoleCreatures[creatureIndex];
        if (creature->kind == HOLE_CREATURE_NONE) {
            s32 random = guRandom();

            creature->kind = HOLE_CREATURE_CHEEP_CHEEP;
            creature->position[AXIS_X] = g_TurnBallSimulation.position[AXIS_X];
            creature->position[AXIS_Y] = g_TurnBallSimulation.position[AXIS_Y] + CHEEP_CHEEP_DEPTH;
            creature->position[AXIS_Z] = g_TurnBallSimulation.position[AXIS_Z];
            speed = (random % CHEEP_CHEEP_SPEED_RANGE) + CHEEP_CHEEP_SPEED_MIN;
            creature->groundHeight = GetGroundOrWaterHeight(
                g_TurnBallSimulation.position[AXIS_X],
                g_TurnBallSimulation.position[AXIS_Z]
            );
            creature->isFacingBack = guRandom() % 2;
            creature->isGold = guRandom() % 2;
            if (!creature->isGold) {
                OpenEncodedAsset(ASSET_CHEEP_CHEEP, &asset);
                ReadEncodedAsset(f_CheepCheepImages[0], &asset);
            }
            else {
                OpenEncodedAsset(ASSET_GOLD_CHEEP_CHEEP, &asset);
                ReadEncodedAsset(f_CheepCheepImages[1], &asset);
            }

            creature->yaw = g_SkyState.yaws[0];
            if (creature->isFacingBack) {
                creature->velocity[0] = cosf(creature->yaw) * -speed;
                creature->velocity[2] = -sinf(creature->yaw) * -speed;
            }
            else {
                creature->velocity[0] = cosf(creature->yaw) * speed;
                creature->velocity[2] = -sinf(creature->yaw) * speed;
            }

            creature->velocity[1] = -CHEEP_CHEEP_LEAP_SPEED;
            creature->pitch = -M_PI / 2;
            translation[AXIS_X] = creature->position[AXIS_X] * COURSE_TO_WORLD_SCALE;
            translation[AXIS_Y] = creature->position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            translation[AXIS_Z] = creature->position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            rotation[AXIS_X] = 0.0f;
            rotation[AXIS_Y] = creature->yaw;
            rotation[AXIS_Z] = creature->pitch + (M_PI / 2);
            scale[AXIS_X] = 1.0f;
            scale[AXIS_Y] = 1.0f;
            scale[AXIS_Z] = 1.0f;
            BuildTransformMatrix(transform, rotation, translation, scale);
            ConvertMatrixToFixed(transform, &creature->matrices[0]);
            ConvertMatrixToFixed(transform, &creature->matrices[1]);
            return;
        }
    }
}

void DrawHoleCreatures(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 creatureIndex;
    HoleCreature *creature;
    f32 unused[6] UNUSED;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 transform;
    Mat4 roll;
    CoursePoint splashPosition;
    EncodedAsset asset;
    f32 horizontalSpeed;
    f32 horizontalSpeedSquared;
    Boolean wasUnderwater;

    IsGameFlagSet(GAME_FLAG_TIMING_DEBUG);
    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_ZB_XLU_SURF | Z_UPD, G_RM_AA_ZB_XLU_SURF2 | Z_UPD);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        TEXEL0,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        TEXEL0,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0x01);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    for (creatureIndex = 0; creatureIndex != HOLE_CREATURE_COUNT; creatureIndex++) {
        creature = &f_HoleCreatures[creatureIndex];
        if (creature->kind == HOLE_CREATURE_NONE) {
        }
        else if (creature->kind == HOLE_CREATURE_CHEEP_CHEEP) {
            wasUnderwater = g_TurnBallSimulation.position[AXIS_Y] < creature->position[AXIS_Y];
            creature->velocity[1] += CHEEP_CHEEP_GRAVITY;
            creature->position[AXIS_X] += creature->velocity[0];
            creature->position[AXIS_Y] += creature->velocity[1];
            creature->position[AXIS_Z] += creature->velocity[2];
            horizontalSpeed = 0.0f;
            horizontalSpeedSquared = (creature->velocity[0] * creature->velocity[0]) +
                (creature->velocity[2] * creature->velocity[2]);
            if ((s32)horizontalSpeedSquared > 0) {
                horizontalSpeed = sqrtf(horizontalSpeedSquared);
            }

            creature->pitch =
                ((f32)atan2(creature->velocity[1], horizontalSpeed) - creature->pitch) *
                CHEEP_CHEEP_PITCH_EASING;
            translation[AXIS_X] = creature->position[AXIS_X] * COURSE_TO_WORLD_SCALE;
            translation[AXIS_Y] = creature->position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            translation[AXIS_Z] = creature->position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            rotation[AXIS_X] = 0.0f;
            rotation[AXIS_Y] = creature->yaw;
            rotation[AXIS_Z] = 0;
            if (!creature->isFacingBack) {
                scale[AXIS_Y] = 1.0f;
                scale[AXIS_Z] = 1.0f;
                scale[AXIS_X] = -1.0f;
            }
            else {
                scale[AXIS_X] = 1.0f;
                scale[AXIS_Y] = 1.0f;
                scale[AXIS_Z] = 1.0f;
            }

            BuildTransformMatrix(transform, rotation, translation, scale);
            guRotateRPYF(roll, 0.0f, 0.0f, -creature->pitch * DEGREES_PER_TURN / FULL_TURN_RADIANS);
            guMtxCatF(roll, transform, transform);
            ConvertMatrixToFixed(transform, &creature->matrices[g_MatrixBufferIndex]);
            if (wasUnderwater == TRUE) {
                if (creature->position[AXIS_Y] < g_TurnBallSimulation.position[AXIS_Y]) {
                    splashPosition[AXIS_X] = creature->position[AXIS_X];
                    splashPosition[AXIS_Z] = creature->position[AXIS_Z];
                    splashPosition[AXIS_Y] = creature->groundHeight;
                    SpawnRipple(splashPosition);
                    PlaySound(SOUND_SPLASH, HOLE_OBJECT_SOUND_CHANNEL, SPLASH_SOUND_VOLUME);
                }
            }
            else if (creature->position[AXIS_Y] > g_TurnBallSimulation.position[AXIS_Y]) {
                splashPosition[AXIS_X] = creature->position[AXIS_X];
                splashPosition[AXIS_Z] = creature->position[AXIS_Z];
                splashPosition[AXIS_Y] = creature->groundHeight;
                SpawnRipple(splashPosition);
                PlaySound(SOUND_SPLASH, HOLE_OBJECT_SOUND_CHANNEL, SPLASH_SOUND_VOLUME);
            }

            if (creature->position[AXIS_Y] < g_TurnBallSimulation.position[AXIS_Y]) {
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
            }
            else {
                s32 alpha = (g_TurnBallSimulation.position[AXIS_Y] - creature->position[AXIS_Y]) *
                    0xFF / CHEEP_CHEEP_FADE_DEPTH;

                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);
            }

            if (g_TurnBallSimulation.position[AXIS_Y] + CHEEP_CHEEP_DEPTH <
                creature->position[AXIS_Y]) {
                creature->kind = HOLE_CREATURE_NONE;
            }

            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(ImageBody(f_CheepCheepImages[creature->isGold])),
                G_IM_FMT_RGBA,
                G_IM_SIZ_16b,
                32,
                32,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                5,
                5,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPMatrix(
                gfx++,
                PhysicalAddress(&creature->matrices[g_MatrixBufferIndex]),
                G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
            );
            gSPVertex(gfx++, f_CheepCheepVertices, QUAD_VERTEX_COUNT, 0);
            gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
            gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
        }
        else if (creature->kind == HOLE_CREATURE_MONKEY) {
            creature->velocity[1] += GRAVITY;
            creature->position[AXIS_Y] += creature->velocity[1];
            switch (++creature->frameCount) {
                case MONKEY_SECOND_FRAME:
                    OpenEncodedAsset(f_MonkeyFrameAssets[1], &asset);
                    ReadEncodedAsset(f_MonkeyImage, &asset);
                    break;
                case MONKEY_THIRD_FRAME:
                    OpenEncodedAsset(f_MonkeyFrameAssets[2], &asset);
                    ReadEncodedAsset(f_MonkeyImage, &asset);
                    break;
                case MONKEY_LAST_FRAME:
                    OpenEncodedAsset(f_MonkeyFrameAssets[3], &asset);
                    ReadEncodedAsset(f_MonkeyImage, &asset);
                    PlaySound(SOUND_MONKEY, HOLE_OBJECT_SOUND_CHANNEL, SOUND_FULL_SCALE);
                    break;
            }

            if (creature->position[AXIS_Y] > g_HoleSetup.cupPosition[AXIS_Y]) {
                creature->position[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y];
            }

            if (creature->frameCount >= MONKEY_LIFETIME) {
                creature->kind = HOLE_CREATURE_NONE;
            }

            translation[AXIS_X] = creature->position[AXIS_X] * COURSE_TO_WORLD_SCALE;
            translation[AXIS_Y] = creature->position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            translation[AXIS_Z] = creature->position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            rotation[AXIS_X] = 0.0f;
            rotation[AXIS_Y] = GetCameraYaw();
            rotation[AXIS_Z] = 0;
            scale[AXIS_X] = 1.0f;
            scale[AXIS_Y] = 1.0f;
            scale[AXIS_Z] = 1.0f;
            BuildTransformMatrix(transform, rotation, translation, scale);
            ConvertMatrixToFixed(transform, &creature->matrices[g_MatrixBufferIndex]);
            gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
            gDPPipeSync(gfx++);
            gSPMatrix(
                gfx++,
                PhysicalAddress(&creature->matrices[g_MatrixBufferIndex]),
                G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
            );
            gSPVertex(gfx++, f_MonkeyVertices, 8, 0);
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(ImageBody(f_MonkeyImage)),
                G_IM_FMT_RGBA,
                G_IM_SIZ_16b,
                64,
                32,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
            gDPPipeSync(gfx++);
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(ImageBody(f_MonkeyImage) + MONKEY_HALF_IMAGE_SIZE),
                G_IM_FMT_RGBA,
                G_IM_SIZ_16b,
                64,
                32,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gSP2Triangles(gfx++, 4, 5, 6, 0, 4, 6, 7, 0);
            gDPPipeSync(gfx++);
            gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
        }
    }

    *displayListHead = gfx;
}
