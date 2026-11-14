#include "scorecard.h"

#include <framebuffer_count.h>
#include <ultra64.h>

#include "bets.h"
#include "character.h"
#include "character_model.h"
#include "controllers/transfer_pak.h"
#include "course.h"
#include "game_flags.h"
#include "game_state.h"
#include "hole_loader.h"
#include "math/math_util.h"
#include "play.h"
#include "player.h"
#include "reaction.h"
#include "round_hud.h"
#include "round_stats.h"
#include "scene.h"
#include "scoring.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "screen/screen_layers.h"
#include "screen/sky_and_hud_animations.h"
#include "sound.h"
#include "system/asset.h"
#include "system/compiler.h"
#include "system/frame_time.h"
#include "system/heap.h"
#include "system/memory_address.h"
#include "text.h"
#include "tournament.h"

#define MATCH_RESULT_SOUND_TIME 1
#define MATCH_RESULT_TIME 5
#define SKINS_TALLY_TIME 20
#define SKINS_TALLY_INTERVAL 4
#define MATCH_RESULT_SOUND_CHANNEL 14

#define SCORECARD_OPEN_TIME 12
#define SCORECARD_CLOSE_TIME 10
#define SCORECARD_SLIDE_SPEED 32
#define SCORECARD_CLOSE_ACCELERATION 8
#define FACE_FRAME_COUNT 5
#define FACE_FRAME_DURATION 3

#define SCORECARD_SLIDE_SOUND_REVERB 20
#define SCORECARD_OPEN_SOUND_PAN 0xDF
#define SCORECARD_CLOSE_SOUND_PAN 0x20

#define UNSPLIT_IMAGE_WIDTH_MAX 96

#define LEADERBOARD_LAST_TOP_ROW (TOURNAMENT_CONTESTANT_COUNT - LEADERBOARD_ROW_COUNT)
#define ROW_FLIP_DEGREES_PER_FRAME 20
#define ROW_FLIP_END_DEGREES 175
#define ROW_FLIP_SOUND_CHANNEL 13

#define BEHIND_FACE_IMAGE_OFFSET 4
#define THREE_PAGE_COUNT 3

void DrawScorecardTable(Gfx **displayListHead, s32 pageCount, s32 pageIndex);

void SetScorecardState(ScorecardState state);

#define ASSET_TABLE_IMAGE 0x68F
#define ASSET_PLACE_ARROW_IMAGE 0x682
#define ASSET_RAISED_PANEL_IMAGE 0x690
#define ASSET_SCORES_PANEL_IMAGE 0x691
#define ASSET_BET_SUBTITLES_IMAGE 0x97A
#define ASSET_TOTAL_LABELS_IMAGE 0x693
#define ASSET_MATCH_PANEL_IMAGE 0x68E
#define ASSET_WIN_MARK_IMAGE 0x68C
#define ASSET_WINS_NEEDED_IMAGE 0x68D
#define ASSET_WINNER_ARROW_IMAGE 0x694
#define ASSET_SKIN_ICON_IMAGE 0x68B
#define ASSET_FLAP_NAME_IMAGES 0x695
#define ASSET_LEADERBOARD_ROW_IMAGE 0x692
#define ASSET_TOTAL_HEADING_IMAGE 0x696
#define ASSET_GRID_IMAGE 0x69D
#define ASSET_SCORECARD_TITLE 0x683
#define ASSET_LEADERBOARD_TITLE 0x684
#define ASSET_SPEED_GOLF_TITLE 0x685
#define ASSET_VERSUS_BOARD_TITLE 0x686
#define ASSET_BET_TITLE 0x687
#define ASSET_LAS_VEGAS_BET_IMAGE 0x672
#define ASSET_LONGEST_DRIVE_BET_IMAGE 0x674
#define ASSET_MATCH_POT_BET_IMAGE 0x676
#define ASSET_NEAREST_PIN_BET_IMAGE 0x678
#define ASSET_STROKES_BET_IMAGE 0x67A
#define ASSET_SKINS_BET_IMAGE 0x67C
#define ASSET_NASSAU_BET_IMAGE 0x67E

#define PAGE_TITLES_SPEED_GOLF 0
#define PAGE_TITLES_SOLO 1
#define PAGE_TITLES_VERSUS 2
#define PAGE_TITLES_SKINS 3
#define PAGE_TITLES_TOURNAMENT 4
#define PAGE_TITLES_ROW_COUNT 5

static s32 f_UnusedWordsBeforePageTitles[] UNUSED = {0, 1, 2, 3, 5, 10};
static AssetId f_PageTitleAssets[PAGE_TITLES_ROW_COUNT][PAGE_TITLE_COUNT] = {
    {ASSET_SPEED_GOLF_TITLE, ASSET_SCORECARD_TITLE, ASSET_SCORECARD_TITLE},
    {ASSET_SCORECARD_TITLE, ASSET_BET_TITLE, ASSET_BET_TITLE},
    {ASSET_VERSUS_BOARD_TITLE, ASSET_SCORECARD_TITLE, ASSET_BET_TITLE},
    {ASSET_VERSUS_BOARD_TITLE, ASSET_SCORECARD_TITLE, ASSET_BET_TITLE},
    {ASSET_LEADERBOARD_TITLE, ASSET_SCORECARD_TITLE, ASSET_SCORECARD_TITLE},
};
static AssetId f_BetImageAssets[BET_COUNT][2] = {
    {ASSET_LONGEST_DRIVE_BET_IMAGE, ASSET_LONGEST_DRIVE_BET_IMAGE + 1},
    {ASSET_NEAREST_PIN_BET_IMAGE, ASSET_NEAREST_PIN_BET_IMAGE + 1},
    {ASSET_STROKES_BET_IMAGE, ASSET_STROKES_BET_IMAGE + 1},
    {ASSET_NASSAU_BET_IMAGE, ASSET_NASSAU_BET_IMAGE + 1},
    {ASSET_SKINS_BET_IMAGE, ASSET_SKINS_BET_IMAGE + 1},
    {ASSET_MATCH_POT_BET_IMAGE, ASSET_MATCH_POT_BET_IMAGE + 1},
    {ASSET_LAS_VEGAS_BET_IMAGE, ASSET_LAS_VEGAS_BET_IMAGE + 1},
};
ResultCharactersState g_ResultCharactersLoadStep = RESULT_CHARACTERS_IDLE;

