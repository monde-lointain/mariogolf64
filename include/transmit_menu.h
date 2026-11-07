#ifndef TRANSMIT_MENU_H
#define TRANSMIT_MENU_H

#include <ultra64.h>

/* The transmit menu, which exchanges data with a Game Boy */

void LoadTransmitMenu(void);
void FreeTransmitMenu(void);
void UpdateTransmitMenu(void);

void DrawTransmitMenu(Gfx **displayListHead);

#endif
