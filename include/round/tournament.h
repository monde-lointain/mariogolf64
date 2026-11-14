/*
 * The tournament's computer contestants and their scores.
 */

#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include <ultra64.h>

#include "course/course.h"

// The player and 29 computer contestants
#define TOURNAMENT_CONTESTANT_COUNT 30

// The contestantId of the player's standing
#define PLAYER_CONTESTANT (-1)

// TournamentStanding.place holds a place from 1 and this bit for a tie
#define PLACE_MASK 0x7F
#define PLACE_TIED 0x80

typedef struct TournamentStanding {
    s8 contestantId;
    s16 score; // Strokes over par, so lower ranks higher
    s8 holeCount;
    u8 place;
} TournamentStanding;

// Each course's tournament draws its computer contestants from its own block of
// names
#define NAMES_PER_COURSE 30
#define CONTESTANT_NAME_LENGTH 13

// From best to worst once RankTournament has run
extern TournamentStanding g_TournamentStandings[TOURNAMENT_CONTESTANT_COUNT];

extern char g_ContestantNames[COURSE_COUNT * NAMES_PER_COURSE][CONTESTANT_NAME_LENGTH];
// Into g_ContestantNames, for the computer contestants
extern s16 g_ContestantNameIndices[TOURNAMENT_CONTESTANT_COUNT - 1];

u8 RankTournament(s32 holeCount, s32 lastAheadIndex, s32 playerScore, s32 seed);

#endif
