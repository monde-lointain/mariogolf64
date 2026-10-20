#include "common.h"
#include "../main/rom_asset.h"
#include "../main/session.h"
#include "../main/controller.h"

extern s32 func_8005B0A0(s32);
extern u8* func_8005AF50(void);

s32 func_ovl0_801F4A40(s32 arg0) {
    s32 sum;
    s32 i;

    sum = 0;
    for (i = 1; i < arg0 + 1; i++) {
        sum += func_8005B0A0(i);
    }
    return sum;
}

extern s32 D_ovl0_8021FC74;
extern s32 D_ovl0_8021FC80;
void func_ovl0_801F4AEC(void);
void func_ovl0_801F618C(void);

void func_ovl0_801F4AA4(s32 arg0) {
    if (arg0 == -1) {
        func_ovl0_801F618C();
        return;
    }
    func_ovl0_801F4AEC();
    D_ovl0_8021FC80 = 0x140;
    D_ovl0_8021FC74 = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F4AEC);

extern void* heap3_alloc(u32 size);
extern u16 D_800C547C[];
extern s16 D_800C54A0[];
extern u16 D_ovl0_8021FDA6[][2];
extern s32 D_ovl0_80225744;
extern void* D_ovl0_802256C8;
extern void* D_ovl0_802256CC;
extern void* D_ovl0_802256D0;
extern void* D_ovl0_8022570C;
extern void* D_ovl0_80225710;
extern void* D_ovl0_80225714;
extern s32 D_ovl0_8022573C;
extern s32 D_ovl0_80225748;
extern s32 D_ovl0_8022574C;
extern s32 D_ovl0_80225750;
extern s32 D_ovl0_80225754;
extern f32 D_ovl0_80225758;
extern s32 D_ovl0_8022575C;
extern s32 D_ovl0_80225760;
extern s32 D_ovl0_80225764;
extern s32 D_ovl0_80225768;
extern s32 D_ovl0_8022576C;

/**
 * Zeroes 0x80225748..0x8022576C, then sets the overlay_0 mode D_ovl0_8022573C
 * and loads its ROM assets. arg0 0, 1 or 2 selects mode 2 with asset index
 * 0xB, 2 or 0xD; any other arg0 selects mode 1 with D_ovl0_8021FDA6 row arg0 - 3.
 */
void func_ovl0_801F5C24(s32 arg0) {
    RomAssetLoad asset;
    RomLoadSlot slot;
    s32 table_index;
    u32 size;

    func_8005AF50();
    D_ovl0_8022575C = 0;
    D_ovl0_80225760 = 0;
    D_ovl0_80225764 = 0;
    D_ovl0_80225768 = 0;
    D_ovl0_8022576C = 0;
    D_ovl0_8022574C = 0;
    D_ovl0_80225748 = 0;
    D_ovl0_80225750 = 0;
    D_ovl0_80225754 = 0;
    D_ovl0_80225758 = 0.0f;

    table_index = 0;
    if (arg0 == 0) {
        D_ovl0_80225744 = 0xB;
        D_ovl0_8022573C = 2;
    } else if (arg0 == 1) {
        D_ovl0_80225744 = 2;
        D_ovl0_8022573C = 2;
    } else if (arg0 == 2) {
        D_ovl0_80225744 = 0xD;
        D_ovl0_8022573C = arg0;
    } else {
        table_index = arg0 - 3;
        D_ovl0_8022573C = 1;
    }

    D_ovl0_80225714 = heap3_alloc(get_rom_asset_allocation_size(0x6A4, &asset));
    load_rom_asset_payload(D_ovl0_80225714, &asset);

    if (D_ovl0_8022573C == 1) {
        D_ovl0_80225710 = heap3_alloc(get_rom_asset_allocation_size(0x55F, &asset));
        load_rom_asset_payload(D_ovl0_80225710, &asset);
        D_ovl0_802256C8 = heap3_alloc(get_rom_asset_allocation_size(D_ovl0_8021FDA6[table_index][0], &asset));
        load_rom_asset_payload(D_ovl0_802256C8, &asset);
    }
    if (D_ovl0_8022573C == 2) {
        D_ovl0_80225710 = heap3_alloc(get_rom_asset_allocation_size(0x560, &asset));
        load_rom_asset_payload(D_ovl0_80225710, &asset);
        D_ovl0_8022570C = heap3_alloc(get_rom_asset_allocation_size(0x832, &asset));
        load_rom_asset_payload(D_ovl0_8022570C, &asset);
        D_ovl0_802256CC = heap3_alloc(get_rom_asset_allocation_size(D_800C547C[D_ovl0_80225744], &asset));
        load_rom_asset_payload(D_ovl0_802256CC, &asset);
        open_rom_asset_slot(D_800C54A0[D_ovl0_80225744], &slot);
        size = get_rom_load_slot_size(&slot);
        D_ovl0_802256D0 = heap3_alloc(size);
        read_rom_load_slot(D_ovl0_802256D0, size, &slot);
    }
}

