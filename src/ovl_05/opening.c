#include "common.h"

#include "asset.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "camera.h"
#include "camera_editor.h"
#include "character_slot.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "effects.h"
#include "functions.h"
#include "heap.h"
#include "hole_display.h"
#include "libc.h"
#include "math_util.h"
#include "round_hud.h"
#include "scene.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "variables.h"

/* The view that the opening draws its characters through */
#define OPENING_VIEW_INDEX 2

/* The size of the heap that the opening's billboards are allocated from */
#define OPENING_BILLBOARD_HEAP_SIZE 0x7038

#define ASSET_SHADOW_TEXTURE 0x4C3
/* The shadow texture is 32x32 I8 */
#define SHADOW_TEXTURE_SIZE 32

/* The face-printed golf balls that the opening shows, two at a time */
#define OPENING_BALL_IMAGE_COUNT 5
#define OPENING_BALL_SLOT_COUNT 2
/* A 64x64 CI8 image with its header and palette */
#define OPENING_BALL_IMAGE_SIZE 0x1208
#define OPENING_BALL_IMAGE_WIDTH 64
/* A ball image is drawn in strips that fit in texture memory */
#define OPENING_BALL_STRIP_HEIGHT 32
#define OPENING_BALL_STRIP_COUNT 2
#define OPENING_BALL_STRIP_SIZE \
  (OPENING_BALL_IMAGE_WIDTH * OPENING_BALL_STRIP_HEIGHT)
/* The scale that a slot starts at */
#define OPENING_BALL_NORMAL_SCALE 1.0f
/* A ball at this scale sits a little higher, with its shadow lower */
#define OPENING_BALL_LARGE_SCALE 3.0f

/* What an opening ball slot shows */
typedef enum OpeningBallState {
  OPENING_BALL_HIDDEN,
  OPENING_BALL_ON_GROUND,
  OPENING_BALL_IN_AIR /* At its own height, rather than the ground's */
} OpeningBallState;

/* The character name logos that the opening shows, two at a time */
#define OPENING_NAME_LOGO_COUNT 7
#define OPENING_NAME_SLOT_COUNT 2
/* A 128x64 CI8 image with its header and palette */
#define OPENING_NAME_LOGO_SIZE 0x2208

typedef struct OpeningNameLogo {
  /* 0x00 */ s32 assetId;
  /* 0x04 */ s32 width;
  /* 0x08 */ s32 height;
  /* 0x0C */ s32 x;
  /* 0x10 */ s32 y;
  /* 0x14 */ s32 shownWidth; /* As the logo unfolds and flips */
  /* 0x18 */ s32 shownHeight;
} OpeningNameLogo; /* size = 0x1C */

extern s32 D_8020AC4C_ovl_05;
extern s32 f_LogoClubPosition[3];
extern f32 f_LogoClubOrbitRadius;
extern s32 f_IsLogoClubSwinging;
extern s32 f_AreAmbientSoundsOn;
extern u8 D_80212830_ovl_05[];
extern u8 f_ShotBallImage[];
/* The steps that a name logo's animation starts each phase at */
#define NAME_LOGO_UNFOLD 0     /* Widens over 8 frames */
#define NAME_LOGO_FLIP_DOWN 10 /* Flattens over 4 frames */
#define NAME_LOGO_FLIP_UP 20   /* Grows back over 4 frames */
#define NAME_LOGO_STILL 30

extern OpeningNameLogo f_OpeningNameLogos[OPENING_NAME_LOGO_COUNT];
extern u8 f_NameSlotImages[OPENING_NAME_SLOT_COUNT][OPENING_NAME_LOGO_SIZE];
extern s32 f_NameSlotLogoIndices[OPENING_NAME_SLOT_COUNT];
extern s32 f_NameSlotSteps[OPENING_NAME_SLOT_COUNT];
extern s32 f_IsNameSlotShown[OPENING_NAME_SLOT_COUNT];
extern s32 f_OpeningBallStates[OPENING_BALL_SLOT_COUNT];
extern f32 f_OpeningBallScales[OPENING_BALL_SLOT_COUNT];
extern s32 f_OpeningBallAssets[OPENING_BALL_IMAGE_COUNT];
extern u8 f_OpeningBallImages[OPENING_BALL_SLOT_COUNT][OPENING_BALL_IMAGE_SIZE];
/* The buffers of character slots 1 and 2 */
extern u8* f_OpeningCharacterBuffers[2];
extern void* f_OpeningBillboardHeap;
/*
 * The cue that the opening's script has given for setting up its characters,
 * from 0 to OPENING_CUE_COUNT - 1, or one of the values below
 */
extern s32 f_OpeningCue;
#define OPENING_CUE_COUNT 29
#define OPENING_CUE_INVALID 29
#define OPENING_CUES_FINISHED 30
#define OPENING_CUE_DONE 31
extern u8* g_ShadowTexture;
extern s32 f_LakituScreenPoint[2];
/* A 32x32 CI8 image with its header and palette */
extern u8 f_LakituImage[];
/* A 48x128 I4 image with its header */
extern u8 f_LakituTrailImage[];
#define LAKITU_TRAIL_SEGMENT_COUNT 40
extern Mtx f_LakituTrailMatrices[LAKITU_TRAIL_SEGMENT_COUNT];
extern f32 f_LakituTrailStart[2];

extern s32 f_OpeningFrameParity;
extern s32 f_IsLookAtMarkerShown;
extern f32 g_LookAtMarkerScale;
extern Vtx f_LookAtMarkerVertices[6][4];
extern Mtx f_LookAtMarkerTranslation;
extern Mtx f_LookAtMarkerScaling;

extern s32 f_IsLakituFlying;
extern s32 f_LakituAngle;
extern s32 f_LookAtEaseMode;
extern f32 f_LookAtEaseRate;

/* A 48x432 CI8 image of the letters, one above the next */
#define ASSET_LOGO_LETTER_IMAGES 0x945
/* 152x78 CI8 */
#define ASSET_LOGO_CLUB_IMAGE 0x946
/* 24x24 CI8 */
#define ASSET_LOGO_MARIO_IMAGE 0x94B
/* A 64x192 CI8 image of three golf balls */
#define ASSET_LOGO_BALL_IMAGES 0x94E
/* A 64x64 I8 gradient that sweeps across the logo */
#define ASSET_LOGO_SHINE_IMAGE 0x6A4

/* "MARIO GOLF" */
#define LOGO_LETTER_COUNT 9
/* The letters of "MARIO GOLF", left to right */
typedef enum LogoLetterIndex {
  LOGO_LETTER_M,
  LOGO_LETTER_A,
  LOGO_LETTER_R,
  LOGO_LETTER_I,
  LOGO_LETTER_FIRST_O,
  LOGO_LETTER_G,
  LOGO_LETTER_SECOND_O,
  LOGO_LETTER_L,
  LOGO_LETTER_F
} LogoLetterIndex;
/* Where a letter starts, off the screen */
#define LOGO_LETTER_HIDDEN (-1000)

/* Where Lakitu drops each letter, on the screen */
#define LOGO_M_DROP_X 57
#define LOGO_A_DROP_X 81
#define LOGO_R_DROP_X 112
#define LOGO_I_DROP_X 132
#define LOGO_FIRST_O_DROP_X 154
#define LOGO_G_DROP_X 193
#define LOGO_SECOND_O_DROP_X 220
#define LOGO_L_DROP_X 242
#define LOGO_F_DROP_X 265
/* Where each letter ends up once the logo shrinks, on the screen */
#define LOGO_M_SHRUNK_X 86
#define LOGO_A_SHRUNK_X 104
#define LOGO_R_SHRUNK_X 125
#define LOGO_I_SHRUNK_X 139
#define LOGO_FIRST_O_SHRUNK_X 154
#define LOGO_G_SHRUNK_X 184
#define LOGO_SECOND_O_SHRUNK_X 204
#define LOGO_L_SHRUNK_X 220
#define LOGO_F_SHRUNK_X 234
/* Each letter's depth, which decides which letters overlap which */
#define LOGO_M_DEPTH 8
#define LOGO_A_DEPTH 12
#define LOGO_R_DEPTH 16
#define LOGO_I_DEPTH 20
#define LOGO_FIRST_O_DEPTH 24
#define LOGO_G_DEPTH 28
#define LOGO_SECOND_O_DEPTH 36
#define LOGO_L_DEPTH 40
#define LOGO_F_DEPTH 44

typedef struct LogoLetter {
  s32 x;      /* On the screen, from the left */
  s32 y;      /* On the screen, from the top */
  s32 depth;  /* Nearer letters are drawn over farther ones */
} LogoLetter; /* size = 0xC */

/* What a logo letter is doing */
typedef enum LogoLetterState {
  LOGO_LETTER_WAITING,    /* For Lakitu to drop it */
  LOGO_LETTER_FALLING,    /* Or rising after a bounce, and growing */
  LOGO_LETTER_SQUASHING,  /* Flattening against the ground */
  LOGO_LETTER_SQUASHED,   /* Holding flat */
  LOGO_LETTER_STRETCHING, /* Springing back up */
  LOGO_LETTER_LANDED = 9
} LogoLetterState;

extern u8 f_LogoLetterImages[];
extern u8 f_LogoClubImage[];
extern u8 f_LogoMarioImage[];
extern u8 f_LogoBallImages[];
extern u8 f_LogoShineImage[];
extern LogoLetter f_LogoLetters[LOGO_LETTER_COUNT];
extern s32 f_LogoLetterStates[LOGO_LETTER_COUNT];
extern f32 f_LogoLetterSpeeds[LOGO_LETTER_COUNT]; /* Down, in pixels a frame */
extern f32 f_LogoLetterScales[LOGO_LETTER_COUNT][2]; /* Across, then up */
/* Frames left in a letter's squashing, squashed or stretching state */
extern s32 f_LogoLetterSquashFrames[LOGO_LETTER_COUNT];

extern s32 f_OpeningShotDashCount;
extern BallSimulation f_OpeningShotSimulation;
/* f_OpeningShotDashCount's starting value, and the most it can be */
#define OPENING_SHOT_MAX_DASH_COUNT 200
extern Mtx f_OpeningShotDashMatrices[DRAW_BUFFER_COUNT]
                                    [OPENING_SHOT_MAX_DASH_COUNT];

/*
 * The opening's shot path is drawn as the turn's shot preview is, but
 * nothing in the opening sets f_IsOpeningShotPathShown, so it never shows
 */
extern s32 f_IsOpeningShotPathShown;
extern s32 f_OpeningShotPathDashCount; /* Starts at 198 */
extern s8 f_IsOpeningShotPathRed;
extern s32 f_OpeningShotPathScroll; /* Of the dash image, in texels */
extern Vtx f_OpeningShotDashVertices[4];

extern s32 f_IsBurstShown;
extern s32 f_BurstFrame; /* Starts at -1, before the burst's first frame */

void AssignCharacterBuffers(void);
void DrawOpeningShotPath(CameraMatrices* matrices);
void DrawNameLogos(Gfx** displayListHead);
void DrawOpeningBalls(Gfx** displayListHead, CameraMatrices* matrices);
void DrawOpeningImpactLines(Gfx** displayListHead);
void DrawLakitu(Gfx** displayListHead);
void DrawOpeningBurst(Gfx** displayListHead);
void DrawOpeningShotBall(Gfx** displayListHead);
void DrawOpeningLogo(Gfx** displayListHead, s32 isDrawn);
void DrawOpeningSparks(Gfx** displayListHead);

void func_801F4A40_ovl_05(void) {
  D_8020AC4C_ovl_05++;
  if (D_8020AC4C_ovl_05 == 11) {
    D_8020AC4C_ovl_05 = 0;
  }
}

void func_801F4A6C_ovl_05(void) {
  if (f_IsLogoClubSwinging != 0) {
    PlaySoundWithEffects(0x51, 12, 48 - (s32)(f_LogoClubOrbitRadius * 0.2f), 0,
                         127 - (f_LogoClubPosition[0] - 160) / 2);
  }
}

/* The opening's ambient sounds, each with a channel and pan of its own */
#define SOUND_AMBIENT_ALTERNATING_FIRST 0x184
#define SOUND_AMBIENT_ALTERNATING_SECOND 0x185
#define SOUND_AMBIENT_REPEATING 0x18B
#define SOUND_AMBIENT_RANDOM_FIRST 0x1A3
#define SOUND_AMBIENT_RANDOM_SECOND 0x1BA
#define ALTERNATING_SOUND_CHANNEL 13
#define REPEATING_SOUND_CHANNEL 14
#define RANDOM_SOUND_CHANNEL 15
#define ALTERNATING_SOUND_PAN 0x9D
#define REPEATING_SOUND_PAN 0x75
#define RANDOM_SOUND_PAN 0x61
#define AMBIENT_SOUND_FULL_VOLUME 127
/* How much quieter the sounds are for each unit of camera distance */
#define AMBIENT_SOUND_FADE_RATE 0.5f
/* Out of the values that rand() & AMBIENT_SOUND_CHANCE_MASK can give */
#define AMBIENT_SOUND_CHANCE_MASK 100
#define AMBIENT_SOUND_CHANCE 80
/* The random sound picks from four choices, two of which are silence */
#define RANDOM_SOUND_CHOICE_MASK 3

/* Which of a pair of sounds is playing, or none */
typedef enum AmbientSoundChoice {
  AMBIENT_SOUND_NONE,
  AMBIENT_SOUND_FIRST,
  AMBIENT_SOUND_SECOND
} AmbientSoundChoice;

extern s32 f_AlternatingSoundTimer;
extern s32 f_RepeatingSoundTimer;
extern s32 f_RandomSoundTimer;
extern s32 f_AlternatingSound;
extern s32 f_RandomSound;
extern s32 f_AlternatingSoundFirstLength;
extern s32 f_AlternatingSoundSecondLength;
extern s32 f_RepeatingSoundLength;
extern s32 f_RandomSoundFirstLength;
extern s32 f_RandomSoundSecondLength;

/*
 * Plays the opening's ambient sounds while f_AreAmbientSoundsOn is set,
 * more quietly the further the camera is from its look-at point.
 *
 * Each kind of sound has a timer. Once its timer runs out, and while the
 * camera is near enough to hear it, the sound may start again; the timer
 * then counts down for the length of the sound that started.
 */
void PlayOpeningAmbientSounds(void) {
  if (f_AreAmbientSoundsOn == 1) {
    if ((u32)(g_CameraEditor.distance * AMBIENT_SOUND_FADE_RATE) <
        AMBIENT_SOUND_FULL_VOLUME) {
      if (f_AlternatingSoundTimer == 0 &&
          (rand() & AMBIENT_SOUND_CHANCE_MASK) < AMBIENT_SOUND_CHANCE) {
        if (f_AlternatingSound == AMBIENT_SOUND_SECOND) {
          f_AlternatingSound = AMBIENT_SOUND_FIRST;
          f_AlternatingSoundTimer = f_AlternatingSoundFirstLength;
        } else {
          f_AlternatingSound = AMBIENT_SOUND_SECOND;
          f_AlternatingSoundTimer = f_AlternatingSoundSecondLength;
        }
      }
      if (f_RepeatingSoundTimer == 0 &&
          (rand() & AMBIENT_SOUND_CHANCE_MASK) < AMBIENT_SOUND_CHANCE) {
        f_RepeatingSoundTimer = f_RepeatingSoundLength;
      }
      if (f_RandomSoundTimer == 0 &&
          (rand() & AMBIENT_SOUND_CHANCE_MASK) < AMBIENT_SOUND_CHANCE) {
        switch (rand() & RANDOM_SOUND_CHOICE_MASK) {
          case 0:
            f_RandomSound = AMBIENT_SOUND_FIRST;
            f_RandomSoundTimer = f_RandomSoundFirstLength;
            break;
          case 1:
            f_RandomSound = AMBIENT_SOUND_SECOND;
            f_RandomSoundTimer = f_RandomSoundSecondLength;
            break;
        }
      }
    }
    if (f_AlternatingSoundTimer != 0) {
      if (f_AlternatingSound == AMBIENT_SOUND_FIRST &&
          f_AlternatingSoundTimer == f_AlternatingSoundFirstLength) {
        PlaySoundWithEffects(
            SOUND_AMBIENT_ALTERNATING_FIRST, ALTERNATING_SOUND_CHANNEL,
            AMBIENT_SOUND_FULL_VOLUME -
                (u32)(g_CameraEditor.distance * AMBIENT_SOUND_FADE_RATE),
            0, ALTERNATING_SOUND_PAN);
      }
      if (f_AlternatingSound == AMBIENT_SOUND_SECOND &&
          f_AlternatingSoundTimer == f_AlternatingSoundSecondLength) {
        PlaySoundWithEffects(
            SOUND_AMBIENT_ALTERNATING_SECOND, ALTERNATING_SOUND_CHANNEL,
            AMBIENT_SOUND_FULL_VOLUME -
                (u32)(g_CameraEditor.distance * AMBIENT_SOUND_FADE_RATE),
            0, ALTERNATING_SOUND_PAN);
      }
      f_AlternatingSoundTimer--;
    }
    if (f_RepeatingSoundTimer != 0) {
      if (f_RepeatingSoundTimer == f_RepeatingSoundLength) {
        PlaySoundWithEffects(
            SOUND_AMBIENT_REPEATING, REPEATING_SOUND_CHANNEL,
            AMBIENT_SOUND_FULL_VOLUME -
                (u32)(g_CameraEditor.distance * AMBIENT_SOUND_FADE_RATE),
            0, REPEATING_SOUND_PAN);
      }
      f_RepeatingSoundTimer--;
    }
    if (f_RandomSoundTimer != 0) {
      if (f_RandomSound == AMBIENT_SOUND_FIRST &&
          f_RandomSoundTimer == f_RandomSoundFirstLength) {
        PlaySoundWithEffects(
            SOUND_AMBIENT_RANDOM_FIRST, RANDOM_SOUND_CHANNEL,
            AMBIENT_SOUND_FULL_VOLUME -
                (u32)(g_CameraEditor.distance * AMBIENT_SOUND_FADE_RATE),
            0, RANDOM_SOUND_PAN);
      }
      if (f_RandomSound == AMBIENT_SOUND_SECOND &&
          f_RandomSoundTimer == f_RandomSoundSecondLength) {
        PlaySoundWithEffects(
            SOUND_AMBIENT_RANDOM_SECOND, RANDOM_SOUND_CHANNEL,
            AMBIENT_SOUND_FULL_VOLUME -
                (u32)(g_CameraEditor.distance * AMBIENT_SOUND_FADE_RATE),
            0, RANDOM_SOUND_PAN);
      }
      if (--f_RandomSoundTimer == 0) {
        f_RandomSound = AMBIENT_SOUND_NONE;
      }
    }
  }
}

void LoadOpening(void) {
  EncodedAsset asset;
  s32 bufferIndex;

  osSyncPrintf("ConstOpening\n");
  f_OpeningBillboardHeap = AllocMemory(OPENING_BILLBOARD_HEAP_SIZE);
  InitBillboardManager(f_OpeningBillboardHeap, OPENING_BILLBOARD_HEAP_SIZE);
  f_OpeningCue = OPENING_CUE_DONE;
  for (bufferIndex = 0; bufferIndex < ARRAY_COUNT(f_OpeningCharacterBuffers);
       bufferIndex++) {
    f_OpeningCharacterBuffers[bufferIndex] =
        AllocMemory(GetCharacterBufferSize());
    SetCharacterBuffer(bufferIndex + 1, f_OpeningCharacterBuffers[bufferIndex]);
  }
  SetCharacterBuffer(3, (u8*)g_DepthBuffer);
  g_ShadowTexture = AllocMemory(OpenEncodedAsset(ASSET_SHADOW_TEXTURE, &asset));
  ReadEncodedAsset(g_ShadowTexture, &asset);
  f_AreAmbientSoundsOn = 0;
  LoadEffectImages();
  LoadOpeningLogo();
}

void FreeOpening(void) {
  s32 bufferIndex;

  FreeMemory(&g_ShadowTexture);
  for (bufferIndex = 0; bufferIndex < ARRAY_COUNT(f_OpeningCharacterBuffers);
       bufferIndex++) {
    FreeMemory(&f_OpeningCharacterBuffers[bufferIndex]);
  }
  AssignCharacterBuffers();
  FreeMemory(&f_OpeningBillboardHeap);
  SetFillColor(255, 255, 255);
  FreeEffectImages();
}

/* The caller passes the frame parity too, which goes unused */
void DrawOpeningCharacters(CameraMatrices* matrices, Gfx** displayListHead,
                           s32 frameParity) {
  s32 slotIndex = 0;

  g_DisplayListHead = *displayListHead;
  do {
    DrawCharacterAndShadow(matrices, OPENING_VIEW_INDEX, slotIndex);
    slotIndex++;
  } while (slotIndex < CHARACTER_SLOT_COUNT);
  *displayListHead = g_DisplayListHead;
}

/*
 * Steps the opening's shot through its simulation, as BuildShotPreview does,
 * and stretches a dash between each pair of positions along its path
 */
void BuildOpeningShotPath(void) {
  CoursePoint start;
  Mat4 dash;
  Mat4 transform;
  s32 step;
  s32 length;

  for (step = 0; step < f_OpeningShotDashCount; step++) {
    start[AXIS_X] = f_OpeningShotSimulation.position[AXIS_X];
    start[AXIS_Y] = f_OpeningShotSimulation.position[AXIS_Y];
    start[AXIS_Z] = f_OpeningShotSimulation.position[AXIS_Z];
    StepBallSimulation(&f_OpeningShotSimulation);
    length =
        IntLength3D(f_OpeningShotSimulation.position[AXIS_X] - start[AXIS_X],
                    f_OpeningShotSimulation.position[AXIS_Y] - start[AXIS_Y],
                    f_OpeningShotSimulation.position[AXIS_Z] - start[AXIS_Z]);
    guTranslateF(dash, start[AXIS_X] * COURSE_TO_WORLD_SCALE,
                 (start[AXIS_Y] - SHOT_PREVIEW_DROP) * COURSE_TO_WORLD_SCALE,
                 start[AXIS_Z] * COURSE_TO_WORLD_SCALE);
    if (length != 0 && length <= SHOT_PREVIEW_MAX_DASH_LENGTH) {
      guScaleF(dash, 1.0f, 1.0f, length * COURSE_TO_WORLD_SCALE);
      guAlignF(transform, 0.0f,
               f_OpeningShotSimulation.position[AXIS_X] - start[AXIS_X],
               f_OpeningShotSimulation.position[AXIS_Y] - start[AXIS_Y],
               f_OpeningShotSimulation.position[AXIS_Z] - start[AXIS_Z]);
      guMtxCatF(dash, transform, dash);
      guTranslateF(transform, start[AXIS_X] * COURSE_TO_WORLD_SCALE,
                   start[AXIS_Y] * COURSE_TO_WORLD_SCALE,
                   start[AXIS_Z] * COURSE_TO_WORLD_SCALE);
      guMtxCatF(dash, transform, dash);
    }
    guMtxF2L(dash, &f_OpeningShotDashMatrices[g_DrawBufferIndex][step]);
  }
}

