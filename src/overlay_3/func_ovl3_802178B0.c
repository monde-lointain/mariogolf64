#include "common.h"
#include "../main/rom_asset.h"

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_802178B0);

typedef struct {
    u8 pad_00[0xD];
    u8 unk_0D;
    s8 slots[8];
    u8 pad_16[0x29 - 0x16];
    u8 unk_29;
    u8 unk_2A;
} Ovl3State;

extern u8* func_8005AF74(void);
extern s32 D_80105B68;
extern s8 D_801B60BA[8];
extern u8 D_ovl3_802304BC[8][8];
extern u8 D_ovl3_802304F4[];
extern s16 D_ovl3_8023BFDC[4];

void func_ovl3_80217B88(s32 arg0) {
    s8 mode = arg0;
    Ovl3State* state = (Ovl3State*)func_8005AF74();
    s16 i;

    if ((u8)arg0 < 7) {
        if ((arg0 << 24) == 0) {
            if (D_80105B68 == 4) {
                mode = 1;
            }
        }
        if ((s8)mode == 5) {
            if (D_80105B68 == 4) {
                mode = 6;
            }
        }
        for (i = 0; i != 8; i++) {
            u8 v = D_ovl3_802304BC[(s8)mode][i];
            state->slots[i] = v;
            D_801B60BA[i] = D_ovl3_802304F4[(s8)v];
        }
    } else {
        for (i = 0; i != 8; i++) {
            state->slots[i] = -1;
            D_801B60BA[i] = -1;
        }
    }

    state->unk_0D = 0;
    for (i = 0; i != 4; i++) {
        D_ovl3_8023BFDC[i] = -(i >= D_80105B68);
    }
    state->unk_29 = 0;
    state->unk_2A = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_80217D20);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021826C);

typedef struct {
    u16 unk0;
    u16 unk2;
} AssetEntry;

typedef struct {
    u8 unk0;
    u8 unk1;
} AssetSelector;

typedef struct {
    s8 unk00;
    u8 unk01[0x67];
} Ovl3ModeRecord; /* 0x68 */

extern u8 D_801B5634;
extern s32 D_ovl3_8022F8C0;
extern u8 D_ovl3_8022F8C4;
extern u8 D_ovl3_8022F8C5;
extern s32 D_ovl3_8023B350[2];
extern AssetEntry D_ovl3_802303CC[][2];
extern AssetSelector D_ovl3_80230400[][6];
extern char D_ovl3_802319E0[];
extern char D_ovl3_8022BF70[];
extern s32 D_ovl3_80230A48[];
extern char D_80105118[];
extern u8* func_8005AF50(void);
extern s32 func_800959F8(void);
extern s32 func_8006AD94(void);
extern s32 func_8006ADA0(void);
extern void func_8006AD1C(s32 table_index);
extern void func_8006ADF8(s32 arg);
extern void func_8009226C(s32 arg0);
extern void flag_set(s32 flag);
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);
extern void func_800719A0(s32 x, s32 y, s32 run, s32 size, s32 index, s32 flags,
                          u8 red, u8 green, u8 blue, u8 alpha);
extern s16 func_ovl3_8021E340(s8 x, s8 y, s32 dir);
extern s16 func_ovl3_8021E468(void);
extern s32 D_800C2000;
extern s32 D_800C2004;
extern s32 D_800FE334;
extern s8 rumble_disable_flag;
extern u8 D_801B9410;
extern u8 D_801052E8;
extern s32 current_game_mode;

void func_80071608(Gfx** dl);
void func_80073F24(Gfx** dl, s16 x, s16 y, u8* str);
void func_80074CA8(Gfx** pgfx, s32 r, s32 g, s32 b);

