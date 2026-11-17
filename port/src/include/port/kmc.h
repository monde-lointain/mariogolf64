/*
 * The port's stand-ins for the KMC C library's routines whose results
 * differ from the host's. The game's sources call them by the C library's
 * names, which their build defines to these (see mg64_game_options), so
 * that the game sees the same numbers on every host and as on the N64.
 */

#ifndef PORT_KMC_H
#define PORT_KMC_H

// The largest number that KmcRandom gives
#define KMC_RANDOM_MAX 0x7FFF

// The game's rand: the next number from 0 to KMC_RANDOM_MAX
int KmcRandom(void);

// The game's srand: starts KmcRandom's sequence again from seed
void SeedKmcRandom(unsigned int seed);

// The game's sin, cos, tan, atan and atan2, which give the N64's results
// to the last bit. The first three give NaN for an angle that isn't
// finite or is 2^31 half turns or more, where the N64 raised an exception.
double KmcSine(double angle);
double KmcCosine(double angle);
double KmcTangent(double angle);
double KmcArcTangent(double value);
double KmcArcTangent2(double y, double x);

#endif
