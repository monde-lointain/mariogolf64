/*
 * _matherr.c -- math-error dispatcher shared by the library's math routines.
 *
 * A math routine that detects an error calls __matherr() with the error class,
 * its name, the offending arguments, and a proposed return value. __matherr()
 * packages those into an exception record, offers it to the user hook
 * _matherr() (see matherr.c), and on a refusal sets errno before returning the
 * (possibly hook-adjusted) result back to the math routine.
 */
#include "_kmclib.h"
#include <math.h>

double __matherr(type, name, a1, a2, retval)
int type;
char* name;
double a1, a2, retval;
{
  struct _mexception ex;

  // Assemble the exception record the user hook inspects (and may rewrite).
  ex.type = type;
  ex.name = name;
  ex.arg1 = a1;
  ex.arg2 = a2;
  ex.retval = retval;

  // A zero return means the hook declined to handle it, so report the standard
  // errno: ERANGE for over/underflow (magnitude unrepresentable), EDOM
  // otherwise (argument outside the routine's domain).
  if (_matherr(&ex) == 0) {
    errno = ((type == _OVERFLOW) || (type == _UNDERFLOW)) ? ERANGE : EDOM;
  }
  return ex.retval;
}
