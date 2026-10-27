#ifndef MENU_H
#define MENU_H

#include "common.h"

/* The screens that the menu overlay shows, which g_MenuScreen picks */
#define MENU_SCREEN_TITLE 0
#define MENU_SCREEN_MAIN 2
#define MENU_SCREEN_OPTIONS 4
#define MENU_SCREEN_CLUBHOUSE 9
#define MENU_SCREEN_EXIT 19                  /* Loads what the chosen game needs and leaves the menu */
#define MENU_SCREEN_MAIN_WITHOUT_TRANSMIT 28 /* The main menu when no Transfer Pak holds Mario Golf for Game Boy */
extern s32 g_MenuScreen;

/* The buttons of every controller: those repeating while held, and those just pressed */
extern s32 g_MenuRepeatButtons;
extern s32 g_MenuPressedButtons;

/* The sounds of the menus, and how loud each plays */
#define MENU_SOUND_CHANNEL 15
#define SOUND_MENU_CURSOR 0x5C
#define SOUND_MENU_CANCEL 0x5F
#define SOUND_MENU_START 0x50
#define MENU_VOLUME_SOFT 80
#define MENU_VOLUME_MEDIUM 100
#define MENU_VOLUME_FULL 127

/* The choices that the menus remember */
typedef struct MenuChoices {
    /* 0x00 */ s8 unk0;
    /* 0x01 */ s8 unk1;
    /* 0x02 */ s8 unk2;
    /* 0x03 */ s8 mainChoice;
    /* 0x04 */ s8 mainChoiceWithoutTransmit;
    /* 0x05 */ s8 clubhouseChoice;
    /* 0x06 */ s8 unk6[0x10];
    /* 0x16 */ s8 unk16;
    /* 0x17 */ s8 wipeShape; /* The course whose shape the closing wipe takes */
} MenuChoices; /* size = 0x18 */
extern MenuChoices g_MenuChoices;

/* An image buffer of the menu overlay */
extern u8 g_MenuImageBuffer[];

/* The states of the menus' popup window, which opens and closes by scaling */
#define MENU_WINDOW_OPENING 0
#define MENU_WINDOW_OPEN 1
#define MENU_WINDOW_CLOSING 2
#define MENU_WINDOW_CLOSED 3

void ResetMenuWindow(void);
void ShowMenuWindow(void);
void CloseMenuWindow(void);
void LoadMenuWindowImage(s32 imageIndex);

/* The title screen's song */
#define SONG_TITLE 0x27

#endif
