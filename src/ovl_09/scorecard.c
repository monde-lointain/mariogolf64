#include "common.h"
#include "asset.h"
#include "character.h"
#include "character_slot.h"
#include "functions.h"
#include "math_util.h"
#include "reaction.h"
#include "heap.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scorecard.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "tournament.h"
#include "variables.h"
/*
 * The scorecard: between holes it slides in over the course with the
 * players' scores, and with each game mode's own pages, such as the
 * tournament's leaderboard and a match's results.
 */

/* sScorecardState values */
#define SCORECARD_OPENING 0  /* Sliding in */
#define SCORECARD_CLOSING 1  /* Sliding out */
#define SCORECARD_SHOWN 2    /* The player can turn its pages */
#define SCORECARD_TALLYING 3 /* Counting skins before it is shown */
#define SCORECARD_CLOSED 4

#define SOUND_SCORECARD_OPEN 0x47
#define SOUND_SCORECARD_CLOSE 0x48
#define SOUND_SCORECARD_PAGE 0x5C
#define SCORECARD_SOUND_VOLUME 100
#define SCORECARD_SLIDE_SOUND_REVERB 20
/* The opening sound pans from the right, the closing one from the left */
#define SCORECARD_OPEN_SOUND_PAN 0xDF
#define SCORECARD_CLOSE_SOUND_PAN 0x20
#define SOUND_PAN_FULL_RIGHT 0xFF

/* The songs for how the first player did against the second */
#define SONG_GET_CHARACTER_WIN 6
#define SONG_GET_CHARACTER_LOSS 7
#define SONG_GET_CHARACTER_DRAW 8
#define SONG_MATCH_RESULT 10

/* The leaderboard writes its contestants' names with these glyph groups */
#define LEADERBOARD_GLYPH_GROUP_COUNT 20

#define FACE_SIZE 32
#define SMALL_FACE_HEIGHT 24
/* With G_TX_MIRROR, texels from this column on mirror the face */
#define MIRRORED_FACE_MASK 5
/*
 * Texture coordinates have this many fraction bits. Mirrored faces start one
 * face width in, where the mirrored tile reads the face backwards.
 */
#define TEXTURE_COORD_FRACTION_BITS 5

typedef struct Scorecard {
    /* 0x00 */ u8 unk0[0xC];
    /* 0x0C */ u8 *tableImage;
    /* 0x10 */ u8 *unk10;
    /* 0x14 */ u8 *unk14;
    /* 0x18 */ u8 *unk18;
    /* 0x1C */ u8 *labelsImage;
    /* 0x20 */ u8 *pageTitleImages[3];
    /* 0x2C */ u8 *unk2C;
    /* 0x30 */ u8 *digitsImage;
    /* 0x34 */ u8 *unk34;
    /* 0x38 */ u8 *unk38;
    /* 0x3C */ u8 *unk3C;
    /* 0x40 */ u8 *unk40;
    /* 0x44 */ u8 *betImages[BET_COUNT][2];
    /* 0x7C */ u8 unk7C[8];
    /* 0x84 */ u8 *unk84;
    /* 0x88 */ u8 *unk88;
    /* 0x8C */ u8 *unk8C;
    /* 0x90 */ u8 *unk90;
    /* 0x94 */ u8 *unk94;
    /* 0x98 */ u8 *faceImages[MAX_PLAYER_COUNT];
    /* 0xA8 */ u16 *facePalettes[MAX_PLAYER_COUNT];
    /* 0xB8 */ u8 *unkB8;
    /* 0xBC */ u8 unkBC;
    /* 0xBD */ u8 unkBD;
    /* 0xBE */ u8 unkBE;
    /* 0xBF */ s8 unkBF;
    /* 0xC0 */ s32 topRow; /* The contestant at the top of the leaderboard */
    /* 0xC4 */ s32 x; /* The scorecard's offset from its place on the screen */
    /* 0xC8 */ s32 y;
    /* 0xCC */ s32 unkCC;
    /* 0xD0 */ s32 stateTime; /* Frames since sScorecardState changed */
    /* 0xD4 */ s32 unkD4;
    /* 0xD8 */ s32 unkD8;
    /* 0xDC */ s32 layout;
    /* 0xE0 */ s32 pageCount;
    /* 0xE4 */ s32 hasBets;
} Scorecard; /* size = 0xE8 */

