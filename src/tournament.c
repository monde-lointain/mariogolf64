#include "common.h"

#include "tournament.h"

#include "course.h"
#include "libc.h"
#include "save.h"

/* Standing 0 is the player's, so the computer contestants start at 1 */
#define FIRST_COMPUTER_CONTESTANT 1

/* A computer contestant's final score stays within this many strokes of par */
#define SCORE_LIMIT 18
#define COURSE_HOLE_COUNT 18

#define RANDOM_MULTIPLIER 0x5D588B65

/* Half a sine wave over a round */
#define RADIANS_PER_HOLE ((f64)(3.1416f / COURSE_HOLE_COUNT))

typedef struct ContestantProfile {
  /* 0x0 */ s8 baseScore; /* Final strokes over par on the first course */
  /* 0x1 */ u8 unk1[2];
  /* 0x3 */ s8 holeOffset; /* Holes played ahead of the player */
} ContestantProfile;       /* size = 0x4 */

extern s32 sTournamentSeed;
extern ContestantProfile sContestantProfiles[TOURNAMENT_CONTESTANT_COUNT - 1];

void SortIndicesDescending(s32* values, s32* order, s32 count);
double sin(double angle);
void NameContestants(void);

void SeedTournamentRandom(s32 seed) { sTournamentSeed = seed; }

u32 NextTournamentRandom(void) {
  return sTournamentSeed = sTournamentSeed * RANDOM_MULTIPLIER + 1;
}

/*
 * Makes up the scores of contestants firstIndex to endIndex - 1 after the
 * player's holeCount holes; those up to lastAheadIndex, or all of them if it
 * is negative, are a hole further on. playerScore goes unused, and nothing
 * is returned; the return type is there to match.
 */
s32 SimulateScores(s32 holeCount, s32 lastAheadIndex, s32 firstIndex,
                   s32 endIndex, s32 playerScore) {
  s32 index;
  s32 shape;
  s32 swing;
  s32 holesPlayed;
  s32 finalScore;
  s32 jitter;
  s32 otherJitter;
  s32 score;

  if (holeCount > COURSE_HOLE_COUNT) {
    holeCount = COURSE_HOLE_COUNT;
  }
  for (index = firstIndex; index < endIndex; index++) {
    shape = index % 3;
    swing = index % 10;
    if (g_CourseIndex < 2) {
      swing = 13 - swing;
      if (swing >= 10) {
        swing = 9;
      }
    }
    holesPlayed = holeCount + (sContestantProfiles[index - 1].holeOffset +
                               (lastAheadIndex < 0 | lastAheadIndex >= index));
    if (holesPlayed > COURSE_HOLE_COUNT) {
      holesPlayed = COURSE_HOLE_COUNT;
    }
    finalScore = sContestantProfiles[index - 1].baseScore - g_CourseIndex * 2;
    jitter = ((NextTournamentRandom() >> 28) & 5) - 2;
    otherJitter = ((NextTournamentRandom() >> 28) & 5) - 2;
    finalScore += (jitter + otherJitter) / 2;
    if (holesPlayed == COURSE_HOLE_COUNT &&
        finalScore <= -(g_CourseIndex * 2) + 1) {
      finalScore = -(g_CourseIndex * 2) + 1;
    }
    if (finalScore < -SCORE_LIMIT) {
      finalScore = -SCORE_LIMIT;
    } else if (finalScore > SCORE_LIMIT) {
      finalScore = SCORE_LIMIT;
    }
    switch (shape) {
      case 0:
        score = finalScore * holesPlayed / COURSE_HOLE_COUNT;
        break;
      case 1:
        score = finalScore * holesPlayed / COURSE_HOLE_COUNT +
                sin(holesPlayed * RADIANS_PER_HOLE) * swing;
        break;
      case 2:
        score = finalScore * holesPlayed / COURSE_HOLE_COUNT -
                sin(holesPlayed * RADIANS_PER_HOLE) * swing;
        break;
    }
    g_TournamentStandings[index].contestantId = index - 1;
    g_TournamentStandings[index].score = score;
    g_TournamentStandings[index].holeCount = holesPlayed;
  }
}

