/*
 * Records the player's progress in the save: wins and losses against the
 * characters, birdie badges and tournament places.
 */
#include "progress.h"

#include <ultra64.h>

#include "course.h"
#include "effects.h"
#include "include_asm.h"
#include "language.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "tournament.h"

/* Counts in the save stop here */
#define MAX_SAVED_COUNT 99

/* Any place past third, which no flag records */
#define PLACE_UNPLACED 4

/*
 * Counts a win or a loss in matchups[row][column], and saves. A win also sets
 * WON_UNSAVED_FLAG unless g_GameState.saveCount is set, and prints "Win Com
 * Chara".
 *
 * Doesn't match: GCC masks the new win bits to a byte before printing them,
 * where the original prints the unmasked value. Building them in newValue
 * gives 19 differing words, not 3; dropping newValue or printing the stored
 * byte gives 5, and an s32 or u32 value gives 9. Blame traces the extra andi
 * to the RTL zero_extendqisi2 at newValue = value; mips.md emits it for a
 * register-to-register QI-to-SI conversion. Widening value removes that mask
 * but moves the flag mask and ties the combined bits to the final result
 * instead of copying them in the branch delay slot. Widening count too gives
 * 33 differing words; casting the comparison back to u8 does not restore the
 * original. Promoting value to newValue before the saveCount test gives 6,
 * and assigning newValue separately in both branches gives 23.
 * END_BASIC_BLOCK before the combination or final promotion leaves 3; before
 * the saveCount test gives 21. Of the 27 combinations of u8, s32 and u32 for
 * value, count and newValue, none gives fewer than 3, and an s32 or u32
 * count gives 31 to 36 whatever the other two are, though the original adds
 * 1 to the count unmasked. Keep the byte draft until both the promotion and
 * delay-slot copy are explained. The register oracle's closest result is
 * these 3 words with no register forced, so allocation isn't the cause. The
 * original picks the printed value as either the combined bits or the bits
 * with the flag, in another register, so choosing newValue from the combined
 * value with a ternary or an if/else in either order was tried, with value
 * and count each a u8, s32 or u32 and newValue an s32 or u32: these give 19
 * to 47 words, and setting newValue to the combined bits and or-ing the flag
 * into it 19 to 27. Without newValue, the 9 combinations of u8, s32 and u32
 * for value and count give 5 words (3 in shape) for two bytes, 9 (5) for a
 * byte count and a wider value, and 33 (13) for any wider count. 3 words
 * differ.
 *
 * A focused permuter run's best made value a u16. Of the 44 combinations
 * with a u16 or s16 among value, count and newValue, none gives fewer than 3
 * words: a u16 or s16 newValue with byte value and count gives 3, as does a
 * u16 value with an s32 newValue and a byte or u16 count, and the rest 4 to
 * 37.
 */
#ifdef NON_MATCHING
void RecordComCharacterResult(s32 row, s32 column, Boolean isWin) {
  Awards* awards;
  u8 count;
  u8 value;
  s32 newValue;

  awards = &GetSaveData()->records.awards;
  if (isWin) {
    value = awards->matchups[row][column].winBits;
    count = value & WIN_COUNT_MASK;
    if (count < MAX_SAVED_COUNT) {
      count = (value & WIN_COUNT_MASK) + 1;
    }

    value = count | (value & WON_UNSAVED_FLAG);
    if (!g_GameState.saveCount) {
      value |= WON_UNSAVED_FLAG;
    }

    newValue = value;
    awards->matchups[row][column].winBits = newValue;
    osSyncPrintf("Win Com Chara[%d]\n", newValue);
  } else {
    awards->matchups[row][column].lossCount++;
    if (awards->matchups[row][column].lossCount > MAX_SAVED_COUNT) {
      awards->matchups[row][column].lossCount = MAX_SAVED_COUNT;
    }
  }

  TransferSaveData(GetSaveData(), TRUE);
}
#else
INCLUDE_ASM("asm/nonmatchings/progress", RecordComCharacterResult);
#endif

/* Adds a birdie badge on the current hole of a tournament, and saves */
void AddBirdieBadge(void) {
  Awards* awards;

  awards = &GetSaveData()->records.awards;
  if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
    awards->birdieBadgeCounts[g_GameState.courseIndex][g_GameState.holeIndex]++;
    if (awards->birdieBadgeCounts[g_GameState.courseIndex]
                                 [g_GameState.holeIndex] > MAX_SAVED_COUNT) {
      awards
          ->birdieBadgeCounts[g_GameState.courseIndex][g_GameState.holeIndex] =
          MAX_SAVED_COUNT;
    }

    TransferSaveData(GetSaveData(), TRUE);
  }
}

/*
 * Passes how many holes have a birdie badge to ShowBirdieBadgeCount, if this is
 * a tournament and the current hole has none yet
 */
void ReportBirdieBadgeCount(void) {
  Awards* awards;
  s32 badgeCount;
  CourseIndex courseIndex;
  s32 holeIndex;

  awards = &GetSaveData()->records.awards;
  if (g_GameState.gameMode == GAME_MODE_TOURNAMENT &&
      awards->birdieBadgeCounts[g_GameState.courseIndex]
                               [g_GameState.holeIndex] == 0) {
    badgeCount = 0;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
      for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
        badgeCount +=
            awards->birdieBadgeCounts[courseIndex][holeIndex] != FALSE;
      }
    }

    ShowBirdieBadgeCount(badgeCount);
  }
}

/* Flags the course's statistics with the player's place in the tournament, if
 * third or better, and saves */
void RecordTournamentPlace(void) {
  CourseStats* stats;
  s32* placeEntry;
  Boolean hasPlace;
  s32 place;
  s32 index;
  TournamentStanding* standing;

  stats = &GetSaveData()->records.courseStats[g_GameState.courseIndex];
  hasPlace = FALSE;
  if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
    RankTournament(GetLastHoleIndex() + 1, 0,
        GetNetScoreToPar(GetLastHoleIndex() + 1, 0), g_GameState.roundSeed);
    place = PLACE_UNPLACED;
    for (index = 0; index != TOURNAMENT_CONTESTANT_COUNT; index++) {
      standing = &g_TournamentStandings[index];
      if (standing->contestantId == PLAYER_CONTESTANT) {
        place = standing->place & PLACE_MASK;
        break;
      }
    }

    switch (place) {
      case 1:
        placeEntry = &stats->hasPlace[FIRST_PLACE_INDEX];
        hasPlace = TRUE;
        break;
      case 2:
        placeEntry = &stats->hasPlace[SECOND_PLACE_INDEX];
        hasPlace = TRUE;
        break;
      case 3:
        placeEntry = &stats->hasPlace[THIRD_PLACE_INDEX];
        hasPlace = TRUE;
        break;
      default:
        /* Not written, since hasPlace stays FALSE */
        placeEntry = &stats->hasPlace[THIRD_PLACE_INDEX];
        break;
    }

    if (hasPlace) {
      *placeEntry = TRUE;
    }

    TransferSaveData(GetSaveData(), TRUE);
  }
}