/* Draws the dashes that BuildOpeningShotPath placed along the shot's path */
void DrawOpeningShotPath(CameraMatrices* matrices) {
  s32 step;

  if (!f_IsOpeningShotPathShown) {
    return;
  }
  gDPPipeSync(g_DisplayListHead++);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_ZB_XLU_SURF,
                   G_RM_AA_ZB_XLU_SURF2);
  gSPLoadGeometryMode(g_DisplayListHead++, 0);
  gSPSetGeometryMode(g_DisplayListHead++,
                     G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
  gDPSetCombineLERP(g_DisplayListHead++, PRIMITIVE, ENVIRONMENT, TEXEL0,
                    ENVIRONMENT, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE,
                    ENVIRONMENT, TEXEL0, ENVIRONMENT, PRIMITIVE, 0, TEXEL0, 0);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetAlphaCompare(g_DisplayListHead++, G_AC_THRESHOLD);
  gDPSetBlendColor(g_DisplayListHead++, 0xFF, 0xFF, 0xFF, 0x40);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
  gSPTexture(g_DisplayListHead++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPLoadTextureBlock(
      g_DisplayListHead++,
      TEXTURE_ADDRESS((u8*)g_ShotPreviewDashImage + IMAGE_HEADER_SIZE),
      G_IM_FMT_I, G_IM_SIZ_8b, SHOT_PREVIEW_IMAGE_SIZE, SHOT_PREVIEW_IMAGE_SIZE,
      0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPSetTileSize(g_DisplayListHead++, G_TX_RENDERTILE, f_OpeningShotPathScroll,
                 0,
                 f_OpeningShotPathScroll +
                     (SHOT_PREVIEW_IMAGE_SIZE << G_TEXTURE_IMAGE_FRAC),
                 SHOT_PREVIEW_IMAGE_SIZE << G_TEXTURE_IMAGE_FRAC);
  gDPSetCombineMode(g_DisplayListHead++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
  if (f_IsOpeningShotPathRed) {
    gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0xFF, 0x00, 0x00, 0xC8);
  } else {
    gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0xFF, 0xFF, 0xFF, 0x80);
  }
  gDPPipeSync(g_DisplayListHead++);
  for (step = 0; step < f_OpeningShotPathDashCount; step++) {
    gSPMatrix(g_DisplayListHead++,
              &f_OpeningShotDashMatrices[g_DrawBufferIndex][step],
              G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(g_DisplayListHead++, f_OpeningShotDashVertices, 4, 0);
    gSP2Triangles(g_DisplayListHead++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPPopMatrix(g_DisplayListHead++, G_MTX_MODELVIEW);
  }
  gDPPipeSync(g_DisplayListHead++);
}

/*
 * The opening's own models, as indices into g_OtherCharacterModels. Each is
 * placed on the tee as the name logo of its character is shown.
 */
#define OPENING_MODEL_MARIO 3
#define OPENING_MODEL_YOSHI 9
#define OPENING_MODEL_BABY_MARIO 10
#define OPENING_MODEL_DONKEY_KONG 11
#define OPENING_MODEL_PLUM 12
#define OPENING_MODEL_PEACH 13
#define OPENING_MODEL_LUIGI 14
/* Shown without a name logo */
#define OPENING_MODEL_UNK16 16

s32 func_80206680_ovl_05(u32 slotIndex, s32 character);

/* Written but never read */
extern s32 f_LastPlacedSlotIndex;
/* Whether Donkey Kong stands in for Yoshi, once he is unlocked */
extern s32 f_IsDonkeyKongShown;
extern s32 f_CueSlotIndex;

/* Places the cue slot's character, leaving the point's height unset */
#define PLACE_CUE_CHARACTER(point, x, z, heading) \
  point[0] = x;                                   \
  point[2] = z;                                   \
  PlaceCharacter(f_CueSlotIndex, point, DEGREES_TO_RADIANS(heading))

/*
 * Sets up the opening's characters for the cue that its script has given, and
 * returns whether the cues have finished.
 *
 * Cues 1 and 27 step on to the next cue instead of being marked done, which
 * sets up its characters on the next frame. When cue 1 places its character,
 * though, it steps from OPENING_CUE_DONE to an invalid cue.
 */
s32 UpdateOpeningCharacters(void) {
  CoursePoint point0;
  CoursePoint point1;
  CoursePoint point2;
  CoursePoint point3;
  CoursePoint point4;
  CoursePoint point5;
  CoursePoint point6;
  CoursePoint point7;
  CoursePoint point8;
  CoursePoint point9;
  CoursePoint point10;
  CoursePoint point11;
  CoursePoint point12;
  CoursePoint point13;
  CoursePoint point14;
  CoursePoint point15;
  CoursePoint point16;
  CoursePoint point17;
  CoursePoint point18;
  CoursePoint point19;
  CoursePoint point20;
  CoursePoint point21;
  CoursePoint point22;
  CoursePoint point23;
  CoursePoint point24;
  CoursePoint point25;
  CoursePoint point26;
  CoursePoint point27;
  CoursePoint point28;
  CoursePoint point29;

  switch (f_OpeningCue) {
    case 0:
      SetCharacterLights();
      f_CueSlotIndex = 0;
      if (func_80206680_ovl_05(0, OPENING_MODEL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x10);
        PLACE_CUE_CHARACTER(point0, 0xF3400, 0x490000, -30);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 1:
      f_CueSlotIndex = 1;
      if (func_80206680_ovl_05(1, OPENING_MODEL_PEACH)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x10);
        PLACE_CUE_CHARACTER(point1, 0xA0B58, 0x4A28BF, 80);
        f_OpeningCue = OPENING_CUE_DONE;
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue++;
      break;
    case 2:
      f_CueSlotIndex = 2;
      if (func_80206680_ovl_05(2, OPENING_MODEL_LUIGI)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x10);
        PLACE_CUE_CHARACTER(point2, 0x1368A0, 0x49D208, 0);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 4:
      FreeCharacterSlot(0);
      FreeCharacterSlot(1);
      FreeCharacterSlot(2);
      f_CueSlotIndex = 3;
      if (func_80206680_ovl_05(3, OPENING_MODEL_PEACH)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x11);
        PLACE_CUE_CHARACTER(point3, 0xF3400, 0x490000, -30);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 5:
      f_CueSlotIndex = 0;
      if (func_80206680_ovl_05(0, OPENING_MODEL_PLUM)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x10);
        PLACE_CUE_CHARACTER(point4, 0xB2A34, 0x4F9800, 10);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 7:
      FreeCharacterSlot(0);
      FreeCharacterSlot(3);
      f_CueSlotIndex = 1;
      if (func_80206680_ovl_05(1, OPENING_MODEL_PLUM)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x11);
        PLACE_CUE_CHARACTER(point5, 0xF3400, 0x490000, 0);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 8:
      f_CueSlotIndex = 2;
      if (func_80206680_ovl_05(2, OPENING_MODEL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x11);
        PLACE_CUE_CHARACTER(point6, 0x1610CF, 0x4AFA34, 0);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 9:
      FreeCharacterSlot(1);
      FreeCharacterSlot(2);
      f_CueSlotIndex = 0;
      if (func_80206680_ovl_05(0, OPENING_MODEL_LUIGI)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x11);
        PLACE_CUE_CHARACTER(point7, 0xF3400, 0x490000, -45);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 11:
      FreeCharacterSlot(0);
      f_CueSlotIndex = 1;
      if (func_80206680_ovl_05(1, OPENING_MODEL_PLUM)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x12);
        PLACE_CUE_CHARACTER(point8, 0x29EB94, 0x26DDDA, 0);
      }
      f_CueSlotIndex = 2;
      if (func_80206680_ovl_05(2, OPENING_MODEL_PEACH)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x12);
        PLACE_CUE_CHARACTER(point9, 0x29EB94, 0x26DDDA, 0);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 12:
      FreeCharacterSlot(1);
      FreeCharacterSlot(2);
      f_CueSlotIndex = 0;
      if (func_80206680_ovl_05(0, OPENING_MODEL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x12);
        PLACE_CUE_CHARACTER(point10, 0x2709F8, 0x1CF4B4, 10);
      }
      f_CueSlotIndex = 3;
      if (func_80206680_ovl_05(3, OPENING_MODEL_LUIGI)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x12);
        PLACE_CUE_CHARACTER(point11, 0x2709F8, 0x1CF4B4, 55);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 13:
      FreeCharacterSlot(0);
      f_CueSlotIndex = 1;
      if (func_80206680_ovl_05(1, OPENING_MODEL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x13);
        PLACE_CUE_CHARACTER(point12, 0x2709F8, 0x1CF4B4, 10);
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 14:
      FreeCharacterSlot(1);
      FreeCharacterSlot(3);
      f_CueSlotIndex = 0;
      if (func_80206680_ovl_05(0, OPENING_MODEL_PEACH)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x13);
        PLACE_CUE_CHARACTER(point13, 0x2CA5D3, 0x22A928, -98);
      }
      f_CueSlotIndex = 2;
      if (func_80206680_ovl_05(2, OPENING_MODEL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x14);
        PLACE_CUE_CHARACTER(point14, 0x2C42D6, 0x22A832, -98);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 15:
      FreeCharacterSlot(0);
      FreeCharacterSlot(2);
      f_CueSlotIndex = 1;
      if (func_80206680_ovl_05(1, OPENING_MODEL_PEACH)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x14);
        PLACE_CUE_CHARACTER(point15, 0x2B1194, 0x249320, -98);
      }
      f_CueSlotIndex = 3;
      if (func_80206680_ovl_05(3, OPENING_MODEL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x15);
        PLACE_CUE_CHARACTER(point16, 0x2B1194, 0x249320, -98);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 17:
      FreeCharacterSlot(1);
      FreeCharacterSlot(3);
      f_CueSlotIndex = 0;
      if (func_80206680_ovl_05(0, OPENING_MODEL_PEACH)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x15);
        PLACE_CUE_CHARACTER(point17, 0x2B1194, 0x249320, -98);
      }
      f_CueSlotIndex = 2;
      if (func_80206680_ovl_05(2, OPENING_MODEL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x16);
        PLACE_CUE_CHARACTER(point18, 0x2B1194, 0x249320, -98);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 18:
      FreeCharacterSlot(2);
      f_CueSlotIndex = 1;
      func_80206680_ovl_05(1, OPENING_MODEL_MARIO);
      SetCharacterAnimation(f_CueSlotIndex, 0x17);
      PLACE_CUE_CHARACTER(point19, 0x2B1194, 0x249320, -98);
      f_OpeningCue = OPENING_CUE_DONE;
      f_LastPlacedSlotIndex = f_CueSlotIndex;
      break;
    case 19:
      SetAnimationSpeed(0, 0.0f);
      SetAnimationSpeed(1, 0.0f);
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 20:
      SetAnimationSpeed(0, 1.0f);
      SetAnimationSpeed(1, 1.0f);
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 21:
      FreeCharacterSlot(0);
      FreeCharacterSlot(1);
      f_CueSlotIndex = 2;
      if (f_IsDonkeyKongShown) {
        func_80206680_ovl_05(2, OPENING_MODEL_DONKEY_KONG);
      } else {
        func_80206680_ovl_05(2, OPENING_MODEL_YOSHI);
      }
      SetAnimationSpeed(2, 0.0f);
      if (f_IsDonkeyKongShown != 0) {
        SetCharacterAnimation(f_CueSlotIndex, 0x11);
      } else {
        SetCharacterAnimation(f_CueSlotIndex, 0x10);
      }
      PLACE_CUE_CHARACTER(point20, 0x202B29, 0xCE7DA, -165);
      f_CueSlotIndex = 3;
      func_80206680_ovl_05(3, OPENING_MODEL_BABY_MARIO);
      SetAnimationSpeed(3, 0.0f);
      if (f_IsDonkeyKongShown != 0) {
        SetCharacterAnimation(f_CueSlotIndex, 0x11);
      } else {
        SetCharacterAnimation(f_CueSlotIndex, 0x10);
      }
      PLACE_CUE_CHARACTER(point21, 0x202B29, 0xCE7DA, -165);
      f_OpeningCue = OPENING_CUE_DONE;
      f_LastPlacedSlotIndex = f_CueSlotIndex;
      break;
    case 22:
      SetAnimationSpeed(2, 1.0f);
      SetAnimationSpeed(3, 1.0f);
      break;
    case 23:
      f_CueSlotIndex = 0;
      func_80206680_ovl_05(0, OPENING_MODEL_PEACH);
      SetCharacterAnimation(f_CueSlotIndex, 0x16);
      PLACE_CUE_CHARACTER(point22, 0x2B1194, 0x249320, -98);
      f_CueSlotIndex = 1;
      func_80206680_ovl_05(1, OPENING_MODEL_MARIO);
      SetCharacterAnimation(f_CueSlotIndex, 0x18);
      PLACE_CUE_CHARACTER(point23, 0x2B1194, 0x249320, -98);
      f_OpeningCue = OPENING_CUE_DONE;
      f_LastPlacedSlotIndex = f_CueSlotIndex;
      break;
    case 24:
      FreeAllCharacterSlots();
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 25:
      f_CueSlotIndex = 0;
      func_80206680_ovl_05(0, OPENING_MODEL_LUIGI);
      SetCharacterAnimation(f_CueSlotIndex, 0x13);
      PLACE_CUE_CHARACTER(point24, 0x16308C, 0x183EDA, 170);
      f_OpeningCue = OPENING_CUE_DONE;
      f_LastPlacedSlotIndex = f_CueSlotIndex;
      break;
    case 26:
      f_CueSlotIndex = 1;
      func_80206680_ovl_05(1, OPENING_MODEL_UNK16);
      SetCharacterAnimation(f_CueSlotIndex, 0x10);
      PLACE_CUE_CHARACTER(point25, 0xE3F2C, 0x1D98E6, 70);
      f_OpeningCue = OPENING_CUE_DONE;
      f_LastPlacedSlotIndex = f_CueSlotIndex;
      break;
    case 27:
      FreeCharacterSlot(0);
      FreeCharacterSlot(1);
      f_CueSlotIndex = 2;
      func_80206680_ovl_05(2, OPENING_MODEL_LUIGI);
      SetCharacterAnimation(f_CueSlotIndex, 0x14);
      PLACE_CUE_CHARACTER(point26, 0x16308C, 0x183EDA, 185);
      f_LastPlacedSlotIndex = f_CueSlotIndex;
      f_CueSlotIndex = 3;
      if (func_80206680_ovl_05(3, OPENING_MODEL_PEACH)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x17);
        PLACE_CUE_CHARACTER(point27, 0x16806D, 0x17E632, 200);
      }
      f_OpeningCue++;
      break;
    case 28:
      f_CueSlotIndex = 0;
      if (func_80206680_ovl_05(0, OPENING_MODEL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x19);
        PLACE_CUE_CHARACTER(point28, 0x15E14D, 0x18155C, 170);
      }
      f_CueSlotIndex = 1;
      if (func_80206680_ovl_05(1, OPENING_MODEL_PLUM)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x13);
        PLACE_CUE_CHARACTER(point29, 0x166EDF, 0x1838C7, 200);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    default:
      f_OpeningCue = OPENING_CUE_INVALID;
      break;
  }
  return f_OpeningCue == OPENING_CUES_FINISHED;
}

/* The opening script's current scene */
extern s32 f_OpeningScene;
/* Ends the opening */
#define OPENING_SCENE_END 999
/* The frames left before the opening's current scene moves on */
extern s32 f_OpeningSceneFramesLeft;
/*
 * Mario shows on the logo ball from this scene on, whenever fewer than this
 * many of a scene's frames are left
 */
#define OPENING_SCENE_LOGO_BALL_MARIO 200
#define LOGO_BALL_MARIO_FRAMES_LEFT 98

/*
 * Whether a slot shows the logo ball, the ball in the title logo. Only this
 * slot's flag is ever set, and setting it also has all balls projected to
 * the screen
 */
extern u8 f_IsLogoBallSlot[OPENING_BALL_SLOT_COUNT];
#define LOGO_BALL_SLOT 1

extern Vec3 f_OpeningBallPositions[OPENING_BALL_SLOT_COUNT];
extern s32 f_AreImpactLinesShown;
extern s32 f_ShotBallScale;
extern s32 D_8020AC48_ovl_05;

/* What the sparks are doing */
typedef enum SparksState {
  SPARKS_HIDDEN,
  SPARKS_STARTING, /* Set elsewhere to start the sparks */
  SPARKS_FLYING
} SparksState;

extern s32 f_OpeningSparksState;
extern Vec3 f_OpeningSparksOrigin;

extern s32 D_8020AC44_ovl_05;
extern Unk20 D_8020ADB8_ovl_05;
extern Unk20 D_8020ADD8_ovl_05;
extern Unk20 D_8020ADF8_ovl_05;
extern Unk20 D_8020AE18_ovl_05;
extern s32 D_8020AE44_ovl_05;
extern f32 D_8020AEBC_ovl_05;
extern f32 D_8020AEC0_ovl_05;
extern CoursePoint D_8020C404_ovl_05;
extern s32 D_8020E3F0_ovl_05;
extern s32 D_8020E3F4_ovl_05;
extern f32 D_80217078_ovl_05;
extern s32 D_800B6808;

void LaunchDrop(CoursePoint point);
void SpawnRisingClumps(CoursePoint point);
s32 IsDonkeyKongUnlocked(void);
void func_80206AA4_ovl_05(BallSimulation* simulation);
void func_8020A488_ovl_05(BallSimulation* simulation, s32 arg1, Unk20* arg2);
void LoadOpeningShotBall(void);
void LoadOpeningBurst(void);
void LoadLakitu(void);
void LoadOpeningImpactLines(void);
void EaseOpeningLookAt(void);
void LoadOpeningBall(s32 imageIndex, s32 slot);

/*
 * Runs the opening's script: steps through its scenes, moving the camera,
 * setting up shots and cues, and playing sounds on given frames. Any button
 * skips to the end, and the debug controllers can stop the script to edit the
 * camera.
 */
void func_801F6788_ovl_05(void) {
  Vec3 point;
  Vec3 logoBallPoint;
  Vec3 handPoint;
  s32 slot;
  s32 channel;
  s32 i;
  s32 pressedButtons;
  s32 skipButtons;

  if (IsGameFlagSet(0x62)) {
    EditOpeningCamera();
    if (g_ControllerStates[2].pressedButtons & 0x4000) {
      PlaySound(0x63, 0xF, 0x7F);
      ClearGameFlag(0x62);
      f_IsLookAtMarkerShown = 0;
      ClearDebugText();
    }
  } else {
    if (D_8020AC44_ovl_05 < 9000) {
      D_8020AC44_ovl_05++;
    } else {
      f_OpeningScene = 999;
    }
    if (g_IsDebugEnabled) {
      sprintf(g_PrintBuffer, "%04d", D_8020AC44_ovl_05);
      PrintDebugText(g_PrintBuffer, 0x1E, 0x1A);
      sprintf(g_PrintBuffer, "CAMERA:%d    ", f_OpeningScene);
      PrintDebugText(g_PrintBuffer, 4, 2);
      sprintf(g_PrintBuffer, "(%d)    ", f_OpeningSceneFramesLeft);
      PrintDebugText(g_PrintBuffer, 0x1D, 2);
    }
    switch (f_OpeningScene) {
      case 0x0:
        g_ScreenFadeDuration = 1;
        g_ScreenFadeFramesLeft = 1;
        g_ScreenFadeKind = 1;
        g_ScreenFadeRed = 0;
        g_ScreenFadeGreen = 0;
        g_ScreenFadeBlue = 0;
        f_OpeningSceneFramesLeft = 0xA;
        f_OpeningScene++;
        break;
      case 0x1:
        g_ScreenFadeDuration = 1;
        g_ScreenFadeFramesLeft = 1;
        g_ScreenFadeKind = 1;
        g_ScreenFadeRed = 0;
        g_ScreenFadeGreen = 0;
        g_ScreenFadeBlue = 0;
        if (f_OpeningSceneFramesLeft == 0) {
          f_OpeningSceneFramesLeft = 0x1E;
          LoadNameLogo(0, 0);
          f_OpeningCue = 0;
          StartScreenFadeIn(0, 0, 0, 0x1E);
          PlaceEditorCamera(997.598f, -160.581f, 4594.014f, 973.0f, -130.0f,
                            4672.0f);
          ZoomEditorCamera(1, 38.01875f, 0x3C);
          TurnEditorCamera(1, -0.8528241f, 0x28);
          TiltEditorCamera(1, -0.324127f, 0x28);
          f_OpeningScene++;
          LoadOpeningShotBall();
          f_OpeningShotSimulation.position[0] = 0xF1B96;
          f_OpeningShotSimulation.position[2] = 0x48CFDC;
          f_OpeningShotSimulation.position[1] =
              GetGroundHeight(0xF1B96, 0x48CFDC);
          f_ShotBallScale = 2;
          f_OpeningSparksOrigin[0] =
              f_OpeningShotSimulation.position[0] * COURSE_TO_WORLD_SCALE;
          f_OpeningSparksOrigin[1] =
              f_OpeningShotSimulation.position[1] * COURSE_TO_WORLD_SCALE;
          f_OpeningSparksOrigin[2] =
              f_OpeningShotSimulation.position[2] * COURSE_TO_WORLD_SCALE;
          ClearEffects();
          D_8020C404_ovl_05[0] = f_OpeningShotSimulation.position[0];
          D_8020C404_ovl_05[1] = f_OpeningShotSimulation.position[1];
          D_8020C404_ovl_05[2] = f_OpeningShotSimulation.position[2];
        }
        break;
      case 0x2:
        if (f_OpeningSceneFramesLeft == 0) {
          f_IsNameSlotShown[0] = 1;
          PlaySong(0x2F, 0);
        }
        if (AreEditorMovesDone()) {
          f_OpeningSceneFramesLeft = 0x28;
          f_OpeningScene++;
        }
        break;
      case 0x3:
        if (f_OpeningSceneFramesLeft == 0x1C) {
          PlaySound(0x51, D_8020AC4C_ovl_05, 0x7F);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0x18) {
          PlaySoundWithEffects(0x59, D_8020AC4C_ovl_05, 0x4C, 0, 0xA7);
          func_801F4A40_ovl_05();
          f_OpeningSparksState = 1;
          LaunchDrop(D_8020C404_ovl_05);
        }
        if (f_OpeningSceneFramesLeft == 0x18) {
          f_OpeningShotSimulation.playerIndex = 0;
          f_OpeningShotSimulation.unkA0 = 0;
          f_OpeningShotSimulation.isRunning = 0;
          f_OpeningShotSimulation.surface = 0x17;
          func_8020A488_ovl_05(&f_OpeningShotSimulation, 0, &D_8020ADB8_ovl_05);
          D_8020AE44_ovl_05 = 1;
          f_ShotBallScale = 1;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          f_OpeningCue = 1;
          f_OpeningScene = 0x14;
        }
        break;
      case 0x14:
        PlaceEditorCamera(679.6445f, -98.28f, 4736.7f, 642.835f, -99.0f,
                          4746.187f);
        ZoomEditorCamera(1, 18.469883f, 0x1E);
        MoveEditorCamera(1, 642.835f, -100.5f, 4746.187f, 0x1E);
        f_IsNameSlotShown[0] = 0;
        D_8020AE44_ovl_05 = 0;
        f_ShotBallScale = 0;
        f_OpeningSceneFramesLeft = 0x1E;
        f_OpeningScene++;
        ClearEffects();
        break;
      case 0x15:
        switch (f_OpeningSceneFramesLeft) {
          case 25:
            PlaySound(0x196, D_8020AC4C_ovl_05, 0x7F);
            func_801F4A40_ovl_05();
            break;
          case 23:
            PlaySoundWithEffects(0x1AE, D_8020AC4C_ovl_05, 0x46, 0, 0xBB);
            func_801F4A40_ovl_05();
            break;
          case 20:
            PlaySoundWithEffects(0x177, D_8020AC4C_ovl_05, 0x46, 0, 0x57);
            func_801F4A40_ovl_05();
            break;
        }
        if (AreEditorMovesDone()) {
          StopEditorMoves();
          f_OpeningScene++;
        }
        break;
      case 0x16:
        f_OpeningCue = 2;
        f_OpeningScene = 0x28;
        break;
      case 0x1E:
        PlaceEditorCamera(1400.411f, -101.306f, 4781.043f, 1414.02f, -107.0f,
                          4800.41f);
        f_OpeningSceneFramesLeft = 0xE;
        f_OpeningScene++;
        break;
      case 0x1F:
        if (f_OpeningSceneFramesLeft == 8) {
          SetAnimationSpeed(2, 0.0f);
        }
        if (f_OpeningSceneFramesLeft == 0) {
          f_OpeningScene = 0x28;
        }
        break;
      case 0x28:
        PlaceEditorCamera(1269.768f, -127.806f, 4719.564f, 1179.989f, -111.987f,
                          4715.451f);
        f_OpeningShotSimulation.position[0] = 0xF1B96;
        f_OpeningShotSimulation.position[2] = 0x48CFDC;
        f_OpeningShotSimulation.position[1] =
            GetGroundHeight(0xF1B96, 0x48CFDC);
        f_OpeningShotSimulation.playerIndex = 0;
        f_OpeningShotSimulation.unkA0 = 0;
        f_OpeningShotSimulation.isRunning = 0;
        f_OpeningShotSimulation.surface = 0x17;
        func_8020A488_ovl_05(&f_OpeningShotSimulation, 0, &D_8020ADB8_ovl_05);
        D_8020AE44_ovl_05 = 1;
        f_ShotBallScale = 1;
        f_OpeningSparksState = 1;
        f_OpeningSceneFramesLeft = 0x14;
        f_OpeningScene++;
        break;
      case 0x29:
        if (f_OpeningSceneFramesLeft == 0xE) {
          PlaySoundWithEffects(0x148, D_8020AC4C_ovl_05, 0x7F, 0, 0x7F);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0) {
          f_LookAtEaseMode = 1;
          f_OpeningSceneFramesLeft = 0x32;
          f_OpeningScene++;
        }
        break;
      case 0x2A:
        if (f_OpeningSceneFramesLeft == 0) {
          StartHudAnimation(9);
          f_OpeningScene = 0x32;
          LoadNameLogo(2, 0);
        }
        break;
      case 0x32:
        PlaceEditorCamera(957.619f, -131.446f, 4629.353f, 973.561f, -133.0f,
                          4671.789f);
        f_OpeningCue = 4;
        f_LookAtEaseMode = 0;
        f_OpeningSceneFramesLeft = 0x64;
        D_8020AE44_ovl_05 = 0;
        f_OpeningShotSimulation.position[0] = 0xF1882;
        f_OpeningShotSimulation.position[2] = 0x48D4C4;
        f_OpeningScene++;
        f_OpeningShotSimulation.position[1] =
            GetGroundHeight(0xF1882, 0x48D4C4);
        f_ShotBallScale = 2;
        D_8020C404_ovl_05[0] = f_OpeningShotSimulation.position[0];
        D_8020C404_ovl_05[1] = f_OpeningShotSimulation.position[1];
        D_8020C404_ovl_05[2] = f_OpeningShotSimulation.position[2];
        f_OpeningSparksOrigin[0] =
            f_OpeningShotSimulation.position[0] * COURSE_TO_WORLD_SCALE;
        f_OpeningSparksOrigin[1] =
            f_OpeningShotSimulation.position[1] * COURSE_TO_WORLD_SCALE;
        f_OpeningSparksOrigin[2] =
            f_OpeningShotSimulation.position[2] * COURSE_TO_WORLD_SCALE;
        break;
      case 0x33:
        if (f_OpeningSceneFramesLeft == 0x18) {
          PlaySound(0x50, D_8020AC4C_ovl_05, 0x7F);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0x14) {
          PlaySoundWithEffects(0x59, D_8020AC4C_ovl_05, 0x4C, 0, 0xA7);
          func_801F4A40_ovl_05();
          f_OpeningSparksState = 1;
          LaunchDrop(D_8020C404_ovl_05);
        }
        if (f_OpeningSceneFramesLeft == 0x16) {
          f_OpeningShotSimulation.playerIndex = 0;
          f_OpeningShotSimulation.unkA0 = 0;
          f_OpeningShotSimulation.isRunning = 0;
          f_OpeningShotSimulation.surface = 0x17;
          func_8020A488_ovl_05(&f_OpeningShotSimulation, 0, &D_8020ADD8_ovl_05);
          D_8020AE44_ovl_05 = 1;
        }
        if (f_OpeningSceneFramesLeft == 0x5A) {
          f_IsNameSlotShown[0] = 1;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          f_OpeningScene = 0x3C;
        }
        break;
      case 0x3C:
        PlaceEditorCamera(726.63f, -98.049f, 5063.849f, 713.268f, -96.0f,
                          5094.7f);
        MoveEditorPoint(1, 713.268f, -102.0f, 5094.7f, 0x28);
        MoveEditorPoint(0, 708.085f, -98.243f, 5073.215f, 0x28);
        f_OpeningSceneFramesLeft = 0x28;
        f_OpeningScene = 0x46;
        f_OpeningCue = 5;
        f_IsNameSlotShown[0] = 0;
        D_8020AE44_ovl_05 = 0;
        f_ShotBallScale = 0;
        PlaySoundWithEffects(0x197, D_8020AC4C_ovl_05, 0x32, 0, 0x2F);
        func_801F4A40_ovl_05();
        ClearEffects();
        break;
      case 0x46:
        switch (f_OpeningSceneFramesLeft) {
          case 0x23:
            PlaySoundWithEffects(0x177, D_8020AC4C_ovl_05, 0x50, 0, 0xA7);
            func_801F4A40_ovl_05();
            break;
          case 0x21:
            PlaySound(0x1AF, D_8020AC4C_ovl_05, 0x7F);
            func_801F4A40_ovl_05();
            break;
          case 0x1F:
            PlaySoundWithEffects(0x146, D_8020AC4C_ovl_05, 0x50, 0, 0xBB);
            func_801F4A40_ovl_05();
            break;
        }
        if (f_OpeningSceneFramesLeft == 0x14) {
          f_OpeningShotSimulation.position[0] = 0xF1B96;
          f_OpeningShotSimulation.position[2] = 0x48CFDC;
          f_OpeningShotSimulation.position[1] =
              GetGroundHeight(0xF1B96, 0x48CFDC);
          f_OpeningShotSimulation.playerIndex = 0;
          f_OpeningShotSimulation.unkA0 = 0;
          f_OpeningShotSimulation.isRunning = 0;
          f_OpeningShotSimulation.surface = 0x17;
          func_8020A488_ovl_05(&f_OpeningShotSimulation, 0, &D_8020ADD8_ovl_05);
          D_8020AE44_ovl_05 = 1;
        }
        if (AreEditorMovesDone()) {
          PlaceEditorCamera(973.156f, -317.049f, 3703.677f, 1276.871f,
                            -112.538f, 4540.245f);
          f_ShotBallScale = 1;
          f_LookAtEaseMode = 1;
          f_OpeningSceneFramesLeft = 0x46;
          f_OpeningScene++;
          func_801F4A40_ovl_05();
        }
        break;
      case 0x47:
        if (f_OpeningSceneFramesLeft == 0) {
          f_OpeningScene = 0x50;
          StartHudAnimation(9);
        }
        break;
      case 0x50:
        PlaceEditorCamera(980.776f, -123.436f, 4636.89f, 971.837f, -133.5f,
                          4671.701f);
        f_OpeningSceneFramesLeft = 0x64;
        f_OpeningCue = 7;
        f_OpeningScene++;
        LoadNameLogo(3, 0);
        D_8020AE44_ovl_05 = 0;
        f_LookAtEaseMode = 0;
        f_OpeningShotSimulation.position[0] = 0xF3930;
        f_OpeningShotSimulation.position[2] = 0x48D1D1;
        f_OpeningShotSimulation.position[1] =
            GetGroundHeight(0xF3930, 0x48D1D1);
        f_ShotBallScale = 2;
        D_8020C404_ovl_05[0] = f_OpeningShotSimulation.position[0];
        D_8020C404_ovl_05[1] = f_OpeningShotSimulation.position[1];
        D_8020C404_ovl_05[2] = f_OpeningShotSimulation.position[2];
        f_OpeningSparksOrigin[0] =
            f_OpeningShotSimulation.position[0] * COURSE_TO_WORLD_SCALE;
        f_OpeningSparksOrigin[1] =
            f_OpeningShotSimulation.position[1] * COURSE_TO_WORLD_SCALE;
        f_OpeningSparksOrigin[2] =
            f_OpeningShotSimulation.position[2] * COURSE_TO_WORLD_SCALE;
        break;
      case 0x51:
        if (f_OpeningSceneFramesLeft == 0x18) {
          PlaySound(0x50, D_8020AC4C_ovl_05, 0x7F);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0x14) {
          PlaySoundWithEffects(0x59, D_8020AC4C_ovl_05, 0x4C, 0, 0xA7);
          func_801F4A40_ovl_05();
          f_OpeningSparksState = 1;
          LaunchDrop(D_8020C404_ovl_05);
        }
        if (f_OpeningSceneFramesLeft == 0x14) {
          f_OpeningShotSimulation.playerIndex = 0;
          f_OpeningShotSimulation.unkA0 = 0;
          f_OpeningShotSimulation.isRunning = 0;
          f_OpeningShotSimulation.surface = 0x17;
          func_8020A488_ovl_05(&f_OpeningShotSimulation, 0, &D_8020ADF8_ovl_05);
          D_8020AE44_ovl_05 = 1;
        }
        if (f_OpeningSceneFramesLeft == 0x5A) {
          f_IsNameSlotShown[0] = 1;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          f_OpeningScene = 0x5A;
          f_OpeningCue = 8;
        }
        break;
      case 0x5A:
        PlaceEditorCamera(1402.165f, -101.306f, 4774.711f, 1412.049f, -107.0f,
                          4798.309f);
        TiltEditorCamera(1, 0.183044f, 0x32);
        TurnEditorCamera(1, 1.1519989f, 0x32);
        ZoomEditorCamera(1, 47.392056f, 0x1E);
        f_OpeningSceneFramesLeft = 0x32;
        f_OpeningScene = 0x64;
        f_IsNameSlotShown[0] = 0;
        D_8020AE44_ovl_05 = 0;
        f_ShotBallScale = 0;
        ClearEffects();
        break;
      case 0x64:
        switch (f_OpeningSceneFramesLeft) {
          case 40:
            PlaySound(0x176, D_8020AC4C_ovl_05, 0x7F);
            func_801F4A40_ovl_05();
            break;
          case 38:
            PlaySoundWithEffects(0x147, D_8020AC4C_ovl_05, 0x46, 0, 0xBB);
            func_801F4A40_ovl_05();
            break;
          case 35:
            PlaySoundWithEffects(0x196, D_8020AC4C_ovl_05, 0x36, 0, 0x57);
            func_801F4A40_ovl_05();
            break;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          StopEditorMoves();
          StartHudAnimation(9);
          f_OpeningScene++;
        }
        break;
      case 0x65:
        PlaceEditorCamera(957.619f, -131.446f, 4629.353f, 973.561f, -133.0f,
                          4671.789f);
        TurnEditorCamera(1, 1.0471976f, 1);
        TiltEditorCamera(1, 0.34906584f, 1);
        f_OpeningCue = 9;
        f_OpeningScene++;
        LoadNameLogo(1, 0);
        f_OpeningSceneFramesLeft = 0xA;
        f_OpeningShotSimulation.position[0] = 0xF190E;
        f_OpeningShotSimulation.position[2] = 0x48D416;
        f_OpeningShotSimulation.position[1] =
            GetGroundHeight(0xF190E, 0x48D416);
        f_ShotBallScale = 2;
        D_8020C404_ovl_05[0] = f_OpeningShotSimulation.position[0];
        D_8020C404_ovl_05[1] = f_OpeningShotSimulation.position[1];
        D_8020C404_ovl_05[2] = f_OpeningShotSimulation.position[2];
        f_OpeningSparksOrigin[0] =
            f_OpeningShotSimulation.position[0] * COURSE_TO_WORLD_SCALE;
        f_OpeningSparksOrigin[1] =
            f_OpeningShotSimulation.position[1] * COURSE_TO_WORLD_SCALE;
        f_OpeningSparksOrigin[2] =
            f_OpeningShotSimulation.position[2] * COURSE_TO_WORLD_SCALE;
        break;
      case 0x66:
        TurnEditorCamera(1, -1.0471976f, 0x50);
        TiltEditorCamera(1, -0.34906584f, 0x50);
        f_OpeningScene = 0x6E;
        break;
      case 0x6E:
        if (f_OpeningSceneFramesLeft == 0) {
          f_IsNameSlotShown[0] = 1;
          f_OpeningScene = 0x6F;
          f_OpeningSceneFramesLeft = 0x37;
        }
        break;
      case 0x6F:
        if (f_OpeningSceneFramesLeft == 4) {
          PlaySound(0x51, D_8020AC4C_ovl_05, 0x7F);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0) {
          f_OpeningShotSimulation.playerIndex = 0;
          f_OpeningShotSimulation.unkA0 = 0;
          f_OpeningShotSimulation.isRunning = 0;
          f_OpeningShotSimulation.surface = 0x17;
          func_8020A488_ovl_05(&f_OpeningShotSimulation, 0, &D_8020AE18_ovl_05);
          D_8020AE44_ovl_05 = 1;
          f_ShotBallScale = 1;
          ZoomEditorCamera(1, 33.056084f, 0x28);
          f_OpeningSceneFramesLeft = 5;
          f_OpeningScene++;
          PlaySoundWithEffects(0x59, D_8020AC4C_ovl_05, 0x4C, 0, 0xA7);
          func_801F4A40_ovl_05();
          f_OpeningSparksState = 1;
          LaunchDrop(D_8020C404_ovl_05);
        }
        break;
      case 0x70:
        func_80206AA4_ovl_05(&f_OpeningShotSimulation);
        if (AreEditorMovesDone()) {
          PlaceEditorCamera(
              934.786f, -128.501f, 4716.105f,
              f_OpeningShotSimulation.position[0] * COURSE_TO_WORLD_SCALE,
              f_OpeningShotSimulation.position[1] * COURSE_TO_WORLD_SCALE,
              f_OpeningShotSimulation.position[2] * COURSE_TO_WORLD_SCALE);
          f_LookAtEaseMode = 2;
          f_OpeningSceneFramesLeft = 0x8C;
          f_IsNameSlotShown[0] = 0;
          f_OpeningScene++;
          ClearEffects();
        }
        break;
      case 0x71:
        if (f_OpeningSceneFramesLeft == 0x37) {
          PlaySoundWithEffects(0xB, D_8020AC4C_ovl_05, 0x50, 0xA, 0x9F);
          SetSoundFrequencyOffset(D_8020AC4C_ovl_05, -9.0f);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0x2D) {
          PlaySoundWithEffects(0xB, D_8020AC4C_ovl_05, 0x50, 0x14, 0x9F);
          SetSoundFrequencyOffset(D_8020AC4C_ovl_05, -10.0f);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0x23) {
          PlaySoundWithEffects(0xB, D_8020AC4C_ovl_05, 0x50, 0x1E, 0x9F);
          SetSoundFrequencyOffset(D_8020AC4C_ovl_05, -11.0f);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0x19) {
          PlaySoundWithEffects(0xB, D_8020AC4C_ovl_05, 0x50, 0x28, 0x9F);
          SetSoundFrequencyOffset(D_8020AC4C_ovl_05, -12.0f);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0xF) {
          PlaySound(5, D_8020AC4C_ovl_05, 0x30);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0) {
          f_OpeningScene = 0x78;
          f_LookAtEaseMode = 0;
        }
        break;
      case 0x78:
        if (AreEditorMovesDone()) {
          StartHudAnimation(1);
          f_OpeningScene++;
          LoadOpeningBall(4, 0);
          LoadOpeningBall(4, 1);
          f_OpeningBallPositions[0][0] = 2709.835f;
          f_OpeningBallPositions[0][2] = 2465.858f;
          f_OpeningBallScales[0] = 1.5f;
          f_OpeningBallPositions[1][0] = 2653.68f;
          f_OpeningBallPositions[1][2] = 2478.326f;
          f_OpeningBallScales[1] = 1.5f;
        }
        break;
      case 0x79:
        PlaceEditorCamera(2737.741f, -364.884f, 2368.88f, 2680.325f, -248.5f,
                          2488.521f);
        f_OpeningCue = 0xB;
        f_OpeningBallStates[0] = 1;
        f_OpeningBallStates[1] = 1;
        f_OpeningScene++;
        break;
      case 0x7A:
        TiltEditorCamera(1, -0.22654298f, 0x5A);
        f_OpeningScene++;
        break;
      case 0x7B:
        if (AreEditorMovesDone()) {
          f_OpeningScene = 0x82;
        }
        if (D_8020AC44_ovl_05 == 0x375) {
          PlaySound(0x4F, D_8020AC4C_ovl_05, 0x3C);
          func_801F4A40_ovl_05();
        }
        break;
      case 0x82:
        PlaceEditorCamera(2413.141f, -271.41f, 1784.364f, 2503.834f, -245.0f,
                          1850.029f);
        f_OpeningBallStates[1] = 0;
        f_OpeningBallPositions[0][0] = 2483.922f;
        f_OpeningBallPositions[0][2] = 1800.958f;
        f_OpeningBallScales[0] = 1.0f;
        MoveEditorCamera(1, 2489.032f, -255.774f, 1840.637f, 0x50);
        TiltEditorCamera(1, -0.116571f, 0x76);
        TurnEditorCamera(1, 1.366719f, 0x76);
        ZoomEditorCamera(1, 87.87598f, 0x76);
        f_OpeningCue = 0xC;
        f_OpeningSceneFramesLeft = 0x76;
        f_OpeningScene++;
        break;
      case 0x83:
        switch (f_OpeningSceneFramesLeft) {
          case 0x49:
          case 0x54:
          case 0x5E:
          case 0x68:
          case 0x73:
            PlaySound(0x4E, D_8020AC4C_ovl_05, 0x46);
            func_801F4A40_ovl_05();
            break;
          case 0x6A:
            PlaySoundWithEffects(0x2A, D_8020AC4C_ovl_05, 0x48, -6, 0x7F);
            func_801F4A40_ovl_05();
            break;
          case 0x64:
            PlaySound(0x18A, D_8020AC4C_ovl_05, 0x7F);
            func_801F4A40_ovl_05();
            break;
          case 0x46:
            if (f_IsDonkeyKongShown) {
              PlaySound(0x188, D_8020AC4C_ovl_05, 0x7F);
            } else {
              PlaySound(0x187, D_8020AC4C_ovl_05, 0x7F);
            }
            func_801F4A40_ovl_05();
            break;
        }
        if (AreEditorMovesDone()) {
          f_OpeningScene = 0x8C;
          f_OpeningSceneFramesLeft = 0x28;
        }
        break;
      case 0x8C:
        if (f_OpeningSceneFramesLeft == 0) {
          f_OpeningCue = 0xD;
          f_OpeningScene++;
          MoveEditorCamera(1, 2489.636f, -257.5f, 1811.529f, 0x1E);
          TiltEditorCamera(1, -0.070194006f, 0x32);
          TurnEditorCamera(1, 0.3807652f, 0x32);
          ZoomEditorCamera(1, 34.2068f, 0x32);
          if (f_IsDonkeyKongShown) {
            PlaySoundWithEffects(0x1A2, D_8020AC4C_ovl_05, 0x7F, 0, 0x3F);
          } else {
            PlaySoundWithEffects(0x1A1, D_8020AC4C_ovl_05, 0x50, 0, 0x3F);
          }
          func_801F4A40_ovl_05();
        }
        break;
      case 0x8D:
        if (AreEditorMovesDone()) {
          f_OpeningScene = 0x96;
          f_OpeningSceneFramesLeft = 0xA;
        }
        break;
      case 0x96:
        if (f_OpeningSceneFramesLeft == 0) {
          StartHudAnimation(9);
          PlaceEditorCamera(2801.815f, -255.822f, 2196.813f, 2823.524f, -256.5f,
                            2177.502f);
          MoveEditorPoint(1, 2855.966f, -256.5f, 2213.121f, 0x32);
          MoveEditorPoint(0, 2702.606f, -279.683f, 2250.851f, 0x32);
          f_OpeningSceneFramesLeft = 0x5A;
          f_OpeningCue = 0xE;
          g_LightPitch = 0x1000;
          /* Stored as a signed value, as the original does */
          *(s16*)&g_LightYaw = -0x7800;
          f_OpeningScene++;
        }
        break;
      case 0x97:
        switch (f_OpeningSceneFramesLeft) {
          case 0x57:
            PlaySoundWithEffects(0x4E, D_8020AC4C_ovl_05, 0x50, 0, 0xA7);
            func_801F4A40_ovl_05();
            break;
          case 0x4D:
            PlaySoundWithEffects(0x4E, D_8020AC4C_ovl_05, 0x4C, 0, 0x9D);
            func_801F4A40_ovl_05();
            break;
          case 0x43:
            PlaySoundWithEffects(0x4E, D_8020AC4C_ovl_05, 0x48, 0, 0x93);
            func_801F4A40_ovl_05();
            break;
          case 0x39:
            PlaySoundWithEffects(0x4E, D_8020AC4C_ovl_05, 0x3A, 0, 0x89);
            func_801F4A40_ovl_05();
            break;
          case 0x2A:
            PlaySound(0x35, D_8020AC4C_ovl_05, 0x46);
            SetSoundFrequencyOffset(D_8020AC4C_ovl_05, 7.0f);
            func_801F4A40_ovl_05();
            break;
        }
        if (f_OpeningSceneFramesLeft >= 0x17 &&
            f_OpeningSceneFramesLeft < 0x2A) {
          GetCharacterPointPosition(2, 2, point);
          g_CurrentShot.ballPosition[0] = (s32)point[0] * 1024;
          g_CurrentShot.ballPosition[2] = (s32)point[2] * 1024;
          g_CurrentShot.ballPosition[1] =
              GetGroundHeight(g_CurrentShot.ballPosition[0],
                              g_CurrentShot.ballPosition[2]) +
              15360.0f;
          SpawnRisingClumps(D_8020C404_ovl_05);
        }
        if (f_OpeningSceneFramesLeft == 0x50) {
          if (IsDonkeyKongUnlocked()) {
            if (!(rand() & 0x80)) {
              f_IsDonkeyKongShown = 1;
            } else {
              f_IsDonkeyKongShown = 0;
            }
          }
          LoadOpeningBall(4, 0);
          f_OpeningBallPositions[0][0] = 2723.423f;
          f_OpeningBallPositions[0][2] = 2246.086f;
        }
        if (f_OpeningSceneFramesLeft == 0x32) {
          f_OpeningBallStates[0] = 1;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          TiltEditorCamera(1, -0.166897f, 0x32);
          f_OpeningScene++;
          PlaySound(0x60, D_8020AC4C_ovl_05, 0x32);
          func_801F4A40_ovl_05();
        }
        break;
      case 0x98:
        if (AreEditorMovesDone()) {
          f_OpeningSceneFramesLeft = 0x14;
          f_OpeningScene = 0xA0;
        }
        break;
      case 0xA0:
        if (f_OpeningSceneFramesLeft == 0) {
          PlaceEditorCamera(2712.031f, -270.705f, 2358.303f, 2770.045f, -250.5f,
                            2337.548f);
          f_OpeningCue = 0xF;
          f_OpeningSceneFramesLeft = 0x40;
          f_OpeningScene = 0xAA;
          f_OpeningBallPositions[0][0] = 2755.104f;
          f_OpeningBallPositions[0][2] = 2341.293f;
        }
        break;
      case 0xAA:
        switch (f_OpeningSceneFramesLeft) {
          case 0x35:
          case 0x2A:
          case 0x1A:
            PlaySound(0x4E, D_8020AC4C_ovl_05, 0x30);
            func_801F4A40_ovl_05();
            break;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          PlaceEditorCamera(2670.074f, -270.531f, 2375.225f, 2692.222f, -248.0f,
                            2366.292f);
          ZoomEditorCamera(1, 16.721228f, 0x28);
          f_OpeningScene++;
          PlaySound(0x60, D_8020AC4C_ovl_05, 0x3C);
          func_801F4A40_ovl_05();
          g_LightPitch = 0x1000;
          g_LightYaw = 0x6800;
          if (f_IsDonkeyKongShown) {
            LoadOpeningBall(1, 1);
          } else {
            LoadOpeningBall(2, 1);
          }
          f_OpeningBallStates[0] = 0;
          f_OpeningBallStates[1] = 1;
          f_OpeningBallPositions[1][0] = 2689.998f;
          f_OpeningBallPositions[1][2] = 2367.191f;
          f_OpeningBallScales[1] = 3.0f;
        }
        break;
      case 0xAB:
        if (AreEditorMovesDone()) {
          f_OpeningScene = 0xB4;
          f_OpeningSceneFramesLeft = 0x14;
        }
        break;
      case 0xB4:
        if (f_OpeningSceneFramesLeft == 0) {
          PlaceEditorCamera(2712.031f, -270.705f, 2358.303f, 2770.045f, -250.5f,
                            2337.548f);
          f_OpeningCue = 0x11;
          f_OpeningSceneFramesLeft = 0x38;
          g_CameraEditor.fieldOfView = 45.0f;
          f_OpeningScene++;
          LoadOpeningBall(4, 0);
          f_OpeningBallStates[0] = 1;
          f_OpeningBallStates[1] = 0;
          f_OpeningBallPositions[0][0] = 2755.104f;
          f_OpeningBallPositions[0][2] = 2341.293f;
          f_OpeningBallScales[0] = 1.0f;
        }
        break;
      case 0xB5:
        if (f_OpeningSceneFramesLeft == 0xA) {
          PlaySoundWithEffects(0xC5, D_8020AC4C_ovl_05, 0x40, 0x14, 0xCF);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0) {
          f_OpeningCue = 0x12;
          f_OpeningSceneFramesLeft = 0x1E;
          f_OpeningScene++;
          LoadOpeningBall(4, 1);
          f_OpeningBallPositions[1][0] = 0.0f;
          f_OpeningBallPositions[1][2] = 0.0f;
        }
        break;
      case 0xB6:
        if (D_8020AC44_ovl_05 == 0x5C5) {
          PlaySoundWithEffects(0x189, D_8020AC4C_ovl_05, 0x7F, 0, 0x89);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0) {
          f_OpeningCue = 0x13;
          f_OpeningScene = 0xBE;
        }
        break;
      case 0xBE:
        GetCharacterPointPosition(1, 3, logoBallPoint);
        f_OpeningBallStates[1] = 2;
        f_OpeningBallPositions[1][0] = 2319.014f;
        f_OpeningBallPositions[1][1] = -560.0f;
        f_OpeningBallPositions[1][2] = 342.12f;
        f_OpeningBallScales[1] = 2.0f;
        PlaceEditorCamera(2662.345f, -254.25f, 2376.081f, 2493.599f, -910.952f,
                          665.178f);
        MoveEditorPoint(0, 2671.388f, -257.489f, 2351.288f, 0x28);
        MoveEditorPoint(1, logoBallPoint[0], logoBallPoint[1] - 4.5f,
                        logoBallPoint[2], 0x28);
        f_IsLogoBallSlot[1] = 1;
        f_OpeningScene++;
        D_80217078_ovl_05 = 1.0f;
        break;
      case 0xBF:
        f_OpeningBallPositions[1][0] +=
            (g_CameraEditor.points[1][0] - f_OpeningBallPositions[1][0]) *
            D_80217078_ovl_05;
        f_OpeningBallPositions[1][1] +=
            (g_CameraEditor.points[1][1] - f_OpeningBallPositions[1][1]) *
            D_80217078_ovl_05;
        f_OpeningBallPositions[1][2] +=
            (g_CameraEditor.points[1][2] - f_OpeningBallPositions[1][2]) *
            D_80217078_ovl_05;
        D_80217078_ovl_05 -= 0.02f;
        if (D_80217078_ovl_05 < 0.1f) {
          D_80217078_ovl_05 = 0.1f;
        }
        if (AreEditorMovesDone()) {
          f_OpeningCue = 0x14;
          f_OpeningSceneFramesLeft = 0x78;
          f_OpeningScene = 0xC8;
          f_LookAtEaseMode = 3;
        }
        break;
      case 0xC8:
        D_80217078_ovl_05 += 0.02f;
        if (D_80217078_ovl_05 > 1.0f) {
          D_80217078_ovl_05 = 1.0f;
        }
        GetCharacterPointPosition(1, 3, handPoint);
        f_OpeningBallPositions[1][0] +=
            (handPoint[0] - f_OpeningBallPositions[1][0]) * D_80217078_ovl_05;
        f_OpeningBallPositions[1][1] +=
            (handPoint[1] - f_OpeningBallPositions[1][1]) * D_80217078_ovl_05;
        f_OpeningBallPositions[1][2] +=
            (handPoint[2] - f_OpeningBallPositions[1][2]) * D_80217078_ovl_05;
        switch (f_OpeningSceneFramesLeft) {
          case 0x61:
            PlaySoundWithEffects(2, D_8020AC4C_ovl_05, 0x7F, 0, 0xA7);
            func_801F4A40_ovl_05();
            break;
          case 0x45:
            PlaySoundWithEffects(2, D_8020AC4C_ovl_05, 0x7F, 0, 0x93);
            func_801F4A40_ovl_05();
            break;
          case 0x37:
            PlaySoundWithEffects(0x14, D_8020AC4C_ovl_05, 0x40, 0x7F, 0x7F);
            break;
          case 0xF:
            StopSound(D_8020AC4C_ovl_05);
            func_801F4A40_ovl_05();
            break;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          StartHudAnimation(9);
          PlaceEditorCamera(2878.956f, -391.282f, 2514.85f, 2431.88f, -315.0f,
                            992.943f);
          MoveEditorPoint(0, 2051.33f, -258.127f, 862.423f, 0x28);
          MoveEditorPoint(1, 2058.79f, -254.0f, 825.963f, 0x28);
          f_OpeningCue = 0x15;
          f_OpeningSceneFramesLeft = 0x14;
          f_OpeningScene++;
          LoadNameLogo(5, 0);
          if (f_IsDonkeyKongShown) {
            LoadNameLogo(6, 1);
          } else {
            LoadNameLogo(4, 1);
          }
          f_OpeningBallStates[1] = 0;
          f_IsLogoBallSlot[1] = 0;
          f_ShotBallScale = 0;
          f_LookAtEaseMode = 0;
          PlaySound(0x60, D_8020AC4C_ovl_05, 0x50);
          func_801F4A40_ovl_05();
        }
        break;
      case 0xC9:
        if (f_OpeningSceneFramesLeft == 0) {
          f_OpeningCue = 0x16;
          f_OpeningSceneFramesLeft = 0x24;
          f_OpeningScene++;
        }
        break;
      case 0xCA:
        if (f_IsDonkeyKongShown) {
          if (f_OpeningSceneFramesLeft == 0x12) {
            PlaySoundWithEffects(0x41, D_8020AC4C_ovl_05, 0x7F, 7, 0x7F);
            SetSoundFrequencyOffset(D_8020AC4C_ovl_05, -4.0f);
            func_801F4A40_ovl_05();
          }
        } else {
          switch (f_OpeningSceneFramesLeft) {
            case 0x13:
            case 9:
              PlaySound(0x4F, D_8020AC4C_ovl_05, 0x43);
              func_801F4A40_ovl_05();
              break;
          }
        }
        if (AreEditorMovesDone()) {
          f_IsNameSlotShown[0] = 1;
          f_IsNameSlotShown[1] = 1;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          if (f_IsDonkeyKongShown) {
            MoveEditorPoint(1, 2043.766f, -256.5f, 851.89f, 0x5A);
            MoveEditorPoint(0, 2032.192f, -262.337f, 875.169f, 0x5A);
          } else {
            MoveEditorPoint(1, 2043.767f, -250.0f, 851.891f, 0x5A);
            MoveEditorPoint(0, 2025.674f, -258.046f, 888.28f, 0x5A);
          }
          f_OpeningSceneFramesLeft = 0x5A;
          f_OpeningScene++;
        }
        break;
      case 0xCB:
        if (f_IsDonkeyKongShown) {
          switch (f_OpeningSceneFramesLeft) {
            case 0x1A:
            case 0x2E:
            case 0x42:
            case 0x56:
              PlaySoundWithEffects(0x41, D_8020AC4C_ovl_05, 0x7F, 7, 0x7F);
              SetSoundFrequencyOffset(D_8020AC4C_ovl_05, -4.0f);
              func_801F4A40_ovl_05();
              break;
            case 0x43:
              PlaySound(0x4D, D_8020AC4C_ovl_05, 0x28);
              func_801F4A40_ovl_05();
              break;
            case 0x2F:
              PlaySound(0x2A, D_8020AC4C_ovl_05, 0x60);
              func_801F4A40_ovl_05();
              break;
            case 0x1F:
              PlaySound(0x2A, D_8020AC4C_ovl_05, 0x30);
              func_801F4A40_ovl_05();
              break;
            case 0xC:
              PlaySoundWithEffects(0xCA, D_8020AC4C_ovl_05, 0x7F, 0, 0x93);
              func_801F4A40_ovl_05();
              break;
            case 0x8:
              PlaySoundWithEffects(0xEA, D_8020AC4C_ovl_05, 0x7F, 0, 0x6B);
              func_801F4A40_ovl_05();
              break;
          }
        } else {
          switch (f_OpeningSceneFramesLeft) {
            case 0x45:
            case 0x4F:
            case 0x59:
              PlaySound(0x4F, D_8020AC4C_ovl_05, 0x57);
              func_801F4A40_ovl_05();
              break;
            case 0x2A:
              PlaySound(0x4F, D_8020AC4C_ovl_05, 0x61);
              func_801F4A40_ovl_05();
              break;
            case 0x3B:
              PlaySound(0x4D, D_8020AC4C_ovl_05, 0x51);
              func_801F4A40_ovl_05();
              break;
            case 0x22:
              PlaySound(0x4D, D_8020AC4C_ovl_05, 0x5B);
              func_801F4A40_ovl_05();
              break;
            case 0x1D:
              PlaySound(0x2A, D_8020AC4C_ovl_05, 0x6B);
              func_801F4A40_ovl_05();
              break;
            case 0x27:
              PlaySoundWithEffects(0x2A, D_8020AC4C_ovl_05, 0x30, 0, 0x65);
              func_801F4A40_ovl_05();
              break;
            case 0x16:
              PlaySoundWithEffects(0x2A, D_8020AC4C_ovl_05, 0x18, 0, 0x5B);
              func_801F4A40_ovl_05();
              break;
            case 0x4:
              PlaySoundWithEffects(0x20B, D_8020AC4C_ovl_05, 0x7F, 0, 0x6B);
              func_801F4A40_ovl_05();
              break;
          }
        }
        if (AreEditorMovesDone()) {
          f_OpeningSceneFramesLeft = 0x14;
          f_OpeningScene = 0xD2;
        }
        break;
      case 0xD2:
        if (!f_IsDonkeyKongShown && f_OpeningSceneFramesLeft == 0x10) {
          PlaySoundWithEffects(0xC3, D_8020AC4C_ovl_05, 0x60, 0, 0x93);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0) {
          PlaceEditorCamera(2708.301f, -264.204f, 2343.526f, 2770.098f, -260.0f,
                            2337.539f);
          f_OpeningCue = 0x17;
          f_OpeningSceneFramesLeft = 0x46;
          f_IsNameSlotShown[0] = 0;
          f_IsNameSlotShown[1] = 0;
          f_OpeningScene++;
          LoadWipeShape(0xC);
        }
        break;
      case 0xD3:
        switch (f_OpeningSceneFramesLeft) {
          case 0x3C:
            PlaySound(0x184, D_8020AC4C_ovl_05, 0x60);
            func_801F4A40_ovl_05();
            /* fallthrough */
          case 0x32:
            PlaySound(0x18B, D_8020AC4C_ovl_05, 0x60);
            func_801F4A40_ovl_05();
            break;
          case 0x28:
            PlaySound(0x18B, D_8020AC4C_ovl_05, 0x60);
            func_801F4A40_ovl_05();
            break;
          case 0x25:
            PlaySound(0x184, D_8020AC4C_ovl_05, 0x60);
            func_801F4A40_ovl_05();
            break;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          StartWipe(1);
          f_OpeningScene++;
        }
        break;
      case 0xD4:
        if (!GetWipeProgress()) {
          f_OpeningSceneFramesLeft = 0x14;
          f_OpeningScene = 0xDC;
          f_OpeningCue = 0x18;
          f_OpeningBallStates[0] = 0;
        }
        break;
      case 0xDC:
        if (f_OpeningSceneFramesLeft == 0) {
          PlaceEditorCamera(1423.923f, -248.221f, 1595.031f, 1450.829f, -254.0f,
                            1546.087f);
          MoveEditorPoint(1, 1420.136f, -250.5f, 1551.713f, 0x32);
          f_OpeningCue = 0x19;
          f_OpeningScene++;
          StartWipe(2);
          f_OpeningSceneFramesLeft = 0xA0;
          LoadOpeningImpactLines();
        }
        break;
      case 0xDD:
        if (f_OpeningSceneFramesLeft == 0x1E) {
          f_AreImpactLinesShown = 1;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          f_OpeningScene = 0xE6;
          LoadOpeningBall(3, 0);
          f_OpeningBallStates[0] = 1;
          f_OpeningBallPositions[0][0] = 932.978f;
          f_OpeningBallPositions[0][2] = 1884.644f;
          f_OpeningBallScales[0] = 1.4f;
        }
        switch (f_OpeningSceneFramesLeft) {
          case 0x93:
            PlaySoundWithEffects(0x4E, D_8020AC4C_ovl_05, 0x50, 0, 0x61);
            func_801F4A40_ovl_05();
            break;
          case 0x89:
            PlaySoundWithEffects(0x4E, D_8020AC4C_ovl_05, 0x50, 0, 0x6B);
            func_801F4A40_ovl_05();
            break;
          case 0x7F:
            PlaySoundWithEffects(0x4E, D_8020AC4C_ovl_05, 0x50, 0, 0x75);
            func_801F4A40_ovl_05();
            break;
          case 0x74:
            PlaySoundWithEffects(0x4E, D_8020AC4C_ovl_05, 0x50, 0, 0x7F);
            func_801F4A40_ovl_05();
            break;
          case 0x1E:
            PlaySound(0x36, D_8020AC4C_ovl_05, 0x46);
            func_801F4A40_ovl_05();
            break;
          case 0x1C:
            if (f_IsDonkeyKongShown) {
              PlaySound(0x141, D_8020AC4C_ovl_05, 0x7F);
              D_8020E3F0_ovl_05 = D_8020AC4C_ovl_05;
            } else {
              PlaySound(0x152, D_8020AC4C_ovl_05, 0x7F);
            }
            func_801F4A40_ovl_05();
            break;
          case 0x14:
            if (f_IsDonkeyKongShown) {
              StopSound(D_8020E3F0_ovl_05);
            }
            break;
        }
        break;
      case 0xE6:
        PlaceEditorCamera(956.687f, -259.45f, 1874.671f, 933.391f, -243.5f,
                          1884.391f);
        f_OpeningSceneFramesLeft = 0x14;
        f_OpeningCue = 0x1A;
        f_AreImpactLinesShown = 0;
        g_CameraEditor.fieldOfView = 18.0f;
        f_OpeningScene++;
        break;
      case 0xE7:
        if (f_OpeningSceneFramesLeft == 6) {
          PlaySoundWithEffects(0x1F, D_8020AC4C_ovl_05, 0x60, 0, 0xA7);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0) {
          MoveEditorPoint(1, 933.391f, -250.5f, 1884.391f, 0x3C);
          f_OpeningSceneFramesLeft = 0x3C;
          f_OpeningScene++;
        }
        break;
      case 0xE8:
        g_CameraEditor.fieldOfView += 0.2666666f;
        switch (f_OpeningSceneFramesLeft) {
          case 0x36:
            PlaySoundWithEffects(0x1F, D_8020AC4C_ovl_05, 0x60, 0, 0x57);
            func_801F4A40_ovl_05();
            break;
          case 0xA:
            PlaySound(0x77, D_8020AC4C_ovl_05, 0x7F);
            func_801F4A40_ovl_05();
            break;
          case 0x24:
            PlaySoundWithEffects(0x1F, D_8020AC4C_ovl_05, 0x60, 0, 0xA7);
            func_801F4A40_ovl_05();
            break;
          case 0x6:
            PlaySoundWithEffects(0x1F, D_8020AC4C_ovl_05, 0x60, 0, 0x57);
            func_801F4A40_ovl_05();
            break;
        }
        if (AreEditorMovesDone()) {
          f_OpeningScene = 0xF0;
          f_OpeningSceneFramesLeft = 0x3C;
        }
        break;
      case 0xF0:
        switch (f_OpeningSceneFramesLeft) {
          case 0x24:
            PlaySoundWithEffects(0x1F, D_8020AC4C_ovl_05, 0x60, 0, 0xA7);
            func_801F4A40_ovl_05();
            /* fallthrough */
          case 0x6:
            PlaySoundWithEffects(0x1F, D_8020AC4C_ovl_05, 0x60, 0, 0x57);
            func_801F4A40_ovl_05();
            break;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          PlaceEditorCamera(1423.703f, -250.799f, 1574.041f, 1420.135f, -252.0f,
                            1551.713f);
          f_OpeningScene = 0xFA;
          f_OpeningSceneFramesLeft = 0x50;
          f_OpeningCue = 0x1B;
          f_OpeningBallStates[0] = 0;
          g_CameraEditor.fieldOfView = 45.0f;
          LoadOpeningBurst();
        }
        break;
      case 0xFA:
        if (f_OpeningSceneFramesLeft == 0x2E) {
          f_IsBurstShown = 1;
          PlaySound(0x34, D_8020AC4C_ovl_05, 0x50);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0x1A) {
          PlaySound(0x141, D_8020AC4C_ovl_05, 0x7F);
          func_801F4A40_ovl_05();
        }
        if (f_OpeningSceneFramesLeft == 0x14) {
          f_AreAmbientSoundsOn = 1;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          MoveEditorPoint(0, 1404.737f, -250.799f, 1633.633f, 0x50);
          f_OpeningScene++;
          LoadLakitu();
          f_OpeningSceneFramesLeft = 0x28;
        }
        break;
      case 0xFB:
        g_CameraEditor.fieldOfView += 0.2f;
        if (g_CameraEditor.fieldOfView > 100.0f) {
          g_CameraEditor.fieldOfView = 100.0f;
        }
        if (AreEditorMovesDone()) {
          f_OpeningSceneFramesLeft = 0x14;
          f_OpeningScene++;
        }
        break;
      case 0xFC:
        g_CameraEditor.fieldOfView += 0.2f;
        if (g_CameraEditor.fieldOfView > 100.0f) {
          g_CameraEditor.fieldOfView = 100.0f;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          MoveEditorPoint(0, 1409.318f, -400.0f, 1609.259f, 0xDC);
          f_OpeningSceneFramesLeft = 0x28;
          f_OpeningScene++;
        }
        break;
      case 0xFD:
        g_CameraEditor.fieldOfView += 0.2f;
        if (g_CameraEditor.fieldOfView > 100.0f) {
          g_CameraEditor.fieldOfView = 100.0f;
        }
        if (f_OpeningSceneFramesLeft == 0) {
          f_IsLakituFlying = 1;
          f_OpeningSceneFramesLeft = 1000;
          f_OpeningScene++;
          PlaySound(0x28, D_8020AC4C_ovl_05, 0x3F);
          func_801F4A40_ovl_05();
          f_AreAmbientSoundsOn = 0;
        }
        break;
      case 0xFE:
        g_CameraEditor.fieldOfView += 0.2f;
        if (g_CameraEditor.fieldOfView > 100.0f) {
          g_CameraEditor.fieldOfView = 100.0f;
        }
        switch (f_OpeningSceneFramesLeft) {
          case 0x3C9:
          case 0x3CA:
          case 0x3CB:
          case 0x3CC:
          case 0x3E1:
          case 0x3E2:
          case 0x3E3:
          case 0x3E5:
          case 0x3E6:
            PlaySound(0x27, D_8020AC4C_ovl_05, 0x40);
            func_801F4A40_ovl_05();
            break;
        }
        switch (f_OpeningSceneFramesLeft) {
          case 0x3A8:
          case 0x3A9:
          case 0x3AB:
          case 0x3AC:
          case 0x3C1:
          case 0x3C3:
          case 0x3C5:
          case 0x3C6:
          case 0x3C8:
            PlaySound(0x4D, D_8020AC4C_ovl_05, 0x60);
            func_801F4A40_ovl_05();
            break;
        }
        func_801F4A6C_ovl_05();
        if (f_OpeningSceneFramesLeft == 0x3CF) {
          PlaySoundWithEffects(0x211, 0xB, 0x7F, 0, 0x7F);
          D_8020E3F4_ovl_05 = 0xB;
        }
        break;
      case 998:
        StartScreenFadeOut(0xFF, 0xFF, 0xFF, 4);
        f_OpeningScene = 999;
        break;
      case 999:
        if (HasScreenFadeEnded()) {
          StopEditorMoves();
          D_800B6808 = 1;
          g_Scene = 0x19;
          g_NextScene = 0x15;
          g_SceneModuleToUnload = 3;
          g_SceneModuleToLoad = 4;
          ClearDebugText();
          StopWipe();
          FreeAllCharacterSlots();
          for (channel = 0; channel < 0xD; channel++) {
            if (D_800B6808 != 1 || (channel != D_8020E3F0_ovl_05 &&
                                    channel != D_8020E3F4_ovl_05)) {
              StopSound(channel);
            }
          }
          if (!D_8020AC48_ovl_05) {
            PlaySound(0x5D, 0xF, 0x7F);
          }
          SetTextSlot(0xA0, 0x72, -1, 1, 0, 0, 0, 0, 0, 0);
          return;
        }
        break;
    }
  }
  if (f_OpeningSceneFramesLeft > 0) {
    f_OpeningSceneFramesLeft--;
  }
  UpdateCameraEditor();
  EaseOpeningLookAt();
  g_Cameras[0].view.mode = 1;
  g_Cameras[0].view.eye[0] = g_CameraEditor.points[0][0];
  g_Cameras[0].view.eye[1] = g_CameraEditor.points[0][1];
  g_Cameras[0].view.eye[2] = g_CameraEditor.points[0][2];
  g_Cameras[0].view.lookAt[0] = g_CameraEditor.points[1][0];
  g_Cameras[0].view.lookAt[1] = g_CameraEditor.points[1][1];
  g_Cameras[0].view.lookAt[2] = g_CameraEditor.points[1][2];
  g_Cameras[0].view.fieldOfView = g_CameraEditor.fieldOfView;
  if (D_8020AE44_ovl_05 == 1) {
    D_8020AEBC_ovl_05 += g_ControllerStates[1].stickX * 0.002f;
    D_8020AEC0_ovl_05 += g_ControllerStates[1].stickY * 0.002f;
    for (slot = 0; slot < 4; slot++) {
      if (IsCharacterSlotBusy(slot)) {
        GetCharacterPointPosition(slot, 1, point);
      }
    }
    point[0] = f_OpeningShotSimulation.position[0] * COURSE_TO_WORLD_SCALE;
    point[1] = f_OpeningShotSimulation.position[1] * COURSE_TO_WORLD_SCALE;
    point[2] = f_OpeningShotSimulation.position[2] * COURSE_TO_WORLD_SCALE;
    func_80206AA4_ovl_05(&f_OpeningShotSimulation);
  } else if (D_8020AE44_ovl_05 == 2) {
    func_80206AA4_ovl_05(&f_OpeningShotSimulation);
  }
  if (f_OpeningScene != 999 && !D_8020AC48_ovl_05 && !IsGameFlagSet(0x62)) {
    if (f_OpeningScene != 998) {
      pressedButtons = 0;
      for (i = 0; i != g_ControllerCount; i++) {
        pressedButtons |= g_ControllerStates[i].pressedButtons;
      }
      if (g_IsDebugEnabled) {
        skipButtons = g_ControllerStates[0].pressedButtons & 0x9000;
      } else {
        skipButtons = pressedButtons & 0xD000;
      }
      if (skipButtons) {
        f_OpeningScene = 998;
      }
    }
    if (g_IsDebugEnabled) {
      if (g_ControllerStates[2].pressedButtons & 0x8000) {
        PlaySound(0x5D, 0xF, 0x7F);
        SetGameFlag(0x62);
        f_IsLookAtMarkerShown = 1;
        f_LookAtEaseMode = 0;
      }
      if (g_IsDebugEnabled && (g_ControllerStates[1].pressedButtons & 0x10)) {
        f_OpeningSceneFramesLeft = 0;
        StopEditorMoves();
      }
    }
  }
  if (UpdateOpeningCharacters()) {
    f_OpeningScene = 999;
  }
  AnimateAllCharacters();
  AnimateNameLogos();
  MoveLakitu();
  PlayOpeningAmbientSounds();
}

/* Draws the camera editor's look-at marker, the characters and the opening's
 * overlays */
void DrawOpening(CameraMatrices* matrices, Gfx** displayListHead) {
  /* Unused; they give the stack frame the original's size */
  Mat4 unusedMatrix;
  Vec3 unusedPoints[4];
  Gfx* gfx = *displayListHead;

  f_OpeningFrameParity ^= 1;
  if (f_IsLookAtMarkerShown) {
    guTranslate(&f_LookAtMarkerTranslation, g_Cameras[0].view.lookAt[0],
                g_Cameras[0].view.lookAt[1], g_Cameras[0].view.lookAt[2]);
    guScale(&f_LookAtMarkerScaling, g_LookAtMarkerScale, g_LookAtMarkerScale,
            g_LookAtMarkerScale);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_LookAtMarkerTranslation),
              G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_LookAtMarkerScaling),
              G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(gfx++, f_LookAtMarkerVertices[0], 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPVertex(gfx++, f_LookAtMarkerVertices[1], 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPVertex(gfx++, f_LookAtMarkerVertices[2], 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPVertex(gfx++, f_LookAtMarkerVertices[3], 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPVertex(gfx++, f_LookAtMarkerVertices[4], 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPVertex(gfx++, f_LookAtMarkerVertices[5], 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
  }
  DrawOpeningCharacters(matrices, &gfx, f_OpeningFrameParity);
  g_DisplayListHead = gfx;
  DrawOpeningShotPath(matrices);
  gfx = g_DisplayListHead;
  DrawLensFlare(&gfx);
  DrawOpeningImpactLines(&gfx);
  DrawOpeningBurst(&gfx);
  DrawLakitu(&gfx);
  DrawOpeningBalls(&gfx, matrices);
  DrawOpeningShotBall(&gfx);
  DrawOpeningSparks(&gfx);
  DrawNameLogos(&gfx);
  DrawOpeningLogo(&gfx, 0);
  *displayListHead = gfx;
}

/* Loads name logo logoIndex into slot and resets the slot */
void LoadNameLogo(s32 logoIndex, s32 slot) {
  EncodedAsset asset;

  OpenEncodedAsset(f_OpeningNameLogos[logoIndex].assetId, &asset);
  ReadEncodedAsset(f_NameSlotImages[slot], &asset);
  f_NameSlotLogoIndices[slot] = logoIndex;
  f_NameSlotSteps[slot] = 0;
  f_IsNameSlotShown[slot] = 0;
}

/* The logo shown in the name slot SLOT */
#define SLOT_LOGO f_OpeningNameLogos[f_NameSlotLogoIndices[slot]]
#define NAME_LOGO_STRIP_COUNT 4
/* The rows that a strip of a logo's image holds, and their log2 */
#define NAME_LOGO_STRIP_ROWS 8
#define NAME_LOGO_STRIP_ROWS_SHIFT 3

#ifdef NON_MATCHING
/*
 * Draws the shown name logos in four strips each. Until a logo flips back up,
 * its strips are drawn in reverse order with their rows mirrored, which turns
 * it upside down.
 *
 * Differs from the original only in stack slots: the pointers of the second
 * and third commands swap slots, as in the opening's sparks.
 */
void DrawNameLogos(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 xEdges[2];                         /* Left and right */
  s32 yEdges[NAME_LOGO_STRIP_COUNT + 1]; /* Top to bottom of each strip */
  s32 strip;
  s32 slot;
  s32 stripHeight;
  s32 texelsOffset;

  for (slot = 0; slot != OPENING_NAME_SLOT_COUNT; slot++) {
    if (f_IsNameSlotShown[slot]) {
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      gDPSetCycleType(gfx++, G_CYC_1CYCLE);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
      gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      gDPPipeSync(gfx++);
      gDPSetColorDither(gfx++, G_CD_DISABLE);
      gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
      gDPPipeSync(gfx++);
      gDPSetTexturePersp(gfx++, G_TP_NONE);
      gDPPipeSync(gfx++);
      gDPSetTextureFilter(gfx++, G_TF_BILERP);
      gDPPipeSync(gfx++);
      gDPSetAlphaCompare(gfx++, G_AC_NONE);
      gDPPipeSync(gfx++);
      gDPSetTextureLUT(gfx++, G_TT_RGBA16);
      gDPLoadTLUT_pal256(
          gfx++, TEXTURE_ADDRESS(f_NameSlotImages[slot] + IMAGE_HEADER_SIZE));
      gDPPipeSync(gfx++);
      for (strip = 0; strip != NAME_LOGO_STRIP_COUNT; strip++) {
        texelsOffset = LARGE_PALETTED_TEXELS_OFFSET +
                       strip * SLOT_LOGO.width * NAME_LOGO_STRIP_ROWS;
        stripHeight = SLOT_LOGO.shownHeight / NAME_LOGO_STRIP_COUNT;
        xEdges[0] = SLOT_LOGO.x - SLOT_LOGO.shownWidth / 2;
        xEdges[1] = SLOT_LOGO.x + SLOT_LOGO.shownWidth / 2;
        yEdges[0] = SLOT_LOGO.y - SLOT_LOGO.shownHeight / 2;
        yEdges[1] = yEdges[0] + stripHeight;
        yEdges[2] = yEdges[1] + stripHeight;
        yEdges[3] = yEdges[2] + stripHeight;
        yEdges[4] = yEdges[3] + stripHeight;
        if (f_NameSlotSteps[slot] < NAME_LOGO_FLIP_UP) {
          s32 drawnWidth;
          s32 drawnHeight;

          gDPLoadTextureBlock(
              gfx++, TEXTURE_ADDRESS(f_NameSlotImages[slot] + texelsOffset),
              G_IM_FMT_CI, G_IM_SIZ_8b, SLOT_LOGO.width,
              SLOT_LOGO.height / NAME_LOGO_STRIP_COUNT, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP, G_TX_NOMASK,
              NAME_LOGO_STRIP_ROWS_SHIFT, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);
          drawnWidth = xEdges[1] - xEdges[0];
          if (drawnWidth <= 0) {
            drawnWidth = 1;
          }
          drawnHeight = yEdges[NAME_LOGO_STRIP_COUNT - strip] -
                        yEdges[NAME_LOGO_STRIP_COUNT - 1 - strip];
          if (drawnHeight <= 0) {
            drawnHeight = 1;
          }
          gSPScisTextureRectangle(
              gfx++, xEdges[0] << 2,
              yEdges[NAME_LOGO_STRIP_COUNT - 1 - strip] << 2, xEdges[1] << 2,
              yEdges[NAME_LOGO_STRIP_COUNT - strip] << 2, G_TX_RENDERTILE, 0,
              NAME_LOGO_STRIP_ROWS << 5, (SLOT_LOGO.width << 10) / drawnWidth,
              (SLOT_LOGO.height << 8) / drawnHeight);
        } else {
          s32 drawnWidth;
          s32 drawnHeight;

          gDPLoadTextureBlock(
              gfx++, TEXTURE_ADDRESS(f_NameSlotImages[slot] + texelsOffset),
              G_IM_FMT_CI, G_IM_SIZ_8b, SLOT_LOGO.width,
              SLOT_LOGO.height / NAME_LOGO_STRIP_COUNT, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
              G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);
          drawnWidth = xEdges[1] - xEdges[0];
          if (drawnWidth <= 0) {
            drawnWidth = 1;
          }
          drawnHeight = yEdges[strip + 1] - yEdges[strip];
          if (drawnHeight <= 0) {
            drawnHeight = 1;
          }
          gSPScisTextureRectangle(gfx++, xEdges[0] << 2, yEdges[strip] << 2,
                                  xEdges[1] << 2, yEdges[strip + 1] << 2,
                                  G_TX_RENDERTILE, 0, 0,
                                  (SLOT_LOGO.width << 10) / drawnWidth,
                                  (SLOT_LOGO.height << 8) / drawnHeight);
        }
        gDPPipeSync(gfx++);
      }
    }
  }
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", DrawNameLogos);
#endif

/* Steps the shown name logos' animations: each unfolds, flips over, then
 * stays */
void AnimateNameLogos(void) {
  s32 slot;

  for (slot = 0; slot != OPENING_NAME_SLOT_COUNT; slot++) {
    if (!f_IsNameSlotShown[slot]) {
      continue;
    }
    if (f_NameSlotSteps[slot] < NAME_LOGO_FLIP_DOWN) {
      f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownWidth +=
          f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].width / 8;
      f_NameSlotSteps[slot]++;
      if (f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownWidth >
          f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].width) {
        f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownWidth =
            f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].width;
      }
      if (f_NameSlotSteps[slot] == NAME_LOGO_UNFOLD + 8) {
        f_NameSlotSteps[slot] = NAME_LOGO_FLIP_DOWN;
      }
    } else if (f_NameSlotSteps[slot] < NAME_LOGO_FLIP_UP) {
      f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight -=
          f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].height / 4;
      f_NameSlotSteps[slot]++;
      if (f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight <= 0) {
        f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight = 1;
      }
      if (f_NameSlotSteps[slot] == NAME_LOGO_FLIP_DOWN + 4) {
        f_NameSlotSteps[slot] = NAME_LOGO_FLIP_UP;
      }
    } else if (f_NameSlotSteps[slot] < NAME_LOGO_STILL) {
      f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight +=
          f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].height / 4;
      f_NameSlotSteps[slot]++;
      if (f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight >
          f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].height) {
        f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight =
            f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].height;
      }
      if (f_NameSlotSteps[slot] == NAME_LOGO_FLIP_UP + 4) {
        f_NameSlotSteps[slot] = NAME_LOGO_STILL;
      }
    } else {
      f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownWidth =
          f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].width;
      f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].shownHeight =
          f_OpeningNameLogos[f_NameSlotLogoIndices[slot]].height;
    }
  }
}

