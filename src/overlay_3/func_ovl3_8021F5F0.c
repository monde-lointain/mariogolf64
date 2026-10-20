#include "common.h"
#include "../main/rom_asset.h"
#include "../main/session.h"

extern u8* func_8005AF50(void);
extern u8* func_8005AF74(void);
extern void func_8005DF54(u8*, s32);
extern void initialize_save_sram(void);
extern void flag_set(s32 flag);
extern s8 D_801B60B8;
extern GameModeRecord D_800FEDD8[];
extern void* D_ovl3_8023B670;
extern void* D_ovl3_8023B674;
extern s32 D_ovl3_8023B678;
extern void* D_ovl3_8023B67C;
extern s32 D_ovl3_80230A90;
extern s32 D_ovl3_80230A94;
extern s32 D_ovl3_80230A98;

typedef struct {
    u16 unknown_00;
    u16 asset_index;
} OverlayAssetEntry;

extern void** D_ovl3_8023BD70;
extern void** D_ovl3_8023BD74;
extern s32 D_ovl3_80230B50[];
extern OverlayAssetEntry D_ovl3_80230B5C[];
extern u16 D_ovl3_80230B40[];
extern char D_ovl3_80231A3C[];
extern s8 D_800C1FF4[];
extern s32 D_8012F480[];
extern s32 transfer_continue_slot(s32 id, s32 restore);
extern s32 flag_is_set(s32 flag);
extern void func_80080564(s32 buffer_index, s32 asset_index, s32 rom_slot_offset);

void func_ovl3_8021F5F0(void* arg0, s16 arg1) {
    RomAssetLoad slot;

    switch (arg1) {
    case 1:
        D_ovl3_8023B670 = arg0;
        return;
    case 2:
        D_ovl3_8023B674 = arg0;
        get_rom_asset_allocation_size(0x5F7, &slot);
        load_rom_asset_payload(D_ovl3_8023B674, &slot);
        return;
    case 3:
        D_ovl3_8023B678 = (s32) arg0;
        get_rom_asset_allocation_size(0x70E, &slot);
        load_rom_asset_payload((void*) D_ovl3_8023B678, &slot);
        return;
    case 4:
        D_ovl3_8023B67C = arg0;
        get_rom_asset_allocation_size(0x720, &slot);
        load_rom_asset_payload(D_ovl3_8023B67C, &slot);
        D_ovl3_80230A90 = 0;
        D_ovl3_80230A94 = 0;
        D_ovl3_80230A98 = 0;
        return;
    }
}

extern u16 D_800C2064[];
extern void** D_ovl3_8023B680;
extern void* D_801052C4[];
extern u8* D_ovl3_8023BD8C[];
extern u8 D_ovl3_80230B4E;

void func_ovl3_8021F6E0(void** destinations) {
    RomAssetLoad asset;
    s16 i;

    D_ovl3_8023B680 = destinations;
    for (i = 0; i != 6; i++) {
        get_rom_asset_allocation_size(D_800C2064[i], &asset);
        load_rom_asset_payload(D_ovl3_8023B680[i], &asset);
    }
    get_rom_asset_allocation_size(0x944, &asset);
    load_rom_asset_payload(D_ovl3_8023B680[6], &asset);
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_8021F79C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_8021FC5C);

/* Save block view: supershot records start at 0x1DF0 (see insert_supershot_record). */
typedef struct {
    u8 unknown_0000[0x1DF0];
    SupershotRecord records[20];
} SaveSupershotView;

extern u16 D_ovl3_802308A4[];
extern s16 D_ovl3_802308C8[][4];
extern u8 D_ovl3_8023BFF0[];
extern u8 D_ovl3_8023E3F0[];
extern void* tile_palette_table[];
extern s16 D_ovl3_8023B684[];

typedef struct {
    u8 unknown_00[0x42];
    s8 unknown_42;
    s8 slot_index;
} ContinueSlotSelection;

s16 func_ovl3_80220E28(SupershotRecord* record);

void func_ovl3_80220A30(void) {
    SaveSupershotView* save;
    ContinueSlotSelection* selection;
    s8 i;

    save = (SaveSupershotView*)func_8005AF50();
    selection = (ContinueSlotSelection*)func_8005AF74();
    initialize_save_sram();
    func_8005DF54((u8*)save, 0);
    for (i = 0; i != 5; i++) {
        D_ovl3_8023B684[i] = -1;
    }
    for (i = 0; i != 5; i++) {
        if (func_ovl3_80220E28(&save->records[i]) != 0) {
            D_ovl3_8023B684[i] = i;
        } else {
            D_ovl3_8023B684[i] = -1;
        }
    }
    if (D_ovl3_8023B684[selection->unknown_42] == -1) {
        for (i = 0; i != 5; i++) {
            if (D_ovl3_8023B684[i] != -1) {
                selection->unknown_42 = i;
            }
        }
    }
}

