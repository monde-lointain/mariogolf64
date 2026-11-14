#ifndef RCP_H
#define RCP_H

#include <ultra64.h>

enum BufferClear {
    BUFFER_CLEAR_DEPTH_ONLY,
    BUFFER_CLEAR_BY_LINE,
    BUFFER_CLEAR_WHOLE
};

void AppendRcpInit(s32 viewportIndex, Gfx **displayListHead);
void AppendBufferClear(s32 clearMode, Gfx **displayListHead);
void InitRcp(s32 viewportIndex);
void SetFillColor(s32 red, s32 green, s32 blue);
void FillScreen(void);
void ClearBuffers(s32 clearMode);

#endif
