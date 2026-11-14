#include "hole_triangles.h"

#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "c_language.h"
#include "camera.h"
#include "compiler.h"
#include "course_space.h"
#include "game.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "math_util.h"
#include "particle_effects.h"
#include "play.h"
#include "player.h"
#include "screen.h"
#include "shot_camera.h"
#include "work_area.h"

#define f_HoleTriangles (g_WorkArea.hole.triangles)

#define BLENDED_TEXTURE_SIZE 32
#define TEXTURE_COORDINATE_SCALE 4

#define SHADED_EDGE_LENGTH_MIN 0.01f
#define SHADED_ALPHA_SCALE 128.0f
#define SHADED_ALPHA_BASE 128.0f
#define SHADED_ALPHA_MAX OPAQUE_ALPHA
#define DEFAULT_SHADED_ALPHA 0x80
#define SHADED_FOG_MIN 700
#define SHADED_FOG_MAX 1000

#define RING_SECTION_COUNT 8
#define RING_SIDE_COUNT 4
#define RING_VERTEX_COUNT (RING_SECTION_COUNT * RING_SIDE_COUNT)
#define RING_SECTION_ANGLE (2 * (f32)M_PI / RING_SECTION_COUNT)
#define RING_RADIUS 128.0f
#define RING_THICKNESS 16.0f
#define RING_NORMAL_SCALE 127.0f
#define RING_VERTEX_ALPHA 0x80

#define RingVertex(section, side) \
    ((((section) & (RING_SECTION_COUNT - 1)) * RING_SIDE_COUNT) + (side))

#define RING_SCALE_ONE 1024.0f

typedef struct RingCorners {
    s32 unusedBeforeCoordinates;
    s16 coordinates[AXIS_COUNT][TRIANGLE_VERTEX_COUNT];
    s16 unusedAfterCoordinates[(0x2C - 0x16) / 2];
} RingCorners;

#define RING_SPIN_SPEED 2.0

#define RING_FADE_STEP 16
#define RING_SPARKLE_SPACING 4
#define RING_SPARKLE_TURN ((f32)M_PI / 64)

#define RING_RED 0xFF
#define RING_GREEN 0xF0
#define RING_BLUE 0x40
#define RING_MODE_FOUR_RED 0xFF
#define RING_MODE_FOUR_GREEN 0xC8
#define RING_MODE_FOUR_BLUE 0x00

static Boolean8 f_DimsTranslucentTriangles = FALSE;
static s32 f_UnusedWordAfterDimmingFlag UNUSED = 0;
static Boolean f_HasTestRing = FALSE;
static s32 f_TextureScroll = 0;
static Degrees f_RingAngle = 0.0f;
static s32 f_UnusedWordAfterRingAngle UNUSED = 0;
static Lights1 f_RingLights = gdSPDefLights1(0x7F, 0x7F, 0x7F, 0xFF, 0xFF, 0xFF, -73, 73, 73);
static Mtx f_RingSpinMatrices[DRAW_BUFFER_COUNT];
static Vtx f_RingVertices[RING_VERTEX_COUNT];

static void AddTestRing(void);
static s32 BuildRingMatrices(void);

FILE_STATIC Mtx f_RingMatrices[RING_COUNT];

void DimNextTranslucentTriangles(void) {
    f_DimsTranslucentTriangles = TRUE;
}

