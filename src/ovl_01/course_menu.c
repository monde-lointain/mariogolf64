/*
 * The clubhouse's course statistics: a wheel of the open courses that turns
 * from one to the next, and the chosen course's panel of statistics.
 */
#include "common.h"

#include "asset.h"
#include "course.h"
#include "cpu_projection.h"
#include "game.h"
#include "hole_display.h"
#include "libc.h"
#include "main_menu.h"
#include "math_util.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"
#include "screen.h"
#include "sound.h"
#include "text.h"
#include "variables.h"

/* A gold coin with a star, reading HOLE IN ONE */
#define ASSET_HOLE_IN_ONE_BADGE 0x8EA
/* Orange 1 to 6, then up and down arrows; the course stats draw only the
 * arrows */
#define ASSET_DIGITS_AND_ARROWS 0x5A5

/* The images of g_CourseIconAssets that the course menu reads */
#define COURSE_MENU_IMAGE_COUNT 7

/* LoadCourseTrophyImages reads the chosen course's first image into two
 * buffers, and its three files */
#define COURSE_IMAGE_COPY_COUNT 2

/* DrawCourseBanner squeezes the banner's strips under the course menu's title,
 * with the course's name on it */
#define COURSE_BANNER_TOP 110
#define COURSE_BANNER_STRIP_SPACING 8
#define COURSE_NAME_Y 116

/* The course's statistics: the panel behind them, and the rows of it that show
 */
/* How far UpdateCourseStats turns the course wheel each move, and how fast */
#define STATS_TURN 90
#define STATS_SPIN_SPEED 18

#define STATS_COUNT 10
#define STATS_SHOWN_ROW_COUNT 4
#define STATS_PANEL_LEFT 24
#define STATS_PANEL_TOP 138
#define STATS_PANEL_RIGHT 302
#define STATS_PANEL_BOTTOM 230
#define STATS_LABEL_X 30
#define STATS_FIRST_Y 154
#define STATS_LINE_HEIGHT 16
/* The statistics in the order the panel lists them */
#define STAT_ROUND_COUNT 0
#define STAT_BEST_SCORE 1
#define STAT_AVERAGE_SCORE 2
#define STAT_BEST_DRIVE 3
#define STAT_AVERAGE_DRIVE 4
#define STAT_FAIRWAY_PERCENT 5
#define STAT_AVERAGE_PUTTS 6
#define STAT_PAR_SAVE_PERCENT 7
#define STAT_PAR_ON_PERCENT 8
#define STAT_SAND_SAVE_PERCENT 9
/* Where DrawStatFigure draws the figures; shorter best scores move right to
 * line up */
#define STATS_ROUND_COUNT_X 96
#define STATS_VALUE_X 112
#define STATS_TWO_DIGIT_X 128
#define STATS_ONE_DIGIT_X 144
#define STATS_UNIT_X 178
#define STATS_UNIT_OFFSET_Y 4
/* The 16x16 CI4 glyphs of ASSET_LARGE_DIGITS in D_8023A82C; two of them spell
 * "OFF" */
#define MENU_GLYPH_SIZE 16
#define MENU_GLYPH_IMAGE_SIZE (MENU_GLYPH_SIZE * MENU_GLYPH_SIZE / 2)
#define MENU_GLYPH_TIMES 0
#define MENU_GLYPH_FIRST_DIGIT 1
#define MENU_GLYPH_PLUS 12
#define MENU_GLYPH_MINUS 13
#define MENU_GLYPH_PLUS_MINUS 14
#define MENU_GLYPH_CIRCLE 15
#define OFF_FIRST_GLYPH 16
#define OFF_GLYPH_COUNT 2
#define MENU_GLYPH_POINT 18
/* The largest number written in one decimal digit, and in two */
#define MAX_ONE_DIGIT_NUMBER (DECIMAL_BASE - 1)
#define MAX_TWO_DIGIT_NUMBER ((DECIMAL_BASE * DECIMAL_BASE) - 1)
/* Whether a signed number's magnitude takes one digit, and two */
#define IsOneDigitNumber(n) \
  ((n) >= -MAX_ONE_DIGIT_NUMBER && (n) <= MAX_ONE_DIGIT_NUMBER)
#define IsTwoDigitNumber(n) \
  (((n) >= DECIMAL_BASE && (n) <= MAX_TWO_DIGIT_NUMBER) || \
      ((n) >= -MAX_TWO_DIGIT_NUMBER && (n) <= -DECIMAL_BASE))

/* The arrows beside the statistics that show there are more rows above or below
 */
#define STATS_FIRST_ARROW_IMAGE 16
#define STATS_ARROW_WIDTH 16
#define STATS_ARROW_HEIGHT 9
#define STATS_ARROW_IMAGE_SIZE (STATS_ARROW_WIDTH * STATS_ARROW_HEIGHT / 2)
#define STATS_ARROW_LEFT 32
#define STATS_UP_ARROW_TOP 144
#define STATS_DOWN_ARROW_TOP 216

/* The blinking arrows that DrawBlinkingSideArrows draws either side of a box,
 * from the third image on */
#define SIDE_ARROW_COUNT 2
#define SIDE_ARROW_RIGHT 0
#define SIDE_ARROW_LEFT 1
#define SIDE_ARROW_FIRST_IMAGE MENU_ARROW_RIGHT
#define SIDE_ARROW_BLINK_FRAME_COUNT 4

/* The birdie badge icon of D_8023A810 and the count beside it */
#define BADGE_ICON_X 222
#define BADGE_ICON_Y 144
#define BADGE_COUNT_X 250
#define BADGE_COUNT_Y 146
#define BADGE_COUNT_DIGIT_WIDTH 16

/* DrawCourseTrophies draws the course's tournament trophies, spinning, or on
 * the totals page the course points */
#define TROPHY_SPACING 24
#define TROPHY_SHIFT_LEFT 20
#define TROPHY_FRAME_DELAY 4
#define POINTS_NEEDED_X 192
#define POINTS_NEEDED_Y 174
#define ALL_COURSES_OPEN_X 190
#define ALL_COURSES_OPEN_Y 180
#define MESSAGE_POINTS_TO_NEXT_COURSE 0x163
#define MESSAGE_ALL_COURSES_OPEN 0x164

typedef struct TrophyPosition {
  s32 x;
  u32 y;
} TrophyPosition;

extern const char D_802318A8_ovl_01[];
extern const char D_802318AC_ovl_01[];

s32 GetOpenCourseCount(void);
void DrawStatFigure(Gfx** displayListHead, s16 statIndex, u8 row);
void DrawSpinningBadge(Gfx** displayListHead, void* image, s32 x, s32 y);
void DrawThreeDigitMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void DrawFourDigitMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void DrawUnalignedMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void DrawDecimalMenuNumber(Gfx** displayListHead, s32 x, s32 y, f32 value);
void DrawMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark, u8 field);

static s16 D_802300A0_ovl_01 = 0;
static s16 D_802300A2_ovl_01 = 0;
/* Which of D_8023A814's buffers holds the course image shown */
static s16 D_802300A4_ovl_01 = 0;
static s16 D_802300A6_ovl_01 = 0;
static s32 D_802300A8_ovl_01 = 0;
static s32 D_802300AC_ovl_01 = 0;
/* Read by no routine */
static s32 D_802300B0_ovl_01 = 0;
static const char* D_802300B4_ovl_01[STATS_COUNT] = {"Round", "Best Score",
    "Average", "Best Drive", "Avg. Drive", "On Fairway", "Avg. Putt",
    "Par Saves", "Par Ons", "Sand Saves"};
