#include "common.h"
#include "../main/rom_asset.h"

void func_80058D04(s32, s32, s32);
extern Gfx* glistp;
extern s32 D_ovl7_8020AC4C;
extern u8 D_ovl7_80213A40[];
extern u8 D_ovl7_80212830[];
extern u16 D_ovl7_8020AD8A[];
extern u8 D_ovl7_80214C50[];
extern u32 D_ovl7_8020AD70[];
extern f32 D_ovl7_8020AD80[];
extern f64 cos(f64);
extern f64 sin(f64);
extern s32 D_ovl7_8020ADA0[2];
extern s32 D_ovl7_8020ADA8;
extern s32 D_ovl7_8020B210;
extern f64 D_ovl7_8020C020;
extern f64 D_ovl7_8020C028;
extern f64 D_ovl7_8020C030;
extern u16 D_ovl7_8020AC9A[][14];
extern u8 D_ovl7_8020E400[];
extern s32 D_ovl7_80212810[2];
extern s32 D_ovl7_80212818[2];
extern s32 D_ovl7_80212820[2];
extern void* D_801B7EE0;
extern void* D_ovl7_802259F8[2];
extern void* D_ovl7_80225A04;
extern s32 D_ovl7_8020AC64;
extern s8 D_ovl7_8020BA70;
extern s32 D_ovl7_8022BFA8;
extern u16* g_auxiliary_framebuffer;
void heap3_free(void** payload_address);
void set_clear_color(s32 red, s32 green, s32 blue);
void func_80029B58(void);
void func_80078910(void);
void *heap3_alloc(s32 size);
s32 func_800577D0(void);
void func_800577DC(s32, void *);
void func_800789C8(void);
void func_ovl7_80200B18(void);
void register_scenery_heap_region(u8 *heap_region_start, s32 heap_region_size);

void func_ovl7_801F4A40(void) {
    D_ovl7_8020AC4C++;
    if (D_ovl7_8020AC4C == 11) {
        D_ovl7_8020AC4C = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F4A6C);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F4AEC);

void func_ovl7_801F5018(void) {
    RomAssetLoad sp10;
    s32 var_s0;
    void *temp_v0;
    void *temp_v0_2;
    void *temp_v0_3;

    osSyncPrintf(&D_ovl7_8020BA70);
    temp_v0 = heap3_alloc(0x7038U);
    D_ovl7_80225A04 = temp_v0;
    register_scenery_heap_region(temp_v0, 0x7038);
    var_s0 = 0;
    D_ovl7_8022BFA8 = 0x1F;
    for (var_s0 = 0; var_s0 < 2; var_s0++) {
        temp_v0_2 = heap3_alloc(func_800577D0());
        D_ovl7_802259F8[var_s0] = temp_v0_2;
        func_800577DC(var_s0 + 1, temp_v0_2);
    }
    func_800577DC(3, g_auxiliary_framebuffer);
    temp_v0_3 = heap3_alloc(get_rom_asset_allocation_size(0x4C3U, &sp10));
    D_801B7EE0 = temp_v0_3;
    load_rom_asset_payload(temp_v0_3, &sp10);
    D_ovl7_8020AC64 = 0;
    func_800789C8();
    func_ovl7_80200B18();
}

void func_ovl7_801F5100(void) {
    s32 slot_index;

    heap3_free(&D_801B7EE0);
    for (slot_index = 0; slot_index < 2; slot_index++) {
        heap3_free(&D_ovl7_802259F8[slot_index]);
    }
    func_80029B58();
    heap3_free(&D_ovl7_80225A04);
    set_clear_color(0xFF, 0xFF, 0xFF);
    func_80078910();
}

void func_ovl7_801F5180(s32 view_state_address, Gfx** cursor) {
    s32 slot_index;

    slot_index = 0;
    glistp = *cursor;
    do {
        func_80058D04(view_state_address, 2, slot_index);
        slot_index += 1;
    } while (slot_index < 4);
    *cursor = glistp;
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F51F0);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F5454);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F58EC);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F6788);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FA4EC);

void func_ovl7_801FA83C(s32 table_index, s32 slot_index) {
    RomAssetLoad asset;

    get_rom_asset_allocation_size(D_ovl7_8020AC9A[table_index][0], &asset);
    load_rom_asset_payload(D_ovl7_8020E400 + slot_index * 0x2208, &asset);
    D_ovl7_80212810[slot_index] = table_index;
    D_ovl7_80212818[slot_index] = 0;
    D_ovl7_80212820[slot_index] = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FA8D8);

typedef struct {
    s32 max_a;
    s32 max_b;
    s32 unk08;
    s32 unk0C;
    s32 cur_a;
    s32 cur_b;
    s32 unk18;
} MeterRec;

extern MeterRec D_ovl7_8020AC9C[];