void DrawHoleTriangles(
    Gfx **displayListHead,
    CameraMatrices *matrices UNUSED,
    Camera *camera UNUSED
) {
    Gfx *gfx = *displayListHead;
    s32 index;
    s32 vertexIndex;
    f32 edgeX;
    f32 edgeY;
    f32 edgeZ;
    f32 zAlignment;
    s32 alpha;

    if (IsGameFlagSet(GAME_FLAG_TEST_RING)) {
        if (!f_HasTestRing) {
            AddTestRing();
            f_HasTestRing = TRUE;
        }
    }
    else {
        f_HasTestRing = FALSE;
    }

    gDPPipeSync(gfx++);
    AppendSyncedRdpState(gfx, gDPSetCycleType, G_CYC_2CYCLE);
    gSPClipRatio(gfx++, FRUSTRATIO_1);
    AppendSyncedRdpState(gfx, gDPSetColorDither, G_CD_MAGICSQ);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_FOG | G_SHADING_SMOOTH);
    AppendSyncedRdpState(gfx, gDPSetTextureFilter, G_TF_BILERP);
    AppendSyncedRdpState(gfx, gDPSetTexturePersp, G_TP_PERSP);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0xFE);
    AppendSyncedRdpState(gfx, gDPSetTextureLOD, G_TL_TILE);
    AppendSyncedRdpState(gfx, gDPSetTextureLUT, G_TT_NONE);
    AppendSyncedRdpState(gfx, gDPSetRenderMode, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2);
    AppendSyncedRdpState(gfx, gDPSetAlphaCompare, G_AC_NONE);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0x3F);
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(ImageBody(g_BlendedTextureScrollingLayer)),
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        BLENDED_TEXTURE_SIZE,
        BLENDED_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(BLENDED_TEXTURE_SIZE),
        TextureMask(BLENDED_TEXTURE_SIZE),
        15,
        15
    );
    gDPLoadMultiBlock(
        gfx++,
        TextureAddress(ImageBody(g_BlendedTextureStillLayer)),
        SECOND_TEXTURE_TMEM,
        1,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        BLENDED_TEXTURE_SIZE,
        BLENDED_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(BLENDED_TEXTURE_SIZE),
        TextureMask(BLENDED_TEXTURE_SIZE),
        15,
        15
    );
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        TEXEL1,
        PRIMITIVE_ALPHA,
        TEXEL1,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        COMBINED,
        0,
        0,
        0,
        0,
        COMBINED
    );
    gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0x7F, 0x7F, 0xA1);
    gDPSetEnvColor(gfx++, 0x3F, 0x7F, 0x9F, OPAQUE_ALPHA);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    f_TextureScroll++;
    gDPSetTileSize(
        gfx++,
        G_TX_RENDERTILE,
        0,
        f_TextureScroll,
        BLENDED_TEXTURE_SIZE,
        f_TextureScroll + BLENDED_TEXTURE_SIZE
    );
    gDPSetTileSize(
        gfx++,
        1,
        f_TextureScroll,
        0,
        f_TextureScroll + BLENDED_TEXTURE_SIZE,
        BLENDED_TEXTURE_SIZE
    );
    for (index = 0; index < HOLE_TRIANGLE_COUNT; index++) {
        switch (f_HoleTriangles[index].kind) {
            case HOLE_TRIANGLE_KIND_FLAT:
                for (vertexIndex = 0; vertexIndex < TRIANGLE_VERTEX_COUNT; vertexIndex++) {
                    f_HoleTriangles[index].vertices[vertexIndex].v.tc[TEXTURE_AXIS_S] =
                        f_HoleTriangles[index].vertices[vertexIndex].v.ob[AXIS_X] *
                        TEXTURE_COORDINATE_SCALE;
                    f_HoleTriangles[index].vertices[vertexIndex].v.tc[TEXTURE_AXIS_T] =
                        (f_HoleTriangles[index].vertices[vertexIndex].v.ob[AXIS_Z] +
                            f_HoleTriangles[index].vertices[vertexIndex].v.ob[AXIS_Y]) *
                        TEXTURE_COORDINATE_SCALE;
                }

                gSPVertex(gfx++, f_HoleTriangles[index].vertices, TRIANGLE_VERTEX_COUNT, 0);
                gSP1Triangle(gfx++, 0, 1, 2, 0);
                break;
            case HOLE_TRIANGLE_KIND_UPRIGHT:
                for (vertexIndex = 0; vertexIndex < TRIANGLE_VERTEX_COUNT; vertexIndex++) {
                    f_HoleTriangles[index].vertices[vertexIndex].v.tc[TEXTURE_AXIS_S] =
                        (f_HoleTriangles[index].vertices[vertexIndex].v.ob[AXIS_X] +
                            f_HoleTriangles[index].vertices[vertexIndex].v.ob[AXIS_Z]) *
                        TEXTURE_COORDINATE_SCALE;
                    f_HoleTriangles[index].vertices[vertexIndex].v.tc[TEXTURE_AXIS_T] =
                        f_HoleTriangles[index].vertices[vertexIndex].v.ob[AXIS_Y] *
                        TEXTURE_COORDINATE_SCALE;
                }

                gDPSetPrimColor(gfx++, 0, 0, 0x3F, 0xB4, 0xFF, 0x40);
                gDPSetEnvColor(gfx++, 0x80, 0xBE, 0xFF, 0x10);
                gSPVertex(gfx++, f_HoleTriangles[index].vertices, TRIANGLE_VERTEX_COUNT, 0);
                gSP1Triangle(gfx++, 0, 1, 2, 0);
                break;
            case HOLE_TRIANGLE_KIND_SHADED:
                if (g_IsCupViewShown) {
                    break;
                }

                edgeY = 0.0f;
                edgeX = f_HoleTriangles[index].vertices[1].v.ob[AXIS_X] -
                    f_HoleTriangles[index].vertices[2].v.ob[AXIS_X];
                edgeZ = f_HoleTriangles[index].vertices[1].v.ob[AXIS_Z] -
                    f_HoleTriangles[index].vertices[2].v.ob[AXIS_Z];
                if (fabsf(edgeX) + fabsf(edgeZ) < SHADED_EDGE_LENGTH_MIN) {
                    break;
                }

                guNormalize(&edgeX, &edgeY, &edgeZ);
                zAlignment = edgeZ;
                if (zAlignment < 0.0f) {
                    zAlignment = -zAlignment;
                }

                alpha = (zAlignment * SHADED_ALPHA_SCALE) + SHADED_ALPHA_BASE;
                if (alpha > SHADED_ALPHA_MAX) {
                    alpha = SHADED_ALPHA_MAX;
                }

                if (alpha < 0) {
                    alpha = DEFAULT_SHADED_ALPHA;
                }

                gSPFogPosition(gfx++, SHADED_FOG_MIN, SHADED_FOG_MAX);
                AppendSyncedRdpState(gfx, gDPSetAlphaDither, G_AD_PATTERN);
                AppendSyncedRdpState(gfx, gDPSetRenderMode, G_RM_PASS, G_RM_AA_ZB_XLU_INTER2);
                if (f_DimsTranslucentTriangles) {
                    gDPSetCombineLERP(
                        gfx++,
                        PRIMITIVE,
                        0,
                        PRIMITIVE_ALPHA,
                        0,
                        0,
                        0,
                        0,
                        1,
                        PRIMITIVE,
                        0,
                        PRIMITIVE_ALPHA,
                        0,
                        ENVIRONMENT,
                        0,
                        SHADE,
                        0
                    );
                }
                else {
                    gDPSetCombineLERP(
                        gfx++,
                        PRIMITIVE,
                        0,
                        PRIMITIVE_ALPHA,
                        0,
                        0,
                        0,
                        0,
                        1,
                        PRIMITIVE,
                        0,
                        PRIMITIVE_ALPHA,
                        0,
                        1,
                        ENVIRONMENT,
                        SHADE,
                        ENVIRONMENT
                    );
                }

                gDPSetPrimColor(gfx++, 0, 0, 0xE0, 0xE0, 0x00, alpha);
                gDPSetEnvColor(gfx++, 0xE0, 0xE0, 0x00, 0x40);
                gDPPipeSync(gfx++);
                gSPVertex(gfx++, f_HoleTriangles[index].vertices, TRIANGLE_VERTEX_COUNT, 0);
                gSP1Triangle(gfx++, 0, 1, 2, 0);
                gDPPipeSync(gfx++);
                break;
        }
    }

    AppendSyncedRdpState(gfx, gDPSetTextureLUT, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    f_DimsTranslucentTriangles = FALSE;
    *displayListHead = gfx;
}

