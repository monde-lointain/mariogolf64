/*
 * Where the window may go: a place remembered from an earlier run may be
 * on a display that's gone since, which would leave the player no way to
 * see or move the window.
 */

#ifndef PORT_PLATFORM_WINDOW_PLACE_H
#define PORT_PLATFORM_WINDOW_PLACE_H

#include <SDL3/SDL.h>
#include <stdbool.h>

// How much of the window's top edge must be on one display for the
// player to take hold of it
#define WINDOW_GRAB_WIDTH 64
#define WINDOW_GRAB_HEIGHT 32

// Whether a stretch of the window's top, WINDOW_GRAB_WIDTH wide and
// WINDOW_GRAB_HEIGHT high, lies wholly within one of the displays' areas
bool IsWindowPlaceVisible(const SDL_Rect *window, const SDL_Rect *displays, int displayCount);

#endif
