#include "tournament.h"

#include <math.h>
#include <string.h>
#include <ultra64.h>

#include "course.h"
#include "game_state.h"
#include "math/math_util.h"
#include "menu.h"
#include "player.h"
#include "system/c_language.h"
#include "system/compiler.h"

#define FIRST_COMPUTER_CONTESTANT 1

#define SCORE_LIMIT 18

#define RADIANS_PER_HOLE ((f64)(3.1416f / COURSE_HOLE_COUNT))

typedef struct ContestantProfile {
    s8 baseScore;
    u8 unusedBeforeHoleOffset[2];
    s8 holeOffset;
} ContestantProfile;

s16 g_ContestantNameIndices[TOURNAMENT_CONTESTANT_COUNT - 1];
TournamentStanding g_TournamentStandings[TOURNAMENT_CONTESTANT_COUNT];

static s32 f_TournamentSeed = 0;
static ContestantProfile f_ContestantProfiles[TOURNAMENT_CONTESTANT_COUNT - 1] = {
    {17, {0, 0}, 7},
    {16, {0, 0}, 7},
    {16, {0, 0}, 7},
    {15, {0, 0}, 7},
    {15, {0, 0}, 6},
    {14, {0, 0}, 6},
    {14, {0, 0}, 6},
    {13, {0, 0}, 6},
    {13, {0, 0}, 5},
    {12, {0, 0}, 5},
    {12, {0, 0}, 5},
    {11, {0, 0}, 5},
    {11, {0, 0}, 4},
    {10, {0, 0}, 4},
    {10, {0, 0}, 4},
    {9, {0, 0}, 4},
    {9, {0, 0}, 3},
    {8, {0, 0}, 3},
    {8, {0, 0}, 3},
    {7, {0, 0}, 3},
    {7, {0, 0}, 2},
    {6, {0, 0}, 2},
    {6, {0, 0}, 2},
    {5, {0, 0}, 2},
    {5, {0, 0}, 1},
    {3, {0, 0}, 1},
    {3, {0, 0}, 1},
    {1, {0, 0}, 1},
    {1, {0, 0}, 0},
};
char g_ContestantNames[COURSE_COUNT * NAMES_PER_COURSE][CONTESTANT_NAME_LENGTH] = {
    "T\xEF\xE1\xE4",
    "K\xE5\xEE",
    "K\xEF\xE2\xE5",
    "U\xEB\xEB\xE9\xEB\xE9",
    "B\xEF\xE2-\xEF\xED\xE2",
    "M\xEF\xEE\xF4\xF9_M\xEF\xEC\xE5",
    "F\xEC\xF9_G\xF5\xF9",
    "M\xE9\xEB\xE5",
    "G\xE1\xE9\xEC",
    "C\xEF\xEC\xE9\xEE",
    "B\xE1\xE2\xF9_M\xE1\xF2\xE9\xEF",
    "G\xF2\xE5\xE7",
    "C\xE8\xF2\xE9\xF3\xF4\xE9\xE1\xEE",
    "B\xE9\xEC\xEC",
    "P\xE5\xE1\xE3\xE8",
    "M\xE9\xE3\xE8\xE5\xEC\xEC\xE5",
    "P\xE8\xE9\xEC",
    "C\xE8\xE1\xF2\xEC\xE9\xE5",
    "L\xE9\xEE\xEB",
    "S\xE8\xE5\xE9\xEB",
    "P\xEC\xF5\xED",
    "D\xE1\xF2\xF5\xEE\xE9\xE1",
    "D\xEF\xF2\xE9\xE5",
    "F\xEF\xF8",
    "E\xF2\xE9\xE3\xE8",
    "S\xE1\xF2\xE9\xE1",
    "M\xE1\xEC\xEF\xEE",
    "M\xE3"
    "C\xEC\xEF\xF5\xE4",
    "B\xF5\xE2",
    "P\xE5\xF0\xF0\xF9",
    "K\xEF\xEF\xF0\xE1",
    "B\xF5\xE2\xE2\xE1",
    "L\xE1\xF2\xF2\xF9",
    "Y\xEF\xF3\xE8\xE9",
    "T\xEF\xE1\xE4",
    "K\xE5\xEE",
    "U\xEB\xEB\xE9\xEB\xE9",
    "B\xEF\xE2-\xEF\xED\xE2",
    "M\xEF\xEE\xF4\xF9_M\xEF\xEC\xE5",
    "L\xF5\xE9\xE7\xE9",
    "F\xEC\xF9_G\xF5\xF9",
    "C\xE8\xE5\xE5\xF0_C\xE8\xE5\xE5\xF0",
    "C\xE8\xE9\xEC\xEC_B\xF5\xEC\xEC\xF9",
    "B\xE9\xE7_P\xE5\xEE\xE7\xF5\xE9\xEE",
    "B\xE1\xE2\xF9_M\xE1\xF2\xE9\xEF",
    "H\xE5\xEE\xF2\xF9",
    "A\xF2\xED\xEF\xEE\xE4",
    "D\xE1\xF6\xE9\xE4",
    "F\xE1\xF2\xE1\xEE",
    "E\xE4",
    "T\xEF\xED",
    "M\xE5\xF4\xE1\xEC_M\xE1\xF2\xE9\xEF",
    "W\xE9\xE7\xE7\xEC\xE5\xF2",
    "T\xE1\xEC\xEF\xEE",
    "R\xE5\xE4_B\xE5\xE5\xFA\xEF",
    "L\xE5\xED\xED\xF9",
    "S\xF0\xE9\xEB\xE5",
    "L\xF5\xE4\xF7\xE9\xE7",
    "M\xEF\xF2\xF4\xEF\xEE",
    "R\xF5\xF4\xEF\xEF\xEF",
    "S\xE8\xF9_G\xF5\xF9",
    "W\xE8\xEF\xED\xF0",
    "Z\xE5\xEC\xE4\xE1",
    "M\xE1\xF0\xEC\xE5",
    "G\xEF\xEF\xED\xE2\xE1",
    "S\xEF\xEE\xEE\xF9",
    "K\xEF\xEF\xF0\xE1",
    "L\xE1\xEB\xE9\xF4\xF5",
    "L\xE1\xF2\xF2\xF9",
    "B\xF5\xFA\xFA\xF9",
    "Y\xEF\xF3\xE8\xE9",
    "S\xF0\xE9\xEE\xF9",
    "B\xEF\xE2-\xEF\xED\xE2",
    "L\xF5\xE9\xE7\xE9",
    "F\xEC\xF9_G\xF5\xF9",
    "C\xE8\xE5\xE5\xF0_C\xE8\xE5\xE5\xF0",
    "C\xE8\xE9\xEC\xEC_B\xF5\xEC\xEC\xF9",
    "B\xE9\xE7_P\xE5\xEE\xE7\xF5\xE9\xEE",
    "B\xE1\xE2\xF9_M\xE1\xF2\xE9\xEF",
    "R\xE5\xF8",
    "P\xE9\xE4\xE7\xE9\xF4",
    "W\xE5\xEE\xE4\xF9_O.",
    "G\xEF\xF2\xEF\xEE_K\xE9\xE4",
    "F\xE1\xEC\xE3\xEF_L.",
    "I\xEE\xF0\xE1",
    "D\xE5\xEB\xF5_S\xE3\xF2\xF5\xE2",
    "P\xE9\xE7\xED\xE1",
    "N\xE1\xF6\xE9",
    "S\xE1\xF2\xE9\xE1",
    "S\xE8\xE5\xE9\xEB",
    "U\xEE\xE1\xE7\xE9",
    "B\xF5\xE2\xE2\xE1",
    "S\xEB\xF5\xEC\xEC_K\xE9\xE4",
    "S\xEC\xE9\xF0\xF0\xF9",
    "H\xE1\xF2\xF2\xF9",
    "P\xE1\xF2\xE1\xF4\xF2\xEF\xEF\xF0\xE1",
    "C\xE8\xE1\xE9\xEE_C\xE8\xEF\xED\xF0",
    "P\xEF\xE4\xEF\xE2\xEF\xEF",
    "W\xE1\xF2\xE9\xEF",
    "S\xE8\xF9_G\xF5\xF9",
    "R\xE1\xF5\xF2\xF5",
    "M\xE1\xF0\xEC\xE5",
    "N\xE1\xE2\xEF\xEF\xF2\xF5",
    "S\xEF\xEE\xEE\xF9",
    "B\xE9\xE7_M\xF2._I.",
    "S\xE3\xF5\xF4\xF4\xEC\xE5_B\xF5\xE7",
    "B\xF5\xFA\xFA\xF9",
    "Y\xEF\xF3\xE8\xE9",
    "S\xF0\xE9\xEE\xF9",
    "C\xE8\xE1\xF2\xEC\xE5\xF3",
    "L\xF5\xE9\xE7\xE9",
    "J\xE5\xEE",
    "D\xE5\xF8",
    "H\xE1\xEC\xE5\xF9",
    "B\xE1\xE2\xF9_M\xE1\xF2\xE9\xEF",
    "T\xE5\xF2\xF2\xF9",
    "M\xE9\xEB\xE5",
    "D\xE1\xF6\xE9\xE4",
    "P\xE5\xEE\xEE",
    "K\xE1\xF4\xE5",
    "B\xEF\xEF",
    "P\xE1\xF2\xE1\xF4\xF2\xEF\xEF\xF0\xE1",
    "C\xE8\xE1\xE9\xEE_C\xE8\xEF\xED\xF0",
    "P\xEF\xE4\xEF\xE2\xEF\xEF",
    "W\xE1\xF2\xE9\xEF",
    "S\xE8\xF9_G\xF5\xF9",
    "M\xE1\xF0\xEC\xE5",
    "Z\xE5\xEC\xE4\xE1",
    "S\xF5\xED\xEF_B\xF2\xEF\xF3.",
    "S\xEF\xEE\xEE\xF9",
    "S\xE3\xF5\xF4\xF4\xEC\xE5_B\xF5\xE7",
    "J\xE5\xF3\xF3\xE9\xE3\xE1",
    "B\xF5\xFA\xFA\xF9",
    "Y\xEF\xF3\xE8\xE9",
    "S\xF0\xE9\xEE\xF9",
    "B\xEF\xE2-\xEF\xED\xE2",
    "L\xF5\xE9\xE7\xE9",
    "F\xEC\xF9_G\xF5\xF9",
    "C\xE8\xE5\xE5\xF0_C\xE8\xE5\xE5\xF0",
    "C\xE8\xE9\xEC\xEC_B\xF5\xEC\xEC\xF9",
    "B\xE1\xE2\xF9_M\xE1\xF2\xE9\xEF",
    "M\xE1\xF2\xEB",
    "T\xE9\xED",
    "M\xE9\xE3\xE8\xE1\xE5\xEC",
    "C\xE8\xF5\xE3\xEB\xF9\xE1",
    "S\xF4\xE1\xF2_W\xEF\xEC\xE6",
    "S\xEC\xE5\xE4\xE7\xE5_B\xF2\xEF\xF3.",
    "D\xE1\xED\xF0\xE5",
    "P\xE9\xE7\xED\xE1",
    "A\xEE\xE4\xF2\xEF\xF3\xF3",
    "B\xEF\xF7\xF3\xE5\xF2",
    "DK",
    "M\xE1\xF2\xE9\xEF",
    "G\xEF\xEF\xED\xE2\xE1",
    "U\xEE\xE1\xE7\xE9",
    "B\xE9\xE7_B\xEF\xE2-\xEF\xED\xE2",
    "A\xEE\xE4\xF9",
    "S\xEC\xE9\xF0\xF0\xF9",
    "H\xE1\xF2\xF2\xF9",
    "B\xEF\xEF",
    "B\xEC\xEF\xEF\xE2\xE5\xF2",
    "F\xE9\xF2\xE5_B\xF2\xEF\xF3.",
    "W\xE1\xF2\xE9\xEF",
    "H\xE1\xED\xED\xE5\xF2_B\xF2\xEF\xF3.",
    "S\xEE\xE9\xE6\xE9\xF4",
    "M\xE1\xF0\xEC\xE5",
    "S\xEF\xEE\xEE\xF9",
    "S\xF5\xED\xEF_B\xF2\xEF\xF3.",
    "B\xE1\xE2\xF9_B\xEF\xF7\xF3\xE5\xF2",
    "M\xF2._I.",
    "Y\xEF\xF3\xE8\xE9",
    "S\xF0\xE9\xEE\xF9",
    "T\xE1\xEC\xEF\xEE",
    "B\xEF\xEE\xEF\xEF\xF2\xF5",
    "P\xE9\xE5\xF2\xF2\xE5",
    "L\xF5\xE9\xE7\xE9",
    "K\xE9\xEE\xE7_Z\xEF\xF2\xE1",
    "P\xE1\xF5\xEC\xE9\xEE\xE5",
    "I\xEE\xE7\xEF",
    "M\xE9\xE4\xEF",
};
static s32 f_UnusedWordAfterContestantProfiles UNUSED = 0;

