#include "common.h"
#include "../main/rom_asset.h"

extern u8* func_8005AF74(void);
extern s32 func_800959F8(void);
extern void func_8009226C(s32 arg0);
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);
extern s32 D_800C2000;
extern s32 D_800C2004;
extern s32 current_game_mode;

extern u8* D_ovl3_8023AC64;
extern u8* D_ovl3_8023AC68;
extern u8* D_ovl3_8023AC6C;
extern u8* D_ovl3_8023AC70;
extern s32 D_ovl3_80230190;
extern s32 D_ovl3_80230194;
extern s32 D_ovl3_80230198;
extern s32 D_ovl3_8023019C;
extern s32 D_ovl3_802301A0;
extern s32 D_ovl3_802301A4;

void func_ovl3_802159C0(u8* arg0, s16 arg1) {
    RomAssetLoad asset;
    u8* state;
    s32 next_a0;
    s32 next_v1;

    state = func_8005AF74();
    switch (arg1) {
    case 1:
        D_ovl3_8023AC64 = arg0;
        get_rom_asset_allocation_size(0x6EF, &asset);
        load_rom_asset_payload(D_ovl3_8023AC64, &asset);
        break;
    case 2:
        D_ovl3_8023AC68 = arg0;
        break;
    case 3:
        D_ovl3_8023AC6C = arg0;
        get_rom_asset_allocation_size(0x94A, &asset);
        load_rom_asset_payload(D_ovl3_8023AC6C, &asset);
        D_ovl3_8023019C = 0;
        D_ovl3_802301A0 = 0;
        D_ovl3_802301A4 = 0;
        break;
    case 4:
        D_ovl3_8023AC70 = arg0;
        get_rom_asset_allocation_size(0x5BE, &asset);
        load_rom_asset_payload(D_ovl3_8023AC70, &asset);
        break;
    }

    if ((s8)state[0x35] < 4 && ((next_a0 = 0xA4), (s8)state[0x36] < 2)) {
        D_ovl3_80230198 = 3;
        next_v1 = 0xAA;
    } else if ((s8)state[0x35] >= 3 && ((next_a0 = 0x44), (s8)state[0x36] < 2)) {
        next_v1 = 0xAA;
        D_ovl3_80230198 = 2;
    } else {
        next_a0 = 0x44;
        if ((s8)state[0x35] < 4) {
            next_v1 = 0x54;
            if ((s8)state[0x36] >= 3) {
                next_a0 = 0xA4;
                D_ovl3_80230198 = 1;
            } else {
                D_ovl3_80230198 = 0;
            }
        } else {
            next_v1 = 0x54;
            D_ovl3_80230198 = 0;
        }
    }
    D_ovl3_80230190 = next_a0;
    D_ovl3_80230194 = next_v1;
}

void func_ovl3_80215B84(void) {
    u8* state;
    u8 unk35;
    u8 unk36;
    u8 unk37;
    u8 unk38;

    state = func_8005AF74();
    if (func_800959F8() == 1) {
        if (D_800C2004 & 0x4000) {
            func_8009226C(9);
            play_sound_effect(0x5F, 0xF, 0x7F);
            func_8009226C(9);
            current_game_mode = 9;
            return;
        }
        unk37 = state[0x37];
        unk38 = state[0x38];
        unk35 = state[0x35];
        unk36 = state[0x36];
        if (D_800C2000 & 0x100) {
            play_sound_effect(0x5C, 0xF, 0x64);
            if (((s8)unk35 == 6) && ((s8)unk37 < 7)) {
                state[0x37]++;
            }
            state[0x35]++;
            if ((s8)state[0x35] >= 7) {
                state[0x35] = 6;
            }
        } else if (D_800C2000 & 0x200) {
            play_sound_effect(0x5C, 0xF, 0x64);
            if ((unk35 == 0) && ((s8)unk37 > 0)) {
                state[0x37]--;
            }
            state[0x35]--;
            if ((s8)state[0x35] < 0) {
                state[0x35] = 0;
            }
        } else if (D_800C2000 & 0x400) {
            play_sound_effect(0x5C, 0xF, 0x64);
            if (((s8)unk36 == 4) && ((s8)unk38 < 0xD)) {
                state[0x38]++;
            }
            state[0x36]++;
            if ((s8)state[0x36] >= 5) {
                state[0x36] = 4;
            }
        } else if (D_800C2000 & 0x800) {
            play_sound_effect(0x5C, 0xF, 0x64);
            if ((unk36 == 0) && ((s8)unk38 > 0)) {
                state[0x38]--;
            }
            state[0x36]--;
            if ((s8)state[0x36] < 0) {
                state[0x36] = 0;
            }
        }
    }
}

