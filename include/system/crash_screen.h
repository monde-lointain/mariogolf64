/*
 * The fault report, which the released game never starts, and the empty
 * string that shares its file.
 */

#ifndef CRASH_SCREEN_H
#define CRASH_SCREEN_H

// An empty string, which menu.c's save copies use as their host file names
extern const char g_EmptyString[];

// The room the report's line buffer has before the next common variable
#define FAULT_TEXT_SIZE 0x80

// Runs at boot before anything else; the original started the fault thread
void RunBootHook(void);

#endif
