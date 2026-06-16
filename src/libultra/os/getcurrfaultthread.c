#include "PR/os_internal.h"
#include "osint.h"

OSThread* __osGetCurrFaultedThread(void) {
    return __osFaultedThread;
}