static f32 D_802300DC_ovl_01 = 0.0f;
/* Read by no routine */
static char D_802300E0_ovl_01[3][10] = {"EXP", "ｺｰｽｹﾞｯﾄ", "純ﾞ"};
static TrophyPosition D_80230100_ovl_01[COURSE_COUNT] = {
    {230, 177}, {230, 177}, {230, 177}, {236, 165}, {226, 184}, {236, 175}};

static Mat4 D_8023A790_ovl_01;
static Mat4 D_8023A7D0_ovl_01;
static void* D_8023A810_ovl_01;
static void** D_8023A814_ovl_01;
static void** D_8023A818_ovl_01;
static void** D_8023A81C_ovl_01;
static void** D_8023A820_ovl_01;
static void* D_8023A824_ovl_01;
static void* D_8023A828_ovl_01;
static void* D_8023A82C_ovl_01;
static void* D_8023A830_ovl_01;

/* Read by no routine; 2 objects, as one would be aligned past its start */
static u8 D_8023A834_ovl_01[0x4];
static u8 D_8023A838_ovl_01[0x188];

/* Reads every course's icon into the course stats' buffers */
void LoadCourseStatsIcons(void** buffers) {
  EncodedAsset asset;
  s32 unused[4];
  s16 imageIndex;

  D_8023A820_ovl_01 = buffers;
  for (imageIndex = 0; imageIndex != COURSE_MENU_IMAGE_COUNT; imageIndex++) {
    OpenEncodedAsset(g_CourseIconAssets[imageIndex], &asset);
    ReadEncodedAsset(D_8023A820_ovl_01[imageIndex], &asset);
  }
}

/*
 * Keeps the buffers as the kind's set, then reads the chosen course's first
 * trophy image into both of them and the trophy's three place palettes
 */
void LoadCourseTrophyImages(void** buffers, s32 requestedKind) {
  EncodedAsset asset;
  AssetFile file;
  s8 bufferKind = requestedKind;
  MenuMemory* memory = GetMenuMemory();
  s16 index;

  if ((s8)requestedKind < 0) {
    bufferKind = COURSE_BUFFER_FIRST;
  }

  if (bufferKind > COURSE_BUFFER_LAST) {
    bufferKind = COURSE_BUFFER_LAST;
  }

  switch (bufferKind) {
    case COURSE_BUFFER_FIRST:
      D_8023A814_ovl_01 = buffers;
      break;
    case COURSE_BUFFER_SECOND:
      D_8023A818_ovl_01 = buffers;
      break;
    case COURSE_BUFFER_LAST:
      D_8023A81C_ovl_01 = buffers;
      break;
  }

  if (memory->statsCourse != COURSE_COUNT) {
    for (index = 0; index != COURSE_IMAGE_COPY_COUNT; index++) {
      OpenEncodedAsset(g_TrophyFrameAssets[memory->statsCourse][0], &asset);
      ReadEncodedAsset(buffers[index], &asset);
    }

    for (index = 0; index != COURSE_FILE_COUNT; index++) {
      OpenAssetFile(g_TrophyPaletteFiles[memory->statsCourse][index], &file);
      ReadAssetFile(g_TrophyPalettes[index], GetAssetFileSize(&file), &file);
    }
  }
}

void LoadCourseMenuBuffer(void* buffer, s16 bufferKind) {
  EncodedAsset asset;

  switch (bufferKind) {
    /* The course menu keeps no buffer of kind 1 */
    case COURSE_MENU_BUFFER_UNUSED:
      break;
    case COURSE_MENU_BUFFER_HOLE_IN_ONE_BADGE:
      D_8023A810_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_HOLE_IN_ONE_BADGE, &asset);
      ReadEncodedAsset(D_8023A810_ovl_01, &asset);
      break;
    case COURSE_MENU_BUFFER_NAME_PLATE:
      D_8023A824_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_NAME_PLATE, &asset);
      ReadEncodedAsset(D_8023A824_ovl_01, &asset);
      break;
    case COURSE_MENU_BUFFER_SPINNING_BADGE:
      D_8023A810_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_SPINNING_BADGE, &asset);
      ReadEncodedAsset(D_8023A810_ovl_01, &asset);
      break;
    case COURSE_MENU_BUFFER_DIGITS_AND_ARROWS:
      D_8023A828_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_DIGITS_AND_ARROWS, &asset);
      ReadEncodedAsset(D_8023A828_ovl_01, &asset);
      break;
    case COURSE_MENU_BUFFER_LARGE_DIGITS:
      D_8023A82C_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_LARGE_DIGITS, &asset);
      ReadEncodedAsset(D_8023A82C_ovl_01, &asset);
      break;
    case COURSE_MENU_BUFFER_GREEN_ARROWS:
      D_8023A830_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_GREEN_ARROWS, &asset);
      ReadEncodedAsset(D_8023A830_ovl_01, &asset);
      D_802300A8_ovl_01 = 0;
      D_802300AC_ovl_01 = 0;
      break;
  }
}

void UpdateCourseStats(void) {
  EncodedAsset asset;
  AssetFile file;
  MenuMemory* memory = GetMenuMemory();
  s32 courseCount = GetOpenCourseCount();
  Boolean isDone;
  s32 index;
  s32 direction;
  s32 step;

  if (courseCount < 1) {
    courseCount = 1;
  }

  if (courseCount > COURSE_COUNT) {
    courseCount = COURSE_COUNT;
  }

  isDone = IsHudAnimationDone();
  if (isDone == TRUE) {
    if (g_MenuPressedButtons & B_BUTTON) {
      StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
      PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED,
          255, 255, 255, 255);
      g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
      return;
    }

    if (memory->statsSpin == 0) {
      if (g_MenuRepeatButtons & R_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED,
            255, 255, 255, 255);
        memory->statsCourse++;
        if (memory->statsCourse > courseCount) {
          memory->statsCourse = 0;
        }

        memory->statsSpin = -STATS_TURN / (courseCount + 1);
        if (memory->statsCourse != courseCount) {
          D_802300A6_ovl_01 = 0;
          D_802300A2_ovl_01 = 0;
          D_802300A4_ovl_01 ^= 1;
          OpenEncodedAsset(g_TrophyFrameAssets[memory->statsCourse][0], &asset);
          ReadEncodedAsset(D_8023A814_ovl_01[D_802300A4_ovl_01], &asset);
          for (index = 0; index != COURSE_FILE_COUNT; index++) {
            OpenAssetFile(
                g_TrophyPaletteFiles[memory->statsCourse][index], &file);
            ReadAssetFile(
                g_TrophyPalettes[index], GetAssetFileSize(&file), &file);
          }
        }
      } else if (g_MenuRepeatButtons & L_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED,
            255, 255, 255, 255);
        memory->statsCourse--;
        if (memory->statsCourse < 0) {
          memory->statsCourse = courseCount;
        }

        memory->statsSpin = STATS_TURN / (courseCount + 1);
        if (memory->statsCourse != courseCount) {
          D_802300A6_ovl_01 = 0;
          D_802300A2_ovl_01 = 0;
          D_802300A4_ovl_01 ^= 1;
          OpenEncodedAsset(g_TrophyFrameAssets[memory->statsCourse][0], &asset);
          ReadEncodedAsset(D_8023A814_ovl_01[D_802300A4_ovl_01], &asset);
          for (index = 0; index != COURSE_FILE_COUNT; index++) {
            OpenAssetFile(
                g_TrophyPaletteFiles[memory->statsCourse][index], &file);
            ReadAssetFile(
                g_TrophyPalettes[index], GetAssetFileSize(&file), &file);
          }
        }
      } else if (g_MenuRepeatButtons & U_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (D_802300A0_ovl_01 > 0) {
          D_802300A0_ovl_01--;
        }
      } else if (g_MenuRepeatButtons & D_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (D_802300A0_ovl_01 < STATS_COUNT - STATS_SHOWN_ROW_COUNT) {
          D_802300A0_ovl_01++;
        }
      }
    }

    if (Abs(memory->statsSpin) != 0) {
      direction = memory->statsSpin < 0 ? -1 : 1;
      step = STATS_SPIN_SPEED / (courseCount + 1);
      if (step == 0) {
        step = 1;
      }

      memory->statsSpin -= direction * step;

      /* Stop on reaching the course rather than turning past it */
      if ((memory->statsSpin < 0 ? -1 : 1) != direction) {
        memory->statsSpin = 0;
      }
    }
  }
}

