#ifndef ROUND_EVENTS_H
#define ROUND_EVENTS_H

#include <ultra64.h>

/* ShowWindFlyGuy's argument that hides the Fly Guy */
#define FLY_GUY_HIDDEN (-1)

/* The contact effect that StartContactEffect shows when no effect is shown */
#define CONTACT_EFFECT_NONE (-1)

/* The word banners that StartBanner flies in, named after their text */
#define BANNER_HIDE_NICE_SHOT (-2) /* Hides the banner only if it is BANNER_NICE_SHOT */
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
s32 IsContactEffectShown(void);
void StartContactEffect(s32 effect);

#endif
