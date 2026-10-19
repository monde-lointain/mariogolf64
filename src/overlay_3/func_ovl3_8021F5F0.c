#include "common.h"
#include "../main/rom_asset.h"
#include "../main/session.h"

extern u8* func_8005AF50(void);
extern u8* func_8005AF74(void);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_8021F5F0);

extern u16 D_800C2064[];
extern void** D_ovl3_8023B680;

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

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80220A30);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80220BB0);

extern s16 D_ovl3_8023B684[];

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

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80220DF8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80220E28);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80220E34);

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

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80221EBC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80222038);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_8022209C);

typedef struct {
    u8 unknown_00[0x43];
    s8 slot_index;
} ContinueSlotSelection;

s32 func_ovl3_80222C94(void) {
    SaveContinueView* save = (SaveContinueView*)func_8005AF50();
    ContinueSlotSelection* selection = (ContinueSlotSelection*)func_8005AF74();
    ContinueSlot* slot = &save->continue_data.slots[selection->slot_index];
    return -(slot->valid == 0);
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80222CDC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80222F2C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_802230C0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80224AFC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80225358);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80225960);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80226190);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_8022644C);
