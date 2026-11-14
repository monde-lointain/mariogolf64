#include "course_stats_screen.h"

#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "compiler.h"
#include "course.h"
#include "course_stats.h"
#include "game.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "language.h"
#include "main_menu.h"
#include "math_util.h"
#include "menu.h"
#include "menu_arrows.h"
#include "menu_memory.h"
#include "menu_number.h"
#include "save.h"
#include "screen.h"
#include "sky_and_hud_animations.h"
#include "sound.h"
#include "text.h"
#include "view_projection.h"

#define ASSET_HOLE_IN_ONE_BADGE 0x8EA
#define ASSET_DIGITS_AND_ARROWS 0x5A5

#define COURSE_MENU_IMAGE_COUNT 7

#define COURSE_BANNER_TOP 110
#define COURSE_BANNER_STRIP_SPACING 8
#define COURSE_NAME_Y 116

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
#define STATS_ROUND_COUNT_X 96
#define STATS_VALUE_X 112
#define STATS_TWO_DIGIT_X 128
#define STATS_ONE_DIGIT_X 144
#define STATS_UNIT_X 178
#define STATS_UNIT_OFFSET_Y 4
#define DIGIT_GLYPH_OFF_COUNT 2
#define ONE_DIGIT_NUMBER_MAX (DECIMAL_BASE - 1)
#define TWO_DIGIT_NUMBER_MAX ((DECIMAL_BASE * DECIMAL_BASE) - 1)
#define IsOneDigitNumber(n) ((n) >= -ONE_DIGIT_NUMBER_MAX && (n) <= ONE_DIGIT_NUMBER_MAX)
#define IsTwoDigitNumber(n) \
    (((n) >= DECIMAL_BASE && (n) <= TWO_DIGIT_NUMBER_MAX) || \
        ((n) >= -TWO_DIGIT_NUMBER_MAX && (n) <= -DECIMAL_BASE))

#define STATS_FIRST_ARROW_IMAGE 16
#define STATS_ARROW_WIDTH 16
#define STATS_ARROW_HEIGHT 9
#define STATS_ARROW_IMAGE_SIZE (STATS_ARROW_WIDTH * STATS_ARROW_HEIGHT / 2)
#define STATS_ARROW_LEFT 32
#define STATS_UP_ARROW_TOP 144
#define STATS_DOWN_ARROW_TOP 216

#define SIDE_ARROW_COUNT 2
#define SIDE_ARROW_RIGHT 0
#define SIDE_ARROW_LEFT 1
#define SIDE_ARROW_FIRST_IMAGE MENU_ARROW_RIGHT
#define SIDE_ARROW_BLINK_FRAME_COUNT 4

#define BADGE_ICON_X 222
#define BADGE_ICON_Y 144
#define BADGE_COUNT_X 250
#define BADGE_COUNT_Y 146

#define TROPHY_SPACING 24
#define TROPHY_SHIFT_LEFT 20
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
    u32 y;
} TrophyPosition;

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

static s16 f_FirstShownStat = 0;
static s16 f_StatsTrophyFrame = 0;
static s16 f_ShownStatsTrophyBufferIndex = 0;
static s16 f_StatsTrophyFrameTime = 0;
static s32 f_StatsArrowFrame = 0;
static s32 f_StatsArrowTimer = 0;
static s32 f_UnreadStatsWord UNUSED = 0;
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
static f32 f_UnreadStatsDrawTime = 0.0f;
static char f_UnreadStatsTexts[3][10] UNUSED = {"EXP", "ｺｰｽｹﾞｯﾄ", "純ﾞ"};
static TrophyPosition f_StatsTrophyPositions[COURSE_COUNT] =
    {{230, 177}, {230, 177}, {230, 177}, {236, 165}, {226, 184}, {236, 175}};

static Mat4 f_WheelProjection;
static Mat4 f_WheelView;
static ImageAsset *f_CourseStatsBadge;
static ImageAsset **f_StatsTrophyBuffers;
static ImageAsset **f_UnreadSecondTrophyBuffers;
static ImageAsset **f_UnreadLastTrophyBuffers;
static ImageAsset **f_CourseStatsIcons;
static ImageAsset *f_CourseStatsNamePlate;
static ImageAsset *f_CourseStatsDigitsAndArrows;
static ImageAsset *f_LargeDigitsImage;
static ImageAsset *f_CourseStatsGreenArrows;

static u8 f_UnreadStatsBytes[0x4] UNUSED;
static u8 f_UnreadStatsBlock[0x188] UNUSED;

void LoadCourseStatsIcons(ImageAsset **buffers) {
    EncodedAsset asset;
    s32 unused[4] UNUSED;
    s16 imageIndex;

    f_CourseStatsIcons = buffers;
    for (imageIndex = 0; imageIndex != COURSE_MENU_IMAGE_COUNT; imageIndex++) {
        OpenEncodedAsset(g_CourseIconAssets[imageIndex], &asset);
        ReadEncodedAsset(f_CourseStatsIcons[imageIndex], &asset);
    }
}

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

#define COURSE_TOTALS_IMAGE COURSE_COUNT

void DrawCourseStats(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    u16 perspectiveNormalization;
    f32 iconX[WHEEL_SLOT_LIMIT];
    f32 iconY[WHEEL_SLOT_LIMIT];
    f32 iconScale[WHEEL_SLOT_LIMIT];
    s32 unusedSlots[WHEEL_SLOT_LIMIT] UNUSED;
    s32 drawOrder[WHEEL_SLOT_LIMIT];
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

void DrawStatsBadgeCount(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s16 badgeCount;
    s16 index;
    s16 holeIndex;
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

#define TWO_DIGIT_FIRST DECIMAL_BASE
#define THREE_DIGIT_FIRST (TWO_DIGIT_FIRST * DECIMAL_BASE)
#define FOUR_DIGIT_FIRST (THREE_DIGIT_FIRST * DECIMAL_BASE)
#define ONE_DIGIT_MAX (TWO_DIGIT_FIRST - 1)
#define TWO_DIGIT_MAX (THREE_DIGIT_FIRST - 1)
#define THREE_DIGIT_MAX (FOUR_DIGIT_FIRST - 1)
#define FOUR_DIGIT_MAX (FOUR_DIGIT_FIRST * DECIMAL_BASE - 1)

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
    s16 index;

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

#define DECIMAL_WHOLE_LIMIT 999
#define DECIMAL_DIGIT_LIMIT 6
#define DECIMAL_POINT_OFFSET (DIGIT_GLYPH_SIZE / 2)

void DrawDecimalMenuNumber(Gfx **displayListHead, s32 x, s32 y, f32 number) {
    Gfx *gfx = *displayListHead;
    s32 whole = number;
    s32 shift;
    s32 fraction;
    s32 digitIndex;
    u8 digit;
    s32 unused[4] UNUSED;

    if (whole > DECIMAL_WHOLE_LIMIT) {
        whole = DECIMAL_WHOLE_LIMIT;
    }

    if (whole < 0) {
        whole = 0;
    }

    fraction = (number - whole) * (f32)HUNDREDTHS_PER_WHOLE;

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

const char f_UnreadFirstEmptyText[] = "";
const char f_UnreadSecondEmptyText[] = "";

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
