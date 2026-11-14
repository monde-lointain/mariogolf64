#ifndef BEST_TIMES_H
#define BEST_TIMES_H

#include <ultra64.h>

#include "asset.h"

enum BestTimesBuffer {
    BEST_TIMES_BUFFER_FACE_STRIP = 1,
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
