#include "common.h"
#include "asset.h"
#include "character.h"
#include "controller.h"
#include "cpu_projection.h"
#include "game_flags.h"
#include "heap.h"
#include "math_util.h"
#include "movie.h"
#include "round_events.h"
#include "save.h"
#include "screen_layers.h"
#include "shot_effects.h"
#include "text.h"
#include "transfer_pak.h"
#include "variables.h"

/* The buffer that a movie's images, or the wind Fly Guy's, are read into */
#define MOVIE_BUFFER_SIZE 0x6270
/* The movie frame after which the hole intro goes on */
#define MOVIE_CUE_FRAME 12

#define FLY_GUY_IMAGE_ASSET 0x6CF
#define WIND_STREAK_IMAGE_ASSET 0x6AF
#define FLY_GUY_IMAGE_SIZE 0x808 /* With its header; the wind streak follows it */

/* Game Boy balls 1 to 4 copy a character's contact effect, and 5 to 7 have their own */
#define GAME_BOY_BALL_TYPE_PLAIN 0
#define GAME_BOY_BALL_TYPE_MARIO 1
#define GAME_BOY_BALL_TYPE_BOWSER 2
#define GAME_BOY_BALL_TYPE_WARIO 3
#define GAME_BOY_BALL_TYPE_HEAVY_HITTER 4
#define GAME_BOY_BALL_TYPE_5 5
#define GAME_BOY_BALL_TYPE_6 6
#define GAME_BOY_BALL_TYPE_7 7

/* Where the contact effect's images go in g_SharedEffectImages, and their assets */
#define CONTACT_IMAGE_OFFSET 0x970
#define CONTACT_SECOND_IMAGE_OFFSET 0x1578
#define MARIO_CONTACT_IMAGE_ASSET 0x6BE
#define BOWSER_CONTACT_IMAGE_ASSET 0x6AE
#define WARIO_CONTACT_IMAGE_ASSET 0x6A7
#define WARIO_CONTACT_SECOND_IMAGE_ASSET 0x6A8
#define HEAVY_HITTER_CONTACT_IMAGE_ASSET 0x6A6

/* A banner's letters, each an image drawn on its own square */
typedef struct Banner {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ u16 imageAsset;
    /* 0x4 */ s8 letterCount;
    /* 0x5 */ u8 letterWidth;
    /* 0x6 */ u8 letterHeight;
    /* 0x7 */ s8 animation;
} Banner; /* size = 0x8 */

/* Where a letter of the shown banner is, and its matrix in each matrix buffer */
typedef struct BannerLetter {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 scale;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ Mtx matrices[2];
} BannerLetter; /* size = 0x90 */

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
extern s32 sIsMovieBufferAllocated;
extern u8 *sMovieBuffer;
extern s32 sCurrentMovie;
extern s32 sMovieFrame;
/* Counts up to MOVIE_TICKS_PER_FRAME between the frames of a Monty Mole movie */
extern s32 sMovieTick;
/* Sets the render state for drawing a movie's paletted strips */
extern Gfx sMovieSetupDisplayList[];
extern s32 sMovieBackBuffer; /* Which of a two-buffer movie's frame buffers to read into next */
extern u16 sToadMovieFrameAssets[];
extern u16 sMontyMoleMovieFrameAssets[];
extern u16 sGoombaMovieFrameAssets[];
/* Counts the frames that DrawMovie has drawn, up to MOVIE_DRAW_COUNT_MAX */
extern s32 sMovieDrawCount;
extern f32 sLakituX;
extern f32 sLakituY;
extern f32 sLakituScale;
extern s32 sWindFlyGuyFrame;
extern s32 sWindFlyGuyAsset;
extern s32 sIsFlipMessageLeaving;
extern s32 sFlipMessageFrame; /* FLIP_MESSAGE_FRAME_NONE while no message is shown */
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
/* The board starts face down and turns this far each frame until it faces the camera */
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
#define ASSET_LAKITU 0x64E
#define LAKITU_BACK_IMAGE_OFFSET 0x1008
#define LAKITU_START_X 384.0f
#define LAKITU_START_Y -64.0f
#define LAKITU_START_SCALE 0.7f
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
/* The controller whose buttons play movies under GAME_FLAG_TEST_AIM_TILT_AND_MOVIES */
#define MOVIE_TEST_CONTROLLER 1

