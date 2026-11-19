/*
 * The program's command line: the options that main.c describes, read into
 * an Options.
 */

#ifndef PLATFORM_OPTIONS_H
#define PLATFORM_OPTIONS_H

#include <stdbool.h>

typedef struct Options {
    const char *romPath;
    const char *savePath;
    const char *configPath;
    const char *screenshotPath;
    const char *inputScriptPath;
    const char *recordingPath;
    int renderScale; // 0 to choose from the display
    long frameLimit; // Retraces to run, or 0 for no limit
    long gameFrameLimit; // The game's frames to run, or 0 for no limit
    bool isFullscreen;
    bool isGpuDebug;
    bool isHeadless;
} Options;

// Reads the command line into the options, or logs what is wrong with it
// and gives false
bool ParseOptions(int argc, char **argv, Options *options);

// Whether a run that stopped after the given retraces and game frames had
// reached a limit that the options set. Without a limit, any stop ends the
// run as it should; with one, a stop before it, as when the player closes
// the window or the host ends the program, leaves the run unfinished.
bool HasRunReachedLimit(const Options *options, long frames, long gameFrames);

#endif
