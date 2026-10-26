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

/*
 * The sun's glare grows while the sun is on the screen with nothing in front
 * of it, flaring past SUN_GLARE_FLARE_STRENGTH before settling at full
 * strength, and fades when something covers it. Only the z-buffer within
 * SUN_DEPTH_MARGIN of the screen's edges is read.
 */
#define SUN_DEPTH_MARGIN 16.0f
#define SUN_GLARE_MARGIN 120.0f
#define SUN_SKY_DEPTH 0xFFFC /* The z-buffer value of the sky */
#define SUN_GLARE_FADE_STEP 0.1f
#define SUN_GLARE_GROWTH_STEP 0.3f
#define SUN_GLARE_FLARE_STRENGTH 1.5f
#define SUN_GLARE_FULL_STRENGTH 1.0f

extern f32 sSunScreenX;
extern f32 sSunScreenY;
extern u16 sSunDepth;
extern s32 sIsSunGlareSettled;
extern f32 sSunGlareStrength;

/* The sky's vertices, for each view and each matrix buffer */
#define SKY_VIEW_COUNT 2
#define SKY_VERTEX_COUNT 10
#define SKY_COLORED_VERTEX_COUNT 5
#define SKY_BLEND_STEP_COUNT 1000 /* sSkyBlendStep counts to this at most */

extern Vtx sSkyVertices[SKY_VIEW_COUNT * 2][SKY_VERTEX_COUNT];
extern s32 sSkyTimeIndex;
extern s32 sSkyBlendStep;
extern s32 sSkyBlendStepCount;

extern s32 D_801B609C;
extern s32 sIsPrimitiveDepthSet;
extern s32 sMenuBackdrop;
extern s32 sHudAnimationFramesLeft;

extern s32 sHudAnimation;
/*
 * The frame that the HUD animations break apart, copied two pixels to a word
 * from inside the HUD's margins. The tile animations copy it as a grid of
 * tiles, and HUD_ANIMATION_SLICE_CAPTURE copies a slice of the middle of the
 * frame each time it starts, for HUD_ANIMATION_SLICES to show.
 */
#define FRAME_WORDS_PER_ROW (SCREEN_WIDTH / 2)
#define FRAME_MARGIN_ROWS 8
#define FRAME_MARGIN_WORDS 4
#define FRAME_TILE_ROW_COUNT 7
#define FRAME_TILE_COLUMN_COUNT 5
#define FRAME_TILE_ROWS 32
#define FRAME_TILE_WORDS 32
#define FRAME_SLICE_COUNT 8
#define FRAME_SLICE_TOP 88
#define FRAME_SLICE_BOTTOM 208
#define FRAME_SLICE_LEFT_WORD 40
#define FRAME_SLICE_RIGHT_WORD 80
#define FRAME_SLICE_WORD_COUNT ((FRAME_SLICE_BOTTOM - FRAME_SLICE_TOP) * (FRAME_SLICE_RIGHT_WORD - FRAME_SLICE_LEFT_WORD))

/*
 * The slices that the slice animation copied fill the screen, a column of
 * them at a time, each drawn in strips that fit in texture memory. A new
 * slice appears every SLICE_FRAME_COUNT frames, with a sound.
 */
#define SLICE_ANIMATION_FRAME_COUNT 64
#define SLICE_FRAME_COUNT 8
#define SLICE_WIDTH ((FRAME_SLICE_RIGHT_WORD - FRAME_SLICE_LEFT_WORD) * 2)
#define SLICE_HEIGHT (FRAME_SLICE_BOTTOM - FRAME_SLICE_TOP)
#define SLICE_COLUMN_COUNT (SCREEN_WIDTH / SLICE_WIDTH)
#define SLICE_STRIP_COUNT 8
#define SLICE_STRIP_HEIGHT (SLICE_HEIGHT / SLICE_STRIP_COUNT)
#define SLICE_STRIP_WORD_COUNT (SLICE_STRIP_HEIGHT * SLICE_WIDTH / 2)
#define SOUND_SLICE 0x45
#define SLICE_SOUND_CHANNEL 13
#define SLICE_SOUND_VOLUME 127

