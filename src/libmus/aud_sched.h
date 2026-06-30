/*
 * aud_sched.h
 *
 * Internal interface to the libmus audio frame scheduler: it submits RSP
 * audio tasks to the OS scheduler and paces the audio thread to the VI
 * retrace. The player layer reaches the scheduler only through this header.
 *
 * Scheduling is indirected through a swappable vtable (musSched). The
 * default back-end (aud_sched.c) runs on the OS scheduler; a title may
 * install its own with MusSetScheduler().
 */
#ifndef _LIBMUS_AUD_SCHED_H_
#define _LIBMUS_AUD_SCHED_H_

/* Active scheduler vtable; every op below dispatches through it. */
extern musSched* __libmus_current_sched;

/* Hand the back-end the host OSSched it submits audio tasks to. */
void __MusIntSchedInit(void* sched);

/*
 * The three scheduler operations. Each call site expands (via the macros
 * below) to an indirect call through __libmus_current_sched, so the active
 * back-end runs rather than a like-named function; the prototypes document
 * each vtable slot's signature.
 */
void __MusIntSched_install(void);         /* one-time client/queue setup */
void __MusIntSched_waitframe(void);       /* block until the next audio frame */
void __MusIntSched_dotask(musTask* task); /* run one RSP task; wait for it */

/* Route each op through the current vtable. */
#define __MusIntSched_install() __libmus_current_sched->install()
#define __MusIntSched_waitframe() __libmus_current_sched->waitframe()
#define __MusIntSched_dotask(task) __libmus_current_sched->dotask((task))

#endif
