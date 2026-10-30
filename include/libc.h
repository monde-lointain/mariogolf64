#ifndef LIBC_H
#define LIBC_H

#include <ultra64.h>

/* The C library routines of libultra and the KMC library */

size_t strlen(const char* string);
void* memcpy(void* dest, const void* src, size_t size);
void* memset(void* dest, s32 value, size_t size);
s32 strcmp(const char* left, const char* right);
s32 rand(void);
void srand(u32 seed);
/* Built into GCC, which compiles it to an abs.d instruction */
double fabs(double value);
/* Built into GCC, which compiles it to an abs.s instruction */
float fabsf(float value);
double atan(double value);
double atan2(double y, double x);
double sin(double angle);
double cos(double angle);
/* Built into GCC, which compiles it to a sqrt.d instruction */
double sqrt(double value);
#ifndef __IS_KMC__
/* The freestanding modern build has no built-ins but those named so, and no
 * library defines these */
#define fabs(value) __builtin_fabs(value)
#define sqrt(value) __builtin_sqrt(value)
#endif

#endif
