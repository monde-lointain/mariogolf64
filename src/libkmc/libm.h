#ifndef LIBKMC_LIBM_H
#define LIBKMC_LIBM_H

/* exception types */
#define DOMAIN 1
#define SING 2
#define OVERFLOW 3
#define UNDERFLOW 4

/* errno values */
#define EDOM 33
#define ERANGE 34

#define M_PI 3.14159265358979323846

/* The CORDIC routines work in fixed point with 60 fractional bits */
#define FIXED_POINT_ONE (1LL << 60)
#define FIXED_POINT_ONE_F 1152921504606846976.0
#define CORDIC_ITERATION_COUNT 52

/* What went wrong in a math routine, for matherr */
struct exception {
    int type;
    char *name;
    double arg1;
    double arg2;
    double retval; /* What the routine returns; matherr may change it */
};

extern int errno;

int matherr(struct exception *exc);
extern long long g_CordicAngles[64];

double ReportMathError(int type, char *name, double arg1, double arg2, double retval);

double atan(double x);
double atan2(double y, double x);

#endif
