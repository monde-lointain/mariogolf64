#ifndef PROGRESS_H
#define PROGRESS_H

#include <ultra64.h>

#include "language.h"

/* The player's progress in the save: wins against the characters, birdie
 * badges and tournament places */

void AddBirdieBadge(void);
void RecordTournamentPlace(void);
void ReportBirdieBadgeCount(void);

void RecordComCharacterResult(s32 row, s32 column, Boolean isWin);

#endif
