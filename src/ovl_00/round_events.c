#include "round_events.h"

#include <math.h>
#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "camera.h"
#include "character.h"
#include "compiler.h"
#include "controller.h"
#include "course_space.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "curve.h"
#include "game.h"
#include "game_flags.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "heap.h"
#include "language.h"
#include "math_util.h"
#include "movie.h"
#include "pin.h"
#include "play.h"
#include "player.h"
#include "round_hud.h"
#include "round_stats.h"
#include "screen.h"
#include "screen_layers.h"
#include "shot_camera.h"
#include "shot_effects.h"
#include "sound.h"
#include "text.h"
#include "transfer_pak.h"
#include "turn.h"

#define MOVIE_BUFFER_SIZE 0x6270
#define MOVIE_CUE_FRAME 12

#define ASSET_FLY_GUY_IMAGE 0x6CF
#define FLY_GUY_IMAGE_SIZE 0x808

#define CONTACT_IMAGE_OFFSET 0x970
#define CONTACT_SECOND_IMAGE_OFFSET 0x1578
#define ASSET_MARIO_CONTACT_IMAGE 0x6BE
#define MARIO_CONTACT_IMAGE_WIDTH 32
#define MARIO_CONTACT_IMAGE_HEIGHT 32
#define ASSET_BOWSER_CONTACT_IMAGE 0x6AE
#define BOWSER_CONTACT_IMAGE_WIDTH 16
#define BOWSER_CONTACT_IMAGE_HEIGHT 100
#define ASSET_WARIO_CONTACT_IMAGE 0x6A7
#define ASSET_WARIO_CONTACT_SECOND_IMAGE 0x6A8
#define WARIO_CONTACT_IMAGE_WIDTH 48
#define WARIO_CONTACT_IMAGE_HEIGHT 128
#define ASSET_RAINBOW_CONTACT_IMAGE 0x6A6
#define RAINBOW_CONTACT_IMAGE_WIDTH 64
#define RAINBOW_CONTACT_IMAGE_HEIGHT 8
#define PLAIN_CONTACT_IMAGE_WIDTH 32
#define PLAIN_CONTACT_IMAGE_HEIGHT 100
#define PUTT_CONTACT_IMAGE_WIDTH 16
#define PUTT_CONTACT_IMAGE_HEIGHT 100

#define BANNER_LETTERS_MAX 11
#define BANNER_LETTER_SLOT_COUNT 10

#define CAPTION_MEET_DRAW 25
#define CAPTION_FLASH_END_DRAW 37
#define CAPTION_GROW_DRAW 44
#define CAPTION_GROW_END_DRAW 56
#define CAPTION_DRAW_COUNT 90
#define CAPTION_TURN_PER_FRAME (M_PI / 12)
#define CAPTION_START_SLIDE 600
#define CAPTION_SLIDE_PER_FRAME 24
#define CAPTION_GROWTH_MAX 48
#define CAPTION_COPY_ALPHA 127
#define CAPTION_COPY_TOP \
    (SHRINKING_IMAGE_CENTER_Y - (MOVIE_IMAGE_STRIP_HEIGHT * SHRINKING_IMAGE_STRIPS_ABOVE_CENTER))
#define CAPTION_FLASH_DEPTH 127.0f
#define CAPTION_SOUND_CHANNEL 13
#define CAPTION_SOUND_VOLUME 63

enum BannerAnimation {
    BANNER_ANIMATION_ZOOM,
    BANNER_ANIMATION_SPIN,
    BANNER_ANIMATION_SLIDE,
    BANNER_ANIMATION_ZOOM_STAY
};

typedef s8 BannerAnimation;

typedef struct Banner {
    s16 unusedBeforeImageAsset;
    AssetId imageAsset;
    s8 letterCount;
    u8 letterWidth;
    u8 letterHeight;
    BannerAnimation animation;
} Banner;

typedef struct BannerLetter {
    f32 x;
    f32 y;
    f32 scale;
    s32 unusedLetterValue;
    Mtx matrices[2];
} BannerLetter;

enum BannerLayoutMode {
    BANNER_LAYOUT_LEFT,
    BANNER_LAYOUT_FIRST_LETTER
};

typedef s16 BannerLayout[BANNER_LAYOUT_FIRST_LETTER + BANNER_LETTERS_MAX];

typedef struct LakituFlight {
    f32 x;
    f32 y;
    f32 scale;
} LakituFlight;

typedef struct LakituVelocity {
    f32 x;
    f32 y;
} LakituVelocity;

#define FLIP_MESSAGE_RESET (-2)
#define FLIP_MESSAGE_LEAVE (-1)
#define FLIP_MESSAGE_FRAME_NONE (-1)
#define INITIAL_IS_FLIP_MESSAGE_LEAVING (-1)
#define FLIP_MESSAGE_GLYPH_GROUP 0
#define FLIP_MESSAGE_TEXT_SLOT 0
#define FLIP_MESSAGE_X (SCREEN_WIDTH / 2)
#define FLIP_MESSAGE_Y 114
#define FLIP_MESSAGE_BOARD_MARGIN_X 6
#define FLIP_MESSAGE_BOARD_MARGIN_Y 14
#define FLIP_MESSAGE_OPEN_FRAME 10
#define BANNER_FRAME_NONE (-1)
#define CONTACT_MATRIX_COUNT 81
#define MOVIE_IMAGE_PALETTE_OFFSET 8
#define MOVIE_IMAGE_PIXELS_OFFSET 0x208
#define MOVIE_IMAGE_STRIP_HEIGHT 6
#define SHRINKING_IMAGE_SOUND_DRAW 26
#define SHRINKING_IMAGE_SOUND_CHANNEL 13
#define SHRINKING_IMAGE_SOUND_VOLUME 63
#define SHRINKING_IMAGE_DRAW_COUNT 90
#define SHRINKING_IMAGE_TURN_PER_FRAME (M_PI / 16)
#define SHRINKING_IMAGE_CENTER_X (SCREEN_WIDTH / 2)
#define SHRINKING_IMAGE_CENTER_Y 104
#define SHRINKING_IMAGE_STRIPS_ABOVE_CENTER 4
#define CONTACT_DROP_PER_PIN_SCALE 0.3f
#define CONTACT_STYLE_SOUND_CHANNEL 15
#define CONTACT_PUTT_Y (-2)
#define BANNER_NUMBERED_OVER_PAR (BANNER_STROKES_OVER_PAR + 1)
#define BANNER_LETTER_SPACING 32
#define BANNER_Z (-300.0f)
#define FLIP_MESSAGE_DEGREES_PER_FRAME 18.0f
#define FLIP_MESSAGE_START_DEGREES 180.0f
#define FLIP_MESSAGE_GROWTH_PER_FRAME 0.1f
#define FLIP_MESSAGE_BOARD_Y 20.0f
#define FLIP_MESSAGE_BOARD_Z (-300.0f)
#define FLIP_MESSAGE_BOARD_ALPHA 159

#define BANNER_LETTER_IMAGES_OFFSET 0x208
#define BANNER_LETTER_IMAGE_SIZE 0x800
#define BANNER_LETTER_IMAGE_WIDTH 32
#define BANNER_LETTER_IMAGE_HEIGHT 64
#define BANNER_PLUS_IMAGE 0
#define BANNER_FIRST_DIGIT_IMAGE 1

#define MOVIE_DRAW_COUNT_MAX 1000

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
#define TOAD_SWING_LEFT_FRAME 8
#define TOAD_SWING_RIGHT_FRAME 2
#define FLAG_SWISH_SOUND_CHANNEL 15
#define FLAG_SWISH_LEFT_PAN 16
#define FLAG_SWISH_RIGHT_PAN 239
#define FLAG_SWISH_PITCH 12.0f
#define OUT_OF_BOUNDS_SOUND_DRAW 16
#define OUT_OF_BOUNDS_SOUND_CHANNEL 13
#define INSET_VIEW_CAMERA 1
#define INSET_FRAME_WIDTH 2
#define INSET_FRAME_HEIGHT 1
#define ASSET_LAKITU 0x64E
#define LAKITU_BACK_IMAGE_OFFSET 0x1008
#define LAKITU_START_X 384.0f
#define LAKITU_START_Y (-64.0f)
#define LAKITU_START_SCALE 0.7f
#define LAKITU_HOVER_X 192.0f
#define LAKITU_HOVER_Y 96.0f
#define LAKITU_ARRIVE_DRAW 30
#define LAKITU_LEAVE_DRAW 120
#define LAKITU_TICKS_PER_FRAME 8
#define LAKITU_LAST_FRAME 12
#define LAKITU_LEAVE_ACCELERATION_X 0.2f
#define LAKITU_LEAVE_ACCELERATION_Y 0.1f
#define LAKITU_LEAVE_GROWTH 0.01f
#define LAKITU_GONE_X (-64.0f)
#define LAKITU_FRAME_SIZE 0x1008
#define LAKITU_WIDTH 56
#define LAKITU_STRIP_HEIGHT 32
#define LAKITU_STRIP_COUNT 2
#define LAKITU_STRIP_SIZE (LAKITU_WIDTH * LAKITU_STRIP_HEIGHT)
#define LAKITU_RED_LIGHT_DRAW 50
#define LAKITU_SECOND_RED_LIGHT_DRAW 80
#define LAKITU_GO_LIGHT_DRAW 110
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

#define MONTY_MOLE_FRAME_COUNT 17
#define MONTY_MOLE_WIDTH 108
#define MONTY_MOLE_STRIP_HEIGHT 18
#define MONTY_MOLE_STRIP_COUNT 4
#define MONTY_MOLE_STRIP_SIZE (MONTY_MOLE_WIDTH * MONTY_MOLE_STRIP_HEIGHT)
#define MONTY_MOLE_X 106
#define MONTY_MOLE_Y 62

#define GOOMBA_FRAME_COUNT 11
#define GOOMBA_WIDTH 80
#define GOOMBA_STRIP_HEIGHT 25
#define GOOMBA_LAST_STRIP_HEIGHT 23
#define GOOMBA_STRIP_COUNT 3
#define GOOMBA_STRIP_SIZE (GOOMBA_WIDTH * GOOMBA_STRIP_HEIGHT)
#define GOOMBA_X 120
#define GOOMBA_Y 62
#define MOVIE_TEST_CONTROLLER 1

