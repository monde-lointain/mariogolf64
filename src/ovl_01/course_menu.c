/*
 * The clubhouse's course statistics: a wheel of the open courses that turns
 * from one to the next, and the chosen course's panel of statistics.
 */
#include "course_menu.h"

#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "compiler.h"
#include "course.h"
#include "course_stats.h"
#include "cpu_projection.h"
#include "game.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "hole_display.h"
#include "include_asm.h"
#include "language.h"
#include "main_menu.h"
#include "math_util.h"
#include "menu.h"
#include "menu_arrows.h"
#include "menu_memory.h"
#include "menu_number.h"
#include "save.h"
#include "screen.h"
#include "sound.h"
#include "text.h"

/* A gold coin with a star, reading HOLE IN ONE */
#define ASSET_HOLE_IN_ONE_BADGE 0x8EA
/* Orange 1 to 6, then up and down arrows; the course stats draw only the
 * arrows */
#define ASSET_DIGITS_AND_ARROWS 0x5A5

/* The images of g_CourseIconAssets that the course menu reads */
#define COURSE_MENU_IMAGE_COUNT 7

/* DrawCourseBanner squeezes the banner's strips under the course menu's title,
 * with the course's name on it */
#define COURSE_BANNER_TOP 110
#define COURSE_BANNER_STRIP_SPACING 8
#define COURSE_NAME_Y 116

/* The course's statistics: the panel behind them, and the rows of it that show
 */
/* How far UpdateCourseStats turns the course wheel each move, and how fast */
#define STATS_TURN 90
#define STATS_SPIN_SPEED 18

#define STATS_COUNT 10
#define STATS_SHOWN_ROW_COUNT 4
#define STATS_PANEL_LEFT 24
#define STATS_PANEL_TOP 138
#define STATS_PANEL_RIGHT 302
#define STATS_PANEL_BOTTOM 230
#define STATS_LABEL_X 30
#define STATS_FIRST_Y 154
#define STATS_LINE_HEIGHT 16
/* The statistics in the order the panel lists them */
#define STAT_ROUND_COUNT 0
#define STAT_BEST_SCORE 1
#define STAT_AVERAGE_SCORE 2
#define STAT_BEST_DRIVE 3
#define STAT_AVERAGE_DRIVE 4
#define STAT_FAIRWAY_PERCENT 5
#define STAT_AVERAGE_PUTTS 6
#define STAT_PAR_SAVE_PERCENT 7
#define STAT_PAR_ON_PERCENT 8
#define STAT_SAND_SAVE_PERCENT 9
/* Where DrawStatFigure draws the figures; shorter best scores move right to
 * line up */
#define STATS_ROUND_COUNT_X 96
#define STATS_VALUE_X 112
#define STATS_TWO_DIGIT_X 128
#define STATS_ONE_DIGIT_X 144
#define STATS_UNIT_X 178
#define STATS_UNIT_OFFSET_Y 4
/* The 16x16 CI4 glyphs of ASSET_LARGE_DIGITS in f_MenuGlyphs; two of them spell
 * "OFF" */
#define MENU_GLYPH_SIZE 16
#define MENU_GLYPH_IMAGE_SIZE (MENU_GLYPH_SIZE * MENU_GLYPH_SIZE / 2)
#define MENU_GLYPH_TIMES 0
#define MENU_GLYPH_FIRST_DIGIT 1
#define MENU_GLYPH_PLUS 12
#define MENU_GLYPH_MINUS 13
#define MENU_GLYPH_PLUS_MINUS 14
#define MENU_GLYPH_CIRCLE 15
#define OFF_FIRST_GLYPH 16
#define OFF_GLYPH_COUNT 2
#define MENU_GLYPH_POINT 18
/* The largest number written in one decimal digit, and in two */
#define MAX_ONE_DIGIT_NUMBER (DECIMAL_BASE - 1)
#define MAX_TWO_DIGIT_NUMBER ((DECIMAL_BASE * DECIMAL_BASE) - 1)
/* Whether a signed number's magnitude takes one digit, and two */
#define IsOneDigitNumber(n) ((n) >= -MAX_ONE_DIGIT_NUMBER && (n) <= MAX_ONE_DIGIT_NUMBER)
#define IsTwoDigitNumber(n) \
    (((n) >= DECIMAL_BASE && (n) <= MAX_TWO_DIGIT_NUMBER) || \
        ((n) >= -MAX_TWO_DIGIT_NUMBER && (n) <= -DECIMAL_BASE))

/* The arrows beside the statistics that show there are more rows above or below
 */
#define STATS_FIRST_ARROW_IMAGE 16
#define STATS_ARROW_WIDTH 16
#define STATS_ARROW_HEIGHT 9
#define STATS_ARROW_IMAGE_SIZE (STATS_ARROW_WIDTH * STATS_ARROW_HEIGHT / 2)
#define STATS_ARROW_LEFT 32
#define STATS_UP_ARROW_TOP 144
#define STATS_DOWN_ARROW_TOP 216

/* The blinking arrows that DrawBlinkingSideArrows draws either side of a box,
 * from the third image on */
#define SIDE_ARROW_COUNT 2
#define SIDE_ARROW_RIGHT 0
#define SIDE_ARROW_LEFT 1
#define SIDE_ARROW_FIRST_IMAGE MENU_ARROW_RIGHT
#define SIDE_ARROW_BLINK_FRAME_COUNT 4

/* The birdie badge icon of f_CourseStatsBadge and the count beside it */
#define BADGE_ICON_X 222
#define BADGE_ICON_Y 144
#define BADGE_COUNT_X 250
#define BADGE_COUNT_Y 146
#define BADGE_COUNT_DIGIT_WIDTH 16

/* DrawCourseTrophies draws the course's tournament trophies, spinning, or on
 * the totals page the course points */
#define TROPHY_SPACING 24
#define TROPHY_SHIFT_LEFT 20
#define TROPHY_FRAME_DELAY 4
#define POINTS_NEEDED_X 192
#define POINTS_NEEDED_Y 174
#define ALL_COURSES_OPEN_X 190
#define ALL_COURSES_OPEN_Y 180
#define MESSAGE_POINTS_TO_NEXT_COURSE 0x163
#define MESSAGE_ALL_COURSES_OPEN 0x164

typedef struct TrophyPosition {
    s32 x;
    /*
     * Unsigned: the original drops the rectangle's clamp of a negative top
     * edge where it works out the texture's t coordinate
     */
    u32 y;
} TrophyPosition;

extern const char f_StatsStringFormat[];
extern const char f_NumberedCourseFormat[];

void DrawStatFigure(Gfx **displayListHead, s16 statIndex, u8 row);
void DrawThreeDigitMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 mark);
void DrawFourDigitMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 mark);
void DrawDecimalMenuNumber(Gfx **displayListHead, s32 x, s32 y, f32 value);
void DrawMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 mark, u8 field);

/* The STAT_* shown on the panel's top row, which Up and Down scroll */
static s16 f_FirstShownStat = 0;
/* The trophy's frame of g_TrophyFrameAssets, stepped when its timer reaches
 * TROPHY_FRAME_DELAY */
static s16 f_StatsTrophyFrame = 0;
/* Which of f_StatsTrophyBuffers holds the trophy frame shown */
static s16 f_ShownStatsTrophyBufferIndex = 0;
/* Counts the trophy's draws since its frame last stepped */
static s16 f_StatsTrophyFrameTimer = 0;
/* The blink frame and timer of the arrows beside the course wheel */
static s32 f_StatsArrowFrame = 0;
static s32 f_StatsArrowTimer = 0;
/* Read by no routine */
static s32 f_UnreadStatsWord UNUSED = 0;
/* The panel's label for each STAT_* */
static const char *f_StatLabels[STATS_COUNT] = {
    "Round",
    "Best Score",
    "Average",
    "Best Drive",
    "Avg. Drive",
    "On Fairway",
    "Avg. Putt",
    "Par Saves",
    "Par Ons",
    "Sand Saves",
};
/* DrawCourseStats adds a tenth to it on every draw, but no routine reads it */
static f32 f_UnreadStatsDrawTime = 0.0f;
/* Read by no routine */
static char f_UnreadStatsTexts[3][10] UNUSED = {"EXP", "ｺｰｽｹﾞｯﾄ", "純ﾞ"};
/* Where each course's first trophy sits, before its shift for the place */
static TrophyPosition f_StatsTrophyPositions[COURSE_COUNT] =
    {{230, 177}, {230, 177}, {230, 177}, {236, 165}, {226, 184}, {236, 175}};

