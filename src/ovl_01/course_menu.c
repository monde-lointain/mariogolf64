#include "common.h"
#include "asset.h"
#include "libc.h"
#include "text.h"
#include "variables.h"

/* The images of D_800C2064 that the course menu reads */
#define COURSE_MENU_IMAGE_COUNT 7

extern u16 D_800C2064[];
extern s32 D_802300A8_ovl_01;
extern s32 D_802300AC_ovl_01;
extern void *D_8023A810_ovl_01;
extern void **D_8023A820_ovl_01;
extern void *D_8023A824_ovl_01;
extern void *D_8023A828_ovl_01;
extern void *D_8023A82C_ovl_01;
extern void *D_8023A830_ovl_01;

void SetTextColor(Gfx **displayListHead, s32 red, s32 green, s32 blue);
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

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020E0AC_ovl_01);

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

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020F794_ovl_01);

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

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020FEF8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_80210190_ovl_01);

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

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_80212C54_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_80212FEC_ovl_01);
