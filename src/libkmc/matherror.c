#include "libm.h"

/* Gives matherr a chance to handle the error, and sets errno if it doesn't */
double ReportMathError(int type, char* name, double arg1, double arg2,
                       double retval) {
  struct exception exc;

  exc.type = type;
  exc.name = name;
  exc.arg1 = arg1;
  exc.arg2 = arg2;
  exc.retval = retval;
  if (!matherr(&exc)) {
    errno = (type == OVERFLOW || type == UNDERFLOW) ? ERANGE : EDOM;
  }
  return exc.retval;
}
