/*
 * The scorecard: between holes it slides in over the course with the
 * players' scores, and with each game mode's own pages, such as the
 * tournament's leaderboard and a match's results.
 */
#include "common.h"

#include "scorecard.h"

#include "asset.h"
#include "character.h"
#include "character_slot.h"
#include "course.h"
#include "framebuffer.h"
#include "functions.h"
#include "game_flags.h"
#include "heap.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "math_util.h"
#include "play.h"
#include "reaction.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "tournament.h"
#include "variables.h"

/*
 * The tallies of a match's hole happen at these frames of the TALLYING state.
 * A skins match carries a halved hole's skins over at SKINS_TALLY_TIME, or
 * from then hands a won hole's skins over one every SKINS_TALLY_INTERVAL.
 */
#define MATCH_RESULT_SOUND_TIME 1
#define MATCH_RESULT_TIME 5
#define SKINS_TALLY_TIME 20
#define SKINS_TALLY_INTERVAL 4
#define MATCH_RESULT_SOUND_CHANNEL 14

/* Frames the scorecard takes to slide in */
#define SCORECARD_OPEN_TIME 12
/* Frames before a closing scorecard is freed */
#define SCORECARD_CLOSE_TIME 10
#define SCORECARD_SLIDE_SPEED 32
#define SCORECARD_CLOSE_ACCELERATION 8
/* The faces step through their images once every FACE_FRAME_TIME frames */
#define FACE_FRAME_COUNT 5
#define FACE_FRAME_TIME 3

#define SCORECARD_SLIDE_SOUND_REVERB 20
/* The opening sound pans from the right, the closing one from the left */
#define SCORECARD_OPEN_SOUND_PAN 0xDF
#define SCORECARD_CLOSE_SOUND_PAN 0x20
#define SOUND_PAN_FULL_RIGHT 0xFF

/* The leaderboard writes its contestants' names with these glyph groups */
#define LEADERBOARD_GLYPH_GROUP_COUNT 20

/* Images wider than this are drawn in two strips */
#define MAX_UNSPLIT_IMAGE_WIDTH 96

#define LEADERBOARD_LAST_TOP_ROW \
  (TOURNAMENT_CONTESTANT_COUNT - LEADERBOARD_ROW_COUNT)
#define ROW_FLIP_DEGREES_PER_FRAME 20
#define ROW_FLIP_END_DEGREES 175
#define ROW_FLIP_SOUND_CHANNEL 13

/* The face shown for a player behind */
#define BEHIND_FACE_IMAGE_OFFSET 4
#define THREE_PAGE_COUNT 3

void DrawScorecardTable(Gfx** displayListHead, s32 pageCount, s32 page);

void ScrollLeaderboard(u16 buttons);

void SetScorecardState(s32 state);

#define TABLE_IMAGE_ASSET 0x68F
#define PLACE_ARROW_IMAGE_ASSET 0x682
#define UNK14_IMAGE_ASSET 0x690
#define SCORES_PANEL_IMAGE_ASSET 0x691
#define BET_SUBTITLES_IMAGE_ASSET 0x97A
#define TOTAL_LABELS_IMAGE_ASSET 0x693
#define MATCH_PANEL_IMAGE_ASSET 0x68E
#define WIN_MARK_IMAGE_ASSET 0x68C
#define WINS_NEEDED_IMAGE_ASSET 0x68D
#define WINNER_ARROW_IMAGE_ASSET 0x694
#define SKIN_ICON_IMAGE_ASSET 0x68B
#define FLAP_NAME_IMAGES_ASSET 0x695
#define LEADERBOARD_ROW_IMAGE_ASSET 0x692
#define TOTAL_HEADING_IMAGE_ASSET 0x696
#define GRID_IMAGE_ASSET 0x69D
#define SCORECARD_TITLE_ASSET 0x683
#define LEADERBOARD_TITLE_ASSET 0x684
#define SPEED_GOLF_TITLE_ASSET 0x685
#define VERSUS_BOARD_TITLE_ASSET 0x686
#define BET_TITLE_ASSET 0x687
/* Each bet's first blink frame; its second is the asset after it */
#define LAS_VEGAS_IMAGE_ASSET 0x672
#define LONGEST_DRIVE_IMAGE_ASSET 0x674
#define BET_UNK5_IMAGE_ASSET 0x676
#define NEAREST_PIN_IMAGE_ASSET 0x678
#define STROKES_IMAGE_ASSET 0x67A
#define BET_UNK4_IMAGE_ASSET 0x67C
#define NASSAU_IMAGE_ASSET 0x67E

/* The rows of f_PageTitleAssets */
#define PAGE_TITLES_SPEED_GOLF 0
#define PAGE_TITLES_SOLO 1
#define PAGE_TITLES_VERSUS 2
#define PAGE_TITLES_SKINS 3
#define PAGE_TITLES_TOURNAMENT 4
#define PAGE_TITLES_ROW_COUNT 5
#define PAGE_TITLE_COUNT 3

