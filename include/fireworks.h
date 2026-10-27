#ifndef FIREWORKS_H
#define FIREWORKS_H

#include <ultra64.h>

void StartFireworks(void);
void StopFireworks(void);
void LoadFireworks(s32 courseRecordIndex);
void FreeFireworks(void);
void UpdateAndDrawFireworks(Gfx** displayListHead);

#endif
