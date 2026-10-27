#ifndef BURST_H
#define BURST_H

#include <ultra64.h>

void StartBursts(void);
void StopBursts(void);
void LoadBursts(s32 courseRecordIndex);
void FreeBursts(void);
void UpdateAndDrawBursts(Gfx** displayListHead);

#endif
