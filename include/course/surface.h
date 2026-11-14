/*
 * The surfaces of the course, the kinds that group them, and how each affects
 * the ball and the shot.
 */

#ifndef SURFACE_H
#define SURFACE_H

#include <ultra64.h>

#include "system/asset.h"

// Indexes into the surface table. Names with UNKNN share another's kind or
// name.
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
    // A rock whose name is blank, with a lie as good as the green edge's
    SURFACE_UNNAMED_ROCK,
    SURFACE_BUNKER_FAIR_LIE,
    SURFACE_BUNKER_POOR_LIE,
    SURFACE_BUNKER_UNUSED_LIE,
    SURFACE_FAIRWAY_FAIR_LIE,
    SURFACE_FAIRWAY_POOR_LIE,
    SURFACE_ROUGH_POOR_LIE,
    SURFACE_OUT_OF_COURSE,
    SURFACE_TEE_UP,
    // The surfaces a mini-golf course has in place of the green, fairway and cart
    // way
    SURFACE_MINI_GOLF_GREEN,
    SURFACE_MINI_GOLF_FAIRWAY,
    SURFACE_MINI_GOLF_CART_WAY,
    // Surfaces past the last are looked up as the first
    SURFACE_COUNT
};

// An enum Surface value; fields kept narrower say so in comments
typedef s32 Surface;
typedef s8 Surface8;

// Kinds that group the surfaces; several surfaces share one. A surface's
// plain kind is its best lie; its FAIR_LIE and POOR_LIE kinds get lower lie
// ratings, and their lie images show the ball sitting worse. Kinds named
// UNKNN share their surface's name with another kind.
enum SurfaceKind {
    SURFACE_KIND_FAIRWAY,
    SURFACE_KIND_FAIRWAY_FAIR_LIE, // In a tuft of grass
    SURFACE_KIND_FAIRWAY_POOR_LIE, // In a divot
    SURFACE_KIND_ROUGH,
    SURFACE_KIND_ROUGH_POOR_LIE, // Deeper in the grass
    // No surface has it, but the code treats it as rough
    SURFACE_KIND_ROUGH_UNUSED_LIE,
    SURFACE_KIND_BUNKER,
    SURFACE_KIND_BUNKER_FAIR_LIE, // Half buried
    SURFACE_KIND_BUNKER_POOR_LIE, // Mostly buried
    // The kind of SURFACE_BUNKER_UNUSED_LIE, which no decompiled code gives a
    // ball. Its surface is BUNKER_POOR_LIE's with a 40 wrong-club penalty, not
    // 30. Has BUNKER_POOR_LIE's image, but the default ratings and clubs
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

// An enum SurfaceKind value
typedef s32 SurfaceKind;
// How far above the tee a ball starts on the main courses
#define TEE_HEIGHT 307.2f

// Whether a ball on a surface kind is out of bounds, which costs a stroke.
// These are macros, as an inline function compiles differently from the
// original in computer_golfer.c.
#define IsOutOfBoundsKind(kind) \
    ((kind) == SURFACE_KIND_OUT_OF_BOUNDS || (kind) == SURFACE_KIND_OUT_OF_COURSE)
// Whether a ball on a surface kind is lost: in water or out of bounds
#define IsLostBallKind(kind) \
    ((kind) == SURFACE_KIND_WATER_HAZARD || \
        (kind) == SURFACE_KIND_OUT_OF_BOUNDS || \
        (kind) == SURFACE_KIND_OUT_OF_COURSE)
// Whether a surface kind is a fairway, at any of its lies
#define IsFairwayKind(kind) \
    ((kind) == SURFACE_KIND_FAIRWAY || \
        (kind) == SURFACE_KIND_FAIRWAY_FAIR_LIE || \
        (kind) == SURFACE_KIND_FAIRWAY_POOR_LIE)
// Whether a surface kind is a bunker with a bunker's ratings, at any of its
// lies. BUNKER_UNUSED_LIE is left out, having the default ratings.
#define IsBunkerKind(kind) \
    ((kind) == SURFACE_KIND_BUNKER || \
        (kind) == SURFACE_KIND_BUNKER_FAIR_LIE || \
        (kind) == SURFACE_KIND_BUNKER_POOR_LIE)

// How far a ball resting on a surface sinks into it, as it is drawn
enum SinkDepth {
    SINK_DEPTH_NONE,
    SINK_DEPTH_HALF,
    SINK_DEPTH_BURIED
};

typedef s32 SinkDepth;

typedef struct SurfaceTraits {
    SurfaceKind kind;
    char *name; // As the lie is shown
    s8 percentMin;
    s8 percentMax;
    s8 wrongClubPenalty; // Taken off both percents for a club not meant for the surface
    // In thousandths: the share of a rolling ball's speed kept each frame, and
    // how much a landing ball bounces
    s16 rollSpeedKept;
    s16 bounciness;
    WideAssetId lieImageAsset; // The HUD's picture of the lie
    SinkDepth sinkDepth;
    // A group of like surfaces, which nothing reads: 0 fairway, 1 rough or bare
    // ground, 2 bunker, 3 water, 4 cart way, 5 deep rough or waste area, 6
    // green, 7 out of bounds, 8 tee and 9 rock
    s32 ignoredSurfaceGroup;
} SurfaceTraits;

SurfaceTraits *GetSurfaceTraits(Surface surface);
SurfaceTraits *GetSurfaceTraitsAt(s32 x, s32 z);

#endif
