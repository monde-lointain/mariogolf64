#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include <ultra64.h>

#include "asset.h"
#include "camera.h"
#include "character.h"
#include "language.h"
#include "player.h"

#define COURSE_SELECT_LAYOUT (*(u8 *)&g_MapCamera.mode)

enum CourseSelectLayout {
    COURSE_SELECT_LAYOUT_AWARDS,
    COURSE_SELECT_LAYOUT_HOLES,
    COURSE_SELECT_LAYOUT_PLAIN
};

typedef s32 CourseSelectLayout;

#define MENU_LAYER_COURSE_SCENE (1 << 0)
#define MENU_LAYER_IGNORED (1 << 1)
#define MENU_LAYER_BOARD (1 << 2)
#define MENU_LAYER_BANNER (1 << 3)
#define MENU_LAYER_LARGE_BANNER (1 << 4)
#define MENU_LAYER_PER_SCREEN (1 << 5)
#define MENU_LAYER_TALL_NARROW_BOARD (1 << 7)
#define PLAIN_BACKDROP_LAYERS (MENU_LAYER_COURSE_SCENE | MENU_LAYER_IGNORED | MENU_LAYER_BANNER)

extern ImageAsset *g_MenuLabelImages[2];
extern s16 g_ChosenLabelGrowth;
extern Boolean g_IsHoleShownInMenuInset;

enum LabelGrowthDirection {
    LABEL_GROWING,
    LABEL_SHRINKING
};

typedef u8 LabelGrowthDirection;

extern LabelGrowthDirection g_ChosenLabelGrowthDirection;

extern ImageAsset *g_NamePlateImage;
extern s32 g_ChosenPlayerCount;

#define MENU_PHASE_START 0
#define MENU_PHASE_CHOOSING 5
#define MENU_PHASE_CANCELED 6
#define MENU_PHASE_CONFIRMED 7
#define BET_PHASE_OPENING 1
#define BET_PHASE_CHOOSING 2
#define BET_PHASE_CANCELED 3
#define BET_PHASE_CONFIRMED 4
extern u8 g_BetPagesPhase;
extern u8 g_BetPagesWallSlide;
extern s32 g_AnimatedIconFrame;
extern u8 g_AnimatedIconBufferIndex;
extern s8 g_AnimatedIconFrameTime;
extern s32 g_MenuFidgetDelays[PLAYER_COUNT_MAX];
extern s32 g_CourseBannersSlide;
#define ABSENT_PLAYER_HANDICAP (-1)
extern s16 g_ChosenHandicaps[PLAYER_COUNT_MAX];

enum CourseSelectKind {
    COURSE_SELECT_KIND_NONE,
    COURSE_SELECT_KIND_COURSES,
    COURSE_SELECT_KIND_TOURNAMENTS
};

typedef u8 CourseSelectKind;
extern CourseSelectKind g_CourseSelectKind;
extern u8 g_IsMenuWipeOpening;
extern u8 g_IsRingShotWipeOpening;
extern Boolean g_IsChosenHoleLoadStarted;
extern s32 g_ChoosingPlayerIndex;
extern char g_MenuCharacterNames[CHARACTER_COUNT][IMPORTED_CHARACTER_NAME_LENGTH];

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
