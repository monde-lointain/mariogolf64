/*
 * Draws the hole triangles, making the shaded ones see-through, and builds and
 * draws Ring Shot's rings, which are kept among them.
 */
#include "common.h"

#include "hole_triangles.h"

#include "camera.h"
#include "game_flags.h"
#include "save.h"
#include "variables.h"
#include "work_area.h"

/* Named like the file-local data it stands in for */
/* NOLINTNEXTLINE(readability-identifier-naming) */
#define f_HoleTriangles (g_WorkArea.hole.triangles)

/* The size and texture scale of the blended texture on the triangles */
#define BLENDED_TEXTURE_SIZE 32
#define TEXTURE_COORDINATE_SCALE 4

/* A shaded triangle's edge shorter than this, along x and z, is not drawn */
#define MIN_SHADED_EDGE_LENGTH 0.01f
#define SHADED_ALPHA_SCALE 128.0f
#define SHADED_ALPHA_BASE 128.0f
#define MAX_SHADED_ALPHA 0xFF
#define DEFAULT_SHADED_ALPHA 0x80
#define SHADED_FOG_MIN 700
#define SHADED_FOG_MAX 1000

/* Each ring is a square tube bent into a circle of sections */
#define RING_SECTION_COUNT 8
#define RING_SIDE_COUNT 4
#define RING_VERTEX_COUNT (RING_SECTION_COUNT * RING_SIDE_COUNT)
#define RING_SECTION_ANGLE (2 * (f32)M_PI / RING_SECTION_COUNT)
#define RING_RADIUS 128.0f
#define RING_THICKNESS 16.0f
#define RING_NORMAL_SCALE 127.0f
#define RING_VERTEX_ALPHA 0x80

/* A ring's corners give its center, its facing and, in the third's x, its scale
 */
#define RING_SCALE_ONE 1024.0f

/*
 * A ring triangle's corners, by axis and then by corner. The words around
 * them are never used; they keep the original's stack layout.
 */
typedef struct RingCorners {
  /* 0x00 */ s32 unk0;
  /* 0x04 */ s16 coordinates[3][HOLE_TRIANGLE_VERTEX_COUNT];
  /* 0x16 */ s16 unk16[(0x2C - 0x16) / 2];
} RingCorners; /* size = 0x2C */

/* Degrees that the rings spin by each frame */
#define RING_SPIN_SPEED 2.0

#define RING_FADE_STEP 16
#define RING_SPARKLE_SPACING 4 /* In sections */
#define RING_SPARKLE_TURN ((f32)M_PI / 64) /* Radians each frame */
#define POSITION_SCALE 1024.0f

/* The rings' colors, apart from under PAUSE_SCREEN_UNK4 */
#define RING_RED 0xFF
#define RING_GREEN 0xF0
#define RING_BLUE 0x40
#define RING_MODE_FOUR_RED 0xFF
#define RING_MODE_FOUR_GREEN 0xC8
#define RING_MODE_FOUR_BLUE 0x00

/* GameState.unk30's value with which InitRings leaves the players' rings alone
 */
#define KEEP_RING_STATES 2

extern s8 f_DimsTranslucentTriangles;
extern s32 f_HasTestRing;
extern s32 f_TextureScroll;
extern f32 f_RingAngle;
extern Lights1 f_RingLights;
extern Mtx f_RingSpinMatrices[2];
extern Vtx f_RingVertices[RING_VERTEX_COUNT];
extern s32 g_IsCupViewShown;
extern u8* g_BlendedTextureScrollingLayer;
extern u8* g_BlendedTextureStillLayer;

void SpawnSparkle(s32* position);
static void AddTestRing(void);
static s32 BuildRingMatrices(void);

/* Not static: the original left it to the linker's common block */
Mtx f_RingMatrices[RING_COUNT];

/* Makes the shaded triangles more see-through for the next frame */
void DimNextTranslucentTriangles(void) {
  f_DimsTranslucentTriangles = TRUE;
}

