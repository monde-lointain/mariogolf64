/*
 * aud_sched.h
 *
 * Interface to the libmus audio frame scheduler: the indirection layer that
 * paces RSP audio-task dispatch off the video retrace cadence. A musSched is a
 * three-entry vtable (install / waitframe / dotask), and __libmus_current_sched
 * selects the active implementation, so a host game can substitute its own
 * scheduler without the audio thread caring. The macros below route each call
 * through that vtable.
 */
#ifndef _LIBMUS_AUD_SCHED_H_
#define _LIBMUS_AUD_SCHED_H_

// Active scheduler vtable; points at the built-in OSSched-based one by default.
extern musSched* __libmus_current_sched;

// Record the OS scheduler that later install/dispatch calls will run against.
void __MusIntSchedInit(void* sched);

// Signatures of the three vtable entry points. The function-like macros that
// follow deliberately shadow these names so every caller dispatches indirectly
// through the current vtable instead of binding to one fixed implementation.
void __MusIntSched_install(void);
void __MusIntSched_waitframe(void);
void __MusIntSched_dotask(musTask* task);

#define __MusIntSched_install() __libmus_current_sched->install()
#define __MusIntSched_waitframe() __libmus_current_sched->waitframe()
#define __MusIntSched_dotask(task) __libmus_current_sched->dotask((task))

#endif