/* The camera's matrices that the course wheel's points are projected with */
static Mat4 f_WheelProjection;
static Mat4 f_WheelView;
/* The spinning badge, read after the hole-in-one badge over the same pointer */
static void *f_CourseStatsBadge;
/* The course stats' TROPHY_FRAME_BUFFER_COUNT buffers for the trophy frame */
static void **f_StatsTrophyBuffers;
/* LoadCourseTrophyImages keeps these for COURSE_BUFFER_SECOND and
 * COURSE_BUFFER_LAST, but no routine reads them */
static void **f_UnreadSecondTrophyBuffers;
static void **f_UnreadLastTrophyBuffers;
/* The wheel's icons of each course and the totals, from LoadCourseStatsIcons */
static void **f_CourseStatsIcons;
/* The images that LoadCourseMenuBuffer reads, by their assets */
static void *f_CourseStatsNamePlate;
static void *f_CourseStatsDigitsAndArrows;
static void *f_MenuGlyphs;
static void *f_CourseStatsGreenArrows;

/* Read by no routine; 2 objects, as one would be aligned past its start */
static u8 f_UnreadStatsBytes[0x4] UNUSED;
static u8 f_UnreadStatsBlock[0x188] UNUSED;

/* Reads every course's icon into the course stats' buffers */
void LoadCourseStatsIcons(void **buffers) {
    EncodedAsset asset;
    /* Pads the frame to match; without it 24 words differ */
    s32 unused[4] UNUSED;
    s16 imageIndex;

    f_CourseStatsIcons = buffers;
    for (imageIndex = 0; imageIndex != COURSE_MENU_IMAGE_COUNT; imageIndex++) {
        OpenEncodedAsset(g_CourseIconAssets[imageIndex], &asset);
        ReadEncodedAsset(f_CourseStatsIcons[imageIndex], &asset);
    }
}

/*
 * Keeps the buffers as the kind's set, then reads the chosen course's first
 * trophy image into both of them and the trophy's three place palettes
 */
void LoadCourseTrophyImages(void **buffers, s32 requestedKind) {
    EncodedAsset asset;
    AssetFile file;
    s8 bufferKind = requestedKind;
    MenuMemory *memory = GetMenuMemory();
    s16 index;

    if ((s8)requestedKind < 0) {
        bufferKind = COURSE_BUFFER_FIRST;
    }

    if (bufferKind > COURSE_BUFFER_LAST) {
        bufferKind = COURSE_BUFFER_LAST;
    }

    switch (bufferKind) {
        case COURSE_BUFFER_FIRST:
            f_StatsTrophyBuffers = buffers;
            break;
        case COURSE_BUFFER_SECOND:
            f_UnreadSecondTrophyBuffers = buffers;
            break;
        case COURSE_BUFFER_LAST:
            f_UnreadLastTrophyBuffers = buffers;
            break;
    }

    if (memory->statsCourse != COURSE_COUNT) {
        for (index = 0; index != TROPHY_FRAME_BUFFER_COUNT; index++) {
            OpenEncodedAsset(g_TrophyFrameAssets[memory->statsCourse][0], &asset);
            ReadEncodedAsset(buffers[index], &asset);
        }

        for (index = 0; index != TROPHY_PALETTE_COUNT; index++) {
            OpenAssetFile(g_TrophyPaletteFiles[memory->statsCourse][index], &file);
            ReadAssetFile(g_TrophyPalettes[index], GetAssetFileSize(&file), &file);
        }
    }
}

void LoadCourseMenuBuffer(void *buffer, s16 bufferKind) {
    EncodedAsset asset;

    switch (bufferKind) {
        /* The course menu keeps no buffer of kind 1 */
        case COURSE_MENU_BUFFER_UNUSED:
            break;
        case COURSE_MENU_BUFFER_HOLE_IN_ONE_BADGE:
            f_CourseStatsBadge = buffer;
            OpenEncodedAsset(ASSET_HOLE_IN_ONE_BADGE, &asset);
            ReadEncodedAsset(f_CourseStatsBadge, &asset);
            break;
        case COURSE_MENU_BUFFER_NAME_PLATE:
            f_CourseStatsNamePlate = buffer;
            OpenEncodedAsset(ASSET_NAME_PLATE, &asset);
            ReadEncodedAsset(f_CourseStatsNamePlate, &asset);
            break;
        case COURSE_MENU_BUFFER_SPINNING_BADGE:
            f_CourseStatsBadge = buffer;
            OpenEncodedAsset(ASSET_SPINNING_BADGE, &asset);
            ReadEncodedAsset(f_CourseStatsBadge, &asset);
            break;
        case COURSE_MENU_BUFFER_DIGITS_AND_ARROWS:
            f_CourseStatsDigitsAndArrows = buffer;
            OpenEncodedAsset(ASSET_DIGITS_AND_ARROWS, &asset);
            ReadEncodedAsset(f_CourseStatsDigitsAndArrows, &asset);
            break;
        case COURSE_MENU_BUFFER_LARGE_DIGITS:
            f_MenuGlyphs = buffer;
            OpenEncodedAsset(ASSET_LARGE_DIGITS, &asset);
            ReadEncodedAsset(f_MenuGlyphs, &asset);
            break;
        case COURSE_MENU_BUFFER_GREEN_ARROWS:
            f_CourseStatsGreenArrows = buffer;
            OpenEncodedAsset(ASSET_GREEN_ARROWS, &asset);
            ReadEncodedAsset(f_CourseStatsGreenArrows, &asset);
            f_StatsArrowFrame = 0;
            f_StatsArrowTimer = 0;
            break;
    }
}

void UpdateCourseStats(void) {
    EncodedAsset asset;
    AssetFile file;
    MenuMemory *memory = GetMenuMemory();
    s32 courseCount = GetOpenCourseCount();
    Boolean isDone;
    s32 index;
    s32 direction;
    s32 step;

    if (courseCount < 1) {
        courseCount = 1;
    }

    if (courseCount > COURSE_COUNT) {
        courseCount = COURSE_COUNT;
    }

    isDone = IsHudAnimationDone();
    if (isDone == TRUE) {
        if (g_MenuPressedButtons & B_BUTTON) {
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                0,
                TEXT_FLAG_TYPED,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
            g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
            return;
        }

        if (memory->statsSpin == 0) {
            if (g_MenuRepeatButtons & R_JPAD) {
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                SetTextSlot(
                    0,
                    0,
                    NO_MESSAGE,
                    TEXT_SLOT_LARGE_FONT,
                    0,
                    TEXT_FLAG_TYPED,
                    255,
                    255,
                    255,
                    OPAQUE_ALPHA
                );
                memory->statsCourse++;
                if (memory->statsCourse > courseCount) {
                    memory->statsCourse = 0;
                }

                memory->statsSpin = -STATS_TURN / (courseCount + 1);
                if (memory->statsCourse != courseCount) {
                    f_StatsTrophyFrameTimer = 0;
                    f_StatsTrophyFrame = 0;
                    f_ShownStatsTrophyBufferIndex ^= 1;
                    OpenEncodedAsset(g_TrophyFrameAssets[memory->statsCourse][0], &asset);
                    ReadEncodedAsset(f_StatsTrophyBuffers[f_ShownStatsTrophyBufferIndex], &asset);
                    for (index = 0; index != TROPHY_PALETTE_COUNT; index++) {
                        OpenAssetFile(g_TrophyPaletteFiles[memory->statsCourse][index], &file);
                        ReadAssetFile(g_TrophyPalettes[index], GetAssetFileSize(&file), &file);
                    }
                }
            }
            else if (g_MenuRepeatButtons & L_JPAD) {
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                SetTextSlot(
                    0,
                    0,
                    NO_MESSAGE,
                    TEXT_SLOT_LARGE_FONT,
                    0,
                    TEXT_FLAG_TYPED,
                    255,
                    255,
                    255,
                    OPAQUE_ALPHA
                );
                memory->statsCourse--;
                if (memory->statsCourse < 0) {
                    memory->statsCourse = courseCount;
                }

                memory->statsSpin = STATS_TURN / (courseCount + 1);
                if (memory->statsCourse != courseCount) {
                    f_StatsTrophyFrameTimer = 0;
                    f_StatsTrophyFrame = 0;
                    f_ShownStatsTrophyBufferIndex ^= 1;
                    OpenEncodedAsset(g_TrophyFrameAssets[memory->statsCourse][0], &asset);
                    ReadEncodedAsset(f_StatsTrophyBuffers[f_ShownStatsTrophyBufferIndex], &asset);
                    for (index = 0; index != TROPHY_PALETTE_COUNT; index++) {
                        OpenAssetFile(g_TrophyPaletteFiles[memory->statsCourse][index], &file);
                        ReadAssetFile(g_TrophyPalettes[index], GetAssetFileSize(&file), &file);
                    }
                }
            }
            else if (g_MenuRepeatButtons & U_JPAD) {
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                if (f_FirstShownStat > 0) {
                    f_FirstShownStat--;
                }
            }
            else if (g_MenuRepeatButtons & D_JPAD) {
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                if (f_FirstShownStat < STATS_COUNT - STATS_SHOWN_ROW_COUNT) {
                    f_FirstShownStat++;
                }
            }
        }

        if (Abs(memory->statsSpin) != 0) {
            direction = memory->statsSpin < 0 ? -1 : 1;
            step = STATS_SPIN_SPEED / (courseCount + 1);
            if (step == 0) {
                step = 1;
            }

            memory->statsSpin -= direction * step;

            /* Stop on reaching the course rather than turning past it */
            if ((memory->statsSpin < 0 ? -1 : 1) != direction) {
                memory->statsSpin = 0;
            }
        }
    }
}

