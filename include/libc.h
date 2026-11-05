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
#ifdef USES_BUILTIN_FABSF
/* Built into GCC, which compiles it to an abs.s instruction */
float fabsf(float value);
#else
/*
 * The game's own fabsf, an abs.s in volatile inline assembly. Local-alloc
 * can't give its result the operand's register, as it can the built-in's,
 * and the scheduler keeps it in place. Every matched function outside the
 * opening compiles the same with it, and DrawRoundHud and
 * IsEasedEyeHeightZero, which differed only at an abs.s, match with it;
 * without volatile, 5 and 7 of their words differ.
 */
static inline float fabsf(float value) {
  float magnitude;

  __asm__ volatile("abs.s %0, %1" : "=f"(magnitude) : "f"(value));
  return magnitude;
}
#endif
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