/* The corners of the tiles, for each of two frames */
#define TILE_GRID_ROW_COUNT 8
#define TILE_GRID_COLUMN_COUNT 6
#define TILE_GRID_LEFT (-152)
#define TILE_GRID_RIGHT 152
#define TILE_GRID_TOP 112
#define TILE_GRID_SPACING 64
#define TILE_GRID_ROW_SPACING 32

extern u32 *sCapturedFrame;
extern Vtx sTileGridVertices[2][TILE_GRID_ROW_COUNT * TILE_GRID_COLUMN_COUNT];
extern s32 sIsFrameCapturePending;
extern s32 sHudAnimationFrame;
/* The frame buffer of the last frame, kept for the next frame's captures */
extern u16 *sLastFrameBuffer;

/*
 * The plain backdrop copies the last frame, inside a border, into
 * sCapturedFrame in strips. Each strip loads two rows more than it steps,
 * and the last loads only the rows that are left.
 */
#define BACKDROP_BORDER 8
#define BACKDROP_RIGHT (SCREEN_WIDTH - BACKDROP_BORDER - 1)
#define BACKDROP_STRIP_COUNT 38
#define BACKDROP_STRIP_STEP 6
#define BACKDROP_STRIP_HEIGHT 8
#define BACKDROP_LAST_STRIP_HEIGHT 2
#define STRIP_HEIGHT(strip) \
    ((strip) != BACKDROP_STRIP_COUNT - 1 ? BACKDROP_STRIP_HEIGHT : BACKDROP_LAST_STRIP_HEIGHT)
#define COPY_MODE_DSDX (4 << 10) /* Copy mode writes four texels a cycle */
#define BACKDROP_START_FADE_LEVEL 255
extern s32 sBackdropFadeLevel;

void CaptureHudAnimationFrame(void);
void func_8009232C(Gfx **displayListHead);
void func_800947A8(void);
void func_80094228(void);
void DrawSliceAnimation(void);
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

/* Grows or fades the sun's glare by whether the z-buffer shows anything in front of the sun */
void UpdateSunGlare(void) {
    u16 *zBuffer;

    if (sSunScreenX >= SUN_DEPTH_MARGIN && sSunScreenX <= SCREEN_WIDTH - SUN_DEPTH_MARGIN &&
        sSunScreenY >= SUN_DEPTH_MARGIN && sSunScreenY <= SCREEN_HEIGHT - SUN_DEPTH_MARGIN) {
        zBuffer = nuGfxZBuffer;
        sSunDepth = zBuffer[(s32)sSunScreenY * SCREEN_WIDTH + (s32)sSunScreenX];
        if (sSunDepth >= SUN_SKY_DEPTH) {
            goto grow;
        }
        sIsSunGlareSettled = FALSE;
        sSunGlareStrength -= SUN_GLARE_FADE_STEP;
        if (sSunGlareStrength < 0.0f) {
            sSunGlareStrength = 0.0f;
        }
    } else if (sSunScreenX > -SUN_GLARE_MARGIN && sSunScreenX < SCREEN_WIDTH + SUN_GLARE_MARGIN &&
               sSunScreenY > -SUN_GLARE_MARGIN && sSunScreenY < SCREEN_HEIGHT + SUN_GLARE_MARGIN) {
    grow:
        if (!sIsSunGlareSettled) {
            sSunGlareStrength += SUN_GLARE_GROWTH_STEP;
        }
        if (sSunGlareStrength > SUN_GLARE_FLARE_STRENGTH && !sIsSunGlareSettled) {
            sIsSunGlareSettled = TRUE;
            sSunGlareStrength = SUN_GLARE_FULL_STRENGTH;
        }
    } else {
        sSunDepth = 0;
        sIsSunGlareSettled = FALSE;
        sSunGlareStrength = 0.0f;
    }
}

INCLUDE_ASM("asm/nonmatchings/hole_display", func_8008D3F4);

/*
 * Moves the sky's and the fog's colors a step on from those of one time of
 * day towards those of the next
 */
