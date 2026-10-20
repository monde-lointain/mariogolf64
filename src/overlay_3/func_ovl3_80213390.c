#include "common.h"
#include "../main/rom_asset.h"
#include "../main/terrain.h"

extern u8* D_ovl3_8023AA44;
extern u8* D_ovl3_8023AA4C;
extern u8* D_ovl3_8023AA48;
extern u8* D_ovl3_8023AA50;
extern s32 D_ovl3_80230130;
extern s32 D_ovl3_80230134;
extern s32 D_ovl3_80230138;

extern u16 D_800C2064[];
extern void** D_ovl3_8023AA40;
extern s32 D_ovl3_8023013C[];
extern char D_ovl3_8022BF70[];
extern char D_ovl3_802318F8[];
extern char D_80105118[];
extern u8* func_8005AF74(void);
extern s32 func_ovl3_80209478(void);
extern s32 func_800959F8(void);
extern void func_8009226C(s32 arg0);
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);
extern s32 D_800C2000;
extern s32 D_800C2004;
extern s32 current_game_mode;

void func_80071608(Gfx** dl);
void func_80073F24(Gfx** dl, s16 x, s16 y, u8* str);
void func_80074CA8(Gfx** pgfx, s32 r, s32 g, s32 b);

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

void func_ovl3_80213464(u8* destination, s16 mode) {
    RomAssetLoad asset;

    switch (mode) {
    case 1:
        D_ovl3_8023AA44 = (u8*)g_terrain_tile_cache;
        get_rom_asset_allocation_size(0x8E9, &asset);
        load_rom_asset_payload(D_ovl3_8023AA44, &asset);
        break;
    case 2:
        D_ovl3_8023AA48 = destination;
        break;
    case 3:
        D_ovl3_8023AA4C = destination;
        get_rom_asset_allocation_size(0x688, &asset);
        load_rom_asset_payload(D_ovl3_8023AA4C, &asset);
        break;
    case 4:
        D_ovl3_8023AA50 = destination;
        get_rom_asset_allocation_size(0x720, &asset);
        load_rom_asset_payload(D_ovl3_8023AA50, &asset);
        D_ovl3_80230130 = 0;
        D_ovl3_80230134 = 0;
        D_ovl3_80230138 = 0;
        break;
    }
}

void func_ovl3_80213558(void) {
    u8* state;
    s32 count;
    s32 value;
    s32 shifted;
    s32 dir;
    s32 step;

    state = func_8005AF74();
    count = func_ovl3_80209478();
    if (func_800959F8() == 1) {
        if (D_800C2004 & 0x4000) {
            func_8009226C(9);
            play_sound_effect(0x5F, 0xF, 0x7F);
            func_8009226C(9);
            current_game_mode = 9;
            return;
        }
        if (((s8*)state)[0x34] == 0) {
            if (D_800C2000 & 0x100) {
                if (count >= 2) {
                    play_sound_effect(0x5C, 0xF, 0x64);
                    state[0x33]++;
                    if (count - 1 < (s8)state[0x33]) {
                        state[0x33] = 0;
                    }
                    state[0x34] = (s8)(-90 / count);
                }
            } else if (D_800C2000 & 0x200) {
                if (count >= 2) {
                    play_sound_effect(0x5C, 0xF, 0x64);
                    state[0x33]--;
                    if ((s8)state[0x33] < 0) {
                        state[0x33] = count - 1;
                    }
                    state[0x34] = (s8)(90 / count);
                }
            }
        }
        shifted = state[0x34] << 24;
        value = shifted >> 24;
        if (value < 0) {
            value = -value;
        }
        if (value != 0) {
            dir = (shifted < 0) ? -1 : 1;
            step = 0x12 / count;
            if (step == 0) {
                step = 1;
            }
            value = state[0x34] - dir * step;
            state[0x34] = value;
            if (((s8)value < 0 ? -1 : 1) != dir) {
                state[0x34] = 0;
            }
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_80213748);

void func_ovl3_802144E4(Gfx** gfxp, s32 arg1, s32 arg2, s32 arg3, u8 arg4) {
    Gfx* gfx = *gfxp;

    if (arg3 < 0) {
        arg3 = 0;
    }
    if (arg3 >= 0x12) {
        arg3 = 0x11;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);

    if (arg4 == 0xA) {
        gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, 10, 10, 10, 255);
    } else {
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    }

    gDPLoadTLUT_pal256(gfx++, (u8*)(((u32)(D_ovl3_8023AA44 + 8)) & ~7));

    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                       (u8*)(((u32)(D_ovl3_8023AA44 + ((D_ovl3_8023013C[arg3] * 0x300) + 0x208))) & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x17F, 0x200);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 4, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 31 << G_TEXTURE_IMAGE_FRAC,
                   23 << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);

    gSPScisTextureRectangle(gfx++, arg1 << 2, arg2 << 2, (arg1 + 0x20) << 2,
                            (arg2 + 0x18) << 2, 0, 0, 0, 0x400, 0x400);
    gDPPipeSync(gfx++);

    if (arg4 == 0) {
        func_80071608(&gfx);
        func_80074CA8(&gfx, 0xFF, 0xFF, 0xFF);
        sprintf(D_80105118, D_ovl3_802318F8, D_ovl3_8022BF70 + D_ovl3_8023013C[arg3] * 12);
        func_80073F24(&gfx, arg1 + 0x18, arg2 + 0xC, (u8*)D_80105118);
        gDPPipeSync(gfx++);
    }

    *gfxp = gfx;
}

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
