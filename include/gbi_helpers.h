#ifndef GBI_HELPERS_H
#define GBI_HELPERS_H

#include <ultra64.h>

/*
 * gSPTexture's scales, in 0.16 fixed point: one half, which most of the
 * game's display lists set, and the largest, just under one
 */
#define TEXTURE_SCALE_HALF 0x8000
#define TEXTURE_SCALE_FULL 0xFFFF

/* Every geometry mode bit, for gSPClearGeometryMode to clear them all */
#define ALL_GEOMETRY_MODES 0xFFFFFFFF

/* The ends of an 8-bit color's alpha */
#define TRANSPARENT_ALPHA 0
#define OPAQUE_ALPHA 0xFF

/*
 * Appends a pipe sync followed by a single-command GBI state setter.
 * displayList must be a side-effect-free Gfx* lvalue with room for both
 * commands; it advances twice. setState is a gDPSet... macro, followed by
 * its state arguments. Those arguments must not modify displayList.
 * Like the SDK's GBI macros, this expands to a block: brace callers' if/else
 * bodies. A do/while wrapper introduces a block boundary in KMC's optimizer.
 *
 * Declaring the state command before the sync preserves KMC's spill order.
 * A macro keeps that declaration in the caller; an inline function changes
 * register allocation. Named variadic arguments support KMC's pre-C99 GCC.
 */
#define AppendSyncedRdpState(displayList, setState, stateArguments...) \
  { \
    Gfx* stateCommand; \
    gDPPipeSync((displayList)++); \
    stateCommand = (displayList)++; \
    setState(stateCommand, stateArguments); \
  }

#endif
