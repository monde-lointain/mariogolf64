/*
 * Thread priority query.
 *
 * Returns the scheduling priority of the given thread. A NULL argument is a
 * shorthand for "the calling thread", so a thread can ask its own priority
 * without holding a handle to itself.
 */

#include "PR/os_internal.h"
#include "osint.h"

OSPri osGetThreadPri(OSThread* thread) {
  if (thread == NULL) {
    thread = __osRunningThread;
  }
  return thread->priority;
}