void ResetPath(void);
void func_802104BC_ovl_00(Gfx **displayListHead);
void func_80210F40_ovl_00(Gfx **displayListHead);
void func_8021169C_ovl_00(Gfx **displayListHead);
void func_80211D60_ovl_00(Gfx **displayListHead);
void DrawMontyMoleMovie(Gfx **displayListHead);
void DrawGoombaMovie(Gfx **displayListHead);
void func_80213038_ovl_00(Gfx **displayListHead);
void func_80213634_ovl_00(Gfx **displayListHead);

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

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", StartBanner);

/* Draws the number banner's "+" and digits */
void DrawNumberBanner(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
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
        LoadTextureBlock(&gfx, sMovieBuffer + (imageOffset + BANNER_LETTER_IMAGES_OFFSET),
                         G_IM_FMT_CI, G_IM_SIZ_8b, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, 0, 0, 0,
                         0, 0, 0, 0);
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sBannerLetters[i].matrices[g_MatrixBufferIndex]),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(gfx++, sBannerLetterVertices, 4, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Draws the shown banner's letters, each placed within the whole banner's matrix */
void DrawBannerLetters(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 i;

    for (i = 0; i != sBanners[sCurrentBanner].letterCount; i++) {
        LoadTextureBlock(&gfx, sMovieBuffer + BANNER_LETTER_IMAGES_OFFSET + i * BANNER_LETTER_IMAGE_SIZE, G_IM_FMT_CI,
                         G_IM_SIZ_8b, BANNER_LETTER_IMAGE_WIDTH, BANNER_LETTER_IMAGE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sBannerMatrices[g_MatrixBufferIndex]),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sBannerLetters[i].matrices[g_MatrixBufferIndex]),
                  G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        gSPVertex(gfx++, sBannerLetterVertices, 4, 0);
        gSP2Triangles(gfx++, 0, 1, 3, 0, 1, 2, 3, 0);
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void func_8020EEEC_ovl_00(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_8020EEF8_ovl_00);

#ifdef NON_MATCHING
/*
 * Flips a message in on a board in the middle of the screen, sized to fit it. FLIP_MESSAGE_LEAVE
 * flips the board back out, and FLIP_MESSAGE_RESET takes it away at once.
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
    halfSize = PlaceMessageGlyphGroup(FLIP_MESSAGE_GLYPH_GROUP, FONT_SIZE_LARGE, 0, 0, messageId);
    halfWidth = (u32)halfSize >> 16;
    sFlipMessageHalfWidth = halfWidth;
    sFlipMessageBoardVertices[0].v.ob[0] = -halfWidth - FLIP_MESSAGE_BOARD_MARGIN_X;
    sFlipMessageBoardVertices[1].v.ob[0] = -halfWidth - FLIP_MESSAGE_BOARD_MARGIN_X;
    sFlipMessageBoardVertices[2].v.ob[0] = halfWidth + FLIP_MESSAGE_BOARD_MARGIN_X;
    sFlipMessageBoardVertices[3].v.ob[0] = halfWidth + FLIP_MESSAGE_BOARD_MARGIN_X;
    halfHeight = halfSize & 0xFFFF;
    sFlipMessageHalfHeight = halfHeight;
    sFlipMessageBoardVertices[0].v.ob[1] = halfHeight + FLIP_MESSAGE_BOARD_MARGIN_Y;
    sFlipMessageBoardVertices[3].v.ob[1] = halfHeight + FLIP_MESSAGE_BOARD_MARGIN_Y;
    sFlipMessageBoardVertices[1].v.ob[1] = -halfHeight - FLIP_MESSAGE_BOARD_MARGIN_Y;
    sFlipMessageBoardVertices[2].v.ob[1] = -halfHeight - FLIP_MESSAGE_BOARD_MARGIN_Y;
    SetTextSlot(FLIP_MESSAGE_X, FLIP_MESSAGE_Y, sFlipMessageId, FLIP_MESSAGE_FONT, FLIP_MESSAGE_TEXT_SLOT,
                TEXT_FLAG_CENTERED | TEXT_FLAG_UNK20, 127, 255, 127, 255);
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", ShowFlipMessage);
#endif

/* Turns the flip message board a frame further in or out, then draws it and its text */
void DrawFlipMessage(Gfx **displayListHead) {
    Vec3 scale;
    Vec3 translation;
    Vec3 rotation;
    Mat4 matrix;
    Mat4 hingeMatrix;
    Gfx *gfx = *displayListHead;
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
        angle = DEGREES_TO_RADIANS(sFlipMessageFrame * FLIP_MESSAGE_DEGREES_PER_FRAME + FLIP_MESSAGE_START_DEGREES);
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
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sFlipMessageMatrices[g_MatrixBufferIndex]),
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

s32 IsMovieFinished(void) {
    return sCurrentMovie == MOVIE_NONE;
}

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
            sLakituX = LAKITU_START_X;
            sLakituY = LAKITU_START_Y;
            sLakituScale = LAKITU_START_SCALE;
            break;
        case MOVIE_MONTY_MOLE_UNK2:
        case MOVIE_MONTY_MOLE_UNK3:
        case MOVIE_MONTY_MOLE_UNK4:
        case MOVIE_MONTY_MOLE:
            OpenAssetFile(ASSET_MONTY_MOLE_MOVIE_PALETTE, &file);
            ReadAssetFile(sMovieBuffer + MOVIE_PALETTE_OFFSET, GetAssetFileSize(&file), &file);
            OpenEncodedAsset(sMontyMoleMovieFrameAssets[0], &asset);
            ReadEncodedAsset(sMovieBuffer + MOVIE_FRAME_OFFSET, &asset);
            break;
        case MOVIE_GOOMBA:
            OpenAssetFile(ASSET_GOOMBA_MOVIE_PALETTE, &file);
            ReadAssetFile(sMovieBuffer + MOVIE_PALETTE_OFFSET, GetAssetFileSize(&file), &file);
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

s32 IsMoviePastCue(void) {
    return sMovieFrame >= MOVIE_CUE_FRAME;
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
    switch (sCurrentMovie) {
        case MOVIE_NONE:
            break;
        case MOVIE_TOAD_OUT_OF_BOUNDS:
            func_80213634_ovl_00(&gfx);
            break;
        case MOVIE_LAKITU:
            func_80213038_ovl_00(&gfx);
            break;
        case MOVIE_MONTY_MOLE:
            DrawMontyMoleMovie(&gfx);
            break;
        case MOVIE_GOOMBA:
            DrawGoombaMovie(&gfx);
            break;
        case MOVIE_LONGEST_DRIVE:
        case MOVIE_NEAREST_PIN:
            func_802104BC_ovl_00(&gfx);
            break;
        case MOVIE_DRIVE_RECORD:
            func_80210F40_ovl_00(&gfx);
            break;
        case MOVIE_ALBATROSS_PUTT:
        case MOVIE_EAGLE_PUTT:
        case MOVIE_BIRDIE_PUTT:
        case MOVIE_SINK_TO_WIN:
        case MOVIE_SINK_TO_TIE:
        case MOVIE_MISS_AND_LOSE:
            func_8021169C_ovl_00(&gfx);
            break;
        case MOVIE_READY_GO:
            func_80211D60_ovl_00(&gfx);
            break;
    }
    if (sMovieDrawCount < MOVIE_DRAW_COUNT_MAX) {
        sMovieDrawCount++;
    }
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_802104BC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80210F40_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_8021169C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80211D60_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80212590_ovl_00);

/* A Monty Mole pops out of the green */
void DrawMontyMoleMovie(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
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
        PlaySoundWithEffects(SOUND_MONTY_MOLE, MOVIE_SOUND_CHANNEL, MOVIE_SOUND_VOLUME, MOVIE_SOUND_REVERB,
                             MOVIE_SOUND_PAN);
    }
    gSPDisplayList(gfx++, sMovieSetupDisplayList);
    gDPLoadTLUT_pal256(gfx++, TEXTURE_ADDRESS(sMovieBuffer + MOVIE_PALETTE_OFFSET));
    for (i = 0; i != MONTY_MOLE_STRIP_COUNT; i++) {
        gDPLoadTextureTile(gfx++, TEXTURE_ADDRESS(sMovieBuffer + (MOVIE_FRAME_OFFSET + i * MONTY_MOLE_STRIP_SIZE)),
                           G_IM_FMT_CI, G_IM_SIZ_8b, MONTY_MOLE_WIDTH, MONTY_MOLE_STRIP_HEIGHT, 0, 0,
                           MONTY_MOLE_WIDTH - 1, MONTY_MOLE_STRIP_HEIGHT - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        DrawTextureRect(&gfx, MONTY_MOLE_X << 2, (MONTY_MOLE_Y + i * MONTY_MOLE_STRIP_HEIGHT) << 2,
                        (MONTY_MOLE_X + MONTY_MOLE_WIDTH) << 2,
                        (MONTY_MOLE_Y + MONTY_MOLE_STRIP_HEIGHT + i * MONTY_MOLE_STRIP_HEIGHT) << 2,
                        G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    }
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* A Goomba hops out of the green */
void DrawGoombaMovie(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
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
        PlaySoundWithEffects(SOUND_GOOMBA, MOVIE_SOUND_CHANNEL, MOVIE_SOUND_VOLUME, MOVIE_SOUND_REVERB,
                             MOVIE_SOUND_PAN);
    }
    gSPDisplayList(gfx++, sMovieSetupDisplayList);
    gDPLoadTLUT_pal256(gfx++, TEXTURE_ADDRESS(sMovieBuffer + MOVIE_PALETTE_OFFSET));
    for (i = 0; i != GOOMBA_STRIP_COUNT; i++) {
        if (i == GOOMBA_STRIP_COUNT - 1) {
            height = GOOMBA_LAST_STRIP_HEIGHT;
        } else {
            height = GOOMBA_STRIP_HEIGHT;
        }
        gDPLoadTextureTile(gfx++, TEXTURE_ADDRESS(sMovieBuffer + (MOVIE_FRAME_OFFSET + i * GOOMBA_STRIP_SIZE)),
                           G_IM_FMT_CI, G_IM_SIZ_8b, GOOMBA_WIDTH, height, 0, 0, GOOMBA_WIDTH - 1, height - 1, 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
        DrawTextureRect(&gfx, GOOMBA_X << 2, (GOOMBA_Y + i * GOOMBA_STRIP_HEIGHT) << 2, (GOOMBA_X + GOOMBA_WIDTH) << 2,
                        (GOOMBA_Y + i * GOOMBA_STRIP_HEIGHT + height) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    }
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80213038_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80213634_ovl_00);

s32 IsContactEffectShown(void) {
    return g_ContactEffect != CONTACT_EFFECT_NONE;
}

/* Returns the CONTACT_STYLE_* of the player's character, or for a Game Boy one, of its ball */
s32 GetContactStyle(UnkB8 *player) {
    GameBoyCharacter *gameBoyCharacter;
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
    if (character == CHARACTER_BABY_MARIO || character == CHARACTER_MARIO || character == CHARACTER_METAL_MARIO) {
        return CONTACT_STYLE_MARIO;
    }
    return CONTACT_STYLE_PLAIN;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", StartContactEffect);

/* Reads the images of the player's contact effect */
void LoadContactImages(UnkB8 *player) {
    EncodedAsset asset;

    switch (GetContactStyle(player)) {
        case CONTACT_STYLE_WARIO:
        case CONTACT_STYLE_GAME_BOY_BALL_7:
            OpenEncodedAsset(WARIO_CONTACT_IMAGE_ASSET, &asset);
            ReadEncodedAsset(g_SharedEffectImages + CONTACT_IMAGE_OFFSET, &asset);
            OpenEncodedAsset(WARIO_CONTACT_SECOND_IMAGE_ASSET, &asset);
            ReadEncodedAsset(g_SharedEffectImages + CONTACT_SECOND_IMAGE_OFFSET, &asset);
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

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80214250_ovl_00);

/* Shows the Fly Guy that warns of a strong wind, or hides it given FLY_GUY_HIDDEN */
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

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_802161D4_ovl_00);