void OpenScorecard(CourseIndex courseIndex, s32 holeIndex) {
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
    void *stackPointer = GetStackPointer();

    g_Scorecard.content.courseIndex = courseIndex;
    g_Scorecard.content.holeIndex = holeIndex;
    g_Scorecard.content.isSuddenDeath = g_GameState.isSuddenDeath;
    osSyncPrintf("SP Address = %x\n", stackPointer);
    g_Scorecard.tableImage = AllocMemory(OpenEncodedAsset(ASSET_TABLE_IMAGE, &asset));
    ReadEncodedAsset(g_Scorecard.tableImage, &asset);
    g_Scorecard.raisedPanelImage = AllocMemory(OpenEncodedAsset(ASSET_RAISED_PANEL_IMAGE, &asset));
    ReadEncodedAsset(g_Scorecard.raisedPanelImage, &asset);
    g_Scorecard.scoresPanelImage = AllocMemory(OpenEncodedAsset(ASSET_SCORES_PANEL_IMAGE, &asset));
    ReadEncodedAsset(g_Scorecard.scoresPanelImage, &asset);
    g_Scorecard.digitsImage = AllocMemory(OpenEncodedAsset(ASSET_LARGE_DIGITS, &asset));
    ReadEncodedAsset(g_Scorecard.digitsImage, &asset);
    g_Scorecard.betSubtitlesImage =
        AllocMemory(OpenEncodedAsset(ASSET_BET_SUBTITLES_IMAGE, &asset));
    ReadEncodedAsset(g_Scorecard.betSubtitlesImage, &asset);
    g_Scorecard.labelsImage = AllocMemory(OpenEncodedAsset(ASSET_SCORE_LABELS, &asset));
    ReadEncodedAsset(g_Scorecard.labelsImage, &asset);
    g_Scorecard.totalLabelsImage = AllocMemory(OpenEncodedAsset(ASSET_TOTAL_LABELS_IMAGE, &asset));
    ReadEncodedAsset(g_Scorecard.totalLabelsImage, &asset);

    g_Scorecard.hasBets = FALSE;
    for (i = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] > 0) {
            g_Scorecard.hasBets = TRUE;
            break;
        }
    }

    for (i = 0; i != g_GameState.playerCount; i++) {
        g_Scorecard.faceImages[i] = AllocMemory(
            OpenEncodedAsset(g_CharacterFirstFaceAssets[g_Players[i].characterIndex], &asset)
        );
        ReadEncodedAsset(g_Scorecard.faceImages[i], &asset);
        OpenAssetFile(
            g_CharacterFirstPaletteFiles[g_Players[i].characterIndex] + g_Players[i].paletteIndex,
            &file
        );
        size = GetAssetFileSize(&file);
        g_Scorecard.facePalettes[i] = AllocMemory(size);
        ReadAssetFile(g_Scorecard.facePalettes[i], size, &file);
    }

    g_Scorecard.isOddFaceStep = FALSE;
    g_Scorecard.faceFrameTime = 0;
    g_Scorecard.faceFrame = 0;

    switch (g_GameState.gameMode) {
        case GAME_MODE_SPEED_GOLF:
            g_Scorecard.pageCount = 2;
            pageTitlesRow = PAGE_TITLES_SPEED_GOLF;
            break;
        case GAME_MODE_STROKE:
        case GAME_MODE_UNLISTED_STROKE:
        case GAME_MODE_TRAINING:
        case GAME_MODE_UNRECORDED_STROKE:
        case GAME_MODE_RECORD_REPLAY:
        case GAME_MODE_RING_SHOT:
            if (g_GameState.playerCount == 1) {
                pageTitlesRow = PAGE_TITLES_SOLO;
                if (g_Scorecard.hasBets == TRUE) {
                    g_Scorecard.pageCount = 2;
                }
                else {
                    g_Scorecard.pageCount = 1;
                }
            }
            else {
                pageTitlesRow = PAGE_TITLES_VERSUS;
                if (g_Scorecard.hasBets == TRUE) {
                    g_Scorecard.pageCount = 3;
                }
                else {
                    g_Scorecard.pageCount = 2;
                }
            }

            g_Scorecard.placesImage = AllocMemory(OpenEncodedAsset(ASSET_PLACES, &asset));
            ReadEncodedAsset(g_Scorecard.placesImage, &asset);
            break;
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_GET_CHARACTER:
            pageTitlesRow = PAGE_TITLES_VERSUS;
            if (g_Scorecard.hasBets == TRUE) {
                g_Scorecard.pageCount = 3;
            }
            else {
                g_Scorecard.pageCount = 2;
            }

            g_Scorecard.matchPanelImage =
                AllocMemory(OpenEncodedAsset(ASSET_MATCH_PANEL_IMAGE, &asset));
            ReadEncodedAsset(g_Scorecard.matchPanelImage, &asset);
            g_Scorecard.winMarkImage = AllocMemory(OpenEncodedAsset(ASSET_WIN_MARK_IMAGE, &asset));
            ReadEncodedAsset(g_Scorecard.winMarkImage, &asset);
            g_Scorecard.winsNeededImage =
                AllocMemory(OpenEncodedAsset(ASSET_WINS_NEEDED_IMAGE, &asset));
            ReadEncodedAsset(g_Scorecard.winsNeededImage, &asset);
            g_Scorecard.winnerArrowImage =
                AllocMemory(OpenEncodedAsset(ASSET_WINNER_ARROW_IMAGE, &asset));
            ReadEncodedAsset(g_Scorecard.winnerArrowImage, &asset);
            break;
        case GAME_MODE_SKINS_MATCH:
        case GAME_MODE_CLUB_SLOTS:
            pageTitlesRow = PAGE_TITLES_SKINS;
            TallySkins(g_Scorecard.content.holeIndex);
            if (g_Scorecard.hasBets == TRUE) {
                g_Scorecard.pageCount = 3;
            }
            else {
                g_Scorecard.pageCount = 2;
            }

            g_Scorecard.placesImage = AllocMemory(OpenEncodedAsset(ASSET_PLACES, &asset));
            ReadEncodedAsset(g_Scorecard.placesImage, &asset);
            g_Scorecard.winMarkImage = AllocMemory(OpenEncodedAsset(ASSET_SKIN_ICON_IMAGE, &asset));
            ReadEncodedAsset(g_Scorecard.winMarkImage, &asset);
            break;
        case GAME_MODE_TOURNAMENT:
            shownHoleCount = g_Scorecard.content.holeIndex;
            g_Scorecard.pageCount = 2;
            g_Scorecard.flapNameImages =
                AllocMemory(OpenEncodedAsset(ASSET_FLAP_NAME_IMAGES, &asset));
            ReadEncodedAsset(g_Scorecard.flapNameImages, &asset);
            g_Scorecard.leaderboardRowImage =
                AllocMemory(OpenEncodedAsset(ASSET_LEADERBOARD_ROW_IMAGE, &asset));
            ReadEncodedAsset(g_Scorecard.leaderboardRowImage, &asset);
            g_Scorecard.placeArrowImage =
                AllocMemory(OpenEncodedAsset(ASSET_PLACE_ARROW_IMAGE, &asset));
            ReadEncodedAsset(g_Scorecard.placeArrowImage, &asset);
            shownHoleCount += g_Players[0].holeStatus != HOLE_STATUS_PLAYING;
            pageTitlesRow = PAGE_TITLES_TOURNAMENT;
            if (shownHoleCount > 0) {
                if (g_Scorecard.content.holeIndex == 1) {
                    RankTournament(shownHoleCount - 1, 0, 0, g_GameState.roundSeed);
                }
                else {
                    RankTournament(
                        shownHoleCount - 1,
                        0,
                        GetNetScoreToPar(g_Scorecard.content.holeIndex - 1, 0),
                        g_GameState.roundSeed
                    );
                }

                oldPlace = -1;
                for (standingIndex = 0; standingIndex != TOURNAMENT_CONTESTANT_COUNT;
                    standingIndex++) {
                    TournamentStanding *oldStanding = &g_TournamentStandings[standingIndex];

                    if (oldStanding->contestantId == PLAYER_CONTESTANT) {
                        oldPlace = oldStanding->place & PLACE_MASK;
                        break;
                    }
                }

                RankTournament(
                    shownHoleCount,
                    0,
                    GetNetScoreToPar(g_Scorecard.content.holeIndex, 0),
                    g_GameState.roundSeed
                );
                for (standingIndex = 0; standingIndex != TOURNAMENT_CONTESTANT_COUNT;
                    standingIndex++) {
                    standing = &g_TournamentStandings[standingIndex];
                    if (standing->contestantId == PLAYER_CONTESTANT) {
                        newPlace = standing->place & PLACE_MASK;
                        if (oldPlace < newPlace) {
                            g_Scorecard.placeChange = PLACE_FELL;
                        }
                        else if (newPlace < oldPlace) {
                            g_Scorecard.placeChange = PLACE_ROSE;
                        }
                        else {
                            g_Scorecard.placeChange = PLACE_HELD;
                        }

                        break;
                    }
                }
            }
            else {
                RankTournament(
                    shownHoleCount,
                    0,
                    GetNetScoreToPar(g_Scorecard.content.holeIndex, 0),
                    g_GameState.roundSeed
                );
                g_Scorecard.placeChange = PLACE_HELD;
            }

            g_Scorecard.topRow = 0;
            break;
    }

    for (i = 0; i != PAGE_TITLE_COUNT; i++) {
        g_Scorecard.pageTitleImages[i] =
            AllocMemory(OpenEncodedAsset(f_PageTitleAssets[pageTitlesRow][i], &asset));
        ReadEncodedAsset(g_Scorecard.pageTitleImages[i], &asset);
    }

    imageIndex = 0;
    for (i = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] != UNSET_BET_RATE) {
            g_Scorecard.betImages[imageIndex][0] =
                AllocMemory(OpenEncodedAsset(f_BetImageAssets[i][0], &asset));
            ReadEncodedAsset(g_Scorecard.betImages[imageIndex][0], &asset);
            g_Scorecard.betImages[imageIndex][1] =
                AllocMemory(OpenEncodedAsset(f_BetImageAssets[i][1], &asset));
            ReadEncodedAsset(g_Scorecard.betImages[imageIndex][1], &asset);
            imageIndex++;
        }
    }

    g_Scorecard.totalHeadingImage =
        AllocMemory(OpenEncodedAsset(ASSET_TOTAL_HEADING_IMAGE, &asset));
    ReadEncodedAsset(g_Scorecard.totalHeadingImage, &asset);
    g_Scorecard.gridImage = AllocMemory(OpenEncodedAsset(ASSET_GRID_IMAGE, &asset));
    ReadEncodedAsset(g_Scorecard.gridImage, &asset);
    SetScorecardState(SCORECARD_OPENING);
    ForceStopRumble();
}

