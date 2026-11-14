#ifndef MENU_NUMBER_H
#define MENU_NUMBER_H

#include <ultra64.h>

enum MenuNumberStyle {
    MENU_NUMBER_PLAIN,
    MENU_NUMBER_TIMES,
    MENU_NUMBER_SIGNED,
    MENU_NUMBER_RED_TINT,
    MENU_NUMBER_GREEN_TINT
};

typedef u8 MenuNumberStyle;

enum MenuSign {
    MENU_SIGN_DASH,
    MENU_SIGN_TIMES,
    MENU_SIGN_CIRCLE
};

void DrawNarrowTwoDigitMenuNumber(
    Gfx **displayListHead,
    s32 x,
    s32 y,
    s32 number,
    MenuNumberStyle mark
);
void DrawOneDigitMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 number, MenuNumberStyle mark);
void DrawTwoDigitMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 number, MenuNumberStyle mark);
void DrawUnalignedMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 number, MenuNumberStyle mark);
void DrawMenuSign(Gfx **displayListHead, s32 x, s32 y, u8 sign);

#endif
