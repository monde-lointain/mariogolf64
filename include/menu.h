#ifndef MENU_H
#define MENU_H

#include "common.h"

/* The screens that the menu overlay shows, which g_MenuScreen picks */
#define MENU_SCREEN_TITLE 0
#define MENU_SCREEN_CHARACTER_SELECT 1
#define MENU_SCREEN_MAIN 2
#define MENU_SCREEN_SAVED_GAMES 3
#define MENU_SCREEN_OPTIONS 4
#define MENU_SCREEN_UNK5 5
#define MENU_SCREEN_UNK6 6
#define MENU_SCREEN_BEST_TIMES 7
#define MENU_SCREEN_CLUBHOUSE 9
#define MENU_SCREEN_COURSE_STATS 11
#define MENU_SCREEN_UNK12 12
#define MENU_SCREEN_RING_SHOT_RECORDS 13
#define MENU_SCREEN_BIRDIE_BADGES 14
#define MENU_SCREEN_CHARACTER_TABLE 15
#define MENU_SCREEN_UNK16 16
#define MENU_SCREEN_HELP 17
/* Loads what the chosen game needs and leaves the menu */
#define MENU_SCREEN_EXIT 19
#define MENU_SCREEN_UNK20 20
#define MENU_SCREEN_UNK22 22
#define MENU_SCREEN_UNK23 23
#define MENU_SCREEN_UNK24 24
#define MENU_SCREEN_UNK26 26
/* A tournament entered with a code */
#define MENU_SCREEN_CLUB_TOURNAMENT 27
/* The main menu when no Transfer Pak holds Mario Golf for Game Boy */
#define MENU_SCREEN_MAIN_WITHOUT_TRANSMIT 28
#define MENU_SCREEN_COUNT 29
extern s32 g_MenuScreen;

/* The buttons of every controller: those repeating while held, and those just
 * pressed */
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
  /* 0x06 */ s8 optionsChoice;
  /* 0x07 */ s8 unk7[0x13 - 0x7];
  /* 0x13 */ s8 unk13;
  /* 0x14 */ s8 unk14;
  /* 0x15 */ s8 unk15;
  /* 0x16 */ s8 holeIndex;
  /* 0x17 */ s8 courseIndex;
} MenuChoices; /* size = 0x18 */

extern MenuChoices g_MenuChoices;

/* Each course's images for the menus' turning wheel of courses, and the trophy
 * files read with them */
#define COURSE_IMAGE_COUNT 16
#define COURSE_FILE_COUNT 3
extern u16 D_800C21B8[][COURSE_IMAGE_COUNT];
extern s16 D_800C2278[][COURSE_FILE_COUNT];
extern void* D_800FBE38[COURSE_FILE_COUNT];

/* An image buffer of the menu overlay */
extern u8 g_MenuImageBuffer[];
void* GetMenuImageBuffer(void);

/* The states of the menus' popup window, which opens and closes by scaling */
#define MENU_WINDOW_OPENING 0
#define MENU_WINDOW_OPEN 1
#define MENU_WINDOW_CLOSING 2
#define MENU_WINDOW_CLOSED 3

void ResetMenuWindow(void);
void ShowMenuWindow(void);
void CloseMenuWindow(void);
/* These return -1 while the window is still moving */
s32 IsMenuWindowOpen(void);
s32 IsMenuWindowClosed(void);
void LoadMenuWindowImage(s32 imageIndex);

/* The character select's grid of slots */
#define CHARACTER_GRID_ROW_COUNT 3
#define CHARACTER_GRID_COLUMN_COUNT 7
/* The rows of the grid that hold the characters to choose from */
#define CHARACTER_CHOICE_ROW_COUNT 2
/* The character select row of the Game Boy characters, whose column is the
 * character's index */
#define GAME_BOY_CHARACTER_ROW 2

/*
 * The first empty slot gets a negative mark: the id of the character that
 * slot holds in g_EveryCharacterGrid, less this offset. In Get Character,
 * func_80200EF0 shows that character as the rival.
 */
#define MARKED_CHARACTER_OFFSET 50
/* Slots marked with MARKED_CHARACTER_OFFSET hold values below this; empty ones
 * hold -1 */
#define MARKED_CHARACTER_LIMIT (-10)

extern s16 g_CharacterGrid[CHARACTER_GRID_ROW_COUNT]
                          [CHARACTER_GRID_COLUMN_COUNT];

