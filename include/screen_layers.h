#ifndef SCREEN_LAYERS_H
#define SCREEN_LAYERS_H

#include <ultra64.h>

#include "asset.h"
#include "c_language.h"
#include "controller.h"
#include "course.h"
#include "gbi_helpers.h"

enum WipeState {
    WIPE_NONE,
    WIPE_CLOSING,
    WIPE_OPENING,
    WIPE_OPENING_FAST
};

typedef s32 WipeState;

#define WIPE_CLOSED_PROGRESS 0
#define WIPE_OPEN_PROGRESS 20

#define PROMPT_TEXT_SLOT 0

enum PromptId {
    NO_PROMPT = -1,
    PROMPT_OVERWRITE_GAME,
    PROMPT_END_GAME,
    PROMPT_OVERWRITE_REPLAY,
    PROMPT_GIVE_UP,
    PROMPT_QUIT_GAME,
    PROMPT_SKIP_REPLAY,
    PROMPT_BRING_MORE_CHARACTERS,
    PROMPT_QUIT_BROUGHT_OVER_GAME,
    PROMPT_QUIT_CLUB_TOURNAMENT,
    PROMPT_SCORE_PASSWORD,
    PROMPT_OVERWRITE_RECORDS
};

typedef s32 PromptId;

enum PromptResult {
    NO_PROMPT_RESULT = -1,
    PROMPT_RESULT_SAVE_GAME,
    PROMPT_RESULT_SAVE_REPLAY,
    PROMPT_RESULT_STAY_IN_SAVE_MENU,
    PROMPT_RESULT_END_GAME,
    PROMPT_RESULT_QUIT_GAME,
    PROMPT_RESULT_RESUME,
    PROMPT_RESULT_GIVE_UP_HOLE,
    PROMPT_RESULT_SKIP_REPLAY,
    PROMPT_RESULT_BRING_MORE,
    PROMPT_RESULT_BRING_NO_MORE,
    PROMPT_RESULT_YES,
    PROMPT_RESULT_NO
};

typedef s32 PromptResult;

enum FadeDirection {
    FADE_NONE = -1,
    FADE_OUT,
    FADE_IN
};

typedef s32 FadeDirection;

extern s32 g_ScreenFadeDuration;
extern s32 g_ScreenFadeFramesLeft;
extern FadeDirection g_ScreenFadeKind;
extern RgbColor g_ScreenFadeColor;
extern Gfx g_DecalTextureSetup[];

void DrawTextureRectangle(
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
void LoadSmallPalette(Gfx **displayListHead, s32 paletteIndex, PaletteColor *tlut);
void LoadLargePalette(Gfx **displayListHead, PaletteColor *tlut);
void LoadNibbleTextureBlock(
    Gfx **displayListHead,
    void *texels,
    s32 format,
    s32 width,
    s32 height,
    s32 paletteIndex,
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
    s32 paletteIndex,
    s32 cms,
    s32 cmt,
    s32 masks,
    s32 maskt,
    s32 shifts,
    s32 shiftt
);
void InitScreenLayers(void);
void LoadWipeShape(CourseIndex courseIndex);
void StopWipe(void);
s32 GetWipeKind(void);
s32 GetWipeProgress(void);
void StartWipe(WipeState kind);
void DrawWipe(Gfx **displayListHead);
void StopScreenFade(void);
Boolean IsScreenFadeDone(void);
void StartScreenFadeOut(s32 red, s32 green, s32 blue, s32 frameCount);
void StartScreenFadeIn(s32 red, s32 green, s32 blue, s32 frameCount);
void DrawScreenFade(Gfx **displayListHead);
Boolean IsPromptClosed(void);
void OpenPrompt(PromptId promptIndex);
PromptResult UpdatePrompt(ControllerState *controller);
void DrawPrompt(Gfx **displayListHead);
void ShowHalfTurn(void);
void HideHalfTurn(void);
void DrawHalfTurn(Gfx **displayListHead);
Boolean IsHalfTurnDismissed(ControllerState *controller);

#endif