void DrawCourseTrophies(Gfx **displayListHead);

void DrawStatsBadgeCount(Gfx **displayListHead);

void DrawStatsPanel(Gfx **displayListHead);

void DrawStatsScrollArrows(Gfx **displayListHead);

/* DrawCourseStats turns a totals image on the wheel of menu.h after the open
 * courses' images */
#define COURSE_TOTALS_IMAGE COURSE_COUNT

/*
 * Draws the course stats menu: the open courses' images and the totals image
 * on a turning wheel, nearest last, then the arrows, the chosen course's
 * banner, its stats panel, badge count and trophies.
 */
void DrawCourseStats(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    u16 perspNorm;
    f32 iconX[WHEEL_SLOT_LIMIT];
    f32 iconY[WHEEL_SLOT_LIMIT];
    f32 iconScale[WHEEL_SLOT_LIMIT];
    /* Pads the frame to match; without it 180 words differ */
    s32 unusedSlots[WHEEL_SLOT_LIMIT] UNUSED;
    s32 drawOrder[WHEEL_SLOT_LIMIT];
    /* Pads the frame to match; without it 170 words differ */
    s32 unused[2] UNUSED;
    MenuMemory *memory = GetMenuMemory();
    s32 courseCount;
    s32 index;
    s32 sortIndex;
    s32 key;
    s32 statsCourse;
    s32 spin;
    s32 angleStep;
    Radians angle;
    f32 wheelX;
    s32 image;
    s32 slot;
    s32 halfSize;
    s32 step;

    DrawMenuBackdropLayers(&gfx, PLAIN_BACKDROP_LAYERS);
    guPerspectiveF(
        f_WheelProjection,
        &perspNorm,
        WHEEL_FIELD_OF_VIEW,
        SCREEN_ASPECT_RATIO,
        WHEEL_NEAR_PLANE,
        WHEEL_FAR_PLANE,
        1.0f
    );
    guLookAtF(
        f_WheelView,
        0.0f,
        WHEEL_EYE_HEIGHT,
        WHEEL_EYE_DISTANCE,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f
    );
    SetCpuProjectionF(f_WheelProjection, f_WheelView, g_Viewports);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    courseCount = GetOpenCourseCount();
    if (courseCount <= 0) {
        courseCount = 1;
    }

    if (courseCount > COURSE_COUNT) {
        courseCount = COURSE_COUNT;
    }

    statsCourse = memory->statsCourse;
    spin = memory->statsSpin;
    angleStep = DEGREES_PER_TURN / (courseCount + 1);
    for (index = 0; index != courseCount + 1; index++) {
        ScreenPoint point;
        s32 spinDegrees = spin * DEGREES_PER_SPIN_STEP;

        angle = -DegreesToRadians(
            (((statsCourse - index) % (courseCount + 1)) * angleStep) + spinDegrees
        );
        wheelX = sinf(angle) * WHEEL_RADIUS_X;
        if (ProjectPoint(wheelX, 0.0f, cosf(angle) * WHEEL_RADIUS_Z, &point) < 0) {
            iconX[index] = point.x;
            iconY[index] = point.y - WHEEL_RAISE;
            iconScale[index] = point.scaleX;
        }
    }

    for (sortIndex = 0; sortIndex != courseCount + 1; sortIndex++) {
        drawOrder[sortIndex] = sortIndex;
    }

    /* Draw the farthest images first, so the nearer ones cover them */
    for (sortIndex = 1; sortIndex != courseCount + 1; sortIndex++) {
        key = drawOrder[sortIndex];
        for (index = sortIndex - 1; index >= 0 && iconY[drawOrder[index]] > iconY[key]; index--) {
            drawOrder[index + 1] = drawOrder[index];
        }

        drawOrder[index + 1] = key;
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

    f_UnreadStatsDrawTime += 0.1f;
    for (index = 0; index != courseCount + 1; index++) {
        slot = drawOrder[index];
        image = (slot == courseCount) ? COURSE_TOTALS_IMAGE : slot;
        halfSize = iconScale[slot] * COURSE_ICON_HALF_SIZE;
        step = (f32)(COURSE_ICON_STRIP_HEIGHT << TEXEL_STEP_SHIFT) / halfSize;
        gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_CourseStatsIcons[image])));
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        for (sortIndex = 0; sortIndex != COURSE_ICON_STRIP_COUNT; sortIndex++) {
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(
                    LargeImageTexels(f_CourseStatsIcons[image]) +
                    sortIndex * COURSE_ICON_WIDTH * COURSE_ICON_STRIP_HEIGHT
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                COURSE_ICON_WIDTH,
                COURSE_ICON_STRIP_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels((s32)(iconX[slot] - halfSize)),
                QuarterPixels(
                    (s32)(iconY[slot] - halfSize + (sortIndex * halfSize)) - WHEEL_ICON_RAISE
                ),
                QuarterPixels((s32)(iconX[slot] + halfSize)),
                QuarterPixels((s32)(iconY[slot] + (sortIndex * halfSize)) - WHEEL_ICON_RAISE),
                G_TX_RENDERTILE,
                0,
                0,
                step,
                step
            );
            gDPPipeSync(gfx++);
        }
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    DrawBlinkingSideArrows(
        &gfx,
        f_CourseStatsGreenArrows,
        &f_StatsArrowFrame,
        &f_StatsArrowTimer,
        WHEEL_ARROW_WIDTH,
        WHEEL_ARROW_X,
        WHEEL_ARROW_Y
    );
    image = (memory->statsCourse == courseCount) ? COURSE_TOTALS_IMAGE : memory->statsCourse;
    DrawCourseBanner(&gfx, 0, image);
    DrawStatsPanel(&gfx);
    DrawStatsBadgeCount(&gfx);
    DrawStatsScrollArrows(&gfx);
    DrawCourseTrophies(&gfx);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/*
 * Draws the name plate banner squeezed under the screen's title, with the
 * course's number and its tournament's name, or with the course's own name if
 * useCourseNames is set. The totals, after the last course, get no number.
 */
void DrawCourseBanner(Gfx **displayListHead, u8 useCourseNames, s32 courseIndex) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    GetMenuMemory();
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_CourseStatsNamePlate)));
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_CourseStatsNamePlate) +
                (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            MENU_BANNER_WIDTH,
            MENU_BANNER_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        stripTop = COURSE_BANNER_TOP + (stripIndex * COURSE_BANNER_STRIP_SPACING);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(MENU_BANNER_LEFT),
            QuarterPixels(stripTop),
            QuarterPixels(MENU_BANNER_LEFT + MENU_BANNER_WIDTH),
            QuarterPixels(stripTop + COURSE_BANNER_STRIP_SPACING),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TexelRunStep(MENU_BANNER_STRIP_HEIGHT, COURSE_BANNER_STRIP_SPACING)
        );
        gDPPipeSync(gfx++);
    }

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    if (!useCourseNames) {
        if (courseIndex + 1 == COURSE_COUNT + 1) {
            sprintf(g_PrintBuffer, f_StatsStringFormat, g_TournamentNames[courseIndex]);
        }
        else {
            sprintf(
                g_PrintBuffer,
                f_NumberedCourseFormat,
                courseIndex + 1,
                g_TournamentNames[courseIndex]
            );
        }
    }
    else if (courseIndex + 1 == COURSE_COUNT + 1) {
        sprintf(g_PrintBuffer, f_StatsStringFormat, g_CourseRecords[courseIndex].name);
    }
    else {
        sprintf(
            g_PrintBuffer,
            f_NumberedCourseFormat,
            courseIndex + 1,
            g_CourseRecords[courseIndex].name
        );
    }

    DrawCenteredLargeText(&gfx, SCREEN_WIDTH / 2, COURSE_NAME_Y, g_PrintBuffer);
    *displayListHead = gfx;
}