/* Read by no routine */
static s32 D_8024C450_ovl_09[] UNUSED = {0, 1, 2, 3, 5, 10};
static AssetId f_PageTitleAssets[PAGE_TITLES_ROW_COUNT][PAGE_TITLE_COUNT] = {
    {SPEED_GOLF_TITLE_ASSET, SCORECARD_TITLE_ASSET, SCORECARD_TITLE_ASSET},
    {SCORECARD_TITLE_ASSET, BET_TITLE_ASSET, BET_TITLE_ASSET},
    {VERSUS_BOARD_TITLE_ASSET, SCORECARD_TITLE_ASSET, BET_TITLE_ASSET},
    {VERSUS_BOARD_TITLE_ASSET, SCORECARD_TITLE_ASSET, BET_TITLE_ASSET},
    {LEADERBOARD_TITLE_ASSET, SCORECARD_TITLE_ASSET, SCORECARD_TITLE_ASSET},
};
/* Each bet's two blink frames */
static AssetId f_BetImageAssets[BET_COUNT][2] = {
    {LONGEST_DRIVE_IMAGE_ASSET, LONGEST_DRIVE_IMAGE_ASSET + 1},
    {NEAREST_PIN_IMAGE_ASSET, NEAREST_PIN_IMAGE_ASSET + 1},
    {STROKES_IMAGE_ASSET, STROKES_IMAGE_ASSET + 1},
    {NASSAU_IMAGE_ASSET, NASSAU_IMAGE_ASSET + 1},
    {BET_UNK4_IMAGE_ASSET, BET_UNK4_IMAGE_ASSET + 1},
    {BET_UNK5_IMAGE_ASSET, BET_UNK5_IMAGE_ASSET + 1},
    {LAS_VEGAS_IMAGE_ASSET, LAS_VEGAS_IMAGE_ASSET + 1},
};
s32 f_ResultCharactersLoadStep = RESULT_CHARACTERS_IDLE;

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
  TournamentStanding* standing;
  register void* stackPointer asm("$sp");

  g_Scorecard.content.courseIndex = courseIndex;
  g_Scorecard.content.holeIndex = holeIndex;
  g_Scorecard.content.isSuddenDeath = g_GameState.isSuddenDeath;
  osSyncPrintf("SP Address = %x\n", stackPointer);
  g_Scorecard.tableImage =
      AllocMemory(OpenEncodedAsset(TABLE_IMAGE_ASSET, &asset));
  ReadEncodedAsset(g_Scorecard.tableImage, &asset);
  g_Scorecard.unk14 = AllocMemory(OpenEncodedAsset(UNK14_IMAGE_ASSET, &asset));
  ReadEncodedAsset(g_Scorecard.unk14, &asset);
  g_Scorecard.scoresPanelImage =
      AllocMemory(OpenEncodedAsset(SCORES_PANEL_IMAGE_ASSET, &asset));
  ReadEncodedAsset(g_Scorecard.scoresPanelImage, &asset);
  g_Scorecard.digitsImage =
      AllocMemory(OpenEncodedAsset(ASSET_LARGE_DIGITS, &asset));
  ReadEncodedAsset(g_Scorecard.digitsImage, &asset);
  g_Scorecard.betSubtitlesImage =
      AllocMemory(OpenEncodedAsset(BET_SUBTITLES_IMAGE_ASSET, &asset));
  ReadEncodedAsset(g_Scorecard.betSubtitlesImage, &asset);
  g_Scorecard.labelsImage =
      AllocMemory(OpenEncodedAsset(ASSET_SCORE_LABELS, &asset));
  ReadEncodedAsset(g_Scorecard.labelsImage, &asset);
  g_Scorecard.totalLabelsImage =
      AllocMemory(OpenEncodedAsset(TOTAL_LABELS_IMAGE_ASSET, &asset));
  ReadEncodedAsset(g_Scorecard.totalLabelsImage, &asset);

  g_Scorecard.hasBets = FALSE;
  for (i = 0; i != BET_COUNT; i++) {
    if (g_GameState.betRates[i] > 0) {
      g_Scorecard.hasBets = TRUE;
      break;
    }
  }

  for (i = 0; i != g_GameState.playerCount; i++) {
    g_Scorecard.faceImages[i] = AllocMemory(OpenEncodedAsset(
        g_CharacterImageAssets[(s8)g_Players[i].characterIndex], &asset));
    ReadEncodedAsset(g_Scorecard.faceImages[i], &asset);
    OpenAssetFile(g_CharacterPaletteAssets[(s8)g_Players[i].characterIndex] +
            (s8)g_Players[i].paletteIndex,
        &file);
    size = GetAssetFileSize(&file);
    g_Scorecard.facePalettes[i] = AllocMemory(size);
    ReadAssetFile(g_Scorecard.facePalettes[i], size, &file);
  }

  g_Scorecard.unkBC = 0;
  g_Scorecard.faceTime = 0;
  g_Scorecard.faceFrame = 0;

  switch (g_GameState.gameMode) {
    case GAME_MODE_SPEED_GOLF:
      g_Scorecard.pageCount = 2;
      pageTitlesRow = PAGE_TITLES_SPEED_GOLF;
      break;
    case GAME_MODE_STROKE:
    case GAME_MODE_UNK1:
    case GAME_MODE_TRAINING:
    case GAME_MODE_UNK6:
    case GAME_MODE_UNK8:
    case GAME_MODE_RING_SHOT:
      /* The bets get a page of their own */
      if (g_GameState.playerCount == 1) {
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

      g_Scorecard.placesImage =
          AllocMemory(OpenEncodedAsset(ASSET_PLACES, &asset));
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

      g_Scorecard.matchPanelImage =
          AllocMemory(OpenEncodedAsset(MATCH_PANEL_IMAGE_ASSET, &asset));
      ReadEncodedAsset(g_Scorecard.matchPanelImage, &asset);
      g_Scorecard.winMarkImage =
          AllocMemory(OpenEncodedAsset(WIN_MARK_IMAGE_ASSET, &asset));
      ReadEncodedAsset(g_Scorecard.winMarkImage, &asset);
      g_Scorecard.winsNeededImage =
          AllocMemory(OpenEncodedAsset(WINS_NEEDED_IMAGE_ASSET, &asset));
      ReadEncodedAsset(g_Scorecard.winsNeededImage, &asset);
      g_Scorecard.winnerArrowImage =
          AllocMemory(OpenEncodedAsset(WINNER_ARROW_IMAGE_ASSET, &asset));
      ReadEncodedAsset(g_Scorecard.winnerArrowImage, &asset);
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

      g_Scorecard.placesImage =
          AllocMemory(OpenEncodedAsset(ASSET_PLACES, &asset));
      ReadEncodedAsset(g_Scorecard.placesImage, &asset);
      g_Scorecard.winMarkImage =
          AllocMemory(OpenEncodedAsset(SKIN_ICON_IMAGE_ASSET, &asset));
      ReadEncodedAsset(g_Scorecard.winMarkImage, &asset);
      break;
    case GAME_MODE_TOURNAMENT:
      shownHoleCount = g_Scorecard.content.holeIndex;
      g_Scorecard.pageCount = 2;
      g_Scorecard.flapNameImages =
          AllocMemory(OpenEncodedAsset(FLAP_NAME_IMAGES_ASSET, &asset));
      ReadEncodedAsset(g_Scorecard.flapNameImages, &asset);
      g_Scorecard.leaderboardRowImage =
          AllocMemory(OpenEncodedAsset(LEADERBOARD_ROW_IMAGE_ASSET, &asset));
      ReadEncodedAsset(g_Scorecard.leaderboardRowImage, &asset);
      g_Scorecard.placeArrowImage =
          AllocMemory(OpenEncodedAsset(PLACE_ARROW_IMAGE_ASSET, &asset));
      ReadEncodedAsset(g_Scorecard.placeArrowImage, &asset);
      shownHoleCount += g_Players[0].holeStatus != HOLE_STATUS_PLAYING;
      pageTitlesRow = PAGE_TITLES_TOURNAMENT;
      if (shownHoleCount > 0) {
        if (g_Scorecard.content.holeIndex == 1) {
          RankTournament(shownHoleCount - 1, 0, 0, g_GameState.unk2C);
        } else {
          RankTournament(shownHoleCount - 1, 0,
              GetNetScoreToPar(g_Scorecard.content.holeIndex - 1, 0),
              g_GameState.unk2C);
        }

        oldPlace = -1;
        for (standingIndex = 0, standing = g_TournamentStandings;
            standingIndex != TOURNAMENT_CONTESTANT_COUNT;
            standingIndex++, standing++) {
          if (standing->contestantId == PLAYER_CONTESTANT) {
            oldPlace = standing->place & PLACE_MASK;
            break;
          }
        }

        RankTournament(shownHoleCount, 0,
            GetNetScoreToPar(g_Scorecard.content.holeIndex, 0),
            g_GameState.unk2C);
        for (standingIndex = 0, standing = g_TournamentStandings;
            standingIndex != TOURNAMENT_CONTESTANT_COUNT;
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
        RankTournament(shownHoleCount, 0,
            GetNetScoreToPar(g_Scorecard.content.holeIndex, 0),
            g_GameState.unk2C);
        g_Scorecard.placeChange = PLACE_HELD;
      }

      g_Scorecard.topRow = 0;
      break;
  }

  for (i = 0; i != PAGE_TITLE_COUNT; i++) {
    g_Scorecard.pageTitleImages[i] = AllocMemory(
        OpenEncodedAsset(f_PageTitleAssets[pageTitlesRow][i], &asset));
    ReadEncodedAsset(g_Scorecard.pageTitleImages[i], &asset);
  }

  /* Only the bets being played have images */
  for (i = 0, imageIndex = 0; i != BET_COUNT; i++) {
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
      AllocMemory(OpenEncodedAsset(TOTAL_HEADING_IMAGE_ASSET, &asset));
  ReadEncodedAsset(g_Scorecard.totalHeadingImage, &asset);
  g_Scorecard.gridImage =
      AllocMemory(OpenEncodedAsset(GRID_IMAGE_ASSET, &asset));
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

  if (g_ScorecardState != SCORECARD_CLOSED) {
    g_ScorecardState = SCORECARD_CLOSED;
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
    switch (g_GameState.gameMode) {
      case GAME_MODE_STROKE:
      case GAME_MODE_UNK1:
      case GAME_MODE_TRAINING:
      case GAME_MODE_UNK6:
      case GAME_MODE_UNK8:
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
        for (i = 0; i != LEADERBOARD_GLYPH_GROUP_COUNT; i++) {
          PlaceGlyphGroup(i, NO_FONT_SIZE, 0, 0, 0, g_PrintBuffer);
        }

        break;
    }

    FreeMemory(&g_Scorecard.totalLabelsImage);
    FreeMemory(&g_Scorecard.labelsImage);
    FreeMemory(&g_Scorecard.digitsImage);
    FreeMemory(&g_Scorecard.betSubtitlesImage);
    FreeMemory(&g_Scorecard.scoresPanelImage);
    FreeMemory(&g_Scorecard.unk14);
    FreeMemory(&g_Scorecard.tableImage);
    for (i = 0; i != g_GameState.playerCount; i++) {
      FreeMemory(&g_Scorecard.faceImages[i]);
      FreeMemory(&g_Scorecard.facePalettes[i]);
    }

    SetTextSlot(
        0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED, 0, 0, 0, 0);
    SetTextSlot(
        0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 1, TEXT_FLAG_TYPED, 0, 0, 0, 0);
  }

  ForceStopRumble();
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", FreeScorecard);
#endif

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
  SongId song;
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
        if (g_Players[0].holes[holeIndex].strokes <
            g_Players[1].holes[holeIndex].strokes) {
          song = SONG_HOLE_WON;
        } else if (g_Players[1].holes[holeIndex].strokes <
            g_Players[0].holes[holeIndex].strokes) {
          song = SONG_HOLE_LOST;
        } else {
          goto HALVED;
        }
      } else {
        song = SONG_HOLE_WON;
        if (g_Players[0].holes[holeIndex].strokes ==
            g_Players[1].holes[holeIndex].strokes) {
HALVED:

          song = SONG_HOLE_HALVED;
        }
      }

      PlaySong(song, 0);
      if (g_Players[0].holes[holeIndex].strokes ==
          g_Players[1].holes[holeIndex].strokes) {
        g_Scorecard.holeWinnerIndex = HOLE_WINNER_HALVED;
      } else if (g_Players[0].holes[holeIndex].strokes <
          g_Players[1].holes[holeIndex].strokes) {
        g_Scorecard.holeWinnerIndex = 0;
      } else {
        g_Scorecard.holeWinnerIndex = 1;
      }

      g_Scorecard.skinsCarryover.onLastHole = 0;
      g_Scorecard.skinsCarryover.intoLastHole =
          g_Scorecard.content.holeIndex - 1;
      g_Scorecard.skinsCarryover.fromLastHole =
          g_Scorecard.skinsCarryover.intoLastHole;
      break;
    case SCORECARD_LAYOUT_SKINS:
      g_Scorecard.holeWinnerIndex = HOLE_WINNER_HALVED;
      for (i = 0; i != g_GameState.playerCount; i++) {
        if (g_Players[i].holes[g_Scorecard.content.holeIndex - 1].strokes !=
            0) {
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
        g_GameState.stopwatchFrameCount = STOPWATCH_MAX_FRAMES;
      }

      for (i = 0, timedHoleCount = 0; i != COURSE_HOLE_COUNT; i++) {
        timedHoleCount += g_HoleTimeFrames[i] != 0;
      }

      if (timedHoleCount > 0) {
        /* The stopwatch runs on through the round, so the last hole's time
         * first takes all of it */
        g_HoleTimeFrames[timedHoleCount - 1] = g_GameState.stopwatchFrameCount;
        g_HoleTimeMinutes[timedHoleCount - 1] =
            g_GameState.stopwatchFrameCount / FRAMES_PER_MINUTE;
        g_HoleTimeSeconds[timedHoleCount - 1] =
            g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND %
            SECONDS_PER_MINUTE;
        g_HoleTimeHundredths[timedHoleCount - 1] =
            (s32)(g_GameState.stopwatchFrameCount *
                ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
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
            g_HoleTimeSeconds[timedHoleCount - 1] +=
                SECONDS_PER_MINUTE - g_HoleTimeSeconds[i];
          } else {
            g_HoleTimeSeconds[timedHoleCount - 1] -= g_HoleTimeSeconds[i];
          }

          if (g_HoleTimeHundredths[timedHoleCount - 1] <
              g_HoleTimeHundredths[i]) {
            if (g_HoleTimeSeconds[timedHoleCount - 1] > 0) {
              g_HoleTimeSeconds[timedHoleCount - 1]--;
            } else {
              g_HoleTimeMinutes[timedHoleCount - 1]--;
              g_HoleTimeSeconds[timedHoleCount - 1] = SECONDS_PER_MINUTE - 1;
            }

            g_HoleTimeHundredths[timedHoleCount - 1] +=
                HUNDREDTHS_PER_SECOND - g_HoleTimeHundredths[i];
          } else {
            g_HoleTimeHundredths[timedHoleCount - 1] -= g_HoleTimeHundredths[i];
          }
        }
      }

      break;
  }

  g_RoundHud.scorecardPage = 0;
  switch (g_GameState.gameMode) {
    case GAME_MODE_UNK1:
    case GAME_MODE_TRAINING:
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
        CourseRecord* course = &g_CourseRecords[GetCourseRecordIndex()];

        if (g_Players[i].holes[g_Scorecard.content.holeIndex - 1].strokes -
                (s8)course->holes[g_Scorecard.content.holeIndex - 1].par +
                GetHandicapOffset(g_GameState.courseIndex,
                    g_Scorecard.content.holeIndex - 1, g_Players[i].handicap) <=
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
      /* A player with more strokes on the hole than anybody else looks behind
       */
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
  switch (g_GameState.gameMode) {
    case GAME_MODE_UNK1:
    case GAME_MODE_TRAINING:
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
        } else if (g_Players[1].holes[i].strokes <
            g_Players[0].holes[i].strokes) {
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
          if (i != j &&
              (s16)g_RoundHud.skinsTotals[i] > (s16)g_RoundHud.skinsTotals[j]) {
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
    PlaySoundWithEffects(SOUND_PANEL_OPEN, g_NextSoundChannel,
        SCORECARD_SOUND_VOLUME, SCORECARD_SLIDE_SOUND_REVERB,
        SCORECARD_OPEN_SOUND_PAN);
  } else if (state == SCORECARD_CLOSING) {
    PlaySoundWithEffects(SOUND_PANEL_CLOSE, g_NextSoundChannel,
        SCORECARD_SOUND_VOLUME, SCORECARD_SLIDE_SOUND_REVERB,
        SCORECARD_CLOSE_SOUND_PAN);
  }

  if (state == SCORECARD_OPENING) {
    g_Scorecard.x = SCREEN_WIDTH;
    g_Scorecard.y = 0;
  }

  g_ScorecardState = state;
  g_Scorecard.stateTime = 0;
}

/* Slides the scorecard in or out, tallies the last hole, and animates the faces
 * on it */
void AnimateScorecard(void) {
  EncodedAsset asset;
  s32 i;
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
        } else {
          g_ScorecardState = SCORECARD_TALLYING;
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
        SetFramebufferCount(TRIPLE_BUFFERING);
        SetScreenEffect(SCREEN_EFFECT_NONE, 0);
        g_RoundHud.isLetterboxShown = TRUE;
        g_RoundHud.unkC = 1;
        g_RoundHud.isWindIndicatorShown = TRUE;
        g_RoundHud.unk6 = 0;
        g_RoundHud.unk8 = 1;
        g_RoundHud.isLieIconShown = TRUE;
        g_RoundHud.isClubHeadIconShown = TRUE;
        g_RoundHud.unkB = 1;
        g_RoundHud.unkD = 1;
      }

      break;
    case SCORECARD_SHOWN:
      if (g_Scorecard.faceTime++ % FACE_FRAME_TIME == 1) {
        g_Scorecard.faceFrame++;
        g_Scorecard.unkBC ^= 1;
        if (g_Scorecard.faceFrame < FACE_FRAME_COUNT) {
          for (i = 0; i != g_GameState.playerCount; i++) {
            OpenEncodedAsset(
                g_CharacterImageAssets[(s8)g_Players[i].characterIndex] +
                    g_Scorecard.faceFrame +
                    g_Scorecard.content.faceImageOffsets[i],
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
 * Left and right on the control pad turn the pages, round from the last to the
 * first.
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
    PlaySoundWithEffects(SOUND_MENU_MOVE, g_NextSoundChannel,
        SCORECARD_SOUND_VOLUME, 0,
        (g_RoundHud.scorecardPage + 1) *
            (SOUND_PAN_FULL_RIGHT / (pageCount + 1)));
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard", TurnScorecardPage);
#endif

/* Flips the leaderboard's rows over, and scrolls it with up and down on the
 * control pad */
void ScrollLeaderboard(u16 buttons) {
  s32 row;

  if (g_Scorecard.content.flipTime < ROW_FLIP_TIME) {
    g_Scorecard.content.flipTime++;
  }

  for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
    if (row * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime &&
        g_Scorecard.content.isRowFlipping[row] == TRUE) {
      g_Scorecard.content.rowFlipAngles[row] +=
          DegreesToRadians(ROW_FLIP_DEGREES_PER_FRAME);
      if (g_Scorecard.content.rowFlipAngles[row] >
          DegreesToRadians(ROW_FLIP_END_DEGREES)) {
        g_Scorecard.content.rowFlipAngles[row] = 0.0f;
        g_Scorecard.content.isRowFlipping[row] = FALSE;
        PlaySound(SOUND_LOOK_DOWN_RANGE, ROW_FLIP_SOUND_CHANNEL,
            SCORECARD_SOUND_VOLUME);
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
      case GAME_MODE_UNK1:
      case GAME_MODE_MATCH_GAME:
      case GAME_MODE_TRAINING:
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
  if (g_ScorecardState != SCORECARD_SHOWN) {
    return FALSE;
  }

  switch (g_GameState.gameMode) {
    case GAME_MODE_STROKE:
    case GAME_MODE_UNK1:
    case GAME_MODE_MATCH_GAME:
    case GAME_MODE_TRAINING:
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
void DrawLargeImage(Gfx** displayListHead, u8* image, s32 left, s32 top) {
  Gfx* gfx = *displayListHead;
  s32 width = (image[IMAGE_WIDTH_OFFSET] << 8) | image[IMAGE_WIDTH_OFFSET + 1];
  s32 height =
      (image[IMAGE_HEIGHT_OFFSET] << 8) | image[IMAGE_HEIGHT_OFFSET + 1];
  s32 stripCount;
  s32 stripIndex;

  stripCount = 1;
  if (width > MAX_UNSPLIT_IMAGE_WIDTH) {
    stripCount = 2;
  }

  LoadLargePalette(&gfx, ImageBody(image));
  height /= stripCount;
  for (stripIndex = 0; stripIndex != stripCount; stripIndex++) {
    LoadTextureBlock(&gfx,
        LargeImageTexels(image) + (stripIndex * width * height), G_IM_FMT_CI,
        G_IM_SIZ_8b, width, height, 0, 0, 0, 0, 0, 0, 0);
    DrawTextureRect(&gfx, QuarterPixels(left), QuarterPixels(top),
        QuarterPixels(left + width), QuarterPixels(top + height),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    top += height;
  }

  *displayListHead = gfx;
}

/* The page titles sit in a row along the scorecard's top, the page shown a
 * pixel higher */
#define PAGE_TITLE_LEFT 32
#define PAGE_TITLE_SPACING 136
#define THREE_PAGE_TITLE_LEFT 29
#define THREE_PAGE_TITLE_SPACING 104
#define PAGE_TITLE_TOP 21
#define UNSHOWN_PAGE_GRAY 0x7F

/*
 * Draws the scorecard: its page titles, with the other pages' grayed out, and
 * the page shown, which depends on the game mode. Draws DrawMatchResult's
 * screen instead while that is up.
 */
void DrawScorecard(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 i;
  s32 dropY;

  if (g_ScorecardState == SCORECARD_MATCH_RESULT) {
    DrawMatchResult(&gfx);
  } else if (g_ScorecardState != SCORECARD_UNK6) {
    if (g_ScorecardState != SCORECARD_CLOSED) {
      DrawScorecardTable(&gfx, g_Scorecard.pageCount, g_RoundHud.scorecardPage);
      gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
      for (i = 0; i != g_Scorecard.pageCount; i++) {
        if (i == g_RoundHud.scorecardPage) {
          gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
          dropY = 0;
        } else {
          gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
          gDPSetPrimColor(gfx++, 0, 0, UNSHOWN_PAGE_GRAY, UNSHOWN_PAGE_GRAY,
              UNSHOWN_PAGE_GRAY, 0xFF);
          dropY = 1;
        }

        gDPPipeSync(gfx++);
        if (g_Scorecard.pageCount == THREE_PAGE_COUNT) {
          DrawLargeImage(&gfx, g_Scorecard.pageTitleImages[i],
              ((i * THREE_PAGE_TITLE_SPACING) + THREE_PAGE_TITLE_LEFT) +
                  g_Scorecard.x,
              g_Scorecard.y + PAGE_TITLE_TOP + dropY);
        } else {
          DrawLargeImage(&gfx, g_Scorecard.pageTitleImages[i],
              ((i * PAGE_TITLE_SPACING) + PAGE_TITLE_LEFT) + g_Scorecard.x,
              g_Scorecard.y + PAGE_TITLE_TOP + dropY);
        }
      }

      switch (g_GameState.gameMode) {
        case GAME_MODE_TOURNAMENT:
          if (g_RoundHud.scorecardPage == 0) {
            DrawLeaderboardPage(&gfx);
          } else {
            DrawScoresPage(&gfx);
          }

          break;
        case GAME_MODE_SPEED_GOLF:
          if (g_RoundHud.scorecardPage == 0) {
            DrawSpeedGolfPage(&gfx);
          } else {
            DrawScoresPage(&gfx);
          }

          break;
        case GAME_MODE_SKINS_MATCH:
        case GAME_MODE_CLUB_SLOTS:
          if (g_RoundHud.scorecardPage == 0) {
            DrawSkinsPage(&gfx);
          } else if (g_RoundHud.scorecardPage == 1) {
            DrawScoresPage(&gfx);
          } else {
            DrawBetsPage(&gfx);
          }

          break;
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_GET_CHARACTER:
          SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0,
              TEXT_FLAG_TYPED, 0, 0, 0, 0);
          SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 1,
              TEXT_FLAG_TYPED, 0, 0, 0, 0);
          if (g_RoundHud.scorecardPage == 0) {
            DrawMatchPage(&gfx);
          } else if (g_RoundHud.scorecardPage == 1) {
            DrawScoresPage(&gfx);
          } else {
            DrawBetsPage(&gfx);
          }

          break;
        case GAME_MODE_STROKE:
        case GAME_MODE_UNK1:
        case GAME_MODE_TRAINING:
        case GAME_MODE_UNK6:
        case GAME_MODE_UNK8:
        case GAME_MODE_RING_SHOT:
          if (g_GameState.playerCount == 1) {
            if (g_RoundHud.scorecardPage == 0) {
              DrawScoresPage(&gfx);
            } else {
              DrawBetsPage(&gfx);
            }
          } else if (g_RoundHud.scorecardPage == 0) {
            DrawStandingsPage(&gfx);
          } else if (g_RoundHud.scorecardPage == 1) {
            DrawScoresPage(&gfx);
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
/* The tabs behind the front one sit lower */
#define TAB_BACK_DROP 2
#define TABLE_BODY_TOP 44
#define TABLE_BODY_HEIGHT 190
#define TABLE_BODY_OFFSET \
  (LARGE_PALETTED_TEXELS_OFFSET + (TAB_STRIP_COUNT * TAB_HEIGHT * TABLE_WIDTH))
/* The rows that fit in texture memory at once */
#define TABLE_LOAD_HEIGHT 7
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
void DrawScorecardTable(Gfx** displayListHead, s32 pageCount, s32 page) {
  s32 tabStrips[THREE_PAGE_COUNT];
  Gfx* gfx = *displayListHead;
  s32 tabIndex;
  s32 row;
  s32 dropY;
  s32 texelsOffset;
  s32 loadHeight;
  s32 top;

  gSPDisplayList(gfx++, g_DecalTextureSetup);
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

  LoadLargePalette(&gfx, ImageBody(g_Scorecard.tableImage));
  gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
  gDPSetPrimColor(
      gfx++, 0, 0, TAB_BACK_GRAY, TAB_BACK_GRAY, TAB_BACK_GRAY, 0xFF);
  for (tabIndex = 0; tabIndex != pageCount; tabIndex++) {
    texelsOffset = (tabStrips[tabIndex] * TAB_HEIGHT * TABLE_WIDTH) +
        LARGE_PALETTED_TEXELS_OFFSET;
    dropY = TAB_BACK_DROP;
    if (tabIndex == pageCount - 1) {
      gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      dropY = 0;
    }

    for (row = 0; row < TAB_HEIGHT; row += TABLE_LOAD_HEIGHT) {
      LoadTextureBlock(&gfx, g_Scorecard.tableImage + texelsOffset, G_IM_FMT_CI,
          G_IM_SIZ_8b, TABLE_WIDTH, TABLE_LOAD_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
      texelsOffset += TABLE_LOAD_HEIGHT * TABLE_WIDTH;
      DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + TABLE_LEFT),
          QuarterPixels(row + g_Scorecard.y + TAB_TOP + dropY),
          QuarterPixels(g_Scorecard.x + TABLE_LEFT + TABLE_SHOWN_WIDTH),
          QuarterPixels(
              row + (g_Scorecard.y + TAB_TOP + TABLE_LOAD_HEIGHT) + dropY),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  texelsOffset = TABLE_BODY_OFFSET;
  for (row = 0; row < TABLE_BODY_HEIGHT; row += TABLE_LOAD_HEIGHT) {
    /* The body's last load is its one leftover row */
    loadHeight =
        (row + TABLE_LOAD_HEIGHT <= TABLE_BODY_HEIGHT) ? TABLE_LOAD_HEIGHT : 1;
    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
        TextureAddress(g_Scorecard.tableImage + texelsOffset));
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0,
        (loadHeight * (TABLE_WIDTH / 2)) - 1,
        CALC_DXT(TABLE_WIDTH, G_IM_SIZ_8b_BYTES));
    top = g_Scorecard.y + TABLE_BODY_TOP;
    texelsOffset += TABLE_LOAD_HEIGHT * TABLE_WIDTH;
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + TABLE_LEFT),
        QuarterPixels(row + top),
        QuarterPixels(g_Scorecard.x + TABLE_LEFT + TABLE_SHOWN_WIDTH),
        QuarterPixels(row + loadHeight + top), G_TX_RENDERTILE, 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
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
  f_ResultCharactersLoadStep = RESULT_CHARACTERS_IDLE;
}

/*
 * Starts loading the two players' characters to react to who did better on
 * the last hole, and plays the result's song. Returns FALSE while the
 * character slots are still busy.
 */
s32 LoadResultCharacters(void) {
  s32 holeIndex = g_Scorecard.content.holeIndex - 1;
  SongId song;

  if (IsCharacterSlotBusy(0) || IsCharacterSlotBusy(1)) {
    FreeCharacterSlot(0);
    FreeCharacterSlot(1);
    return FALSE;
  }

  LoadCharacter(0, (s8)g_Players[0].characterIndex + CHARACTER_COUNT,
      (s8)g_Players[0].paletteIndex);
  LoadCharacter(1, (s8)g_Players[1].characterIndex + CHARACTER_COUNT,
      (s8)g_Players[1].paletteIndex);
  song = SONG_MATCH_RESULT;
  if (g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
    /* The original passes the strokes through song's register */
    song = g_Players[0].holes[holeIndex].strokes;
    if (song < g_Players[1].holes[holeIndex].strokes) {
      song = SONG_HOLE_WON;
    } else if (g_Players[1].holes[holeIndex].strokes <
        g_Players[0].holes[holeIndex].strokes) {
      song = SONG_HOLE_LOST;
    } else {
      song = SONG_HOLE_HALVED;
    }
  }

  PlaySong(song, 0);
  if (g_Players[0].holes[holeIndex].strokes <
      g_Players[1].holes[holeIndex].strokes) {
    SetCharacterAnimation(0, REACTION_ANIMATION_BIRDIE);
    SetCharacterAnimation(1, REACTION_ANIMATION_BOGEY);
  } else if (g_Players[1].holes[holeIndex].strokes <
      g_Players[0].holes[holeIndex].strokes) {
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

/*
 * Counts the last hole's result onto the scorecard. Returns TRUE once it is
 * done.
 */
s32 TallyScorecardSkins(void) {
  switch (g_Scorecard.layout) {
    case SCORECARD_LAYOUT_STROKE:
    case SCORECARD_LAYOUT_TOURNAMENT:
    case SCORECARD_LAYOUT_SPEED_GOLF:
      return TRUE;
    case SCORECARD_LAYOUT_MATCH:
      if (g_Scorecard.stateTime == MATCH_RESULT_SOUND_TIME &&
          g_Scorecard.holeWinnerIndex != HOLE_WINNER_HALVED) {
        PlaySound(
            SOUND_TALLY_DONE, TALLY_SOUND_CHANNEL, SCORECARD_SOUND_VOLUME);
      }

      if (g_Scorecard.stateTime == MATCH_RESULT_TIME) {
        g_Scorecard.skinsCarryover.onLastHole = 1;
        if (g_Scorecard.holeWinnerIndex != HOLE_WINNER_HALVED) {
          PlaySound(SOUND_THREE_STARS, MATCH_RESULT_SOUND_CHANNEL,
              SCORECARD_SOUND_VOLUME);
        }

        return TRUE;
      }

      break;
    case SCORECARD_LAYOUT_SKINS:
      if (g_Scorecard.holeWinnerIndex == HOLE_WINNER_HALVED) {
        if (g_Scorecard.stateTime == SKINS_TALLY_TIME) {
          g_Scorecard.skinsCarryover.intoLastHole++;
          PlaySound(
              SOUND_SKIN_COUNTED, TALLY_SOUND_CHANNEL, SCORECARD_SOUND_VOLUME);
          return TRUE;
        }
      } else if (g_Scorecard.stateTime >= SKINS_TALLY_TIME) {
        if (g_Scorecard.skinsCarryover.onLastHole <= 0) {
          return TRUE;
        }

        if ((g_Scorecard.stateTime - SKINS_TALLY_TIME) % SKINS_TALLY_INTERVAL ==
            1) {
          if (g_Scorecard.skinsCarryover.intoLastHole > 0) {
            g_Scorecard.skinsCarryover.intoLastHole--;
          }

          g_Scorecard.skinsCarryover.onLastHole--;
          PlaySound(
              SOUND_SKIN_COUNTED, TALLY_SOUND_CHANNEL, SCORECARD_SOUND_VOLUME);
        }
      }

      break;
  }

  return FALSE;
}
