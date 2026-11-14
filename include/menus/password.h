#ifndef PASSWORD_H
#define PASSWORD_H

#include <ultra64.h>

#include "characters/character.h"
#include "course/course.h"
#include "system/c_language.h"

#define TOURNAMENT_CODE_SIZE 3

#define TOURNAMENT_PASSWORD_LENGTH 8
#define SCORE_PASSWORD_LENGTH 20

#define ANY_CHARACTER (-1)

typedef struct TournamentRules {
    CharacterIndex8 characterIndex;
    Boolean8 isSpeedGolf;
    CourseIndex8 courseIndex;
} TournamentRules;

typedef struct ScoreReport {
    u8 tournamentCode[TOURNAMENT_CODE_SIZE];
    UCharacterIndex8 characterIndex;
    u16 score;
    u32 time;
    u8 gameBoyLevel;
} ScoreReport;

Outcome DecodeTournamentPassword(u8 *code, char *password);
void EncodeScorePassword(ScoreReport *report, char *password);
TournamentRules *GetTournamentRules(u8 rulesIndex);

extern TournamentRules g_ClubTournamentRules;
void CopyTournamentCode(u8 *destination);

#endif
