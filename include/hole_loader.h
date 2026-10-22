#ifndef HOLE_LOADER_H
#define HOLE_LOADER_H

#include <ultra64.h>

/* Cleared while the loader thread loads a hole; set once it is done or stopped */
extern s32 g_HasLoaderFinished;

void ResetLoaderThread(void);
void StartLoaderThread(void);
void StopLoaderThread(void);

/* Decompress the size bytes at romAddress into destination, returning the decompressed size */
s32 LoadLzData(u32 romAddress, void *destination, u32 size);
s32 LoadPatchedLzData(u32 romAddress, void *destination, u32 size);

#endif