/* Writes a label or the totals' name as it is */
const char f_StatsStringFormat[] = "%s";
/* Writes a course's number before its name */
const char f_NumberedCourseFormat[] = "%d:%s";

/*
 * Draws the dimmed panel of the course's statistics, with the label and figure
 * of each shown row
 */
void DrawStatsPanel(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 row = 0;
    s16 statIndex;

    GetSaveData();
    GetMenuMemory();
    GetOpenCourseCount();
    gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(
        gfx++,
        QuarterPixels(STATS_PANEL_LEFT),
        QuarterPixels(STATS_PANEL_TOP),
        QuarterPixels(STATS_PANEL_RIGHT),
        QuarterPixels(STATS_PANEL_BOTTOM),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    for (statIndex = f_FirstShownStat; statIndex != f_FirstShownStat + STATS_SHOWN_ROW_COUNT;
        statIndex++) {
        SetUpTextDrawing(&gfx);
        SetTextColor(&gfx, 255, 255, 255);
        sprintf(g_PrintBuffer, f_StatsStringFormat, f_StatLabels[statIndex]);
        DrawLargeText(
            &gfx,
            STATS_LABEL_X,
            STATS_FIRST_Y + ((s16)row * STATS_LINE_HEIGHT),
            g_PrintBuffer
        );
        DrawStatFigure(&gfx, statIndex, row);
        row++;
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawPercentMark(Gfx **displayListHead, s16 x, s16 y);
void DrawYardMark(Gfx **displayListHead, s16 x, s16 y);

/*
 * Draws the statistic's figure for the chosen course, or for all courses on the
 * totals, on the panel's row, with its unit; nothing shows before any round has
 * been played
 */
void DrawStatFigure(Gfx **displayListHead, s16 statIndex, u8 row) {
    Gfx *gfx = *displayListHead;
    MenuMemory *memory;
    s32 courseIndex;
    s32 bestScore;
    s32 openCourseCount;

    GetSaveData();
    memory = GetMenuMemory();
    openCourseCount = GetOpenCourseCount();
    if (memory->statsCourse != openCourseCount) {
        courseIndex = memory->statsCourse;
    }
    else {
        /* The figures for all the courses together */
        courseIndex = COURSE_COUNT;
    }

    if (g_CourseStatFigures[courseIndex].roundCount == 0.0f) {
        return;
    }

    switch (statIndex) {
        case STAT_ROUND_COUNT:
            DrawFourDigitMenuNumber(
                &gfx,
                STATS_ROUND_COUNT_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
                g_CourseStatFigures[courseIndex].roundCount,
                MENU_NUMBER_PLAIN
            );
            break;
        case STAT_BEST_SCORE:
            bestScore = g_CourseStatFigures[courseIndex].bestScore;
            if (IsOneDigitNumber(bestScore)) {
                DrawUnalignedMenuNumber(
                    &gfx,
                    STATS_ONE_DIGIT_X,
                    STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
                    bestScore,
                    MENU_NUMBER_SIGNED
                );
            }
            else if (IsTwoDigitNumber(bestScore)) {
                DrawUnalignedMenuNumber(
                    &gfx,
                    STATS_TWO_DIGIT_X,
                    STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
                    bestScore,
                    MENU_NUMBER_SIGNED
                );
            }
            else {
                DrawThreeDigitMenuNumber(
                    &gfx,
                    STATS_VALUE_X,
                    STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
                    bestScore,
                    MENU_NUMBER_SIGNED
                );
            }

            break;
        case STAT_AVERAGE_SCORE:
            DrawDecimalMenuNumber(
                &gfx,
                STATS_VALUE_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
                g_CourseStatFigures[courseIndex].averageScore
            );
            break;
        case STAT_BEST_DRIVE:
            DrawDecimalMenuNumber(
                &gfx,
                STATS_VALUE_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
                g_CourseStatFigures[courseIndex].bestDrive
            );
            gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
            DrawYardMark(
                &gfx,
                STATS_UNIT_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT) + STATS_UNIT_OFFSET_Y
            );
            break;
        case STAT_AVERAGE_DRIVE:
            DrawDecimalMenuNumber(
                &gfx,
                STATS_VALUE_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
                g_CourseStatFigures[courseIndex].averageDrive
            );
            DrawYardMark(
                &gfx,
                STATS_UNIT_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT) + STATS_UNIT_OFFSET_Y
            );
            break;
        case STAT_FAIRWAY_PERCENT:
            DrawDecimalMenuNumber(
                &gfx,
                STATS_VALUE_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
                g_CourseStatFigures[courseIndex].fairwayPercent
            );
            DrawPercentMark(
                &gfx,
                STATS_UNIT_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT) + STATS_UNIT_OFFSET_Y
            );
            break;
        case STAT_AVERAGE_PUTTS:
            DrawDecimalMenuNumber(
                &gfx,
                STATS_VALUE_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
                g_CourseStatFigures[courseIndex].averagePutts
            );
            break;
        case STAT_PAR_SAVE_PERCENT:
            DrawDecimalMenuNumber(
                &gfx,
                STATS_VALUE_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
                g_CourseStatFigures[courseIndex].parSavePercent
            );
            DrawPercentMark(
                &gfx,
                STATS_UNIT_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT) + STATS_UNIT_OFFSET_Y
            );
            break;
        case STAT_PAR_ON_PERCENT:
            DrawDecimalMenuNumber(
                &gfx,
                STATS_VALUE_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
                g_CourseStatFigures[courseIndex].parOnPercent
            );
            DrawPercentMark(
                &gfx,
                STATS_UNIT_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT) + STATS_UNIT_OFFSET_Y
            );
            break;
        case STAT_SAND_SAVE_PERCENT:
            DrawDecimalMenuNumber(
                &gfx,
                STATS_VALUE_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT),
                g_CourseStatFigures[courseIndex].sandSavePercent
            );
            DrawPercentMark(
                &gfx,
                STATS_UNIT_X,
                STATS_FIRST_Y + (row * STATS_LINE_HEIGHT) + STATS_UNIT_OFFSET_Y
            );
            break;
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Writes the unit mark after a figure */
const char f_UnitMarkFormat[] = "%c";

/* Draws the green Y that marks a figure in yards */
void DrawYardMark(Gfx **displayListHead, s16 x, s16 y) {
    Gfx *gfx = *displayListHead;

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 63, 255, 63);
    sprintf(g_PrintBuffer, f_UnitMarkFormat, 'Y');
    DrawLargeText(&gfx, x, y, g_PrintBuffer);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Draws the pink percent sign after a percentage */
void DrawPercentMark(Gfx **displayListHead, s16 x, s16 y) {
    Gfx *gfx = *displayListHead;

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 64, 192);
    sprintf(g_PrintBuffer, f_UnitMarkFormat, '%');
    DrawLargeText(&gfx, x, y, g_PrintBuffer);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/*
 * Draws the spinning badge and how many of the course's holes, or of every
 * course's on the totals, have a birdie badge.
 */
void DrawStatsBadgeCount(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s16 badgeCount;
    /*
     * A hole, then a course, then where the count moves to, to match: a
     * separate holeIndex for the open course's holes, or an s16 countShift for
     * the shift, gives 60 words differing (2 in shape), a courseIndex 64 (2),
     * and an s32 countShift 72 (4)
     */
    s16 index;
    s16 holeIndex;
    /* The stats page after the open courses' pages shows every course */
    s32 totalsPage = GetOpenCourseCount();

    if (memory->statsCourse != totalsPage) {
        badgeCount = 0;
        for (index = 0; index != COURSE_HOLE_COUNT; index++) {
            if (save->records.awards.birdieBadgeCounts[memory->statsCourse][index]) {
                badgeCount++;
            }
        }
    }
    else {
        badgeCount = 0;
        for (index = 0; index != COURSE_COUNT; index++) {
            for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
                if (save->records.awards.birdieBadgeCounts[index][holeIndex]) {
                    badgeCount++;
                }
            }
        }
    }

    /* Centers the count by its number of digits */
    if (badgeCount < 10) {
        index = BADGE_COUNT_DIGIT_WIDTH;
    }

    if (badgeCount >= 10 && badgeCount < 100) {
        index = 0;
    }

    if (badgeCount >= 100) {
        index = -BADGE_COUNT_DIGIT_WIDTH;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    DrawSpinningBadge(&gfx, f_CourseStatsBadge, BADGE_ICON_X + index, BADGE_ICON_Y);
    DrawUnalignedMenuNumber(
        &gfx,
        BADGE_COUNT_X + index,
        BADGE_COUNT_Y,
        badgeCount,
        MENU_NUMBER_TIMES
    );
    *displayListHead = gfx;
}

