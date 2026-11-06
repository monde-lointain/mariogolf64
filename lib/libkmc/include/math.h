#ifndef LIBKMC_MATH_H
#define LIBKMC_MATH_H

/* The KMC C library's math routines that the game calls */

double atan(double value);
double atan2(double y, double x);
double sin(double angle);
double cos(double angle);
double tan(double angle);
/* Built into GCC, which compiles it to an abs.d instruction */
double fabs(double value);
/* Built into GCC, which compiles it to a sqrt.d instruction */
double sqrt(double value);

#ifndef __IS_KMC__
/* The freestanding modern build has no built-ins but those named so, and no
 * library defines these */
#define fabs(value) __builtin_fabs(value)
#define sqrt(value) __builtin_sqrt(value)
#endif

#endif
