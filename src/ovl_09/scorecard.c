#include "common.h"
#include "asset.h"
#include "burst.h"
#include "character.h"
#include "character_slot.h"
#include "cpu_projection.h"
#include "functions.h"
#include "heap.h"
#include "hole_display.h"
#include "math_util.h"
#include "reaction.h"
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

/* The layouts that SetScorecardLayout picks for the game modes */
#define SCORECARD_LAYOUT_STROKE 0
#define SCORECARD_LAYOUT_MATCH 1 /* Match games and Get Character */
#define SCORECARD_LAYOUT_SKINS 2 /* Skins matches and club slots */
#define SCORECARD_LAYOUT_TOURNAMENT 3
#define SCORECARD_LAYOUT_UNK4 4 /* For GAME_MODE_UNK7 */

/*
 * The tallies of a match's hole happen at these frames of the TALLYING state.
 * A skins match carries a halved hole's skins over at SKINS_TALLY_TIME, or
 * from then hands a won hole's skins over one every SKINS_TALLY_INTERVAL.
 */
#define MATCH_RESULT_SOUND_TIME 1
#define MATCH_RESULT_TIME 5
#define SKINS_TALLY_TIME 20
#define SKINS_TALLY_INTERVAL 4
#define SOUND_UNK22 0x22
#define SOUND_UNK67 0x67
#define SOUND_SKIN_COUNTED 0x6B
#define TALLY_SOUND_CHANNEL 13
#define MATCH_RESULT_SOUND_CHANNEL 14

#define SCORECARD_OPEN_TIME 12  /* Frames the scorecard takes to slide in */
#define SCORECARD_CLOSE_TIME 10 /* Frames before a closing scorecard is freed */
#define SCORECARD_SLIDE_SPEED 32
#define SCORECARD_CLOSE_ACCELERATION 8
#define ROUND_FRAMEBUFFER_COUNT 3
/* The faces step through their images once every FACE_FRAME_TIME frames */
#define FACE_FRAME_COUNT 5
#define FACE_FRAME_TIME 3

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

/* Images wider than this are drawn in two strips */
#define MAX_UNSPLIT_IMAGE_WIDTH 96

/*
 * placesImage holds the left halves of the four places' images, then their
 * right halves, each half PLACE_HALF_WIDTH by PLACE_HEIGHT. The right half
 * overlaps the left one.
 */
#define PLACE_IMAGE_COUNT 4
#define PLACE_HALF_WIDTH 32
#define PLACE_HEIGHT 32
#define PLACE_HALF_SIZE (PLACE_HALF_WIDTH * PLACE_HEIGHT)
#define PLACE_RIGHT_HALF_X 24
#define PLACE_RIGHT_HALVES_OFFSET (LARGE_PALETTED_TEXELS_OFFSET + PLACE_IMAGE_COUNT * PLACE_HALF_SIZE)

/* labelsImage holds four-bit labels of this size, one after another */
#define LABEL_WIDTH 80
#define LABEL_HEIGHT 18
#define LABEL_SIZE (LABEL_WIDTH * LABEL_HEIGHT / 2)
#define LABEL_SHOWN_WIDTH 54

/* Where DrawBetTallies draws the players' rows, from the scorecard's left */
#define BET_TALLY_LEFT 64
#define BET_TALLY_TOP 96
#define BET_TALLY_COLUMN_SPACING 34
#define BET_TALLY_ROW_SPACING 36
#define BET_TOTAL_LEFT 302

/* Where a leaderboard row's columns start in its text */
#define LEADERBOARD_SCORE_COLUMN 12
#define LEADERBOARD_HOLES_COLUMN 18
#define SMALL_FACE_HEIGHT 24
/* With G_TX_MIRROR, texels from this column on mirror the face */
#define MIRRORED_FACE_MASK 5
/*
 * Texture coordinates have this many fraction bits. Mirrored faces start one
 * face width in, where the mirrored tile reads the face backwards.
 */
#define TEXTURE_COORD_FRACTION_BITS 5

