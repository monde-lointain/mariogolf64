#ifndef LIBC_H
#define LIBC_H

#include <ultra64.h>

/* The C library routines of libultra and the KMC library */

size_t strlen(const char *string);
void *memcpy(void *dest, const void *src, size_t size);
void *memset(void *dest, s32 value, size_t size);
s32 strcmp(const char *left, const char *right);
s32 rand(void);
void srand(u32 seed);
double fabs(double value);
double atan(double value);
double atan2(double y, double x);
double sin(double angle);
double cos(double angle);

#endif
