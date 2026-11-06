#ifndef MENU_WINDOW_H
#define MENU_WINDOW_H

#include <ultra64.h>

/* The menus' popup window, which tournament_menu.c opens and closes */

/* The states of the menus' popup window, which opens and closes by scaling */
enum MenuWindowState {
  MENU_WINDOW_OPENING,
  MENU_WINDOW_OPEN,
  MENU_WINDOW_CLOSING,
  MENU_WINDOW_CLOSED
};

/*
 * The answers of IsMenuWindowOpen and IsMenuWindowClosed. MOVING is nonzero, so
 * a caller that only tests the answer counts a moving window as open or closed.
 */
enum MenuWindowCheck {
  MENU_WINDOW_CHECK_MOVING = -1,
  MENU_WINDOW_CHECK_NO,
  MENU_WINDOW_CHECK_YES
};

void ResetMenuWindow(void);
void ShowMenuWindow(void);
void CloseMenuWindow(void);
/* These return MENU_WINDOW_CHECK_* values */
s32 IsMenuWindowOpen(void);
s32 IsMenuWindowClosed(void);
void LoadMenuWindowImage(s32 imageIndex);

#endif
