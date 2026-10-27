#include "common.h"

#include "asset.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "camera.h"
#include "camera_editor.h"
#include "character.h"
#include "character_slot.h"
#include "cpu_projection.h"
#include "effects.h"
#include "functions.h"
#include "heap.h"
#include "hole_display.h"
#include "libc.h"
#include "math_util.h"
#include "round_hud.h"
#include "screen.h"
#include "sound.h"
#include "variables.h"

/* The view that the opening draws its characters through */
#define OPENING_VIEW_INDEX 2

/* The size of the heap that the opening's billboards are allocated from */
#define OPENING_BILLBOARD_HEAP_SIZE 0x7038

#define ASSET_SHADOW_TEXTURE 0x4C3

/* The face-printed golf balls that the opening shows, two at a time */
#define OPENING_BALL_IMAGE_COUNT 5
#define OPENING_BALL_SLOT_COUNT 2
/* A 64x64 CI8 image with its header and palette */
#define OPENING_BALL_IMAGE_SIZE 0x1208

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
extern s32 D_8020AC50_ovl_05;
extern f32 D_8020AC5C_ovl_05;
extern s32 D_8020AC60_ovl_05;
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
extern s32 D_8020AD70_ovl_05[OPENING_BALL_SLOT_COUNT];
extern f32 D_8020AD80_ovl_05[OPENING_BALL_SLOT_COUNT];
extern s32 f_OpeningBallAssets[OPENING_BALL_IMAGE_COUNT];
extern u8 D_80214C50_ovl_05[OPENING_BALL_SLOT_COUNT][OPENING_BALL_IMAGE_SIZE];
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
extern Vec3 D_8021706C_ovl_05;
extern CoursePoint f_LookAtTarget;

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
/* Where a letter starts, off the screen */
#define LOGO_LETTER_HIDDEN (-1000)

typedef struct LogoLetter {
  s32 unk0;
  s32 unk4;
  s32 unk8;
} LogoLetter; /* size = 0xC */

extern u8 f_LogoLetterImages[];
extern u8 f_LogoClubImage[];
extern u8 f_LogoMarioImage[];
extern u8 f_LogoBallImages[];
extern u8 f_LogoShineImage[];
extern LogoLetter f_LogoLetters[LOGO_LETTER_COUNT];
extern s32 D_80224708_ovl_05[LOGO_LETTER_COUNT];
extern f32 D_80224730_ovl_05[LOGO_LETTER_COUNT];
extern f32 D_80224758_ovl_05[LOGO_LETTER_COUNT][2];
extern s32 D_802247A0_ovl_05[LOGO_LETTER_COUNT];

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
void func_801FBB94_ovl_05(Gfx** displayListHead, CameraMatrices* matrices);
void DrawOpeningImpactLines(Gfx** displayListHead);
void DrawLakitu(Gfx** displayListHead);
void DrawOpeningBurst(Gfx** displayListHead);
void DrawOpeningShotBall(Gfx** displayListHead);
void func_80200CBC_ovl_05(Gfx** displayListHead, s32 arg1);
void DrawOpeningSparks(Gfx** displayListHead);

void func_801F4A40_ovl_05(void) {
  D_8020AC4C_ovl_05++;
  if (D_8020AC4C_ovl_05 == 11) {
    D_8020AC4C_ovl_05 = 0;
  }
}

