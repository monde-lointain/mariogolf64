#include "common.h"

#include "asset.h"
#include "character.h"
#include "hole_display.h"
#include "libc.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"
#include "screen.h"
#include "sound.h"
#include "text.h"
#include "variables.h"
#include "work_area.h"

/* The images of D_800C2064 that func_80213390 reads */
#define MENU_SCENE_IMAGE_COUNT 6

/* The bytes kept of each best time holder's palette file */
#define MENU_SCENE_FILE_SIZE 0x200

/* The best times' courses turn this far in all between one course and the next,
 * and this fast */
#define BEST_TIMES_TURN 90
#define BEST_TIMES_SPIN_SPEED 18

/*
 * Asset 0x688 holds 32 by 32 frames of the digits 1 to 4, then the
 * suffixes ST, ND, RD and TH, each drawn at 24 by 24
 */
#define PLACE_LABEL_FRAME_SIZE 32
#define PLACE_LABEL_FRAME_BYTES \
  (PLACE_LABEL_FRAME_SIZE * PLACE_LABEL_FRAME_SIZE)
#define PLACE_LABEL_DIGIT_COUNT 4
#define PLACE_LABEL_SIZE 24
#define PLACE_LABEL_SUFFIX_X 16
#define PLACE_LABEL_LAST_PLACE 3

/*
 * func_8021508C writes a best time as minutes, seconds and hundredths, two
 * digits each, from the 16 by 16 CI4 glyphs of D_8023AA48: the digits follow a
 * first glyph, and a separator follows each group but the last
 */
#define TIME_GROUP_COUNT 3
#define TIME_GROUP_DIGIT_COUNT 2
#define TIME_DIGIT_BASE 10
#define HUNDREDTHS_PER_MINUTE (SECONDS_PER_MINUTE * HUNDREDTHS_PER_SECOND)
#define TIME_GLYPH_SIZE 16
#define TIME_GLYPH_BYTES (TIME_GLYPH_SIZE * TIME_GLYPH_SIZE / 2)
#define TIME_FIRST_DIGIT_GLYPH 1
#define TIME_SEPARATOR_GLYPH 11
#define TIME_DIGIT_SPACING 14
#define TIME_SEPARATOR_SHIFT_LEFT 4
#define TIME_SEPARATOR_SPACING 6

extern u16 D_800C2064[];
extern void* D_8012D430[CHARACTER_COUNT];
extern u16 D_802308A4_ovl_01[CHARACTER_COUNT];
extern s16 D_802308C8_ovl_01[CHARACTER_COUNT][CHARACTER_PALETTE_COUNT];
extern u8 D_8023BFF0_ovl_01[COURSE_COUNT * BEST_TIME_COUNT]
                           [MENU_SCENE_FILE_SIZE];
extern void** D_8023AA40_ovl_01;
extern s32 D_80230130_ovl_01;
extern s32 D_80230134_ovl_01;
extern s32 D_80230138_ovl_01;
extern void* D_8023AA44_ovl_01;
extern s32 D_8023013C_ovl_01[CHARACTER_COUNT];
extern char D_8022BF70_ovl_01[][IMPORTED_CHARACTER_NAME_LENGTH];

extern void* D_8023AA48_ovl_01;
extern void* D_8023AA4C_ovl_01;
extern void* D_8023AA50_ovl_01;

s32 GetOpenCourseCount(void);

void func_80213390_ovl_01(void** buffers) {
  EncodedAsset asset;

  s16 imageIndex;

  D_8023AA40_ovl_01 = buffers;
  for (imageIndex = 0; imageIndex != MENU_SCENE_IMAGE_COUNT; imageIndex++) {
    OpenEncodedAsset(D_800C2064[imageIndex], &asset);
    ReadEncodedAsset(D_8023AA40_ovl_01[imageIndex], &asset);
  }
}

