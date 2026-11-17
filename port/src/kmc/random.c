/*
 * The KMC C library's random numbers: the C standard's example linear
 * congruential generator, except that each number comes from the bits of
 * one more than the new state.
 */

#include <stdint.h>

#include "port/kmc.h"

#define RANDOM_MULTIPLIER 1103515245u
#define RANDOM_INCREMENT 12345u
#define RANDOM_SHIFT 16

// The generator's state. On the N64 the game's main_bss.c defines it, to
// fix its address; elsewhere it is the library's own, as here.
static uint32_t f_RandomState;

int KmcRandom(void) {
    // The state is unsigned, so it wraps as the N64's register did
    f_RandomState = f_RandomState * RANDOM_MULTIPLIER + RANDOM_INCREMENT;
    return (int)(((f_RandomState + 1u) >> RANDOM_SHIFT) & KMC_RANDOM_MAX);
}

void SeedKmcRandom(unsigned int seed) {
    f_RandomState = seed;
}
