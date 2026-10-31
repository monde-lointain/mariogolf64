/*
 * Two screens for choosing where to play, which share the course image
 * that turns over to the next course. The Ring Shot select draws a
 * backdrop of planks and a star for each hole, gold once its rings are
 * cleared. The hole select draws a sign with the course's name and hole.
 */
#include "common.h"

#include "asset.h"
#include "billboard.h"
#include "course.h"
#include "game_flags.h"
#include "heap.h"
#include "libc.h"
#include "menu_memory.h"
#include "save.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"

/* f_LoadedScreen values */
enum CourseSelectState {
  COURSE_SELECT_UNLOADED,
  COURSE_SELECT_RING_SHOT,
  COURSE_SELECT_HOLES
};

/* What UpdateRingShotSelect and UpdateHoleSelect return */
enum SelectResult {
  SELECT_IDLE,
  SELECT_MOVED,
  SELECT_BACK
};

/* f_PressedDirection values */
#define DIRECTION_UP 1
#define DIRECTION_DOWN 2
#define DIRECTION_LEFT 4
#define DIRECTION_RIGHT 8

/* f_PlankMotion values */
enum PlanksState {
  PLANKS_SHUT = -1,
  PLANKS_CLOSING,
  PLANKS_OPENING
};

/* Each four stars open one more Ring Shot course */
#define STARS_PER_OPENED_COURSE 4
#define StarsToOpen(courseCount) (((courseCount) - 1) * STARS_PER_OPENED_COURSE)

/* The asset IDs of the images */
#define HOLE_SIGN_ASSET 0x739
#define PLANKS_ASSET 0x8ED
#define COURSE_IMAGE_PALETTE_ASSET 0x876
#define BLANK_COURSE_IMAGE_ASSET 0x834
#define SPINNING_EMPTY_STAR_ASSET 0x5E5

/* The stars spin through this many frames, each shown for STAR_FRAME_TIME
 * frames */
#define STAR_FRAME_COUNT 15
#define STAR_FRAME_TIME 2
/* One star image is read while the other is drawn */
#define STAR_BUFFER_COUNT 2
/* The stars that are not chosen keep the image after the buffers */
#define STILL_STAR_IMAGE STAR_BUFFER_COUNT
#define STAR_IMAGE_COUNT (STAR_BUFFER_COUNT + 1)
#define STAR_WIDTH 32
#define STAR_HEIGHT 24
/* The chosen star grows by this many pixels each way */
#define STAR_GROWTH 16
#define STARS_LEFT 46
#define STAR_SPACING 38
#define STARS_TOP 88
/* The four star textures' tiles and palettes */
#define TILE_SPINNING_STAR 0
#define TILE_SPINNING_EMPTY_STAR 1
#define TILE_STILL_STAR 2
#define TILE_STILL_EMPTY_STAR 3
#define STAR_TMEM_STEP 0x40

/* The course image, two strips of 64 by 32 eight-bit texels */
#define COURSE_IMAGE_WIDTH 64
#define COURSE_IMAGE_STRIP_HEIGHT 32
#define COURSE_IMAGE_STRIP_COUNT 2
#define COURSE_IMAGE_STRIP_SIZE (COURSE_IMAGE_WIDTH * COURSE_IMAGE_STRIP_HEIGHT)
#define COURSE_IMAGE_LEFT 128
#define RING_SHOT_COURSE_IMAGE_TOP 144
#define HOLE_COURSE_IMAGE_TOP 152
/* The images of a course turning, in f_CourseImageAssets' order, before its
 * front */
#define COURSE_IMAGE_FRONT 2
/* f_CourseTurnFrames' entry for the image between two courses */
#define COURSE_IMAGE_BLANK -1
#define COURSE_TURN_FRAME_COUNT 7
/* The turn shows the course it leaves until this entry */
#define COURSE_TURN_MIDDLE 4

/* The arrows' image, two frames of 16 by 64 four-bit texels */
#define ARROWS_WIDTH 16
#define ARROWS_HEIGHT 64
#define ARROWS_FRAME_SIZE (ARROWS_WIDTH * ARROWS_HEIGHT / 2)
/* The arrows change frame every this many frames */
#define ARROW_FRAME_TIME 4
#define ARROW_LEFT 152
#define ARROW_SIZE 16
/* The up arrow moves back this far while up repeats, and the down arrow this
 * far down */
#define ARROW_PUSH_UP -6
#define ARROW_PUSH_DOWN 6
/* A chosen direction's arrow moves out this far */
#define ARROW_STEP 4

/* The name plate, three strips of 200 by 10 texels drawn 208 wide */
#define NAME_PLATE_WIDTH 200
#define NAME_PLATE_STRIP_HEIGHT 10
#define NAME_PLATE_STRIP_COUNT 3
#define NAME_PLATE_HALF_WIDTH 104
#define NAME_PLATE_TEXEL_STEP 984

/* The hole sign, six strips of 176 by 8 texels */
#define HOLE_SIGN_WIDTH 176
#define HOLE_SIGN_STRIP_HEIGHT 8
#define HOLE_SIGN_STRIP_COUNT 6
#define HOLE_SIGN_LEFT 72
#define HOLE_SIGN_TOP 16
/* The hole select slides down from this far above its place */
#define HOLE_SELECT_SLIDE_START 64
#define HOLE_SELECT_SLIDE_STEP 4

/* The planks, 304 by 224 texels in strips 6 high, the last one 2 */
#define PLANKS_WIDTH 304
#define PLANK_STRIP_HEIGHT 6
#define PLANK_STRIP_COUNT 38
#define PLANK_LAST_STRIP_HEIGHT 2
/* Each half of the planks slides in from its side until they meet in the middle
 */