/* The leaderboard shows this many of the tournament's contestants at a time */
#define LEADERBOARD_ROW_COUNT 10
#define LEADERBOARD_LAST_TOP_ROW (TOURNAMENT_CONTESTANT_COUNT - LEADERBOARD_ROW_COUNT)
/* Each row starts to flip over this many frames after the one above it */
#define ROW_FLIP_STAGGER 4
#define ROW_FLIP_TIME (LEADERBOARD_ROW_COUNT * ROW_FLIP_STAGGER)
#define ROW_FLIP_DEGREES_PER_FRAME 20
#define ROW_FLIP_END_DEGREES 175
#define SOUND_ROW_FLIPPED 0x5B
#define ROW_FLIP_SOUND_CHANNEL 13

typedef struct ScorecardContent {
    /* 0x00 */ f32 rowFlipAngles[LEADERBOARD_ROW_COUNT];
    /* 0x28 */ s8 isRowFlipping[LEADERBOARD_ROW_COUNT];
    /* 0x34 */ s32 flipTime;
    /* 0x38 */ s32 unk38[MAX_PLAYER_COUNT];
    /* 0x48 */ s32 courseIndex;
    /* 0x4C */ s32 holeIndex;
    /* 0x50 */ s32 unk50;
} ScorecardContent; /* size = 0x54 */

extern Scorecard sScorecard;
extern ScorecardContent sScorecardContent;
extern s32 sScorecardState;
#define RESULT_CHARACTERS_IDLE (-1) /* sResultCharactersLoadStep when no characters are loading */
extern s32 sResultCharactersLoadStep;

void ScrollLeaderboard(u16 buttons);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", OpenScorecard);

/*
 * Closes the scorecard: frees the images that OpenScorecard loaded for the
 * game mode and clears its text slots. It stops any rumble even if the
 * scorecard was already closed.
 *
 * Doesn't match: the target keeps the address it frees the last images from
 * in $s0 and derives $s1 from it, where this keeps it in $s1.
 */
