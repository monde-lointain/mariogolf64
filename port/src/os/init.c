/*
 * The start of the port's libultra, the first thread, and the reports that
 * the game sends to the development host.
 */

#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "os_internal.h"
#include "port/os_host.h"
#include "scheduler.h"

// Set to 1 to see osSyncPrintf's reports on the standard error
#define LOG_ENVIRONMENT_VARIABLE "MG64_OS_LOG"

static int f_IsLogging;

void PcInitOs(void) {
    const char *logging = SDL_getenv(LOG_ENVIRONMENT_VARIABLE);

    f_IsLogging = (logging != NULL && SDL_atoi(logging) != 0);
    PcInitClock();
    PcInitScheduler();
    PcInitSegments();
}

void PcStartBootThread(OSThread *thread, void (*entry)(void *), void *argument, OSPri priority) {
    osCreateThread(thread, 1, entry, argument, NULL, priority);
    osStartThread(thread);
}

void osSyncPrintf(const char *format, ...) {
    va_list arguments;

    if (!f_IsLogging) {
        return;
    }

    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
}

void PcFatalError(const char *format, ...) {
    char message[512];
    va_list arguments;

    va_start(arguments, format);
    SDL_vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    fprintf(stderr, "mariogolf64: %s\n", message);
    fflush(stderr);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Mario Golf", message, NULL);
    abort();
}
