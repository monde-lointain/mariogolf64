/*
 * Pre-NMI shutdown helper.
 *
 * Called once an application has quiesced its RSP work in response to a pre-NMI
 * (reset-pending) event. Resets the RSP program counter to 0 so the signal
 * processor is left in a known, idle state before the imminent reset.
 */

#include "PR/os_internal.h"

s32 osAfterPreNMI() { return __osSpSetPc(0); }