#define BytesPastTableImage(member) \
    (MemberOffset(Scorecard, member) - MemberOffset(Scorecard, tableImage))

void FreeScorecard(void) {
    s32 i;
    s32 imageIndex;
    ImageAsset **image;
    ImageAsset **tableImage;

    if (g_ScorecardState != SCORECARD_CLOSED) {
        g_ScorecardState = SCORECARD_CLOSED;
        g_RoundHud.scorecardPage = NO_SCORECARD_PAGE;
        g_RoundHud.unusedScorecardValue = 0;
        g_RoundHud.otherUnusedScorecardValue = 0;

        imageIndex = 0;
        for (i = 0; i != BET_COUNT; i++) {
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
        switch (g_GameState.gameMode) {
            case GAME_MODE_STROKE:
            case GAME_MODE_UNLISTED_STROKE:
            case GAME_MODE_TRAINING:
            case GAME_MODE_UNRECORDED_STROKE:
            case GAME_MODE_RECORD_REPLAY:
            case GAME_MODE_RING_SHOT:
                FreeMemory(&g_Scorecard.placesImage);
                break;
            case GAME_MODE_MATCH_GAME:
            case GAME_MODE_GET_CHARACTER:
                FreeMemory(&g_Scorecard.matchPanelImage);
                FreeMemory(&g_Scorecard.winMarkImage);
                FreeMemory(&g_Scorecard.winsNeededImage);
                FreeMemory(&g_Scorecard.winnerArrowImage);
                break;
            case GAME_MODE_SKINS_MATCH:
            case GAME_MODE_CLUB_SLOTS:
                FreeMemory(&g_Scorecard.placesImage);
                FreeMemory(&g_Scorecard.winMarkImage);
                break;
            case GAME_MODE_TOURNAMENT:
                FreeMemory(&g_Scorecard.placeArrowImage);
                FreeMemory(&g_Scorecard.flapNameImages);
                FreeMemory(&g_Scorecard.leaderboardRowImage);
                for (i = 0; i != GLYPH_GROUP_COUNT; i++) {
                    PlaceGlyphGroup(i, NO_FONT_SIZE, 0, 0, 0, g_PrintBuffer);
                }

                break;
        }

        image = &g_Scorecard.totalLabelsImage;
        FreeMemory(image);
        FreeMemory(&g_Scorecard.labelsImage);
        FreeMemory(&g_Scorecard.digitsImage);
        FreeMemory(&g_Scorecard.betSubtitlesImage);
        FreeMemory(&g_Scorecard.scoresPanelImage);
        FreeMemory(&g_Scorecard.raisedPanelImage);
        tableImage = &g_Scorecard.tableImage;
        image = tableImage;
        FreeMemory(image);
        for (i = 0; i != g_GameState.playerCount; i++) {
            ImageAsset **faceImages =
                (ImageAsset **)AddressPast(tableImage, BytesPastTableImage(faceImages));
            PaletteColor **facePalettes =
                (PaletteColor **)AddressPast(tableImage, BytesPastTableImage(facePalettes));

            FreeMemory(&faceImages[i]);
            FreeMemory(&facePalettes[i]);
        }

        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED, 0, 0, 0, 0);
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 1, TEXT_FLAG_TYPED, 0, 0, 0, 0);
    }

    ForceStopRumble();
}

#define SHOWN_MINUTE_LIMIT 60