void func_8021342C_ovl_01(void* buffer) {
  EncodedAsset asset;

  D_8023AA44_ovl_01 = buffer;
  OpenEncodedAsset(0x8E9, &asset);
  ReadEncodedAsset(D_8023AA44_ovl_01, &asset);
}

void func_80213464_ovl_01(void* buffer, s16 bufferKind) {
  EncodedAsset asset;

  switch (bufferKind) {
    case 1:
      D_8023AA44_ovl_01 = &g_WorkArea;
      OpenEncodedAsset(0x8E9, &asset);
      ReadEncodedAsset(D_8023AA44_ovl_01, &asset);
      break;
    case 2:
      D_8023AA48_ovl_01 = buffer;
      break;
    case 3:
      D_8023AA4C_ovl_01 = buffer;
      OpenEncodedAsset(0x688, &asset);
      ReadEncodedAsset(D_8023AA4C_ovl_01, &asset);
      break;
    case 4:
      D_8023AA50_ovl_01 = buffer;
      OpenEncodedAsset(0x720, &asset);
      ReadEncodedAsset(D_8023AA50_ovl_01, &asset);
      D_80230130_ovl_01 = 0;
      D_80230134_ovl_01 = 0;
      D_80230138_ovl_01 = 0;
      break;
  }
}

