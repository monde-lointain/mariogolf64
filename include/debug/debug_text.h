/*
 * The debug text grid, its log, and the messages drawn straight into the
 * framebuffers.
 */

#ifndef DEBUG_TEXT_H
#define DEBUG_TEXT_H

#include <ultra64.h>

#include "system/c_language.h"
#include "system/compiler.h"

// The framebuffer that DrawMessageCharacter last drew a message into, which
// the main loop keeps on screen once set
extern u16 *g_MessageFramebuffer;

// The size of the buffers that lines of debug text are formatted into
#define DEBUG_TEXT_BUFFER_SIZE 128
#define DEBUG_GLYPH_SIZE 8
#define SIMULATION_FAULT_TEXT_COLUMN 10
#define SIMULATION_FAULT_TEXT_ROW 12
// Where the plane routines report a triangle with no normal
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
