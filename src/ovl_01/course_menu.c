#include "common.h"
#include "asset.h"
#include "libc.h"
#include "menu_memory.h"
#include "save.h"
#include "screen.h"
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

/* The course's statistics: the panel behind them, and the rows of it that show */
#define STATS_COUNT 10
#define STATS_SHOWN_ROW_COUNT 4
#define STATS_PANEL_LEFT 24
#define STATS_PANEL_TOP 138
#define STATS_PANEL_RIGHT 302
#define STATS_PANEL_BOTTOM 230
#define STATS_LABEL_X 30
#define STATS_FIRST_Y 154
#define STATS_LINE_HEIGHT 16

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

extern u16 D_800C2064[];
extern u16 D_800C21B8[][COURSE_IMAGE_COUNT];
extern s16 D_800C2278[][COURSE_FILE_COUNT];
extern void *D_800FBE38[COURSE_FILE_COUNT];
extern s16 D_802300A0_ovl_01;
extern s32 D_802300A8_ovl_01;
extern s32 D_802300AC_ovl_01;
extern const char *D_802300B4_ovl_01[STATS_COUNT];
extern const char D_802318A8_ovl_01[];
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
void SetTextColor(Gfx **displayListHead, s32 red, s32 green, s32 blue);
void func_80207E28_ovl_01(Gfx **displayListHead, void *image, s32 x, s32 y);
void func_80210DF4_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4);
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
    if (memory->unk30 != COURSE_COUNT) {
        for (index = 0; index != COURSE_IMAGE_COPY_COUNT; index++) {
            OpenEncodedAsset(D_800C21B8[memory->unk30][0], &asset);
            ReadEncodedAsset(buffers[index], &asset);
        }
        for (index = 0; index != COURSE_FILE_COUNT; index++) {
            OpenAssetFile(D_800C2278[memory->unk30][index], &file);
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

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020E390_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020E898_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020F2D0_ovl_01);

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

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020F9B8_ovl_01);

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

    if (GetOpenCourseCount() != memory->unk30) {
        for (index = 0; index != COURSE_HOLE_COUNT; index++) {
            if (save->unk990.birdieBadgeCounts[memory->unk30][index]) {
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

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_802104F0_ovl_01);

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

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_802123E0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_80212858_ovl_01);

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
