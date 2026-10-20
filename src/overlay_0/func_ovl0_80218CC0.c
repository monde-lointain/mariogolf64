#include "common.h"
#include "../main/controller.h"
#include "../main/rom_asset.h"

extern void heap3_free(void** payload_ptr);
extern void func_80051A88(s32 volume);
extern s32 D_ovl0_80224A7C;
extern s32 D_ovl0_80224A80;
extern s32 D_ovl0_8022D678;
extern void* D_ovl0_8022B530;
extern void* heap3_alloc(u32 size);
extern void* D_ovl0_8022B534;
extern void* D_ovl0_8022B538;
extern void* D_ovl0_8022B53C;

extern s8 D_801061BD;
extern s8 D_801061BF;
extern s8 D_801061C0;
extern s8 D_801061C1;
extern s8 D_801061C2;
extern s8 D_801061C3;
extern s8 D_801061C4;
extern s8 D_801061C5;
extern u8 D_80106241;
extern u8 D_80106242;
extern u8 D_80106243;
extern s32 D_800C306C;
extern void func_800934CC(s32 arg0);
extern void func_8002A97C(s32 arg0, s32 arg1);
extern void set_color_framebuffer_count(u32 buffer_count);
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);
extern void func_8006A2C0(Gfx **, s32, s32, s32, s32, s32, s32, s32, s32, s32);
extern void func_8006A4A0(Gfx **, u32, u32);
extern void load_texture_block_4b(Gfx **, u32, s32, s32, s32, s32, s32, s32, s32, s32, s32, s32);
extern s32 D_800C0E60;
extern s32 D_ovl0_80224A90;
extern s32 D_ovl0_80224A98;
extern s32 D_ovl0_80224A9C;
extern s32 D_ovl0_80224A84;

void func_ovl0_80218CC0(void) {
    D_801061BD = 0;
    D_801061C4 = 0;
    D_801061BF = 0;
    D_80106241 = 0;
    D_80106242 = 0;
    D_80106243 = 0;
    D_801061C0 = 0;
    D_801061C1 = 0;
    D_801061C2 = 0;
    D_801061C3 = 0;
    D_801061C5 = 0;
}

void func_ovl0_80218D1C(void) {
    D_801061BD = 1;
    D_801061C4 = 1;
    D_801061BF = 1;
    D_80106241 = 0;
    D_80106242 = 0;
    D_80106243 = 0;
    D_801061C0 = 1;
    D_801061C1 = 1;
    D_801061C2 = 1;
    D_801061C3 = 1;
    D_801061C5 = 1;
    func_800934CC(-1);
    D_800C306C = 0;
    set_color_framebuffer_count(3);
    func_8002A97C(4, 0);
}

void func_ovl0_80218DB0(s32 arg0, s32 arg1) {
    if (arg1 == 0) {
        D_ovl0_80224A80 = 1;
    } else {
        D_ovl0_80224A80 = -1;
    }
    D_ovl0_8022D678 = arg0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_80218DD0);

void func_ovl0_80218FDC(void) {
    if (D_ovl0_80224A7C == 1) {
        D_ovl0_80224A7C = 0;
        heap3_free(&D_ovl0_8022B530);
        heap3_free(&D_ovl0_8022B534);
        heap3_free(&D_ovl0_8022B538);
    }
    func_80051A88(0x7F);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_80219034);

void func_ovl0_802199EC(Gfx **pgfx, s32 x, s32 y, s32 width) {
    Gfx *gfx;
    s32 phase;
    s32 frame;
    s32 half;
    s32 left;
    s32 right;
    s32 top;
    s32 bottom;

    gfx = *pgfx;
    phase = D_ovl0_80224A84 + 1;
    D_ovl0_80224A84 = phase % 8;

    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

    func_8006A4A0(&gfx, 0, (u32) (D_ovl0_8022B538 + 8));
    frame = D_ovl0_80224A84;
    if (frame < 0) {
        frame += 3;
    }
    load_texture_block_4b(&gfx, (u32) (D_ovl0_8022B538 + (((frame >> 2) << 9) + 0x28)), 2, 0x10, 0x40, 0, 0, 0, 0, 0, 0, 0);

    half = width / 2;
    left = x - half;
    top = (y - 8) * 4;
    bottom = (y + 8) * 4;
    gDPPipeSync(gfx++);
    func_8006A2C0(&gfx, (left - 8) * 4, top, (left + 8) * 4, bottom, 0, 0, 0x600, 0x400, 0x400);
    right = x + half;
    func_8006A2C0(&gfx, (right - 8) * 4, top, (right + 8) * 4, bottom, 0, 0, 0x400, 0x400, 0x400);

    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    *pgfx = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_80219C74);

