#ifndef HOLE_LOADER_H
#define HOLE_LOADER_H

#include <ultra64.h>

/* Cleared while the loader thread loads a hole; set once it is done or stopped */
extern s32 g_HasLoaderFinished;

void ResetLoaderThread(void);
void StartLoaderThread(void);
void StopLoaderThread(void);

#endif