/* Loads ball image ballIndex into slot and resets the slot */
void LoadOpeningBall(s32 ballIndex, s32 slot) {
  EncodedAsset asset;

  OpenEncodedAsset(f_OpeningBallAssets[ballIndex], &asset);
  ReadEncodedAsset(f_OpeningBallImages[slot], &asset);
  f_OpeningBallStates[slot] = OPENING_BALL_HIDDEN;
  f_OpeningBallScales[slot] = OPENING_BALL_NORMAL_SCALE;
}

/* f_LogoBallImages holds three 64x64 frames of the ball rolling */
#define LOGO_BALL_FRAME_COUNT 3
#define LOGO_BALL_FRAME_SIZE \
  (OPENING_BALL_IMAGE_WIDTH * OPENING_BALL_IMAGE_WIDTH)
/* The logo ball shows its next frame each time it rolls further than this */
#define LOGO_BALL_FRAME_ANGLE 4.0f
/* The logo ball's starting turn, in degrees */
#define LOGO_BALL_START_ROLL (-40.0f)
#define LOGO_BALL_START_PITCH 0.0f
#define LOGO_BALL_START_YAW (-30.0f)
#define LOGO_MARIO_IMAGE_SIZE 24

/* Every ball model is scaled by this much */
#define OPENING_BALL_MODEL_SCALE 0.7f
/* How far below the ground an in-air or large ball's shadow sits */
#define OPENING_BALL_IN_AIR_SHADOW_DROP 0.25f
#define OPENING_BALL_LARGE_SHADOW_DROP 1.0f
#define PROJECTED_BALL_LARGE_SHADOW_DROP 2.0f
/* How far above the ground a large ball sits */
#define OPENING_BALL_LARGE_LIFT 0.5f
/* The Mario image turns about a point this far below the logo ball */
#define LOGO_BALL_MARIO_DROP 1.5f
#define PROJECTED_LOGO_BALL_MARIO_DROP 1.7f
#define LOGO_BALL_MARIO_SCALE 0.55f
#define PROJECTED_LOGO_BALL_MARIO_SCALE 0.6f
/* The Mario image's offset from the point it turns about, before scaling */
#define LOGO_BALL_MARIO_OFFSET_Y (-0.3f)
#define PROJECTED_LOGO_BALL_MARIO_OFFSET_Y (-0.38f)