/*
 * D_800C1FF4 holds this for a player with no controller of their own. The
 * computer plays them, and they never clash with the others.
 */
#define NO_CONTROLLER_INDEX 4

/* The 1P to 4P, 1C to 4C and Com labels, 16 by 8 each */
#define PLAYER_LABEL_WIDTH 16
#define PLAYER_LABEL_HEIGHT 8
#define PLAYER_LABEL_IMAGE_SIZE (PLAYER_LABEL_WIDTH * PLAYER_LABEL_HEIGHT / 2)
#define PLAYER_LABEL_COMPUTER_FIRST 4
#define PLAYER_LABEL_COM 8
/* Where a player's label goes below their 32 by 32 controller icon */
#define PLAYER_LABEL_OFFSET_X 8
#define PLAYER_LABEL_OFFSET_Y 25
#define CONTROLLER_ICON_SIZE 32
#define CONTROLLER_ICON_IMAGE_SIZE (CONTROLLER_ICON_SIZE * CONTROLLER_ICON_SIZE)

/* The menus' character models */
/* Baby Mario stands higher to look as tall as the others */
#define BABY_MARIO_RAISE 24
#define MENU_ANIMATION_STANDING 16
#define MENU_ANIMATION_FIDGET 17
/* The most frames a character stands before it fidgets again */
#define FIDGET_DELAY_RANGE 128
/* The orthographic projection that the menus view their characters with, and
 * the eye of the LookAt that guLookAtReflectF builds for them */
#define MENU_CHARACTERS_NEAR 1.0f
#define MENU_CHARACTERS_FAR 3000.0f
#define MENU_CHARACTERS_ORTHO_SCALE 10.0f
#define MENU_CHARACTERS_EYE_Y 70.0f
#define MENU_CHARACTERS_EYE_Z 60.0f

/* Indices of the game-mode icon animations in g_ModeIconAnimations */
#define MODE_ICON_TOURNAMENT 0
#define MODE_ICON_GET_CHARACTER 1
#define MODE_ICON_STROKE 2
#define MODE_ICON_SPEED_GOLF 3
#define MODE_ICON_TRAINING 4
#define MODE_ICON_MINI_GOLF 5
#define MODE_ICON_MATCH_GAME 6
#define MODE_ICON_SKINS_MATCH 7
#define MODE_ICON_RING_SHOT 8
#define MODE_ICON_UNK9 9
#define MODE_ICON_CLUB_SLOTS 10
#define NO_MODE_ICON -1
#define MODE_ICON_COUNT 11
/* Each icon's frames are asset ids, ended by END_OF_ICON_FRAMES */
#define MODE_ICON_FRAME_COUNT 27
#define END_OF_ICON_FRAMES -1

extern s16 g_ModeIconAnimations[MODE_ICON_COUNT][MODE_ICON_FRAME_COUNT];

/*
 * The menu banners, 256 color images such as asset 0x521, drawn in strips
 * that fit the texture memory
 */
#define MENU_BANNER_WIDTH 200
#define MENU_BANNER_STRIP_HEIGHT 10
#define MENU_BANNER_STRIP_COUNT 3
#define MENU_BANNER_LEFT 60

/*
 * The menus draw characters' 32x24 faces, darkened or with the name
 * beside them
 */
#define FACE_WIDTH 32
#define FACE_HEIGHT 24
#define FACE_WITH_NAME 0
#define FACE_PLAIN 1
#define FACE_DARKENED 10
#define FACE_DARKENED_SHADE 10
#define FACE_NAME_OFFSET_X 24
#define FACE_NAME_OFFSET_Y 12

/* The arrows that func_80212FEC draws, by the order of their images */
#define MENU_ARROW_UP 0
#define MENU_ARROW_DOWN 1
#define MENU_ARROW_RIGHT 2
#define MENU_ARROW_LEFT 3

/*
 * The wheel that func_8020E898 and func_80213748 turn the open courses' 64 by
 * 64 images on, seen from above, with the blinking arrows either side
 */
#define WHEEL_FIELD_OF_VIEW 120.0f
#define WHEEL_ASPECT_RATIO (4.0f / 3.0f)
#define WHEEL_NEAR_PLANE 10.0f
#define WHEEL_FAR_PLANE 30000.0f
#define WHEEL_EYE_HEIGHT 80.0f
#define WHEEL_EYE_DISTANCE 120.0f
#define WHEEL_RADIUS_X 104.0f
#define WHEEL_RADIUS_Z 40.0f
#define WHEEL_SLOT_LIMIT 8
/* The images sit above the wheel's projected points, and each strip 12 pixels
 * higher again */
