/*
 * The staff roll, MODULE_STAFF_ROLL, shown in SCENE_STAFF_ROLL. It scrolls the
 * staff text a pixel a frame, has characters from the cast swing on Yoshi's
 * Island's seventh hole, and ends on a closing picture.
 */
#include "common.h"

#include "asset.h"
#include "billboard.h"
#include "camera.h"
#include "character.h"
#include "character_slot.h"
#include "club.h"
#include "controller.h"
#include "course.h"
#include "current_shot.h"
#include "debug_text.h"
#include "framebuffer.h"
#include "framing.h"
#include "functions.h"
#include "game.h"
#include "game_flags.h"
#include "heap.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "libc.h"
#include "main.h"
#include "math_util.h"
#include "overlay_manager.h"
#include "pin.h"
#include "reaction.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "sound.h"
#include "variables.h"

/* The cast leaves out one character, the one after the start ShuffleCastOrder
 * is given */
#define CAST_MEMBER_COUNT (CHARACTER_COUNT - 1)
#define FIRST_CAST_OFFSET 2
#define SHUFFLE_ROUND_COUNT 10
#define SHUFFLED_MEMBER_COUNT 16

/* The cast's character buffers, given to the slots from FIRST_CAST_SLOT on */
#define CAST_BUFFER_COUNT 1
#define CAST_BUFFER_CAPACITY 3
#define FIRST_CAST_SLOT 1

#define PAINTING_ASSET 0x959
#define STAFF_FONT_ASSET 0x94C
/* Big enough for the painting and for the closing picture read over it later */
#define STAFF_ROLL_IMAGE_SIZE 0x2AC00
#define BILLBOARD_HEAP_SIZE 0x7038
#define CLOSING_PICTURE_ASSET 0x967

/* The staff text's first bytes hold its size, big-endian */
#define STAFF_TEXT_HEADER_SIZE 8
#define STAFF_GLYPH_WIDTH 12
#define STAFF_GLYPH_HEIGHT 12
/* The staff font is a 4-bit intensity image of 12 by 12 glyphs, 16 to a row */
#define STAFF_FONT_WIDTH (STAFF_FONT_COLUMN_COUNT * STAFF_GLYPH_WIDTH)
#define STAFF_FONT_COLUMN_COUNT 16
/* Only characters below this are drawn */
#define STAFF_CHARACTER_LIMIT 0x100
/*
 * These characters are drawn half a glyph to the left and 4 pixels up,
 * over the character before, and have no width
 */
#define FIRST_OVERLAID_CHARACTER 0xDE
#define OVERLAID_CHARACTER_COUNT 2
#define OVERLAID_CHARACTER_SHIFT_X (STAFF_GLYPH_WIDTH / 2)
#define OVERLAID_CHARACTER_SHIFT_Y 4

#define STAFF_ROLL_HOLE_INDEX 6
#define STAFF_ROLL_WIND_STEP 6000
#define STAFF_ROLL_WIND_STEP_MASK 0x3F
#define STAFF_ROLL_LIGHT_PITCH 0x1000
#define STAFF_ROLL_LIGHT_YAW 0x5800

/* f_StaffRollFrame values at which UpdateStaffRoll acts */
#define STAFF_SONG_FRAME 60
#define TIMELINE_START_FRAME 280
#define CLOSING_PICTURE_READ_FRAME 1000
#define STAFF_SONG_END_FRAME 3688
/* The closing song starts, and the closing picture fades in */
#define CLOSING_FRAME 3798
#define FIRST_LEAVING_FRAME 4300
#define MAX_STAFF_ROLL_FRAME 0xFFFF
/* The scroll also stops this many frames after the lines' heights add up to the
 * frame */
#define SCROLL_END_DELAY 376
#define STAFF_LINE_HEIGHT 24
#define FIRST_TOP_LINE_INDEX (-10)
#define DEBUG_SKIPPED_FRAMES 100

#define EARLIER_SONG_FADE_FRAMES 40
#define STAFF_SONG_FADE_FRAMES 200

/* The staff text's left edge; its shadow is a pixel right of and below it */
#define STAFF_TEXT_LEFT 32
#define STAFF_ROW_COUNT 11
#define TEXT_SHADOW_PASS 0
#define TEXT_PASS_COUNT 2

/* The fade covers only this rectangle */
#define FADE_RIGHT 312
#define FADE_BOTTOM 160

/* Where DrawStaffRoll prints the closing picture's alpha and the frame */
#define ALPHA_TEXT_COLUMN 5
#define ALPHA_TEXT_ROW 13
#define FRAME_TEXT_COLUMN 15
#define FRAME_TEXT_ROW 5

/* Where UpdateStaffRoll prints the line count */
#define LINE_COUNT_TEXT_COLUMN 5
#define LINE_COUNT_TEXT_ROW 12

/*
 * The painting and the closing picture are drawn in strips
 * IMAGE_STRIP_STEP rows apart, each IMAGE_STRIP_OVERLAP rows taller so
 * that the next strip overlaps it. The last strip is only the overlap,
 * which ends the image at the screen's bottom border.
 */
#define STAFF_ROLL_IMAGE_WIDTH (SCREEN_WIDTH - (2 * SCREEN_BORDER))
#define IMAGE_STRIP_COUNT 38
#define IMAGE_STRIP_STEP 6
#define IMAGE_STRIP_OVERLAP 2
#define PAINTING_SCROLL_FRAME 60
/* Rows scrolled when the painting's bottom is shown */
#define PAINTING_SCROLL_LIMIT 228
#define OPAQUE_ALPHA 255
#define CLOSING_FADE_RATE 2 /* Alpha per frame */
#define PAINTING_FADE_RATE 4 /* Alpha per frame */
#define PAINTING_FADE_IN_FRAMES 64
#define CLOUD_ALPHA 180 /* Below this an image is drawn with G_RM_CLD_SURF */

/* GetGroundHeight's units, 1024 to a world unit */
#define COURSE_UNITS_PER_UNIT 1024.0f
#define QUARTER_TURN_RADIANS 1.5707964f
#define PITCH_STEP_COUNT 10
#define FRAMING_GROUND_CLEARANCE 0xC00

/*
 * The cast's timeline; its frames count from TIMELINE_START_FRAME. Only
 * each stage's first frames are named; the later shots and reactions
 * keep literal frames, since names would only repeat the numbers.
 */
#define FIRST_SWING_FRAME 60
#define VIEW_SHRINK_FRAME 150 /* Also the tee's second shot */
#define TEE_REACTIONS_FRAME 300
#define FAIRWAY_FADE_OUT_FRAME 560
#define FAIRWAY_SHOTS_FRAME 600
#define FAIRWAY_SWING_FRAME 650
#define FAIRWAY_REACTIONS_FRAME 940
#define APPROACH_FADE_OUT_FRAME 1200
#define APPROACH_SHOTS_FRAME 1240
#define APPROACH_SWING_FRAME 1300
#define APPROACH_REACTIONS_FRAME 1600
#define PUTT_FADE_OUT_FRAME 1920
#define PUTT_SHOTS_FRAME 1960
#define PUTT_SWING_FRAME 2000
#define PUTT_REACTIONS_FRAME 2200
#define FINALE_FADE_OUT_FRAME 2590
#define FINALE_FRAME 2630 /* The last of the cast takes all four shots */
#define LAST_FADE_OUT_FRAME 3360
#define NEVER_FRAME 0x100000
#define CAST_FADE_FRAMES 40
#define LAST_FADE_FRAMES 80
#define SHOTS_PER_STAGE 4
#define TEE_CLUB 0
#define FAIRWAY_CLUB 3
#define APPROACH_CLUB 8
/* How many frames before its ANIMATION_EVENT_UNK6 frame a swing is joined */
#define CAST_SWING_LEAD_FRAMES 15
#define FINAL_PUTT_LEAD_FRAMES 10
/* Degrees between the framing yaws of a stage's shots, and between its
 * reactors' headings */
#define CAST_YAW_STEP 30
/* The putts are framed from three steps to the left of straight on */
#define PUTT_YAW_OFFSET 15
#define DEGREES_TO_RADIANS_F ((f32)M_PI / 180)
#define GOLFER_HEADING ((f32)DEGREES_TO_RADIANS(-30))
/* The putts are taken from this far along the x axis from the cup */
#define NEAR_CUP_DISTANCE (150 * 1024)

/* A random framing pitch, in 127ths of a degree; controller 2's stick tilts it
 */
#define RANDOM_PITCH_MASK 0x7F
#define RANDOM_PITCH_BIAS 0x20
#define RANDOM_PITCH_SCALE 70
#define PITCH_UNIT_TO_RADIANS ((f32)DEGREES_TO_RADIANS(1.0 / 127))
#define PITCH_CONTROLLER 1
#define PITCH_STICK_RATE 0.002f