/* A projected ball's width on the screen, in pixels, for each step of scale */
#define PROJECTED_BALL_WIDTH_PER_SCALE 1.5f
/* So that it stays in sight, an in-air ball is at least this tall */
#define PROJECTED_BALL_MIN_HALF_SIZE 2
/* Balls this big or bigger aren't drawn */
#define PROJECTED_BALL_HALF_SIZE_LIMIT 256
/* The balls other than the logo ball are drawn this many pixels lower */
#define PROJECTED_BALL_DROP 2

extern Vtx f_OpeningBallStripVertices[OPENING_BALL_STRIP_COUNT][4];
extern Vtx f_OpeningBallShadowVertices[4];
extern Mtx f_OpeningBallMatrices[OPENING_BALL_SLOT_COUNT][DRAW_BUFFER_COUNT];
extern Mtx f_OpeningBallScaleMatrices[OPENING_BALL_SLOT_COUNT];
extern Mtx f_OpeningBallShadowMatrices[OPENING_BALL_SLOT_COUNT]
                                      [DRAW_BUFFER_COUNT];

/* The logo ball's roll, from its position's change each frame */
extern s32 f_ShouldResetLogoBallRoll;
extern Mat4 f_LogoBallRoll;
extern s32 f_HasLogoBallLastPosition;
extern Vec3 f_LogoBallLastPosition;
extern s32 f_LogoBallFrameIndex;
extern Vtx f_LogoBallMarioVertices[4];
extern Mtx f_LogoBallMarioTranslations[DRAW_BUFFER_COUNT];
extern Mtx f_LogoBallMarioRolls[DRAW_BUFFER_COUNT];
extern Mtx f_LogoBallMarioScale;
extern Mtx f_LogoBallMarioOffset;

