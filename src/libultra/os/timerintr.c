/*
 * Software timer services.
 *
 * Drives the kernel's countdown timers off the CPU's compare interrupt. Holds
 * the timer-list state, the interrupt handler that fires due timers and re-arms
 * the hardware, and the helpers that program the compare register and splice a
 * timer into the list.
 *
 * The timer list is kept in *delta* order: each timer's `value` holds the
 * number of CPU-counter cycles to wait after the previous timer in the list
 * fires, not an absolute deadline. Only the head's delta need ever be loaded
 * into the compare register, which makes adding and expiring timers cheap.
 *
 * On non-final builds the same interrupt also samples the PC for the
 * statistical profiler.
 */

#include "PRinternal/macros.h"
#include "PR/os_internal.h"
#include "osint.h"

extern OSTime __osCurrentTime;
extern u32 __osBaseCounter;
extern u32 __osViIntrCount;
extern u32 __osTimerCounter;
extern OSTimer __osBaseTimer;

// List head is the base timer itself, used as a circular sentinel node.
OSTimer* __osTimerList = &__osBaseTimer;

#ifndef _FINALROM
// Profiler state: a queue tag that marks profiling "timers", plus the bins that
// accumulate PC hit counts. Present only when profiling is compiled in.
OSMesgQueue __osProfTimerQ ALIGNED(0x8);
OSProf* __osProfileList;
OSProf* __osProfileListEnd;
u32 __osProfileOverflowBin;
#endif

// Reset all timer bookkeeping and close the list into an empty ring (head
// points at itself). Called once during OS bring-up.
void __osTimerServicesInit(void) {
  __osCurrentTime = 0;
  __osBaseCounter = 0;
  __osViIntrCount = 0;
  __osTimerList->next = __osTimerList->prev = __osTimerList;
  __osTimerList->interval = __osTimerList->value = 0;
  __osTimerList->mq = NULL;
  __osTimerList->msg = 0;
}

// Compare-interrupt handler. Walks the delta-ordered list from the head firing
// every timer whose delay has elapsed, re-arms the hardware for the next one,
// and reschedules any periodic timers.
void __osTimerInterrupt(void) {
  OSTimer* t;
  u32 count;
  u32 elapsed_cycles;
#ifndef _FINALROM
  u32 pc;
  s32 offset;
  OSProf* prof = __osProfileList;
#endif

  // Empty list: nothing is armed, so the interrupt was spurious.
  if (__osTimerList->next == __osTimerList) {
    return;
  }

  for (;;) {
    t = __osTimerList->next;

    // Reached the sentinel: no timers left, so disable the compare interrupt.
    if (t == __osTimerList) {
      __osSetCompare(0);
      __osTimerCounter = 0;
      break;
    }

    // Charge the cycles spent since the last interrupt against the head timer's
    // delta. If it has not yet elapsed, re-arm the hardware for the remainder
    // and stop -- every later timer is delta-relative to this one.
    count = osGetCount();
    elapsed_cycles = count - __osTimerCounter;
    __osTimerCounter = count;
    if (elapsed_cycles < t->value) {
      t->value -= elapsed_cycles;
      __osSetTimerIntr(t->value);
      break;
    }

    // This timer has fired: unlink it from the list before notifying.
    t->prev->next = t->next;
    t->next->prev = t->prev;
    t->next = NULL;
    t->prev = NULL;

    if (t->mq != NULL) {
#ifdef _FINALROM
      osSendMesg(t->mq, t->msg, OS_MESG_NOBLOCK);
    }
#else
      // A normal timer just posts its message. The profiling pseudo-timer
      // instead samples the interrupted thread's PC into a histogram.
      if (t->mq != &__osProfTimerQ) {
        osSendMesg(t->mq, t->msg, OS_MESG_NOBLOCK);
      } else {
        // Find which profiled text range the PC fell into and bump the bin for
        // that instruction (one u16 counter per 4-byte instruction slot).
        pc = __osRunQueue->context.pc;
        for (prof = __osProfileList; prof < __osProfileListEnd; prof++) {
          offset = pc - (u32)prof->text_start;
          if (offset >= 0) {
            if ((s32)prof->text_end - (s32)pc > 0) {
              (*(u16*)(u32)((offset >> 2) + prof->histo_base))++;
              goto __ProfDone;
            }
          }
        }
        // PC was outside every profiled range: tally it in the catch-all bin.
        __osProfileOverflowBin++;
      }
    }
#endif
  __ProfDone:
    // Periodic timer: reload its full interval and splice it back in.
    if (t->interval != 0) {
      t->value = t->interval;
      __osInsertTimer(t);
    }
  }
}

// Program the CPU compare register to interrupt `tim` cycles from now.
void __osSetTimerIntr(OSTime tim) {
  OSTime NewTime;
  u32 savedMask;

#if BUILD_VERSION >= VERSION_K
  // Clamp to a floor so an extremely short delay cannot be missed by scheduling
  // the compare in the past relative to interrupt-handling latency.
  if (tim < 468) {
    tim = 468;
  }
#endif

  savedMask = __osDisableInt();
  __osTimerCounter = osGetCount();
  NewTime = __osTimerCounter + tim;
  __osSetCompare(NewTime);
  __osRestoreInt(savedMask);
}

// Splice a timer into the delta-ordered list and return the cycles until it
// fires. Walks the list accumulating deltas until the insertion point is found,
// rewriting `t->value` to the delta from its predecessor and shrinking the
// successor's delta by the same amount so the chain stays consistent.
OSTime __osInsertTimer(OSTimer* t) {
  OSTimer* timep;
  OSTime tim;
  u32 savedMask = __osDisableInt();

  timep = __osTimerList->next;
  tim = t->value;

  // Subtract each earlier timer's delta from our absolute delay until we reach
  // the timer that should fire after us (or the sentinel).
  for (; timep != __osTimerList && tim > timep->value; timep = timep->next) {
    tim -= timep->value;
  }
  t->value = tim;

  // Our insertion shortened the wait that the successor inherits, so reduce its
  // delta by the time we now consume ahead of it.
  if (timep != __osTimerList) {
    timep->value -= tim;
  }

  // Link t in just before timep.
  t->next = timep;
  t->prev = timep->prev;
  timep->prev->next = t;
  timep->prev = t;

  __osRestoreInt(savedMask);
  return tim;
}
