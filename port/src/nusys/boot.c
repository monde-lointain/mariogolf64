/*
 * NuSystem's start for the port: a boot thread readies the cartridge and
 * the scheduler, then starts the main thread on the game's mainproc. The
 * N64's idle thread only kept the CPU busy, which the host needn't do, so
 * the boot thread ends there.
 */

#include "nusys_internal.h"
#include "port/nusys_host.h"
#include "port/os_host.h"

// The game's main thread
void mainproc(void *argument);

// Below every thread that it starts, so that each runs as it starts
#define BOOT_THREAD_PRI (NU_MAIN_THREAD_PRI - 1)

static OSThread f_BootThread;
static OSThread f_MainThread;

static void RunBootThread(void *argument) {
    (void)argument;
    nuPiInit();
    NuStartScheduler();
    osCreateThread(&f_MainThread, NU_MAIN_THREAD_ID, mainproc, NULL, NULL, NU_MAIN_THREAD_PRI);
    osStartThread(&f_MainThread);
}

void PcBootNuSystem(void) {
    PcStartBootThread(&f_BootThread, RunBootThread, NULL, BOOT_THREAD_PRI);
}
