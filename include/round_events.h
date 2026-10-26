#ifndef ROUND_EVENTS_H
#define ROUND_EVENTS_H

#include <ultra64.h>

/* ShowWindFlyGuy's argument that hides the Fly Guy */
#define FLY_GUY_HIDDEN (-1)

/* The contact effect that StartContactEffect shows when no effect is shown */
#define CONTACT_EFFECT_NONE (-1)

extern s32 g_ContactEffect;

void ShowWindFlyGuy(s32 show);
s32 IsContactEffectShown(void);
void StartContactEffect(s32 effect);

#endif
