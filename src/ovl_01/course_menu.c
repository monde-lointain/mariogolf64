#include "common.h"
#include "asset.h"
#include "course.h"
#include "hole_display.h"
#include "libc.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"
#include "screen.h"
#include "sound.h"
#include "text.h"
#include "variables.h"

/* The images of D_800C2064 that the course menu reads */
#define COURSE_MENU_IMAGE_COUNT 7

/* The buffers that func_8020E0AC keeps, by the kind its caller gives */
#define COURSE_BUFFER_FIRST 1
#define COURSE_BUFFER_SECOND 2
#define COURSE_BUFFER_LAST 3

/* func_8020E0AC reads the chosen course's first image into two buffers, and its three files */
#define COURSE_IMAGE_COPY_COUNT 2
#define COURSE_IMAGE_COUNT 16
#define COURSE_FILE_COUNT 3

/* func_8020F2D0 squeezes the banner's strips under the course menu's title, with the course's name on it */
#define COURSE_BANNER_TOP 110
#define COURSE_BANNER_STRIP_SPACING 8
#define COURSE_NAME_Y 116

/* The course's statistics: the panel behind them, and the rows of it that show */
/* How far func_8020E390 turns the course wheel for each move, and how fast */
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
/* Where func_8020F9B8 draws the figures; shorter best scores move right to line up */
#define STATS_ROUND_COUNT_X 96
#define STATS_VALUE_X 112
#define STATS_TWO_DIGIT_X 128
#define STATS_ONE_DIGIT_X 144
#define STATS_UNIT_X 178
#define STATS_UNIT_OFFSET_Y 4
#define MENU_NUMBER_UNK2 2
/* The 16x16 CI4 glyphs of asset 0x689 in D_8023A82C; two of them spell "OFF" */
#define MENU_GLYPH_SIZE 16
#define MENU_GLYPH_IMAGE_SIZE (MENU_GLYPH_SIZE * MENU_GLYPH_SIZE / 2)
#define MENU_GLYPH_TIMES 0
#define MENU_GLYPH_PLUS 13
#define MENU_GLYPH_PLUS_MINUS 15
#define OFF_FIRST_GLYPH 16
#define OFF_GLYPH_COUNT 2

/* The arrows beside the statistics that show there are more rows above or below */
#define STATS_FIRST_ARROW_IMAGE 16
#define STATS_ARROW_WIDTH 16
#define STATS_ARROW_HEIGHT 9
#define STATS_ARROW_IMAGE_SIZE (STATS_ARROW_WIDTH * STATS_ARROW_HEIGHT / 2)
#define STATS_ARROW_LEFT 32
#define STATS_UP_ARROW_TOP 144
#define STATS_DOWN_ARROW_TOP 216

/* The blinking arrows that func_80212C54 draws either side of a box, from the third image on */
#define SIDE_ARROW_COUNT 2
#define SIDE_ARROW_RIGHT 0
#define SIDE_ARROW_LEFT 1
#define SIDE_ARROW_SIZE 16
#define SIDE_ARROW_FIRST_IMAGE 2
#define SIDE_ARROW_IMAGE_SIZE (SIDE_ARROW_SIZE * SIDE_ARROW_SIZE / 2)
#define SIDE_ARROW_BLINK_STRIDE (4 * SIDE_ARROW_IMAGE_SIZE)
#define SIDE_ARROW_BLINK_FRAME_COUNT 4
/* func_80212FEC draws one of the four arrow images, in either blink state */
#define ARROW_BLINK_STATE_COUNT 2
#define ARROW_DIRECTION_COUNT 4

/* The birdie badge icon of D_8023A810 and the count beside it */
#define BADGE_ICON_X 222
#define BADGE_ICON_Y 144
#define BADGE_COUNT_X 250
#define BADGE_COUNT_Y 146
#define BADGE_COUNT_DIGIT_WIDTH 16

/* func_802104F0 draws the course's tournament trophies, spinning, or on the totals page the course points */
#define TROPHY_SPACING 24
#define TROPHY_SHIFT_LEFT 20
#define TROPHY_FRAME_DELAY 4
#define TROPHY_FRAME_END (-1)
#define POINTS_NEEDED_X 192
#define POINTS_NEEDED_Y 174
#define ALL_COURSES_OPEN_X 190
#define ALL_COURSES_OPEN_Y 180
#define MESSAGE_POINTS_TO_NEXT_COURSE 0x163
#define MESSAGE_ALL_COURSES_OPEN 0x164

