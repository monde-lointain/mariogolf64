#ifndef SCREEN_LAYERS_H
#define SCREEN_LAYERS_H

#include <ultra64.h>

#include "controller.h"
#include "language.h"

/* StartWipe's kinds */
enum WipeState {
    WIPE_NONE,
    WIPE_CLOSING,
    WIPE_OPENING,
    WIPE_OPENING_FAST
};

/* GetWipeProgress's values when the screen is fully covered and fully shown */
#define WIPE_CLOSED_PROGRESS 0
#define WIPE_OPEN_PROGRESS 20

/* The prompts that OpenPrompt opens */
enum PromptId {
    NO_PROMPT = -1,
    PROMPT_OVERWRITE_GAME,
    PROMPT_END_GAME,
    PROMPT_OVERWRITE_REPLAY,
    /* The pause menu's give up */
    PROMPT_GIVE_UP,
    /* The pause menu's quit */
    PROMPT_QUIT_GAME,
    PROMPT_SKIP_REPLAY,
    /* From the Game Boy cartridges, from the Transmit menu */
    PROMPT_BRING_MORE_CHARACTERS,
    /* The pause menu's quit, in a game brought over from a cartridge */
    PROMPT_QUIT_BROUGHT_OVER_GAME,
    /* The pause menu's PAUSE_ACTION_QUIT_CLUB_TOURNAMENT */
    PROMPT_QUIT_CLUB_TOURNAMENT,
    /* Under a tournament round's score password */
    PROMPT_SCORE_PASSWORD,
    /* On the cartridge, from the Transmit menu */
    PROMPT_OVERWRITE_RECORDS
};

/* What UpdatePrompt returns: the chosen answer's result */
enum PromptResult {
    /* The prompt is still open */
    NO_PROMPT_RESULT = -1,
    PROMPT_RESULT_SAVE_GAME,
    PROMPT_RESULT_SAVE_REPLAY,
    /* No to a save prompt, which goes back to the save menu */
    PROMPT_RESULT_STAY_IN_SAVE_MENU,
    PROMPT_RESULT_END_GAME,
    PROMPT_RESULT_QUIT_GAME,
    /* No to giving up or quitting */
    PROMPT_RESULT_RESUME,
    PROMPT_RESULT_GIVE_UP_HOLE,
    PROMPT_RESULT_SKIP_REPLAY,
    PROMPT_RESULT_BRING_MORE,
    PROMPT_RESULT_BRING_NO_MORE,
    /* The answers to PROMPT_SCORE_PASSWORD and PROMPT_OVERWRITE_RECORDS */
    PROMPT_RESULT_YES,
    PROMPT_RESULT_NO
};

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

void DrawTextureRect(
    Gfx **displayListHead,
    s32 xl,
    s32 yl,
    s32 xh,
    s32 yh,
    s32 tile,
    s32 s,
    s32 t,
    s32 dsdx,
    s32 dtdy
);
void LoadSmallPalette(Gfx **displayListHead, s32 palette, void *tlut);
void LoadLargePalette(Gfx **displayListHead, void *tlut);
void LoadNibbleTextureBlock(
    Gfx **displayListHead,
    void *texels,
    s32 format,
    s32 width,
    s32 height,
    s32 palette,
    s32 cms,
    s32 cmt,
    s32 masks,
    s32 maskt,
    s32 shifts,
    s32 shiftt
);
void LoadTextureBlock(
    Gfx **displayListHead,
    void *texels,
    s32 format,
    s32 size,
    s32 width,
    s32 height,
    s32 palette,
    s32 cms,
    s32 cmt,
    s32 masks,
    s32 maskt,
    s32 shifts,
    s32 shiftt
);
void InitScreenLayers(void);
void LoadWipeShape(s32 courseIndex);
void StopWipe(void);
s32 GetWipeKind(void);
s32 GetWipeProgress(void);
void StartWipe(s32 kind);
void DrawWipe(Gfx **displayListHead);
void StopScreenFade(void);
Boolean HasScreenFadeEnded(void);
void StartScreenFadeOut(s32 red, s32 green, s32 blue, s32 frameCount);
void StartScreenFadeIn(s32 red, s32 green, s32 blue, s32 frameCount);
void DrawScreenFade(Gfx **displayListHead);
Boolean IsPromptClosed(void);
void OpenPrompt(s32 prompt);
s32 UpdatePrompt(ControllerState *controller);
void DrawPrompt(Gfx **displayListHead);
void ShowHalfTurn(void);
void HideHalfTurn(void);
void DrawHalfTurn(Gfx **displayListHead);
Boolean IsHalfTurnDismissed(ControllerState *controller);

#endif
