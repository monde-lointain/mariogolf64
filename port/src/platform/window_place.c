/*
 * Where the window may go.
 */

#include "window_place.h"

bool IsWindowPlaceVisible(const SDL_Rect *window, const SDL_Rect *displays, int displayCount) {
    SDL_Rect top;
    int i;

    SDL_assert(window != NULL && (displays != NULL || displayCount == 0));
    top.x = window->x;
    top.y = window->y;
    top.w = window->w;
    top.h = WINDOW_GRAB_HEIGHT;
    for (i = 0; i < displayCount; i++) {
        SDL_Rect overlap;

        if (SDL_GetRectIntersection(&top, &displays[i], &overlap) &&
            overlap.w >= WINDOW_GRAB_WIDTH &&
            overlap.h >= WINDOW_GRAB_HEIGHT) {
            return true;
        }
    }

    return false;
}
