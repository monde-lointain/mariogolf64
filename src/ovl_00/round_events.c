#include "common.h"

#include "round_events.h"

#include "asset.h"
#include "character.h"
#include "controller.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "game_flags.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "movie.h"
#include "pin.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "screen.h"
#include "screen_layers.h"
#include "shot_effects.h"
#include "sound.h"
#include "text.h"
#include "transfer_pak.h"
#include "variables.h"

/* The buffer that a movie's images, or the wind Fly Guy's, are read into */
#define MOVIE_BUFFER_SIZE 0x6270
/* The movie frame after which the hole intro goes on */
#define MOVIE_CUE_FRAME 12

#define FLY_GUY_IMAGE_ASSET 0x6CF
#define WIND_STREAK_IMAGE_ASSET 0x6AF
/* With its header; the wind streak follows it */
#define FLY_GUY_IMAGE_SIZE 0x808

/* Game Boy balls 1 to 4 copy a character's contact effect, and 5 to 7 have
 * their own */
#define GAME_BOY_BALL_TYPE_PLAIN 0
#define GAME_BOY_BALL_TYPE_MARIO 1
#define GAME_BOY_BALL_TYPE_BOWSER 2
#define GAME_BOY_BALL_TYPE_WARIO 3
#define GAME_BOY_BALL_TYPE_HEAVY_HITTER 4
#define GAME_BOY_BALL_TYPE_5 5
#define GAME_BOY_BALL_TYPE_6 6
#define GAME_BOY_BALL_TYPE_7 7

/* Where the contact effect's images go in g_SharedEffectImages, and their
 * assets */
#define CONTACT_IMAGE_OFFSET 0x970
#define CONTACT_SECOND_IMAGE_OFFSET 0x1578
#define MARIO_CONTACT_IMAGE_ASSET 0x6BE
#define BOWSER_CONTACT_IMAGE_ASSET 0x6AE
#define WARIO_CONTACT_IMAGE_ASSET 0x6A7
#define WARIO_CONTACT_SECOND_IMAGE_ASSET 0x6A8
#define HEAVY_HITTER_CONTACT_IMAGE_ASSET 0x6A6

/* A banner's letters, each an image drawn on its own square */
#define BANNER_MAX_LETTERS 11

typedef struct Banner {
  /* 0x0 */ s16 unk0;
  /* 0x2 */ u16 imageAsset;
  /* 0x4 */ s8 letterCount;
  /* 0x5 */ u8 letterWidth;
  /* 0x6 */ u8 letterHeight;
  /* 0x7 */ s8 animation;
} Banner; /* size = 0x8 */

/* Where a letter of the shown banner is, and its matrix in each matrix buffer
 */
typedef struct BannerLetter {
  /* 0x00 */ f32 x;
  /* 0x04 */ f32 y;
  /* 0x08 */ f32 scale;
  /* 0x0C */ s32 unkC;
  /* 0x10 */ Mtx matrices[2];
} BannerLetter; /* size = 0x90 */

/*
 * A banner's left edge (BANNER_LAYOUT_LEFT), then where each letter sits from
 * it (BANNER_LAYOUT_FIRST_LETTER onwards)
 */
#define BANNER_LAYOUT_LEFT 0
#define BANNER_LAYOUT_FIRST_LETTER 1
typedef s16 BannerLayout[BANNER_LAYOUT_FIRST_LETTER + BANNER_MAX_LETTERS];

/* Frames since the banner started, or BANNER_FRAME_NONE */
extern s32 sBannerFrame;
extern s32 sCurrentBanner;
/* The banner whose images are in sMovieBuffer */
extern s32 sLoadedBanner;
/* The number that BANNER_STROKES_OVER_PAR's banner shows after its "+" */
extern s32 sBannerNumber;
/* The "+" and the number's digits */
extern s32 sBannerNumberLetterCount;
extern Vtx sBannerLetterVertices[4];
extern Banner sBanners[];
extern Mtx sBannerMatrices[2];
extern BannerLetter sBannerLetters[];
extern BannerLayout sBannerLayouts[];
extern s32 sIsMovieBufferAllocated;
extern u8* sMovieBuffer;
/* The contact effect's quad spans from vertex 0 to vertex 1 */
extern Vtx sContactVertices[2];
extern s32 sIsContactEffectFading;
extern s32 sContactStyle;
/* The texture coordinates of the quad's far corner, in S10.5 */
extern s32 sContactTextureS;
extern s32 sContactTextureT;
extern Mtx sContactMatrices[];
extern s32 sContactTrailHead;
extern f32 sContactSpinAngle;
extern Vtx sContactSpinVertices[4];
extern Mtx sContactRibbonMatrices[2];
extern Mtx sContactSpinMatrices[2];
extern s32 sCurrentMovie;
extern s32 sMovieFrame;
/* Counts up to MOVIE_TICKS_PER_FRAME between the frames of a Monty Mole movie
 */
extern s32 sMovieTick;
/* Sets the render state for drawing a movie's paletted strips */
extern Gfx sMovieSetupDisplayList[];
extern Gfx sMovieImageSetupDisplayList[];
extern s32 sMovieBackBuffer; /* Which of a two-buffer movie's frame buffers to
                                read into next */
extern u16 sToadMovieFrameAssets[];
extern u16 sMontyMoleMovieFrameAssets[];
extern u16 sGoombaMovieFrameAssets[];
/* Counts the frames that DrawMovie has drawn, up to MOVIE_DRAW_COUNT_MAX */
extern s32 sMovieDrawCount;
typedef struct LakituFlight {
  /* 0x0 */ f32 x;
  /* 0x4 */ f32 y;
  /* 0x8 */ f32 scale;
} LakituFlight;

typedef struct LakituVelocity {
  /* 0x0 */ f32 x;
  /* 0x4 */ f32 y;
} LakituVelocity;

extern LakituFlight sLakitu;
extern LakituVelocity sLakituVelocity;
extern Mtx sReadyGoMatrices[2];
extern Vtx sReadyGoVertices[];
extern u16 sLakituMovieFrameAssets[];
extern s32 sWindFlyGuyFrame;
extern s32 sWindFlyGuyAsset;
extern s32 sIsFlipMessageLeaving;
extern s32
    sFlipMessageFrame; /* FLIP_MESSAGE_FRAME_NONE while no message is shown */
extern s32 sFlipMessageId;
extern s32 sFlipMessageHalfWidth;
extern s32 sFlipMessageHalfHeight;
extern Vtx sFlipMessageBoardVertices[4];
extern Mtx sFlipMessageMatrices[2];

/* ShowFlipMessage's arguments, besides a message to flip in */
#define FLIP_MESSAGE_RESET (-2)
#define FLIP_MESSAGE_LEAVE (-1)
#define FLIP_MESSAGE_FRAME_NONE (-1)
#define FLIP_MESSAGE_GLYPH_GROUP 0
#define FLIP_MESSAGE_TEXT_SLOT 0
#define FLIP_MESSAGE_X 160
#define FLIP_MESSAGE_Y 114
#define FLIP_MESSAGE_FONT 1
/* How far the board reaches past the message on each side */
#define FLIP_MESSAGE_BOARD_MARGIN_X 6
#define FLIP_MESSAGE_BOARD_MARGIN_Y 14
#define FLIP_MESSAGE_OPEN_FRAME 10
#define BANNER_FRAME_NONE (-1)
#define CONTACT_MATRIX_COUNT 81
/* A movie image starts with its size, then its palette, then its CI8 pixels */
#define MOVIE_IMAGE_WIDTH_OFFSET 2
#define MOVIE_IMAGE_HEIGHT_OFFSET 4
#define MOVIE_IMAGE_PALETTE_OFFSET 8
#define MOVIE_IMAGE_PIXELS_OFFSET 0x208
#define MOVIE_IMAGE_STRIP_HEIGHT 6
#define SHRINKING_IMAGE_SOUND_DRAW 26
#define SOUND_SHRINKING_IMAGE 0x43
#define SHRINKING_IMAGE_SOUND_CHANNEL 13
#define SHRINKING_IMAGE_SOUND_VOLUME 63
#define SHRINKING_IMAGE_DRAW_COUNT 90
/* The image shrinks from double size over the quarter turn's 8 frames */
#define SHRINKING_IMAGE_TURN_PER_FRAME (M_PI / 16)
#define SHRINKING_IMAGE_CENTRE_X 160
#define SHRINKING_IMAGE_CENTRE_Y 104
#define SHRINKING_IMAGE_STRIPS_ABOVE_CENTRE 4
/* The effect sits a little below the ball's centre */
#define CONTACT_DROP_PER_PIN_SCALE 0.3f
#define CONTACT_SOUND_CHANNEL 2
#define CONTACT_STYLE_SOUND_CHANNEL 15
#define CONTACT_SOUND_VOLUME 127
#define SOUND_CONTACT_PLAIN 0x51
#define SOUND_CONTACT_MARIO 0x2E /* Also Bowser's */
#define SOUND_CONTACT_HEAVY_HITTER 0x2F
#define SOUND_CONTACT_WARIO 0x30
#define CONTACT_PUTT_Y -2
/* The banner that shows "+N", which BANNER_STROKES_OVER_PAR + N starts */
#define BANNER_OVER_PAR_NUMBER (BANNER_STROKES_OVER_PAR + 1)
#define BANNER_LETTER_SPACING 32
#define BANNER_Z -300.0f
/* The board starts face down and turns this far each frame until it faces the
 * camera */
#define FLIP_MESSAGE_DEGREES_PER_FRAME 18.0f
#define FLIP_MESSAGE_START_DEGREES 180.0f
#define FLIP_MESSAGE_GROWTH_PER_FRAME 0.1f
/* The board hinges on its left edge */
#define FLIP_MESSAGE_BOARD_Y 20.0f
#define FLIP_MESSAGE_BOARD_Z -300.0f
#define FLIP_MESSAGE_BOARD_ALPHA 159

/* Each letter image follows the images' header and palette */
#define BANNER_LETTER_IMAGES_OFFSET 0x208
#define BANNER_LETTER_IMAGE_SIZE 0x800
#define BANNER_LETTER_IMAGE_WIDTH 32
#define BANNER_LETTER_IMAGE_HEIGHT 64
/* The number banner's first image is "+", and its digit images follow */
#define BANNER_PLUS_IMAGE 0
#define BANNER_FIRST_DIGIT_IMAGE 1

#define MOVIE_DRAW_COUNT_MAX 1000

/* Each movie frame read into sMovieBuffer, after its palette */
#define MOVIE_PALETTE_OFFSET 0x4208
#define MOVIE_FRAME_OFFSET 0x4408
#define ASSET_TOAD_MOVIE_PALETTE 0x92C
#define TOAD_FRAME_OFFSET 0x200
#define TOAD_BACK_FRAME_OFFSET 0x2880
#define TOAD_FRAME_SIZE (TOAD_BACK_FRAME_OFFSET - TOAD_FRAME_OFFSET)
#define TOAD_FRAME_COUNT 12
#define TOAD_WIDTH 112
#define TOAD_STRIP_HEIGHT 11
#define TOAD_STRIP_COUNT 8
#define TOAD_STRIP_SIZE (TOAD_WIDTH * TOAD_STRIP_HEIGHT)
#define TOAD_X 104
#define TOAD_Y 86
/* The flag swishes as it swings to each side */
#define TOAD_SWING_LEFT_FRAME 8
#define TOAD_SWING_RIGHT_FRAME 2
#define SOUND_FLAG_SWISH 0x1F
#define FLAG_SWISH_SOUND_CHANNEL 15
#define FLAG_SWISH_SOUND_VOLUME 127
#define FLAG_SWISH_LEFT_PAN 16
#define FLAG_SWISH_RIGHT_PAN 239
#define FLAG_SWISH_PITCH 12.0f
#define SOUND_OUT_OF_BOUNDS 0x77
#define OUT_OF_BOUNDS_SOUND_DRAW 16
#define OUT_OF_BOUNDS_SOUND_CHANNEL 13
/* Toad draws in the inset view, kept off its frame */
#define INSET_VIEW_CAMERA 1
#define INSET_FRAME_WIDTH 2
#define INSET_FRAME_HEIGHT 1
#define ASSET_LAKITU 0x64E
#define LAKITU_BACK_IMAGE_OFFSET 0x1008
#define LAKITU_START_X 384.0f
#define LAKITU_START_Y -64.0f
#define LAKITU_START_SCALE 0.7f
/* Lakitu flies in to its hover point over LAKITU_ARRIVE_DRAW draws */
#define LAKITU_HOVER_X 192.0f
#define LAKITU_HOVER_Y 96.0f
#define LAKITU_ARRIVE_DRAW 30
/* Then it counts down on its starting light until LAKITU_LEAVE_DRAW, and flies
 * off to the left */
#define LAKITU_LEAVE_DRAW 120
#define LAKITU_TICKS_PER_FRAME 8
#define LAKITU_LAST_FRAME 12
#define LAKITU_LEAVE_ACCELERATION_X 0.2f
#define LAKITU_LEAVE_ACCELERATION_Y 0.1f
#define LAKITU_LEAVE_GROWTH 0.01f
#define LAKITU_GONE_X -64.0f
#define LAKITU_FRAME_SIZE 0x1008
#define LAKITU_WIDTH 56
#define LAKITU_STRIP_HEIGHT 32
#define LAKITU_STRIP_COUNT 2
#define LAKITU_STRIP_SIZE (LAKITU_WIDTH * LAKITU_STRIP_HEIGHT)
#define LAKITU_RED_LIGHT_DRAW 50
#define LAKITU_SECOND_RED_LIGHT_DRAW 80
#define LAKITU_GO_LIGHT_DRAW 110
#define SOUND_START_LIGHT_RED 0x23
#define SOUND_START_LIGHT_GO 0x24
#define START_LIGHT_SOUND_VOLUME 127
#define ASSET_MONTY_MOLE_MOVIE_PALETTE 0x914
#define ASSET_GOOMBA_MOVIE_PALETTE 0x91F
#define ASSET_DRIVE_RECORD_TEXT 0x55E
#define ASSET_LONGEST_DRIVE_TEXT 0x55C
#define ASSET_NEAREST_PIN_TEXT 0x55B
#define ASSET_ALBATROSS_PUTT_TEXT 0x556
#define ASSET_EAGLE_PUTT_TEXT 0x557
#define ASSET_BIRDIE_PUTT_TEXT 0x558
#define ASSET_SINK_TO_WIN_TEXT 0x559
#define ASSET_MISS_AND_LOSE_TEXT 0x55A
#define ASSET_SINK_TO_TIE_TEXT 0x55D
#define ASSET_READY_TEXT 0x976
#define ASSET_GO_TEXT 0x977
#define GO_TEXT_OFFSET 0x2508
#define MOVIE_TICKS_PER_FRAME 3
#define MOVIE_SOUND_CHANNEL 2
#define MOVIE_SOUND_VOLUME 100
#define MOVIE_SOUND_REVERB 10
#define MOVIE_SOUND_PAN 127

#define SOUND_MONTY_MOLE 0x2A
#define MONTY_MOLE_FRAME_COUNT 17
/* Each frame is drawn in strips, as a frame is too big to load at once */
#define MONTY_MOLE_WIDTH 108
#define MONTY_MOLE_STRIP_HEIGHT 18
#define MONTY_MOLE_STRIP_COUNT 4
#define MONTY_MOLE_STRIP_SIZE (MONTY_MOLE_WIDTH * MONTY_MOLE_STRIP_HEIGHT)
#define MONTY_MOLE_X 106
#define MONTY_MOLE_Y 62

#define SOUND_GOOMBA 0x2A
#define GOOMBA_FRAME_COUNT 11
#define GOOMBA_WIDTH 80
#define GOOMBA_STRIP_HEIGHT 25
/* The last strip holds what is left of the 73 rows */
#define GOOMBA_LAST_STRIP_HEIGHT 23
#define GOOMBA_STRIP_COUNT 3
#define GOOMBA_STRIP_SIZE (GOOMBA_WIDTH * GOOMBA_STRIP_HEIGHT)
#define GOOMBA_X 120
#define GOOMBA_Y 62
/* The controller whose buttons play movies under
 * GAME_FLAG_TEST_AIM_TILT_AND_MOVIES */
#define MOVIE_TEST_CONTROLLER 1

void ResetPath(void);
void DrawContestWinnerMovie(Gfx** displayListHead);
void DrawDriveRecord(Gfx** displayListHead);
void DrawPuttCaption(Gfx** displayListHead);
void DrawReadyGoMovie(Gfx** displayListHead);
void DrawMontyMoleMovie(Gfx** displayListHead);
void DrawGoombaMovie(Gfx** displayListHead);
void DrawLakituMovie(Gfx** displayListHead);
void DrawToadMovie(Gfx** displayListHead);

void AllocMovieBuffer(void) {
  EncodedAsset unused;

  sCurrentBanner = -1;
  sIsMovieBufferAllocated = TRUE;
  sMovieBuffer = AllocMemory(MOVIE_BUFFER_SIZE);
  sLoadedBanner = -1;
}

void FreeMovieBuffer(void) {
  sIsMovieBufferAllocated = FALSE;
  FreeMemory(&sMovieBuffer);
}