void NameContestants(void);

void SeedTournamentRandom(s32 seed) {
    f_TournamentSeed = seed;
}

u32 NextTournamentRandom(void) {
    return f_TournamentSeed = NextRandomSeed(f_TournamentSeed);
}

enum ScoreShape {
    SCORE_SHAPE_EVEN,
    SCORE_SHAPE_SWING_ABOVE,
    SCORE_SHAPE_SWING_BELOW,
    SCORE_SHAPE_COUNT
};

#define SWING_AMPLITUDE_COUNT 10
#define EARLY_COURSE_SWING_REVERSAL 13

NO_RESULT SimulateScores(
    s32 holeCount,
    s32 lastAheadIndex,
    s32 firstIndex,
    s32 endIndex,
    s32 playerScore UNUSED
) {
    s32 index;
    s32 scoreShape;
    s32 swingAmplitude;
    s32 holesPlayed;
    s32 finalScore;
    s32 jitter;
    s32 otherJitter;
    s32 score;

    if (holeCount > COURSE_HOLE_COUNT) {
        holeCount = COURSE_HOLE_COUNT;
    }

    for (index = firstIndex; index < endIndex; index++) {
        scoreShape = index % SCORE_SHAPE_COUNT;
        swingAmplitude = index % SWING_AMPLITUDE_COUNT;
        if (g_GameState.courseIndex < COURSE_SHY_GUY_DESERT) {
            swingAmplitude = EARLY_COURSE_SWING_REVERSAL - swingAmplitude;
            if (swingAmplitude >= SWING_AMPLITUDE_COUNT) {
                swingAmplitude = SWING_AMPLITUDE_COUNT - 1;
            }
        }

        holesPlayed = holeCount +
            (f_ContestantProfiles[index - 1].holeOffset +
                ((lastAheadIndex < 0) | (lastAheadIndex >= index)));
        if (holesPlayed > COURSE_HOLE_COUNT) {
            holesPlayed = COURSE_HOLE_COUNT;
        }

        finalScore = f_ContestantProfiles[index - 1].baseScore - (g_GameState.courseIndex * 2);
        jitter = ((NextTournamentRandom() >> 28) & 5) - 2;
        otherJitter = ((NextTournamentRandom() >> 28) & 5) - 2;
        finalScore += (jitter + otherJitter) / 2;
        if (holesPlayed == COURSE_HOLE_COUNT && finalScore <= -(g_GameState.courseIndex * 2) + 1) {
            finalScore = -(g_GameState.courseIndex * 2) + 1;
        }

        if (finalScore < -SCORE_LIMIT) {
            finalScore = -SCORE_LIMIT;
        }
        else if (finalScore > SCORE_LIMIT) {
            finalScore = SCORE_LIMIT;
        }

        switch (scoreShape) {
#ifdef AVOID_UB
            default:
#endif
            case SCORE_SHAPE_EVEN:
                score = finalScore * holesPlayed / COURSE_HOLE_COUNT;
                break;
            case SCORE_SHAPE_SWING_ABOVE:
                score = (finalScore * holesPlayed / COURSE_HOLE_COUNT) +
                    (sin(holesPlayed * RADIANS_PER_HOLE) * swingAmplitude);
                break;
            case SCORE_SHAPE_SWING_BELOW:
                score = (finalScore * holesPlayed / COURSE_HOLE_COUNT) -
                    (sin(holesPlayed * RADIANS_PER_HOLE) * swingAmplitude);
                break;
        }

        g_TournamentStandings[index].contestantId = index - 1;
        g_TournamentStandings[index].score = score;
        g_TournamentStandings[index].holeCount = holesPlayed;
    }
}

