#include "common.h"
#include "asset.h"
#include "work_area.h"

/* The images of D_800C2064 that func_80213390 reads */
#define MENU_SCENE_IMAGE_COUNT 6

extern u16 D_800C2064[];
extern void **D_8023AA40_ovl_01;
extern s32 D_80230130_ovl_01;
extern s32 D_80230134_ovl_01;
extern s32 D_80230138_ovl_01;
extern void *D_8023AA44_ovl_01;
extern void *D_8023AA48_ovl_01;
extern void *D_8023AA4C_ovl_01;
extern void *D_8023AA50_ovl_01;

void func_80213390_ovl_01(void **buffers) {
    EncodedAsset asset;

    s16 imageIndex;

    D_8023AA40_ovl_01 = buffers;
    for (imageIndex = 0; imageIndex != MENU_SCENE_IMAGE_COUNT; imageIndex++) {
        OpenEncodedAsset(D_800C2064[imageIndex], &asset);
        ReadEncodedAsset(D_8023AA40_ovl_01[imageIndex], &asset);
    }
}

void func_8021342C_ovl_01(void *buffer) {
    EncodedAsset asset;

    D_8023AA44_ovl_01 = buffer;
    OpenEncodedAsset(0x8E9, &asset);
    ReadEncodedAsset(D_8023AA44_ovl_01, &asset);
}

void func_80213464_ovl_01(void *buffer, s16 bufferKind) {
    EncodedAsset asset;

    switch (bufferKind) {
        case 1:
            D_8023AA44_ovl_01 = &g_WorkArea;
            OpenEncodedAsset(0x8E9, &asset);
            ReadEncodedAsset(D_8023AA44_ovl_01, &asset);
            break;
        case 2:
            D_8023AA48_ovl_01 = buffer;
            break;
        case 3:
            D_8023AA4C_ovl_01 = buffer;
            OpenEncodedAsset(0x688, &asset);
            ReadEncodedAsset(D_8023AA4C_ovl_01, &asset);
            break;
        case 4:
            D_8023AA50_ovl_01 = buffer;
            OpenEncodedAsset(0x720, &asset);
            ReadEncodedAsset(D_8023AA50_ovl_01, &asset);
            D_80230130_ovl_01 = 0;
            D_80230134_ovl_01 = 0;
            D_80230138_ovl_01 = 0;
            break;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_80213558_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_80213748_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_802144E4_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_802149A8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_8021508C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_8021585C_ovl_01);
