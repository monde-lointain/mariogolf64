/*
 * The clubhouse's best times, in ovl_01/best_times.c
 */

#ifndef BEST_TIMES_H
#define BEST_TIMES_H

#include <ultra64.h>

#include "system/asset.h"

// The buffers that LoadBestTimesBuffer keeps, by the kind its caller gives
enum BestTimesBuffer {
    // Read into g_WorkArea, whatever buffer the caller gives
    BEST_TIMES_BUFFER_FACE_STRIP = 1,
    // Kept without reading an image into it
    BEST_TIMES_BUFFER_UNREAD,
    BEST_TIMES_BUFFER_PLACES,
    BEST_TIMES_BUFFER_GREEN_ARROWS
};

typedef s16 BestTimesBuffer;
void LoadBestTimesCourseIcons(ImageAsset **buffers);
void LoadCharacterFaceStrip(void *buffer);
void LoadBestTimesBuffer(void *buffer, BestTimesBuffer bufferKind);
void UpdateBestTimes(void);
void DrawBestTimes(Gfx **displayListHead);
void LoadBestTimesFaces(void);

#endif