/*
 * Draws the arrows beside the panel that show there are more statistics above
 * or below.
 * Doesn't match: 94 words differ, 20 in shape. The original picks the first
 * arrow with a branch around a copy of hasRowsBelow; GCC's jump pass makes
 * the copy branchless here, so the registers and the spills to the stack
 * differ. The first arrow is an s16, as the arrow index it is added to is;
 * as an s32 102 words differed, 22 in shape, and the register oracle got to
 * 83 words where it now gets to 77, forcing 14 pseudos either way. With it
 * an s16, the copy under "if (arrowCount == 1)", a ternary or an if/else
 * gives 101 words (23 in shape). The counts that follow were measured with
 * the draft at 102 words. Copying hasRowsBelow under "if (arrowCount == 1)"
 * gives 107 words (33 in shape), and copying hasRowsBelow != FALSE there
 * 109; the same with "arrowCount += hasRowsBelow" gives 99 words but 23 in
 * shape, as the original branches to count that arrow. Adding the arrows'
 * offsets as two products gives 103-113 words, 27-31 in shape. The
 * permuter's best outputs (66 and 70 words) load the arrow's texture only
 * for a lone down arrow, which changes what is drawn: the original's loop
 * sets the texture image on every pass. They are left out, as are a later
 * run's outputs of 65 and 66 words, which do the same. A ternary or an
 * if/else choosing hasRowsBelow or FALSE gives the 107 words (33 in shape)
 * of the copy, and setting TRUE under nested tests 109 (39). A u16 arrow
 * index gives 75 words (27 in shape), but zero-extends it with andi 0xffff
 * at both of the two places where the original sign-extends it with sll and
 * sra by 16, as the s16 does.
 *
 * Testing "(arrowCount == 1) & firstArrow" for the lone down arrow gives 183
 * words (63 in shape) and a frame 16 bytes larger.
 *
 * The original's four spill slots, 8 bytes apart, are at 0x4-0x1C, with the
 * stack unused above them, from 0x20 to 0x37; the draft's are the same four
 * slots above its 16 bytes of padding, and without the padding its frame is
 * 16 bytes smaller. Reload gives spilled pseudos their slots after the
 * locals, at higher offsets, so no local can stand for the original's unused
 * bytes: those would follow from reload giving slots to pseudos whose stack
 * references were later removed. *
 * The two flags are s16s, as the arrow count they start is, which takes the
 * draft from 94 words differing (20 in shape) to 90 (18), and the register
 * oracle's closest result from 77 words, forcing 15 pseudos, to 65, forcing
 * 14. Both as u8s or s8s give 93 (21); the flag for the rows below alone as a
 * byte gives 90 (18) too, and the flag for the rows above alone 95 (19). 90
 * words differ, 18 in shape.
 */
#ifdef NON_MATCHING
void DrawStatsScrollArrows(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s16 hasRowsAbove = f_FirstShownStat > 0;
    s16 arrowCount = hasRowsAbove;
    s16 hasRowsBelow = f_FirstShownStat + STATS_SHOWN_ROW_COUNT < STATS_COUNT;
    s16 firstArrow;
    s16 arrowIndex;
    s32 top;
    /* Pads the frame to match; without it 118 words differ, against 90 */
    s32 unused[4] UNUSED;

    if (hasRowsBelow) {
        arrowCount++;
    }

    if (arrowCount != 0) {
        firstArrow = arrowCount == 1 && hasRowsBelow;

        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_CourseStatsDigitsAndArrows)));
        for (arrowIndex = 0; arrowIndex != arrowCount; arrowIndex++) {
            top = (arrowIndex != 0) ? STATS_DOWN_ARROW_TOP : STATS_UP_ARROW_TOP;
            if (arrowCount == 1 && firstArrow) {
                top = STATS_DOWN_ARROW_TOP;
            }

            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(
                    (u32)f_CourseStatsDigitsAndArrows +
                    (PALETTED_TEXELS_OFFSET + (STATS_FIRST_ARROW_IMAGE * STATS_ARROW_IMAGE_SIZE)) +
                    (firstArrow + arrowIndex) * STATS_ARROW_IMAGE_SIZE
                ),
                G_IM_FMT_CI,
                STATS_ARROW_WIDTH,
                STATS_ARROW_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPTextureRectangle(
                gfx++,
                QuarterPixels(STATS_ARROW_LEFT),
                QuarterPixels(top),
                QuarterPixels(STATS_ARROW_LEFT + STATS_ARROW_WIDTH),
                QuarterPixels(top + STATS_ARROW_HEIGHT),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }
    }

    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", DrawStatsScrollArrows);
#endif

/*
 * Draws the course's spinning trophy in the color of each place won in its
 * tournament, stepping the spin's images, or on the totals the points needed to
 * open the next course.
 *
 * Doesn't match: the original loads the trophies' shift once as 0xFFEC, the
 * low half of -20, and adds it in a register, where this adds -20 to each
 * edge, and the load sync and tile commands' upper halves swap registers.
 * TrophyPosition's y is unsigned: with an s32 y, 31 words differ (21 in
 * shape), as GCC works out the clipped top edge a second time for the
 * rectangle's t coordinate, a clamp of a negative top edge that the original
 * lacks; the unsigned y makes that clamp dead, as -Wextra reports in the
 * modern build, and gives 19 words (5 in shape), and the register oracle's
 * closest result falls from 29 words to 11, forcing 3 pseudos for each. An
 * unsigned x as well gives 20 words, the oracle's closest 14, forcing 4.
 * Taking the offset into a local, of any type, gave 41 words or more, and an
 * unsigned x or place 20 or more, with the s32 y. The permuter's best cast y
 * to unsigned at the rectangle's top edge, the same 19 words, 5 in shape;
 * its fewest words, 12 (10 in shape), came from casting the rectangle's dtdy
 * to a char, which makes it 0.
 *
 * Re-measured with the unsigned y: the offset in a local, as an s32, s16,
 * u16 or u32, gives 179 words (11 in shape); place as an s8 gives 27 (5), as
 * a u8 or u16 50 (14), as an s32 174 (22) and as a u32 179 (23). Subtracting
 * 20U, or TROPHY_SHIFT_LEFT cast to u32, or casting the offset to u16, loads
 * 0xFFEC as the original does, but once before the trophies' loop, where the
 * original loads it on each pass: 28 words (6 in shape), or 36 (8) for the
 * casts. 19 words differ, 5 in shape.
 *
 * The trophies' spacing, place times TROPHY_SPACING, is a u16 local from
 * which each edge subtracts TROPHY_SHIFT_LEFT, as a permuter run pointed to:
 * set with the texels, the tile load or the rectangle, that takes the draft
 * from 19 words (5 in shape) to 4 (2), the shift then loaded as 0xFFEC in
 * the loop as the original does, and the register oracle's closest result
 * to these 4 with nothing forced. Set at the top of the place's branch, 39
 * words differ (17 in shape), and before its first sync 32 (10), which is
 * where it was set when it gave 48. As an s32 it gives 27 (5), an s16 31
 * (5) and a u32 36 (8); with place an s8, 12 (2), or a u16, 35 (11). The
 * edges' other orders of position, spacing and shift give 14 to 67 words.
 * What's left is the shift's load, four instructions before the original's,
 * and one register. Picking the palette with g_TrophyPalettes[place] in
 * any of the three ifs, rather than the place's constant, gives 12 to 33
 * words. 4 words differ, 2 in shape.
 */