#define PLANKS_MIDDLE (SCREEN_WIDTH / 2)
#define PLANKS_MIN_SLIDE SCREEN_BORDER
/* Each group of strips trails the one above by PLANK_GROUP_LAG */
#define PLANK_GROUP_STRIP_COUNT 7
#define PLANK_LAST_GROUP 4
#define PLANK_GROUP_LAG 64
#define PLANK_SLIDE_MAX 480
#define PLANK_SLIDE_STEP 24

/* Ring Shot hole descriptions are text images from this one */
#define RING_SHOT_TEXT_FIRST_IMAGE 0x4B
#define RING_SHOT_TEXT_SLOT 2

void DrawNamePlate(Gfx** displayListHead, s32 centerX, s32 centerY);
void DrawRingShotStars(Gfx** displayListHead, s32 centerX, s32 centerY);

static AssetId f_StarFrameAssets[STAR_FRAME_COUNT] = {
    0x5BE,
    0x5BF,
    0x5C0,
    0x5C1,
    0x5C2,
    0x5C3,
    0x5C4,
    0x5C5,
    0x5C6,
    0x5C7,
    0x5C8,
    0x5C9,
    0x5CA,
    0x5CB,
    0x5CC,
};
static AssetId f_EmptyStarFrameAssets[STAR_FRAME_COUNT] = {
    0x5E5,
    0x5E6,
    0x5E7,
    0x5E8,
    0x5E9,
    0x5EA,
    0x5EB,
    0x5EC,
    0x5ED,
    0x5EE,
    0x5EF,
    0x5F0,
    0x5F1,
    0x5F2,
    0x5F3,
};
/* "COURSE" badges, numbered for the main courses; read in, but never drawn here
 */
static AssetId f_CourseBadgeAssets[COURSE_INDEX_COUNT] = {
    0x733,
    0x734,
    0x735,
    0x736,
    0x737,
    0x738,
    0x738,
    0x738,
    0x738,
    0x738,
    0x738,
    0x738,
    0x738,
};
/* Each course has five images of turning, the third its front */
static AssetId f_CourseImageAssets[COURSE_INDEX_COUNT] = {
    0x835,
    0x83A,
    0x83F,
    0x844,
    0x849,
    0x84E,
    0x853,
    0x858,
    0x85D,
    0x862,
    0x867,
    0x86C,
    0x871,
};
static s32 f_StarFrameIndex = 0;
static u8 f_StarBufferIndex = 0;
static s8 f_StarFrameTime = 0;
static s32 f_CourseArrowFrameTime = 0;
/* Never read or written */
static s32 f_CourseSelectUnusedWord UNUSED = 0;
static s32 f_LoadedScreen = COURSE_SELECT_UNLOADED;
static s32 f_SlideOffset = 0;
/* While the course image turns, it turns from f_ShownCourse to f_NextCourse */
static CourseIndex f_ShownCourse = 0;
static CourseIndex f_NextCourse = 0;
static s32 f_CourseTurnIndex = 0;
static s32 f_CourseTurnStep =
    0; /* 1 or -1 through f_CourseTurnFrames, or 0 when still */
static u8 f_PressedDirection = 0;
static s32 f_CourseTurnFrames[COURSE_TURN_FRAME_COUNT] = {
    2, 3, 4, COURSE_IMAGE_BLANK, 0, 1, 2};
static s32 f_PlankSlide = 0;
static s32 f_PlankMotion = PLANKS_SHUT;

static u8* f_BoardImage; /* The name plate or the hole sign */
static u8* f_CourseBadges[COURSE_INDEX_COUNT];
static u8* f_CourseImage;
static u16* f_CourseImagePalette;
static u8* f_ArrowsImage;
static u8* f_StarImages[STAR_IMAGE_COUNT];
static u8* f_EmptyStarImages[STAR_IMAGE_COUNT];
static u8* f_PlanksImage;
static s32 f_PressedButtons;
static u32 f_RepeatedButtons;
/* Read by no routine */
static u8 f_UnusedBufferAfterRepeatedButtons[0x40] UNUSED;

s32 CountOpenRingShotCourses(void) {
  SaveData* save = GetSaveData();
  s32 starCount = 0;
  CourseIndex course;
  s32 hole;

  for (course = 0; course != RING_SHOT_COURSE_COUNT; course++) {
    for (hole = 0; hole != RING_SHOT_HOLE_COUNT; hole++) {
      starCount += save->hasRingShotStar[course][hole] > 0;
    }
  }

  if (IsGameFlagSet(GAME_FLAG_ALL_COURSES)) {
    return RING_SHOT_COURSE_COUNT;
  }

  if (starCount >= StarsToOpen(6)) {
    return 6;
  }

  if (starCount >= StarsToOpen(5)) {
    return 5;
  }

  if (starCount >= StarsToOpen(4)) {
    return 4;
  }

  if (starCount >= StarsToOpen(3)) {
    return 3;
  }

  if (starCount >= StarsToOpen(2)) {
    return 2;
  }

  return 1;
}

