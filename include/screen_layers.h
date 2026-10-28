#ifndef SCREEN_LAYERS_H
#define SCREEN_LAYERS_H

#include "common.h"

#include "controller.h"

/* StartWipe's kinds */
enum WipeState {
  WIPE_NONE,
  WIPE_CLOSING,
  WIPE_OPENING,
  WIPE_OPENING_FAST
};

/* The prompts that OpenPrompt opens */
#define NO_PROMPT -1
#define PROMPT_OVERWRITE_GAME 0
#define PROMPT_END_GAME 1
#define PROMPT_OVERWRITE_REPLAY 2
#define PROMPT_SKIP_REPLAY 5
/* From the Game Boy cartridges, from the Transmit menu */
#define PROMPT_BRING_MORE_CHARACTERS 6
/* Under a tournament round's score password */
#define PROMPT_SCORE_PASSWORD 9
/* On the cartridge, from the Transmit menu */
#define PROMPT_OVERWRITE_RECORDS 10

/* The screen fade's kinds */
enum FadeDirection {
  FADE_NONE = -1,
  FADE_OUT,
  FADE_IN
};

extern s32 g_ScreenFadeDuration;
extern s32 g_ScreenFadeFramesLeft;
extern s32 g_ScreenFadeKind;
extern s32 g_ScreenFadeRed;
extern s32 g_ScreenFadeGreen;
extern s32 g_ScreenFadeBlue;
/* Draws textures with their own colors, cut at their alpha */
extern Gfx g_DecalTextureSetup[];

void DrawTextureRect(Gfx** displayListHead, s32 xl, s32 yl, s32 xh, s32 yh,
    s32 tile, s32 s, s32 t, s32 dsdx, s32 dtdy);
void LoadSmallPalette(Gfx** displayListHead, s32 palette, void* tlut);
void LoadLargePalette(Gfx** displayListHead, void* tlut);
void LoadNibbleTextureBlock(Gfx** displayListHead, void* texels, s32 format,
    s32 width, s32 height, s32 palette, s32 cms, s32 cmt, s32 masks, s32 maskt,
    s32 shifts, s32 shiftt);
void LoadTextureBlock(Gfx** displayListHead, void* texels, s32 format, s32 size,
    s32 width, s32 height, s32 palette, s32 cms, s32 cmt, s32 masks, s32 maskt,
    s32 shifts, s32 shiftt);
void InitScreenLayers(void);
void LoadWipeShape(s32 courseIndex);
void StopWipe(void);
s32 GetWipeKind(void);
s32 GetWipeProgress(void);
void StartWipe(s32 kind);
void DrawWipe(Gfx** displayListHead);
void StopScreenFade(void);
Boolean HasScreenFadeEnded(void);
void StartScreenFadeOut(s32 red, s32 green, s32 blue, s32 frameCount);
void StartScreenFadeIn(s32 red, s32 green, s32 blue, s32 frameCount);
void DrawScreenFade(Gfx** displayListHead);
Boolean IsPromptClosed(void);
void OpenPrompt(s32 prompt);
s32 UpdatePrompt(ControllerState* controller);
void DrawPrompt(Gfx** displayListHead);
void ShowHalfTurn(void);
void HideHalfTurn(void);
void DrawHalfTurn(Gfx** displayListHead);

#endif