#define STAFF_ROLL_FIELD_OF_VIEW 65.0f
#define CAST_FOCUS_POINT 1
#define SHADOW_SCALE_TO_RADIUS 0.105000004f
#define DEFAULT_FRAMED_RADIUS 15.0f
#define CAST_EASE_RATE 0.1f
/* Camera 0's view shrinks from the whole screen to its upper right */
#define VIEW_SHRINK_FRAMES 30
/* The scissor box's size before the shrink, and its changes per frame of it */
#define FULL_VIEW_WIDTH 303.0f
#define FULL_VIEW_HEIGHT 223.0f
#define SHRINK_LEFT_STEP 5.0000005f
#define SHRINK_TOP_STEP 0.5f
#define SHRINK_WIDTH_STEP 5.3333335f
#define SHRINK_HEIGHT_STEP 4.0f
/* The scissor height that gives a zoom of 2 */
#define UNIT_ZOOM_HEIGHT 224

/* Sets frames to count clamped to 0 through limit; its do-while is there to
 * match */
#define CLAMP_FRAMES(frames, count, limit) \
  do { \
    (frames) = (count); \
    if ((frames) < 0) { \
      (frames) = 0; \
    } \
    if ((frames) > (limit)) { \
      (frames) = (limit); \
    } \
  } while (0)

/*
 * Sets a view's scissor box, viewport center and zoom for a frame of
 * its shrink to the screen's upper right. Its do-while is there to
 * match.
 */
#define SHRINK_VIEW(view, frames) \
  do { \
    f32 progress = (frames); \
    s32 left = progress * SHRINK_LEFT_STEP; \
    s32 top = progress * SHRINK_TOP_STEP; \
    s32 height = FULL_VIEW_HEIGHT - (progress * SHRINK_HEIGHT_STEP); \
    s32 width = FULL_VIEW_WIDTH - (progress * SHRINK_WIDTH_STEP); \
\
    (view).scissorX = left; \
    (view).scissorY = top; \
    (view).scissorWidth = width; \
    (view).scissorHeight = height; \
    (view).viewportCenterX = (left * 2) + width; \
    (view).viewportCenterY = (top * 2) + height; \
    (view).zoom = 2.0f * height * (1.0f / UNIT_ZOOM_HEIGHT); \
  } while (0)

/* EaseStaffRollView's easedParts bits */
#define EASE_LOOK_AT (1 << 0)
#define EASE_EYE (1 << 1)

typedef struct StaffRollImage {
  /* 0x0 */ ImageHeader header;
  /* 0x8 */ u8 texels[STAFF_ROLL_IMAGE_SIZE];
} StaffRollImage; /* size = 0x2AC08 */

typedef struct StaffLine {
  /* 0x0 */ u16 start; /* In characters, from the end of the lines */
  /* 0x2 */ u16 length;
} StaffLine; /* size = 0x4 */

/* Read from the ROM; the 16-bit characters follow the lines */
typedef struct StaffText {
  /* 0x0 */ u16 size;
  /* 0x2 */ u16 lineCount;
  /* 0x4 */ StaffLine lines[1];
} StaffText;

typedef struct StaffRollEasedView {
  /* 0x00 */ f32 eye[AXIS_COUNT];
  /* 0x0C */ f32 lookAt[AXIS_COUNT];
  /* 0x18 */ s8 isEyeEased;
  /* 0x19 */ s8 isLookAtEased;
} StaffRollEasedView; /* size = 0x1C */

extern u8 g_StaffTextRomStart[];
extern u8* g_ShadowTexture;

void AssignCharacterBuffers(void);
void FreeEffectImages(void);
void DrawStaffLine(Gfx** displayListHead, s32 left, s32 top, s32 lineIndex);
void DrawStaffRollPainting(Gfx** displayListHead);
void LoadEffectImages(void);

static u8 f_IsPaintingShown = TRUE;
static s32 f_StaffRollFrame = 0;
static Boolean f_IsLeavingStaffRoll = FALSE;
static s32 f_TextScrollOffset = 0;
static s32 f_TopLineIndex = 0;
static s32 f_ShownSceneColorIndex = -1;
static Boolean f_IsTextScrolling = TRUE;
static s32 f_DebugRestartCount = 0;
/* Negative while fading out; zero when not fading */
static s32 f_FadeLength = 0;
static f32 f_CastFramingYaw = 0.0f;
static f32 f_CastFramingPitch = 0.0f;
static s32 f_CastIndex = 0;
static Boolean f_IsReactionShown = 0;
static f32 f_UnreadValue = 0.0f;
static s32 f_ViewShrinkStartFrame = NEVER_FRAME;
static s32 f_UnusedWord = 0; /* Unused */
/* How far right each character moves the next one */
static u8 f_StaffGlyphAdvances[STAFF_CHARACTER_LIMIT] = {
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    6,
    12,
    6,
    12,
    8,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    10,
    10,
    12,
    12,
    12,
    12,
    12,
    11,
    10,
    8,
    9,
    8,
    6,
    10,
    9,
    4,
    9,
    7,
    7,
    12,
    10,
    12,
    9,
    11,
    9,
    7,
    9,
    11,
    12,
    12,
    10,
    9,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    0,
    9,
    7,
    7,
    9,
    9,
    8,
    8,
    8,
    4,
    5,
    8,
    4,
    8,
    8,
    8,
    6,
    9,
    6,
    6,
    6,
    9,
    9,
    12,
    9,
    10,
    7,
    0,
    0,
    0,
    0,
    0,
};

/* Read by no routine */
static u8 D_801F8B50_ovl_14[0x10];
static s32 f_CastSlot; /* The slot of the character shown, 0 or 1 */
static CoursePoint f_NearCupPoint;
/* Read by no routine */
static u8 D_801F8B70_ovl_14[16];
static StaffRollImage f_StaffRollImage;
static StaffText* f_StaffText;
static u8* f_StaffFont;
static void* f_BillboardHeap;
static u8* f_CastBuffers[CAST_BUFFER_CAPACITY];
static StaffRollEasedView f_StaffRollEasedView;
static s32 f_CastOrder[CAST_MEMBER_COUNT];
static s32 f_FadeFramesLeft;

/* The kind of model the cast is loaded as, from the first player's paletteIndex
 */
s32 GetCastModelKind(void) {
  return g_Players[0].paletteIndex & (CHARACTER_MODEL_KIND_COUNT - 1);
}

/*
 * Fills f_CastOrder with the characters that follow start, leaving out the
 * one just after it, then shuffles all but the last unless
 * GAME_FLAG_UNK99 is set. SwapCastMembers is a nested function, a GNU C
 * extension.
 */
void ShuffleCastOrder(s32 start) {
  s32 member;
  s32 index;
  s32 round;

  void SwapCastMembers(s32 first, s32 second) {
    s32 member = f_CastOrder[first];

    f_CastOrder[first] = f_CastOrder[second];
    f_CastOrder[second] = member;
  }

  for (member = 0; member < CAST_MEMBER_COUNT; member++) {
    f_CastOrder[member] =
        (member + start + FIRST_CAST_OFFSET) % CHARACTER_COUNT;
  }

  if (!IsGameFlagSet(GAME_FLAG_UNK99)) {
    for (round = 0; round < SHUFFLE_ROUND_COUNT; round++) {
      for (index = 0; index < SHUFFLED_MEMBER_COUNT; index++) {
        SwapCastMembers(index, (rand() & 0x7FFF) % SHUFFLED_MEMBER_COUNT);
      }
    }
  }
}

/* Fades the screen to black over frameCount frames */
void StartFadeOut(s32 frameCount) {
  f_FadeLength = -frameCount;
  f_FadeFramesLeft = frameCount;
}

/* Fades the screen in from black over frameCount frames */
void StartFadeIn(s32 frameCount) {
  f_FadeLength = frameCount;
  f_FadeFramesLeft = frameCount;
}

void StopFade(void) {
  f_FadeLength = 0;
}

/*
 * Module 0x1F's onLoad. It reads the painting, the font and the staff
 * text, and sets up a one-player game on Yoshi's Island's seventh hole
 * for the cast to play on.
 */