typedef struct TrophySize {
    s8 width;
    s8 height;
} TrophySize;

typedef struct TrophyPosition {
    s32 x;
    u32 y;
} TrophyPosition;

extern u16 D_800C2064[];
extern TrophySize D_800C20E0[COURSE_COUNT];
extern u16 D_800C21B8[][COURSE_IMAGE_COUNT];
extern s16 D_800C2278[][COURSE_FILE_COUNT];
extern void *D_800FBE38[COURSE_FILE_COUNT];
extern s16 D_802300A0_ovl_01;
extern s16 D_802300A2_ovl_01;
extern s16 D_802300A4_ovl_01; /* Which of D_8023A814's buffers holds the course image shown */
extern s16 D_802300A6_ovl_01;
extern s32 D_802300A8_ovl_01;
extern s32 D_802300AC_ovl_01;
extern TrophyPosition D_80230100_ovl_01[COURSE_COUNT];
extern const char *D_802300B4_ovl_01[STATS_COUNT];
extern const char D_802318A8_ovl_01[];
extern const char D_802318AC_ovl_01[];
extern void *D_8023A810_ovl_01;
extern void **D_8023A814_ovl_01;
extern void **D_8023A818_ovl_01;
extern void **D_8023A81C_ovl_01;
extern void **D_8023A820_ovl_01;
extern void *D_8023A824_ovl_01;
extern void *D_8023A828_ovl_01;
extern void *D_8023A82C_ovl_01;
extern void *D_8023A830_ovl_01;

s32 GetOpenCourseCount(void);
void func_8020F9B8_ovl_01(Gfx **displayListHead, s16 statIndex, u8 row);
void func_80207E28_ovl_01(Gfx **displayListHead, void *image, s32 x, s32 y);
void func_80210D34_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4);
void func_80210D94_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4);
void func_80210DF4_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4);
void func_80211A5C_ovl_01(Gfx **displayListHead, s32 x, s32 y, f32 value);
void DrawMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4, u8 style);

void func_8020E010_ovl_01(void **buffers) {
    EncodedAsset asset;
    s32 unused[4];
    s16 imageIndex;

    D_8023A820_ovl_01 = buffers;
    for (imageIndex = 0; imageIndex != COURSE_MENU_IMAGE_COUNT; imageIndex++) {
        OpenEncodedAsset(D_800C2064[imageIndex], &asset);
        ReadEncodedAsset(D_8023A820_ovl_01[imageIndex], &asset);
    }
}

void func_8020E0AC_ovl_01(void **buffers, s32 requestedKind) {
    EncodedAsset asset;
    AssetFile file;
    s8 bufferKind = requestedKind;
    MenuMemory *memory = func_8005AF74();
    s16 index;

    if ((s8)requestedKind < 0) {
        bufferKind = COURSE_BUFFER_FIRST;
    }
    if (bufferKind > COURSE_BUFFER_LAST) {
        bufferKind = COURSE_BUFFER_LAST;
    }
    switch (bufferKind) {
        case COURSE_BUFFER_FIRST:
            D_8023A814_ovl_01 = buffers;
            break;
        case COURSE_BUFFER_SECOND:
            D_8023A818_ovl_01 = buffers;
            break;
        case COURSE_BUFFER_LAST:
            D_8023A81C_ovl_01 = buffers;
            break;
    }
    if (memory->statsCourse != COURSE_COUNT) {
        for (index = 0; index != COURSE_IMAGE_COPY_COUNT; index++) {
            OpenEncodedAsset(D_800C21B8[memory->statsCourse][0], &asset);
            ReadEncodedAsset(buffers[index], &asset);
        }
        for (index = 0; index != COURSE_FILE_COUNT; index++) {
            OpenAssetFile(D_800C2278[memory->statsCourse][index], &file);
            ReadAssetFile(D_800FBE38[index], GetAssetFileSize(&file), &file);
        }
    }
}