void func_ovl3_80220BB0(void) {
    RomAssetLoad asset;
    RomLoadSlot slot;
    s32 i;
    SaveSupershotView* save;
    s16 record_index;
    s32 row;
    s32 col;

    save = (SaveSupershotView*)func_8005AF50();
    for (i = 0; i != 18; i++) {
        get_rom_asset_allocation_size(D_ovl3_802308A4[i], &asset);
        load_rom_asset_payload(tile_palette_table[i], &asset);
        open_rom_asset_slot(D_ovl3_802308C8[i][0], &slot);
        read_rom_load_slot(&D_ovl3_8023BFF0[i << 9],
                           get_rom_load_slot_size(&slot), &slot);
    }
    for (i = 0; i != 5; i++) {
        record_index = D_ovl3_8023B684[i];
        if (record_index != -1) {
            row = (s8)save->records[record_index].character_index;
            col = (s8)save->records[record_index].asset_variant;
            if (row < 0) {
                row = 0;
            }
            if (row >= 0x12) {
                row = 0x11;
            }
            if (col < 0) {
                col = 0;
            }
            if (col >= 4) {
                col = 3;
            }
        } else {
            col = 0;
            row = 0;
        }
        open_rom_asset_slot(D_ovl3_802308C8[row][col], &slot);
        read_rom_load_slot(&D_ovl3_8023E3F0[i << 9],
                           get_rom_load_slot_size(&slot), &slot);
    }
}

s16 func_ovl3_80220D68(s8 arg0, s32 arg1) {
    s32 cur;
    s32 step;
    s32 i;
    s32 idx;

    cur = arg0;
    if (cur >= 0) {
        if (cur < 5) {
            step = ((arg1 & 0xFF) == 0) ? -1 : 1;
            idx = cur + step;
            for (i = 0; i != 4; i++) {
                if (idx >= 5) {
                    idx = 0;
                }
                if (idx < 0) {
                    idx = 4;
                }
                if (D_ovl3_8023B684[idx] != -1) {
                    break;
                }
                idx += step;
            }
            if (i != 4) {
                return idx;
            }
        }
    }
    return -1;
}

s16 func_ovl3_80220DF8(void) {
    return D_ovl3_8023B684[((s8*)func_8005AF74())[0x42]];
}

s16 func_ovl3_80220E28(SupershotRecord* record) {
    return record->validation_flags >> 7;
}

void func_ovl3_80220E34(Gfx** gfxp, s32 arg1, s32 arg2, s32 arg3, u8 arg4) {
    Gfx* gfx = *gfxp;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, (D_ovl3_8023B678 + 8) & ~7);
    gDPLoadTextureTile_4b(gfx++, (D_ovl3_8023B678 + 0x28) & ~7, G_IM_FMT_CI, 136, 16, 0, 0, 135, 15, 0, 0, 0, 0, 0, 0, 0);

    if ((arg4 & 0xFF) == 1) {
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, (arg2 - 5) << 2, arg3 << 2, (arg2 + 0x23) << 2, (arg3 + 0x10) << 2, 0, 0xC00, 0, 0x400, 0x400);
        gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (arg2 + 0x21) << 2, arg3 << 2, (arg2 + 0x31) << 2, (arg3 + 0x10) << 2, 0, arg1 << 9, 0, 0x400, 0x400);
    gDPPipeSync(gfx++);

    *gfxp = gfx;
}

