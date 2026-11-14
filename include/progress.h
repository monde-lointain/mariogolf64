#ifndef PROGRESS_H
#define PROGRESS_H

#include <ultra64.h>

#include "language.h"

void AddBirdieBadge(void);
void RecordTournamentPlace(void);
void ReportBirdieBadgeCount(void);

void RecordComCharacterResult(s32 row, s32 column, Boolean isWin);

#endif