void func_8020E26C_ovl_01(void *buffer, s16 bufferKind) {
    EncodedAsset asset;

    switch (bufferKind) {
        /* The course menu keeps no buffer of kind 1 */
        case 1:
            break;
        case 2:
            D_8023A810_ovl_01 = buffer;
            OpenEncodedAsset(0x8EA, &asset);
            ReadEncodedAsset(D_8023A810_ovl_01, &asset);
            break;
        case 3:
            D_8023A824_ovl_01 = buffer;
            OpenEncodedAsset(0x521, &asset);
            ReadEncodedAsset(D_8023A824_ovl_01, &asset);
            break;
        case 4:
            D_8023A810_ovl_01 = buffer;
            OpenEncodedAsset(0x663, &asset);
            ReadEncodedAsset(D_8023A810_ovl_01, &asset);
            break;
        case 5:
            D_8023A828_ovl_01 = buffer;
            OpenEncodedAsset(0x5A5, &asset);
            ReadEncodedAsset(D_8023A828_ovl_01, &asset);
            break;
        case 6:
            D_8023A82C_ovl_01 = buffer;
            OpenEncodedAsset(0x689, &asset);
            ReadEncodedAsset(D_8023A82C_ovl_01, &asset);
            break;
        case 7:
            D_8023A830_ovl_01 = buffer;
            OpenEncodedAsset(0x720, &asset);
            ReadEncodedAsset(D_8023A830_ovl_01, &asset);
            D_802300A8_ovl_01 = 0;
            D_802300AC_ovl_01 = 0;
            break;
    }
}

void func_8020E390_ovl_01(void) {
    EncodedAsset asset;
    AssetFile file;
    MenuMemory *memory = func_8005AF74();
    s32 courseCount = GetOpenCourseCount();
    s32 isDone;
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
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
            SetTextSlot(0, 0, NO_MESSAGE, 1, 0, TEXT_FLAG_TYPED, 255, 255, 255, 255);
            g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
            return;
        }
        if (memory->statsSpin == 0) {
            if (g_MenuRepeatButtons & R_JPAD) {
                PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                SetTextSlot(0, 0, NO_MESSAGE, 1, 0, TEXT_FLAG_TYPED, 255, 255, 255, 255);
                memory->statsCourse++;
                if (memory->statsCourse > courseCount) {
                    memory->statsCourse = 0;
                }
                memory->statsSpin = -STATS_TURN / (courseCount + 1);
                if (memory->statsCourse != courseCount) {
                    D_802300A6_ovl_01 = 0;
                    D_802300A2_ovl_01 = 0;
                    D_802300A4_ovl_01 ^= 1;
                    OpenEncodedAsset(D_800C21B8[memory->statsCourse][0], &asset);
                    ReadEncodedAsset(D_8023A814_ovl_01[D_802300A4_ovl_01], &asset);
                    for (index = 0; index != COURSE_FILE_COUNT; index++) {
                        OpenAssetFile(D_800C2278[memory->statsCourse][index], &file);
                        ReadAssetFile(D_800FBE38[index], GetAssetFileSize(&file), &file);
                    }
                }
            } else if (g_MenuRepeatButtons & L_JPAD) {
                PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                SetTextSlot(0, 0, NO_MESSAGE, 1, 0, TEXT_FLAG_TYPED, 255, 255, 255, 255);
                memory->statsCourse--;
                if (memory->statsCourse < 0) {
                    memory->statsCourse = courseCount;
                }
                memory->statsSpin = STATS_TURN / (courseCount + 1);
                if (memory->statsCourse != courseCount) {
                    D_802300A6_ovl_01 = 0;
                    D_802300A2_ovl_01 = 0;
                    D_802300A4_ovl_01 ^= 1;
                    OpenEncodedAsset(D_800C21B8[memory->statsCourse][0], &asset);
                    ReadEncodedAsset(D_8023A814_ovl_01[D_802300A4_ovl_01], &asset);
                    for (index = 0; index != COURSE_FILE_COUNT; index++) {
                        OpenAssetFile(D_800C2278[memory->statsCourse][index], &file);
                        ReadAssetFile(D_800FBE38[index], GetAssetFileSize(&file), &file);
                    }
                }
            } else if (g_MenuRepeatButtons & U_JPAD) {
                PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                if (D_802300A0_ovl_01 > 0) {
                    D_802300A0_ovl_01--;
                }
            } else if (g_MenuRepeatButtons & D_JPAD) {
                PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                if (D_802300A0_ovl_01 < STATS_COUNT - STATS_SHOWN_ROW_COUNT) {
                    D_802300A0_ovl_01++;
                }
            }
        }
        if (ABS(memory->statsSpin) != 0) {
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

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020E898_ovl_01);