/* DrawCourseStats turns a totals image on the wheel of menu.h after the open
 * courses' images */
#define COURSE_TOTALS_IMAGE COURSE_COUNT

/*
 * Doesn't match: the original copies the wheel's slot count to a saved register
 * after working out the spin's angle, where GCC copies it first (2 diff lines,
 * no opcode differences). Putting the spin first in the angle's sum moves the
 * copy but swaps two registers and the sum's operands.
 */
#ifdef NON_MATCHING
void DrawCourseStats(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  u16 perspNorm;
  f32 iconX[WHEEL_SLOT_LIMIT];
  f32 iconY[WHEEL_SLOT_LIMIT];
  f32 iconScale[WHEEL_SLOT_LIMIT];
  s32 unusedSlots[WHEEL_SLOT_LIMIT];
  s32 drawOrder[WHEEL_SLOT_LIMIT];
  s32 unused[2];
  MenuMemory* memory = GetMenuMemory();
  s32 courseCount;
  s32 index;
  s32 sortIndex;
  s32 key;
  s32 statsCourse;
  s32 spin;
  s32 angleStep;
  Radians angle;
  f32 wheelX;
  s32 image;
  s32 slot;
  s32 halfSize;
  s32 step;

  DrawMenuBackdropLayers(&gfx, PLAIN_BACKDROP_LAYERS);
  guPerspectiveF(D_8023A790_ovl_01, &perspNorm, WHEEL_FIELD_OF_VIEW,
      SCREEN_ASPECT_RATIO, WHEEL_NEAR_PLANE, WHEEL_FAR_PLANE, 1.0f);
  guLookAtF(D_8023A7D0_ovl_01, 0.0f, WHEEL_EYE_HEIGHT, WHEEL_EYE_DISTANCE, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
  SetCpuProjectionF(D_8023A790_ovl_01, D_8023A7D0_ovl_01, g_Viewports);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  courseCount = GetOpenCourseCount();
  if (courseCount <= 0) {
    courseCount = 1;
  }

  if (courseCount > COURSE_COUNT) {
    courseCount = COURSE_COUNT;
  }

  statsCourse = memory->statsCourse;
  spin = memory->statsSpin;
  angleStep = DEGREES_PER_TURN / (courseCount + 1);
  for (index = 0; index != courseCount + 1; index++) {
    ScreenPoint point;

    angle = -DegreesToRadians(
        (((statsCourse - index) % (courseCount + 1)) * angleStep) +
        (spin * DEGREES_PER_SPIN_STEP));
    wheelX = sinf(angle) * WHEEL_RADIUS_X;
    if (ProjectPoint(wheelX, 0.0f, cosf(angle) * WHEEL_RADIUS_Z, &point) < 0) {
      iconX[index] = point.x;
      iconY[index] = point.y - WHEEL_RAISE;
      iconScale[index] = point.scaleX;
    }
  }

  for (sortIndex = 0; sortIndex != courseCount + 1; sortIndex++) {
    drawOrder[sortIndex] = sortIndex;
  }

  /* Draw the farthest images first, so the nearer ones cover them */
  for (sortIndex = 1; sortIndex != courseCount + 1; sortIndex++) {
    key = drawOrder[sortIndex];
    for (index = sortIndex - 1;
        index >= 0 && iconY[drawOrder[index]] > iconY[key]; index--) {
      drawOrder[index + 1] = drawOrder[index];
    }

    drawOrder[index + 1] = key;
  }

  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PRIM);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

  /* Nothing here reads D_802300DC */
  D_802300DC_ovl_01 += 0.1f;
  for (index = 0; index != courseCount + 1; index++) {
    slot = drawOrder[index];
    image = (slot == courseCount) ? COURSE_TOTALS_IMAGE : slot;
    halfSize = iconScale[slot] * COURSE_ICON_HALF_SIZE;
    step = (f32)(COURSE_ICON_STRIP_HEIGHT << 10) / halfSize;
    gDPLoadTLUT_pal256(
        gfx++, TextureAddress(ImageBody(D_8023A820_ovl_01[image])));
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    for (sortIndex = 0; sortIndex != COURSE_ICON_STRIP_COUNT; sortIndex++) {
      gDPLoadTextureBlock(gfx++,
          TextureAddress(LargeImageTexels(D_8023A820_ovl_01[image]) +
              sortIndex * COURSE_ICON_WIDTH * COURSE_ICON_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, COURSE_ICON_WIDTH, COURSE_ICON_STRIP_HEIGHT,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, (s32)(iconX[slot] - halfSize) << 2,
          QuarterPixels((s32)(iconY[slot] - halfSize + (sortIndex * halfSize)) -
              WHEEL_ICON_RAISE),
          (s32)(iconX[slot] + halfSize) << 2,
          QuarterPixels(
              (s32)(iconY[slot] + (sortIndex * halfSize)) - WHEEL_ICON_RAISE),
          G_TX_RENDERTILE, 0, 0, step, step);
      gDPPipeSync(gfx++);
    }
  }

  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  DrawBlinkingSideArrows(&gfx, D_8023A830_ovl_01, &D_802300A8_ovl_01,
      &D_802300AC_ovl_01, WHEEL_ARROW_WIDTH, WHEEL_ARROW_X, WHEEL_ARROW_Y);
  image = (memory->statsCourse == courseCount) ? COURSE_TOTALS_IMAGE
                                               : memory->statsCourse;
  DrawCourseBanner(&gfx, 0, image);
  DrawStatsPanel(&gfx);
  DrawStatsBadgeCount(&gfx);
  DrawStatsScrollArrows(&gfx);
  DrawCourseTrophies(&gfx);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", DrawCourseStats);
#endif

/*
 * Draws the name plate banner squeezed under the screen's title, with the
 * course's number and its tournament's name, or with the course's own name if
 * useCourseNames is set. The totals, after the last course, get no number.
 */
void DrawCourseBanner(
    Gfx** displayListHead, u8 useCourseNames, s32 courseIndex) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;
  s32 stripTop;

  GetMenuMemory();
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(D_8023A824_ovl_01)));
  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(D_8023A824_ovl_01) +
            (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)),
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = COURSE_BANNER_TOP + (stripIndex * COURSE_BANNER_STRIP_SPACING);
    gSPScisTextureRectangle(gfx++, QuarterPixels(MENU_BANNER_LEFT),
        QuarterPixels(stripTop),
        QuarterPixels(MENU_BANNER_LEFT + MENU_BANNER_WIDTH),
        QuarterPixels(stripTop + COURSE_BANNER_STRIP_SPACING), G_TX_RENDERTILE,
        0, 0, TEXEL_STEP_ONE,
        (MENU_BANNER_STRIP_HEIGHT << 10) / COURSE_BANNER_STRIP_SPACING);
    gDPPipeSync(gfx++);
  }

  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  if (!useCourseNames) {
    if (courseIndex + 1 == COURSE_COUNT + 1) {
      sprintf(g_PrintBuffer, D_802318A8_ovl_01, g_TournamentNames[courseIndex]);
    } else {
      sprintf(g_PrintBuffer, D_802318AC_ovl_01, courseIndex + 1,
          g_TournamentNames[courseIndex]);
    }
  } else if (courseIndex + 1 == COURSE_COUNT + 1) {
    sprintf(
        g_PrintBuffer, D_802318A8_ovl_01, g_CourseRecords[courseIndex].name);
  } else {
    sprintf(g_PrintBuffer, D_802318AC_ovl_01, courseIndex + 1,
        g_CourseRecords[courseIndex].name);
  }

  DrawCenteredLargeText(&gfx, SCREEN_WIDTH / 2, COURSE_NAME_Y, g_PrintBuffer);
  *displayListHead = gfx;
}

