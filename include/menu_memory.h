#ifndef MENU_MEMORY_H
#define MENU_MEMORY_H

#include <ultra64.h>

#include "save.h"

/* The values of unkE, which func_80217B88 also turns into GameState's
 * betRates */
#define MENU_UNKE_COUNT 8

/* The presets of func_80217B88; two of them have a variant that four players
 * take instead */
#define MENU_UNKE_PRESET_COUNT 7
#define MENU_UNKE_PRESET_STROKE 0
#define MENU_UNKE_PRESET_STROKE_FOUR_PLAYER 1
#define MENU_UNKE_PRESET_MATCH_GAME 2
#define MENU_UNKE_PRESET_SKINS_MATCH 3
#define MENU_UNKE_PRESET_CLUB_SLOTS 4
#define MENU_UNKE_PRESET_MINI_GOLF 5
#define MENU_UNKE_PRESET_MINI_GOLF_FOUR_PLAYER 6
/* Past func_80217B88's presets, so that it clears the values instead */
#define MENU_UNKE_PRESET_NONE 10

/* What unk18 holds once the C buttons choose the holes to play */
enum HolesChoice {
  HOLES_CHOICE_UNK0,
  HOLES_CHOICE_FRONT_NINE,
  HOLES_CHOICE_BACK_NINE,
  HOLES_CHOICE_ALL
};

/* What the menus keep between visits, such as the last choices made */
typedef struct MenuMemory {
  /* 0x00 */ s8
      playerCountChoice; /* The title screen's player count, less one */
  /* 0x01 */ s8 titleChoice; /* The title screen's row: a new game, the saved
                                games or the menus */
  /* 0x02 */ s8 unk2;
  /* 0x03 */ s8 characterColumns[MAX_PLAYER_COUNT]; /* Each player's character
                                                       select cursor column */
  /* 0x07 */ s8 characterRows[MAX_PLAYER_COUNT]; /* And row */
  /* 0x0B */ s8 modeChoice; /* The mode select cursor's place in the player
                               count's modes */
  /* 0x0C */ s8 modeSpin; /* How far the modes still have to turn to reach it */
  /* 0x0D */ s8 unkD;
  /* 0x0E */ s8
      unkE[MENU_UNKE_COUNT]; /* Set from one of func_80217B88's presets */
  /* 0x16 */ s8 courseChoice; /* The course select's course: an open one, or the
                                 next one still shut */
  /* 0x17 */ s8
      courseSpin; /* How far the courses still have to turn to reach it */
  /* 0x18 */ s8 unk18;
  /* 0x19 */ s8 unk19;
  /* 0x1A */ s8 unk1A;
  /* 0x1B */ s8 mainChoice;
  /* 0x1C */ s8 mainChoiceWithoutTransmit;
  /* 0x1D */ s8 clubhouseChoice;
  /* 0x1E */ s8 optionsChoice;
  /* 0x1F */ s8 eraseChoice; /* The erase window's yes or no */
  /* 0x20 */ s8 helpTopicIndex;
  /* 0x21 */ u8 unk21[0x29 - 0x21];
  /* 0x29 */ s8 unk29;
  /* 0x2A */ s8 unk2A;
  /* 0x2B */ s8 unk2B;
  /* 0x2C */ s8 unk2C;
  /* 0x2D */ s8 unk2D;
  /* 0x2E */ s8 holeIndex; /* The hole the Ring Shot select points at */
  /* 0x2F */ s8 courseIndex; /* And its course */
  /* 0x30 */ s8 statsCourse; /* The course whose statistics show; one past the
                                open courses totals them */
  /* 0x31 */ u8 unk31;
  /* 0x32 */ s8
      statsSpin; /* How far the courses still have to turn to reach it */
  /* 0x33 */ s8 bestTimesCourse; /* The open course whose best times show */
  /* 0x34 */ s8
      bestTimesSpin; /* How far the courses still have to turn to reach it */
  /* 0x35 */ s8
      tableColumn; /* The cursor's place among a table's shown columns */
  /* 0x36 */ s8 tableRow; /* And among its shown rows */
  /* 0x37 */ s8 tableFirstColumn; /* The first column shown of a table wider
                                     than the screen */
  /* 0x38 */ s8 tableFirstRow; /* And the first row shown of one taller */
  /* 0x39 */ s8 unk39;
  /* 0x3A */ s8 unk3A;
  /* 0x3B */ s8 ringShotRow; /* The Ring Shot records' row: a course, or the
                                replays after them */
  /* 0x3C */ s8
      ringShotFirstCharacters[RING_SHOT_COURSE_COUNT]; /* Each course's first
                                                          character shown */
  /* 0x42 */ s8 ringShotReplaySlot; /* The Ring Shot replay chosen */
  /* 0x43 */ s8 savedGameIndex;
  /* 0x44 */ s8 savedGameTurn; /* Updates left of the saved games' boards
                                 turning, negative when turning right */
  /* 0x45 */ u8 optionBits; /* Each OPTION_BIT_* mirrors a saved option */
  /* 0x46 */ u8 isTransmitShown; /* The menus were entered with the transmit
                                  buttons, so the main menu offers it */
  /* 0x47 */ s8
      codeKeyColumn; /* The club tournament code keyboard's chosen key */
  /* 0x48 */ s8 codeKeyRow;
} MenuMemory;

MenuMemory* func_8005AF74(void);

extern MenuMemory D_801323A0;

/* The character select's record of which characters are taken */
typedef struct TakenCharacters {
  /* 0x00 */ s16 unk0;
  /* 0x02 */ u8 unk2[0x8 - 0x2];
  /* 0x08 */ Boolean isCharacterTaken[CHARACTER_COUNT];
  /* 0x50 */ u8 unk50[0x54 - 0x50];
} TakenCharacters; /* size = 0x54 */

extern TakenCharacters g_TakenCharacters;

/* The character each player has chosen, or NO_CHOSEN_CHARACTER */
#define NO_CHOSEN_CHARACTER -1
extern s32 D_8012F480[MAX_PLAYER_COUNT];

/* Whether the menus have loaded each character slot's character */
extern u8 D_801B70F0[MAXCONTROLLERS];

#endif
