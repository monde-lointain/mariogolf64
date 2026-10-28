#ifndef DEBUG_TEXT_H
#define DEBUG_TEXT_H

#include "common.h"

/*
 * The framebuffer that DrawMessageCharacter last drew a message into, which
 * the main loop keeps on screen once set
 */
extern u16* g_MessageFramebuffer;

void SetUpDebugTextDrawing(Gfx** displayListHead);
void DrawTextIntoTexture(s8* text, s32 length, u16* texture);
void PrintMessage(s8* text, u32 column, u32 row);
void DrawMessageCharacter(s8 character, u32 column, u32 row);
void ClearDebugText(void);
void DrawDebugText(Gfx** displayListHead);
void SetDebugTextHighlight(Boolean isHighlighted);
void PutDebugText(s8* text, s32 column, s32 row);
void PrintDebugText(char* text, s32 column, s32 row);
void PrintDebugHex(u32 value, s32 column, s32 row);
void PrintDebugPaddedHex(u32 value, s32 column, s32 row);
void AddToDebugLog(char* text);
void ShowDebugLog(void);

#endif
