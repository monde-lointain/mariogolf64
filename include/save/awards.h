/*
 * The player's progress in the save: wins against the characters, birdie
 * badges and tournament places
 */

#ifndef AWARDS_H
#define AWARDS_H

#include <ultra64.h>

#include "system/c_language.h"

void AddBirdieBadge(void);
void RecordTournamentPlace(void);
void ReportBirdieBadgeCount(void);

void RecordComputerMatchupResult(s32 rowCharacter, s32 columnCharacter, Boolean isWin);

#endif