/* Draws the hole's triangles other than the rings */
#ifdef NON_MATCHING
void DrawHoleTriangles(
    Gfx** displayListHead, CameraMatrices* matrices, Camera* camera) {
  Gfx* gfx = *displayListHead;
  s32 index;
  s32 vertexIndex;
  f32 edgeX;
  f32 edgeY;
  f32 edgeZ;
  f32 along;
  s32 alpha;

  if (IsGameFlagSet(GAME_FLAG_TEST_RING)) {
    if (!f_HasTestRing) {
      AddTestRing();
      f_HasTestRing = TRUE;
    }
  } else {
    f_HasTestRing = FALSE;
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_2CYCLE);
  gSPClipRatio(gfx++, FRUSTRATIO_1);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_MAGICSQ);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_FOG | G_SHADING_SMOOTH);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0xFE);
  gDPPipeSync(gfx++);
  gDPSetTextureLOD(gfx++, G_TL_TILE);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0x3F);
  gDPLoadTextureBlock(gfx++, (u32)(g_BlendedTextureScrollingLayer + 8) & ~7,
      G_IM_FMT_RGBA, G_IM_SIZ_16b, BLENDED_TEXTURE_SIZE, BLENDED_TEXTURE_SIZE,
      0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5, 15, 15);
  gDPLoadMultiBlock(gfx++, (u32)(g_BlendedTextureStillLayer + 8) & ~7,
      SECOND_TEXTURE_TMEM, 1, G_IM_FMT_RGBA, G_IM_SIZ_16b, BLENDED_TEXTURE_SIZE,
      BLENDED_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, 5, 5, 15, 15);
  gDPSetCombineLERP(gfx++, TEXEL0, TEXEL1, PRIMITIVE_ALPHA, TEXEL1, 0, 0, 0,
      PRIMITIVE, PRIMITIVE, 0, COMBINED, 0, 0, 0, 0, COMBINED);
  gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0x7F, 0x7F, 0xA1);
  gDPSetEnvColor(gfx++, 0x3F, 0x7F, 0x9F, 0xFF);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  f_TextureScroll++;
  gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, f_TextureScroll,
      BLENDED_TEXTURE_SIZE, f_TextureScroll + BLENDED_TEXTURE_SIZE);
  gDPSetTileSize(gfx++, 1, f_TextureScroll, 0,
      f_TextureScroll + BLENDED_TEXTURE_SIZE, BLENDED_TEXTURE_SIZE);
  for (index = 0; index < HOLE_TRIANGLE_COUNT; index++) {
    switch (f_HoleTriangles[index].kind) {
      case HOLE_TRIANGLE_KIND_FLAT:
        for (vertexIndex = 0; vertexIndex < HOLE_TRIANGLE_VERTEX_COUNT;
            vertexIndex++) {
          f_HoleTriangles[index].vertices[vertexIndex].v.tc[0] =
              f_HoleTriangles[index].vertices[vertexIndex].v.ob[0] *
              TEXTURE_COORDINATE_SCALE;
          f_HoleTriangles[index].vertices[vertexIndex].v.tc[1] =
              (f_HoleTriangles[index].vertices[vertexIndex].v.ob[2] +
                  f_HoleTriangles[index].vertices[vertexIndex].v.ob[1]) *
              TEXTURE_COORDINATE_SCALE;
        }

        gSPVertex(gfx++, f_HoleTriangles[index].vertices,
            HOLE_TRIANGLE_VERTEX_COUNT, 0);
        gSP1Triangle(gfx++, 0, 1, 2, 0);
        break;
      case HOLE_TRIANGLE_KIND_UPRIGHT:
        for (vertexIndex = 0; vertexIndex < HOLE_TRIANGLE_VERTEX_COUNT;
            vertexIndex++) {
          f_HoleTriangles[index].vertices[vertexIndex].v.tc[0] =
              (f_HoleTriangles[index].vertices[vertexIndex].v.ob[0] +
                  f_HoleTriangles[index].vertices[vertexIndex].v.ob[2]) *
              TEXTURE_COORDINATE_SCALE;
          f_HoleTriangles[index].vertices[vertexIndex].v.tc[1] =
              f_HoleTriangles[index].vertices[vertexIndex].v.ob[1] *
              TEXTURE_COORDINATE_SCALE;
        }

        gDPSetPrimColor(gfx++, 0, 0, 0x3F, 0xB4, 0xFF, 0x40);
        gDPSetEnvColor(gfx++, 0x80, 0xBE, 0xFF, 0x10);
        gSPVertex(gfx++, f_HoleTriangles[index].vertices,
            HOLE_TRIANGLE_VERTEX_COUNT, 0);
        gSP1Triangle(gfx++, 0, 1, 2, 0);
        break;
      case HOLE_TRIANGLE_KIND_SHADED:
        if (g_IsCupViewShown != 0) {
          break;
        }

        edgeY = 0.0f;
        edgeX = f_HoleTriangles[index].vertices[1].v.ob[0] -
            f_HoleTriangles[index].vertices[2].v.ob[0];
        edgeZ = f_HoleTriangles[index].vertices[1].v.ob[2] -
            f_HoleTriangles[index].vertices[2].v.ob[2];
        if (fabsf(edgeX) + fabsf(edgeZ) < MIN_SHADED_EDGE_LENGTH) {
          break;
        }

        guNormalize(&edgeX, &edgeY, &edgeZ);
        along = edgeZ;
        if (along < 0.0f) {
          along = -along;
        }

        alpha = (along * SHADED_ALPHA_SCALE) + SHADED_ALPHA_BASE;
        if (alpha > MAX_SHADED_ALPHA) {
          alpha = MAX_SHADED_ALPHA;
        }

        if (alpha < 0) {
          alpha = DEFAULT_SHADED_ALPHA;
        }

        gSPFogPosition(gfx++, SHADED_FOG_MIN, SHADED_FOG_MAX);
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_PASS, G_RM_AA_ZB_XLU_INTER2);
        if (f_DimsTranslucentTriangles) {
          gDPSetCombineLERP(gfx++, PRIMITIVE, 0, PRIMITIVE_ALPHA, 0, 0, 0, 0, 1,
              PRIMITIVE, 0, PRIMITIVE_ALPHA, 0, ENVIRONMENT, 0, SHADE, 0);
        } else {
          gDPSetCombineLERP(gfx++, PRIMITIVE, 0, PRIMITIVE_ALPHA, 0, 0, 0, 0, 1,
              PRIMITIVE, 0, PRIMITIVE_ALPHA, 0, 1, ENVIRONMENT, SHADE,
              ENVIRONMENT);
        }

        gDPSetPrimColor(gfx++, 0, 0, 0xE0, 0xE0, 0x00, alpha);
        gDPSetEnvColor(gfx++, 0xE0, 0xE0, 0x00, 0x40);
        gDPPipeSync(gfx++);
        gSPVertex(gfx++, f_HoleTriangles[index].vertices,
            HOLE_TRIANGLE_VERTEX_COUNT, 0);
        gSP1Triangle(gfx++, 0, 1, 2, 0);
        gDPPipeSync(gfx++);
        break;
    }
  }

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  f_DimsTranslucentTriangles = FALSE;
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_triangles", DrawHoleTriangles);
#endif

