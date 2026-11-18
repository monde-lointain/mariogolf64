/*
 * The settings file on disk: config.ini in the user's data folder, beside
 * the save. The program creates it with every setting's explanation when
 * there's none, and rewrites it only to remember the window, keeping the
 * player's settings but not their own comments.
 */

#ifndef PORT_PLATFORM_CONFIG_FILE_H
#define PORT_PLATFORM_CONFIG_FILE_H

#include "config.h"

// The settings file in the user's data folder, freed with SDL_free, or
// NULL with the problem logged when there's no such folder
char *FindDefaultConfigPath(void);

// Sets config to the defaults and then to what the file at path gives,
// logging each line that it can't use. Where there's no file, writes the
// defaults there. Returns how many problems it logged, counting a file
// that exists but can't be read as one.
int LoadConfigFile(const char *path, Config *config);

// Writes every setting to the file at path, in a new file renamed over the
// old, so that a crash leaves one or the other whole; false, with the
// problem logged, when it can't
bool StoreConfigFile(const char *path, const Config *config);

// Rereads the file at path, which the player may have changed since, and
// stores it with the window's settings from window: its size, position and
// whether it's fullscreen. Leaves a file that has problems, and one that
// already holds those settings, as it is. Returns whether the file holds
// them afterwards.
bool StoreWindowSettings(const char *path, const Config *window);

#endif
