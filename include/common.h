#ifndef COMMON_H
#define COMMON_H

#include <ultra64.h>

#include "include_asm.h"

/* Declares the linker-generated PI ROM bounds of a named mariogolf64.yaml
 * segment, e.g. USE_ASSET(asset_directory) for asset_directory_ROM_START. */
#define USE_ASSET(id)         \
  extern u8 id##_ROM_START[]; \
  extern u8 id##_ROM_END[]

#endif
