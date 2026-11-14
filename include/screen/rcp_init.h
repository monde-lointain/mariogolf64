/*
 * Sets up the RCP for a frame and clears the frame and depth buffers
 */

#ifndef RCP_INIT_H
#define RCP_INIT_H

#include <ultra64.h>

// What ClearBuffers and AppendBufferClear clear
enum BufferClear {
    // Clears the depth buffer and keeps the frame
    BUFFER_CLEAR_DEPTH_ONLY,
    // Also fills the frame black, a line at a time
    BUFFER_CLEAR_BY_LINE,
    BUFFER_CLEAR_WHOLE // Also fills the frame black at once
};

typedef s32 BufferClear;

void AppendRcpInit(s32 viewportIndex, Gfx **displayListHead);
void AppendBufferClear(BufferClear clearMode, Gfx **displayListHead);
void InitRcp(s32 viewportIndex);
void SetFillColor(s32 red, s32 green, s32 blue);
void FillScreen(void);
void ClearBuffers(BufferClear clearMode);

#endif