void func_8020F2D0_ovl_01(Gfx **displayListHead, u8 useCourseNames, s32 courseIndex) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    func_8005AF74();
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_8023A824_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_8023A824_ovl_01 + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = COURSE_BANNER_TOP + stripIndex * COURSE_BANNER_STRIP_SPACING;
        gSPScisTextureRectangle(gfx++, MENU_BANNER_LEFT << 2, stripTop << 2,
                                (MENU_BANNER_LEFT + MENU_BANNER_WIDTH) << 2,
                                (stripTop + COURSE_BANNER_STRIP_SPACING) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                                (MENU_BANNER_STRIP_HEIGHT << 10) / COURSE_BANNER_STRIP_SPACING);
        gDPPipeSync(gfx++);
    }
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    if (!useCourseNames) {
        if (courseIndex + 1 == COURSE_COUNT + 1) {
            sprintf(g_PrintBuffer, D_802318A8_ovl_01, g_TournamentNames[courseIndex]);
        } else {
            sprintf(g_PrintBuffer, D_802318AC_ovl_01, courseIndex + 1, g_TournamentNames[courseIndex]);
        }
    } else if (courseIndex + 1 == COURSE_COUNT + 1) {
        sprintf(g_PrintBuffer, D_802318A8_ovl_01, g_CourseRecords[courseIndex].name);
    } else {
        sprintf(g_PrintBuffer, D_802318AC_ovl_01, courseIndex + 1, g_CourseRecords[courseIndex].name);
    }
    DrawCenteredLargeText(&gfx, SCREEN_WIDTH / 2, COURSE_NAME_Y, g_PrintBuffer);
    *displayListHead = gfx;
}

const char D_802318A8_ovl_01[] = "%s";
const char D_802318AC_ovl_01[] = "%d:%s";

