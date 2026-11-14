#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include <ultra64.h>

#include "course/course.h"

#define TOURNAMENT_CONTESTANT_COUNT 30

#define PLAYER_CONTESTANT (-1)

#define PLACE_MASK 0x7F
#define PLACE_TIED 0x80

typedef struct TournamentStanding {
    s8 contestantId;
    s16 score;
    s8 holeCount;
    u8 place;
} TournamentStanding;

#define NAMES_PER_COURSE 30
#define CONTESTANT_NAME_LENGTH 13

extern TournamentStanding g_TournamentStandings[TOURNAMENT_CONTESTANT_COUNT];

extern char g_ContestantNames[COURSE_COUNT * NAMES_PER_COURSE][CONTESTANT_NAME_LENGTH];
extern s16 g_ContestantNameIndices[TOURNAMENT_CONTESTANT_COUNT - 1];

u8 RankTournament(s32 holeCount, s32 lastAheadIndex, s32 playerScore, s32 seed);

#endif
