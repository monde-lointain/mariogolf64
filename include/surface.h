#ifndef SURFACE_H
#define SURFACE_H

#include <ultra64.h>

#include "asset.h"

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
    SURFACE_NO_DROP_GROUND,
    SURFACE_WASTE_AREA,
    SURFACE_UNNAMED_ROCK,
    SURFACE_BUNKER_FAIR_LIE,
    SURFACE_BUNKER_POOR_LIE,
    SURFACE_BUNKER_UNUSED_LIE,
    SURFACE_FAIRWAY_FAIR_LIE,
    SURFACE_FAIRWAY_POOR_LIE,
    SURFACE_ROUGH_POOR_LIE,
    SURFACE_OUT_OF_COURSE,
    SURFACE_TEE_UP,
    SURFACE_MINI_GOLF_GREEN,
    SURFACE_MINI_GOLF_FAIRWAY,
    SURFACE_MINI_GOLF_CART_WAY,
    SURFACE_COUNT
};

typedef s32 Surface;
typedef s8 Surface8;

enum SurfaceKind {
    SURFACE_KIND_FAIRWAY,
    SURFACE_KIND_FAIRWAY_FAIR_LIE,
    SURFACE_KIND_FAIRWAY_POOR_LIE,
    SURFACE_KIND_ROUGH,
    SURFACE_KIND_ROUGH_POOR_LIE,
    SURFACE_KIND_ROUGH_UNUSED_LIE,
    SURFACE_KIND_BUNKER,
    SURFACE_KIND_BUNKER_FAIR_LIE,
    SURFACE_KIND_BUNKER_POOR_LIE,
    SURFACE_KIND_BUNKER_UNUSED_LIE,
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
    SURFACE_KIND_NO_DROP_GROUND,
    SURFACE_KIND_OUT_OF_COURSE,
    SURFACE_KIND_TEE_UP,
    SURFACE_KIND_WASTE_AREA
};

typedef s32 SurfaceKind;

#define TEE_HEIGHT 307.2f

#define IsOutOfBoundsKind(kind) \
    ((kind) == SURFACE_KIND_OUT_OF_BOUNDS || (kind) == SURFACE_KIND_OUT_OF_COURSE)
#define IsLostBallKind(kind) \
    ((kind) == SURFACE_KIND_WATER_HAZARD || \
        (kind) == SURFACE_KIND_OUT_OF_BOUNDS || \
        (kind) == SURFACE_KIND_OUT_OF_COURSE)
#define IsFairwayKind(kind) \
    ((kind) == SURFACE_KIND_FAIRWAY || \
        (kind) == SURFACE_KIND_FAIRWAY_FAIR_LIE || \
        (kind) == SURFACE_KIND_FAIRWAY_POOR_LIE)
#define IsBunkerKind(kind) \
    ((kind) == SURFACE_KIND_BUNKER || \
        (kind) == SURFACE_KIND_BUNKER_FAIR_LIE || \
        (kind) == SURFACE_KIND_BUNKER_POOR_LIE)

enum SinkDepth {
    SINK_DEPTH_NONE,
    SINK_DEPTH_HALF,
    SINK_DEPTH_BURIED
};

typedef s32 SinkDepth;

typedef struct SurfaceTraits {
    SurfaceKind kind;
    char *name;
    s8 percentMin;
    s8 percentMax;
    s8 wrongClubPenalty;
    s16 rollSpeedKept;
    s16 bounciness;
    WideAssetId lieImageAsset;
    SinkDepth sinkDepth;
    s32 ignoredSurfaceGroup;
} SurfaceTraits;

SurfaceTraits *GetSurfaceTraits(Surface surface);
SurfaceTraits *GetSurfaceTraitsAt(s32 x, s32 z);

#endif
