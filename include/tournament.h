#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include <ultra64.h>

/* The player and 29 computer contestants */
#define TOURNAMENT_CONTESTANT_COUNT 30

/* The contestantId of the player's standing */
#define PLAYER_CONTESTANT (-1)

/* TournamentStanding.place holds a place from 1 and this bit for a tie */
#define PLACE_MASK 0x7F
#define PLACE_TIED 0x80

typedef struct TournamentStanding {
    /* 0x0 */ s8 contestantId;
    /* 0x1 */ u8 unk1;
    /* 0x2 */ s16 score; /* Strokes over par, so lower ranks higher */
    /* 0x4 */ s8 holeCount;
    /* 0x5 */ u8 place;
} TournamentStanding; /* size = 0x6 */

/* From best to worst once RankTournament has run */
extern TournamentStanding g_TournamentStandings[TOURNAMENT_CONTESTANT_COUNT];

u8 RankTournament(s32 holeCount, s32 lastAheadIndex, s32 playerScore, s32 seed);

#endif
