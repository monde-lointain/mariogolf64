#ifndef SCREEN_LAYERS_H
#define SCREEN_LAYERS_H

#include "common.h"
#include "controller.h"

/* StartWipe's kinds */
#define WIPE_NONE 0
#define WIPE_CLOSING 1
#define WIPE_OPENING 2
#define WIPE_OPENING_FAST 3

/* The prompts that OpenPrompt opens */
#define NO_PROMPT -1
#define PROMPT_OVERWRITE_GAME 0
#define PROMPT_END_GAME 1
#define PROMPT_OVERWRITE_REPLAY 2
#define PROMPT_SKIP_REPLAY 5
#define PROMPT_BRING_MORE_CHARACTERS 6 /* From the Game Boy cartridges, from the Transmit menu */
#define PROMPT_OVERWRITE_RECORDS 10 /* On the cartridge, from the Transmit menu */

void DrawTextureRect(Gfx **displayListHead, s32 xl, s32 yl, s32 xh, s32 yh, s32 tile, s32 s, s32 t, s32 dsdx,
                     s32 dtdy);
void LoadSmallPalette(Gfx **displayListHead, s32 palette, void *tlut);
void LoadLargePalette(Gfx **displayListHead, void *tlut);
void LoadNibbleTextureBlock(Gfx **displayListHead, void *texels, s32 format, s32 width, s32 height, s32 palette,
                            s32 cms, s32 cmt, s32 masks, s32 maskt, s32 shifts, s32 shiftt);
void LoadTextureBlock(Gfx **displayListHead, void *texels, s32 format, s32 size, s32 width, s32 height,
                      s32 palette, s32 cms, s32 cmt, s32 masks, s32 maskt, s32 shifts, s32 shiftt);
void InitScreenLayers(void);
void LoadWipeShape(s32 courseIndex);
void StopWipe(void);
s32 GetWipeKind(void);
s32 GetWipeProgress(void);
void StartWipe(s32 kind);
void DrawWipe(Gfx **displayListHead);
void StopScreenFade(void);
s32 HasScreenFadeEnded(void);
void StartScreenFadeOut(s32 red, s32 green, s32 blue, s32 frameCount);
void StartScreenFadeIn(s32 red, s32 green, s32 blue, s32 frameCount);
void DrawScreenFade(Gfx **displayListHead);
s32 IsPromptClosed(void);
void OpenPrompt(s32 prompt);
s32 UpdatePrompt(ControllerState *controller);
void DrawPrompt(Gfx **displayListHead);
void ShowHalfTurn(void);
void HideHalfTurn(void);
void DrawHalfTurn(Gfx **displayListHead);

#endif