/* The current projected ball's position and rectangle on the screen */
extern Vec3 f_ProjectedBallPosition;
extern s32 f_ProjectedBallLeft;
extern s32 f_ProjectedBallTop;
extern s32 f_ProjectedBallRight;
extern s32 f_ProjectedBallBottom;
extern s32 f_ProjectedBallTexelStep;
/* 1 when the ball's size is odd, for the pixel that halving it loses */
extern s32 f_ProjectedBallExtraPixel;
extern Vtx f_ProjectedShadowVertices[4];
extern Mtx f_ProjectedShadowMatrices[DRAW_BUFFER_COUNT]
                                    [OPENING_BALL_SLOT_COUNT];
extern Mtx f_ProjectedShadowScaleMatrices[OPENING_BALL_SLOT_COUNT];

/* The same as the logo ball's roll, kept apart for the projected logo ball */
extern s32 f_ShouldResetProjectedLogoBallRoll;
extern Mat4 f_ProjectedLogoBallRoll;
extern s32 f_HasProjectedLogoBallLastPosition;
extern Vec3 f_ProjectedLogoBallLastPosition;
extern s32 f_ProjectedLogoBallFrameIndex;
extern Vtx f_ProjectedLogoBallMarioVertices[4];
extern Mtx f_ProjectedLogoBallMarioTranslations[DRAW_BUFFER_COUNT];
extern Mtx f_ProjectedLogoBallMarioRolls[DRAW_BUFFER_COUNT];
extern Mtx f_ProjectedLogoBallMarioScale;
extern Mtx f_ProjectedLogoBallMarioOffset;

#ifdef NON_MATCHING
/*
 * Draws the balls in the opening's ball slots, each with a shadow on the
 * ground. Normally a ball is a billboard in the scene, drawn in strips. While
 * the logo ball is out, every ball is instead projected to the screen and
 * drawn as texture rectangles. The logo ball rolls with its movement, with
 * the Mario image turning on it.
 *
 * Differs from the original only in stack slot choices: two pairs of spilled
 * display list pointers swap slots before the Mario image is drawn.
 */
