/*
 * The score and tournament passwords.
 */

#ifndef PASSWORD_H
#define PASSWORD_H

#include <ultra64.h>

#include "characters/character.h"
#include "course/course.h"
#include "system/c_language.h"

// The bytes a tournament password decodes to
#define TOURNAMENT_CODE_SIZE 3

// The characters in each password, which has no terminating null. A score
// password holds three numbers of five symbols, and five symbols of their
// checksums.
#define TOURNAMENT_PASSWORD_LENGTH 8
#define SCORE_PASSWORD_LENGTH 20
#define ANY_CHARACTER (-1)

// What a tournament password's rules index picks
typedef struct TournamentRules {
    CharacterIndex8 characterIndex; // -1 for none; it picks a portrait
    Boolean8 isSpeedGolf; // Otherwise stroke play
    CourseIndex8 courseIndex;
} TournamentRules;

// What a score password records of a tournament round
typedef struct ScoreReport {
    u8 tournamentCode[TOURNAMENT_CODE_SIZE];
    UCharacterIndex8 characterIndex;
    u16 score;
    u32 time;
    u8 gameBoyLevel; // Of the player's Game Boy character
} ScoreReport;

// It returns 0 for a valid password and -1 otherwise
Outcome DecodeTournamentPassword(u8 *code, char *password);
void EncodeScorePassword(ScoreReport *report, char *password);
TournamentRules *GetTournamentRules(u8 rulesIndex);

// The rules of the club tournament whose code was entered
extern TournamentRules g_ClubTournamentRules;
void CopyTournamentCode(u8 *destination);

#endif
