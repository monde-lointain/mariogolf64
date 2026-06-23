/*
 * math.h -- floating-point math interface for the KMC C library.
 *
 * Declares the standard double-precision math routines plus the library's
 * math-error reporting hook (`_matherr`/`__matherr`) and the exception-type
 * codes it classifies failures with. All routines take and return `double`.
 * On a domain or range error a routine sets `errno` (see errno.h).
 */
#ifndef _MATH_H
#define _MATH_H

// Mirror of errno.h: error cell and the codes the math routines report.
extern int errno;
#define EDOM 33
#define ERANGE 34

// Trigonometric and inverse-trigonometric functions (arguments in radians).
double sin(double);
double cos(double);
double tan(double);
double atan(double);
double atan2(double, double);  // atan(y/x), quadrant from the sign of both args
double sqrt(double);
double asin(double);
double acos(double);

// Exponential, hyperbolic, and logarithmic functions.
double exp(double);
double sinh(double);
double cosh(double);
double tanh(double);
double log(double);    // natural logarithm (base e)
double log10(double);  // common logarithm (base 10)

// Decomposition, power, and rounding.
double frexp(double, int*);    // split into normalized fraction and exponent
double pow(double, double);    // base raised to the given power
double fabs(double);           // absolute value
double fmod(double, double);   // floating-point remainder
double ceil(double);           // round toward +infinity
double floor(double);          // round toward -infinity
double ldexp(double, int);     // fraction * 2^exp
double modf(double, double*);  // split into integer and fractional parts

// Math-error report passed to the error hook: which routine failed, with what
// arguments and intended result, so the hook can substitute a value.
struct _mexception {
  int type;                   // one of the _DOMAIN.._UNDERFLOW codes below
  char* name;                 // name of the routine that raised the error
  double arg1, arg2, retval;  // offending arguments and proposed return value
};

// Error hook invoked when a math routine fails; a nonzero return suppresses the
// default errno/message handling. __matherr is the library's internal raiser.
int _matherr(struct _mexception*);
double __matherr(int type, char* name, double arg1, double arg2, double retval);

// Exception-type codes carried in _mexception.type.
#define _DOMAIN 1     // argument outside the routine's domain
#define _SING 2       // argument produced a singularity (e.g. pole)
#define _OVERFLOW 3   // result too large to represent
#define _UNDERFLOW 4  // result too small to represent

#endif
