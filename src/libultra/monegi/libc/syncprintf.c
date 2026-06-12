#include "stdarg.h"
#include "PR/os.h"

extern void* __printfunc;

void osSyncPrintf(const char* fmt, ...) {
    int ans;
    va_list ap;

#ifndef _FINALROM
    va_start(ap, fmt);
    __osSyncVPrintf(fmt, ap);
    va_end(ap);
#endif
}

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
