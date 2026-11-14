/*
 * What the round shows between shots: banners, flip messages, movies and
 * contact effects.
 */

#ifndef ROUND_EVENTS_H
#define ROUND_EVENTS_H

#include <ultra64.h>

#include "camera/camera.h"
#include "round/player.h"
#include "system/c_language.h"
#include "text/text.h"

// ShowWindFlyGuy's argument
enum FlyGuyVisibility {
    FLY_GUY_HIDDEN = -1,
    FLY_GUY_SHOWN = 1
};

typedef s32 FlyGuyVisibility;

// The contact effects that StartContactEffect shows where the club meets the
// ball
enum ContactEffect {
    CONTACT_EFFECT_NONE = -1,
    CONTACT_EFFECT_POWER_WOOD, // In the character's own style
    CONTACT_EFFECT_POWER_IRON, // Only for the longer irons
    CONTACT_EFFECT_SHOT,
    CONTACT_EFFECT_PUTT,
    // Or any other value: fades the shown effect out
    CONTACT_EFFECT_FADE = 10
};

typedef s32 ContactEffect;

// Whether a ContactEffect shows an effect, in one of the styles
#define IsShownContactEffect(effect) \
    ((effect) == CONTACT_EFFECT_POWER_WOOD || \
        (effect) == CONTACT_EFFECT_POWER_IRON || \
        (effect) == CONTACT_EFFECT_SHOT || \
        (effect) == CONTACT_EFFECT_PUTT)

// The styles of contact effect; each character has one, and Game Boy characters
// one for their ball
enum ContactStyle {
    CONTACT_STYLE_MARIO, // For Mario, Baby Mario and Metal Mario
    CONTACT_STYLE_BOWSER,
    CONTACT_STYLE_WARIO,
    CONTACT_STYLE_RAINBOW, // Yoshi's and Donkey Kong's rainbow trail
    CONTACT_STYLE_PLAIN,
    // The styles that every character shares
    CONTACT_STYLE_POWER_IRON,
    CONTACT_STYLE_SHOT,
    CONTACT_STYLE_PUTT,
    CONTACT_STYLE_GAME_BOY_BALL_5,
    CONTACT_STYLE_GAME_BOY_BALL_6,
    CONTACT_STYLE_GAME_BOY_BALL_7
};

typedef s32 ContactStyle;

// Whether a ContactStyle shows the long-shot effect; Wario's and the Game Boy's
// ball 7 show a spark trail instead. The original tests with & rather than &&
#define ShowsLongShotEffect(style) \
    (((style) != CONTACT_STYLE_GAME_BOY_BALL_7) & ((style) != CONTACT_STYLE_WARIO))

// The word banners that StartBanner flies in, named after their text
// Hides the banner only if it is BANNER_NICE_SHOT
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
// Speed Golf's
#define BANNER_FINISH 9
// Ring Shot's
#define BANNER_COMPLETE 10
#define BANNER_HOLE_IN_ONE 11
#define BANNER_ALBATROSS 12
#define BANNER_EAGLE 13
#define BANNER_BIRDIE 14
#define BANNER_PAR 15
#define BANNER_BOGEY 16
// Plus two or more strokes over par, which the banner shows as "+N"
#define BANNER_STROKES_OVER_PAR 16

#define NO_CONTACT_EFFECT_AGE (-1)
extern s32 g_ContactEffectAge;

void StartBanner(s32 banner);
// Shows the Fly Guy that warns of a strong wind, or hides it given FLY_GUY_HIDDEN
void ShowWindFlyGuy(FlyGuyVisibility show);
ContactStyle GetContactStyle(Player *player);
void LoadContactImages(Player *player);
Boolean IsContactEffectShown(void);
// Places a contact effect at the ball and sizes it for its style; CONTACT_EFFECT_NONE removes it
// and CONTACT_EFFECT_FADE fades it out
void StartContactEffect(ContactEffect effect);
void ShowFlipMessage(MessageId messageId);

void DrawBanner(Gfx **displayListHead);
void DrawContactEffect(Gfx **displayListHead, CameraMatrices *matrices);
void DrawFlipMessage(Gfx **displayListHead);
void DrawWindFlyGuy(Gfx **displayListHead);

#endif