void func_ovl0_8021A62C(void) {
    RomAssetLoad asset;

    D_ovl0_8022B53C = heap3_alloc(get_rom_asset_allocation_size(0x70F, &asset));
    load_rom_asset_payload(D_ovl0_8022B53C, &asset);
    D_ovl0_8022B534 = heap3_alloc(get_rom_asset_allocation_size(0x71F, &asset));
    load_rom_asset_payload(D_ovl0_8022B534, &asset);
}

void func_ovl0_8021A690(void) {
    heap3_free(&D_ovl0_8022B53C);
    heap3_free(&D_ovl0_8022B534);
}

void func_ovl0_8021A6BC(s32 arg0) {
    D_ovl0_80224A98 = arg0;
    if (arg0 == -1) {
        D_ovl0_80224A90 = arg0;
        func_ovl0_8021A690();
        play_sound_effect(0x65, D_800C0E60, 0x64);
    } else {
        D_ovl0_80224A9C = 0;
        D_ovl0_80224A90 = 0;
        func_ovl0_8021A62C();
        play_sound_effect(0x64, D_800C0E60, 0x64);
    }
}

typedef struct Ovl0Entry {
    s32 unk0;
    s32 unk4;
    s32 unk8;
    u8 unkC[8];
} Ovl0Entry;

extern Ovl0Entry D_ovl0_80224928[];

s32 func_ovl0_8021A724(ControllerState *input) {
    Ovl0Entry *temp_s1;

    temp_s1 = &D_ovl0_80224928[D_ovl0_80224A98];
    if (D_ovl0_80224A98 != -1) {
        switch (D_ovl0_80224A90) {
        case 1:
            D_ovl0_80224A90 = 2;
            return -1;
        case 2:
            func_ovl0_8021A6BC(-1);
            return temp_s1->unkC[4 + D_ovl0_80224A9C];
        case 0:
            if (input->pressed == 0x8000) {
                play_sound_effect(0x5D, D_800C0E60, 0x64);
                D_ovl0_80224A90 = 1;
                return -1;
            }
            if (temp_s1->unk8 == 0) {
                if (input->repeated & 0x200) {
                    D_ovl0_80224A9C -= 1;
                    play_sound_effect(0x5C, D_800C0E60, 0x64);
                }
                if (input->repeated & 0x100) {
                    D_ovl0_80224A9C += 1;
                    play_sound_effect(0x5C, D_800C0E60, 0x64);
                }
            } else {
                if (input->repeated & 0x800) {
                    play_sound_effect(0x5C, D_800C0E60, 0x64);
                    D_ovl0_80224A9C -= 1;
                }
                if (input->repeated & 0x400) {
                    play_sound_effect(0x5C, D_800C0E60, 0x64);
                    D_ovl0_80224A9C += 1;
                }
            }
            if ((temp_s1->unk0 - 1) < D_ovl0_80224A9C) {
                D_ovl0_80224A9C = 0;
            }
            if (D_ovl0_80224A9C < 0) {
                D_ovl0_80224A9C = temp_s1->unk0 - 1;
            }
            return -1;
        default:
            return -1;
        }
    }
    return -1;
}

extern void func_80071608(Gfx **);
extern s32 func_800738BC(u8 *, s32, s32);
extern void func_800747B0(Gfx **, s32, s32, u8 *);
extern u8 *D_ovl0_80224978[];
extern f32 D_ovl0_80224AA0;
extern f32 D_ovl0_80224AA4;
extern f64 D_ovl0_802255D0;
extern f64 D_ovl0_802255D8;
extern f64 D_ovl0_802255E0;
extern f64 D_ovl0_802255E8;
extern f64 D_ovl0_802255F0;