void func_ovl7_801FB864(void) {
    s32 *temp_a1;
    s32 temp_v1;
    s32 var_a3;
    s32 var_v1;

    var_a3 = 0;
    do {
        if (D_ovl7_80212820[var_a3] != 0) {
            temp_a1 = &D_ovl7_80212818[var_a3];
            temp_v1 = *temp_a1;
            if (temp_v1 < 0xA) {
                var_v1 = D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].max_a;
                if (var_v1 < 0) {
                    var_v1 += 7;
                }
                D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].cur_a += var_v1 >> 3;
                *temp_a1 += 1;
                if (D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].cur_a > D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].max_a) {
                    D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].cur_a = D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].max_a;
                }
                if (*temp_a1 == 8) {
                    *temp_a1 = 0xA;
                }
            } else if (temp_v1 < 0x14) {
                var_v1 = D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].max_b;
                if (var_v1 < 0) {
                    var_v1 += 3;
                }
                D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].cur_b -= var_v1 >> 2;
                *temp_a1 += 1;
                if (D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].cur_b <= 0) {
                    D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].cur_b = 1;
                }
                if (*temp_a1 == 0xE) {
                    *temp_a1 = 0x14;
                }
            } else if (temp_v1 < 0x1E) {
                var_v1 = D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].max_b;
                if (var_v1 < 0) {
                    var_v1 += 3;
                }
                D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].cur_b += var_v1 >> 2;
                *temp_a1 += 1;
                if (D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].cur_b > D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].max_b) {
                    D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].cur_b = D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].max_b;
                }
                if (*temp_a1 == 0x18) {
                    *temp_a1 = 0x1E;
                }
            } else {
                D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].cur_a = D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].max_a;
                D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].cur_b = D_ovl7_8020AC9C[D_ovl7_80212810[var_a3]].max_b;
            }
        }
        var_a3 += 1;
    } while (var_a3 != 2);
}

void func_ovl7_801FBB10(s32 arg0, s32 arg1) {
    RomAssetLoad asset;

    get_rom_asset_allocation_size(D_ovl7_8020AD8A[arg0 * 2], &asset);
    load_rom_asset_payload(D_ovl7_80214C50 + arg1 * 0x1208, &asset);
    D_ovl7_8020AD70[arg1] = 0;
    D_ovl7_8020AD80[arg1] = 1.0f;
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FBB94);

void func_ovl7_801FE270(void) {
    RomAssetLoad asset;

    get_rom_asset_allocation_size(0x908, &asset);
    load_rom_asset_payload(D_ovl7_80212830, &asset);
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FE2A0);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FE7B4);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FE8F8);

void func_ovl7_801FF5E8(void) {
    f32 angle;
    s32 phase;
    s32 scaled_cos;
    s32 x;
    s32 y;

    if (D_ovl7_8020ADA8 != 0) {
        phase = D_ovl7_8020B210;
        angle = (f64)phase * D_ovl7_8020C020;
        phase += 0xC;
        D_ovl7_8020B210 = phase;
        if (phase >= 0x21D) {
            D_ovl7_8020ADA8 = 0;
        }
        x = D_ovl7_8020ADA0[0] + 8;
        scaled_cos = (s32)(cos((f64)angle) * D_ovl7_8020C028);
        y = D_ovl7_8020ADA0[1] - 2;
        D_ovl7_8020ADA0[0] = x + scaled_cos;
        D_ovl7_8020ADA0[1] = y - (s32)(sin((f64)angle) * D_ovl7_8020C030);
    }
}

void func_ovl7_801FF6DC(void) {
    RomAssetLoad asset;

    get_rom_asset_allocation_size(0x93D, &asset);
    load_rom_asset_payload(D_ovl7_80212830, &asset);
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FF70C);

void func_ovl7_801FFC10(void) {
    RomAssetLoad asset;

    get_rom_asset_allocation_size(0x939, &asset);
    load_rom_asset_payload(D_ovl7_80213A40, &asset);
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FFC40);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_802008D8);

typedef struct {
    s32 unk00;
    s32 unk04;
    s32 unk08;
} Ovl7Rec;

typedef struct {
    f32 unk00;
    f32 unk04;
} Ovl7Pair;

extern u8 D_ovl7_80217090[];
extern u8 D_ovl7_8021BEE0[];
extern u8 D_ovl7_8021C330[];
extern u8 D_ovl7_80221640[];
extern u8 D_ovl7_802247D0[];
extern Ovl7Rec D_ovl7_80224698[9];
extern s32 D_ovl7_80224708[9];
extern f32 D_ovl7_80224730[9];
extern Ovl7Pair D_ovl7_80224758[9];
extern s32 D_ovl7_802247A0[9];

void func_ovl7_80200B18(void) {
    RomAssetLoad sp10;
    s32 i;

    get_rom_asset_allocation_size(0x945U, &sp10);
    load_rom_asset_payload(D_ovl7_8021C330, &sp10);
    get_rom_asset_allocation_size(0x946U, &sp10);
    load_rom_asset_payload(D_ovl7_80221640, &sp10);
    for (i = 0; i < 9; i++) {
        D_ovl7_80224708[i] = 0;
        D_ovl7_80224730[i] = -14.0f;
        D_ovl7_80224698[i].unk00 = -0x3E8;
        D_ovl7_80224698[i].unk04 = -0x3E8;
        D_ovl7_80224758[i].unk00 = 0.1f;
        D_ovl7_80224758[i].unk04 = 0.1f;
        D_ovl7_802247A0[i] = 0;
    }
    D_ovl7_80224698[0].unk08 = 8;
    D_ovl7_80224698[1].unk08 = 0xC;
    D_ovl7_80224698[2].unk08 = 0x10;
    D_ovl7_80224698[3].unk08 = 0x14;
    D_ovl7_80224698[4].unk08 = 0x18;
    D_ovl7_80224698[5].unk08 = 0x1C;
    D_ovl7_80224698[6].unk08 = 0x24;
    D_ovl7_80224698[7].unk08 = 0x28;
    D_ovl7_80224698[8].unk08 = 0x2C;
    get_rom_asset_allocation_size(0x94BU, &sp10);
    load_rom_asset_payload(D_ovl7_8021BEE0, &sp10);
    get_rom_asset_allocation_size(0x94EU, &sp10);
    load_rom_asset_payload(D_ovl7_80217090, &sp10);
    get_rom_asset_allocation_size(0x6A4U, &sp10);
    load_rom_asset_payload(D_ovl7_802247D0, &sp10);
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_80200CBC);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_80202D34);
