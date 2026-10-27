#ifndef ROUND_EVENTS_H
#define ROUND_EVENTS_H

#include <ultra64.h>

#include "save.h"

/* ShowWindFlyGuy's argument that hides the Fly Guy */
#define FLY_GUY_HIDDEN (-1)

/* The contact effects that StartContactEffect shows where the club meets the
 * ball */
#define CONTACT_EFFECT_NONE (-1)
#define CONTACT_EFFECT_POWER_WOOD 0 /* In the character's own style */
#define CONTACT_EFFECT_POWER_IRON 1 /* Only for the longer irons */
#define CONTACT_EFFECT_SHOT 2
#define CONTACT_EFFECT_PUTT 3
/* Or any other value: fades the shown effect out */
#define CONTACT_EFFECT_FADE 10

/* The styles of contact effect; each character has one, and Game Boy characters
 * one for their ball */
#define CONTACT_STYLE_MARIO 0 /* For Mario, Baby Mario and Metal Mario */
#define CONTACT_STYLE_BOWSER 1
#define CONTACT_STYLE_WARIO 2
#define CONTACT_STYLE_HEAVY_HITTER 3 /* For Maple and Donkey Kong */
#define CONTACT_STYLE_PLAIN 4
/* The styles that every character shares */
#define CONTACT_STYLE_POWER_IRON 5
#define CONTACT_STYLE_SHOT 6
#define CONTACT_STYLE_PUTT 7
#define CONTACT_STYLE_GAME_BOY_BALL_5 8
#define CONTACT_STYLE_GAME_BOY_BALL_6 9
#define CONTACT_STYLE_GAME_BOY_BALL_7 10

/* The word banners that StartBanner flies in, named after their text */
/* Hides the banner only if it is BANNER_NICE_SHOT */
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
#define BANNER_FINISH 9    /* Speed Golf */
#define BANNER_COMPLETE 10 /* Ring Shot */
#define BANNER_HOLE_IN_ONE 11
#define BANNER_ALBATROSS 12
#define BANNER_EAGLE 13
#define BANNER_BIRDIE 14
#define BANNER_PAR 15
#define BANNER_BOGEY 16
/* Plus two or more strokes over par, which the banner shows as "+N" */
#define BANNER_STROKES_OVER_PAR 16

extern s32 g_ContactEffect;

void StartBanner(s32 banner);
void ShowWindFlyGuy(s32 show);
s32 GetContactStyle(UnkB8* player);
void LoadContactImages(UnkB8* player);
s32 IsContactEffectShown(void);
void StartContactEffect(s32 effect);

#endif