void StartBanner(s32 banner) {
  EncodedAsset asset;
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
  Mat4 matrix;
  s32 i;

  if (sIsMovieBufferAllocated != TRUE) {
    return;
  }
  if (banner == BANNER_HIDE_NICE_SHOT) {
    if (sCurrentBanner == BANNER_NICE_SHOT) {
      sBannerFrame = BANNER_FRAME_NONE;
    }
    return;
  }
  if (banner == BANNER_NONE) {
    sBannerFrame = BANNER_FRAME_NONE;
    return;
  }
  sCurrentBanner = banner;
  sBannerFrame = 0;
  if (banner > BANNER_STROKES_OVER_PAR) {
    sBannerNumber = banner - BANNER_STROKES_OVER_PAR;
    sCurrentBanner = BANNER_OVER_PAR_NUMBER;
    if (sLoadedBanner < BANNER_OVER_PAR_NUMBER ||
        sLoadedBanner == BANNER_NONE) {
      OpenEncodedAsset(sBanners[BANNER_OVER_PAR_NUMBER].imageAsset, &asset);
      ReadEncodedAsset(sMovieBuffer, &asset);
    }
    sLoadedBanner = sCurrentBanner;
    /* The "+" and the digits */
    if (sBannerNumber < 10) {
      sBannerNumberLetterCount = 2;
    } else if (sBannerNumber < 100) {
      sBannerNumberLetterCount = 3;
    } else {
      sBannerNumberLetterCount = 4;
    }
    for (i = 0; i != sBannerNumberLetterCount; i++) {
      sBannerLetters[i].x =
          i * BANNER_LETTER_SPACING -
          (sBannerNumberLetterCount - 1) * (BANNER_LETTER_SPACING / 2);
      sBannerLetters[i].y = 0.0f;
      sBannerLetters[i].unkC = 0;
      sBannerLetters[i].scale = 1.0f;
      rotation[AXIS_X] = 0;
      rotation[AXIS_Y] = 0;
      rotation[AXIS_Z] = 0;
      translation[AXIS_X] = sBannerLetters[i].x;
      translation[AXIS_Y] = sBannerLetters[i].y;
      translation[AXIS_Z] = BANNER_Z;
      scale[AXIS_X] = sBannerLetters[i].scale;
      scale[AXIS_Y] = sBannerLetters[i].scale;
      scale[AXIS_Z] = sBannerLetters[i].scale;
      BuildTransformMatrix(matrix, rotation, translation, scale);
      ConvertMatrixToFixed(matrix,
                           &sBannerLetters[i].matrices[g_MatrixBufferIndex]);
    }
  }
  if ((sLoadedBanner != sCurrentBanner &&
       sCurrentBanner < BANNER_OVER_PAR_NUMBER) ||
      sLoadedBanner == BANNER_NONE) {
    OpenEncodedAsset(sBanners[sCurrentBanner].imageAsset, &asset);
    ReadEncodedAsset(sMovieBuffer, &asset);
    sLoadedBanner = sCurrentBanner;
    for (i = 0; i != sBanners[sCurrentBanner].letterCount; i++) {
      sBannerLetters[i].x =
          (f32)sBannerLayouts[sCurrentBanner][BANNER_LAYOUT_FIRST_LETTER + i] +
          (f32)sBannerLayouts[sCurrentBanner][BANNER_LAYOUT_LEFT];
      sBannerLetters[i].y = 0.0f;
      sBannerLetters[i].unkC = 0;
      sBannerLetters[i].scale = 1.0f;
      rotation[AXIS_X] = 0;
      rotation[AXIS_Y] = 0;
      rotation[AXIS_Z] = 0;
      translation[AXIS_X] = sBannerLetters[i].x;
      translation[AXIS_Y] = sBannerLetters[i].y;
      translation[AXIS_Z] = BANNER_Z;
      scale[AXIS_X] = sBannerLetters[i].scale;
      scale[AXIS_Y] = sBannerLetters[i].scale;
      scale[AXIS_Z] = sBannerLetters[i].scale;
      BuildTransformMatrix(matrix, rotation, translation, scale);
      ConvertMatrixToFixed(matrix,
                           &sBannerLetters[i].matrices[g_MatrixBufferIndex]);
    }
  }
}

