/*
 * stdlib.h -- general utilities for the KMC C library.
 *
 * Declares the standard utility set: integer/string conversions, pseudo-random
 * numbers, integer arithmetic helpers, search/sort, and the heap allocator and
 * program-termination calls. Shared types (size_t, div_t, ldiv_t) and the
 * min/max helper macros live here too.
 */
#ifndef _STDLIB_H
#define _STDLIB_H

// size_t guarded by a shared flag so the several headers that need it agree.
#ifndef _SIZE_T_DEF
#define _SIZE_T_DEF
typedef unsigned size_t;
#endif

// Quotient/remainder pair returned by div().
#ifndef _DIV_T_DEF
#define _DIV_T_DEF
typedef struct DIV_T {
  int quot;
  int rem;
} div_t;
#endif

// Quotient/remainder pair returned by ldiv() (long variant).
#ifndef _LDIV_T_DEF
#define _LDIV_T_DEF
typedef struct LDIV_T {
  long quot;
  long rem;
} ldiv_t;
#endif

#ifndef NULL
#define NULL 0
#endif

// Beware: arguments are evaluated twice, so avoid side effects in them.
#define _max(a, b) (((a) > (b)) ? (a) : (b))
#define _min(a, b) (((a) < (b)) ? (a) : (b))

#define RAND_MAX 32767  // largest value rand() can return

// Pseudo-random numbers: rand() yields 0..RAND_MAX; srand() reseeds it.
int rand(void);
void srand(unsigned);

// Integer absolute value and integer division returning quotient and remainder.
int abs(int);
long labs(long);
div_t div(int, int);
ldiv_t ldiv(long, long);

// String-to-number conversions. The strtol/strtoul forms also report the first
// unconverted character and accept an explicit radix (0 = auto-detect).
int atoi(const char*);
long atol(const char*);
long strtol(const char*, char**, int);
unsigned long strtoul(const char*, char**, int);

// Number-to-string conversions writing into a caller-supplied buffer, with an
// explicit radix (2..36).
char* itoa(int, char*, int);
char* ltoa(long, char*, int);
char* ultoa(unsigned long, char*, int);

// String-to-floating-point conversions.
double atof(const char*);
double strtod(const char*, char**);

// Generic search and sort over an array of `size`-byte elements, ordered by a
// caller-supplied comparison callback.
void qsort(void*, size_t, size_t, int (*)(const void*, const void*));
void* bsearch(const void*, const void*, size_t, size_t,
              int (*)(const void*, const void*));

// Heap allocation and program termination.
void* malloc(size_t);
void* calloc(size_t, size_t);  // zero-initialized array of count * size bytes
void* realloc(void*, size_t);
void free(void*);
void exit(int);    // normal termination, running cleanup handlers
void abort(void);  // abnormal termination, no cleanup

#endif
