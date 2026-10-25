#include "common.h"
#include "course.h"
#include "game_flags.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "libc.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "variables.h"

/*
 * The hole that plays behind the title screen: the last hole of the first
 * course, calm and lit from a fixed direction.
 */
#define TITLE_HOLE_INDEX 9
#define TITLE_COURSE_INDEX 0
#define TITLE_WIND_SPEED_MASK 0x3F
#define TITLE_WIND_SPEED_STEP 6000
#define TITLE_LIGHT_PITCH 0x1000
#define TITLE_LIGHT_YAW 0x6000
#define GAME_FLAG_UNK7 0x07
#define TITLE_FRAMEBUFFER_COUNT 2

extern s32 D_801B609C;
extern s32 g_PlayerCount;
extern s32 sIsPrimitiveDepthSet;
extern s32 sMenuBackdrop;
extern s32 sHudAnimationFramesLeft;

extern s32 sHudAnimation;
extern s32 sIsHudAnimationPlaying;
extern s32 sHudAnimationFrame;
extern s32 D_800C7300;
extern u16 *D_800FED10;

void func_80095150(void);
void func_8009232C(Gfx **displayListHead);
void func_800947A8(void);
void func_80094228(void);
void func_8009548C(void);
void func_80092F18(Gfx **displayListHead);

void SetFramebufferCount(s32 count);

/* Sets up a single-player game of the hole behind the title screen */
void SetUpTitleHole(void) {
    memset(&g_GameState, 0, sizeof(GameState));
    g_HoleIndex = TITLE_HOLE_INDEX;
    g_CourseIndex = TITLE_COURSE_INDEX;
    D_801B609C = 0;
    g_GameMode = GAME_MODE_UNK12;
    g_HoleConditions.isRaining = FALSE;
    g_HoleConditions.windSpeed = (rand() & TITLE_WIND_SPEED_MASK) * TITLE_WIND_SPEED_STEP;
    g_HoleConditions.windDirection = rand();
    g_HoleConditions.sceneColorIndex = SCENE_COLOR_INDEX_DRIVING_RANGE;
    g_LightPitch = TITLE_LIGHT_PITCH;
    g_LightYaw = TITLE_LIGHT_YAW;
    SetGameFlag(GAME_FLAG_UNK7);
    g_PlayerCount = 1;
    StopLoaderThread();
    StartLoaderThread();
    SetFramebufferCount(TITLE_FRAMEBUFFER_COUNT);
}

INCLUDE_ASM("asm/nonmatchings/hole_display", UpdateZBufferVisibility);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_8008D3F4);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_8008DDDC);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_8008E164);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_8008E1B4);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_8008E82C);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_8008F30C);

/* Sets whether the course's drawing gives its primitives a fixed depth */
void UsePrimitiveDepth(s32 isPrimitiveDepthSet) {
    sIsPrimitiveDepthSet = isPrimitiveDepthSet;
}

INCLUDE_ASM("asm/nonmatchings/hole_display", func_8008FF20);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_80091110);

/* Starts one of the HUD's animations, which lasts a given number of frames, or stops it with HUD_ANIMATION_NONE */
void StartHudAnimation(s32 animation) {
    if (animation == HUD_ANIMATION_NONE) {
        sIsHudAnimationPlaying = FALSE;
        sHudAnimationFramesLeft = 0;
        return;
    }
    if (animation != HUD_ANIMATION_UNK4 && animation != HUD_ANIMATION_UNK5) {
        D_800C7300 = 0;
    }
    switch (animation) {
        case HUD_ANIMATION_UNK0:
            sHudAnimationFramesLeft = 352;
            break;
        case HUD_ANIMATION_UNK1:
        case HUD_ANIMATION_UNK9:
            sHudAnimationFramesLeft = 255;
            break;
        case HUD_ANIMATION_UNK2:
            sHudAnimationFramesLeft = 255;
            break;
        case HUD_ANIMATION_UNK8:
            sHudAnimationFramesLeft = 160;
            break;
        case HUD_ANIMATION_UNK10:
            sHudAnimationFramesLeft = 72;
            break;
        case HUD_ANIMATION_UNK3:
            sHudAnimationFramesLeft = 255;
            break;
        case HUD_ANIMATION_UNK5:
            sHudAnimationFramesLeft = 64;
            break;
        case HUD_ANIMATION_UNK6:
        case HUD_ANIMATION_UNK7:
            sHudAnimationFramesLeft = 32;
            break;
    }
    sHudAnimation = animation;
    sIsHudAnimationPlaying = TRUE;
    sHudAnimationFrame = 0;
}

void HandleFrameBufferSwap(void) {
}

INCLUDE_ASM("asm/nonmatchings/hole_display", func_8009232C);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_80092E10);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_80092F18);

/* Shows a menu's backdrop in place of the course, or MENU_BACKDROP_NONE to show the course again */
void SetMenuBackdrop(s32 backdrop) {
    if (backdrop == MENU_BACKDROP_NONE) {
        g_MenuBackdropStyle = MENU_BACKDROP_STYLE_NONE;
        return;
    }
    if (backdrop == MENU_BACKDROP_UNK3) {
        sMenuBackdrop = backdrop;
        g_MenuBackdropStyle = MENU_BACKDROP_STYLE_UNK2;
        return;
    }
    g_MenuBackdropStyle = MENU_BACKDROP_STYLE_PLAIN;
    sMenuBackdrop = backdrop;
}

INCLUDE_ASM("asm/nonmatchings/hole_display", func_8009351C);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_800939E8);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_80094228);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_800947A8);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_80095150);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_8009548C);

/* Draws the animation that StartHudAnimation started over the frame */
void DrawHudAnimation(void) {
    func_80095150();
    gDPPipeSync(g_DisplayListHead++);
    gDPSetColorDither(g_DisplayListHead++, G_CD_DISABLE);
    switch (sHudAnimation) {
        case HUD_ANIMATION_UNK1:
        case HUD_ANIMATION_UNK6:
        case HUD_ANIMATION_UNK7:
        case HUD_ANIMATION_UNK8:
        case HUD_ANIMATION_UNK9:
            func_8009232C(&g_DisplayListHead);
            break;
        case HUD_ANIMATION_UNK2:
            func_800947A8();
            break;
        case HUD_ANIMATION_UNK3:
            func_80094228();
            break;
        case HUD_ANIMATION_UNK5:
            func_8009548C();
            break;
        case HUD_ANIMATION_UNK10:
            func_80092F18(&g_DisplayListHead);
            break;
    }
    D_800FED10 = nuGfxCfb_ptr;
}

/* Lets the HUD draw over the whole screen again */
void ResetHudScissor(void) {
    g_HudScissor.left = 0;
    g_HudScissor.right = SCREEN_WIDTH;
    g_HudScissor.bottom = SCREEN_HEIGHT;
    g_HudScissor.top = 0;
    gDPPipeSync(g_DisplayListHead++);
    gDPSetScissor(g_DisplayListHead++, G_SC_NON_INTERLACE, g_HudScissor.left, g_HudScissor.top, g_HudScissor.right,
                  g_HudScissor.bottom);
}

/* Whether the animation that StartHudAnimation started has finished */
s32 IsHudAnimationDone(void) {
    return sHudAnimationFramesLeft <= 0;
}