void StartScorecardTally(s32 layout) {
    s32 ranks[PLAYER_COUNT_MAX];
    s32 scoresToPar[PLAYER_COUNT_MAX];
    s32 holeIndex;
    SongId songId;
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
            if (g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
                if (g_Players[0].holes[holeIndex].strokeCount <
                    g_Players[1].holes[holeIndex].strokeCount) {
                    songId = SONG_HOLE_WON;
                }
                else if (g_Players[1].holes[holeIndex].strokeCount <
                    g_Players[0].holes[holeIndex].strokeCount) {
                    songId = SONG_HOLE_LOST;
                }
                else {
                    goto HALVED;
                }
            }
            else {
                songId = SONG_HOLE_WON;
                if (g_Players[0].holes[holeIndex].strokeCount ==
                    g_Players[1].holes[holeIndex].strokeCount) {
HALVED:

                    songId = SONG_HOLE_HALVED;
                }
            }

            PlaySong(songId, 0);
            if (g_Players[0].holes[holeIndex].strokeCount ==
                g_Players[1].holes[holeIndex].strokeCount) {
                g_Scorecard.holeWinnerIndex = HOLE_WINNER_HALVED;
            }
            else if (g_Players[0].holes[holeIndex].strokeCount <
                g_Players[1].holes[holeIndex].strokeCount) {
                g_Scorecard.holeWinnerIndex = 0;
            }
            else {
                g_Scorecard.holeWinnerIndex = 1;
            }

            g_Scorecard.skinsCarryover.onLastHole = 0;
            g_Scorecard.skinsCarryover.intoLastHole = g_Scorecard.content.holeIndex - 1;
            g_Scorecard.skinsCarryover.fromLastHole = g_Scorecard.skinsCarryover.intoLastHole;
            break;
        case SCORECARD_LAYOUT_SKINS:
            g_Scorecard.holeWinnerIndex = HOLE_WINNER_HALVED;
            for (i = 0; i != g_GameState.playerCount; i++) {
                if (g_Players[i].holes[g_Scorecard.content.holeIndex - 1].strokeCount != 0) {
                    g_Scorecard.holeWinnerIndex = i;
                }
            }

            songId = SONG_HOLE_WON;
            if (g_Scorecard.holeWinnerIndex == HOLE_WINNER_HALVED) {
                songId = SONG_HOLE_HALVED;
            }

            PlaySong(songId, 0);
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
                g_GameState.stopwatchFrameCount = STOPWATCH_FRAMES_MAX;
            }

            for (i = 0, timedHoleCount = 0; i != COURSE_HOLE_COUNT; i++) {
                timedHoleCount += g_HoleTimeFrames[i] != 0;
            }

            if (timedHoleCount > 0) {
                g_HoleTimeFrames[timedHoleCount - 1] = g_GameState.stopwatchFrameCount;
                g_HoleTimeMinutes[timedHoleCount - 1] =
                    g_GameState.stopwatchFrameCount / FRAMES_PER_MINUTE;
                g_HoleTimeSeconds[timedHoleCount - 1] =
                    g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND % SECONDS_PER_MINUTE;
                g_HoleTimeHundredths[timedHoleCount - 1] =
                    (s32)(g_GameState.stopwatchFrameCount *
                        ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
                    HUNDREDTHS_PER_SECOND;
                if (timedHoleCount == 1 && g_HoleTimeMinutes[0] >= SHOWN_MINUTE_LIMIT) {
                    g_HoleTimeMinutes[0] = -1;
                    g_HoleTimeSeconds[0] = -1;
                    g_HoleTimeHundredths[0] = -1;
                }

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
                        g_HoleTimeSeconds[timedHoleCount - 1] +=
                            SECONDS_PER_MINUTE - g_HoleTimeSeconds[i];
                    }
                    else {
                        g_HoleTimeSeconds[timedHoleCount - 1] -= g_HoleTimeSeconds[i];
                    }

                    if (g_HoleTimeHundredths[timedHoleCount - 1] < g_HoleTimeHundredths[i]) {
                        if (g_HoleTimeSeconds[timedHoleCount - 1] > 0) {
                            g_HoleTimeSeconds[timedHoleCount - 1]--;
                        }
                        else {
                            g_HoleTimeMinutes[timedHoleCount - 1]--;
                            g_HoleTimeSeconds[timedHoleCount - 1] = SECONDS_PER_MINUTE - 1;
                        }

                        g_HoleTimeHundredths[timedHoleCount - 1] +=
                            HUNDREDTHS_PER_SECOND - g_HoleTimeHundredths[i];
                    }
                    else {
                        g_HoleTimeHundredths[timedHoleCount - 1] -= g_HoleTimeHundredths[i];
                    }
                }
            }

            break;
    }

    g_RoundHud.scorecardPage = 0;
    switch (g_GameState.gameMode) {
        case GAME_MODE_UNLISTED_STROKE:
        case GAME_MODE_TRAINING:
        case GAME_MODE_TOURNAMENT:
        case GAME_MODE_UNRECORDED_STROKE:
        case GAME_MODE_SPEED_GOLF:
        case GAME_MODE_RECORD_REPLAY:
        case GAME_MODE_RING_SHOT:
            for (i = 0; i != g_GameState.playerCount; i++) {
                g_Scorecard.content.faceImageOffsets[i] = 0;
            }

            break;
        case GAME_MODE_STROKE:
            for (i = 0; i < g_GameState.playerCount; i++) {
                scoresToPar[i] = GetNetScoreToPar(g_Scorecard.content.holeIndex, i);
            }

            for (i = 0; i != g_GameState.playerCount; i++) {
                ranks[i] = 0;
                for (j = 0; j != g_GameState.playerCount; j++) {
                    if (i != j && scoresToPar[i] > scoresToPar[j]) {
                        ranks[i]++;
                    }
                }
            }

            for (i = 0; i != g_GameState.playerCount; i++) {
                CourseRecord *course = &g_CourseRecords[GetCourseRecordIndex()];

                if (g_Players[i].holes[g_Scorecard.content.holeIndex - 1].strokeCount -
                        course->holes[g_Scorecard.content.holeIndex - 1].par +
                        GetHandicapOffset(
                            g_GameState.courseIndex,
                            g_Scorecard.content.holeIndex - 1,
                            g_Players[i].handicap
                        ) <=
                    0) {
                    g_Scorecard.content.faceImageOffsets[i] = 0;
                }
                else {
                    g_Scorecard.content.faceImageOffsets[i] = BEHIND_FACE_IMAGE_OFFSET;
                }
            }

            break;
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_GET_CHARACTER:
            if (g_Players[0].holes[g_Scorecard.content.holeIndex - 1].strokeCount <
                g_Players[1].holes[g_Scorecard.content.holeIndex - 1].strokeCount) {
                g_Scorecard.content.faceImageOffsets[0] = 0;
                g_Scorecard.content.faceImageOffsets[1] = BEHIND_FACE_IMAGE_OFFSET;
            }
            else if (g_Players[1].holes[g_Scorecard.content.holeIndex - 1].strokeCount <
                g_Players[0].holes[g_Scorecard.content.holeIndex - 1].strokeCount) {
                g_Scorecard.content.faceImageOffsets[0] = BEHIND_FACE_IMAGE_OFFSET;
                g_Scorecard.content.faceImageOffsets[1] = 0;
            }
            else {
                g_Scorecard.content.faceImageOffsets[0] = 0;
                g_Scorecard.content.faceImageOffsets[1] = 0;
            }

            break;
        case GAME_MODE_SKINS_MATCH:
        case GAME_MODE_CLUB_SLOTS:
            for (i = 0; i != g_GameState.playerCount; i++) {
                g_Scorecard.content.faceImageOffsets[i] = 0;
                for (j = 0; j != g_GameState.playerCount; j++) {
                    if (g_Players[i].holes[g_Scorecard.content.holeIndex - 1].strokeCount <
                        g_Players[j].holes[g_Scorecard.content.holeIndex - 1].strokeCount) {
                        g_Scorecard.content.faceImageOffsets[i] = BEHIND_FACE_IMAGE_OFFSET;
                    }
                }
            }

            break;
    }
}