void LoadStaffRoll(void) {
  EncodedAsset asset;
  EncodedAsset shadowAsset;
  u8* header;
  s32 index;
  s32 textSize;

  f_ShownSceneColorIndex = 11;
  ShuffleCastOrder((s8)g_Players[0].characterIndex);
  FreeAllCharacterSlots();
  for (index = 0; index < CAST_BUFFER_COUNT; index++) {
    f_CastBuffers[index] = AllocMemory(GetCharacterBufferSize());
    SetCharacterBuffer(index + FIRST_CAST_SLOT, f_CastBuffers[index]);
  }

  OpenEncodedAsset(PAINTING_ASSET, &asset);
  ReadEncodedAsset(&f_StaffRollImage, &asset);
  f_StaffFont = AllocMemory(OpenEncodedAsset(STAFF_FONT_ASSET, &asset));
  ReadEncodedAsset(f_StaffFont, &asset);
  header = AllocMemory(STAFF_TEXT_HEADER_SIZE);
  nuPiReadRom((u32)g_StaffTextRomStart, header, STAFF_TEXT_HEADER_SIZE);
  textSize = header[0] << 8 | header[1];
  FreeMemory(&header);
  f_StaffText = AllocMemory(textSize);
  nuPiReadRom((u32)g_StaffTextRomStart, f_StaffText, textSize);
  g_ShadowTexture =
      AllocMemory(OpenEncodedAsset(ASSET_SHADOW_TEXTURE, &shadowAsset));
  ReadEncodedAsset(g_ShadowTexture, &shadowAsset);
  f_BillboardHeap = AllocMemory(BILLBOARD_HEAP_SIZE);
  InitBillboardManager(f_BillboardHeap, BILLBOARD_HEAP_SIZE);
  memset(&g_GameState, 0, sizeof(g_GameState));
  g_GameState.courseIndex = COURSE_YOSHIS_ISLAND;
  g_GameState.holeIndex = STAFF_ROLL_HOLE_INDEX;
  g_GameState.unk14 = 0;
  g_GameState.gameMode = GAME_MODE_STAFF_ROLL;
  g_HoleSetup.conditions.isRaining = FALSE;
  g_HoleSetup.conditions.windSpeed =
      (rand() & STAFF_ROLL_WIND_STEP_MASK) * STAFF_ROLL_WIND_STEP;
  g_HoleSetup.conditions.windDirection = rand();
  g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_LATE;
  g_HoleSetup.conditions.lightPitch = STAFF_ROLL_LIGHT_PITCH;
  g_HoleSetup.conditions.lightYaw = STAFF_ROLL_LIGHT_YAW;
  g_HoleSetup.cupPosition[AXIS_X] = g_HolePoints.cupPositions[0][AXIS_X];
  g_HoleSetup.cupPosition[AXIS_Y] = g_HolePoints.cupPositions[0][AXIS_Y];
  g_HoleSetup.cupPosition[AXIS_Z] = g_HolePoints.cupPositions[0][AXIS_Z];
  g_HoleSetup.teePosition[AXIS_X] = g_HolePoints.teePositions[0][AXIS_X];
  g_HoleSetup.teePosition[AXIS_Y] = g_HolePoints.teePositions[0][AXIS_Y];
  g_HoleSetup.teePosition[AXIS_Z] = g_HolePoints.teePositions[0][AXIS_Z];
  SetGameFlag(GAME_FLAG_HOLE_LOADED);
  g_GameState.playerCount = 1;
  StopLoaderThread();
  StartLoaderThread();
  SetFramebufferCount(TRIPLE_BUFFERING);
  f_IsLeavingStaffRoll = FALSE;
  f_TextScrollOffset = 0;
  f_TopLineIndex = FIRST_TOP_LINE_INDEX;
  LoadEffectImages();
}

/* Module 0x1F's onUnload */
void UnloadStaffRoll(void) {
  s32 index;

  FreeEffectImages();
  g_Cameras[0].view.scissorX = SCREEN_BORDER;
  g_Cameras[0].view.scissorY = SCREEN_BORDER;
  g_Cameras[0].view.scissorWidth = SCREEN_WIDTH - (2 * SCREEN_BORDER);
  g_Cameras[0].view.scissorHeight = SCREEN_HEIGHT - (2 * SCREEN_BORDER);
  g_Cameras[0].view.zoom = 1.0f;
  FreeAllCharacterSlots();
  for (index = 0; index < CAST_BUFFER_COUNT; index++) {
    FreeMemory(&f_CastBuffers[index]);
  }

  AssignCharacterBuffers();
  SetFillColor(0, 0, 0);
  FreeMemory(&f_BillboardHeap);
  FreeMemory(&f_StaffText);
  FreeMemory(&f_StaffFont);
  FreeMemory(&g_ShadowTexture);
}

/* BlendClamped again */
f32 BlendStaffRollClamped(f32 from, f32 to, f32 weight) {
  if (weight < 0.0f) {
    weight = 0.0f;
  }

  if (weight > 1.0f) {
    weight = 1.0f;
  }

  return (from * (1.0f - weight)) + (to * weight);
}

void ResetStaffRollEase(void) {
  f_StaffRollEasedView.isEyeEased = FALSE;
  f_StaffRollEasedView.isLookAtEased = FALSE;
}

/* EaseReactionView again, with its own EasedView */
void EaseStaffRollView(f32* eye, f32* lookAt, f32 rate, s32 easedParts) {
  if (easedParts & EASE_EYE) {
    if (!f_StaffRollEasedView.isEyeEased) {
      f_StaffRollEasedView.eye[AXIS_X] = eye[AXIS_X];
      f_StaffRollEasedView.eye[AXIS_Y] = eye[AXIS_Y];
      f_StaffRollEasedView.eye[AXIS_Z] = eye[AXIS_Z];
      f_StaffRollEasedView.isEyeEased = TRUE;
    }

    eye[AXIS_X] = BlendStaffRollClamped(
        f_StaffRollEasedView.eye[AXIS_X], eye[AXIS_X], rate);
    f_StaffRollEasedView.eye[AXIS_X] = eye[AXIS_X];
    eye[AXIS_Y] = BlendStaffRollClamped(
        f_StaffRollEasedView.eye[AXIS_Y], eye[AXIS_Y], rate);
    f_StaffRollEasedView.eye[AXIS_Y] = eye[AXIS_Y];
    eye[AXIS_Z] = BlendStaffRollClamped(
        f_StaffRollEasedView.eye[AXIS_Z], eye[AXIS_Z], rate);
    f_StaffRollEasedView.eye[AXIS_Z] = eye[AXIS_Z];
  }

  if (easedParts & EASE_LOOK_AT) {
    if (!f_StaffRollEasedView.isLookAtEased) {
      f_StaffRollEasedView.lookAt[AXIS_X] = lookAt[AXIS_X];
      f_StaffRollEasedView.lookAt[AXIS_Y] = lookAt[AXIS_Y];
      f_StaffRollEasedView.lookAt[AXIS_Z] = lookAt[AXIS_Z];
      f_StaffRollEasedView.isLookAtEased = TRUE;
    }

    lookAt[AXIS_X] = BlendStaffRollClamped(
        f_StaffRollEasedView.lookAt[AXIS_X], lookAt[AXIS_X], rate);
    f_StaffRollEasedView.lookAt[AXIS_X] = lookAt[AXIS_X];
    lookAt[AXIS_Y] = BlendStaffRollClamped(
        f_StaffRollEasedView.lookAt[AXIS_Y], lookAt[AXIS_Y], rate);
    f_StaffRollEasedView.lookAt[AXIS_Y] = lookAt[AXIS_Y];
    lookAt[AXIS_Z] = BlendStaffRollClamped(
        f_StaffRollEasedView.lookAt[AXIS_Z], lookAt[AXIS_Z], rate);
    f_StaffRollEasedView.lookAt[AXIS_Z] = lookAt[AXIS_Z];
  }
}

/* The first of the club kind's animations; shotMode is unused */
s32 GetStanceAnimation(u32 slotIndex, s32 club, s32 shotMode) {
  s32 animation;

  switch (GetCharacterClubKind(slotIndex, club)) {
    case CLUB_KIND_WOOD:
      animation = WOOD_ANIMATIONS;
      break;
    case CLUB_KIND_IRON:
      animation = IRON_ANIMATIONS;
      break;
    case CLUB_KIND_WEDGE:
      animation = WEDGE_ANIMATIONS;
      break;
    case CLUB_KIND_PUTTER:
      animation = PUTTER_ANIMATIONS;
      break;
    default:
      animation = WOOD_ANIMATIONS;
      break;
  }

  return animation;
}

/*
 * The second of the club kind's animations, or the third for an approach
 * shot; the timeline skips to just before its ANIMATION_EVENT_UNK6 frame
 */
s32 GetSwingAnimation(u32 slotIndex, s32 club, s32 shotMode) {
  s32 animation;

  switch (GetCharacterClubKind(slotIndex, club)) {
    case CLUB_KIND_WOOD:
      animation = WOOD_ANIMATIONS + SWING_ANIMATION;
      break;
    case CLUB_KIND_IRON:
      animation = IRON_ANIMATIONS + SWING_ANIMATION;
      break;
    case CLUB_KIND_WEDGE:
      animation = WEDGE_ANIMATIONS + SWING_ANIMATION;
      break;
    case CLUB_KIND_PUTTER:
      animation = PUTTER_ANIMATIONS + SWING_ANIMATION;
      break;
    default:
      animation = 0;
      break;
  }

  return animation + (shotMode == SHOT_MODE_APPROACH);
}

void SetStanceAnimation(u32 slotIndex, s32 club, s32 shotMode) {
  SetCharacterAnimation(
      slotIndex, GetStanceAnimation(slotIndex, club, shotMode));
}

void SetSwingAnimation(u32 slotIndex, s32 club, s32 shotMode) {
  SetCharacterAnimation(
      slotIndex, GetSwingAnimation(slotIndex, club, shotMode));
}

/*
 * Frames the spheres at yaw, halving the range between pitch and
 * straight down up to ten times, and ends at the last pitch tried that put
 * the eye at least FRAMING_GROUND_CLEARANCE under the ground, or straight
 * down if none did. It stops early if the first pitch does. The final
 * ground height is read, but not used.
 */