void func_ovl3_802213C8(Gfx** arg0, s32 arg1, s32 arg2, s32 arg3) {
    Gfx* gfx;
    s32 i;
    s32 y;

    gfx = *arg0;
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, (u8*)(((u32)D_ovl3_8023B680[arg1] + 8) & ~7));

    for (i = 0; i != 2; i++) {
        gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                           (u8*)(((u32)D_ovl3_8023B680[arg1] + 0x208 + i * 0x800) & ~7));
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
        gDPLoadSync(gfx++);
        gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x3FF, 0x100);
        gDPPipeSync(gfx++);
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 8, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
        gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 63 << G_TEXTURE_IMAGE_FRAC,
                       31 << G_TEXTURE_IMAGE_FRAC);
        gDPPipeSync(gfx++);
        y = arg3 + i * 12;
        gSPScisTextureRectangle(gfx++, arg2 << G_TEXTURE_IMAGE_FRAC,
                                y << G_TEXTURE_IMAGE_FRAC,
                                (arg2 + 0x20) << G_TEXTURE_IMAGE_FRAC,
                                (y + 0xC) << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE,
                                0, 0, 0x800, 0xAAA);
        gDPPipeSync(gfx++);
    }

    *arg0 = gfx;
}

s32 func_ovl3_8022181C(void) {
    u8* ret = func_8005AF50();
    s32 count = 0, i = 0, n = 6;
    u8* base = ret;
    do {
        u32 p = (u32)base + 0x1320;
        u32 end = n + p;
        do {
            count += (*(s8*)p > 0);
            p++;
        } while (p != end);
        base += 6;
        i++;
    } while (i != n);
    if (count >= 0x14) {
        return 6;
    }
    if (count >= 0x10) {
        return 5;
    }
    if (count >= 0xC) {
        return 4;
    }
    if (count >= 8) {
        return 3;
    }
    if (count >= 4) {
        return 2;
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_802218C0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80221B20);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80221CC0);

void func_ovl3_80221EBC(void** arg0, u8 arg1) {
    RomAssetLoad asset;
    u8* save;
    s32* slots;
    s32 i;
    s32 idx;

    save = func_8005AF74();
    switch (arg1) {
    case 1:
        D_ovl3_8023BD70 = arg0;
        i = 0;
        {
            OverlayAssetEntry* table = D_ovl3_80230B5C;
            for (; i != 3; i++) {
                idx = D_ovl3_80230B50[i];
                if (idx == -1) {
                    idx = 0;
                }
                get_rom_asset_allocation_size(table[idx].asset_index, &asset);
                load_rom_asset_payload(D_ovl3_8023BD70[i], &asset);
            }
        }
        get_rom_asset_allocation_size(0x70D, &asset);
        load_rom_asset_payload(D_ovl3_8023BD70[3], &asset);
        return;
    case 2:
        D_ovl3_8023BD74 = arg0;
        slots = D_ovl3_80230B50;
        {
            OverlayAssetEntry* table = D_ovl3_80230B5C;
            for (i = 0; i != 2; i++) {
                idx = slots[(s8)save[0x43]];
                if (idx == -1) {
                    idx = 0;
                }
                get_rom_asset_allocation_size(table[idx].asset_index, &asset);
                load_rom_asset_payload(D_ovl3_8023BD74[i], &asset);
            }
        }
        return;
    }
}

extern void* D_ovl3_8023BD78;
extern void* D_ovl3_8023BD7C;