void BlendSkyColors(void) {
    SkyColors *from;
    SkyColors *to;
    s32 group;
    s32 channel;

    if (sSkyBlendStep < sSkyBlendStepCount) {
        sSkyBlendStep++;
    }
    from = &g_SkyColors[GetCourseRecordIndex()][g_HoleConditions.isRaining * SKY_TIME_COUNT + sSkyTimeIndex];
    to = &g_SkyColors[GetCourseRecordIndex()][g_HoleConditions.isRaining * SKY_TIME_COUNT + sSkyTimeIndex + 1];
    for (group = 0; group != SKY_VIEW_COUNT * 2; group++) {
        for (channel = 0; channel != 3; channel++) {
            sSkyVertices[group][0].v.cn[channel] =
                from->unk4[channel] + (to->unk4[channel] - from->unk4[channel]) * sSkyBlendStep / SKY_BLEND_STEP_COUNT;
            sSkyVertices[group][1].v.cn[channel] =
                from->unk7[channel] + (to->unk7[channel] - from->unk7[channel]) * sSkyBlendStep / SKY_BLEND_STEP_COUNT;
            sSkyVertices[group][2].v.cn[channel] =
                from->unk7[channel] + (to->unk7[channel] - from->unk7[channel]) * sSkyBlendStep / SKY_BLEND_STEP_COUNT;
            sSkyVertices[group][3].v.cn[channel] =
                from->unk4[channel] + (to->unk4[channel] - from->unk4[channel]) * sSkyBlendStep / SKY_BLEND_STEP_COUNT;
            sSkyVertices[group][4].v.cn[channel] =
                from->unk4[channel] + (to->unk4[channel] - from->unk4[channel]) * sSkyBlendStep / SKY_BLEND_STEP_COUNT;
        }
    }
    for (channel = 0; channel != 3; channel++) {
        g_SceneFog.color[channel] = from->fogColor[channel] +
                                    (to->fogColor[channel] - from->fogColor[channel]) * sSkyBlendStep / SKY_BLEND_STEP_COUNT;
    }
    g_SceneFog.near = from->fogNear + (to->fogNear - from->fogNear) * sSkyBlendStep / SKY_BLEND_STEP_COUNT;
    g_SceneFog.far = from->fogFar + (to->fogFar - from->fogFar) * sSkyBlendStep / SKY_BLEND_STEP_COUNT;
}

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
        sIsFrameCapturePending = FALSE;
        sHudAnimationFramesLeft = 0;
        return;
    }
    if (animation != HUD_ANIMATION_SLICE_CAPTURE && animation != HUD_ANIMATION_SLICES) {
        g_CapturedSliceCount = 0;
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
        case HUD_ANIMATION_SLICES:
            sHudAnimationFramesLeft = 64;
            break;
        case HUD_ANIMATION_NEGATIVE:
        case HUD_ANIMATION_UNK7:
            sHudAnimationFramesLeft = 32;
            break;
    }
    sHudAnimation = animation;
    sIsFrameCapturePending = TRUE;
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

/* Starts the plain menu backdrop by capturing the last frame for it */
#ifdef NON_MATCHING
void CaptureMenuBackdrop(Gfx **displayListHead, s32 unused) {
    Gfx *gfx = *displayListHead;
    s32 strip;
    s32 height;
    u16 *source;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_COPY);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    if (g_MenuBackdropStyle == MENU_BACKDROP_STYLE_PLAIN) {
        g_MenuBackdropStyle = MENU_BACKDROP_STYLE_UNK2;
        sBackdropFadeLevel = BACKDROP_START_FADE_LEVEL;
        gDPPipeSync(gfx++);
        gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, OS_K0_TO_PHYSICAL(sCapturedFrame) & ~7);
        gDPPipeSync(gfx++);
        source = sLastFrameBuffer;
        for (strip = 0; strip != BACKDROP_STRIP_COUNT; strip++) {
            height = STRIP_HEIGHT(strip);
            gDPLoadTextureTile(gfx++, (u32)source & ~7,
                               G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, 0, BACKDROP_BORDER, BACKDROP_BORDER,
                               BACKDROP_RIGHT, BACKDROP_BORDER + height - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
                               G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            gSPScisTextureRectangle(gfx++, BACKDROP_BORDER << 2, (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER) << 2,
                                    BACKDROP_RIGHT << 2,
                                    (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER + height - 1) << 2,
                                    G_TX_RENDERTILE, BACKDROP_BORDER << 5, BACKDROP_BORDER << 5, COPY_MODE_DSDX,
                                    1 << 10);
            gDPPipeSync(gfx++);
            source += BACKDROP_STRIP_STEP * SCREEN_WIDTH;
        }
        gDPPipeSync(gfx++);
        gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, OS_K0_TO_PHYSICAL(nuGfxCfb_ptr) & ~7);
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_display", CaptureMenuBackdrop);
#endif