/* Draws the current player's rings; one past RING_STATE_SHOWN fades out while
 * sparkles circle it */
#ifdef NON_MATCHING
void DrawRings(
    Gfx** displayListHead, CameraMatrices* matrices, Camera* camera) {
  Gfx* gfx = *displayListHead;
  f32 transform[4][4];
  f32 ringPoint[3];
  f32 worldPoint[3];
  s32 position[3];
  s32 triangleIndex;
  s32 red;
  s32 green;
  s32 blue;
  Mtx* matrix;
  s32 ringIndex;
  s32 state;
  s32 section;
  s32 side;
  s32 nextSection;
  s32 alpha;
  f32 angle;
  f32 turn;

  gDPPipeSync(gfx++);
  gSPSetLights1(gfx++, f_RingLights);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gSPClipRatio(gfx++, FRUSTRATIO_1);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_MAGICSQ);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++,
      G_ZBUFFER | G_SHADE | G_CULL_BACK | G_FOG | G_LIGHTING | G_TEXTURE_GEN |
          G_SHADING_SMOOTH);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0xFE);
  gDPPipeSync(gfx++);
  gDPSetTextureLOD(gfx++, G_TL_TILE);
  gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0x3F);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPLoadTextureBlock(gfx++, (u32)g_MirrorTexture & ~7, G_IM_FMT_RGBA,
      G_IM_SIZ_16b, DYNAMIC_TEXTURE_SIZE, DYNAMIC_TEXTURE_SIZE, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD,
      G_TX_NOLOD);
  gSPTexture(gfx++, 0x07C0, 0x07C0, 0, G_TX_RENDERTILE, G_ON);
  if (g_PauseScreen == PAUSE_SCREEN_UNK4) {
    gDPSetCombineLERP(gfx++, PRIMITIVE, TEXEL0, ENV_ALPHA, TEXEL0, 0, 0, 0,
        PRIMITIVE, PRIMITIVE, TEXEL0, ENV_ALPHA, TEXEL0, 0, 0, 0, PRIMITIVE);
    red = RING_MODE_FOUR_RED;
    green = RING_MODE_FOUR_GREEN;
    blue = RING_MODE_FOUR_BLUE;
  } else {
    gDPSetCombineLERP(gfx++, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, PRIMITIVE,
        PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, PRIMITIVE);
    red = RING_RED;
    green = RING_GREEN;
    blue = RING_BLUE;
  }

  gDPSetEnvColor(gfx++, 0x78, 0x78, 0x20, 0xBF);
  matrix = f_RingMatrices;
  f_RingAngle += RING_SPIN_SPEED;
  guRotateRPY(&f_RingSpinMatrices[g_DrawBufferIndex], 0.0f, 0.0f, f_RingAngle);
  ringIndex = 0;
  for (triangleIndex = 0; triangleIndex < HOLE_TRIANGLE_COUNT;
      triangleIndex++) {
    if (ringIndex >= RING_COUNT) {
      break;
    }

    if (f_HoleTriangles[triangleIndex].kind == HOLE_TRIANGLE_KIND_RING) {
      state = g_Players[g_GameState.currentPlayerIndex].ringStates[ringIndex];
      if (state > RING_STATE_NONE) {
        if (state != RING_STATE_SHOWN) {
          if (state < RING_STATE_FADED) {
            section = state & (RING_SPARKLE_SPACING - 1);
            turn = state * RING_SPARKLE_TURN;
            guMtxL2F(transform, matrix);
            for (; section < RING_SECTION_COUNT;
                section += RING_SPARKLE_SPACING) {
              angle = turn + (section * RING_SECTION_ANGLE);
              ringPoint[0] = cosf(angle) * RING_RADIUS;
              ringPoint[1] = sinf(angle) * RING_RADIUS;
              ringPoint[2] = 0;
              guMtxXFMF(transform, ringPoint[0], ringPoint[1], ringPoint[2],
                  &worldPoint[0], &worldPoint[1], &worldPoint[2]);
              position[0] = worldPoint[0] * POSITION_SCALE;
              position[1] = worldPoint[1] * POSITION_SCALE;
              position[2] = worldPoint[2] * POSITION_SCALE;
              SpawnSparkle(position);
            }
          }

          state++;
          if (state > RING_STATE_FADED) {
            state = RING_STATE_FADED;
          }

          alpha = 0xFF - (state * RING_FADE_STEP);
          if (alpha < 0) {
            alpha = 0;
          }

          g_Players[g_GameState.currentPlayerIndex].ringStates[ringIndex] =
              state;
          gDPPipeSync(gfx++);
          gDPSetRenderMode(gfx++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
        } else {
          alpha = 0xFF;
          gDPPipeSync(gfx++);
          gDPSetRenderMode(gfx++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
        }

        gDPSetPrimColor(gfx++, 0, 0, red, green, blue, alpha);
        gSPMatrix(gfx++, matrix, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
        gSPMatrix(gfx++, &f_RingSpinMatrices[g_DrawBufferIndex],
            G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
        gSPVertex(gfx++, f_RingVertices, RING_VERTEX_COUNT, 0);
        for (section = 0; section < RING_SECTION_COUNT; section++) {
          nextSection = (section + 1) % RING_SECTION_COUNT;
          for (side = 0; side < RING_SIDE_COUNT; side++) {
            gSP2Triangles(gfx++, section * RING_SIDE_COUNT + side,
                section * RING_SIDE_COUNT + (side + 1) % RING_SIDE_COUNT,
                nextSection * RING_SIDE_COUNT + (side + 1) % RING_SIDE_COUNT, 0,
                section * RING_SIDE_COUNT + side,
                nextSection * RING_SIDE_COUNT + (side + 1) % RING_SIDE_COUNT,
                nextSection * RING_SIDE_COUNT + side, 0);
          }
        }

        gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
      }

      matrix++;
      ringIndex++;
    }
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_triangles", DrawRings);
#endif

/* Builds the ring's vertices, with the first side of each section inside */
static void BuildRingVertices(void) {
  s32 index;
  Vtx* vertex;
  s32 isOuter;
  s32 isRaised;
  f32 angle;
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
    offsetZ = sinf(angle) * (isOuter - 0.5f) * RING_THICKNESS;
    x = (cosf(angle) * RING_RADIUS) + offsetX;
    y = (sinf(angle) * RING_RADIUS) + offsetZ;
    offsetY = (isRaised - 0.5f) * RING_THICKNESS;
    vertex->n.ob[0] = x;
    vertex->n.ob[1] = y;
    vertex->n.ob[2] = offsetY;
    guNormalize(&offsetX, &offsetY, &offsetZ);
    vertex->n.n[0] = offsetX * RING_NORMAL_SCALE;
    vertex->n.n[1] = offsetZ * RING_NORMAL_SCALE;
    vertex->n.n[2] = offsetY * RING_NORMAL_SCALE;
    vertex->n.a = RING_VERTEX_ALPHA;
  }
}

/* Puts a ring at a fixed place in the first triangle */
static void AddTestRing(void) {
  f_HoleTriangles[0].kind = HOLE_TRIANGLE_KIND_RING;
  f_HoleTriangles[0].vertices[0].v.ob[0] = 0x800;
  f_HoleTriangles[0].vertices[0].v.ob[1] = -0x100;
  f_HoleTriangles[0].vertices[0].v.ob[2] = 0x1000;
  f_HoleTriangles[0].vertices[1].v.ob[0] = 0x800;
  f_HoleTriangles[0].vertices[1].v.ob[1] = -0x100;
  f_HoleTriangles[0].vertices[1].v.ob[2] = 0xF00;
  f_HoleTriangles[0].vertices[2].v.ob[0] = 0x700;
  f_HoleTriangles[0].vertices[2].v.ob[1] = -0x100;
  f_HoleTriangles[0].vertices[2].v.ob[2] = 0xF00;
  InitRings();
}

/*
 * Builds a matrix for each ring triangle, which places the ring at its
 * first corner, turns it to face along its second and scales it by its
 * third's x. Returns the number of rings.
 */
static s32 BuildRingMatrices(void) {
  f32 rotation[4][4];
  f32 transform[4][4];
  RingCorners corners;
  f32 up[3];
  f32 scale;
  s32 ringCount = 0;
  s32 index;
  HoleTriangle* triangle;

  for (index = 0; index < HOLE_TRIANGLE_COUNT; index++) {
    triangle = &f_HoleTriangles[index];
    if (triangle->kind != HOLE_TRIANGLE_KIND_RING) {
      continue;
    }

    corners.coordinates[0][0] = triangle->vertices[0].v.ob[0];
    corners.coordinates[0][1] = triangle->vertices[1].v.ob[0];
    corners.coordinates[0][2] = triangle->vertices[2].v.ob[0];
    corners.coordinates[1][0] = triangle->vertices[0].v.ob[1];
    corners.coordinates[1][1] = triangle->vertices[1].v.ob[1];
    corners.coordinates[1][2] = triangle->vertices[2].v.ob[1];
    corners.coordinates[2][0] = triangle->vertices[0].v.ob[2];
    corners.coordinates[2][1] = triangle->vertices[1].v.ob[2];
    corners.coordinates[2][2] = triangle->vertices[2].v.ob[2];
    if (corners.coordinates[0][1] | corners.coordinates[1][1] |
        corners.coordinates[2][1]) {
      triangle->center[0] =
          (corners.coordinates[0][0] + corners.coordinates[0][1] +
              corners.coordinates[0][2]) /
          3;
      triangle->center[1] =
          (corners.coordinates[1][0] + corners.coordinates[1][1] +
              corners.coordinates[1][2]) /
          3;
      triangle->center[2] =
          (corners.coordinates[2][0] + corners.coordinates[2][1] +
              corners.coordinates[2][2]) /
          3;
      /* Never read; the stores match */
      up[0] = up[2] = 0.0f;
      up[1] = -1.0f;
      guAlignF(rotation, 0.0f, corners.coordinates[0][1],
          corners.coordinates[1][1], corners.coordinates[2][1]);
      scale = corners.coordinates[0][2] / RING_SCALE_ONE;
      guScaleF(transform, scale, scale, scale);
      guMtxCatF(transform, rotation, rotation);
      guTranslateF(transform, corners.coordinates[0][0],
          corners.coordinates[1][0], corners.coordinates[2][0]);
      guMtxCatF(rotation, transform, rotation);
    } else {
      guMtxIdentF(rotation);
    }

    guMtxF2L(rotation, &f_RingMatrices[ringCount]);
    ringCount++;
  }

  return ringCount;
}

/* Builds the rings and shows as many of each player's as the hole has */
void InitRings(void) {
  s32 ringCount;
  s32 playerIndex;
  s32 ringIndex;

  BuildRingVertices();
  ringCount = BuildRingMatrices();
  if (ringCount >= RING_COUNT) {
    ringCount = RING_COUNT;
  }

  if (g_GameState.unk30 != KEEP_RING_STATES) {
    for (playerIndex = 0; playerIndex < MAX_PLAYER_COUNT; playerIndex++) {
      for (ringIndex = 0; ringIndex < RING_COUNT; ringIndex++) {
        g_Players[playerIndex].ringStates[ringIndex] = ringIndex < ringCount;
      }
    }
  }
}