typedef struct {
    u8 unkAF0;
    u8 unkAF1;
} Ovl3Cell;

typedef struct {
    u8 pad[0xAF0];
    Ovl3Cell cells[18][18];
} Ovl3Save;

typedef struct {
    u8 pad[0x35];
    s8 unk35;
    s8 unk36;
    u8 unk37;
    u8 unk38;
} Ovl3State;

extern u8* func_8005AF50(void);
extern void func_ovl3_801FB54C(Gfx **, s32);
extern void func_ovl3_802169CC(Gfx **);
extern void func_ovl3_80216C14(Gfx **);
extern void func_ovl3_8021640C(Gfx **, s32, s32);
extern void func_ovl3_80217464(Gfx **, s32, s32);
extern void func_ovl3_802123E0(Gfx **, s32, s32, u8);
extern void func_ovl3_80212FEC(Gfx **, u8 *, s32, s32, s32, s32);
extern void func_ovl3_8021F124(Gfx **, u8 *, u8 *, s32, s32, s16, u8);

extern void *tile_palette_table[];
extern u8 D_ovl3_8023BFF0[];
extern s32 D_ovl3_802301A8[];
extern s32 D_ovl3_8023AC78[];

void func_ovl3_80215DC4(Gfx **arg0) {
    Gfx *gfx;
    Ovl3Save *save;
    Ovl3State *state;
    s16 fp;
    s16 s1;
    s16 s3;
    s16 s5;
    s16 s0;
    s16 s0b;
    s32 a1;

    gfx = *arg0;
    save = (Ovl3Save *)func_8005AF50();
    state = (Ovl3State *)func_8005AF74();

    func_ovl3_801FB54C(&gfx, 0xB);

    gDPSetPrimColor(gfx++, 0, 0, 0x40, 0x40, 0x40, 0xBF);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, 24 << 2, 50 << 2, 302 << 2, 230 << 2, G_TX_RENDERTILE, 0, 0, 0x400, 0x400);
    gDPPipeSync(gfx++);

    func_ovl3_802169CC(&gfx);

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, 0, 1);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

    fp = 0;
    s5 = (s8)state->unk38;
    do {
        s0 = D_ovl3_802301A8[s5];
        a1 = (D_ovl3_8023AC78[s5] == 1) ? 1 : 0xA;
        func_ovl3_8021F124(&gfx, tile_palette_table[s0], D_ovl3_8023BFF0 + (s0 << 9), 0x1C,
                           fp * 0x1E + 0x52, s0, a1);
        s1 = 0;
        s3 = (s8)state->unk37;
        do {
            s32 g0;
            s32 g1;
            s0b = D_ovl3_802301A8[s3];
            a1 = (D_ovl3_8023AC78[s3] != -1) ? 1 : 0xA;
            if (fp == 0) {
                func_ovl3_8021F124(&gfx, tile_palette_table[s0b], D_ovl3_8023BFF0 + (s0b << 9),
                                   s1 * 0x23 + 0x3E, 0x34, s0b, a1);
            }
            g0 = save->cells[s0][s0b].unkAF0 & 0x7F;
            g1 = save->cells[s0][s0b].unkAF1 & 0x7F;
            if (save->cells[s0][s0b].unkAF1 & 0x80) {
                func_ovl3_80217464(&gfx, s1 * 0x23 + 0x3E, fp * 0x1E + 0x55);
            } else {
                if (g0) {
                    func_ovl3_802123E0(&gfx, s1 * 0x23 + 0x43, fp * 0x1E + 0x56, 1);
                } else if (g1) {
                    func_ovl3_802123E0(&gfx, s1 * 0x23 + 0x43, fp * 0x1E + 0x56, 2);
                } else {
                    func_ovl3_802123E0(&gfx, s1 * 0x23 + 0x43, fp * 0x1E + 0x58, 0);
                }
            }
            s1++;
            s3++;
        } while (s1 != 7);
        s5++;
        fp++;
    } while (fp != 5);

    if ((s8)state->unk38 > 0) {
        func_ovl3_80212FEC(&gfx, D_ovl3_8023AC6C, D_ovl3_8023019C, 0, 0x28, 0x58);
    }
    if ((s8)state->unk38 < 0xD) {
        func_ovl3_80212FEC(&gfx, D_ovl3_8023AC6C, D_ovl3_8023019C, 1, 0x28, 0xDE);
    }
    if ((s8)state->unk37 > 0) {
        func_ovl3_80212FEC(&gfx, D_ovl3_8023AC6C, D_ovl3_8023019C, 3, 0x40, 0x44);
    }
    if ((s8)state->unk37 < 7) {
        func_ovl3_80212FEC(&gfx, D_ovl3_8023AC6C, D_ovl3_8023019C, 2, 0x128, 0x44);
    }

    D_ovl3_802301A0 = D_ovl3_802301A0 + 1;
    if (D_ovl3_802301A0 == 4) {
        D_ovl3_802301A0 = 0;
        D_ovl3_8023019C = (D_ovl3_8023019C == 0);
    }

    func_ovl3_80216C14(&gfx);
    func_ovl3_8021640C(&gfx, state->unk35 * 0x23 + 0x4A, state->unk36 * 0x1E + 0x5D);

    gDPPipeSync(gfx++);
    *arg0 = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802159C0", func_ovl3_8021640C);