void func_801F4A6C_ovl_05(void) {
  if (D_8020AC60_ovl_05 != 0) {
    PlaySoundWithEffects(0x51, 12, 48 - (s32)(D_8020AC5C_ovl_05 * 0.2f), 0,
                         127 - (D_8020AC50_ovl_05 - 160) / 2);
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

s32 func_80206680_ovl_05(u32 slotIndex, s32 character);

/* Written but never read */
extern s32 f_LastPlacedSlotIndex;
/* Whether Donkey Kong stands in for Mario, once he is unlocked */
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
      if (func_80206680_ovl_05(0, CHARACTER_BABY_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x10);
        PLACE_CUE_CHARACTER(point0, 0xF3400, 0x490000, -30);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 1:
      f_CueSlotIndex = 1;
      if (func_80206680_ovl_05(1, CHARACTER_METAL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x10);
        PLACE_CUE_CHARACTER(point1, 0xA0B58, 0x4A28BF, 80);
        f_OpeningCue = OPENING_CUE_DONE;
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue++;
      break;
    case 2:
      f_CueSlotIndex = 2;
      if (func_80206680_ovl_05(2, CHARACTER_KID)) {
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
      if (func_80206680_ovl_05(3, CHARACTER_METAL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x11);
        PLACE_CUE_CHARACTER(point3, 0xF3400, 0x490000, -30);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 5:
      f_CueSlotIndex = 0;
      if (func_80206680_ovl_05(0, CHARACTER_BOWSER)) {
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
      if (func_80206680_ovl_05(1, CHARACTER_BOWSER)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x11);
        PLACE_CUE_CHARACTER(point5, 0xF3400, 0x490000, 0);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 8:
      f_CueSlotIndex = 2;
      if (func_80206680_ovl_05(2, CHARACTER_BABY_MARIO)) {
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
      if (func_80206680_ovl_05(0, CHARACTER_KID)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x11);
        PLACE_CUE_CHARACTER(point7, 0xF3400, 0x490000, -45);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 11:
      FreeCharacterSlot(0);
      f_CueSlotIndex = 1;
      if (func_80206680_ovl_05(1, CHARACTER_BOWSER)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x12);
        PLACE_CUE_CHARACTER(point8, 0x29EB94, 0x26DDDA, 0);
      }
      f_CueSlotIndex = 2;
      if (func_80206680_ovl_05(2, CHARACTER_METAL_MARIO)) {
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
      if (func_80206680_ovl_05(0, CHARACTER_BABY_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x12);
        PLACE_CUE_CHARACTER(point10, 0x2709F8, 0x1CF4B4, 10);
      }
      f_CueSlotIndex = 3;
      if (func_80206680_ovl_05(3, CHARACTER_KID)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x12);
        PLACE_CUE_CHARACTER(point11, 0x2709F8, 0x1CF4B4, 55);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 13:
      FreeCharacterSlot(0);
      f_CueSlotIndex = 1;
      if (func_80206680_ovl_05(1, CHARACTER_BABY_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x13);
        PLACE_CUE_CHARACTER(point12, 0x2709F8, 0x1CF4B4, 10);
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 14:
      FreeCharacterSlot(1);
      FreeCharacterSlot(3);
      f_CueSlotIndex = 0;
      if (func_80206680_ovl_05(0, CHARACTER_METAL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x13);
        PLACE_CUE_CHARACTER(point13, 0x2CA5D3, 0x22A928, -98);
      }
      f_CueSlotIndex = 2;
      if (func_80206680_ovl_05(2, CHARACTER_BABY_MARIO)) {
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
      if (func_80206680_ovl_05(1, CHARACTER_METAL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x14);
        PLACE_CUE_CHARACTER(point15, 0x2B1194, 0x249320, -98);
      }
      f_CueSlotIndex = 3;
      if (func_80206680_ovl_05(3, CHARACTER_BABY_MARIO)) {
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
      if (func_80206680_ovl_05(0, CHARACTER_METAL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x15);
        PLACE_CUE_CHARACTER(point17, 0x2B1194, 0x249320, -98);
      }
      f_CueSlotIndex = 2;
      if (func_80206680_ovl_05(2, CHARACTER_BABY_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x16);
        PLACE_CUE_CHARACTER(point18, 0x2B1194, 0x249320, -98);
        f_LastPlacedSlotIndex = f_CueSlotIndex;
      }
      f_OpeningCue = OPENING_CUE_DONE;
      break;
    case 18:
      FreeCharacterSlot(2);
      f_CueSlotIndex = 1;
      func_80206680_ovl_05(1, CHARACTER_BABY_MARIO);
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
        func_80206680_ovl_05(2, CHARACTER_DONKEY_KONG);
      } else {
        func_80206680_ovl_05(2, CHARACTER_MARIO);
      }
      SetAnimationSpeed(2, 0.0f);
      if (f_IsDonkeyKongShown != 0) {
        SetCharacterAnimation(f_CueSlotIndex, 0x11);
      } else {
        SetCharacterAnimation(f_CueSlotIndex, 0x10);
      }
      PLACE_CUE_CHARACTER(point20, 0x202B29, 0xCE7DA, -165);
      f_CueSlotIndex = 3;
      func_80206680_ovl_05(3, CHARACTER_MAPLE);
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
      func_80206680_ovl_05(0, CHARACTER_METAL_MARIO);
      SetCharacterAnimation(f_CueSlotIndex, 0x16);
      PLACE_CUE_CHARACTER(point22, 0x2B1194, 0x249320, -98);
      f_CueSlotIndex = 1;
      func_80206680_ovl_05(1, CHARACTER_BABY_MARIO);
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
      func_80206680_ovl_05(0, CHARACTER_KID);
      SetCharacterAnimation(f_CueSlotIndex, 0x13);
      PLACE_CUE_CHARACTER(point24, 0x16308C, 0x183EDA, 170);
      f_OpeningCue = OPENING_CUE_DONE;
      f_LastPlacedSlotIndex = f_CueSlotIndex;
      break;
    case 26:
      f_CueSlotIndex = 1;
      func_80206680_ovl_05(1, CHARACTER_AZALEA);
      SetCharacterAnimation(f_CueSlotIndex, 0x10);
      PLACE_CUE_CHARACTER(point25, 0xE3F2C, 0x1D98E6, 70);
      f_OpeningCue = OPENING_CUE_DONE;
      f_LastPlacedSlotIndex = f_CueSlotIndex;
      break;
    case 27:
      FreeCharacterSlot(0);
      FreeCharacterSlot(1);
      f_CueSlotIndex = 2;
      func_80206680_ovl_05(2, CHARACTER_KID);
      SetCharacterAnimation(f_CueSlotIndex, 0x14);
      PLACE_CUE_CHARACTER(point26, 0x16308C, 0x183EDA, 185);
      f_LastPlacedSlotIndex = f_CueSlotIndex;
      f_CueSlotIndex = 3;
      if (func_80206680_ovl_05(3, CHARACTER_METAL_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x17);
        PLACE_CUE_CHARACTER(point27, 0x16806D, 0x17E632, 200);
      }
      f_OpeningCue++;
      break;
    case 28:
      f_CueSlotIndex = 0;
      if (func_80206680_ovl_05(0, CHARACTER_BABY_MARIO)) {
        SetCharacterAnimation(f_CueSlotIndex, 0x19);
        PLACE_CUE_CHARACTER(point28, 0x15E14D, 0x18155C, 170);
      }
      f_CueSlotIndex = 1;
      if (func_80206680_ovl_05(1, CHARACTER_BOWSER)) {
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

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801F6788_ovl_05);

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
  func_801FBB94_ovl_05(&gfx, matrices);
  DrawOpeningShotBall(&gfx);
  DrawOpeningSparks(&gfx);
  DrawNameLogos(&gfx);
  func_80200CBC_ovl_05(&gfx, 0);
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
void func_801FBB10_ovl_05(s32 ballIndex, s32 slot) {
  EncodedAsset asset;

  OpenEncodedAsset(f_OpeningBallAssets[ballIndex], &asset);
  ReadEncodedAsset(D_80214C50_ovl_05[slot], &asset);
  D_8020AD70_ovl_05[slot] = 0;
  D_8020AD80_ovl_05[slot] = 1.0f;
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FBB94_ovl_05);

/* A 64x64 CI8 image of yellow and green lines bursting outwards */
#define ASSET_IMPACT_LINES_IMAGE 0x908

void LoadOpeningImpactLines(void) {
  EncodedAsset asset;

  OpenEncodedAsset(ASSET_IMPACT_LINES_IMAGE, &asset);
  ReadEncodedAsset(D_80212830_ovl_05, &asset);
}

extern s32 f_AreImpactLinesShown;
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
#define SHOT_BALL_SHADOW_IMAGE_SIZE 32

extern s32 f_ShotBallScale;
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
    f_ShotBallPosition[0] = f_LookAtTarget[0] * COURSE_TO_WORLD_SCALE;
    f_ShotBallPosition[2] = f_LookAtTarget[2] * COURSE_TO_WORLD_SCALE;
    f_ShotBallPosition[1] =
        f_LookAtTarget[1] * COURSE_TO_WORLD_SCALE + SHOT_BALL_LIFT;
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
      gDPLoadTextureBlock(gfx++,
                          TEXTURE_ADDRESS(g_ShadowTexture + IMAGE_HEADER_SIZE),
                          G_IM_FMT_I, G_IM_SIZ_8b, SHOT_BALL_SHADOW_IMAGE_SIZE,
                          SHOT_BALL_SHADOW_IMAGE_SIZE, 0,
                          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                          G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
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
#define LOOK_AT_EASE_FAST                                                    \
  1                         /* To f_LookAtTarget, a tenth of the way a frame \
                             */
#define LOOK_AT_EASE_SLOW 2 /* To f_LookAtTarget, a hundredth of the way */
/* Near D_8021706C_ovl_05, at a rate that grows to a tenth */
#define LOOK_AT_EASE_SPEEDING_UP 3

/* Eases the camera's look-at point as f_LookAtEaseMode says */
void EaseOpeningLookAt(void) {
  switch (f_LookAtEaseMode) {
    case LOOK_AT_EASE_FAST:
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] +=
          (f_LookAtTarget[0] * COURSE_TO_WORLD_SCALE -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0]) *
          0.1f;
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] +=
          (f_LookAtTarget[1] * COURSE_TO_WORLD_SCALE -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1]) *
          0.1f;
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] +=
          (f_LookAtTarget[2] * COURSE_TO_WORLD_SCALE -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2]) *
          0.1f;
      func_80203620_ovl_05();
      break;
    case LOOK_AT_EASE_SLOW:
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] +=
          (f_LookAtTarget[0] * COURSE_TO_WORLD_SCALE -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0]) *
          0.01f;
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] +=
          (f_LookAtTarget[1] * COURSE_TO_WORLD_SCALE -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1]) *
          0.01f;
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] +=
          (f_LookAtTarget[2] * COURSE_TO_WORLD_SCALE -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2]) *
          0.01f;
      func_80203620_ovl_05();
      break;
    case LOOK_AT_EASE_SPEEDING_UP:
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] +=
          ((D_8021706C_ovl_05[0] + 38.1f) -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0]) *
          f_LookAtEaseRate;
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] +=
          ((D_8021706C_ovl_05[1] - 4.5f) -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1]) *
          f_LookAtEaseRate;
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] +=
          ((D_8021706C_ovl_05[2] - 4.795f) -
           g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2]) *
          f_LookAtEaseRate;
      func_80203620_ovl_05();
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
    D_80224708_ovl_05[letterIndex] = 0;
    D_80224730_ovl_05[letterIndex] = -14.0f;
    f_LogoLetters[letterIndex].unk0 = LOGO_LETTER_HIDDEN;
    f_LogoLetters[letterIndex].unk4 = LOGO_LETTER_HIDDEN;
    D_80224758_ovl_05[letterIndex][0] = 0.1f;
    D_80224758_ovl_05[letterIndex][1] = 0.1f;
    D_802247A0_ovl_05[letterIndex] = 0;
  }
  f_LogoLetters[0].unk8 = 8;
  f_LogoLetters[1].unk8 = 12;
  f_LogoLetters[2].unk8 = 16;
  f_LogoLetters[3].unk8 = 20;
  f_LogoLetters[4].unk8 = 24;
  f_LogoLetters[5].unk8 = 28;
  f_LogoLetters[6].unk8 = 36;
  f_LogoLetters[7].unk8 = 40;
  f_LogoLetters[8].unk8 = 44;
  OpenEncodedAsset(ASSET_LOGO_MARIO_IMAGE, &asset);
  ReadEncodedAsset(f_LogoMarioImage, &asset);
  OpenEncodedAsset(ASSET_LOGO_BALL_IMAGES, &asset);
  ReadEncodedAsset(f_LogoBallImages, &asset);
  OpenEncodedAsset(ASSET_LOGO_SHINE_IMAGE, &asset);
  ReadEncodedAsset(f_LogoShineImage, &asset);
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_80200CBC_ovl_05);

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

/* What the sparks are doing */
typedef enum SparksState {
  SPARKS_HIDDEN,
  SPARKS_STARTING, /* Set elsewhere to start the sparks */
  SPARKS_FLYING
} SparksState;

extern s32 f_OpeningSparksState;
extern Vtx f_OpeningSparkVertices[3];
extern f32 f_OpeningSparkPositions[SPARK_COUNT][2];
extern f32 f_OpeningSparkAngles[SPARK_COUNT];
extern f32 f_OpeningSparkVelocities[SPARK_COUNT][2];
extern f32 f_OpeningSparkScale;
extern s32 f_OpeningSparkFramesLeft;
extern Mtx f_OpeningSparkMatrices[DRAW_BUFFER_COUNT][SPARK_COUNT];
extern Vec3 f_OpeningSparksOrigin;

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