void func_ovl0_8021A940(Gfx **arg0) {
    Gfx *gfx;
    Ovl0Entry *entry;
    s32 count;
    s32 half_w;
    s32 max_w;
    s32 w;
    s32 width;
    s32 i;
    s32 tint_r;
    s32 tint_g;
    s32 tint_b;
    s32 x;
    s32 y0;
    s16 y;

    gfx = *arg0;
    entry = &D_ovl0_80224928[D_ovl0_80224A98];
    half_w = entry->unk4 + 0x20;
    D_ovl0_80224AA0 = (f32)(D_ovl0_80224AA0 + D_ovl0_802255D0);
    tint_r = (s32)(sinf(D_ovl0_80224AA0) * 127.0f) + 0x7F;
    tint_b = (s32)(sinf((f32)(D_ovl0_80224AA0 + D_ovl0_802255D8)) * 127.0f) + 0x7F;
    tint_g = (s32)(sinf((f32)(D_ovl0_80224AA0 + D_ovl0_802255E0)) * 127.0f) + 0x7F;
    max_w = 0;

    if (D_ovl0_80224A98 != -1) {
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 0x1F, 0x1F, 0x1F, 0xBF);
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xBF);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);

        for (i = 0; i != entry->unk0; i++) {
            w = func_800738BC(D_ovl0_80224978[entry->unkC[i]], 0xC, 2);
            if (max_w < w) {
                max_w = w;
            }
            half_w = (max_w / 2) + 0x20;
        }

        for (i = 0; i != 1; i++) {
            load_texture_block_4b(&gfx, (u32)D_ovl0_8022B534 + 8 + i * 0x1000, G_IM_FMT_I, 128, 64, 0, 0, 0, 0, 0, 0, 0);
            func_8006A2C0(&gfx, (0xA0 - half_w) * 4, 0x120, (half_w + 0xA0) * 4, 0x290, 0, 0, 0, 0x10000 / half_w, 0x2C8);
        }
        gDPPipeSync(gfx++);
        func_80071608(&gfx);

        if (entry->unk8 != 0) {
            count = entry->unk0;
            y0 = 0x71 - ((count - 1) * 8);
            for (i = 0; i != entry->unk0; i++) {
                if (i == D_ovl0_80224A9C) {
                    gDPPipeSync(gfx++);
                    gDPSetPrimColor(gfx++, 0, 0, tint_r, tint_g, tint_b, 0xFF);
                    gDPPipeSync(gfx++);
                } else {
                    gDPPipeSync(gfx++);
                    gDPSetPrimColor(gfx++, 0, 0, 0x5F, 0x3F, 0xFF, 0xFF);
                    gDPPipeSync(gfx++);
                }
                y = y0 + (i * 0x10);
                func_800747B0(&gfx, 0xA0, y, D_ovl0_80224978[entry->unkC[i]]);
            }
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

            D_ovl0_80224AA4 = (f32)(D_ovl0_80224AA4 + D_ovl0_802255E8);
            if (D_ovl0_802255F0 < D_ovl0_80224AA4) {
                D_ovl0_80224AA4 = (f32)(D_ovl0_80224AA4 - D_ovl0_802255F0);
            }
            width = func_800738BC(D_ovl0_80224978[entry->unkC[D_ovl0_80224A9C]], 0xC, 2);
            x = (s32)(sinf(D_ovl0_80224AA4) * 8.0f);
            x += 0x7C;
            width /= 2;
            x -= width;
            func_8006A4A0(&gfx, 0, (u32)D_ovl0_8022B53C + 8);

            gDPLoadTextureTile_4b(gfx++, ((u32)D_ovl0_8022B53C + 0x28) & ~7, G_IM_FMT_CI, 24, 76, 0, 0, 23, 75, 0, 0, 0, 0, 0, 0, 0);
            func_8006A2C0(&gfx, x * 4, ((y0 + (D_ovl0_80224A9C * 0x10)) - 2) * 4, (x + 0x18) * 4, ((y0 + (D_ovl0_80224A9C * 0x10)) + 0x16) * 4, 0, 0, 0, 0x400, 0x400);
        }
    }
    *arg0 = gfx;
}
