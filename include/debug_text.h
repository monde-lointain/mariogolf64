#ifndef DEBUG_TEXT_H
#define DEBUG_TEXT_H

#include <ultra64.h>

#include "compiler.h"
#include "language.h"

extern u16 *g_MessageFramebuffer;

#define DEBUG_TEXT_BUFFER_SIZE 128

#define SIMULATION_FAULT_TEXT_COLUMN 10
#define SIMULATION_FAULT_TEXT_ROW 12
#define NORMAL_ERROR_TEXT_COLUMN 3
#define NORMAL_ERROR_TEXT_ROW 15

void SetUpDebugTextDrawing(Gfx **displayListHead);
void DrawTextIntoTexture(READ_ONLY char *text, s32 length, u16 *texture);
void PrintMessage(s8 *text, u32 column, u32 row);
void DrawMessageCharacter(s8 character, u32 column, u32 row);
void ClearDebugText(void);
void DrawDebugText(Gfx **displayListHead);
void SetDebugTextHighlight(Boolean isHighlighted);
void PutDebugText(s8 *text, s32 column, s32 row);
void PrintDebugText(const char *text, s32 column, s32 row);
void PrintDebugHex(u32 number, s32 column, s32 row);
void PrintDebugPaddedHex(u32 number, s32 column, s32 row);
void AddToDebugLog(char *text);
void ShowDebugLog(void);

#endif