/*
 * Simulates the computer contestants, ranks everyone by score and returns
 * the player's place
 */
u8 RankTournament(s32 holeCount, s32 lastAheadIndex, s32 playerScore,
                  s32 seed) {
  s32 keys[TOURNAMENT_CONTESTANT_COUNT];
  s32 order[TOURNAMENT_CONTESTANT_COUNT];
  TournamentStanding unsorted[TOURNAMENT_CONTESTANT_COUNT];
  s32 index;
  s32 otherIndex;
  s32 place;
  u8 tieBit;
  u8 playerPlace;

  playerPlace = 1;
  SeedTournamentRandom(seed);
  SimulateScores(holeCount, lastAheadIndex, FIRST_COMPUTER_CONTESTANT,
                 TOURNAMENT_CONTESTANT_COUNT, playerScore);
  NameContestants();
  g_TournamentStandings[0].score = playerScore;
  g_TournamentStandings[0].contestantId = PLAYER_CONTESTANT;
  g_TournamentStandings[0].holeCount = holeCount;
  for (index = 0; index < TOURNAMENT_CONTESTANT_COUNT; index++) {
    keys[index] = -g_TournamentStandings[index].score;
  }
  SortIndicesDescending(keys, order, TOURNAMENT_CONTESTANT_COUNT);
  for (index = 0; index < TOURNAMENT_CONTESTANT_COUNT; index++) {
    unsorted[index] = g_TournamentStandings[index];
  }
  for (index = 0; index < TOURNAMENT_CONTESTANT_COUNT; index++) {
    g_TournamentStandings[index] = unsorted[order[index]];
  }
  for (index = 0; index < TOURNAMENT_CONTESTANT_COUNT; index++) {
    place = 1;
    tieBit = 0;
    for (otherIndex = 0; otherIndex < index; otherIndex++) {
      if (g_TournamentStandings[index].score >
          g_TournamentStandings[otherIndex].score) {
        place++;
      } else {
        tieBit = PLACE_TIED;
        g_TournamentStandings[otherIndex].place |= PLACE_TIED;
      }
    }
    g_TournamentStandings[index].place = tieBit | place;
    if (g_TournamentStandings[index].contestantId == PLAYER_CONTESTANT) {
      playerPlace = tieBit | place;
    }
  }
  return playerPlace;
}

/*
 * Gives the computer contestants the course's names, skipping any that
 * matches the first player's record and taking the block's last name in
 * its place
 */
#ifdef NON_MATCHING
void NameContestants(void) {
  s32 index;
  s32 nameOffset;
  s32 hasSkipped;
  s32 lastName;
  s16* nameIndex;
  u32 isFirstName;

  nameOffset = TOURNAMENT_CONTESTANT_COUNT - 2;
  hasSkipped = FALSE;
  nameIndex = &g_ContestantNameIndices[TOURNAMENT_CONTESTANT_COUNT - 2];
  lastName = (g_CourseIndex + 1) * NAMES_PER_COURSE - 2;
  for (index = TOURNAMENT_CONTESTANT_COUNT - 2; index >= 0;
       index--, nameIndex--) {
    if (!hasSkipped && strcmp(g_Players[0].name,
                              g_ContestantNames[lastName - nameOffset]) == 0) {
      hasSkipped = TRUE;
      nameOffset--;
    }
    isFirstName = &g_ContestantNameIndices[1] > nameIndex;
    if (hasSkipped & isFirstName) {
      *nameIndex = lastName + 1;
    } else {
      *nameIndex = lastName - nameOffset;
      nameOffset--;
    }
  }
}

#else
INCLUDE_ASM("asm/nonmatchings/tournament", NameContestants);
#endif