void DrawOpeningBalls(Gfx** displayListHead, CameraMatrices* matrices) {
  Vec3 movement;
  Mat4 turn;
  Vec3 position;
  ScreenPoint ballPoint;
  Mat4 projectedTurn;
  Gfx* gfx;
  s32 slot;
  s32 stripIndex;
  s32 halfSize;
  f32 screenScale;
  f32 rollAngle;

  if (f_IsLogoBallSlot[LOGO_BALL_SLOT] == 0) {
    gfx = *displayListHead;
    for (slot = 0; slot < OPENING_BALL_SLOT_COUNT; slot++) {
      if (f_OpeningBallStates[slot] != OPENING_BALL_HIDDEN) {
        position[0] = f_OpeningBallPositions[slot][0];
        position[2] = f_OpeningBallPositions[slot][2];
        position[1] = GetGroundHeight(position[0] * WORLD_TO_COURSE_SCALE,
                                      position[2] * WORLD_TO_COURSE_SCALE) *
                      COURSE_TO_WORLD_SCALE;
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        if (f_OpeningBallStates[slot] == OPENING_BALL_IN_AIR) {
          guTranslate(&f_OpeningBallShadowMatrices[slot][g_DrawBufferIndex],
                      position[0],
                      position[1] - OPENING_BALL_IN_AIR_SHADOW_DROP,
                      position[2]);
        } else if (f_OpeningBallScales[slot] == OPENING_BALL_LARGE_SCALE) {
          guTranslate(&f_OpeningBallShadowMatrices[slot][g_DrawBufferIndex],
                      position[0], position[1] - OPENING_BALL_LARGE_SHADOW_DROP,
                      position[2]);
        } else {
          guTranslate(&f_OpeningBallShadowMatrices[slot][g_DrawBufferIndex],
                      position[0], position[1], position[2]);
        }
        guScale(&f_OpeningBallScaleMatrices[slot],
                f_OpeningBallScales[slot] * OPENING_BALL_MODEL_SCALE,
                f_OpeningBallScales[slot] * OPENING_BALL_MODEL_SCALE,
                f_OpeningBallScales[slot] * OPENING_BALL_MODEL_SCALE);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        gDPSetCombineLERP(gfx++, 0, 0, 0, 0, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0,
                          0, TEXEL0, 0, PRIMITIVE, 0);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
        gDPSetBlendColor(gfx++, 255, 255, 255, 16);
        gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 127);
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        gDPPipeSync(gfx++);
        gDPSetColorDither(gfx++, G_CD_MAGICSQ);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_PERSP);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPLoadTextureBlock(
            gfx++, TEXTURE_ADDRESS(g_ShadowTexture + IMAGE_HEADER_SIZE),
            G_IM_FMT_I, G_IM_SIZ_8b, SHADOW_TEXTURE_SIZE, SHADOW_TEXTURE_SIZE,
            0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gDPSetTextureLOD(gfx++, G_TL_TILE);
        gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
        gSPLoadGeometryMode(gfx++, 0);
        gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
        gSPMatrix(gfx++,
                  OS_K0_TO_PHYSICAL(
                      &f_OpeningBallShadowMatrices[slot][g_DrawBufferIndex]),
                  G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_OpeningBallScaleMatrices[slot]),
                  G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        gSPVertex(gfx++, f_OpeningBallShadowVertices, 4, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
        gDPPipeSync(gfx++);
        if (f_OpeningBallStates[slot] == OPENING_BALL_IN_AIR) {
          position[1] = f_OpeningBallPositions[slot][1];
        }
        if (f_OpeningBallScales[slot] == OPENING_BALL_LARGE_SCALE) {
          position[1] += OPENING_BALL_LARGE_LIFT;
        }
        guTranslate(&f_OpeningBallMatrices[slot][g_DrawBufferIndex],
                    position[0], position[1], position[2]);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        gDPPipeSync(gfx++);
        gDPSetColorDither(gfx++, G_CD_MAGICSQ);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_PERSP);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPPipeSync(gfx++);
        gDPSetTextureLOD(gfx++, G_TL_TILE);
        gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
        if (f_IsLogoBallSlot[slot]) {
          gDPLoadTLUT_pal256(
              gfx++, TEXTURE_ADDRESS(f_LogoBallImages + IMAGE_HEADER_SIZE));
        } else {
          gDPLoadTLUT_pal256(gfx++, TEXTURE_ADDRESS(f_OpeningBallImages[slot] +
                                                    IMAGE_HEADER_SIZE));
        }
        for (stripIndex = 0; stripIndex < OPENING_BALL_STRIP_COUNT;
             stripIndex++) {
          gDPPipeSync(gfx++);
          gSPLoadGeometryMode(gfx++, 0);
          gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
          gSPMatrix(gfx++,
                    OS_K0_TO_PHYSICAL(
                        &f_OpeningBallMatrices[slot][g_DrawBufferIndex]),
                    G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
          gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&matrices->billboard),
                    G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
          gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_OpeningBallScaleMatrices[slot]),
                    G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
          if (f_IsLogoBallSlot[slot]) {
            gDPLoadTextureBlock(
                gfx++,
                TEXTURE_ADDRESS(f_LogoBallImages +
                                LARGE_PALETTED_TEXELS_OFFSET +
                                (stripIndex * OPENING_BALL_STRIP_SIZE +
                                 f_LogoBallFrameIndex * LOGO_BALL_FRAME_SIZE)),
                G_IM_FMT_CI, G_IM_SIZ_8b, OPENING_BALL_IMAGE_WIDTH,
                OPENING_BALL_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                G_TX_NOLOD);
          } else {
            gDPLoadTextureBlock(
                gfx++,
                TEXTURE_ADDRESS(f_OpeningBallImages[slot] +
                                LARGE_PALETTED_TEXELS_OFFSET +
                                stripIndex * OPENING_BALL_STRIP_SIZE),
                G_IM_FMT_CI, G_IM_SIZ_8b, OPENING_BALL_IMAGE_WIDTH,
                OPENING_BALL_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                G_TX_NOLOD);
          }
          gDPPipeSync(gfx++);
          gSPVertex(gfx++, f_OpeningBallStripVertices[stripIndex], 4, 0);
          gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        }
        if (f_IsLogoBallSlot[slot]) {
          if (f_ShouldResetLogoBallRoll) {
            guMtxIdentF(f_LogoBallRoll);
            f_ShouldResetLogoBallRoll = 0;
            guRotateRPYF(f_LogoBallRoll, LOGO_BALL_START_ROLL,
                         LOGO_BALL_START_PITCH, LOGO_BALL_START_YAW);
          }
          if (f_HasLogoBallLastPosition) {
            movement[0] =
                f_OpeningBallPositions[slot][0] - f_LogoBallLastPosition[0];
            movement[1] =
                f_OpeningBallPositions[slot][1] - f_LogoBallLastPosition[1];
            movement[2] =
                f_OpeningBallPositions[slot][2] - f_LogoBallLastPosition[2];
          } else {
            f_HasLogoBallLastPosition = 1;
            movement[0] = 0.0f;
            movement[1] = 0.0f;
            movement[2] = 0.0f;
          }
          rollAngle =
              sqrtf(movement[0] * movement[0] + movement[1] * movement[1] +
                    movement[2] * movement[2]);
          rollAngle *= (f32)RADIANS_TO_DEGREES;
          f_LogoBallLastPosition[0] = f_OpeningBallPositions[slot][0];
          f_LogoBallLastPosition[1] = f_OpeningBallPositions[slot][1];
          f_LogoBallLastPosition[2] = f_OpeningBallPositions[slot][2];
          rollAngle = fabsf(rollAngle);
          if (rollAngle > LOGO_BALL_FRAME_ANGLE) {
            f_LogoBallFrameIndex++;
            if (f_LogoBallFrameIndex == LOGO_BALL_FRAME_COUNT) {
              f_LogoBallFrameIndex = 0;
            }
          }
          guScale(&f_LogoBallMarioScale, LOGO_BALL_MARIO_SCALE,
                  LOGO_BALL_MARIO_SCALE, LOGO_BALL_MARIO_SCALE);
          if (movement[2] != 0.0f || movement[0] != 0.0f) {
            guRotateF(turn, rollAngle, -movement[2], 0.0f, movement[0]);
            guMtxCatF(f_LogoBallRoll, turn, f_LogoBallRoll);
            ConvertMatrixToFixed(f_LogoBallRoll,
                                 &f_LogoBallMarioRolls[g_DrawBufferIndex]);
          }
          guTranslate(&f_LogoBallMarioOffset, 0.0f, LOGO_BALL_MARIO_OFFSET_Y,
                      0.0f);
          guTranslate(&f_LogoBallMarioTranslations[g_DrawBufferIndex],
                      f_OpeningBallPositions[slot][0],
                      f_OpeningBallPositions[slot][1] - LOGO_BALL_MARIO_DROP,
                      f_OpeningBallPositions[slot][2]);
          gDPPipeSync(gfx++);
          gDPSetCycleType(gfx++, G_CYC_1CYCLE);
          gDPPipeSync(gfx++);
          gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
          gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
          gDPPipeSync(gfx++);
          gDPSetAlphaDither(gfx++, G_AD_PATTERN);
          gDPPipeSync(gfx++);
          gDPSetColorDither(gfx++, G_CD_MAGICSQ);
          gDPPipeSync(gfx++);
          gDPSetTexturePersp(gfx++, G_TP_PERSP);
          gDPPipeSync(gfx++);
          gDPSetTextureLUT(gfx++, G_TT_RGBA16);
          gDPLoadTLUT_pal256(
              gfx++, TEXTURE_ADDRESS(f_LogoMarioImage + IMAGE_HEADER_SIZE));
          gDPLoadTextureBlock(
              gfx++,
              TEXTURE_ADDRESS(f_LogoMarioImage + LARGE_PALETTED_TEXELS_OFFSET),
              G_IM_FMT_CI, G_IM_SIZ_8b, LOGO_MARIO_IMAGE_SIZE,
              LOGO_MARIO_IMAGE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
              G_TX_NOLOD);
          gDPPipeSync(gfx++);
          gDPSetTextureLOD(gfx++, G_TL_TILE);
          gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
          gSPLoadGeometryMode(gfx++, 0);
          gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_FRONT | G_SHADING_SMOOTH);
          gSPMatrix(gfx++,
                    OS_K0_TO_PHYSICAL(
                        &f_LogoBallMarioTranslations[g_DrawBufferIndex]),
                    G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
          gSPMatrix(gfx++,
                    OS_K0_TO_PHYSICAL(&f_LogoBallMarioRolls[g_DrawBufferIndex]),
                    G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
          gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_LogoBallMarioScale),
                    G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
          gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_LogoBallMarioOffset),
                    G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
          if (f_OpeningScene >= OPENING_SCENE_LOGO_BALL_MARIO &&
              f_OpeningSceneFramesLeft < LOGO_BALL_MARIO_FRAMES_LEFT) {
            gSPVertex(gfx++, f_LogoBallMarioVertices, 4, 0);
            gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
          }
          gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
          gDPPipeSync(gfx++);
        }
        gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
        gDPPipeSync(gfx++);
      }
    }
    *displayListHead = gfx;
  } else {
    Gfx* gfx;
    s32 stripIndex;

    gfx = *displayListHead;
    for (slot = 0; slot != OPENING_BALL_SLOT_COUNT; slot++) {
      if (f_OpeningBallStates[slot] != OPENING_BALL_HIDDEN) {
        f_ProjectedBallPosition[0] = f_OpeningBallPositions[slot][0];
        f_ProjectedBallPosition[2] = f_OpeningBallPositions[slot][2];
        if (f_OpeningBallStates[slot] == OPENING_BALL_IN_AIR) {
          f_ProjectedBallPosition[1] = f_OpeningBallPositions[slot][1];
        } else {
          f_ProjectedBallPosition[1] =
              GetGroundHeight(
                  f_ProjectedBallPosition[0] * WORLD_TO_COURSE_SCALE,
                  f_ProjectedBallPosition[2] * WORLD_TO_COURSE_SCALE) *
              COURSE_TO_WORLD_SCALE;
        }
        if (ProjectPoint(f_ProjectedBallPosition[0], f_ProjectedBallPosition[1],
                         f_ProjectedBallPosition[2], &ballPoint) < 0) {
          screenScale = ballPoint.scaleX;
          if (screenScale < 0.0f) {
            screenScale = -screenScale;
          }
          halfSize = screenScale * (PROJECTED_BALL_WIDTH_PER_SCALE *
                                    f_OpeningBallScales[slot]);
          if (halfSize & 1) {
            f_ProjectedBallExtraPixel = 1;
          } else {
            f_ProjectedBallExtraPixel = 0;
          }
          halfSize /= 2;
          f_ProjectedBallLeft = ballPoint.x - halfSize;
          f_ProjectedBallRight = ballPoint.x + halfSize;
          if (f_OpeningBallStates[slot] == OPENING_BALL_IN_AIR) {
            if (halfSize < PROJECTED_BALL_MIN_HALF_SIZE) {
              halfSize = PROJECTED_BALL_MIN_HALF_SIZE;
            }
          }
          f_ProjectedBallPosition[1] =
              GetGroundHeight(
                  f_ProjectedBallPosition[0] * WORLD_TO_COURSE_SCALE,
                  f_ProjectedBallPosition[2] * WORLD_TO_COURSE_SCALE) *
              COURSE_TO_WORLD_SCALE;
          if (f_OpeningBallScales[slot] == OPENING_BALL_LARGE_SCALE) {
            f_ProjectedBallPosition[1] -= PROJECTED_BALL_LARGE_SHADOW_DROP;
          }
          if (f_OpeningBallStates[slot] == OPENING_BALL_IN_AIR) {
            guTranslate(
                &f_ProjectedShadowMatrices[g_DrawBufferIndex][slot],
                f_ProjectedBallPosition[0],
                f_ProjectedBallPosition[1] - OPENING_BALL_IN_AIR_SHADOW_DROP,
                f_ProjectedBallPosition[2]);
          } else {
            guTranslate(&f_ProjectedShadowMatrices[g_DrawBufferIndex][slot],
                        f_ProjectedBallPosition[0], f_ProjectedBallPosition[1],
                        f_ProjectedBallPosition[2]);
          }
          guScale(&f_ProjectedShadowScaleMatrices[slot],
                  f_OpeningBallScales[slot], f_OpeningBallScales[slot],
                  f_OpeningBallScales[slot]);
          gDPPipeSync(gfx++);
          gDPPipeSync(gfx++);
          gDPSetCycleType(gfx++, G_CYC_1CYCLE);
          gDPPipeSync(gfx++);
          gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
          gDPSetCombineLERP(gfx++, 0, 0, 0, 0, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0,
                            0, TEXEL0, 0, PRIMITIVE, 0);
          gDPPipeSync(gfx++);
          gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
          gDPSetBlendColor(gfx++, 255, 255, 255, 16);
          gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 127);
          gDPPipeSync(gfx++);
          gDPSetAlphaDither(gfx++, G_AD_PATTERN);
          gDPPipeSync(gfx++);
          gDPSetColorDither(gfx++, G_CD_MAGICSQ);
          gDPPipeSync(gfx++);
          gDPSetTexturePersp(gfx++, G_TP_PERSP);
          gDPPipeSync(gfx++);
          gDPSetTextureLUT(gfx++, G_TT_NONE);
          gDPLoadTextureBlock(
              gfx++, TEXTURE_ADDRESS(g_ShadowTexture + IMAGE_HEADER_SIZE),
              G_IM_FMT_I, G_IM_SIZ_8b, SHADOW_TEXTURE_SIZE, SHADOW_TEXTURE_SIZE,
              0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
              G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);
          gDPSetTextureLOD(gfx++, G_TL_TILE);
          gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
          gSPLoadGeometryMode(gfx++, 0);
          gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
          gSPMatrix(gfx++,
                    OS_K0_TO_PHYSICAL(
                        &f_ProjectedShadowMatrices[g_DrawBufferIndex][slot]),
                    G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
          gSPMatrix(gfx++,
                    OS_K0_TO_PHYSICAL(&f_ProjectedShadowScaleMatrices[slot]),
                    G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
          gSPVertex(gfx++, f_ProjectedShadowVertices, 4, 0);
          gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
          gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
          gDPPipeSync(gfx++);
          gDPPipeSync(gfx++);
          gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
          gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
          gDPPipeSync(gfx++);
          gDPSetColorDither(gfx++, G_CD_MAGICSQ);
          gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
          gDPPipeSync(gfx++);
          gDPSetTexturePersp(gfx++, G_TP_NONE);
          gDPPipeSync(gfx++);
          gDPSetTextureFilter(gfx++, G_TF_BILERP);
          gDPPipeSync(gfx++);
          gDPSetAlphaCompare(gfx++, G_AC_NONE);
          gDPPipeSync(gfx++);
          gDPSetTextureLUT(gfx++, G_TT_RGBA16);
          if (f_IsLogoBallSlot[slot]) {
            gDPLoadTLUT_pal256(
                gfx++, TEXTURE_ADDRESS(f_LogoBallImages + IMAGE_HEADER_SIZE));
          } else {
            gDPLoadTLUT_pal256(
                gfx++,
                TEXTURE_ADDRESS(f_OpeningBallImages[slot] + IMAGE_HEADER_SIZE));
          }
          gDPPipeSync(gfx++);
          for (stripIndex = 0; stripIndex != OPENING_BALL_STRIP_COUNT;
               stripIndex++) {
            if (f_IsLogoBallSlot[slot]) {
              gDPLoadTextureBlock(
                  gfx++,
                  TEXTURE_ADDRESS(
                      f_LogoBallImages + LARGE_PALETTED_TEXELS_OFFSET +
                      (stripIndex * OPENING_BALL_STRIP_SIZE +
                       f_ProjectedLogoBallFrameIndex * LOGO_BALL_FRAME_SIZE)),
                  G_IM_FMT_CI, G_IM_SIZ_8b, OPENING_BALL_IMAGE_WIDTH,
                  OPENING_BALL_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                  G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                  G_TX_NOLOD, G_TX_NOLOD);
            } else {
              gDPLoadTextureBlock(
                  gfx++,
                  TEXTURE_ADDRESS(f_OpeningBallImages[slot] +
                                  LARGE_PALETTED_TEXELS_OFFSET +
                                  stripIndex * OPENING_BALL_STRIP_SIZE),
                  G_IM_FMT_CI, G_IM_SIZ_8b, OPENING_BALL_IMAGE_WIDTH,
                  OPENING_BALL_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                  G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                  G_TX_NOLOD, G_TX_NOLOD);
            }
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            if (halfSize > 0 && halfSize < PROJECTED_BALL_HALF_SIZE_LIMIT) {
              if (f_IsLogoBallSlot[slot]) {
                f_ProjectedBallLeft =
                    ballPoint.x - halfSize - f_ProjectedBallExtraPixel;
                f_ProjectedBallRight = ballPoint.x + halfSize;
                f_ProjectedBallTop =
                    ballPoint.y -
                    halfSize * (OPENING_BALL_STRIP_COUNT - stripIndex) -
                    f_ProjectedBallExtraPixel;
                f_ProjectedBallBottom =
                    ballPoint.y + stripIndex * halfSize - halfSize;
              } else {
                f_ProjectedBallLeft =
                    ballPoint.x - halfSize - f_ProjectedBallExtraPixel;
                f_ProjectedBallRight = ballPoint.x + halfSize;
                f_ProjectedBallTop =
                    ballPoint.y -
                    halfSize * (OPENING_BALL_STRIP_COUNT - stripIndex) -
                    f_ProjectedBallExtraPixel + PROJECTED_BALL_DROP;
                f_ProjectedBallBottom = ballPoint.y + stripIndex * halfSize -
                                        halfSize + PROJECTED_BALL_DROP;
              }
              f_ProjectedBallTexelStep = OPENING_BALL_STRIP_HEIGHT *
                                         TEXEL_STEP_ONE /
                                         (halfSize + f_ProjectedBallExtraPixel);
              gSPScisTextureRectangle(
                  gfx++, f_ProjectedBallLeft << 2, f_ProjectedBallTop << 2,
                  f_ProjectedBallRight << 2, f_ProjectedBallBottom << 2,
                  G_TX_RENDERTILE, 0, 0, f_ProjectedBallTexelStep,
                  f_ProjectedBallTexelStep);
            }
            gDPPipeSync(gfx++);
          }
          if (f_IsLogoBallSlot[slot]) {
            if (f_ShouldResetProjectedLogoBallRoll) {
              guMtxIdentF(f_ProjectedLogoBallRoll);
              f_ShouldResetProjectedLogoBallRoll = 0;
              guRotateRPYF(f_ProjectedLogoBallRoll, LOGO_BALL_START_ROLL,
                           LOGO_BALL_START_PITCH, LOGO_BALL_START_YAW);
            }
            if (f_HasProjectedLogoBallLastPosition) {
              movement[0] = f_OpeningBallPositions[slot][0] -
                            f_ProjectedLogoBallLastPosition[0];
              movement[1] = f_OpeningBallPositions[slot][1] -
                            f_ProjectedLogoBallLastPosition[1];
              movement[2] = f_OpeningBallPositions[slot][2] -
                            f_ProjectedLogoBallLastPosition[2];
            } else {
              f_HasProjectedLogoBallLastPosition = 1;
              movement[0] = 0.0f;
              movement[1] = 0.0f;
              movement[2] = 0.0f;
            }
            rollAngle =
                sqrtf(movement[0] * movement[0] + movement[1] * movement[1] +
                      movement[2] * movement[2]);
            rollAngle *= (f32)RADIANS_TO_DEGREES;
            f_ProjectedLogoBallLastPosition[0] =
                f_OpeningBallPositions[slot][0];
            f_ProjectedLogoBallLastPosition[1] =
                f_OpeningBallPositions[slot][1];
            f_ProjectedLogoBallLastPosition[2] =
                f_OpeningBallPositions[slot][2];
            rollAngle = fabsf(rollAngle);
            if (rollAngle > LOGO_BALL_FRAME_ANGLE) {
              f_ProjectedLogoBallFrameIndex++;
              if (f_ProjectedLogoBallFrameIndex == LOGO_BALL_FRAME_COUNT) {
                f_ProjectedLogoBallFrameIndex = 0;
              }
            }
            guScale(&f_ProjectedLogoBallMarioScale,
                    PROJECTED_LOGO_BALL_MARIO_SCALE,
                    PROJECTED_LOGO_BALL_MARIO_SCALE,
                    PROJECTED_LOGO_BALL_MARIO_SCALE);
            if (movement[2] != 0.0f || movement[0] != 0.0f) {
              guRotateF(projectedTurn, rollAngle, -movement[2], 0.0f,
                        movement[0]);
              guMtxCatF(f_ProjectedLogoBallRoll, projectedTurn,
                        f_ProjectedLogoBallRoll);
              ConvertMatrixToFixed(
                  f_ProjectedLogoBallRoll,
                  &f_ProjectedLogoBallMarioRolls[g_DrawBufferIndex]);
            }
            guTranslate(&f_ProjectedLogoBallMarioOffset, 0.0f,
                        PROJECTED_LOGO_BALL_MARIO_OFFSET_Y, 0.0f);
            guTranslate(
                &f_ProjectedLogoBallMarioTranslations[g_DrawBufferIndex],
                f_OpeningBallPositions[slot][0],
                f_OpeningBallPositions[slot][1] -
                    PROJECTED_LOGO_BALL_MARIO_DROP,
                f_OpeningBallPositions[slot][2]);
            gDPPipeSync(gfx++);
            gDPSetCycleType(gfx++, G_CYC_1CYCLE);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            gDPPipeSync(gfx++);
            gDPSetAlphaDither(gfx++, G_AD_PATTERN);
            gDPPipeSync(gfx++);
            gDPSetColorDither(gfx++, G_CD_MAGICSQ);
            gDPPipeSync(gfx++);
            gDPSetTexturePersp(gfx++, G_TP_PERSP);
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            gDPLoadTLUT_pal256(
                gfx++, TEXTURE_ADDRESS(f_LogoMarioImage + IMAGE_HEADER_SIZE));
            gDPLoadTextureBlock(gfx++,
                                TEXTURE_ADDRESS(f_LogoMarioImage +
                                                LARGE_PALETTED_TEXELS_OFFSET),
                                G_IM_FMT_CI, G_IM_SIZ_8b, LOGO_MARIO_IMAGE_SIZE,
                                LOGO_MARIO_IMAGE_SIZE, 0,
                                G_TX_NOMIRROR | G_TX_WRAP,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                                G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            gDPPipeSync(gfx++);
            gDPSetTextureLOD(gfx++, G_TL_TILE);
            gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
            gSPLoadGeometryMode(gfx++, 0);
            gSPSetGeometryMode(gfx++,
                               G_SHADE | G_CULL_FRONT | G_SHADING_SMOOTH);
            gSPMatrix(
                gfx++,
                OS_K0_TO_PHYSICAL(
                    &f_ProjectedLogoBallMarioTranslations[g_DrawBufferIndex]),
                G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPMatrix(gfx++,
                      OS_K0_TO_PHYSICAL(
                          &f_ProjectedLogoBallMarioRolls[g_DrawBufferIndex]),
                      G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
            gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_ProjectedLogoBallMarioScale),
                      G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
            gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_ProjectedLogoBallMarioOffset),
                      G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
            gSPVertex(gfx++, f_ProjectedLogoBallMarioVertices, 4, 0);
            gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
            gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
            gDPPipeSync(gfx++);
          }
        }
      }
    }
    *displayListHead = gfx;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", DrawOpeningBalls);
#endif

/* A 64x64 CI8 image of yellow and green lines bursting outwards */
#define ASSET_IMPACT_LINES_IMAGE 0x908

void LoadOpeningImpactLines(void) {
  EncodedAsset asset;

  OpenEncodedAsset(ASSET_IMPACT_LINES_IMAGE, &asset);
  ReadEncodedAsset(D_80212830_ovl_05, &asset);
}

extern s32 f_ImpactLinesFrame;

/* The impact lines' left and bottom edges on the screen, in pixels */
#define IMPACT_LINES_LEFT 206
#define IMPACT_LINES_BOTTOM 88
/* The 64x64 image loads into texture memory in two strips */
#define IMPACT_LINES_SIZE 64
#define IMPACT_LINES_STRIP_HEIGHT 32
#define IMPACT_LINES_STRIP_COUNT 2
/* The lines blink off for a few frames, twice, then stop showing */
#define IMPACT_LINES_FIRST_BLINK_FRAME 3
#define IMPACT_LINES_SECOND_BLINK_FRAME 9
#define IMPACT_LINES_BLINK_FRAME_COUNT 3
#define IMPACT_LINES_FRAME_COUNT 19

/*
 * Draws blinking impact lines around the burst for a few frames, once
 * f_AreImpactLinesShown is set.
 *
 * The original keeps the bottom edge's 88 in one register for both of a
 * strip's edges, where this folds the top edge into a separate constant.
 */
#ifdef NON_MATCHING
void DrawOpeningImpactLines(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 frame;
  s32 stripIndex;
  s32 texelOffset;
  s32 row;
  /* A variable, so GCC does not fold the right edge into the command */
  s32 width = IMPACT_LINES_SIZE;
  s32 bottom;

  if (f_AreImpactLinesShown) {
    frame = f_ImpactLinesFrame++;
    if ((frame < IMPACT_LINES_FIRST_BLINK_FRAME ||
         frame >=
             IMPACT_LINES_FIRST_BLINK_FRAME + IMPACT_LINES_BLINK_FRAME_COUNT) &&
        (frame < IMPACT_LINES_SECOND_BLINK_FRAME ||
         frame >= IMPACT_LINES_SECOND_BLINK_FRAME +
                      IMPACT_LINES_BLINK_FRAME_COUNT) &&
        f_ImpactLinesFrame < IMPACT_LINES_FRAME_COUNT) {
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      gDPSetCycleType(gfx++, G_CYC_1CYCLE);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
      gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      gDPPipeSync(gfx++);
      gDPSetColorDither(gfx++, G_CD_DISABLE);
      gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
      gDPPipeSync(gfx++);
      gDPSetTexturePersp(gfx++, G_TP_NONE);
      gDPPipeSync(gfx++);
      gDPSetTextureFilter(gfx++, G_TF_BILERP);
      gDPPipeSync(gfx++);
      gDPSetAlphaCompare(gfx++, G_AC_NONE);
      gDPPipeSync(gfx++);
      gDPSetTextureLUT(gfx++, G_TT_RGBA16);
      gDPLoadTLUT_pal256(gfx++,
                         (u32)(D_80212830_ovl_05 + IMAGE_HEADER_SIZE) & ~7);
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      texelOffset = LARGE_PALETTED_TEXELS_OFFSET;
      /* The rows count up from the bottom, so the top strip is row 1 */
      row = IMPACT_LINES_STRIP_COUNT - 1;
      for (stripIndex = 0; stripIndex != IMPACT_LINES_STRIP_COUNT;
           stripIndex++) {
        bottom = IMPACT_LINES_BOTTOM - row * IMPACT_LINES_STRIP_HEIGHT;
        gDPLoadTextureBlock(
            gfx++, (u32)(D_80212830_ovl_05 + texelOffset) & ~7, G_IM_FMT_I,
            G_IM_SIZ_8b, IMPACT_LINES_SIZE, IMPACT_LINES_STRIP_HEIGHT, 0,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, IMPACT_LINES_LEFT << 2,
                                (bottom - IMPACT_LINES_STRIP_HEIGHT) << 2,
                                (IMPACT_LINES_LEFT + width) << 2, bottom << 2,
                                G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        gDPPipeSync(gfx++);
        texelOffset += IMPACT_LINES_SIZE * IMPACT_LINES_STRIP_HEIGHT;
        row--;
      }
      *displayListHead = gfx;
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", DrawOpeningImpactLines);
#endif

#define ASSET_LAKITU_IMAGE 0x64B
#define ASSET_LAKITU_TRAIL_IMAGE 0x6AF
/* How far in front of the camera Lakitu's trail is drawn */
#define LAKITU_TRAIL_DEPTH 300.0f

/* Loads Lakitu's images and gathers his trail behind him */
void LoadLakitu(void) {
  EncodedAsset asset;
  s32 pass;
  s32 segmentIndex;

  OpenEncodedAsset(ASSET_LAKITU_IMAGE, &asset);
  ReadEncodedAsset(f_LakituImage, &asset);
  OpenEncodedAsset(ASSET_LAKITU_TRAIL_IMAGE, &asset);
  ReadEncodedAsset(f_LakituTrailImage, &asset);
  /* The game places the trail twice over */
  for (pass = 0; pass < 2; pass++) {
    for (segmentIndex = 0; segmentIndex < LAKITU_TRAIL_SEGMENT_COUNT;
         segmentIndex++) {
      guTranslate(&f_LakituTrailMatrices[segmentIndex],
                  f_LakituScreenPoint[0] - SCREEN_WIDTH / 2,
                  SCREEN_HEIGHT / 2 - f_LakituScreenPoint[1],
                  -LAKITU_TRAIL_DEPTH);
    }
  }
  f_LakituTrailStart[0] = f_LakituScreenPoint[0] - SCREEN_WIDTH;
  f_LakituTrailStart[1] = SCREEN_HEIGHT / 2 - f_LakituScreenPoint[1];
}

extern s32 f_LakituTrailNewestIndex;
extern s32 f_LakituTrailAlpha;
extern Vtx f_LakituTrailVertices[2];

#ifdef NON_MATCHING
#define LAKITU_IMAGE_SIZE 32
#define LAKITU_DRAWN_SIZE 48
/* Texels a pixel when stretching Lakitu's image to its drawn size, in 5.10 */
#define LAKITU_SCALE ((LAKITU_IMAGE_SIZE << 10) / LAKITU_DRAWN_SIZE)
#define LAKITU_TRAIL_IMAGE_WIDTH 48
#define LAKITU_TRAIL_IMAGE_HEIGHT 128
/* Segments whose vertices fit in the vertex buffer at once */
#define LAKITU_TRAIL_BATCH_SEGMENT_COUNT 16
#define LAKITU_TRAIL_DRAWN_SEGMENT_COUNT 31
#define LAKITU_TRAIL_ROWS_PER_SEGMENT 8
/* Texture coordinates, in 10.5, of the trail's right edge and of a segment */
#define LAKITU_TRAIL_RIGHT_S ((LAKITU_TRAIL_IMAGE_WIDTH << 5) - 1)
#define LAKITU_TRAIL_T(segment) \
  (((segment) * LAKITU_TRAIL_ROWS_PER_SEGMENT) << 5)
/* First vertex buffer slot of a segment in the second batch */
#define LAKITU_TRAIL_SECOND_BATCH_SLOT(segment) \
  ((LAKITU_TRAIL_BATCH_SEGMENT_COUNT - 2) * 2 - \
   ((segment) - LAKITU_TRAIL_BATCH_SEGMENT_COUNT) * 2)
#define LAKITU_TRAIL_FADE_STEP 4

/*
 * Draws Lakitu while he flies, with a fading trail of the places he has been.
 *
 * Differs from the original only in stack slots: the pointers of some pairs
 * of a pipe sync and the command after it swap slots, as in the opening's
 * sparks.
 */
void DrawLakitu(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  Vec3 translation;
  Vec3 rotation;
  Mat4 matrix;
  s32 segmentIndex;
  s32 drawnIndex;

  if (f_IsLakituFlying) {
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++,
                       TEXTURE_ADDRESS(f_LakituImage + IMAGE_HEADER_SIZE));
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock(
        gfx++, TEXTURE_ADDRESS(f_LakituImage + LARGE_PALETTED_TEXELS_OFFSET),
        G_IM_FMT_CI, G_IM_SIZ_8b, LAKITU_IMAGE_SIZE, LAKITU_IMAGE_SIZE, 0,
        G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, G_TX_NOMASK,
        G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++, (f_LakituScreenPoint[0] - LAKITU_DRAWN_SIZE / 2) << 2,
        (f_LakituScreenPoint[1] - LAKITU_DRAWN_SIZE / 2) << 2,
        (f_LakituScreenPoint[0] + LAKITU_DRAWN_SIZE / 2) << 2,
        (f_LakituScreenPoint[1] + LAKITU_DRAWN_SIZE / 2) << 2, G_TX_RENDERTILE,
        LAKITU_IMAGE_SIZE << 5, 0, LAKITU_SCALE, LAKITU_SCALE);
    gDPPipeSync(gfx++);
    rotation[0] = 0.0f;
    rotation[1] = 0.0f;
    rotation[2] = atan2(f_LakituTrailStart[0] -
                            (f32)(f_LakituScreenPoint[0] - SCREEN_WIDTH / 2),
                        f_LakituTrailStart[1] -
                            (f32)(SCREEN_HEIGHT / 2 - f_LakituScreenPoint[1]));
    translation[0] = f_LakituScreenPoint[0] - SCREEN_WIDTH / 2;
    translation[1] = SCREEN_HEIGHT / 2 - f_LakituScreenPoint[1];
    translation[2] = -LAKITU_TRAIL_DEPTH;
    BuildRotationMatrix(matrix, rotation, translation);
    ConvertMatrixToFixed(matrix,
                         &f_LakituTrailMatrices[f_LakituTrailNewestIndex]);
    f_LakituTrailStart[0] = f_LakituScreenPoint[0] - SCREEN_WIDTH / 2;
    f_LakituTrailStart[1] = SCREEN_HEIGHT / 2 - f_LakituScreenPoint[1];
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, f_LakituTrailAlpha);
    gDPSetEnvColor(gfx++, 255, 255, 255, 255);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0,
                      0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
    gDPLoadTextureBlock_4b(
        gfx++, TEXTURE_ADDRESS(f_LakituTrailImage + IMAGE_HEADER_SIZE),
        G_IM_FMT_I, LAKITU_TRAIL_IMAGE_WIDTH, LAKITU_TRAIL_IMAGE_HEIGHT, 0,
        G_TX_MIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP, G_TX_NOMASK, 7,
        G_TX_NOLOD, G_TX_NOLOD);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(gfx++);
    segmentIndex = f_LakituTrailNewestIndex;
    for (drawnIndex = 0; drawnIndex != LAKITU_TRAIL_BATCH_SEGMENT_COUNT;
         drawnIndex++) {
      gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_LakituTrailMatrices[segmentIndex]),
                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
      gSPVertex(gfx++, f_LakituTrailVertices, 2, drawnIndex * 2);
      gSPModifyVertex(gfx++, drawnIndex * 2, G_MWO_POINT_ST,
                      LAKITU_TRAIL_T(drawnIndex));
      gSPModifyVertex(
          gfx++, drawnIndex * 2 + 1, G_MWO_POINT_ST,
          (LAKITU_TRAIL_RIGHT_S << 16) + LAKITU_TRAIL_T(drawnIndex));
      segmentIndex--;
      if (segmentIndex < 0) {
        segmentIndex = LAKITU_TRAIL_SEGMENT_COUNT - 1;
      }
    }
    for (drawnIndex = 0; drawnIndex != LAKITU_TRAIL_BATCH_SEGMENT_COUNT - 1;
         drawnIndex++) {
      gSP2Triangles(gfx++, drawnIndex * 2 + 1, drawnIndex * 2,
                    drawnIndex * 2 + 2, 0, drawnIndex * 2 + 1,
                    drawnIndex * 2 + 2, drawnIndex * 2 + 3, 0);
    }
    /*
     * The second batch fills the vertex buffer from the end, keeping the
     * last pair of the first batch in its slots, so the same triangles join
     * up the rest of the trail.
     */
    for (drawnIndex = LAKITU_TRAIL_BATCH_SEGMENT_COUNT;
         drawnIndex != LAKITU_TRAIL_DRAWN_SEGMENT_COUNT; drawnIndex++) {
      gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_LakituTrailMatrices[segmentIndex]),
                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
      gSPVertex(gfx++, f_LakituTrailVertices, 2,
                LAKITU_TRAIL_SECOND_BATCH_SLOT(drawnIndex));
      gSPModifyVertex(gfx++, LAKITU_TRAIL_SECOND_BATCH_SLOT(drawnIndex),
                      G_MWO_POINT_ST, LAKITU_TRAIL_T(drawnIndex));
      gSPModifyVertex(
          gfx++, LAKITU_TRAIL_SECOND_BATCH_SLOT(drawnIndex) + 1, G_MWO_POINT_ST,
          (LAKITU_TRAIL_RIGHT_S << 16) + LAKITU_TRAIL_T(drawnIndex));
      segmentIndex--;
      if (segmentIndex < 0) {
        segmentIndex = LAKITU_TRAIL_SEGMENT_COUNT - 1;
      }
    }
    for (drawnIndex = 0; drawnIndex != LAKITU_TRAIL_BATCH_SEGMENT_COUNT - 1;
         drawnIndex++) {
      gSP2Triangles(gfx++, drawnIndex * 2 + 1, drawnIndex * 2,
                    drawnIndex * 2 + 2, 0, drawnIndex * 2 + 1,
                    drawnIndex * 2 + 2, drawnIndex * 2 + 3, 0);
    }
    gDPPipeSync(gfx++);
    if (++f_LakituTrailNewestIndex >= LAKITU_TRAIL_SEGMENT_COUNT) {
      f_LakituTrailNewestIndex = 0;
    }
    f_LakituTrailAlpha -= LAKITU_TRAIL_FADE_STEP;
    if (f_LakituTrailAlpha < 0) {
      f_LakituTrailAlpha = 0;
    }
    *displayListHead = gfx;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", DrawLakitu);
#endif

/* Flies Lakitu across the screen in loops until it has turned through 540
 * degrees */
#define LAKITU_TURN_STEP 12 /* In degrees a frame */
#define LAKITU_LAST_ANGLE 540
/* In pixels a frame, besides the loops */
#define LAKITU_DRIFT_X 8
#define LAKITU_DRIFT_Y 2
/* The radii of Lakitu's loops, in pixels */
#define LAKITU_LOOP_RADIUS_X 20.0
#define LAKITU_LOOP_RADIUS_Y 16.0

void MoveLakitu(void) {
  f32 angle;

  if (f_IsLakituFlying) {
    angle = DEGREES_TO_RADIANS(f_LakituAngle);
    f_LakituAngle += LAKITU_TURN_STEP;
    if (f_LakituAngle > LAKITU_LAST_ANGLE) {
      f_IsLakituFlying = FALSE;
    }
    f_LakituScreenPoint[0] +=
        LAKITU_DRIFT_X + (s32)(cos(angle) * LAKITU_LOOP_RADIUS_X);
    f_LakituScreenPoint[1] -=
        LAKITU_DRIFT_Y + (s32)(sin(angle) * LAKITU_LOOP_RADIUS_Y);
  }
}

/* A 32x32 CI4 image of a yellow burst */
#define ASSET_BURST_IMAGE 0x93D

void LoadOpeningBurst(void) {
  EncodedAsset asset;

  OpenEncodedAsset(ASSET_BURST_IMAGE, &asset);
  ReadEncodedAsset(D_80212830_ovl_05, &asset);
}

/* The burst's corners on the screen, in pixels */
#define BURST_LEFT 227
#define BURST_TOP 55
#define BURST_RIGHT 255
#define BURST_BOTTOM 81
#define BURST_IMAGE_SIZE 32
/* How many frames the burst shows for, once f_IsBurstShown is set */
#define BURST_FRAME_COUNT 4

/*
 * Draws a yellow burst for a few frames, stretched over the burst's corners.
 *
 * The original divides for the texture's steps at run time, where this folds
 * them into constants. GCC keeps such a division when the size is set the
 * same way in both arms of an if, which CSE cannot see through but
 * cross-jumping later merges, so the original likely chose the size by some
 * condition. Placing such an if here keeps the divisions, but its arms spill
 * the size differently and are not merged.
 */
#ifdef NON_MATCHING
void DrawOpeningBurst(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 left = BURST_LEFT;
  s32 top = BURST_TOP;
  s32 right = BURST_RIGHT;
  s32 bottom = BURST_BOTTOM;

  if (f_IsBurstShown && ++f_BurstFrame < BURST_FRAME_COUNT) {
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, D_80212830_ovl_05 + IMAGE_HEADER_SIZE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++, D_80212830_ovl_05 + PALETTED_TEXELS_OFFSET,
                           G_IM_FMT_CI, BURST_IMAGE_SIZE, BURST_IMAGE_SIZE, 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(gfx++, left << 2, top << 2, right << 2, bottom << 2,
                        G_TX_RENDERTILE, 0, 0,
                        (BURST_IMAGE_SIZE << 10) / (right - left),
                        (BURST_IMAGE_SIZE << 10) / (bottom - top));
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", DrawOpeningBurst);
#endif

/* A 64x64 CI8 image of a plain golf ball */
#define ASSET_SHOT_BALL_IMAGE 0x939

/* Loads the image of the ball that the opening's shot sends flying */
void LoadOpeningShotBall(void) {
  EncodedAsset asset;

  OpenEncodedAsset(ASSET_SHOT_BALL_IMAGE, &asset);
  ReadEncodedAsset(f_ShotBallImage, &asset);
}

/* The ball image is drawn in strips that fit in texture memory */
#define SHOT_BALL_IMAGE_SIZE 64
#define SHOT_BALL_STRIP_HEIGHT 32
#define SHOT_BALL_STRIP_COUNT 2
/*
 * A strip's rows in 10-bit fixed point, less a little so that its last
 * screen row stays within the strip
 */
#define SHOT_BALL_STRIP_ROWS_FIXED 0x7FA0
/* Half the ball's width on the screen, in pixels, for each step of scale */
#define SHOT_BALL_HALF_WIDTH_PER_SCALE 2
/* The ball is projected this far above the look-at point */
#define SHOT_BALL_LIFT 0.1f
/* Beyond this camera distance, the ball is a fading dot */
#define SHOT_BALL_FAR_DISTANCE 400.0f
#define SHOT_BALL_DOT_SIZE 3
/* How much alpha the dot loses for each unit of camera distance */
#define SHOT_BALL_FADE_RATE 0.04
#define SHOT_BALL_OPAQUE_ALPHA 255
/* The shadow's scale for each step of f_ShotBallScale */
#define SHOT_BALL_SHADOW_SCALE 0.5

extern s32 f_ShotBallAlpha;
extern Vtx f_ShotBallShadowVertices[4];
extern Vec3 f_ShotBallPosition;
extern Mtx f_ShotBallShadowTranslation;
extern Mtx f_ShotBallShadowScaling;

#ifdef NON_MATCHING
/*
 * Draws the opening's ball while f_ShotBallScale is set: a shadow on the
 * ground below the look-at point, then the ball image above the point in
 * strips, at f_ShotBallScale times the image's size. When the camera is far
 * away, the ball is instead a dot that fades with distance.
 *
 * Differs from the original only in register and stack slot choices: some
 * pairs of spilled display list pointers swap slots, and the far branch
 * uses other registers.
 */
void DrawOpeningShotBall(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  ScreenPoint ballPoint;
  s32 stripIndex;
  s32 size;
  s32 left;
  s32 top;
  s32 right;
  s32 bottom;

  if (f_ShotBallScale != 0) {
    f_ShotBallPosition[0] =
        f_OpeningShotSimulation.position[0] * COURSE_TO_WORLD_SCALE;
    f_ShotBallPosition[2] =
        f_OpeningShotSimulation.position[2] * COURSE_TO_WORLD_SCALE;
    f_ShotBallPosition[1] =
        f_OpeningShotSimulation.position[1] * COURSE_TO_WORLD_SCALE +
        SHOT_BALL_LIFT;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    if (ProjectPoint(f_ShotBallPosition[0], f_ShotBallPosition[1],
                     f_ShotBallPosition[2], &ballPoint) < 0) {
      gDPPipeSync(gfx++);
      gDPSetDepthSource(gfx++, G_ZS_PRIM);
      gDPSetPrimDepth(gfx++, ballPoint.depth, 0);
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      f_ShotBallPosition[1] =
          GetGroundHeight(f_ShotBallPosition[0] * WORLD_TO_COURSE_SCALE,
                          f_ShotBallPosition[2] * WORLD_TO_COURSE_SCALE) *
          COURSE_TO_WORLD_SCALE;
      guTranslate(&f_ShotBallShadowTranslation, f_ShotBallPosition[0],
                  f_ShotBallPosition[1], f_ShotBallPosition[2]);
      guScale(&f_ShotBallShadowScaling,
              f_ShotBallScale * SHOT_BALL_SHADOW_SCALE,
              f_ShotBallScale * (f32)SHOT_BALL_SHADOW_SCALE,
              f_ShotBallScale * (f32)SHOT_BALL_SHADOW_SCALE);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
      gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0,
                        0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
      gDPSetBlendColor(gfx++, 255, 255, 255, 16);
      gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 127);
      gDPPipeSync(gfx++);
      gDPSetAlphaDither(gfx++, G_AD_PATTERN);
      gDPPipeSync(gfx++);
      gDPSetColorDither(gfx++, G_CD_MAGICSQ);
      gDPPipeSync(gfx++);
      gDPSetTexturePersp(gfx++, G_TP_PERSP);
      gDPPipeSync(gfx++);
      gDPSetTextureLUT(gfx++, G_TT_NONE);
      gDPPipeSync(gfx++);
      gDPSetTextureLOD(gfx++, G_TL_TILE);
      gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
      gDPPipeSync(gfx++);
      gDPLoadTextureBlock(
          gfx++, TEXTURE_ADDRESS(g_ShadowTexture + IMAGE_HEADER_SIZE),
          G_IM_FMT_I, G_IM_SIZ_8b, SHADOW_TEXTURE_SIZE, SHADOW_TEXTURE_SIZE, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_ShotBallShadowTranslation),
                G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
      gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_ShotBallShadowScaling),
                G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
      gSPVertex(gfx++, f_ShotBallShadowVertices, 4, 0);
      gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
      gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
      gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, PRIMITIVE, 0, TEXEL0, 0, 0, 0,
                        0, TEXEL0, PRIMITIVE, 0, TEXEL0, 0);
      gDPPipeSync(gfx++);
      gDPSetColorDither(gfx++, G_CD_DISABLE);
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
      gDPSetBlendColor(gfx++, 255, 255, 255, 2);
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, f_ShotBallAlpha);
      gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
      gDPPipeSync(gfx++);
      gDPSetTexturePersp(gfx++, G_TP_NONE);
      gDPPipeSync(gfx++);
      gDPSetTextureFilter(gfx++, G_TF_BILERP);
      gDPPipeSync(gfx++);
      gDPSetAlphaCompare(gfx++, G_AC_NONE);
      gDPPipeSync(gfx++);
      gDPSetTextureLUT(gfx++, G_TT_RGBA16);
      gDPLoadTLUT_pal256(gfx++,
                         TEXTURE_ADDRESS(f_ShotBallImage + IMAGE_HEADER_SIZE));
      gDPPipeSync(gfx++);
      for (stripIndex = 0; stripIndex != SHOT_BALL_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TEXTURE_ADDRESS(f_ShotBallImage + LARGE_PALETTED_TEXELS_OFFSET +
                            stripIndex * SHOT_BALL_IMAGE_SIZE *
                                SHOT_BALL_STRIP_HEIGHT),
            G_IM_FMT_CI, G_IM_SIZ_8b, SHOT_BALL_IMAGE_SIZE,
            SHOT_BALL_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
            G_TX_NOLOD);
        if (g_CameraEditor.distance > SHOT_BALL_FAR_DISTANCE) {
          size = f_ShotBallScale * SHOT_BALL_HALF_WIDTH_PER_SCALE;
          left = ballPoint.x - size;
          right = left + SHOT_BALL_DOT_SIZE;
          top = ballPoint.y - (SHOT_BALL_STRIP_COUNT - stripIndex) * size;
          bottom = top + SHOT_BALL_DOT_SIZE;
          f_ShotBallAlpha = SHOT_BALL_OPAQUE_ALPHA -
                            (u32)g_CameraEditor.distance * SHOT_BALL_FADE_RATE;
          if (f_ShotBallAlpha < 0) {
            f_ShotBallAlpha = 0;
          }
        } else {
          size = f_ShotBallScale * SHOT_BALL_HALF_WIDTH_PER_SCALE;
          f_ShotBallAlpha = SHOT_BALL_OPAQUE_ALPHA;
          left = ballPoint.x - size;
          right = ballPoint.x + size;
          top = ballPoint.y - (SHOT_BALL_STRIP_COUNT - stripIndex) * size;
          bottom = ballPoint.y + stripIndex * size - size;
        }
        gSPTextureRectangle(gfx++, left << 2, top << 2, right << 2, bottom << 2,
                            G_TX_RENDERTILE, 0, 0,
                            (SHOT_BALL_IMAGE_SIZE << 10) / (right - left),
                            SHOT_BALL_STRIP_ROWS_FIXED / (bottom - top));
        gDPPipeSync(gfx++);
      }
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      gDPSetDepthSource(gfx++, G_ZS_PIXEL);
      gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", DrawOpeningShotBall);
#endif

/* f_LookAtEaseMode values: how EaseOpeningLookAt moves the look-at point */
/* To the shot ball, a tenth of the way a frame */
#define LOOK_AT_EASE_FAST 1
#define LOOK_AT_EASE_SLOW 2 /* To the shot ball, a hundredth of the way */
/* Near f_OpeningBallPositions[1], at a rate that grows to a tenth */
#define LOOK_AT_EASE_SPEEDING_UP 3

/* Eases the camera's look-at point as f_LookAtEaseMode says */
void EaseOpeningLookAt(void) {
  switch (f_LookAtEaseMode) {
    case LOOK_AT_EASE_FAST:
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] +=
          (f_OpeningShotSimulation.position[0] * COURSE_TO_WORLD_SCALE -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0]) *
          0.1f;
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] +=
          (f_OpeningShotSimulation.position[1] * COURSE_TO_WORLD_SCALE -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1]) *
          0.1f;
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] +=
          (f_OpeningShotSimulation.position[2] * COURSE_TO_WORLD_SCALE -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2]) *
          0.1f;
      MeasureEditorCamera();
      break;
    case LOOK_AT_EASE_SLOW:
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] +=
          (f_OpeningShotSimulation.position[0] * COURSE_TO_WORLD_SCALE -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0]) *
          0.01f;
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] +=
          (f_OpeningShotSimulation.position[1] * COURSE_TO_WORLD_SCALE -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1]) *
          0.01f;
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] +=
          (f_OpeningShotSimulation.position[2] * COURSE_TO_WORLD_SCALE -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2]) *
          0.01f;
      MeasureEditorCamera();
      break;
    case LOOK_AT_EASE_SPEEDING_UP:
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] +=
          ((f_OpeningBallPositions[1][0] + 38.1f) -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0]) *
          f_LookAtEaseRate;
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] +=
          ((f_OpeningBallPositions[1][1] - 4.5f) -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1]) *
          f_LookAtEaseRate;
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] +=
          ((f_OpeningBallPositions[1][2] - 4.795f) -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2]) *
          f_LookAtEaseRate;
      MeasureEditorCamera();
      f_LookAtEaseRate += 0.002f;
      if (f_LookAtEaseRate > 0.1f) {
        f_LookAtEaseRate = 0.1f;
      }
      break;
  }
}