void LoadCourseSelect(Boolean isHoleSelect) {
  EncodedAsset asset;
  AssetFile file;
  s32 i;
  s32 size;

  if (f_LoadedScreen != COURSE_SELECT_UNLOADED) {
    osSyncPrintf("ring const err !");
    return;
  }

  f_PressedDirection = 0;
  if (!isHoleSelect) {
    f_LoadedScreen = COURSE_SELECT_RING_SHOT;
    f_BoardImage = AllocMemory(OpenEncodedAsset(ASSET_NAME_PLATE, &asset));
    ReadEncodedAsset(f_BoardImage, &asset);
    f_StarImages[0] =
        AllocMemory(OpenEncodedAsset(ASSET_SPINNING_STAR, &asset));
    ReadEncodedAsset(f_StarImages[0], &asset);
    f_StarImages[1] =
        AllocMemory(OpenEncodedAsset(ASSET_SPINNING_STAR, &asset));
    ReadEncodedAsset(f_StarImages[1], &asset);
    f_StarImages[STILL_STAR_IMAGE] =
        AllocMemory(OpenEncodedAsset(ASSET_SPINNING_STAR, &asset));
    ReadEncodedAsset(f_StarImages[STILL_STAR_IMAGE], &asset);
    f_EmptyStarImages[0] =
        AllocMemory(OpenEncodedAsset(SPINNING_EMPTY_STAR_ASSET, &asset));
    ReadEncodedAsset(f_EmptyStarImages[0], &asset);
    f_EmptyStarImages[1] =
        AllocMemory(OpenEncodedAsset(SPINNING_EMPTY_STAR_ASSET, &asset));
    ReadEncodedAsset(f_EmptyStarImages[1], &asset);
    f_EmptyStarImages[STILL_STAR_IMAGE] =
        AllocMemory(OpenEncodedAsset(SPINNING_EMPTY_STAR_ASSET, &asset));
    ReadEncodedAsset(f_EmptyStarImages[STILL_STAR_IMAGE], &asset);
    f_PlanksImage = AllocMemory(OpenEncodedAsset(PLANKS_ASSET, &asset));
    ReadEncodedAsset(f_PlanksImage, &asset);
  } else {
    f_LoadedScreen = COURSE_SELECT_HOLES;
    f_SlideOffset = HOLE_SELECT_SLIDE_START;
    f_BoardImage = AllocMemory(OpenEncodedAsset(HOLE_SIGN_ASSET, &asset));
    ReadEncodedAsset(f_BoardImage, &asset);
  }

  f_CourseTurnIndex = 0;
  f_CourseTurnStep = 0;
  f_ShownCourse = g_GameState.courseIndex;
  f_NextCourse = g_GameState.courseIndex;
  for (i = 0; i != COURSE_INDEX_COUNT; i++) {
    f_CourseBadges[i] =
        AllocMemory(OpenEncodedAsset(f_CourseBadgeAssets[i], &asset));
    ReadEncodedAsset(f_CourseBadges[i], &asset);
  }

  f_CourseImage = AllocMemory(OpenEncodedAsset(
      f_CourseImageAssets[g_GameState.courseIndex] + COURSE_IMAGE_FRONT,
      &asset));
  ReadEncodedAsset(f_CourseImage, &asset);
  OpenAssetFile(COURSE_IMAGE_PALETTE_ASSET, &file);
  size = GetAssetFileSize(&file);
  f_CourseImagePalette = AllocMemory(size);
  ReadAssetFile(f_CourseImagePalette, size, &file);
  f_ArrowsImage =
      AllocMemory(OpenEncodedAsset(ASSET_GREEN_ARROWS_COPY, &asset));
  ReadEncodedAsset(f_ArrowsImage, &asset);
  f_StarFrameIndex = 0;
  f_StarBufferIndex = 0;
  f_StarFrameTime = 0;
}

/* Reads the next image of the course image's turn, if it is turning */
void TurnCourseImage(void) {
  EncodedAsset asset;
  s32 frame;

  if (f_CourseTurnStep != 0) {
    frame = f_CourseTurnFrames[f_CourseTurnIndex];
    if (frame == COURSE_IMAGE_BLANK) {
      OpenEncodedAsset(BLANK_COURSE_IMAGE_ASSET, &asset);
    } else {
      if (f_CourseTurnIndex < COURSE_TURN_MIDDLE) {
        if (f_CourseTurnStep != -1) {
          OpenEncodedAsset(f_CourseImageAssets[f_ShownCourse] + frame, &asset);
        } else {
          OpenEncodedAsset(f_CourseImageAssets[f_NextCourse] + frame, &asset);
        }
      } else if (f_CourseTurnStep == -1) {
        OpenEncodedAsset(f_CourseImageAssets[f_ShownCourse] + frame, &asset);
      } else {
        OpenEncodedAsset(f_CourseImageAssets[f_NextCourse] + frame, &asset);
      }
    }

    ReadEncodedAsset(f_CourseImage, &asset);
    f_CourseTurnIndex += f_CourseTurnStep;
    if (f_CourseTurnIndex !=
        ((f_CourseTurnStep == 1) ? COURSE_TURN_FRAME_COUNT : -1)) {
      return;
    }

    f_CourseTurnStep = 0;
    f_ShownCourse = f_NextCourse;
  }
}

void FreeCourseSelect(void) {
  s32 i;

  if (f_LoadedScreen == COURSE_SELECT_UNLOADED) {
    osSyncPrintf("ring dest err");
    return;
  }

  FreeMemory(&f_BoardImage);
  for (i = 0; i != COURSE_INDEX_COUNT; i++) {
    FreeMemory(&f_CourseBadges[i]);
  }

  FreeMemory(&f_CourseImage);
  FreeMemory(&f_CourseImagePalette);
  FreeMemory(&f_ArrowsImage);
  if (f_LoadedScreen == COURSE_SELECT_RING_SHOT) {
    FreeMemory(&f_StarImages[0]);
    FreeMemory(&f_StarImages[1]);
    FreeMemory(&f_StarImages[STILL_STAR_IMAGE]);
    FreeMemory(&f_EmptyStarImages[0]);
    FreeMemory(&f_EmptyStarImages[1]);
    FreeMemory(&f_EmptyStarImages[STILL_STAR_IMAGE]);
    FreeMemory(&f_PlanksImage);
  }

  f_LoadedScreen = COURSE_SELECT_UNLOADED;
}

