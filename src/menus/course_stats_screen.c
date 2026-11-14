/*
 * The clubhouse's course statistics: a wheel of the open courses that turns
 * from one to the next, and the chosen course's panel of statistics.
 */

#include "menus/course_stats_screen.h"

#include <ultra64.h>

#include "audio/sound.h"
#include "camera/view_projection.h"
#include "course/course.h"
#include "game/game.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "menus/main_menu.h"
#include "menus/menu.h"
#include "menus/menu_arrows.h"
#include "menus/menu_memory.h"
#include "menus/menu_number.h"
#include "save/course_stats.h"
#include "save/save.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "screen/sky_and_hud_animations.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/memory_address.h"
#include "text/text.h"

// A gold coin with a star, reading HOLE IN ONE
#define ASSET_HOLE_IN_ONE_BADGE 0x8EA
// Orange 1 to 6, then up and down arrows; the course stats draw only the
// arrows
#define ASSET_DIGITS_AND_ARROWS 0x5A5

// The images of g_CourseIconAssets that the course menu reads
#define COURSE_MENU_IMAGE_COUNT 7

// DrawCourseBanner squeezes the banner's strips under the course menu's title,
// with the course's name on it
#define COURSE_BANNER_TOP 110
#define COURSE_BANNER_STRIP_SPACING 8
#define COURSE_NAME_Y 116

// How far UpdateCourseStats turns the course wheel each move, and how fast
#define STATS_TURN 90
#define STATS_SPIN_SPEED 18

// The course's statistics: the panel behind them, and the rows of it that show
#define STATS_COUNT 10
#define STATS_SHOWN_ROW_COUNT 4
#define STATS_PANEL_LEFT 24
#define STATS_PANEL_TOP 138
#define STATS_PANEL_RIGHT 302
#define STATS_PANEL_BOTTOM 230
#define STATS_LABEL_X 30
#define STATS_FIRST_Y 154
#define STATS_LINE_HEIGHT 16
// The statistics in the order the panel lists them
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
// Where DrawStatFigure draws the figures; shorter best scores move right to
// line up
#define STATS_ROUND_COUNT_X 96
#define STATS_VALUE_X 112
#define STATS_TWO_DIGIT_X 128
#define STATS_ONE_DIGIT_X 144
#define STATS_UNIT_X 178
#define STATS_UNIT_OFFSET_Y 4
// Two of the glyphs of ASSET_LARGE_DIGITS in f_MenuGlyphs spell "OFF"
#define DIGIT_GLYPH_OFF_COUNT 2

// The largest number written in one decimal digit, and in two
#define ONE_DIGIT_NUMBER_MAX (DECIMAL_BASE - 1)
#define TWO_DIGIT_NUMBER_MAX ((DECIMAL_BASE * DECIMAL_BASE) - 1)
// Whether a signed number's magnitude takes one digit, and two
#define IsOneDigitNumber(n) ((n) >= -ONE_DIGIT_NUMBER_MAX && (n) <= ONE_DIGIT_NUMBER_MAX)
#define IsTwoDigitNumber(n) \
    (((n) >= DECIMAL_BASE && (n) <= TWO_DIGIT_NUMBER_MAX) || \
        ((n) >= -TWO_DIGIT_NUMBER_MAX && (n) <= -DECIMAL_BASE))

// The arrows beside the statistics that show there are more rows above or below
#define STATS_FIRST_ARROW_IMAGE 16
#define STATS_ARROW_WIDTH 16
#define STATS_ARROW_HEIGHT 9
#define STATS_ARROW_IMAGE_SIZE (STATS_ARROW_WIDTH * STATS_ARROW_HEIGHT / 2)
#define STATS_ARROW_LEFT 32
#define STATS_UP_ARROW_TOP 144
#define STATS_DOWN_ARROW_TOP 216

// The blinking arrows that DrawBlinkingSideArrows draws either side of a box,
// from the third image on
#define SIDE_ARROW_COUNT 2
#define SIDE_ARROW_RIGHT 0
#define SIDE_ARROW_LEFT 1
#define SIDE_ARROW_FIRST_IMAGE MENU_ARROW_RIGHT
#define SIDE_ARROW_BLINK_FRAME_COUNT 4

// The birdie badge icon of f_CourseStatsBadge and the count beside it
#define BADGE_ICON_X 222
#define BADGE_ICON_Y 144
#define BADGE_COUNT_X 250
#define BADGE_COUNT_Y 146

// DrawCourseTrophies draws the course's tournament trophies, spinning, or on
// the totals page the course points
#define TROPHY_SPACING 24
#define TROPHY_SHIFT_LEFT 20
// How far right of its course's position the place's trophy is. The spacing is
// unsigned and 16 bits wide, as the original's sum is
#define TrophyOffset(place) ((u16)((place) * TROPHY_SPACING) - TROPHY_SHIFT_LEFT)
#define TROPHY_FRAME_DELAY 4
#define POINTS_NEEDED_X 192
#define POINTS_NEEDED_Y 174
#define ALL_COURSES_OPEN_X 190
#define ALL_COURSES_OPEN_Y 180
#define MESSAGE_POINTS_TO_NEXT_COURSE 0x163
#define MESSAGE_ALL_COURSES_OPEN_TWO_LINES 0x164

