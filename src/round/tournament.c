/*
 * The tournament's computer contestants: their random numbers, simulating their
 * scores, ranking the field and naming them.
 */

#include "round/tournament.h"

#include <math.h>
#include <string.h>
#include <ultra64.h>

#include "course/course.h"
#include "math/math_util.h"
#include "menus/menu.h"
#include "round/game_state.h"
#include "round/player.h"
#include "system/c_language.h"
#include "system/compiler.h"

// Standing 0 is the player's, so the computer contestants start at 1
#define FIRST_COMPUTER_CONTESTANT 1

// A computer contestant's final score stays within this many strokes of par
#define SCORE_LIMIT 18

// Half a sine wave over a round
#define RADIANS_PER_HOLE ((f64)(3.1416f / COURSE_HOLE_COUNT))

typedef struct ContestantProfile {
    s8 baseScore; // Final strokes over par on the first course
    u8 unusedBeforeHoleOffset[2];
    s8 holeOffset; // Holes played ahead of the player
} ContestantProfile;

s16 g_ContestantNameIndices[TOURNAMENT_CONTESTANT_COUNT - 1];
TournamentStanding g_TournamentStandings[TOURNAMENT_CONTESTANT_COUNT];

static s32 f_TournamentSeed = 0;
// The computer contestants', from the most strokes over par to the fewest
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
// NAMES_PER_COURSE names for each course in turn, spelled with the top bit set
// on each lowercase letter. A name breaks into two strings where a capital
// that is also a hex digit follows an escape, which would otherwise absorb it.
char g_ContestantNames[COURSE_COUNT * NAMES_PER_COURSE][CONTESTANT_NAME_LENGTH] = {
    "T\xEF\xE1\xE4", // Toad
    "K\xE5\xEE", // Ken
    "K\xEF\xE2\xE5", // Kobe
    "U\xEB\xEB\xE9\xEB\xE9", // Ukkiki
    "B\xEF\xE2-\xEF\xED\xE2", // Bob-omb
    "M\xEF\xEE\xF4\xF9_M\xEF\xEC\xE5", // Monty_Mole
    "F\xEC\xF9_G\xF5\xF9", // Fly_Guy
    "M\xE9\xEB\xE5", // Mike
    "G\xE1\xE9\xEC", // Gail
    "C\xEF\xEC\xE9\xEE", // Colin
    "B\xE1\xE2\xF9_M\xE1\xF2\xE9\xEF", // Baby_Mario
    "G\xF2\xE5\xE7", // Greg
    "C\xE8\xF2\xE9\xF3\xF4\xE9\xE1\xEE", // Christian
    "B\xE9\xEC\xEC", // Bill
    "P\xE5\xE1\xE3\xE8", // Peach
    "M\xE9\xE3\xE8\xE5\xEC\xEC\xE5", // Michelle
    "P\xE8\xE9\xEC", // Phil
    "C\xE8\xE1\xF2\xEC\xE9\xE5", // Charlie
    "L\xE9\xEE\xEB", // Link
    "S\xE8\xE5\xE9\xEB", // Sheik
    "P\xEC\xF5\xED", // Plum
    "D\xE1\xF2\xF5\xEE\xE9\xE1", // Darunia
    "D\xEF\xF2\xE9\xE5", // Dorie
    "F\xEF\xF8", // Fox
    "E\xF2\xE9\xE3\xE8", // Erich
    "S\xE1\xF2\xE9\xE1", // Saria
    "M\xE1\xEC\xEF\xEE", // Malon
    "M\xE3"
    "C\xEC\xEF\xF5\xE4", // McCloud
    "B\xF5\xE2", // Bub
    "P\xE5\xF0\xF0\xF9", // Peppy
    "K\xEF\xEF\xF0\xE1", // Koopa
    "B\xF5\xE2\xE2\xE1", // Bubba
    "L\xE1\xF2\xF2\xF9", // Larry
    "Y\xEF\xF3\xE8\xE9", // Yoshi
    "T\xEF\xE1\xE4", // Toad
    "K\xE5\xEE", // Ken
    "U\xEB\xEB\xE9\xEB\xE9", // Ukkiki
    "B\xEF\xE2-\xEF\xED\xE2", // Bob-omb
    "M\xEF\xEE\xF4\xF9_M\xEF\xEC\xE5", // Monty_Mole
    "L\xF5\xE9\xE7\xE9", // Luigi
    "F\xEC\xF9_G\xF5\xF9", // Fly_Guy
    "C\xE8\xE5\xE5\xF0_C\xE8\xE5\xE5\xF0", // Cheep_Cheep
    "C\xE8\xE9\xEC\xEC_B\xF5\xEC\xEC\xF9", // Chill_Bully
    "B\xE9\xE7_P\xE5\xEE\xE7\xF5\xE9\xEE", // Big_Penguin
    "B\xE1\xE2\xF9_M\xE1\xF2\xE9\xEF", // Baby_Mario
    "H\xE5\xEE\xF2\xF9", // Henry
    "A\xF2\xED\xEF\xEE\xE4", // Armond
    "D\xE1\xF6\xE9\xE4", // David
    "F\xE1\xF2\xE1\xEE", // Faran
    "E\xE4", // Ed
    "T\xEF\xED", // Tom
    "M\xE5\xF4\xE1\xEC_M\xE1\xF2\xE9\xEF", // Metal_Mario
    "W\xE9\xE7\xE7\xEC\xE5\xF2", // Wiggler
    "T\xE1\xEC\xEF\xEE", // Talon
    "R\xE5\xE4_B\xE5\xE5\xFA\xEF", // Red_Beezo
    "L\xE5\xED\xED\xF9", // Lemmy
    "S\xF0\xE9\xEB\xE5", // Spike
    "L\xF5\xE4\xF7\xE9\xE7", // Ludwig
    "M\xEF\xF2\xF4\xEF\xEE", // Morton
    "R\xF5\xF4\xEF\xEF\xEF", // Rutooo
    "S\xE8\xF9_G\xF5\xF9", // Shy_Guy
    "W\xE8\xEF\xED\xF0", // Whomp
    "Z\xE5\xEC\xE4\xE1", // Zelda
    "M\xE1\xF0\xEC\xE5", // Maple
    "G\xEF\xEF\xED\xE2\xE1", // Goomba
    "S\xEF\xEE\xEE\xF9", // Sonny
    "K\xEF\xEF\xF0\xE1", // Koopa
    "L\xE1\xEB\xE9\xF4\xF5", // Lakitu
    "L\xE1\xF2\xF2\xF9", // Larry
    "B\xF5\xFA\xFA\xF9", // Buzzy
    "Y\xEF\xF3\xE8\xE9", // Yoshi
    "S\xF0\xE9\xEE\xF9", // Spiny
    "B\xEF\xE2-\xEF\xED\xE2", // Bob-omb
    "L\xF5\xE9\xE7\xE9", // Luigi
    "F\xEC\xF9_G\xF5\xF9", // Fly_Guy
    "C\xE8\xE5\xE5\xF0_C\xE8\xE5\xE5\xF0", // Cheep_Cheep
    "C\xE8\xE9\xEC\xEC_B\xF5\xEC\xEC\xF9", // Chill_Bully
    "B\xE9\xE7_P\xE5\xEE\xE7\xF5\xE9\xEE", // Big_Penguin
    "B\xE1\xE2\xF9_M\xE1\xF2\xE9\xEF", // Baby_Mario
    "R\xE5\xF8", // Rex
    "P\xE9\xE4\xE7\xE9\xF4", // Pidgit
    "W\xE5\xEE\xE4\xF9_O.", // Wendy_O.
    "G\xEF\xF2\xEF\xEE_K\xE9\xE4", // Goron_Kid
    "F\xE1\xEC\xE3\xEF_L.", // Falco_L.
    "I\xEE\xF0\xE1", // Inpa
    "D\xE5\xEB\xF5_S\xE3\xF2\xF5\xE2", // Deku_Scrub
    "P\xE9\xE7\xED\xE1", // Pigma
    "N\xE1\xF6\xE9", // Navi
    "S\xE1\xF2\xE9\xE1", // Saria
    "S\xE8\xE5\xE9\xEB", // Sheik
    "U\xEE\xE1\xE7\xE9", // Unagi
    "B\xF5\xE2\xE2\xE1", // Bubba
    "S\xEB\xF5\xEC\xEC_K\xE9\xE4", // Skull_Kid
    "S\xEC\xE9\xF0\xF0\xF9", // Slippy
    "H\xE1\xF2\xF2\xF9", // Harry
    "P\xE1\xF2\xE1\xF4\xF2\xEF\xEF\xF0\xE1", // Paratroopa
    "C\xE8\xE1\xE9\xEE_C\xE8\xEF\xED\xF0", // Chain_Chomp
    "P\xEF\xE4\xEF\xE2\xEF\xEF", // Podoboo
    "W\xE1\xF2\xE9\xEF", // Wario
    "S\xE8\xF9_G\xF5\xF9", // Shy_Guy
    "R\xE1\xF5\xF2\xF5", // Rauru
    "M\xE1\xF0\xEC\xE5", // Maple
    "N\xE1\xE2\xEF\xEF\xF2\xF5", // Nabooru
    "S\xEF\xEE\xEE\xF9", // Sonny
    "B\xE9\xE7_M\xF2._I.", // Big_Mr._I.
    "S\xE3\xF5\xF4\xF4\xEC\xE5_B\xF5\xE7", // Scuttle_Bug
    "B\xF5\xFA\xFA\xF9", // Buzzy
    "Y\xEF\xF3\xE8\xE9", // Yoshi
    "S\xF0\xE9\xEE\xF9", // Spiny
    "C\xE8\xE1\xF2\xEC\xE5\xF3", // Charles
    "L\xF5\xE9\xE7\xE9", // Luigi
    "J\xE5\xEE", // Jen
    "D\xE5\xF8", // Dex
    "H\xE1\xEC\xE5\xF9", // Haley
    "B\xE1\xE2\xF9_M\xE1\xF2\xE9\xEF", // Baby_Mario
    "T\xE5\xF2\xF2\xF9", // Terry
    "M\xE9\xEB\xE5", // Mike
    "D\xE1\xF6\xE9\xE4", // David
    "P\xE5\xEE\xEE", // Penn
    "K\xE1\xF4\xE5", // Kate
    "B\xEF\xEF", // Boo
    "P\xE1\xF2\xE1\xF4\xF2\xEF\xEF\xF0\xE1", // Paratroopa
    "C\xE8\xE1\xE9\xEE_C\xE8\xEF\xED\xF0", // Chain_Chomp
    "P\xEF\xE4\xEF\xE2\xEF\xEF", // Podoboo
    "W\xE1\xF2\xE9\xEF", // Wario
    "S\xE8\xF9_G\xF5\xF9", // Shy_Guy
    "M\xE1\xF0\xEC\xE5", // Maple
    "Z\xE5\xEC\xE4\xE1", // Zelda
    "S\xF5\xED\xEF_B\xF2\xEF\xF3.", // Sumo_Bros.
    "S\xEF\xEE\xEE\xF9", // Sonny
    "S\xE3\xF5\xF4\xF4\xEC\xE5_B\xF5\xE7", // Scuttle_Bug
    "J\xE5\xF3\xF3\xE9\xE3\xE1", // Jessica
    "B\xF5\xFA\xFA\xF9", // Buzzy
    "Y\xEF\xF3\xE8\xE9", // Yoshi
    "S\xF0\xE9\xEE\xF9", // Spiny
    "B\xEF\xE2-\xEF\xED\xE2", // Bob-omb
    "L\xF5\xE9\xE7\xE9", // Luigi
    "F\xEC\xF9_G\xF5\xF9", // Fly_Guy
    "C\xE8\xE5\xE5\xF0_C\xE8\xE5\xE5\xF0", // Cheep_Cheep
    "C\xE8\xE9\xEC\xEC_B\xF5\xEC\xEC\xF9", // Chill_Bully
    "B\xE1\xE2\xF9_M\xE1\xF2\xE9\xEF", // Baby_Mario
    "M\xE1\xF2\xEB", // Mark
    "T\xE9\xED", // Tim
    "M\xE9\xE3\xE8\xE1\xE5\xEC", // Michael
    "C\xE8\xF5\xE3\xEB\xF9\xE1", // Chuckya
    "S\xF4\xE1\xF2_W\xEF\xEC\xE6", // Star_Wolf
    "S\xEC\xE5\xE4\xE7\xE5_B\xF2\xEF\xF3.", // Sledge_Bros.
    "D\xE1\xED\xF0\xE5", // Dampe
    "P\xE9\xE7\xED\xE1", // Pigma
    "A\xEE\xE4\xF2\xEF\xF3\xF3", // Andross
    "B\xEF\xF7\xF3\xE5\xF2", // Bowser
    "DK", // DK
    "M\xE1\xF2\xE9\xEF", // Mario
    "G\xEF\xEF\xED\xE2\xE1", // Goomba
    "U\xEE\xE1\xE7\xE9", // Unagi
    "B\xE9\xE7_B\xEF\xE2-\xEF\xED\xE2", // Big_Bob-omb
    "A\xEE\xE4\xF9", // Andy
    "S\xEC\xE9\xF0\xF0\xF9", // Slippy
    "H\xE1\xF2\xF2\xF9", // Harry
    "B\xEF\xEF", // Boo
    "B\xEC\xEF\xEF\xE2\xE5\xF2", // Bloober
    "F\xE9\xF2\xE5_B\xF2\xEF\xF3.", // Fire_Bros.
    "W\xE1\xF2\xE9\xEF", // Wario
    "H\xE1\xED\xED\xE5\xF2_B\xF2\xEF\xF3.", // Hammer_Bros.
    "S\xEE\xE9\xE6\xE9\xF4", // Snifit
    "M\xE1\xF0\xEC\xE5", // Maple
    "S\xEF\xEE\xEE\xF9", // Sonny
    "S\xF5\xED\xEF_B\xF2\xEF\xF3.", // Sumo_Bros.
    "B\xE1\xE2\xF9_B\xEF\xF7\xF3\xE5\xF2", // Baby_Bowser
    "M\xF2._I.", // Mr._I.
    "Y\xEF\xF3\xE8\xE9", // Yoshi
    "S\xF0\xE9\xEE\xF9", // Spiny
    "T\xE1\xEC\xEF\xEE", // Talon
    "B\xEF\xEE\xEF\xEF\xF2\xF5", // Bonooru
    "P\xE9\xE5\xF2\xF2\xE5", // Pierre
    "L\xF5\xE9\xE7\xE9", // Luigi
    "K\xE9\xEE\xE7_Z\xEF\xF2\xE1", // King_Zora
    "P\xE1\xF5\xEC\xE9\xEE\xE5", // Pauline
    "I\xEE\xE7\xEF", // Ingo
    "M\xE9\xE4\xEF", // Mido
};
static s32 f_UnusedWordAfterContestantProfiles UNUSED = 0;

void NameContestants(void);

void SeedTournamentRandom(s32 seed) {
    f_TournamentSeed = seed;
}

u32 NextTournamentRandom(void) {
    return f_TournamentSeed = NextRandomSeed(f_TournamentSeed);
}

// How a contestant's score to par moves toward its final score over the
// holes: evenly, or swinging above or below the even line along a sine
enum ScoreShape {
    SCORE_SHAPE_EVEN,
    SCORE_SHAPE_SWING_ABOVE,
    SCORE_SHAPE_SWING_BELOW,
    SCORE_SHAPE_COUNT
};

#define SWING_AMPLITUDE_COUNT 10
#define EARLY_COURSE_SWING_REVERSAL 13

// Makes up the scores of contestants firstIndex to endIndex - 1 after the
// player's holeCount holes; those up to lastAheadIndex, or all of them if it
// is negative, are a hole further on. playerScore goes unused.
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

// Simulates the computer contestants, ranks everyone by score and returns
// the player's place
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

// Gives the computer contestants the course's names, skipping any that
// matches the first player's record and taking the block's last name in
// its place
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
