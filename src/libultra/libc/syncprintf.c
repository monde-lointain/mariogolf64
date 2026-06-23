/*
 * Diagnostic printf entry points that emit over the debug channel to the
 * development host. Both collapse to empty stubs in a final ROM build, so
 * release images carry no debug formatting code.
 */

#include "stdarg.h"
#include "PR/os.h"

// Output hook installed by the debug monitor; NULL until one is registered.
extern void* __printfunc;

/*
 * Formatted print routed to the host debug console, synchronized so the text
 * is delivered before the call returns. Stubbed out of the final ROM.
 */
void osSyncPrintf(const char* fmt, ...) {
  int ans;
  va_list ap;
#ifndef _FINALROM
  va_start(ap, fmt);
  __osSyncVPrintf(fmt, ap);
  va_end(ap);
#endif
}

/*
 * Formatted print for the remote monitor: format through _Printf into the
 * registered output hook. Silently does nothing if no hook is installed, and
 * is stubbed out of the final ROM.
 */
void rmonPrintf(const char* fmt, ...) {
  int ans;
  va_list ap;
#ifndef _FINALROM
  va_start(ap, fmt);
  if (__printfunc != NULL) {
    ans = _Printf(__printfunc, NULL, fmt, ap);
  }
  va_end(ap);
#endif
}