typedef struct TrophyPosition {
    s32 x;
    // Unsigned: the original drops the rectangle's clamp of a negative top
    // edge where it works out the texture's t coordinate
    u32 y;
} TrophyPosition;

// DrawMenuNumber's sixth argument: the field it right-aligns the number in
enum NumberField {
    NUMBER_FIELD_TWO_NARROW,
    NUMBER_FIELD_ONE,
    NUMBER_FIELD_TWO,
    NUMBER_FIELD_THREE,
    NUMBER_FIELD_FOUR,
    NUMBER_FIELD_NONE
};

typedef u8 NumberField;

void DrawStatFigure(Gfx **displayListHead, s16 statIndex, u8 row);
void DrawThreeDigitMenuNumber(
    Gfx **displayListHead,
    s32 x,
    s32 y,
    s32 number,
    MenuNumberStyle mark
);
void DrawFourDigitMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 number, MenuNumberStyle mark);
void DrawDecimalMenuNumber(Gfx **displayListHead, s32 x, s32 y, f32 number);
void DrawMenuNumber(
    Gfx **displayListHead,
    s32 x,
    s32 y,
    s32 number,
    MenuNumberStyle mark,
    NumberField field
);

// The STAT_ row shown at the top of the panel, which Up and Down scroll
static s16 f_FirstShownStat = 0;
// The trophy's frame of g_TrophyFrameAssets, stepped when its timer reaches
// TROPHY_FRAME_DELAY
static s16 f_StatsTrophyFrame = 0;
// Which of f_StatsTrophyBuffers holds the trophy frame shown
static s16 f_ShownStatsTrophyBufferIndex = 0;
// Counts the trophy's draws since its frame last stepped
static s16 f_StatsTrophyFrameTime = 0;
// The blink frame and timer of the arrows beside the course wheel
static s32 f_StatsArrowFrame = 0;
static s32 f_StatsArrowTimer = 0;
// Read by no routine
static s32 f_UnreadStatsWord UNUSED = 0;
// The panel's label for each STAT_ row
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
// DrawCourseStats adds a tenth to it on every draw, but no routine reads it
static f32 f_UnreadStatsDrawTime = 0.0f;
// Read by no routine
static char f_UnreadStatsTexts[3][10] UNUSED = {"EXP", "ｺｰｽｹﾞｯﾄ", "純ﾞ"};
// Where each course's first trophy sits, before its shift for the place
static TrophyPosition f_StatsTrophyPositions[COURSE_COUNT] =
    {{230, 177}, {230, 177}, {230, 177}, {236, 165}, {226, 184}, {236, 175}};

// The camera's matrices that the course wheel's points are projected with
static Mat4 f_WheelProjection;
static Mat4 f_WheelView;
// The spinning badge, read after the hole-in-one badge over the same pointer
static ImageAsset *f_CourseStatsBadge;
// The course stats' TROPHY_FRAME_BUFFER_COUNT buffers for the trophy frame
static ImageAsset **f_StatsTrophyBuffers;
// LoadCourseTrophyImages keeps these for COURSE_BUFFER_SECOND and
// COURSE_BUFFER_LAST, but no routine reads them
static ImageAsset **f_UnreadSecondTrophyBuffers;
static ImageAsset **f_UnreadLastTrophyBuffers;
// The wheel's icons of each course and the totals, from LoadCourseStatsIcons
static ImageAsset **f_CourseStatsIcons;
// The images that LoadCourseMenuBuffer reads, by their assets
static ImageAsset *f_CourseStatsNamePlate;
static ImageAsset *f_CourseStatsDigitsAndArrows;
static ImageAsset *f_LargeDigitsImage;
static ImageAsset *f_CourseStatsGreenArrows;

// Read by no routine; 2 objects, as one would be aligned past its start
static u8 f_UnreadStatsBytes[0x4] UNUSED;
static u8 f_UnreadStatsBlock[0x188] UNUSED;

// Reads every course's icon into the course stats' buffers
void LoadCourseStatsIcons(ImageAsset **buffers) {
    EncodedAsset asset;
    // Pads the frame to match; without it 24 words differ
    s32 unused[4] UNUSED;
    s16 imageIndex;

    f_CourseStatsIcons = buffers;
    for (imageIndex = 0; imageIndex != COURSE_MENU_IMAGE_COUNT; imageIndex++) {
        OpenEncodedAsset(g_CourseIconAssets[imageIndex], &asset);
        ReadEncodedAsset(f_CourseStatsIcons[imageIndex], &asset);
    }
}

