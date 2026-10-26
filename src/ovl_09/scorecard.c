#include "common.h"
#include "asset.h"
#include "burst.h"
#include "character.h"
#include "character_slot.h"
#include "cpu_projection.h"
#include "course.h"
#include "functions.h"
#include "game_flags.h"
#include "heap.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "math_util.h"
#include "reaction.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scorecard.h"
#include "scoring.h"
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
#define SCORECARD_MATCH_RESULT 5 /* DrawMatchResult shows who won the hole */
#define SCORECARD_UNK6 6

/* The layouts that SetScorecardLayout picks for the game modes */
#define SCORECARD_LAYOUT_STROKE 0
#define SCORECARD_LAYOUT_MATCH 1 /* Match games and Get Character */
#define SCORECARD_LAYOUT_SKINS 2 /* Skins matches and club slots */
#define SCORECARD_LAYOUT_TOURNAMENT 3
#define SCORECARD_LAYOUT_SPEED_GOLF 4

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

/*
 * The songs for a hole's result. Get Character plays the one for how the
 * first player did; match games and skins matches play SONG_HOLE_WON for any
 * hole that somebody won.
 */
#define SONG_HOLE_WON 6
#define SONG_HOLE_LOST 7
#define SONG_HOLE_HALVED 8
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
extern Gfx D_800C3FC0[];
/* "Hole %d" in half-width katakana */
extern const char sHoleNumberFormat[];
/* Formats that more than one page shares */
extern const char sStringFormat[];
extern const char sCompactHoleFormat[];
extern const char sTwoDigitFormat[];
extern const char sZeroToParFormat[];
extern const char sScoreToParFormat[];
extern const char sWideTotalFormat[];
extern const char sTotalFormat[];
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
void DrawMatchResult(Gfx **displayListHead);
void DrawScorecardTable(Gfx **displayListHead, s32 pageCount, s32 page);
void DrawStandingsPage(Gfx **displayListHead);
void DrawMatchPage(Gfx **displayListHead);
void DrawSkinsPage(Gfx **displayListHead);
void DrawBetsPage(Gfx **displayListHead);
void DrawSpeedGolfPage(Gfx **displayListHead);
void func_80247E3C_ovl_09(Gfx **displayListHead);
void func_802493DC_ovl_09(Gfx **displayListHead);
void SetTextColor(Gfx **displayListHead, s32 red, s32 green, s32 blue);

void ScrollLeaderboard(u16 buttons);

void SetScorecardState(s32 state);

#define TABLE_IMAGE_ASSET 0x68F
#define UNK10_IMAGE_ASSET 0x682
#define UNK14_IMAGE_ASSET 0x690
#define UNK18_IMAGE_ASSET 0x691
#define DIGITS_IMAGE_ASSET 0x689
#define BET_SUBTITLES_IMAGE_ASSET 0x97A
#define LABELS_IMAGE_ASSET 0x68A
#define UNK8C_IMAGE_ASSET 0x693
#define PLACES_IMAGE_ASSET 0x688
#define UNK2C_IMAGE_ASSET 0x68E
#define MATCH_UNK38_IMAGE_ASSET 0x68C
#define UNK3C_IMAGE_ASSET 0x68D
#define UNK40_IMAGE_ASSET 0x694
#define SKIN_ICON_IMAGE_ASSET 0x68B
#define UNK90_IMAGE_ASSET 0x695
#define UNK88_IMAGE_ASSET 0x692
#define TOTAL_HEADING_IMAGE_ASSET 0x696
#define GRID_IMAGE_ASSET 0x69D

/* The rows of sPageTitleAssets */
#define PAGE_TITLES_SPEED_GOLF 0
#define PAGE_TITLES_SOLO 1
#define PAGE_TITLES_VERSUS 2
#define PAGE_TITLES_SKINS 3
#define PAGE_TITLES_TOURNAMENT 4
#define PAGE_TITLES_ROW_COUNT 5
#define PAGE_TITLE_COUNT 3

/* Values of Scorecard.placeChange */
#define PLACE_HELD (-1)
#define PLACE_ROSE 0
#define PLACE_FELL 1

extern u16 sPageTitleAssets[PAGE_TITLES_ROW_COUNT][PAGE_TITLE_COUNT];
/* Each bet's two images */
extern u16 sBetImageAssets[BET_COUNT][2];
extern s32 D_801B60A0;
extern s32 D_801B60B4;

/*
 * Opens the scorecard at the given hole: loads the images that every game
 * mode shows, the players' faces and those that the game mode's pages show,
 * and counts the pages. In a tournament it also ranks the player before and
 * after the hole to see which way their place moved.
 *
 * Doesn't match: the target copies the arguments to other registers before
 * it saves its own and stores them after it reads $sp, and it loads
 * g_TournamentStandings after setting up the loops' other registers.
 */
#ifdef NON_MATCHING
void OpenScorecard(s32 courseIndex, s32 holeIndex) {
    EncodedAsset asset;
    AssetFile file;
    s32 pageTitlesRow = PAGE_TITLES_SPEED_GOLF;
    s32 shownHoleCount;
    s32 oldPlace;
    s32 newPlace;
    s32 size;
    s32 i;
    s32 imageIndex;
    s32 standingIndex;
    TournamentStanding *standing;
    register void *stackPointer asm("$sp");

    g_Scorecard.content.courseIndex = courseIndex;
    g_Scorecard.content.holeIndex = holeIndex;
    g_Scorecard.content.isSuddenDeath = D_801B60A0;
    osSyncPrintf("SP Address = %x\n", stackPointer);
    g_Scorecard.tableImage = AllocMemory(OpenEncodedAsset(TABLE_IMAGE_ASSET, &asset));
    ReadEncodedAsset(g_Scorecard.tableImage, &asset);
    g_Scorecard.unk14 = AllocMemory(OpenEncodedAsset(UNK14_IMAGE_ASSET, &asset));
    ReadEncodedAsset(g_Scorecard.unk14, &asset);
    g_Scorecard.unk18 = AllocMemory(OpenEncodedAsset(UNK18_IMAGE_ASSET, &asset));
    ReadEncodedAsset(g_Scorecard.unk18, &asset);
    g_Scorecard.digitsImage = AllocMemory(OpenEncodedAsset(DIGITS_IMAGE_ASSET, &asset));
    ReadEncodedAsset(g_Scorecard.digitsImage, &asset);
    g_Scorecard.betSubtitlesImage = AllocMemory(OpenEncodedAsset(BET_SUBTITLES_IMAGE_ASSET, &asset));
    ReadEncodedAsset(g_Scorecard.betSubtitlesImage, &asset);
    g_Scorecard.labelsImage = AllocMemory(OpenEncodedAsset(LABELS_IMAGE_ASSET, &asset));
    ReadEncodedAsset(g_Scorecard.labelsImage, &asset);
    g_Scorecard.unk8C = AllocMemory(OpenEncodedAsset(UNK8C_IMAGE_ASSET, &asset));
    ReadEncodedAsset(g_Scorecard.unk8C, &asset);

    g_Scorecard.hasBets = FALSE;
    for (i = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] > 0) {
            g_Scorecard.hasBets = TRUE;
            break;
        }
    }

    for (i = 0; i != g_PlayerCount; i++) {
        g_Scorecard.faceImages[i] =
            AllocMemory(OpenEncodedAsset(g_CharacterImageAssets[(s8)g_Players[i].characterIndex], &asset));
        ReadEncodedAsset(g_Scorecard.faceImages[i], &asset);
        OpenAssetFile(g_CharacterPaletteAssets[(s8)g_Players[i].characterIndex] + (s8)g_Players[i].paletteIndex,
                      &file);
        size = GetAssetFileSize(&file);
        g_Scorecard.facePalettes[i] = AllocMemory(size);
        ReadAssetFile(g_Scorecard.facePalettes[i], size, &file);
    }
    g_Scorecard.unkBC = 0;
    g_Scorecard.faceTime = 0;
    g_Scorecard.faceFrame = 0;

    switch (g_GameMode) {
        case GAME_MODE_SPEED_GOLF:
            g_Scorecard.pageCount = 2;
            pageTitlesRow = PAGE_TITLES_SPEED_GOLF;
            break;
        case GAME_MODE_STROKE:
        case GAME_MODE_UNK1:
        case GAME_MODE_UNK3:
        case GAME_MODE_UNK6:
        case GAME_MODE_UNK8:
        case GAME_MODE_RING_SHOT:
            /* The bets get a page of their own */
            if (g_PlayerCount == 1) {
                pageTitlesRow = PAGE_TITLES_SOLO;
                if (g_Scorecard.hasBets == TRUE) {
                    g_Scorecard.pageCount = 2;
                } else {
                    g_Scorecard.pageCount = 1;
                }
            } else {
                pageTitlesRow = PAGE_TITLES_VERSUS;
                if (g_Scorecard.hasBets == TRUE) {
                    g_Scorecard.pageCount = 3;
                } else {
                    g_Scorecard.pageCount = 2;
                }
            }
            g_Scorecard.placesImage = AllocMemory(OpenEncodedAsset(PLACES_IMAGE_ASSET, &asset));
            ReadEncodedAsset(g_Scorecard.placesImage, &asset);
            break;
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_GET_CHARACTER:
            pageTitlesRow = PAGE_TITLES_VERSUS;
            if (g_Scorecard.hasBets == TRUE) {
                g_Scorecard.pageCount = 3;
            } else {
                g_Scorecard.pageCount = 2;
            }
            g_Scorecard.unk2C = AllocMemory(OpenEncodedAsset(UNK2C_IMAGE_ASSET, &asset));
            ReadEncodedAsset(g_Scorecard.unk2C, &asset);
            g_Scorecard.unk38 = AllocMemory(OpenEncodedAsset(MATCH_UNK38_IMAGE_ASSET, &asset));
            ReadEncodedAsset(g_Scorecard.unk38, &asset);
            g_Scorecard.unk3C = AllocMemory(OpenEncodedAsset(UNK3C_IMAGE_ASSET, &asset));
            ReadEncodedAsset(g_Scorecard.unk3C, &asset);
            g_Scorecard.unk40 = AllocMemory(OpenEncodedAsset(UNK40_IMAGE_ASSET, &asset));
            ReadEncodedAsset(g_Scorecard.unk40, &asset);
            break;
        case GAME_MODE_SKINS_MATCH:
        case GAME_MODE_CLUB_SLOTS:
            pageTitlesRow = PAGE_TITLES_SKINS;
            TallySkins(g_Scorecard.content.holeIndex);
            if (g_Scorecard.hasBets == TRUE) {
                g_Scorecard.pageCount = 3;
            } else {
                g_Scorecard.pageCount = 2;
            }
            g_Scorecard.placesImage = AllocMemory(OpenEncodedAsset(PLACES_IMAGE_ASSET, &asset));
            ReadEncodedAsset(g_Scorecard.placesImage, &asset);
            g_Scorecard.unk38 = AllocMemory(OpenEncodedAsset(SKIN_ICON_IMAGE_ASSET, &asset));
            ReadEncodedAsset(g_Scorecard.unk38, &asset);
            break;
        case GAME_MODE_TOURNAMENT:
            shownHoleCount = g_Scorecard.content.holeIndex;
            g_Scorecard.pageCount = 2;
            g_Scorecard.unk90 = AllocMemory(OpenEncodedAsset(UNK90_IMAGE_ASSET, &asset));
            ReadEncodedAsset(g_Scorecard.unk90, &asset);
            g_Scorecard.unk88 = AllocMemory(OpenEncodedAsset(UNK88_IMAGE_ASSET, &asset));
            ReadEncodedAsset(g_Scorecard.unk88, &asset);
            g_Scorecard.unk10 = AllocMemory(OpenEncodedAsset(UNK10_IMAGE_ASSET, &asset));
            ReadEncodedAsset(g_Scorecard.unk10, &asset);
            shownHoleCount += g_Players[0].unk26 != 0;
            pageTitlesRow = PAGE_TITLES_TOURNAMENT;
            if (shownHoleCount > 0) {
                if (g_Scorecard.content.holeIndex == 1) {
                    RankTournament(shownHoleCount - 1, 0, 0, D_801B60B4);
                } else {
                    RankTournament(shownHoleCount - 1, 0, GetNetScoreToPar(g_Scorecard.content.holeIndex - 1, 0),
                                   D_801B60B4);
                }
                oldPlace = -1;
                for (standingIndex = 0, standing = g_TournamentStandings; standingIndex != TOURNAMENT_CONTESTANT_COUNT;
                     standingIndex++, standing++) {
                    if (standing->contestantId == PLAYER_CONTESTANT) {
                        oldPlace = standing->place & PLACE_MASK;
                        break;
                    }
                }
                RankTournament(shownHoleCount, 0, GetNetScoreToPar(g_Scorecard.content.holeIndex, 0), D_801B60B4);
                for (standingIndex = 0, standing = g_TournamentStandings; standingIndex != TOURNAMENT_CONTESTANT_COUNT;
                     standingIndex++, standing++) {
                    if (standing->contestantId == PLAYER_CONTESTANT) {
                        newPlace = standing->place & PLACE_MASK;
                        if (oldPlace < newPlace) {
                            g_Scorecard.placeChange = PLACE_FELL;
                        } else if (newPlace < oldPlace) {
                            g_Scorecard.placeChange = PLACE_ROSE;
                        } else {
                            g_Scorecard.placeChange = PLACE_HELD;
                        }
                        break;
                    }
                }
            } else {
                RankTournament(shownHoleCount, 0, GetNetScoreToPar(g_Scorecard.content.holeIndex, 0), D_801B60B4);
                g_Scorecard.placeChange = PLACE_HELD;
            }
            g_Scorecard.topRow = 0;
            break;
    }

    for (i = 0; i != PAGE_TITLE_COUNT; i++) {
        g_Scorecard.pageTitleImages[i] = AllocMemory(OpenEncodedAsset(sPageTitleAssets[pageTitlesRow][i], &asset));
        ReadEncodedAsset(g_Scorecard.pageTitleImages[i], &asset);
    }
    /* Only the bets being played have images */
    for (i = 0, imageIndex = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] != UNSET_BET_RATE) {
            g_Scorecard.betImages[imageIndex][0] = AllocMemory(OpenEncodedAsset(sBetImageAssets[i][0], &asset));
            ReadEncodedAsset(g_Scorecard.betImages[imageIndex][0], &asset);
            g_Scorecard.betImages[imageIndex][1] = AllocMemory(OpenEncodedAsset(sBetImageAssets[i][1], &asset));
            ReadEncodedAsset(g_Scorecard.betImages[imageIndex][1], &asset);
            imageIndex++;
        }
    }
    g_Scorecard.totalHeadingImage = AllocMemory(OpenEncodedAsset(TOTAL_HEADING_IMAGE_ASSET, &asset));
    ReadEncodedAsset(g_Scorecard.totalHeadingImage, &asset);
    g_Scorecard.gridImage = AllocMemory(OpenEncodedAsset(GRID_IMAGE_ASSET, &asset));
    ReadEncodedAsset(g_Scorecard.gridImage, &asset);
    SetScorecardState(SCORECARD_OPENING);
    ForceStopRumble();
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", OpenScorecard);
#endif

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
        FreeMemory(&g_Scorecard.totalHeadingImage);
        FreeMemory(&g_Scorecard.pageTitleImages[2]);
        FreeMemory(&g_Scorecard.pageTitleImages[1]);
        FreeMemory(&g_Scorecard.pageTitleImages[0]);
        FreeMemory(&g_Scorecard.gridImage);
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
        FreeMemory(&g_Scorecard.betSubtitlesImage);
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