void BisectFramingPitch(f32 pitch, f32 yaw, f32 depthOffset) {
  Vec3 eye;
  Vec3 lookAt;
  CoursePoint point;
  u8 unusedBuffer[0x10]; /* Unused; it is there to match */
  f32 buriedPitch = -QUARTER_TURN_RADIANS;
  s32 step = 0;
  f32 clearPitch = pitch;

  for (; step < PITCH_STEP_COUNT; step++) {
    SetFramingAngles(pitch, yaw);
    FrameSpheres(depthOffset);
    GetFramedView(eye, lookAt);
    point[AXIS_X] = eye[AXIS_X] * COURSE_UNITS_PER_UNIT;
    point[AXIS_Y] = eye[AXIS_Y] * COURSE_UNITS_PER_UNIT;
    point[AXIS_Z] = eye[AXIS_Z] * COURSE_UNITS_PER_UNIT;
    if (point[AXIS_Y] - GetGroundHeight(point[AXIS_X], point[AXIS_Z]) <=
        -FRAMING_GROUND_CLEARANCE) {
      buriedPitch = pitch;
      if (step == 0) {
        break;
      }
    } else {
      clearPitch = pitch;
    }

    pitch = (clearPitch + buriedPitch) * 0.5f;
  }

  SetFramingAngles(buriedPitch, yaw);
  FrameSpheres(depthOffset);
  GetFramedView(eye, lookAt);
  point[AXIS_X] = eye[AXIS_X] * COURSE_UNITS_PER_UNIT;
  point[AXIS_Y] = eye[AXIS_Y] * COURSE_UNITS_PER_UNIT;
  point[AXIS_Z] = eye[AXIS_Z] * COURSE_UNITS_PER_UNIT;
  GetGroundHeight(point[AXIS_X], point[AXIS_Z]);
}

/*
 * The cast's timeline, from TIMELINE_START_FRAME. In four stages, at the
 * tee, on the fairway, on the approach and on the green, four of the cast
 * take a shot, and then the same four react with a par, a bogey, a birdie
 * and an eagle respectively. The last of the cast then takes all four
 * shots. Swings are framed by the camera; reactions are framed by
 * UpdateReaction. Camera 0's view shrinks into the screen's upper right
 * over VIEW_SHRINK_FRAMES frames from VIEW_SHRINK_FRAME.
 */
