#ifndef PASSWORD_H
#define PASSWORD_H

#include <ultra64.h>

/* The bytes a tournament password decodes to */
#define TOURNAMENT_CODE_SIZE 3

/* What a tournament password's rules index picks */
typedef struct TournamentRules {
    /* 0x0 */ s8 characterIndex; /* -1 for none; it picks a portrait */
    /* 0x1 */ s8 isSpeedGolf;    /* Otherwise stroke play */
    /* 0x2 */ s8 courseIndex;
} TournamentRules; /* size = 0x3 */

/* What a score password records of a tournament round */
typedef struct ScoreReport {
    /* 0x0 */ u8 tournamentCode[TOURNAMENT_CODE_SIZE];
    /* 0x3 */ u8 characterIndex;
    /* 0x4 */ u16 score;
    /* 0x8 */ u32 time;
    /* 0xC */ u8 unkC; /* Byte 0x1B of the player's Game Boy character */
} ScoreReport; /* size = 0x10 */

/* It returns 0 for a valid password and -1 otherwise */
s32 DecodeTournamentPassword(u8 *code, char *password);
void EncodeScorePassword(ScoreReport *report, char *password);
TournamentRules *GetTournamentRules(u8 rulesIndex);

#endif
