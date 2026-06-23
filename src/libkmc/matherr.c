/*
 * matherr.c -- default user-replaceable math-exception hook.
 *
 * The library calls _matherr() whenever a math routine hits an error (domain,
 * singularity, overflow, underflow). This stock version does nothing: a program
 * that wants custom handling supplies its own _matherr() to override it. See
 * __matherr() in _matherr.c for the caller.
 */
#include "_kmclib.h"
#include <math.h>

/* Returning 0 means "not handled" -- the library then falls back to its default
 * errno behaviour. The exception record sp is left untouched. */
int _matherr(sp)
struct _mexception* sp;
{
  return 0;
}