// Keeps the buffers as the kind's set, then reads the chosen course's first
// trophy image into both of them and the trophy's three place palettes
void LoadCourseTrophyImages(ImageAsset **buffers, CourseBuffer requestedKind) {
    EncodedAsset asset;
    AssetFile file;
    CourseBuffer8 bufferKind = requestedKind;
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

void LoadCourseMenuBuffer(void *buffer, CourseMenuBuffer bufferKind) {
    EncodedAsset asset;

    switch (bufferKind) {
        // The course menu keeps no buffer of kind 1
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
            f_LargeDigitsImage = buffer;
            OpenEncodedAsset(ASSET_LARGE_DIGITS, &asset);
            ReadEncodedAsset(f_LargeDigitsImage, &asset);
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
                    f_StatsTrophyFrameTime = 0;
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
                    f_StatsTrophyFrameTime = 0;
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

            // Stop on reaching the course rather than turning past it
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

// DrawCourseStats turns a totals image on the wheel of menu.h after the open
// courses' images
#define COURSE_TOTALS_IMAGE COURSE_COUNT

// Draws the course stats menu: the open courses' images and the totals image
// on a turning wheel, nearest last, then the arrows, the chosen course's
// banner, its stats panel, badge count and trophies.
void DrawCourseStats(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    u16 perspectiveNormalization;
    f32 iconX[WHEEL_SLOT_LIMIT];
    f32 iconY[WHEEL_SLOT_LIMIT];
    f32 iconScale[WHEEL_SLOT_LIMIT];
    // Pads the frame to match; without it 180 words differ
    s32 unusedSlots[WHEEL_SLOT_LIMIT] UNUSED;
    s32 drawOrder[WHEEL_SLOT_LIMIT];
    // Pads the frame to match; without it 170 words differ
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
    s32 drawnIndex;
    s32 halfSize;
    s32 texelStep;

    DrawMenuBackdropLayers(&gfx, PLAIN_BACKDROP_LAYERS);
    guPerspectiveF(
        f_WheelProjection,
        &perspectiveNormalization,
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

    // Draw the farthest images first, so the nearer ones cover them
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
        drawnIndex = drawOrder[index];
        image = (drawnIndex == courseCount) ? COURSE_TOTALS_IMAGE : drawnIndex;
        halfSize = iconScale[drawnIndex] * COURSE_ICON_HALF_SIZE;
        texelStep = (f32)(COURSE_ICON_STRIP_HEIGHT << TEXEL_STEP_SHIFT) / halfSize;
        gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_CourseStatsIcons[image])));
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
                QuarterPixels((s32)(iconX[drawnIndex] - halfSize)),
                QuarterPixels(
                    (s32)(iconY[drawnIndex] - halfSize + (sortIndex * halfSize)) - WHEEL_ICON_RAISE
                ),
                QuarterPixels((s32)(iconX[drawnIndex] + halfSize)),
                QuarterPixels((s32)(iconY[drawnIndex] + (sortIndex * halfSize)) - WHEEL_ICON_RAISE),
                G_TX_RENDERTILE,
                0,
                0,
                texelStep,
                texelStep
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

// Draws the name plate banner squeezed under the screen's title, with the
// course's number and its tournament's name, or with the course's own name if
// useCourseNames is set. The totals, after the last course, get no number.
void DrawCourseBanner(Gfx **displayListHead, u8 useCourseNames, CourseIndex courseIndex) {
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
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_CourseStatsNamePlate)));
    for (stripIndex = 0; stripIndex != NAME_PLATE_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_CourseStatsNamePlate) +
                (stripIndex * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NAME_PLATE_WIDTH,
            NAME_PLATE_STRIP_HEIGHT,
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
            QuarterPixels(MENU_BANNER_LEFT + NAME_PLATE_WIDTH),
            QuarterPixels(stripTop + COURSE_BANNER_STRIP_SPACING),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TexelRunStep(NAME_PLATE_STRIP_HEIGHT, COURSE_BANNER_STRIP_SPACING)
        );
        gDPPipeSync(gfx++);
    }

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    if (!useCourseNames) {
        if (courseIndex + 1 == COURSE_COUNT + 1) {
            sprintf(g_PrintBuffer, "%s", g_TournamentNames[courseIndex]);
        }
        else {
            sprintf(g_PrintBuffer, "%d:%s", courseIndex + 1, g_TournamentNames[courseIndex]);
        }
    }
    else if (courseIndex + 1 == COURSE_COUNT + 1) {
        sprintf(g_PrintBuffer, "%s", g_CourseRecords[courseIndex].name);
    }
    else {
        sprintf(g_PrintBuffer, "%d:%s", courseIndex + 1, g_CourseRecords[courseIndex].name);
    }

    DrawCenteredLargeText(&gfx, SCREEN_WIDTH / 2, COURSE_NAME_Y, g_PrintBuffer);
    *displayListHead = gfx;
}

// Draws the dimmed panel of the course's statistics, with the label and figure
// of each shown row
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
        sprintf(g_PrintBuffer, "%s", f_StatLabels[statIndex]);
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