void DrawRings(Gfx **displayListHead, CameraMatrices *matrices UNUSED, Camera *camera UNUSED) {
    Gfx *gfx = *displayListHead;
    Mat4 transform;
    Vec3 ringPoint;
    Vec3 worldPoint;
    s32 position[AXIS_COUNT];
    s32 triangleIndex;
    s32 red;
    s32 green;
    s32 blue;
    s32 ringIndex;
    s32 ringState;
    s32 section;
    s32 sparkleSection;
    s32 side;
    s32 nextSide;
    s32 sectionFirst;
    s32 nextSectionFirst;
    s32 alpha;
    Radians angle;
    f32 sparkleTurn;

    gDPPipeSync(gfx++);
    gSPSetLights1(gfx++, f_RingLights);
    gDPPipeSync(gfx++);
    AppendSyncedRdpState(gfx, gDPSetCycleType, G_CYC_1CYCLE);
    gSPClipRatio(gfx++, FRUSTRATIO_1);
    AppendSyncedRdpState(gfx, gDPSetColorDither, G_CD_MAGICSQ);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(
        gfx++,
        G_ZBUFFER | G_SHADE | G_CULL_BACK | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_SHADING_SMOOTH
    );
    AppendSyncedRdpState(gfx, gDPSetTextureFilter, G_TF_BILERP);
    AppendSyncedRdpState(gfx, gDPSetTexturePersp, G_TP_PERSP);
    AppendSyncedRdpState(gfx, gDPSetAlphaCompare, G_AC_NONE);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0xFE);
    AppendSyncedRdpState(gfx, gDPSetTextureLOD, G_TL_TILE);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0x3F);
    AppendSyncedRdpState(gfx, gDPSetTextureLUT, G_TT_NONE);
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(g_MirrorTexture),
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gSPTexture(gfx++, 0x07C0, 0x07C0, 0, G_TX_RENDERTILE, G_ON);
    if (g_PauseScreen == PAUSE_SCREEN_FREE_LOOK) {
        gDPSetCombineLERP(
            gfx++,
            PRIMITIVE,
            TEXEL0,
            ENV_ALPHA,
            TEXEL0,
            0,
            0,
            0,
            PRIMITIVE,
            PRIMITIVE,
            TEXEL0,
            ENV_ALPHA,
            TEXEL0,
            0,
            0,
            0,
            PRIMITIVE
        );
        red = RING_MODE_FOUR_RED;
        green = RING_MODE_FOUR_GREEN;
        blue = RING_MODE_FOUR_BLUE;
    }
    else {
        gDPSetCombineLERP(
            gfx++,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            0,
            0,
            0,
            PRIMITIVE,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            0,
            0,
            0,
            PRIMITIVE
        );
        red = RING_RED;
        green = RING_GREEN;
        blue = RING_BLUE;
    }

    gDPSetEnvColor(gfx++, 0x78, 0x78, 0x20, 0xBF);
    f_RingAngle += RING_SPIN_SPEED;
    guRotateRPY(&f_RingSpinMatrices[g_DrawBufferIndex], 0.0f, 0.0f, f_RingAngle);
    ringIndex = 0;
    for (triangleIndex = 0; triangleIndex < HOLE_TRIANGLE_COUNT; triangleIndex++) {
        if (ringIndex >= RING_COUNT) {
            break;
        }

        if (f_HoleTriangles[triangleIndex].kind == HOLE_TRIANGLE_KIND_RING) {
            ringState = g_Players[g_GameState.currentPlayerIndex].ringStates[ringIndex];
            if (ringState > RING_STATE_NONE) {
                if (ringState != RING_STATE_SHOWN) {
                    if (ringState < RING_STATE_FADED) {
                        sparkleTurn = ringState * RING_SPARKLE_TURN;
                        guMtxL2F(transform, &f_RingMatrices[ringIndex]);
                        for (sparkleSection = ringState & (RING_SPARKLE_SPACING - 1);
                            sparkleSection < RING_SECTION_COUNT;
                            sparkleSection += RING_SPARKLE_SPACING) {
                            angle = sparkleTurn + (sparkleSection * RING_SECTION_ANGLE);
                            ringPoint[AXIS_X] = cosf(angle) * RING_RADIUS;
                            ringPoint[AXIS_Y] = sinf(angle) * RING_RADIUS;
                            ringPoint[AXIS_Z] = 0;
                            guMtxXFMF(
                                transform,
                                ringPoint[AXIS_X],
                                ringPoint[AXIS_Y],
                                ringPoint[AXIS_Z],
                                &worldPoint[AXIS_X],
                                &worldPoint[AXIS_Y],
                                &worldPoint[AXIS_Z]
                            );
                            position[AXIS_X] = worldPoint[AXIS_X] * WORLD_TO_COURSE_SCALE;
                            position[AXIS_Y] = worldPoint[AXIS_Y] * WORLD_TO_COURSE_SCALE;
                            position[AXIS_Z] = worldPoint[AXIS_Z] * WORLD_TO_COURSE_SCALE;
                            SpawnSparkle(position);
                        }
                    }

                    ringState++;
                    if (ringState > RING_STATE_FADED) {
                        ringState = RING_STATE_FADED;
                    }

                    alpha = OPAQUE_ALPHA - (ringState * RING_FADE_STEP);
                    if (alpha < 0) {
                        alpha = 0;
                    }

                    g_Players[g_GameState.currentPlayerIndex].ringStates[ringIndex] = ringState;
                    AppendSyncedRdpState(
                        gfx,
                        gDPSetRenderMode,
                        G_RM_AA_ZB_XLU_SURF,
                        G_RM_AA_ZB_XLU_SURF2
                    );
                }
                else {
                    alpha = OPAQUE_ALPHA;
                    AppendSyncedRdpState(
                        gfx,
                        gDPSetRenderMode,
                        G_RM_AA_ZB_OPA_SURF,
                        G_RM_AA_ZB_OPA_SURF2
                    );
                }

                gDPSetPrimColor(gfx++, 0, 0, red, green, blue, alpha);
                gSPMatrix(
                    gfx++,
                    &f_RingMatrices[ringIndex],
                    G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH
                );
                gSPMatrix(
                    gfx++,
                    &f_RingSpinMatrices[g_DrawBufferIndex],
                    G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH
                );
                gSPVertex(gfx++, f_RingVertices, RING_VERTEX_COUNT, 0);
                for (section = 0; section < RING_SECTION_COUNT; section++) {
                    sectionFirst = RingVertex(section, 0);
                    nextSectionFirst = RingVertex(section + 1, 0);
                    for (side = 0; side < RING_SIDE_COUNT; side++) {
                        nextSide = (side + 1) & (RING_SIDE_COUNT - 1);
                        gSP2Triangles(
                            gfx++,
                            sectionFirst + side,
                            sectionFirst + nextSide,
                            nextSectionFirst + nextSide,
                            0,
                            sectionFirst + side,
                            nextSectionFirst + nextSide,
                            nextSectionFirst + side,
                            0
                        );
                    }
                }

                gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
            }

            ringIndex++;
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

static void BuildRingVertices(void) {
    s32 index;
    Vtx *vertex;
    Boolean isOuter;
    Boolean isRaised;
    Radians angle;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 x;
    f32 y;

    for (index = 0; index < RING_VERTEX_COUNT; index++) {
        vertex = &f_RingVertices[index];
        isOuter = (index & 2) >> 1;
        isRaised = (index & 1) ^ isOuter;
        angle = (index / RING_SIDE_COUNT) * RING_SECTION_ANGLE;
        offsetX = cosf(angle) * (isOuter - 0.5f) * RING_THICKNESS;
        offsetY = sinf(angle) * (isOuter - 0.5f) * RING_THICKNESS;
        x = (cosf(angle) * RING_RADIUS) + offsetX;
        y = (sinf(angle) * RING_RADIUS) + offsetY;
        offsetZ = (isRaised - 0.5f) * RING_THICKNESS;
        vertex->n.ob[AXIS_X] = x;
        vertex->n.ob[AXIS_Y] = y;
        vertex->n.ob[AXIS_Z] = offsetZ;
        guNormalize(&offsetX, &offsetZ, &offsetY);
        vertex->n.n[AXIS_X] = offsetX * RING_NORMAL_SCALE;
        vertex->n.n[AXIS_Y] = offsetY * RING_NORMAL_SCALE;
        vertex->n.n[AXIS_Z] = offsetZ * RING_NORMAL_SCALE;
        vertex->n.a = RING_VERTEX_ALPHA;
    }
}

static void AddTestRing(void) {
    f_HoleTriangles[0].kind = HOLE_TRIANGLE_KIND_RING;
    f_HoleTriangles[0].vertices[0].v.ob[AXIS_X] = 0x800;
    f_HoleTriangles[0].vertices[0].v.ob[AXIS_Y] = -0x100;
    f_HoleTriangles[0].vertices[0].v.ob[AXIS_Z] = 0x1000;
    f_HoleTriangles[0].vertices[1].v.ob[AXIS_X] = 0x800;
    f_HoleTriangles[0].vertices[1].v.ob[AXIS_Y] = -0x100;
    f_HoleTriangles[0].vertices[1].v.ob[AXIS_Z] = 0xF00;
    f_HoleTriangles[0].vertices[2].v.ob[AXIS_X] = 0x700;
    f_HoleTriangles[0].vertices[2].v.ob[AXIS_Y] = -0x100;
    f_HoleTriangles[0].vertices[2].v.ob[AXIS_Z] = 0xF00;
    InitRings();
}

static s32 BuildRingMatrices(void) {
    Mat4 rotation;
    Mat4 transform;
    RingCorners corners;
    Vec3 up;
    f32 scale;
    s32 ringCount = 0;
    s32 index;
    HoleTriangle *triangle;

    for (index = 0; index < HOLE_TRIANGLE_COUNT; index++) {
        triangle = &f_HoleTriangles[index];
        if (triangle->kind != HOLE_TRIANGLE_KIND_RING) {
            continue;
        }

        corners.coordinates[AXIS_X][RING_CENTER] = triangle->vertices[RING_CENTER].v.ob[AXIS_X];
        corners.coordinates[AXIS_X][RING_FACING] = triangle->vertices[RING_FACING].v.ob[AXIS_X];
        corners.coordinates[AXIS_X][RING_SCALE] = triangle->vertices[RING_SCALE].v.ob[AXIS_X];
        corners.coordinates[AXIS_Y][RING_CENTER] = triangle->vertices[RING_CENTER].v.ob[AXIS_Y];
        corners.coordinates[AXIS_Y][RING_FACING] = triangle->vertices[RING_FACING].v.ob[AXIS_Y];
        corners.coordinates[AXIS_Y][RING_SCALE] = triangle->vertices[RING_SCALE].v.ob[AXIS_Y];
        corners.coordinates[AXIS_Z][RING_CENTER] = triangle->vertices[RING_CENTER].v.ob[AXIS_Z];
        corners.coordinates[AXIS_Z][RING_FACING] = triangle->vertices[RING_FACING].v.ob[AXIS_Z];
        corners.coordinates[AXIS_Z][RING_SCALE] = triangle->vertices[RING_SCALE].v.ob[AXIS_Z];
        if (corners.coordinates[AXIS_X][RING_FACING] | corners.coordinates[AXIS_Y][RING_FACING] |
            corners.coordinates[AXIS_Z][RING_FACING]) {
            triangle->ignoredCenter[AXIS_X] = (corners.coordinates[AXIS_X][RING_CENTER] +
                                                  corners.coordinates[AXIS_X][RING_FACING] +
                                                  corners.coordinates[AXIS_X][RING_SCALE]) /
                TRIANGLE_VERTEX_COUNT;
            triangle->ignoredCenter[AXIS_Y] = (corners.coordinates[AXIS_Y][RING_CENTER] +
                                                  corners.coordinates[AXIS_Y][RING_FACING] +
                                                  corners.coordinates[AXIS_Y][RING_SCALE]) /
                TRIANGLE_VERTEX_COUNT;
            triangle->ignoredCenter[AXIS_Z] = (corners.coordinates[AXIS_Z][RING_CENTER] +
                                                  corners.coordinates[AXIS_Z][RING_FACING] +
                                                  corners.coordinates[AXIS_Z][RING_SCALE]) /
                TRIANGLE_VERTEX_COUNT;
            up[AXIS_X] = up[AXIS_Z] = 0.0f;
            up[AXIS_Y] = -1.0f;
            guAlignF(
                rotation,
                0.0f,
                corners.coordinates[AXIS_X][RING_FACING],
                corners.coordinates[AXIS_Y][RING_FACING],
                corners.coordinates[AXIS_Z][RING_FACING]
            );
            scale = corners.coordinates[AXIS_X][RING_SCALE] / RING_SCALE_ONE;
            guScaleF(transform, scale, scale, scale);
            guMtxCatF(transform, rotation, rotation);
            guTranslateF(
                transform,
                corners.coordinates[AXIS_X][RING_CENTER],
                corners.coordinates[AXIS_Y][RING_CENTER],
                corners.coordinates[AXIS_Z][RING_CENTER]
            );
            guMtxCatF(rotation, transform, rotation);
        }
        else {
            guMtxIdentF(rotation);
        }

        guMtxF2L(rotation, &f_RingMatrices[ringCount]);
        ringCount++;
    }

    return ringCount;
}

void InitRings(void) {
    s32 ringCount;
    s32 playerIndex;
    s32 ringIndex;

    BuildRingVertices();
    ringCount = BuildRingMatrices();
    if (ringCount >= RING_COUNT) {
        ringCount = RING_COUNT;
    }

    if (g_GameState.isRoundContinued != ROUND_CONTINUED) {
        for (playerIndex = 0; playerIndex < PLAYER_COUNT_MAX; playerIndex++) {
            for (ringIndex = 0; ringIndex < RING_COUNT; ringIndex++) {
                g_Players[playerIndex].ringStates[ringIndex] = ringIndex < ringCount;
            }
        }
    }
}