extern void heap3_free(void** payload_ptr);

void func_ovl0_801F5E9C(void) {
    heap3_free(&D_ovl0_80225710);
    heap3_free(&D_ovl0_80225714);
    if (D_ovl0_8022573C == 1) {
        heap3_free(&D_ovl0_802256C8);
    }
    if (D_ovl0_8022573C == 2) {
        heap3_free(&D_ovl0_8022570C);
        heap3_free(&D_ovl0_802256CC);
        heap3_free(&D_ovl0_802256D0);
    }
    D_ovl0_8022573C = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F5F2C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F618C);

extern s32 D_ovl0_802257D8[4];
extern s32 D_ovl0_802257E8[4];
extern s32 D_ovl0_802257F8[6];

void func_ovl0_801F63CC(s32 arg0, s32 arg1) {
    D_ovl0_802257D8[arg1] = arg0;
    D_ovl0_802257E8[arg1] = 0;
    D_ovl0_802257F8[arg1] = 0;
}

extern s32 D_800C0E60;
extern s32 D_ovl0_8021FC88;
extern s32 D_ovl0_8021FDB8;
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);

void func_ovl0_801F63F8(void) {
    s32 tick_count;

    tick_count = D_ovl0_8021FDB8 + 1;
    D_ovl0_8021FDB8 = tick_count;
    if ((D_ovl0_8021FC88 != 1) || !(tick_count & 3)) {
        play_sound_effect(0x20, D_800C0E60, 0x7F);
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F6450);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F6718);

extern void func_8006A548(Gfx** pgfx, u32 tlut);
extern void func_8006A4A0(Gfx** pgfx, u32 pal, u32 addr);
extern void load_texture_block(Gfx** pgfx, u32 timg, s32 fmt, s32 siz,
                               s32 width, s32 height, s32 pal, s32 cms,
                               s32 cmt, s32 masks, s32 maskt, s32 shifts,
                               s32 shiftt);
extern void func_8006A2C0(Gfx** pgfx, s32 ulx, s32 uly, s32 lrx, s32 lry,
                          s32 tile, s32 s, s32 t, s32 dsdx, s32 dtdy);
extern void* D_ovl0_80225704;
extern void* D_ovl0_802256FC;
void func_ovl0_801F7740(Gfx** pgfx);
void func_ovl0_801F8094(Gfx** pgfx);

/**
 * Loads the shared palette at block+8 and the index-th 32x32 CI8 texture at
 * block+0x208 + index * 0x400, then draws it at pixel (x, y).
 */
void func_ovl0_801F6D0C(Gfx** pgfx, s32 x, s32 y, s32 index) {
    Gfx* gfx = *pgfx;
    s32 offset;

    func_8006A548(&gfx, (u32)D_ovl0_80225704 + 8);
    offset = (index << 0xA) + 0x208;
    load_texture_block(&gfx, (u32)D_ovl0_80225704 + offset, G_IM_FMT_CI,
                       G_IM_SIZ_8b, 0x20, 0x20, 0, 0, 0, 0, 0, 0, 0);
    func_8006A2C0(&gfx, x * 4, y * 4, (x + 0x20) * 4, (y + 0x20) * 4, 0, 0, 0,
                  0x400, 0x400);
    *pgfx = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F6DFC);

/**
 * Loads the palette at block+8 and the 72x18 CI4 texture (rows 40 bytes, at
 * block+0x28 + index * 0x2D0), then draws it at pixel (x, y).
 */
void func_ovl0_801F7294(Gfx** pgfx, s32 x, s32 y, s32 index) {
    Gfx* gfx = *pgfx;
    s32 offset;

    func_8006A4A0(&gfx, 0, (u32)D_ovl0_802256FC + 8);
    offset = (index * 0x2D0) + 0x28;
    gDPLoadTextureTile_4b(gfx++, ((u32)D_ovl0_802256FC + offset) & ~7, G_IM_FMT_CI, 80, 18, 0, 0, 71, 17, 0, 0, 0, 0, 0, 0, 0);
    func_8006A2C0(&gfx, x * 4, y * 4, (x + 0x48) * 4, (y + 0x12) * 4, 0, 0, 0, 0x400, 0x400);
    *pgfx = gfx;
}

