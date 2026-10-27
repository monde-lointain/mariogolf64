#ifndef DEBUG_TEXT_H
#define DEBUG_TEXT_H

#include "common.h"

void SetUpDebugTextDrawing(Gfx** displayListHead);
void DrawTextIntoTexture(s8* text, s32 length, u16* texture);
void PrintMessage(s8* text, u32 column, u32 row);
void DrawMessageCharacter(s8 character, u32 column, u32 row);
void ClearDebugText(void);
void DrawDebugText(Gfx** displayListHead);
void SetDebugTextHighlight(s32 isHighlighted);
void PutDebugText(s8* text, s32 column, s32 row);
void PrintDebugText(char* text, s32 column, s32 row);
void PrintDebugHex(u32 value, s32 column, s32 row);
void PrintDebugPaddedHex(u32 value, s32 column, s32 row);
void AddToDebugLog(char* text);
void ShowDebugLog(void);

#endif
