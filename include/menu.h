#ifndef MENU_H
#define MENU_H

#include "common.h"

#include "asset.h"
#include "character.h"
#include "course.h"
#include "save.h"

/* The screens that the menu overlay shows, which g_MenuScreen picks */
enum MenuScreen {
  MENU_SCREEN_TITLE,
  MENU_SCREEN_CHARACTER_SELECT,
  MENU_SCREEN_MAIN,
  MENU_SCREEN_SAVED_GAMES,
  MENU_SCREEN_OPTIONS,
  MENU_SCREEN_UNK5,
  MENU_SCREEN_UNK6,
  MENU_SCREEN_BEST_TIMES,
  MENU_SCREEN_CLUBHOUSE = 9,
  MENU_SCREEN_COURSE_STATS = 11,
  MENU_SCREEN_UNK12,
  MENU_SCREEN_RING_SHOT_RECORDS,
  MENU_SCREEN_BIRDIE_BADGES,
  MENU_SCREEN_CHARACTER_TABLE,
  MENU_SCREEN_UNK16,
  MENU_SCREEN_HELP,
  /* Loads what the chosen game needs and leaves the menu */
  MENU_SCREEN_EXIT = 19,
  MENU_SCREEN_UNK20,
  MENU_SCREEN_UNK22 = 22,
  MENU_SCREEN_UNK23,
  MENU_SCREEN_UNK24,
  MENU_SCREEN_UNK26 = 26,
  /* A tournament entered with a code */
  MENU_SCREEN_CLUB_TOURNAMENT,
  /* The main menu when no Transfer Pak holds Mario Golf for Game Boy */
  MENU_SCREEN_MAIN_WITHOUT_TRANSMIT
};

#define MENU_SCREEN_COUNT 29
/* g_LoadedMenuScreen's value before any screen's assets are read */
#define NO_MENU_SCREEN (-1)
/* The screen whose assets func_801F4C78 last read */
extern s8 g_LoadedMenuScreen;
/* Each screen's image */
extern AssetId g_MenuScreenImageAssets[MENU_SCREEN_COUNT];
extern s32 g_MenuScreen;

/* The buttons of every controller: those repeating while held, and those just
 * pressed */
extern s32 g_MenuRepeatButtons;
extern s32 g_MenuPressedButtons;

/* Each course's icon, then mini golf's */
extern AssetId g_CourseIconAssets[];
/* The courses' icons, then the putter tournaments' badges */
extern AssetId g_TournamentIconAssets[];
/* How many courses the course select offers */
extern s32 g_MenuCourseCount;
/* The holes choice's Bet badge cycles its color unless this is set */
extern u8 D_800C2BD5;

/* The menus' sound channel, and how loud each of their sounds plays */
#define MENU_SOUND_CHANNEL 15
#define MENU_VOLUME_SOFT 80
#define MENU_VOLUME_MEDIUM 100

/* Each course's images for the menus' turning wheel of courses, and the trophy
 * files read with them */
#define COURSE_IMAGE_COUNT 16
#define COURSE_FILE_COUNT 3
extern AssetId g_CourseWheelImageAssets[][COURSE_IMAGE_COUNT];
/* Ends a course's images before COURSE_IMAGE_COUNT */
#define TROPHY_FRAME_END (-1)
extern s16 g_CourseTrophyFiles[][COURSE_FILE_COUNT];
extern void* D_800FBE38[COURSE_FILE_COUNT];
/* The menus' ASSET_COURSE_SCENE: the sky and the course select's backdrops */
extern void* g_MenuCourseSceneryAsset;

/* Screen 5's images in place of g_CourseIconAssets's: a row of asset IDs for
 * each g_ChosenPlayerCount from 1, of which it reads as many as
 * g_CourseIconAssets has */
#define PLAYER_COUNT_ASSET_ROW_LENGTH 12
extern AssetId g_PlayerCountIconAssets[][PLAYER_COUNT_ASSET_ROW_LENGTH];

/* The size of a course's tournament trophy images */
typedef struct TrophySize {
  s8 width;
  s8 height;
} TrophySize;

extern TrophySize g_TrophySizes[COURSE_COUNT];

/* The mini-golf disc's images in each of D_801049A0's buffers */
#define MINI_GOLF_DISC_IMAGE_COUNT 2
/*
 * The disc's frames as it turns from one course to the next, each two images,
 * read into the buffers of D_801049A0 not shown; the courses face the screen at
 * the middle frame
 */
#define MINI_GOLF_TURN_FRAME_COUNT 5
#define MINI_GOLF_FACING_FRAME 2
extern AssetId g_MiniGolfDiscFrameAssets[MINI_GOLF_COURSES_PER_RECORD *
    MINI_GOLF_DISC_IMAGE_COUNT][MINI_GOLF_TURN_FRAME_COUNT];
/* Each mini-golf course's disc image as it faces the screen */
extern AssetId g_MiniGolfCourseIconAssets[];
/* The file read with each mini-golf course's disc image */
extern s16 g_MiniGolfDiscFiles[];

/* The most bytes of each character palette file that the menus keep */
#define MENU_PALETTE_FILE_SIZE 0x200
/* Character palette files that the menu screens read, one per slot */
extern u8 D_8023BFF0_ovl_01[CHARACTER_COUNT][MENU_PALETTE_FILE_SIZE];

/* Returns an image buffer of the menu overlay */
void* GetMenuImageBuffer(void);

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
 * slot holds in f_EveryCharacterGrid, less this offset. In Get Character,
 * func_80200EF0 shows that character as the rival.
 */
#define MARKED_CHARACTER_OFFSET 50
/* A slot of the grid that holds no character */
#define EMPTY_GRID_SLOT (-1)
/* Slots marked with MARKED_CHARACTER_OFFSET hold values below this */
#define MARKED_CHARACTER_LIMIT (-10)