extern void* D_ovl0_802256D8[];
extern void* D_ovl0_802256E8[];

/**
 * Loads the player's palette and 32x32 CI8 texture, then draws the texture at
 * pixel (x, y). The tile uses S-mirror with a 32-texel mask, so flip = 1 starts
 * S at 32 texels and shows the mirrored copy.
 */
void func_ovl0_801F7418(Gfx** pgfx, s32 x, s32 y, s32 player_index, s32 flip) {
    Gfx* gfx = *pgfx;

    func_8006A548(&gfx, (u32)D_ovl0_802256E8[player_index]);
    load_texture_block(&gfx, (u32)D_ovl0_802256D8[player_index], G_IM_FMT_CI,
                       G_IM_SIZ_8b, 0x20, 0x20, 0, G_TX_MIRROR, G_TX_WRAP, 5,
                       0, 0, 0);
    func_8006A2C0(&gfx, x * 4, y * 4, (x + 0x20) * 4, (y + 0x20) * 4, 0,
                  flip << 0xA, 0, 0x400, 0x400);
    *pgfx = gfx;
}

/**
 * Sets the overlay_0 mode D_ovl0_8022573C. Mode 1 resets the mode-1 state
 * (0x80225748..0x80225758), mode 2 zeroes the mode-2 state
 * (0x8022575C..0x8022576C); other values only store the mode.
 */
void func_ovl0_801F7518(s32 arg0) {
    D_ovl0_8022573C = arg0;
    switch (arg0) {
    case 0:
        break;
    case 1:
        D_ovl0_80225748 = 0;
        D_ovl0_8022574C = 0;
        D_ovl0_80225750 = 0x140;
        D_ovl0_80225754 = 0;
        D_ovl0_80225758 = -1.57f;
        break;
    case 2:
        D_ovl0_8022575C = 0;
        D_ovl0_80225760 = 0;
        D_ovl0_80225764 = 0;
        D_ovl0_80225768 = 0;
        D_ovl0_8022576C = 0;
        break;
    }
}

extern s32 D_ovl0_8021FD40;
extern s32 D_ovl0_8021FD48;
extern f64 D_ovl0_80224CC0;
extern f64 D_ovl0_80224CC8;
extern f64 D_ovl0_80224CD0;

