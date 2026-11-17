/*
 * Gives the game's calls to the C library's random numbers and
 * trigonometry to the port's KMC stand-ins (see port/kmc.h). The build
 * includes it before each of the game's sources.
 *
 * The host's own headers come first, so that their include guards keep
 * the names below out of them: glibc's math.h builds other names from
 * these, which a rename would break. The host's own callers, such as a GPU
 * driver, keep the host's routines, so they neither take the game's
 * numbers nor move its sequence on.
 */

#ifndef PORT_KMC_NAMES_H
#define PORT_KMC_NAMES_H

#include <math.h>
#include <stdlib.h>

// Its neighbour, by a path that needs none of the port's include directories
#include "kmc.h"

#define rand KmcRandom
#define srand SeedKmcRandom
#define sin KmcSine
#define cos KmcCosine
#define tan KmcTangent
#define atan KmcArcTangent
#define atan2 KmcArcTangent2

#endif