/*
 * Up and down change the course among those open, left and right the hole.
 * Returns SELECT_MOVED, SELECT_BACK for B, or SELECT_IDLE.
 */
s32 UpdateRingShotSelect(s32 pressedButtons, u32 repeatedButtons) {
  MenuMemory* memory = GetMenuMemory();
  s32 openCourseCount = CountOpenRingShotCourses();
  s32 result = SELECT_IDLE;

  f_PressedButtons = pressedButtons;
  f_RepeatedButtons = repeatedButtons;
  f_PressedDirection = 0;
  if (f_CourseTurnStep == 0 && !(pressedButtons & A_BUTTON)) {
    if (pressedButtons & B_BUTTON) {
      result = SELECT_BACK;
    } else if ((repeatedButtons & U_JPAD) && openCourseCount >= 2) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      result = SELECT_MOVED;
      memory->holeIndex = 0;
      memory->courseIndex--;
      f_CourseTurnStep = 1;
      f_CourseTurnIndex = 1;
      f_PressedDirection = DIRECTION_UP;
    } else if ((repeatedButtons & D_JPAD) && openCourseCount >= 2) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      result = SELECT_MOVED;
      memory->holeIndex = 0;
      memory->courseIndex++;
      f_CourseTurnStep = -1;
      f_CourseTurnIndex = COURSE_TURN_FRAME_COUNT - 2;
      f_PressedDirection = DIRECTION_DOWN;
    } else if (repeatedButtons & L_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      result = SELECT_MOVED;
      memory->holeIndex--;
      f_PressedDirection = DIRECTION_LEFT;
    } else if (repeatedButtons & R_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      result = SELECT_MOVED;
      memory->holeIndex++;
      f_PressedDirection = DIRECTION_RIGHT;
    }
  }

  if (memory->courseIndex < 0) {
    memory->courseIndex = openCourseCount - 1;
  }

  if (memory->courseIndex > openCourseCount - 1) {
    memory->courseIndex = 0;
  }

  if (memory->holeIndex < 0) {
    memory->holeIndex = RING_SHOT_HOLE_COUNT - 1;
  }

  if (memory->holeIndex >= RING_SHOT_HOLE_COUNT) {
    memory->holeIndex = 0;
  }

  g_GameState.holeIndex = memory->holeIndex;
  g_GameState.courseIndex = memory->courseIndex;
  f_NextCourse = g_GameState.courseIndex;
  return result;
}

/*
 * Up and down change the course among those open and the mini-golf
 * courses, left and right the hole. Returns SELECT_MOVED, SELECT_BACK for
 * B, or SELECT_IDLE. A leaves the selection as it stands, and its return
 * shares the end's so that it returns result rather than a constant.
 */