const char D_802318A8_ovl_01[] = "%s";
const char D_802318AC_ovl_01[] = "%d:%s";

/*
 * Draws the dimmed panel of the course's statistics, with the label and figure
 * of each shown row
 */
void DrawStatsPanel(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 row = 0;
  s16 statIndex;

  GetSaveData();
  GetMenuMemory();
  GetOpenCourseCount();
  gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
  gDPPipeSync(gfx++);
  gSPTextureRectangle(gfx++, QuarterPixels(STATS_PANEL_LEFT),
      QuarterPixels(STATS_PANEL_TOP), QuarterPixels(STATS_PANEL_RIGHT),
      QuarterPixels(STATS_PANEL_BOTTOM), G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
      TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  for (statIndex = D_802300A0_ovl_01;
      statIndex != D_802300A0_ovl_01 + STATS_SHOWN_ROW_COUNT; statIndex++) {
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    sprintf(g_PrintBuffer, D_802318A8_ovl_01, D_802300B4_ovl_01[statIndex]);
    DrawLargeText(&gfx, STATS_LABEL_X,
        STATS_FIRST_Y + ((s16)row * STATS_LINE_HEIGHT), g_PrintBuffer);
    DrawStatFigure(&gfx, statIndex, row);
    row++;
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/*
 * Draws the statistic's figure for the chosen course, or for all courses on the
 * totals, on the panel's row, with its unit; nothing shows before any round has
 * been played
 */
void DrawStatFigure(Gfx** displayListHead, s16 statIndex, u8 row) {
  Gfx* gfx = *displayListHead;
  MenuMemory* memory;
  s32 courseIndex;
  s32 bestScore;
  s32 openCourseCount;

  GetSaveData();
  memory = GetMenuMemory();
  openCourseCount = GetOpenCourseCount();
  if (memory->statsCourse != openCourseCount) {
    courseIndex = memory->statsCourse;
  } else {
    /* The figures for all the courses together */
    courseIndex = COURSE_COUNT;
  }

  if (g_CourseStatFigures[courseIndex].roundCount == 0.0f) {
    return;
  }

  switch (statIndex) {
    case STAT_ROUND_COUNT:
      DrawFourDigitMenuNumber(&gfx, STATS_ROUND_COUNT_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
          g_CourseStatFigures[courseIndex].roundCount, MENU_NUMBER_PLAIN);
      break;
    case STAT_BEST_SCORE:
      bestScore = g_CourseStatFigures[courseIndex].bestScore;
      if (IsOneDigitNumber(bestScore)) {
        DrawUnalignedMenuNumber(&gfx, STATS_ONE_DIGIT_X,
            STATS_FIRST_Y + (row * STATS_LINE_HEIGHT), bestScore,
            MENU_NUMBER_SIGNED);
      } else if (IsTwoDigitNumber(bestScore)) {
        DrawUnalignedMenuNumber(&gfx, STATS_TWO_DIGIT_X,
            STATS_FIRST_Y + (row * STATS_LINE_HEIGHT), bestScore,
            MENU_NUMBER_SIGNED);
      } else {
        DrawThreeDigitMenuNumber(&gfx, STATS_VALUE_X,
            STATS_FIRST_Y + (row * STATS_LINE_HEIGHT), bestScore,
            MENU_NUMBER_SIGNED);
      }

      break;
    case STAT_AVERAGE_SCORE:
      DrawDecimalMenuNumber(&gfx, STATS_VALUE_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
          g_CourseStatFigures[courseIndex].averageScore);
      break;
    case STAT_BEST_DRIVE:
      DrawDecimalMenuNumber(&gfx, STATS_VALUE_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
          g_CourseStatFigures[courseIndex].bestDrive);
      gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
      DrawYardMark(&gfx, STATS_UNIT_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT) + STATS_UNIT_OFFSET_Y);
      break;
    case STAT_AVERAGE_DRIVE:
      DrawDecimalMenuNumber(&gfx, STATS_VALUE_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
          g_CourseStatFigures[courseIndex].averageDrive);
      DrawYardMark(&gfx, STATS_UNIT_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT) + STATS_UNIT_OFFSET_Y);
      break;
    case STAT_FAIRWAY_PERCENT:
      DrawDecimalMenuNumber(&gfx, STATS_VALUE_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
          g_CourseStatFigures[courseIndex].fairwayPercent);
      DrawPercentMark(&gfx, STATS_UNIT_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT) + STATS_UNIT_OFFSET_Y);
      break;
    case STAT_AVERAGE_PUTTS:
      DrawDecimalMenuNumber(&gfx, STATS_VALUE_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
          g_CourseStatFigures[courseIndex].averagePutts);
      break;
    case STAT_PAR_SAVE_PERCENT:
      DrawDecimalMenuNumber(&gfx, STATS_VALUE_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
          g_CourseStatFigures[courseIndex].parSavePercent);
      DrawPercentMark(&gfx, STATS_UNIT_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT) + STATS_UNIT_OFFSET_Y);
      break;
    case STAT_PAR_ON_PERCENT:
      DrawDecimalMenuNumber(&gfx, STATS_VALUE_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
          g_CourseStatFigures[courseIndex].parOnPercent);
      DrawPercentMark(&gfx, STATS_UNIT_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT) + STATS_UNIT_OFFSET_Y);
      break;
    case STAT_SAND_SAVE_PERCENT:
      DrawDecimalMenuNumber(&gfx, STATS_VALUE_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
          g_CourseStatFigures[courseIndex].sandSavePercent);
      DrawPercentMark(&gfx, STATS_UNIT_X,
          STATS_FIRST_Y + (row * STATS_LINE_HEIGHT) + STATS_UNIT_OFFSET_Y);
      break;
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

const char D_802318E0_ovl_01[] = "%c";

/* Draws the green Y that marks a figure in yards */
void DrawYardMark(Gfx** displayListHead, s16 x, s16 y) {
  Gfx* gfx = *displayListHead;

  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 63, 255, 63);
  sprintf(g_PrintBuffer, D_802318E0_ovl_01, 'Y');
  DrawLargeText(&gfx, x, y, g_PrintBuffer);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* Draws the pink percent sign after a percentage */
void DrawPercentMark(Gfx** displayListHead, s16 x, s16 y) {
  Gfx* gfx = *displayListHead;

  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 255, 64, 192);
  sprintf(g_PrintBuffer, D_802318E0_ovl_01, '%');
  DrawLargeText(&gfx, x, y, g_PrintBuffer);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/*
 * Draws the spinning badge and how many of the course's holes, or of every
 * course's on the totals, have a birdie badge.
 * Doesn't match: the original keeps the badge count in a1 and copies it to s1
 * after counting
 */
