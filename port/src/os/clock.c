/*
 * The CPU's count register and libultra's time, from the host's clock, and
 * the interrupt mask, which only records what the game sets: the game's
 * threads never run at once, so no interrupt can come between a thread's
 * libultra calls.
 */

#include <SDL3/SDL.h>

#include "os_internal.h"

static u64 f_ClockStart;
static u64 f_ClockFrequency;
static OSTime f_TimeOffset; // Added by osSetTime
static OSIntMask f_InterruptMask = OS_IM_ALL;

void PcInitClock(void) {
    f_ClockStart = SDL_GetPerformanceCounter();
    f_ClockFrequency = SDL_GetPerformanceFrequency();
}

// The count register's cycles since the clock started, in 64 bits. The
// quotient and the remainder are scaled apart, so that neither overflows.
static OSTime ElapsedCycles(void) {
    u64 ticks = SDL_GetPerformanceCounter() - f_ClockStart;
    u64 seconds = ticks / f_ClockFrequency;
    u64 remainder = ticks % f_ClockFrequency;

    return seconds * OS_CPU_COUNTER + (remainder * OS_CPU_COUNTER) / f_ClockFrequency;
}

OSTime osGetTime(void) {
    return ElapsedCycles() + f_TimeOffset;
}

void osSetTime(OSTime time) {
    f_TimeOffset = time - ElapsedCycles();
}

u32 osGetCount(void) {
    return (u32)osGetTime();
}

OSIntMask osSetIntMask(OSIntMask mask) {
    OSIntMask previous = f_InterruptMask;

    f_InterruptMask = mask;
    return previous;
}

OSIntMask osGetIntMask(void) {
    return f_InterruptMask;
}

// The port's libmus sets the host's audio rate itself
s32 osAiSetFrequency(u32 frequency) {
    return (s32)frequency;
}
