#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include "common.h"

#include "character.h"
#include "menu_memory.h"

/*
 * The menus set the top byte of g_MapCamera's mode. Reading it as a
 * byte of the field rather than of the whole camera lets the compiler move
 * the read past display list words, as the original does.
 */
#define MAP_CAMERA_MODE_TOP_BYTE (*(u8*)&g_MapCamera.mode)

/*
 * The parts of the menu backdrop that DrawLargeBannerBackdropLayers and
 * DrawMenuBackdropLayers draw. The screens also pass MENU_LAYER_UNK2, which
 * neither tests.
 */
#define MENU_LAYER_SCENERY (1 << 0)
#define MENU_LAYER_UNK2 (1 << 1)
#define MENU_LAYER_BOARD (1 << 2)
#define MENU_LAYER_BANNER (1 << 3)
#define MENU_LAYER_LARGE_BANNER (1 << 4)
#define MENU_LAYER_UNK20 (1 << 5)
#define MENU_LAYER_TALL_NARROW_BOARD (1 << 7)
/* Just the scenery and the banner, behind the screens that draw their own
 * panel or model where the others have a board */
#define PLAIN_BACKDROP_LAYERS \
  (MENU_LAYER_SCENERY | MENU_LAYER_UNK2 | MENU_LAYER_BANNER)

/* A screen's lit labels, or other images in their place, read alternately
 * into each */
extern void* g_MenuLabelImages[2];
/* How many pixels wider and taller than its size the chosen label is drawn */
extern s16 g_ChosenLabelGrowth;

enum LabelGrowthDirection {
  LABEL_GROWING,
  LABEL_SHRINKING
};

typedef u8 LabelGrowthDirection;

/* Whether the chosen label is growing or shrinking */
extern LabelGrowthDirection g_ChosenLabelGrowthDirection;

extern void* g_NamePlateImage;
/* The player count chosen on the title screen, which the saved games, the
 * menus and the club tournament set to 1 */
extern s32 g_ChosenPlayerCount;
extern u8 g_BetPagesPhase;
extern u8 g_BetPagesWallSlide;
extern s32 g_AnimatedIconFrameIndex;
extern u8 g_AnimatedIconBufferIndex;
extern s8 g_AnimatedIconFrameTime;
/* Updates until each character fidgets, or FIDGETING while it does */
extern s32 g_MenuFidgetDelays[MAX_PLAYER_COUNT];
extern s32 g_CourseBannersSlide;
/* The handicap of a place that no player takes */
#define ABSENT_PLAYER_HANDICAP (-1)
/*
 * Each player's handicap on the handicap screen, or ABSENT_PLAYER_HANDICAP for
 * no player
 */
extern s16 g_ChosenHandicaps[MAX_PLAYER_COUNT];

/* What the course select names the courses by */
enum CourseSelectKind {
  COURSE_SELECT_KIND_NONE,
  COURSE_SELECT_KIND_COURSES,
  COURSE_SELECT_KIND_TOURNAMENTS
};

typedef u8 CourseSelectKind;
extern CourseSelectKind g_CourseSelectKind;
/* Whether the running wipe is the one opening onto the menu */
extern u8 g_IsMenuWipeOpening;
/* Whether the wipe opens onto the menu, as the Ring Shot records see it */
extern u8 g_IsRingShotWipeOpening;
/* Whether the chosen hole has started loading behind the menu */
extern Boolean g_IsChosenHoleLoadStarted;
extern s32 g_ChoosingPlayerIndex;
/* Each character's name, coded, in CharacterChoice order */
extern char g_MenuCharacterNames[CHARACTER_COUNT]
                                [IMPORTED_CHARACTER_NAME_LENGTH];

s32 GetOpenCourseCount(void);
void CheckControllersAndSetPans(void);
void DrawCourseBanner(
    Gfx** displayListHead, u8 useCourseNames, s32 courseIndex);
void DrawBlinkingSideArrows(Gfx** displayListHead, void* image, s32* blinkFrame,
    s32* blinkTimer, s32 width, s32 centerX, s32 centerY);
s32 UpdateCharacterSelect(
    s32 firstPlayerIndex, s32 playerCount, Boolean canChooseMarked);
void DrawMenuBackdropLayers(Gfx** displayListHead, s32 layers);
void DrawLargeBannerBackdropLayers(Gfx** displayListHead, s32 layers);

#endif