static s32 f_BannerFrame = BANNER_FRAME_NONE;
static s32 f_CurrentBanner = BANNER_NONE;
static s32 f_BannerNumber = 0;
static s32 f_BannerNumberLetterCount = 0;
static s32 f_LoadedBanner = BANNER_NONE;
static Boolean f_IsMovieBufferAllocated = FALSE;
static s32 f_UnusedWordAfterMovieBufferFlag UNUSED = -1;
static Vtx f_BannerLetterVertices[QUAD_VERTEX_COUNT] = {
    {{{0, 32, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{0, -32, 0}, 0, {0, 0xFC0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{32, -32, 0}, 0, {0x7FF, 0xFC0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{32, 32, 0}, 0, {0x7FF, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Banner f_Banners[] = {
    {0, 0x542, 10, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SLIDE},
    {0, 0x544, 8, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SPIN},
    {0, 0x543, 9, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SPIN},
    {0, 0x545, 9, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SPIN},
    {0, 0x546, 8, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SPIN},
    {0, 0x547, 9, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SPIN},
    {0, 0x548, 9, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SPIN},
    {0, 0x54B, 9, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SPIN},
    {0, 0x54C, 7, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SPIN},
    {0, 0x54A, 6, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SLIDE},
    {0, 0x549, 8, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SLIDE},
    {0, 0x551, 10, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SLIDE},
    {0, 0x552, 9, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SLIDE},
    {0, 0x550, 6, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SLIDE},
    {0, 0x54E, 7, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SLIDE},
    {0, 0x54D, 5, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, BANNER_ANIMATION_SLIDE},
    {
        0,
        0x54F,
        6,
        BANNER_LETTER_IMAGE_WIDTH,
        BANNER_LETTER_IMAGE_HEIGHT,
        BANNER_ANIMATION_ZOOM_STAY,
    },
    {
        0,
        0x553,
        11,
        BANNER_LETTER_IMAGE_WIDTH,
        BANNER_LETTER_IMAGE_HEIGHT,
        BANNER_ANIMATION_ZOOM_STAY,
    },
};
static BannerLayout f_BannerLayouts[] = {
    {-144, 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 0},
    {-128, 0, 32, 64, 96, 128, 160, 192, 224, 0, 0, 0},
    {-144, 0, 32, 64, 96, 128, 160, 192, 224, 256, 0, 0},
    {-132, 0, 32, 64, 96, 128, 160, 192, 224, 256, 0, 0},
    {-128, 0, 32, 64, 96, 128, 160, 192, 224, 0, 0, 0},
    {-128, 0, 32, 64, 96, 128, 160, 192, 224, 256, 0, 0},
    {-136, 0, 32, 64, 96, 128, 160, 192, 224, 256, 0, 0},
    {-144, 0, 32, 64, 96, 128, 160, 192, 224, 256, 0, 0},
    {-96, 0, 32, 64, 96, 128, 160, 192, 0, 0, 0, 0},
    {-88, 0, 32, 64, 96, 128, 160, 0, 0, 0, 0, 0},
    {-120, 0, 32, 64, 96, 128, 160, 192, 224, 0, 0, 0},
    {-148, 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 0},
    {-132, 0, 32, 64, 96, 128, 160, 192, 224, 256, 0, 0},
    {-88, 0, 32, 64, 96, 128, 160, 0, 0, 0, 0, 0},
    {-104, 0, 32, 64, 96, 128, 160, 192, 0, 0, 0, 0},
    {-64, 0, 32, 64, 96, 128, 160, 0, 0, 0, 0, 0},
    {-96, 0, 32, 64, 96, 128, 160, 0, 0, 0, 0, 0},
    {-105, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static s32 f_TestBanner = 0;
static Boolean f_IsFlipMessageLeaving = INITIAL_IS_FLIP_MESSAGE_LEAVING;
static s32 f_FlipMessageFrame = FLIP_MESSAGE_FRAME_NONE;
static MessageId f_FlipMessageId = 0;
static s32 f_FlipMessageHalfWidth = 0;
static s32 f_FlipMessageHalfHeight = 0;
static Vtx f_FlipMessageBoardVertices[QUAD_VERTEX_COUNT] = {
    {{{-72, 12, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-72, -12, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{72, -12, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{72, 12, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static s32 f_MovieDrawCount = 0;
static s32 f_MovieTick = 0;
static Movie f_CurrentMovie = MOVIE_NONE;
static s32 f_MovieBackBuffer = 0;
static s32 f_MovieFrame = 0;
static AssetId f_ToadMovieFrameAssets[] =
    {0x92D, 0x92E, 0x92F, 0x930, 0x931, 0x932, 0x933, 0x934, 0x935, 0x936, 0x937, 0x938};
static AssetId f_LakituMovieFrameAssets[] =
    {0x64E, 0x64F, 0x650, 0x651, 0x652, 0x653, 0x654, 0x655, 0x656, 0x657, 0x658, 0x659, 0x65A};
static AssetId f_MontyMoleMovieFrameAssets[] = {
    0x915,
    0x916,
    0x917,
    0x918,
    0x919,
    0x91A,
    0x91B,
    0x91A,
    0x919,
    0x91C,
    0x91D,
    0x91E,
    0x919,
    0x918,
    0x917,
    0x916,
    0x915,
};
static AssetId f_GoombaMovieFrameAssets[] =
    {0x920, 0x921, 0x922, 0x923, 0x924, 0x924, 0x925, 0x926, 0x927, 0x928, 0x929};
static Gfx f_MovieImageSetupDisplayList[] = {
    gsDPPipeSync(),
    gsDPPipeSync(),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsDPPipeSync(),
    gsSPTexture(0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON),
    gsDPPipeSync(),
    gsDPSetAlphaCompare(G_AC_THRESHOLD),
    gsDPSetBlendColor(0xFF, 0xFF, 0xFF, 0x01),
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_RGBA16),
    gsDPPipeSync(),
    gsDPSetTextureFilter(G_TF_BILERP),
    gsDPPipeSync(),
    gsDPSetTexturePersp(G_TP_NONE),
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_RGBA16),
    gsSPEndDisplayList(),
    gsDPPipeSync(),
    gsDPSetAlphaDither(G_AD_DISABLE),
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2),
};
static Vtx f_ReadyGoVertices[12] = {
    {{{-76, 28, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{76, 28, 0}, 0, {0x1300, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-76, 16, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{76, 16, 0}, 0, {0, 0x800}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-76, 4, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{76, 4, 0}, 0, {0, 0x800}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-76, -8, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{76, -8, 0}, 0, {0, 0x800}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-76, -20, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{76, -20, 0}, 0, {0, 0x800}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-76, -32, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{76, -32, 0}, 0, {0, 0x800}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Gfx f_MovieSetupDisplayList[] = {
    gsDPPipeSync(),
    gsDPPipeSync(),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_DECALRGBA, G_CC_DECALRGBA),
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2),
    gsSPTexture(0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON),
    gsDPPipeSync(),
    gsDPSetAlphaCompare(G_AC_NONE),
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_RGBA16),
    gsDPPipeSync(),
    gsDPSetTextureFilter(G_TF_BILERP),
    gsDPPipeSync(),
    gsDPSetTexturePersp(G_TP_NONE),
    gsDPPipeSync(),
    gsDPSetDepthSource(G_ZS_PRIM),
    gsDPSetPrimDepth(16415, 0),
    gsSPEndDisplayList(),
};
static Vtx f_ContactSpinVertices[QUAD_VERTEX_COUNT] = {
    {{{-25, 25, 0}, 0, {0, 0}, {0xFF, 0x7F, 0x00, 0xFF}}},
    {{{-25, -15, 0}, 0, {0, 0x7FF}, {0xFF, 0xFF, 0xBF, 0xFF}}},
    {{{15, -15, 0}, 0, {0x7FF, 0x7FF}, {0xFF, 0x7F, 0x00, 0xFF}}},
    {{{15, 25, 0}, 0, {0x7FF, 0}, {0xFF, 0xFF, 0xBF, 0xFF}}},
};
static Vtx f_ContactVertices[2] = {
    {{{-12, 0, 0}, 0, {0, 0}, {0xFF, 0x7F, 0x00, 0xFF}}},
    {{{12, 0, 0}, 0, {0xC00, 0}, {0xFF, 0xFF, 0xBF, 0xFF}}},
};
ContactEffect g_ContactEffect = CONTACT_EFFECT_NONE;
static Boolean f_IsContactEffectFading = FALSE;
static ContactStyle f_ContactStyle = 0;
#define CONTACT_RIBBON_FIRST_SEGMENTS 16
#define CONTACT_RIBBON_RELOAD_SEGMENTS 15
#define CONTACT_RIBBON_RELOAD_PASSES 2
#define ContactRunStart(pass, run) \
    (CONTACT_RIBBON_FIRST_SEGMENTS + ((run) * CONTACT_RIBBON_RELOAD_SEGMENTS) + \
        ((pass) * 2 * CONTACT_RIBBON_RELOAD_SEGMENTS))
#define CONTACT_RIBBON_SEGMENT_COUNT ContactRunStart(CONTACT_RIBBON_RELOAD_PASSES, 0)
#define ContactStepT(texels) (TexelCoordinate(texels) / CONTACT_RIBBON_SEGMENT_COUNT)
static s32 f_ContactTextureS = 0;
static s32 f_ContactTextureT = 0;
static Mtx f_UnusedRoundEventsIdentity UNUSED = IDENTITY_MTX;
static s32 f_ContactTrailHead = 0;
static Degrees f_ContactSpinAngle = 0.0f;
static PathPoint f_WindFlyGuyPath[] = {
    {-128.0f, 214.0f, 0.0f},
    {1e+02f, 215.0f, 0.0f},
    {1.6e+02f, 1.9e+02f, 0.0f},
    {1.8e+02f, 1.5e+02f, 0.0f},
    {1.8e+02f, 1.2e+02f, 0.0f},
    {1.7e+02f, 1e+02f, 0.0f},
    {1.5e+02f, 85.0f, 0.0f},
    {1.3e+02f, 8e+01f, 0.0f},
    {115.0f, 85.0f, 0.0f},
    {1e+02f, 1e+02f, 0.0f},
    {95.0f, 1.2e+02f, 0.0f},
    {1e+02f, 1.5e+02f, 0.0f},
    {1.2e+02f, 1.8e+02f, 0.0f},
    {145.0f, 195.0f, 0.0f},
    {2e+02f, 2e+02f, 0.0f},
    {285.0f, 1.9e+02f, 0.0f},
    {3.3e+02f, 1.8e+02f, 0.0f},
    {3.6e+02f, 175.0f, 0.0f},
    {3.8e+02f, 1.7e+02f, 0.0f},
};
static PathPoint f_WindFlyGuyExit = {3.8e+02f, 1.7e+02f, 0.0f};
static s32 f_WindFlyGuyFrame = FLY_GUY_HIDDEN;
static WideAssetId f_WindFlyGuyAsset = ASSET_FLY_GUY_IMAGE;
static s32 f_UnusedWordAfterFlyGuyAsset UNUSED = 0;
static s32 f_WindStreakHead = 0;

static Mtx f_FlipMessageMatrices[MATRIX_BUFFER_COUNT];
static Mtx f_ReadyGoMatrices[2];
static Mtx f_ContactRibbonMatrices[DRAW_BUFFER_COUNT];
static Mtx f_ContactSpinMatrices[MATRIX_BUFFER_COUNT];
static PathPoint f_WindFlyGuyLastPosition;
static Mtx f_BannerMatrices[MATRIX_BUFFER_COUNT];
static u8 f_UnusedBufferAfterBannerMatrices[0xD0] UNUSED;

BannerLetter f_ActiveBannerLetters[BANNER_LETTER_SLOT_COUNT];
u8 *f_MovieBuffer;
Mtx f_ContactMatrices[CONTACT_MATRIX_COUNT];
LakituFlight f_Lakitu;
LakituVelocity f_LakituVelocity;

void DrawContestWinnerMovie(Gfx **displayListHead);
void DrawDriveRecord(Gfx **displayListHead);
void DrawPuttCaption(Gfx **displayListHead);
void DrawReadyGoMovie(Gfx **displayListHead);
void DrawMontyMoleMovie(Gfx **displayListHead);
void DrawGoombaMovie(Gfx **displayListHead);
void DrawLakituMovie(Gfx **displayListHead);
void DrawToadMovie(Gfx **displayListHead);

void AllocMovieBuffer(void) {
    EncodedAsset unused UNUSED;

    f_CurrentBanner = BANNER_NONE;
    f_IsMovieBufferAllocated = TRUE;
    f_MovieBuffer = AllocMemory(MOVIE_BUFFER_SIZE);
    f_LoadedBanner = BANNER_NONE;
}

void FreeMovieBuffer(void) {
    f_IsMovieBufferAllocated = FALSE;
    FreeMemory(&f_MovieBuffer);
}

void StartBanner(s32 banner) {
    EncodedAsset asset;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;
    s32 letterIndex;

    if (f_IsMovieBufferAllocated != TRUE) {
        return;
    }

    if (banner == BANNER_HIDE_NICE_SHOT) {
        if (f_CurrentBanner == BANNER_NICE_SHOT) {
            f_BannerFrame = BANNER_FRAME_NONE;
        }

        return;
    }

    if (banner == BANNER_NONE) {
        f_BannerFrame = BANNER_FRAME_NONE;
        return;
    }

    f_CurrentBanner = banner;
    f_BannerFrame = 0;
    if (banner > BANNER_STROKES_OVER_PAR) {
        f_BannerNumber = banner - BANNER_STROKES_OVER_PAR;
        f_CurrentBanner = BANNER_NUMBERED_OVER_PAR;
        if (f_LoadedBanner < BANNER_NUMBERED_OVER_PAR || f_LoadedBanner == BANNER_NONE) {
            OpenEncodedAsset(f_Banners[BANNER_NUMBERED_OVER_PAR].imageAsset, &asset);
            ReadEncodedAsset(f_MovieBuffer, &asset);
        }

        f_LoadedBanner = f_CurrentBanner;

        if (f_BannerNumber < 10) {
            f_BannerNumberLetterCount = 2;
        }
        else if (f_BannerNumber < 100) {
            f_BannerNumberLetterCount = 3;
        }
        else {
            f_BannerNumberLetterCount = 4;
        }

        for (letterIndex = 0; letterIndex != f_BannerNumberLetterCount; letterIndex++) {
            f_ActiveBannerLetters[letterIndex].x = (letterIndex * BANNER_LETTER_SPACING) -
                ((f_BannerNumberLetterCount - 1) * (BANNER_LETTER_SPACING / 2));
            f_ActiveBannerLetters[letterIndex].y = 0.0f;
            f_ActiveBannerLetters[letterIndex].unusedLetterValue = 0;
            f_ActiveBannerLetters[letterIndex].scale = 1.0f;
            rotation[AXIS_X] = 0;
            rotation[AXIS_Y] = 0;
            rotation[AXIS_Z] = 0;
            translation[AXIS_X] = f_ActiveBannerLetters[letterIndex].x;
            translation[AXIS_Y] = f_ActiveBannerLetters[letterIndex].y;
            translation[AXIS_Z] = BANNER_Z;
            scale[AXIS_X] = f_ActiveBannerLetters[letterIndex].scale;
            scale[AXIS_Y] = f_ActiveBannerLetters[letterIndex].scale;
            scale[AXIS_Z] = f_ActiveBannerLetters[letterIndex].scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(
                matrix,
                &f_ActiveBannerLetters[letterIndex].matrices[g_MatrixBufferIndex]
            );
        }
    }

    if ((f_LoadedBanner != f_CurrentBanner && f_CurrentBanner < BANNER_NUMBERED_OVER_PAR) ||
        f_LoadedBanner == BANNER_NONE) {
        OpenEncodedAsset(f_Banners[f_CurrentBanner].imageAsset, &asset);
        ReadEncodedAsset(f_MovieBuffer, &asset);
        f_LoadedBanner = f_CurrentBanner;
        for (letterIndex = 0; letterIndex != f_Banners[f_CurrentBanner].letterCount;
            letterIndex++) {
            f_ActiveBannerLetters[letterIndex].x =
                (f32)f_BannerLayouts[f_CurrentBanner][BANNER_LAYOUT_FIRST_LETTER + letterIndex] +
                (f32)f_BannerLayouts[f_CurrentBanner][BANNER_LAYOUT_LEFT];
            f_ActiveBannerLetters[letterIndex].y = 0.0f;
            f_ActiveBannerLetters[letterIndex].unusedLetterValue = 0;
            f_ActiveBannerLetters[letterIndex].scale = 1.0f;
            rotation[AXIS_X] = 0;
            rotation[AXIS_Y] = 0;
            rotation[AXIS_Z] = 0;
            translation[AXIS_X] = f_ActiveBannerLetters[letterIndex].x;
            translation[AXIS_Y] = f_ActiveBannerLetters[letterIndex].y;
            translation[AXIS_Z] = BANNER_Z;
            scale[AXIS_X] = f_ActiveBannerLetters[letterIndex].scale;
            scale[AXIS_Y] = f_ActiveBannerLetters[letterIndex].scale;
            scale[AXIS_Z] = f_ActiveBannerLetters[letterIndex].scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(
                matrix,
                &f_ActiveBannerLetters[letterIndex].matrices[g_MatrixBufferIndex]
            );
        }
    }
}

void DrawNumberBanner(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 letterIndex;
    s32 imageOffset;

    sprintf(g_PrintBuffer, "%d", (int)f_BannerNumber);
    for (letterIndex = 0; letterIndex != f_BannerNumberLetterCount; letterIndex++) {
        if (letterIndex == 0) {
            imageOffset = BANNER_PLUS_IMAGE * BANNER_LETTER_IMAGE_SIZE;
        }
        else {
            imageOffset = ((g_PrintBuffer[letterIndex - 1] - '0') * BANNER_LETTER_IMAGE_SIZE) +
                (BANNER_FIRST_DIGIT_IMAGE * BANNER_LETTER_IMAGE_SIZE);
        }

        LoadTextureBlock(
            &gfx,
            f_MovieBuffer + (imageOffset + BANNER_LETTER_IMAGES_OFFSET),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            BANNER_LETTER_IMAGE_WIDTH,
            BANNER_LETTER_IMAGE_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_ActiveBannerLetters[letterIndex].matrices[g_MatrixBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gSPVertex(gfx++, f_BannerLetterVertices, QUAD_VERTEX_COUNT, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawBannerLetters(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 letterIndex;

    for (letterIndex = 0; letterIndex != f_Banners[f_CurrentBanner].letterCount; letterIndex++) {
        LoadTextureBlock(
            &gfx,
            f_MovieBuffer + BANNER_LETTER_IMAGES_OFFSET + (letterIndex * BANNER_LETTER_IMAGE_SIZE),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            BANNER_LETTER_IMAGE_WIDTH,
            BANNER_LETTER_IMAGE_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_BannerMatrices[g_MatrixBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_ActiveBannerLetters[letterIndex].matrices[g_MatrixBufferIndex]),
            G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
        );
        gSPVertex(gfx++, f_BannerLetterVertices, QUAD_VERTEX_COUNT, 0);
        gSP2Triangles(gfx++, 0, 1, 3, 0, 1, 2, 3, 0);
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawLetterBannerExtras(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    *displayListHead = gfx;
}

#define GAME_FLAG_TEST_BANNERS 0x6D
#define TEST_BANNER_LAST 37

#define BANNER_LAST_FRAME 200
#define BANNER_END_FRAME 60
#define BANNER_SOUND_FRAME 10
#define BANNER_SOUND_CHANNEL 13
#define BANNER_SOUND_VOLUME 63
#define BANNER_Y 64.0f
#define BANNER_ZOOM_FRAMES 20
#define BANNER_ZOOM_PER_FRAME 0.3f
#define BANNER_ZOOM_SMALLEST 1.2f
#define BANNER_ZOOM_STAY_SMALLEST 1.0f
#define BANNER_SPIN_GROWTH_PER_FRAME 0.1f
#define BANNER_SLIDE_FRAMES 10
#define BANNER_SLIDE_PER_FRAME 36.0f
#define BANNER_WOBBLE_FRAMES 20
#define BANNER_WOBBLE_DEPTH 0.4f
#define BANNER_WOBBLE_DEPTH_LOSS 0.05f
#define BANNER_WOBBLE_SMALLEST_DEPTH 0.1f
#define BANNER_WOBBLE_TURN_PER_FRAME (M_PI / 10)
#define BANNER_ALPHA_MAX 255
#define BANNER_BLEND_ALPHA 1

void DrawBanner(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;
    s32 alpha;
    f32 letterScale;
    Radians angle;
    f32 slide;
    f32 wobbleDepth;
    s32 letterIndex;

    angle = 0.0f;
#ifdef AVOID_UB
    alpha = BANNER_ALPHA_MAX;
#endif
    if (IsGameFlagSet(GAME_FLAG_TEST_BANNERS)) {
        if ((g_ControllerStates[0].pressedButtons & L_JPAD) && f_TestBanner > 0) {
            f_TestBanner--;
        }

        if ((g_ControllerStates[0].pressedButtons & R_JPAD) && f_TestBanner < TEST_BANNER_LAST) {
            f_TestBanner++;
        }

        if (g_ControllerStates[0].pressedButtons & D_JPAD) {
            StartBanner(f_TestBanner);
        }
    }

    if (f_CurrentBanner == BANNER_NONE || f_BannerFrame == BANNER_FRAME_NONE) {
        return;
    }

    f_BannerFrame++;
    if (f_BannerFrame > BANNER_LAST_FRAME) {
        f_BannerFrame = BANNER_LAST_FRAME;
    }

    if (f_BannerFrame == BANNER_SOUND_FRAME) {
        PlaySound(SOUND_BANNER, BANNER_SOUND_CHANNEL, BANNER_SOUND_VOLUME);
    }

    switch (f_Banners[f_CurrentBanner].animation) {
        case BANNER_ANIMATION_ZOOM:
            letterScale = (BANNER_ZOOM_FRAMES - f_BannerFrame) * BANNER_ZOOM_PER_FRAME;
            if (letterScale < BANNER_ZOOM_SMALLEST) {
                letterScale = BANNER_ZOOM_SMALLEST;
            }

            alpha = f_BannerFrame * f_BannerFrame;
            if (alpha > BANNER_ALPHA_MAX) {
                alpha = BANNER_ALPHA_MAX;
            }

            if (f_BannerFrame == BANNER_END_FRAME) {
                f_BannerFrame = BANNER_FRAME_NONE;
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
            ConvertMatrixToFixed(matrix, &f_BannerMatrices[g_MatrixBufferIndex]);
            break;
        case BANNER_ANIMATION_SPIN:
            letterScale = f_BannerFrame * BANNER_SPIN_GROWTH_PER_FRAME;
            if (letterScale > 1.0f) {
                letterScale = 1.0f;
            }

            alpha = f_BannerFrame * f_BannerFrame;
            if (alpha > BANNER_ALPHA_MAX) {
                alpha = BANNER_ALPHA_MAX;
            }

            angle = f_BannerFrame * BANNER_SPIN_GROWTH_PER_FRAME * FULL_TURN_RADIANS;
            if (angle > FULL_TURN_RADIANS) {
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
            ConvertMatrixToFixed(matrix, &f_BannerMatrices[g_MatrixBufferIndex]);
            break;
        case BANNER_ANIMATION_SLIDE:
            slide = (BANNER_SLIDE_FRAMES - f_BannerFrame) * BANNER_SLIDE_PER_FRAME;
            if (slide < 0.0f) {
                slide = 0.0f;
            }

            if (f_BannerFrame >= BANNER_SLIDE_FRAMES &&
                f_BannerFrame < BANNER_SLIDE_FRAMES + BANNER_WOBBLE_FRAMES) {
                wobbleDepth = BANNER_WOBBLE_DEPTH -
                    ((f_BannerFrame - BANNER_WOBBLE_FRAMES) * BANNER_WOBBLE_DEPTH_LOSS);
                if (wobbleDepth < BANNER_WOBBLE_SMALLEST_DEPTH) {
                    wobbleDepth = BANNER_WOBBLE_SMALLEST_DEPTH;
                }

                letterScale =
                    ((sinf((f_BannerFrame - BANNER_WOBBLE_FRAMES) * BANNER_WOBBLE_TURN_PER_FRAME) +
                         1.0f) *
                        wobbleDepth) +
                    (1.0f - wobbleDepth);
            }
            else {
                letterScale = 1.0f;
            }

            slide += f_BannerLayouts[f_CurrentBanner][BANNER_LAYOUT_LEFT] * (1.0f - letterScale);
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
            ConvertMatrixToFixed(matrix, &f_BannerMatrices[g_MatrixBufferIndex]);
            letterScale = 1.0f;
            if (f_BannerFrame == BANNER_END_FRAME) {
                f_BannerFrame = BANNER_FRAME_NONE;
            }

            break;
#ifdef AVOID_UB
        default:
#endif
        case BANNER_ANIMATION_ZOOM_STAY:
            letterScale = (BANNER_ZOOM_FRAMES - f_BannerFrame) * BANNER_ZOOM_PER_FRAME;
            if (letterScale < BANNER_ZOOM_STAY_SMALLEST) {
                letterScale = BANNER_ZOOM_STAY_SMALLEST;
            }

            alpha = f_BannerFrame * f_BannerFrame;
            if (alpha > BANNER_ALPHA_MAX) {
                alpha = BANNER_ALPHA_MAX;
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
            ConvertMatrixToFixed(matrix, &f_BannerMatrices[g_MatrixBufferIndex]);
            break;
    }

    if (f_CurrentBanner < BANNER_NUMBERED_OVER_PAR) {
        for (letterIndex = 0; letterIndex != f_Banners[f_CurrentBanner].letterCount;
            letterIndex++) {
            f_ActiveBannerLetters[letterIndex].x =
                ((f32)f_BannerLayouts[f_CurrentBanner][BANNER_LAYOUT_FIRST_LETTER + letterIndex] +
                    (f32)f_BannerLayouts[f_CurrentBanner][BANNER_LAYOUT_LEFT]) *
                letterScale;
            f_ActiveBannerLetters[letterIndex].y = 0.0f;
            f_ActiveBannerLetters[letterIndex].unusedLetterValue = 0;
            f_ActiveBannerLetters[letterIndex].scale = letterScale;
            rotation[AXIS_X] = 0;
            rotation[AXIS_Y] = 0;
            rotation[AXIS_Z] = 0.0f;
            translation[AXIS_X] = f_ActiveBannerLetters[letterIndex].x;
            translation[AXIS_Y] = f_ActiveBannerLetters[letterIndex].y;
            translation[AXIS_Z] = BANNER_Z;
            scale[AXIS_X] = f_ActiveBannerLetters[letterIndex].scale;
            scale[AXIS_Y] = f_ActiveBannerLetters[letterIndex].scale;
            scale[AXIS_Z] = f_ActiveBannerLetters[letterIndex].scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(
                matrix,
                &f_ActiveBannerLetters[letterIndex].matrices[g_MatrixBufferIndex]
            );
        }
    }
    else {
        for (letterIndex = 0; letterIndex != f_BannerNumberLetterCount; letterIndex++) {
            f_ActiveBannerLetters[letterIndex].x =
                ((letterIndex * BANNER_LETTER_SPACING) -
                    (f_BannerNumberLetterCount * (BANNER_LETTER_SPACING / 2))) *
                letterScale;
            f_ActiveBannerLetters[letterIndex].y = 0.0f;
            f_ActiveBannerLetters[letterIndex].unusedLetterValue = 0;
            f_ActiveBannerLetters[letterIndex].scale = letterScale;
            rotation[AXIS_X] = 0;
            rotation[AXIS_Y] = 0;
            rotation[AXIS_Z] = 0.0f;
            translation[AXIS_X] = f_ActiveBannerLetters[letterIndex].x;
            translation[AXIS_Y] = f_ActiveBannerLetters[letterIndex].y;
            translation[AXIS_Z] = BANNER_Z;
            scale[AXIS_X] = f_ActiveBannerLetters[letterIndex].scale;
            scale[AXIS_Y] = f_ActiveBannerLetters[letterIndex].scale;
            scale[AXIS_Z] = f_ActiveBannerLetters[letterIndex].scale;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(
                matrix,
                &f_ActiveBannerLetters[letterIndex].matrices[g_MatrixBufferIndex]
            );
        }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    switch (f_Banners[f_CurrentBanner].animation) {
        case BANNER_ANIMATION_ZOOM:
        case BANNER_ANIMATION_ZOOM_STAY:
            gDPSetCombineLERP(
                gfx++,
                SHADE,
                TEXEL0,
                PRIMITIVE_ALPHA,
                TEXEL0,
                ENVIRONMENT,
                0,
                TEXEL0,
                0,
                SHADE,
                TEXEL0,
                PRIMITIVE_ALPHA,
                TEXEL0,
                ENVIRONMENT,
                0,
                TEXEL0,
                0
            );
            if (alpha == BANNER_ALPHA_MAX) {
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
            }
            else {
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
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(f_MovieBuffer + MOVIE_IMAGE_PALETTE_OFFSET));
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&g_ScreenOrthoMatrix),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    if (f_CurrentBanner < BANNER_NUMBERED_OVER_PAR) {
        DrawBannerLetters(&gfx);
        DrawLetterBannerExtras(&gfx);
    }
    else {
        DrawNumberBanner(&gfx);
    }

    *displayListHead = gfx;
}

void ShowFlipMessage(MessageId messageId) {
    s32 halfSize;
    s32 halfWidth;
    s32 halfHeight;

    f_FlipMessageId = messageId;
    if (messageId == FLIP_MESSAGE_RESET) {
        f_IsFlipMessageLeaving = FALSE;
        f_FlipMessageFrame = FLIP_MESSAGE_FRAME_NONE;
        return;
    }

    if (messageId == FLIP_MESSAGE_LEAVE) {
        f_IsFlipMessageLeaving = TRUE;
        return;
    }

    f_IsFlipMessageLeaving = FALSE;
    f_FlipMessageFrame = 0;
    halfSize = PlaceMessageGlyphGroup(FLIP_MESSAGE_GLYPH_GROUP, FONT_SIZE_LARGE, 0, 0, messageId);
    halfWidth = HalfSizeWidth(halfSize);
    f_FlipMessageHalfWidth = halfWidth;
    halfHeight = HalfSizeHeight(halfSize);
    f_FlipMessageHalfHeight = halfHeight;
    f_FlipMessageBoardVertices[0].v.ob[AXIS_X] = -halfWidth - FLIP_MESSAGE_BOARD_MARGIN_X;
    f_FlipMessageBoardVertices[1].v.ob[AXIS_X] = -halfWidth - FLIP_MESSAGE_BOARD_MARGIN_X;
    f_FlipMessageBoardVertices[2].v.ob[AXIS_X] = halfWidth + FLIP_MESSAGE_BOARD_MARGIN_X;
    f_FlipMessageBoardVertices[3].v.ob[AXIS_X] = halfWidth + FLIP_MESSAGE_BOARD_MARGIN_X;
    f_FlipMessageBoardVertices[0].v.ob[AXIS_Y] = halfHeight + FLIP_MESSAGE_BOARD_MARGIN_Y;
    f_FlipMessageBoardVertices[3].v.ob[AXIS_Y] = halfHeight + FLIP_MESSAGE_BOARD_MARGIN_Y;
    f_FlipMessageBoardVertices[1].v.ob[AXIS_Y] = -halfHeight - FLIP_MESSAGE_BOARD_MARGIN_Y;
    f_FlipMessageBoardVertices[2].v.ob[AXIS_Y] = -halfHeight - FLIP_MESSAGE_BOARD_MARGIN_Y;
    SetTextSlot(
        FLIP_MESSAGE_X,
        FLIP_MESSAGE_Y,
        f_FlipMessageId,
        TEXT_SLOT_LARGE_FONT,
        FLIP_MESSAGE_TEXT_SLOT,
        TEXT_FLAG_CENTERED | TEXT_FLAG_DRAWN_BY_OWNER,
        127,
        255,
        127,
        OPAQUE_ALPHA
    );
}

void DrawFlipMessage(Gfx **displayListHead) {
    Vec3 scale;
    Vec3 translation;
    Vec3 rotation;
    Mat4 matrix;
    Mat4 hingeMatrix;
    Gfx *gfx = *displayListHead;
    Radians angle;
    f32 size;

    if (f_FlipMessageFrame != FLIP_MESSAGE_FRAME_NONE) {
        if (!f_IsFlipMessageLeaving) {
            if (f_FlipMessageFrame < FLIP_MESSAGE_OPEN_FRAME) {
                f_FlipMessageFrame++;
            }
        }
        else if (f_FlipMessageFrame > 0) {
            f_FlipMessageFrame--;
        }

        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
        gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_PERSP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
        gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, FLIP_MESSAGE_BOARD_ALPHA);
        gDPPipeSync(gfx++);
        angle = DegreesToRadians(
            (f_FlipMessageFrame * FLIP_MESSAGE_DEGREES_PER_FRAME) + FLIP_MESSAGE_START_DEGREES
        );
        if (angle > FULL_TURN_RADIANS) {
            angle = FULL_TURN_RADIANS;
        }

        size = f_FlipMessageFrame * FLIP_MESSAGE_GROWTH_PER_FRAME;
        if (size > 1.0f) {
            size = 1.0f;
        }

        scale[AXIS_X] = size;
        scale[AXIS_Y] = size;
        scale[AXIS_Z] = size;
        rotation[AXIS_X] = 0;
        rotation[AXIS_Y] = 0;
        rotation[AXIS_Z] = -angle;
        translation[AXIS_X] = -f_FlipMessageHalfWidth;
        translation[AXIS_Y] = FLIP_MESSAGE_BOARD_Y;
        translation[AXIS_Z] = FLIP_MESSAGE_BOARD_Z;
        BuildTransformMatrix(matrix, rotation, translation, scale);
        guTranslateF(hingeMatrix, f_FlipMessageHalfWidth, -FLIP_MESSAGE_BOARD_Y, 0);
        guMtxCatF(hingeMatrix, matrix, matrix);
        ConvertMatrixToFixed(matrix, &f_FlipMessageMatrices[g_MatrixBufferIndex]);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_FlipMessageMatrices[g_MatrixBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gSPVertex(gfx++, f_FlipMessageBoardVertices, QUAD_VERTEX_COUNT, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        if (f_FlipMessageFrame == FLIP_MESSAGE_OPEN_FRAME) {
            SetUpTextDrawing(&gfx);
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 240, OPAQUE_ALPHA);
            DrawLargeTextSlot(&gfx, FLIP_MESSAGE_TEXT_SLOT);
        }
        else {
            SetUpPerspectiveTextDrawing(&gfx);
            DrawGlyphGroup(&gfx, FLIP_MESSAGE_GLYPH_GROUP);
        }
    }

    *displayListHead = gfx;
}

Boolean IsMovieDone(void) {
    return f_CurrentMovie == MOVIE_NONE;
}

void StartMovie(Movie movie) {
    EncodedAsset asset;
    AssetFile file;

    osSyncPrintf("MOVIE %d \n", movie);
    if (f_IsMovieBufferAllocated != TRUE) {
        return;
    }

    f_CurrentMovie = movie;
    if (movie == MOVIE_NONE) {
        return;
    }

    f_LoadedBanner = BANNER_NONE;
    f_CurrentBanner = BANNER_NONE;
    f_MovieDrawCount = 0;
    f_MovieTick = 0;
    f_MovieBackBuffer = 0;

    f_MovieFrame = 1;
    switch (movie) {
        case MOVIE_NONE:
            break;
        case MOVIE_TOAD_OUT_OF_BOUNDS:

            OpenAssetFile(ASSET_TOAD_MOVIE_PALETTE, &file);
            ReadAssetFile(f_MovieBuffer, GetAssetFileSize(&file), &file);
            OpenEncodedAsset(f_ToadMovieFrameAssets[0], &asset);
            ReadEncodedAsset(f_MovieBuffer + TOAD_FRAME_OFFSET, &asset);
            OpenEncodedAsset(f_ToadMovieFrameAssets[0], &asset);
            ReadEncodedAsset(f_MovieBuffer + TOAD_BACK_FRAME_OFFSET, &asset);
            break;
        case MOVIE_LAKITU:
            OpenEncodedAsset(ASSET_LAKITU, &asset);
            ReadEncodedAsset(f_MovieBuffer, &asset);
            OpenEncodedAsset(ASSET_LAKITU, &asset);
            ReadEncodedAsset(f_MovieBuffer + LAKITU_BACK_IMAGE_OFFSET, &asset);
            f_Lakitu.x = LAKITU_START_X;
            f_Lakitu.y = LAKITU_START_Y;
            f_Lakitu.scale = LAKITU_START_SCALE;
            break;
        case MOVIE_UNUSED_MONTY_MOLE_A:
        case MOVIE_UNUSED_MONTY_MOLE_B:
        case MOVIE_UNUSED_MONTY_MOLE_C:
        case MOVIE_MONTY_MOLE:
            OpenAssetFile(ASSET_MONTY_MOLE_MOVIE_PALETTE, &file);
            ReadAssetFile(f_MovieBuffer + MOVIE_PALETTE_OFFSET, GetAssetFileSize(&file), &file);
            OpenEncodedAsset(f_MontyMoleMovieFrameAssets[0], &asset);
            ReadEncodedAsset(f_MovieBuffer + MOVIE_FRAME_OFFSET, &asset);
            break;
        case MOVIE_GOOMBA:
            OpenAssetFile(ASSET_GOOMBA_MOVIE_PALETTE, &file);
            ReadAssetFile(f_MovieBuffer + MOVIE_PALETTE_OFFSET, GetAssetFileSize(&file), &file);
            OpenEncodedAsset(f_GoombaMovieFrameAssets[0], &asset);
            ReadEncodedAsset(f_MovieBuffer + MOVIE_FRAME_OFFSET, &asset);
            break;
        case MOVIE_DRIVE_RECORD:
            OpenEncodedAsset(ASSET_DRIVE_RECORD_TEXT, &asset);
            ReadEncodedAsset(f_MovieBuffer, &asset);
            break;
        case MOVIE_LONGEST_DRIVE:
            OpenEncodedAsset(ASSET_LONGEST_DRIVE_TEXT, &asset);
            ReadEncodedAsset(f_MovieBuffer, &asset);
            break;
        case MOVIE_NEAREST_PIN:
            OpenEncodedAsset(ASSET_NEAREST_PIN_TEXT, &asset);
            ReadEncodedAsset(f_MovieBuffer, &asset);
            break;
        case MOVIE_ALBATROSS_PUTT:
            OpenEncodedAsset(ASSET_ALBATROSS_PUTT_TEXT, &asset);
            ReadEncodedAsset(f_MovieBuffer, &asset);
            break;
        case MOVIE_EAGLE_PUTT:
            OpenEncodedAsset(ASSET_EAGLE_PUTT_TEXT, &asset);
            ReadEncodedAsset(f_MovieBuffer, &asset);
            break;
        case MOVIE_BIRDIE_PUTT:
            OpenEncodedAsset(ASSET_BIRDIE_PUTT_TEXT, &asset);
            ReadEncodedAsset(f_MovieBuffer, &asset);
            break;
        case MOVIE_SINK_TO_WIN:
            OpenEncodedAsset(ASSET_SINK_TO_WIN_TEXT, &asset);
            ReadEncodedAsset(f_MovieBuffer, &asset);
            break;
        case MOVIE_MISS_AND_LOSE:
            OpenEncodedAsset(ASSET_MISS_AND_LOSE_TEXT, &asset);
            ReadEncodedAsset(f_MovieBuffer, &asset);
            break;
        case MOVIE_SINK_TO_TIE:
            OpenEncodedAsset(ASSET_SINK_TO_TIE_TEXT, &asset);
            ReadEncodedAsset(f_MovieBuffer, &asset);
            break;
        case MOVIE_READY_GO:
            OpenEncodedAsset(ASSET_READY_TEXT, &asset);
            ReadEncodedAsset(f_MovieBuffer, &asset);
            OpenEncodedAsset(ASSET_GO_TEXT, &asset);
            ReadEncodedAsset(f_MovieBuffer + GO_TEXT_OFFSET, &asset);
            break;
    }
}

Boolean IsMoviePastCue(void) {
    return f_MovieFrame >= MOVIE_CUE_FRAME;
}

void DrawMovie(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

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

    switch (f_CurrentMovie) {
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

    if (f_MovieDrawCount < MOVIE_DRAW_COUNT_MAX) {
        f_MovieDrawCount++;
    }

    *displayListHead = gfx;
}

#define WINNER_PALETTE_OFFSET 0x2788
#define WINNER_FACE_OFFSET 0x2988
#define WINNER_PLAQUE_OFFSET 0x2D88
#define WINNER_FACE_SIZE 32
#define WINNER_PLAQUE_MARGIN 8
#define WINNER_CENTER_X (SCREEN_WIDTH / 2)
#define WINNER_CENTER_Y 128
#define WINNER_GROWTH_MAX 4
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
#define CONTEST_IMAGE_CENTER_X (SCREEN_WIDTH / 2)
#define CONTEST_IMAGE_CENTER_Y 80
#define CONTEST_IMAGE_STRIPS_ABOVE_CENTER 4
#define CONTEST_CHEER_DRAW 26
#define WINNER_FIRST_FRAME 1
#define WINNER_LAST_FRAME 32
#define WINNER_SOUND_VOLUME 63
#define WINNER_SOUND_REVERB 30
#define WINNER_BLINK_FRAME 8

#define LoadWinnerFace(asset, frameIndex) \
    { \
        OpenEncodedAsset( \
            g_CharacterFirstFaceAssets[g_Players[GetContestWinner()].characterIndex] + \
                (frameIndex), \
            (asset) \
        ); \
        ReadEncodedAsset(f_MovieBuffer + WINNER_FACE_OFFSET, (asset)); \
    }

void DrawContestWinnerMovie(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    EncodedAsset asset;
    AssetFile file;
    u32 width;
    s32 height;
    s32 stripCount;
    s32 halfWidth;
    s32 stripHeight;
    s32 slide;
    s32 centerX;
    s32 bottom;
    s32 stripIndex;
    s32 frame;
    s32 growth;
    s32 plaqueHalfSize;
    s32 texelStep;
    f32 alpha;

    width = ReadImageWidth(f_MovieBuffer);
    stripHeight = MOVIE_IMAGE_STRIP_HEIGHT;
    height = ReadImageHeight(f_MovieBuffer);
    stripCount = height / stripHeight;
    slide = 0;
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE_ALPHA,
        ENVIRONMENT,
        0,
        0,
        0,
        TEXEL0,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE_ALPHA,
        ENVIRONMENT,
        0,
        0,
        0,
        TEXEL0
    );
    halfWidth = width >> 1;
    if (f_MovieDrawCount == CONTEST_SLIDE_DRAW) {
        PlaySoundWithEffects(
            SOUND_SHRINKING_IMAGE,
            SHRINKING_IMAGE_SOUND_CHANNEL,
            SHRINKING_IMAGE_SOUND_VOLUME,
            CONTEST_SOUND_REVERB,
            CONTEST_PAN_START
        );
    }

    if (f_MovieDrawCount == CONTEST_CHEER_DRAW) {
        if (f_CurrentMovie == MOVIE_LONGEST_DRIVE) {
            PlayCharacterSound(
                g_Players[GetContestWinner()].characterIndex,
                GetContestWinner(),
                CHARACTER_SOUND_LONGEST_DRIVE,
                SOUND_FULL_SCALE,
                SOUND_FULL_SCALE
            );
        }
        else {
            PlayCharacterSound(
                g_Players[GetContestWinner()].characterIndex,
                GetContestWinner(),
                CHARACTER_SOUND_NEAREST_PIN,
                SOUND_FULL_SCALE,
                SOUND_FULL_SCALE
            );
        }
    }

    if (f_MovieDrawCount > CONTEST_SLIDE_DRAW && f_MovieDrawCount < CONTEST_PAN_DRAW_END) {
        SetSoundPan(
            SHRINKING_IMAGE_SOUND_CHANNEL,
            CONTEST_PAN_START - ((f_MovieDrawCount - CONTEST_SLIDE_DRAW) * CONTEST_PAN_STEP)
        );
    }

    if (f_MovieDrawCount < CONTEST_MEET_DRAW) {
        slide = CONTEST_START_SLIDE - (f_MovieDrawCount * CONTEST_SLIDE_PER_FRAME);
        if (slide < 0) {
            slide = 0;
        }

        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        gDPSetEnvColor(gfx++, 255, 255, 255, CONTEST_COPY_ALPHA);
        if (f_MovieDrawCount == 0) {
            OpenAssetFile(
                g_CharacterFirstPaletteFiles[g_Players[GetContestWinner()].characterIndex] +
                    g_Players[GetContestWinner()].paletteIndex,
                &file
            );
            ReadAssetFile(f_MovieBuffer + WINNER_PALETTE_OFFSET, GetAssetFileSize(&file), &file);
            LoadWinnerFace(&asset, 0);
            OpenEncodedAsset(ASSET_WOODEN_PLAQUE, &asset);
            ReadEncodedAsset(f_MovieBuffer + WINNER_PLAQUE_OFFSET, &asset);
        }
    }
    else if (f_MovieDrawCount < CONTEST_FLASH_END_DRAW) {
        alpha = 255.0f -
            (sinf((f_MovieDrawCount - CONTEST_MEET_DRAW) * CONTEST_TURN_PER_FRAME) *
                CONTEST_FLASH_DEPTH);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
        gDPSetEnvColor(gfx++, 255, 255, 255, OPAQUE_ALPHA);
    }
    else if (f_MovieDrawCount < CONTEST_SLIDE_OUT_DRAW) {
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        gDPSetEnvColor(gfx++, 255, 255, 255, OPAQUE_ALPHA);
    }
    else if (f_MovieDrawCount < CONTEST_END_DRAW) {
        slide = (CONTEST_SLIDE_OUT_DRAW - f_MovieDrawCount) * CONTEST_SLIDE_PER_FRAME;
    }
    else {
        f_CurrentMovie = MOVIE_NONE;
        halfWidth = 0;
    }

    gSPDisplayList(gfx++, f_MovieImageSetupDisplayList);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(f_MovieBuffer + MOVIE_IMAGE_PALETTE_OFFSET));
    if (f_MovieDrawCount < CONTEST_DRAW_COUNT && halfWidth != 0) {
        for (stripIndex = 0; stripIndex != stripCount; stripIndex++) {
            LoadTextureBlock(
                &gfx,
                f_MovieBuffer +
                    ((stripIndex * width * MOVIE_IMAGE_STRIP_HEIGHT) + MOVIE_IMAGE_PIXELS_OFFSET),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                width,
                MOVIE_IMAGE_STRIP_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            bottom = CONTEST_IMAGE_CENTER_Y + stripIndex * MOVIE_IMAGE_STRIP_HEIGHT;
            centerX = CONTEST_IMAGE_CENTER_X + slide;
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(centerX - halfWidth),
                QuarterPixels(bottom - (stripHeight * CONTEST_IMAGE_STRIPS_ABOVE_CENTER)),
                QuarterPixels(halfWidth + centerX),
                QuarterPixels(
                    bottom - (stripHeight * CONTEST_IMAGE_STRIPS_ABOVE_CENTER) +
                    MOVIE_IMAGE_STRIP_HEIGHT
                ),
                G_TX_RENDERTILE,
                0,
                0,
                (s32)((width >> 1) << TEXEL_STEP_SHIFT) / halfWidth,
                TexelRunStep(MOVIE_IMAGE_STRIP_HEIGHT, stripHeight)
            );
        }
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    if (f_MovieDrawCount > CONTEST_MEET_DRAW &&
        f_MovieDrawCount <= CONTEST_MEET_DRAW + WINNER_LAST_FRAME) {
        frame = f_MovieDrawCount - CONTEST_MEET_DRAW;
        if (frame == WINNER_BLINK_FRAME) {
            LoadWinnerFace(&asset, 1);
        }
        else if (frame == WINNER_BLINK_FRAME + 2) {
            LoadWinnerFace(&asset, 2);
        }
        else if (frame == WINNER_BLINK_FRAME + 4) {
            LoadWinnerFace(&asset, 3);
        }
        else if (frame == WINNER_BLINK_FRAME + 6) {
            LoadWinnerFace(&asset, 4);
        }

        if (frame == WINNER_FIRST_FRAME) {
            PlaySoundWithEffects(
                SOUND_READY,
                g_NextSoundChannel,
                WINNER_SOUND_VOLUME,
                WINNER_SOUND_REVERB,
                SOUND_FULL_SCALE
            );
        }

        if (frame == WINNER_SHRINK_FRAME) {
            PlaySoundWithEffects(
                SOUND_GO,
                g_NextSoundChannel,
                WINNER_SOUND_VOLUME,
                WINNER_SOUND_REVERB,
                SOUND_FULL_SCALE
            );
        }

        if (frame >= WINNER_SHRINK_FRAME) {
            frame = WINNER_SHRINK_FRAME + 3 - frame;
            if (frame <= 0) {
                frame = 1;
            }
        }
        else if (frame > WINNER_GROWTH_MAX) {
            frame = WINNER_GROWTH_MAX;
        }

        growth = frame * WINNER_GROWTH_STEP;
        LoadSmallPalette(&gfx, 0, ImageBody(f_MovieBuffer + WINNER_PLAQUE_OFFSET));
        LoadNibbleTextureBlock(
            &gfx,
            ImageTexels(f_MovieBuffer + WINNER_PLAQUE_OFFSET),
            G_IM_FMT_CI,
            WOODEN_PLAQUE_SIZE,
            WOODEN_PLAQUE_SIZE,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        plaqueHalfSize = growth + WINNER_PLAQUE_MARGIN;
        texelStep = TexelRunStep((WOODEN_PLAQUE_SIZE / 2), plaqueHalfSize);
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(WINNER_CENTER_X - plaqueHalfSize),
            QuarterPixels(WINNER_CENTER_Y - plaqueHalfSize),
            QuarterPixels(WINNER_CENTER_X + plaqueHalfSize),
            QuarterPixels(WINNER_CENTER_Y + plaqueHalfSize),
            G_TX_RENDERTILE,
            0,
            0,
            texelStep,
            texelStep
        );
        LoadLargePalette(&gfx, f_MovieBuffer + WINNER_PALETTE_OFFSET);
        LoadTextureBlock(
            &gfx,
            f_MovieBuffer + WINNER_FACE_OFFSET,
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            WINNER_FACE_SIZE,
            WINNER_FACE_SIZE,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        texelStep = TexelRunStep((WINNER_FACE_SIZE / 2), growth);
        width = QuarterPixels(WINNER_CENTER_X - growth);
        DrawTextureRectangle(
            &gfx,
            width,
            QuarterPixels(WINNER_CENTER_Y - growth),
            QuarterPixels(WINNER_CENTER_X + growth),
            QuarterPixels(WINNER_CENTER_Y + growth),
            G_TX_RENDERTILE,
            0,
            0,
            texelStep,
            texelStep
        );
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#define DRIVE_RECORD_DISTANCE_DRAW 26
#define DRIVE_RECORD_DISTANCE_END_DRAW 45
#define DRIVE_RECORD_DISTANCE_X 136
#define DRIVE_RECORD_DISTANCE_Y 116
#define DRIVE_RECORD_DISTANCE_ENV_COLOR 0xFF600000
#define DRIVE_RECORD_DISTANCE_PRIM_COLOR 0xFFFF0000

void DrawDriveRecord(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    u32 width;
    s32 height;
    s32 stripCount;
    s32 halfWidth;
    s32 stripHeight;
    s32 slide;
    s32 centerX;
    s32 bottom;
    s32 copyTop;
    s32 stripIndex;
    Radians angle;
    f32 fade;
    f32 alpha;

    width = ReadImageWidth(f_MovieBuffer);
    height = ReadImageHeight(f_MovieBuffer);
    if (f_MovieDrawCount == CAPTION_MEET_DRAW + 1) {
        PlaySoundWithEffects(
            SOUND_SHRINKING_IMAGE,
            CAPTION_SOUND_CHANNEL,
            CAPTION_SOUND_VOLUME,
            0,
            SOUND_FULL_SCALE
        );
    }

    gDPPipeSync(gfx++);
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE_ALPHA,
        ENVIRONMENT,
        ENVIRONMENT,
        0,
        TEXEL0,
        0,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE_ALPHA,
        ENVIRONMENT,
        ENVIRONMENT,
        0,
        TEXEL0,
        0
    );
    halfWidth = width >> 1;
    stripHeight = MOVIE_IMAGE_STRIP_HEIGHT;
    stripCount = height / stripHeight;
    if (f_MovieDrawCount < CAPTION_MEET_DRAW) {
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        gDPSetEnvColor(gfx++, 255, 255, 255, CAPTION_COPY_ALPHA);
    }
    else if (f_MovieDrawCount < CAPTION_FLASH_END_DRAW) {
        alpha = 255.0f -
            (sinf((f_MovieDrawCount - CAPTION_MEET_DRAW) * CAPTION_TURN_PER_FRAME) *
                CAPTION_FLASH_DEPTH);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
        gDPSetEnvColor(gfx++, 255, 255, 255, OPAQUE_ALPHA);
    }
    else if (f_MovieDrawCount < CAPTION_GROW_DRAW) {
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        gDPSetEnvColor(gfx++, 255, 255, 255, OPAQUE_ALPHA);
    }
    else if (f_MovieDrawCount < CAPTION_GROW_END_DRAW) {
        angle = (f_MovieDrawCount - CAPTION_GROW_DRAW) * CAPTION_TURN_PER_FRAME;
        fade = sinf(angle) * CAPTION_FLASH_DEPTH;
        halfWidth = cosf(angle) * halfWidth;
        stripHeight = ((1.0f - cosf(angle)) * CAPTION_GROWTH_MAX) + MOVIE_IMAGE_STRIP_HEIGHT;
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255.0f - fade);
        gDPSetEnvColor(gfx++, 255, 255, 255, 255.0f - fade);
    }
    else {
        f_CurrentMovie = MOVIE_NONE;
        halfWidth = 0;
    }

    gSPDisplayList(gfx++, f_MovieImageSetupDisplayList);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(f_MovieBuffer + MOVIE_IMAGE_PALETTE_OFFSET));
    if (f_MovieDrawCount < CAPTION_DRAW_COUNT && halfWidth > 0) {
        for (stripIndex = 0; stripIndex != stripCount; stripIndex++) {
            slide = CAPTION_START_SLIDE - (f_MovieDrawCount * CAPTION_SLIDE_PER_FRAME);
            if (slide < 0) {
                slide = 0;
            }

            LoadTextureBlock(
                &gfx,
                f_MovieBuffer +
                    ((stripIndex * width * MOVIE_IMAGE_STRIP_HEIGHT) + MOVIE_IMAGE_PIXELS_OFFSET),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                width,
                MOVIE_IMAGE_STRIP_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            centerX = SHRINKING_IMAGE_CENTER_X + slide;
            bottom = SHRINKING_IMAGE_CENTER_Y + (stripIndex * stripHeight);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(centerX - halfWidth),
                QuarterPixels(bottom - (stripHeight * SHRINKING_IMAGE_STRIPS_ABOVE_CENTER)),
                QuarterPixels(halfWidth + centerX),
                QuarterPixels(
                    bottom - (stripHeight * SHRINKING_IMAGE_STRIPS_ABOVE_CENTER) + stripHeight
                ),
                G_TX_RENDERTILE,
                0,
                0,
                (s32)((width >> 1) << TEXEL_STEP_SHIFT) / halfWidth,
                TexelRunStep(MOVIE_IMAGE_STRIP_HEIGHT, stripHeight)
            );
            if (f_MovieDrawCount >= CAPTION_MEET_DRAW) {
                continue;
            }

            copyTop = CAPTION_COPY_TOP + (stripIndex * MOVIE_IMAGE_STRIP_HEIGHT);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(SHRINKING_IMAGE_CENTER_X - (s32)(width >> 1) - slide),
                QuarterPixels(copyTop),
                QuarterPixels(SHRINKING_IMAGE_CENTER_X + (s32)(width >> 1) - slide),
                QuarterPixels(copyTop + MOVIE_IMAGE_STRIP_HEIGHT),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    if (f_MovieDrawCount >= DRIVE_RECORD_DISTANCE_DRAW &&
        f_MovieDrawCount < DRIVE_RECORD_DISTANCE_END_DRAW) {
        SetUpGradientTextDrawing(&gfx, 0);
        sprintf(
            g_PrintBuffer,
            "%5.2f%c",
            g_Players[0].lastDriveDistance * YARDS_PER_UNIT,
            YARDS_GLYPH
        );
        DrawLargeTextWithShadow(
            &gfx,
            DRIVE_RECORD_DISTANCE_X,
            DRIVE_RECORD_DISTANCE_Y,
            g_PrintBuffer,
            DRIVE_RECORD_DISTANCE_ENV_COLOR,
            DRIVE_RECORD_DISTANCE_PRIM_COLOR,
            TEXT_FLAG_FIXED_WIDTH
        );
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawPuttCaption(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    u32 width;
    s32 height;
    s32 stripCount;
    s32 halfWidth;
    s32 stripHeight;
    s32 slide;
    s32 centerX;
    s32 bottom;
    s32 copyTop;
    s32 stripIndex;
    Radians angle;
    f32 fade;
    f32 alpha;

    width = ReadImageWidth(f_MovieBuffer);
    height = ReadImageHeight(f_MovieBuffer);
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE_ALPHA,
        ENVIRONMENT,
        ENVIRONMENT,
        0,
        TEXEL0,
        0,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE_ALPHA,
        ENVIRONMENT,
        ENVIRONMENT,
        0,
        TEXEL0,
        0
    );
    halfWidth = width >> 1;
    stripHeight = MOVIE_IMAGE_STRIP_HEIGHT;
    stripCount = height / stripHeight;
    if (f_MovieDrawCount == CAPTION_MEET_DRAW + 1) {
        PlaySoundWithEffects(
            SOUND_CAPTION_MEET,
            CAPTION_SOUND_CHANNEL,
            CAPTION_SOUND_VOLUME,
            0,
            SOUND_FULL_SCALE
        );
    }

    if (f_MovieDrawCount < CAPTION_MEET_DRAW) {
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        gDPSetEnvColor(gfx++, 255, 255, 255, CAPTION_COPY_ALPHA);
    }
    else if (f_MovieDrawCount < CAPTION_FLASH_END_DRAW) {
        alpha = 255.0f -
            (sinf((f_MovieDrawCount - CAPTION_MEET_DRAW) * CAPTION_TURN_PER_FRAME) *
                CAPTION_FLASH_DEPTH);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
        gDPSetEnvColor(gfx++, 255, 255, 255, OPAQUE_ALPHA);
    }
    else if (f_MovieDrawCount < CAPTION_GROW_DRAW) {
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        gDPSetEnvColor(gfx++, 255, 255, 255, OPAQUE_ALPHA);
    }
    else if (f_MovieDrawCount < CAPTION_GROW_END_DRAW) {
        angle = (f_MovieDrawCount - CAPTION_GROW_DRAW) * CAPTION_TURN_PER_FRAME;
        fade = sinf(angle) * CAPTION_FLASH_DEPTH;
        halfWidth = cosf(angle) * halfWidth;
        stripHeight = ((1.0f - cosf(angle)) * CAPTION_GROWTH_MAX) + MOVIE_IMAGE_STRIP_HEIGHT;
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255.0f - fade);
        gDPSetEnvColor(gfx++, 255, 255, 255, 255.0f - fade);
    }
    else {
        f_CurrentMovie = MOVIE_NONE;
        halfWidth = 0;
    }

    gSPDisplayList(gfx++, f_MovieImageSetupDisplayList);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(f_MovieBuffer + MOVIE_IMAGE_PALETTE_OFFSET));
    if (f_MovieDrawCount < CAPTION_DRAW_COUNT && halfWidth > 0) {
        for (stripIndex = 0; stripIndex != stripCount; stripIndex++) {
            slide = CAPTION_START_SLIDE - (f_MovieDrawCount * CAPTION_SLIDE_PER_FRAME);
            if (slide < 0) {
                slide = 0;
            }

            LoadTextureBlock(
                &gfx,
                f_MovieBuffer +
                    ((stripIndex * width * MOVIE_IMAGE_STRIP_HEIGHT) + MOVIE_IMAGE_PIXELS_OFFSET),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                width,
                MOVIE_IMAGE_STRIP_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            centerX = SHRINKING_IMAGE_CENTER_X + slide;
            bottom = SHRINKING_IMAGE_CENTER_Y + (stripIndex * stripHeight);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(centerX - halfWidth),
                QuarterPixels(bottom - (stripHeight * SHRINKING_IMAGE_STRIPS_ABOVE_CENTER)),
                QuarterPixels(halfWidth + centerX),
                QuarterPixels(
                    bottom - (stripHeight * SHRINKING_IMAGE_STRIPS_ABOVE_CENTER) + stripHeight
                ),
                G_TX_RENDERTILE,
                0,
                0,
                (s32)((width >> 1) << TEXEL_STEP_SHIFT) / halfWidth,
                TexelRunStep(MOVIE_IMAGE_STRIP_HEIGHT, stripHeight)
            );
            if (f_MovieDrawCount >= CAPTION_MEET_DRAW) {
                continue;
            }

            copyTop = CAPTION_COPY_TOP + (stripIndex * MOVIE_IMAGE_STRIP_HEIGHT);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(SHRINKING_IMAGE_CENTER_X - (s32)(width >> 1) - slide),
                QuarterPixels(copyTop),
                QuarterPixels(SHRINKING_IMAGE_CENTER_X + (s32)(width >> 1) - slide),
                QuarterPixels(copyTop + MOVIE_IMAGE_STRIP_HEIGHT),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

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
#define READY_GO_DISTANCE (-300.0f)
#define READY_GO_VERTEX_COUNT 12
#define READY_GO_STRIP_COUNT 5
#define READY_WIDTH 160
#define READY_STRIP_HEIGHT 12
#define GO_WIDTH 144
#define GO_STRIP_HEIGHT 14
#define READY_GO_LAST_STRIP_HEIGHT 8

void DrawReadyGoMovie(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    Vec3 rotation;
    Vec3 translation;
    Vec3 scale;
    Mat4 matrix;
    s32 alpha;
    s32 stripHeight;
    s32 stripIndex;

    f_MovieBackBuffer ^= 1;
    alpha = OPAQUE_ALPHA;
    if (f_MovieDrawCount == READY_SOUND_DRAW) {
        PlaySoundWithEffects(
            SOUND_READY,
            g_NextSoundChannel,
            READY_GO_SOUND_VOLUME,
            READY_GO_SOUND_REVERB,
            SOUND_FULL_SCALE
        );
    }

    if (f_MovieDrawCount == GO_GROW_DRAW) {
        PlaySoundWithEffects(
            SOUND_GO,
            g_NextSoundChannel,
            READY_GO_SOUND_VOLUME,
            READY_GO_SOUND_REVERB,
            SOUND_FULL_SCALE
        );
    }

    scale[AXIS_X] = 1.0f;
    scale[AXIS_Y] = 1.0f;
    scale[AXIS_Z] = 1.0f;
    if (f_MovieDrawCount < READY_FLIP_DRAW) {
        rotation[AXIS_X] = M_PI;
        rotation[AXIS_Y] = (M_PI / 2) - ((f32)f_MovieDrawCount * READY_GO_TURN_PER_FRAME);
        rotation[AXIS_Z] = 0.0f;
    }
    else if (f_MovieDrawCount < READY_FACING_DRAW) {
        rotation[AXIS_X] =
            M_PI - ((f32)(f_MovieDrawCount - READY_FLIP_DRAW) * READY_GO_TURN_PER_FRAME);
        rotation[AXIS_Y] = 0.0f;
        rotation[AXIS_Z] = 0.0f;
    }
    else if (f_MovieDrawCount < GO_DRAW) {
        rotation[AXIS_X] =
            M_PI - ((f32)(f_MovieDrawCount - READY_FLIP_DRAW) * READY_GO_TURN_PER_FRAME);
        rotation[AXIS_Y] = 0.0f;
        rotation[AXIS_Z] = 0.0f;
    }
    else if (f_MovieDrawCount < GO_GROW_DRAW) {
        rotation[AXIS_X] =
            M_PI - ((f32)(f_MovieDrawCount - READY_FACING_DRAW) * READY_GO_TURN_PER_FRAME);
        rotation[AXIS_Y] = 0.0f;
        rotation[AXIS_Z] = 0.0f;
    }
    else if (f_MovieDrawCount < READY_GO_END_DRAW) {
        alpha = OPAQUE_ALPHA - ((f_MovieDrawCount - GO_GROW_DRAW) * GO_FADE_PER_FRAME);
        if (alpha < 0) {
            alpha = 0;
        }

        rotation[AXIS_X] = rotation[AXIS_Y] = rotation[AXIS_Z] = 0.0f;
        scale[AXIS_Z] = (GO_GROWTH_PER_FRAME * (f_MovieDrawCount - GO_GROW_DRAW)) + 1.0f;
        scale[AXIS_X] = scale[AXIS_Y] = scale[AXIS_Z];
    }
    else {
        f_CurrentMovie = MOVIE_NONE;
        return;
    }

    translation[AXIS_X] = 0.0f;
    translation[AXIS_Y] = 0.0f;
    translation[AXIS_Z] = READY_GO_DISTANCE;
    BuildTransformMatrix(matrix, rotation, translation, scale);
    ConvertMatrixToFixed(matrix, &f_ReadyGoMatrices[f_MovieBackBuffer]);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPSetEnvColor(gfx++, 255, 255, 255, alpha);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        TEXEL0,
        ENVIRONMENT,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        TEXEL0,
        ENVIRONMENT,
        0,
        TEXEL0,
        0
    );
    gSPDisplayList(gfx++, f_MovieImageSetupDisplayList);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    if (alpha == OPAQUE_ALPHA) {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    }
    else {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    }

    if (f_MovieDrawCount < GO_DRAW) {
        gDPLoadTLUT_pal256(gfx++, TextureAddress(f_MovieBuffer + MOVIE_IMAGE_PALETTE_OFFSET));
    }
    else {
        gDPLoadTLUT_pal256(
            gfx++,
            TextureAddress(f_MovieBuffer + GO_TEXT_OFFSET + MOVIE_IMAGE_PALETTE_OFFSET)
        );
    }

    gSPMatrix(
        gfx++,
        PhysicalAddress(&g_ScreenOrthoMatrix),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_ReadyGoMatrices[f_MovieBackBuffer]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
    );
    gSPVertex(gfx++, f_ReadyGoVertices, READY_GO_VERTEX_COUNT, 0);
    for (stripIndex = 0; stripIndex != READY_GO_STRIP_COUNT; stripIndex++) {
        if (f_MovieDrawCount < GO_DRAW) {
            stripHeight = (stripIndex != READY_GO_STRIP_COUNT - 1) ? READY_STRIP_HEIGHT :
                                                                     READY_GO_LAST_STRIP_HEIGHT;
            gDPLoadTextureTile(
                gfx++,
                TextureAddress(
                    f_MovieBuffer +
                    (MOVIE_IMAGE_PIXELS_OFFSET + (stripIndex * READY_WIDTH * READY_STRIP_HEIGHT))
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                READY_WIDTH,
                stripHeight,
                0,
                0,
                READY_WIDTH - 1,
                stripHeight - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_MIRROR,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gSPModifyVertex(gfx++, stripIndex * 2, G_MWO_POINT_ST, 0);
            gSPModifyVertex(
                gfx++,
                (stripIndex * 2) + 1,
                G_MWO_POINT_ST,
                TextureCoordinateWord(TexelCoordinate(READY_WIDTH) - 1, 0)
            );
            gSPModifyVertex(
                gfx++,
                (stripIndex * 2) + 2,
                G_MWO_POINT_ST,
                TexelCoordinate(stripHeight)
            );
            gSPModifyVertex(
                gfx++,
                (stripIndex * 2) + 3,
                G_MWO_POINT_ST,
                TextureCoordinateWord(
                    TexelCoordinate(READY_WIDTH) - 1,
                    TexelCoordinate(stripHeight)
                )
            );
        }
        else {
            stripHeight = (stripIndex != READY_GO_STRIP_COUNT - 1) ? GO_STRIP_HEIGHT :
                                                                     READY_GO_LAST_STRIP_HEIGHT;
            gDPLoadTextureTile(
                gfx++,
                TextureAddress(
                    f_MovieBuffer +
                    (GO_TEXT_OFFSET + MOVIE_IMAGE_PIXELS_OFFSET +
                        (stripIndex * GO_WIDTH * GO_STRIP_HEIGHT))
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                GO_WIDTH,
                stripHeight,
                0,
                0,
                GO_WIDTH - 1,
                stripHeight - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_MIRROR,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gSPModifyVertex(gfx++, stripIndex * 2, G_MWO_POINT_ST, 0);
            gSPModifyVertex(
                gfx++,
                (stripIndex * 2) + 1,
                G_MWO_POINT_ST,
                TextureCoordinateWord(TexelCoordinate(GO_WIDTH) - 1, 0)
            );
            gSPModifyVertex(
                gfx++,
                (stripIndex * 2) + 2,
                G_MWO_POINT_ST,
                TexelCoordinate(stripHeight)
            );
            gSPModifyVertex(
                gfx++,
                (stripIndex * 2) + 3,
                G_MWO_POINT_ST,
                TextureCoordinateWord(TexelCoordinate(GO_WIDTH) - 1, TexelCoordinate(stripHeight))
            );
        }

        gSP2Triangles(
            gfx++,
            stripIndex * 2,
            (stripIndex * 2) + 2,
            (stripIndex * 2) + 3,
            0,
            stripIndex * 2,
            (stripIndex * 2) + 3,
            (stripIndex * 2) + 1,
            0
        );
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawShrinkingMovieImage(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 width;
    u16 narrowWidth;
    s32 texelWidth;
    s32 stripCount;
    Radians angle;
    s32 halfWidth;
    s32 stripHeight;
    s32 left;
    s32 bottom;
    s32 texelStep;
    s32 stripIndex;

    stripHeight = MOVIE_IMAGE_STRIP_HEIGHT;
    width = ReadImageWidth(f_MovieBuffer);
    stripCount = ReadImageHeight(f_MovieBuffer) / stripHeight;
    narrowWidth = width;
    texelWidth = narrowWidth;
    if (f_MovieDrawCount == SHRINKING_IMAGE_SOUND_DRAW) {
        PlaySoundWithEffects(
            SOUND_SHRINKING_IMAGE,
            SHRINKING_IMAGE_SOUND_CHANNEL,
            SHRINKING_IMAGE_SOUND_VOLUME,
            0,
            SOUND_FULL_SCALE
        );
    }

    gSPDisplayList(gfx++, f_MovieImageSetupDisplayList);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPSetEnvColor(gfx++, 255, 255, 255, OPAQUE_ALPHA);
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE_ALPHA,
        ENVIRONMENT,
        ENVIRONMENT,
        0,
        TEXEL0,
        0,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE_ALPHA,
        ENVIRONMENT,
        ENVIRONMENT,
        0,
        TEXEL0,
        0
    );
    gDPLoadTLUT_pal256(gfx++, TextureAddress(f_MovieBuffer + MOVIE_IMAGE_PALETTE_OFFSET));
    if (f_MovieDrawCount < SHRINKING_IMAGE_DRAW_COUNT) {
        angle = (f32)f_MovieDrawCount * SHRINKING_IMAGE_TURN_PER_FRAME;
        if (angle > M_PI / 2) {
            angle = M_PI / 2;
        }

        halfWidth = (cosf(angle) + 1.0f) * width * 0.5f;
        stripHeight = (cosf(angle) + 1.0f) * MOVIE_IMAGE_STRIP_HEIGHT;
        if (halfWidth > 0 && stripHeight > 0) {
            for (stripIndex = 0; stripIndex != stripCount; stripIndex++) {
                LoadTextureBlock(
                    &gfx,
                    f_MovieBuffer +
                        ((stripIndex * width * MOVIE_IMAGE_STRIP_HEIGHT) +
                            MOVIE_IMAGE_PIXELS_OFFSET),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    width,
                    MOVIE_IMAGE_STRIP_HEIGHT,
                    0,
                    0,
                    0,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                left = SHRINKING_IMAGE_CENTER_X - halfWidth;
                texelStep = TexelRunStep((texelWidth >> 1), halfWidth);
                bottom = SHRINKING_IMAGE_CENTER_Y + (stripIndex * stripHeight);
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(left),
                    QuarterPixels(bottom - (stripHeight * SHRINKING_IMAGE_STRIPS_ABOVE_CENTER)),
                    QuarterPixels(SHRINKING_IMAGE_CENTER_X + halfWidth),
                    QuarterPixels(
                        bottom - (stripHeight * SHRINKING_IMAGE_STRIPS_ABOVE_CENTER) + stripHeight
                    ),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    texelStep,
                    TexelRunStep(MOVIE_IMAGE_STRIP_HEIGHT, stripHeight)
                );
            }
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawMontyMoleMovie(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    EncodedAsset asset;
    s32 unused[4] UNUSED;
    s32 stripIndex;

    f_MovieTick++;
    if (f_MovieTick == MOVIE_TICKS_PER_FRAME) {
        f_MovieTick = 0;
        OpenEncodedAsset(f_MontyMoleMovieFrameAssets[f_MovieFrame], &asset);
        ReadEncodedAsset(f_MovieBuffer + MOVIE_FRAME_OFFSET, &asset);
        f_MovieFrame++;
        if (f_MovieFrame == MONTY_MOLE_FRAME_COUNT) {
            f_CurrentMovie = MOVIE_NONE;
        }
    }

    if (f_MovieDrawCount == 1) {
        PlaySoundWithEffects(
            SOUND_GOOMBA,
            MOVIE_SOUND_CHANNEL,
            MOVIE_SOUND_VOLUME,
            MOVIE_SOUND_REVERB,
            SOUND_FULL_SCALE
        );
    }

    gSPDisplayList(gfx++, f_MovieSetupDisplayList);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(f_MovieBuffer + MOVIE_PALETTE_OFFSET));
    for (stripIndex = 0; stripIndex != MONTY_MOLE_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureTile(
            gfx++,
            TextureAddress(
                f_MovieBuffer + (MOVIE_FRAME_OFFSET + (stripIndex * MONTY_MOLE_STRIP_SIZE))
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            MONTY_MOLE_WIDTH,
            MONTY_MOLE_STRIP_HEIGHT,
            0,
            0,
            MONTY_MOLE_WIDTH - 1,
            MONTY_MOLE_STRIP_HEIGHT - 1,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(MONTY_MOLE_X),
            QuarterPixels(MONTY_MOLE_Y + (stripIndex * MONTY_MOLE_STRIP_HEIGHT)),
            QuarterPixels(MONTY_MOLE_X + MONTY_MOLE_WIDTH),
            QuarterPixels(
                MONTY_MOLE_Y + MONTY_MOLE_STRIP_HEIGHT + (stripIndex * MONTY_MOLE_STRIP_HEIGHT)
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawGoombaMovie(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    EncodedAsset asset;
    s32 unused[4] UNUSED;
    s32 stripIndex;
    s32 height;

    f_MovieTick++;
    if (f_MovieTick == MOVIE_TICKS_PER_FRAME) {
        f_MovieTick = 0;
        OpenEncodedAsset(f_GoombaMovieFrameAssets[f_MovieFrame], &asset);
        ReadEncodedAsset(f_MovieBuffer + MOVIE_FRAME_OFFSET, &asset);
        f_MovieFrame++;
        if (f_MovieFrame == GOOMBA_FRAME_COUNT) {
            f_CurrentMovie = MOVIE_NONE;
        }
    }

    if (f_MovieDrawCount == 1) {
        PlaySoundWithEffects(
            SOUND_GOOMBA,
            MOVIE_SOUND_CHANNEL,
            MOVIE_SOUND_VOLUME,
            MOVIE_SOUND_REVERB,
            SOUND_FULL_SCALE
        );
    }

    gSPDisplayList(gfx++, f_MovieSetupDisplayList);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(f_MovieBuffer + MOVIE_PALETTE_OFFSET));
    for (stripIndex = 0; stripIndex != GOOMBA_STRIP_COUNT; stripIndex++) {
        if (stripIndex == GOOMBA_STRIP_COUNT - 1) {
            height = GOOMBA_LAST_STRIP_HEIGHT;
        }
        else {
            height = GOOMBA_STRIP_HEIGHT;
        }

        gDPLoadTextureTile(
            gfx++,
            TextureAddress(f_MovieBuffer + (MOVIE_FRAME_OFFSET + (stripIndex * GOOMBA_STRIP_SIZE))),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            GOOMBA_WIDTH,
            height,
            0,
            0,
            GOOMBA_WIDTH - 1,
            height - 1,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(GOOMBA_X),
            QuarterPixels(GOOMBA_Y + (stripIndex * GOOMBA_STRIP_HEIGHT)),
            QuarterPixels(GOOMBA_X + GOOMBA_WIDTH),
            QuarterPixels(GOOMBA_Y + (stripIndex * GOOMBA_STRIP_HEIGHT) + height),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawLakituMovie(Gfx **displayListHead) {
    EncodedAsset asset;
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    f32 halfWidth;
    f32 halfHeight;
    s32 roundedHalfWidth;
    s32 roundedHalfHeight;
    s32 x;
    s32 y;
    s32 left;
    s32 offset;
    f32 startS;
    f32 startT;
    f32 stepS;
    f32 stepT;

    if (f_MovieDrawCount == LAKITU_RED_LIGHT_DRAW) {
        PlaySound(SOUND_START_LIGHT_RED, g_NextSoundChannel, SOUND_FULL_SCALE);
    }

    if (f_MovieDrawCount == LAKITU_SECOND_RED_LIGHT_DRAW) {
        PlaySound(SOUND_START_LIGHT_RED, g_NextSoundChannel, SOUND_FULL_SCALE);
    }

    if (f_MovieDrawCount == LAKITU_GO_LIGHT_DRAW) {
        PlaySound(SOUND_START_LIGHT_GO, g_NextSoundChannel, SOUND_FULL_SCALE);
    }

    if (f_MovieDrawCount < LAKITU_ARRIVE_DRAW) {
        f_Lakitu.x -= (LAKITU_START_X - LAKITU_HOVER_X) / LAKITU_ARRIVE_DRAW;
        f_Lakitu.y += (LAKITU_HOVER_Y - LAKITU_START_Y) / LAKITU_ARRIVE_DRAW;
    }
    else if (f_MovieDrawCount < LAKITU_LEAVE_DRAW) {
        f_MovieTick++;
        if (f_MovieTick == LAKITU_TICKS_PER_FRAME) {
            f_MovieTick = 0;
            if (f_MovieFrame < LAKITU_LAST_FRAME) {
                f_MovieFrame++;
                f_MovieBackBuffer ^= 1;
                OpenEncodedAsset(f_LakituMovieFrameAssets[f_MovieFrame], &asset);
                ReadEncodedAsset(f_MovieBuffer + (f_MovieBackBuffer * LAKITU_FRAME_SIZE), &asset);
            }
        }

        f_LakituVelocity.x = 0.0f;
        f_LakituVelocity.y = 0.0f;
    }
    else {
        f_LakituVelocity.x -= LAKITU_LEAVE_ACCELERATION_X;
        f_LakituVelocity.y -= LAKITU_LEAVE_ACCELERATION_Y;
        f_Lakitu.x += f_LakituVelocity.x;
        f_Lakitu.scale += LAKITU_LEAVE_GROWTH;
        f_Lakitu.y += f_LakituVelocity.y;
        if (f_Lakitu.x < LAKITU_GONE_X) {
            f_CurrentMovie = MOVIE_NONE;
        }
    }

    gSPDisplayList(gfx++, f_MovieImageSetupDisplayList);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPLoadTLUT_pal256(
        gfx++,
        TextureAddress(
            f_MovieBuffer + ((f_MovieBackBuffer * LAKITU_FRAME_SIZE) + MOVIE_IMAGE_PALETTE_OFFSET)
        )
    );
    for (stripIndex = 0; stripIndex != LAKITU_STRIP_COUNT; stripIndex++) {
        offset = MOVIE_IMAGE_PIXELS_OFFSET + (stripIndex * LAKITU_STRIP_SIZE);
        halfWidth = f_Lakitu.scale * (LAKITU_WIDTH / 2);
        halfHeight = f_Lakitu.scale * LAKITU_STRIP_HEIGHT;
        stepS = TexelRunStep((LAKITU_WIDTH / 2), halfWidth);
        stepT = TexelRunStep(LAKITU_STRIP_HEIGHT, halfHeight);
        roundedHalfWidth = halfWidth + 0.5f;

        startS = 16.0f - (((f32)roundedHalfWidth - halfWidth) * 16.0f);
        gDPLoadTextureTile(
            gfx++,
            TextureAddress(f_MovieBuffer + ((f_MovieBackBuffer * LAKITU_FRAME_SIZE) + offset)),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            LAKITU_WIDTH,
            LAKITU_STRIP_HEIGHT,
            0,
            0,
            LAKITU_WIDTH - 1,
            LAKITU_STRIP_HEIGHT - 1,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        roundedHalfHeight = halfHeight + 0.5f;
        startT = 16.0f - (((f32)roundedHalfHeight - halfHeight) * 16.0f);
        x = f_Lakitu.x;
        left = x - roundedHalfWidth;
        y = f_Lakitu.y;

        DrawTextureRectangle(
            &gfx,
            QuarterPixels(left),
            QuarterPixels(y - roundedHalfHeight + (stripIndex * roundedHalfHeight)),
            QuarterPixels(x + roundedHalfWidth),
            QuarterPixels(y + (stripIndex * roundedHalfHeight)),
            G_TX_RENDERTILE,
            startS,
            startT,
            stepS,
            stepT
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawToadMovie(Gfx **displayListHead) {
    EncodedAsset asset;
    Gfx *gfx = *displayListHead;
    s32 stripIndex;

    f_MovieTick++;
    if (f_MovieTick == MOVIE_TICKS_PER_FRAME) {
        f_MovieTick = 0;
        f_MovieFrame = (f_MovieFrame + 1) % TOAD_FRAME_COUNT;
        if (f_MovieFrame == TOAD_SWING_LEFT_FRAME) {
            PlaySoundWithEffects(
                SOUND_FLAG_SWISH,
                FLAG_SWISH_SOUND_CHANNEL,
                SOUND_FULL_SCALE,
                MOVIE_SOUND_REVERB,
                FLAG_SWISH_LEFT_PAN
            );
            SetSoundFrequencyOffset(FLAG_SWISH_SOUND_CHANNEL, FLAG_SWISH_PITCH);
        }

        if (f_MovieFrame == TOAD_SWING_RIGHT_FRAME) {
            PlaySoundWithEffects(
                SOUND_FLAG_SWISH,
                FLAG_SWISH_SOUND_CHANNEL,
                SOUND_FULL_SCALE,
                MOVIE_SOUND_REVERB,
                FLAG_SWISH_RIGHT_PAN
            );
            SetSoundFrequencyOffset(FLAG_SWISH_SOUND_CHANNEL, FLAG_SWISH_PITCH);
        }

        f_MovieBackBuffer ^= 1;
        OpenEncodedAsset(f_ToadMovieFrameAssets[f_MovieFrame], &asset);
        ReadEncodedAsset(
            f_MovieBuffer + ((f_MovieBackBuffer * TOAD_FRAME_SIZE) + TOAD_FRAME_OFFSET),
            &asset
        );
    }

    if (f_MovieDrawCount == OUT_OF_BOUNDS_SOUND_DRAW) {
        PlaySoundWithEffects(
            SOUND_OUT_OF_BOUNDS,
            OUT_OF_BOUNDS_SOUND_CHANNEL,
            MOVIE_SOUND_VOLUME,
            MOVIE_SOUND_REVERB,
            SOUND_FULL_SCALE
        );
    }

    gSPDisplayList(gfx++, f_MovieImageSetupDisplayList);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetScissor(
        gfx++,
        G_SC_NON_INTERLACE,
        g_Cameras[INSET_VIEW_CAMERA].view.scissorX + INSET_FRAME_WIDTH,
        g_Cameras[INSET_VIEW_CAMERA].view.scissorY + INSET_FRAME_HEIGHT,
        g_Cameras[INSET_VIEW_CAMERA].view.scissorX +
            g_Cameras[INSET_VIEW_CAMERA].view.scissorWidth - INSET_FRAME_WIDTH,
        g_Cameras[INSET_VIEW_CAMERA].view.scissorY +
            g_Cameras[INSET_VIEW_CAMERA].view.scissorHeight - INSET_FRAME_HEIGHT
    );
    gDPLoadTLUT_pal256(gfx++, TextureAddress(f_MovieBuffer));
    for (stripIndex = 0; stripIndex != TOAD_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureTile(
            gfx++,
            TextureAddress(
                f_MovieBuffer +
                (TOAD_FRAME_OFFSET + (stripIndex * TOAD_STRIP_SIZE) +
                    (f_MovieBackBuffer * TOAD_FRAME_SIZE))
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            TOAD_WIDTH,
            TOAD_STRIP_HEIGHT,
            0,
            0,
            TOAD_WIDTH - 1,
            TOAD_STRIP_HEIGHT - 1,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(TOAD_X),
            QuarterPixels(TOAD_Y + (stripIndex * TOAD_STRIP_HEIGHT)),
            QuarterPixels(TOAD_X + TOAD_WIDTH),
            QuarterPixels(TOAD_Y + TOAD_STRIP_HEIGHT + (stripIndex * TOAD_STRIP_HEIGHT)),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    gDPPipeSync(gfx++);
    gDPSetScissor(
        gfx++,
        G_SC_NON_INTERLACE,
        g_Cameras[0].view.scissorX,
        g_Cameras[0].view.scissorY,
        g_Cameras[0].view.scissorX + g_Cameras[0].view.scissorWidth,
        g_Cameras[0].view.scissorY + g_Cameras[0].view.scissorHeight
    );
    *displayListHead = gfx;
}

Boolean IsContactEffectShown(void) {
    return g_ContactEffect != CONTACT_EFFECT_NONE;
}

ContactStyle GetContactStyle(Player *player) {
    GameBoyCharacter *gameBoyCharacter;
    CharacterIndex8 characterIndex = player->characterIndex;

    if (characterIndex >= CHARACTER_KID) {
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
            case GAME_BOY_BALL_TYPE_RAINBOW:
                return CONTACT_STYLE_RAINBOW;
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

    if (characterIndex == CHARACTER_WARIO) {
        return CONTACT_STYLE_WARIO;
    }

    if (characterIndex == CHARACTER_YOSHI || characterIndex == CHARACTER_DONKEY_KONG) {
        return CONTACT_STYLE_RAINBOW;
    }

    if (characterIndex == CHARACTER_BOWSER) {
        return CONTACT_STYLE_BOWSER;
    }

    if (characterIndex == CHARACTER_BABY_MARIO ||
        characterIndex == CHARACTER_MARIO ||
        characterIndex == CHARACTER_METAL_MARIO) {
        return CONTACT_STYLE_MARIO;
    }

    return CONTACT_STYLE_PLAIN;
}

static inline void SetContactHalfWidth(s16 halfWidth) {
    f_ContactVertices[0].v.ob[AXIS_X] = -halfWidth;
    f_ContactVertices[1].v.ob[AXIS_X] = halfWidth;
}

void StartContactEffect(ContactEffect effect) {
    s32 matrixIndex;

    if (effect == CONTACT_EFFECT_NONE) {
        g_ContactEffect = effect;
    }
    else if (IsShownContactEffect(effect)) {
        for (matrixIndex = 0; matrixIndex != CONTACT_MATRIX_COUNT; matrixIndex++) {
            guTranslate(
                &f_ContactMatrices[matrixIndex],
                g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                (g_CurrentShot.ballPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE) -
                    (g_PinVertexScale * CONTACT_DROP_PER_PIN_SCALE),
                g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE
            );
        }

        g_ContactEffect = 0;
        f_IsContactEffectFading = FALSE;
        f_ContactVertices[0].v.ob[AXIS_Y] = 0;
        f_ContactVertices[1].v.ob[AXIS_Y] = 0;
        if (effect == CONTACT_EFFECT_POWER_IRON) {
            f_ContactStyle = CONTACT_STYLE_POWER_IRON;
        }
        else if (effect == CONTACT_EFFECT_SHOT) {
            f_ContactStyle = CONTACT_STYLE_SHOT;
        }
        else if (effect == CONTACT_EFFECT_PUTT) {
            f_ContactStyle = CONTACT_STYLE_PUTT;
        }
        else {
            f_ContactStyle = GetContactStyle(&g_Players[g_RoundHud.shooterIndex]);
        }
    }
    else {
        f_IsContactEffectFading = TRUE;
    }

    if (effect != CONTACT_EFFECT_NONE && IsShownContactEffect(effect)) {
        if (f_ContactStyle == CONTACT_STYLE_POWER_IRON) {
            SetContactHalfWidth(20);
            f_ContactTextureS = TexelCoordinate(PLAIN_CONTACT_IMAGE_WIDTH) - 1;
            f_ContactTextureT = 0xD6;
            PlaySound(SOUND_CONTACT_PLAIN, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
        else if (f_ContactStyle == CONTACT_STYLE_SHOT) {
            SetContactHalfWidth(10);
            f_ContactTextureS = TexelCoordinate(PLAIN_CONTACT_IMAGE_WIDTH) - 1;
            f_ContactTextureT = 0xD6;
        }
        else if (f_ContactStyle == CONTACT_STYLE_PUTT) {
            SetContactHalfWidth(18);
            f_ContactVertices[0].v.ob[AXIS_Y] = CONTACT_PUTT_Y;
            f_ContactVertices[1].v.ob[AXIS_Y] = CONTACT_PUTT_Y;
            f_ContactTextureS = TexelCoordinate(PUTT_CONTACT_IMAGE_WIDTH) - 1;
            f_ContactTextureT = ContactStepT(PUTT_CONTACT_IMAGE_HEIGHT);
        }
        else if (f_ContactStyle == CONTACT_STYLE_WARIO ||
            f_ContactStyle == CONTACT_STYLE_GAME_BOY_BALL_7) {
            StartShotEffect(SHOT_EFFECT_LONG_SHOT);
            SetContactHalfWidth(120);
            f_ContactTextureS = TexelCoordinate(WARIO_CONTACT_IMAGE_WIDTH) - 1;
            f_ContactTextureT = 0x7FF;
            PlaySound(SOUND_CONTACT_WARIO, CONTACT_STYLE_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
        else if (f_ContactStyle == CONTACT_STYLE_RAINBOW) {
            SetContactHalfWidth(60);
            f_ContactTextureS = TexelCoordinate(RAINBOW_CONTACT_IMAGE_WIDTH) - 1;
            f_ContactTextureT = 0;
            PlaySound(SOUND_CONTACT_RAINBOW, CONTACT_STYLE_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
        else if (f_ContactStyle == CONTACT_STYLE_BOWSER) {
            SetContactHalfWidth(20);
            f_ContactTextureS = TexelCoordinate(2 * BOWSER_CONTACT_IMAGE_WIDTH) - 1;
            f_ContactTextureT = ContactStepT(2 * BOWSER_CONTACT_IMAGE_HEIGHT);
            PlaySound(SOUND_CONTACT_MARIO, CONTACT_STYLE_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
        else if (f_ContactStyle == CONTACT_STYLE_MARIO) {
            SetContactHalfWidth(160);
            f_ContactTextureS = TexelCoordinate(PLAIN_CONTACT_IMAGE_WIDTH) - 1;
            f_ContactTextureT = ContactStepT(PLAIN_CONTACT_IMAGE_HEIGHT);
            PlaySound(SOUND_CONTACT_MARIO, CONTACT_STYLE_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
        else if (f_ContactStyle == CONTACT_STYLE_GAME_BOY_BALL_5) {
            SetContactHalfWidth(30);
            f_ContactTextureS = TexelCoordinate(PLAIN_CONTACT_IMAGE_WIDTH) - 1;
            f_ContactTextureT = ContactStepT(PLAIN_CONTACT_IMAGE_HEIGHT);
            PlaySound(SOUND_CONTACT_PLAIN, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
        else {
            SetContactHalfWidth(20);
            f_ContactTextureS = TexelCoordinate(PLAIN_CONTACT_IMAGE_WIDTH) - 1;
            f_ContactTextureT = ContactStepT(PLAIN_CONTACT_IMAGE_HEIGHT);
            PlaySound(SOUND_CONTACT_PLAIN, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
    }
}

void LoadContactImages(Player *player) {
    EncodedAsset asset;

    switch (GetContactStyle(player)) {
        case CONTACT_STYLE_WARIO:
        case CONTACT_STYLE_GAME_BOY_BALL_7:
            OpenEncodedAsset(ASSET_WARIO_CONTACT_IMAGE, &asset);
            ReadEncodedAsset(g_SharedEffectImages + CONTACT_IMAGE_OFFSET, &asset);
            OpenEncodedAsset(ASSET_WARIO_CONTACT_SECOND_IMAGE, &asset);
            ReadEncodedAsset(g_SharedEffectImages + CONTACT_SECOND_IMAGE_OFFSET, &asset);
            break;
        case CONTACT_STYLE_RAINBOW:
            OpenEncodedAsset(ASSET_RAINBOW_CONTACT_IMAGE, &asset);
            ReadEncodedAsset(g_SharedEffectImages + CONTACT_IMAGE_OFFSET, &asset);
            break;
        case CONTACT_STYLE_MARIO:
            OpenEncodedAsset(ASSET_MARIO_CONTACT_IMAGE, &asset);
            ReadEncodedAsset(g_SharedEffectImages + CONTACT_IMAGE_OFFSET, &asset);
            break;
        case CONTACT_STYLE_BOWSER:
            OpenEncodedAsset(ASSET_BOWSER_CONTACT_IMAGE, &asset);
            ReadEncodedAsset(g_SharedEffectImages + CONTACT_IMAGE_OFFSET, &asset);
            break;
    }
}

#define CONTACT_AGE_MAX 255
#define CONTACT_FADE_STEP 4
#define CONTACT_ALPHA_MIN 4
#define CONTACT_WARIO_ALPHA 200
#define CONTACT_WARIO_HOLD_AGE 8
#define CONTACT_WARIO_FADE_PER_AGE 30
#define CONTACT_CHARACTER_ALPHA 160
#define CONTACT_CHARACTER_FADE_PER_AGE 3.2f
#define CONTACT_SHARED_ALPHA 96
#define CONTACT_SHARED_FADE_PER_AGE 2.0f
#define CONTACT_HOLD_AGE 100
#define CONTACT_TRAIL_LENGTH 80
#define CONTACT_RIBBON_SCALE 0.1f
#define CONTACT_PALETTE_SIZE 0x200
#define CONTACT_IMAGE_PIXELS_OFFSET \
    (CONTACT_IMAGE_OFFSET + IMAGE_HEADER_SIZE + CONTACT_PALETTE_SIZE)
#define CONTACT_PUTT_IMAGE_OFFSET 0x648
#define CONTACT_SPIN_DEGREES_PER_FRAME 70.0f
#define CONTACT_SPIN_SCALE_PER_ALPHA (1.0f / 512)
#define CONTACT_SPIN_ALPHA 230

#define DrawContactSegment(segment, firstVertex) \
    { \
        s32 slot = (f_ContactTrailHead + (segment)) % CONTACT_TRAIL_LENGTH; \
\
        gSPMatrix( \
            gfx++, \
            PhysicalAddress(&f_ContactMatrices[slot]), \
            G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW \
        ); \
        gSPMatrix( \
            gfx++, \
            PhysicalAddress(&f_ContactRibbonMatrices[g_DrawBufferIndex]), \
            G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW \
        ); \
        gSPVertex(gfx++, f_ContactVertices, 2, firstVertex); \
        gSPPopMatrix(gfx++, G_MTX_MODELVIEW); \
        gSPModifyVertex(gfx++, firstVertex, G_MWO_POINT_ST, f_ContactTextureT * (segment)); \
        gSPModifyVertex( \
            gfx++, \
            (firstVertex) + 1, \
            G_MWO_POINT_ST, \
            TextureCoordinateWord(f_ContactTextureS, f_ContactTextureT * (segment)) \
        ); \
    }

#define DrawContactTriangles(segmentIndex) \
    for ((segmentIndex) = 0; (segmentIndex) != CONTACT_RIBBON_RELOAD_SEGMENTS; (segmentIndex)++) { \
        gSP2Triangles( \
            gfx++, \
            ((segmentIndex) * 2) + 1, \
            (segmentIndex) * 2, \
            ((segmentIndex) * 2) + 2, \
            0, \
            ((segmentIndex) * 2) + 1, \
            ((segmentIndex) * 2) + 2, \
            ((segmentIndex) * 2) + 3, \
            0 \
        ); \
    }

#define DrawContactRibbon(segmentIndex) \
    { \
        for ((segmentIndex) = 0; (segmentIndex) != CONTACT_RIBBON_FIRST_SEGMENTS; \
            (segmentIndex)++) { \
            DrawContactSegment((segmentIndex), (segmentIndex) * 2); \
        } \
        DrawContactTriangles(segmentIndex); \
        if (f_ContactStyle != CONTACT_STYLE_SHOT) { \
            for (pass = 0; pass != CONTACT_RIBBON_RELOAD_PASSES; pass++) { \
                for ((segmentIndex) = ContactRunStart(pass, 0); \
                    (segmentIndex) != ContactRunStart(pass, 1); \
                    (segmentIndex)++) { \
                    DrawContactSegment( \
                        (segmentIndex), \
                        ((CONTACT_RIBBON_RELOAD_SEGMENTS - 1) * 2) - \
                            (((segmentIndex) - ContactRunStart(pass, 0)) * 2) \
                    ); \
                } \
                DrawContactTriangles(segmentIndex); \
                for ((segmentIndex) = ContactRunStart(pass, 1); \
                    (segmentIndex) != ContactRunStart(pass, 2); \
                    (segmentIndex)++) { \
                    DrawContactSegment( \
                        (segmentIndex), \
                        (((segmentIndex) - ContactRunStart(pass, 1)) * 2) + 2 \
                    ); \
                } \
                DrawContactTriangles(segmentIndex); \
            } \
        } \
    }

void DrawContactEffect(Gfx **displayListHead, CameraMatrices *matrices) {
    Mat4 scale;
    Mat4 antiBillboard;
    Mat4 ribbon;
    Mat4 spin;
    Vec3 translation;
    Vec3 rotation;
    Vec3 spinScale;
    Gfx *gfx = *displayListHead;
    s32 alpha;
    /* Counts both contact matrices and ribbon segments; separate
       variables allocate registers differently. */
    s32 sharedIndex;
    s32 pass;

    if (g_ContactEffect == CONTACT_EFFECT_NONE) {
        return;
    }

    if (f_IsContactEffectFading == TRUE) {
        g_ContactEffect += CONTACT_FADE_STEP;
        if (g_ContactEffect > CONTACT_AGE_MAX) {
            g_ContactEffect = CONTACT_AGE_MAX;
        }
    }
    else if (g_ContactEffect < CONTACT_AGE_MAX) {
        g_ContactEffect++;
    }

    if (g_IsCupViewShown == TRUE) {
        return;
    }

    if (f_ContactStyle == CONTACT_STYLE_WARIO || f_ContactStyle == CONTACT_STYLE_GAME_BOY_BALL_7) {
        if (g_ContactEffect < CONTACT_WARIO_HOLD_AGE) {
            alpha = CONTACT_WARIO_ALPHA;
        }
        else {
            alpha = CONTACT_WARIO_ALPHA -
                ((g_ContactEffect - CONTACT_WARIO_HOLD_AGE) * CONTACT_WARIO_FADE_PER_AGE);
        }
    }
    else if (f_ContactStyle == CONTACT_STYLE_MARIO) {
        alpha = CONTACT_WARIO_ALPHA - g_ContactEffect;
    }
    else if (f_ContactStyle == CONTACT_STYLE_RAINBOW) {
        if (g_ContactEffect < CONTACT_HOLD_AGE) {
            alpha = CONTACT_CHARACTER_ALPHA;
        }
        else {
            alpha = CONTACT_CHARACTER_ALPHA -
                ((g_ContactEffect - CONTACT_HOLD_AGE) * CONTACT_CHARACTER_FADE_PER_AGE);
        }
    }
    else if (f_ContactStyle == CONTACT_STYLE_PLAIN) {
        if (g_ContactEffect < CONTACT_HOLD_AGE) {
            alpha = CONTACT_CHARACTER_ALPHA;
        }
        else {
            alpha = CONTACT_CHARACTER_ALPHA -
                ((g_ContactEffect - CONTACT_HOLD_AGE) * CONTACT_CHARACTER_FADE_PER_AGE);
        }
    }
    else if (f_ContactStyle == CONTACT_STYLE_BOWSER) {
        if (g_ContactEffect < CONTACT_HOLD_AGE) {
            alpha = CONTACT_CHARACTER_ALPHA;
        }
        else {
            alpha = CONTACT_CHARACTER_ALPHA -
                ((g_ContactEffect - CONTACT_HOLD_AGE) * CONTACT_CHARACTER_FADE_PER_AGE);
        }
    }
    else if (g_ContactEffect < CONTACT_HOLD_AGE) {
        alpha = CONTACT_SHARED_ALPHA;
    }
    else {
        alpha = CONTACT_SHARED_ALPHA -
            (CONTACT_SHARED_FADE_PER_AGE * (g_ContactEffect - CONTACT_HOLD_AGE));
    }

    if (alpha < CONTACT_ALPHA_MIN) {
        g_ContactEffect = CONTACT_EFFECT_NONE;
    }

    if (alpha > 0) {
        if (g_ContactEffect == 1) {
            f_ContactTrailHead = 0;
            for (sharedIndex = 0; sharedIndex != CONTACT_MATRIX_COUNT; sharedIndex++) {
                guTranslate(
                    &f_ContactMatrices[sharedIndex],
                    g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                    (g_CurrentShot.ballPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE) -
                        (g_PinVertexScale * CONTACT_DROP_PER_PIN_SCALE),
                    g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE
                );
            }
        }
        else {
            guTranslate(
                &f_ContactMatrices[f_ContactTrailHead],
                g_TurnBallSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE,
                (g_TurnBallSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE) -
                    (g_PinVertexScale * CONTACT_DROP_PER_PIN_SCALE),
                g_TurnBallSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE
            );
        }

        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPPipeSync(gfx++);
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
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
        gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
        gDPSetEnvColor(gfx++, 255, 255, 255, OPAQUE_ALPHA);
        if (f_ContactStyle == CONTACT_STYLE_WARIO) {
            gDPSetPrimColor(gfx++, 0, 0, 255, 0, 255, alpha);
        }
        else if (f_ContactStyle == CONTACT_STYLE_GAME_BOY_BALL_7) {
            gDPSetEnvColor(gfx++, 255, 255, 127, OPAQUE_ALPHA);
            gDPSetPrimColor(
                gfx++,
                0,
                0,
                guRandom() % 255,
                guRandom() % 255,
                guRandom() % 255,
                alpha
            );
        }
        else if (f_ContactStyle == CONTACT_STYLE_BOWSER) {
            gDPSetPrimColor(gfx++, 0, 0, 255, 0, 255, alpha);
            gDPSetPrimColor(gfx++, 0, 0, 255, 0, 0, alpha);
            gDPSetEnvColor(gfx++, 255, 247, 239, OPAQUE_ALPHA);
        }
        else if (f_ContactStyle == CONTACT_STYLE_MARIO) {
            gDPSetPrimColor(gfx++, 0, 0, 255, 0, 0, alpha);
        }
        else if (f_ContactStyle == CONTACT_STYLE_PLAIN ||
            f_ContactStyle == CONTACT_STYLE_GAME_BOY_BALL_5 ||
            f_ContactStyle == CONTACT_STYLE_PUTT) {
            gDPSetPrimColor(gfx++, 0, 0, 0, 0, 255, alpha);
        }
        else if (f_ContactStyle == CONTACT_STYLE_GAME_BOY_BALL_6) {
            gDPSetPrimColor(gfx++, 0, 0, 255, 127, 0, alpha);
            gDPSetEnvColor(gfx++, 255, 191, 63, OPAQUE_ALPHA);
        }
        else {
            gDPSetPrimColor(gfx++, 0, 0, 191, 191, 255, alpha);
        }

        if (f_ContactStyle == CONTACT_STYLE_BOWSER || f_ContactStyle == CONTACT_STYLE_RAINBOW) {
            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                TEXEL0,
                TEXEL0,
                0,
                PRIMITIVE,
                0,
                0,
                0,
                0,
                TEXEL0,
                TEXEL0,
                0,
                PRIMITIVE,
                0
            );
        }
        else {
            gDPSetCombineLERP(
                gfx++,
                ENVIRONMENT,
                PRIMITIVE,
                TEXEL0,
                PRIMITIVE,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                ENVIRONMENT,
                PRIMITIVE,
                TEXEL0,
                PRIMITIVE,
                PRIMITIVE,
                0,
                TEXEL0,
                0
            );
        }

        if (f_ContactStyle == CONTACT_STYLE_RAINBOW) {
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            gDPLoadTLUT_pal256(
                gfx++,
                TextureAddress(ImageBody(g_SharedEffectImages + CONTACT_IMAGE_OFFSET))
            );
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(g_SharedEffectImages + CONTACT_IMAGE_PIXELS_OFFSET),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                RAINBOW_CONTACT_IMAGE_WIDTH,
                RAINBOW_CONTACT_IMAGE_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                7,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }
        else if (f_ContactStyle == CONTACT_STYLE_BOWSER) {
            gDPPipeSync(gfx++);
            gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
            gDPSetBlendColor(gfx++, 255, 255, 255, 1);
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            gDPLoadTLUT_pal256(
                gfx++,
                TextureAddress(ImageBody(g_SharedEffectImages + CONTACT_IMAGE_OFFSET))
            );
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(g_SharedEffectImages + CONTACT_IMAGE_PIXELS_OFFSET),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                BOWSER_CONTACT_IMAGE_WIDTH,
                BOWSER_CONTACT_IMAGE_HEIGHT,
                0,
                G_TX_MIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                TextureMask(BOWSER_CONTACT_IMAGE_WIDTH),
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }
        else if (f_ContactStyle == CONTACT_STYLE_WARIO ||
            f_ContactStyle == CONTACT_STYLE_GAME_BOY_BALL_7) {
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(ImageBody(g_SharedEffectImages + CONTACT_IMAGE_OFFSET)),
                G_IM_FMT_I,
                WARIO_CONTACT_IMAGE_WIDTH,
                WARIO_CONTACT_IMAGE_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                TextureMask(WARIO_CONTACT_IMAGE_HEIGHT),
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }
        else if (f_ContactStyle == CONTACT_STYLE_PLAIN ||
            f_ContactStyle == CONTACT_STYLE_GAME_BOY_BALL_6 ||
            f_ContactStyle == CONTACT_STYLE_GAME_BOY_BALL_5) {
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(ImageBody(g_SharedEffectImages)),
                G_IM_FMT_I,
                PLAIN_CONTACT_IMAGE_WIDTH,
                PLAIN_CONTACT_IMAGE_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }
        else if (f_ContactStyle == CONTACT_STYLE_PUTT) {
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(ImageBody(g_SharedEffectImages + CONTACT_PUTT_IMAGE_OFFSET)),
                G_IM_FMT_I,
                PUTT_CONTACT_IMAGE_WIDTH,
                PUTT_CONTACT_IMAGE_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }
        else {
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(ImageBody(g_SharedEffectImages)),
                G_IM_FMT_I,
                PLAIN_CONTACT_IMAGE_WIDTH,
                PLAIN_CONTACT_IMAGE_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }

        guScaleF(scale, CONTACT_RIBBON_SCALE, CONTACT_RIBBON_SCALE, CONTACT_RIBBON_SCALE);
        guMtxL2F(antiBillboard, &matrices->antiBillboard);
        MultiplyMatrices(scale, antiBillboard, ribbon);
        guMtxF2L(ribbon, &f_ContactRibbonMatrices[g_DrawBufferIndex]);
        if (f_ContactStyle != CONTACT_STYLE_MARIO) {
            gDPPipeSync(gfx++);
            DrawContactRibbon(sharedIndex);
            if (f_ContactStyle == CONTACT_STYLE_WARIO ||
                f_ContactStyle == CONTACT_STYLE_GAME_BOY_BALL_7) {
                gDPLoadTextureBlock_4b(
                    gfx++,
                    TextureAddress(ImageBody(g_SharedEffectImages + CONTACT_SECOND_IMAGE_OFFSET)),
                    G_IM_FMT_I,
                    WARIO_CONTACT_IMAGE_WIDTH,
                    WARIO_CONTACT_IMAGE_HEIGHT,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    TextureMask(WARIO_CONTACT_IMAGE_HEIGHT),
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gDPPipeSync(gfx++);
                DrawContactRibbon(sharedIndex);
            }
        }

        if (f_ContactStyle == CONTACT_STYLE_MARIO) {
            translation[AXIS_X] = translation[AXIS_Y] = translation[AXIS_Z] = 0.0f;
            f_ContactSpinAngle += CONTACT_SPIN_DEGREES_PER_FRAME;
            rotation[AXIS_X] = 0.0f;
            rotation[AXIS_Y] = 0.0f;
            rotation[AXIS_Z] = f_ContactSpinAngle;
            spinScale[AXIS_Z] = alpha * CONTACT_SPIN_SCALE_PER_ALPHA;
            spinScale[AXIS_Y] = spinScale[AXIS_Z];
            spinScale[AXIS_X] = spinScale[AXIS_Y];
            BuildTransformMatrix(spin, rotation, translation, spinScale);
            ConvertMatrixToFixed(spin, &f_ContactSpinMatrices[g_MatrixBufferIndex]);
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            gDPLoadTLUT_pal256(
                gfx++,
                TextureAddress(ImageBody(g_SharedEffectImages + CONTACT_IMAGE_OFFSET))
            );
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, CONTACT_SPIN_ALPHA);
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(g_SharedEffectImages + CONTACT_IMAGE_PIXELS_OFFSET),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                MARIO_CONTACT_IMAGE_WIDTH,
                MARIO_CONTACT_IMAGE_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
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
            gDPPipeSync(gfx++);
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_ContactMatrices[f_ContactTrailHead]),
                G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
            );
            gSPMatrix(
                gfx++,
                PhysicalAddress(&matrices->antiBillboard),
                G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
            );
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_ContactSpinMatrices[g_MatrixBufferIndex]),
                G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
            );
            gSPVertex(gfx++, f_ContactSpinVertices, QUAD_VERTEX_COUNT, 0);
            gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
            gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
        }

        f_ContactTrailHead--;
        if (f_ContactTrailHead < 0) {
            f_ContactTrailHead = CONTACT_TRAIL_LENGTH - 1;
        }

        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void ShowWindFlyGuy(s32 show) {
    EncodedAsset asset;

    if (f_IsMovieBufferAllocated == TRUE) {
        if (show == FLY_GUY_HIDDEN) {
            f_WindFlyGuyFrame = FLY_GUY_HIDDEN;
            return;
        }

        f_WindFlyGuyFrame = 0;
        f_WindFlyGuyAsset = ASSET_FLY_GUY_IMAGE;
        OpenEncodedAsset(ASSET_FLY_GUY_IMAGE, &asset);
        ReadEncodedAsset(f_MovieBuffer, &asset);
        OpenEncodedAsset(ASSET_STREAK_IMAGE, &asset);
        ReadEncodedAsset(f_MovieBuffer + FLY_GUY_IMAGE_SIZE, &asset);
        f_LoadedBanner = BANNER_NONE;
        f_CurrentBanner = BANNER_NONE;
        ResetPath();
    }
}

#define WIND_FLY_GUY_PATH_POINT_COUNT 17
#define WIND_FLY_GUY_PATH_PACE PathPace(1, 2)
#define WIND_FLY_GUY_FRAME_COUNT 14
#define WIND_FLY_GUY_STARTING 0
#define WIND_FLY_GUY_FLYING 1
#define WIND_FLY_GUY_ARRIVED 2
#define WIND_FLY_GUY_GONE_FRAME 20
#define WIND_STREAK_ALPHA 190
#define WIND_STREAK_FADE_STEP 10
#define WIND_STREAK_HALF_WIDTH 10
#define WIND_STREAK_LENGTH 80
#define WIND_STREAK_FIRST_RUN 16
#define WIND_STREAK_SECOND_RUN_END 31
#define WIND_STREAK_QUADS_PER_RUN 15
#define WIND_STREAK_LAST_SLOT ((WIND_STREAK_QUADS_PER_RUN - 1) * 2)
#define WIND_STREAK_TEXTURE_S 0x5FF
#define WIND_STREAK_TEXTURE_T_SHIFT 8
#define WIND_STREAK_IMAGE_MASK_T 7
#define WIND_STREAK_DEPTH (-300.0f)
#define FLY_GUY_IMAGE_WIDTH 32
#define FLY_GUY_IMAGE_HEIGHT 32
#define WIND_FLY_GUY_SOUND_CHANNEL 10
#define WIND_FLY_GUY_SOUND_VOLUME 63

void DrawWindFlyGuy(Gfx **displayListHead) {
    EncodedAsset asset;
    PathPoint position;
    Vec3 translation;
    Vec3 rotation;
    Mat4 matrix;
    Gfx *gfx = *displayListHead;
    s32 alpha;
    s32 i;
    s32 slotsBack;
    s32 streakSlot;

    if (f_WindFlyGuyFrame == FLY_GUY_HIDDEN || g_PauseScreen) {
        return;
    }

    f_WindFlyGuyAsset++;
    if (f_WindFlyGuyAsset == ASSET_FLY_GUY_IMAGE + WIND_FLY_GUY_FRAME_COUNT) {
        f_WindFlyGuyAsset = ASSET_FLY_GUY_IMAGE;
    }

    OpenEncodedAsset(f_WindFlyGuyAsset, &asset);
    ReadEncodedAsset(f_MovieBuffer, &asset);
    if (f_WindFlyGuyFrame < WIND_FLY_GUY_ARRIVED) {
        if (StepAlongPath(
                f_WindFlyGuyPath,
                WIND_FLY_GUY_PATH_POINT_COUNT,
                WIND_FLY_GUY_PATH_PACE,
                &position
            ) == PATH_END_REACHED) {
            f_WindFlyGuyFrame = WIND_FLY_GUY_ARRIVED;
        }

        alpha = WIND_STREAK_ALPHA;
    }
    else {
        f_WindFlyGuyFrame++;
        alpha = WIND_STREAK_ALPHA - (f_WindFlyGuyFrame * WIND_STREAK_FADE_STEP);
        if (alpha < 0) {
            alpha = 0;
        }

        if (f_WindFlyGuyFrame == WIND_FLY_GUY_GONE_FRAME) {
            f_WindFlyGuyFrame = FLY_GUY_HIDDEN;
        }

        position = f_WindFlyGuyExit;
    }

    if (f_WindFlyGuyFrame == WIND_FLY_GUY_STARTING) {
        f_WindFlyGuyLastPosition = f_WindFlyGuyPath[0];
        f_WindStreakHead = 0;
        SetContactHalfWidth(WIND_STREAK_HALF_WIDTH);
        for (i = 0; i != CONTACT_MATRIX_COUNT; i++) {
            guTranslate(
                &f_ContactMatrices[i],
                position.x - (SCREEN_WIDTH / 2),
                (SCREEN_HEIGHT / 2) - position.y,
                WIND_STREAK_DEPTH
            );
        }

        PlaySound(SOUND_WIND_FLY_GUY, WIND_FLY_GUY_SOUND_CHANNEL, WIND_FLY_GUY_SOUND_VOLUME);
        f_WindFlyGuyFrame = WIND_FLY_GUY_FLYING;
    }

    rotation[AXIS_X] = 0.0f;
    rotation[AXIS_Y] = 0.0f;
    rotation[AXIS_Z] =
        atan2(f_WindFlyGuyLastPosition.x - position.x, f_WindFlyGuyLastPosition.y - position.y);
    translation[AXIS_Z] = WIND_STREAK_DEPTH;
    translation[AXIS_X] = position.x - (SCREEN_WIDTH / 2);
    translation[AXIS_Y] = (SCREEN_HEIGHT / 2) - position.y;
    BuildRotationMatrix(matrix, rotation, translation);
    ConvertMatrixToFixed(matrix, &f_ContactMatrices[f_WindStreakHead]);
    f_WindFlyGuyLastPosition = position;
    SetUpTextDrawing(&gfx);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
    gDPSetEnvColor(gfx++, 255, 255, 255, OPAQUE_ALPHA);
    gDPSetCombineLERP(
        gfx++,
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
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    LoadNibbleTextureBlock(
        &gfx,
        ImageBody(f_MovieBuffer + FLY_GUY_IMAGE_SIZE),
        G_IM_FMT_I,
        STREAK_IMAGE_WIDTH,
        STREAK_IMAGE_HEIGHT,
        0,
        G_TX_MIRROR,
        G_TX_MIRROR,
        G_TX_NOMASK,
        WIND_STREAK_IMAGE_MASK_T,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&g_ScreenOrthoMatrix),
        G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
    );
    gDPPipeSync(gfx++);
    for (i = 0; i != WIND_STREAK_FIRST_RUN; i++) {
        streakSlot = (f_WindStreakHead + i) % WIND_STREAK_LENGTH;
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_ContactMatrices[streakSlot]),
            G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH
        );
        gSPVertex(gfx++, f_ContactVertices, 2, i * 2);
        gSPModifyVertex(gfx++, i * 2, G_MWO_POINT_ST, i << WIND_STREAK_TEXTURE_T_SHIFT);
        gSPModifyVertex(
            gfx++,
            (i * 2) + 1,
            G_MWO_POINT_ST,
            TextureCoordinateWord(WIND_STREAK_TEXTURE_S, i << WIND_STREAK_TEXTURE_T_SHIFT)
        );
    }

    for (i = 0; i != WIND_STREAK_QUADS_PER_RUN; i++) {
        gSP2Triangles(
            gfx++,
            (i * 2) + 1,
            i * 2,
            (i * 2) + 2,
            0,
            (i * 2) + 1,
            (i * 2) + 2,
            (i * 2) + 3,
            0
        );
    }

    for (i = WIND_STREAK_FIRST_RUN; i != WIND_STREAK_SECOND_RUN_END; i++) {
        slotsBack = (i - WIND_STREAK_FIRST_RUN) * 2;
        streakSlot = (f_WindStreakHead + i) % WIND_STREAK_LENGTH;
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_ContactMatrices[streakSlot]),
            G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH
        );
        gSPVertex(gfx++, f_ContactVertices, 2, WIND_STREAK_LAST_SLOT - slotsBack);
        gSPModifyVertex(
            gfx++,
            WIND_STREAK_LAST_SLOT - slotsBack,
            G_MWO_POINT_ST,
            i << WIND_STREAK_TEXTURE_T_SHIFT
        );
        gSPModifyVertex(
            gfx++,
            WIND_STREAK_LAST_SLOT + 1 - slotsBack,
            G_MWO_POINT_ST,
            TextureCoordinateWord(WIND_STREAK_TEXTURE_S, i << WIND_STREAK_TEXTURE_T_SHIFT)
        );
    }

    for (i = 0; i != WIND_STREAK_QUADS_PER_RUN; i++) {
        gSP2Triangles(
            gfx++,
            (i * 2) + 1,
            i * 2,
            (i * 2) + 2,
            0,
            (i * 2) + 1,
            (i * 2) + 2,
            (i * 2) + 3,
            0
        );
    }

    f_WindStreakHead--;
    if (f_WindStreakHead < 0) {
        f_WindStreakHead = WIND_STREAK_LENGTH - 1;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    LoadTextureBlock(
        &gfx,
        ImageBody(f_MovieBuffer),
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        FLY_GUY_IMAGE_WIDTH,
        FLY_GUY_IMAGE_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels((s32)position.x - (FLY_GUY_IMAGE_WIDTH / 2)),
        QuarterPixels((s32)position.y - (FLY_GUY_IMAGE_HEIGHT / 2)),
        QuarterPixels((s32)position.x + (FLY_GUY_IMAGE_WIDTH / 2)),
        QuarterPixels((s32)position.y + (FLY_GUY_IMAGE_HEIGHT / 2)),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