void SetScorecardLayout(ScorecardLayout layout) {
    s32 ranks[PLAYER_COUNT_MAX];
    s32 scoresToPar[PLAYER_COUNT_MAX];
    s32 holesWon[MATCH_PLAYER_COUNT];
    s32 skinsRanks[PLAYER_COUNT_MAX];
    s32 unused[4] UNUSED;
    s32 i;
    s32 j;

    g_Scorecard.isTallied = TRUE;
    g_Scorecard.layout = layout;
    switch (layout) {
        case SCORECARD_LAYOUT_STROKE:
        case SCORECARD_LAYOUT_SPEED_GOLF:
            break;
        case SCORECARD_LAYOUT_MATCH:
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
    switch (g_GameState.gameMode) {
        case GAME_MODE_UNLISTED_STROKE:
        case GAME_MODE_TRAINING:
        case GAME_MODE_TOURNAMENT:
        case GAME_MODE_UNRECORDED_STROKE:
        case GAME_MODE_SPEED_GOLF:
        case GAME_MODE_RECORD_REPLAY:
        case GAME_MODE_RING_SHOT:
            for (i = 0; i != g_GameState.playerCount; i++) {
                g_Scorecard.content.faceImageOffsets[i] = 0;
            }

            break;
        case GAME_MODE_STROKE:
            for (i = 0; i < g_GameState.playerCount; i++) {
                scoresToPar[i] = GetNetScoreToPar(g_Scorecard.content.holeIndex, i);
            }

            for (i = 0; i != g_GameState.playerCount; i++) {
                ranks[i] = 0;
                for (j = 0; j != g_GameState.playerCount; j++) {
                    if (i != j && scoresToPar[i] > scoresToPar[j]) {
                        ranks[i]++;
                    }
                }
            }

            for (i = 0; i != g_GameState.playerCount; i++) {
                if (ranks[i] < g_GameState.playerCount + 1) {
                    g_Scorecard.content.faceImageOffsets[i] = 0;
                }
                else {
                    g_Scorecard.content.faceImageOffsets[i] = BEHIND_FACE_IMAGE_OFFSET;
                }
            }

            break;
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_GET_CHARACTER:
            holesWon[0] = 0;
            holesWon[1] = 0;
            for (i = GetFirstHoleIndex(); i != g_Scorecard.content.holeIndex; i++) {
                if (g_Players[0].holes[i].strokeCount < g_Players[1].holes[i].strokeCount) {
                    holesWon[0]++;
                }
                else if (g_Players[1].holes[i].strokeCount < g_Players[0].holes[i].strokeCount) {
                    holesWon[1]++;
                }
            }

            if (holesWon[0] > holesWon[1]) {
                g_Scorecard.content.faceImageOffsets[0] = 0;
                g_Scorecard.content.faceImageOffsets[1] = BEHIND_FACE_IMAGE_OFFSET;
            }
            else if (holesWon[0] < holesWon[1]) {
                g_Scorecard.content.faceImageOffsets[0] = BEHIND_FACE_IMAGE_OFFSET;
                g_Scorecard.content.faceImageOffsets[1] = 0;
            }
            else {
                g_Scorecard.content.faceImageOffsets[0] = 0;
                g_Scorecard.content.faceImageOffsets[1] = 0;
            }

            break;
        case GAME_MODE_SKINS_MATCH:
        case GAME_MODE_CLUB_SLOTS:
            for (i = 0; i != g_GameState.playerCount; i++) {
                skinsRanks[i] = 0;
                for (j = 0; j != g_GameState.playerCount; j++) {
                    if (i != j && g_RoundHud.skinsTotals[i] > g_RoundHud.skinsTotals[j]) {
                        skinsRanks[i]++;
                    }
                }
            }

            for (i = 0; i != g_GameState.playerCount; i++) {
                if (skinsRanks[i] < g_GameState.playerCount + 1) {
                    g_Scorecard.content.faceImageOffsets[i] = 0;
                }
                else {
                    g_Scorecard.content.faceImageOffsets[i] = BEHIND_FACE_IMAGE_OFFSET;
                }
            }

            break;
    }
}

void SetScorecardState(ScorecardState state) {
    if (state == SCORECARD_OPENING) {
        PlaySoundWithEffects(
            SOUND_PANEL_OPEN,
            g_NextSoundChannel,
            SCORECARD_SOUND_VOLUME,
            SCORECARD_SLIDE_SOUND_REVERB,
            SCORECARD_OPEN_SOUND_PAN
        );
    }
    else if (state == SCORECARD_CLOSING) {
        PlaySoundWithEffects(
            SOUND_PANEL_CLOSE,
            g_NextSoundChannel,
            SCORECARD_SOUND_VOLUME,
            SCORECARD_SLIDE_SOUND_REVERB,
            SCORECARD_CLOSE_SOUND_PAN
        );
    }

    if (state == SCORECARD_OPENING) {
        g_Scorecard.x = SCREEN_WIDTH;
        g_Scorecard.y = 0;
    }

    g_ScorecardState = state;
    g_Scorecard.stateTime = 0;
}

Boolean TallyScorecardSkins(void);

void AnimateScorecard(void) {
    EncodedAsset asset;
    s32 playerIndex;
    s32 slideSpeed;

    switch (g_ScorecardState) {
        case SCORECARD_TALLYING:
            if (TallyScorecardSkins()) {
                g_ScorecardState = SCORECARD_SHOWN;
            }

            g_Scorecard.stateTime++;
            break;
        case SCORECARD_OPENING:
            g_Scorecard.stateTime++;
            if (g_Scorecard.stateTime == SCORECARD_OPEN_TIME) {
                g_Scorecard.stateTime = 0;
                if (g_Scorecard.isTallied) {
                    g_ScorecardState = SCORECARD_SHOWN;
                }
                else {
                    g_ScorecardState = SCORECARD_TALLYING;
                }
            }

            if (g_Scorecard.x > 0) {
                if (g_Scorecard.x < 2) {
                    g_Scorecard.x = 0;
                }

                g_Scorecard.x -= g_Scorecard.x / 2;
            }
            else if (g_Scorecard.x < 0) {
                g_Scorecard.x += SCORECARD_SLIDE_SPEED;
            }

            if (g_Scorecard.y > 0) {
                g_Scorecard.y -= SCORECARD_SLIDE_SPEED;
                if (g_Scorecard.y < 0) {
                    g_Scorecard.y = 0;
                }
            }
            else if (g_Scorecard.y < 0) {
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

            g_Scorecard.y = g_Scorecard.y;
            if (g_Scorecard.stateTime == SCORECARD_CLOSE_TIME) {
                FreeScorecard();
                g_PauseScreen = PAUSE_SCREEN_NONE;
                SetMenuBackdrop(MENU_BACKDROP_NONE);
                SetFramebufferCount(TRIPLE_BUFFERING);
                SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                g_RoundHud.isLetterboxShown = TRUE;
                g_RoundHud.isClubPanelShown = TRUE;
                g_RoundHud.isWindIndicatorShown = TRUE;
                g_RoundHud.isIgnoredPausePartShown = FALSE;
                g_RoundHud.isHoleInfoShown = TRUE;
                g_RoundHud.isLieIconShown = TRUE;
                g_RoundHud.isClubHeadIconShown = TRUE;
                g_RoundHud.isUnreadItemShown = TRUE;
                g_RoundHud.isProgressShown = TRUE;
            }

            break;
        case SCORECARD_SHOWN:
            if (g_Scorecard.faceFrameTime++ % FACE_FRAME_DURATION == 1) {
                g_Scorecard.faceFrame++;
                g_Scorecard.isOddFaceStep ^= TRUE;
                if (g_Scorecard.faceFrame < FACE_FRAME_COUNT) {
                    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
                        OpenEncodedAsset(
                            g_CharacterFirstFaceAssets[g_Players[playerIndex].characterIndex] +
                                g_Scorecard.faceFrame +
                                g_Scorecard.content.faceImageOffsets[playerIndex],
                            &asset
                        );
                        ReadEncodedAsset(g_Scorecard.faceImages[playerIndex], &asset);
                    }
                }
                else {
                    g_Scorecard.faceFrame = FACE_FRAME_COUNT - 1;
                }
            }

            break;
    }
}

void TurnScorecardPage(u16 buttons) {
    RoundHud *hud;
    s32 pageCount = g_Scorecard.pageCount;
    SoundChannel *channel = &g_NextSoundChannel;

    if (pageCount > 1) {
        if (buttons & L_JPAD) {
            hud = &g_RoundHud;
            hud->scorecardPage--;
            if (hud->scorecardPage < 0) {
                hud->scorecardPage = pageCount - 1;
            }
        }
        else if (buttons & R_JPAD) {
            hud = &g_RoundHud;
            hud->scorecardPage++;
            if (hud->scorecardPage > pageCount - 1) {
                hud->scorecardPage = 0;
            }
        }
        else {
            return;
        }

        PlaySoundWithEffects(
            SOUND_MENU_MOVE,
            *channel,
            SCORECARD_SOUND_VOLUME,
            NO_REVERB,
            (hud->scorecardPage + 1) * (SOUND_PAN_RIGHT / (pageCount + 1))
        );
    }
}

void ScrollLeaderboard(u16 buttons) {
    s32 row;

    if (g_Scorecard.content.flipTime < ROW_FLIP_TIME) {
        g_Scorecard.content.flipTime++;
    }

    for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
        if (row * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime &&
            g_Scorecard.content.isRowFlipping[row] == TRUE) {
            g_Scorecard.content.rowFlipAngles[row] += DegreesToRadians(ROW_FLIP_DEGREES_PER_FRAME);
            if (g_Scorecard.content.rowFlipAngles[row] > DegreesToRadians(ROW_FLIP_END_DEGREES)) {
                g_Scorecard.content.rowFlipAngles[row] = 0.0f;
                g_Scorecard.content.isRowFlipping[row] = FALSE;
                PlaySound(SOUND_LOOK_DOWN_RANGE, ROW_FLIP_SOUND_CHANNEL, SCORECARD_SOUND_VOLUME);
            }
        }
    }

    if (g_RoundHud.scorecardPage == 0) {
        if (buttons & U_JPAD) {
            if (g_Scorecard.topRow > 0) {
                g_Scorecard.topRow--;
                PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SCORECARD_SOUND_VOLUME);
            }
        }

        if ((buttons & D_JPAD) && g_Scorecard.topRow < LEADERBOARD_LAST_TOP_ROW) {
            g_Scorecard.topRow++;
            PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SCORECARD_SOUND_VOLUME);
        }
    }
}