void func_ovl3_80219460(Gfx** gfxp, s32 x, s32 y, s32 width, s32 scale) {
    RomAssetLoad asset;
    Gfx* gfx = *gfxp;
    u8* state = func_8005AF74();
    u8 set_index;
    AssetSelector* selectors;
    AssetEntry* entries;
    s32 buf;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    buf = D_ovl3_8023B350[D_ovl3_8022F8C4];
    gDPLoadTLUT_pal16(gfx++, 0, (u8*)((buf + 8) & ~7));
    gDPLoadTextureTile_4b(gfx++, (u8*)((buf + 0x28) & ~7), G_IM_FMT_CI, 24, 24,
                          0, 0, 23, 23, 0, 0, 0, 0, 0, 0, 0);
    gSPScisTextureRectangle(gfx++, x << 2, y << 2, (x + width) << 2,
                            (y + width) << 2, 0, 0, 0, scale, scale);
    gDPPipeSync(gfx++);

    D_ovl3_8022F8C5 = D_ovl3_8022F8C5 + 1;
    if ((s8) D_ovl3_8022F8C5 == 4) {
        D_ovl3_8022F8C5 = 0;
        D_ovl3_8022F8C0 = D_ovl3_8022F8C0 + 1;
        if (D_ovl3_8022F8C0 >= 2) {
            D_ovl3_8022F8C0 = 0;
        }
        D_ovl3_8022F8C4 = D_ovl3_8022F8C4 == 0;
        selectors = D_ovl3_80230400[D_801B5634];
        set_index = selectors[(s8) state[0xD]].unk1;
        entries = D_ovl3_802303CC[set_index];
        get_rom_asset_allocation_size(entries[D_ovl3_8022F8C0].unk2, &asset);
        load_rom_asset_payload((void*)D_ovl3_8023B350[D_ovl3_8022F8C4],
                               &asset);
    }

    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_80219948);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_80219BD4);

extern u8* D_ovl3_8023B364;
extern s16 D_ovl3_8023B640[4][5];

void func_ovl3_8021B1E0(Gfx** gfxp, s32 x, s32 y, u8 idx) {
    Gfx* gfx = *gfxp;
    s32 texOff = idx * 0x500 + 0x28;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, (u8*)(((u32)D_ovl3_8023B364 + 8) & ~7));
    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                       (u8*)(((u32)D_ovl3_8023B364 + texOff) & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0,
               0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 639, 256);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_4b, 8, 0, G_TX_RENDERTILE, 0, 0, 0, 0,
               0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 127 << G_TEXTURE_IMAGE_FRAC,
                   19 << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (x - 0x40) << G_TEXTURE_IMAGE_FRAC,
                            y << G_TEXTURE_IMAGE_FRAC,
                            (x + 0x40) << G_TEXTURE_IMAGE_FRAC,
                            (y + 0x14) << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE,
                            0, 0, 0x400, 0x400);
    gDPPipeSync(gfx++);
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021B618);

extern u8* D_ovl3_8023B36C;
extern s32 D_ovl3_8023073C[];
extern s32 D_ovl3_80230760[];
extern s32 D_ovl3_80230784[];

/* Draws nine 152x13 CI8 strips and slides each one horizontally: a strip
 * whose delay timer has run out moves by its speed toward the edge `mode`
 * selects (1/2 toward 0, 3/4 toward +-320). */
void func_ovl3_8021B87C(Gfx** gfxp, u8 mode) {
    Gfx* gfx = *gfxp;
    s32 i;
    s32 w;
    s32 h;
    s32* xp;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPLoadTLUT_pal256(gfx++, (u8*)(((u32)D_ovl3_8023B36C + 8) & ~7));
    w = 152;
    h = 13;
    for (i = 0; i != 9; i++) {
        xp = &D_ovl3_80230784[i];
        gDPPipeSync(gfx++);
        gDPLoadTextureBlock(gfx++, (u8*)(((u32)D_ovl3_8023B36C + 0x208 + h * (i * w)) & ~7),
                            G_IM_FMT_CI, G_IM_SIZ_8b, 152, h, 0, 0, 0, 0, 0, 0, 0);
        gDPPipeSync(gfx++);
        if (D_ovl3_8023073C[i] > 0) {
            D_ovl3_8023073C[i]--;
        } else {
            switch (mode) {
            case 1:
                *xp -= D_ovl3_80230760[i];
                if (*xp <= 0) {
                    *xp = 0;
                }
                break;
            case 2:
                *xp += D_ovl3_80230760[i];
                if (*xp > 0) {
                    *xp = 0;
                }
                break;
            case 3:
                *xp += D_ovl3_80230760[i];
                if (*xp > 320) {
                    *xp = 320;
                }
                break;
            case 4:
                *xp -= D_ovl3_80230760[i];
                if (*xp < -320) {
                    *xp = -320;
                }
                break;
            }
        }
        gSPScisTextureRectangle(gfx++, D_ovl3_80230784[i] << 2, (i * 27) << 2,
                                (D_ovl3_80230784[i] + 320) << 2, (i * 27 + 27) << 2,
                                G_TX_RENDERTILE, 0, 0, 0x1E6, (h << 10) / 27);
    }
    gDPPipeSync(gfx++);
    *gfxp = gfx;
}

