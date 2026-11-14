#ifndef ROUND_EVENTS_H
#define ROUND_EVENTS_H

#include <ultra64.h>

#include "camera.h"
#include "language.h"
#include "player.h"
#include "text.h"

#define FLY_GUY_HIDDEN (-1)

enum ContactEffect {
    CONTACT_EFFECT_NONE = -1,
    CONTACT_EFFECT_POWER_WOOD,
    CONTACT_EFFECT_POWER_IRON,
    CONTACT_EFFECT_SHOT,
    CONTACT_EFFECT_PUTT,
    CONTACT_EFFECT_FADE = 10
};

typedef s32 ContactEffect;

#define IsShownContactEffect(effect) \
    ((effect) == CONTACT_EFFECT_POWER_WOOD || \
        (effect) == CONTACT_EFFECT_POWER_IRON || \
        (effect) == CONTACT_EFFECT_SHOT || \
        (effect) == CONTACT_EFFECT_PUTT)

enum ContactStyle {
    CONTACT_STYLE_MARIO,
    CONTACT_STYLE_BOWSER,
    CONTACT_STYLE_WARIO,
    CONTACT_STYLE_RAINBOW,
    CONTACT_STYLE_PLAIN,
    CONTACT_STYLE_POWER_IRON,
    CONTACT_STYLE_SHOT,
    CONTACT_STYLE_PUTT,
    CONTACT_STYLE_GAME_BOY_BALL_5,
    CONTACT_STYLE_GAME_BOY_BALL_6,
    CONTACT_STYLE_GAME_BOY_BALL_7
};

typedef s32 ContactStyle;

#define ShowsLongShotEffect(style) \
    (((style) != CONTACT_STYLE_GAME_BOY_BALL_7) & ((style) != CONTACT_STYLE_WARIO))

#define BANNER_HIDE_NICE_SHOT (-2)
#define BANNER_NONE (-1)
#define BANNER_NICE_SHOT 0
#define BANNER_NICE_ON 1
#define BANNER_NICE_APPROACH 2
#define BANNER_NICE_TOUCH 3
#define BANNER_CHIP_IN 4
#define BANNER_PIN_SHOT 5
#define BANNER_FLAG_SHOT 6
#define BANNER_EXCELLENT 7
#define BANNER_GREAT 8
#define BANNER_FINISH 9
#define BANNER_COMPLETE 10
#define BANNER_HOLE_IN_ONE 11
#define BANNER_ALBATROSS 12
#define BANNER_EAGLE 13
#define BANNER_BIRDIE 14
#define BANNER_PAR 15
#define BANNER_BOGEY 16
#define BANNER_STROKES_OVER_PAR 16

// The frames since the contact effect started, or NO_CONTACT_EFFECT_AGE
// while none is shown
#define NO_CONTACT_EFFECT_AGE CONTACT_EFFECT_NONE

extern s32 g_ContactEffectAge;

void StartBanner(s32 banner);
void ShowWindFlyGuy(s32 show);
ContactStyle GetContactStyle(Player *player);
void LoadContactImages(Player *player);
Boolean IsContactEffectShown(void);
void StartContactEffect(s32 effect);
void ShowFlipMessage(MessageId messageId);

void DrawBanner(Gfx **displayListHead);
void DrawContactEffect(Gfx **displayListHead, CameraMatrices *matrices);
void DrawFlipMessage(Gfx **displayListHead);
void DrawWindFlyGuy(Gfx **displayListHead);

#endif