#define LEADERBOARD_LAST_TOP_ROW (TOURNAMENT_CONTESTANT_COUNT - LEADERBOARD_ROW_COUNT)
/* Each row starts to flip over this many frames after the one above it */
#define ROW_FLIP_STAGGER 4
#define ROW_FLIP_TIME (LEADERBOARD_ROW_COUNT * ROW_FLIP_STAGGER)
#define ROW_FLIP_DEGREES_PER_FRAME 20
#define ROW_FLIP_END_DEGREES 175
#define SOUND_ROW_FLIPPED 0x5B
#define ROW_FLIP_SOUND_CHANNEL 13

extern s32 sScorecardState;
#define RESULT_CHARACTERS_IDLE (-1) /* sResultCharactersLoadStep when no characters are loading */
/* Match games are between two players */
#define MATCH_PLAYER_COUNT 2
/* The step counts up to LOADING, then waits for LoadResultCharacters */
#define RESULT_CHARACTERS_LOADING 2
#define RESULT_CHARACTERS_LOADED 3
extern s32 sMatchResultFrameCount;
extern Mtx sResultCharacterMatrices[MATCH_PLAYER_COUNT];
/* "Hole %d" in half-width katakana */
extern const char sHoleNumberFormat[];
/* Each player's result in a match. func_802460F8 shows sWinText too. */
extern const char sDrawText[];
extern const char sWinText[];
extern const char sLoseText[];

/* Where DrawMatchResult draws its text */
#define MATCH_TITLE_X 32
#define MATCH_TITLE_Y 28
#define MATCH_HOLE_X 136
#define MATCH_HOLE_Y 48
#define MATCH_RESULT_LEFT 64
#define MATCH_RESULT_SPACING 128
#define MATCH_RESULT_Y 64
/* And where it draws the characters */
#define RESULT_CHARACTER_LEFT -56.0f
#define RESULT_CHARACTER_SPACING 112.0f
#define RESULT_CHARACTER_Y -80.0f
#define RESULT_CHARACTER_Z -500.0f
#define RESULT_CHARACTER_SCALE 5.0f
extern s32 sResultCharactersLoadStep;
extern const char sDecimalFormat[];

void SetFramebufferCount(s32 count);
void SetTextColor(Gfx **displayListHead, s32 red, s32 green, s32 blue);

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
                FreeMemory(&g_Scorecard.betImages[imageIndex][0]);
                FreeMemory(&g_Scorecard.betImages[imageIndex][1]);
                imageIndex++;
            }
        }
        FreeMemory(&g_Scorecard.unk84);
        FreeMemory(&g_Scorecard.pageTitleImages[2]);
        FreeMemory(&g_Scorecard.pageTitleImages[1]);
        FreeMemory(&g_Scorecard.pageTitleImages[0]);
        FreeMemory(&g_Scorecard.unk94);
        switch (g_GameMode) {
            case GAME_MODE_STROKE:
            case GAME_MODE_UNK1:
            case GAME_MODE_UNK3:
            case GAME_MODE_UNK6:
            case GAME_MODE_UNK8:
            case GAME_MODE_RING_SHOT:
                FreeMemory(&g_Scorecard.placesImage);
                break;
            case GAME_MODE_MATCH_GAME:
            case GAME_MODE_GET_CHARACTER:
                FreeMemory(&g_Scorecard.unk2C);
                FreeMemory(&g_Scorecard.unk38);
                FreeMemory(&g_Scorecard.unk3C);
                FreeMemory(&g_Scorecard.unk40);
                break;
            case GAME_MODE_SKINS_MATCH:
            case GAME_MODE_CLUB_SLOTS:
                FreeMemory(&g_Scorecard.placesImage);
                FreeMemory(&g_Scorecard.unk38);
                break;
            case GAME_MODE_TOURNAMENT:
                FreeMemory(&g_Scorecard.unk10);
                FreeMemory(&g_Scorecard.unk90);
                FreeMemory(&g_Scorecard.unk88);
                for (i = 0; i != LEADERBOARD_GLYPH_GROUP_COUNT; i++) {
                    PlaceGlyphGroup(i, NO_FONT_SIZE, 0, 0, 0, g_PrintBuffer);
                }
                break;
        }
        FreeMemory(&g_Scorecard.unk8C);
        FreeMemory(&g_Scorecard.labelsImage);
        FreeMemory(&g_Scorecard.digitsImage);
        FreeMemory(&g_Scorecard.unkB8);
        FreeMemory(&g_Scorecard.unk18);
        FreeMemory(&g_Scorecard.unk14);
        FreeMemory(&g_Scorecard.tableImage);
        for (i = 0; i != g_PlayerCount; i++) {
            FreeMemory(&g_Scorecard.faceImages[i]);
            FreeMemory(&g_Scorecard.facePalettes[i]);
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
        g_Scorecard.x = SCREEN_WIDTH;
        g_Scorecard.y = 0;
    }
    sScorecardState = state;
    g_Scorecard.stateTime = 0;
}

