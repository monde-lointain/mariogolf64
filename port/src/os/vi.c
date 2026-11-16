/*
 * The video interface: which frame buffer the screen shows, and the
 * retrace's message. The platform's timer calls PcRaiseRetrace, and its
 * presenter shows the frame buffer that it returns.
 */

#include <SDL3/SDL.h>

#include "os_internal.h"
#include "port/os_host.h"
#include "scheduler.h"

// The mode that the game sets through NuSystem: 320x240, antialiased,
// 16-bit color
#define DEFAULT_VI_MODE OS_VI_NTSC_LAN1

static void *f_CurrentFramebuffer;
static void *f_NextFramebuffer;
static int f_IsBlack = TRUE;
static u32 f_RetraceCount;
static OSMesgQueue *f_RetraceQueue;
static OSMesg f_RetraceMessage;
static u32 f_RetracesPerMessage = 1;
static u32 f_ModeType = DEFAULT_VI_MODE;

// Kept for libultra's interface; only the type is read
OSViMode osViModeTable[OS_VI_MPAL_LAF2 + 1];

void osCreateViManager(OSPri priority) {
    (void)priority;
}

void osViSetEvent(OSMesgQueue *queue, OSMesg message, u32 retraceCount) {
    LockScheduler();
    f_RetraceQueue = queue;
    f_RetraceMessage = message;
    f_RetracesPerMessage = (retraceCount != 0) ? retraceCount : 1;
    UnlockScheduler();
}

void osViSwapBuffer(void *framebuffer) {
    LockScheduler();
    f_NextFramebuffer = framebuffer;
    UnlockScheduler();
}

void *osViGetCurrentFramebuffer(void) {
    void *framebuffer;

    LockScheduler();
    framebuffer = f_CurrentFramebuffer;
    UnlockScheduler();
    return framebuffer;
}

void *osViGetNextFramebuffer(void) {
    void *framebuffer;

    LockScheduler();
    framebuffer = f_NextFramebuffer;
    UnlockScheduler();
    return framebuffer;
}

void osViBlack(u8 isBlack) {
    LockScheduler();
    f_IsBlack = isBlack;
    UnlockScheduler();
}

void osViSetMode(OSViMode *mode) {
    if (mode != NULL) {
        f_ModeType = mode->type;
    }
}

u32 osViGetCurrentMode(void) {
    return f_ModeType;
}

u32 osViGetStatus(void) {
    return 0;
}

u32 osViGetCurrentField(void) {
    return 0;
}

u32 osViGetCurrentLine(void) {
    return 0;
}

// The host's presenter scales the picture to its window, so the features
// and scales that the N64's VI applied have nothing to change
void osViSetSpecialFeatures(u32 features) {
    (void)features;
}

void osViSetXScale(f32 scale) {
    (void)scale;
}

void osViSetYScale(f32 scale) {
    (void)scale;
}

void osViFade(u8 active, u16 factor) {
    (void)active;
    (void)factor;
}

void osViRepeatLine(u8 active) {
    (void)active;
}

void *PcRaiseRetrace(void) {
    void *shown;

    LockScheduler();
    f_CurrentFramebuffer = f_NextFramebuffer;
    shown = f_IsBlack ? NULL : f_CurrentFramebuffer;
    f_RetraceCount++;
    if (f_RetraceCount % f_RetracesPerMessage == 0) {
        PcSendInterruptMessage(f_RetraceQueue, f_RetraceMessage);
    }

    WakeInterruptWaiters();
    ReachPreemptionPoint();
    UnlockScheduler();
    return shown;
}
