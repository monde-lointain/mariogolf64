#include "common.h"
#include "asset.h"
#include "character.h"
#include "save.h"
#include "work_area.h"

/* The images of D_800C2064 that func_80213390 reads */
#define MENU_SCENE_IMAGE_COUNT 6

/* The bytes kept of each best time holder's palette file */
#define MENU_SCENE_FILE_SIZE 0x200

extern u16 D_800C2064[];
extern void *D_8012D430[CHARACTER_COUNT];
extern u16 D_802308A4_ovl_01[CHARACTER_COUNT];
extern s16 D_802308C8_ovl_01[CHARACTER_COUNT][CHARACTER_PALETTE_COUNT];
extern u8 D_8023BFF0_ovl_01[COURSE_COUNT * BEST_TIME_COUNT][MENU_SCENE_FILE_SIZE];
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

void func_8021585C_ovl_01(void) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save = GetSaveData();
    s32 characterIndex;
    s32 courseIndex;
    s32 rank;
    s32 character;
    s32 palette;

    for (characterIndex = 0; characterIndex != CHARACTER_COUNT; characterIndex++) {
        OpenEncodedAsset(D_802308A4_ovl_01[characterIndex], &asset);
        ReadEncodedAsset(D_8012D430[characterIndex], &asset);
    }
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        for (rank = 0; rank != BEST_TIME_COUNT; rank++) {
            character = save->unkE08[courseIndex][rank];
            if (character != -1) {
                if (character < 0) {
                    character = 0;
                }
                if (character >= CHARACTER_COUNT) {
                    character = CHARACTER_COUNT - 1;
                }
                palette = save->unkE50[courseIndex][rank];
            } else {
                character = 0;
                palette = 0;
            }
            OpenAssetFile(D_802308C8_ovl_01[character][palette], &file);
            ReadAssetFile(D_8023BFF0_ovl_01[courseIndex * BEST_TIME_COUNT + rank], GetAssetFileSize(&file), &file);
        }
    }
}