// Draws the statistic's figure for the chosen course, or for all courses on the
// totals, on the panel's row, with its unit; nothing shows before any round has
// been played
void DrawStatFigure(Gfx **displayListHead, s16 statIndex, u8 row) {
    Gfx *gfx = *displayListHead;
    MenuMemory *memory;
    CourseIndex courseIndex;
    s32 bestScore;
    s32 openCourseCount;

    GetSaveData();
    memory = GetMenuMemory();
    openCourseCount = GetOpenCourseCount();
    if (memory->statsCourse != openCourseCount) {
        courseIndex = memory->statsCourse;
    }
    else {
        // The figures for all the courses together
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
                g_CourseStatFigures[courseIndex].scoreAverage
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
                g_CourseStatFigures[courseIndex].driveAverage
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
                g_CourseStatFigures[courseIndex].puttAverage
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

// Draws the green Y that marks a figure in yards
void DrawYardMark(Gfx **displayListHead, s16 x, s16 y) {
    Gfx *gfx = *displayListHead;

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 63, 255, 63);
    sprintf(g_PrintBuffer, "%c", 'Y');
    DrawLargeText(&gfx, x, y, g_PrintBuffer);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// Draws the pink percent sign after a percentage
void DrawPercentMark(Gfx **displayListHead, s16 x, s16 y) {
    Gfx *gfx = *displayListHead;

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 64, 192);
    sprintf(g_PrintBuffer, "%c", '%');
    DrawLargeText(&gfx, x, y, g_PrintBuffer);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// Draws the spinning badge and how many of the course's holes, or of every
// course's on the totals, have a birdie badge.
void DrawStatsBadgeCount(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s16 badgeCount;
    // A hole, then a course, then where the count moves to, to match: a
    // separate holeIndex for the open course's holes, or an s16 countShift for
    // the shift, gives 60 words differing (2 in shape), a courseIndex 64 (2),
    // and an s32 countShift 72 (4)
    s16 index;
    s16 holeIndex;
    // The stats page after the open courses' pages shows every course
    s32 totalsPage = GetOpenCourseCount();

    if (memory->statsCourse != totalsPage) {
        badgeCount = 0;
        for (index = 0; index != COURSE_HOLE_COUNT; index++) {
            if (save->records.awards.birdieBadgeCounts[memory->statsCourse][index] != 0) {
                badgeCount++;
            }
        }
    }
    else {
        badgeCount = 0;
        for (index = 0; index != COURSE_COUNT; index++) {
            for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
                if (save->records.awards.birdieBadgeCounts[index][holeIndex] != 0) {
                    badgeCount++;
                }
            }
        }
    }

    // Centers the count by its number of digits
    if (badgeCount < 10) {
        index = DIGIT_GLYPH_SIZE;
    }

    if (badgeCount >= 10 && badgeCount < 100) {
        index = 0;
    }

    if (badgeCount >= 100) {
        index = -DIGIT_GLYPH_SIZE;
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

// Draws the arrows beside the panel that show there are more statistics above
// or below.
//
// The rectangle is drawn with gSPScisTextureRectangle, which clamps each edge
// at zero and, for a left or top edge off the screen, moves the texel
// coordinate on by the edge times its step. All that is left of that in the
// code is a shift of the top by 2 whose result nothing reads, left from the
// test of the top's sign, and 16 bytes of the frame. combine, knowing the top
// is 144 or 216, drops the bottom edge's clamp and the top's texel offset, and
// where it can't place the note for the death of each one's register it leaves
// a use of it. With nothing else reading them they get no hard register, and
// reload gives each a stack slot of 8 bytes that nothing reads. The frame's
// other five slots are displayListHead's, the arrow count's, the first arrow's
// and the lone down arrow's test's, which the code reads at 0x4 to 0x1C, and
// one for the texture table mode's word, which reload takes out of $s2 and
// never reads back. With gSPTextureRectangle the frame is 16 bytes smaller,
// which the old draft made up with an unused array.
//
// The top is an s16, as the count, the first arrow and the index are. As an
// s32, the top's two shifts for the rectangle's words trade places and
// registers.
//
// The rows below are tested where the down arrow is counted and again for the
// first arrow, and cse joins the two: the original keeps the test's result in
// $a1 and copies it under a branch on the count. Kept in a flag, the copy is
// made without a branch. The lone down arrow's test in the loop tests the count
// again, though firstArrow is only set for a count of one: loop.c moves the
// test out as (arrowCount == 1) & firstArrow, the original's xori, sltiu and
// and, kept at 0x1C($sp).
//
// Measured against the match, as words differing and how many of them in shape:
//   The old draft: gSPTextureRectangle, an s32 top and 16
//   unused bytes                                              71 (11)
//   gSPTextureRectangle, with no unused bytes                 96 (14)
//   gSPTextureRectangle, with the unused bytes                72 (14)
//   The unused bytes as well as gSPScisTextureRectangle       56 (0)
//   top as an s32                                             8 (4)
//   top as a u8                                               24 (4)
//   top as a u32                                              55 (11)
//   top as an s8                                              116 (30)
//   The bottom edge as the top's quarter pixels plus the
//   height's                                                  51 (3)
//   The rows below tested once, in an s16 flag                113 (45)
//   The same in an s32 flag                                   117 (47)
//   The rows above in an s16 flag as well                     113 (45)
//   The down arrow counted by adding its test                 18 (8)
//   The first arrow as a conditional expression on the count  113 (45)
//   The first arrow cleared, then set under the count's test  20 (8)
//   The first arrow with the rows' test ahead of the count's  113 (57)
//   firstArrow as an s32                                      28 (10)
//   firstArrow as a u16                                       3 (3)
//   arrowCount as an s32                                      65 (29)
//   arrowCount as a u16                                       2 (2)
//   arrowIndex as an s32                                      23 (7)
//   arrowIndex as a u16                                       9 (9)
//   The lone down arrow tested by firstArrow alone            146 (16)
//   The same tested with firstArrow ahead of the count        10 (0)
//   The top in one conditional expression, a down arrow's for
//   a later arrow or a lone down arrow                        17 (15)
//   The lone down arrow tested first, and the index in its
//   else                                                      17 (13)
//   The top chosen by the arrow's image, firstArrow +
//   arrowIndex                                                191 (37)
//   The arrow's image as arrowIndex + firstArrow              19 (1)
//   The arrow's image offset as a product for each            41 (15)
//   The loop's test as <                                      283 (37)
//
// Also matching: top as a u16; the top chosen by an if and an else, or set to
// the up arrow's and then to the down arrow's under the index's test; the index
// tested as zero with the tops swapped; arrowCount cleared and counted up under
// a test for each arrow; and top declared before arrowIndex.
void DrawStatsScrollArrows(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s16 arrowCount = f_FirstShownStat > 0;
    s16 firstArrow;
    s16 arrowIndex;
    s16 top;

    if (f_FirstShownStat + STATS_SHOWN_ROW_COUNT < STATS_COUNT) {
        arrowCount++;
    }

    if (arrowCount != 0) {
        firstArrow = arrowCount == 1 && (f_FirstShownStat + STATS_SHOWN_ROW_COUNT < STATS_COUNT);

        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_CourseStatsDigitsAndArrows)));
        for (arrowIndex = 0; arrowIndex != arrowCount; arrowIndex++) {
            top = (arrowIndex != 0) ? STATS_DOWN_ARROW_TOP : STATS_UP_ARROW_TOP;
            // A lone down arrow. The count is tested again, to match.
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
            gSPScisTextureRectangle(
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

// Draws the course's spinning trophy in the color of each place won in its
// tournament, stepping the spin's images, or on the totals the points needed to
// open the next course.
//
// Each edge works out TrophyOffset anew, and that is there to match: the
// rectangle's command casts its edges to s16, so GCC adds the shift left in 16
// bits, as 0xFFEC in a register, and the scheduler puts the offset's
// instructions among the first edge's, where they lengthen the life of the
// left position's register and so decide which registers the edges get. With
// the spacing in a u16 local, 48 words differ in those registers alone, and
// with the whole offset in a local, 29 to 51. Without the cast to u16 the shift
// left is added as -20 to each edge: 27 words differ.
void DrawCourseTrophies(Gfx **displayListHead) {
    EncodedAsset asset;
    Gfx *gfx = *displayListHead;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    PaletteColor *palette = NULL;
    s32 openCourseCount;
    void *texels;
    s16 place;

    openCourseCount = GetOpenCourseCount();
    if (memory->statsCourse == openCourseCount) {
        SetUpShadowedTextDrawing(&gfx);
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
                MESSAGE_ALL_COURSES_OPEN_TWO_LINES,
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
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(
                        f_StatsTrophyPositions[memory->statsCourse].x + TrophyOffset(place)
                    ),
                    QuarterPixels(f_StatsTrophyPositions[memory->statsCourse].y),
                    QuarterPixels(
                        f_StatsTrophyPositions[memory->statsCourse].x + TrophyOffset(place) +
                        g_TrophySizes[memory->statsCourse].width
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

        f_StatsTrophyFrameTime++;
        if (f_StatsTrophyFrameTime >= TROPHY_FRAME_DELAY) {
            f_StatsTrophyFrameTime = 0;
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

// The smallest number of each length
#define TWO_DIGIT_FIRST DECIMAL_BASE
#define THREE_DIGIT_FIRST (TWO_DIGIT_FIRST * DECIMAL_BASE)
#define FOUR_DIGIT_FIRST (THREE_DIGIT_FIRST * DECIMAL_BASE)
// The largest number of each length
#define ONE_DIGIT_MAX (TWO_DIGIT_FIRST - 1)
#define TWO_DIGIT_MAX (THREE_DIGIT_FIRST - 1)
#define THREE_DIGIT_MAX (FOUR_DIGIT_FIRST - 1)
#define FOUR_DIGIT_MAX (FOUR_DIGIT_FIRST * DECIMAL_BASE - 1)

// Draws a number right-aligned in a two-digit field, with narrower spacing,
// clamped to the digits it holds
void DrawNarrowTwoDigitMenuNumber(
    Gfx **displayListHead,
    s32 x,
    s32 y,
    s32 number,
    MenuNumberStyle mark
) {
    Gfx *gfx = *displayListHead;

    if (number > TWO_DIGIT_MAX) {
        number = TWO_DIGIT_MAX;
    }

    if (number < -TWO_DIGIT_MAX) {
        number = -TWO_DIGIT_MAX;
    }

    DrawMenuNumber(&gfx, x, y, number, mark, NUMBER_FIELD_TWO_NARROW);
    *displayListHead = gfx;
}

// Draws a number right-aligned in a one-digit field, clamped to the
// digits it holds
void DrawOneDigitMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 number, MenuNumberStyle mark) {
    Gfx *gfx = *displayListHead;

    if (number > ONE_DIGIT_MAX) {
        number = ONE_DIGIT_MAX;
    }

    if (number < -ONE_DIGIT_MAX) {
        number = -ONE_DIGIT_MAX;
    }

    DrawMenuNumber(&gfx, x, y, number, mark, NUMBER_FIELD_ONE);
    *displayListHead = gfx;
}

// Draws a number right-aligned in a two-digit field, clamped to the
// digits it holds
void DrawTwoDigitMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 number, MenuNumberStyle mark) {
    Gfx *gfx = *displayListHead;

    if (number > TWO_DIGIT_MAX) {
        number = TWO_DIGIT_MAX;
    }

    if (number < -TWO_DIGIT_MAX) {
        number = -TWO_DIGIT_MAX;
    }

    DrawMenuNumber(&gfx, x, y, number, mark, NUMBER_FIELD_TWO);
    *displayListHead = gfx;
}

// Draws a number right-aligned in a three-digit field, clamped to the
// digits it holds
void DrawThreeDigitMenuNumber(
    Gfx **displayListHead,
    s32 x,
    s32 y,
    s32 number,
    MenuNumberStyle mark
) {
    Gfx *gfx = *displayListHead;

    if (number > THREE_DIGIT_MAX) {
        number = THREE_DIGIT_MAX;
    }

    if (number < -THREE_DIGIT_MAX) {
        number = -THREE_DIGIT_MAX;
    }

    DrawMenuNumber(&gfx, x, y, number, mark, NUMBER_FIELD_THREE);
    *displayListHead = gfx;
}

// Draws a number right-aligned in a four-digit field, clamped to the
// digits it holds
void DrawFourDigitMenuNumber(
    Gfx **displayListHead,
    s32 x,
    s32 y,
    s32 number,
    MenuNumberStyle mark
) {
    Gfx *gfx = *displayListHead;

    if (number > FOUR_DIGIT_MAX) {
        number = FOUR_DIGIT_MAX;
    }

    if (number < -FOUR_DIGIT_MAX) {
        number = -FOUR_DIGIT_MAX;
    }

    DrawMenuNumber(&gfx, x, y, number, mark, NUMBER_FIELD_FOUR);
    *displayListHead = gfx;
}

// Draws a number without right-aligning it in a field
void DrawUnalignedMenuNumber(
    Gfx **displayListHead,
    s32 x,
    s32 y,
    s32 number,
    MenuNumberStyle mark
) {
    Gfx *gfx = *displayListHead;

    DrawMenuNumber(&gfx, x, y, number, mark, NUMBER_FIELD_NONE);
    *displayListHead = gfx;
}

#define NARROW_DIGIT_SPACING 12
#define MENU_NUMBER_DIGIT_LIMIT 4
#define MENU_NUMBER_LIMIT FOUR_DIGIT_MAX

// Draws a number of up to four digits with the glyphs of ASSET_LARGE_DIGITS,
// marked as the mark argument says and right-aligned in a field. The field's
// shift depends only on the field, so a number two or three digits long in a
// wider field moves as far right as a one-digit one. Numbers past four digits
// draw nothing.
void DrawMenuNumber(
    Gfx **displayListHead,
    s32 x,
    s32 y,
    s32 number,
    MenuNumberStyle mark,
    NumberField field
) {
    Gfx *gfx = *displayListHead;
    s16 digits[MENU_NUMBER_DIGIT_LIMIT];
    s16 digitCount = 0;
    s32 shift;
    s16 spacing;
    s16 index; // The mark's glyph, then the digit being drawn

    if (number > MENU_NUMBER_LIMIT) {
        return;
    }

    if (number < -MENU_NUMBER_LIMIT) {
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
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_LargeDigitsImage)));
    shift = 0;
    if (mark != MENU_NUMBER_PLAIN) {
        switch (mark) {
            case MENU_NUMBER_SIGNED:
                if (number > 0) {
                    index = DIGIT_GLYPH_PLUS;
                    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0x7F, OPAQUE_ALPHA);
                }
                else if (number < 0) {
                    index = DIGIT_GLYPH_MINUS;
                    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                    gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0xFF, 0x7F, OPAQUE_ALPHA);
                }
                else {
                    index = DIGIT_GLYPH_PLUS_MINUS;
                }
                break;
            case MENU_NUMBER_RED_TINT:
                gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x7F, 0x7F, OPAQUE_ALPHA);
                index = DIGIT_GLYPH_TIMES;
                break;
            case MENU_NUMBER_GREEN_TINT:
                gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0xFF, 0x7F, OPAQUE_ALPHA);
                index = DIGIT_GLYPH_TIMES;
                break;
            case MENU_NUMBER_TIMES:
            default:
                index = DIGIT_GLYPH_TIMES;
                break;
        }

        if (mark != MENU_NUMBER_RED_TINT && mark != MENU_NUMBER_GREEN_TINT) {
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(ImageTexels(f_LargeDigitsImage) + (index * DIGIT_GLYPH_IMAGE_SIZE)),
                G_IM_FMT_CI,
                DIGIT_GLYPH_SIZE,
                DIGIT_GLYPH_SIZE,
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
                QuarterPixels(x + DIGIT_GLYPH_SIZE),
                QuarterPixels(y + DIGIT_GLYPH_SIZE),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
            shift = DIGIT_GLYPH_SIZE;
        }
    }

    if (number >= 0 && number < TWO_DIGIT_FIRST) {
        digitCount = 1;
        if (field == NUMBER_FIELD_TWO) {
            shift += DIGIT_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_THREE) {
            shift += 2 * DIGIT_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_FOUR) {
            shift += 3 * DIGIT_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_TWO_NARROW) {
            shift += NARROW_DIGIT_SPACING;
        }

        digits[0] = number % DECIMAL_BASE;
    }

    if (number >= TWO_DIGIT_FIRST && number < THREE_DIGIT_FIRST) {
        digitCount = 2;
        if (field == NUMBER_FIELD_THREE) {
            shift += 2 * DIGIT_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_FOUR) {
            shift += 3 * DIGIT_GLYPH_SIZE;
        }

        digits[0] = number / DECIMAL_BASE;
        digits[1] = number % DECIMAL_BASE;
    }

    if (number >= THREE_DIGIT_FIRST && number < FOUR_DIGIT_FIRST) {
        digitCount = 3;
        if (field == NUMBER_FIELD_FOUR) {
            shift += 3 * DIGIT_GLYPH_SIZE;
        }

        digits[0] = number / THREE_DIGIT_FIRST;
        digits[1] = (number % THREE_DIGIT_FIRST) / DECIMAL_BASE;
        digits[2] = (number % THREE_DIGIT_FIRST) % DECIMAL_BASE;
    }

    if (number >= FOUR_DIGIT_FIRST && number <= MENU_NUMBER_LIMIT) {
        digitCount = 4;
        digits[0] = number / FOUR_DIGIT_FIRST;
        digits[1] = (number % FOUR_DIGIT_FIRST) / THREE_DIGIT_FIRST;
        digits[2] = ((number % FOUR_DIGIT_FIRST) % THREE_DIGIT_FIRST) / DECIMAL_BASE;
        digits[3] = ((number % FOUR_DIGIT_FIRST) % THREE_DIGIT_FIRST) % DECIMAL_BASE;
    }

    if (number < 0 && number > -TWO_DIGIT_FIRST) {
        digitCount = 1;
        if (field == NUMBER_FIELD_TWO) {
            shift += DIGIT_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_THREE) {
            shift += 2 * DIGIT_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_FOUR) {
            shift += 3 * DIGIT_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_TWO_NARROW) {
            shift += NARROW_DIGIT_SPACING;
        }

        digits[0] = -number % DECIMAL_BASE;
    }

    if (number <= -TWO_DIGIT_FIRST && number > -THREE_DIGIT_FIRST) {
        digitCount = 2;
        if (field == NUMBER_FIELD_THREE) {
            shift += 2 * DIGIT_GLYPH_SIZE;
        }

        if (field == NUMBER_FIELD_FOUR) {
            shift += 3 * DIGIT_GLYPH_SIZE;
        }

        digits[0] = -number / DECIMAL_BASE;
        digits[1] = -number % DECIMAL_BASE;
    }

    if (number <= -THREE_DIGIT_FIRST && number > -FOUR_DIGIT_FIRST) {
        digitCount = 3;
        if (field == NUMBER_FIELD_FOUR) {
            shift += 3 * DIGIT_GLYPH_SIZE;
        }

        digits[0] = -number / THREE_DIGIT_FIRST;
        digits[1] = (-number % THREE_DIGIT_FIRST) / DECIMAL_BASE;
        digits[2] = (-number % THREE_DIGIT_FIRST) % DECIMAL_BASE;
    }

    if (number <= -FOUR_DIGIT_FIRST && number >= -MENU_NUMBER_LIMIT) {
        digitCount = 4;
        digits[0] = -number / FOUR_DIGIT_FIRST;
        digits[1] = (-number % FOUR_DIGIT_FIRST) / THREE_DIGIT_FIRST;
        digits[2] = ((-number % FOUR_DIGIT_FIRST) % THREE_DIGIT_FIRST) / DECIMAL_BASE;
        digits[3] = ((-number % FOUR_DIGIT_FIRST) % THREE_DIGIT_FIRST) % DECIMAL_BASE;
    }

    spacing = DIGIT_GLYPH_SIZE;
    if (field == NUMBER_FIELD_TWO_NARROW) {
        spacing = NARROW_DIGIT_SPACING;
    }

    for (index = 0; index != digitCount; index++) {
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(
                ImageTexels(f_LargeDigitsImage) +
                ((digits[index] + DIGIT_GLYPH_ZERO) * DIGIT_GLYPH_IMAGE_SIZE)
            ),
            G_IM_FMT_CI,
            DIGIT_GLYPH_SIZE,
            DIGIT_GLYPH_SIZE,
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
            QuarterPixels(x + shift + (index * spacing) + DIGIT_GLYPH_SIZE),
            QuarterPixels(y + DIGIT_GLYPH_SIZE),
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

// DrawDecimalMenuNumber writes a figure to one decimal place, its whole part
// lined up on the right
#define DECIMAL_WHOLE_LIMIT 999
#define DECIMAL_DIGIT_LIMIT 6
// The point is drawn half a glyph back, over the right half of the last whole
// digit
#define DECIMAL_POINT_OFFSET (DIGIT_GLYPH_SIZE / 2)

// Draws value to one decimal place in the large digits, its whole part
// right-aligned in three digits from x
void DrawDecimalMenuNumber(Gfx **displayListHead, s32 x, s32 y, f32 number) {
    Gfx *gfx = *displayListHead;
    s32 whole = number;
    s32 shift;
    s32 fraction;
    s32 digitIndex;
    u8 digit;
    // Pads the frame to match; without it 112 words differ
    s32 unused[4] UNUSED;

    if (whole > DECIMAL_WHOLE_LIMIT) {
        whole = DECIMAL_WHOLE_LIMIT;
    }

    if (whole < 0) {
        whole = 0;
    }

    fraction = (number - whole) * (f32)HUNDREDTHS_PER_WHOLE;

    // Round the hundredths to tenths
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
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_LargeDigitsImage)));
    shift = 0;
    if (whole >= 0 && whole < DECIMAL_BASE) {
        shift = 2 * DIGIT_GLYPH_SIZE;
    }

    if (whole >= DECIMAL_BASE && whole < DECIMAL_BASE * DECIMAL_BASE) {
        shift += DIGIT_GLYPH_SIZE;
    }

    sprintf(g_PrintBuffer, "%d", whole);
    for (digitIndex = 0; digitIndex != DECIMAL_DIGIT_LIMIT; digitIndex++) {
        digit = g_PrintBuffer[digitIndex];
        if (digit == '\0') {
            break;
        }

        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(
                ImageTexels(f_LargeDigitsImage) +
                ((digit - '0' + DIGIT_GLYPH_ZERO) * DIGIT_GLYPH_IMAGE_SIZE)
            ),
            G_IM_FMT_CI,
            DIGIT_GLYPH_SIZE,
            DIGIT_GLYPH_SIZE,
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
            QuarterPixels(x + shift + (digitIndex * DIGIT_GLYPH_SIZE)),
            QuarterPixels(y),
            QuarterPixels(x + shift + (digitIndex * DIGIT_GLYPH_SIZE) + DIGIT_GLYPH_SIZE),
            QuarterPixels(y + DIGIT_GLYPH_SIZE),
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
        TextureAddress(
            ImageTexels(f_LargeDigitsImage) + (DIGIT_GLYPH_POINT * DIGIT_GLYPH_IMAGE_SIZE)
        ),
        G_IM_FMT_CI,
        DIGIT_GLYPH_SIZE,
        DIGIT_GLYPH_SIZE,
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
        QuarterPixels(x + shift + ((digitIndex - 1) * DIGIT_GLYPH_SIZE) + DECIMAL_POINT_OFFSET),
        QuarterPixels(y),
        QuarterPixels(
            x + shift + ((digitIndex - 1) * DIGIT_GLYPH_SIZE) + DECIMAL_POINT_OFFSET +
            DIGIT_GLYPH_SIZE
        ),
        QuarterPixels(y + DIGIT_GLYPH_SIZE),
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
            ImageTexels(f_LargeDigitsImage) +
            ((fraction + DIGIT_GLYPH_ZERO) * DIGIT_GLYPH_IMAGE_SIZE)
        ),
        G_IM_FMT_CI,
        DIGIT_GLYPH_SIZE,
        DIGIT_GLYPH_SIZE,
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
        QuarterPixels(x + shift + (digitIndex * DIGIT_GLYPH_SIZE)),
        QuarterPixels(y),
        QuarterPixels(x + shift + (digitIndex * DIGIT_GLYPH_SIZE) + DIGIT_GLYPH_SIZE),
        QuarterPixels(y + DIGIT_GLYPH_SIZE),
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