/* Loads the title logo's images and lines its letters up, off the screen */
void LoadOpeningLogo(void) {
  EncodedAsset asset;
  s32 letterIndex;

  OpenEncodedAsset(ASSET_LOGO_LETTER_IMAGES, &asset);
  ReadEncodedAsset(f_LogoLetterImages, &asset);
  OpenEncodedAsset(ASSET_LOGO_CLUB_IMAGE, &asset);
  ReadEncodedAsset(f_LogoClubImage, &asset);
  for (letterIndex = 0; letterIndex < LOGO_LETTER_COUNT; letterIndex++) {
    f_LogoLetterStates[letterIndex] = LOGO_LETTER_WAITING;
    f_LogoLetterSpeeds[letterIndex] = -14.0f;
    f_LogoLetters[letterIndex].x = LOGO_LETTER_HIDDEN;
    f_LogoLetters[letterIndex].y = LOGO_LETTER_HIDDEN;
    f_LogoLetterScales[letterIndex][0] = 0.1f;
    f_LogoLetterScales[letterIndex][1] = 0.1f;
    f_LogoLetterSquashFrames[letterIndex] = 0;
  }
  f_LogoLetters[LOGO_LETTER_M].depth = LOGO_M_DEPTH;
  f_LogoLetters[LOGO_LETTER_A].depth = LOGO_A_DEPTH;
  f_LogoLetters[LOGO_LETTER_R].depth = LOGO_R_DEPTH;
  f_LogoLetters[LOGO_LETTER_I].depth = LOGO_I_DEPTH;
  f_LogoLetters[LOGO_LETTER_FIRST_O].depth = LOGO_FIRST_O_DEPTH;
  f_LogoLetters[LOGO_LETTER_G].depth = LOGO_G_DEPTH;
  f_LogoLetters[LOGO_LETTER_SECOND_O].depth = LOGO_SECOND_O_DEPTH;
  f_LogoLetters[LOGO_LETTER_L].depth = LOGO_L_DEPTH;
  f_LogoLetters[LOGO_LETTER_F].depth = LOGO_F_DEPTH;
  OpenEncodedAsset(ASSET_LOGO_MARIO_IMAGE, &asset);
  ReadEncodedAsset(f_LogoMarioImage, &asset);
  OpenEncodedAsset(ASSET_LOGO_BALL_IMAGES, &asset);
  ReadEncodedAsset(f_LogoBallImages, &asset);
  OpenEncodedAsset(ASSET_LOGO_SHINE_IMAGE, &asset);
  ReadEncodedAsset(f_LogoShineImage, &asset);
}

/* What the title logo as a whole is doing */
typedef enum LogoState {
  LOGO_DROPPING_LETTERS,
  LOGO_CLUB_STARTING, /* The last letter has been dropped */
  LOGO_CLUB_CIRCLING, /* Spiralling in to the letters, spinning */
  LOGO_CLUB_SPINNING, /* In place, until it turns upright */
  LOGO_SHINING,
  LOGO_SHRINK_STARTING,
  LOGO_SHRINKING,
  LOGO_SHRUNK
} LogoState;

/* How the screen fade behind the logo follows the logo */
typedef enum LogoFadeStage {
  LOGO_FADE_NONE,
  LOGO_FADE_GRAYING, /* Starts as the first letter is dropped */
  LOGO_FADE_GRAY,    /* Until the shine starts */
  LOGO_FADE_WHITENING,
  LOGO_FADE_WHITE, /* Until the logo shrinks */
  LOGO_FADE_LOGO_OUT
} LogoFadeStage;

/* The gray the screen fades to while the letters fall */
#define LOGO_FADE_GRAY_LEVEL 128
/* The fade's frames left when graying stops, and whitening's length */
#define LOGO_FADE_WHITENING_FRAMES 24
/* About (256 - LOGO_FADE_GRAY_LEVEL) / LOGO_FADE_WHITENING_FRAMES */
#define LOGO_FADE_WHITENING_STEP 5.3333f
#define LOGO_FADE_WHITE_LEVEL 0xFF
/* Keeps the white fade fully opaque */
#define LOGO_FADE_WHITE_DURATION 2
/* The alpha the logo loses each frame as it fades out */
#define LOGO_FADE_OUT_STEP 24
#define LOGO_GRAYING_DURATION 64

/* Where the letters stop falling, on the screen; the L sits higher */
#define LOGO_LETTER_GROUND_Y 125
#define LOGO_LETTER_L_GROUND_Y 122
/* The share of its speed a letter keeps when it bounces */
#define LOGO_LETTER_BOUNCINESS 0.2f
/* Bounces slower than this, in pixels a frame, end in landing */
#define LOGO_LETTER_MIN_BOUNCE_SPEED 1.0f
#define LOGO_LETTER_GRAVITY 0.5f
/* How much a falling letter grows each frame, to full size */
#define LOGO_LETTER_GROWTH 0.04f
#define LOGO_LETTER_SQUASH_FRAMES 10
#define LOGO_LETTER_SQUASH_STEP 0.03f
#define LOGO_LETTER_SQUASHED_FRAMES 2
#define LOGO_LETTER_STRETCH_FRAMES 5
#define LOGO_LETTER_STRETCH_STEP 0.06f

/* What the club spirals around, on the screen */
#define LOGO_CLUB_ORBIT_CENTER_X 165
#define LOGO_CLUB_ORBIT_CENTER_Y 141
/* How far the club strays from its spiral, in pixels */
#define LOGO_CLUB_FIRST_WOBBLE 40.0f
#define LOGO_CLUB_WOBBLE 100.0f
/* How far the club closes in each frame, in pixels */
#define LOGO_CLUB_ORBIT_RADIUS_STEP 4.0f
/* Each frame's turns, in degrees */
#define LOGO_CLUB_ORBIT_ANGLE_STEP 5.5f
#define LOGO_CLUB_SPIN_STEP 23.0f
#define LOGO_CLUB_UNUSED_ANGLE_STEP 3.0f
#define LOGO_CLUB_WOBBLE_ANGLE_STEP 6.0f
/* The original's single-precision approximation of pi / 180 */
#define LOGO_DEGREES_TO_RADIANS 0.017453294f

/*
 * The shine's frames. Counting starts after the first, so the first shown
 * frame is never the limit.
 */
#define LOGO_SHINE_FIRST_FRAME 20
#define LOGO_SHINE_FIRST_SHOWN_FRAME 4
#define LOGO_SHINE_LAST_SHOWN_FRAME 36
#define LOGO_SHINE_LAST_FRAME 64
/* Where the shine's image starts on the frame given */
#define LOGO_SHINE_ORIGIN_TEXEL 64
#define LOGO_SHINE_ORIGIN_FRAME 16
/* How far the shine moves each frame, in texels */
#define LOGO_SHINE_SPEED 18
/* About 0.64 texels a pixel, in s5.10 */
#define LOGO_SHINE_TEXEL_STEP 0x28F
#define LOGO_SHINE_IMAGE_SIZE 64
/* Where the shine sweeps, on the screen; it runs past the right edge */
#define LOGO_SHINE_LEFT 10
#define LOGO_SHINE_TOP 60
#define LOGO_SHINE_RIGHT 630
#define LOGO_SHINE_BOTTOM 120
/*
 * The shine is drawn only over the letters: their area is first filled with
 * a near depth, the letters are redrawn with a far one, and the shine is
 * depth tested between the two.
 */
#define LOGO_SHINE_MASK_LEFT 8
#define LOGO_SHINE_MASK_RIGHT 312
#define LOGO_SHINE_MASK_DEPTH 4
#define LOGO_SHINE_LETTER_DEPTH 40
#define LOGO_SHINE_DEPTH 36

/* Each letter is drawn in a top and a bottom half */
#define LOGO_LETTER_HALF_COUNT 2
#define LOGO_LETTER_WIDTH 48
#define LOGO_LETTER_HALF_HEIGHT 24
/* Added to each letter's depth to give its primitive depth */
#define LOGO_LETTER_DEPTH_OFFSET 64
#define LOGO_CLUB_WIDTH 152
#define LOGO_CLUB_STRIP_HEIGHT 13
#define LOGO_CLUB_DEPTH 92
/* The alpha below which the logo's texels aren't drawn */
#define LOGO_ALPHA_THRESHOLD 0x50
#define LOGO_FADING_ALPHA_THRESHOLD 10

#define LOGO_SHRINK_FRAMES 10
/* Where the logo ends up, on the screen */
#define LOGO_SHRUNK_Y 70
#define LOGO_SHRUNK_L_Y 68
#define LOGO_SHRUNK_SCALE 0.72f
#define LOGO_SHRUNK_CLUB_X 160
#define LOGO_SHRUNK_CLUB_Y 188

/* Where the logo is drawn, in the screen's orthographic projection */
#define LOGO_Z (-300.0f)

/* Starts at 0xFF */
extern s32 f_LogoLettersAlpha;
extern s32 f_LogoClubAlpha;
/* Starts at LOGO_SHINE_FIRST_FRAME, and counts while the logo shines */
extern s32 f_LogoShineFrame;
/* How long, at least, the club spins in place; starts at 40 */
extern s32 f_LogoClubSpinFramesLeft;
extern s32 f_LogoState;
extern s32 f_LogoFadeStage;
extern s32 f_LogoFadeFramesLeft; /* Starts at 63 */
extern f32 f_LogoClubScale[2];   /* Across, then up */
/* In degrees */
extern f32 f_LogoClubAngle;
extern f32 f_LogoClubOrbitAngle;
extern f32 f_LogoClubUnusedAngle; /* Its sine is never used */
extern f32 f_LogoClubWobbleAngle;
/* The top and bottom halves of a letter */
extern Vtx f_LogoLetterVertices[2][4];
/* The club's image is drawn in six strips */
#define LOGO_CLUB_STRIP_COUNT 6
extern Vtx f_LogoClubVertices[LOGO_CLUB_STRIP_COUNT][4];
extern Mtx f_LogoLetterTranslations[LOGO_LETTER_COUNT][DRAW_BUFFER_COUNT];
extern Mtx f_LogoLetterScalings[LOGO_LETTER_COUNT][DRAW_BUFFER_COUNT];
extern Mtx f_LogoClubTranslations[DRAW_BUFFER_COUNT];
extern Mtx f_LogoClubScalings[DRAW_BUFFER_COUNT];
extern Mtx f_LogoClubRotations[DRAW_BUFFER_COUNT];
/* While the logo shrinks, each frame's moves, in pixels */
extern s32 f_LogoLetterSteps[LOGO_LETTER_COUNT];
extern s32 f_LogoLetterRises[2]; /* Up; the other letters', then the L's */
extern f32 f_LogoScaleStep;
extern s32 f_LogoClubStepX;
extern s32 f_LogoClubStepY;

/*
 * Drives the title logo: Lakitu drops the letters, which bounce into place,
 * the club spirals in to them, a shine sweeps across, and the logo shrinks
 * into place. Meanwhile the screen behind it fades to gray and then white.
 * The logo is only drawn when isDrawn is 1, but moves on every call.
 *
 * Differs from the original in register allocation and stack slots: some
 * spilled display list pointers swap slots, a few addresses are kept in
 * registers instead of reloaded, and the letters' texture offsets are folded
 * into their relocations.
 */
