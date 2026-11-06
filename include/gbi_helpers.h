#ifndef GBI_HELPERS_H
#define GBI_HELPERS_H

#include <ultra64.h>

/*
 * gSPTexture's scales, in 0.16 fixed point: one half, which most of the
 * game's display lists set, and the largest, just under one
 */
#define TEXTURE_SCALE_HALF 0x8000
#define TEXTURE_SCALE_FULL 0xFFFF

/*
 * The texture coordinate mask that wraps or mirrors a texture every size
 * texels, the base-2 logarithm of size, for a power of two up to 256
 */
#define TextureMask(size) \
  ((size) >= 256          ? 8 \
          : (size) >= 128 ? 7 \
          : (size) >= 64  ? 6 \
          : (size) >= 32  ? 5 \
          : (size) >= 16  ? 4 \
          : (size) >= 8   ? 3 \
          : (size) >= 4   ? 2 \
          : (size) >= 2   ? 1 \
                          : 0)

/* The vertices of a quad, drawn as two triangles */
#define QUAD_VERTEX_COUNT 4

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
 * Declaring the state command's pointer before the sync's swaps the stack
 * slots of the two pointers when neither gets a register, matching the
 * original's slots.
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