INCLUDE_ASM("asm/nonmatchings/hole_display", func_800939E8);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_80094228);

INCLUDE_ASM("asm/nonmatchings/hole_display", func_800947A8);

/* Copies the frame that a HUD animation that has just started breaks apart */
#ifdef NON_MATCHING
void CaptureHudAnimationFrame(void) {
    u32 *frame;
    s32 wordIndex;
    s32 tileRow;
    s32 tileColumn;
    s32 row;
    s32 column;
    Vtx *vertex;
    Vtx *nextVertex;
    s32 y;
    s32 x;

    if (!sIsFrameCapturePending) {
        return;
    }
    frame = (u32 *)nuGfxCfb_ptr;
    sIsFrameCapturePending = FALSE;
    wordIndex = 0;
    if (sHudAnimation == HUD_ANIMATION_UNK2 || sHudAnimation == HUD_ANIMATION_UNK3) {
        for (tileRow = 0; tileRow != FRAME_TILE_ROW_COUNT; tileRow++) {
            for (tileColumn = 0; tileColumn != FRAME_TILE_COLUMN_COUNT; tileColumn++) {
                for (row = FRAME_MARGIN_ROWS; row != FRAME_MARGIN_ROWS + FRAME_TILE_ROWS; row++) {
                    for (column = FRAME_MARGIN_WORDS; column != FRAME_MARGIN_WORDS + FRAME_TILE_WORDS; column++) {
                        sCapturedFrame[wordIndex++] =
                            frame[row * FRAME_WORDS_PER_ROW + column + tileColumn * FRAME_TILE_WORDS +
                                  tileRow * FRAME_TILE_ROWS * FRAME_WORDS_PER_ROW];
                    }
                }
            }
        }
        vertex = sTileGridVertices[0];
        nextVertex = sTileGridVertices[1];
        for (row = 0, y = TILE_GRID_TOP; row != TILE_GRID_ROW_COUNT; row++, y -= TILE_GRID_ROW_SPACING) {
            for (column = 0, x = TILE_GRID_LEFT; column != TILE_GRID_COLUMN_COUNT - 1; column++, x += TILE_GRID_SPACING) {
                vertex->v.ob[0] = x;
                vertex->v.ob[1] = y;
                vertex->v.ob[2] = 0;
                nextVertex->v.ob[0] = x;
                nextVertex->v.ob[1] = y;
                nextVertex->v.ob[2] = 0;
                vertex++;
                nextVertex++;
            }
            /* The last column is narrower, ending inside the HUD's margin */
            for (column = TILE_GRID_COLUMN_COUNT - 1, x = TILE_GRID_RIGHT; column != TILE_GRID_COLUMN_COUNT;
                 column++, x += TILE_GRID_SPACING) {
                vertex->v.ob[0] = x;
                vertex->v.ob[1] = y;
                vertex->v.ob[2] = 0;
                nextVertex->v.ob[0] = x;
                nextVertex->v.ob[1] = y;
                nextVertex->v.ob[2] = 0;
                vertex++;
                nextVertex++;
            }
        }
    } else if (sHudAnimation == HUD_ANIMATION_UNK1 || sHudAnimation == HUD_ANIMATION_UNK9 ||
               sHudAnimation == HUD_ANIMATION_UNK8 || sHudAnimation == HUD_ANIMATION_UNK10) {
        for (row = FRAME_MARGIN_ROWS; row != SCREEN_HEIGHT - FRAME_MARGIN_ROWS; row++) {
            for (column = FRAME_MARGIN_WORDS; column != FRAME_WORDS_PER_ROW - FRAME_MARGIN_WORDS; column++) {
                sCapturedFrame[wordIndex++] = frame[row * FRAME_WORDS_PER_ROW + column];
            }
        }
    } else if (sHudAnimation == HUD_ANIMATION_SLICE_CAPTURE) {
            if (g_CapturedSliceCount < FRAME_SLICE_COUNT) {
                wordIndex = 0;
                for (row = FRAME_SLICE_TOP; row != FRAME_SLICE_BOTTOM; row++) {
                    for (column = FRAME_SLICE_LEFT_WORD; column != FRAME_SLICE_RIGHT_WORD;) {
                        sCapturedFrame[g_CapturedSliceCount * FRAME_SLICE_WORD_COUNT + wordIndex++] =
                            frame[row * FRAME_WORDS_PER_ROW + column++];
                    }
                }
                g_CapturedSliceCount++;
            }
    } else if (sHudAnimation == HUD_ANIMATION_UNK7) {
    } else if (sHudAnimation == HUD_ANIMATION_NEGATIVE) {
            for (row = FRAME_MARGIN_ROWS; row != SCREEN_HEIGHT - FRAME_MARGIN_ROWS; row++) {
                for (column = FRAME_MARGIN_WORDS; column != FRAME_WORDS_PER_ROW - FRAME_MARGIN_WORDS; column++) {
                    sCapturedFrame[wordIndex++] = ~frame[row * FRAME_WORDS_PER_ROW + column];
                }
            }
    }
    sHudAnimationFrame = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_display", CaptureHudAnimationFrame);
#endif

/* Draws the slice animation, which fills the screen with copies of its middle */
#ifdef NON_MATCHING
void DrawSliceAnimation(void) {
    s32 frame;
    s32 sliceCount;
    s32 slice;
    s32 strip;
    s32 column;
    s32 wordIndex;
    s32 sliceY;
    s32 y;

    if (sHudAnimationFramesLeft <= 0) {
        return;
    }
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTexturePersp(g_DisplayListHead++, G_TP_NONE);
    gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
    gDPPipeSync(g_DisplayListHead++);
    column = 0;
    wordIndex = 0;
    frame = SLICE_ANIMATION_FRAME_COUNT - sHudAnimationFramesLeft;
    sliceCount = frame / SLICE_FRAME_COUNT + 1;
    if (frame % SLICE_FRAME_COUNT == 0) {
        PlaySound(SOUND_SLICE, SLICE_SOUND_CHANNEL, SLICE_SOUND_VOLUME);
    }
    sliceY = 0;
    for (slice = 0; slice != sliceCount; slice++) {
        y = sliceY;
        for (strip = 0; strip != SLICE_STRIP_COUNT; strip++) {
            gDPLoadTextureBlock(g_DisplayListHead++, (u32)&sCapturedFrame[wordIndex] & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b,
                                SLICE_WIDTH, SLICE_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            wordIndex += SLICE_STRIP_WORD_COUNT;
            gSPTextureRectangle(g_DisplayListHead++, column * SLICE_WIDTH << 2, y << 2,
                                (column * SLICE_WIDTH + SLICE_WIDTH) << 2, (y + SLICE_STRIP_HEIGHT) << 2,
                                G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
            gDPPipeSync(g_DisplayListHead++);
            y += SLICE_STRIP_HEIGHT;
        }
        column++;
        if (column == SLICE_COLUMN_COUNT) {
            column = 0;
            sliceY += SLICE_HEIGHT;
        }
    }
    sHudAnimationFramesLeft--;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_display", DrawSliceAnimation);
#endif

/* Draws the animation that StartHudAnimation started over the frame */
void DrawHudAnimation(void) {
    CaptureHudAnimationFrame();
    gDPPipeSync(g_DisplayListHead++);
    gDPSetColorDither(g_DisplayListHead++, G_CD_DISABLE);
    switch (sHudAnimation) {
        case HUD_ANIMATION_UNK1:
        case HUD_ANIMATION_NEGATIVE:
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
        case HUD_ANIMATION_SLICES:
            DrawSliceAnimation();
            break;
        case HUD_ANIMATION_UNK10:
            func_80092F18(&g_DisplayListHead);
            break;
    }
    sLastFrameBuffer = nuGfxCfb_ptr;
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

