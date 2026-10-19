#include "common.h"
#include "../main/terrain.h"

extern u8 D_80451A60[];

u8* func_ovlgap_80400000(void) {
    return D_80451A60;
}

extern s32 D_800BB020;
extern u16 D_800FED18;
extern u16 D_800FED1E;
extern f32 D_80105220;
extern f32 D_80105228;
extern f32 D_8010525C;
extern s32 D_ovlgap_8040C5A8;
extern s32 D_ovlgap_8040C5AC;
extern s32 D_ovlgap_8040C5B0;
extern s32 D_ovlgap_8040C5B4;
extern u16 debug_menu_pad_buttons_a;
void clear_text_grid(void);
void func_ovlgap_80405AF4(s32, s32);
s16 *get_terrain_vertex_pointer(u32, u32);

void func_ovlgap_8040000C(void) {
    u16 buttons;
    f32 temp_fv0;
    f32 temp_fv0_2;
    GridVertex *temp_s0;
    GridVertex *temp_v0;

    buttons = D_800FED18;
    if (buttons & 0x100) {
        D_ovlgap_8040C5A8 += 1;
    }
    if (buttons & 0x200) {
        D_ovlgap_8040C5A8 -= 1;
    }
    if (buttons & 0x400) {
        D_ovlgap_8040C5AC -= 1;
    }
    if (buttons & 0x800) {
        D_ovlgap_8040C5AC += 1;
    }
    if (D_ovlgap_8040C5A8 < 0) {
        D_ovlgap_8040C5A8 = 0;
    }
    if (D_ovlgap_8040C5A8 >= 0x20) {
        D_ovlgap_8040C5A8 = 0x1F;
    }
    if (D_ovlgap_8040C5AC < 0) {
        D_ovlgap_8040C5AC = 0;
    }
    if (D_ovlgap_8040C5AC >= 0x40) {
        D_ovlgap_8040C5AC = 0x3F;
    }
    if (debug_menu_pad_buttons_a & 0x10) {
        f32 *scale = &D_8010525C;
        temp_fv0 = *scale * 1.1f;
        *scale = temp_fv0;
        if (temp_fv0 >= 10.0f) {
            *scale = 10.0f;
        }
    }
    if (debug_menu_pad_buttons_a & 0x20) {
        f32 *scale = &D_8010525C;
        temp_fv0_2 = *scale * 0.9f;
        *scale = temp_fv0_2;
        if (temp_fv0_2 < 0.1f) {
            *scale = 0.1f;
        }
    }
    if (D_800FED18 & 0x8000) {
        func_ovlgap_80405AF4(D_ovlgap_8040C5A8, D_ovlgap_8040C5AC);
    }
    if (D_800FED1E & 0x2000) {
        D_ovlgap_8040C5B0 = D_ovlgap_8040C5A8;
        D_ovlgap_8040C5B4 = D_ovlgap_8040C5AC;
        clear_text_grid();
        D_800BB020 = 0xA;
    }
    temp_s0 = (GridVertex *) get_terrain_vertex_pointer(D_ovlgap_8040C5A8, D_ovlgap_8040C5AC);
    temp_v0 = (GridVertex *) get_terrain_vertex_pointer(D_ovlgap_8040C5A8 + 1, D_ovlgap_8040C5AC + 1);
    D_80105220 = (f32) ((temp_s0->x + temp_v0->x) / 2);
    do {
        D_80105228 = (f32) ((temp_s0->z + temp_v0->z) / 2);
    } while (0);
}

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_804002C0);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80400FBC);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80402428);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80402E1C);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80403A04);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_804045C0);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80404850);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80404B04);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80404B84);

void func_ovlgap_8040582C(s32 *arg0, s32 arg1) {
    s32 *var_a0;
    s32 var_v1;
    s32 temp_v0;
    s32 tmp[2];
    s32 uninit;

    tmp[0] = uninit;
    var_a0 = arg0;
    var_v1 = 0;
    do {
        temp_v0 = *var_a0;
        if (temp_v0 == arg1) {
            return;
        }
        var_v1++;
        if (temp_v0 == -1) {
            *var_a0 = arg1;
            return;
        }
        var_a0++;
    } while (var_v1 < 2);
}

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80405870);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80405AF4);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80405B80);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80405C94);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80405DB0);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80406570);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_804066C8);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80406818);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80406C3C);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_804077B0);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_804079B8);

extern f64 sin(f64);
extern f64 cos(f64);
extern u8 D_80491A60[];
extern f64 D_ovlgap_8040CA70;
extern f64 D_ovlgap_8040CA78;

void func_ovlgap_80407D64(void) {
    s32 texture_index = 0;
    s32 texture_row = 0;
    s32 bias = 0xFFFFF;
    s32 texture_column;
    s32 sine_phase;
    u8* texture_pixel;
    f64 angle_step = D_ovlgap_8040CA70;
    f64 amplitude = D_ovlgap_8040CA78;
    f64 cosine;
    s32 intensity;
    s32 scaled;

    while (texture_row < 32) {
        texture_column = 0;
        sine_phase = texture_row * 4;
        texture_pixel = &D_80491A60[texture_index];
        while (texture_column < 32) {
            cosine = cos((f64) (f32) (texture_column * 4) * angle_step);
            intensity = (s32) (cosine * sin((f64) (f32) sine_phase * angle_step) * amplitude);
            intensity = intensity * (intensity * intensity);
            scaled = intensity;
            if (intensity < 0) {
                scaled = intensity + bias;
            }
            intensity = scaled >> 0x14;
            if (intensity <= 0) {
                intensity = 1;
            }
            if (intensity >= 0x10) {
                intensity = 0xF;
            }
            *texture_pixel++ = (intensity << 4) | intensity;
            texture_column++;
            texture_index++;
        }
        texture_row++;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80407EC4);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80407F08);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80408080);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80408170);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_804083E0);

/* Dead 8-byte frame and `sw $v0,0(sp)`: static-chain home of the nested-function original, as in func_ovl7_802068B4. */
u16 func_ovlgap_8040BF64(u32 packed_xz) {
    s32 r;
    volatile s32 unused = r;
    u32 gx;
    u32 gz;
    u32 block;
    u32 sx;
    u32 sz;
    u16* base;

    gx = packed_xz & 0xFF;
    gz = (packed_xz >> 8) & 0x1FF;
    block = ((gx >> 5) + (gz >> 5) * 8) * sizeof(TerrainTileCache);
    sx = gx & 0x1F;
    sz = gz & 0x1F;
    /* Preserve the retail member-rooted scalar address and 36-halfword stride. */
    base = &g_terrain_tile_cache[0].colors[1][1];
    return *(u16*)((u8*)(base + (sx + sz * 36)) + block);
}

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_8040BFC8);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_8040C124);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_8040C260);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_8040C360);