s32 UpdateHoleSelect(s32 pressedButtons, s32 repeatedButtons) {
  s32 result = SELECT_IDLE;
  s32 openCourseCount;

  GetMenuMemory();
  openCourseCount = CountOpenCourses(GetSaveData()->coursePoints);
  if (IsGameFlagSet(GAME_FLAG_ALL_COURSES)) {
    openCourseCount = COURSE_COUNT;
  }

  f_PressedDirection = 0;
  if (!(pressedButtons & A_BUTTON)) {
    if (pressedButtons & B_BUTTON) {
      return SELECT_BACK;
    }

    if (f_CourseTurnStep == 0) {
      if (repeatedButtons & L_JPAD) {
        if (!IsOnDrivingRange()) {
          PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
          result = SELECT_MOVED;
          f_PressedDirection = DIRECTION_LEFT;
          g_GameState.holeIndex--;
        } else {
          PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
      } else if (repeatedButtons & R_JPAD) {
        if (IsOnDrivingRange()) {
          PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        } else {
          PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
          result = SELECT_MOVED;
          f_PressedDirection = DIRECTION_RIGHT;
          g_GameState.holeIndex++;
        }
      } else if (repeatedButtons & U_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        g_GameState.courseIndex =
            (g_GameState.courseIndex + COURSE_INDEX_COUNT - 1) %
            COURSE_INDEX_COUNT;
        if (g_GameState.courseIndex >= MINI_GOLF_COURSE_INDEX &&
            g_GameState.courseIndex < DRIVING_RANGE_COURSE_INDEX) {
          g_GameState.isMiniGolf = TRUE;
        } else {
          g_GameState.isMiniGolf = FALSE;
        }

        if (g_GameState.courseIndex > openCourseCount - 1 &&
            g_GameState.courseIndex < MINI_GOLF_COURSE_INDEX) {
          g_GameState.courseIndex = openCourseCount - 1;
        }

        result = SELECT_MOVED;
        f_PressedDirection = DIRECTION_UP;
        f_CourseTurnStep = 1;
        f_CourseTurnIndex = 1;
      } else if (repeatedButtons & D_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        g_GameState.courseIndex =
            (g_GameState.courseIndex + COURSE_INDEX_COUNT + 1) %
            COURSE_INDEX_COUNT;
        if (g_GameState.courseIndex > openCourseCount - 1 &&
            g_GameState.courseIndex < MINI_GOLF_COURSE_INDEX) {
          g_GameState.courseIndex = MINI_GOLF_COURSE_INDEX;
        }

        if (g_GameState.courseIndex >= MINI_GOLF_COURSE_INDEX &&
            g_GameState.courseIndex < DRIVING_RANGE_COURSE_INDEX) {
          g_GameState.isMiniGolf = TRUE;
        } else {
          g_GameState.isMiniGolf = FALSE;
        }

        result = SELECT_MOVED;
        f_PressedDirection = DIRECTION_DOWN;
        f_CourseTurnStep = -1;
        f_CourseTurnIndex = COURSE_TURN_FRAME_COUNT - 2;
      }
    }

    g_GameState.courseIndex =
        (g_GameState.courseIndex + COURSE_INDEX_COUNT) % COURSE_INDEX_COUNT;
    f_NextCourse = g_GameState.courseIndex;
    if (IsOnDrivingRange()) {
      g_GameState.holeIndex = 0;
    } else {
      g_GameState.holeIndex =
          (g_GameState.holeIndex + COURSE_HOLE_COUNT) % COURSE_HOLE_COUNT;
    }
  }

  return result;
}

Boolean ArePlanksOpen(void) {
  return f_PlankSlide == 0;
}

void SetPlankMotion(s32 motion) {
  if (f_PlankMotion != motion) {
    f_PlankMotion = motion;
  }
}

#ifdef NON_MATCHING
void DrawPlanks(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;
  s32 stripHeight;
  s32 slide;
  s32 top;
  s32 bottom;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
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
  LoadLargePalette(&gfx, ImageBody(f_PlanksImage));
  switch (f_PlankMotion) {
    case PLANKS_SHUT:
      break;
    case PLANKS_OPENING:
      if (f_PlankSlide > 0) {
        f_PlankSlide -= PLANK_SLIDE_STEP;
      }

      if (f_PlankSlide < 0) {
        f_PlankSlide = 0;
      }

      break;
    case PLANKS_CLOSING:
      if (f_PlankSlide < PLANK_SLIDE_MAX) {
        f_PlankSlide += PLANK_SLIDE_STEP;
      }

      if (f_PlankSlide > PLANK_SLIDE_MAX) {
        f_PlankSlide = PLANK_SLIDE_MAX;
      }

      break;
  }

  for (stripIndex = 0; stripIndex != PLANK_STRIP_COUNT; stripIndex++) {
    stripHeight = (stripIndex != PLANK_STRIP_COUNT - 1)
        ? PLANK_STRIP_HEIGHT
        : PLANK_LAST_STRIP_HEIGHT;
    if (f_PlankMotion == PLANKS_SHUT) {
      slide = PLANKS_MIDDLE;
    } else {
      /* bottom holds the strip's plank group here, to match */
      bottom = stripIndex / PLANK_GROUP_STRIP_COUNT;
      if (bottom > PLANK_LAST_GROUP) {
        bottom = PLANK_LAST_GROUP;
      }

      slide = f_PlankSlide - (bottom * PLANK_GROUP_LAG);
      if (slide > PLANKS_MIDDLE) {
        slide = PLANKS_MIDDLE;
      }

      if (slide < PLANKS_MIN_SLIDE) {
        slide = PLANKS_MIN_SLIDE;
      }
    }

    gDPPipeSync(gfx++);
    LoadTextureBlock(&gfx,
        LargeImageTexels(f_PlanksImage) +
            (stripIndex * PLANK_STRIP_HEIGHT * PLANKS_WIDTH),
        G_IM_FMT_CI, G_IM_SIZ_8b, PLANKS_WIDTH, stripHeight, 0, 0, 0, 0, 0, 0,
        0);
    top = (SCREEN_BORDER + (stripIndex * PLANK_STRIP_HEIGHT)) << 2;
    bottom = (SCREEN_BORDER + (stripIndex * PLANK_STRIP_HEIGHT) + stripHeight)
        << 2;
    DrawTextureRect(&gfx, QuarterPixels(SCREEN_BORDER), top,
        QuarterPixels(slide), bottom, G_TX_RENDERTILE,
        TexelCoordinate(PLANKS_MIDDLE - slide), TexelCoordinate(1),
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    DrawTextureRect(&gfx, QuarterPixels(SCREEN_WIDTH - slide), top,
        QuarterPixels(SCREEN_WIDTH - SCREEN_BORDER), bottom, G_TX_RENDERTILE,
        TexelCoordinate(PLANKS_WIDTH / 2), TexelCoordinate(1), TEXEL_STEP_ONE,
        TEXEL_STEP_ONE);
  }

  *displayListHead = gfx;
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_11/course_select", DrawPlanks);
#endif

/* Doesn't match: the original sets each arrow coordinate's second choice in
 * the branch's delay slot and shifts it in each arm, where GCC picks the sum
 * first and shifts it once after the arms join */
#ifdef NON_MATCHING
void DrawRingShotSelect(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  MenuMemory* memory = GetMenuMemory();
  s32 stripIndex;
  s16 arrowFrame;
  s32 push;
  u8 unusedBuffer[0x1C] UNUSED; /* Unused; it is there to match */

  if (f_LoadedScreen == COURSE_SELECT_RING_SHOT) {
    DrawPlanks(&gfx);
    DrawNamePlate(&gfx, SCREEN_WIDTH / 2, 48);
    TurnCourseImage();
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    LoadLargePalette(&gfx, f_CourseImagePalette);
    for (stripIndex = 0; stripIndex != COURSE_IMAGE_STRIP_COUNT; stripIndex++) {
      LoadTextureBlock(&gfx,
          f_CourseImage + (stripIndex * COURSE_IMAGE_STRIP_SIZE), G_IM_FMT_CI,
          G_IM_SIZ_8b, COURSE_IMAGE_WIDTH, COURSE_IMAGE_STRIP_HEIGHT, 0, 0, 0,
          0, 0, 0, 0);
      gDPPipeSync(gfx++);
      DrawTextureRect(&gfx, QuarterPixels(COURSE_IMAGE_LEFT),
          QuarterPixels((stripIndex * COURSE_IMAGE_STRIP_HEIGHT) +
              f_SlideOffset + RING_SHOT_COURSE_IMAGE_TOP),
          QuarterPixels(COURSE_IMAGE_LEFT + COURSE_IMAGE_WIDTH),
          QuarterPixels((stripIndex * COURSE_IMAGE_STRIP_HEIGHT) +
              f_SlideOffset + RING_SHOT_COURSE_IMAGE_TOP +
              COURSE_IMAGE_STRIP_HEIGHT),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadSmallPalette(&gfx, 0, ImageBody(f_ArrowsImage));
    if (g_GameState.replayKind == REPLAY_KIND_NONE &&
        CountOpenRingShotCourses() >= 2) {
      arrowFrame =
          f_CourseArrowFrameTime++ % (2 * ARROW_FRAME_TIME) >= ARROW_FRAME_TIME;
      LoadNibbleTextureBlock(&gfx,
          ImageTexels(f_ArrowsImage) + (arrowFrame * ARROWS_FRAME_SIZE),
          G_IM_FMT_CI, ARROWS_WIDTH, ARROWS_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      gDPPipeSync(gfx++);
      push = (f_RepeatedButtons & U_JPAD) ? ARROW_PUSH_UP : 0;
      IN_OWN_BLOCK(DrawTextureRect(&gfx, QuarterPixels(ARROW_LEFT),
          QuarterPixels(
              f_PressedDirection == DIRECTION_UP ? push + 128 : push + 132),
          QuarterPixels(ARROW_LEFT + ARROW_SIZE),
          QuarterPixels(
              f_PressedDirection == DIRECTION_UP ? push + 144 : push + 148),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE));
      push = (f_RepeatedButtons & D_JPAD) ? ARROW_PUSH_DOWN : 0;
      DrawTextureRect(&gfx, QuarterPixels(ARROW_LEFT),
          QuarterPixels(
              f_PressedDirection == DIRECTION_DOWN ? push + 208 : push + 204),
          QuarterPixels(ARROW_LEFT + ARROW_SIZE),
          QuarterPixels(
              f_PressedDirection == DIRECTION_DOWN ? push + 224 : push + 220),
          G_TX_RENDERTILE, 0, TexelCoordinate(ARROW_SIZE), TEXEL_STEP_ONE,
          TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }

    DrawRingShotStars(&gfx, SCREEN_WIDTH / 2, 150);
    gDPPipeSync(gfx++);
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    SetTextSlot(SCREEN_WIDTH / 2, 42,
        (memory->courseIndex * RING_SHOT_HOLE_COUNT) + memory->holeIndex +
            RING_SHOT_TEXT_FIRST_IMAGE,
        TEXT_SLOT_LARGE_FONT, RING_SHOT_TEXT_SLOT,
        TEXT_FLAG_CENTERED | TEXT_FLAG_UNK20, 255, 255, 255, 255);
    DrawLargeTextSlot(&gfx, RING_SHOT_TEXT_SLOT);
    *displayListHead = gfx;
  }
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_11/course_select", DrawRingShotSelect);
#endif

#ifdef NON_MATCHING
void DrawHoleSelect(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;
  s32 arrowFrame;
  s32 push;
  s32 colorIndex;

  GetMenuMemory();
  f_SlideOffset -= HOLE_SELECT_SLIDE_STEP;
  if (f_SlideOffset < 0) {
    f_SlideOffset = 0;
  }

  if (f_LoadedScreen == COURSE_SELECT_HOLES) {
    gDPPipeSync(gfx++);
    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadLargePalette(&gfx, ImageBody(f_BoardImage));
    for (stripIndex = 0; stripIndex != HOLE_SIGN_STRIP_COUNT; stripIndex++) {
      gDPLoadTextureTile(gfx++,
          LargeImageTexels(f_BoardImage) +
              stripIndex * HOLE_SIGN_STRIP_HEIGHT * HOLE_SIGN_WIDTH,
          G_IM_FMT_CI, G_IM_SIZ_8b, HOLE_SIGN_WIDTH, 0, 0, 0,
          HOLE_SIGN_WIDTH - 1, HOLE_SIGN_STRIP_HEIGHT - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      DrawTextureRect(&gfx, QuarterPixels(HOLE_SIGN_LEFT),
          QuarterPixels((stripIndex * HOLE_SIGN_STRIP_HEIGHT) -
              (f_SlideOffset - HOLE_SIGN_TOP)),
          QuarterPixels(HOLE_SIGN_LEFT + HOLE_SIGN_WIDTH),
          QuarterPixels((stripIndex * HOLE_SIGN_STRIP_HEIGHT) -
              (f_SlideOffset - (HOLE_SIGN_TOP + HOLE_SIGN_STRIP_HEIGHT))),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }

    TurnCourseImage();
    LoadLargePalette(&gfx, f_CourseImagePalette);
    for (stripIndex = 0; stripIndex != COURSE_IMAGE_STRIP_COUNT; stripIndex++) {
      LoadTextureBlock(&gfx,
          f_CourseImage + (stripIndex * COURSE_IMAGE_STRIP_SIZE), G_IM_FMT_CI,
          G_IM_SIZ_8b, COURSE_IMAGE_WIDTH, COURSE_IMAGE_STRIP_HEIGHT, 0, 0, 0,
          0, 0, 0, 0);
      gDPPipeSync(gfx++);
      DrawTextureRect(&gfx, QuarterPixels(COURSE_IMAGE_LEFT),
          QuarterPixels((stripIndex * COURSE_IMAGE_STRIP_HEIGHT) +
              f_SlideOffset + HOLE_COURSE_IMAGE_TOP),
          QuarterPixels(COURSE_IMAGE_LEFT + COURSE_IMAGE_WIDTH),
          QuarterPixels((stripIndex * COURSE_IMAGE_STRIP_HEIGHT) +
              f_SlideOffset + HOLE_COURSE_IMAGE_TOP +
              COURSE_IMAGE_STRIP_HEIGHT),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }

    arrowFrame = f_CourseArrowFrameTime++;
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadSmallPalette(&gfx, 0, ImageBody(f_ArrowsImage));
    LoadNibbleTextureBlock(&gfx,
        ImageTexels(f_ArrowsImage) +
            ((arrowFrame % (2 * ARROW_FRAME_TIME) >= ARROW_FRAME_TIME) *
                ARROWS_FRAME_SIZE),
        G_IM_FMT_CI, ARROWS_WIDTH, ARROWS_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    gDPPipeSync(gfx++);
    push = (f_RepeatedButtons & U_JPAD) ? ARROW_PUSH_UP : 0;
    DrawTextureRect(&gfx, QuarterPixels(ARROW_LEFT),
        QuarterPixels(f_PressedDirection != DIRECTION_UP
                ? push + 140 + f_SlideOffset
                : push + 140 + f_SlideOffset - ARROW_STEP),
        QuarterPixels(ARROW_LEFT + ARROW_SIZE),
        QuarterPixels(f_PressedDirection != DIRECTION_UP
                ? push + 156 + f_SlideOffset
                : push + 156 + f_SlideOffset - ARROW_STEP),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    push = (f_RepeatedButtons & D_JPAD) ? ARROW_PUSH_DOWN : 0;
    DrawTextureRect(&gfx, QuarterPixels(ARROW_LEFT),
        QuarterPixels(f_PressedDirection != DIRECTION_DOWN
                ? push + 212 + f_SlideOffset
                : push + 212 + f_SlideOffset + ARROW_STEP),
        QuarterPixels(ARROW_LEFT + ARROW_SIZE),
        QuarterPixels(f_PressedDirection != DIRECTION_DOWN
                ? push + 228 + f_SlideOffset
                : push + 228 + f_SlideOffset + ARROW_STEP),
        G_TX_RENDERTILE, 0, TexelCoordinate(ARROW_SIZE), TEXEL_STEP_ONE,
        TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
    if (!IsOnDrivingRange()) {
      DrawTextureRect(&gfx,
          QuarterPixels(f_PressedDirection == DIRECTION_LEFT ? 100 : 104),
          QuarterPixels(44 - f_SlideOffset),
          QuarterPixels(f_PressedDirection == DIRECTION_LEFT ? 116 : 120),
          QuarterPixels(60 - f_SlideOffset), G_TX_RENDERTILE, 0,
          TexelCoordinate(3 * ARROW_SIZE), TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
      DrawTextureRect(&gfx,
          QuarterPixels(f_PressedDirection == DIRECTION_RIGHT ? 204 : 200),
          QuarterPixels(44 - f_SlideOffset),
          QuarterPixels(f_PressedDirection == DIRECTION_RIGHT ? 220 : 216),
          QuarterPixels(60 - f_SlideOffset), G_TX_RENDERTILE, 0,
          TexelCoordinate(2 * ARROW_SIZE), TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    SetUpGradientTextDrawing(&gfx, 0);
    SetTextColor(&gfx, 255, 255, 255);
    colorIndex = g_GameState.courseIndex;
    if (colorIndex >= MINI_GOLF_COURSE_INDEX + MINI_GOLF_COURSES_PER_RECORD) {
      colorIndex -= MINI_GOLF_COURSES_PER_RECORD;
    }

    sprintf(g_PrintBuffer, "%s", g_CourseRecords[GetCourseRecordIndex()].name);
    DrawLargeTextWithShadow(&gfx, SCREEN_WIDTH / 2, 24 - f_SlideOffset,
        g_PrintBuffer, g_CourseNameEnvColors[colorIndex],
        g_CourseNamePrimColors[colorIndex], TEXT_FLAG_CENTERED);
    if (!IsOnDrivingRange()) {
      sprintf(g_PrintBuffer, "Hole %d", g_GameState.holeIndex + 1);
      DrawLargeTextWithShadow(&gfx, SCREEN_WIDTH / 2, 44 - f_SlideOffset,
          g_PrintBuffer, g_CourseNameEnvColors[colorIndex],
          g_CourseNamePrimColors[colorIndex], TEXT_FLAG_CENTERED);
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
  }
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_11/course_select", DrawHoleSelect);
#endif

/*
 * Draws the course name plate in strips, centered on centerX and centerY.
 *
 * Doesn't match: 27 words differ. GCC masks top to 16 bits each strip, and
 * allocates the loop's registers in a different order. With an s32 top,
 * GCC strength-reduces the quarter-pixel rows into registers, which the
 * original doesn't (79). Setting top where it is declared gives 39, and
 * just before the loop 31; the order of the declarations makes no
 * difference. Hoisting the right edge out of the loop gives 62, and
 * stepping a pointer through the strips 55.
 */
#ifdef NON_MATCHING
void DrawNamePlate(Gfx** displayListHead, s32 centerX, s32 centerY) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;
  /* 16 bits to match */
  u16 top;

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  LoadLargePalette(&gfx, ImageBody(f_BoardImage));
  for (stripIndex = 0, top = centerY; stripIndex != NAME_PLATE_STRIP_COUNT;
      stripIndex++) {
    LoadTextureBlock(&gfx,
        LargeImageTexels(f_BoardImage) +
            (stripIndex * NAME_PLATE_STRIP_HEIGHT * NAME_PLATE_WIDTH),
        G_IM_FMT_CI, G_IM_SIZ_8b, NAME_PLATE_WIDTH, NAME_PLATE_STRIP_HEIGHT, 0,
        0, 0, 0, 0, 0, 0);
    gDPPipeSync(gfx++);
    DrawTextureRect(&gfx, QuarterPixels(centerX - NAME_PLATE_HALF_WIDTH),
        QuarterPixels(top - 15), QuarterPixels(centerX + NAME_PLATE_HALF_WIDTH),
        QuarterPixels(top - 5), G_TX_RENDERTILE, 0, 0, NAME_PLATE_TEXEL_STEP,
        TEXEL_STEP_ONE);
    top += NAME_PLATE_STRIP_HEIGHT;
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_11/course_select", DrawNamePlate);
#endif

/* Spins the chosen hole's star; the position is unused, the stars are always in
 * one place */
#ifdef NON_MATCHING
void DrawRingShotStars(
    Gfx** displayListHead, s32 centerX UNUSED, s32 centerY UNUSED) {
  EncodedAsset asset;
  SaveData* save = GetSaveData();
  MenuMemory* memory = GetMenuMemory();
  Gfx* gfx = *displayListHead;

  /* Unused */
  f32 hexagon[6][2] UNUSED = {
      {0.0f, -1.0f},
      {0.89f, -0.45f},
      {0.89f, 0.45f},
      {0.0f, 1.0f},
      {-0.89f, 0.45f},
      {-0.89f, -0.45f},
  };
  u8 hole;
  s32 width;
  s32 height;
  s32 left;
  s32 tile;
  u8* starImage = f_StarImages[f_StarBufferIndex];
  u8* emptyStarImage;

  LoadSmallPalette(&gfx, TILE_SPINNING_STAR, ImageBody(starImage));
  gDPLoadTextureBlock_4b(gfx++, ImageTexels(starImage), G_IM_FMT_CI, STAR_WIDTH,
      STAR_HEIGHT, TILE_SPINNING_STAR, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPPipeSync(gfx++);
  emptyStarImage = f_EmptyStarImages[f_StarBufferIndex];
  LoadSmallPalette(&gfx, TILE_SPINNING_EMPTY_STAR, ImageBody(emptyStarImage));
  gDPLoadMultiBlock_4b(gfx++, ImageTexels(emptyStarImage),
      STAR_TMEM_STEP * TILE_SPINNING_EMPTY_STAR, TILE_SPINNING_EMPTY_STAR,
      G_IM_FMT_CI, STAR_WIDTH, STAR_HEIGHT, TILE_SPINNING_EMPTY_STAR,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  LoadSmallPalette(
      &gfx, TILE_STILL_STAR, ImageBody(f_StarImages[STILL_STAR_IMAGE]));
  gDPLoadMultiBlock_4b(gfx++, ImageTexels(f_StarImages[STILL_STAR_IMAGE]),
      STAR_TMEM_STEP * TILE_STILL_STAR, TILE_STILL_STAR, G_IM_FMT_CI,
      STAR_WIDTH, STAR_HEIGHT, TILE_STILL_STAR, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  LoadSmallPalette(&gfx, TILE_STILL_EMPTY_STAR,
      ImageBody(f_EmptyStarImages[STILL_STAR_IMAGE]));
  gDPLoadMultiBlock_4b(gfx++, ImageTexels(f_EmptyStarImages[STILL_STAR_IMAGE]),
      STAR_TMEM_STEP * TILE_STILL_EMPTY_STAR, TILE_STILL_EMPTY_STAR,
      G_IM_FMT_CI, STAR_WIDTH, STAR_HEIGHT, TILE_STILL_EMPTY_STAR,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  for (hole = 0; hole < RING_SHOT_HOLE_COUNT; hole++) {
    if (hole == memory->holeIndex) {
      width = STAR_GROWTH;
      height = STAR_GROWTH;
    } else {
      width = 0;
      height = 0;
    }

    left = (hole * STAR_SPACING) + STARS_LEFT;
    if (hole == memory->holeIndex) {
      tile = (save->hasRingShotStar[memory->courseIndex][hole] == 0)
          ? TILE_SPINNING_EMPTY_STAR
          : TILE_SPINNING_STAR;
    } else {
      tile = (save->hasRingShotStar[memory->courseIndex][hole] == 0)
          ? TILE_STILL_EMPTY_STAR
          : TILE_STILL_STAR;
    }

    DrawTextureRect(&gfx, QuarterPixels(left - (width / 2)),
        QuarterPixels(STARS_TOP - (height / 2)),
        QuarterPixels(left + (width / 2) + STAR_WIDTH),
        QuarterPixels((height / 2) + STAR_HEIGHT + STARS_TOP), tile, 0, 0,
        (STAR_WIDTH * TEXEL_STEP_ONE) / (width + STAR_WIDTH),
        (STAR_HEIGHT * TEXEL_STEP_ONE) / (height + STAR_HEIGHT));
    gDPPipeSync(gfx++);
  }

  f_StarFrameTime++;
  if (f_StarFrameTime == STAR_FRAME_TIME) {
    f_StarFrameTime = 0;
    f_StarFrameIndex++;
    if (f_StarFrameIndex >= STAR_FRAME_COUNT) {
      f_StarFrameIndex = 0;
    }

    f_StarBufferIndex = f_StarBufferIndex == 0;
    OpenEncodedAsset(f_StarFrameAssets[f_StarFrameIndex], &asset);
    ReadEncodedAsset(f_StarImages[f_StarBufferIndex], &asset);
    OpenEncodedAsset(f_EmptyStarFrameAssets[f_StarFrameIndex], &asset);
    ReadEncodedAsset(f_EmptyStarImages[f_StarBufferIndex], &asset);
  }

  *displayListHead = gfx;
}

#else
/* The hexagon above, which the asm copies to the stack */
static const f32 f_UnusedHexagon[6][2] = {
    {0.0f, -1.0f},
    {0.89f, -0.45f},
    {0.89f, 0.45f},
    {0.0f, 1.0f},
    {-0.89f, 0.45f},
    {-0.89f, -0.45f},
};
INCLUDE_ASM("asm/nonmatchings/ovl_11/course_select", DrawRingShotStars);
#endif