/* Draws the number banner's "+" and digits */
void DrawNumberBanner(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 i;
  s32 imageOffset;

  sprintf(g_PrintBuffer, "%d", sBannerNumber);
  for (i = 0; i != sBannerNumberLetterCount; i++) {
    if (i == 0) {
      imageOffset = BANNER_PLUS_IMAGE * BANNER_LETTER_IMAGE_SIZE;
    } else {
      imageOffset = (g_PrintBuffer[i - 1] - '0') * BANNER_LETTER_IMAGE_SIZE +
                    BANNER_FIRST_DIGIT_IMAGE * BANNER_LETTER_IMAGE_SIZE;
    }
    LoadTextureBlock(&gfx,
                     sMovieBuffer + (imageOffset + BANNER_LETTER_IMAGES_OFFSET),
                     G_IM_FMT_CI, G_IM_SIZ_8b, BANNER_LETTER_IMAGE_WIDTH,
                     BANNER_LETTER_IMAGE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    gSPMatrix(
        gfx++,
        OS_K0_TO_PHYSICAL(&sBannerLetters[i].matrices[g_MatrixBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPVertex(gfx++, sBannerLetterVertices, 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
  }
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* Draws the shown banner's letters, each placed within the whole banner's
 * matrix */
void DrawBannerLetters(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 i;

  for (i = 0; i != sBanners[sCurrentBanner].letterCount; i++) {
    LoadTextureBlock(&gfx,
                     sMovieBuffer + BANNER_LETTER_IMAGES_OFFSET +
                         i * BANNER_LETTER_IMAGE_SIZE,
                     G_IM_FMT_CI, G_IM_SIZ_8b, BANNER_LETTER_IMAGE_WIDTH,
                     BANNER_LETTER_IMAGE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sBannerMatrices[g_MatrixBufferIndex]),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(
        gfx++,
        OS_K0_TO_PHYSICAL(&sBannerLetters[i].matrices[g_MatrixBufferIndex]),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(gfx++, sBannerLetterVertices, 4, 0);
    gSP2Triangles(gfx++, 0, 1, 3, 0, 1, 2, 3, 0);
  }
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

void func_8020EEEC_ovl_00(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  *displayListHead = gfx;
}

/* How a banner comes in, as its Banner.animation */
/* Shrinks onto the screen while fading in, then leaves */
#define BANNER_ANIMATION_ZOOM 0
#define BANNER_ANIMATION_SPIN 1 /* Grows while turning once */
/* Slides in from the right and wobbles, then leaves */
#define BANNER_ANIMATION_SLIDE 2
/* Like BANNER_ANIMATION_ZOOM, but larger and stays */
#define BANNER_ANIMATION_ZOOM_STAY 3

/* The debug flag that lets the D-Pad pick a banner and start it */
#define GAME_FLAG_TEST_BANNERS 0x6D
#define TEST_BANNER_LAST 37

#define BANNER_LAST_FRAME 200
#define BANNER_END_FRAME 60
#define SOUND_BANNER 0x44
#define BANNER_SOUND_FRAME 10
#define BANNER_SOUND_CHANNEL 13
#define BANNER_SOUND_VOLUME 63
#define BANNER_Y 64.0f
/* The zooming banners start large and shrink by this each frame */
#define BANNER_ZOOM_FRAMES 20
#define BANNER_ZOOM_PER_FRAME 0.3f
#define BANNER_ZOOM_SMALLEST 1.2f
#define BANNER_ZOOM_STAY_SMALLEST 1.0f
#define BANNER_SPIN_GROWTH_PER_FRAME 0.1f
#define BANNER_SLIDE_FRAMES 10
#define BANNER_SLIDE_PER_FRAME 36.0f
/* The sliding banner wobbles over these frames, less and less */
#define BANNER_WOBBLE_FRAMES 20
#define BANNER_WOBBLE_DEPTH 0.4f
#define BANNER_WOBBLE_DEPTH_LOSS 0.05f
#define BANNER_WOBBLE_SMALLEST_DEPTH 0.1f
#define BANNER_WOBBLE_TURN_PER_FRAME (M_PI / 10)
/* The banner's shading shows through less as it fades in */
#define BANNER_MAX_ALPHA 255
#define BANNER_BLEND_ALPHA 1

extern s32 sTestBanner;

/* Animates the shown banner and draws it, its letters each on their own square
 */
void DrawBanner(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
  Mat4 matrix;
  s32 alpha;
  f32 letterScale;
  f32 angle;
  f32 slide;
  f32 wobbleDepth;
  s32 i;

  angle = 0.0f;
  if (IsGameFlagSet(GAME_FLAG_TEST_BANNERS)) {
    if ((g_ControllerStates[0].pressedButtons & L_JPAD) && sTestBanner > 0) {
      sTestBanner--;
    }
    if ((g_ControllerStates[0].pressedButtons & R_JPAD) &&
        sTestBanner < TEST_BANNER_LAST) {
      sTestBanner++;
    }
    if (g_ControllerStates[0].pressedButtons & D_JPAD) {
      StartBanner(sTestBanner);
    }
  }
  if (sCurrentBanner == BANNER_NONE || sBannerFrame == BANNER_FRAME_NONE) {
    return;
  }
  sBannerFrame++;
  if (sBannerFrame > BANNER_LAST_FRAME) {
    sBannerFrame = BANNER_LAST_FRAME;
  }
  if (sBannerFrame == BANNER_SOUND_FRAME) {
    PlaySound(SOUND_BANNER, BANNER_SOUND_CHANNEL, BANNER_SOUND_VOLUME);
  }
  switch (sBanners[sCurrentBanner].animation) {
    case BANNER_ANIMATION_ZOOM:
      letterScale = (BANNER_ZOOM_FRAMES - sBannerFrame) * BANNER_ZOOM_PER_FRAME;
      if (letterScale < BANNER_ZOOM_SMALLEST) {
        letterScale = BANNER_ZOOM_SMALLEST;
      }
      alpha = sBannerFrame * sBannerFrame;
      if (alpha > BANNER_MAX_ALPHA) {
        alpha = BANNER_MAX_ALPHA;
      }
      if (sBannerFrame == BANNER_END_FRAME) {
        sBannerFrame = BANNER_FRAME_NONE;
      }
      rotation[AXIS_X] = 0;
      rotation[AXIS_Y] = 0;
      rotation[AXIS_Z] = angle;
      translation[AXIS_X] = 0.0f;
      translation[AXIS_Z] = 0;
      translation[AXIS_Y] = BANNER_Y;
      scale[AXIS_X] = 1.0f;
      scale[AXIS_Y] = 1.0f;
      scale[AXIS_Z] = 1.0f;
      BuildTransformMatrix(matrix, rotation, translation, scale);
      ConvertMatrixToFixed(matrix, &sBannerMatrices[g_MatrixBufferIndex]);
      break;
    case BANNER_ANIMATION_SPIN:
      letterScale = sBannerFrame * BANNER_SPIN_GROWTH_PER_FRAME;
      if (letterScale > 1.0f) {
        letterScale = 1.0f;
      }
      alpha = sBannerFrame * sBannerFrame;
      if (alpha > BANNER_MAX_ALPHA) {
        alpha = BANNER_MAX_ALPHA;
      }
      angle = sBannerFrame * BANNER_SPIN_GROWTH_PER_FRAME * (2 * M_PI);
      if (angle > 2 * M_PI) {
        angle = 0.0f;
      }
      rotation[AXIS_X] = 0;
      rotation[AXIS_Y] = 0;
      rotation[AXIS_Z] = angle;
      translation[AXIS_X] = 0.0f;
      translation[AXIS_Z] = 0;
      scale[AXIS_X] = 1.0f;
      scale[AXIS_Y] = 1.0f;
      scale[AXIS_Z] = 1.0f;
      translation[AXIS_Y] = BANNER_Y;
      BuildTransformMatrix(matrix, rotation, translation, scale);
      ConvertMatrixToFixed(matrix, &sBannerMatrices[g_MatrixBufferIndex]);
      break;
    case BANNER_ANIMATION_SLIDE:
      slide = (BANNER_SLIDE_FRAMES - sBannerFrame) * BANNER_SLIDE_PER_FRAME;
      if (slide < 0.0f) {
        slide = 0.0f;
      }
      if (sBannerFrame >= BANNER_SLIDE_FRAMES &&
          sBannerFrame < BANNER_SLIDE_FRAMES + BANNER_WOBBLE_FRAMES) {
        wobbleDepth =
            BANNER_WOBBLE_DEPTH -
            (sBannerFrame - BANNER_WOBBLE_FRAMES) * BANNER_WOBBLE_DEPTH_LOSS;
        if (wobbleDepth < BANNER_WOBBLE_SMALLEST_DEPTH) {
          wobbleDepth = BANNER_WOBBLE_SMALLEST_DEPTH;
        }
        letterScale = (sinf((sBannerFrame - BANNER_WOBBLE_FRAMES) *
                            BANNER_WOBBLE_TURN_PER_FRAME) +
                       1.0f) *
                          wobbleDepth +
                      (1.0f - wobbleDepth);
      } else {
        letterScale = 1.0f;
      }
      /* Keeps the banner's left edge in place while it stretches */
      slide += sBannerLayouts[sCurrentBanner][BANNER_LAYOUT_LEFT] *
               (1.0f - letterScale);
      rotation[AXIS_X] = 0;
      rotation[AXIS_Y] = 0;
      rotation[AXIS_Z] = 0.0f;
      translation[AXIS_Y] = BANNER_Y;
      translation[AXIS_Z] = 0;
      scale[AXIS_X] = letterScale;
      scale[AXIS_Y] = 1.0f;
      scale[AXIS_Z] = 1.0f;
      translation[AXIS_X] = slide;
      BuildTransformMatrix(matrix, rotation, translation, scale);
      ConvertMatrixToFixed(matrix, &sBannerMatrices[g_MatrixBufferIndex]);
      letterScale = 1.0f;
      if (sBannerFrame == BANNER_END_FRAME) {
        sBannerFrame = BANNER_FRAME_NONE;
      }
      break;
    case BANNER_ANIMATION_ZOOM_STAY:
      letterScale = (BANNER_ZOOM_FRAMES - sBannerFrame) * BANNER_ZOOM_PER_FRAME;
      if (letterScale < BANNER_ZOOM_STAY_SMALLEST) {
        letterScale = BANNER_ZOOM_STAY_SMALLEST;
      }
      alpha = sBannerFrame * sBannerFrame;
      if (alpha > BANNER_MAX_ALPHA) {
        alpha = BANNER_MAX_ALPHA;
      }
      rotation[AXIS_X] = 0;
      rotation[AXIS_Y] = 0;
      rotation[AXIS_Z] = 0;
      translation[AXIS_X] = 0.0f;
      translation[AXIS_Z] = 0;
      scale[AXIS_X] = 1.0f;
      scale[AXIS_Y] = 1.0f;
      scale[AXIS_Z] = 1.0f;
      translation[AXIS_Y] = BANNER_Y;
      BuildTransformMatrix(matrix, rotation, translation, scale);
      ConvertMatrixToFixed(matrix, &sBannerMatrices[g_MatrixBufferIndex]);
      break;
  }
  if (sCurrentBanner < BANNER_OVER_PAR_NUMBER) {
    for (i = 0; i != sBanners[sCurrentBanner].letterCount; i++) {
      sBannerLetters[i].x =
          ((f32)sBannerLayouts[sCurrentBanner][BANNER_LAYOUT_FIRST_LETTER + i] +
           (f32)sBannerLayouts[sCurrentBanner][BANNER_LAYOUT_LEFT]) *
          letterScale;
      sBannerLetters[i].y = 0.0f;
      sBannerLetters[i].unkC = 0;
      sBannerLetters[i].scale = letterScale;
      rotation[AXIS_X] = 0;
      rotation[AXIS_Y] = 0;
      rotation[AXIS_Z] = 0.0f;
      translation[AXIS_X] = sBannerLetters[i].x;
      translation[AXIS_Y] = sBannerLetters[i].y;
      translation[AXIS_Z] = BANNER_Z;
      scale[AXIS_X] = sBannerLetters[i].scale;
      scale[AXIS_Y] = sBannerLetters[i].scale;
      scale[AXIS_Z] = sBannerLetters[i].scale;
      BuildTransformMatrix(matrix, rotation, translation, scale);
      ConvertMatrixToFixed(matrix,
                           &sBannerLetters[i].matrices[g_MatrixBufferIndex]);
    }
  } else {
    for (i = 0; i != sBannerNumberLetterCount; i++) {
      sBannerLetters[i].x =
          (i * BANNER_LETTER_SPACING -
           sBannerNumberLetterCount * (BANNER_LETTER_SPACING / 2)) *
          letterScale;
      sBannerLetters[i].y = 0.0f;
      sBannerLetters[i].unkC = 0;
      sBannerLetters[i].scale = letterScale;
      rotation[AXIS_X] = 0;
      rotation[AXIS_Y] = 0;
      rotation[AXIS_Z] = 0.0f;
      translation[AXIS_X] = sBannerLetters[i].x;
      translation[AXIS_Y] = sBannerLetters[i].y;
      translation[AXIS_Z] = BANNER_Z;
      scale[AXIS_X] = sBannerLetters[i].scale;
      scale[AXIS_Y] = sBannerLetters[i].scale;
      scale[AXIS_Z] = sBannerLetters[i].scale;
      BuildTransformMatrix(matrix, rotation, translation, scale);
      ConvertMatrixToFixed(matrix,
                           &sBannerLetters[i].matrices[g_MatrixBufferIndex]);
    }
  }
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  switch (sBanners[sCurrentBanner].animation) {
    case BANNER_ANIMATION_ZOOM:
    case BANNER_ANIMATION_ZOOM_STAY:
      gDPSetCombineLERP(gfx++, SHADE, TEXEL0, PRIMITIVE_ALPHA, TEXEL0,
                        ENVIRONMENT, 0, TEXEL0, 0, SHADE, TEXEL0,
                        PRIMITIVE_ALPHA, TEXEL0, ENVIRONMENT, 0, TEXEL0, 0);
      if (alpha == BANNER_MAX_ALPHA) {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
      } else {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
      }
      break;
    case BANNER_ANIMATION_SPIN:
    case BANNER_ANIMATION_SLIDE:
      gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
      break;
  }
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_DISABLE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, ~alpha);
  gDPSetEnvColor(gfx++, 0, 0, 0, alpha);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
  gDPSetBlendColor(gfx++, 255, 255, 255, BANNER_BLEND_ALPHA);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
  gDPLoadTLUT_pal256(
      gfx++, TEXTURE_ADDRESS(sMovieBuffer + MOVIE_IMAGE_PALETTE_OFFSET));
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  if (sCurrentBanner < BANNER_OVER_PAR_NUMBER) {
    DrawBannerLetters(&gfx);
    func_8020EEEC_ovl_00(&gfx);
  } else {
    DrawNumberBanner(&gfx);
  }
  *displayListHead = gfx;
}

#ifdef NON_MATCHING
/*
 * Flips a message in on a board in the middle of the screen, sized to fit it.
 * FLIP_MESSAGE_LEAVE flips the board back out, and FLIP_MESSAGE_RESET takes it
 * away at once.
 */
void ShowFlipMessage(s32 messageId) {
  s32 halfSize;
  s32 halfWidth;
  s32 halfHeight;

  sFlipMessageId = messageId;
  if (messageId == FLIP_MESSAGE_RESET) {
    sIsFlipMessageLeaving = FALSE;
    sFlipMessageFrame = FLIP_MESSAGE_FRAME_NONE;
    return;
  }
  if (messageId == FLIP_MESSAGE_LEAVE) {
    sIsFlipMessageLeaving = TRUE;
    return;
  }
  sIsFlipMessageLeaving = FALSE;
  sFlipMessageFrame = 0;
  halfSize = PlaceMessageGlyphGroup(FLIP_MESSAGE_GLYPH_GROUP, FONT_SIZE_LARGE,
                                    0, 0, messageId);
  halfWidth = (u32)halfSize >> 16;
  sFlipMessageHalfWidth = halfWidth;
  sFlipMessageBoardVertices[0].v.ob[0] =
      -halfWidth - FLIP_MESSAGE_BOARD_MARGIN_X;
  sFlipMessageBoardVertices[1].v.ob[0] =
      -halfWidth - FLIP_MESSAGE_BOARD_MARGIN_X;
  sFlipMessageBoardVertices[2].v.ob[0] =
      halfWidth + FLIP_MESSAGE_BOARD_MARGIN_X;
  sFlipMessageBoardVertices[3].v.ob[0] =
      halfWidth + FLIP_MESSAGE_BOARD_MARGIN_X;
  halfHeight = halfSize & 0xFFFF;
  sFlipMessageHalfHeight = halfHeight;
  sFlipMessageBoardVertices[0].v.ob[1] =
      halfHeight + FLIP_MESSAGE_BOARD_MARGIN_Y;
  sFlipMessageBoardVertices[3].v.ob[1] =
      halfHeight + FLIP_MESSAGE_BOARD_MARGIN_Y;
  sFlipMessageBoardVertices[1].v.ob[1] =
      -halfHeight - FLIP_MESSAGE_BOARD_MARGIN_Y;
  sFlipMessageBoardVertices[2].v.ob[1] =
      -halfHeight - FLIP_MESSAGE_BOARD_MARGIN_Y;
  SetTextSlot(FLIP_MESSAGE_X, FLIP_MESSAGE_Y, sFlipMessageId, FLIP_MESSAGE_FONT,
              FLIP_MESSAGE_TEXT_SLOT, TEXT_FLAG_CENTERED | TEXT_FLAG_UNK20, 127,
              255, 127, 255);
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", ShowFlipMessage);
#endif

/* Turns the flip message board a frame further in or out, then draws it and its
 * text */
void DrawFlipMessage(Gfx** displayListHead) {
  Vec3 scale;
  Vec3 translation;
  Vec3 rotation;
  Mat4 matrix;
  Mat4 hingeMatrix;
  Gfx* gfx = *displayListHead;
  f32 angle;
  f32 size;

  if (sFlipMessageFrame != FLIP_MESSAGE_FRAME_NONE) {
    if (!sIsFlipMessageLeaving) {
      if (sFlipMessageFrame < FLIP_MESSAGE_OPEN_FRAME) {
        sFlipMessageFrame++;
      }
    } else if (sFlipMessageFrame > 0) {
      sFlipMessageFrame--;
    }
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, FLIP_MESSAGE_BOARD_ALPHA);
    gDPPipeSync(gfx++);
    angle =
        DEGREES_TO_RADIANS(sFlipMessageFrame * FLIP_MESSAGE_DEGREES_PER_FRAME +
                           FLIP_MESSAGE_START_DEGREES);
    if (angle > 2 * M_PI) {
      angle = 2 * M_PI;
    }
    size = sFlipMessageFrame * FLIP_MESSAGE_GROWTH_PER_FRAME;
    if (size > 1.0f) {
      size = 1.0f;
    }
    scale[AXIS_X] = size;
    scale[AXIS_Y] = size;
    scale[AXIS_Z] = size;
    rotation[AXIS_X] = 0;
    rotation[AXIS_Y] = 0;
    rotation[AXIS_Z] = -angle;
    translation[AXIS_X] = -sFlipMessageHalfWidth;
    translation[AXIS_Y] = FLIP_MESSAGE_BOARD_Y;
    translation[AXIS_Z] = FLIP_MESSAGE_BOARD_Z;
    BuildTransformMatrix(matrix, rotation, translation, scale);
    guTranslateF(hingeMatrix, sFlipMessageHalfWidth, -FLIP_MESSAGE_BOARD_Y, 0);
    guMtxCatF(hingeMatrix, matrix, matrix);
    ConvertMatrixToFixed(matrix, &sFlipMessageMatrices[g_MatrixBufferIndex]);
    gSPMatrix(gfx++,
              OS_K0_TO_PHYSICAL(&sFlipMessageMatrices[g_MatrixBufferIndex]),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPVertex(gfx++, sFlipMessageBoardVertices, 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    if (sFlipMessageFrame == FLIP_MESSAGE_OPEN_FRAME) {
      SetUpTextDrawing(&gfx);
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 240, 255);
      DrawLargeTextSlot(&gfx, FLIP_MESSAGE_TEXT_SLOT);
    } else {
      SetUpPerspectiveTextDrawing(&gfx);
      DrawGlyphGroup(&gfx, FLIP_MESSAGE_GLYPH_GROUP);
    }
  }
  *displayListHead = gfx;
}

s32 IsMovieFinished(void) { return sCurrentMovie == MOVIE_NONE; }

void StartMovie(s32 movie) {
  EncodedAsset asset;
  AssetFile file;

  osSyncPrintf("MOVIE %d \n", movie);
  if (sIsMovieBufferAllocated != TRUE) {
    return;
  }
  sCurrentMovie = movie;
  if (movie == MOVIE_NONE) {
    return;
  }
  sLoadedBanner = BANNER_NONE;
  sCurrentBanner = BANNER_NONE;
  sMovieDrawCount = 0;
  sMovieTick = 0;
  sMovieBackBuffer = 0;
  /* The first frame is loaded here */
  sMovieFrame = 1;
  switch (movie) {
    case MOVIE_NONE:
      break;
    case MOVIE_TOAD_OUT_OF_BOUNDS:

      OpenAssetFile(ASSET_TOAD_MOVIE_PALETTE, &file);
      ReadAssetFile(sMovieBuffer, GetAssetFileSize(&file), &file);
      OpenEncodedAsset(sToadMovieFrameAssets[0], &asset);
      ReadEncodedAsset(sMovieBuffer + TOAD_FRAME_OFFSET, &asset);
      OpenEncodedAsset(sToadMovieFrameAssets[0], &asset);
      ReadEncodedAsset(sMovieBuffer + TOAD_BACK_FRAME_OFFSET, &asset);
      break;
    case MOVIE_LAKITU:
      OpenEncodedAsset(ASSET_LAKITU, &asset);
      ReadEncodedAsset(sMovieBuffer, &asset);
      OpenEncodedAsset(ASSET_LAKITU, &asset);
      ReadEncodedAsset(sMovieBuffer + LAKITU_BACK_IMAGE_OFFSET, &asset);
      sLakitu.x = LAKITU_START_X;
      sLakitu.y = LAKITU_START_Y;
      sLakitu.scale = LAKITU_START_SCALE;
      break;
    case MOVIE_MONTY_MOLE_UNK2:
    case MOVIE_MONTY_MOLE_UNK3:
    case MOVIE_MONTY_MOLE_UNK4:
    case MOVIE_MONTY_MOLE:
      OpenAssetFile(ASSET_MONTY_MOLE_MOVIE_PALETTE, &file);
      ReadAssetFile(sMovieBuffer + MOVIE_PALETTE_OFFSET,
                    GetAssetFileSize(&file), &file);
      OpenEncodedAsset(sMontyMoleMovieFrameAssets[0], &asset);
      ReadEncodedAsset(sMovieBuffer + MOVIE_FRAME_OFFSET, &asset);
      break;
    case MOVIE_GOOMBA:
      OpenAssetFile(ASSET_GOOMBA_MOVIE_PALETTE, &file);
      ReadAssetFile(sMovieBuffer + MOVIE_PALETTE_OFFSET,
                    GetAssetFileSize(&file), &file);
      OpenEncodedAsset(sGoombaMovieFrameAssets[0], &asset);
      ReadEncodedAsset(sMovieBuffer + MOVIE_FRAME_OFFSET, &asset);
      break;
    case MOVIE_DRIVE_RECORD:
      OpenEncodedAsset(ASSET_DRIVE_RECORD_TEXT, &asset);
      ReadEncodedAsset(sMovieBuffer, &asset);
      break;
    case MOVIE_LONGEST_DRIVE:
      OpenEncodedAsset(ASSET_LONGEST_DRIVE_TEXT, &asset);
      ReadEncodedAsset(sMovieBuffer, &asset);
      break;
    case MOVIE_NEAREST_PIN:
      OpenEncodedAsset(ASSET_NEAREST_PIN_TEXT, &asset);
      ReadEncodedAsset(sMovieBuffer, &asset);
      break;
    case MOVIE_ALBATROSS_PUTT:
      OpenEncodedAsset(ASSET_ALBATROSS_PUTT_TEXT, &asset);
      ReadEncodedAsset(sMovieBuffer, &asset);
      break;
    case MOVIE_EAGLE_PUTT:
      OpenEncodedAsset(ASSET_EAGLE_PUTT_TEXT, &asset);
      ReadEncodedAsset(sMovieBuffer, &asset);
      break;
    case MOVIE_BIRDIE_PUTT:
      OpenEncodedAsset(ASSET_BIRDIE_PUTT_TEXT, &asset);
      ReadEncodedAsset(sMovieBuffer, &asset);
      break;
    case MOVIE_SINK_TO_WIN:
      OpenEncodedAsset(ASSET_SINK_TO_WIN_TEXT, &asset);
      ReadEncodedAsset(sMovieBuffer, &asset);
      break;
    case MOVIE_MISS_AND_LOSE:
      OpenEncodedAsset(ASSET_MISS_AND_LOSE_TEXT, &asset);
      ReadEncodedAsset(sMovieBuffer, &asset);
      break;
    case MOVIE_SINK_TO_TIE:
      OpenEncodedAsset(ASSET_SINK_TO_TIE_TEXT, &asset);
      ReadEncodedAsset(sMovieBuffer, &asset);
      break;
    case MOVIE_READY_GO:
      OpenEncodedAsset(ASSET_READY_TEXT, &asset);
      ReadEncodedAsset(sMovieBuffer, &asset);
      OpenEncodedAsset(ASSET_GO_TEXT, &asset);
      ReadEncodedAsset(sMovieBuffer + GO_TEXT_OFFSET, &asset);
      break;
  }
}

s32 IsMoviePastCue(void) { return sMovieFrame >= MOVIE_CUE_FRAME; }

void DrawMovie(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  if (g_PauseScreen != PAUSE_SCREEN_NONE) {
    return;
  }
  if (IsGameFlagSet(GAME_FLAG_TEST_AIM_TILT_AND_MOVIES)) {
    if (g_ControllerStates[MOVIE_TEST_CONTROLLER].pressedButtons & A_BUTTON) {
      StartMovie(MOVIE_DRIVE_RECORD);
    }
    if (g_ControllerStates[MOVIE_TEST_CONTROLLER].pressedButtons & B_BUTTON) {
      StartMovie(MOVIE_READY_GO);
    }
  }
  switch (sCurrentMovie) {
    case MOVIE_NONE:
      break;
    case MOVIE_TOAD_OUT_OF_BOUNDS:
      DrawToadMovie(&gfx);
      break;
    case MOVIE_LAKITU:
      DrawLakituMovie(&gfx);
      break;
    case MOVIE_MONTY_MOLE:
      DrawMontyMoleMovie(&gfx);
      break;
    case MOVIE_GOOMBA:
      DrawGoombaMovie(&gfx);
      break;
    case MOVIE_LONGEST_DRIVE:
    case MOVIE_NEAREST_PIN:
      DrawContestWinnerMovie(&gfx);
      break;
    case MOVIE_DRIVE_RECORD:
      DrawDriveRecord(&gfx);
      break;
    case MOVIE_ALBATROSS_PUTT:
    case MOVIE_EAGLE_PUTT:
    case MOVIE_BIRDIE_PUTT:
    case MOVIE_SINK_TO_WIN:
    case MOVIE_SINK_TO_TIE:
    case MOVIE_MISS_AND_LOSE:
      DrawPuttCaption(&gfx);
      break;
    case MOVIE_READY_GO:
      DrawReadyGoMovie(&gfx);
      break;
  }
  if (sMovieDrawCount < MOVIE_DRAW_COUNT_MAX) {
    sMovieDrawCount++;
  }
  *displayListHead = gfx;
}

/* Where the contest winner's palette, face and plaque are kept, after the movie
 * image */
#define WINNER_PALETTE_OFFSET 0x2788
#define WINNER_FACE_OFFSET 0x2988
#define WINNER_PLAQUE_OFFSET 0x2D88
/* A wooden plaque behind the winner's face */
#define WINNER_PLAQUE_ASSET 0x832
#define WINNER_FACE_SIZE 32
#define WINNER_PLAQUE_SIZE 48
/* The plaque shows this much more than the face on each side */
#define WINNER_PLAQUE_MARGIN 8
#define WINNER_CENTRE_X 160
#define WINNER_CENTRE_Y 128
/* The face and plaque grow to this many steps of WINNER_GROWTH_STEP pixels,
 * then shrink back */
#define WINNER_MAX_GROWTH 4
#define WINNER_GROWTH_STEP 5
#define WINNER_SHRINK_FRAME 31
#define CONTEST_SLIDE_DRAW 20
#define CONTEST_PAN_DRAW_END 25
#define CONTEST_PAN_START 223
#define CONTEST_PAN_STEP 20
#define CONTEST_SOUND_REVERB 20
#define CONTEST_MEET_DRAW 25
#define CONTEST_FLASH_END_DRAW 37
#define CONTEST_SLIDE_OUT_DRAW 58
#define CONTEST_END_DRAW 70
#define CONTEST_DRAW_COUNT 90
#define CONTEST_START_SLIDE 600
#define CONTEST_SLIDE_PER_FRAME 24
#define CONTEST_COPY_ALPHA 127
#define CONTEST_FLASH_DEPTH 127.0f
#define CONTEST_TURN_PER_FRAME (M_PI / 12)
#define CONTEST_IMAGE_CENTRE_X 160
#define CONTEST_IMAGE_CENTRE_Y 80
#define CONTEST_IMAGE_STRIPS_ABOVE_CENTRE 4
/* The winner cheers as the caption meets */
#define CONTEST_CHEER_DRAW 26
#define CHARACTER_SOUND_LONGEST_DRIVE 16
#define CHARACTER_SOUND_NEAREST_PIN 18
#define CHARACTER_SOUND_VOLUME 127
#define CHARACTER_SOUND_PAN 127
/* The face appears and vanishes over these frames, counted from
 * CONTEST_MEET_DRAW */
#define WINNER_FIRST_FRAME 1
#define WINNER_LAST_FRAME 32
#define SOUND_WINNER_APPEAR 0x49
#define SOUND_WINNER_VANISH 0x4A
#define WINNER_SOUND_VOLUME 63
#define WINNER_SOUND_REVERB 30
#define WINNER_SOUND_PAN 127
/* The face's later frames, which g_CharacterImageAssets lists after its first
 */
#define WINNER_BLINK_FRAME 8
#define WINNER_FACE_FRAME_COUNT 4

#ifdef NON_MATCHING
/* The caption of a longest drive or nearest pin contest, then its winner's face
 */
void DrawContestWinnerMovie(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  EncodedAsset asset;
  AssetFile file;
  u32 width;
  s32 height;
  s32 stripCount;
  s32 halfWidth;
  s32 stripHeight;
  s32 slide;
  s32 centreX;
  s32 bottom;
  s32 i;
  s32 frame;
  s32 growth;
  s32 plaqueHalfSize;
  s32 step;
  f32 alpha;

  width = (sMovieBuffer[MOVIE_IMAGE_WIDTH_OFFSET] << 8) |
          sMovieBuffer[MOVIE_IMAGE_WIDTH_OFFSET + 1];
  stripHeight = MOVIE_IMAGE_STRIP_HEIGHT;
  height = (sMovieBuffer[MOVIE_IMAGE_HEIGHT_OFFSET] << 8) |
           sMovieBuffer[MOVIE_IMAGE_HEIGHT_OFFSET + 1];
  stripCount = height / stripHeight;
  slide = 0;
  gDPPipeSync(gfx++);
  gDPSetCombineLERP(gfx++, TEXEL0, ENVIRONMENT, PRIMITIVE_ALPHA, ENVIRONMENT, 0,
                    0, 0, TEXEL0, TEXEL0, ENVIRONMENT, PRIMITIVE_ALPHA,
                    ENVIRONMENT, 0, 0, 0, TEXEL0);
  halfWidth = width >> 1;
  if (sMovieDrawCount == CONTEST_SLIDE_DRAW) {
    PlaySoundWithEffects(SOUND_SHRINKING_IMAGE, SHRINKING_IMAGE_SOUND_CHANNEL,
                         SHRINKING_IMAGE_SOUND_VOLUME, CONTEST_SOUND_REVERB,
                         CONTEST_PAN_START);
  }
  if (sMovieDrawCount == CONTEST_CHEER_DRAW) {
    if (sCurrentMovie == MOVIE_LONGEST_DRIVE) {
      PlayCharacterSound((s8)g_Players[GetContestWinner()].characterIndex,
                         GetContestWinner(), CHARACTER_SOUND_LONGEST_DRIVE,
                         CHARACTER_SOUND_VOLUME, CHARACTER_SOUND_PAN);
    } else {
      PlayCharacterSound((s8)g_Players[GetContestWinner()].characterIndex,
                         GetContestWinner(), CHARACTER_SOUND_NEAREST_PIN,
                         CHARACTER_SOUND_VOLUME, CHARACTER_SOUND_PAN);
    }
  }
  if (sMovieDrawCount > CONTEST_SLIDE_DRAW &&
      sMovieDrawCount < CONTEST_PAN_DRAW_END) {
    SetSoundPan(SHRINKING_IMAGE_SOUND_CHANNEL,
                CONTEST_PAN_START -
                    (sMovieDrawCount - CONTEST_SLIDE_DRAW) * CONTEST_PAN_STEP);
  }
  if (sMovieDrawCount < CONTEST_MEET_DRAW) {
    slide = CONTEST_START_SLIDE - sMovieDrawCount * CONTEST_SLIDE_PER_FRAME;
    if (slide < 0) {
      slide = 0;
    }
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(gfx++, 255, 255, 255, CONTEST_COPY_ALPHA);
    if (sMovieDrawCount == 0) {
      OpenAssetFile(g_CharacterPaletteAssets[(s8)g_Players[GetContestWinner()]
                                                 .characterIndex] +
                        (s8)g_Players[GetContestWinner()].paletteIndex,
                    &file);
      ReadAssetFile(sMovieBuffer + WINNER_PALETTE_OFFSET,
                    GetAssetFileSize(&file), &file);
      OpenEncodedAsset(g_CharacterImageAssets[(s8)g_Players[GetContestWinner()]
                                                  .characterIndex],
                       &asset);
      ReadEncodedAsset(sMovieBuffer + WINNER_FACE_OFFSET, &asset);
      OpenEncodedAsset(WINNER_PLAQUE_ASSET, &asset);
      ReadEncodedAsset(sMovieBuffer + WINNER_PLAQUE_OFFSET, &asset);
    }
  } else if (sMovieDrawCount < CONTEST_FLASH_END_DRAW) {
    alpha = 255.0f - sinf((sMovieDrawCount - CONTEST_MEET_DRAW) *
                          CONTEST_TURN_PER_FRAME) *
                         CONTEST_FLASH_DEPTH;
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
    gDPSetEnvColor(gfx++, 255, 255, 255, 255);
  } else if (sMovieDrawCount < CONTEST_SLIDE_OUT_DRAW) {
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(gfx++, 255, 255, 255, 255);
  } else if (sMovieDrawCount < CONTEST_END_DRAW) {
    slide =
        (CONTEST_SLIDE_OUT_DRAW - sMovieDrawCount) * CONTEST_SLIDE_PER_FRAME;
  } else {
    sCurrentMovie = MOVIE_NONE;
    halfWidth = 0;
  }
  gSPDisplayList(gfx++, sMovieImageSetupDisplayList);
  gDPLoadTLUT_pal256(
      gfx++, TEXTURE_ADDRESS(sMovieBuffer + MOVIE_IMAGE_PALETTE_OFFSET));
  if (sMovieDrawCount < CONTEST_DRAW_COUNT && halfWidth != 0) {
    bottom = CONTEST_IMAGE_CENTRE_Y;
    for (i = 0; i != stripCount; i++) {
      LoadTextureBlock(&gfx,
                       sMovieBuffer + (i * width * MOVIE_IMAGE_STRIP_HEIGHT +
                                       MOVIE_IMAGE_PIXELS_OFFSET),
                       G_IM_FMT_CI, G_IM_SIZ_8b, width,
                       MOVIE_IMAGE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      centreX = CONTEST_IMAGE_CENTRE_X + slide;
      DrawTextureRect(
          &gfx, (centreX - halfWidth) << 2,
          (bottom - stripHeight * CONTEST_IMAGE_STRIPS_ABOVE_CENTRE) << 2,
          (centreX + halfWidth) << 2,
          (bottom - stripHeight * CONTEST_IMAGE_STRIPS_ABOVE_CENTRE +
           MOVIE_IMAGE_STRIP_HEIGHT)
              << 2,
          G_TX_RENDERTILE, 0, 0, (s32)((width >> 1) << 10) / halfWidth,
          (MOVIE_IMAGE_STRIP_HEIGHT << 10) / stripHeight);
      bottom += MOVIE_IMAGE_STRIP_HEIGHT;
    }
  }
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  if (sMovieDrawCount > CONTEST_MEET_DRAW &&
      sMovieDrawCount <= CONTEST_MEET_DRAW + WINNER_LAST_FRAME) {
    frame = sMovieDrawCount - CONTEST_MEET_DRAW;
    if (frame == WINNER_BLINK_FRAME) {
      OpenEncodedAsset(g_CharacterImageAssets[(s8)g_Players[GetContestWinner()]
                                                  .characterIndex] +
                           1,
                       &asset);
      ReadEncodedAsset(sMovieBuffer + WINNER_FACE_OFFSET, &asset);
    } else if (frame == WINNER_BLINK_FRAME + 2) {
      OpenEncodedAsset(g_CharacterImageAssets[(s8)g_Players[GetContestWinner()]
                                                  .characterIndex] +
                           2,
                       &asset);
      ReadEncodedAsset(sMovieBuffer + WINNER_FACE_OFFSET, &asset);
    } else if (frame == WINNER_BLINK_FRAME + 4) {
      OpenEncodedAsset(g_CharacterImageAssets[(s8)g_Players[GetContestWinner()]
                                                  .characterIndex] +
                           3,
                       &asset);
      ReadEncodedAsset(sMovieBuffer + WINNER_FACE_OFFSET, &asset);
    } else if (frame == WINNER_BLINK_FRAME + 6) {
      OpenEncodedAsset(g_CharacterImageAssets[(s8)g_Players[GetContestWinner()]
                                                  .characterIndex] +
                           4,
                       &asset);
      ReadEncodedAsset(sMovieBuffer + WINNER_FACE_OFFSET, &asset);
    }
    if (frame == WINNER_FIRST_FRAME) {
      PlaySoundWithEffects(SOUND_WINNER_APPEAR, g_NextSoundChannel,
                           WINNER_SOUND_VOLUME, WINNER_SOUND_REVERB,
                           WINNER_SOUND_PAN);
    }
    if (frame == WINNER_SHRINK_FRAME) {
      PlaySoundWithEffects(SOUND_WINNER_VANISH, g_NextSoundChannel,
                           WINNER_SOUND_VOLUME, WINNER_SOUND_REVERB,
                           WINNER_SOUND_PAN);
    }
    if (frame >= WINNER_SHRINK_FRAME) {
      frame = WINNER_SHRINK_FRAME + 3 - frame;
      if (frame <= 0) {
        frame = 1;
      }
    } else if (frame > WINNER_MAX_GROWTH) {
      frame = WINNER_MAX_GROWTH;
    }
    growth = frame * WINNER_GROWTH_STEP;
    LoadSmallPalette(&gfx, 0,
                     sMovieBuffer + WINNER_PLAQUE_OFFSET + IMAGE_HEADER_SIZE);
    LoadNibbleTextureBlock(
        &gfx, sMovieBuffer + WINNER_PLAQUE_OFFSET + PALETTED_TEXELS_OFFSET,
        G_IM_FMT_CI, WINNER_PLAQUE_SIZE, WINNER_PLAQUE_SIZE, 0, 0, 0, 0, 0, 0,
        0);
    plaqueHalfSize = growth + WINNER_PLAQUE_MARGIN;
    step = ((WINNER_PLAQUE_SIZE / 2) << 10) / plaqueHalfSize;
    DrawTextureRect(&gfx, (WINNER_CENTRE_X - plaqueHalfSize) << 2,
                    (WINNER_CENTRE_Y - plaqueHalfSize) << 2,
                    (WINNER_CENTRE_X + plaqueHalfSize) << 2,
                    (WINNER_CENTRE_Y + plaqueHalfSize) << 2, G_TX_RENDERTILE, 0,
                    0, step, step);
    LoadLargePalette(&gfx, sMovieBuffer + WINNER_PALETTE_OFFSET);
    LoadTextureBlock(&gfx, sMovieBuffer + WINNER_FACE_OFFSET, G_IM_FMT_CI,
                     G_IM_SIZ_8b, WINNER_FACE_SIZE, WINNER_FACE_SIZE, 0, 0, 0,
                     0, 0, 0, 0);
    step = ((WINNER_FACE_SIZE / 2) << 10) / growth;
    DrawTextureRect(
        &gfx, (WINNER_CENTRE_X - growth) << 2, (WINNER_CENTRE_Y - growth) << 2,
        (WINNER_CENTRE_X + growth) << 2, (WINNER_CENTRE_Y + growth) << 2,
        G_TX_RENDERTILE, 0, 0, step, step);
  }
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", DrawContestWinnerMovie);
#endif

/*
 * The caption and a copy of it slide in from either side until they meet
 * at the centre. The caption flashes, then grows and fades away.
 */
#define CAPTION_MEET_DRAW 25
#define CAPTION_FLASH_END_DRAW 37
#define CAPTION_GROW_DRAW 44
#define CAPTION_GROW_END_DRAW 56
#define CAPTION_DRAW_COUNT 90
#define CAPTION_TURN_PER_FRAME (M_PI / 12)
#define CAPTION_START_SLIDE 600
#define CAPTION_SLIDE_PER_FRAME 24
#define CAPTION_MAX_GROWTH 48
#define CAPTION_COPY_ALPHA 127
#define CAPTION_COPY_TOP      \
  (SHRINKING_IMAGE_CENTRE_Y - \
   MOVIE_IMAGE_STRIP_HEIGHT * SHRINKING_IMAGE_STRIPS_ABOVE_CENTRE)
#define CAPTION_FLASH_DEPTH 127.0f
#define SOUND_CAPTION_MEET 0x4B
#define CAPTION_SOUND_CHANNEL 13
#define CAPTION_SOUND_VOLUME 63

/* The record distance shows under the caption once it has met at the centre */
#define DRIVE_RECORD_DISTANCE_DRAW 26
#define DRIVE_RECORD_DISTANCE_END_DRAW 45
#define DRIVE_RECORD_DISTANCE_X 136
#define DRIVE_RECORD_DISTANCE_Y 116
#define DRIVE_RECORD_DISTANCE_ENV_COLOR 0xFF600000
#define DRIVE_RECORD_DISTANCE_PRIM_COLOR 0xFFFF0000

#ifdef NON_MATCHING
void DrawDriveRecord(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  u32 width;
  s32 height;
  s32 stripCount;
  s32 halfWidth;
  s32 stripHeight;
  s32 slide;
  s32 centreX;
  s32 bottom;
  s32 copyTop;
  s32 i;
  f32 angle;
  f32 fade;
  f32 alpha;

  width = (sMovieBuffer[MOVIE_IMAGE_WIDTH_OFFSET] << 8) |
          sMovieBuffer[MOVIE_IMAGE_WIDTH_OFFSET + 1];
  height = (sMovieBuffer[MOVIE_IMAGE_HEIGHT_OFFSET] << 8) |
           sMovieBuffer[MOVIE_IMAGE_HEIGHT_OFFSET + 1];
  if (sMovieDrawCount == CAPTION_MEET_DRAW + 1) {
    PlaySoundWithEffects(SOUND_SHRINKING_IMAGE, CAPTION_SOUND_CHANNEL,
                         CAPTION_SOUND_VOLUME, 0, MOVIE_SOUND_PAN);
  }
  stripHeight = MOVIE_IMAGE_STRIP_HEIGHT;
  halfWidth = width >> 1;
  gDPPipeSync(gfx++);
  gDPSetCombineLERP(gfx++, TEXEL0, ENVIRONMENT, PRIMITIVE_ALPHA, ENVIRONMENT,
                    ENVIRONMENT, 0, TEXEL0, 0, TEXEL0, ENVIRONMENT,
                    PRIMITIVE_ALPHA, ENVIRONMENT, ENVIRONMENT, 0, TEXEL0, 0);
  stripCount = height / stripHeight;
  if (sMovieDrawCount < CAPTION_MEET_DRAW) {
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(gfx++, 255, 255, 255, CAPTION_COPY_ALPHA);
  } else if (sMovieDrawCount < CAPTION_FLASH_END_DRAW) {
    alpha = 255.0f - sinf((sMovieDrawCount - CAPTION_MEET_DRAW) *
                          CAPTION_TURN_PER_FRAME) *
                         CAPTION_FLASH_DEPTH;
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
    gDPSetEnvColor(gfx++, 255, 255, 255, 255);
  } else if (sMovieDrawCount < CAPTION_GROW_DRAW) {
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(gfx++, 255, 255, 255, 255);
  } else if (sMovieDrawCount < CAPTION_GROW_END_DRAW) {
    angle = (sMovieDrawCount - CAPTION_GROW_DRAW) * CAPTION_TURN_PER_FRAME;
    fade = sinf(angle) * CAPTION_FLASH_DEPTH;
    halfWidth = cosf(angle) * halfWidth;
    stripHeight =
        (1.0f - cosf(angle)) * CAPTION_MAX_GROWTH + MOVIE_IMAGE_STRIP_HEIGHT;
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255.0f - fade);
    gDPSetEnvColor(gfx++, 255, 255, 255, 255.0f - fade);
  } else {
    sCurrentMovie = MOVIE_NONE;
    halfWidth = 0;
  }
  gSPDisplayList(gfx++, sMovieImageSetupDisplayList);
  gDPLoadTLUT_pal256(
      gfx++, TEXTURE_ADDRESS(sMovieBuffer + MOVIE_IMAGE_PALETTE_OFFSET));
  if (sMovieDrawCount < CAPTION_DRAW_COUNT && halfWidth > 0) {
    bottom = SHRINKING_IMAGE_CENTRE_Y;
    for (i = 0; i != stripCount; i++) {
      slide = CAPTION_START_SLIDE - sMovieDrawCount * CAPTION_SLIDE_PER_FRAME;
      if (slide < 0) {
        slide = 0;
      }
      LoadTextureBlock(&gfx,
                       sMovieBuffer + (i * width * MOVIE_IMAGE_STRIP_HEIGHT +
                                       MOVIE_IMAGE_PIXELS_OFFSET),
                       G_IM_FMT_CI, G_IM_SIZ_8b, width,
                       MOVIE_IMAGE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      centreX = SHRINKING_IMAGE_CENTRE_X + slide;
      DrawTextureRect(
          &gfx, (centreX - halfWidth) << 2,
          (bottom - stripHeight * SHRINKING_IMAGE_STRIPS_ABOVE_CENTRE) << 2,
          (centreX + halfWidth) << 2,
          (bottom - stripHeight * SHRINKING_IMAGE_STRIPS_ABOVE_CENTRE +
           stripHeight)
              << 2,
          G_TX_RENDERTILE, 0, 0, (s32)((width >> 1) << 10) / halfWidth,
          (MOVIE_IMAGE_STRIP_HEIGHT << 10) / stripHeight);
      if (sMovieDrawCount < CAPTION_MEET_DRAW) {
        centreX = SHRINKING_IMAGE_CENTRE_X - slide;
        copyTop = CAPTION_COPY_TOP + i * MOVIE_IMAGE_STRIP_HEIGHT;
        DrawTextureRect(&gfx, (centreX - (s32)(width >> 1)) << 2, copyTop << 2,
                        (centreX + (s32)(width >> 1)) << 2,
                        (copyTop + MOVIE_IMAGE_STRIP_HEIGHT) << 2,
                        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      }
      bottom += stripHeight;
    }
  }
  if (sMovieDrawCount >= DRIVE_RECORD_DISTANCE_DRAW &&
      sMovieDrawCount < DRIVE_RECORD_DISTANCE_END_DRAW) {
    SetUpGradientTextDrawing(&gfx, 0);
    sprintf(g_PrintBuffer, "%5.2f%c",
            g_Players[0].lastDriveDistance * YARDS_PER_UNIT, YARDS_GLYPH);
    DrawLargeTextWithShadow(
        &gfx, DRIVE_RECORD_DISTANCE_X, DRIVE_RECORD_DISTANCE_Y, g_PrintBuffer,
        DRIVE_RECORD_DISTANCE_ENV_COLOR, DRIVE_RECORD_DISTANCE_PRIM_COLOR,
        TEXT_FLAG_FIXED_WIDTH);
  }
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", DrawDriveRecord);
#endif

#ifdef NON_MATCHING
void DrawPuttCaption(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  u32 width;
  s32 stripCount;
  s32 halfWidth;
  s32 stripHeight;
  s32 slide;
  s32 centreX;
  s32 bottom;
  s32 copyTop;
  s32 i;
  f32 angle;
  f32 fade;
  f32 alpha;

  stripHeight = MOVIE_IMAGE_STRIP_HEIGHT;
  width = (sMovieBuffer[MOVIE_IMAGE_WIDTH_OFFSET] << 8) |
          sMovieBuffer[MOVIE_IMAGE_WIDTH_OFFSET + 1];
  stripCount = ((sMovieBuffer[MOVIE_IMAGE_HEIGHT_OFFSET] << 8) |
                sMovieBuffer[MOVIE_IMAGE_HEIGHT_OFFSET + 1]) /
               stripHeight;
  halfWidth = width >> 1;
  gDPPipeSync(gfx++);
  gDPSetCombineLERP(gfx++, TEXEL0, ENVIRONMENT, PRIMITIVE_ALPHA, ENVIRONMENT,
                    ENVIRONMENT, 0, TEXEL0, 0, TEXEL0, ENVIRONMENT,
                    PRIMITIVE_ALPHA, ENVIRONMENT, ENVIRONMENT, 0, TEXEL0, 0);
  if (sMovieDrawCount == CAPTION_MEET_DRAW + 1) {
    PlaySoundWithEffects(SOUND_CAPTION_MEET, CAPTION_SOUND_CHANNEL,
                         CAPTION_SOUND_VOLUME, 0, MOVIE_SOUND_PAN);
  }
  if (sMovieDrawCount < CAPTION_MEET_DRAW) {
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(gfx++, 255, 255, 255, CAPTION_COPY_ALPHA);
  } else if (sMovieDrawCount < CAPTION_FLASH_END_DRAW) {
    alpha = 255.0f - sinf((sMovieDrawCount - CAPTION_MEET_DRAW) *
                          CAPTION_TURN_PER_FRAME) *
                         CAPTION_FLASH_DEPTH;
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
    gDPSetEnvColor(gfx++, 255, 255, 255, 255);
  } else if (sMovieDrawCount < CAPTION_GROW_DRAW) {
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(gfx++, 255, 255, 255, 255);
  } else if (sMovieDrawCount < CAPTION_GROW_END_DRAW) {
    angle = (sMovieDrawCount - CAPTION_GROW_DRAW) * CAPTION_TURN_PER_FRAME;
    fade = sinf(angle) * CAPTION_FLASH_DEPTH;
    halfWidth = cosf(angle) * halfWidth;
    stripHeight =
        (1.0f - cosf(angle)) * CAPTION_MAX_GROWTH + MOVIE_IMAGE_STRIP_HEIGHT;
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255.0f - fade);
    gDPSetEnvColor(gfx++, 255, 255, 255, 255.0f - fade);
  } else {
    sCurrentMovie = MOVIE_NONE;
    halfWidth = 0;
  }
  gSPDisplayList(gfx++, sMovieImageSetupDisplayList);
  gDPLoadTLUT_pal256(
      gfx++, TEXTURE_ADDRESS(sMovieBuffer + MOVIE_IMAGE_PALETTE_OFFSET));
  if (sMovieDrawCount < CAPTION_DRAW_COUNT && halfWidth > 0) {
    bottom = SHRINKING_IMAGE_CENTRE_Y;
    for (i = 0; i != stripCount; i++) {
      slide = CAPTION_START_SLIDE - sMovieDrawCount * CAPTION_SLIDE_PER_FRAME;
      if (slide < 0) {
        slide = 0;
      }
      LoadTextureBlock(&gfx,
                       sMovieBuffer + (i * width * MOVIE_IMAGE_STRIP_HEIGHT +
                                       MOVIE_IMAGE_PIXELS_OFFSET),
                       G_IM_FMT_CI, G_IM_SIZ_8b, width,
                       MOVIE_IMAGE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      centreX = SHRINKING_IMAGE_CENTRE_X + slide;
      DrawTextureRect(
          &gfx, (centreX - halfWidth) << 2,
          (bottom - stripHeight * SHRINKING_IMAGE_STRIPS_ABOVE_CENTRE) << 2,
          (centreX + halfWidth) << 2,
          (bottom - stripHeight * SHRINKING_IMAGE_STRIPS_ABOVE_CENTRE +
           stripHeight)
              << 2,
          G_TX_RENDERTILE, 0, 0, (s32)((width >> 1) << 10) / halfWidth,
          (MOVIE_IMAGE_STRIP_HEIGHT << 10) / stripHeight);
      if (sMovieDrawCount < CAPTION_MEET_DRAW) {
        centreX = SHRINKING_IMAGE_CENTRE_X - slide;
        copyTop = CAPTION_COPY_TOP + i * MOVIE_IMAGE_STRIP_HEIGHT;
        DrawTextureRect(&gfx, (centreX - (s32)(width >> 1)) << 2, copyTop << 2,
                        (centreX + (s32)(width >> 1)) << 2,
                        (copyTop + MOVIE_IMAGE_STRIP_HEIGHT) << 2,
                        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      }
      bottom += stripHeight;
    }
  }
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", DrawPuttCaption);
#endif

/*
 * "Ready" flips over to face the camera and away again, then "Go" flips in,
 * grows and fades. Each word is a textured quad, drawn in strips whose
 * texture coordinates are patched into the shared vertices.
 */
#define SOUND_READY 0x49
#define SOUND_GO 0x4A
#define READY_GO_SOUND_VOLUME 63
#define READY_GO_SOUND_REVERB 30
#define READY_SOUND_DRAW 1
#define READY_FLIP_DRAW 6
#define READY_FACING_DRAW 18
#define GO_DRAW 24
#define GO_GROW_DRAW 30
#define READY_GO_END_DRAW 42
#define READY_GO_TURN_PER_FRAME (M_PI / 12)
#define GO_GROWTH_PER_FRAME 2.0f
#define GO_FADE_PER_FRAME 32
#define READY_GO_DISTANCE -300.0f
#define READY_GO_VERTEX_COUNT 12
#define READY_GO_STRIP_COUNT 5
#define READY_WIDTH 160
#define READY_STRIP_HEIGHT 12
#define GO_WIDTH 144
#define GO_STRIP_HEIGHT 14
#define READY_GO_LAST_STRIP_HEIGHT 8
/* Texture coordinates are in 10.5 fixed point */
#define TEXEL_COORD(texels) ((texels) << 5)

void DrawReadyGoMovie(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  Vec3 rotation;
  Vec3 translation;
  Vec3 scale;
  Mat4 matrix;
  s32 alpha;
  s32 stripHeight;
  s32 i;

  sMovieBackBuffer ^= 1;
  alpha = 255;
  if (sMovieDrawCount == READY_SOUND_DRAW) {
    PlaySoundWithEffects(SOUND_READY, g_NextSoundChannel, READY_GO_SOUND_VOLUME,
                         READY_GO_SOUND_REVERB, MOVIE_SOUND_PAN);
  }
  if (sMovieDrawCount == GO_GROW_DRAW) {
    PlaySoundWithEffects(SOUND_GO, g_NextSoundChannel, READY_GO_SOUND_VOLUME,
                         READY_GO_SOUND_REVERB, MOVIE_SOUND_PAN);
  }
  scale[0] = 1.0f;
  scale[1] = 1.0f;
  scale[2] = 1.0f;
  if (sMovieDrawCount < READY_FLIP_DRAW) {
    rotation[0] = M_PI;
    rotation[1] = M_PI / 2 - (f32)sMovieDrawCount * READY_GO_TURN_PER_FRAME;
    rotation[2] = 0.0f;
  } else if (sMovieDrawCount < READY_FACING_DRAW) {
    rotation[0] = M_PI - (f32)(sMovieDrawCount - READY_FLIP_DRAW) *
                             READY_GO_TURN_PER_FRAME;
    rotation[1] = 0.0f;
    rotation[2] = 0.0f;
  } else if (sMovieDrawCount < GO_DRAW) {
    rotation[0] = M_PI - (f32)(sMovieDrawCount - READY_FLIP_DRAW) *
                             READY_GO_TURN_PER_FRAME;
    rotation[1] = 0.0f;
    rotation[2] = 0.0f;
  } else if (sMovieDrawCount < GO_GROW_DRAW) {
    rotation[0] = M_PI - (f32)(sMovieDrawCount - READY_FACING_DRAW) *
                             READY_GO_TURN_PER_FRAME;
    rotation[1] = 0.0f;
    rotation[2] = 0.0f;
  } else if (sMovieDrawCount < READY_GO_END_DRAW) {
    alpha = 255 - (sMovieDrawCount - GO_GROW_DRAW) * GO_FADE_PER_FRAME;
    if (alpha < 0) {
      alpha = 0;
    }
    rotation[0] = rotation[1] = rotation[2] = 0.0f;
    scale[0] = scale[1] = scale[2] =
        GO_GROWTH_PER_FRAME * (sMovieDrawCount - GO_GROW_DRAW) + 1.0f;
  } else {
    sCurrentMovie = MOVIE_NONE;
    return;
  }
  translation[0] = 0.0f;
  translation[1] = 0.0f;
  translation[2] = READY_GO_DISTANCE;
  BuildTransformMatrix(matrix, rotation, translation, scale);
  ConvertMatrixToFixed(matrix, &sReadyGoMatrices[sMovieBackBuffer]);
  gDPPipeSync(gfx++);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
  gDPSetEnvColor(gfx++, 255, 255, 255, alpha);
  gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, ENVIRONMENT, 0, TEXEL0, 0, 0, 0, 0,
                    TEXEL0, ENVIRONMENT, 0, TEXEL0, 0);
  gSPDisplayList(gfx++, sMovieImageSetupDisplayList);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
  if (alpha == 255) {
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  } else {
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
  }
  if (sMovieDrawCount < GO_DRAW) {
    gDPLoadTLUT_pal256(
        gfx++, TEXTURE_ADDRESS(sMovieBuffer + MOVIE_IMAGE_PALETTE_OFFSET));
  } else {
    gDPLoadTLUT_pal256(gfx++, TEXTURE_ADDRESS(sMovieBuffer + GO_TEXT_OFFSET +
                                              MOVIE_IMAGE_PALETTE_OFFSET));
  }
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sReadyGoMatrices[sMovieBackBuffer]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
  gSPVertex(gfx++, sReadyGoVertices, READY_GO_VERTEX_COUNT, 0);
  for (i = 0; i != READY_GO_STRIP_COUNT; i++) {
    if (sMovieDrawCount < GO_DRAW) {
      stripHeight = (i != READY_GO_STRIP_COUNT - 1)
                        ? READY_STRIP_HEIGHT
                        : READY_GO_LAST_STRIP_HEIGHT;
      gDPLoadTextureTile(
          gfx++,
          TEXTURE_ADDRESS(sMovieBuffer +
                          (MOVIE_IMAGE_PIXELS_OFFSET +
                           i * READY_WIDTH * READY_STRIP_HEIGHT)),
          G_IM_FMT_CI, G_IM_SIZ_8b, READY_WIDTH, stripHeight, 0, 0,
          READY_WIDTH - 1, stripHeight - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_MIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gSPModifyVertex(gfx++, i * 2, G_MWO_POINT_ST, 0);
      gSPModifyVertex(gfx++, i * 2 + 1, G_MWO_POINT_ST,
                      (TEXEL_COORD(READY_WIDTH) - 1) << 16);
      gSPModifyVertex(gfx++, i * 2 + 2, G_MWO_POINT_ST,
                      TEXEL_COORD(stripHeight));
      gSPModifyVertex(
          gfx++, i * 2 + 3, G_MWO_POINT_ST,
          ((TEXEL_COORD(READY_WIDTH) - 1) << 16) + TEXEL_COORD(stripHeight));
    } else {
      stripHeight = (i != READY_GO_STRIP_COUNT - 1)
                        ? GO_STRIP_HEIGHT
                        : READY_GO_LAST_STRIP_HEIGHT;
      gDPLoadTextureTile(
          gfx++,
          TEXTURE_ADDRESS(sMovieBuffer +
                          (GO_TEXT_OFFSET + MOVIE_IMAGE_PIXELS_OFFSET +
                           i * GO_WIDTH * GO_STRIP_HEIGHT)),
          G_IM_FMT_CI, G_IM_SIZ_8b, GO_WIDTH, stripHeight, 0, 0, GO_WIDTH - 1,
          stripHeight - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_MIRROR,
          G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gSPModifyVertex(gfx++, i * 2, G_MWO_POINT_ST, 0);
      gSPModifyVertex(gfx++, i * 2 + 1, G_MWO_POINT_ST,
                      (TEXEL_COORD(GO_WIDTH) - 1) << 16);
      gSPModifyVertex(gfx++, i * 2 + 2, G_MWO_POINT_ST,
                      TEXEL_COORD(stripHeight));
      gSPModifyVertex(
          gfx++, i * 2 + 3, G_MWO_POINT_ST,
          ((TEXEL_COORD(GO_WIDTH) - 1) << 16) + TEXEL_COORD(stripHeight));
    }
    gSP2Triangles(gfx++, i * 2, i * 2 + 2, i * 2 + 3, 0, i * 2, i * 2 + 3,
                  i * 2 + 1, 0);
  }
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

#ifdef NON_MATCHING
/* Unused: draws the movie image in sMovieBuffer, shrinking it from double size
 * to its own size */
void DrawShrinkingMovieImage(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 width;
  s32 stripCount;
  f32 angle;
  s32 halfWidth;
  s32 stripHeight;
  s32 i;
  s32 offset;
  s32 bottom;
  s32 top;

  stripHeight = MOVIE_IMAGE_STRIP_HEIGHT;
  width = (sMovieBuffer[MOVIE_IMAGE_WIDTH_OFFSET] << 8) |
          sMovieBuffer[MOVIE_IMAGE_WIDTH_OFFSET + 1];
  stripCount = ((sMovieBuffer[MOVIE_IMAGE_HEIGHT_OFFSET] << 8) |
                sMovieBuffer[MOVIE_IMAGE_HEIGHT_OFFSET + 1]) /
               stripHeight;
  if (sMovieDrawCount == SHRINKING_IMAGE_SOUND_DRAW) {
    PlaySoundWithEffects(SOUND_SHRINKING_IMAGE, SHRINKING_IMAGE_SOUND_CHANNEL,
                         SHRINKING_IMAGE_SOUND_VOLUME, 0, MOVIE_SOUND_PAN);
  }
  gSPDisplayList(gfx++, sMovieImageSetupDisplayList);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
  gDPSetEnvColor(gfx++, 255, 255, 255, 255);
  gDPSetCombineLERP(gfx++, TEXEL0, ENVIRONMENT, PRIMITIVE_ALPHA, ENVIRONMENT,
                    ENVIRONMENT, 0, TEXEL0, 0, TEXEL0, ENVIRONMENT,
                    PRIMITIVE_ALPHA, ENVIRONMENT, ENVIRONMENT, 0, TEXEL0, 0);
  gDPLoadTLUT_pal256(
      gfx++, TEXTURE_ADDRESS(sMovieBuffer + MOVIE_IMAGE_PALETTE_OFFSET));
  if (sMovieDrawCount < SHRINKING_IMAGE_DRAW_COUNT) {
    angle = (f32)sMovieDrawCount * SHRINKING_IMAGE_TURN_PER_FRAME;
    if (angle > M_PI / 2) {
      angle = M_PI / 2;
    }
    halfWidth = (cosf(angle) + 1.0f) * width * 0.5f;
    stripHeight = (cosf(angle) + 1.0f) * MOVIE_IMAGE_STRIP_HEIGHT;
    if (halfWidth > 0 && stripHeight > 0) {
      bottom = SHRINKING_IMAGE_CENTRE_Y;
      offset = 0;
      for (i = 0; i < stripCount; i++) {
        LoadTextureBlock(&gfx,
                         sMovieBuffer + (offset * MOVIE_IMAGE_STRIP_HEIGHT +
                                         MOVIE_IMAGE_PIXELS_OFFSET),
                         G_IM_FMT_CI, G_IM_SIZ_8b, width,
                         MOVIE_IMAGE_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
        offset += width;
        top = bottom - stripHeight * SHRINKING_IMAGE_STRIPS_ABOVE_CENTRE;
        bottom += stripHeight;
        DrawTextureRect(&gfx, (SHRINKING_IMAGE_CENTRE_X - halfWidth) << 2,
                        top << 2, (SHRINKING_IMAGE_CENTRE_X + halfWidth) << 2,
                        (top + stripHeight) << 2, G_TX_RENDERTILE, 0, 0,
                        ((width >> 1) << 10) / halfWidth,
                        (MOVIE_IMAGE_STRIP_HEIGHT << 10) / stripHeight);
      }
    }
  }
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", DrawShrinkingMovieImage);
#endif

/* A Monty Mole pops out of the green */
void DrawMontyMoleMovie(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  EncodedAsset asset;
  s32 unused[4];
  s32 i;

  sMovieTick++;
  if (sMovieTick == MOVIE_TICKS_PER_FRAME) {
    sMovieTick = 0;
    OpenEncodedAsset(sMontyMoleMovieFrameAssets[sMovieFrame], &asset);
    ReadEncodedAsset(sMovieBuffer + MOVIE_FRAME_OFFSET, &asset);
    sMovieFrame++;
    if (sMovieFrame == MONTY_MOLE_FRAME_COUNT) {
      sCurrentMovie = MOVIE_NONE;
    }
  }
  if (sMovieDrawCount == 1) {
    PlaySoundWithEffects(SOUND_MONTY_MOLE, MOVIE_SOUND_CHANNEL,
                         MOVIE_SOUND_VOLUME, MOVIE_SOUND_REVERB,
                         MOVIE_SOUND_PAN);
  }
  gSPDisplayList(gfx++, sMovieSetupDisplayList);
  gDPLoadTLUT_pal256(gfx++,
                     TEXTURE_ADDRESS(sMovieBuffer + MOVIE_PALETTE_OFFSET));
  for (i = 0; i != MONTY_MOLE_STRIP_COUNT; i++) {
    gDPLoadTextureTile(
        gfx++,
        TEXTURE_ADDRESS(sMovieBuffer +
                        (MOVIE_FRAME_OFFSET + i * MONTY_MOLE_STRIP_SIZE)),
        G_IM_FMT_CI, G_IM_SIZ_8b, MONTY_MOLE_WIDTH, MONTY_MOLE_STRIP_HEIGHT, 0,
        0, MONTY_MOLE_WIDTH - 1, MONTY_MOLE_STRIP_HEIGHT - 1, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    DrawTextureRect(
        &gfx, MONTY_MOLE_X << 2,
        (MONTY_MOLE_Y + i * MONTY_MOLE_STRIP_HEIGHT) << 2,
        (MONTY_MOLE_X + MONTY_MOLE_WIDTH) << 2,
        (MONTY_MOLE_Y + MONTY_MOLE_STRIP_HEIGHT + i * MONTY_MOLE_STRIP_HEIGHT)
            << 2,
        G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
  }
  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* A Goomba hops out of the green */
void DrawGoombaMovie(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  EncodedAsset asset;
  s32 unused[4];
  s32 i;
  s32 height;

  sMovieTick++;
  if (sMovieTick == MOVIE_TICKS_PER_FRAME) {
    sMovieTick = 0;
    OpenEncodedAsset(sGoombaMovieFrameAssets[sMovieFrame], &asset);
    ReadEncodedAsset(sMovieBuffer + MOVIE_FRAME_OFFSET, &asset);
    sMovieFrame++;
    if (sMovieFrame == GOOMBA_FRAME_COUNT) {
      sCurrentMovie = MOVIE_NONE;
    }
  }
  if (sMovieDrawCount == 1) {
    PlaySoundWithEffects(SOUND_GOOMBA, MOVIE_SOUND_CHANNEL, MOVIE_SOUND_VOLUME,
                         MOVIE_SOUND_REVERB, MOVIE_SOUND_PAN);
  }
  gSPDisplayList(gfx++, sMovieSetupDisplayList);
  gDPLoadTLUT_pal256(gfx++,
                     TEXTURE_ADDRESS(sMovieBuffer + MOVIE_PALETTE_OFFSET));
  for (i = 0; i != GOOMBA_STRIP_COUNT; i++) {
    if (i == GOOMBA_STRIP_COUNT - 1) {
      height = GOOMBA_LAST_STRIP_HEIGHT;
    } else {
      height = GOOMBA_STRIP_HEIGHT;
    }
    gDPLoadTextureTile(
        gfx++,
        TEXTURE_ADDRESS(sMovieBuffer +
                        (MOVIE_FRAME_OFFSET + i * GOOMBA_STRIP_SIZE)),
        G_IM_FMT_CI, G_IM_SIZ_8b, GOOMBA_WIDTH, height, 0, 0, GOOMBA_WIDTH - 1,
        height - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    DrawTextureRect(&gfx, GOOMBA_X << 2,
                    (GOOMBA_Y + i * GOOMBA_STRIP_HEIGHT) << 2,
                    (GOOMBA_X + GOOMBA_WIDTH) << 2,
                    (GOOMBA_Y + i * GOOMBA_STRIP_HEIGHT + height) << 2,
                    G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
  }
  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

#ifdef NON_MATCHING
/* Lakitu flies in, counts down to the start on its starting light, then flies
 * off */
void DrawLakituMovie(Gfx** displayListHead) {
  EncodedAsset asset;
  Gfx* gfx = *displayListHead;
  s32 i;
  f32 halfWidth;
  f32 halfHeight;
  s32 roundedHalfWidth;
  s32 roundedHalfHeight;
  s32 x;
  s32 y;
  s32 offset;

  if (sMovieDrawCount == LAKITU_RED_LIGHT_DRAW) {
    PlaySound(SOUND_START_LIGHT_RED, g_NextSoundChannel,
              START_LIGHT_SOUND_VOLUME);
  }
  if (sMovieDrawCount == LAKITU_SECOND_RED_LIGHT_DRAW) {
    PlaySound(SOUND_START_LIGHT_RED, g_NextSoundChannel,
              START_LIGHT_SOUND_VOLUME);
  }
  if (sMovieDrawCount == LAKITU_GO_LIGHT_DRAW) {
    PlaySound(SOUND_START_LIGHT_GO, g_NextSoundChannel,
              START_LIGHT_SOUND_VOLUME);
  }
  if (sMovieDrawCount < LAKITU_ARRIVE_DRAW) {
    sLakitu.x -= (LAKITU_START_X - LAKITU_HOVER_X) / LAKITU_ARRIVE_DRAW;
    sLakitu.y += (LAKITU_HOVER_Y - LAKITU_START_Y) / LAKITU_ARRIVE_DRAW;
  } else if (sMovieDrawCount < LAKITU_LEAVE_DRAW) {
    sMovieTick++;
    if (sMovieTick == LAKITU_TICKS_PER_FRAME) {
      sMovieTick = 0;
      if (sMovieFrame < LAKITU_LAST_FRAME) {
        sMovieFrame++;
        sMovieBackBuffer ^= 1;
        OpenEncodedAsset(sLakituMovieFrameAssets[sMovieFrame], &asset);
        ReadEncodedAsset(sMovieBuffer + sMovieBackBuffer * LAKITU_FRAME_SIZE,
                         &asset);
      }
    }
    sLakituVelocity.x = 0.0f;
    sLakituVelocity.y = 0.0f;
  } else {
    sLakituVelocity.x -= LAKITU_LEAVE_ACCELERATION_X;
    sLakituVelocity.y -= LAKITU_LEAVE_ACCELERATION_Y;
    sLakitu.x += sLakituVelocity.x;
    sLakitu.scale += LAKITU_LEAVE_GROWTH;
    sLakitu.y += sLakituVelocity.y;
    if (sLakitu.x < LAKITU_GONE_X) {
      sCurrentMovie = MOVIE_NONE;
    }
  }
  gSPDisplayList(gfx++, sMovieImageSetupDisplayList);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPLoadTLUT_pal256(
      gfx++,
      TEXTURE_ADDRESS(sMovieBuffer + (sMovieBackBuffer * LAKITU_FRAME_SIZE +
                                      MOVIE_IMAGE_PALETTE_OFFSET)));
  offset = MOVIE_IMAGE_PIXELS_OFFSET;
  for (i = 0; i != LAKITU_STRIP_COUNT; i++) {
    halfWidth = sLakitu.scale * (LAKITU_WIDTH / 2);
    halfHeight = sLakitu.scale * LAKITU_STRIP_HEIGHT;
    gDPLoadTextureTile(
        gfx++,
        TEXTURE_ADDRESS(sMovieBuffer +
                        (sMovieBackBuffer * LAKITU_FRAME_SIZE + offset)),
        G_IM_FMT_CI, G_IM_SIZ_8b, LAKITU_WIDTH, LAKITU_STRIP_HEIGHT, 0, 0,
        LAKITU_WIDTH - 1, LAKITU_STRIP_HEIGHT - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    roundedHalfWidth = halfWidth + 0.5f;
    roundedHalfHeight = halfHeight + 0.5f;
    x = sLakitu.x;
    y = sLakitu.y;
    /* Start half a texel in, less the rounding, so that the image stays centred
     * as it scales */
    DrawTextureRect(&gfx, (x - roundedHalfWidth) << 2,
                    (y - roundedHalfHeight + i * roundedHalfHeight) << 2,
                    (x + roundedHalfWidth) << 2,
                    (y + i * roundedHalfHeight) << 2, G_TX_RENDERTILE,
                    16.0f - ((f32)roundedHalfWidth - halfWidth) * 16.0f,
                    16.0f - ((f32)roundedHalfHeight - halfHeight) * 16.0f,
                    ((LAKITU_WIDTH / 2) << 10) / halfWidth,
                    (LAKITU_STRIP_HEIGHT << 10) / halfHeight);
    gDPPipeSync(gfx++);
    offset += LAKITU_STRIP_SIZE;
  }
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", DrawLakituMovie);
#endif

#ifdef NON_MATCHING
/* Toad waves the out of bounds flag in the inset view */
void DrawToadMovie(Gfx** displayListHead) {
  EncodedAsset asset;
  Gfx* gfx = *displayListHead;
  s32 i;

  sMovieTick++;
  if (sMovieTick == MOVIE_TICKS_PER_FRAME) {
    sMovieTick = 0;
    sMovieFrame = (sMovieFrame + 1) % TOAD_FRAME_COUNT;
    if (sMovieFrame == TOAD_SWING_LEFT_FRAME) {
      PlaySoundWithEffects(SOUND_FLAG_SWISH, FLAG_SWISH_SOUND_CHANNEL,
                           FLAG_SWISH_SOUND_VOLUME, MOVIE_SOUND_REVERB,
                           FLAG_SWISH_LEFT_PAN);
      SetSoundFrequencyOffset(FLAG_SWISH_SOUND_CHANNEL, FLAG_SWISH_PITCH);
    }
    if (sMovieFrame == TOAD_SWING_RIGHT_FRAME) {
      PlaySoundWithEffects(SOUND_FLAG_SWISH, FLAG_SWISH_SOUND_CHANNEL,
                           FLAG_SWISH_SOUND_VOLUME, MOVIE_SOUND_REVERB,
                           FLAG_SWISH_RIGHT_PAN);
      SetSoundFrequencyOffset(FLAG_SWISH_SOUND_CHANNEL, FLAG_SWISH_PITCH);
    }
    sMovieBackBuffer ^= 1;
    OpenEncodedAsset(sToadMovieFrameAssets[sMovieFrame], &asset);
    ReadEncodedAsset(
        sMovieBuffer + (sMovieBackBuffer * TOAD_FRAME_SIZE + TOAD_FRAME_OFFSET),
        &asset);
  }
  if (sMovieDrawCount == OUT_OF_BOUNDS_SOUND_DRAW) {
    PlaySoundWithEffects(SOUND_OUT_OF_BOUNDS, OUT_OF_BOUNDS_SOUND_CHANNEL,
                         MOVIE_SOUND_VOLUME, MOVIE_SOUND_REVERB,
                         MOVIE_SOUND_PAN);
  }
  gSPDisplayList(gfx++, sMovieImageSetupDisplayList);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPSetScissor(
      gfx++, G_SC_NON_INTERLACE,
      g_Cameras[INSET_VIEW_CAMERA].view.scissorX + INSET_FRAME_WIDTH,
      g_Cameras[INSET_VIEW_CAMERA].view.scissorY + INSET_FRAME_HEIGHT,
      g_Cameras[INSET_VIEW_CAMERA].view.scissorX +
          g_Cameras[INSET_VIEW_CAMERA].view.scissorWidth - INSET_FRAME_WIDTH,
      g_Cameras[INSET_VIEW_CAMERA].view.scissorY +
          g_Cameras[INSET_VIEW_CAMERA].view.scissorHeight - INSET_FRAME_HEIGHT);
  gDPLoadTLUT_pal256(gfx++, TEXTURE_ADDRESS(sMovieBuffer));
  for (i = 0; i != TOAD_STRIP_COUNT; i++) {
    gDPLoadTextureTile(gfx++,
                       TEXTURE_ADDRESS(sMovieBuffer + TOAD_FRAME_OFFSET +
                                       (sMovieBackBuffer * TOAD_FRAME_SIZE +
                                        i * TOAD_STRIP_SIZE)),
                       G_IM_FMT_CI, G_IM_SIZ_8b, TOAD_WIDTH, TOAD_STRIP_HEIGHT,
                       0, 0, TOAD_WIDTH - 1, TOAD_STRIP_HEIGHT - 1, 0,
                       G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                       G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    DrawTextureRect(&gfx, TOAD_X << 2, (TOAD_Y + i * TOAD_STRIP_HEIGHT) << 2,
                    (TOAD_X + TOAD_WIDTH) << 2,
                    (TOAD_Y + TOAD_STRIP_HEIGHT + i * TOAD_STRIP_HEIGHT) << 2,
                    G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
  }
  gDPPipeSync(gfx++);
  gDPSetScissor(gfx++, G_SC_NON_INTERLACE, g_Cameras[0].view.scissorX,
                g_Cameras[0].view.scissorY,
                g_Cameras[0].view.scissorX + g_Cameras[0].view.scissorWidth,
                g_Cameras[0].view.scissorY + g_Cameras[0].view.scissorHeight);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", DrawToadMovie);
#endif

s32 IsContactEffectShown(void) {
  return g_ContactEffect != CONTACT_EFFECT_NONE;
}

/* Returns the CONTACT_STYLE_* of the player's character, or for a Game Boy one,
 * of its ball */
s32 GetContactStyle(UnkB8* player) {
  GameBoyCharacter* gameBoyCharacter;
  s8 character = player->characterIndex;

  if (character >= CHARACTER_KID) {
    gameBoyCharacter = &g_GameBoyCharacters[player->gameBoyCharacterIndex];
    if (gameBoyCharacter->status != GAME_BOY_CHARACTER_STATUS_PRESENT) {
      return CONTACT_STYLE_PLAIN;
    }
    switch (gameBoyCharacter->ballType) {
      case GAME_BOY_BALL_TYPE_PLAIN:
        return CONTACT_STYLE_PLAIN;
      case GAME_BOY_BALL_TYPE_MARIO:
        return CONTACT_STYLE_MARIO;
      case GAME_BOY_BALL_TYPE_BOWSER:
        return CONTACT_STYLE_BOWSER;
      case GAME_BOY_BALL_TYPE_WARIO:
        return CONTACT_STYLE_WARIO;
      case GAME_BOY_BALL_TYPE_HEAVY_HITTER:
        return CONTACT_STYLE_HEAVY_HITTER;
      case GAME_BOY_BALL_TYPE_5:
        return CONTACT_STYLE_GAME_BOY_BALL_5;
      case GAME_BOY_BALL_TYPE_6:
        return CONTACT_STYLE_GAME_BOY_BALL_6;
      case GAME_BOY_BALL_TYPE_7:
        return CONTACT_STYLE_GAME_BOY_BALL_7;
      default:
        return CONTACT_STYLE_PLAIN;
    }
  }
  if (character == CHARACTER_WARIO) {
    return CONTACT_STYLE_WARIO;
  }
  if (character == CHARACTER_MAPLE || character == CHARACTER_DONKEY_KONG) {
    return CONTACT_STYLE_HEAVY_HITTER;
  }
  if (character == CHARACTER_BOWSER) {
    return CONTACT_STYLE_BOWSER;
  }
  if (character == CHARACTER_BABY_MARIO || character == CHARACTER_MARIO ||
      character == CHARACTER_METAL_MARIO) {
    return CONTACT_STYLE_MARIO;
  }
  return CONTACT_STYLE_PLAIN;
}

/*
 * Places a contact effect at the ball and sizes it for its style;
 * CONTACT_EFFECT_NONE removes it and CONTACT_EFFECT_FADE fades it out
 */
void StartContactEffect(s32 effect) {
  s32 i;

  if (effect == CONTACT_EFFECT_NONE) {
    g_ContactEffect = effect;
  } else if (effect == CONTACT_EFFECT_POWER_WOOD ||
             effect == CONTACT_EFFECT_POWER_IRON ||
             effect == CONTACT_EFFECT_SHOT || effect == CONTACT_EFFECT_PUTT) {
    for (i = 0; i != CONTACT_MATRIX_COUNT; i++) {
      guTranslate(&sContactMatrices[i],
                  g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                  g_CurrentShot.ballPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE -
                      g_PinVertexScale * CONTACT_DROP_PER_PIN_SCALE,
                  g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE);
    }
    g_ContactEffect = 0;
    sIsContactEffectFading = FALSE;
    sContactVertices[0].v.ob[1] = 0;
    sContactVertices[1].v.ob[1] = 0;
    if (effect == CONTACT_EFFECT_POWER_IRON) {
      sContactStyle = CONTACT_STYLE_POWER_IRON;
    } else if (effect == CONTACT_EFFECT_SHOT) {
      sContactStyle = CONTACT_STYLE_SHOT;
    } else if (effect == CONTACT_EFFECT_PUTT) {
      sContactStyle = CONTACT_STYLE_PUTT;
    } else {
      sContactStyle = GetContactStyle(&g_Players[g_RoundHud.unk87]);
    }
  } else {
    sIsContactEffectFading = TRUE;
  }
  if (effect != CONTACT_EFFECT_NONE &&
      (effect == CONTACT_EFFECT_POWER_WOOD ||
       effect == CONTACT_EFFECT_POWER_IRON || effect == CONTACT_EFFECT_SHOT ||
       effect == CONTACT_EFFECT_PUTT)) {
    /* Each style sets its quad's half width, its texture's far corner and its
     * sound */
    if (sContactStyle == CONTACT_STYLE_POWER_IRON) {
      sContactVertices[0].v.ob[0] = -20;
      sContactVertices[1].v.ob[0] = 20;
      sContactTextureS = 0x3FF;
      sContactTextureT = 0xD6;
      PlaySound(SOUND_CONTACT_PLAIN, CONTACT_SOUND_CHANNEL,
                CONTACT_SOUND_VOLUME);
    } else if (sContactStyle == CONTACT_STYLE_SHOT) {
      sContactVertices[0].v.ob[0] = -10;
      sContactVertices[1].v.ob[0] = 10;
      sContactTextureS = 0x3FF;
      sContactTextureT = 0xD6;
    } else if (sContactStyle == CONTACT_STYLE_PUTT) {
      sContactVertices[0].v.ob[0] = -18;
      sContactVertices[1].v.ob[0] = 18;
      sContactVertices[0].v.ob[1] = CONTACT_PUTT_Y;
      sContactVertices[1].v.ob[1] = CONTACT_PUTT_Y;
      sContactTextureS = 0x1FF;
      sContactTextureT = 0x2A;
    } else if (sContactStyle == CONTACT_STYLE_WARIO ||
               sContactStyle == CONTACT_STYLE_GAME_BOY_BALL_7) {
      StartShotEffect(0);
      sContactVertices[0].v.ob[0] = -120;
      sContactVertices[1].v.ob[0] = 120;
      sContactTextureS = 0x5FF;
      sContactTextureT = 0x7FF;
      PlaySound(SOUND_CONTACT_WARIO, CONTACT_STYLE_SOUND_CHANNEL,
                CONTACT_SOUND_VOLUME);
    } else if (sContactStyle == CONTACT_STYLE_HEAVY_HITTER) {
      sContactVertices[0].v.ob[0] = -60;
      sContactVertices[1].v.ob[0] = 60;
      sContactTextureS = 0x7FF;
      sContactTextureT = 0;
      PlaySound(SOUND_CONTACT_HEAVY_HITTER, CONTACT_STYLE_SOUND_CHANNEL,
                CONTACT_SOUND_VOLUME);
    } else if (sContactStyle == CONTACT_STYLE_BOWSER) {
      sContactVertices[0].v.ob[0] = -20;
      sContactVertices[1].v.ob[0] = 20;
      sContactTextureS = 0x3FF;
      sContactTextureT = 0x54;
      PlaySound(SOUND_CONTACT_MARIO, CONTACT_STYLE_SOUND_CHANNEL,
                CONTACT_SOUND_VOLUME);
    } else if (sContactStyle == CONTACT_STYLE_MARIO) {
      sContactVertices[0].v.ob[0] = -160;
      sContactVertices[1].v.ob[0] = 160;
      sContactTextureS = 0x3FF;
      sContactTextureT = 0x2A;
      PlaySound(SOUND_CONTACT_MARIO, CONTACT_STYLE_SOUND_CHANNEL,
                CONTACT_SOUND_VOLUME);
    } else if (sContactStyle == CONTACT_STYLE_GAME_BOY_BALL_5) {
      sContactVertices[0].v.ob[0] = -30;
      sContactVertices[1].v.ob[0] = 30;
      sContactTextureS = 0x3FF;
      sContactTextureT = 0x2A;
      PlaySound(SOUND_CONTACT_PLAIN, CONTACT_SOUND_CHANNEL,
                CONTACT_SOUND_VOLUME);
    } else {
      sContactVertices[0].v.ob[0] = -20;
      sContactVertices[1].v.ob[0] = 20;
      sContactTextureS = 0x3FF;
      sContactTextureT = 0x2A;
      PlaySound(SOUND_CONTACT_PLAIN, CONTACT_SOUND_CHANNEL,
                CONTACT_SOUND_VOLUME);
    }
  }
}

/* Reads the images of the player's contact effect */
void LoadContactImages(UnkB8* player) {
  EncodedAsset asset;

  switch (GetContactStyle(player)) {
    case CONTACT_STYLE_WARIO:
    case CONTACT_STYLE_GAME_BOY_BALL_7:
      OpenEncodedAsset(WARIO_CONTACT_IMAGE_ASSET, &asset);
      ReadEncodedAsset(g_SharedEffectImages + CONTACT_IMAGE_OFFSET, &asset);
      OpenEncodedAsset(WARIO_CONTACT_SECOND_IMAGE_ASSET, &asset);
      ReadEncodedAsset(g_SharedEffectImages + CONTACT_SECOND_IMAGE_OFFSET,
                       &asset);
      break;
    case CONTACT_STYLE_HEAVY_HITTER:
      OpenEncodedAsset(HEAVY_HITTER_CONTACT_IMAGE_ASSET, &asset);
      ReadEncodedAsset(g_SharedEffectImages + CONTACT_IMAGE_OFFSET, &asset);
      break;
    case CONTACT_STYLE_MARIO:
      OpenEncodedAsset(MARIO_CONTACT_IMAGE_ASSET, &asset);
      ReadEncodedAsset(g_SharedEffectImages + CONTACT_IMAGE_OFFSET, &asset);
      break;
    case CONTACT_STYLE_BOWSER:
      OpenEncodedAsset(BOWSER_CONTACT_IMAGE_ASSET, &asset);
      ReadEncodedAsset(g_SharedEffectImages + CONTACT_IMAGE_OFFSET, &asset);
      break;
  }
}

#ifdef NON_MATCHING
/* g_ContactEffect counts the frames since contact, faster while fading, up to
 * here */
#define CONTACT_AGE_MAX 255
#define CONTACT_FADE_STEP 4
#define CONTACT_MIN_ALPHA 4
#define CONTACT_WARIO_ALPHA 200 /* Also Mario's, which fades from the start */
#define CONTACT_WARIO_HOLD_AGE 8
#define CONTACT_WARIO_FADE_PER_AGE 30
#define CONTACT_CHARACTER_ALPHA 160
#define CONTACT_CHARACTER_FADE_PER_AGE 3.2f
#define CONTACT_SHARED_ALPHA 96
#define CONTACT_SHARED_FADE_PER_AGE 2.0f
#define CONTACT_HOLD_AGE 100
/* sContactMatrices is a ring of the ball's positions, newest at
 * sContactTrailHead */
#define CONTACT_TRAIL_LENGTH 80
#define CONTACT_RIBBON_SCALE 0.1f
/* RSP's vertex buffer holds this many segments; the rest of the ribbon reloads
 * them back and forth */
#define CONTACT_RIBBON_FIRST_SEGMENTS 16
#define CONTACT_RIBBON_RELOAD_SEGMENTS 15
#define CONTACT_RIBBON_RELOAD_PASSES 2
#define CONTACT_PALETTE_SIZE 0x200
#define CONTACT_IMAGE_PIXELS_OFFSET \
  (CONTACT_IMAGE_OFFSET + IMAGE_HEADER_SIZE + CONTACT_PALETTE_SIZE)
#define CONTACT_PUTT_IMAGE_OFFSET 0x648
#define CONTACT_SPIN_DEGREES_PER_FRAME 70.0f
#define CONTACT_SPIN_SCALE_PER_ALPHA (1.0f / 512)
#define CONTACT_SPIN_ALPHA 230

/* Draws the ribbon segment between two of the ball's positions, as vertex pair
 * firstVertex */
#define DRAW_CONTACT_SEGMENT(segment, firstVertex)                          \
  gSPMatrix(                                                                \
      gfx++,                                                                \
      OS_K0_TO_PHYSICAL(&sContactMatrices[(sContactTrailHead + (segment)) % \
                                          CONTACT_TRAIL_LENGTH]),           \
      G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);                            \
  gSPMatrix(gfx++,                                                          \
            OS_K0_TO_PHYSICAL(&sContactRibbonMatrices[g_DrawBufferIndex]),  \
            G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);                    \
  gSPVertex(gfx++, sContactVertices, 2, firstVertex);                       \
  gSPPopMatrix(gfx++, G_MTX_MODELVIEW);                                     \
  gSPModifyVertex(gfx++, firstVertex, G_MWO_POINT_ST,                       \
                  sContactTextureT*(segment));                              \
  gSPModifyVertex(gfx++, (firstVertex) + 1, G_MWO_POINT_ST,                 \
                  (sContactTextureS << 16) + sContactTextureT * (segment))

#define DRAW_CONTACT_TRIANGLES()                                               \
  for (i = 0; i != CONTACT_RIBBON_RELOAD_SEGMENTS; i++) {                      \
    gSP2Triangles(gfx++, i * 2 + 1, i * 2, i * 2 + 2, 0, i * 2 + 1, i * 2 + 2, \
                  i * 2 + 3, 0);                                               \
  }

/* The shot style draws only the first segments */
#define DRAW_CONTACT_RIBBON()                                      \
  for (i = 0; i != CONTACT_RIBBON_FIRST_SEGMENTS; i++) {           \
    DRAW_CONTACT_SEGMENT(i, i * 2);                                \
  }                                                                \
  DRAW_CONTACT_TRIANGLES();                                        \
  if (sContactStyle != CONTACT_STYLE_SHOT) {                       \
    for (pass = 0; pass != CONTACT_RIBBON_RELOAD_PASSES; pass++) { \
      for (i = pass * 30 + 16; i != pass * 30 + 31; i++) {         \
        DRAW_CONTACT_SEGMENT(i, 28 - (i - pass * 30 - 16) * 2);    \
      }                                                            \
      DRAW_CONTACT_TRIANGLES();                                    \
      for (i = pass * 30 + 31; i != pass * 30 + 46; i++) {         \
        DRAW_CONTACT_SEGMENT(i, (i - pass * 30 - 31) * 2 + 2);     \
      }                                                            \
      DRAW_CONTACT_TRIANGLES();                                    \
    }                                                              \
  }

/*
 * Draws the contact effect as a ribbon through the ball's last positions, and
 * Mario's as a spinning image, while it fades
 */
void DrawContactEffect(Gfx** displayListHead, CameraMatrices* matrices) {
  Mat4 scale;
  Mat4 antiBillboard;
  Mat4 ribbon;
  Mat4 spin;
  Vec3 translation;
  Vec3 rotation;
  Vec3 spinScale;
  Gfx* gfx = *displayListHead;
  s32 alpha;
  s32 i;
  s32 pass;

  if (g_ContactEffect == CONTACT_EFFECT_NONE) {
    return;
  }
  if (sIsContactEffectFading == TRUE) {
    g_ContactEffect += CONTACT_FADE_STEP;
    if (g_ContactEffect > CONTACT_AGE_MAX) {
      g_ContactEffect = CONTACT_AGE_MAX;
    }
  } else if (g_ContactEffect < CONTACT_AGE_MAX) {
    g_ContactEffect++;
  }
  if (g_IsCupViewShown == 1) {
    return;
  }
  if (sContactStyle == CONTACT_STYLE_WARIO ||
      sContactStyle == CONTACT_STYLE_GAME_BOY_BALL_7) {
    if (g_ContactEffect < CONTACT_WARIO_HOLD_AGE) {
      alpha = CONTACT_WARIO_ALPHA;
    } else {
      alpha = CONTACT_WARIO_ALPHA - (g_ContactEffect - CONTACT_WARIO_HOLD_AGE) *
                                        CONTACT_WARIO_FADE_PER_AGE;
    }
  } else if (sContactStyle == CONTACT_STYLE_MARIO) {
    alpha = CONTACT_WARIO_ALPHA - g_ContactEffect;
  } else if (sContactStyle == CONTACT_STYLE_HEAVY_HITTER) {
    if (g_ContactEffect < CONTACT_HOLD_AGE) {
      alpha = CONTACT_CHARACTER_ALPHA;
    } else {
      alpha = CONTACT_CHARACTER_ALPHA - (g_ContactEffect - CONTACT_HOLD_AGE) *
                                            CONTACT_CHARACTER_FADE_PER_AGE;
    }
  } else if (sContactStyle == CONTACT_STYLE_PLAIN) {
    if (g_ContactEffect < CONTACT_HOLD_AGE) {
      alpha = CONTACT_CHARACTER_ALPHA;
    } else {
      alpha = CONTACT_CHARACTER_ALPHA - (g_ContactEffect - CONTACT_HOLD_AGE) *
                                            CONTACT_CHARACTER_FADE_PER_AGE;
    }
  } else if (sContactStyle == CONTACT_STYLE_BOWSER) {
    if (g_ContactEffect < CONTACT_HOLD_AGE) {
      alpha = CONTACT_CHARACTER_ALPHA;
    } else {
      alpha = CONTACT_CHARACTER_ALPHA - (g_ContactEffect - CONTACT_HOLD_AGE) *
                                            CONTACT_CHARACTER_FADE_PER_AGE;
    }
  } else if (g_ContactEffect < CONTACT_HOLD_AGE) {
    alpha = CONTACT_SHARED_ALPHA;
  } else {
    alpha = CONTACT_SHARED_ALPHA -
            CONTACT_SHARED_FADE_PER_AGE * (g_ContactEffect - CONTACT_HOLD_AGE);
  }
  if (alpha < CONTACT_MIN_ALPHA) {
    g_ContactEffect = CONTACT_EFFECT_NONE;
  }
  if (alpha <= 0) {
    return;
  }
  /* The ribbon starts where the ball was hit, then follows it */
  if (g_ContactEffect == 1) {
    sContactTrailHead = 0;
    for (i = 0; i != CONTACT_MATRIX_COUNT; i++) {
      guTranslate(&sContactMatrices[i],
                  g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                  g_CurrentShot.ballPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE -
                      g_PinVertexScale * CONTACT_DROP_PER_PIN_SCALE,
                  g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE);
    }
  } else {
    guTranslate(&sContactMatrices[sContactTrailHead],
                g_BallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                g_BallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE -
                    g_PinVertexScale * CONTACT_DROP_PER_PIN_SCALE,
                g_BallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE);
  }
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_PATTERN);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
  gDPSetEnvColor(gfx++, 255, 255, 255, 255);
  if (sContactStyle == CONTACT_STYLE_WARIO) {
    gDPSetPrimColor(gfx++, 0, 0, 255, 0, 255, alpha);
  } else if (sContactStyle == CONTACT_STYLE_GAME_BOY_BALL_7) {
    gDPSetEnvColor(gfx++, 255, 255, 127, 255);
    gDPSetPrimColor(gfx++, 0, 0, guRandom() % 255, guRandom() % 255,
                    guRandom() % 255, alpha);
  } else if (sContactStyle == CONTACT_STYLE_BOWSER) {
    gDPSetPrimColor(gfx++, 0, 0, 255, 0, 255, alpha);
    gDPSetPrimColor(gfx++, 0, 0, 255, 0, 0, alpha);
    gDPSetEnvColor(gfx++, 255, 247, 239, 255);
  } else if (sContactStyle == CONTACT_STYLE_MARIO) {
    gDPSetPrimColor(gfx++, 0, 0, 255, 0, 0, alpha);
  } else if (sContactStyle == CONTACT_STYLE_PLAIN ||
             sContactStyle == CONTACT_STYLE_GAME_BOY_BALL_5 ||
             sContactStyle == CONTACT_STYLE_PUTT) {
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 255, alpha);
  } else if (sContactStyle == CONTACT_STYLE_GAME_BOY_BALL_6) {
    gDPSetPrimColor(gfx++, 0, 0, 255, 127, 0, alpha);
    gDPSetEnvColor(gfx++, 255, 191, 63, 255);
  } else {
    gDPSetPrimColor(gfx++, 0, 0, 191, 191, 255, alpha);
  }
  /* Bowser's and the heavy hitters' images have their own colours */
  if (sContactStyle == CONTACT_STYLE_BOWSER ||
      sContactStyle == CONTACT_STYLE_HEAVY_HITTER) {
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0,
                      TEXEL0, TEXEL0, 0, PRIMITIVE, 0);
  } else {
    gDPSetCombineLERP(gfx++, ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE,
                      PRIMITIVE, 0, TEXEL0, 0, ENVIRONMENT, PRIMITIVE, TEXEL0,
                      PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
  }
  if (sContactStyle == CONTACT_STYLE_HEAVY_HITTER) {
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(
        gfx++, TEXTURE_ADDRESS(g_SharedEffectImages + CONTACT_IMAGE_OFFSET +
                               IMAGE_HEADER_SIZE));
    gDPLoadTextureBlock(
        gfx++,
        TEXTURE_ADDRESS(g_SharedEffectImages + CONTACT_IMAGE_PIXELS_OFFSET),
        G_IM_FMT_CI, G_IM_SIZ_8b, 64, 8, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, 7, G_TX_NOLOD, G_TX_NOLOD);
  } else if (sContactStyle == CONTACT_STYLE_BOWSER) {
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 255, 255, 255, 1);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(
        gfx++, TEXTURE_ADDRESS(g_SharedEffectImages + CONTACT_IMAGE_OFFSET +
                               IMAGE_HEADER_SIZE));
    gDPLoadTextureBlock(
        gfx++,
        TEXTURE_ADDRESS(g_SharedEffectImages + CONTACT_IMAGE_PIXELS_OFFSET),
        G_IM_FMT_CI, G_IM_SIZ_8b, 16, 100, 0, G_TX_MIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, 4, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  } else if (sContactStyle == CONTACT_STYLE_WARIO ||
             sContactStyle == CONTACT_STYLE_GAME_BOY_BALL_7) {
    gDPLoadTextureBlock_4b(
        gfx++,
        TEXTURE_ADDRESS(g_SharedEffectImages + CONTACT_IMAGE_OFFSET +
                        IMAGE_HEADER_SIZE),
        G_IM_FMT_I, 48, 128, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, 7, G_TX_NOLOD, G_TX_NOLOD);
  } else if (sContactStyle == CONTACT_STYLE_PLAIN ||
             sContactStyle == CONTACT_STYLE_GAME_BOY_BALL_6 ||
             sContactStyle == CONTACT_STYLE_GAME_BOY_BALL_5) {
    gDPLoadTextureBlock_4b(
        gfx++, TEXTURE_ADDRESS(g_SharedEffectImages + IMAGE_HEADER_SIZE),
        G_IM_FMT_I, 32, 100, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
  } else if (sContactStyle == CONTACT_STYLE_PUTT) {
    gDPLoadTextureBlock_4b(
        gfx++,
        TEXTURE_ADDRESS(g_SharedEffectImages + CONTACT_PUTT_IMAGE_OFFSET +
                        IMAGE_HEADER_SIZE),
        G_IM_FMT_I, 16, 100, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
  } else {
    gDPLoadTextureBlock_4b(
        gfx++, TEXTURE_ADDRESS(g_SharedEffectImages + IMAGE_HEADER_SIZE),
        G_IM_FMT_I, 32, 100, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
  }
  guScaleF(scale, CONTACT_RIBBON_SCALE, CONTACT_RIBBON_SCALE,
           CONTACT_RIBBON_SCALE);
  guMtxL2F(antiBillboard, &matrices->antiBillboard);
  MultiplyMatrices(scale, antiBillboard, ribbon);
  guMtxF2L(ribbon, &sContactRibbonMatrices[g_DrawBufferIndex]);
  if (sContactStyle != CONTACT_STYLE_MARIO) {
    gDPPipeSync(gfx++);
    DRAW_CONTACT_RIBBON();
    if (sContactStyle == CONTACT_STYLE_WARIO ||
        sContactStyle == CONTACT_STYLE_GAME_BOY_BALL_7) {
      gDPLoadTextureBlock_4b(
          gfx++,
          TEXTURE_ADDRESS(g_SharedEffectImages + CONTACT_SECOND_IMAGE_OFFSET +
                          IMAGE_HEADER_SIZE),
          G_IM_FMT_I, 48, 128, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, 7, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      DRAW_CONTACT_RIBBON();
    }
  }
  if (sContactStyle == CONTACT_STYLE_MARIO) {
    translation[AXIS_X] = translation[AXIS_Y] = translation[AXIS_Z] = 0.0f;
    rotation[AXIS_X] = 0.0f;
    rotation[AXIS_Y] = 0.0f;
    rotation[AXIS_Z] = sContactSpinAngle += CONTACT_SPIN_DEGREES_PER_FRAME;
    spinScale[AXIS_X] = spinScale[AXIS_Y] = spinScale[AXIS_Z] =
        alpha * CONTACT_SPIN_SCALE_PER_ALPHA;
    BuildTransformMatrix(spin, rotation, translation, spinScale);
    ConvertMatrixToFixed(spin, &sContactSpinMatrices[g_MatrixBufferIndex]);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(
        gfx++, TEXTURE_ADDRESS(g_SharedEffectImages + CONTACT_IMAGE_OFFSET +
                               IMAGE_HEADER_SIZE));
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, CONTACT_SPIN_ALPHA);
    gDPLoadTextureBlock(
        gfx++,
        TEXTURE_ADDRESS(g_SharedEffectImages + CONTACT_IMAGE_PIXELS_OFFSET),
        G_IM_FMT_CI, G_IM_SIZ_8b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
                      TEXEL0, PRIMITIVE, 0, TEXEL0, 0);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sContactMatrices[sContactTrailHead]),
              G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&matrices->antiBillboard),
              G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPMatrix(gfx++,
              OS_K0_TO_PHYSICAL(&sContactSpinMatrices[g_MatrixBufferIndex]),
              G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(gfx++, sContactSpinVertices, 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
  }
  /* The ring of matrices turns back one so the newest position takes the slot
   * before */
  sContactTrailHead--;
  if (sContactTrailHead < 0) {
    sContactTrailHead = CONTACT_TRAIL_LENGTH - 1;
  }
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", DrawContactEffect);
#endif

/* Shows the Fly Guy that warns of a strong wind, or hides it given
 * FLY_GUY_HIDDEN */
void ShowWindFlyGuy(s32 show) {
  EncodedAsset asset;

  if (sIsMovieBufferAllocated == TRUE) {
    if (show == FLY_GUY_HIDDEN) {
      sWindFlyGuyFrame = FLY_GUY_HIDDEN;
      return;
    }
    sWindFlyGuyFrame = 0;
    sWindFlyGuyAsset = FLY_GUY_IMAGE_ASSET;
    OpenEncodedAsset(FLY_GUY_IMAGE_ASSET, &asset);
    ReadEncodedAsset(sMovieBuffer, &asset);
    OpenEncodedAsset(WIND_STREAK_IMAGE_ASSET, &asset);
    ReadEncodedAsset(sMovieBuffer + FLY_GUY_IMAGE_SIZE, &asset);
    sLoadedBanner = -1;
    sCurrentBanner = -1;
    ResetPath();
  }
}

/* The Fly Guy and its streak are drawn on screen; a point's x and y are in
 * screen pixels */
typedef struct FlyGuyPoint {
  /* 0x0 */ f32 x;
  /* 0x4 */ f32 y;
  /* 0x8 */ f32 z;
} FlyGuyPoint;

extern FlyGuyPoint sWindFlyGuyPath[];
/* Where the Fly Guy stays while its streak fades */
extern FlyGuyPoint sWindFlyGuyExit;
extern FlyGuyPoint sWindFlyGuyLastPosition;
/* The contact matrix that the streak starts from; the streak's matrices form a
 * ring behind it */
extern s32 sWindStreakHead;

#define WIND_FLY_GUY_PATH_POINT_COUNT 17
#define WIND_FLY_GUY_PATH_PACE 12
#define WIND_FLY_GUY_FRAME_COUNT 14
/* sWindFlyGuyFrame while the Fly Guy flies its path; after it, the frame counts
 * up as the streak fades */
#define WIND_FLY_GUY_STARTING 0
#define WIND_FLY_GUY_FLYING 1
#define WIND_FLY_GUY_ARRIVED 2
#define WIND_FLY_GUY_GONE_FRAME 20
#define WIND_STREAK_ALPHA 190
#define WIND_STREAK_FADE_STEP 10
#define WIND_STREAK_HALF_WIDTH 10
#define WIND_STREAK_LENGTH 80
/* The streak's quads, drawn as two runs since only 32 vertices fit in the
 * vertex cache */
#define WIND_STREAK_FIRST_RUN 16
#define WIND_STREAK_SECOND_RUN_END 31
#define WIND_STREAK_QUADS_PER_RUN 15
/* The first vertex slot of the last quad of a run */
#define WIND_STREAK_LAST_SLOT ((WIND_STREAK_QUADS_PER_RUN - 1) * 2)
/* The far side of the streak texture, in S10.5 */
#define WIND_STREAK_TEXTURE_S 0x5FF
/* Each quad steps the streak texture by 8 texels */
#define WIND_STREAK_TEXTURE_T_SHIFT 8
#define WIND_STREAK_IMAGE_WIDTH 48
#define WIND_STREAK_IMAGE_HEIGHT 128
#define WIND_STREAK_IMAGE_MASK_T 7
#define WIND_STREAK_DEPTH -300.0f
#define FLY_GUY_IMAGE_WIDTH 32
#define FLY_GUY_IMAGE_HEIGHT 32
#define SOUND_WIND_FLY_GUY 0x28
#define WIND_FLY_GUY_SOUND_CHANNEL 10
#define WIND_FLY_GUY_SOUND_VOLUME 63

#ifdef NON_MATCHING
/* The Fly Guy that warns of a strong wind, flying across the screen and
 * trailing a streak of wind */
void DrawWindFlyGuy(Gfx** displayListHead) {
  EncodedAsset asset;
  FlyGuyPoint position;
  Vec3 translation;
  Vec3 rotation;
  Mat4 matrix;
  Gfx* gfx = *displayListHead;
  s32 alpha;
  s32 i;
  s32 slotsBack;

  if (sWindFlyGuyFrame == FLY_GUY_HIDDEN || g_PauseScreen) {
    return;
  }
  sWindFlyGuyAsset++;
  if (sWindFlyGuyAsset == FLY_GUY_IMAGE_ASSET + WIND_FLY_GUY_FRAME_COUNT) {
    sWindFlyGuyAsset = FLY_GUY_IMAGE_ASSET;
  }
  OpenEncodedAsset(sWindFlyGuyAsset, &asset);
  ReadEncodedAsset(sMovieBuffer, &asset);
  if (sWindFlyGuyFrame < WIND_FLY_GUY_ARRIVED) {
    if (StepAlongPath((f32(*)[AXIS_COUNT])sWindFlyGuyPath,
                      WIND_FLY_GUY_PATH_POINT_COUNT, WIND_FLY_GUY_PATH_PACE,
                      &position.x) == 1) {
      sWindFlyGuyFrame = WIND_FLY_GUY_ARRIVED;
    }
    alpha = WIND_STREAK_ALPHA;
  } else {
    sWindFlyGuyFrame++;
    alpha = WIND_STREAK_ALPHA - sWindFlyGuyFrame * WIND_STREAK_FADE_STEP;
    if (alpha < 0) {
      alpha = 0;
    }
    if (sWindFlyGuyFrame == WIND_FLY_GUY_GONE_FRAME) {
      sWindFlyGuyFrame = FLY_GUY_HIDDEN;
    }
    position = sWindFlyGuyExit;
  }
  if (sWindFlyGuyFrame == WIND_FLY_GUY_STARTING) {
    sWindFlyGuyLastPosition = sWindFlyGuyPath[0];
    sContactVertices[0].v.ob[0] = -WIND_STREAK_HALF_WIDTH;
    sWindStreakHead = 0;
    sContactVertices[1].v.ob[0] = WIND_STREAK_HALF_WIDTH;
    for (i = 0; i != CONTACT_MATRIX_COUNT; i++) {
      guTranslate(&sContactMatrices[i], position.x - SCREEN_WIDTH / 2,
                  SCREEN_HEIGHT / 2 - position.y, WIND_STREAK_DEPTH);
    }
    PlaySound(SOUND_WIND_FLY_GUY, WIND_FLY_GUY_SOUND_CHANNEL,
              WIND_FLY_GUY_SOUND_VOLUME);
    sWindFlyGuyFrame = WIND_FLY_GUY_FLYING;
  }
  rotation[AXIS_X] = 0.0f;
  rotation[AXIS_Y] = 0.0f;
  rotation[AXIS_Z] = atan2(sWindFlyGuyLastPosition.x - position.x,
                           sWindFlyGuyLastPosition.y - position.y);
  translation[AXIS_Z] = WIND_STREAK_DEPTH;
  translation[AXIS_X] = position.x - SCREEN_WIDTH / 2;
  translation[AXIS_Y] = SCREEN_HEIGHT / 2 - position.y;
  BuildRotationMatrix(matrix, rotation, translation);
  ConvertMatrixToFixed(matrix, &sContactMatrices[sWindStreakHead]);
  sWindFlyGuyLastPosition = position;
  SetUpTextDrawing(&gfx);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
  gDPSetEnvColor(gfx++, 255, 255, 255, 255);
  gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
                    PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
  LoadNibbleTextureBlock(
      &gfx, sMovieBuffer + FLY_GUY_IMAGE_SIZE + IMAGE_HEADER_SIZE, G_IM_FMT_I,
      WIND_STREAK_IMAGE_WIDTH, WIND_STREAK_IMAGE_HEIGHT, 0, G_TX_MIRROR,
      G_TX_MIRROR, 0, WIND_STREAK_IMAGE_MASK_T, 0, 0);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
            G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gDPPipeSync(gfx++);
  for (i = 0; i != WIND_STREAK_FIRST_RUN; i++) {
    gSPMatrix(
        gfx++,
        OS_K0_TO_PHYSICAL(
            &sContactMatrices[(sWindStreakHead + i) % WIND_STREAK_LENGTH]),
        G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPVertex(gfx++, sContactVertices, 2, i * 2);
    gSPModifyVertex(gfx++, i * 2, G_MWO_POINT_ST,
                    i << WIND_STREAK_TEXTURE_T_SHIFT);
    gSPModifyVertex(
        gfx++, i * 2 + 1, G_MWO_POINT_ST,
        (WIND_STREAK_TEXTURE_S << 16) + (i << WIND_STREAK_TEXTURE_T_SHIFT));
  }
  for (i = 0; i != WIND_STREAK_QUADS_PER_RUN; i++) {
    gSP2Triangles(gfx++, i * 2 + 1, i * 2, i * 2 + 2, 0, i * 2 + 1, i * 2 + 2,
                  i * 2 + 3, 0);
  }
  /* The second run reuses the vertex slots backwards from the last vertex of
   * the first */
  for (i = WIND_STREAK_FIRST_RUN, slotsBack = 0;
       i != WIND_STREAK_SECOND_RUN_END; i++, slotsBack += 2) {
    gSPMatrix(
        gfx++,
        OS_K0_TO_PHYSICAL(
            &sContactMatrices[(sWindStreakHead + i) % WIND_STREAK_LENGTH]),
        G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPVertex(gfx++, sContactVertices, 2, WIND_STREAK_LAST_SLOT - slotsBack);
    gSPModifyVertex(gfx++, WIND_STREAK_LAST_SLOT - slotsBack, G_MWO_POINT_ST,
                    i << WIND_STREAK_TEXTURE_T_SHIFT);
    gSPModifyVertex(
        gfx++, WIND_STREAK_LAST_SLOT + 1 - slotsBack, G_MWO_POINT_ST,
        (WIND_STREAK_TEXTURE_S << 16) + (i << WIND_STREAK_TEXTURE_T_SHIFT));
  }
  for (i = 0; i != WIND_STREAK_QUADS_PER_RUN; i++) {
    gSP2Triangles(gfx++, i * 2 + 1, i * 2, i * 2 + 2, 0, i * 2 + 1, i * 2 + 2,
                  i * 2 + 3, 0);
  }
  sWindStreakHead--;
  if (sWindStreakHead < 0) {
    sWindStreakHead = WIND_STREAK_LENGTH - 1;
  }
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  LoadTextureBlock(&gfx, sMovieBuffer + IMAGE_HEADER_SIZE, G_IM_FMT_RGBA,
                   G_IM_SIZ_16b, FLY_GUY_IMAGE_WIDTH, FLY_GUY_IMAGE_HEIGHT, 0,
                   0, 0, 0, 0, 0, 0);
  DrawTextureRect(&gfx, ((s32)position.x - FLY_GUY_IMAGE_WIDTH / 2) * 4,
                  ((s32)position.y - FLY_GUY_IMAGE_HEIGHT / 2) * 4,
                  ((s32)position.x + FLY_GUY_IMAGE_WIDTH / 2) * 4,
                  ((s32)position.y + FLY_GUY_IMAGE_HEIGHT / 2) * 4,
                  G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", DrawWindFlyGuy);
#endif
