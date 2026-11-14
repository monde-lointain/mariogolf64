#ifndef FIREWORKS_H
#define FIREWORKS_H

#include <ultra64.h>

#include "system/c_language.h"

#define FIREWORK_CAPACITY 4

typedef struct FireworkShape FireworkShape;
typedef struct FireworkParticle FireworkParticle;

typedef struct Firework {
    FireworkParticle *particles;
    s32 particleCount;
    s32 timer;
    s32 ignoredLaunchValue;
    u8 unusedAfterLaunchValue[0x14];
    u8 ignoredLoadValue;
    UBoolean8 isActive;
    FireworkShape *shape;
} Firework;

void StartFireworks(void);
void StopFireworks(void);
void LoadFireworks(s32 courseRecordIndex);
void FreeFireworks(void);
void UpdateAndDrawFireworks(Gfx **displayListHead);

#endif
