#ifndef FIREWORKS_H
#define FIREWORKS_H

#include <ultra64.h>

/* How many fireworks f_Fireworks has room for */
#define FIREWORK_CAPACITY 4

/* Laid out in fireworks.c, which alone reads their fields */
typedef struct FireworkShape FireworkShape;
typedef struct FireworkParticle FireworkParticle;

typedef struct Firework {
    /* 0x00 */ FireworkParticle *particles;
    /* 0x04 */ s32 particleCount;
    /* 0x08 */ s32 timer;
    /* Set to 0xFF as the fireworks load and at each launch; nothing reads it */
    /* 0x0C */ s32 ignoredLaunchValue;
    /* 0x10 */ u8 unusedAfterLaunchValue[0x14];
    /* Cleared as the fireworks load; nothing reads it */
    /* 0x24 */ u8 ignoredLoadValue;
    /* 0x25 */ u8 isActive;
    /* 0x28 */ FireworkShape *shape;
} Firework; /* size = 0x2C */

void StartFireworks(void);
void StopFireworks(void);
void LoadFireworks(s32 courseRecordIndex);
void FreeFireworks(void);
void UpdateAndDrawFireworks(Gfx **displayListHead);

#endif