#define BEHIND_FACE_IMAGE_OFFSET 4 /* The face shown for a player behind */
#define STOPWATCH_HOUR (60 * SECONDS_PER_MINUTE * FRAMES_PER_SECOND)
/* A hole time of this many minutes or more shows as dashes */
#define SHOWN_MINUTE_LIMIT 60

/*
 * Picks the scorecard's layout for the last hole's result with the result
 * not yet tallied: it plays the result's song, picks the hole's winner and,
 * in Speed Golf, records the hole's time. It then turns to the first page and
 * picks each player's face by how they did on the hole.
 */
void StartScorecardTally(s32 layout) {
    s32 ranks[MAX_PLAYER_COUNT];
    s32 scoresToPar[MAX_PLAYER_COUNT];
    s32 holeIndex;
    s32 song;
    s32 timedHoleCount;
    s32 i;
    s32 j;

    g_Scorecard.layout = layout;
    SettleHoleBets();
    g_Scorecard.isTallied = FALSE;
    switch (layout) {
        case SCORECARD_LAYOUT_STROKE:
            break;
        case SCORECARD_LAYOUT_MATCH:
            holeIndex = g_Scorecard.content.holeIndex - 1;
            if (g_GameMode == GAME_MODE_GET_CHARACTER) {
                if (g_Players[0].holes[holeIndex].strokes < g_Players[1].holes[holeIndex].strokes) {
                    song = SONG_HOLE_WON;
                } else if (g_Players[1].holes[holeIndex].strokes < g_Players[0].holes[holeIndex].strokes) {
                    song = SONG_HOLE_LOST;
                } else {
                    goto halved;
                }
            } else {
                song = SONG_HOLE_WON;
                if (g_Players[0].holes[holeIndex].strokes == g_Players[1].holes[holeIndex].strokes) {
                halved:
                    song = SONG_HOLE_HALVED;
                }
            }
            PlaySong(song, 0);
            if (g_Players[0].holes[holeIndex].strokes == g_Players[1].holes[holeIndex].strokes) {
                g_Scorecard.holeWinnerIndex = HOLE_WINNER_HALVED;
            } else if (g_Players[0].holes[holeIndex].strokes < g_Players[1].holes[holeIndex].strokes) {
                g_Scorecard.holeWinnerIndex = 0;
            } else {
                g_Scorecard.holeWinnerIndex = 1;
            }
            g_Scorecard.skinsCarryover.onLastHole = 0;
            g_Scorecard.skinsCarryover.fromLastHole = g_Scorecard.skinsCarryover.intoLastHole =
                g_Scorecard.content.holeIndex - 1;
            break;
        case SCORECARD_LAYOUT_SKINS:
            g_Scorecard.holeWinnerIndex = HOLE_WINNER_HALVED;
            for (i = 0; i != g_PlayerCount; i++) {
                if (g_Players[i].holes[g_Scorecard.content.holeIndex - 1].strokes != 0) {
                    g_Scorecard.holeWinnerIndex = i;
                }
            }
            song = SONG_HOLE_WON;
            if (g_Scorecard.holeWinnerIndex == HOLE_WINNER_HALVED) {
                song = SONG_HOLE_HALVED;
            }
            PlaySong(song, 0);
            break;
        case SCORECARD_LAYOUT_TOURNAMENT:
            g_Scorecard.content.flipTime = 0;
            for (i = 0; i != LEADERBOARD_ROW_COUNT; i++) {
                g_Scorecard.content.rowFlipAngles[i] = 0;
                g_Scorecard.content.isRowFlipping[i] = TRUE;
            }
            break;
        case SCORECARD_LAYOUT_SPEED_GOLF:
            if (IsGameFlagSet(GAME_FLAG_STOPWATCH_HOUR)) {
                g_GameState.stopwatchFrameCount = STOPWATCH_HOUR;
            }
            for (i = 0, timedHoleCount = 0; i != COURSE_HOLE_COUNT; i++) {
                timedHoleCount += g_HoleTimeFrames[i] != 0;
            }
            if (timedHoleCount > 0) {
                /* The stopwatch runs on through the round, so the last hole's time first takes all of it */
                g_HoleTimeFrames[timedHoleCount - 1] = g_GameState.stopwatchFrameCount;
                g_HoleTimeMinutes[timedHoleCount - 1] =
                    g_GameState.stopwatchFrameCount / (FRAMES_PER_SECOND * SECONDS_PER_MINUTE);
                g_HoleTimeSeconds[timedHoleCount - 1] =
                    g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND % SECONDS_PER_MINUTE;
                g_HoleTimeHundredths[timedHoleCount - 1] =
                    (s32)(g_GameState.stopwatchFrameCount * ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
                    HUNDREDTHS_PER_SECOND;
                if (timedHoleCount == 1 && g_HoleTimeMinutes[0] >= SHOWN_MINUTE_LIMIT) {
                    g_HoleTimeMinutes[0] = -1;
                    g_HoleTimeSeconds[0] = -1;
                    g_HoleTimeHundredths[0] = -1;
                }
                /* Then loses each earlier hole's time */
                for (i = 0; i < timedHoleCount - 1; i++) {
                    if (g_HoleTimeMinutes[timedHoleCount - 1] < 0 ||
                        g_HoleTimeMinutes[timedHoleCount - 1] >= SHOWN_MINUTE_LIMIT ||
                        g_HoleTimeMinutes[i] < 0) {
                        g_HoleTimeMinutes[timedHoleCount - 1] = -1;
                        g_HoleTimeSeconds[timedHoleCount - 1] = -1;
                        g_HoleTimeHundredths[timedHoleCount - 1] = -1;
                        break;
                    }
                    g_HoleTimeFrames[timedHoleCount - 1] -= g_HoleTimeFrames[i];
                    g_HoleTimeMinutes[timedHoleCount - 1] -= g_HoleTimeMinutes[i];
                    if (g_HoleTimeSeconds[timedHoleCount - 1] < g_HoleTimeSeconds[i]) {
                        g_HoleTimeMinutes[timedHoleCount - 1]--;
                        g_HoleTimeSeconds[timedHoleCount - 1] += SECONDS_PER_MINUTE - g_HoleTimeSeconds[i];
                    } else {
                        g_HoleTimeSeconds[timedHoleCount - 1] -= g_HoleTimeSeconds[i];
                    }
                    if (g_HoleTimeHundredths[timedHoleCount - 1] < g_HoleTimeHundredths[i]) {
                        if (g_HoleTimeSeconds[timedHoleCount - 1] > 0) {
                            g_HoleTimeSeconds[timedHoleCount - 1]--;
                        } else {
                            g_HoleTimeMinutes[timedHoleCount - 1]--;
                            g_HoleTimeSeconds[timedHoleCount - 1] = SECONDS_PER_MINUTE - 1;
                        }
                        g_HoleTimeHundredths[timedHoleCount - 1] += HUNDREDTHS_PER_SECOND - g_HoleTimeHundredths[i];
                    } else {
                        g_HoleTimeHundredths[timedHoleCount - 1] -= g_HoleTimeHundredths[i];
                    }
                }
            }
            break;
    }

    g_RoundHud.scorecardPage = 0;
    switch (g_GameMode) {
        case GAME_MODE_UNK1:
        case GAME_MODE_UNK3:
        case GAME_MODE_TOURNAMENT:
        case GAME_MODE_UNK6:
        case GAME_MODE_SPEED_GOLF:
        case GAME_MODE_UNK8:
        case GAME_MODE_RING_SHOT:
            for (i = 0; i != g_GameState.playerCount; i++) {
                g_Scorecard.content.faceImageOffsets[i] = 0;
            }
            break;
        case GAME_MODE_STROKE:
            for (i = 0; i < g_GameState.playerCount; i++) {
                scoresToPar[i] = GetNetScoreToPar(g_Scorecard.content.holeIndex, i);
            }
            /* The ranks go unused */
            for (i = 0; i != g_GameState.playerCount; i++) {
                ranks[i] = 0;
                for (j = 0; j != g_GameState.playerCount; j++) {
                    if (i != j && scoresToPar[i] > scoresToPar[j]) {
                        ranks[i]++;
                    }
                }
            }
            /* A player over par on the hole, after their handicap, looks behind */
            for (i = 0; i != g_GameState.playerCount; i++) {
                CourseRecord *course = &g_CourseRecords[GetCourseRecordIndex()];

                if (g_Players[i].holes[g_Scorecard.content.holeIndex - 1].strokes -
                        (s8)course->holes[g_Scorecard.content.holeIndex - 1].par +
                        GetHandicapOffset(g_CourseIndex, g_Scorecard.content.holeIndex - 1, g_Players[i].handicap) <=
                    0) {
                    g_Scorecard.content.faceImageOffsets[i] = 0;
                } else {
                    g_Scorecard.content.faceImageOffsets[i] = BEHIND_FACE_IMAGE_OFFSET;
                }
            }
            break;
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_GET_CHARACTER:
            if (g_Players[0].holes[g_Scorecard.content.holeIndex - 1].strokes <
                g_Players[1].holes[g_Scorecard.content.holeIndex - 1].strokes) {
                g_Scorecard.content.faceImageOffsets[0] = 0;
                g_Scorecard.content.faceImageOffsets[1] = BEHIND_FACE_IMAGE_OFFSET;
            } else if (g_Players[1].holes[g_Scorecard.content.holeIndex - 1].strokes <
                       g_Players[0].holes[g_Scorecard.content.holeIndex - 1].strokes) {
                g_Scorecard.content.faceImageOffsets[0] = BEHIND_FACE_IMAGE_OFFSET;
                g_Scorecard.content.faceImageOffsets[1] = 0;
            } else {
                g_Scorecard.content.faceImageOffsets[0] = 0;
                g_Scorecard.content.faceImageOffsets[1] = 0;
            }
            break;
        case GAME_MODE_SKINS_MATCH:
        case GAME_MODE_CLUB_SLOTS:
            /* A player with more strokes on the hole than anybody else looks behind */
            for (i = 0; i != g_GameState.playerCount; i++) {
                g_Scorecard.content.faceImageOffsets[i] = 0;
                for (j = 0; j != g_GameState.playerCount; j++) {
                    if (g_Players[i].holes[g_Scorecard.content.holeIndex - 1].strokes <
                        g_Players[j].holes[g_Scorecard.content.holeIndex - 1].strokes) {
                        g_Scorecard.content.faceImageOffsets[i] = BEHIND_FACE_IMAGE_OFFSET;
                    }
                }
            }
            break;
    }
}

/*
 * Picks the scorecard's layout for the last hole's result and sets up its
 * tally, then turns to the first page and picks each player's face.
 */
void SetScorecardLayout(s32 layout) {
    s32 ranks[MAX_PLAYER_COUNT];
    s32 scoresToPar[MAX_PLAYER_COUNT];
    s32 holesWon[MATCH_PLAYER_COUNT];
    s32 skinsRanks[MAX_PLAYER_COUNT];
    s32 unused[4]; /* The original frame has room for this */
    s32 i;
    s32 j;

    g_Scorecard.isTallied = TRUE;
    g_Scorecard.layout = layout;
    switch (layout) {
        case SCORECARD_LAYOUT_STROKE:
        case SCORECARD_LAYOUT_SPEED_GOLF:
            break;
        case SCORECARD_LAYOUT_MATCH:
            /*
             * A match's tally reuses the skins fields: two keep the hole number,
             * and TallyScorecardSkins sets onLastHole once it counts the result.
             */
            g_Scorecard.holeWinnerIndex = HOLE_WINNER_UNSET;
            g_Scorecard.skinsCarryover.onLastHole = 0;
            g_Scorecard.skinsCarryover.intoLastHole = g_Scorecard.content.holeIndex;
            g_Scorecard.skinsCarryover.fromLastHole = g_Scorecard.content.holeIndex;
            break;
        case SCORECARD_LAYOUT_SKINS:
            g_Scorecard.holeWinnerIndex = HOLE_WINNER_UNSET;
            g_Scorecard.skinsCarryover.intoLastHole++;
            break;
        case SCORECARD_LAYOUT_TOURNAMENT:
            g_Scorecard.content.flipTime = ROW_FLIP_TIME;
            for (i = 0; i != LEADERBOARD_ROW_COUNT; i++) {
                g_Scorecard.content.rowFlipAngles[i] = 0;
                g_Scorecard.content.isRowFlipping[i] = FALSE;
            }
            break;
    }

    g_RoundHud.scorecardPage = 0;
    switch (g_GameMode) {
        case GAME_MODE_UNK1:
        case GAME_MODE_UNK3:
        case GAME_MODE_TOURNAMENT:
        case GAME_MODE_UNK6:
        case GAME_MODE_SPEED_GOLF:
        case GAME_MODE_UNK8:
        case GAME_MODE_RING_SHOT:
            for (i = 0; i != g_GameState.playerCount; i++) {
                g_Scorecard.content.faceImageOffsets[i] = 0;
            }
            break;
        case GAME_MODE_STROKE:
            for (i = 0; i < g_GameState.playerCount; i++) {
                scoresToPar[i] = GetNetScoreToPar(g_Scorecard.content.holeIndex, i);
            }
            /* Each player's rank counts the players ahead of them */
            for (i = 0; i != g_GameState.playerCount; i++) {
                ranks[i] = 0;
                for (j = 0; j != g_GameState.playerCount; j++) {
                    if (i != j && scoresToPar[i] > scoresToPar[j]) {
                        ranks[i]++;
                    }
                }
            }
            /* Every rank is below the player count, so this always picks 0 */
            for (i = 0; i != g_GameState.playerCount; i++) {
                if (ranks[i] < g_GameState.playerCount + 1) {
                    g_Scorecard.content.faceImageOffsets[i] = 0;
                } else {
                    g_Scorecard.content.faceImageOffsets[i] = BEHIND_FACE_IMAGE_OFFSET;
                }
            }
            break;
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_GET_CHARACTER:
            holesWon[0] = 0;
            holesWon[1] = 0;
            for (i = GetFirstHoleIndex(); i != g_Scorecard.content.holeIndex; i++) {
                if (g_Players[0].holes[i].strokes < g_Players[1].holes[i].strokes) {
                    holesWon[0]++;
                } else if (g_Players[1].holes[i].strokes < g_Players[0].holes[i].strokes) {
                    holesWon[1]++;
                }
            }
            if (holesWon[0] > holesWon[1]) {
                g_Scorecard.content.faceImageOffsets[0] = 0;
                g_Scorecard.content.faceImageOffsets[1] = BEHIND_FACE_IMAGE_OFFSET;
            } else if (holesWon[0] < holesWon[1]) {
                g_Scorecard.content.faceImageOffsets[0] = BEHIND_FACE_IMAGE_OFFSET;
                g_Scorecard.content.faceImageOffsets[1] = 0;
            } else {
                g_Scorecard.content.faceImageOffsets[0] = 0;
                g_Scorecard.content.faceImageOffsets[1] = 0;
            }
            break;
        case GAME_MODE_SKINS_MATCH:
        case GAME_MODE_CLUB_SLOTS:
            /* Each player's rank counts the players with fewer skins */
            for (i = 0; i != g_GameState.playerCount; i++) {
                skinsRanks[i] = 0;
                for (j = 0; j != g_GameState.playerCount; j++) {
                    if (i != j && (s16)g_RoundHud.skinsTotals[i] > (s16)g_RoundHud.skinsTotals[j]) {
                        skinsRanks[i]++;
                    }
                }
            }
            /* Every rank is below the player count, so this always picks 0 */
            for (i = 0; i != g_GameState.playerCount; i++) {
                if (skinsRanks[i] < g_GameState.playerCount + 1) {
                    g_Scorecard.content.faceImageOffsets[i] = 0;
                } else {
                    g_Scorecard.content.faceImageOffsets[i] = BEHIND_FACE_IMAGE_OFFSET;
                }
            }
            break;
    }
}

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
            case GAME_MODE_SPEED_GOLF:
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
        case GAME_MODE_SPEED_GOLF:
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

/* The page titles sit in a row along the scorecard's top, the page shown a pixel higher */
#define PAGE_TITLE_LEFT 32
#define PAGE_TITLE_SPACING 136
#define THREE_PAGE_TITLE_LEFT 29
#define THREE_PAGE_TITLE_SPACING 104
#define PAGE_TITLE_TOP 21
#define THREE_PAGE_COUNT 3
#define UNSHOWN_PAGE_GRAY 0x7F

/*
 * Draws the scorecard: its page titles, with the other pages' grayed out, and
 * the page shown, which depends on the game mode. Draws DrawMatchResult's
 * screen instead while that is up.
 */
void DrawScorecard(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 i;
    s32 dropY;

    if (sScorecardState == SCORECARD_MATCH_RESULT) {
        DrawMatchResult(&gfx);
    } else if (sScorecardState != SCORECARD_UNK6) {
        if (sScorecardState != SCORECARD_CLOSED) {
            DrawScorecardTable(&gfx, g_Scorecard.pageCount, g_RoundHud.scorecardPage);
            gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
            for (i = 0; i != g_Scorecard.pageCount; i++) {
                if (i == g_RoundHud.scorecardPage) {
                    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                    dropY = 0;
                } else {
                    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                    gDPSetPrimColor(gfx++, 0, 0, UNSHOWN_PAGE_GRAY, UNSHOWN_PAGE_GRAY, UNSHOWN_PAGE_GRAY, 0xFF);
                    dropY = 1;
                }
                gDPPipeSync(gfx++);
                if (g_Scorecard.pageCount == THREE_PAGE_COUNT) {
                    DrawLargeImage(&gfx, g_Scorecard.pageTitleImages[i],
                                   (i * THREE_PAGE_TITLE_SPACING + THREE_PAGE_TITLE_LEFT) + g_Scorecard.x,
                                   g_Scorecard.y + PAGE_TITLE_TOP + dropY);
                } else {
                    DrawLargeImage(&gfx, g_Scorecard.pageTitleImages[i],
                                   (i * PAGE_TITLE_SPACING + PAGE_TITLE_LEFT) + g_Scorecard.x,
                                   g_Scorecard.y + PAGE_TITLE_TOP + dropY);
                }
            }
            switch (g_GameMode) {
                case GAME_MODE_TOURNAMENT:
                    if (g_RoundHud.scorecardPage == 0) {
                        func_80247E3C_ovl_09(&gfx);
                    } else {
                        func_802493DC_ovl_09(&gfx);
                    }
                    break;
                case GAME_MODE_SPEED_GOLF:
                    if (g_RoundHud.scorecardPage == 0) {
                        DrawSpeedGolfPage(&gfx);
                    } else {
                        func_802493DC_ovl_09(&gfx);
                    }
                    break;
                case GAME_MODE_SKINS_MATCH:
                case GAME_MODE_CLUB_SLOTS:
                    if (g_RoundHud.scorecardPage == 0) {
                        DrawSkinsPage(&gfx);
                    } else if (g_RoundHud.scorecardPage == 1) {
                        func_802493DC_ovl_09(&gfx);
                    } else {
                        DrawBetsPage(&gfx);
                    }
                    break;
                case GAME_MODE_MATCH_GAME:
                case GAME_MODE_GET_CHARACTER:
                    SetTextSlot(0, 0, NO_MESSAGE, 1, 0, 1, 0, 0, 0, 0);
                    SetTextSlot(0, 0, NO_MESSAGE, 1, 1, 1, 0, 0, 0, 0);
                    if (g_RoundHud.scorecardPage == 0) {
                        DrawMatchPage(&gfx);
                    } else if (g_RoundHud.scorecardPage == 1) {
                        func_802493DC_ovl_09(&gfx);
                    } else {
                        DrawBetsPage(&gfx);
                    }
                    break;
                case GAME_MODE_STROKE:
                case GAME_MODE_UNK1:
                case GAME_MODE_UNK3:
                case GAME_MODE_UNK6:
                case GAME_MODE_UNK8:
                case GAME_MODE_RING_SHOT:
                    if (g_PlayerCount == 1) {
                        if (g_RoundHud.scorecardPage == 0) {
                            func_802493DC_ovl_09(&gfx);
                        } else {
                            DrawBetsPage(&gfx);
                        }
                    } else if (g_RoundHud.scorecardPage == 0) {
                        DrawStandingsPage(&gfx);
                    } else if (g_RoundHud.scorecardPage == 1) {
                        func_802493DC_ovl_09(&gfx);
                    } else {
                        DrawBetsPage(&gfx);
                    }
                    break;
            }
        }
    }
    *displayListHead = gfx;
}

/*
 * The table image holds strips of tabs, then the table's body. Each strip is a
 * row of tabs drawn across the table's full width.
 */
#define TABLE_WIDTH 288
#define TABLE_SHOWN_WIDTH 284
#define TABLE_LEFT 20
#define TAB_TOP 20
#define TAB_HEIGHT 24
#define TAB_STRIP_COUNT 5
#define TAB_BACK_DROP 2 /* The tabs behind the front one sit lower */
#define TABLE_BODY_TOP 44
#define TABLE_BODY_HEIGHT 190
#define TABLE_BODY_OFFSET (LARGE_PALETTED_TEXELS_OFFSET + TAB_STRIP_COUNT * TAB_HEIGHT * TABLE_WIDTH)
#define TABLE_LOAD_HEIGHT 7 /* The rows that fit in texture memory at once */
#define TAB_BACK_GRAY 0xBF
#define FIRST_OF_THREE_TAB_STRIP 0
#define SECOND_OF_THREE_TAB_STRIP 1
#define THIRD_OF_THREE_TAB_STRIP 2
#define FIRST_OF_TWO_TAB_STRIP 3
#define SECOND_OF_TWO_TAB_STRIP 4

/*
 * Draws the scorecard's tabs and the table under them. The tab strips are
 * drawn back to front, the last in front at full brightness, and their order
 * depends on the page shown.
 *
 * Doesn't match: the target adds each tab rectangle's top offset to the
 * scorecard's y before adding the row, where this adds the row first, and it
 * swaps two saved registers.
 */
#ifdef NON_MATCHING
void DrawScorecardTable(Gfx **displayListHead, s32 pageCount, s32 page) {
    s32 tabStrips[THREE_PAGE_COUNT];
    Gfx *gfx = *displayListHead;
    s32 tabIndex;
    s32 row;
    s32 dropY;
    s32 texelsOffset;
    s32 loadHeight;
    s32 top;

    gSPDisplayList(gfx++, D_800C3FC0);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    if (pageCount == 1) {
        tabStrips[0] = SECOND_OF_TWO_TAB_STRIP;
    } else if (pageCount == 2) {
        if (page == 0) {
            tabStrips[0] = FIRST_OF_TWO_TAB_STRIP;
            tabStrips[1] = SECOND_OF_TWO_TAB_STRIP;
        } else {
            tabStrips[0] = SECOND_OF_TWO_TAB_STRIP;
            tabStrips[1] = FIRST_OF_TWO_TAB_STRIP;
        }
    } else if (page == 0) {
        tabStrips[0] = FIRST_OF_THREE_TAB_STRIP;
        tabStrips[1] = SECOND_OF_THREE_TAB_STRIP;
        tabStrips[2] = THIRD_OF_THREE_TAB_STRIP;
    } else if (page == 1) {
        tabStrips[0] = FIRST_OF_THREE_TAB_STRIP;
        tabStrips[1] = THIRD_OF_THREE_TAB_STRIP;
        tabStrips[2] = SECOND_OF_THREE_TAB_STRIP;
    } else {
        tabStrips[0] = THIRD_OF_THREE_TAB_STRIP;
        tabStrips[1] = SECOND_OF_THREE_TAB_STRIP;
        tabStrips[2] = FIRST_OF_THREE_TAB_STRIP;
    }

    LoadLargePalette(&gfx, g_Scorecard.tableImage + IMAGE_HEADER_SIZE);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(gfx++, 0, 0, TAB_BACK_GRAY, TAB_BACK_GRAY, TAB_BACK_GRAY, 0xFF);
    for (tabIndex = 0; tabIndex != pageCount; tabIndex++) {
        texelsOffset = tabStrips[tabIndex] * TAB_HEIGHT * TABLE_WIDTH + LARGE_PALETTED_TEXELS_OFFSET;
        dropY = TAB_BACK_DROP;
        if (tabIndex == pageCount - 1) {
            gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
            dropY = 0;
        }
        for (row = 0; row < TAB_HEIGHT; row += TABLE_LOAD_HEIGHT) {
            LoadTextureBlock(&gfx, g_Scorecard.tableImage + texelsOffset, G_IM_FMT_CI, G_IM_SIZ_8b, TABLE_WIDTH,
                             TABLE_LOAD_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
            texelsOffset += TABLE_LOAD_HEIGHT * TABLE_WIDTH;
            DrawTextureRect(&gfx, (g_Scorecard.x + TABLE_LEFT) << 2, (row + g_Scorecard.y + TAB_TOP + dropY) << 2,
                            (g_Scorecard.x + TABLE_LEFT + TABLE_SHOWN_WIDTH) << 2,
                            (row + (g_Scorecard.y + TAB_TOP + TABLE_LOAD_HEIGHT) + dropY) << 2, G_TX_RENDERTILE, 0, 0,
                            TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    texelsOffset = TABLE_BODY_OFFSET;
    for (row = 0; row < TABLE_BODY_HEIGHT; row += TABLE_LOAD_HEIGHT) {
        /* The body's last load is its one leftover row */
        loadHeight = (row + TABLE_LOAD_HEIGHT <= TABLE_BODY_HEIGHT) ? TABLE_LOAD_HEIGHT : 1;
        gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1, (u32)(g_Scorecard.tableImage + texelsOffset) & ~7);
        gDPLoadSync(gfx++);
        gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, loadHeight * (TABLE_WIDTH / 2) - 1,
                     CALC_DXT(TABLE_WIDTH, G_IM_SIZ_8b_BYTES));
        top = g_Scorecard.y + TABLE_BODY_TOP;
        texelsOffset += TABLE_LOAD_HEIGHT * TABLE_WIDTH;
        DrawTextureRect(&gfx, (g_Scorecard.x + TABLE_LEFT) << 2, (row + top) << 2,
                        (g_Scorecard.x + TABLE_LEFT + TABLE_SHOWN_WIDTH) << 2, (row + loadHeight + top) << 2,
                        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", DrawScorecardTable);
#endif

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
    LoadCharacter(0, (s8)g_Players[0].characterIndex + CHARACTER_COUNT, (s8)g_Players[0].paletteIndex);
    LoadCharacter(1, (s8)g_Players[1].characterIndex + CHARACTER_COUNT, (s8)g_Players[1].paletteIndex);
    song = SONG_MATCH_RESULT;
    if (g_GameMode == GAME_MODE_GET_CHARACTER) {
        if (g_Players[0].holes[holeIndex].strokes < g_Players[1].holes[holeIndex].strokes) {
            song = SONG_HOLE_WON;
        } else if (g_Players[1].holes[holeIndex].strokes < g_Players[0].holes[holeIndex].strokes) {
            song = SONG_HOLE_LOST;
        } else {
            song = SONG_HOLE_HALVED;
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
        case SCORECARD_LAYOUT_SPEED_GOLF:
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

#define HOLE_NUMBER_ENV_COLOR 0xFFFF0000
#define HOLE_NUMBER_PRIM_COLOR 0x30FF0000
/* The colors of the headings that some pages draw beside their columns */
#define HEADING_ENV_COLOR 0xFF730000
#define HEADING_PRIM_COLOR 0xFFFF1C00

/*
 * The standings page lists each player's place, face, score to par and,
 * when there are bets, bet total. Without bets its columns shift right.
 */
#define STANDINGS_HOLE_LEFT 40
#define STANDINGS_HOLE_TOP 48
#define NO_BETS_SHIFT 32
#define STANDINGS_PLACE_LEFT 32
#define STANDINGS_FACE_LEFT 108
#define STANDINGS_SCORE_LEFT 152
#define STANDINGS_BET_TOTAL_LEFT 224
#define STANDINGS_ROW_TOP 72
#define STANDINGS_TEXT_TOP 82
#define STANDINGS_ROW_SPACING 36
#define STANDINGS_LABEL_TOP 52
#define STANDINGS_SCORE_LABEL_LEFT 160
#define STANDINGS_BET_TOTAL_LABEL_LEFT 248
#define SCORE_LABEL 2     /* Drawn over the score column */
#define BET_TOTAL_LABEL 4 /* Drawn over the bet total column */
#define WIDE_BET_TOTAL 1000

/*
 * Draws the standings page of a round with more than one player: the hole,
 * then each player's place, face and score to par, and their bet total from
 * the bets that are worth something.
 */
void DrawStandingsPage(Gfx **displayListHead) {
    s32 places[MAX_PLAYER_COUNT];
    s32 scoresToPar[MAX_PLAYER_COUNT];
    Gfx *gfx;
    s32 holeIndex;
    s32 shift;
    s32 i;
    s32 j;
    s32 betTotal;

    gfx = *displayListHead;
    holeIndex = g_Scorecard.content.holeIndex;
    shift = g_Scorecard.hasBets ? 0 : NO_BETS_SHIFT;
    SetUpGradientTextDrawing(&gfx, 0);
    sprintf(g_PrintBuffer, "Hole %d", holeIndex + g_Scorecard.isTallied);
    DrawLargeTextWithShadow(&gfx, g_Scorecard.x + STANDINGS_HOLE_LEFT, g_Scorecard.y + STANDINGS_HOLE_TOP,
                            g_PrintBuffer, HOLE_NUMBER_ENV_COLOR, HOLE_NUMBER_PRIM_COLOR, 0);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);

    for (i = 0; i < g_GameState.playerCount; i++) {
        scoresToPar[i] = GetNetScoreToPar(g_Scorecard.content.holeIndex, i);
    }
    /* Each player's place counts the players ahead of them */
    for (i = 0; i != g_GameState.playerCount; i++) {
        places[i] = 0;
        for (j = 0; j != g_GameState.playerCount; j++) {
            if (i != j && scoresToPar[i] > scoresToPar[j]) {
                places[i]++;
            }
        }
    }

    gSPDisplayList(gfx++, D_800C3FC0);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    for (i = 0; i != g_GameState.playerCount; i++) {
        DrawScorecardFace(&gfx, shift + STANDINGS_FACE_LEFT + g_Scorecard.x,
                          STANDINGS_ROW_TOP + i * STANDINGS_ROW_SPACING, i);
    }
    for (i = 0; i != g_GameState.playerCount; i++) {
        DrawPlace(&gfx, shift + STANDINGS_PLACE_LEFT + g_Scorecard.x, STANDINGS_ROW_TOP + i * STANDINGS_ROW_SPACING,
                  places[i] & PLACE_MASK);
    }
    DrawScorecardLabel(&gfx, shift + STANDINGS_SCORE_LABEL_LEFT + g_Scorecard.x, STANDINGS_LABEL_TOP, SCORE_LABEL);
    if (g_Scorecard.hasBets) {
        DrawScorecardLabel(&gfx, g_Scorecard.x + STANDINGS_BET_TOTAL_LABEL_LEFT, STANDINGS_LABEL_TOP, BET_TOTAL_LABEL);
    }

    LoadSmallPalette(&gfx, 0, g_Scorecard.digitsImage + IMAGE_HEADER_SIZE);
    LoadNibbleTextureBlock(&gfx, g_Scorecard.digitsImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI, DIGIT_GLYPH_SIZE,
                           DIGIT_FONT_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    for (i = 0; i != g_GameState.playerCount; i++) {
        betTotal = 0;
        if (scoresToPar[i] == 0) {
            sprintf(g_PrintBuffer, sZeroToParFormat);
        } else {
            sprintf(g_PrintBuffer, sScoreToParFormat, scoresToPar[i]);
        }
        DrawDigitText(&gfx, shift + STANDINGS_SCORE_LEFT + g_Scorecard.x,
                      STANDINGS_TEXT_TOP + i * STANDINGS_ROW_SPACING, g_PrintBuffer);
        if (g_Scorecard.hasBets) {
            for (j = 0; j != BET_COUNT; j++) {
                if (g_GameState.betRates[j] > 0) {
                    betTotal += g_BetTallies[i][j];
                }
            }
            if (betTotal >= WIDE_BET_TOTAL) {
                sprintf(g_PrintBuffer, sWideTotalFormat, betTotal);
            } else {
                sprintf(g_PrintBuffer, sTotalFormat, betTotal);
            }
            DrawDigitText(&gfx, g_Scorecard.x + STANDINGS_BET_TOTAL_LEFT,
                          STANDINGS_TEXT_TOP + i * STANDINGS_ROW_SPACING, g_PrintBuffer);
        }
    }
    *displayListHead = gfx;
}

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", sZeroToParFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", sScoreToParFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", sWideTotalFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", sTotalFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", sCompactHoleFormat);

#define SOUND_UNK61 0x61
#define MATCH_SOUND_PAN 0x7F
/* SetTextSlot messages */
#define WINS_NEEDED_MESSAGE 1 /* Shows SetTextSlotValue's count */
#define LAST_WIN_NEEDED_MESSAGE 2 /* Shown instead of a count of one when the match can be drawn */
#define SUDDEN_DEATH_ENV_COLOR 0x00FFFF00
#define SUDDEN_DEATH_PRIM_COLOR 0xCF48FF00
#define MATCH_TITLE_LEFT 128
#define MATCH_TITLE_TOP 48
#define MATCH_ROW_SPACING 80
#define WINS_NEEDED_LEFT 88
#define WINS_NEEDED_TOP 72
/* The panel behind each player's row, drawn at twice its image's size */
#define PANEL_LEFT 32
#define PANEL_TOP 64
#define PANEL_WIDTH 128
#define PANEL_HEIGHT 32
#define PANEL_SHOWN_HEIGHT 62
#define PANEL_SECOND_TOP 144
#define TEXEL_STEP_HALF (TEXEL_STEP_ONE / 2)
/* The tiles that count off the wins needed, each from three pieces */
#define TILE_IMAGE_WIDTH 32
#define TILE_IMAGE_HEIGHT 96
#define TILE_LEFT 36
#define TILE_TOP 98
#define TILE_SPACING 25
#define TILE_PIECE_WIDTH 8
#define TILE_HEIGHT 24
#define TILES_PER_IMAGE_ROW 4
#define TILE_EDGE_IMAGE_ROW 3
#define WIDE_TILE_INDEX 9 /* The tenth tile has an image of its own, three pieces wide */
#define WIDE_TILE_LEFT 261
#define WIDE_TILE_IMAGE_COLUMN 1
#define WIDE_TILE_IMAGE_ROW 2
#define MATCH_FACE_LEFT 48
#define MATCH_FACE_TOP 64
/* The marks of the holes won, from a strip of images */
#define WIN_MARK_WIDTH 48
#define WIN_MARK_HEIGHT 24
#define WIN_MARK_SHOWN_WIDTH 24
#define WIN_MARK_TOP 97
#define WIN_MARK_SIZE (WIN_MARK_WIDTH * WIN_MARK_HEIGHT)
#define WIN_MARK_START_TIME 2
#define WIN_MARK_ANIMATION_TIME 18
#define WIN_MARK_FRAME_TIME 2
#define WIN_MARK_BOUNCE_HEIGHT 24.0f
#define WIN_MARK_BOUNCE_STEP (M_PI / 12)
/* The arrow that rises over the hole's winner */
#define ARROW_WIDTH 16
#define ARROW_HEIGHT 8
#define ARROW_LEFT 40
#define ARROW_TOP 81
#define MATCH_COUNT_TIME 20 /* When the hole's result joins the counts */
#define MATCH_ANIMATION_TIME 30

extern u8 sWinMarkFrames[];

/*
 * Draws a match's first page: the hole, any sudden death or dormie, and, for
 * each player, their face, the tiles of the holes they still need and a mark
 * for each hole won. Once the hole's result is tallied, it animates the
 * winner's new mark.
 *
 * Doesn't match: the target counts the tiles' left edges with one register
 * where this keeps one for each edge, so its frame is 0x98 bytes, not 0xB8,
 * and it halves winsNeeded before it subtracts one.
 */
#ifdef NON_MATCHING
void DrawMatchPage(Gfx **displayListHead) {
    s32 wins[MATCH_PLAYER_COUNT];
    Gfx *gfx;
    Gfx **head;
    s32 winsNeeded;
    s32 rowOffset;
    s32 canDraw = FALSE;
    s32 halvedCount;
    s32 countEnd;
    s32 holeIndex;
    s32 i;
    s32 j;
    s32 column;
    s32 imageRow;
    s32 frame;
    s32 bounce;

    head = displayListHead;
    gfx = *displayListHead;
    if (g_Scorecard.content.isSuddenDeath == TRUE) {
        winsNeeded = 2;
    } else {
        winsNeeded = GetRoundHoleCount() + 2;
    }
    if (g_Scorecard.skinsCarryover.onLastHole > 0) {
        if (++g_Scorecard.skinsCarryover.onLastHole == MATCH_COUNT_TIME) {
            g_Scorecard.skinsCarryover.intoLastHole++;
        }
        if (g_Scorecard.skinsCarryover.onLastHole == MATCH_ANIMATION_TIME) {
            g_Scorecard.skinsCarryover.onLastHole = 0;
        }
    }

    /* Each halved hole leaves one fewer to win */
    countEnd = g_Scorecard.skinsCarryover.intoLastHole;
    halvedCount = 0;
    wins[1] = 0;
    wins[0] = 0;
    for (i = GetFirstHoleIndex(); i != countEnd; i++) {
        if (g_Players[0].holes[i].strokes < g_Players[1].holes[i].strokes) {
            wins[0]++;
        } else if (g_Players[1].holes[i].strokes < g_Players[0].holes[i].strokes) {
            wins[1]++;
        } else if (g_Players[0].holes[i].strokes == g_Players[1].holes[i].strokes) {

            winsNeeded--;
            halvedCount++;
        }
    }
    if (g_Scorecard.skinsCarryover.onLastHole == MATCH_COUNT_TIME && g_Scorecard.content.isSuddenDeath == FALSE &&
        g_Scorecard.holeWinnerIndex == HOLE_WINNER_HALVED && (halvedCount & 1) == ((GetRoundHoleCount() - 1) & 1)) {
        PlaySoundWithEffects(SOUND_UNK61, TALLY_SOUND_CHANNEL, SCORECARD_SOUND_VOLUME, 0, MATCH_SOUND_PAN);
    }
    /* An even count of holes to share out can end level */
    if ((winsNeeded - 1) / 2 != (winsNeeded /= 2)) {
        canDraw = TRUE;
    }
    if (winsNeeded <= 0) {
        winsNeeded = 1;
    }

    SetUpGradientTextDrawing(&gfx, 0);
    gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0, 0xFF);
    gDPSetPrimColor(gfx++, 0, 0, 0x30, 0xFF, 0, 0xFF);
    sprintf(g_PrintBuffer, sCompactHoleFormat, g_Scorecard.content.holeIndex + g_Scorecard.isTallied);
    DrawLargeTextWithShadow(&gfx, g_Scorecard.x + STANDINGS_HOLE_LEFT, g_Scorecard.y + STANDINGS_HOLE_TOP,
                            g_PrintBuffer, HOLE_NUMBER_ENV_COLOR, HOLE_NUMBER_PRIM_COLOR, 0);
    if (g_Scorecard.content.isSuddenDeath) {
        sprintf(g_PrintBuffer, "SUDDEN DEATH");
        DrawLargeTextWithShadow(&gfx, g_Scorecard.x + MATCH_TITLE_LEFT, g_Scorecard.y + MATCH_TITLE_TOP,
                                g_PrintBuffer, SUDDEN_DEATH_ENV_COLOR, SUDDEN_DEATH_PRIM_COLOR, 0);
    } else {
        s32 points[MATCH_PLAYER_COUNT];

        /* A win counts two points and a halved hole one */
        points[0] = 0;
        points[1] = 0;
        for (i = GetFirstHoleIndex(); i != g_Scorecard.skinsCarryover.fromLastHole; i++) {
            if (g_Players[0].holes[i].strokes < g_Players[1].holes[i].strokes) {
                points[0] += 2;
            } else if (g_Players[1].holes[i].strokes < g_Players[0].holes[i].strokes) {
                points[1] += 2;
            } else if (g_Players[0].holes[i].strokes == g_Players[1].holes[i].strokes) {
                points[0]++;
                points[1]++;
            }
        }
        if (GetRoundHoleCount() - 1 < points[0] || GetRoundHoleCount() - 1 < points[1]) {
            sprintf(g_PrintBuffer, "DORMIE");
            DrawLargeTextWithShadow(&gfx, g_Scorecard.x + MATCH_TITLE_LEFT, g_Scorecard.y + MATCH_TITLE_TOP,
                                    g_PrintBuffer, HEADING_ENV_COLOR, HEADING_PRIM_COLOR, 0);
        }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gSPDisplayList(gfx++, D_800C3FC0);
    for (i = 0; i != MATCH_PLAYER_COUNT; i++) {
        if (canDraw == TRUE && winsNeeded - wins[i] == 1 && winsNeeded - wins[i ^ 1] >= 2) {
            SetTextSlot(g_Scorecard.x + WINS_NEEDED_LEFT, i * MATCH_ROW_SPACING + g_Scorecard.y + WINS_NEEDED_TOP,
                        LAST_WIN_NEEDED_MESSAGE, 1, i, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        } else if (winsNeeded != wins[i] && winsNeeded != wins[i ^ 1]) {
            SetTextSlotValue(winsNeeded - wins[i], i, 0);
            SetTextSlot(g_Scorecard.x + WINS_NEEDED_LEFT, i * MATCH_ROW_SPACING + g_Scorecard.y + WINS_NEEDED_TOP,
                        WINS_NEEDED_MESSAGE, 1, i, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        } else {
            SetTextSlot(0, 0, NO_MESSAGE, 1, i, 1, 0, 0, 0, 0);
        }
    }

    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadSmallPalette(&gfx, 0, g_Scorecard.unk2C + IMAGE_HEADER_SIZE);
    LoadNibbleTextureBlock(&gfx, g_Scorecard.unk2C + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI, PANEL_WIDTH, PANEL_HEIGHT, 0,
                           0, 0, 0, 0, 0, 0);
    DrawTextureRect(&gfx, (g_Scorecard.x + PANEL_LEFT) << 2, (g_Scorecard.y + PANEL_TOP) << 2,
                    (g_Scorecard.x + PANEL_LEFT + PANEL_WIDTH * 2) << 2,
                    (g_Scorecard.y + PANEL_TOP + PANEL_SHOWN_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_HALF,
                    TEXEL_STEP_HALF);
    DrawTextureRect(&gfx, (g_Scorecard.x + PANEL_LEFT) << 2, (g_Scorecard.y + PANEL_SECOND_TOP) << 2,
                    (g_Scorecard.x + PANEL_LEFT + PANEL_WIDTH * 2) << 2,
                    (g_Scorecard.y + PANEL_SECOND_TOP + PANEL_SHOWN_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                    TEXEL_STEP_HALF, TEXEL_STEP_HALF);

    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    LoadSmallPalette(&gfx, 0, g_Scorecard.unk3C + IMAGE_HEADER_SIZE);
    LoadNibbleTextureBlock(&gfx, g_Scorecard.unk3C + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI, TILE_IMAGE_WIDTH,
                           TILE_IMAGE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    gDPPipeSync(gfx++);
    for (i = 0, rowOffset = 0; i != MATCH_PLAYER_COUNT; i++, rowOffset += MATCH_ROW_SPACING) {
        for (j = 0; j != winsNeeded; j++) {
            column = j % TILES_PER_IMAGE_ROW;
            imageRow = j / TILES_PER_IMAGE_ROW;
            if (j == WIDE_TILE_INDEX) {
                DrawTextureRect(&gfx, (g_Scorecard.x + WIDE_TILE_LEFT) << 2,
                                (rowOffset + (g_Scorecard.y + TILE_TOP)) << 2,
                                (g_Scorecard.x + WIDE_TILE_LEFT + TILE_PIECE_WIDTH * 3) << 2,
                                (rowOffset + (g_Scorecard.y + TILE_TOP + TILE_HEIGHT)) << 2, G_TX_RENDERTILE,
                                (WIDE_TILE_IMAGE_COLUMN * TILE_PIECE_WIDTH) << TEXTURE_COORD_FRACTION_BITS,
                                (WIDE_TILE_IMAGE_ROW * TILE_HEIGHT) << TEXTURE_COORD_FRACTION_BITS, TEXEL_STEP_ONE,
                                TEXEL_STEP_ONE);
            } else {
                DrawTextureRect(&gfx, (j * TILE_SPACING + (g_Scorecard.x + TILE_LEFT)) << 2,
                                (rowOffset + (g_Scorecard.y + TILE_TOP)) << 2,
                                (j * TILE_SPACING + (g_Scorecard.x + TILE_LEFT + TILE_PIECE_WIDTH)) << 2,
                                (rowOffset + (g_Scorecard.y + TILE_TOP + TILE_HEIGHT)) << 2, G_TX_RENDERTILE, 0,
                                (TILE_EDGE_IMAGE_ROW * TILE_HEIGHT) << TEXTURE_COORD_FRACTION_BITS, TEXEL_STEP_ONE,
                                TEXEL_STEP_ONE);
                DrawTextureRect(&gfx, (j * TILE_SPACING + (g_Scorecard.x + TILE_LEFT + TILE_PIECE_WIDTH)) << 2,
                                (rowOffset + (g_Scorecard.y + TILE_TOP)) << 2,
                                (j * TILE_SPACING + (g_Scorecard.x + TILE_LEFT + TILE_PIECE_WIDTH * 2)) << 2,
                                (rowOffset + (g_Scorecard.y + TILE_TOP + TILE_HEIGHT)) << 2, G_TX_RENDERTILE,
                                (column * TILE_PIECE_WIDTH) << TEXTURE_COORD_FRACTION_BITS,
                                (imageRow * TILE_HEIGHT) << TEXTURE_COORD_FRACTION_BITS, TEXEL_STEP_ONE,
                                TEXEL_STEP_ONE);
                DrawTextureRect(&gfx, (j * TILE_SPACING + (g_Scorecard.x + TILE_LEFT + TILE_PIECE_WIDTH * 2)) << 2,
                                (rowOffset + (g_Scorecard.y + TILE_TOP)) << 2,
                                (j * TILE_SPACING + (g_Scorecard.x + TILE_LEFT + TILE_PIECE_WIDTH * 3)) << 2,
                                (rowOffset + (g_Scorecard.y + TILE_TOP + TILE_HEIGHT)) << 2, G_TX_RENDERTILE,
                                TILE_PIECE_WIDTH << TEXTURE_COORD_FRACTION_BITS,
                                (TILE_EDGE_IMAGE_ROW * TILE_HEIGHT) << TEXTURE_COORD_FRACTION_BITS, TEXEL_STEP_ONE,
                                TEXEL_STEP_ONE);
            }
        }
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    for (i = 0; i != g_PlayerCount; i++) {
        DrawScorecardFace(&gfx, g_Scorecard.x + MATCH_FACE_LEFT, MATCH_FACE_TOP + i * MATCH_ROW_SPACING, i);
    }

    /* A mark that the other player has matched shows its second image */
    gDPPipeSync(gfx++);
    LoadLargePalette(&gfx, g_Scorecard.unk38 + IMAGE_HEADER_SIZE);
    LoadTextureBlock(&gfx, g_Scorecard.unk38 + LARGE_PALETTED_TEXELS_OFFSET, G_IM_FMT_CI, G_IM_SIZ_8b, WIN_MARK_WIDTH,
                     WIN_MARK_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    gDPPipeSync(gfx++);
    for (i = 0; i != MATCH_PLAYER_COUNT; i++) {
        for (j = 0; j != wins[i]; j++) {
            DrawTextureRect(&gfx, (j * TILE_SPACING + (g_Scorecard.x + TILE_LEFT)) << 2,
                            (i * MATCH_ROW_SPACING + (g_Scorecard.y + WIN_MARK_TOP)) << 2,
                            (j * TILE_SPACING + (g_Scorecard.x + TILE_LEFT + WIN_MARK_SHOWN_WIDTH)) << 2,
                            (i * MATCH_ROW_SPACING + (g_Scorecard.y + WIN_MARK_TOP + WIN_MARK_HEIGHT)) << 2,
                            G_TX_RENDERTILE, 0,
                            (j < wins[i ^ 1]) ? WIN_MARK_HEIGHT << TEXTURE_COORD_FRACTION_BITS : 0, TEXEL_STEP_ONE,
                            TEXEL_STEP_ONE);
        }
    }

    if ((u8)(g_Scorecard.skinsCarryover.onLastHole - WIN_MARK_START_TIME) < WIN_MARK_ANIMATION_TIME) {
        frame = g_Scorecard.skinsCarryover.onLastHole - WIN_MARK_START_TIME;
        j = sWinMarkFrames[frame / WIN_MARK_FRAME_TIME];
        bounce = -(s32)(sinf((f32)frame * WIN_MARK_BOUNCE_STEP) * WIN_MARK_BOUNCE_HEIGHT);
        /* The mark only rises */
        bounce = (bounce <= 0) ? bounce : 0;
        LoadTextureBlock(&gfx, g_Scorecard.unk38 + LARGE_PALETTED_TEXELS_OFFSET + j * WIN_MARK_SIZE, G_IM_FMT_CI,
                         G_IM_SIZ_8b, WIN_MARK_WIDTH, WIN_MARK_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
        holeIndex = g_Scorecard.skinsCarryover.intoLastHole;
        if (g_Players[0].holes[holeIndex].strokes < g_Players[1].holes[holeIndex].strokes) {
            DrawTextureRect(&gfx, (wins[0] * TILE_SPACING + (g_Scorecard.x + TILE_LEFT)) << 2,
                            (bounce + (g_Scorecard.y + WIN_MARK_TOP)) << 2,
                            (wins[0] * TILE_SPACING + (g_Scorecard.x + TILE_LEFT + WIN_MARK_SHOWN_WIDTH)) << 2,
                            (bounce + (g_Scorecard.y + WIN_MARK_TOP + WIN_MARK_HEIGHT)) << 2, G_TX_RENDERTILE, 0,
                            (wins[1] < wins[0]) ? WIN_MARK_HEIGHT << TEXTURE_COORD_FRACTION_BITS : 0,
                            TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        } else if (g_Players[1].holes[holeIndex].strokes < g_Players[0].holes[holeIndex].strokes) {
            DrawTextureRect(&gfx, (wins[1] * TILE_SPACING + (g_Scorecard.x + TILE_LEFT)) << 2,
                            (bounce + (g_Scorecard.y + WIN_MARK_TOP + MATCH_ROW_SPACING)) << 2,
                            (wins[1] * TILE_SPACING + (g_Scorecard.x + TILE_LEFT + WIN_MARK_SHOWN_WIDTH)) << 2,
                            (bounce + (g_Scorecard.y + WIN_MARK_TOP + MATCH_ROW_SPACING + WIN_MARK_HEIGHT)) << 2,
                            G_TX_RENDERTILE, 0,
                            (wins[0] < wins[1]) ? WIN_MARK_HEIGHT << TEXTURE_COORD_FRACTION_BITS : 0,
                            TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }

        LoadSmallPalette(&gfx, 0, g_Scorecard.unk40 + IMAGE_HEADER_SIZE);
        LoadNibbleTextureBlock(&gfx, g_Scorecard.unk40 + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI, ARROW_WIDTH,
                               ARROW_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
        holeIndex = g_Scorecard.skinsCarryover.intoLastHole;
        if (g_Players[0].holes[holeIndex].strokes < g_Players[1].holes[holeIndex].strokes) {
            DrawTextureRect(&gfx, (wins[0] * TILE_SPACING + (g_Scorecard.x + ARROW_LEFT)) << 2,
                            (-g_Scorecard.skinsCarryover.onLastHole + (g_Scorecard.y + ARROW_TOP)) << 2,
                            (wins[0] * TILE_SPACING + (g_Scorecard.x + ARROW_LEFT + ARROW_WIDTH)) << 2,
                            (-g_Scorecard.skinsCarryover.onLastHole + (g_Scorecard.y + ARROW_TOP + ARROW_HEIGHT)) << 2,
                            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        } else if (g_Players[1].holes[holeIndex].strokes < g_Players[0].holes[holeIndex].strokes) {
            DrawTextureRect(&gfx, (wins[1] * TILE_SPACING + (g_Scorecard.x + ARROW_LEFT)) << 2,
                            (-g_Scorecard.skinsCarryover.onLastHole +
                             (g_Scorecard.y + ARROW_TOP + MATCH_ROW_SPACING)) << 2,
                            (wins[1] * TILE_SPACING + (g_Scorecard.x + ARROW_LEFT + ARROW_WIDTH)) << 2,
                            (-g_Scorecard.skinsCarryover.onLastHole +
                             (g_Scorecard.y + ARROW_TOP + MATCH_ROW_SPACING + ARROW_HEIGHT)) << 2,
                            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }
    }
    gDPPipeSync(gfx++);
    *head = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", DrawMatchPage);
#endif

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", sStringFormat);

/*
 * The skins page lists each player's place, face and skins, and the skins
 * carried over
 */
#define SKINS_TITLE_LEFT 122
#define SKINS_TITLE_TOP 52
#define SKINS_ROW_TOP 72
#define SKINS_ROW_SPACING 36
#define SKINS_WIN_LEFT 32
#define SKINS_WIN_TOP 82
#define SKINS_PLACE_LEFT 56
#define SKINS_FACE_LEFT 120
#define SKIN_ICON_LEFT 168
#define SKIN_ICON_TOP 80
#define SKIN_ICON_SIZE 24
#define SKINS_COUNT_LEFT 208
#define SKINS_COUNT_TOP 83
#define CARRYOVER_ICON_LEFT 204
#define CARRYOVER_ICON_TOP 46
#define CARRYOVER_COUNT_LEFT 236
#define CARRYOVER_COUNT_TOP 49
#define SKIN_ICON_TEXELS_OFFSET LARGE_PALETTED_TEXELS_OFFSET
/* Only the star and the digits */
#define SKINS_DIGIT_FONT_HEIGHT (DIGIT_GLYPH_COLON * DIGIT_GLYPH_SIZE)
#define M_PI 3.14159265358979323846
/* "Win" cycles through the colors, each channel a sine wave about mid-gray */
#define WIN_COLOR_STEP (M_PI / 9)
#define WIN_COLOR_MIDPOINT 0x7F
#define WIN_COLOR_SWING 127.0f

extern const char sCarryOverText[];
/* The angle the colors of "Win" are at */
extern f32 sWinColorAngle;

/*
 * Draws the skins page of a skins match or club slots round: the hole, each
 * player's place, face and skins, and the skins carried over. The last hole's
 * winner is shown without the skins still on it, which TallyScorecardSkins
 * counts down. While a hole has a winner, "Win" flashes beside each player
 * with strokes at holes[holeIndex].
 */
void DrawSkinsPage(Gfx **displayListHead) {
    s32 places[MAX_PLAYER_COUNT];
    s32 skinCounts[MAX_PLAYER_COUNT];
    Gfx *gfx;
    s32 holeIndex;
    s32 i;
    s32 j;
    s32 red;
    s32 green;

    gfx = *displayListHead;
    holeIndex = g_Scorecard.content.holeIndex;
    SetUpGradientTextDrawing(&gfx, 0);
    sprintf(g_PrintBuffer, sCompactHoleFormat, holeIndex + g_Scorecard.isTallied);
    DrawLargeTextWithShadow(&gfx, g_Scorecard.x + STANDINGS_HOLE_LEFT, g_Scorecard.y + STANDINGS_HOLE_TOP,
                            g_PrintBuffer, HOLE_NUMBER_ENV_COLOR, HOLE_NUMBER_PRIM_COLOR, 0);
    sprintf(g_PrintBuffer, sStringFormat, sCarryOverText);
    DrawLargeTextWithShadow(&gfx, g_Scorecard.x + SKINS_TITLE_LEFT, g_Scorecard.y + SKINS_TITLE_TOP, g_PrintBuffer,
                            HEADING_ENV_COLOR, HEADING_PRIM_COLOR, 0);

    for (i = 0; i != g_GameState.playerCount; i++) {
        if (g_Scorecard.holeWinnerIndex == i) {
            skinCounts[i] = (s16)g_RoundHud.skinsTotals[i] - g_Scorecard.skinsCarryover.onLastHole;
        } else {
            skinCounts[i] = (s16)g_RoundHud.skinsTotals[i];
        }
    }
    /* Each player's place counts the players with more skins */
    for (i = 0; i != g_GameState.playerCount; i++) {
        places[i] = 0;
        for (j = 0; j != g_GameState.playerCount; j++) {
            if (i != j) {
                if (skinCounts[i] < skinCounts[j]) {
                    places[i]++;
                }
                if (skinCounts[i] == skinCounts[j]) {
                    places[i] |= PLACE_TIED;
                }
            }
        }
    }

    for (i = 0; i != g_GameState.playerCount; i++) {
        if (g_Scorecard.holeWinnerIndex >= 0 && g_Players[i].holes[holeIndex].strokes != 0) {
            sWinColorAngle += WIN_COLOR_STEP;
            red = (u8)((s32)(sinf(sWinColorAngle) * WIN_COLOR_SWING) + WIN_COLOR_MIDPOINT);
            green = (u8)((s32)(sinf(sWinColorAngle + M_PI) * WIN_COLOR_SWING) + WIN_COLOR_MIDPOINT);
            SetTextColor(&gfx, red, green,
                         (u8)((s32)(sinf(sWinColorAngle + M_PI / 2) * WIN_COLOR_SWING) + WIN_COLOR_MIDPOINT));
            DrawSmallText(&gfx, g_Scorecard.x + SKINS_WIN_LEFT, SKINS_WIN_TOP + i * SKINS_ROW_SPACING, sWinText);
        }
    }

    gSPDisplayList(gfx++, D_800C3FC0);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    for (i = 0; i != g_GameState.playerCount; i++) {
        DrawScorecardFace(&gfx, g_Scorecard.x + SKINS_FACE_LEFT, SKINS_ROW_TOP + i * SKINS_ROW_SPACING, i);
    }
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    for (i = 0; i != g_GameState.playerCount; i++) {
        DrawPlace(&gfx, g_Scorecard.x + SKINS_PLACE_LEFT, SKINS_ROW_TOP + i * SKINS_ROW_SPACING,
                  places[i] & PLACE_MASK);
    }

    LoadLargePalette(&gfx, g_Scorecard.unk38 + IMAGE_HEADER_SIZE);
    LoadTextureBlock(&gfx, g_Scorecard.unk38 + SKIN_ICON_TEXELS_OFFSET, G_IM_FMT_CI, G_IM_SIZ_8b, SKIN_ICON_SIZE,
                     SKIN_ICON_SIZE, 0, 0, 0, 0, 0, 0, 0);
    for (i = 0; i != g_GameState.playerCount; i++) {
        DrawTextureRect(&gfx, (g_Scorecard.x + SKIN_ICON_LEFT) << 2, (SKIN_ICON_TOP + i * SKINS_ROW_SPACING) << 2,
                        (g_Scorecard.x + SKIN_ICON_LEFT + SKIN_ICON_SIZE) << 2,
                        (SKIN_ICON_TOP + SKIN_ICON_SIZE + i * SKINS_ROW_SPACING) << 2, G_TX_RENDERTILE, 0, 0,
                        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
    DrawTextureRect(&gfx, (g_Scorecard.x + CARRYOVER_ICON_LEFT) << 2, CARRYOVER_ICON_TOP << 2,
                    (g_Scorecard.x + CARRYOVER_ICON_LEFT + SKIN_ICON_SIZE) << 2,
                    (CARRYOVER_ICON_TOP + SKIN_ICON_SIZE) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE);

    LoadSmallPalette(&gfx, 0, g_Scorecard.digitsImage + IMAGE_HEADER_SIZE);
    LoadNibbleTextureBlock(&gfx, g_Scorecard.digitsImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI, DIGIT_GLYPH_SIZE,
                           SKINS_DIGIT_FONT_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    for (i = 0; i != g_GameState.playerCount; i++) {
        sprintf(g_PrintBuffer, "* %d", skinCounts[i]);
        DrawDigitText(&gfx, g_Scorecard.x + SKINS_COUNT_LEFT, SKINS_COUNT_TOP + i * SKINS_ROW_SPACING, g_PrintBuffer);
    }
    sprintf(g_PrintBuffer, "* %d",
            (g_Scorecard.isTallied == TRUE) ? g_Scorecard.skinsCarryover.fromLastHole
                                            : g_Scorecard.skinsCarryover.intoLastHole);
    DrawDigitText(&gfx, g_Scorecard.x + CARRYOVER_COUNT_LEFT, CARRYOVER_COUNT_TOP, g_PrintBuffer);
    *displayListHead = gfx;
}

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C848_ovl_09);

/* The bets page: a grid with a column for each bet being played and one for the totals */
#define BET_COLUMN_SPACING 35
#define BET_NAME_SPACING 36
#define BET_BLINK_TIME 7
#define UNPLAYED_BET_GRAY 0xBF
/* The grid's lines are cut from a small image of 3 texel pieces */
#define GRID_IMAGE_SIZE 16
#define GRID_LINE_WIDTH 3
#define GRID_LEFT 32
#define GRID_RIGHT 292
#define GRID_TOP 64
#define GRID_BOTTOM 220
#define GRID_HEADER_LINE_TOP 74
#define GRID_ROW_LINE_TOP 96
#define GRID_ROW_LINE_LEFT 28
#define GRID_ROW_COUNT 4
#define GRID_ROW_HEIGHT 32
#define GRID_COLUMN_LINE_LEFT 54
#define GRID_COLUMN_COUNT 7
#define GRID_STRETCHED_STEP 11 /* Stretches one texel along a header line */
#define GRID_COLUMN_STEP 18    /* Stretches the column line's texels down the grid */
#define GRID_TEXEL_LINE 3
#define GRID_TEXEL_TOP_END 6
#define GRID_TEXEL_BOTTOM_END 9
#define GRID_TEXEL_CROSSING 12
/* The bets' heading images, and the totals' heading after them */
#define BET_IMAGE_SIZE 24
#define BET_IMAGE_LEFT 60
#define BET_IMAGE_TOP 46
/* The subtitles under some bets' headings, from rows of one image */
#define BET_SUBTITLE_WIDTH 32
#define BET_SUBTITLE_HEIGHT 16
#define BET_SUBTITLE_IMAGE_HEIGHT 48
#define BET_SUBTITLE_LEFT 56
#define BET_SUBTITLE_TOP 58
#define NO_BET_SUBTITLE -1
#define BET_FACE_LEFT 18
#define BET_FACE_TOP 100
#define BET_NAME_LEFT 71
#define BET_NAME_TOP 46
#define BET_RATE_LEFT 74
#define BET_RATE_TOP 48
#define BET_RATE_RED 0xFF
#define BET_RATE_GREEN 0xA0
#define CARRY_OVER_TOP 78
#define CARRY_OVER_SECOND_TOP 86
#define BET_POT_LEFT 69
#define BET_POT_TOP 86
#define BET_TALLY_LEFT 53
#define NO_TALLY_LEFT 69
#define BET_TALLY_TOP 112

/* The headings blink between their two images, a frame each this many draws */
extern s32 sBetBlinkFrame;
extern s32 sBetBlinkTime;
extern s8 sBetSubtitleRows[BET_COUNT];
extern char *sBetNames[BET_COUNT];

/*
 * Draws the bets page: each bet being played, its rate, any pot carried over,
 * and what each player has won on it and in total. A bet with a rate of zero is
 * grayed out.
 *
 * Doesn't match: the target adds each offset to the position it loads, where
 * this folds the offsets' constants into extra loop registers, so its frame
 * is 0x70 bytes, not 0x78.
 */
#ifdef NON_MATCHING
void DrawBetsPage(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 i;
    s32 j;
    s32 left;
    s32 frame;
    s32 column;
    s32 total;

    if (++sBetBlinkTime >= BET_BLINK_TIME) {
        sBetBlinkTime = 0;
        sBetBlinkFrame ^= 1;
    }

    gSPDisplayList(gfx++, D_800C3FC0);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    LoadSmallPalette(&gfx, 0, g_Scorecard.gridImage + IMAGE_HEADER_SIZE);
    LoadNibbleTextureBlock(&gfx, g_Scorecard.gridImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI, GRID_IMAGE_SIZE,
                           GRID_IMAGE_SIZE, 0, 0, 0, 0, 0, 0, 0);
    DrawTextureRect(&gfx, (g_Scorecard.x + GRID_LEFT) << 2, (g_Scorecard.y + GRID_HEADER_LINE_TOP) << 2,
                    (g_Scorecard.x + GRID_RIGHT) << 2, (g_Scorecard.y + GRID_HEADER_LINE_TOP + GRID_LINE_WIDTH) << 2,
                    G_TX_RENDERTILE, GRID_TEXEL_LINE << TEXTURE_COORD_FRACTION_BITS, 0, GRID_STRETCHED_STEP,
                    TEXEL_STEP_ONE);
    for (i = 0; i != GRID_ROW_COUNT; i++) {
        DrawTextureRect(&gfx, (g_Scorecard.x + GRID_ROW_LINE_LEFT) << 2,
                        (i * GRID_ROW_HEIGHT + (g_Scorecard.y + GRID_ROW_LINE_TOP)) << 2,
                        (g_Scorecard.x + GRID_RIGHT) << 2,
                        (i * GRID_ROW_HEIGHT + (g_Scorecard.y + GRID_ROW_LINE_TOP + GRID_LINE_WIDTH)) << 2,
                        G_TX_RENDERTILE, GRID_TEXEL_LINE << TEXTURE_COORD_FRACTION_BITS, 0, GRID_STRETCHED_STEP,
                        TEXEL_STEP_ONE);
        DrawTextureRect(&gfx, (g_Scorecard.x + GRID_ROW_LINE_LEFT) << 2,
                        (i * GRID_ROW_HEIGHT + (g_Scorecard.y + GRID_ROW_LINE_TOP)) << 2,
                        (g_Scorecard.x + GRID_ROW_LINE_LEFT + GRID_LINE_WIDTH) << 2,
                        (i * GRID_ROW_HEIGHT + (g_Scorecard.y + GRID_ROW_LINE_TOP + GRID_LINE_WIDTH)) << 2,
                        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        DrawTextureRect(&gfx, (g_Scorecard.x + GRID_RIGHT - GRID_LINE_WIDTH) << 2,
                        (i * GRID_ROW_HEIGHT + (g_Scorecard.y + GRID_ROW_LINE_TOP)) << 2,
                        (g_Scorecard.x + GRID_RIGHT) << 2,
                        (i * GRID_ROW_HEIGHT + (g_Scorecard.y + GRID_ROW_LINE_TOP + GRID_LINE_WIDTH)) << 2,
                        G_TX_RENDERTILE, 0, GRID_TEXEL_LINE << TEXTURE_COORD_FRACTION_BITS, TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE);
    }
    for (i = 0, left = 0; i != GRID_COLUMN_COUNT; i++, left += BET_COLUMN_SPACING) {
        DrawTextureRect(&gfx, (left + (g_Scorecard.x + GRID_COLUMN_LINE_LEFT)) << 2, (g_Scorecard.y + GRID_TOP) << 2,
                        (left + (g_Scorecard.x + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH)) << 2,
                        (g_Scorecard.y + GRID_BOTTOM) << 2, G_TX_RENDERTILE,
                        GRID_TEXEL_LINE << TEXTURE_COORD_FRACTION_BITS,
                        GRID_TEXEL_LINE << TEXTURE_COORD_FRACTION_BITS, TEXEL_STEP_ONE, GRID_COLUMN_STEP);
        DrawTextureRect(&gfx, (left + (g_Scorecard.x + GRID_COLUMN_LINE_LEFT)) << 2, (g_Scorecard.y + GRID_TOP) << 2,
                        (left + (g_Scorecard.x + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH)) << 2,
                        (g_Scorecard.y + GRID_TOP + GRID_LINE_WIDTH) << 2, G_TX_RENDERTILE, 0,
                        GRID_TEXEL_TOP_END << TEXTURE_COORD_FRACTION_BITS, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        DrawTextureRect(&gfx, (left + (g_Scorecard.x + GRID_COLUMN_LINE_LEFT)) << 2,
                        (g_Scorecard.y + GRID_BOTTOM - GRID_LINE_WIDTH) << 2,
                        (left + (g_Scorecard.x + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH)) << 2,
                        (g_Scorecard.y + GRID_BOTTOM) << 2, G_TX_RENDERTILE, 0,
                        GRID_TEXEL_BOTTOM_END << TEXTURE_COORD_FRACTION_BITS, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
    /* Where the lines cross */
    for (i = 0, left = 0; i != GRID_COLUMN_COUNT; i++, left += BET_COLUMN_SPACING) {
        DrawTextureRect(&gfx, (left + (g_Scorecard.x + GRID_COLUMN_LINE_LEFT)) << 2,
                        (g_Scorecard.y + GRID_HEADER_LINE_TOP) << 2,
                        (left + (g_Scorecard.x + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH)) << 2,
                        (g_Scorecard.y + GRID_HEADER_LINE_TOP + GRID_LINE_WIDTH) << 2, G_TX_RENDERTILE, 0,
                        GRID_TEXEL_CROSSING << TEXTURE_COORD_FRACTION_BITS, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        for (j = 0; j != GRID_ROW_COUNT; j++) {
            DrawTextureRect(&gfx, (left + (g_Scorecard.x + GRID_COLUMN_LINE_LEFT)) << 2,
                            (j * GRID_ROW_HEIGHT + (g_Scorecard.y + GRID_ROW_LINE_TOP)) << 2,
                            (left + (g_Scorecard.x + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH)) << 2,
                            (j * GRID_ROW_HEIGHT + (g_Scorecard.y + GRID_ROW_LINE_TOP + GRID_LINE_WIDTH)) << 2,
                            G_TX_RENDERTILE, 0, GRID_TEXEL_CROSSING << TEXTURE_COORD_FRACTION_BITS, TEXEL_STEP_ONE,
                            TEXEL_STEP_ONE);
        }
    }

    gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
    for (i = 0, column = 0, left = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] != UNSET_BET_RATE) {
            frame = 0;
            if (g_GameState.betRates[i] == 0) {
                gDPSetPrimColor(gfx++, 0, 0, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY, 0xFF);
            } else {
                frame = sBetBlinkFrame;
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
            }
            LoadSmallPalette(&gfx, 0, g_Scorecard.betImages[column][frame] + IMAGE_HEADER_SIZE);
            gDPLoadTextureTile_4b(gfx++, g_Scorecard.betImages[column][frame] + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI,
                                  BET_IMAGE_SIZE, BET_IMAGE_SIZE, 0, 0, BET_IMAGE_SIZE - 1, BET_IMAGE_SIZE - 1, 0,
                                  G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                  G_TX_NOLOD, G_TX_NOLOD);
            DrawTextureRect(&gfx, (left + (g_Scorecard.x + BET_IMAGE_LEFT)) << 2, BET_IMAGE_TOP << 2,
                            (left + (g_Scorecard.x + BET_IMAGE_LEFT + BET_IMAGE_SIZE)) << 2,
                            (BET_IMAGE_TOP + BET_IMAGE_SIZE) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                            TEXEL_STEP_ONE);
            column++;
            left += BET_COLUMN_SPACING;
        }
    }
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
    LoadSmallPalette(&gfx, 0, g_Scorecard.totalHeadingImage + IMAGE_HEADER_SIZE);
    gDPLoadTextureTile_4b(gfx++, g_Scorecard.totalHeadingImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI, BET_IMAGE_SIZE,
                          BET_IMAGE_SIZE, 0, 0, BET_IMAGE_SIZE - 1, BET_IMAGE_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
                          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    DrawTextureRect(&gfx, (column * BET_COLUMN_SPACING + (g_Scorecard.x + BET_IMAGE_LEFT)) << 2, BET_IMAGE_TOP << 2,
                    (column * BET_COLUMN_SPACING + (g_Scorecard.x + BET_IMAGE_LEFT + BET_IMAGE_SIZE)) << 2,
                    (BET_IMAGE_TOP + BET_IMAGE_SIZE) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);

    LoadSmallPalette(&gfx, 0, g_Scorecard.betSubtitlesImage + IMAGE_HEADER_SIZE);
    LoadNibbleTextureBlock(&gfx, g_Scorecard.betSubtitlesImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI,
                           BET_SUBTITLE_WIDTH, BET_SUBTITLE_IMAGE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    for (i = 0, left = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] != UNSET_BET_RATE) {
            if (sBetSubtitleRows[i] != NO_BET_SUBTITLE) {
                if (g_GameState.betRates[i] == 0) {
                    SetTextColor(&gfx, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY);
                } else {
                    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
                }
                DrawTextureRect(&gfx, (left + (g_Scorecard.x + BET_SUBTITLE_LEFT)) << 2, BET_SUBTITLE_TOP << 2,
                                (left + (g_Scorecard.x + BET_SUBTITLE_LEFT + BET_SUBTITLE_WIDTH)) << 2,
                                (BET_SUBTITLE_TOP + BET_SUBTITLE_HEIGHT) << 2, G_TX_RENDERTILE, 0,
                                (sBetSubtitleRows[i] * BET_SUBTITLE_HEIGHT) << TEXTURE_COORD_FRACTION_BITS,
                                TEXEL_STEP_ONE, TEXEL_STEP_ONE);
            }
            left += BET_COLUMN_SPACING;
        }
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    for (i = 0; i != g_PlayerCount; i++) {
        DrawSmallFace(&gfx, g_Scorecard.x + BET_FACE_LEFT, BET_FACE_TOP + i * GRID_ROW_HEIGHT, i, TRUE);
    }

    BeginShadowedText(&gfx);
    for (i = 0, left = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] != UNSET_BET_RATE) {
            if (g_GameState.betRates[i] == 0) {
                SetTextColor(&gfx, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY);
            } else {
                SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
            }
            DrawCenteredSmallText(&gfx, left + (g_Scorecard.x + BET_NAME_LEFT), g_Scorecard.y + BET_NAME_TOP,
                                  sBetNames[i]);
            left += BET_NAME_SPACING;
        }
    }
    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
    /* Drawn a letter at a time, each at its own position */
    DrawSmallText(&gfx, g_Scorecard.x + 21, g_Scorecard.y + CARRY_OVER_TOP, "C");
    DrawSmallText(&gfx, g_Scorecard.x + 29, g_Scorecard.y + CARRY_OVER_TOP, "a");
    DrawSmallText(&gfx, g_Scorecard.x + 35, g_Scorecard.y + CARRY_OVER_TOP, "r");
    DrawSmallText(&gfx, g_Scorecard.x + 40, g_Scorecard.y + CARRY_OVER_TOP, "r");
    DrawSmallText(&gfx, g_Scorecard.x + 46, g_Scorecard.y + CARRY_OVER_TOP, "y");
    DrawSmallText(&gfx, g_Scorecard.x + 24, g_Scorecard.y + CARRY_OVER_SECOND_TOP, "o");
    DrawSmallText(&gfx, g_Scorecard.x + 31, g_Scorecard.y + CARRY_OVER_SECOND_TOP, "v");
    DrawSmallText(&gfx, g_Scorecard.x + 39, g_Scorecard.y + CARRY_OVER_SECOND_TOP, "e");
    DrawSmallText(&gfx, g_Scorecard.x + 45, g_Scorecard.y + CARRY_OVER_SECOND_TOP, "r");
    /* A pot holds its bet's rate plus whatever ties carried over */
    for (i = 0, column = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] > 0) {
            if (i == BET_LONGEST_DRIVE) {
                sprintf(g_PrintBuffer, sTotalFormat, g_LongestDrivePot - g_GameState.betRates[BET_LONGEST_DRIVE]);
                DrawCenteredSmallText(&gfx, column * BET_COLUMN_SPACING + (g_Scorecard.x + BET_POT_LEFT),
                                      g_Scorecard.y + BET_POT_TOP, g_PrintBuffer);
            }
            if (i == BET_NEAREST_PIN) {
                sprintf(g_PrintBuffer, sTotalFormat, g_NearestPinPot - g_GameState.betRates[BET_NEAREST_PIN]);
                DrawCenteredSmallText(&gfx, column * BET_COLUMN_SPACING + (g_Scorecard.x + BET_POT_LEFT),
                                      g_Scorecard.y + BET_POT_TOP, g_PrintBuffer);
            }
            if (i == BET_UNK4) {
                sprintf(g_PrintBuffer, sTotalFormat, g_HolePot - g_GameState.betRates[BET_UNK4]);
                DrawCenteredSmallText(&gfx, column * BET_COLUMN_SPACING + (g_Scorecard.x + BET_POT_LEFT),
                                      g_Scorecard.y + BET_POT_TOP, g_PrintBuffer);
            }
            if (i == BET_UNK5) {
                sprintf(g_PrintBuffer, sTotalFormat, g_HolePot - g_GameState.betRates[BET_UNK5]);
                DrawCenteredSmallText(&gfx, column * BET_COLUMN_SPACING + (g_Scorecard.x + BET_POT_LEFT),
                                      g_Scorecard.y + BET_POT_TOP, g_PrintBuffer);
            }
        }
        column += g_GameState.betRates[i] >= 0;
    }

    BeginShadowedText(&gfx);
    SetTextColor(&gfx, BET_RATE_RED, BET_RATE_GREEN, 0);
    for (i = 0, left = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] != UNSET_BET_RATE) {
            if (g_GameState.betRates[i] != 0) {
                sprintf(g_PrintBuffer, sTwoDigitFormat, g_GameState.betRates[i]);
                DrawCondensedShadowedText(&gfx, left + (g_Scorecard.x + BET_RATE_LEFT), g_Scorecard.y + BET_RATE_TOP,
                                          g_PrintBuffer);
            }
            left += BET_COLUMN_SPACING;
        }
    }
    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
    for (i = 0; i != g_PlayerCount; i++) {
        total = 0;
        column = 0;
        for (j = 0, left = 0; j != BET_COUNT; j++) {
            if (g_GameState.betRates[j] == 0) {
                column++;
                DrawShadowedText(&gfx, left + (g_Scorecard.x + NO_TALLY_LEFT),
                                 i * GRID_ROW_HEIGHT + (g_Scorecard.y + BET_TALLY_TOP), "-");
                left += BET_COLUMN_SPACING;
            } else if (g_GameState.betRates[j] != UNSET_BET_RATE) {
                column++;
                total += g_BetTallies[i][j];
                sprintf(g_PrintBuffer, sWideTotalFormat, g_BetTallies[i][j]);
                DrawCondensedShadowedText(&gfx, left + (g_Scorecard.x + BET_TALLY_LEFT),
                                          i * GRID_ROW_HEIGHT + (g_Scorecard.y + BET_TALLY_TOP), g_PrintBuffer);
                left += BET_COLUMN_SPACING;
            }
        }
        sprintf(g_PrintBuffer, sWideTotalFormat, total);
        DrawCondensedShadowedText(&gfx, column * BET_COLUMN_SPACING + (g_Scorecard.x + BET_TALLY_LEFT),
                                  i * GRID_ROW_HEIGHT + (g_Scorecard.y + BET_TALLY_TOP), g_PrintBuffer);
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", DrawBetsPage);
#endif

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", sTwoDigitFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard", D_8024C86C_ovl_09);

/* The Speed Golf page: the front nine's times on the left, the back nine's on the right */
#define HOLE_NUMBER_LEFT 32
#define HOLE_TIME_LEFT 64
#define BACK_NINE_SPACING 128
#define HOLE_NUMBER_TOP 50
#define HOLE_TIME_TOP 51
#define HOLE_ROW_SPACING 16
#define NINE_HOLE_COUNT 9
#define ROUND_TOTAL_LABEL_LEFT 96
#define ROUND_TOTAL_LEFT 160
#define ROUND_TIME_TOP 194
#define ROUND_SCORE_TOP 208

/*
 * Draws the Speed Golf page: each hole's time, with dashes for a hole without
 * one, and the round's time and score so far.
 *
 * Doesn't match: the target indexes the minute arrays from the offset it
 * shares with the other time arrays, where this steps pointers through them,
 * and it adds the rows' offsets to the scorecard's y in a different order.
 */
#ifdef NON_MATCHING
void DrawSpeedGolfPage(Gfx **displayListHead) {
    s32 shownHoleCount = g_Scorecard.content.holeIndex + (g_Players[0].unk26 != 0);
    Gfx *gfx = *displayListHead;
    s32 i;
    s32 minutes;
    s32 scoreToPar;

    SetUpGradientTextDrawing(&gfx, 0);
    for (i = 0; i != NINE_HOLE_COUNT; i++) {
        sprintf(g_PrintBuffer, sTwoDigitFormat, i + 1);
        DrawLargeTextWithShadow(&gfx, g_Scorecard.x + HOLE_NUMBER_LEFT,
                                i * HOLE_ROW_SPACING + (g_Scorecard.y + HOLE_NUMBER_TOP), g_PrintBuffer,
                                HOLE_NUMBER_ENV_COLOR, HOLE_NUMBER_PRIM_COLOR, 0);
        sprintf(g_PrintBuffer, sTwoDigitFormat, i + NINE_HOLE_COUNT + 1);
        DrawLargeTextWithShadow(&gfx, g_Scorecard.x + HOLE_NUMBER_LEFT + BACK_NINE_SPACING,
                                i * HOLE_ROW_SPACING + (g_Scorecard.y + HOLE_NUMBER_TOP), g_PrintBuffer,
                                HOLE_NUMBER_ENV_COLOR, HOLE_NUMBER_PRIM_COLOR, 0);
    }
    sprintf(g_PrintBuffer, "Time");
    DrawLargeTextWithShadow(&gfx, g_Scorecard.x + ROUND_TOTAL_LABEL_LEFT, g_Scorecard.y + ROUND_TIME_TOP,
                            g_PrintBuffer, HEADING_ENV_COLOR, HEADING_PRIM_COLOR, 0);
    sprintf(g_PrintBuffer, "Score");
    DrawLargeTextWithShadow(&gfx, g_Scorecard.x + ROUND_TOTAL_LABEL_LEFT, g_Scorecard.y + ROUND_SCORE_TOP,
                            g_PrintBuffer, HEADING_ENV_COLOR, HEADING_PRIM_COLOR, 0);

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
    for (i = 0; i != NINE_HOLE_COUNT; i++) {
        if (i < shownHoleCount) {
            minutes = g_HoleTimeMinutes[i];
            if (minutes < 0) {
                sprintf(g_PrintBuffer, sStringFormat, "--------");
            } else {
                sprintf(g_PrintBuffer, "%02d:%02d:%02d", minutes, g_HoleTimeSeconds[i],
                        g_HoleTimeHundredths[i]);
            }
            DrawMediumText(&gfx, g_Scorecard.x + HOLE_TIME_LEFT, i * HOLE_ROW_SPACING + (g_Scorecard.y + HOLE_TIME_TOP),
                           g_PrintBuffer);
        }
        if (i + NINE_HOLE_COUNT < shownHoleCount) {
            minutes = g_HoleTimeMinutes[i + NINE_HOLE_COUNT];
            if (minutes < 0) {
                sprintf(g_PrintBuffer, sStringFormat, "--------");
            } else {
                sprintf(g_PrintBuffer, "%02d:%02d:%02d", minutes, g_HoleTimeSeconds[i + NINE_HOLE_COUNT],
                        g_HoleTimeHundredths[i + NINE_HOLE_COUNT]);
            }
            DrawMediumText(&gfx, g_Scorecard.x + HOLE_TIME_LEFT + BACK_NINE_SPACING,
                           i * HOLE_ROW_SPACING + (g_Scorecard.y + HOLE_TIME_TOP), g_PrintBuffer);
        }
    }

    sprintf(g_PrintBuffer, "%02d:%02d:%02d",
            g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND / SECONDS_PER_MINUTE,
            g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND % SECONDS_PER_MINUTE,
            (s32)(g_GameState.stopwatchFrameCount * ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
                HUNDREDTHS_PER_SECOND);
    DrawFixedWidthLargeText(&gfx, g_Scorecard.x + ROUND_TOTAL_LEFT, g_Scorecard.y + ROUND_TIME_TOP, g_PrintBuffer);
    scoreToPar = GetNetScoreToPar(shownHoleCount, 0);
    if (scoreToPar == 0) {
        sprintf(g_PrintBuffer, "#0");
    } else {
        sprintf(g_PrintBuffer, "%+2d", scoreToPar);
    }
    DrawFixedWidthLargeText(&gfx, g_Scorecard.x + ROUND_TOTAL_LEFT, g_Scorecard.y + ROUND_SCORE_TOP, g_PrintBuffer);
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", DrawSpeedGolfPage);
#endif

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