void func_80213558_ovl_01(void) {
  MenuMemory* memory = func_8005AF74();
  s32 courseCount = GetOpenCourseCount();
  s32 direction;
  s32 step;

  if (IsHudAnimationDone() == TRUE) {
    if (g_MenuPressedButtons & B_BUTTON) {
      StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
      PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
      StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
      g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
      return;
    }
    if (memory->bestTimesSpin == 0) {
      if (g_MenuRepeatButtons & R_JPAD) {
        if (courseCount >= 2) {
          PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          memory->bestTimesCourse++;
          if (memory->bestTimesCourse > courseCount - 1) {
            memory->bestTimesCourse = 0;
          }
          memory->bestTimesSpin = -BEST_TIMES_TURN / courseCount;
        }
      } else if (g_MenuRepeatButtons & L_JPAD) {
        if (courseCount >= 2) {
          PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          memory->bestTimesCourse--;
          if (memory->bestTimesCourse < 0) {
            memory->bestTimesCourse = courseCount - 1;
          }
          memory->bestTimesSpin = BEST_TIMES_TURN / courseCount;
        }
      }
    }
    if (ABS(memory->bestTimesSpin) != 0) {
      direction = memory->bestTimesSpin < 0 ? -1 : 1;
      step = BEST_TIMES_SPIN_SPEED / courseCount;
      if (step == 0) {
        step = 1;
      }
      memory->bestTimesSpin -= direction * step;
      /* Stop on reaching the course rather than turning past it */
      if ((memory->bestTimesSpin < 0 ? -1 : 1) != direction) {
        memory->bestTimesSpin = 0;
      }
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_80213748_ovl_01);

void func_802144E4_ovl_01(Gfx** displayListHead, s32 left, s32 top,
                          s32 character, u8 drawMode) {
  Gfx* gfx = *displayListHead;

  if (character < 0) {
    character = 0;
  }
  if (character >= CHARACTER_COUNT) {
    character = CHARACTER_COUNT - 1;
  }
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  if (drawMode == FACE_DARKENED) {
    gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
    gDPSetPrimColor(gfx++, 0, 0, FACE_DARKENED_SHADE, FACE_DARKENED_SHADE,
                    FACE_DARKENED_SHADE, 255);
  } else {
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  }
  gDPLoadTLUT_pal256(gfx++, ((u32)D_8023AA44_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  gDPLoadTextureBlock(
      gfx++,
      ((u32)D_8023AA44_ovl_01 + LARGE_PALETTED_TEXELS_OFFSET +
       D_8023013C_ovl_01[character] * FACE_WIDTH * FACE_HEIGHT) &
          ~7,
      G_IM_FMT_CI, G_IM_SIZ_8b, FACE_WIDTH, FACE_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, left << 2, top << 2, (left + FACE_WIDTH) << 2,
                          (top + FACE_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  if (drawMode == FACE_WITH_NAME) {
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    sprintf(g_PrintBuffer, "%s",
            D_8022BF70_ovl_01[D_8023013C_ovl_01[character]]);
    DrawLargeText(&gfx, left + FACE_NAME_OFFSET_X, top + FACE_NAME_OFFSET_Y,
                  g_PrintBuffer);
    gDPPipeSync(gfx++);
  }
  *displayListHead = gfx;
}

/*
 * Doesn't match: the original needs one register fewer for the display
 * list's pointers, so its frame is 16 bytes smaller.
 */
#ifdef NON_MATCHING
void func_802149A8_ovl_01(Gfx** displayListHead, s32 left, s32 top, s32 place) {
  Gfx* gfx = *displayListHead;

  if (place <= 0) {
    place = 1;
  }
  if (place > PLACE_LABEL_LAST_PLACE) {
    place = PLACE_LABEL_LAST_PLACE;
  }
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, ((u32)D_8023AA4C_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  gDPPipeSync(gfx++);
  gDPLoadTextureBlock(
      gfx++,
      ((u32)D_8023AA4C_ovl_01 + (place - 1) * PLACE_LABEL_FRAME_BYTES +
       LARGE_PALETTED_TEXELS_OFFSET) &
          ~7,
      G_IM_FMT_CI, G_IM_SIZ_8b, PLACE_LABEL_FRAME_SIZE, PLACE_LABEL_FRAME_SIZE,
      0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, left << 2, top << 2,
                          (left + PLACE_LABEL_SIZE) << 2,
                          (top + PLACE_LABEL_SIZE) << 2, G_TX_RENDERTILE, 0, 0,
                          (PLACE_LABEL_FRAME_SIZE << 10) / PLACE_LABEL_SIZE,
                          (PLACE_LABEL_FRAME_SIZE << 10) / PLACE_LABEL_SIZE);
  gDPPipeSync(gfx++);
  gDPLoadTextureBlock(
      gfx++,
      ((u32)D_8023AA4C_ovl_01 +
       (place + PLACE_LABEL_DIGIT_COUNT - 1) * PLACE_LABEL_FRAME_BYTES +
       LARGE_PALETTED_TEXELS_OFFSET) &
          ~7,
      G_IM_FMT_CI, G_IM_SIZ_8b, PLACE_LABEL_FRAME_SIZE, PLACE_LABEL_FRAME_SIZE,
      0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, (left + PLACE_LABEL_SUFFIX_X) << 2, top << 2,
                          (left + PLACE_LABEL_SUFFIX_X + PLACE_LABEL_SIZE) << 2,
                          (top + PLACE_LABEL_SIZE) << 2, G_TX_RENDERTILE, 0, 0,
                          (PLACE_LABEL_FRAME_SIZE << 10) / PLACE_LABEL_SIZE,
                          (PLACE_LABEL_FRAME_SIZE << 10) / PLACE_LABEL_SIZE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_802149A8_ovl_01);
#endif

void func_8021508C_ovl_01(Gfx** displayListHead, s32 left, s32 top,
                          s32 frameCount) {
  s32 digits[TIME_GROUP_COUNT][TIME_GROUP_DIGIT_COUNT];
  Gfx* gfx = *displayListHead;
  s32 hundredths = frameCount * HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND;
  s32 minutes = hundredths / HUNDREDTHS_PER_MINUTE;
  s32 seconds;
  u16 separatorShift;
  s16 group;
  s16 digit;

  hundredths %= HUNDREDTHS_PER_MINUTE;
  seconds = hundredths / HUNDREDTHS_PER_SECOND;
  hundredths %= HUNDREDTHS_PER_SECOND;
  digits[0][0] = minutes / TIME_DIGIT_BASE;
  digits[0][1] = minutes % TIME_DIGIT_BASE;
  digits[1][0] = seconds / TIME_DIGIT_BASE;
  digits[1][1] = seconds % TIME_DIGIT_BASE;
  digits[2][0] = hundredths / TIME_DIGIT_BASE;
  digits[2][1] = hundredths % TIME_DIGIT_BASE;
  separatorShift = 0;
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal16(gfx++, 0,
                    ((u32)D_8023AA48_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  for (group = 0; group != TIME_GROUP_COUNT; group++) {
    for (digit = 0; digit != TIME_GROUP_DIGIT_COUNT; digit++) {
      gDPLoadTextureBlock_4b(
          gfx++,
          ((u32)D_8023AA48_ovl_01 + PALETTED_TEXELS_OFFSET +
           (digits[group][digit] + TIME_FIRST_DIGIT_GLYPH) * TIME_GLYPH_BYTES) &
              ~7,
          G_IM_FMT_CI, TIME_GLYPH_SIZE, TIME_GLYPH_SIZE, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(
          gfx++,
          (separatorShift + left +
           (group * TIME_GROUP_DIGIT_COUNT + digit) * TIME_DIGIT_SPACING)
              << 2,
          top << 2,
          (separatorShift + left +
           (group * TIME_GROUP_DIGIT_COUNT + digit) * TIME_DIGIT_SPACING +
           TIME_GLYPH_SIZE)
              << 2,
          (top + TIME_GLYPH_SIZE) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
          TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
    if (group != TIME_GROUP_COUNT - 1) {
      gDPLoadTextureBlock_4b(
          gfx++,
          ((u32)D_8023AA48_ovl_01 + TIME_SEPARATOR_GLYPH * TIME_GLYPH_BYTES +
           PALETTED_TEXELS_OFFSET) &
              ~7,
          G_IM_FMT_CI, TIME_GLYPH_SIZE, TIME_GLYPH_SIZE, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(
          gfx++,
          (separatorShift + left +
           (group * TIME_GROUP_DIGIT_COUNT + TIME_GROUP_DIGIT_COUNT) *
               TIME_DIGIT_SPACING -
           TIME_SEPARATOR_SHIFT_LEFT)
              << 2,
          top << 2,
          (separatorShift + left +
           (group * TIME_GROUP_DIGIT_COUNT + TIME_GROUP_DIGIT_COUNT) *
               TIME_DIGIT_SPACING -
           TIME_SEPARATOR_SHIFT_LEFT + TIME_GLYPH_SIZE)
              << 2,
          (top + TIME_GLYPH_SIZE) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
          TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
      separatorShift += TIME_SEPARATOR_SPACING;
    }
  }
  *displayListHead = gfx;
}

void func_8021585C_ovl_01(void) {
  EncodedAsset asset;
  AssetFile file;
  SaveData* save = GetSaveData();
  s32 characterIndex;
  s32 courseIndex;
  s32 rank;
  s32 character;
  s32 palette;

  for (characterIndex = 0; characterIndex != CHARACTER_COUNT;
       characterIndex++) {
    OpenEncodedAsset(D_802308A4_ovl_01[characterIndex], &asset);
    ReadEncodedAsset(D_8012D430[characterIndex], &asset);
  }
  for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
    for (rank = 0; rank != BEST_TIME_COUNT; rank++) {
      character = save->unkE08[courseIndex][rank];
      if (character != -1) {
        if (character < 0) {
          character = 0;
        }
        if (character >= CHARACTER_COUNT) {
          character = CHARACTER_COUNT - 1;
        }
        palette = save->unkE50[courseIndex][rank];
      } else {
        character = 0;
        palette = 0;
      }
      OpenAssetFile(D_802308C8_ovl_01[character][palette], &file);
      ReadAssetFile(D_8023BFF0_ovl_01[courseIndex * BEST_TIME_COUNT + rank],
                    GetAssetFileSize(&file), &file);
    }
  }
}