void func_8020F794_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 row = 0;
    s16 statIndex;

    GetSaveData();
    func_8005AF74();
    GetOpenCourseCount();
    gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(gfx++, STATS_PANEL_LEFT << 2, STATS_PANEL_TOP << 2, STATS_PANEL_RIGHT << 2,
                        STATS_PANEL_BOTTOM << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
    for (statIndex = D_802300A0_ovl_01; statIndex != D_802300A0_ovl_01 + STATS_SHOWN_ROW_COUNT; statIndex++) {
        SetUpTextDrawing(&gfx);
        SetTextColor(&gfx, 255, 255, 255);
        sprintf(g_PrintBuffer, D_802318A8_ovl_01, D_802300B4_ovl_01[statIndex]);
        DrawLargeText(&gfx, STATS_LABEL_X, STATS_FIRST_Y + (s16)row * STATS_LINE_HEIGHT, g_PrintBuffer);
        func_8020F9B8_ovl_01(&gfx, statIndex, row);
        row++;
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void func_8020F9B8_ovl_01(Gfx **displayListHead, s16 statIndex, u8 row) {
    Gfx *gfx = *displayListHead;
    MenuMemory *memory;
    s32 courseIndex;
    s32 bestScore;
    s32 openCourseCount;

    GetSaveData();
    memory = func_8005AF74();
    openCourseCount = GetOpenCourseCount();
    if (memory->statsCourse != openCourseCount) {
        courseIndex = memory->statsCourse;
    } else {
        /* The figures for all the courses together */
        courseIndex = COURSE_COUNT;
    }
    if (D_80105C38[courseIndex].roundCount == 0.0f) {
        return;
    }
    switch (statIndex) {
        case STAT_ROUND_COUNT:
            func_80210D94_ovl_01(&gfx, STATS_ROUND_COUNT_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT,
                                 D_80105C38[courseIndex].roundCount, 0);
            break;
        case STAT_BEST_SCORE:
            bestScore = D_80105C38[courseIndex].bestScore;
            if (bestScore >= -9 && bestScore <= 9) {
                func_80210DF4_ovl_01(&gfx, STATS_ONE_DIGIT_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT, bestScore,
                                     MENU_NUMBER_UNK2);
            } else if ((bestScore >= 10 && bestScore <= 99) || (bestScore >= -99 && bestScore <= -10)) {
                func_80210DF4_ovl_01(&gfx, STATS_TWO_DIGIT_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT, bestScore,
                                     MENU_NUMBER_UNK2);
            } else {
                func_80210D34_ovl_01(&gfx, STATS_VALUE_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT, bestScore,
                                     MENU_NUMBER_UNK2);
            }
            break;
        case STAT_AVERAGE_SCORE:
            func_80211A5C_ovl_01(&gfx, STATS_VALUE_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT,
                                 D_80105C38[courseIndex].averageScore);
            break;
        case STAT_BEST_DRIVE:
            func_80211A5C_ovl_01(&gfx, STATS_VALUE_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT,
                                 D_80105C38[courseIndex].bestDrive);
            gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
            func_8020FD48_ovl_01(&gfx, STATS_UNIT_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT + STATS_UNIT_OFFSET_Y);
            break;
        case STAT_AVERAGE_DRIVE:
            func_80211A5C_ovl_01(&gfx, STATS_VALUE_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT,
                                 D_80105C38[courseIndex].averageDrive);
            func_8020FD48_ovl_01(&gfx, STATS_UNIT_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT + STATS_UNIT_OFFSET_Y);
            break;
        case STAT_FAIRWAY_PERCENT:
            func_80211A5C_ovl_01(&gfx, STATS_VALUE_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT,
                                 D_80105C38[courseIndex].fairwayPercent);
            func_8020FE20_ovl_01(&gfx, STATS_UNIT_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT + STATS_UNIT_OFFSET_Y);
            break;
        case STAT_AVERAGE_PUTTS:
            func_80211A5C_ovl_01(&gfx, STATS_VALUE_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT,
                                 D_80105C38[courseIndex].averagePutts);
            break;
        case STAT_PAR_SAVE_PERCENT:
            func_80211A5C_ovl_01(&gfx, STATS_VALUE_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT,
                                 D_80105C38[courseIndex].parSavePercent);
            func_8020FE20_ovl_01(&gfx, STATS_UNIT_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT + STATS_UNIT_OFFSET_Y);
            break;
        case STAT_PAR_ON_PERCENT:
            func_80211A5C_ovl_01(&gfx, STATS_VALUE_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT,
                                 D_80105C38[courseIndex].parOnPercent);
            func_8020FE20_ovl_01(&gfx, STATS_UNIT_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT + STATS_UNIT_OFFSET_Y);
            break;
        case STAT_SAND_SAVE_PERCENT:
            func_80211A5C_ovl_01(&gfx, STATS_VALUE_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT,
                                 D_80105C38[courseIndex].sandSavePercent);
            func_8020FE20_ovl_01(&gfx, STATS_UNIT_X, STATS_FIRST_Y + row * STATS_LINE_HEIGHT + STATS_UNIT_OFFSET_Y);
            break;
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

const char D_802318E0_ovl_01[] = "%c";

void func_8020FD48_ovl_01(Gfx **displayListHead, s16 x, s16 y) {
    Gfx *gfx = *displayListHead;

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 63, 255, 63);
    sprintf(g_PrintBuffer, D_802318E0_ovl_01, 'Y');
    DrawLargeText(&gfx, x, y, g_PrintBuffer);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void func_8020FE20_ovl_01(Gfx **displayListHead, s16 x, s16 y) {
    Gfx *gfx = *displayListHead;

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 64, 192);
    sprintf(g_PrintBuffer, D_802318E0_ovl_01, '%');
    DrawLargeText(&gfx, x, y, g_PrintBuffer);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Doesn't match: the original keeps the badge count in a1 and copies it to s1 after counting */
#ifdef NON_MATCHING
void func_8020FEF8_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    SaveData *save = GetSaveData();
    MenuMemory *memory = func_8005AF74();
    s16 badgeCount = 0;
    s16 index; /* A hole, then a course, then where the count moves to */
    s16 holeIndex;

    if (GetOpenCourseCount() != memory->statsCourse) {
        for (index = 0; index != COURSE_HOLE_COUNT; index++) {
            if (save->unk990.birdieBadgeCounts[memory->statsCourse][index]) {
                badgeCount++;
            }
        }
    } else {
        for (index = 0; index != COURSE_COUNT; index++) {
            for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
                if (save->unk990.birdieBadgeCounts[index][holeIndex]) {
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
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    func_80207E28_ovl_01(&gfx, D_8023A810_ovl_01, BADGE_ICON_X + index, BADGE_ICON_Y);
    func_80210DF4_ovl_01(&gfx, BADGE_COUNT_X + index, BADGE_COUNT_Y, badgeCount, 1);
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020FEF8_ovl_01);
#endif

/*
 * Doesn't match: the original counts the arrows and picks the first one with
 * branches, where this version computes them without, so the registers and
 * the spills to the stack differ.
 */
#ifdef NON_MATCHING
void func_80210190_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 hasRowsAbove = D_802300A0_ovl_01 > 0;
    s32 hasRowsBelow = D_802300A0_ovl_01 + STATS_SHOWN_ROW_COUNT < STATS_COUNT;
    s16 arrowCount = hasRowsAbove;
    s32 firstArrow;
    s16 arrowIndex;
    s32 top;
    s32 unused[4];

    if (hasRowsBelow) {
        arrowCount++;
    }
    if (arrowCount != 0) {
        firstArrow = 0;
        if (arrowCount == 1) {
            firstArrow = hasRowsBelow;
        }
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_8023A828_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
        for (arrowIndex = 0; arrowIndex != arrowCount; arrowIndex++) {
            top = (arrowIndex != 0) ? STATS_DOWN_ARROW_TOP : STATS_UP_ARROW_TOP;
            if (arrowCount == 1 && firstArrow) {
                top = STATS_DOWN_ARROW_TOP;
            }
            gDPLoadTextureBlock_4b(gfx++,
                                   ((u32)D_8023A828_ovl_01 + (firstArrow + arrowIndex) * STATS_ARROW_IMAGE_SIZE +
                                    (PALETTED_TEXELS_OFFSET + STATS_FIRST_ARROW_IMAGE * STATS_ARROW_IMAGE_SIZE)) &
                                       ~7,
                                   G_IM_FMT_CI, STATS_ARROW_WIDTH, STATS_ARROW_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            gDPPipeSync(gfx++);
            gSPTextureRectangle(gfx++, STATS_ARROW_LEFT << 2, top << 2, (STATS_ARROW_LEFT + STATS_ARROW_WIDTH) << 2,
                                (top + STATS_ARROW_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                                TEXEL_STEP_ONE);
            gDPPipeSync(gfx++);
        }
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_80210190_ovl_01);
#endif

/*
 * Doesn't match: the original adds the trophies' shift as 0xFFEC, the low half of -20, after reading the course's
 * position, and adds the position to it before the width. "+ (u16)-TROPHY_SHIFT_LEFT" gives that constant.
 */
#ifdef NON_MATCHING
void func_802104F0_ovl_01(Gfx **displayListHead) {
    EncodedAsset asset;
    Gfx *gfx = *displayListHead;
    SaveData *save = GetSaveData();
    MenuMemory *memory = func_8005AF74();
    void *palette = NULL;
    s32 openCourseCount;
    s32 left;
    void *texels;
    s16 place;

    openCourseCount = GetOpenCourseCount();
    if (memory->statsCourse == openCourseCount) {
        BeginShadowedText(&gfx);
        SetTextColor(&gfx, 255, 255, 255);
        openCourseCount = CountOpenCourses(save->coursePoints);
        if (openCourseCount < COURSE_COUNT) {
            SetTextSlotValue(GetCoursePointsNeeded(openCourseCount) - save->coursePoints, 0, 0);
            SetTextSlot(POINTS_NEEDED_X, POINTS_NEEDED_Y, MESSAGE_POINTS_TO_NEXT_COURSE, 1, 0, 0, 255, 255, 255, 255);
        } else {
            SetTextSlot(ALL_COURSES_OPEN_X, ALL_COURSES_OPEN_Y, MESSAGE_ALL_COURSES_OPEN, 1, 0, 0, 255, 255, 255, 255);
        }
    } else {
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        for (place = 0; place != TOURNAMENT_PLACE_COUNT; place++) {
            if (D_80105C38[memory->statsCourse].hasPlace[place]) {
                gDPPipeSync(gfx++);
                if (place == FIRST_PLACE_INDEX) {
                    palette = D_800FBE38[FIRST_PLACE_INDEX];
                }
                if (place == SECOND_PLACE_INDEX) {
                    palette = D_800FBE38[SECOND_PLACE_INDEX];
                }
                if (place == THIRD_PLACE_INDEX) {
                    palette = D_800FBE38[THIRD_PLACE_INDEX];
                }
                texels = D_8023A814_ovl_01[D_802300A4_ovl_01];
                gDPLoadTLUT_pal256(gfx++, (u32)palette & ~7);
                gDPLoadTextureTile(gfx++, (u32)texels & ~7, G_IM_FMT_CI, G_IM_SIZ_8b,
                                   D_800C20E0[memory->statsCourse].width, D_800C20E0[memory->statsCourse].height, 0, 0,
                                   D_800C20E0[memory->statsCourse].width - 1,
                                   D_800C20E0[memory->statsCourse].height - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
                gDPPipeSync(gfx++);
                left = place * TROPHY_SPACING + -TROPHY_SHIFT_LEFT;
                gSPScisTextureRectangle(
                    gfx++, (D_80230100_ovl_01[memory->statsCourse].x + left) << 2,
                    D_80230100_ovl_01[memory->statsCourse].y << 2,
                    (D_800C20E0[memory->statsCourse].width + D_80230100_ovl_01[memory->statsCourse].x + left) << 2,
                    (D_800C20E0[memory->statsCourse].height + D_80230100_ovl_01[memory->statsCourse].y) << 2,
                    G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
                gDPPipeSync(gfx++);
            }
        }
        D_802300A6_ovl_01++;
        if (D_802300A6_ovl_01 >= TROPHY_FRAME_DELAY) {
            D_802300A6_ovl_01 = 0;
            D_802300A2_ovl_01++;
            if ((s16)D_800C21B8[memory->statsCourse][D_802300A2_ovl_01] == TROPHY_FRAME_END) {
                D_802300A2_ovl_01 = 0;
            }
            if (D_802300A2_ovl_01 >= COURSE_IMAGE_COUNT) {
                D_802300A2_ovl_01 = 0;
            }
            D_802300A4_ovl_01 ^= 1;
            OpenEncodedAsset(D_800C21B8[memory->statsCourse][D_802300A2_ovl_01], &asset);
            ReadEncodedAsset(D_8023A814_ovl_01[D_802300A4_ovl_01], &asset);
        }
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_802104F0_ovl_01);
#endif

void func_80210C18_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4) {
    Gfx *gfx = *displayListHead;

    if (value > 99) {
        value = 99;
    }
    if (value < -99) {
        value = -99;
    }
    DrawMenuNumber(&gfx, x, y, value, arg4, 0);
    *displayListHead = gfx;
}

void func_80210C74_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4) {
    Gfx *gfx = *displayListHead;

    if (value > 9) {
        value = 9;
    }
    if (value < -9) {
        value = -9;
    }
    DrawMenuNumber(&gfx, x, y, value, arg4, 1);
    *displayListHead = gfx;
}

void func_80210CD4_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4) {
    Gfx *gfx = *displayListHead;

    if (value > 99) {
        value = 99;
    }
    if (value < -99) {
        value = -99;
    }
    DrawMenuNumber(&gfx, x, y, value, arg4, 2);
    *displayListHead = gfx;
}

void func_80210D34_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4) {
    Gfx *gfx = *displayListHead;

    if (value > 999) {
        value = 999;
    }
    if (value < -999) {
        value = -999;
    }
    DrawMenuNumber(&gfx, x, y, value, arg4, 3);
    *displayListHead = gfx;
}

void func_80210D94_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4) {
    Gfx *gfx = *displayListHead;

    if (value > 9999) {
        value = 9999;
    }
    if (value < -9999) {
        value = -9999;
    }
    DrawMenuNumber(&gfx, x, y, value, arg4, 4);
    *displayListHead = gfx;
}

void func_80210DF4_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4) {
    Gfx *gfx = *displayListHead;

    DrawMenuNumber(&gfx, x, y, value, arg4, 5);
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", DrawMenuNumber);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_80211A5C_ovl_01);

void func_802123E0_ovl_01(Gfx **displayListHead, s32 x, s32 y, u8 sign) {
    Gfx *gfx = *displayListHead;
    s32 glyphIndex = MENU_GLYPH_TIMES;

    switch (sign) {
        case MENU_SIGN_PLUS:
            glyphIndex = MENU_GLYPH_PLUS;
            break;
        case MENU_SIGN_TIMES:
            break;
        case MENU_SIGN_PLUS_MINUS:
            glyphIndex = MENU_GLYPH_PLUS_MINUS;
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
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_8023A82C_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureBlock_4b(gfx++,
                           ((u32)D_8023A82C_ovl_01 + PALETTED_TEXELS_OFFSET + glyphIndex * MENU_GLYPH_IMAGE_SIZE) & ~7,
                           G_IM_FMT_CI, MENU_GLYPH_SIZE, MENU_GLYPH_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, x << 2, y << 2, (x + MENU_GLYPH_SIZE) << 2, (y + MENU_GLYPH_SIZE) << 2,
                            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Nothing calls this */
void func_80212858_ovl_01(Gfx **displayListHead, s32 x, s32 y) {
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
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_8023A82C_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    for (glyphIndex = 0; glyphIndex != OFF_GLYPH_COUNT; glyphIndex++) {
        gDPLoadTextureBlock_4b(gfx++,
                               ((u32)D_8023A82C_ovl_01 + PALETTED_TEXELS_OFFSET +
                                (OFF_FIRST_GLYPH + glyphIndex) * MENU_GLYPH_IMAGE_SIZE) & ~7,
                               G_IM_FMT_CI, MENU_GLYPH_SIZE, MENU_GLYPH_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                               G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, (x + glyphIndex * MENU_GLYPH_SIZE) << 2, y << 2,
                                (x + glyphIndex * MENU_GLYPH_SIZE + MENU_GLYPH_SIZE) << 2, (y + MENU_GLYPH_SIZE) << 2,
                                G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

/*
 * Doesn't match: the original spills displayListHead to its argument slot,
 * keeps the arrow top in a register only for the lower edge, and needs one
 * stack slot fewer.
 */
#ifdef NON_MATCHING
void func_80212C54_ovl_01(Gfx **displayListHead, void *image, s32 *blinkFrame, s32 *blinkTimer, s32 width,
                          s32 centerX, s32 centerY) {
    Gfx *gfx = *displayListHead;
    s32 left = 0;
    s32 arrowIndex;
    s32 frameOffset;

    (*blinkTimer)++;
    if (*blinkTimer == SIDE_ARROW_BLINK_FRAME_COUNT) {
        *blinkFrame = !*blinkFrame;
        *blinkTimer = 0;
    }
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    frameOffset = *blinkFrame * SIDE_ARROW_BLINK_STRIDE + PALETTED_TEXELS_OFFSET;
    for (arrowIndex = 0; arrowIndex != SIDE_ARROW_COUNT; arrowIndex++) {
        gDPLoadTextureTile_4b(gfx++,
                              ((u32)image +
                               ((SIDE_ARROW_FIRST_IMAGE + arrowIndex) * SIDE_ARROW_IMAGE_SIZE + frameOffset)) &
                                  ~7,
                              G_IM_FMT_CI, SIDE_ARROW_SIZE, SIDE_ARROW_SIZE, 0, 0, SIDE_ARROW_SIZE - 1,
                              SIDE_ARROW_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                              G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        if (arrowIndex == SIDE_ARROW_RIGHT) {
            left = centerX + width / 2;
        }
        if (arrowIndex == SIDE_ARROW_LEFT) {
            left = centerX - width / 2 - SIDE_ARROW_SIZE;
        }
        gSPScisTextureRectangle(gfx++, left << 2, (centerY - SIDE_ARROW_SIZE / 2) << 2, (left + SIDE_ARROW_SIZE) << 2,
                                (centerY + SIDE_ARROW_SIZE / 2) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                                TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_80212C54_ovl_01);
#endif

/*
 * Doesn't match: the original adds PALETTED_TEXELS_OFFSET to the blink offset
 * before shifting the direction, rather than after.
 */
#ifdef NON_MATCHING
void func_80212FEC_ovl_01(Gfx **displayListHead, void *image, s32 blinkFrame, s32 direction, s32 centerX,
                          s32 centerY) {
    Gfx *gfx = *displayListHead;
    s32 top;
    s32 left;
    void *texels;

    if (blinkFrame < 0) {
        blinkFrame = 0;
    }
    if (blinkFrame >= ARROW_BLINK_STATE_COUNT) {
        blinkFrame = ARROW_BLINK_STATE_COUNT - 1;
    }
    if (direction < 0) {
        direction = 0;
    }
    if (direction >= ARROW_DIRECTION_COUNT) {
        direction = ARROW_DIRECTION_COUNT - 1;
    }
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureTile_4b(gfx++,
                          ((u32)image + (direction * SIDE_ARROW_IMAGE_SIZE +
                                         (blinkFrame * SIDE_ARROW_BLINK_STRIDE + PALETTED_TEXELS_OFFSET))) &
                              ~7,
                          G_IM_FMT_CI, SIDE_ARROW_SIZE, SIDE_ARROW_SIZE, 0, 0, SIDE_ARROW_SIZE - 1, SIDE_ARROW_SIZE - 1,
                          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                          G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    left = centerX - SIDE_ARROW_SIZE / 2;
    top = centerY - SIDE_ARROW_SIZE / 2;
    gSPScisTextureRectangle(gfx++, left << 2, top << 2,
                            (centerX + SIDE_ARROW_SIZE / 2) << 2, (centerY + SIDE_ARROW_SIZE / 2) << 2,
                            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_80212FEC_ovl_01);
#endif
