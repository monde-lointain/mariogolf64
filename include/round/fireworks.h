/*
 * The fireworks of sudden death.
 */

#ifndef FIREWORKS_H
#define FIREWORKS_H

#include <ultra64.h>

#include "system/c_language.h"

// How many fireworks f_Fireworks has room for
#define FIREWORK_CAPACITY 4

// Laid out in fireworks.c, which alone reads their fields
typedef struct FireworkShape FireworkShape;
typedef struct FireworkParticle FireworkParticle;

typedef struct Firework {
    FireworkParticle *particles;
    s32 particleCount;
    s32 timer;
    // Set to 0xFF as the fireworks load and at each launch; nothing reads it
    s32 ignoredLaunchValue;
    u8 unusedAfterLaunchValue[0x14];
    // Cleared as the fireworks load; nothing reads it
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
