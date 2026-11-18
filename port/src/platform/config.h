/*
 * The player's settings, and the text file that holds them: lines of
 * "key = value" under "[section]" headers, with "#" or ";" starting a
 * comment line. FormatConfig writes every setting with its explanation,
 * and ParseConfig reads them back.
 *
 * A control's value lists up to PAD_SOURCES_PER_CONTROL sources, separated
 * by commas: a key by SDL's name for it (X, Return, Left Shift, Up), with
 * Comma and Keypad Comma for the keys whose names are commas; "pad:" and
 * a gamepad button's name (pad:south, pad:start, pad:dpup); or "pad:", a
 * sign and a gamepad axis's name, for one half of the axis (pad:-lefty,
 * pad:+righttrigger). An empty value unbinds the control.
 */

#ifndef PORT_PLATFORM_CONFIG_H
#define PORT_PLATFORM_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#include "pad_mapping.h"

// Room for the ROM's path and its NUL
#define CONFIG_PATH_SIZE 4096
#define CONFIG_SCALE_MAX 8
#define CONFIG_VOLUME_MAX 100
// The window's least and greatest width and height, besides 0
#define CONFIG_WINDOW_SIZE_MIN 160
#define CONFIG_WINDOW_SIZE_MAX 16384
// How far from the origin the window may be placed
#define CONFIG_WINDOW_POSITION_LIMIT 65536

typedef enum PresentChoice {
    PRESENT_MAILBOX, // The newest frame at each refresh, or else VSYNC
    PRESENT_VSYNC, // Every frame, each waiting for a refresh
    PRESENT_IMMEDIATE // At once, which may tear, or else MAILBOX
} PresentChoice;

typedef struct Config {
    char romPath[CONFIG_PATH_SIZE]; // Empty to look for the default
    int renderScale; // 0 to fit the display, or 1 to CONFIG_SCALE_MAX
    bool isFullscreen;
    PresentChoice present;
    int windowWidth; // 0 for the default, or within the limits above
    int windowHeight;
    bool hasWindowPosition; // Whether windowX and windowY place it
    int windowX;
    int windowY;
    bool pausesWhenUnfocused;
    int volume; // From 0 to CONFIG_VOLUME_MAX
    PadBindings bindings;
} Config;

// Told of each line that ParseConfig can't use, from 1, or 0 for the file
// as a whole
typedef void (*ConfigProblemHandler)(void *context, int line, const char *problem);

void SetDefaultConfig(Config *config);

// Reads the settings that the text gives into config, which keeps the
// rest. A line that it can't use changes nothing, and goes to the handler,
// which may be NULL; so does a stick whose dead zone the text leaves at or
// past its full throw, whose shape then stays as it was. Returns how many
// problems it reported.
int ParseConfig(
    const char *text,
    size_t size,
    Config *config,
    ConfigProblemHandler handler,
    void *context
);

// The text of every setting, which ParseConfig reads back the same, or
// NULL when memory runs out; the caller frees it with SDL_free
char *FormatConfig(const Config *config);

#endif