#ifdef NON_MATCHING
void DrawStatsBadgeCount(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  SaveData* save = GetSaveData();
  MenuMemory* memory = GetMenuMemory();
  s16 badgeCount = 0;
  s16 index; /* A hole, then a course, then where the count moves to */
  s16 holeIndex;

  if (GetOpenCourseCount() != memory->statsCourse) {
    for (index = 0; index != COURSE_HOLE_COUNT; index++) {
      if (save->awards.birdieBadgeCounts[memory->statsCourse][index]) {
        badgeCount++;
      }
    }
  } else {
    for (index = 0; index != COURSE_COUNT; index++) {
      for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
        if (save->awards.birdieBadgeCounts[index][holeIndex]) {
          badgeCount++;
        }
      }
    }
  }

  /* Centers the count by its number of digits */
  if (badgeCount < 10) {
    index = BADGE_COUNT_DIGIT_WIDTH;
  }

  if (badgeCount >= 10 && badgeCount < 100) {
    index = 0;
  }

  if (badgeCount >= 100) {
    index = -BADGE_COUNT_DIGIT_WIDTH;
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  DrawSpinningBadge(
      &gfx, D_8023A810_ovl_01, BADGE_ICON_X + index, BADGE_ICON_Y);
  DrawUnalignedMenuNumber(&gfx, BADGE_COUNT_X + index, BADGE_COUNT_Y,
      badgeCount, MENU_NUMBER_TIMES);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", DrawStatsBadgeCount);
#endif

/*
 * Draws the arrows beside the panel that show there are more statistics above
 * or below.
 * Doesn't match: the original counts the arrows and picks the first one with
 * branches, where this version computes them without, so the registers and the
 * spills to the stack differ.
 */
#ifdef NON_MATCHING
void DrawStatsScrollArrows(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  Boolean hasRowsAbove = D_802300A0_ovl_01 > 0;
  Boolean hasRowsBelow =
      D_802300A0_ovl_01 + STATS_SHOWN_ROW_COUNT < STATS_COUNT;
  s16 arrowCount = hasRowsAbove;
  s32 firstArrow;
  s16 arrowIndex;
  s32 top;
  s32 unused[4];

  if (hasRowsBelow) {
    arrowCount++;
  }

  if (arrowCount != 0) {
    firstArrow = 0;
    if (arrowCount == 1) {
      firstArrow = hasRowsBelow;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(D_8023A828_ovl_01)));
    for (arrowIndex = 0; arrowIndex != arrowCount; arrowIndex++) {
      top = (arrowIndex != 0) ? STATS_DOWN_ARROW_TOP : STATS_UP_ARROW_TOP;
      if (arrowCount == 1 && firstArrow) {
        top = STATS_DOWN_ARROW_TOP;
      }

      gDPLoadTextureBlock_4b(gfx++,
          TextureAddress((u32)D_8023A828_ovl_01 +
              (firstArrow + arrowIndex) * STATS_ARROW_IMAGE_SIZE +
              (PALETTED_TEXELS_OFFSET +
                  (STATS_FIRST_ARROW_IMAGE * STATS_ARROW_IMAGE_SIZE))),
          G_IM_FMT_CI, STATS_ARROW_WIDTH, STATS_ARROW_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPTextureRectangle(gfx++, QuarterPixels(STATS_ARROW_LEFT),
          QuarterPixels(top),
          QuarterPixels(STATS_ARROW_LEFT + STATS_ARROW_WIDTH),
          QuarterPixels(top + STATS_ARROW_HEIGHT), G_TX_RENDERTILE, 0, 0,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", DrawStatsScrollArrows);
#endif

/*
 * Draws the course's spinning trophy in the color of each place won in its
 * tournament, stepping the spin's images, or on the totals the points needed to
 * open the next course.
 * Doesn't match: the original adds the trophies' shift as 0xFFEC, the low half
 * of -20, after reading the course's position, and adds the position to it
 * before the width. "+ (u16)-TROPHY_SHIFT_LEFT" gives that constant.
 */
#ifdef NON_MATCHING
void DrawCourseTrophies(Gfx** displayListHead) {
  EncodedAsset asset;
  Gfx* gfx = *displayListHead;
  SaveData* save = GetSaveData();
  MenuMemory* memory = GetMenuMemory();
  void* palette = NULL;
  s32 openCourseCount;
  s32 left;
  void* texels;
  s16 place;

  openCourseCount = GetOpenCourseCount();
  if (memory->statsCourse == openCourseCount) {
    BeginShadowedText(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    openCourseCount = CountOpenCourses(save->coursePoints);
    if (openCourseCount < COURSE_COUNT) {
      SetTextSlotValue(
          GetCoursePointsNeeded(openCourseCount) - save->coursePoints, 0, 0);
      SetTextSlot(POINTS_NEEDED_X, POINTS_NEEDED_Y,
          MESSAGE_POINTS_TO_NEXT_COURSE, TEXT_SLOT_LARGE_FONT, 0, 0, 255, 255,
          255, 255);
    } else {
      SetTextSlot(ALL_COURSES_OPEN_X, ALL_COURSES_OPEN_Y,
          MESSAGE_ALL_COURSES_OPEN, TEXT_SLOT_LARGE_FONT, 0, 0, 255, 255, 255,
          255);
    }
  } else {
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    for (place = 0; place != TOURNAMENT_PLACE_COUNT; place++) {
      if (g_CourseStatFigures[memory->statsCourse].hasPlace[place]) {
        gDPPipeSync(gfx++);
        if (place == FIRST_PLACE_INDEX) {
          palette = g_TrophyPalettes[FIRST_PLACE_INDEX];
        }

        if (place == SECOND_PLACE_INDEX) {
          palette = g_TrophyPalettes[SECOND_PLACE_INDEX];
        }

        if (place == THIRD_PLACE_INDEX) {
          palette = g_TrophyPalettes[THIRD_PLACE_INDEX];
        }

        texels = D_8023A814_ovl_01[D_802300A4_ovl_01];
        gDPLoadTLUT_pal256(gfx++, TextureAddress(palette));
        gDPLoadTextureTile(gfx++, TextureAddress(texels), G_IM_FMT_CI,
            G_IM_SIZ_8b, g_TrophySizes[memory->statsCourse].width,
            g_TrophySizes[memory->statsCourse].height, 0, 0,
            g_TrophySizes[memory->statsCourse].width - 1,
            g_TrophySizes[memory->statsCourse].height - 1, 0,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        left = (place * TROPHY_SPACING) + -TROPHY_SHIFT_LEFT;
        gSPScisTextureRectangle(gfx++,
            QuarterPixels(D_80230100_ovl_01[memory->statsCourse].x + left),
            QuarterPixels(D_80230100_ovl_01[memory->statsCourse].y),
            QuarterPixels(g_TrophySizes[memory->statsCourse].width +
                D_80230100_ovl_01[memory->statsCourse].x + left),
            QuarterPixels(g_TrophySizes[memory->statsCourse].height +
                D_80230100_ovl_01[memory->statsCourse].y),
            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
      }
    }

    D_802300A6_ovl_01++;
    if (D_802300A6_ovl_01 >= TROPHY_FRAME_DELAY) {
      D_802300A6_ovl_01 = 0;
      D_802300A2_ovl_01++;
      if ((s16)g_TrophyFrameAssets[memory->statsCourse][D_802300A2_ovl_01] ==
          TROPHY_FRAME_END) {
        D_802300A2_ovl_01 = 0;
      }

      if (D_802300A2_ovl_01 >= COURSE_IMAGE_COUNT) {
        D_802300A2_ovl_01 = 0;
      }

      D_802300A4_ovl_01 ^= 1;
      OpenEncodedAsset(
          g_TrophyFrameAssets[memory->statsCourse][D_802300A2_ovl_01], &asset);
      ReadEncodedAsset(D_8023A814_ovl_01[D_802300A4_ovl_01], &asset);
    }
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", DrawCourseTrophies);
#endif

/* The smallest number of each length */
#define TWO_DIGIT_FIRST DECIMAL_BASE
#define THREE_DIGIT_FIRST (TWO_DIGIT_FIRST * DECIMAL_BASE)
#define FOUR_DIGIT_FIRST (THREE_DIGIT_FIRST * DECIMAL_BASE)
/* The largest number of each length */
#define ONE_DIGIT_MAX (TWO_DIGIT_FIRST - 1)
#define TWO_DIGIT_MAX (THREE_DIGIT_FIRST - 1)
#define THREE_DIGIT_MAX (FOUR_DIGIT_FIRST - 1)
#define FOUR_DIGIT_MAX (FOUR_DIGIT_FIRST * DECIMAL_BASE - 1)

/* DrawMenuNumber's sixth argument: the field it right-aligns the number in */
enum NumberField {
  NUMBER_FIELD_TWO_NARROW,
  NUMBER_FIELD_ONE,
  NUMBER_FIELD_TWO,
  NUMBER_FIELD_THREE,
  NUMBER_FIELD_FOUR,
  NUMBER_FIELD_NONE
};

/* Draws a number right-aligned in a two-digit field, with narrower spacing,
 * clamped to the digits it holds */
void DrawNarrowTwoDigitMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark) {
  Gfx* gfx = *displayListHead;

  if (value > TWO_DIGIT_MAX) {
    value = TWO_DIGIT_MAX;
  }

  if (value < -TWO_DIGIT_MAX) {
    value = -TWO_DIGIT_MAX;
  }

  DrawMenuNumber(&gfx, x, y, value, mark, NUMBER_FIELD_TWO_NARROW);
  *displayListHead = gfx;
}

/* Draws a number right-aligned in a one-digit field, clamped to the
 * digits it holds */
void DrawOneDigitMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark) {
  Gfx* gfx = *displayListHead;

  if (value > ONE_DIGIT_MAX) {
    value = ONE_DIGIT_MAX;
  }

  if (value < -ONE_DIGIT_MAX) {
    value = -ONE_DIGIT_MAX;
  }

  DrawMenuNumber(&gfx, x, y, value, mark, NUMBER_FIELD_ONE);
  *displayListHead = gfx;
}

/* Draws a number right-aligned in a two-digit field, clamped to the
 * digits it holds */
void DrawTwoDigitMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark) {
  Gfx* gfx = *displayListHead;

  if (value > TWO_DIGIT_MAX) {
    value = TWO_DIGIT_MAX;
  }

  if (value < -TWO_DIGIT_MAX) {
    value = -TWO_DIGIT_MAX;
  }

  DrawMenuNumber(&gfx, x, y, value, mark, NUMBER_FIELD_TWO);
  *displayListHead = gfx;
}

/* Draws a number right-aligned in a three-digit field, clamped to the
 * digits it holds */
void DrawThreeDigitMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark) {
  Gfx* gfx = *displayListHead;

  if (value > THREE_DIGIT_MAX) {
    value = THREE_DIGIT_MAX;
  }

  if (value < -THREE_DIGIT_MAX) {
    value = -THREE_DIGIT_MAX;
  }

  DrawMenuNumber(&gfx, x, y, value, mark, NUMBER_FIELD_THREE);
  *displayListHead = gfx;
}

/* Draws a number right-aligned in a four-digit field, clamped to the
 * digits it holds */
void DrawFourDigitMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark) {
  Gfx* gfx = *displayListHead;

  if (value > FOUR_DIGIT_MAX) {
    value = FOUR_DIGIT_MAX;
  }

  if (value < -FOUR_DIGIT_MAX) {
    value = -FOUR_DIGIT_MAX;
  }

  DrawMenuNumber(&gfx, x, y, value, mark, NUMBER_FIELD_FOUR);
  *displayListHead = gfx;
}

/* Draws a number without right-aligning it in a field */
void DrawUnalignedMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark) {
  Gfx* gfx = *displayListHead;

  DrawMenuNumber(&gfx, x, y, value, mark, NUMBER_FIELD_NONE);
  *displayListHead = gfx;
}

#define NARROW_DIGIT_SPACING 12
#define MENU_NUMBER_DIGIT_LIMIT 4
#define MENU_NUMBER_LIMIT FOUR_DIGIT_MAX

/*
 * Draws a number of up to four digits with the glyphs of ASSET_LARGE_DIGITS,
 * marked as the mark argument says and right-aligned in a field. The field's
 * shift depends only on the field, so a number two or three digits long in a
 * wider field moves as far right as a one-digit one. Numbers past four digits
 * draw nothing.
 *
 * Doesn't match: the original keeps the digit count in a register through the
 * digit loop and sign-extends the index separately for each use, where GCC
 * reloads the count from the stack each pass and reuses one extended copy.
 */
#ifdef NON_MATCHING
void DrawMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark, u8 field) {
  Gfx* gfx = *displayListHead;
  s16 digits[MENU_NUMBER_DIGIT_LIMIT];
  u16 digitCount = 0;
  s32 shift;
  s32 spacing;
  s16 index; /* The mark's glyph, then the digit being drawn */

  if (value > MENU_NUMBER_LIMIT) {
    return;
  }

  if (value < -MENU_NUMBER_LIMIT) {
    return;
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(D_8023A82C_ovl_01)));
  shift = 0;
  if (mark != MENU_NUMBER_PLAIN) {
    switch (mark) {
      case MENU_NUMBER_SIGNED:
        if (value > 0) {
          index = MENU_GLYPH_PLUS;
          gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
          gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0x7F, 0xFF);
        } else if (value < 0) {
          index = MENU_GLYPH_MINUS;
          gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
          gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0xFF, 0x7F, 0xFF);
        } else {
          index = MENU_GLYPH_PLUS_MINUS;
        }
        break;
      case MENU_NUMBER_RED_TINT:
        gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0x7F, 0xFF);
        index = MENU_GLYPH_TIMES;
        break;
      case MENU_NUMBER_GREEN_TINT:
        gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0xFF, 0x7F, 0xFF);
        index = MENU_GLYPH_TIMES;
        break;
      case MENU_NUMBER_TIMES:
      default:
        index = MENU_GLYPH_TIMES;
        break;
    }

    if (mark != MENU_NUMBER_RED_TINT && mark != MENU_NUMBER_GREEN_TINT) {
      gDPLoadTextureBlock_4b(gfx++,
          TextureAddress(
              ImageTexels(D_8023A82C_ovl_01) + (index * MENU_GLYPH_IMAGE_SIZE)),
          G_IM_FMT_CI, MENU_GLYPH_SIZE, MENU_GLYPH_SIZE, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, QuarterPixels(x), QuarterPixels(y),
          QuarterPixels(x + MENU_GLYPH_SIZE),
          QuarterPixels(y + MENU_GLYPH_SIZE), G_TX_RENDERTILE, 0, 0,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
      shift = MENU_GLYPH_SIZE;
    }
  }

  if (value >= 0 && value < TWO_DIGIT_FIRST) {
    digitCount = 1;
    if (field == NUMBER_FIELD_TWO) {
      shift += MENU_GLYPH_SIZE;
    }

    if (field == NUMBER_FIELD_THREE) {
      shift += 2 * MENU_GLYPH_SIZE;
    }

    if (field == NUMBER_FIELD_FOUR) {
      shift += 3 * MENU_GLYPH_SIZE;
    }

    if (field == NUMBER_FIELD_TWO_NARROW) {
      shift += NARROW_DIGIT_SPACING;
    }

    digits[0] = value % DECIMAL_BASE;
  }

  if (value >= TWO_DIGIT_FIRST && value < THREE_DIGIT_FIRST) {
    digitCount = 2;
    if (field == NUMBER_FIELD_THREE) {
      shift += 2 * MENU_GLYPH_SIZE;
    }

    if (field == NUMBER_FIELD_FOUR) {
      shift += 3 * MENU_GLYPH_SIZE;
    }

    digits[0] = value / DECIMAL_BASE;
    digits[1] = value % DECIMAL_BASE;
  }

  if (value >= THREE_DIGIT_FIRST && value < FOUR_DIGIT_FIRST) {
    digitCount = 3;
    if (field == NUMBER_FIELD_FOUR) {
      shift += 3 * MENU_GLYPH_SIZE;
    }

    digits[0] = value / THREE_DIGIT_FIRST;
    digits[1] = (value % THREE_DIGIT_FIRST) / DECIMAL_BASE;
    digits[2] = (value % THREE_DIGIT_FIRST) % DECIMAL_BASE;
  }

  if (value >= FOUR_DIGIT_FIRST && value <= MENU_NUMBER_LIMIT) {
    digitCount = 4;
    digits[0] = value / FOUR_DIGIT_FIRST;
    digits[1] = (value % FOUR_DIGIT_FIRST) / THREE_DIGIT_FIRST;
    digits[2] = ((value % FOUR_DIGIT_FIRST) % THREE_DIGIT_FIRST) / DECIMAL_BASE;
    digits[3] = ((value % FOUR_DIGIT_FIRST) % THREE_DIGIT_FIRST) % DECIMAL_BASE;
  }

  if (value < 0 && value > -TWO_DIGIT_FIRST) {
    digitCount = 1;
    if (field == NUMBER_FIELD_TWO) {
      shift += MENU_GLYPH_SIZE;
    }

    if (field == NUMBER_FIELD_THREE) {
      shift += 2 * MENU_GLYPH_SIZE;
    }

    if (field == NUMBER_FIELD_FOUR) {
      shift += 3 * MENU_GLYPH_SIZE;
    }

    if (field == NUMBER_FIELD_TWO_NARROW) {
      shift += NARROW_DIGIT_SPACING;
    }

    digits[0] = -value % DECIMAL_BASE;
  }

  if (value <= -TWO_DIGIT_FIRST && value > -THREE_DIGIT_FIRST) {
    digitCount = 2;
    if (field == NUMBER_FIELD_THREE) {
      shift += 2 * MENU_GLYPH_SIZE;
    }

    if (field == NUMBER_FIELD_FOUR) {
      shift += 3 * MENU_GLYPH_SIZE;
    }

    digits[0] = -value / DECIMAL_BASE;
    digits[1] = -value % DECIMAL_BASE;
  }

  if (value <= -THREE_DIGIT_FIRST && value > -FOUR_DIGIT_FIRST) {
    digitCount = 3;
    if (field == NUMBER_FIELD_FOUR) {
      shift += 3 * MENU_GLYPH_SIZE;
    }

    digits[0] = -value / THREE_DIGIT_FIRST;
    digits[1] = (-value % THREE_DIGIT_FIRST) / DECIMAL_BASE;
    digits[2] = (-value % THREE_DIGIT_FIRST) % DECIMAL_BASE;
  }

  if (value <= -FOUR_DIGIT_FIRST && value >= -MENU_NUMBER_LIMIT) {
    digitCount = 4;
    digits[0] = -value / FOUR_DIGIT_FIRST;
    digits[1] = (-value % FOUR_DIGIT_FIRST) / THREE_DIGIT_FIRST;
    digits[2] =
        ((-value % FOUR_DIGIT_FIRST) % THREE_DIGIT_FIRST) / DECIMAL_BASE;
    digits[3] =
        ((-value % FOUR_DIGIT_FIRST) % THREE_DIGIT_FIRST) % DECIMAL_BASE;
  }

  spacing = MENU_GLYPH_SIZE;
  if (field == NUMBER_FIELD_TWO_NARROW) {
    spacing = NARROW_DIGIT_SPACING;
  }

  for (index = 0; index != digitCount; index++) {
    gDPLoadTextureBlock_4b(gfx++,
        TextureAddress(ImageTexels(D_8023A82C_ovl_01) +
            ((digits[index] + MENU_GLYPH_FIRST_DIGIT) * MENU_GLYPH_IMAGE_SIZE)),
        G_IM_FMT_CI, MENU_GLYPH_SIZE, MENU_GLYPH_SIZE, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, QuarterPixels(x + shift + (index * spacing)),
        QuarterPixels(y),
        QuarterPixels(x + shift + (index * spacing) + MENU_GLYPH_SIZE),
        QuarterPixels(y + MENU_GLYPH_SIZE), G_TX_RENDERTILE, 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", DrawMenuNumber);
#endif

/* DrawDecimalMenuNumber writes a figure to one decimal place, its whole part
 * lined up on the right */
#define DECIMAL_WHOLE_LIMIT 999
#define DECIMAL_DIGIT_LIMIT 6
#define HUNDREDTHS_PER_UNIT 100.0f
/* The point is drawn half a glyph back, over the right half of the last whole
 * digit */
#define DECIMAL_POINT_OFFSET (MENU_GLYPH_SIZE / 2)

const char D_802318E4_ovl_01[] = "%d";

/*
 * Draws value to one decimal place in the large digits, its whole part
 * right-aligned in three digits from x
 */
void DrawDecimalMenuNumber(Gfx** displayListHead, s32 x, s32 y, f32 value) {
  Gfx* gfx = *displayListHead;
  s32 whole = value;
  s32 shift;
  s32 fraction;
  s32 digitIndex;
  u8 digit;
  s32 unused[4];

  if (whole > DECIMAL_WHOLE_LIMIT) {
    whole = DECIMAL_WHOLE_LIMIT;
  }

  if (whole < 0) {
    whole = 0;
  }

  fraction = (value - whole) * HUNDREDTHS_PER_UNIT;

  /* Round the hundredths to tenths */
  if (fraction % DECIMAL_BASE >= DECIMAL_BASE / 2) {
    fraction += DECIMAL_BASE;
  }

  fraction /= DECIMAL_BASE;
  if (fraction >= DECIMAL_BASE) {
    whole++;
    if (whole > DECIMAL_WHOLE_LIMIT) {
      whole = DECIMAL_WHOLE_LIMIT;
      fraction = DECIMAL_BASE - 1;
    } else {
      fraction = 0;
    }
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(D_8023A82C_ovl_01)));
  shift = 0;
  if (whole >= 0 && whole < DECIMAL_BASE) {
    shift = 2 * MENU_GLYPH_SIZE;
  }

  if (whole >= DECIMAL_BASE && whole < DECIMAL_BASE * DECIMAL_BASE) {
    shift += MENU_GLYPH_SIZE;
  }

  sprintf(g_PrintBuffer, D_802318E4_ovl_01, whole);
  for (digitIndex = 0; digitIndex != DECIMAL_DIGIT_LIMIT; digitIndex++) {
    digit = g_PrintBuffer[digitIndex];
    if (digit == '\0') {
      break;
    }

    gDPLoadTextureBlock_4b(gfx++,
        TextureAddress(ImageTexels(D_8023A82C_ovl_01) +
            ((digit - '0' + MENU_GLYPH_FIRST_DIGIT) * MENU_GLYPH_IMAGE_SIZE)),
        G_IM_FMT_CI, MENU_GLYPH_SIZE, MENU_GLYPH_SIZE, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++,
        QuarterPixels(x + shift + (digitIndex * MENU_GLYPH_SIZE)),
        QuarterPixels(y),
        QuarterPixels(
            x + shift + (digitIndex * MENU_GLYPH_SIZE) + MENU_GLYPH_SIZE),
        QuarterPixels(y + MENU_GLYPH_SIZE), G_TX_RENDERTILE, 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  gDPLoadTextureBlock_4b(gfx++,
      TextureAddress(ImageTexels(D_8023A82C_ovl_01) +
          (MENU_GLYPH_POINT * MENU_GLYPH_IMAGE_SIZE)),
      G_IM_FMT_CI, MENU_GLYPH_SIZE, MENU_GLYPH_SIZE, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++,
      QuarterPixels(x + shift + ((digitIndex - 1) * MENU_GLYPH_SIZE) +
          DECIMAL_POINT_OFFSET),
      QuarterPixels(y),
      QuarterPixels(x + shift + ((digitIndex - 1) * MENU_GLYPH_SIZE) +
          DECIMAL_POINT_OFFSET + MENU_GLYPH_SIZE),
      QuarterPixels(y + MENU_GLYPH_SIZE), G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
      TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  gDPLoadTextureBlock_4b(gfx++,
      TextureAddress(ImageTexels(D_8023A82C_ovl_01) +
          ((fraction + MENU_GLYPH_FIRST_DIGIT) * MENU_GLYPH_IMAGE_SIZE)),
      G_IM_FMT_CI, MENU_GLYPH_SIZE, MENU_GLYPH_SIZE, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++,
      QuarterPixels(x + shift + (digitIndex * MENU_GLYPH_SIZE)),
      QuarterPixels(y),
      QuarterPixels(
          x + shift + (digitIndex * MENU_GLYPH_SIZE) + MENU_GLYPH_SIZE),
      QuarterPixels(y + MENU_GLYPH_SIZE), G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
      TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* Nothing reads these */
const char D_802318E8_ovl_01[] = "";
const char D_802318EC_ovl_01[] = "";

/* Draws the MENU_SIGN_* sign's glyph with its top-left corner at (x, y) */
void DrawMenuSign(Gfx** displayListHead, s32 x, s32 y, u8 sign) {
  Gfx* gfx = *displayListHead;
  s32 glyphIndex = MENU_GLYPH_TIMES;

  switch (sign) {
    case MENU_SIGN_DASH:
      glyphIndex = MENU_GLYPH_MINUS;
      break;
    case MENU_SIGN_TIMES:
      break;
    case MENU_SIGN_CIRCLE:
      glyphIndex = MENU_GLYPH_CIRCLE;
      break;
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(D_8023A82C_ovl_01)));
  gDPLoadTextureBlock_4b(gfx++,
      TextureAddress(ImageTexels(D_8023A82C_ovl_01) +
          (glyphIndex * MENU_GLYPH_IMAGE_SIZE)),
      G_IM_FMT_CI, MENU_GLYPH_SIZE, MENU_GLYPH_SIZE, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, QuarterPixels(x), QuarterPixels(y),
      QuarterPixels(x + MENU_GLYPH_SIZE), QuarterPixels(y + MENU_GLYPH_SIZE),
      G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* Draws "OFF" in the large digits' two glyphs for it; nothing calls this */
void DrawOffLabel(Gfx** displayListHead, s32 x, s32 y) {
  Gfx* gfx = *displayListHead;
  s32 glyphIndex;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(D_8023A82C_ovl_01)));
  for (glyphIndex = 0; glyphIndex != OFF_GLYPH_COUNT; glyphIndex++) {
    gDPLoadTextureBlock_4b(gfx++,
        TextureAddress(ImageTexels(D_8023A82C_ovl_01) +
            ((OFF_FIRST_GLYPH + glyphIndex) * MENU_GLYPH_IMAGE_SIZE)),
        G_IM_FMT_CI, MENU_GLYPH_SIZE, MENU_GLYPH_SIZE, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++,
        QuarterPixels(x + (glyphIndex * MENU_GLYPH_SIZE)), QuarterPixels(y),
        QuarterPixels(x + (glyphIndex * MENU_GLYPH_SIZE) + MENU_GLYPH_SIZE),
        QuarterPixels(y + MENU_GLYPH_SIZE), G_TX_RENDERTILE, 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

/*
 * Draws the green left and right arrows either side of a width centered on a
 * point, switching their blink frame every SIDE_ARROW_BLINK_FRAME_COUNT calls
 */
/*
 * Doesn't match: the original spills displayListHead to its argument slot,
 * keeps the arrow top in a register only for the lower edge, and needs one
 * stack slot fewer.
 */
#ifdef NON_MATCHING
void DrawBlinkingSideArrows(Gfx** displayListHead, void* image, s32* blinkFrame,
    s32* blinkTimer, s32 width, s32 centerX, s32 centerY) {
  Gfx* gfx = *displayListHead;
  s32 left = 0;
  s32 arrowIndex;
  s32 frameOffset;

  (*blinkTimer)++;
  if (*blinkTimer == SIDE_ARROW_BLINK_FRAME_COUNT) {
    *blinkFrame = !*blinkFrame;
    *blinkTimer = 0;
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(image)));
  frameOffset = (*blinkFrame * GREEN_ARROW_FRAME_SIZE) + PALETTED_TEXELS_OFFSET;
  for (arrowIndex = 0; arrowIndex != SIDE_ARROW_COUNT; arrowIndex++) {
    gDPLoadTextureTile_4b(gfx++,
        TextureAddress((u32)image +
            ((SIDE_ARROW_FIRST_IMAGE + arrowIndex) * GREEN_ARROW_IMAGE_SIZE +
                frameOffset)),
        G_IM_FMT_CI, GREEN_ARROW_SIZE, GREEN_ARROW_SIZE, 0, 0,
        GREEN_ARROW_SIZE - 1, GREEN_ARROW_SIZE - 1, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    if (arrowIndex == SIDE_ARROW_RIGHT) {
      left = centerX + (width / 2);
    }

    if (arrowIndex == SIDE_ARROW_LEFT) {
      left = centerX - (width / 2) - GREEN_ARROW_SIZE;
    }

    gSPScisTextureRectangle(gfx++, QuarterPixels(left),
        QuarterPixels(centerY - (GREEN_ARROW_SIZE / 2)),
        QuarterPixels(left + GREEN_ARROW_SIZE),
        QuarterPixels(centerY + (GREEN_ARROW_SIZE / 2)), G_TX_RENDERTILE, 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", DrawBlinkingSideArrows);
#endif

/*
 * Draws one of the four side arrow images, in either blink state, centered on
 * a point. The header size is held in a local, as the matching code needs:
 * with the constant, GCC folds the texels' offset differently.
 */
void DrawGreenArrow(Gfx** displayListHead, void* image, s32 blinkFrame,
    s32 direction, s32 centerX, s32 centerY) {
  Gfx* gfx = *displayListHead;
  s32 top;
  s32 left;
  void* texels;
  s32 headerSize = IMAGE_HEADER_SIZE;

  if (blinkFrame < 0) {
    blinkFrame = 0;
  }

  if (blinkFrame >= GREEN_ARROW_FRAME_COUNT) {
    blinkFrame = GREEN_ARROW_FRAME_COUNT - 1;
  }

  if (direction < 0) {
    direction = 0;
  }

  if (direction >= MENU_ARROW_COUNT) {
    direction = MENU_ARROW_COUNT - 1;
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress((u32)image + headerSize));
  gDPLoadTextureTile_4b(gfx++,
      TextureAddress((u32)image +
          (direction * GREEN_ARROW_IMAGE_SIZE +
              (blinkFrame * GREEN_ARROW_FRAME_SIZE +
                  (headerSize +
                      SMALL_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE)))),
      G_IM_FMT_CI, GREEN_ARROW_SIZE, GREEN_ARROW_SIZE, 0, 0,
      GREEN_ARROW_SIZE - 1, GREEN_ARROW_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPPipeSync(gfx++);
  left = centerX - (GREEN_ARROW_SIZE / 2);
  top = centerY - (GREEN_ARROW_SIZE / 2);
  gSPScisTextureRectangle(gfx++, QuarterPixels(left), QuarterPixels(top),
      QuarterPixels(centerX + (GREEN_ARROW_SIZE / 2)),
      QuarterPixels(centerY + (GREEN_ARROW_SIZE / 2)), G_TX_RENDERTILE, 0, 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