void RunStaffRollTimeline(s32 frame) {
  CoursePoint point;
  Vec3 focus;
  CharacterSlot* character;
  s32 slot;
  s32 shrink;
  f32 radius;
  s32 swingAnimation;
  CameraView* view;

  void PickFramingPitch(void) {
    f_CastFramingPitch = ((rand() & RANDOM_PITCH_MASK) - RANDOM_PITCH_BIAS) *
        RANDOM_PITCH_SCALE * PITCH_UNIT_TO_RADIANS;
  }

  /* The GetAnimationEventFrame calls whose results are dropped are there to
   * match */
  f_CastFramingPitch +=
      g_ControllerStates[PITCH_CONTROLLER].stickY * PITCH_STICK_RATE;
  SetCharacterLights();
  if (frame == 0) {
    SetGameFlag(GAME_FLAG_UNK9A);
    StopFade();
    f_NearCupPoint[AXIS_X] =
        g_HoleSetup.cupPosition[AXIS_X] + NEAR_CUP_DISTANCE;
    f_NearCupPoint[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z];
    f_NearCupPoint[AXIS_Y] =
        GetGroundHeight(f_NearCupPoint[AXIS_X], f_NearCupPoint[AXIS_Z]);
    ClearEffects();
    f_ViewShrinkStartFrame = NEVER_FRAME;
    f_UnreadValue = 0.0f;
    f_CastFramingPitch = 0.0f;
    f_IsReactionShown = FALSE;
    f_CastIndex = 0;
    g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_DRIVING_RANGE;
    FreeAllCharacterSlots();
    ResetStaffRollEase();
    f_CastSlot = 0;
    LoadCharacter(0, f_CastOrder[0], GetCastModelKind());
    SetCharacterLighting(0, CHARACTER_LIGHTING_SCENE);
    SetCharacterClub(0, TEE_CLUB);
    SetStanceAnimation(0, TEE_CLUB, SHOT_MODE_NORMAL);
    point[AXIS_X] = g_HoleSetup.teePosition[AXIS_X];
    point[AXIS_Y] = g_HoleSetup.teePosition[AXIS_Y];
    point[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
    PlaceCharacter(0, point, GOLFER_HEADING);
    f_CastSlot = 0;
  }

  if (frame == FIRST_SWING_FRAME) {
    SetSwingAnimation(f_CastSlot, TEE_CLUB, SHOT_MODE_NORMAL);
    f_CastIndex = 1;
  }

  if (frame == VIEW_SHRINK_FRAME) {
    f_ViewShrinkStartFrame = frame;
  }

  if (frame == VIEW_SHRINK_FRAME || frame == 190 || frame == 240) {
    PickFramingPitch();
    f_CastFramingYaw = (f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F;
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    slot = f_CastSlot ^ 1;
    LoadCharacter(slot, f_CastOrder[f_CastIndex++], GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterClub(slot, TEE_CLUB);
    SetSwingAnimation(slot, TEE_CLUB, SHOT_MODE_NORMAL);
    swingAnimation = GetAnimationIndex(
        slot, GetSwingAnimation(slot, TEE_CLUB, SHOT_MODE_NORMAL));
    AdvanceAnimation(slot,
        GetAnimationEventFrame(slot, swingAnimation, ANIMATION_EVENT_UNK6) -
            CAST_SWING_LEAD_FRAMES);
    point[AXIS_X] = g_HoleSetup.teePosition[AXIS_X];
    point[AXIS_Y] = g_HoleSetup.teePosition[AXIS_Y];
    point[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
    PlaceCharacter(slot, point, GOLFER_HEADING);
    f_CastSlot = slot;
  }

  if (frame == TEE_REACTIONS_FRAME || frame == 360 || frame == 420 ||
      frame == 480) {
    ClearEffects();
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    slot = f_CastSlot ^ 1;
    g_CurrentShot.unk0 = -(f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F *
        RADIANS_TO_BINARY_ANGLE;
    if (frame == TEE_REACTIONS_FRAME) {
      f_CastIndex -= SHOTS_PER_STAGE;
    }

    LoadCharacter(
        slot, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterAnimation(slot, REACTION_ANIMATION_PAR);
    GetAnimationEventFrame(slot,
        GetAnimationIndex(
            slot, GetSwingAnimation(slot, TEE_CLUB, SHOT_MODE_NORMAL)),
        ANIMATION_EVENT_UNK6);
    ClearReactionSpot();
    ConsiderReactionSpot(g_HoleSetup.teePosition);
    StartReaction(slot, f_CastOrder[f_CastIndex], REACTION_ANIMATION_PAR);
    f_IsReactionShown = TRUE;
    point[AXIS_X] = g_HoleSetup.teePosition[AXIS_X];
    point[AXIS_Y] = g_HoleSetup.teePosition[AXIS_Y];
    point[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
    PlaceCharacter(
        slot, point, DEGREES_TO_RADIANS(-(f_CastIndex * CAST_YAW_STEP)));
    f_CastSlot = slot;
    f_CastIndex++;
  }

  if (frame == FAIRWAY_FADE_OUT_FRAME) {
    StartFadeOut(CAST_FADE_FRAMES);
  }

  if (frame == FAIRWAY_SHOTS_FRAME) {
    StartFadeIn(CAST_FADE_FRAMES);
  }

  if (frame == FAIRWAY_SHOTS_FRAME || frame == 770 || frame == 810 ||
      frame == 850) {
    ClearEffects();
    PickFramingPitch();
    g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_UNK8;
    f_CastFramingYaw = ((f_CastIndex - SHOTS_PER_STAGE) * CAST_YAW_STEP) *
        DEGREES_TO_RADIANS_F;
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    f_IsReactionShown = FALSE;
    slot = f_CastSlot ^ 1;
    LoadCharacter(slot, f_CastOrder[f_CastIndex++], GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterClub(slot, FAIRWAY_CLUB);
    if (frame == FAIRWAY_SHOTS_FRAME) {
      SetStanceAnimation(slot, FAIRWAY_CLUB, SHOT_MODE_NORMAL);
    } else {
      SetSwingAnimation(slot, FAIRWAY_CLUB, SHOT_MODE_NORMAL);
      swingAnimation = GetAnimationIndex(
          slot, GetSwingAnimation(slot, FAIRWAY_CLUB, SHOT_MODE_NORMAL));
      AdvanceAnimation(slot,
          GetAnimationEventFrame(slot, swingAnimation, ANIMATION_EVENT_UNK6) -
              CAST_SWING_LEAD_FRAMES);
    }

    point[AXIS_X] = g_HolePoints.unk60[0][AXIS_X];
    point[AXIS_Y] = g_HolePoints.unk60[0][AXIS_Y];
    point[AXIS_Z] = g_HolePoints.unk60[0][AXIS_Z];
    PlaceCharacter(slot, point, GOLFER_HEADING);
    f_CastSlot = slot;
  }

  if (frame == FAIRWAY_SWING_FRAME) {
    SetSwingAnimation(f_CastSlot, FAIRWAY_CLUB, SHOT_MODE_NORMAL);
  }

  if (frame == FAIRWAY_REACTIONS_FRAME || frame == 1000 || frame == 1060 ||
      frame == 1120) {
    ClearEffects();
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    slot = f_CastSlot ^ 1;
    g_CurrentShot.unk0 = -(f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F *
        RADIANS_TO_BINARY_ANGLE;
    if (frame == FAIRWAY_REACTIONS_FRAME) {
      f_CastIndex -= SHOTS_PER_STAGE;
    }

    LoadCharacter(
        slot, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterAnimation(slot, REACTION_ANIMATION_BOGEY);
    GetAnimationEventFrame(slot,
        GetAnimationIndex(
            slot, GetSwingAnimation(slot, TEE_CLUB, SHOT_MODE_NORMAL)),
        ANIMATION_EVENT_UNK6);
    ClearReactionSpot();
    ConsiderReactionSpot(g_HolePoints.unk60[0]);
    StartReaction(slot, f_CastOrder[f_CastIndex], REACTION_ANIMATION_BOGEY);
    f_IsReactionShown = TRUE;
    point[AXIS_X] = g_HolePoints.unk60[0][AXIS_X];
    point[AXIS_Y] = g_HolePoints.unk60[0][AXIS_Y];
    point[AXIS_Z] = g_HolePoints.unk60[0][AXIS_Z];
    PlaceCharacter(
        slot, point, -(f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F);
    f_CastSlot = slot;
    f_CastIndex++;
  }

  if (frame == APPROACH_FADE_OUT_FRAME) {
    StartFadeOut(CAST_FADE_FRAMES);
  }

  if (frame == APPROACH_SHOTS_FRAME) {
    StartFadeIn(CAST_FADE_FRAMES);
  }

  if (frame == APPROACH_SHOTS_FRAME || frame == 1420 || frame == 1460 ||
      frame == 1500) {
    ClearEffects();
    PickFramingPitch();
    g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_LATE;
    f_CastFramingYaw = ((f_CastIndex - (2 * SHOTS_PER_STAGE)) * CAST_YAW_STEP) *
        DEGREES_TO_RADIANS_F;
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    f_IsReactionShown = FALSE;
    slot = f_CastSlot ^ 1;
    LoadCharacter(slot, f_CastOrder[f_CastIndex++], GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterClub(slot, APPROACH_CLUB);
    if (frame == APPROACH_SHOTS_FRAME) {
      SetStanceAnimation(slot, APPROACH_CLUB, SHOT_MODE_NORMAL);
    } else {
      SetSwingAnimation(slot, APPROACH_CLUB, SHOT_MODE_NORMAL);
      swingAnimation = GetAnimationIndex(
          slot, GetSwingAnimation(slot, APPROACH_CLUB, SHOT_MODE_NORMAL));
      AdvanceAnimation(slot,
          GetAnimationEventFrame(slot, swingAnimation, ANIMATION_EVENT_UNK6) -
              CAST_SWING_LEAD_FRAMES);
    }

    point[AXIS_X] = g_HolePoints.unk60[1][AXIS_X];
    point[AXIS_Y] = g_HolePoints.unk60[1][AXIS_Y];
    point[AXIS_Z] = g_HolePoints.unk60[1][AXIS_Z];
    PlaceCharacter(slot, point, GOLFER_HEADING);
    f_CastSlot = slot;
  }

  if (frame == APPROACH_SWING_FRAME) {
    SetSwingAnimation(f_CastSlot, APPROACH_CLUB, SHOT_MODE_NORMAL);
  }

  if (frame == APPROACH_REACTIONS_FRAME || frame == 1680 || frame == 1760 ||
      frame == 1840) {
    ClearEffects();
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    slot = f_CastSlot ^ 1;
    g_CurrentShot.unk0 = -(f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F *
        RADIANS_TO_BINARY_ANGLE;
    if (frame == APPROACH_REACTIONS_FRAME) {
      f_CastIndex -= SHOTS_PER_STAGE;
    }

    LoadCharacter(
        slot, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterAnimation(slot, REACTION_ANIMATION_BIRDIE);
    GetAnimationEventFrame(slot,
        GetAnimationIndex(
            slot, GetSwingAnimation(slot, TEE_CLUB, SHOT_MODE_NORMAL)),
        ANIMATION_EVENT_UNK6);
    ClearReactionSpot();
    ConsiderReactionSpot(g_HolePoints.unk60[1]);
    StartReaction(slot, f_CastOrder[f_CastIndex], REACTION_ANIMATION_BIRDIE);
    f_IsReactionShown = TRUE;
    point[AXIS_X] = g_HolePoints.unk60[1][AXIS_X];
    point[AXIS_Y] = g_HolePoints.unk60[1][AXIS_Y];
    point[AXIS_Z] = g_HolePoints.unk60[1][AXIS_Z];
    PlaceCharacter(
        slot, point, -(f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F);
    f_CastSlot = slot;
    f_CastIndex++;
  }

  if (frame == PUTT_FADE_OUT_FRAME) {
    StartFadeOut(CAST_FADE_FRAMES);
  }

  if (frame == PUTT_SHOTS_FRAME) {
    StartFadeIn(CAST_FADE_FRAMES);
  }

  if (frame == PUTT_SHOTS_FRAME || frame == 2060 || frame == 2100 ||
      frame == 2150) {
    ClearEffects();
    f_CastFramingYaw = ((f_CastIndex - PUTT_YAW_OFFSET) * CAST_YAW_STEP) *
        DEGREES_TO_RADIANS_F;
    PickFramingPitch();
    g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_END;
    f_UnreadValue = 15.0f;
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    f_IsReactionShown = FALSE;
    slot = f_CastSlot ^ 1;
    LoadCharacter(slot, f_CastOrder[f_CastIndex++], GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterClub(slot, CLUB_TYPE_PUTTER);
    if (frame == PUTT_SHOTS_FRAME) {
      SetStanceAnimation(slot, CLUB_TYPE_PUTTER, SHOT_MODE_NORMAL);
    } else {
      SetSwingAnimation(slot, CLUB_TYPE_PUTTER, SHOT_MODE_NORMAL);
      swingAnimation = GetAnimationIndex(
          slot, GetSwingAnimation(slot, CLUB_TYPE_PUTTER, SHOT_MODE_NORMAL));
      AdvanceAnimation(slot,
          GetAnimationEventFrame(slot, swingAnimation, ANIMATION_EVENT_UNK6) -
              CAST_SWING_LEAD_FRAMES);
    }

    point[AXIS_X] = f_NearCupPoint[AXIS_X];
    point[AXIS_Y] = f_NearCupPoint[AXIS_Y];
    point[AXIS_Z] = f_NearCupPoint[AXIS_Z];
    PlaceCharacter(slot, point, GOLFER_HEADING);
    f_CastSlot = slot;
  }

  if (frame == PUTT_SWING_FRAME) {
    SetSwingAnimation(f_CastSlot, CLUB_TYPE_PUTTER, SHOT_MODE_NORMAL);
  }

  if (frame == PUTT_REACTIONS_FRAME || frame == 2300 || frame == 2400 ||
      frame == 2500) {
    ClearEffects();
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    slot = f_CastSlot ^ 1;
    g_CurrentShot.unk0 = -(f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F *
        RADIANS_TO_BINARY_ANGLE;
    if (frame == PUTT_REACTIONS_FRAME) {
      f_CastIndex -= SHOTS_PER_STAGE;
    }

    LoadCharacter(
        slot, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterAnimation(slot, REACTION_ANIMATION_EAGLE);
    GetAnimationEventFrame(slot,
        GetAnimationIndex(
            slot, GetSwingAnimation(slot, TEE_CLUB, SHOT_MODE_NORMAL)),
        ANIMATION_EVENT_UNK6);
    ClearReactionSpot();
    ConsiderReactionSpot(f_NearCupPoint);
    StartReaction(slot, f_CastOrder[f_CastIndex], REACTION_ANIMATION_EAGLE);
    f_IsReactionShown = TRUE;
    point[AXIS_X] = f_NearCupPoint[AXIS_X];
    point[AXIS_Y] = f_NearCupPoint[AXIS_Y];
    point[AXIS_Z] = f_NearCupPoint[AXIS_Z];
    PlaceCharacter(
        slot, point, DEGREES_TO_RADIANS(-(f_CastIndex * CAST_YAW_STEP)));
    f_CastSlot = slot;
    f_CastIndex++;
  }

  if (frame == FINALE_FADE_OUT_FRAME) {
    StartFadeOut(CAST_FADE_FRAMES);
  }

  if (frame == FINALE_FRAME) {
    StartFadeIn(CAST_FADE_FRAMES);
    f_CastFramingYaw = 0.0f;
    ClearEffects();
    PickFramingPitch();
    f_IsReactionShown = FALSE;
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    slot = f_CastSlot ^ 1;
    LoadCharacter(slot, f_CastOrder[f_CastIndex], GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterClub(slot, TEE_CLUB);
    SetSwingAnimation(slot, TEE_CLUB, SHOT_MODE_NORMAL);
    GetAnimationEventFrame(slot,
        GetAnimationIndex(
            slot, GetSwingAnimation(slot, TEE_CLUB, SHOT_MODE_NORMAL)),
        ANIMATION_EVENT_UNK6);
    point[AXIS_X] = g_HoleSetup.teePosition[AXIS_X];
    point[AXIS_Y] = g_HoleSetup.teePosition[AXIS_Y];
    point[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
    PlaceCharacter(slot, point, GOLFER_HEADING);
    f_CastSlot = slot;
  }

  if (frame == 2740) {
    ClearEffects();
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    slot = f_CastSlot ^ 1;
    LoadCharacter(
        slot, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterAnimation(slot, REACTION_ANIMATION_PAR);
    GetAnimationEventFrame(slot,
        GetAnimationIndex(
            slot, GetSwingAnimation(slot, TEE_CLUB, SHOT_MODE_NORMAL)),
        ANIMATION_EVENT_UNK6);
    ClearReactionSpot();
    ConsiderReactionSpot(g_HoleSetup.teePosition);
    StartReaction(slot, f_CastOrder[f_CastIndex], REACTION_ANIMATION_PAR);
    f_IsReactionShown = TRUE;
    point[AXIS_X] = g_HoleSetup.teePosition[AXIS_X];
    point[AXIS_Y] = g_HoleSetup.teePosition[AXIS_Y];
    point[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
    PlaceCharacter(slot, point, GOLFER_HEADING);
    f_CastSlot = slot;
  }

  if (frame == 2800) {
    ClearEffects();
    f_CastFramingYaw = 0.0f;
    PickFramingPitch();
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    f_IsReactionShown = FALSE;
    slot = f_CastSlot ^ 1;
    LoadCharacter(slot, f_CastOrder[f_CastIndex], GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterClub(slot, FAIRWAY_CLUB);
    SetSwingAnimation(slot, FAIRWAY_CLUB, SHOT_MODE_NORMAL);
    GetAnimationEventFrame(slot,
        GetAnimationIndex(
            slot, GetSwingAnimation(slot, FAIRWAY_CLUB, SHOT_MODE_NORMAL)),
        ANIMATION_EVENT_UNK6);
    point[AXIS_X] = g_HolePoints.unk60[0][AXIS_X];
    point[AXIS_Y] = g_HolePoints.unk60[0][AXIS_Y];
    point[AXIS_Z] = g_HolePoints.unk60[0][AXIS_Z];
    PlaceCharacter(slot, point, GOLFER_HEADING);
    f_CastSlot = slot;
  }

  if (frame == 2950) {
    ClearEffects();
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    slot = f_CastSlot ^ 1;
    LoadCharacter(
        slot, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterAnimation(slot, REACTION_ANIMATION_BOGEY);
    GetAnimationEventFrame(slot,
        GetAnimationIndex(
            slot, GetSwingAnimation(slot, TEE_CLUB, SHOT_MODE_NORMAL)),
        ANIMATION_EVENT_UNK6);
    ClearReactionSpot();
    ConsiderReactionSpot(g_HolePoints.unk60[0]);
    StartReaction(slot, f_CastOrder[f_CastIndex], REACTION_ANIMATION_BOGEY);
    f_IsReactionShown = TRUE;
    point[AXIS_X] = g_HolePoints.unk60[0][AXIS_X];
    point[AXIS_Y] = g_HolePoints.unk60[0][AXIS_Y];
    point[AXIS_Z] = g_HolePoints.unk60[0][AXIS_Z];
    PlaceCharacter(slot, point, GOLFER_HEADING);
    f_CastSlot = slot;
  }

  if (frame == 3030) {
    f_CastFramingYaw = DEGREES_TO_RADIANS_F * CAST_YAW_STEP;
    ClearEffects();
    PickFramingPitch();
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    f_IsReactionShown = FALSE;
    slot = f_CastSlot ^ 1;
    LoadCharacter(slot, f_CastOrder[f_CastIndex], GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterClub(slot, APPROACH_CLUB);
    SetSwingAnimation(slot, APPROACH_CLUB, SHOT_MODE_NORMAL);
    swingAnimation = GetAnimationIndex(
        slot, GetSwingAnimation(slot, APPROACH_CLUB, SHOT_MODE_NORMAL));
    AdvanceAnimation(slot,
        GetAnimationEventFrame(slot, swingAnimation, ANIMATION_EVENT_UNK6) -
            (2 * CAST_SWING_LEAD_FRAMES));
    point[AXIS_X] = g_HolePoints.unk60[1][AXIS_X];
    point[AXIS_Y] = g_HolePoints.unk60[1][AXIS_Y];
    point[AXIS_Z] = g_HolePoints.unk60[1][AXIS_Z];
    PlaceCharacter(slot, point, GOLFER_HEADING);
    f_CastSlot = slot;
  }

  if (frame == 3100) {
    ClearEffects();
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    slot = f_CastSlot ^ 1;
    LoadCharacter(
        slot, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterAnimation(slot, REACTION_ANIMATION_BIRDIE);
    GetAnimationEventFrame(slot,
        GetAnimationIndex(
            slot, GetSwingAnimation(slot, TEE_CLUB, SHOT_MODE_NORMAL)),
        ANIMATION_EVENT_UNK6);
    ClearReactionSpot();
    ConsiderReactionSpot(g_HolePoints.unk60[1]);
    StartReaction(slot, f_CastOrder[f_CastIndex], REACTION_ANIMATION_BIRDIE);
    f_IsReactionShown = TRUE;
    point[AXIS_X] = g_HolePoints.unk60[1][AXIS_X];
    point[AXIS_Y] = g_HolePoints.unk60[1][AXIS_Y];
    point[AXIS_Z] = g_HolePoints.unk60[1][AXIS_Z];
    PlaceCharacter(slot, point, GOLFER_HEADING);
    f_CastSlot = slot;
  }

  if (frame == 3180) {
    ClearEffects();
    PickFramingPitch();
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    f_IsReactionShown = FALSE;
    slot = f_CastSlot ^ 1;
    LoadCharacter(slot, f_CastOrder[f_CastIndex], GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterClub(slot, CLUB_TYPE_PUTTER);
    SetSwingAnimation(slot, CLUB_TYPE_PUTTER, SHOT_MODE_NORMAL);
    swingAnimation = GetAnimationIndex(
        slot, GetSwingAnimation(slot, CLUB_TYPE_PUTTER, SHOT_MODE_NORMAL));
    AdvanceAnimation(slot,
        GetAnimationEventFrame(slot, swingAnimation, ANIMATION_EVENT_UNK6) -
            FINAL_PUTT_LEAD_FRAMES);
    point[AXIS_X] = f_NearCupPoint[AXIS_X];
    point[AXIS_Y] = f_NearCupPoint[AXIS_Y];
    point[AXIS_Z] = f_NearCupPoint[AXIS_Z];
    PlaceCharacter(slot, point, GOLFER_HEADING);
    f_CastSlot = slot;
  }

  if (frame == 3240) {
    ClearEffects();
    FreeCharacterSlot(f_CastSlot);
    ResetStaffRollEase();
    slot = f_CastSlot ^ 1;
    LoadCharacter(
        slot, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
    SetCharacterLighting(slot, CHARACTER_LIGHTING_SCENE);
    SetCharacterAnimation(slot, REACTION_ANIMATION_EAGLE);
    GetAnimationEventFrame(slot,
        GetAnimationIndex(
            slot, GetSwingAnimation(slot, TEE_CLUB, SHOT_MODE_NORMAL)),
        ANIMATION_EVENT_UNK6);
    ClearReactionSpot();
    ConsiderReactionSpot(f_NearCupPoint);
    StartReaction(slot, f_CastOrder[f_CastIndex], REACTION_ANIMATION_EAGLE);
    f_IsReactionShown = TRUE;
    point[AXIS_X] = f_NearCupPoint[AXIS_X];
    point[AXIS_Y] = f_NearCupPoint[AXIS_Y];
    point[AXIS_Z] = f_NearCupPoint[AXIS_Z];
    PlaceCharacter(slot, point, GOLFER_HEADING);
    f_CastSlot = slot;
  }

  if (frame == LAST_FADE_OUT_FRAME) {
    StartFadeOut(LAST_FADE_FRAMES);
  }

  TakePinOut();
  UpdateEffects();
  if (f_ShownSceneColorIndex != g_HoleSetup.conditions.sceneColorIndex) {
    InitSkyColors();
    f_ShownSceneColorIndex = g_HoleSetup.conditions.sceneColorIndex;
  }

  g_Cameras[0].view.fieldOfView = STAFF_ROLL_FIELD_OF_VIEW;
  AnimateAllCharacters();
  if (f_IsReactionShown) {
    PlaceAndViewCharacter(f_CastSlot);
    UpdateReaction(f_CastSlot);
  } else {
    character = GetUsedCharacterSlot(f_CastSlot);
    ClearFramedSpheres();
    SetFramingAngles(f_CastFramingPitch, f_CastFramingYaw);
    GetCharacterPointPosition(f_CastSlot, CAST_FOCUS_POINT, focus);
    if (character != NULL) {
      radius = character->shadowScale * SHADOW_SCALE_TO_RADIUS;
    } else {
      radius = DEFAULT_FRAMED_RADIUS;
    }

    AddFramedSphere(focus, radius);
    FrameSpheres(0.0f);
    BisectFramingPitch(f_CastFramingPitch, f_CastFramingYaw, 0.0f);
    view = &g_Cameras[0].view;
    GetFramedView(view->eye, view->lookAt);
    EaseStaffRollView(
        view->eye, view->lookAt, CAST_EASE_RATE, EASE_LOOK_AT | EASE_EYE);
  }

  g_Cameras[0].view.fieldOfView = STAFF_ROLL_FIELD_OF_VIEW;
  CLAMP_FRAMES(shrink, frame - f_ViewShrinkStartFrame, VIEW_SHRINK_FRAMES);
  SHRINK_VIEW(g_Cameras[0].view, shrink);
  g_Viewports[VIEWPORT_STAFF_ROLL + g_DrawBufferIndex].vp.vscale[0] =
      g_Cameras[0].view.scissorWidth * 4;
  g_Viewports[VIEWPORT_STAFF_ROLL + g_DrawBufferIndex].vp.vscale[1] =
      g_Cameras[0].view.scissorHeight * 4;
  g_Viewports[VIEWPORT_STAFF_ROLL + g_DrawBufferIndex].vp.vtrans[0] =
      (g_Cameras[0].view.scissorX + (g_Cameras[0].view.scissorWidth / 2)) * 4;
  g_Viewports[VIEWPORT_STAFF_ROLL + g_DrawBufferIndex].vp.vtrans[1] =
      (g_Cameras[0].view.scissorY + (g_Cameras[0].view.scissorHeight / 2)) * 4;
}

/*
 * Runs while the loader thread has finished the hole and a controller is
 * plugged in. It plays the songs, ends the scroll, reads in the closing
 * picture, and runs the cast's timeline. Once the closing picture has
 * been up a while, A or Start goes to the boot logos. With debugging on,
 * B restarts the staff roll with the next character first, and C-Down
 * skips 100 frames.
 */
void UpdateStaffRoll(void) {
  EncodedAsset asset;
  s32 pressedButtons = 0;
  s32 lineCount = f_StaffText->lineCount;
  s32 controllerIndex;

  for (controllerIndex = 0; controllerIndex != g_ControllerCount;
      controllerIndex++) {
    pressedButtons |= g_ControllerStates[controllerIndex].pressedButtons;
  }

  if (g_ControllerCount != 0 && g_HasLoaderFinished == TRUE) {
    if (f_StaffRollFrame == 0) {
      f_IsPaintingShown = TRUE;
      f_IsTextScrolling = TRUE;
      StopSong(EARLIER_SONG_FADE_FRAMES);
    }

    if (f_StaffRollFrame == STAFF_SONG_FRAME) {
      PlaySong(SONG_STAFF_ROLL, SONG_FLAG_RESTART);
    }

    if (f_StaffRollFrame == STAFF_SONG_END_FRAME) {
      f_IsTextScrolling = FALSE;
      StopSong(STAFF_SONG_FADE_FRAMES);
    }

    if (f_StaffRollFrame ==
        (lineCount * STAFF_LINE_HEIGHT) + SCROLL_END_DELAY) {
      f_IsTextScrolling = FALSE;
    }

    if (f_StaffRollFrame == CLOSING_PICTURE_READ_FRAME) {
      OpenEncodedAsset(CLOSING_PICTURE_ASSET, &asset);
      ReadEncodedAsset(&f_StaffRollImage, &asset);
    }

    if (f_StaffRollFrame == CLOSING_FRAME) {
      PlaySong(SONG_CLOSING, SONG_FLAG_RESTART);
    }

    if ((pressedButtons & (A_BUTTON | START_BUTTON)) &&
        f_StaffRollFrame >= FIRST_LEAVING_FRAME) {
      RequestSongStop();
      StartHudAnimation(HUD_ANIMATION_SPIRAL);
      g_Scene = SCENE_SWAPPING_MODULES;
      g_NextScene = SCENE_BOOT_LOGOS;
      g_SceneModuleToUnload = MODULE_STAFF_ROLL;
      g_SceneModuleToLoad = MODULE_BOOT_LOGOS;
      f_StaffRollFrame = 0;
      f_IsLeavingStaffRoll = TRUE;
      PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    }

    if (g_IsDebugEnabled) {
      if (pressedButtons & B_BUTTON) {
        OpenEncodedAsset(PAINTING_ASSET, &asset);
        ReadEncodedAsset(&f_StaffRollImage, &asset);
        g_Players[0].characterIndex =
            ((s8)g_Players[0].characterIndex + 1) % CHARACTER_COUNT;
        ShuffleCastOrder((s8)g_Players[0].characterIndex);
        f_TopLineIndex = FIRST_TOP_LINE_INDEX;
        f_StaffRollFrame = -1;
        f_IsLeavingStaffRoll = FALSE;
        f_TextScrollOffset = 0;
        f_IsPaintingShown = TRUE;
        f_DebugRestartCount++;
      }

      if (g_IsDebugEnabled && (pressedButtons & D_CBUTTONS)) {
        f_StaffRollFrame += DEBUG_SKIPPED_FRAMES;
      }
    }

    if (f_StaffRollFrame >= TIMELINE_START_FRAME) {
      RunStaffRollTimeline(f_StaffRollFrame - TIMELINE_START_FRAME);
    }

    if (f_StaffRollFrame < MAX_STAFF_ROLL_FRAME) {
      f_StaffRollFrame++;
    }

    sprintf(g_PrintBuffer, "MAX:%d    ", lineCount);
    PrintDebugText(g_PrintBuffer, LINE_COUNT_TEXT_COLUMN, LINE_COUNT_TEXT_ROW);
    StepNoiseTexture();
    g_SkyState.viewedWindAngles[0] = VIEWED_WIND_ANGLE(
        g_HoleSetup.conditions.windDirection, &g_Cameras[0].view);
    if (g_SkyState.viewedWindAngles[0] > 2 * M_PI) {
      g_SkyState.viewedWindAngles[0] -= 2 * M_PI;
    }

    if (g_SkyState.viewedWindAngles[0] < 0.0f) {
      g_SkyState.viewedWindAngles[0] += 2 * M_PI;
    }
  }
}

/*
 * Module 0x1F's draw. It draws the fade, the painting, the staff text
 * with its shadow, which scrolls up once the painting has faded out, and
 * the closing picture, which fades in from CLOSING_FRAME.
 */
void DrawStaffRoll(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 lineCount = f_StaffText->lineCount;
  s32 fadeAlpha;
  s32 closingAlpha;
  s32 pass;
  s32 index; /* Counts the rows, then the strips; sharing it is there to match
              */
  s32 lineIndex;
  s32 stripHeight;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_DISABLE);
  gDPPipeSync(gfx++);
  gDPSetScissor(gfx++, G_SC_NON_INTERLACE, SCREEN_BORDER, SCREEN_BORDER,
      SCREEN_WIDTH - SCREEN_BORDER, SCREEN_HEIGHT - SCREEN_BORDER);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
  gDPSetBlendColor(gfx++, 255, 255, 255, 1);
  gDPPipeSync(gfx++);
  if (f_FadeLength != 0) {
    if (f_FadeFramesLeft != 0) {
      f_FadeFramesLeft--;
    }

    fadeAlpha = (f_FadeFramesLeft << 8) / f_FadeLength;
    if (f_FadeLength < 0) {
      fadeAlpha += 0x100;
    }

    if (fadeAlpha > 255) {
      fadeAlpha = 255;
    }

    if (fadeAlpha < 0) {
      fadeAlpha = 0;
    }

    if (fadeAlpha == 0) {
      f_FadeLength = 0;
    }

    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, fadeAlpha);
    gDPSetCombineLERP(
        gfx++, 0, 0, 0, 0, 0, 0, 0, PRIMITIVE, 0, 0, 0, 0, 0, 0, 0, PRIMITIVE);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gSPTextureRectangle(gfx++, 0, 0, FADE_RIGHT << 2, FADE_BOTTOM << 2,
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  gDPPipeSync(gfx++);
  gSPLoadGeometryMode(gfx++, 0);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
  gDPSetBlendColor(gfx++, 0, 0, 0, 1);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_DISABLE);
  gDPSetEnvColor(gfx++, 0x80, 0x80, 0xFF, 0xFF);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
  gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
      PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
  if (f_IsPaintingShown) {
    DrawStaffRollPainting(&gfx);
  }

  if (f_StaffRollFrame < CLOSING_FRAME) {
    closingAlpha = 0;
    if (!f_IsPaintingShown && f_IsTextScrolling) {
      if (--f_TextScrollOffset == -STAFF_LINE_HEIGHT) {
        f_TextScrollOffset = 0;
        f_TopLineIndex++;
      }
    }
  } else {
    closingAlpha = (f_StaffRollFrame - CLOSING_FRAME) * CLOSING_FADE_RATE;
    if (closingAlpha > 255) {
      closingAlpha = 255;
    }
  }

  sprintf(g_PrintBuffer, "alpha:%d    ", closingAlpha);
  PrintDebugText(g_PrintBuffer, ALPHA_TEXT_COLUMN, ALPHA_TEXT_ROW);
  if (!f_IsPaintingShown) {
    for (pass = 0; pass < TEXT_PASS_COUNT; pass++) {
      if (pass == TEXT_SHADOW_PASS) {
        gDPSetCombineLERP(gfx++, 0, 0, 0, 0, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
            0, PRIMITIVE, 0, TEXEL0, 0);
      } else {
        gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0,
            0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
      }

      for (index = 0; index != STAFF_ROW_COUNT; index++) {
        lineIndex = index + f_TopLineIndex;
        if (lineIndex >= 0) {
          if (lineIndex < lineCount) {
            DrawStaffLine(&gfx, STAFF_TEXT_LEFT - pass,
                (index * STAFF_LINE_HEIGHT) + f_TextScrollOffset - pass,
                lineIndex);
          }
        }
      }
    }
  }

  if (g_IsDebugEnabled) {
    sprintf(g_PrintBuffer, "%5d", f_StaffRollFrame);
    PrintDebugText(g_PrintBuffer, FRAME_TEXT_COLUMN, FRAME_TEXT_ROW);
  }

  gDPPipeSync(gfx++);
  if (closingAlpha > 0) {
    if (closingAlpha == OPAQUE_ALPHA) {
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
      gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    } else {
      if (closingAlpha < CLOUD_ALPHA) {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
      } else {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
      }

      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, closingAlpha);
      gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0,
          TEXEL0, 0, 0, 0, PRIMITIVE);
    }

    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    gDPPipeSync(gfx++);
    for (index = 0; index != IMAGE_STRIP_COUNT; index++) {
      stripHeight = ((index != IMAGE_STRIP_COUNT - 1) ? IMAGE_STRIP_STEP : 0) |
          IMAGE_STRIP_OVERLAP;
      gDPLoadTextureTile(gfx++,
          (u32) &
              ((u16*)f_StaffRollImage.texels)[index * IMAGE_STRIP_STEP *
                  STAFF_ROLL_IMAGE_WIDTH] &
              ~7,
          G_IM_FMT_RGBA, G_IM_SIZ_16b, STAFF_ROLL_IMAGE_WIDTH, 0, 0, 0,
          STAFF_ROLL_IMAGE_WIDTH - 1, stripHeight - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, SCREEN_BORDER << 2,
          (SCREEN_BORDER + (index * IMAGE_STRIP_STEP)) << 2,
          (SCREEN_BORDER + STAFF_ROLL_IMAGE_WIDTH) << 2,
          (SCREEN_BORDER + (index * IMAGE_STRIP_STEP) + stripHeight) << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

/* Gives the width of the staff text's line; every line is given no height */
#ifdef NON_MATCHING
void MeasureStaffLine(s32 lineIndex, s32* width, s32* height) {
  s32 lineWidth = 0;
  s32 index = 0;
  StaffText* text = f_StaffText;
  s32 start;
  s32 length;
  s32 character;

  start = (text->lineCount * 2) + 2 + text->lines[lineIndex].start;
  length = text->lines[lineIndex].length;
  for (; index != length; index++) {
    character = ((u16*)text)[start + index];
    if (character < STAFF_CHARACTER_LIMIT &&
        (u32)(character - FIRST_OVERLAID_CHARACTER) >=
            OVERLAID_CHARACTER_COUNT) {
      lineWidth += STAFF_GLYPH_WIDTH;
    }
  }

  *width = lineWidth;
  *height = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_14/staff_roll", MeasureStaffLine);
#endif

/* Draws a line of the staff text from left and top, a glyph per character */
void DrawStaffLine(Gfx** displayListHead, s32 left, s32 top, s32 lineIndex) {
  Gfx* gfx = *displayListHead;
  s32 start = (f_StaffText->lines + lineIndex)->start;
  s32 length = (f_StaffText->lines + lineIndex)->length;

  /* In u16s, past the size, the line count and the lines */
  s32 firstCharacter = (f_StaffText->lineCount * 2) + 2;
  s32 width; /* Unused; it is there to match */
  s32 height; /* Unused; it is there to match */
  s32 index;
  s32 character;
  s32 row;
  s32 column;
  s32 glyphLeft;
  s32 glyphTop;

  MeasureStaffLine(lineIndex, &width, &height);
  gDPPipeSync(gfx++);
  for (index = 0; index != length; index++) {
    character = ((u16*)f_StaffText)[start + index + firstCharacter];
    if (character < STAFF_CHARACTER_LIMIT) {
      row = character / STAFF_FONT_COLUMN_COUNT;
      column = character - (row * STAFF_FONT_COLUMN_COUNT);
      glyphLeft = column * STAFF_GLYPH_WIDTH;
      glyphTop = row * STAFF_GLYPH_HEIGHT;
      gDPPipeSync(gfx++);
      gDPLoadTextureTile_4b(gfx++, (u32)(f_StaffFont + IMAGE_HEADER_SIZE) & ~7,
          G_IM_FMT_I, STAFF_FONT_WIDTH, 0, glyphLeft, glyphTop,
          glyphLeft + STAFF_GLYPH_WIDTH - 1, glyphTop + STAFF_GLYPH_HEIGHT - 1,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      if ((u32)(character - FIRST_OVERLAID_CHARACTER) <
          OVERLAID_CHARACTER_COUNT) {
        gSPTextureRectangle(gfx++, (left - OVERLAID_CHARACTER_SHIFT_X) << 2,
            (top - OVERLAID_CHARACTER_SHIFT_Y) << 2,
            (left + OVERLAID_CHARACTER_SHIFT_X) << 2,
            (top - OVERLAID_CHARACTER_SHIFT_Y + STAFF_GLYPH_HEIGHT) << 2,
            G_TX_RENDERTILE, (glyphLeft) << 5, (glyphTop) << 5, TEXEL_STEP_ONE,
            TEXEL_STEP_ONE);
      } else {
        gSPTextureRectangle(gfx++, left << 2, top << 2,
            (left + STAFF_GLYPH_WIDTH) << 2, (top + STAFF_GLYPH_HEIGHT) << 2,
            G_TX_RENDERTILE, (glyphLeft) << 5, (glyphTop) << 5, TEXEL_STEP_ONE,
            TEXEL_STEP_ONE);
        left += f_StaffGlyphAdvances[character];
      }
    }
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/*
 * Draws the painting in strips, scrolled down it by a row a frame from
 * PAINTING_SCROLL_FRAME. It fades in at the start, and fades out once the
 * timeline starts; when it has faded out, f_IsPaintingShown is cleared.
 */
void DrawStaffRollPainting(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 alpha;
  s32 stripIndex;
  s32 stripHeight;
  s32 scrolledRows;
  s32 texelOffset;

  if (f_StaffRollFrame > TIMELINE_START_FRAME) {
    alpha = OPAQUE_ALPHA -
        ((f_StaffRollFrame - TIMELINE_START_FRAME) * PAINTING_FADE_RATE);
    if (alpha < 0) {
      f_IsPaintingShown = FALSE;
      alpha = 0;
    }
  } else {
    alpha = OPAQUE_ALPHA;
  }

  gDPPipeSync(gfx++);
  if (alpha > 0) {
    if (alpha == OPAQUE_ALPHA) {
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
      gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      if (f_StaffRollFrame < PAINTING_FADE_IN_FRAMES) {
        gDPSetPrimColor(
            gfx++, 0, 0, 255, 255, 255, f_StaffRollFrame * PAINTING_FADE_RATE);
        gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE_ALPHA, 0, 0, 0, 0, 1,
            TEXEL0, 0, PRIMITIVE_ALPHA, 0, 0, 0, 0, 1);
      }
    } else {
      if (alpha < CLOUD_ALPHA) {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
      } else {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
      }

      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
      gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0,
          TEXEL0, 0, 0, 0, PRIMITIVE);
    }

    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, (u32)f_StaffRollImage.texels & ~7);
    for (stripIndex = 0; stripIndex != IMAGE_STRIP_COUNT; stripIndex++) {
      scrolledRows = f_StaffRollFrame - PAINTING_SCROLL_FRAME;
      if (scrolledRows > PAINTING_SCROLL_LIMIT) {
        scrolledRows = PAINTING_SCROLL_LIMIT;
      }

      if (scrolledRows < 0) {
        scrolledRows = 0;
      }

      texelOffset = (scrolledRows * STAFF_ROLL_IMAGE_WIDTH) +
          LARGE_PALETTED_TEXELS_OFFSET;
      stripHeight =
          ((stripIndex != IMAGE_STRIP_COUNT - 1) ? IMAGE_STRIP_STEP : 0) |
          IMAGE_STRIP_OVERLAP;
      gDPLoadTextureTile(gfx++,
          (u32) &
              ((u8*)&f_StaffRollImage)[(stripIndex * IMAGE_STRIP_STEP *
                                           STAFF_ROLL_IMAGE_WIDTH) +
                  texelOffset] &
              ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, STAFF_ROLL_IMAGE_WIDTH, 0, 0, 0,
          STAFF_ROLL_IMAGE_WIDTH - 1, stripHeight - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, SCREEN_BORDER << 2,
          (SCREEN_BORDER + (stripIndex * IMAGE_STRIP_STEP)) << 2,
          (SCREEN_BORDER + STAFF_ROLL_IMAGE_WIDTH) << 2,
          (SCREEN_BORDER + (stripIndex * IMAGE_STRIP_STEP) + stripHeight) << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

/* Unreferenced */
void DoNothingAfterStaffRoll(void) {}
