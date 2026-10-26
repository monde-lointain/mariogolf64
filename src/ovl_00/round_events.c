#include "common.h"
#include "asset.h"
#include "character.h"
#include "heap.h"
#include "movie.h"
#include "round_events.h"
#include "save.h"
#include "shot_effects.h"
#include "text.h"
#include "transfer_pak.h"

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

extern s32 sMovieUnk14;
extern s32 sMovieUnk20;
extern s32 sIsMovieBufferAllocated;
extern u8 *sMovieBuffer;
extern s32 sCurrentMovie;
extern s32 sMovieFrame;
extern s32 sWindFlyGuyFrame;
extern s32 sWindFlyGuyAsset;
extern s32 sIsFlipMessageLeaving;
extern s32 sFlipMessageFrame; /* FLIP_MESSAGE_FRAME_NONE while no message is shown */
extern s32 sFlipMessageId;
extern s32 sFlipMessageHalfWidth;
extern s32 sFlipMessageHalfHeight;
extern Vtx sFlipMessageBoardVertices[4];

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

void ResetPath(void);

void AllocMovieBuffer(void) {
    EncodedAsset unused;

    sMovieUnk14 = -1;
    sIsMovieBufferAllocated = TRUE;
    sMovieBuffer = AllocMemory(MOVIE_BUFFER_SIZE);
    sMovieUnk20 = -1;
}

void FreeMovieBuffer(void) {
    sIsMovieBufferAllocated = FALSE;
    FreeMemory(&sMovieBuffer);
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", StartBanner);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_8020EBBC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_8020ED54_ovl_00);

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

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_8020FC10_ovl_00);

s32 IsMovieFinished(void) {
    return sCurrentMovie == MOVIE_NONE;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", StartMovie);

s32 IsMoviePastCue(void) {
    return sMovieFrame >= MOVIE_CUE_FRAME;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80210370_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_802104BC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80210F40_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_8021169C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80211D60_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80212590_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_802129C0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80212CE0_ovl_00);

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
        sMovieUnk20 = -1;
        sMovieUnk14 = -1;
        ResetPath();
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_802161D4_ovl_00);