extern s16 D_ovl3_802303A6;
extern s16 D_ovl3_802303A8;
extern u8* D_ovl3_8023B35C;

void func_ovl3_8021BE88(Gfx** gfxp) {
    Gfx* gfx = *gfxp;
    s32 i;
    s32 left;
    s32 flag;

    if (++D_ovl3_802303A8 >= 3) {
        if (++D_ovl3_802303A6 >= 8) {
            D_ovl3_802303A6 = 0;
        }
        D_ovl3_802303A8 = 0;
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, (u8*)(((u32)D_ovl3_8023B35C + 8) & ~7));
    gDPPipeSync(gfx++);
    for (i = 0; i != 2; i++) {
        if (i == 1) {
            gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 16,
                               (u8*)((u32)(D_ovl3_8023B35C + ((D_ovl3_802303A6 << 9) + 0x28)) & ~7));
            gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 2, 0, G_TX_LOADTILE, 0, G_TX_MIRROR, 5, 0,
                       G_TX_MIRROR, 5, 0);
            gDPLoadSync(gfx++);
            gDPLoadTile(gfx++, G_TX_LOADTILE, 0, 0, 62, 124);
            gDPPipeSync(gfx++);
            gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_4b, 2, 0, G_TX_RENDERTILE, 0, G_TX_MIRROR, 5, 0,
                       G_TX_MIRROR, 5, 0);
            gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 31 << G_TEXTURE_IMAGE_FRAC,
                           31 << G_TEXTURE_IMAGE_FRAC);
            left = 212;
        } else {
            gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 16,
                               (u8*)((u32)(D_ovl3_8023B35C + ((D_ovl3_802303A6 << 9) + 0x28)) & ~7));
            gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 2, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
            gDPLoadSync(gfx++);
            gDPLoadTile(gfx++, G_TX_LOADTILE, 0, 0, 62, 124);
            gDPPipeSync(gfx++);
            gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_4b, 2, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
            gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 31 << G_TEXTURE_IMAGE_FRAC,
                           31 << G_TEXTURE_IMAGE_FRAC);
            left = 76;
        }
        flag = (i == 1);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, left << G_TEXTURE_IMAGE_FRAC, 122 << G_TEXTURE_IMAGE_FRAC,
                                (left + 32) << G_TEXTURE_IMAGE_FRAC, 154 << G_TEXTURE_IMAGE_FRAC,
                                G_TX_RENDERTILE, flag << 10, 0, 0x400, 0x400);
        gDPPipeSync(gfx++);
    }
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021C200);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021C67C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021CBDC);

extern u16 D_800C2064[];
extern void** D_ovl3_8023B630;

void func_ovl3_8021CF10(void** destinations) {
    RomAssetLoad asset;
    s16 i;

    D_ovl3_8023B630 = destinations;
    for (i = 0; i != 6; i++) {
        get_rom_asset_allocation_size(D_800C2064[i], &asset);
        load_rom_asset_payload(D_ovl3_8023B630[i], &asset);
    }
}

typedef u8 Ovl3AssetBlock[0x258];

extern Ovl3AssetBlock* D_ovl3_8023B634;
extern Ovl3AssetBlock* D_ovl3_8023B638;
extern Ovl3AssetBlock* D_ovl3_8023B63C;

void func_ovl3_8021CFAC(Ovl3AssetBlock* arg0, s16 arg1) {
    RomAssetLoad asset;

    switch (arg1) {
    case 1:
        D_ovl3_8023B634 = arg0;
        break;
    case 2:
        D_ovl3_8023B638 = arg0;
        get_rom_asset_allocation_size(0x6EF, &asset);
        load_rom_asset_payload(D_ovl3_8023B638, &asset);
        break;
    case 3:
        D_ovl3_8023B63C = arg0;
        get_rom_asset_allocation_size(0x82D, &asset);
        load_rom_asset_payload(D_ovl3_8023B63C[0], &asset);
        get_rom_asset_allocation_size(0x82F, &asset);
        load_rom_asset_payload(D_ovl3_8023B63C[1], &asset);
        get_rom_asset_allocation_size(0x82E, &asset);
        load_rom_asset_payload(D_ovl3_8023B63C[2], &asset);
        get_rom_asset_allocation_size(0x830, &asset);
        load_rom_asset_payload(D_ovl3_8023B63C[3], &asset);
        break;
    }
}