#ifdef NON_MATCHING
void DrawCourseTrophies(Gfx **displayListHead) {
    EncodedAsset asset;
    Gfx *gfx = *displayListHead;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    void *palette = NULL;
    s32 openCourseCount;
    void *texels;
    s16 place;
    /*
     * How far right of the first place's trophy the place's is. Unsigned, as the
     * original adds the shift left as 0xFFEC, its low half
     */
    u16 spacing;

    openCourseCount = GetOpenCourseCount();
    if (memory->statsCourse == openCourseCount) {
        BeginShadowedText(&gfx);
        SetTextColor(&gfx, 255, 255, 255);
        openCourseCount = CountOpenCourses(save->coursePoints);
        if (openCourseCount < COURSE_COUNT) {
            SetTextSlotValue(GetCoursePointsNeeded(openCourseCount) - save->coursePoints, 0, 0);
            SetTextSlot(
                POINTS_NEEDED_X,
                POINTS_NEEDED_Y,
                MESSAGE_POINTS_TO_NEXT_COURSE,
                TEXT_SLOT_LARGE_FONT,
                0,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }
        else {
            SetTextSlot(
                ALL_COURSES_OPEN_X,
                ALL_COURSES_OPEN_Y,
                MESSAGE_ALL_COURSES_OPEN,
                TEXT_SLOT_LARGE_FONT,
                0,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }
    }
    else {
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        for (place = 0; place != TOURNAMENT_PLACE_COUNT; place++) {
            if (g_CourseStatFigures[memory->statsCourse].hasPlace[place]) {
                gDPPipeSync(gfx++);
                if (place == FIRST_PLACE_INDEX) {
                    palette = g_TrophyPalettes[FIRST_PLACE_INDEX];
                }

                if (place == SECOND_PLACE_INDEX) {
                    palette = g_TrophyPalettes[SECOND_PLACE_INDEX];
                }

                if (place == THIRD_PLACE_INDEX) {
                    palette = g_TrophyPalettes[THIRD_PLACE_INDEX];
                }

                spacing = place * TROPHY_SPACING;
                texels = f_StatsTrophyBuffers[f_ShownStatsTrophyBufferIndex];
                gDPLoadTLUT_pal256(gfx++, TextureAddress(palette));
                gDPLoadTextureTile(
                    gfx++,
                    TextureAddress(texels),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    g_TrophySizes[memory->statsCourse].width,
                    g_TrophySizes[memory->statsCourse].height,
                    0,
                    0,
                    g_TrophySizes[memory->statsCourse].width - 1,
                    g_TrophySizes[memory->statsCourse].height - 1,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gDPPipeSync(gfx++);
                /*
                 * The right edge adds the offset before the position, to match: the
                 * other orders give 14 to 67 words differing against 4
                 */
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(
                        f_StatsTrophyPositions[memory->statsCourse].x +
                        (spacing - TROPHY_SHIFT_LEFT)
                    ),
                    QuarterPixels(f_StatsTrophyPositions[memory->statsCourse].y),
                    QuarterPixels(
                        g_TrophySizes[memory->statsCourse].width + (spacing - TROPHY_SHIFT_LEFT) +
                        f_StatsTrophyPositions[memory->statsCourse].x
                    ),
                    QuarterPixels(
                        g_TrophySizes[memory->statsCourse].height +
                        f_StatsTrophyPositions[memory->statsCourse].y
                    ),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                gDPPipeSync(gfx++);
            }
        }

        f_StatsTrophyFrameTimer++;
        if (f_StatsTrophyFrameTimer >= TROPHY_FRAME_DELAY) {
            f_StatsTrophyFrameTimer = 0;
            f_StatsTrophyFrame++;
            if ((s16)g_TrophyFrameAssets[memory->statsCourse][f_StatsTrophyFrame] ==
                TROPHY_FRAME_END) {
                f_StatsTrophyFrame = 0;
            }

            if (f_StatsTrophyFrame >= TROPHY_FRAME_LIMIT) {
                f_StatsTrophyFrame = 0;
            }

            f_ShownStatsTrophyBufferIndex ^= 1;
            OpenEncodedAsset(g_TrophyFrameAssets[memory->statsCourse][f_StatsTrophyFrame], &asset);
            ReadEncodedAsset(f_StatsTrophyBuffers[f_ShownStatsTrophyBufferIndex], &asset);
        }
    }

    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", DrawCourseTrophies);
#endif

/* The smallest number of each length */
#define TWO_DIGIT_FIRST DECIMAL_BASE
#define THREE_DIGIT_FIRST (TWO_DIGIT_FIRST * DECIMAL_BASE)
#define FOUR_DIGIT_FIRST (THREE_DIGIT_FIRST * DECIMAL_BASE)
/* The largest number of each length */
#define ONE_DIGIT_MAX (TWO_DIGIT_FIRST - 1)
#define TWO_DIGIT_MAX (THREE_DIGIT_FIRST - 1)
#define THREE_DIGIT_MAX (FOUR_DIGIT_FIRST - 1)
#define FOUR_DIGIT_MAX (FOUR_DIGIT_FIRST * DECIMAL_BASE - 1)

/* DrawMenuNumber's sixth argument: the field it right-aligns the number in */
enum NumberField {
    NUMBER_FIELD_TWO_NARROW,
    NUMBER_FIELD_ONE,
    NUMBER_FIELD_TWO,
    NUMBER_FIELD_THREE,
    NUMBER_FIELD_FOUR,
    NUMBER_FIELD_NONE
};

/* Draws a number right-aligned in a two-digit field, with narrower spacing,
 * clamped to the digits it holds */
void DrawNarrowTwoDigitMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 mark) {
    Gfx *gfx = *displayListHead;

    if (value > TWO_DIGIT_MAX) {
        value = TWO_DIGIT_MAX;
    }

    if (value < -TWO_DIGIT_MAX) {
        value = -TWO_DIGIT_MAX;
    }

    DrawMenuNumber(&gfx, x, y, value, mark, NUMBER_FIELD_TWO_NARROW);
    *displayListHead = gfx;
}

/* Draws a number right-aligned in a one-digit field, clamped to the
 * digits it holds */
void DrawOneDigitMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 mark) {
    Gfx *gfx = *displayListHead;

    if (value > ONE_DIGIT_MAX) {
        value = ONE_DIGIT_MAX;
    }

    if (value < -ONE_DIGIT_MAX) {
        value = -ONE_DIGIT_MAX;
    }

    DrawMenuNumber(&gfx, x, y, value, mark, NUMBER_FIELD_ONE);
    *displayListHead = gfx;
}

/* Draws a number right-aligned in a two-digit field, clamped to the
 * digits it holds */
void DrawTwoDigitMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 mark) {
    Gfx *gfx = *displayListHead;

    if (value > TWO_DIGIT_MAX) {
        value = TWO_DIGIT_MAX;
    }

    if (value < -TWO_DIGIT_MAX) {
        value = -TWO_DIGIT_MAX;
    }

    DrawMenuNumber(&gfx, x, y, value, mark, NUMBER_FIELD_TWO);
    *displayListHead = gfx;
}

/* Draws a number right-aligned in a three-digit field, clamped to the
 * digits it holds */
void DrawThreeDigitMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 mark) {
    Gfx *gfx = *displayListHead;

    if (value > THREE_DIGIT_MAX) {
        value = THREE_DIGIT_MAX;
    }

    if (value < -THREE_DIGIT_MAX) {
        value = -THREE_DIGIT_MAX;
    }

    DrawMenuNumber(&gfx, x, y, value, mark, NUMBER_FIELD_THREE);
    *displayListHead = gfx;
}

/* Draws a number right-aligned in a four-digit field, clamped to the
 * digits it holds */
void DrawFourDigitMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 mark) {
    Gfx *gfx = *displayListHead;

    if (value > FOUR_DIGIT_MAX) {
        value = FOUR_DIGIT_MAX;
    }

    if (value < -FOUR_DIGIT_MAX) {
        value = -FOUR_DIGIT_MAX;
    }

    DrawMenuNumber(&gfx, x, y, value, mark, NUMBER_FIELD_FOUR);
    *displayListHead = gfx;
}

/* Draws a number without right-aligning it in a field */
void DrawUnalignedMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 mark) {
    Gfx *gfx = *displayListHead;

    DrawMenuNumber(&gfx, x, y, value, mark, NUMBER_FIELD_NONE);
    *displayListHead = gfx;
}

#define NARROW_DIGIT_SPACING 12
#define MENU_NUMBER_DIGIT_LIMIT 4
#define MENU_NUMBER_LIMIT FOUR_DIGIT_MAX

/*
 * Draws a number of up to four digits with the glyphs of ASSET_LARGE_DIGITS,
 * marked as the mark argument says and right-aligned in a field. The field's
 * shift depends only on the field, so a number two or three digits long in a
 * wider field moves as far right as a one-digit one. Numbers past four digits
 * draw nothing.
 */
void DrawMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 mark, u8 field) {
    Gfx *gfx = *displayListHead;
    s16 digits[MENU_NUMBER_DIGIT_LIMIT];
    s16 digitCount = 0;
    s32 shift;
    s16 spacing;
    s16 index; /* The mark's glyph, then the digit being drawn */

    if (value > MENU_NUMBER_LIMIT) {
        return;
    }

    if (value < -MENU_NUMBER_LIMIT) {
        return;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_MenuGlyphs)));
    shift = 0;
    if (mark != MENU_NUMBER_PLAIN) {
        switch (mark) {
            case MENU_NUMBER_SIGNED:
                if (value > 0) {
                    index = MENU_GLYPH_PLUS;
                    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0x7F, OPAQUE_ALPHA);
                }
                else if (value < 0) {
                    index = MENU_GLYPH_MINUS;
                    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                    gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0xFF, 0x7F, OPAQUE_ALPHA);
                }
                else {
                    index = MENU_GLYPH_PLUS_MINUS;
                }
                break;
            case MENU_NUMBER_RED_TINT:
                gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0x7F, OPAQUE_ALPHA);
                index = MENU_GLYPH_TIMES;
                break;
            case MENU_NUMBER_GREEN_TINT:
                gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0xFF, 0x7F, OPAQUE_ALPHA);
                index = MENU_GLYPH_TIMES;
                break;
            case MENU_NUMBER_TIMES:
            default:
                index = MENU_GLYPH_TIMES;
                break;
        }

        if (mark != MENU_NUMBER_RED_TINT && mark != MENU_NUMBER_GREEN_TINT) {
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(ImageTexels(f_MenuGlyphs) + (index * MENU_GLYPH_IMAGE_SIZE)),
                G_IM_FMT_CI,
                MENU_GLYPH_SIZE,
                MENU_GLYPH_SIZE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(x),
                QuarterPixels(y),
                QuarterPixels(x + MENU_GLYPH_SIZE),
                QuarterPixels(y + MENU_GLYPH_SIZE),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
            shift = MENU_GLYPH_SIZE;
        }
    }

    if (value >= 0 && value < TWO_DIGIT_FIRST) {
        digitCount = 1;
        if (field == NUMBER_FIELD_TWO) {
            shift += MENU_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_THREE) {
            shift += 2 * MENU_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_FOUR) {
            shift += 3 * MENU_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_TWO_NARROW) {
            shift += NARROW_DIGIT_SPACING;
        }

        digits[0] = value % DECIMAL_BASE;
    }

    if (value >= TWO_DIGIT_FIRST && value < THREE_DIGIT_FIRST) {
        digitCount = 2;
        if (field == NUMBER_FIELD_THREE) {
            shift += 2 * MENU_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_FOUR) {
            shift += 3 * MENU_GLYPH_SIZE;
        }

        digits[0] = value / DECIMAL_BASE;
        digits[1] = value % DECIMAL_BASE;
    }

    if (value >= THREE_DIGIT_FIRST && value < FOUR_DIGIT_FIRST) {
        digitCount = 3;
        if (field == NUMBER_FIELD_FOUR) {
            shift += 3 * MENU_GLYPH_SIZE;
        }

        digits[0] = value / THREE_DIGIT_FIRST;
        digits[1] = (value % THREE_DIGIT_FIRST) / DECIMAL_BASE;
        digits[2] = (value % THREE_DIGIT_FIRST) % DECIMAL_BASE;
    }

    if (value >= FOUR_DIGIT_FIRST && value <= MENU_NUMBER_LIMIT) {
        digitCount = 4;
        digits[0] = value / FOUR_DIGIT_FIRST;
        digits[1] = (value % FOUR_DIGIT_FIRST) / THREE_DIGIT_FIRST;
        digits[2] = ((value % FOUR_DIGIT_FIRST) % THREE_DIGIT_FIRST) / DECIMAL_BASE;
        digits[3] = ((value % FOUR_DIGIT_FIRST) % THREE_DIGIT_FIRST) % DECIMAL_BASE;
    }

    if (value < 0 && value > -TWO_DIGIT_FIRST) {
        digitCount = 1;
        if (field == NUMBER_FIELD_TWO) {
            shift += MENU_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_THREE) {
            shift += 2 * MENU_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_FOUR) {
            shift += 3 * MENU_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_TWO_NARROW) {
            shift += NARROW_DIGIT_SPACING;
        }

        digits[0] = -value % DECIMAL_BASE;
    }

    if (value <= -TWO_DIGIT_FIRST && value > -THREE_DIGIT_FIRST) {
        digitCount = 2;
        if (field == NUMBER_FIELD_THREE) {
            shift += 2 * MENU_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_FOUR) {
            shift += 3 * MENU_GLYPH_SIZE;
        }

        digits[0] = -value / DECIMAL_BASE;
        digits[1] = -value % DECIMAL_BASE;
    }

    if (value <= -THREE_DIGIT_FIRST && value > -FOUR_DIGIT_FIRST) {
        digitCount = 3;
        if (field == NUMBER_FIELD_FOUR) {
            shift += 3 * MENU_GLYPH_SIZE;
        }

        digits[0] = -value / THREE_DIGIT_FIRST;
        digits[1] = (-value % THREE_DIGIT_FIRST) / DECIMAL_BASE;
        digits[2] = (-value % THREE_DIGIT_FIRST) % DECIMAL_BASE;
    }

    if (value <= -FOUR_DIGIT_FIRST && value >= -MENU_NUMBER_LIMIT) {
        digitCount = 4;
        digits[0] = -value / FOUR_DIGIT_FIRST;
        digits[1] = (-value % FOUR_DIGIT_FIRST) / THREE_DIGIT_FIRST;
        digits[2] = ((-value % FOUR_DIGIT_FIRST) % THREE_DIGIT_FIRST) / DECIMAL_BASE;
        digits[3] = ((-value % FOUR_DIGIT_FIRST) % THREE_DIGIT_FIRST) % DECIMAL_BASE;
    }

    spacing = MENU_GLYPH_SIZE;
    if (field == NUMBER_FIELD_TWO_NARROW) {
        spacing = NARROW_DIGIT_SPACING;
    }

    for (index = 0; index != digitCount; index++) {
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(
                ImageTexels(f_MenuGlyphs) +
                ((digits[index] + MENU_GLYPH_FIRST_DIGIT) * MENU_GLYPH_IMAGE_SIZE)
            ),
            G_IM_FMT_CI,
            MENU_GLYPH_SIZE,
            MENU_GLYPH_SIZE,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(x + shift + (index * spacing)),
            QuarterPixels(y),
            QuarterPixels(x + shift + (index * spacing) + MENU_GLYPH_SIZE),
            QuarterPixels(y + MENU_GLYPH_SIZE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* DrawDecimalMenuNumber writes a figure to one decimal place, its whole part
 * lined up on the right */
#define DECIMAL_WHOLE_LIMIT 999
#define DECIMAL_DIGIT_LIMIT 6
#define HUNDREDTHS_PER_UNIT 100.0f
/* The point is drawn half a glyph back, over the right half of the last whole
 * digit */
#define DECIMAL_POINT_OFFSET (MENU_GLYPH_SIZE / 2)

/* Writes a decimal figure's whole part */
const char f_DecimalWholeFormat[] = "%d";

/*
 * Draws value to one decimal place in the large digits, its whole part
 * right-aligned in three digits from x
 */
void DrawDecimalMenuNumber(Gfx **displayListHead, s32 x, s32 y, f32 value) {
    Gfx *gfx = *displayListHead;
    s32 whole = value;
    s32 shift;
    s32 fraction;
    s32 digitIndex;
    u8 digit;
    /* Pads the frame to match; without it 112 words differ */
    s32 unused[4] UNUSED;

    if (whole > DECIMAL_WHOLE_LIMIT) {
        whole = DECIMAL_WHOLE_LIMIT;
    }

    if (whole < 0) {
        whole = 0;
    }

    fraction = (value - whole) * HUNDREDTHS_PER_UNIT;

    /* Round the hundredths to tenths */
    if (fraction % DECIMAL_BASE >= DECIMAL_BASE / 2) {
        fraction += DECIMAL_BASE;
    }

    fraction /= DECIMAL_BASE;
    if (fraction >= DECIMAL_BASE) {
        whole++;
        if (whole > DECIMAL_WHOLE_LIMIT) {
            whole = DECIMAL_WHOLE_LIMIT;
            fraction = DECIMAL_BASE - 1;
        }
        else {
            fraction = 0;
        }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_MenuGlyphs)));
    shift = 0;
    if (whole >= 0 && whole < DECIMAL_BASE) {
        shift = 2 * MENU_GLYPH_SIZE;
    }

    if (whole >= DECIMAL_BASE && whole < DECIMAL_BASE * DECIMAL_BASE) {
        shift += MENU_GLYPH_SIZE;
    }

    sprintf(g_PrintBuffer, f_DecimalWholeFormat, whole);
    for (digitIndex = 0; digitIndex != DECIMAL_DIGIT_LIMIT; digitIndex++) {
        digit = g_PrintBuffer[digitIndex];
        if (digit == '\0') {
            break;
        }

        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(
                ImageTexels(f_MenuGlyphs) +
                ((digit - '0' + MENU_GLYPH_FIRST_DIGIT) * MENU_GLYPH_IMAGE_SIZE)
            ),
            G_IM_FMT_CI,
            MENU_GLYPH_SIZE,
            MENU_GLYPH_SIZE,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(x + shift + (digitIndex * MENU_GLYPH_SIZE)),
            QuarterPixels(y),
            QuarterPixels(x + shift + (digitIndex * MENU_GLYPH_SIZE) + MENU_GLYPH_SIZE),
            QuarterPixels(y + MENU_GLYPH_SIZE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(f_MenuGlyphs) + (MENU_GLYPH_POINT * MENU_GLYPH_IMAGE_SIZE)),
        G_IM_FMT_CI,
        MENU_GLYPH_SIZE,
        MENU_GLYPH_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(x + shift + ((digitIndex - 1) * MENU_GLYPH_SIZE) + DECIMAL_POINT_OFFSET),
        QuarterPixels(y),
        QuarterPixels(
            x + shift + ((digitIndex - 1) * MENU_GLYPH_SIZE) + DECIMAL_POINT_OFFSET +
            MENU_GLYPH_SIZE
        ),
        QuarterPixels(y + MENU_GLYPH_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(
            ImageTexels(f_MenuGlyphs) +
            ((fraction + MENU_GLYPH_FIRST_DIGIT) * MENU_GLYPH_IMAGE_SIZE)
        ),
        G_IM_FMT_CI,
        MENU_GLYPH_SIZE,
        MENU_GLYPH_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(x + shift + (digitIndex * MENU_GLYPH_SIZE)),
        QuarterPixels(y),
        QuarterPixels(x + shift + (digitIndex * MENU_GLYPH_SIZE) + MENU_GLYPH_SIZE),
        QuarterPixels(y + MENU_GLYPH_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Nothing reads these */
const char f_UnreadFirstEmptyText[] = "";
const char f_UnreadSecondEmptyText[] = "";

/* Draws the MENU_SIGN_* sign's glyph with its top-left corner at (x, y) */
void DrawMenuSign(Gfx **displayListHead, s32 x, s32 y, u8 sign) {
    Gfx *gfx = *displayListHead;
    s32 glyphIndex = MENU_GLYPH_TIMES;

    switch (sign) {
        case MENU_SIGN_DASH:
            glyphIndex = MENU_GLYPH_MINUS;
            break;
        case MENU_SIGN_TIMES:
            break;
        case MENU_SIGN_CIRCLE:
            glyphIndex = MENU_GLYPH_CIRCLE;
            break;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_MenuGlyphs)));
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(f_MenuGlyphs) + (glyphIndex * MENU_GLYPH_IMAGE_SIZE)),
        G_IM_FMT_CI,
        MENU_GLYPH_SIZE,
        MENU_GLYPH_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(x),
        QuarterPixels(y),
        QuarterPixels(x + MENU_GLYPH_SIZE),
        QuarterPixels(y + MENU_GLYPH_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Draws "OFF" in the large digits' two glyphs for it; nothing calls this */
void DrawOffLabel(Gfx **displayListHead, s32 x, s32 y) {
    Gfx *gfx = *displayListHead;
    s32 glyphIndex;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_MenuGlyphs)));
    for (glyphIndex = 0; glyphIndex != OFF_GLYPH_COUNT; glyphIndex++) {
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(
                ImageTexels(f_MenuGlyphs) + ((OFF_FIRST_GLYPH + glyphIndex) * MENU_GLYPH_IMAGE_SIZE)
            ),
            G_IM_FMT_CI,
            MENU_GLYPH_SIZE,
            MENU_GLYPH_SIZE,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(x + (glyphIndex * MENU_GLYPH_SIZE)),
            QuarterPixels(y),
            QuarterPixels(x + (glyphIndex * MENU_GLYPH_SIZE) + MENU_GLYPH_SIZE),
            QuarterPixels(y + MENU_GLYPH_SIZE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

/*
 * Draws the green left and right arrows either side of a width centered on a
 * point, switching their blink frame every SIDE_ARROW_BLINK_FRAME_COUNT calls
 *
 * frameOffset is worked out before the palette load; after it, 264 words
 * differ. The arrow's image offset is added to frameOffset before the
 * image's address; added to the address in turn, 4 words differ, and with
 * frameOffset first, 2. Setting top at its declaration, not just before the
 * rectangle, gives 134 words, 38 in shape.
 */
void DrawBlinkingSideArrows(
    Gfx **displayListHead,
    void *image,
    s32 *blinkFrame,
    s32 *blinkTimer,
    s32 width,
    s32 centerX,
    s32 centerY
) {
    Gfx *gfx = *displayListHead;
    s32 left = 0;
    s32 arrowIndex;
    s32 frameOffset;
    s32 top;
    s32 imageOffset;

    (*blinkTimer)++;
    if (*blinkTimer == SIDE_ARROW_BLINK_FRAME_COUNT) {
        *blinkFrame = !*blinkFrame;
        *blinkTimer = 0;
    }

    frameOffset = (*blinkFrame * GREEN_ARROW_FRAME_SIZE) + PALETTED_TEXELS_OFFSET;
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(image)));
    for (arrowIndex = 0; arrowIndex != SIDE_ARROW_COUNT; arrowIndex++) {
        imageOffset = (SIDE_ARROW_FIRST_IMAGE + arrowIndex) * GREEN_ARROW_IMAGE_SIZE;
        gDPLoadTextureTile_4b(
            gfx++,
            TextureAddress(AddressPast(image, imageOffset + frameOffset)),
            G_IM_FMT_CI,
            GREEN_ARROW_SIZE,
            GREEN_ARROW_SIZE,
            0,
            0,
            GREEN_ARROW_SIZE - 1,
            GREEN_ARROW_SIZE - 1,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        if (arrowIndex == SIDE_ARROW_RIGHT) {
            left = centerX + (width / 2);
        }

        if (arrowIndex == SIDE_ARROW_LEFT) {
            left = centerX - (width / 2) - GREEN_ARROW_SIZE;
        }

        top = centerY - (GREEN_ARROW_SIZE / 2);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(left),
            QuarterPixels(top),
            QuarterPixels(left + GREEN_ARROW_SIZE),
            QuarterPixels(centerY + (GREEN_ARROW_SIZE / 2)),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

/*
 * Draws one of the four side arrow images, in either blink state, centered on
 * a point. The header size is held in a local, as the matching code needs:
 * with the constant, GCC folds the texels' offset differently.
 */
void DrawGreenArrow(
    Gfx **displayListHead,
    void *image,
    s32 blinkFrame,
    s32 direction,
    s32 centerX,
    s32 centerY
) {
    Gfx *gfx = *displayListHead;
    s32 top;
    s32 left;
    s32 headerSize = IMAGE_HEADER_SIZE;

    if (blinkFrame < 0) {
        blinkFrame = 0;
    }

    if (blinkFrame >= GREEN_ARROW_FRAME_COUNT) {
        blinkFrame = GREEN_ARROW_FRAME_COUNT - 1;
    }

    if (direction < 0) {
        direction = 0;
    }

    if (direction >= MENU_ARROW_COUNT) {
        direction = MENU_ARROW_COUNT - 1;
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(AddressPast(image, headerSize)));
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(
            (u32)image +
            (direction * GREEN_ARROW_IMAGE_SIZE +
                (blinkFrame * GREEN_ARROW_FRAME_SIZE +
                    (headerSize + SMALL_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE)))
        ),
        G_IM_FMT_CI,
        GREEN_ARROW_SIZE,
        GREEN_ARROW_SIZE,
        0,
        0,
        GREEN_ARROW_SIZE - 1,
        GREEN_ARROW_SIZE - 1,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    left = centerX - (GREEN_ARROW_SIZE / 2);
    top = centerY - (GREEN_ARROW_SIZE / 2);
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(left),
        QuarterPixels(top),
        QuarterPixels(centerX + (GREEN_ARROW_SIZE / 2)),
        QuarterPixels(centerY + (GREEN_ARROW_SIZE / 2)),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