void func_ovl3_80222038(void* arg0, u8 arg1) {
    RomAssetLoad asset;

    switch (arg1) {
    case 1:
        D_ovl3_8023BD78 = arg0;
        return;
    case 2:
        D_ovl3_8023BD7C = arg0;
        get_rom_asset_allocation_size(0x528, &asset);
        load_rom_asset_payload(D_ovl3_8023BD7C, &asset);
        return;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_8022209C);

s32 func_ovl3_80222C94(void) {
    SaveContinueView* save = (SaveContinueView*)func_8005AF50();
    ContinueSlotSelection* selection = (ContinueSlotSelection*)func_8005AF74();
    ContinueSlot* slot = &save->continue_data.slots[selection->slot_index];
    return -(slot->valid == 0);
}

s32 func_ovl3_80222CDC(void) {
    ContinueSlot* slot;
    GameModeRecord* modes;
    PlayerState* recs;
    u8* save;
    s32 mode;
    s32 count;
    s32 i;

    save = func_8005AF50();
    mode = (s8)func_8005AF74()[0x43];
    count = 0;
    switch (mode) {
    case 0:
        slot = (ContinueSlot*)(save + 0xD8);
        modes = ((SessionStorage1*)(save + 0x388))->mode_records;
        recs = ((SessionStorage1*)(save + 0x388))->players;
        count = 1;
        break;
    case 1:
        slot = (ContinueSlot*)(save + 0x38);
        modes = ((SessionStorage2*)(save + 0x218))->mode_records;
        recs = ((SessionStorage2*)(save + 0x218))->players;
        count = 2;
        break;
    case 2:
        slot = (ContinueSlot*)(save + 0x178);
        modes = ((SessionStorage4*)(save + 0x440))->mode_records;
        recs = ((SessionStorage4*)(save + 0x440))->players;
        count = 4;
        break;
    }

    if (slot->valid == 0) {
        return -1;
    }
    if (count < slot->state.active_player_count) {
        return -1;
    }
    count = slot->state.active_player_count;
    for (i = 0; i != count; i++) {
        player_records[i] = recs[i];
        D_800FEDD8[i] = modes[i];
    }
    scenario_state_id = slot->state;
    scenario_state_id.state_id = 1;
    flag_set(0x1E);
    hole_environment = slot->environment;
    D_801B60B8 = 2;
    return 0;
}

s32 func_ovl3_80222F2C(void) {
    SaveContinueView* save;
    ContinueSlotSelection* selection;
    PlayerState* src;
    s32 i;
    u16 val;
    s32 char_index;

    save = (SaveContinueView*)func_8005AF50();
    selection = (ContinueSlotSelection*)func_8005AF74();
    if (D_ovl3_80230B50[selection->slot_index] == -1) {
        return -1;
    }
    if (transfer_continue_slot(selection->slot_index, 1) == -1) {
        return -1;
    }
    src = save->continue_data.players[selection->slot_index];
    i = 0;
    do {
        val = D_ovl3_80230B40[i];
        D_800C1FF4[i] = (s8)val;
        player_records[i].unknown_27[0] = (s8)val;
        sprintf((char*)&player_records[i], D_ovl3_80231A3C, &src[i]);
        char_index = src[i].character_index;
        D_8012F480[i] = char_index;
        player_records[i].character_index = char_index;
        func_80080564(i, char_index, (s8)player_records[i].asset_variant);
        i++;
    } while (i != 4);
    if (flag_is_set(0x5D) == 0) {
        memset(&save->continue_data.slots[selection->slot_index], 0, sizeof(ContinueSlot));
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_802230C0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80224AFC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80225358);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80225960);

void func_ovl3_80226190(Gfx** gfxp) {
    Gfx* gfx;
    s32 i;

    gfx = *gfxp;
    func_8005AF50();
    func_8005AF74();
    gDPLoadTLUT_pal256(gfx++, (u8*)((((u32)D_ovl3_8023BD8C[D_ovl3_80230B4E]) + 8) & ~7));
    for (i = 0; i != 2; i++) {
        gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                           (u8*)((((u32)D_ovl3_8023BD8C[D_ovl3_80230B4E]) + 0x208 + i * 0x800) & ~7));
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
        gDPLoadSync(gfx++);
        gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x3FF, 0x100);
        gDPPipeSync(gfx++);
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 8, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
        gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 63 << G_TEXTURE_IMAGE_FRAC,
                       31 << G_TEXTURE_IMAGE_FRAC);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, 0xA0, 0x2D8 + i * 0x48, 0x170, 0x320 + i * 0x48,
                                G_TX_RENDERTILE, 0, 0, 0x4EC, 0x71C);
        gDPPipeSync(gfx++);
    }

    *gfxp = gfx;
}

void func_ovl3_8022644C(Gfx** gfxp) {
    Gfx* gfx;
    u32 buf;
    s32 i;

    gfx = *gfxp;
    func_8005AF50();
    func_8005AF74();
    buf = (u32)D_801052C4[0];
    gDPLoadTLUT_pal256(gfx++, (u8*)(buf & ~7));
    for (i = 0; i != 2; i++) {
        gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                           (u8*)((((u32)D_ovl3_8023BD8C[D_ovl3_80230B4E]) + i * 0x800) & ~7));
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
        gDPLoadSync(gfx++);
        gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x3FF, 0x100);
        gDPPipeSync(gfx++);
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 8, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
        gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 63 << G_TEXTURE_IMAGE_FRAC,
                       31 << G_TEXTURE_IMAGE_FRAC);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, 40 << G_TEXTURE_IMAGE_FRAC,
                                (182 + i * 18) << G_TEXTURE_IMAGE_FRAC,
                                92 << G_TEXTURE_IMAGE_FRAC,
                                (200 + i * 18) << G_TEXTURE_IMAGE_FRAC,
                                G_TX_RENDERTILE, 0, 0, 0x4EC, 0x71C);
        gDPPipeSync(gfx++);
    }

    *gfxp = gfx;
}