extern s32 D_ovl3_80230248[][2];

void func_ovl3_802169CC(Gfx **arg0) {
    Gfx *gfx;
    s32 i;

    gfx = *arg0;
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    for (i = 0; i != 0x18; i += 2) {
        gSPScisTextureRectangle(gfx++, D_ovl3_80230248[i][0] * 4,
                                D_ovl3_80230248[i][1] * 4,
                                D_ovl3_80230248[i + 1][0] * 4,
                                D_ovl3_80230248[i + 1][1] * 4,
                                G_TX_RENDERTILE, 0, 0, 0x400, 0x400);
        gDPPipeSync(gfx++);
    }
    *arg0 = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802159C0", func_ovl3_80216C14);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802159C0", func_ovl3_8021730C);

void func_ovl3_80217464(Gfx** gfxp, s32 x, s32 y) {
    Gfx* gfx = *gfxp;

    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, (u8*)(((u32)D_ovl3_8023AC70 + 8) & ~7));
    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                       (u8*)(((u32)D_ovl3_8023AC70 + 0x28) & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0,
               0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 191, 1024);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_4b, 2, 0, G_TX_RENDERTILE, 0, 0, 0, 0,
               0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 31 << G_TEXTURE_IMAGE_FRAC,
                   23 << G_TEXTURE_IMAGE_FRAC);
    gSPScisTextureRectangle(gfx++, x << G_TEXTURE_IMAGE_FRAC,
                            y << G_TEXTURE_IMAGE_FRAC,
                            (x + 0x18) << G_TEXTURE_IMAGE_FRAC,
                            (y + 0x14) << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE,
                            0, 0, 0x555, 0x4CC);
    gDPPipeSync(gfx++);
    *gfxp = gfx;
}

extern u8* func_8005AF50(void);
extern u16 D_ovl3_802308A4[];
extern s16 D_ovl3_802308C8[][4];
extern u8 D_ovl3_8023BFF0[];
extern void* tile_palette_table[];

void func_ovl3_802177D0(void) {
    RomAssetLoad asset;
    RomLoadSlot slot;
    s32 i;

    func_8005AF50();
    for (i = 0; i != 18; i++) {
        get_rom_asset_allocation_size(D_ovl3_802308A4[i], &asset);
        load_rom_asset_payload(tile_palette_table[i], &asset);
        open_rom_asset_slot(D_ovl3_802308C8[i][0], &slot);
        read_rom_load_slot(&D_ovl3_8023BFF0[i << 9],
                           get_rom_load_slot_size(&slot), &slot);
    }
}
