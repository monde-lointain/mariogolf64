#ifndef COMMON_H
#define COMMON_H

#include <ultra64.h>
#include "include_asm.h"

#define ARRAY_COUNT(array) (s32)(sizeof(array) / sizeof((array)[0]))

/* The magnitude of an integer. The game's code evaluates the value once,
   into a variable of its own, as a GNU statement expression does. */
#define ABS(value) ({ s32 absValue = (value); (absValue < 0) ? -absValue : absValue; })

#endif