u8 RankTournament(s32 holeCount, s32 lastAheadIndex, s32 playerScore, s32 seed) {
    s32 sortKeys[TOURNAMENT_CONTESTANT_COUNT];
    s32 order[TOURNAMENT_CONTESTANT_COUNT];
    TournamentStanding unsortedStandings[TOURNAMENT_CONTESTANT_COUNT];
    s32 index;
    s32 otherIndex;
    s32 place;
    u8 tieBit;
    u8 playerPlace;

    playerPlace = 1;
    SeedTournamentRandom(seed);
    SimulateScores(
        holeCount,
        lastAheadIndex,
        FIRST_COMPUTER_CONTESTANT,
        TOURNAMENT_CONTESTANT_COUNT,
        playerScore
    );
    NameContestants();
    g_TournamentStandings[0].score = playerScore;
    g_TournamentStandings[0].contestantId = PLAYER_CONTESTANT;
    g_TournamentStandings[0].holeCount = holeCount;
    for (index = 0; index < TOURNAMENT_CONTESTANT_COUNT; index++) {
        sortKeys[index] = -g_TournamentStandings[index].score;
    }

    SortIndicesDescending(sortKeys, order, TOURNAMENT_CONTESTANT_COUNT);
    for (index = 0; index < TOURNAMENT_CONTESTANT_COUNT; index++) {
        unsortedStandings[index] = g_TournamentStandings[index];
    }

    for (index = 0; index < TOURNAMENT_CONTESTANT_COUNT; index++) {
        g_TournamentStandings[index] = unsortedStandings[order[index]];
    }

    for (index = 0; index < TOURNAMENT_CONTESTANT_COUNT; index++) {
        place = 1;
        tieBit = 0;
        for (otherIndex = 0; otherIndex < index; otherIndex++) {
            if (g_TournamentStandings[index].score > g_TournamentStandings[otherIndex].score) {
                place++;
            }
            else {
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

void NameContestants(void) {
    s32 index;
    s32 nameOffset;
    s32 lastName;
    Boolean hasSkipped;
    UBoolean isFirstName;
    s32 blockEnd;

    index = TOURNAMENT_CONTESTANT_COUNT - 2;
    nameOffset = index;
    hasSkipped = FALSE;
    blockEnd = (g_GameState.courseIndex + 1) * NAMES_PER_COURSE;
    lastName = blockEnd - 2;
    for (; index >= 0; index--) {
        if (!hasSkipped &&
            strcmp(g_Players[0].name, g_ContestantNames[lastName - nameOffset]) == 0) {
            hasSkipped = TRUE;
            nameOffset--;
        }

        isFirstName = index == 0;
        if (hasSkipped & isFirstName) {
            g_ContestantNameIndices[index] = blockEnd - 1;
        }
        else {
            g_ContestantNameIndices[index] = lastName - nameOffset;
            nameOffset--;
        }
    }
}
