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
double ReportMathError(int type, char *name, double arg1, double arg2, double retval);

#endif
