#include "common.h"

extern OSThread *__osFaultedThread;

OSThread *__osGetCurrFaultedThread(void) { return __osFaultedThread; }