void func_ovl3_8021D0B8(void) {
    u8* save;
    s8* state;
    s32 i;
    s16 idx;
    s16 ret;
    s32 dir;
    s32 col;
    Ovl3ModeRecord* rec;

    save = func_8005AF50();
    state = (s8*)func_8005AF74();
    if (func_800959F8() == 1) {
        if (func_8006AD94() != 0 && D_801B9410 == 0) {
            if (func_8006ADA0() == 0) {
                current_game_mode = 0x13;
                D_800FE334 = 0;
                rumble_disable_flag = 1;
                flag_set(0x1E);
                i = 0;
                do {
                    func_800719A0(0, 0, -1, 1, i, 0, 0xFF, 0xFF, 0xFF, 0xFF);
                    i++;
                } while (i != 4);
            }
        } else if (func_8006AD94() == 0 || D_801B9410 != 1) {
            if (D_800C2004 & 0x8000) {
                if (func_ovl3_8021E468() != -1) {
                    play_sound_effect(0x5D, 0xF, 0x50);
                    D_801B9410 = 0;
                    col = state[0x39];
                    idx = D_ovl3_8023B640[state[0x3A]][col];
                    rec = (Ovl3ModeRecord*)(save + 0x1608);
                    func_8006AD1C(rec[idx].unk00);
                    func_8006ADF8(1);
                    D_801052E8 = 0;
                }
            } else if (D_800C2004 & 0x4000) {
                func_8009226C(9);
                play_sound_effect(0x5F, 0xF, 0x7F);
                func_8009226C(9);
                current_game_mode = 9;
                i = 0;
                do {
                    func_800719A0(0, 0, -1, 1, i, 0, 0xFF, 0xFF, 0xFF, 0xFF);
                    i++;
                } while (i != 4);
                return;
            } else {
                if (D_800C2000 & 0x800) {
                    dir = 2;
                    ret = func_ovl3_8021E340(state[0x39], state[0x3A], dir);
                    if (ret != -1) {
                        play_sound_effect(0x5C, 0xF, 0x64);
                        state[0x3A] = (s8)ret;
                    }
                } else if (D_800C2000 & 0x400) {
                    dir = 3;
                    ret = func_ovl3_8021E340(state[0x39], state[0x3A], dir);
                    if (ret != -1) {
                        play_sound_effect(0x5C, 0xF, 0x64);
                        state[0x3A] = (s8)ret;
                    }
                } else if (D_800C2000 & 0x200) {
                    dir = 0;
                    ret = func_ovl3_8021E340(state[0x39], state[0x3A], dir);
                    if (ret != -1) {
                        play_sound_effect(0x5C, 0xF, 0x64);
                        state[0x39] = (s8)ret;
                    }
                } else if (D_800C2000 & 0x100) {
                    dir = 1;
                    ret = func_ovl3_8021E340(state[0x39], state[0x3A], dir);
                    if (ret != -1) {
                        play_sound_effect(0x5C, 0xF, 0x64);
                        state[0x39] = (s8)ret;
                    }
                }
            }
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021D3AC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021E01C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021E340);

s16 func_ovl3_8021E468(void) {
    s8* temp_v0;

    temp_v0 = (s8*)func_8005AF74();
    return D_ovl3_8023B640[temp_v0[0x3A]][temp_v0[0x39]];
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021E4B0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021E97C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021EF64);

void func_ovl3_8021F124(Gfx** gfxp, u8* image, u8* tlut, s32 x, s32 y, s16 index, u8 mode) {
    Gfx* gfx = *gfxp;

    if (index < 0) {
        index = 0;
    }
    if (index >= 0x12) {
        index = 0x11;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    if (mode == 0xA) {
        gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, 10, 10, 10, 255);
    } else {
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    }
    gDPLoadTLUT_pal256(gfx++, (u8*)((u32)tlut & ~7));
    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1, (u8*)((u32)image & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x17F, 0x200);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 4, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 31 << G_TEXTURE_IMAGE_FRAC, 23 << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, x << 2, y << 2, (x + 0x20) << 2, (y + 0x18) << 2, 0, 0, 0, 0x400, 0x400);
    gDPPipeSync(gfx++);

    if (mode == 0) {
        if (index < 0xE) {
            func_80071608(&gfx);
            func_80074CA8(&gfx, 0xFF, 0xFF, 0xFF);
            sprintf(D_80105118, D_ovl3_802319E0, D_ovl3_8022BF70 + D_ovl3_80230A48[index] * 12);
            func_80073F24(&gfx, x + 0x18, y + 0xC, D_80105118);
            gDPPipeSync(gfx++);
        }
    }

    *gfxp = gfx;
}
