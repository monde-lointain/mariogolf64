/*
 * The menus' screens, their update and their drawing.
 */

#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include <ultra64.h>

#include "camera/camera.h"
#include "characters/character.h"
#include "menus/menu.h"
#include "round/player.h"
#include "system/asset.h"
#include "system/c_language.h"

// The course select's layout, which the menus keep in the top byte of
// g_MapCamera's mode.
#define COURSE_SELECT_LAYOUT (*(u8 *)&g_MapCamera.mode)

enum CourseSelectLayout {
    // Each course's birdie badge count and tournament trophies; also set on
    // leaving the course select
    COURSE_SELECT_LAYOUT_AWARDS,
    // The holes choice, and the bet settings on C-up
    COURSE_SELECT_LAYOUT_HOLES,
    // Neither, for modes without bets or a holes choice
    COURSE_SELECT_LAYOUT_PLAIN
};

typedef s32 CourseSelectLayout;

// The parts of the menu backdrop that DrawLargeBannerBackdropLayers and
// DrawMenuBackdropLayers draw. The screens also pass MENU_LAYER_IGNORED, which
// neither tests, so it draws nothing.
#define MENU_LAYER_COURSE_SCENE (1 << 0)
#define MENU_LAYER_IGNORED (1 << 1)
#define MENU_LAYER_BOARD (1 << 2)
#define MENU_LAYER_BANNER (1 << 3)
#define MENU_LAYER_LARGE_BANNER (1 << 4)
// DrawMenuBackdropLayers reads g_MenuScreen for it but draws nothing on any
// screen
#define MENU_LAYER_PER_SCREEN (1 << 5)
#define MENU_LAYER_TALL_NARROW_BOARD (1 << 7)
// Just the scenery and the banner, behind the screens that draw their own
// panel or model where the others have a board
#define PLAIN_BACKDROP_LAYERS (MENU_LAYER_COURSE_SCENE | MENU_LAYER_IGNORED | MENU_LAYER_BANNER)
#define CAROUSEL_EYE_HEIGHT 70.0f
#define CAROUSEL_EYE_DISTANCE 60.0f
#define CAROUSEL_RADIUS_X 48.0f
#define CAROUSEL_RADIUS_Z 40.0f

#define CAROUSEL_BOARD_HALF_SIZE 32.0f
#define UNCHOSEN_CAROUSEL_BOARD_SHADE 127
#define CAROUSEL_BOB_PHASE_STEP 0.1f
#define CAROUSEL_BOB_HEIGHT 8.0f
// A screen's lit labels, or other images in their place, read alternately into each
extern ImageAsset *g_MenuLabelImages[MENU_IMAGE_BUFFER_COUNT];
// How many pixels wider and taller than its size the chosen label is drawn
extern s16 g_ChosenLabelGrowth;
// Whether DrawGame draws the loaded hole in the menus' inset view once the
// loader has finished. UpdateMenus clears it, and nothing sets it.
extern Boolean g_IsHoleShownInMenuInset;

enum LabelGrowthDirection {
    LABEL_GROWING,
    LABEL_SHRINKING
};

typedef u8 LabelGrowthDirection;

// Whether the chosen label is growing or shrinking
extern LabelGrowthDirection g_ChosenLabelGrowthDirection;

extern ImageAsset *g_NamePlateImage;
// The player count chosen on the title screen, which the saved games, the
// menus and the club tournament set to 1
extern s32 g_ChosenPlayerCount;

// The phases of the handicap screen's update, which g_BetPagesPhase holds
enum BetPagesPhase {
    MENU_PHASE_START,
    BET_PHASE_OPENING,
    BET_PHASE_CHOOSING,
    BET_PHASE_CANCELED,
    // The bet settings screen's phases in g_BetPagesPhase
    BET_PHASE_CONFIRMED,
    MENU_PHASE_CHOOSING,
    MENU_PHASE_CANCELED,
    MENU_PHASE_CONFIRMED
};

typedef u8 UBetPagesPhase8;
extern UBetPagesPhase8 g_BetPagesPhase;
extern UStatsSlide8 g_BetPagesWallSlide;
extern s32 g_AnimatedIconFrame;
extern u8 g_AnimatedIconBufferIndex;
extern s8 g_AnimatedIconFrameTime;
// Updates until each character fidgets, or FIDGETING while it does
extern s32 g_MenuFidgetDelays[PLAYER_COUNT_MAX];
extern s32 g_CourseBannersSlide;
// The handicap of a place that no player takes
#define ABSENT_PLAYER_HANDICAP (-1)
// Each player's handicap on the handicap screen, or ABSENT_PLAYER_HANDICAP for
// no player
extern s16 g_ChosenHandicaps[PLAYER_COUNT_MAX];

// What the course select names the courses by
enum CourseSelectKind {
    COURSE_SELECT_KIND_NONE,
    COURSE_SELECT_KIND_COURSES,
    COURSE_SELECT_KIND_TOURNAMENTS
};

typedef u8 CourseSelectKind;
// Whether the course select names the courses or the tournaments
extern CourseSelectKind g_CourseSelectKind;
// Whether the running wipe is the one opening onto the menu
extern UBoolean8 g_IsMenuWipeOpening;
// Whether the wipe opens onto the menu, as the Ring Shot records see it
extern UBoolean8 g_IsRingShotWipeOpening;
// Whether the chosen hole has started loading behind the menu
extern Boolean g_IsChosenHoleLoadStarted;
extern s32 g_ChoosingPlayerIndex;
// Each character's name, coded, in CharacterChoice order
extern char g_MenuCharacterNames[CHARACTER_COUNT][IMPORTED_CHARACTER_NAME_LENGTH];

// The menu banners, 256 color images such as asset 0x521, drawn in strips
// that fit the texture memory
#define MENU_BANNER_LEFT 60

s32 GetOpenCourseCount(void);
void CheckControllersAndSetPans(void);
void DrawMenuBackdropLayers(Gfx **displayListHead, s32 layers);
void DrawLargeBannerBackdropLayers(Gfx **displayListHead, s32 layers);
void UpdateMenus(void);
void *GetMenuImageBuffer(void);
void StartLoadingChosenHole(void);
void DrawMenuScreen(Gfx **displayListHead);
void DrawMenuCourseScene(Gfx **displayListHead);
void DrawMenuBanner(Gfx **displayListHead);
void DrawSpinningBadge(Gfx **displayListHead, ImageAsset *image, s32 x, s32 y);

#endif