#define WHEEL_RAISE 32
#define WHEEL_ICON_RAISE 12
#define DEGREES_PER_TURN 360
/* The spins count steps of four degrees */
#define DEGREES_PER_SPIN_STEP 4
#define COURSE_ICON_WIDTH 64
#define COURSE_ICON_STRIP_HEIGHT 32
#define COURSE_ICON_STRIP_COUNT 2
#define COURSE_ICON_HALF_SIZE 32.0f
#define WHEEL_ARROW_WIDTH 32
#define WHEEL_ARROW_X 160
#define WHEEL_ARROW_Y 94

/*
 * How DrawMenuNumber marks a number: with nothing, a cross before it, its sign
 * (a plus tinted red, a minus tinted green, or plus-minus for zero), or a red
 * or green tint
 */
#define MENU_NUMBER_PLAIN 0
#define MENU_NUMBER_TIMES 1
#define MENU_NUMBER_SIGNED 2
#define MENU_NUMBER_RED_TINT 3
#define MENU_NUMBER_GREEN_TINT 4

/* The signs that func_802123E0 draws */
#define MENU_SIGN_DASH 0
#define MENU_SIGN_TIMES 1
#define MENU_SIGN_CIRCLE 2

/*
 * The cursor of the character table and the player stats: a quad whose corners
 * cycle through the colours, masked by the 48x48 IA4 image of asset 0x6EF
 */
#define MENU_CURSOR_SIZE 48
#define MENU_CURSOR_VERTEX_COUNT 4
#define MENU_CURSOR_PHASE_STEP (M_PI / 12)
#define MENU_CURSOR_COLOR_SWING 127.0f
#define MENU_CURSOR_COLOR_MIDDLE 127
#define MENU_CURSOR_NEAR 10.0f
#define MENU_CURSOR_FAR 2000.0f
#define MENU_CURSOR_Z -100.0f
#define MENU_CURSOR_SCALE_Y 0.87f

/* The title screen's song */
#define SONG_TITLE 0x27

/* The buffers of func_802218C0, and what it reads into them */
#define SAVE_GAME_BUFFER_CONTROLLER_ICONS 1
#define SAVE_GAME_BUFFER_PLAYER_LABELS 2
#define SAVE_GAME_BUFFER_FIRST_BOARD 3
#define SAVE_GAME_BUFFER_SECOND_BOARD 4
#define SAVE_GAME_BUFFER_THIRD_BOARD 5
#define SAVE_GAME_BUFFER_COURSE 6
#define SAVE_GAME_BUFFER_CHOSEN_COURSE 7

/* What func_80221EBC reads: each saved game's mode image and a frame, or the
 * chosen game's two */
#define SAVE_MENU_BUFFER_SLOTS 1
#define SAVE_MENU_BUFFER_CHOSEN 2

/* The buffer that func_8020B2C4 keeps without reading an image into it */
#define CHARACTER_SELECT_UNREAD_KIND 10

/* The buffers that func_802159C0 keeps, by the kind its caller gives */
#define TABLE_BUFFER_CURSOR 1
#define TABLE_BUFFER_UNREAD 2
#define TABLE_BUFFER_ARROWS 3
#define TABLE_BUFFER_STAR 4

/* The buffers that func_8020E0AC keeps, by the kind its caller gives */
#define COURSE_BUFFER_FIRST 1
#define COURSE_BUFFER_SECOND 2
#define COURSE_BUFFER_LAST 3

/* The buffers that func_802178B0 fills, and the assets it reads into them */
#define PLAYER_STATS_BUFFER_ICONS 1
#define PLAYER_STATS_BUFFER_UNK2 2
#define PLAYER_STATS_BUFFER_UNK3 3
#define PLAYER_STATS_BUFFER_UNK4 4
#define PLAYER_STATS_BUFFER_UNK5 5
#define PLAYER_STATS_BUFFER_UNK6 6
#define PLAYER_STATS_BUFFER_UNK7 7
#define PLAYER_STATS_BUFFER_UNK8 8
#define PLAYER_STATS_BUFFER_GAME_LABELS 9
#define PLAYER_STATS_BUFFER_PLAYER_LABELS 10
#define PLAYER_STATS_BUFFER_BRICK_WALL 11
#define PLAYER_STATS_BUFFER_UNK12 12
#define PLAYER_STATS_BUFFER_UNK13 13

#endif
