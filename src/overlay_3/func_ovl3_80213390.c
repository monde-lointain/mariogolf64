#include "common.h"
#include "../main/rom_asset.h"

extern u8* D_ovl3_8023AA44;
extern u8* D_ovl3_8023AA4C;

extern u16 D_800C2064[];
extern void** D_ovl3_8023AA40;

void func_ovl3_80213390(void** destinations) {
    RomAssetLoad asset;
    s16 i;

    D_ovl3_8023AA40 = destinations;
    for (i = 0; i != 6; i++) {
        get_rom_asset_allocation_size(D_800C2064[i], &asset);
        load_rom_asset_payload(D_ovl3_8023AA40[i], &asset);
    }
}

void func_ovl3_8021342C(u8* destination) {
    RomAssetLoad asset;

    D_ovl3_8023AA44 = destination;
    get_rom_asset_allocation_size(0x8E9, &asset);
    load_rom_asset_payload(D_ovl3_8023AA44, &asset);
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_80213464);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_80213558);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_80213748);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_802144E4);

void func_ovl3_802149A8(Gfx** gfxp, s32 arg1, s32 arg2, s32 arg3) {
    Gfx* gfx = *gfxp;

    if (arg3 <= 0) {
        arg3 = 1;
    }
    if (arg3 >= 4) {
        arg3 = 3;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, (u8*)(((u32)(D_ovl3_8023AA4C + 8)) & ~7));

    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                       (u8*)(((u32)(D_ovl3_8023AA4C + (((arg3 - 1) << 10) + 0x208))) & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x1FF, 0x200);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 4, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 31 << G_TEXTURE_IMAGE_FRAC,
                   31 << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);

    gSPScisTextureRectangle(gfx++, arg1 << 2, arg2 << 2, (arg1 + 0x18) << 2,
                            (arg2 + 0x18) << 2, 0, 0, 0, 0x555, 0x555);
    gDPPipeSync(gfx++);

    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                       (u8*)(((u32)(D_ovl3_8023AA4C + ((arg3 << 10) + 0xE08))) & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x1FF, 0x200);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 4, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 31 << G_TEXTURE_IMAGE_FRAC,
                   31 << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);

    gSPScisTextureRectangle(gfx++, (arg1 + 0x10) << 2, arg2 << 2, (arg1 + 0x28) << 2,
                            (arg2 + 0x18) << 2, 0, 0, 0, 0x555, 0x555);
    gDPPipeSync(gfx++);

    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_8021508C);

/* Save block: s32[6][3] asset row (-1 = empty) and column, indexed [group][slot]. */
typedef struct {
    u8 padding[0xE08];
    s32 table_row[6][3];
    s32 table_col[6][3];
} SaveSlotKeys;

extern u8* func_8005AF50(void);
extern u16 D_ovl3_802308A4[];
extern s16 D_ovl3_802308C8[][4];
extern u8 D_ovl3_8023BFF0[];
extern void* tile_palette_table[];

void func_ovl3_8021585C(void) {
    RomAssetLoad asset;
    RomLoadSlot slot;
    SaveSlotKeys* save;
    s32 i;
    s32 g;
    s32 j;
    s32 key;
    s32 row;
    s32 col;

    save = (SaveSlotKeys*)func_8005AF50();
    for (i = 0; i != 18; i++) {
        get_rom_asset_allocation_size(D_ovl3_802308A4[i], &asset);
        load_rom_asset_payload(tile_palette_table[i], &asset);
    }
    for (g = 0; g != 6; g++) {
        for (j = 0; j != 3; j++) {
            key = save->table_row[g][j];
            if (key != -1) {
                row = key & ((s32) ~key >> 0x1F);
                if (row >= 18) {
                    row = 17;
                }
                col = save->table_col[g][j];
            } else {
                row = 0;
                col = 0;
            }
            open_rom_asset_slot(D_ovl3_802308C8[row][col], &slot);
            read_rom_load_slot(&D_ovl3_8023BFF0[(g * 3 + j) << 9],
                               get_rom_load_slot_size(&slot), &slot);
        }
    }
}