void func_ovl0_801F75B0(void) {
    SaveCounterView* save = (SaveCounterView*)func_8005AF50();
    s32 idx;

    D_ovl0_8022574C++;
    switch (D_ovl0_80225748) {
    case 0: {
        f32 amp = -sinf(D_ovl0_80225758) * 160.0f;
        D_ovl0_80225754 = 0;
        D_ovl0_80225758 = (f32)((f64)D_ovl0_80225758 + D_ovl0_80224CC0);
        D_ovl0_80225750 = (s32)amp;
        if (D_ovl0_80224CC8 < (f64)D_ovl0_80225758) {
            D_ovl0_80225758 = 3.1415927f;
        }
        if (D_ovl0_8022574C >= 0x2E) {
            D_ovl0_80225748 = 1;
        }
        break;
    }
    case 1: {
        f32 amp = -sinf(D_ovl0_80225758) * 160.0f;
        D_ovl0_80225758 = (f32)((f64)D_ovl0_80225758 + D_ovl0_80224CD0);
        D_ovl0_80225750 = (s32)amp;
        if (D_ovl0_8022574C >= 0x33) {
            func_ovl0_801F7518(0);
            idx = D_ovl0_8021FD40 + 1;
            D_ovl0_8021FD40 = idx;
            if (idx < 6) {
                D_ovl0_8021FD48 = func_8005B0A0(idx) - save->unknown_15CC;
            }
        }
        break;
    }
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F7740);

extern s8 D_801B72A5;

/**
 * Per-call update of the mode-2 phase. Every call increments D_ovl0_80225760.
 * While D_ovl0_8022575C is zero, the counts 12, 16 and 20 (from mode-2 entry)
 * each stream one ROM asset into D_ovl0_802256CC; D_ovl0_80225764 counts the
 * loads and caps them at three. From count 0x33 on, D_ovl0_80225768 decays, and
 * it drops the overlay to mode 0 via func_ovl0_801F7518(0) below -0xC8.
 */
void func_ovl0_801F7F7C(void) {
    RomAssetLoad asset;
    s32 count;
    s32 asset_step;

    func_8005AF50();
    count = D_ovl0_80225760 + 1;
    D_ovl0_80225760 = count;
    if (D_ovl0_8022575C == 0) {
        if ((count >= 0xB) && !(count & 3) && (D_ovl0_80225764 < 3)) {
            asset_step = D_ovl0_80225764 + 1;
            D_ovl0_80225764 = asset_step;
            get_rom_asset_allocation_size((D_800C547C[D_801B72A5] + asset_step) & 0xFFFF,
                                          &asset);
            load_rom_asset_payload(D_ovl0_802256CC, &asset);
        }
        if (D_ovl0_80225760 >= 0x33) {
            if (D_ovl0_80225768 >= -1) {
                D_ovl0_80225768 = D_ovl0_80225768 - 8;
            } else {
                D_ovl0_80225768 = D_ovl0_80225768 + ((D_ovl0_80225768 * 2) / 3);
            }
            if (D_ovl0_80225768 < -0xC8) {
                func_ovl0_801F7518(0);
            }
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F8094);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F8A30);

extern s32 D_ovl0_8021FD94;

s32 func_ovl0_801F8B7C(void) {
    return D_ovl0_8021FD94;
}

extern s32 D_ovl0_8021FD98;
extern s32 D_ovl0_8021FD9C;
extern s32 D_ovl0_8021FDA0;

void func_ovl0_801F8B88(s32 state, s32 sub_state) {
    D_ovl0_8021FD94 = state;
    D_ovl0_8021FD98 = sub_state;
    if (state == 0) {
        D_ovl0_8021FD9C = 0x140;
        D_ovl0_8021FDA0 = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F8BB8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F9AD0);

/* Request block passed to func_800990D0; bytes 0..2 come from func_8005D2B8. */
typedef struct {
    /* 0x00 */ u8 bytes[3];
    /* 0x03 */ u8 unk3;
    /* 0x04 */ s16 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ u8 unkC;
    /* 0x0D */ u8 unkD[0xB];
} Ovl0Request; /* 0x18 */

extern void func_8005D2B8(u8* arg0);
extern void func_8006BA24(s32 dialog_index);
extern s32 func_8006BA94(ControllerState* input);
extern void func_800990D0(Ovl0Request* req, void* arg1);
extern u8 D_800FF201[][0x8C];
extern s32 D_801B60B0;
extern u8 D_801B71ED;
extern s8 D_801B724B[];
extern s32 D_ovl0_8021FC78;
extern s32 D_ovl0_8021FC7C;
extern s32 D_ovl0_8021FC8C;
extern u8 D_ovl0_80225770[];
extern s32 D_ovl0_80225788;

s32 func_ovl0_801FA098(ControllerState* input) {
    Ovl0Request req;

    func_8005AF50();
    D_ovl0_8021FC7C++;
    switch (D_ovl0_8021FC78) {
    case 0:
        D_ovl0_8021FC78 = 1;
        func_ovl0_801F63CC(1, 0);
        func_8005D2B8(req.bytes);
        req.unk3 = D_801B71ED;
        req.unk4 = D_ovl0_80225788;
        req.unk8 = D_801B60B0;
        req.unkC = D_800FF201[D_801B724B[1]][2];
        func_800990D0(&req, D_ovl0_80225770);
        D_ovl0_8021FC8C = 1;
        break;
    case 1:
        if (input->buttons & 0x8000) {
            D_ovl0_8021FC8C = 0;
            D_ovl0_8021FC78 = 2;
            func_8006BA24(9);
            play_sound_effect(0x5D, 0xF, 0x64);
        }
        break;
    case 2:
        switch (func_8006BA94(input)) {
        case 10:
            D_ovl0_8021FC78 = 3;
            break;
        case 11:
            D_ovl0_8021FC78 = 1;
            D_ovl0_8021FC8C = 1;
            break;
        }
        break;
    case 3:
        D_ovl0_8021FC8C = 0;
        return 1;
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FA248);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FA684);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FAD2C);

extern void func_800719A0(s32 x, s32 y, s32 run, s32 size, s32 index, s32 flags, u8 red, u8 green, u8 blue, u8 alpha);
extern void func_ovl0_801F6450(Gfx** pgfx, s32 x, s32 y, s32 index);
extern void load_texture_block_4b(Gfx** pgfx, u32 timg, s32 fmt, s32 width, s32 height, s32 pal, s32 cms,
                                  s32 cmt, s32 masks, s32 maskt, s32 shifts, s32 shiftt);
extern void emit_glyph_sheet_preamble_dl(Gfx** chain, s32 arg1);
extern void emit_glyph_string_dl(Gfx** gfxp, s32 y, s32 x, char* str);
extern void func_8007624C(Gfx** chain, s32 x, s32 y, char* str, u32 fg, u32 bg, s32 arg6);
extern s32 D_ovl0_8021FC84;
extern s32 D_ovl0_80225798;
extern s32 D_ovl0_802257C8;
extern s32 D_ovl0_802257D0;
extern void* D_ovl0_80225700;
extern char D_80105118[];
extern char D_ovl0_80224DD4[];
extern char D_ovl0_80224DD8[];
extern char D_ovl0_80224DE0[];
extern char D_ovl0_80224DE4[];
extern char D_ovl0_80224DF4[];
extern char D_ovl0_80224DFC[];

void func_ovl0_801FB02C(Gfx** pgfx) {
    Gfx* gfx;
    s32 secs;
    s32 mins;
    s32 frames;

    gfx = *pgfx;
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);

    func_ovl0_801F7418(&gfx, D_ovl0_8021FC80 + 0x28, D_ovl0_8021FC84 + 0x58, 0, 0);
    func_ovl0_801F7294(&gfx, D_ovl0_8021FC80 + 0x52, D_ovl0_8021FC84 + 0x38, 2);
    func_ovl0_801F7294(&gfx, D_ovl0_8021FC80 + 0xE8, D_ovl0_8021FC84 + 0x38, 5);
    if (D_ovl0_8021FD40 == 6) {
        func_800719A0(D_ovl0_8021FC80 + 0x20, D_ovl0_8021FC84 + 0xC0, 0x47, 1, 0, 0, 0xE6, 0xE6, 0xFF, 0xFF);
    } else {
        func_ovl0_801F6450(&gfx, D_ovl0_8021FC80 + 0x1D, 0xC9,
                           D_ovl0_8021FD48 + D_ovl0_802257C8 + D_ovl0_802257D0);
        func_800719A0(D_ovl0_8021FC80 + 0x40, D_ovl0_8021FC84 + 0xC8, 0x46, 1, 0, 0, 0xE6, 0xE6, 0xFF, 0xFF);
    }
    func_8006A4A0(&gfx, 0, (u32)D_ovl0_80225700 + 8);
    load_texture_block_4b(&gfx, (u32)D_ovl0_80225700 + 0x28, 2, 0x10, 0x100, 0, 0, 0, 0, 0, 0, 0);

    if (D_ovl0_80225788 == 0) {
        sprintf(D_80105118, D_ovl0_80224DD4, 0);
    } else {
        sprintf(D_80105118, D_ovl0_80224DD8, D_ovl0_80225788);
    }
    emit_glyph_string_dl(&gfx, D_ovl0_8021FC80 + 0x5A, D_ovl0_8021FC84 + 0x84, D_80105118);

    frames = (s32)((f32)D_801B60B0 * 3.3333335f) % 100;
    secs = D_801B60B0 / 30;
    mins = (secs / 60) % 99;
    sprintf(D_80105118, D_ovl0_80224DE4, mins, secs % 60, frames);
    emit_glyph_string_dl(&gfx, D_ovl0_8021FC80 + 0x5A, D_ovl0_8021FC84 + 0x5E, D_80105118);

    sprintf(D_80105118, D_ovl0_80224DE0, D_ovl0_80225798);
    emit_glyph_string_dl(&gfx, D_ovl0_8021FC80 + 0xE4, D_ovl0_8021FC84 + 0x58, D_80105118);

    emit_glyph_sheet_preamble_dl(&gfx, 0);
    sprintf(D_80105118, D_ovl0_80224DF4);
    func_8007624C(&gfx, D_ovl0_8021FC80 + 0x56, D_ovl0_8021FC84 + 0x4C, D_80105118, 0xFFFF0000, 0x30FF0000, 0);
    sprintf(D_80105118, D_ovl0_80224DFC);
    func_8007624C(&gfx, D_ovl0_8021FC80 + 0x56, D_ovl0_8021FC84 + 0x72, D_80105118, 0xFFFF0000, 0x30FF0000, 0);

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);

    *pgfx = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FB500);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FBBBC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FC3D4);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FCC48);

void func_ovl0_801FD578(Gfx** pgfx) {
    Gfx* gfx;

    gfx = *pgfx;
    switch (D_ovl0_8022573C) {
    case 0:
        break;
    case 1:
        func_ovl0_801F7740(&gfx);
        break;
    case 2:
        func_ovl0_801F8094(&gfx);
        break;
    }
    *pgfx = gfx;
}
