/*
 * Timer arm / schedule.
 *
 * Arms a one-shot or repeating timer. After `countdown` CPU-counter cycles the
 * kernel posts `msg` to `mq`; a non-zero `interval` then re-arms it to fire
 * every `interval` cycles thereafter. The timer is inserted into the kernel's
 * delta-ordered timer list, and the compare interrupt is reprogrammed only when
 * this timer becomes the new earliest one to fire.
 */

#include "PR/os_internal.h"
#include "osint.h"

int osSetTimer(OSTimer* t, OSTime countdown, OSTime interval, OSMesgQueue* mq,
               OSMesg msg) {
  OSTime time;

  t->next = NULL;
  t->prev = NULL;
  t->interval = interval;

  // A zero countdown means "use the interval for the first shot too", so a
  // pure periodic timer can be armed without restating the period twice.
  t->value = (countdown == 0) ? interval : countdown;
  t->mq = mq;
  t->msg = msg;

  // Insert into the ordered list; it returns the cycles until this timer fires.
  // Reprogram the hardware compare only if our timer is now the list head (the
  // soonest to fire) -- otherwise the existing head's interrupt still wins.
  time = __osInsertTimer(t);
  if (__osTimerList->next == t) {
    __osSetTimerIntr(time);
  }

  return 0;
}
