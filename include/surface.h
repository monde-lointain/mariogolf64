#ifndef SURFACE_H
#define SURFACE_H

#include <ultra64.h>

/* Indexes into the surface table. Names with UNKNN share another's kind or name. */
#define SURFACE_OUT_OF_BOUNDS 0
#define SURFACE_FAIRWAY 1
#define SURFACE_ROUGH 2
#define SURFACE_BUNKER 3
#define SURFACE_GREEN 4
#define SURFACE_WATER_HAZARD 5
#define SURFACE_CART_WAY 6
#define SURFACE_TEE_GROUND 7
#define SURFACE_ROCK 8
#define SURFACE_GREEN_EDGE 9
#define SURFACE_SEMI_ROUGH 10
#define SURFACE_BARE_GROUND 11
#define SURFACE_DEEP_ROUGH 12
#define SURFACE_BARE_GROUND_UNK13 13
#define SURFACE_WASTE_AREA 14
#define SURFACE_UNK15 15 /* A nameless rock */
#define SURFACE_BUNKER_FAIR_LIE 16
#define SURFACE_BUNKER_POOR_LIE 17
#define SURFACE_BUNKER_UNK18 18
#define SURFACE_FAIRWAY_FAIR_LIE 19
#define SURFACE_FAIRWAY_POOR_LIE 20
#define SURFACE_ROUGH_POOR_LIE 21
#define SURFACE_OUT_OF_COURSE 22
#define SURFACE_TEE_UP 23
/* The surfaces a mini-golf course has in place of the green, fairway and cart way */
#define SURFACE_MINI_GOLF_GREEN 24
#define SURFACE_MINI_GOLF_FAIRWAY 25
#define SURFACE_MINI_GOLF_CART_WAY 26
/* Surfaces past the last are looked up as the first */
#define SURFACE_COUNT 27

/*
 * Kinds that group the surfaces; several surfaces share one. A surface's
 * plain kind is its best lie; its FAIR_LIE and POOR_LIE kinds get lower lie
 * ratings, and their lie images show the ball sitting worse. Kinds named
 * UNKNN share their surface's name with another kind.
 */
#define SURFACE_KIND_FAIRWAY 0x0
#define SURFACE_KIND_FAIRWAY_FAIR_LIE 0x1 /* In a tuft of grass */
#define SURFACE_KIND_FAIRWAY_POOR_LIE 0x2 /* In a divot */
#define SURFACE_KIND_ROUGH 0x3
#define SURFACE_KIND_ROUGH_POOR_LIE 0x4 /* Deeper in the grass */
#define SURFACE_KIND_BUNKER 0x6
#define SURFACE_KIND_BUNKER_FAIR_LIE 0x7 /* Half buried */
#define SURFACE_KIND_BUNKER_POOR_LIE 0x8 /* Mostly buried */
#define SURFACE_KIND_BUNKER_UNK9 0x9 /* Has BUNKER_POOR_LIE's image, but the default ratings and clubs */
#define SURFACE_KIND_WATER_HAZARD 0xA
#define SURFACE_KIND_GREEN 0xB
#define SURFACE_KIND_GREEN_EDGE 0xC
#define SURFACE_KIND_OUT_OF_BOUNDS 0xD
#define SURFACE_KIND_CART_WAY 0xE
#define SURFACE_KIND_TEE_GROUND 0xF
#define SURFACE_KIND_ROCK 0x10
#define SURFACE_KIND_SEMI_ROUGH 0x11
#define SURFACE_KIND_BARE_GROUND 0x12
#define SURFACE_KIND_DEEP_ROUGH 0x13
#define SURFACE_KIND_BARE_GROUND_UNK20 0x14
#define SURFACE_KIND_OUT_OF_COURSE 0x15
#define SURFACE_KIND_TEE_UP 0x16
#define SURFACE_KIND_WASTE_AREA 0x17

typedef struct SurfaceInfo {
    /* 0x00 */ s32 kind;
    /* 0x04 */ char *name; /* As the lie is shown */
    /* 0x08 */ s8 minPercent;
    /* 0x09 */ s8 maxPercent;
    /* 0x0A */ s8 wrongClubPenalty; /* Taken off both percents for a club not meant for the surface */
    /* 0x0C */ s16 unkC;
    /* 0x0E */ s16 unkE;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
} SurfaceInfo; /* size = 0x1C */

SurfaceInfo *GetSurfaceInfo(s32 surface);
SurfaceInfo *GetSurfaceInfoAt(s32 x, s32 z);

#endif