/* Slides the scorecard in or out, tallies the last hole, and animates the faces on it */
void AnimateScorecard(void) {
    EncodedAsset asset;
    s32 i;
    s32 slideSpeed;

    switch (sScorecardState) {
        case SCORECARD_TALLYING:
            if (TallyScorecardSkins()) {
                sScorecardState = SCORECARD_SHOWN;
            }
            g_Scorecard.stateTime++;
            break;
        case SCORECARD_OPENING:
            g_Scorecard.stateTime++;
            if (g_Scorecard.stateTime == SCORECARD_OPEN_TIME) {
                g_Scorecard.stateTime = 0;
                if (g_Scorecard.isTallied) {
                    sScorecardState = SCORECARD_SHOWN;
                } else {
                    sScorecardState = SCORECARD_TALLYING;
                }
            }
            if (g_Scorecard.x > 0) {
                if (g_Scorecard.x < 2) {
                    g_Scorecard.x = 0;
                }
                g_Scorecard.x -= g_Scorecard.x / 2;
            } else if (g_Scorecard.x < 0) {
                g_Scorecard.x += SCORECARD_SLIDE_SPEED;
            }
            if (g_Scorecard.y > 0) {
                g_Scorecard.y -= SCORECARD_SLIDE_SPEED;
                if (g_Scorecard.y < 0) {
                    g_Scorecard.y = 0;
                }
            } else if (g_Scorecard.y < 0) {
                g_Scorecard.y += SCORECARD_SLIDE_SPEED;
                if (g_Scorecard.y > 0) {
                    g_Scorecard.y = 0;
                }
            }
            break;
        case SCORECARD_CLOSING:
            g_Scorecard.stateTime++;
            if (g_Scorecard.x > -SCREEN_WIDTH) {
                slideSpeed = g_Scorecard.stateTime * SCORECARD_CLOSE_ACCELERATION;
                if (slideSpeed > SCORECARD_SLIDE_SPEED * 2) {
                    slideSpeed = SCORECARD_SLIDE_SPEED * 2;
                }
                g_Scorecard.x -= slideSpeed;
            }
            /* The target stores the y offset back unchanged here */
            g_Scorecard.y = g_Scorecard.y;
            if (g_Scorecard.stateTime == SCORECARD_CLOSE_TIME) {
                FreeScorecard();
                g_PauseScreen = PAUSE_SCREEN_NONE;
                SetMenuBackdrop(MENU_BACKDROP_NONE);
                SetFramebufferCount(ROUND_FRAMEBUFFER_COUNT);
                SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                g_RoundHud.isLetterboxShown = TRUE;
                g_RoundHud.unkC = 1;
                g_RoundHud.unk7 = 1;
                g_RoundHud.unk6 = 0;
                g_RoundHud.unk8 = 1;
                g_RoundHud.unk9 = 1;
                g_RoundHud.unkA = 1;
                g_RoundHud.unkB = 1;
                g_RoundHud.unkD = 1;
            }
            break;
        case SCORECARD_SHOWN:
            if (g_Scorecard.faceTime++ % FACE_FRAME_TIME == 1) {
                g_Scorecard.faceFrame++;
                g_Scorecard.unkBC ^= 1;
                if (g_Scorecard.faceFrame < FACE_FRAME_COUNT) {
                    for (i = 0; i != g_PlayerCount; i++) {
                        OpenEncodedAsset(g_CharacterImageAssets[(s8)g_Players[i].characterIndex] +
                                             g_Scorecard.faceFrame + g_Scorecard.content.faceImageOffsets[i],
                                         &asset);
                        ReadEncodedAsset(g_Scorecard.faceImages[i], &asset);
                    }
                } else {
                    g_Scorecard.faceFrame = FACE_FRAME_COUNT - 1;
                }
            }
            break;
    }
}

