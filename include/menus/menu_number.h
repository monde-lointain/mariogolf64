/*
 * The numbers and signs that the menus draw, defined in course_menu.c
 */

#ifndef MENU_NUMBER_H
#define MENU_NUMBER_H

#include <ultra64.h>

// How DrawMenuNumber marks a number: with nothing, a cross before it, its
// sign (a plus tinted red, a minus tinted green, or plus-minus for zero),
// or a red or green tint
enum MenuNumberStyle {
    MENU_NUMBER_PLAIN,
    MENU_NUMBER_TIMES,
    MENU_NUMBER_SIGNED,
    MENU_NUMBER_RED_TINT,
    MENU_NUMBER_GREEN_TINT
};

typedef u8 MenuNumberStyle;

// The signs that DrawMenuSign draws
enum MenuSign {
    MENU_SIGN_DASH,
    MENU_SIGN_TIMES,
    MENU_SIGN_CIRCLE
};

typedef u8 MenuSign;
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
void DrawMenuSign(Gfx **displayListHead, s32 x, s32 y, MenuSign sign);

#endif
