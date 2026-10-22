#ifndef SURFACE_H
#define SURFACE_H

#include <ultra64.h>

/* Surfaces past the last are looked up as the first */
#define SURFACE_COUNT 27

/* Kinds that group the surfaces; several surfaces share one */
#define SURFACE_KIND_WATER_HAZARD 0xA
#define SURFACE_KIND_GREEN 0xB
#define SURFACE_KIND_OUT_OF_BOUNDS 0xD
#define SURFACE_KIND_OUT_OF_COURSE 0x15

typedef struct SurfaceInfo {
    /* 0x00 */ s32 kind;
    /* 0x04 */ char *name; /* As the lie is shown */
    /* 0x08 */ u8 unk8[0x1C - 0x8];
} SurfaceInfo; /* size = 0x1C */

SurfaceInfo *GetSurfaceInfo(s32 surface);

#endif