/*
 * Left and right on the control pad turn the pages, round from the last to the first.
 *
 * Doesn't match: the target addresses the page through $a0 and reuses it for
 * the last read, where this uses $v1 and loads the address again.
 */
#ifdef NON_MATCHING
void TurnScorecardPage(u16 buttons) {
    s32 pageCount = g_Scorecard.pageCount;

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

    if (g_Scorecard.content.flipTime < ROW_FLIP_TIME) {
        g_Scorecard.content.flipTime++;
    }
    for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
        if (row * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime && g_Scorecard.content.isRowFlipping[row] == TRUE) {
            g_Scorecard.content.rowFlipAngles[row] += DEGREES_TO_RADIANS(ROW_FLIP_DEGREES_PER_FRAME);
            if (g_Scorecard.content.rowFlipAngles[row] > DEGREES_TO_RADIANS(ROW_FLIP_END_DEGREES)) {
                g_Scorecard.content.rowFlipAngles[row] = 0.0f;
                g_Scorecard.content.isRowFlipping[row] = FALSE;
                PlaySound(SOUND_ROW_FLIPPED, ROW_FLIP_SOUND_CHANNEL, SCORECARD_SOUND_VOLUME);
            }
        }
    }
    if (g_RoundHud.scorecardPage == 0) {
        if (buttons & U_JPAD) {
            if (g_Scorecard.topRow > 0) {
                g_Scorecard.topRow--;
                PlaySound(SOUND_SCORECARD_PAGE, g_NextSoundChannel, SCORECARD_SOUND_VOLUME);
            }
        }
        if ((buttons & D_JPAD) && g_Scorecard.topRow < LEADERBOARD_LAST_TOP_ROW) {
            g_Scorecard.topRow++;
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

/*
 * Draws an eight-bit image at its own size. Images wider than
 * MAX_UNSPLIT_IMAGE_WIDTH are loaded and drawn as two strips, top then bottom.
 */
void DrawLargeImage(Gfx **displayListHead, u8 *image, s32 left, s32 top) {
    Gfx *gfx = *displayListHead;
    s32 width = (image[IMAGE_WIDTH_OFFSET] << 8) | image[IMAGE_WIDTH_OFFSET + 1];
    s32 height = (image[IMAGE_HEIGHT_OFFSET] << 8) | image[IMAGE_HEIGHT_OFFSET + 1];
    s32 stripCount;
    s32 stripIndex;

    stripCount = 1;
    if (width > MAX_UNSPLIT_IMAGE_WIDTH) {
        stripCount = 2;
    }
    LoadLargePalette(&gfx, image + IMAGE_HEADER_SIZE);
    height /= stripCount;
    for (stripIndex = 0; stripIndex != stripCount; stripIndex++) {
        LoadTextureBlock(&gfx, image + LARGE_PALETTED_TEXELS_OFFSET + stripIndex * width * height, G_IM_FMT_CI,
                         G_IM_SIZ_8b, width, height, 0, 0, 0, 0, 0, 0, 0);
        DrawTextureRect(&gfx, left << 2, top << 2, (left + width) << 2, (top + height) << 2, G_TX_RENDERTILE, 0, 0,
                        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        top += height;
    }
    *displayListHead = gfx;
}

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
    s32 holeIndex = g_Scorecard.content.holeIndex - 1;
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

/*
 * Counts the last hole's result onto the scorecard. Returns TRUE once it is done.
 *
 * Doesn't match: the code matches, but this puts its jump table in .rodata
 * without the word of padding after it, which moves the rodata that follows
 * 4 bytes early.
 */
#ifdef NON_MATCHING
s32 TallyScorecardSkins(void) {
    switch (g_Scorecard.layout) {
        case SCORECARD_LAYOUT_STROKE:
        case SCORECARD_LAYOUT_TOURNAMENT:
        case SCORECARD_LAYOUT_UNK4:
            return TRUE;
        case SCORECARD_LAYOUT_MATCH:
            if (g_Scorecard.stateTime == MATCH_RESULT_SOUND_TIME && g_Scorecard.holeWinnerIndex != HOLE_WINNER_HALVED) {
                PlaySound(SOUND_UNK22, TALLY_SOUND_CHANNEL, SCORECARD_SOUND_VOLUME);
            }
            if (g_Scorecard.stateTime == MATCH_RESULT_TIME) {
                g_Scorecard.skinsCarryover.onLastHole = 1;
                if (g_Scorecard.holeWinnerIndex != HOLE_WINNER_HALVED) {
                    PlaySound(SOUND_UNK67, MATCH_RESULT_SOUND_CHANNEL, SCORECARD_SOUND_VOLUME);
                }
                return TRUE;
            }
            break;
        case SCORECARD_LAYOUT_SKINS:
            if (g_Scorecard.holeWinnerIndex == HOLE_WINNER_HALVED) {
                if (g_Scorecard.stateTime == SKINS_TALLY_TIME) {
                    g_Scorecard.skinsCarryover.intoLastHole++;
                    PlaySound(SOUND_SKIN_COUNTED, TALLY_SOUND_CHANNEL, SCORECARD_SOUND_VOLUME);
                    return TRUE;
                }
            } else if (g_Scorecard.stateTime >= SKINS_TALLY_TIME) {
                if (g_Scorecard.skinsCarryover.onLastHole <= 0) {
                    return TRUE;
                }
                if ((g_Scorecard.stateTime - SKINS_TALLY_TIME) % SKINS_TALLY_INTERVAL == 1) {
                    if (g_Scorecard.skinsCarryover.intoLastHole > 0) {
                        g_Scorecard.skinsCarryover.intoLastHole--;
                    }
                    g_Scorecard.skinsCarryover.onLastHole--;
                    PlaySound(SOUND_SKIN_COUNTED, TALLY_SOUND_CHANNEL, SCORECARD_SOUND_VOLUME);
                }
            }
            break;
    }
    return FALSE;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", TallyScorecardSkins);
#endif

void DrawScorecardFace(Gfx **displayListHead, s32 left, s32 top, s32 playerIndex) {
    Gfx *gfx = *displayListHead;

    LoadLargePalette(&gfx, g_Scorecard.facePalettes[playerIndex]);
    LoadTextureBlock(&gfx, g_Scorecard.faceImages[playerIndex], G_IM_FMT_CI, G_IM_SIZ_8b, FACE_SIZE, FACE_SIZE, 0, 0, 0,
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
                    G_TX_RENDERTILE, isMirrored * (FACE_SIZE << TEXTURE_COORD_FRACTION_BITS), 0, TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE);
    *displayListHead = gfx;
}

/* Draws a place, from 0 for first, as the left and right halves of its image */
void DrawPlace(Gfx **displayListHead, s32 left, s32 top, s32 place) {
    Gfx *gfx = *displayListHead;

    LoadLargePalette(&gfx, g_Scorecard.placesImage + IMAGE_HEADER_SIZE);
    LoadTextureBlock(&gfx, g_Scorecard.placesImage + (place * PLACE_HALF_SIZE + PLACE_RIGHT_HALVES_OFFSET), G_IM_FMT_CI,
                     G_IM_SIZ_8b, PLACE_HALF_WIDTH, PLACE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    DrawTextureRect(&gfx, (left + PLACE_RIGHT_HALF_X) << 2, top << 2,
                    (left + PLACE_RIGHT_HALF_X + PLACE_HALF_WIDTH) << 2, (top + PLACE_HEIGHT) << 2, G_TX_RENDERTILE,
                    0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    LoadTextureBlock(&gfx, g_Scorecard.placesImage + (place * PLACE_HALF_SIZE + LARGE_PALETTED_TEXELS_OFFSET),
                     G_IM_FMT_CI, G_IM_SIZ_8b, PLACE_HALF_WIDTH, PLACE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    DrawTextureRect(&gfx, left << 2, top << 2, (left + PLACE_HALF_WIDTH) << 2, (top + PLACE_HEIGHT) << 2,
                    G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    *displayListHead = gfx;
}

/* Draws one of the labels in labelsImage, which shows only its left LABEL_SHOWN_WIDTH texels */
void DrawScorecardLabel(Gfx **displayListHead, s32 left, s32 top, s32 labelIndex) {
    Gfx *gfx = *displayListHead;

    LoadSmallPalette(&gfx, 0, g_Scorecard.labelsImage + IMAGE_HEADER_SIZE);
    gDPLoadTextureTile_4b(gfx++, (u32)(g_Scorecard.labelsImage + PALETTED_TEXELS_OFFSET + labelIndex * LABEL_SIZE) & ~7,
                          G_IM_FMT_CI, LABEL_WIDTH, LABEL_HEIGHT, 0, 0, LABEL_WIDTH - 1, LABEL_HEIGHT - 1, 0,
                          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                          G_TX_NOLOD);
    DrawTextureRect(&gfx, left << 2, top << 2, (left + LABEL_SHOWN_WIDTH) << 2, (top + LABEL_HEIGHT) << 2,
                    G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    *displayListHead = gfx;
}

/*
 * Shows who won the last hole of a match game, above the two players'
 * characters, once LoadResultCharacters has loaded them.
 *
 * Doesn't match: the target keeps the hole's offset in $s0 and the player's
 * in $s1, where this swaps them.
 */
#ifdef NON_MATCHING
void DrawMatchResult(Gfx **displayListHead) {
    s32 holeNumber = g_Scorecard.content.holeIndex;
    s32 i;
    s32 x;
    s32 holeIndex;
    Gfx *gfx = *displayListHead;

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
    DrawLargeText(&gfx, MATCH_TITLE_X, MATCH_TITLE_Y, "Match Game");
    sprintf(g_PrintBuffer, sHoleNumberFormat, holeNumber);
    DrawLargeText(&gfx, MATCH_HOLE_X, MATCH_HOLE_Y, g_PrintBuffer);
    holeIndex = holeNumber - 1;
    for (i = 0, x = MATCH_RESULT_LEFT; i != MATCH_PLAYER_COUNT; i++) {
        if (g_Players[i].holes[holeIndex].strokes == g_Players[i ^ 1].holes[holeIndex].strokes) {
            SetTextColor(&gfx, 0xFF, 0xFF, 0);
            DrawLargeText(&gfx, x, MATCH_RESULT_Y, sDrawText);
        } else if (g_Players[i].holes[holeIndex].strokes <
                   g_Players[i ^ 1].holes[holeIndex].strokes) {
            SetTextColor(&gfx, 0xFF, 0x7F, 0x7F);
            DrawLargeText(&gfx, x, MATCH_RESULT_Y, sWinText);
        } else {
            SetTextColor(&gfx, 0x7F, 0x7F, 0xFF);
            DrawLargeText(&gfx, x, MATCH_RESULT_Y, sLoseText);
        }
        x += MATCH_RESULT_SPACING;
    }
    UpdateAndDrawBursts(&gfx);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    if (sResultCharactersLoadStep == RESULT_CHARACTERS_LOADING && LoadResultCharacters() == TRUE) {
        sResultCharactersLoadStep = RESULT_CHARACTERS_LOADED;
    }
    if (sResultCharactersLoadStep < RESULT_CHARACTERS_LOADING) {
        sResultCharactersLoadStep++;
    }
    if (sResultCharactersLoadStep == RESULT_CHARACTERS_LOADED) {
        Vec3 translation;
        Vec3 rotation;
        Vec3 scale;
        Mat4 matrix;

        sMatchResultFrameCount++;
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        for (i = 0; i != MATCH_PLAYER_COUNT; i++) {
            translation[0] = i * RESULT_CHARACTER_SPACING + RESULT_CHARACTER_LEFT;
            rotation[0] = 0.0f;
            rotation[1] = 0;
            rotation[2] = 0;
            translation[1] = RESULT_CHARACTER_Y;
            translation[2] = RESULT_CHARACTER_Z;
            scale[0] = RESULT_CHARACTER_SCALE;
            scale[1] = RESULT_CHARACTER_SCALE;
            scale[2] = RESULT_CHARACTER_SCALE;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &sResultCharacterMatrices[i]);
            gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sResultCharacterMatrices[i]),
                      G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
            g_DisplayListHead = gfx;
            DrawCharacter(i);
            gfx = *displayListHead = g_DisplayListHead;
        }
        AnimateAllCharacters();
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", DrawMatchResult);
#endif

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", sHoleNumberFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", sDrawText);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", sWinText);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", sLoseText);

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

/* Writes a standing's place, score to par and holes played into a leaderboard row */
void FormatLeaderboardRow(char *row, TournamentStanding *standing) {
    u8 place = standing->place;
    s16 score;
    s8 holeCount;

    if (place & PLACE_TIED) {
        sprintf(row, "%2dT          ", place & PLACE_MASK);
    } else {
        sprintf(row, "%2d           ", place);
    }

    score = standing->score;
    if (score == 0) {
        sprintf(row + LEADERBOARD_SCORE_COLUMN, "   #0   ", 0);
    } else {
        sprintf(row + LEADERBOARD_SCORE_COLUMN, "  %+3d   ", score);
    }
    holeCount = standing->holeCount;
    if (holeCount == 0) {
        sprintf(row + LEADERBOARD_HOLES_COLUMN, "  -");
    } else {
        sprintf(row + LEADERBOARD_HOLES_COLUMN, " %2d", holeCount);
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_80247E3C_ovl_09);

INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", func_802493DC_ovl_09);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", sDecimalFormat);

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

/*
 * Draws each player's row of bet tallies in the digit font, with their total
 * at the end. The bet columns show g_PrintBuffer as it already stands, and
 * are skipped when the bet rate at the player's index is zero or unset.
 *
 * Doesn't match: this keeps loop values on the stack that the target keeps in
 * saved registers, and it tests the bet rate as a range where the target
 * compares it with zero and UNSET_BET_RATE in turn.
 */
#ifdef NON_MATCHING
void DrawBetTallies(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 playerIndex;
    s32 betIndex;
    s32 total;
    s32 column;
    s32 left;
    s32 x;
    char *text;

    LoadSmallPalette(&gfx, 0, g_Scorecard.digitsImage + IMAGE_HEADER_SIZE);
    LoadNibbleTextureBlock(&gfx, g_Scorecard.digitsImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI, DIGIT_GLYPH_SIZE,
                           DIGIT_FONT_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    for (playerIndex = 0; playerIndex != g_PlayerCount; playerIndex++) {
        total = 0;
        column = BET_TALLY_LEFT;
        for (betIndex = 0; betIndex != BET_COUNT; betIndex++) {
            total += g_BetTallies[playerIndex][betIndex];
            if (g_GameState.betRates[playerIndex] != 0 && g_GameState.betRates[playerIndex] != UNSET_BET_RATE) {
                for (text = g_PrintBuffer, left = column; *text != '\0'; left += DIGIT_GLYPH_SIZE) {
                    x = left + g_Scorecard.x;
                    DrawTextureRect(&gfx, x << 2, (BET_TALLY_TOP + playerIndex * BET_TALLY_ROW_SPACING) << 2,
                                    (x + DIGIT_GLYPH_SIZE) << 2,
                                    (BET_TALLY_TOP + DIGIT_GLYPH_SIZE + playerIndex * BET_TALLY_ROW_SPACING) << 2,
                                    G_TX_RENDERTILE, 0, GetDigitGlyphTop(*text++) << 5, TEXEL_STEP_ONE,
                                    TEXEL_STEP_ONE);
                }
            }
            column += BET_TALLY_COLUMN_SPACING;
        }
        sprintf(g_PrintBuffer, sDecimalFormat, total);
        for (text = g_PrintBuffer, left = BET_TOTAL_LEFT; *text != '\0'; left += DIGIT_GLYPH_SIZE) {
            x = left + g_Scorecard.x;
            DrawTextureRect(&gfx, x << 2, (BET_TALLY_TOP + playerIndex * BET_TALLY_ROW_SPACING) << 2,
                            (x + DIGIT_GLYPH_SIZE) << 2,
                            (BET_TALLY_TOP + DIGIT_GLYPH_SIZE + playerIndex * BET_TALLY_ROW_SPACING) << 2,
                            G_TX_RENDERTILE, 0, GetDigitGlyphTop(*text++) << 5, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", DrawBetTallies);
#endif

