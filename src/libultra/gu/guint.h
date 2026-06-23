/*
 * Private declarations shared by the gu (graphics utility) math routines.
 *
 * Holds the bit-pattern accessor unions used by the software floating-point
 * transcendentals (sinf/cosf evaluate their polynomials in double precision and
 * need to read raw IEEE-754 bits), the host-side Matrix type, and the small
 * rounding/absolute-value helpers the polynomial evaluators reuse.
 */

#include "mbi.h"
#include "gu.h"

// A double accessed as its two raw 32-bit IEEE-754 words, so a constant can be
// written as an exact hex bit pattern instead of a decimal literal that the
// compiler might round.
typedef union {
  struct {
    unsigned int hi;
    unsigned int lo;
  } word;
  double d;
} du;

// A float accessed as its raw 32-bit bit pattern (used to dissect the exponent
// field and to return exact special values such as +0.0).
typedef union {
  unsigned int i;
  float f;
} fu;

// The host-CPU 4x4 float matrix worked on before packing to the RSP's
// fixed-point Mtx. Skipped when the OpenGL headers already define it.
#ifndef __GL_GL_H__
typedef float Matrix[4][4];
#endif

// Round a double to the nearest int, away from zero on a tie.
#define ROUND(d) (int)(((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))

// Absolute value of a double (branchless sign flip).
#define ABS(d) ((d) > 0) ? (d) : -(d)

extern float __libm_qnan_f;