extern s16 g_CharacterGrid[CHARACTER_GRID_ROW_COUNT]
                          [CHARACTER_GRID_COLUMN_COUNT];

/*
 * g_PlayerControllerIndices holds this for a player with no controller of their
 * own. The computer plays them, and they never clash with the others.
 */
#define NO_CONTROLLER_INDEX 4
/* A player's controller before the player chooses */
#define UNCHOSEN_CONTROLLER (-1)
/* g_ImportedCharacterColumns's value for a player without an imported character
 */
#define NO_IMPORTED_COLUMN (-1)
extern s8 g_PlayerControllerIndices[MAX_PLAYER_COUNT];
extern s8 g_ChoosingControllerIndices[MAX_PLAYER_COUNT];
extern s16 g_ImportedCharacterColumns[MAX_PLAYER_COUNT];

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
/* The table holds three rows past the last icon, copies of rows 2, 2 and 5 */
#define MODE_ICON_ANIMATION_COUNT 14
/* Each icon's frames are asset ids, ended by END_OF_ICON_FRAMES */
#define MODE_ICON_FRAME_COUNT 27
#define END_OF_ICON_FRAMES -1

extern s16 g_ModeIconAnimations[MODE_ICON_ANIMATION_COUNT]
                               [MODE_ICON_FRAME_COUNT];

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
enum MenuArrow {
  MENU_ARROW_UP,
  MENU_ARROW_DOWN,
  MENU_ARROW_RIGHT,
  MENU_ARROW_LEFT
};

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
enum MenuNumberStyle {
  MENU_NUMBER_PLAIN,
  MENU_NUMBER_TIMES,
  MENU_NUMBER_SIGNED,
  MENU_NUMBER_RED_TINT,
  MENU_NUMBER_GREEN_TINT
};

/* The signs that func_802123E0 draws */
enum MenuSign {
  MENU_SIGN_DASH,
  MENU_SIGN_TIMES,
  MENU_SIGN_CIRCLE
};

/*
 * The cursor of the character table and the player stats: a quad whose corners
 * cycle through the colors, masked by the 48x48 IA4 image of asset 0x6EF
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

/* The buffers of func_802218C0, and what it reads into them */
enum SaveGameBuffer {
  SAVE_GAME_BUFFER_CONTROLLER_ICONS = 1,
  SAVE_GAME_BUFFER_PLAYER_LABELS,
  SAVE_GAME_BUFFER_FIRST_BOARD,
  SAVE_GAME_BUFFER_SECOND_BOARD,
  SAVE_GAME_BUFFER_THIRD_BOARD,
  SAVE_GAME_BUFFER_COURSE,
  SAVE_GAME_BUFFER_CHOSEN_COURSE
};

/* What func_80221EBC reads: each saved game's mode image and a frame, or the
 * chosen game's two */
enum SaveMenuBuffer {
  SAVE_MENU_BUFFER_SLOTS = 1,
  SAVE_MENU_BUFFER_CHOSEN
};

/* The buffer that func_8020B2C4 keeps without reading an image into it */
#define CHARACTER_SELECT_UNREAD_KIND 10

/* The buffers that func_802159C0 keeps, by the kind its caller gives */
enum TableBuffer {
  TABLE_BUFFER_CURSOR = 1,
  TABLE_BUFFER_UNREAD,
  TABLE_BUFFER_ARROWS,
  TABLE_BUFFER_STAR
};

/* The buffers that func_8020E0AC keeps, by the kind its caller gives */
enum CourseBuffer {
  COURSE_BUFFER_FIRST = 1,
  COURSE_BUFFER_SECOND,
  COURSE_BUFFER_LAST
};

/* The buffers that func_802178B0 fills, and the assets it reads into them */
enum PlayerStatsBuffer {
  PLAYER_STATS_BUFFER_ICONS = 1,
  PLAYER_STATS_BUFFER_UNK2,
  PLAYER_STATS_BUFFER_UNK3,
  PLAYER_STATS_BUFFER_UNK4,
  PLAYER_STATS_BUFFER_UNK5,
  PLAYER_STATS_BUFFER_UNK6,
  PLAYER_STATS_BUFFER_UNK7,
  PLAYER_STATS_BUFFER_UNK8,
  PLAYER_STATS_BUFFER_GAME_LABELS,
  PLAYER_STATS_BUFFER_PLAYER_LABELS,
  PLAYER_STATS_BUFFER_BRICK_WALL,
  PLAYER_STATS_BUFFER_UNK12,
  PLAYER_STATS_BUFFER_UNK13
};

/* The ways func_8021B618 slides the stats panel's rows */
#define STATS_SLIDE_CURRENT 0
#define STATS_SLIDE_OUT_RIGHT 1
#define STATS_SLIDE_OUT_LEFT 2
#define STATS_SLIDE_IN_FROM_RIGHT 3
#define STATS_SLIDE_IN_FROM_LEFT 4
#define STATS_SLIDE_COUNT 5

/* The phases of the screen 20 handler, which D_8022BE88 holds */
#define MENU_PHASE_START 0
#define MENU_PHASE_CHOOSING 5
#define MENU_PHASE_CANCELED 6
#define MENU_PHASE_CONFIRMED 7
/* Values of D_8022BE89 */
#define MENU_UNK89_UNK3 3
#define MENU_UNK89_UNK4 4
/* The bet settings screen's phases in D_8022BE88 */
#define BET_PHASE_OPENING 1
#define BET_PHASE_CHOOSING 2
#define BET_PHASE_CANCELED 3
#define BET_PHASE_CONFIRMED 4

#endif