#ifdef NON_MATCHING
void DrawOpeningLogo(Gfx** displayListHead, s32 isDrawn) {
  Gfx* gfx = *displayListHead;
  s32 pad[6];
  s32 i;
  s32 j;
  f32 angle;
  f32 wobble;

  switch (f_LogoFadeStage) {
    case LOGO_FADE_GRAYING:
      g_ScreenFadeFramesLeft = f_LogoFadeFramesLeft;
      f_LogoFadeFramesLeft--;
      if (f_LogoFadeFramesLeft == LOGO_FADE_WHITENING_FRAMES) {
        f_LogoFadeStage++;
      }
      break;
    case LOGO_FADE_GRAY:
      g_ScreenFadeFramesLeft = f_LogoFadeFramesLeft;
      if (f_LogoShineFrame > LOGO_SHINE_FIRST_FRAME) {
        f_LogoFadeStage++;
      }
      break;
    case LOGO_FADE_WHITENING:
      g_ScreenFadeFramesLeft = f_LogoFadeFramesLeft;
      g_ScreenFadeRed = g_ScreenFadeGreen = g_ScreenFadeBlue =
          (s32)((LOGO_FADE_WHITENING_FRAMES - f_LogoFadeFramesLeft) *
                LOGO_FADE_WHITENING_STEP) +
          LOGO_FADE_GRAY_LEVEL;
      f_LogoFadeFramesLeft--;
      if (f_LogoFadeFramesLeft == 0) {
        f_LogoFadeStage++;
      }
      break;
    case LOGO_FADE_WHITE:
      g_ScreenFadeDuration = LOGO_FADE_WHITE_DURATION;
      g_ScreenFadeRed = g_ScreenFadeGreen = g_ScreenFadeBlue =
          LOGO_FADE_WHITE_LEVEL;
      g_ScreenFadeKind = FADE_OUT;
      g_ScreenFadeFramesLeft = 0;
      if (f_LogoState == LOGO_SHRINKING) {
        f_LogoFadeStage++;
      }
      break;
    case LOGO_FADE_LOGO_OUT:
      g_ScreenFadeDuration = LOGO_FADE_WHITE_DURATION;
      g_ScreenFadeKind = FADE_OUT;
      g_ScreenFadeFramesLeft = 0;
      g_ScreenFadeRed = g_ScreenFadeGreen = g_ScreenFadeBlue =
          LOGO_FADE_WHITE_LEVEL;
      f_LogoLettersAlpha -= LOGO_FADE_OUT_STEP;
      f_LogoClubAlpha -= LOGO_FADE_OUT_STEP;
      if (f_LogoLettersAlpha < 0) {
        f_LogoLettersAlpha = 0;
        f_LogoClubAlpha = 0;
        f_OpeningScene = OPENING_SCENE_END;
      }
      break;
  }
  /* Lakitu drops each letter as he passes where it lands */
  if (f_LogoLetterStates[LOGO_LETTER_M] == LOGO_LETTER_WAITING &&
      f_LakituScreenPoint[0] > LOGO_M_DROP_X) {
    f_LogoLetters[LOGO_LETTER_M].x = LOGO_M_DROP_X;
    f_LogoLetters[LOGO_LETTER_M].y = f_LakituScreenPoint[1];
    f_LogoLetterStates[LOGO_LETTER_M] = LOGO_LETTER_FALLING;
    g_ScreenFadeDuration = g_ScreenFadeFramesLeft = LOGO_GRAYING_DURATION;
    g_ScreenFadeKind = FADE_OUT;
    g_ScreenFadeRed = g_ScreenFadeGreen = g_ScreenFadeBlue =
        LOGO_FADE_GRAY_LEVEL;
    f_LogoFadeStage = LOGO_FADE_GRAYING;
    D_8020AC48_ovl_05 = 1;
  }
  if (f_LogoLetterStates[LOGO_LETTER_A] == LOGO_LETTER_WAITING &&
      f_LakituScreenPoint[0] > LOGO_A_DROP_X) {
    f_LogoLetters[LOGO_LETTER_A].x = LOGO_A_DROP_X;
    f_LogoLetters[LOGO_LETTER_A].y = f_LakituScreenPoint[1];
    f_LogoLetterStates[LOGO_LETTER_A] = LOGO_LETTER_FALLING;
  }
  if (f_LogoLetterStates[LOGO_LETTER_R] == LOGO_LETTER_WAITING &&
      f_LakituScreenPoint[0] > LOGO_R_DROP_X) {
    f_LogoLetters[LOGO_LETTER_R].x = LOGO_R_DROP_X;
    f_LogoLetters[LOGO_LETTER_R].y = f_LakituScreenPoint[1];
    f_LogoLetterStates[LOGO_LETTER_R] = LOGO_LETTER_FALLING;
  }
  if (f_LogoLetterStates[LOGO_LETTER_I] == LOGO_LETTER_WAITING &&
      f_LakituScreenPoint[0] > LOGO_I_DROP_X) {
    f_LogoLetters[LOGO_LETTER_I].x = LOGO_I_DROP_X;
    f_LogoLetters[LOGO_LETTER_I].y = f_LakituScreenPoint[1];
    f_LogoLetterStates[LOGO_LETTER_I] = LOGO_LETTER_FALLING;
  }
  if (f_LogoLetterStates[LOGO_LETTER_FIRST_O] == LOGO_LETTER_WAITING &&
      f_LakituScreenPoint[0] > LOGO_FIRST_O_DROP_X) {
    f_LogoLetters[LOGO_LETTER_FIRST_O].x = LOGO_FIRST_O_DROP_X;
    f_LogoLetters[LOGO_LETTER_FIRST_O].y = f_LakituScreenPoint[1];
    f_LogoLetterStates[LOGO_LETTER_FIRST_O] = LOGO_LETTER_FALLING;
  }
  if (f_LogoLetterStates[LOGO_LETTER_G] == LOGO_LETTER_WAITING &&
      f_LakituScreenPoint[0] > LOGO_G_DROP_X) {
    f_LogoLetters[LOGO_LETTER_G].x = LOGO_G_DROP_X;
    f_LogoLetters[LOGO_LETTER_G].y = f_LakituScreenPoint[1];
    f_LogoLetterStates[LOGO_LETTER_G] = LOGO_LETTER_FALLING;
  }
  if (f_LogoLetterStates[LOGO_LETTER_SECOND_O] == LOGO_LETTER_WAITING &&
      f_LakituScreenPoint[0] > LOGO_SECOND_O_DROP_X) {
    f_LogoLetters[LOGO_LETTER_SECOND_O].x = LOGO_SECOND_O_DROP_X;
    f_LogoLetters[LOGO_LETTER_SECOND_O].y = f_LakituScreenPoint[1];
    f_LogoLetterStates[LOGO_LETTER_SECOND_O] = LOGO_LETTER_FALLING;
  }
  if (f_LogoLetterStates[LOGO_LETTER_L] == LOGO_LETTER_WAITING &&
      f_LakituScreenPoint[0] > LOGO_L_DROP_X) {
    f_LogoLetters[LOGO_LETTER_L].x = LOGO_L_DROP_X;
    f_LogoLetters[LOGO_LETTER_L].y = f_LakituScreenPoint[1];
    f_LogoLetterStates[LOGO_LETTER_L] = LOGO_LETTER_FALLING;
  }
  if (f_LogoLetterStates[LOGO_LETTER_F] == LOGO_LETTER_WAITING &&
      f_LakituScreenPoint[0] > LOGO_F_DROP_X) {
    f_LogoLetters[LOGO_LETTER_F].x = LOGO_F_DROP_X;
    f_LogoLetters[LOGO_LETTER_F].y = f_LakituScreenPoint[1];
    f_LogoLetterStates[LOGO_LETTER_F] = LOGO_LETTER_FALLING;
    f_LogoState = LOGO_CLUB_STARTING;
  }
  if (isDrawn == 1) {
    for (i = 0; i < LOGO_LETTER_COUNT; i++) {
      guTranslate(&f_LogoLetterTranslations[i][g_DrawBufferIndex],
                  f_LogoLetters[i].x - SCREEN_WIDTH / 2.0f,
                  (SCREEN_HEIGHT - f_LogoLetters[i].y) - SCREEN_HEIGHT / 2.0f,
                  f_LogoLetters[i].depth + LOGO_Z);
      guScale(&f_LogoLetterScalings[i][g_DrawBufferIndex],
              f_LogoLetterScales[i][0], f_LogoLetterScales[i][1], 1.0f);
    }
    guTranslate(&f_LogoClubTranslations[g_DrawBufferIndex],
                f_LogoClubPosition[0] - SCREEN_WIDTH / 2.0f,
                f_LogoClubPosition[1] - SCREEN_HEIGHT / 2.0f,
                f_LogoClubPosition[2] + LOGO_Z);
    guRotate(&f_LogoClubRotations[g_DrawBufferIndex], f_LogoClubAngle, 0.0f,
             0.0f, 1.0f);
    guScale(&f_LogoClubScalings[g_DrawBufferIndex], f_LogoClubScale[0],
            f_LogoClubScale[1], 1.0f);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
                      TEXEL0, PRIMITIVE, 0, TEXEL0, 0);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    if (f_LogoFadeStage == LOGO_FADE_LOGO_OUT) {
      gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, LOGO_FADING_ALPHA_THRESHOLD);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_AA_ZB_XLU_SURF | Z_UPD,
                       G_RM_AA_ZB_XLU_SURF2 | Z_UPD);
    } else {
      gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, LOGO_ALPHA_THRESHOLD);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    }
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_DISABLE);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureLOD(gfx++, G_TL_TILE);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, f_LogoLettersAlpha);
    gDPLoadTLUT_pal256(gfx++,
                       TEXTURE_ADDRESS(f_LogoLetterImages + IMAGE_HEADER_SIZE));
    gDPPipeSync(gfx++);
    for (i = 0; i < LOGO_LETTER_COUNT; i++) {
      for (j = 0; j < LOGO_LETTER_HALF_COUNT; j++) {
        gDPLoadTextureBlock(
            gfx++,
            TEXTURE_ADDRESS(f_LogoLetterImages +
                            ((j + i * LOGO_LETTER_HALF_COUNT) *
                                 LOGO_LETTER_WIDTH * LOGO_LETTER_HALF_HEIGHT +
                             LARGE_PALETTED_TEXELS_OFFSET)),
            G_IM_FMT_CI, G_IM_SIZ_8b, LOGO_LETTER_WIDTH,
            LOGO_LETTER_HALF_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
            G_TX_NOLOD);
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
                  G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        gSPMatrix(
            gfx++,
            OS_K0_TO_PHYSICAL(&f_LogoLetterTranslations[i][g_DrawBufferIndex]),
            G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gSPMatrix(
            gfx++,
            OS_K0_TO_PHYSICAL(&f_LogoLetterScalings[i][g_DrawBufferIndex]),
            G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
        gDPSetPrimDepth(gfx++,
                        f_LogoLetters[i].depth + LOGO_LETTER_DEPTH_OFFSET, 0);
        gDPPipeSync(gfx++);
        gSPVertex(gfx++, f_LogoLetterVertices[j], 4, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        gDPPipeSync(gfx++);
      }
    }
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, f_LogoClubAlpha);
    gDPLoadTLUT_pal256(gfx++,
                       TEXTURE_ADDRESS(f_LogoClubImage + IMAGE_HEADER_SIZE));
    gDPPipeSync(gfx++);
    for (i = 0; i < LOGO_CLUB_STRIP_COUNT; i++) {
      gDPLoadTextureBlock(
          gfx++,
          TEXTURE_ADDRESS(f_LogoClubImage +
                          (i * LOGO_CLUB_WIDTH * LOGO_CLUB_STRIP_HEIGHT +
                           LARGE_PALETTED_TEXELS_OFFSET)),
          G_IM_FMT_CI, G_IM_SIZ_8b, LOGO_CLUB_WIDTH, LOGO_CLUB_STRIP_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
                G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
      gSPMatrix(gfx++,
                OS_K0_TO_PHYSICAL(&f_LogoClubTranslations[g_DrawBufferIndex]),
                G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
      gSPMatrix(gfx++,
                OS_K0_TO_PHYSICAL(&f_LogoClubRotations[g_DrawBufferIndex]),
                G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
      gSPMatrix(gfx++,
                OS_K0_TO_PHYSICAL(&f_LogoClubScalings[g_DrawBufferIndex]),
                G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
      gDPSetPrimDepth(gfx++, LOGO_CLUB_DEPTH, 0);
      gDPPipeSync(gfx++);
      gSPVertex(gfx++, f_LogoClubVertices[i], 4, 0);
      gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
      gDPPipeSync(gfx++);
    }
  }
  if ((f_LogoState == LOGO_SHINING) & (isDrawn == 1)) {
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPSetPrimDepth(gfx++, LOGO_SHINE_MASK_DEPTH, 0);
    gDPSetCombineLERP(gfx++, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++,
                     GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
                     G_RM_AA_ZB_OPA_SURF2);
    gSPTextureRectangle(gfx++, LOGO_SHINE_MASK_LEFT << 2, LOGO_SHINE_TOP << 2,
                        LOGO_SHINE_MASK_RIGHT << 2, LOGO_SHINE_BOTTOM << 2,
                        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPSetPrimDepth(gfx++, LOGO_SHINE_LETTER_DEPTH, 0);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(
        gfx++,
        AA_EN | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | CVG_X_ALPHA |
            ALPHA_CVG_SEL |
            GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
        AA_EN | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | CVG_X_ALPHA |
            ALPHA_CVG_SEL |
            GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM));
    gDPLoadTLUT_pal256(gfx++,
                       TEXTURE_ADDRESS(f_LogoLetterImages + IMAGE_HEADER_SIZE));
    for (i = 0; i < LOGO_LETTER_COUNT; i++) {
      for (j = 0; j < LOGO_LETTER_HALF_COUNT; j++) {
        gDPLoadTextureBlock(
            gfx++,
            TEXTURE_ADDRESS(f_LogoLetterImages +
                            ((j + i * LOGO_LETTER_HALF_COUNT) *
                                 LOGO_LETTER_WIDTH * LOGO_LETTER_HALF_HEIGHT +
                             LARGE_PALETTED_TEXELS_OFFSET)),
            G_IM_FMT_CI, G_IM_SIZ_8b, LOGO_LETTER_WIDTH,
            LOGO_LETTER_HALF_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
            G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
                  G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        gSPMatrix(
            gfx++,
            OS_K0_TO_PHYSICAL(&f_LogoLetterTranslations[i][g_DrawBufferIndex]),
            G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gSPMatrix(
            gfx++,
            OS_K0_TO_PHYSICAL(&f_LogoLetterScalings[i][g_DrawBufferIndex]),
            G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
        gDPPipeSync(gfx++);
        gSPVertex(gfx++, f_LogoLetterVertices[j], 4, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        gDPPipeSync(gfx++);
      }
    }
    if (isDrawn == 1) {
      gDPSetPrimDepth(gfx++, LOGO_SHINE_DEPTH, 0);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
      gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0,
                        0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
      gDPPipeSync(gfx++);
      gDPSetTextureLUT(gfx++, G_TT_NONE);
      gDPPipeSync(gfx++);
      gDPSetAlphaDither(gfx++, G_AD_PATTERN);
      gDPPipeSync(gfx++);
      gDPSetAlphaCompare(gfx++, G_AC_NONE);
      gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      gDPPipeSync(gfx++);
      gDPSetTexturePersp(gfx++, G_TP_NONE);
      gDPLoadTextureBlock(
          gfx++, TEXTURE_ADDRESS(f_LogoShineImage + IMAGE_HEADER_SIZE),
          G_IM_FMT_I, G_IM_SIZ_8b, LOGO_SHINE_IMAGE_SIZE, LOGO_SHINE_IMAGE_SIZE,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
    }
    f_LogoShineFrame++;
    if (f_LogoShineFrame > LOGO_SHINE_LAST_FRAME) {
      f_LogoState = LOGO_SHRINK_STARTING;
    }
    if (isDrawn == 1 && f_LogoShineFrame > LOGO_SHINE_FIRST_SHOWN_FRAME &&
        f_LogoShineFrame < LOGO_SHINE_LAST_SHOWN_FRAME) {
      gSPTextureRectangle(
          gfx++, LOGO_SHINE_LEFT << 2, LOGO_SHINE_TOP << 2,
          LOGO_SHINE_RIGHT << 2, LOGO_SHINE_BOTTOM << 2, G_TX_RENDERTILE,
          (LOGO_SHINE_ORIGIN_TEXEL -
           (f_LogoShineFrame - LOGO_SHINE_ORIGIN_FRAME) * LOGO_SHINE_SPEED)
              << 5,
          0, LOGO_SHINE_TEXEL_STEP, LOGO_SHINE_TEXEL_STEP);
      gDPPipeSync(gfx++);
    }
  }
  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  *displayListHead = gfx;

  for (i = 0; i < LOGO_LETTER_COUNT; i++) {
    if (f_LogoLetterStates[i] == LOGO_LETTER_FALLING) {
      if (f_LogoLetterSpeeds[i] > 0.0f) {
        if (i != LOGO_LETTER_L) {
          if (f_LogoLetters[i].y + (s32)f_LogoLetterSpeeds[i] >=
              LOGO_LETTER_GROUND_Y) {
            f_LogoLetterSpeeds[i] *= LOGO_LETTER_BOUNCINESS;
            f_LogoLetters[i].y = LOGO_LETTER_GROUND_Y - 1;
            if (f_LogoLetterSpeeds[i] <= LOGO_LETTER_MIN_BOUNCE_SPEED) {
              f_LogoLetterSpeeds[i] = 0.0f;
              f_LogoLetterStates[i] = LOGO_LETTER_LANDED;
              continue;
            }
            f_LogoLetterSpeeds[i] = -f_LogoLetterSpeeds[i];
            f_LogoLetterStates[i] = LOGO_LETTER_SQUASHING;
            f_LogoLetterSquashFrames[i] = LOGO_LETTER_SQUASH_FRAMES;
          }
        } else {
          if (f_LogoLetters[LOGO_LETTER_L].y +
                  (s32)f_LogoLetterSpeeds[LOGO_LETTER_L] >=
              LOGO_LETTER_L_GROUND_Y) {
            f_LogoLetterSpeeds[LOGO_LETTER_L] *= LOGO_LETTER_BOUNCINESS;
            f_LogoLetters[LOGO_LETTER_L].y = LOGO_LETTER_L_GROUND_Y - 1;
            if (f_LogoLetterSpeeds[LOGO_LETTER_L] <=
                LOGO_LETTER_MIN_BOUNCE_SPEED) {
              f_LogoLetterSpeeds[LOGO_LETTER_L] = 0.0f;
              f_LogoLetterStates[LOGO_LETTER_L] = LOGO_LETTER_LANDED;
              continue;
            }
            f_LogoLetterSpeeds[LOGO_LETTER_L] =
                -f_LogoLetterSpeeds[LOGO_LETTER_L];
            f_LogoLetterStates[LOGO_LETTER_L] = LOGO_LETTER_SQUASHING;
            f_LogoLetterSquashFrames[LOGO_LETTER_L] = LOGO_LETTER_SQUASH_FRAMES;
          }
        }
      }
      f_LogoLetters[i].y += (s32)f_LogoLetterSpeeds[i];
      f_LogoLetterSpeeds[i] += LOGO_LETTER_GRAVITY;
      f_LogoLetterScales[i][0] += LOGO_LETTER_GROWTH;
      f_LogoLetterScales[i][1] += LOGO_LETTER_GROWTH;
      if (f_LogoLetterScales[i][0] > 1.0f) {
        f_LogoLetterScales[i][0] = 1.0f;
      }
      if (f_LogoLetterScales[i][1] > 1.0f) {
        f_LogoLetterScales[i][1] = 1.0f;
      }
    } else if (f_LogoLetterStates[i] == LOGO_LETTER_SQUASHING) {
      f_LogoLetterScales[i][0] += LOGO_LETTER_SQUASH_STEP;
      f_LogoLetterScales[i][1] -= LOGO_LETTER_SQUASH_STEP;
      f_LogoLetterSquashFrames[i]--;
      if (f_LogoLetterSquashFrames[i] == 0) {
        f_LogoLetterStates[i] = LOGO_LETTER_SQUASHED;
        f_LogoLetterSquashFrames[i] = LOGO_LETTER_SQUASHED_FRAMES;
      }
    } else if (f_LogoLetterStates[i] == LOGO_LETTER_SQUASHED) {
      f_LogoLetterSquashFrames[i]--;
      if (f_LogoLetterSquashFrames[i] == 0) {
        f_LogoLetterStates[i] = LOGO_LETTER_STRETCHING;
        f_LogoLetterSquashFrames[i] = LOGO_LETTER_STRETCH_FRAMES;
      }
    } else if (f_LogoLetterStates[i] == LOGO_LETTER_STRETCHING) {
      f_LogoLetterScales[i][0] -= LOGO_LETTER_STRETCH_STEP;
      f_LogoLetterScales[i][1] += LOGO_LETTER_STRETCH_STEP;
      f_LogoLetterSquashFrames[i]--;
      if (f_LogoLetterSquashFrames[i] == 0) {
        f_LogoLetterStates[i] = LOGO_LETTER_FALLING;
      }
    }
  }
  if (f_LogoState == LOGO_CLUB_STARTING) {
    angle = f_LogoClubOrbitAngle * LOGO_DEGREES_TO_RADIANS;
    sinf(f_LogoClubUnusedAngle * LOGO_DEGREES_TO_RADIANS);
    wobble = -sinf(f_LogoClubWobbleAngle * LOGO_DEGREES_TO_RADIANS) *
             LOGO_CLUB_FIRST_WOBBLE;
    f_LogoState = LOGO_CLUB_CIRCLING;
    f_LogoClubPosition[0] =
        (s32)(-cosf(angle) * (f_LogoClubOrbitRadius + wobble)) +
        LOGO_CLUB_ORBIT_CENTER_X;
    f_LogoClubPosition[1] =
        (s32)(-sinf(angle) * (f_LogoClubOrbitRadius + wobble)) +
        LOGO_CLUB_ORBIT_CENTER_Y;
    f_IsLogoClubSwinging = 1;
  }
  if (f_LogoState == LOGO_CLUB_CIRCLING) {
    angle = f_LogoClubOrbitAngle * LOGO_DEGREES_TO_RADIANS;
    sinf(f_LogoClubUnusedAngle * LOGO_DEGREES_TO_RADIANS);
    wobble = -sinf(f_LogoClubWobbleAngle * LOGO_DEGREES_TO_RADIANS) *
             LOGO_CLUB_WOBBLE;
    f_LogoClubPosition[0] =
        (s32)(-cosf(angle) * (f_LogoClubOrbitRadius + wobble)) +
        LOGO_CLUB_ORBIT_CENTER_X;
    f_LogoClubPosition[1] =
        (s32)(-sinf(angle) * (f_LogoClubOrbitRadius + wobble)) +
        LOGO_CLUB_ORBIT_CENTER_Y;
    f_LogoClubOrbitRadius -= LOGO_CLUB_ORBIT_RADIUS_STEP;
    f_LogoClubOrbitAngle += LOGO_CLUB_ORBIT_ANGLE_STEP;
    f_LogoClubAngle -= LOGO_CLUB_SPIN_STEP;
    f_LogoClubUnusedAngle += LOGO_CLUB_UNUSED_ANGLE_STEP;
    f_LogoClubWobbleAngle += LOGO_CLUB_WOBBLE_ANGLE_STEP;
    if (f_LogoClubOrbitRadius < 0.0f) {
      f_LogoClubOrbitRadius = 0.0f;
      f_LogoState = LOGO_CLUB_SPINNING;
    }
  }
  if (f_LogoState == LOGO_CLUB_SPINNING) {
    f_LogoClubAngle -= LOGO_CLUB_SPIN_STEP;
    f_LogoClubSpinFramesLeft--;
    if (f_LogoClubSpinFramesLeft < 0) {
      f_LogoClubSpinFramesLeft = 0;
      if (f_LogoClubAngle < 0.0f) {
        f_LogoClubAngle = 0.0f;
        f_LogoState = LOGO_SHINING;
        f_IsLogoClubSwinging = 0;
      }
    }
  }
  if (f_LogoState == LOGO_SHRINK_STARTING) {
    f_LogoState = LOGO_SHRINKING;
    /* The letters' shrunk x positions */
    f_LogoLetterSteps[LOGO_LETTER_M] =
        (LOGO_M_SHRUNK_X - f_LogoLetters[LOGO_LETTER_M].x) / LOGO_SHRINK_FRAMES;
    f_LogoLetterSteps[LOGO_LETTER_A] =
        (LOGO_A_SHRUNK_X - f_LogoLetters[LOGO_LETTER_A].x) / LOGO_SHRINK_FRAMES;
    f_LogoLetterSteps[LOGO_LETTER_R] =
        (LOGO_R_SHRUNK_X - f_LogoLetters[LOGO_LETTER_R].x) / LOGO_SHRINK_FRAMES;
    f_LogoLetterSteps[LOGO_LETTER_I] =
        (LOGO_I_SHRUNK_X - f_LogoLetters[LOGO_LETTER_I].x) / LOGO_SHRINK_FRAMES;
    f_LogoLetterSteps[LOGO_LETTER_FIRST_O] =
        (LOGO_FIRST_O_SHRUNK_X - f_LogoLetters[LOGO_LETTER_FIRST_O].x) /
        LOGO_SHRINK_FRAMES;
    f_LogoLetterSteps[LOGO_LETTER_G] =
        (LOGO_G_SHRUNK_X - f_LogoLetters[LOGO_LETTER_G].x) / LOGO_SHRINK_FRAMES;
    f_LogoLetterSteps[LOGO_LETTER_SECOND_O] =
        (LOGO_SECOND_O_SHRUNK_X - f_LogoLetters[LOGO_LETTER_SECOND_O].x) /
        LOGO_SHRINK_FRAMES;
    f_LogoLetterSteps[LOGO_LETTER_L] =
        (LOGO_L_SHRUNK_X - f_LogoLetters[LOGO_LETTER_L].x) / LOGO_SHRINK_FRAMES;
    f_LogoLetterSteps[LOGO_LETTER_F] =
        (LOGO_F_SHRUNK_X - f_LogoLetters[LOGO_LETTER_F].x) / LOGO_SHRINK_FRAMES;
    f_LogoLetterRises[LOGO_LETTER_M] =
        (LOGO_SHRUNK_Y - f_LogoLetters[LOGO_LETTER_M].y) / LOGO_SHRINK_FRAMES;
    f_LogoLetterRises[LOGO_LETTER_A] =
        (LOGO_SHRUNK_L_Y - f_LogoLetters[LOGO_LETTER_L].y) / LOGO_SHRINK_FRAMES;
    f_LogoScaleStep =
        (LOGO_SHRUNK_SCALE - f_LogoLetterScales[LOGO_LETTER_M][0]) *
        (1.0f / LOGO_SHRINK_FRAMES);
    f_LogoClubStepX =
        (LOGO_SHRUNK_CLUB_X - f_LogoClubPosition[0]) / LOGO_SHRINK_FRAMES;
    f_LogoClubStepY =
        (LOGO_SHRUNK_CLUB_Y - f_LogoClubPosition[1]) / LOGO_SHRINK_FRAMES;
  }
  if (f_LogoState == LOGO_SHRINKING) {
    for (i = 0; i < LOGO_LETTER_COUNT; i++) {
      if (i != LOGO_LETTER_L) {
        f_LogoLetters[i].x += f_LogoLetterSteps[i];
        f_LogoLetters[i].y += f_LogoLetterRises[LOGO_LETTER_M];
        f_LogoLetterScales[i][0] += f_LogoScaleStep;
        f_LogoLetterScales[i][1] += f_LogoScaleStep;
      }
    }
    f_LogoLetters[LOGO_LETTER_L].x += f_LogoLetterSteps[LOGO_LETTER_L];
    f_LogoLetters[LOGO_LETTER_L].y += f_LogoLetterRises[LOGO_LETTER_A];
    f_LogoClubPosition[0] += f_LogoClubStepX;
    f_LogoClubPosition[1] += f_LogoClubStepY;
    f_LogoLetterScales[LOGO_LETTER_L][0] += f_LogoScaleStep;
    f_LogoLetterScales[LOGO_LETTER_L][1] += f_LogoScaleStep;
    f_LogoClubScale[0] += f_LogoScaleStep;
    f_LogoClubScale[1] += f_LogoScaleStep;
    if (f_LogoLetterScales[LOGO_LETTER_M][0] < LOGO_SHRUNK_SCALE) {
      f_LogoState = LOGO_SHRUNK;
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", DrawOpeningLogo);
#endif

/* The opening's sparks, which fan out from a point and slow down */
#define SPARK_COUNT 20
#define SPARK_FRAME_COUNT 10
/* The spark scale is this divided by the origin's depth */
#define SPARK_SCALE_DEPTH 48.0f
/* The sparks are spread evenly around a circle, then jittered */
#define SPARK_ANGLE_STEP (M_PI * (2.0f / SPARK_COUNT))
#define SPARK_ANGLE_JITTER_RANGE 20
/* Before scaling, a spark's speed is this plus a random amount */
#define SPARK_MIN_SPEED 10.0f
#define SPARK_SPEED_RANGE 16
/* How much of its speed a spark keeps each frame */
#define SPARK_DRAG 0.98
/* The depth the sparks are drawn at under the screen's projection */
#define SPARK_DEPTH -100.0f

extern Vtx f_OpeningSparkVertices[3];
extern f32 f_OpeningSparkPositions[SPARK_COUNT][2];
extern f32 f_OpeningSparkAngles[SPARK_COUNT];
extern f32 f_OpeningSparkVelocities[SPARK_COUNT][2];
extern f32 f_OpeningSparkScale;
extern s32 f_OpeningSparkFramesLeft;
extern Mtx f_OpeningSparkMatrices[DRAW_BUFFER_COUNT][SPARK_COUNT];

#ifdef NON_MATCHING
/*
 * Draws sparks flying out from a point once f_OpeningSparksState is set to
 * SPARKS_STARTING. The point is projected to the screen, and the sparks
 * are scaled down the further away it is.
 *
 * The only difference from the original is that the display list
 * pointers spilled after the first and second commands swap stack slots.
 */
void DrawOpeningSparks(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  ScreenPoint origin;
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
  Mat4 matrix;
  s32 depth;
  s32 sparkIndex;
  f32 speed;

  if (f_OpeningSparksState == SPARKS_STARTING) {
    depth = ProjectPoint(f_OpeningSparksOrigin[0], f_OpeningSparksOrigin[1],
                         f_OpeningSparksOrigin[2], &origin);
    if (depth < 0) {
      f_OpeningSparkScale = SPARK_SCALE_DEPTH / (f32)-depth;
      for (sparkIndex = 0; sparkIndex < SPARK_COUNT; sparkIndex++) {
        f_OpeningSparkPositions[sparkIndex][0] = origin.x - SCREEN_WIDTH / 2;
        f_OpeningSparkPositions[sparkIndex][1] = SCREEN_HEIGHT / 2 - origin.y;
        f_OpeningSparkAngles[sparkIndex] =
            (f32)sparkIndex * SPARK_ANGLE_STEP +
            (f32)DEGREES_TO_RADIANS(guRandom() % SPARK_ANGLE_JITTER_RANGE -
                                    SPARK_ANGLE_JITTER_RANGE / 2);
        speed = (guRandom() % SPARK_SPEED_RANGE) * f_OpeningSparkScale +
                SPARK_MIN_SPEED;
        f_OpeningSparkVelocities[sparkIndex][0] =
            sinf(f_OpeningSparkAngles[sparkIndex]) * speed *
            f_OpeningSparkScale;
        f_OpeningSparkVelocities[sparkIndex][1] =
            -cosf(f_OpeningSparkAngles[sparkIndex]) * speed *
            f_OpeningSparkScale;
        f_OpeningSparksState = SPARKS_FLYING;
      }
      f_OpeningSparkFramesLeft = SPARK_FRAME_COUNT;
    } else {
      f_OpeningSparksState = SPARKS_HIDDEN;
    }
  }
  if (f_OpeningSparksState == SPARKS_FLYING) {
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_DISABLE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_OFF);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineLERP(gfx++, PRIMITIVE, 0, SHADE, 0, 0, 0, 0, SHADE, PRIMITIVE,
                      0, SHADE, 0, 0, 0, 0, SHADE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    for (sparkIndex = 0; sparkIndex < SPARK_COUNT; sparkIndex++) {
      f_OpeningSparkPositions[sparkIndex][0] +=
          f_OpeningSparkVelocities[sparkIndex][0];
      f_OpeningSparkPositions[sparkIndex][1] +=
          f_OpeningSparkVelocities[sparkIndex][1];
      f_OpeningSparkVelocities[sparkIndex][0] *= SPARK_DRAG;
      f_OpeningSparkVelocities[sparkIndex][1] *= SPARK_DRAG;
      translation[0] = f_OpeningSparkPositions[sparkIndex][0];
      translation[1] = f_OpeningSparkPositions[sparkIndex][1];
      translation[2] = SPARK_DEPTH;
      rotation[0] = 0.0f;
      rotation[1] = 0.0f;
      rotation[2] = f_OpeningSparkAngles[sparkIndex];
      scale[0] = f_OpeningSparkScale;
      scale[1] = f_OpeningSparkScale;
      scale[2] = f_OpeningSparkScale;
      BuildTransformMatrix(matrix, rotation, translation, scale);
      ConvertMatrixToFixed(
          matrix, &f_OpeningSparkMatrices[g_DrawBufferIndex][sparkIndex]);
      gSPMatrix(gfx++,
                OS_K0_TO_PHYSICAL(
                    &f_OpeningSparkMatrices[g_DrawBufferIndex][sparkIndex]),
                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
      gSPVertex(gfx++, f_OpeningSparkVertices, 3, 0);
      gSP1Triangle(gfx++, 0, 1, 2, 0);
    }
    if (--f_OpeningSparkFramesLeft == 0) {
      f_OpeningSparksState = SPARKS_HIDDEN;
    }
  }
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", DrawOpeningSparks);
#endif