#ifdef NON_MATCHING
void FreeScorecard(void) {
    s32 i;
    s32 imageIndex;

    if (sScorecardState != SCORECARD_CLOSED) {
        sScorecardState = SCORECARD_CLOSED;
        g_RoundHud.scorecardPage = NO_SCORECARD_PAGE;
        g_RoundHud.unk1 = 0;
        g_RoundHud.unk3 = 0;
        /* Only the bets being played have images */
        for (i = 0, imageIndex = 0; i != BET_COUNT; i++) {
            if (g_GameState.betRates[i] != UNSET_BET_RATE) {
                FreeMemory(&sScorecard.betImages[imageIndex][0]);
                FreeMemory(&sScorecard.betImages[imageIndex][1]);
                imageIndex++;
            }
        }
        FreeMemory(&sScorecard.unk84);
        FreeMemory(&sScorecard.pageTitleImages[2]);
        FreeMemory(&sScorecard.pageTitleImages[1]);
        FreeMemory(&sScorecard.pageTitleImages[0]);
        FreeMemory(&sScorecard.unk94);
        switch (g_GameMode) {
            case GAME_MODE_STROKE:
            case GAME_MODE_UNK1:
            case GAME_MODE_UNK3:
            case GAME_MODE_UNK6:
            case GAME_MODE_UNK8:
            case GAME_MODE_RING_SHOT:
                FreeMemory(&sScorecard.unk34);
                break;
            case GAME_MODE_MATCH_GAME:
            case GAME_MODE_GET_CHARACTER:
                FreeMemory(&sScorecard.unk2C);
                FreeMemory(&sScorecard.unk38);
                FreeMemory(&sScorecard.unk3C);
                FreeMemory(&sScorecard.unk40);
                break;
            case GAME_MODE_SKINS_MATCH:
            case GAME_MODE_CLUB_SLOTS:
                FreeMemory(&sScorecard.unk34);
                FreeMemory(&sScorecard.unk38);
                break;
            case GAME_MODE_TOURNAMENT:
                FreeMemory(&sScorecard.unk10);
                FreeMemory(&sScorecard.unk90);
                FreeMemory(&sScorecard.unk88);
                for (i = 0; i != LEADERBOARD_GLYPH_GROUP_COUNT; i++) {
                    PlaceGlyphGroup(i, NO_FONT_SIZE, 0, 0, 0, g_PrintBuffer);
                }
                break;
        }
        FreeMemory(&sScorecard.unk8C);
        FreeMemory(&sScorecard.labelsImage);
        FreeMemory(&sScorecard.digitsImage);
        FreeMemory(&sScorecard.unkB8);
        FreeMemory(&sScorecard.unk18);
        FreeMemory(&sScorecard.unk14);
        FreeMemory(&sScorecard.tableImage);
        for (i = 0; i != g_PlayerCount; i++) {
            FreeMemory(&sScorecard.faceImages[i]);
            FreeMemory(&sScorecard.facePalettes[i]);
        }
        SetTextSlot(0, 0, NO_MESSAGE, 1, 0, 1, 0, 0, 0, 0);
        SetTextSlot(0, 0, NO_MESSAGE, 1, 1, 1, 0, 0, 0, 0);
    }
    ForceStopRumble();
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", FreeScorecard);
#endif

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_80242334_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", SetScorecardLayout);

void SetScorecardState(s32 state) {
    if (state == SCORECARD_OPENING) {
        PlaySoundWithEffects(SOUND_SCORECARD_OPEN, g_NextSoundChannel, SCORECARD_SOUND_VOLUME,
                             SCORECARD_SLIDE_SOUND_REVERB, SCORECARD_OPEN_SOUND_PAN);
    } else if (state == SCORECARD_CLOSING) {
        PlaySoundWithEffects(SOUND_SCORECARD_CLOSE, g_NextSoundChannel, SCORECARD_SOUND_VOLUME,
                             SCORECARD_SLIDE_SOUND_REVERB, SCORECARD_CLOSE_SOUND_PAN);
    }
    if (state == SCORECARD_OPENING) {
        sScorecard.x = SCREEN_WIDTH;
        sScorecard.y = 0;
    }
    sScorecardState = state;
    sScorecard.stateTime = 0;
}

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", AnimateScorecard);

/*
 * Left and right on the control pad turn the pages, round from the last to the first.
 *
 * Doesn't match: the target addresses the page through $a0 and reuses it for
 * the last read, where this uses $v1 and loads the address again.
 */
#ifdef NON_MATCHING
void TurnScorecardPage(u16 buttons) {
    s32 pageCount = sScorecard.pageCount;

    if (pageCount > 1) {
        if (buttons & L_JPAD) {
            g_RoundHud.scorecardPage--;
            if (g_RoundHud.scorecardPage < 0) {
                g_RoundHud.scorecardPage = pageCount - 1;
            }
        } else if (buttons & R_JPAD) {
            g_RoundHud.scorecardPage++;
            if (g_RoundHud.scorecardPage > pageCount - 1) {
                g_RoundHud.scorecardPage = 0;
            }
        } else {
            return;
        }
        /* The sound pans further right on each later page */
        PlaySoundWithEffects(SOUND_SCORECARD_PAGE, g_NextSoundChannel, SCORECARD_SOUND_VOLUME, 0,
                             (g_RoundHud.scorecardPage + 1) * (SOUND_PAN_FULL_RIGHT / (pageCount + 1)));
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", TurnScorecardPage);
#endif

/* Flips the leaderboard's rows over, and scrolls it with up and down on the control pad */
void ScrollLeaderboard(u16 buttons) {
    s32 row;

    if (sScorecardContent.flipTime < ROW_FLIP_TIME) {
        sScorecardContent.flipTime++;
    }
    for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
        if (row * ROW_FLIP_STAGGER < sScorecardContent.flipTime && sScorecardContent.isRowFlipping[row] == TRUE) {
            sScorecardContent.rowFlipAngles[row] += DEGREES_TO_RADIANS(ROW_FLIP_DEGREES_PER_FRAME);
            if (sScorecardContent.rowFlipAngles[row] > DEGREES_TO_RADIANS(ROW_FLIP_END_DEGREES)) {
                sScorecardContent.rowFlipAngles[row] = 0.0f;
                sScorecardContent.isRowFlipping[row] = FALSE;
                PlaySound(SOUND_ROW_FLIPPED, ROW_FLIP_SOUND_CHANNEL, SCORECARD_SOUND_VOLUME);
            }
        }
    }
    if (g_RoundHud.scorecardPage == 0) {
        if (buttons & U_JPAD) {
            if (sScorecard.topRow > 0) {
                sScorecard.topRow--;
                PlaySound(SOUND_SCORECARD_PAGE, g_NextSoundChannel, SCORECARD_SOUND_VOLUME);
            }
        }
        if ((buttons & D_JPAD) && sScorecard.topRow < LEADERBOARD_LAST_TOP_ROW) {
            sScorecard.topRow++;
            PlaySound(SOUND_SCORECARD_PAGE, g_NextSoundChannel, SCORECARD_SOUND_VOLUME);
        }
    }
}

void UpdateScorecard(u16 buttons, u16 pressedButtons) {
    AnimateScorecard();
    if (sScorecardState == SCORECARD_SHOWN) {
        if (pressedButtons & (A_BUTTON | B_BUTTON)) {
            SetScorecardState(SCORECARD_CLOSING);
            return;
        }
        switch (g_GameMode) {
            case GAME_MODE_STROKE:
            case GAME_MODE_UNK1:
            case GAME_MODE_MATCH_GAME:
            case GAME_MODE_UNK3:
            case GAME_MODE_GET_CHARACTER:
            case GAME_MODE_UNK6:
            case GAME_MODE_UNK7:
            case GAME_MODE_UNK8:
            case GAME_MODE_RING_SHOT:
            case GAME_MODE_SKINS_MATCH:
            case GAME_MODE_CLUB_SLOTS:
                break;
            case GAME_MODE_TOURNAMENT:
                ScrollLeaderboard(buttons);
                break;
        }
        TurnScorecardPage(buttons);
    }
}

s32 UpdateShownScorecard(u16 buttons) {
    AnimateScorecard();
    if (sScorecardState != SCORECARD_SHOWN) {
        return FALSE;
    }
    switch (g_GameMode) {
        case GAME_MODE_STROKE:
        case GAME_MODE_UNK1:
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_UNK3:
        case GAME_MODE_GET_CHARACTER:
        case GAME_MODE_UNK6:
        case GAME_MODE_UNK7:
        case GAME_MODE_UNK8:
        case GAME_MODE_RING_SHOT:
        case GAME_MODE_SKINS_MATCH:
        case GAME_MODE_CLUB_SLOTS:
            break;
        case GAME_MODE_TOURNAMENT:
            ScrollLeaderboard(buttons);
            break;
    }
    TurnScorecardPage(buttons);
    return TRUE;
}

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_8024373C_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", DrawScorecard);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_80243C48_ovl_09);

void FreeResultCharacters(void) {
    FreeCharacterSlot(0);
    FreeCharacterSlot(1);
    sResultCharactersLoadStep = RESULT_CHARACTERS_IDLE;
}

/*
 * Starts loading the two players' characters to react to who did better on
 * the last hole, and plays the result's song. Returns FALSE while the
 * character slots are still busy.
 *
 * Doesn't match: the target loads the first player's stroke count for the
 * song into $a0, where this uses $a1.
 */
#ifdef NON_MATCHING
s32 LoadResultCharacters(void) {
    s32 holeIndex = sScorecardContent.holeIndex - 1;
    s32 song;

    if (IsCharacterSlotBusy(0) || IsCharacterSlotBusy(1)) {
        FreeCharacterSlot(0);
        FreeCharacterSlot(1);
        return FALSE;
    }
    LoadCharacter(0, (s8)g_Players[0].characterIndex + CHARACTER_COUNT, (s8)g_Players[0].unk7B);
    LoadCharacter(1, (s8)g_Players[1].characterIndex + CHARACTER_COUNT, (s8)g_Players[1].unk7B);
    song = SONG_MATCH_RESULT;
    if (g_GameMode == GAME_MODE_GET_CHARACTER) {
        if (g_Players[0].holes[holeIndex].strokes < g_Players[1].holes[holeIndex].strokes) {
            song = SONG_GET_CHARACTER_WIN;
        } else if (g_Players[1].holes[holeIndex].strokes < g_Players[0].holes[holeIndex].strokes) {
            song = SONG_GET_CHARACTER_LOSS;
        } else {
            song = SONG_GET_CHARACTER_DRAW;
        }
    }
    PlaySong(song, 0);
    if (g_Players[0].holes[holeIndex].strokes < g_Players[1].holes[holeIndex].strokes) {
        SetCharacterAnimation(0, REACTION_ANIMATION_BIRDIE);
        SetCharacterAnimation(1, REACTION_ANIMATION_BOGEY);
    } else if (g_Players[1].holes[holeIndex].strokes < g_Players[0].holes[holeIndex].strokes) {
        SetCharacterAnimation(0, REACTION_ANIMATION_BOGEY);
        SetCharacterAnimation(1, REACTION_ANIMATION_BIRDIE);
    } else {
        SetCharacterAnimation(0, REACTION_ANIMATION_PAR);
        SetCharacterAnimation(1, REACTION_ANIMATION_PAR);
    }
    SetAnimationSpeed(0, 1.0f);
    SetAnimationSpeed(1, 1.0f);
    return TRUE;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", LoadResultCharacters);
#endif

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", TallyScorecardSkins);

void DrawScorecardFace(Gfx **displayListHead, s32 left, s32 top, s32 playerIndex) {
    Gfx *gfx = *displayListHead;

    LoadLargePalette(&gfx, sScorecard.facePalettes[playerIndex]);
    LoadTextureBlock(&gfx, sScorecard.faceImages[playerIndex], G_IM_FMT_CI, G_IM_SIZ_8b, FACE_SIZE, FACE_SIZE, 0, 0, 0,
                     0, 0, 0, 0);
    DrawTextureRect(&gfx, left << 2, top << 2, (left + FACE_SIZE) << 2, (top + FACE_SIZE) << 2, G_TX_RENDERTILE, 0, 0,
                    TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    *displayListHead = gfx;
}

void DrawSmallFace(Gfx **displayListHead, s32 left, s32 top, s32 playerIndex, s32 isMirrored) {
    Gfx *gfx = *displayListHead;

    LoadLargePalette(&gfx, g_PlayerFacePalettes[playerIndex]);
    LoadTextureBlock(&gfx, g_PlayerFaceImages[playerIndex], G_IM_FMT_CI, G_IM_SIZ_8b, FACE_SIZE, SMALL_FACE_HEIGHT, 0,
                     G_TX_MIRROR, G_TX_MIRROR, isMirrored * MIRRORED_FACE_MASK, 0, 0, 0);
    DrawTextureRect(&gfx, left << 2, top << 2, (left + FACE_SIZE) << 2, (top + SMALL_FACE_HEIGHT) << 2,
                    G_TX_RENDERTILE, isMirrored * (FACE_SIZE << TEXTURE_COORD_FRACTION_BITS), 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_802445C0_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_80244748_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_802448D4_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C7BC_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C7C4_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C7CC_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C7D0_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_80244CA0_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C7E0_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C7E8_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C7F0_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C7F4_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C7F8_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_80245184_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C820_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_802460F8_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C848_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_80246804_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C868_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C86C_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_802478B0_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_80247D80_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_80247E3C_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_802493DC_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C960_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C964_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C968_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C96C_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C974_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C97C_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C984_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C988_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C990_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C994_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C998_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C9A0_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C9A8_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C9B0_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C9B8_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_8024C1D8_ovl_09);

