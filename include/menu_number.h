#ifndef MENU_NUMBER_H
#define MENU_NUMBER_H

#include <ultra64.h>

/* The numbers and signs that the menus draw, defined in course_menu.c */

/*
 * How DrawMenuNumber marks a number: with nothing, a cross before it, its
 * sign (a plus tinted red, a minus tinted green, or plus-minus for zero),
 * or a red or green tint
 */
enum MenuNumberStyle {
  MENU_NUMBER_PLAIN,
  MENU_NUMBER_TIMES,
  MENU_NUMBER_SIGNED,
  MENU_NUMBER_RED_TINT,
  MENU_NUMBER_GREEN_TINT
};

/* The signs that DrawMenuSign draws */
enum MenuSign {
  MENU_SIGN_DASH,
  MENU_SIGN_TIMES,
  MENU_SIGN_CIRCLE
};

void DrawNarrowTwoDigitMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void DrawOneDigitMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void DrawTwoDigitMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void DrawUnalignedMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void DrawMenuSign(Gfx** displayListHead, s32 x, s32 y, u8 sign);

#endif