void UpdateScorecard(u16 buttons, u16 pressedButtons) {
    AnimateScorecard();
    if (g_ScorecardState == SCORECARD_SHOWN) {
        if (pressedButtons & (A_BUTTON | B_BUTTON)) {
            SetScorecardState(SCORECARD_CLOSING);
            return;
        }

        switch (g_GameState.gameMode) {
            case GAME_MODE_STROKE:
            case GAME_MODE_UNLISTED_STROKE:
            case GAME_MODE_MATCH_GAME:
            case GAME_MODE_TRAINING:
            case GAME_MODE_GET_CHARACTER:
            case GAME_MODE_UNRECORDED_STROKE:
            case GAME_MODE_SPEED_GOLF:
            case GAME_MODE_RECORD_REPLAY:
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

Boolean UpdateShownScorecard(u16 buttons) {
    AnimateScorecard();
    if (g_ScorecardState != SCORECARD_SHOWN) {
        return FALSE;
    }

    switch (g_GameState.gameMode) {
        case GAME_MODE_STROKE:
        case GAME_MODE_UNLISTED_STROKE:
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_TRAINING:
        case GAME_MODE_GET_CHARACTER:
        case GAME_MODE_UNRECORDED_STROKE:
        case GAME_MODE_SPEED_GOLF:
        case GAME_MODE_RECORD_REPLAY:
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

void DrawLargeImage(Gfx **displayListHead, u8 *image, s32 left, s32 top) {
    Gfx *gfx = *displayListHead;
    s32 width = ReadImageWidth(image);
    s32 height = ReadImageHeight(image);
    s32 stripCount;
    s32 stripIndex;

    stripCount = 1;
    if (width > UNSPLIT_IMAGE_WIDTH_MAX) {
        stripCount = 2;
    }

    LoadLargePalette(&gfx, ImagePalette(image));
    height /= stripCount;
    for (stripIndex = 0; stripIndex != stripCount; stripIndex++) {
        LoadTextureBlock(
            &gfx,
            LargeImageTexels(image) + (stripIndex * width * height),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            width,
            height,
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
            QuarterPixels(left),
            QuarterPixels(top),
            QuarterPixels(left + width),
            QuarterPixels(top + height),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        top += height;
    }

    *displayListHead = gfx;
}

#define PAGE_TITLE_LEFT 32
#define PAGE_TITLE_SPACING 136
#define THREE_PAGE_TITLE_LEFT 29
#define THREE_PAGE_TITLE_SPACING 104
#define PAGE_TITLE_TOP 21
#define UNSHOWN_PAGE_GRAY 0x7F

void DrawScorecard(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 pageIndex;
    s32 dropY;

    if (g_ScorecardState == SCORECARD_UNUSED_MATCH_RESULT) {
        DrawMatchResult(&gfx);
    }
    else if (g_ScorecardState != SCORECARD_UNUSED_HIDDEN) {
        if (g_ScorecardState != SCORECARD_CLOSED) {
            DrawScorecardTable(&gfx, g_Scorecard.pageCount, g_RoundHud.scorecardPage);
            gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
            for (pageIndex = 0; pageIndex != g_Scorecard.pageCount; pageIndex++) {
                if (pageIndex == g_RoundHud.scorecardPage) {
                    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                    dropY = 0;
                }
                else {
                    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        UNSHOWN_PAGE_GRAY,
                        UNSHOWN_PAGE_GRAY,
                        UNSHOWN_PAGE_GRAY,
                        OPAQUE_ALPHA
                    );
                    dropY = 1;
                }

                gDPPipeSync(gfx++);
                if (g_Scorecard.pageCount == THREE_PAGE_COUNT) {
                    DrawLargeImage(
                        &gfx,
                        g_Scorecard.pageTitleImages[pageIndex],
                        ((pageIndex * THREE_PAGE_TITLE_SPACING) + THREE_PAGE_TITLE_LEFT) +
                            g_Scorecard.x,
                        g_Scorecard.y + PAGE_TITLE_TOP + dropY
                    );
                }
                else {
                    DrawLargeImage(
                        &gfx,
                        g_Scorecard.pageTitleImages[pageIndex],
                        ((pageIndex * PAGE_TITLE_SPACING) + PAGE_TITLE_LEFT) + g_Scorecard.x,
                        g_Scorecard.y + PAGE_TITLE_TOP + dropY
                    );
                }
            }

            switch (g_GameState.gameMode) {
                case GAME_MODE_TOURNAMENT:
                    if (g_RoundHud.scorecardPage == 0) {
                        DrawLeaderboardPage(&gfx);
                    }
                    else {
                        DrawScoresPage(&gfx);
                    }

                    break;
                case GAME_MODE_SPEED_GOLF:
                    if (g_RoundHud.scorecardPage == 0) {
                        DrawSpeedGolfPage(&gfx);
                    }
                    else {
                        DrawScoresPage(&gfx);
                    }

                    break;
                case GAME_MODE_SKINS_MATCH:
                case GAME_MODE_CLUB_SLOTS:
                    if (g_RoundHud.scorecardPage == 0) {
                        DrawSkinsPage(&gfx);
                    }
                    else if (g_RoundHud.scorecardPage == 1) {
                        DrawScoresPage(&gfx);
                    }
                    else {
                        DrawBetsPage(&gfx);
                    }

                    break;
                case GAME_MODE_MATCH_GAME:
                case GAME_MODE_GET_CHARACTER:
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        0,
                        TEXT_FLAG_TYPED,
                        0,
                        0,
                        0,
                        0
                    );
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        1,
                        TEXT_FLAG_TYPED,
                        0,
                        0,
                        0,
                        0
                    );
                    if (g_RoundHud.scorecardPage == 0) {
                        DrawMatchPage(&gfx);
                    }
                    else if (g_RoundHud.scorecardPage == 1) {
                        DrawScoresPage(&gfx);
                    }
                    else {
                        DrawBetsPage(&gfx);
                    }

                    break;
                case GAME_MODE_STROKE:
                case GAME_MODE_UNLISTED_STROKE:
                case GAME_MODE_TRAINING:
                case GAME_MODE_UNRECORDED_STROKE:
                case GAME_MODE_RECORD_REPLAY:
                case GAME_MODE_RING_SHOT:
                    if (g_GameState.playerCount == 1) {
                        if (g_RoundHud.scorecardPage == 0) {
                            DrawScoresPage(&gfx);
                        }
                        else {
                            DrawBetsPage(&gfx);
                        }
                    }
                    else if (g_RoundHud.scorecardPage == 0) {
                        DrawStandingsPage(&gfx);
                    }
                    else if (g_RoundHud.scorecardPage == 1) {
                        DrawScoresPage(&gfx);
                    }
                    else {
                        DrawBetsPage(&gfx);
                    }

                    break;
            }
        }
    }

    *displayListHead = gfx;
}

#define TABLE_WIDTH 288
#define TABLE_SHOWN_WIDTH 284
#define TABLE_LEFT 20
#define TAB_TOP 20
#define TAB_HEIGHT 24
#define TAB_STRIP_COUNT 5
#define TAB_BACK_DROP 2
#define TABLE_BODY_TOP 44
#define TABLE_BODY_HEIGHT 190
#define TABLE_BODY_OFFSET \
    (LARGE_PALETTED_TEXELS_OFFSET + (TAB_STRIP_COUNT * TAB_HEIGHT * TABLE_WIDTH))
#define TABLE_LOAD_HEIGHT 7
#define TAB_BACK_GRAY 0xBF
#define FIRST_OF_THREE_TAB_STRIP 0
#define SECOND_OF_THREE_TAB_STRIP 1
#define THIRD_OF_THREE_TAB_STRIP 2
#define FIRST_OF_TWO_TAB_STRIP 3
#define SECOND_OF_TWO_TAB_STRIP 4

void DrawScorecardTable(Gfx **displayListHead, s32 pageCount, s32 pageIndex) {
    s32 tabStrips[THREE_PAGE_COUNT];
    Gfx *gfx = *displayListHead;
    s32 tabIndex;
    s32 row;
    s32 dropY;
    s32 texelsOffset;
    s32 loadHeight;
    s32 top;
    s32 tabTop;
    s32 tabBottom;

    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    if (pageCount == 1) {
        tabStrips[0] = SECOND_OF_TWO_TAB_STRIP;
    }
    else if (pageCount == 2) {
        if (pageIndex == 0) {
            tabStrips[0] = FIRST_OF_TWO_TAB_STRIP;
            tabStrips[1] = SECOND_OF_TWO_TAB_STRIP;
        }
        else {
            tabStrips[0] = SECOND_OF_TWO_TAB_STRIP;
            tabStrips[1] = FIRST_OF_TWO_TAB_STRIP;
        }
    }
    else if (pageIndex == 0) {
        tabStrips[0] = FIRST_OF_THREE_TAB_STRIP;
        tabStrips[1] = SECOND_OF_THREE_TAB_STRIP;
        tabStrips[2] = THIRD_OF_THREE_TAB_STRIP;
    }
    else if (pageIndex == 1) {
        tabStrips[0] = FIRST_OF_THREE_TAB_STRIP;
        tabStrips[1] = THIRD_OF_THREE_TAB_STRIP;
        tabStrips[2] = SECOND_OF_THREE_TAB_STRIP;
    }
    else {
        tabStrips[0] = THIRD_OF_THREE_TAB_STRIP;
        tabStrips[1] = SECOND_OF_THREE_TAB_STRIP;
        tabStrips[2] = FIRST_OF_THREE_TAB_STRIP;
    }

    LoadLargePalette(&gfx, ImagePalette(g_Scorecard.tableImage));
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(gfx++, 0, 0, TAB_BACK_GRAY, TAB_BACK_GRAY, TAB_BACK_GRAY, OPAQUE_ALPHA);
    for (tabIndex = 0; tabIndex != pageCount; tabIndex++) {
        texelsOffset =
            (tabStrips[tabIndex] * TAB_HEIGHT * TABLE_WIDTH) + LARGE_PALETTED_TEXELS_OFFSET;
        if (tabIndex == pageCount - 1) {
            gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
            dropY = 0;
        }
        else {
            dropY = TAB_BACK_DROP;
        }

        for (row = 0; row < TAB_HEIGHT; row += TABLE_LOAD_HEIGHT) {
            LoadTextureBlock(
                &gfx,
                g_Scorecard.tableImage + texelsOffset,
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                TABLE_WIDTH,
                TABLE_LOAD_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            tabBottom = g_Scorecard.y + TAB_TOP + TABLE_LOAD_HEIGHT;
            tabTop = g_Scorecard.y + TAB_TOP;
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(g_Scorecard.x + TABLE_LEFT),
                QuarterPixels(row + tabTop + dropY),
                QuarterPixels(g_Scorecard.x + TABLE_LEFT + TABLE_SHOWN_WIDTH),
                QuarterPixels(row + tabBottom + dropY),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            texelsOffset += TABLE_LOAD_HEIGHT * TABLE_WIDTH;
        }
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    texelsOffset = TABLE_BODY_OFFSET;
    for (row = 0; row < TABLE_BODY_HEIGHT; row += TABLE_LOAD_HEIGHT) {
        loadHeight = (row + TABLE_LOAD_HEIGHT <= TABLE_BODY_HEIGHT) ? TABLE_LOAD_HEIGHT : 1;
        gDPSetTextureImage(
            gfx++,
            G_IM_FMT_CI,
            G_IM_SIZ_16b,
            1,
            TextureAddress(g_Scorecard.tableImage + texelsOffset)
        );
        gDPLoadSync(gfx++);
        gDPLoadBlock(
            gfx++,
            G_TX_LOADTILE,
            0,
            0,
            (loadHeight * (TABLE_WIDTH / 2)) - 1,
            CALC_DXT(TABLE_WIDTH, G_IM_SIZ_8b_BYTES)
        );
        top = g_Scorecard.y + TABLE_BODY_TOP;
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + TABLE_LEFT),
            QuarterPixels(row + top),
            QuarterPixels(g_Scorecard.x + TABLE_LEFT + TABLE_SHOWN_WIDTH),
            QuarterPixels(row + loadHeight + top),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        texelsOffset += TABLE_LOAD_HEIGHT * TABLE_WIDTH;
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void FreeResultCharacters(void) {
    FreeCharacterSlot(0);
    FreeCharacterSlot(1);
    g_ResultCharactersLoadStep = RESULT_CHARACTERS_IDLE;
}

Boolean LoadResultCharacters(void) {
    s32 holeIndex = g_Scorecard.content.holeIndex - 1;
    SongId songId;

    if (IsCharacterSlotBusy(0) || IsCharacterSlotBusy(1)) {
        FreeCharacterSlot(0);
        FreeCharacterSlot(1);
        return FALSE;
    }

    LoadCharacter(0, g_Players[0].characterIndex + CHARACTER_COUNT, g_Players[0].paletteIndex);
    LoadCharacter(1, g_Players[1].characterIndex + CHARACTER_COUNT, g_Players[1].paletteIndex);
    songId = SONG_MATCH_RESULT;
    if (g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
        songId = g_Players[0].holes[holeIndex].strokeCount;
        if (songId < g_Players[1].holes[holeIndex].strokeCount) {
            songId = SONG_HOLE_WON;
        }
        else if (g_Players[1].holes[holeIndex].strokeCount <
            g_Players[0].holes[holeIndex].strokeCount) {
            songId = SONG_HOLE_LOST;
        }
        else {
            songId = SONG_HOLE_HALVED;
        }
    }

    PlaySong(songId, 0);
    if (g_Players[0].holes[holeIndex].strokeCount < g_Players[1].holes[holeIndex].strokeCount) {
        SetCharacterAnimation(0, REACTION_ANIMATION_BIRDIE);
        SetCharacterAnimation(1, REACTION_ANIMATION_BOGEY);
    }
    else if (g_Players[1].holes[holeIndex].strokeCount <
        g_Players[0].holes[holeIndex].strokeCount) {
        SetCharacterAnimation(0, REACTION_ANIMATION_BOGEY);
        SetCharacterAnimation(1, REACTION_ANIMATION_BIRDIE);
    }
    else {
        SetCharacterAnimation(0, REACTION_ANIMATION_PAR);
        SetCharacterAnimation(1, REACTION_ANIMATION_PAR);
    }

    SetAnimationSpeed(0, 1.0f);
    SetAnimationSpeed(1, 1.0f);
    return TRUE;
}

Boolean TallyScorecardSkins(void) {
    switch (g_Scorecard.layout) {
        case SCORECARD_LAYOUT_STROKE:
        case SCORECARD_LAYOUT_TOURNAMENT:
        case SCORECARD_LAYOUT_SPEED_GOLF:
            return TRUE;
        case SCORECARD_LAYOUT_MATCH:
            if (g_Scorecard.stateTime == MATCH_RESULT_SOUND_TIME &&
                g_Scorecard.holeWinnerIndex != HOLE_WINNER_HALVED) {
                PlaySound(SOUND_TALLY_DONE, TALLY_SOUND_CHANNEL, SCORECARD_SOUND_VOLUME);
            }

            if (g_Scorecard.stateTime == MATCH_RESULT_TIME) {
                g_Scorecard.skinsCarryover.onLastHole = 1;
                if (g_Scorecard.holeWinnerIndex != HOLE_WINNER_HALVED) {
                    PlaySound(
                        SOUND_THREE_STARS,
                        MATCH_RESULT_SOUND_CHANNEL,
                        SCORECARD_SOUND_VOLUME
                    );
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
            }
            else if (g_Scorecard.stateTime >= SKINS_TALLY_TIME) {
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
