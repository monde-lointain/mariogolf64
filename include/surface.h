#ifndef SURFACE_H
#define SURFACE_H

#include <ultra64.h>

/* Indexes into the surface table. Names with UNKNN share another's kind or
 * name. */
enum Surface {
  SURFACE_OUT_OF_BOUNDS,
  SURFACE_FAIRWAY,
  SURFACE_ROUGH,
  SURFACE_BUNKER,
  SURFACE_GREEN,
  SURFACE_WATER_HAZARD,
  SURFACE_CART_WAY,
  SURFACE_TEE_GROUND,
  SURFACE_ROCK,
  SURFACE_GREEN_EDGE,
  SURFACE_SEMI_ROUGH,
  SURFACE_BARE_GROUND,
  SURFACE_DEEP_ROUGH,
  SURFACE_BARE_GROUND_UNK13,
  SURFACE_WASTE_AREA,
  SURFACE_UNK15, /* A nameless rock */
  SURFACE_BUNKER_FAIR_LIE,
  SURFACE_BUNKER_POOR_LIE,
  SURFACE_BUNKER_UNK18,
  SURFACE_FAIRWAY_FAIR_LIE,
  SURFACE_FAIRWAY_POOR_LIE,
  SURFACE_ROUGH_POOR_LIE,
  SURFACE_OUT_OF_COURSE,
  SURFACE_TEE_UP,
  /* The surfaces a mini-golf course has in place of the green, fairway and cart
   * way */
  SURFACE_MINI_GOLF_GREEN,
  SURFACE_MINI_GOLF_FAIRWAY,
  SURFACE_MINI_GOLF_CART_WAY,
  /* Surfaces past the last are looked up as the first */
  SURFACE_COUNT
};

/* An enum Surface value; fields kept narrower say so in comments */
typedef s32 Surface;

/*
 * Kinds that group the surfaces; several surfaces share one. A surface's
 * plain kind is its best lie; its FAIR_LIE and POOR_LIE kinds get lower lie
 * ratings, and their lie images show the ball sitting worse. Kinds named
 * UNKNN share their surface's name with another kind.
 */
enum SurfaceKind {
  SURFACE_KIND_FAIRWAY,
  SURFACE_KIND_FAIRWAY_FAIR_LIE, /* In a tuft of grass */
  SURFACE_KIND_FAIRWAY_POOR_LIE, /* In a divot */
  SURFACE_KIND_ROUGH,
  SURFACE_KIND_ROUGH_POOR_LIE, /* Deeper in the grass */
  SURFACE_KIND_BUNKER = 0x6,
  SURFACE_KIND_BUNKER_FAIR_LIE, /* Half buried */
  SURFACE_KIND_BUNKER_POOR_LIE, /* Mostly buried */
  /* Has BUNKER_POOR_LIE's image, but the default ratings and clubs */
  SURFACE_KIND_BUNKER_UNK9,
  SURFACE_KIND_WATER_HAZARD,
  SURFACE_KIND_GREEN,
  SURFACE_KIND_GREEN_EDGE,
  SURFACE_KIND_OUT_OF_BOUNDS,
  SURFACE_KIND_CART_WAY,
  SURFACE_KIND_TEE_GROUND,
  SURFACE_KIND_ROCK,
  SURFACE_KIND_SEMI_ROUGH,
  SURFACE_KIND_BARE_GROUND,
  SURFACE_KIND_DEEP_ROUGH,
  SURFACE_KIND_BARE_GROUND_UNK20,
  SURFACE_KIND_OUT_OF_COURSE,
  SURFACE_KIND_TEE_UP,
  SURFACE_KIND_WASTE_AREA
};

/* An enum SurfaceKind value */
typedef s32 SurfaceKind;

typedef struct SurfaceInfo {
  /* 0x00 */ SurfaceKind kind;
  /* 0x04 */ char* name; /* As the lie is shown */
  /* 0x08 */ s8 minPercent;
  /* 0x09 */ s8 maxPercent;
  /* 0x0A */ s8 wrongClubPenalty; /* Taken off both percents for a club not
                                     meant for the surface */
  /* 0x0C */ s16 unkC;
  /* 0x0E */ s16 unkE;
  /* 0x10 */ s32 unk10;
  /* 0x14 */ s32 unk14;
  /* 0x18 */ s32 unk18;
} SurfaceInfo; /* size = 0x1C */

SurfaceInfo* GetSurfaceInfo(Surface surface);
SurfaceInfo* GetSurfaceInfoAt(s32 x, s32 z);

#endif