// Nothing reads these
const char f_UnreadFirstEmptyText[] = "";
const char f_UnreadSecondEmptyText[] = "";

// Draws the sign's glyph with its top-left corner at (x, y)
void DrawMenuSign(Gfx **displayListHead, s32 x, s32 y, MenuSign sign) {
    Gfx *gfx = *displayListHead;
    s32 glyphIndex = DIGIT_GLYPH_TIMES;

    switch (sign) {
        case MENU_SIGN_DASH:
            glyphIndex = DIGIT_GLYPH_MINUS;
            break;
        case MENU_SIGN_TIMES:
            break;
        case MENU_SIGN_CIRCLE:
            glyphIndex = DIGIT_GLYPH_CIRCLE;
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
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_LargeDigitsImage)));
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(f_LargeDigitsImage) + (glyphIndex * DIGIT_GLYPH_IMAGE_SIZE)),
        G_IM_FMT_CI,
        DIGIT_GLYPH_SIZE,
        DIGIT_GLYPH_SIZE,
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
        QuarterPixels(x + DIGIT_GLYPH_SIZE),
        QuarterPixels(y + DIGIT_GLYPH_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// Draws "OFF" in the large digits' two glyphs for it; nothing calls this
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
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_LargeDigitsImage)));
    for (glyphIndex = 0; glyphIndex != DIGIT_GLYPH_OFF_COUNT; glyphIndex++) {
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(
                ImageTexels(f_LargeDigitsImage) +
                ((DIGIT_GLYPH_OFF + glyphIndex) * DIGIT_GLYPH_IMAGE_SIZE)
            ),
            G_IM_FMT_CI,
            DIGIT_GLYPH_SIZE,
            DIGIT_GLYPH_SIZE,
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
            QuarterPixels(x + (glyphIndex * DIGIT_GLYPH_SIZE)),
            QuarterPixels(y),
            QuarterPixels(x + (glyphIndex * DIGIT_GLYPH_SIZE) + DIGIT_GLYPH_SIZE),
            QuarterPixels(y + DIGIT_GLYPH_SIZE),
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

// Draws the green left and right arrows either side of a width centered on a
// point, switching their blink frame every SIDE_ARROW_BLINK_FRAME_COUNT calls
//
// frameOffset is worked out before the palette load; after it, 264 words
// differ. The arrow's image offset is added to frameOffset before the
// image's address; added to the address in turn, 4 words differ, and with
// frameOffset first, 2. Setting top at its declaration, not just before the
// rectangle, gives 134 words, 38 in shape.
void DrawBlinkingSideArrows(
    Gfx **displayListHead,
    ImageAsset *image,
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
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(image)));
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

// Draws one of the four side arrow images, in either blink state, centered on
// a point. The header size is held in a local, as the matching code needs:
// with the constant, GCC folds the texels' offset differently.
void DrawGreenArrow(
    Gfx **displayListHead,
    ImageAsset *image,
    s32 blinkFrame,
    MenuArrow direction,
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
