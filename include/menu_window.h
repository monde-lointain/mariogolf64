#ifndef MENU_WINDOW_H
#define MENU_WINDOW_H

#include <ultra64.h>

enum MenuWindowState {
    MENU_WINDOW_OPENING,
    MENU_WINDOW_OPEN,
    MENU_WINDOW_CLOSING,
    MENU_WINDOW_CLOSED
};

typedef s32 MenuWindowState;

enum MenuWindowCheck {
    MENU_WINDOW_CHECK_MOVING = -1,
    MENU_WINDOW_CHECK_NO,
    MENU_WINDOW_CHECK_YES
};

typedef s32 MenuWindowCheck;

void ResetMenuWindow(void);
void ShowMenuWindow(void);
void CloseMenuWindow(void);
s32 IsMenuWindowOpen(void);
s32 IsMenuWindowClosed(void);
void LoadMenuWindowImage(s32 imageIndex);

#endif
