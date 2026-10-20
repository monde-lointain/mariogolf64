#include "common.h"
#include <nusys.h>
#include "../main/session.h"
#include "../main/rom_asset.h"

typedef struct {
  /* 0x00 */ u8 unknown_00[0x30];
  /* 0x30 */ u8 checksum[2];
  /* 0x32 */ u8 payload[0x1CE];
} GbPakSaveRecord; /* 0x200 */

typedef struct {
    void *unk0;
    void *unk4;
    void *unk8;
    void *unkC[4];
    void *unk1C[4];
    void *unk2C;
    void *unk30;
    void *unk34;
    void *unk38;
    void *unk3C;
    void *unk40;
    void *unk44;
    void *unk48;
    void *unk4C;
    void *unk50;
    void *unk54;
    u8 unk58[0x14];
    NUContPakFile gbpakFile; /* 0x6C */
    OSGbpakId gbpakId; /* 0x7C */
} Ovl15AssetBlock; /* known extent 0xCC */

extern s32 D_ovl15_801FBB78;
extern s32 D_ovl15_801FBB7C;
extern s32 D_ovl15_801FBB80;
extern void func_ovl15_801F7664(Gfx** dl, s32* arg1);
extern s32 D_80105218;
extern u32 controller_handle_array[];
extern s32 nuContRmbCheck(u32 contNo);
extern u16 D_800C5498[];
extern s16 D_800C54BC[];
extern Ovl15AssetBlock D_800FE450;
extern char D_ovl15_801FBC40[];
extern char D_ovl15_801FBC4C[];
extern char D_ovl15_801FBC54[];
extern char D_ovl15_801FBC5C[];
extern char D_ovl15_801FBC78[];
extern void check_and_print_grid(char *str, s32 col, s32 row);
extern s32 validate_heap_string(char *source_string, s32 allocation_tag);
s32 func_ovl15_801F9CEC(s32 arg0, OSGbpakId *arg1, NUContPakFile *arg2);
extern void *heap3_alloc(u32 size_bytes);
extern void heap3_free(void **payload_ptr);
extern void play_bgm_by_id(s32 id, s32 flags);
extern void nuContRmbForceStop(void);
extern void func_ovl15_801FB400(u8 *image);
extern void func_8006A548(Gfx **pgfx, u32 tlut);
extern void load_texture_block(Gfx **pgfx, u32 timg, s32 fmt, s32 siz, s32 width, s32 height,
                               s32 pal, s32 cms, s32 cmt, s32 masks, s32 maskt, s32 shifts, s32 shiftt);
extern void func_8006A2C0(Gfx **pgfx, s32 ulx, s32 uly, s32 lrx, s32 lry, s32 tile, s32 s, s32 t,
                          s32 dsdx, s32 dtdy);

void func_ovl15_801F4A40(void) {
    Ovl15AssetBlock *blk;
    u8 buf[0x2000];

    nuContGBPakMgrInit();
    check_and_print_grid(D_ovl15_801FBC40, 0xA, 3);
    check_and_print_grid(D_ovl15_801FBC40, 0xA, 4);
    check_and_print_grid(D_ovl15_801FBC40, 0xA, 5);
    blk = &D_800FE450;
    if (func_ovl15_801F9CEC(0, &blk->gbpakId, &blk->gbpakFile) == 0) {
        check_and_print_grid(D_ovl15_801FBC4C, 0xA, 3);
        if (nuContGBPakFread(&blk->gbpakFile, 0xA000, buf, 0x2000) == 0) {
            check_and_print_grid(D_ovl15_801FBC54, 0xA, 4);
            if (validate_heap_string(D_ovl15_801FBC5C, 0x2000) == 0) {
                check_and_print_grid(D_ovl15_801FBC78, 0xA, 5);
            }
        }
    }
}

/* Overlay 15 initialize hook: loads asset payloads, then slot buffers. */
void func_ovl15_801F4B20(void) {
    RomLoadSlot sp10;
    RomAssetLoad sp20;
    s16 *var_s3;
    void **var_s2;
    void **var_s4;
    void *temp_v0;
    void *temp_v0_10;
    void *temp_v0_11;
    void *temp_v0_12;
    void *temp_v0_13;
    void *temp_v0_14;
    void *temp_v0_15;
    u32 temp_v0_16;
    void *temp_v0_17;
    void *temp_v0_2;
    void *temp_v0_3;
    void *temp_v0_4;
    void *temp_v0_5;
    void *temp_v0_6;
    void *temp_v0_7;
    void *temp_v0_8;
    void *temp_v0_9;
    s32 var_s5;
    u16 *var_s1;
    void *temp_s1;
    RomAssetLoad *var_s6;
    s32 var_s7;

    temp_v0 = heap3_alloc(get_rom_asset_allocation_size(0x8FDU, &sp20));
    temp_s1 = &D_800FE450.unk30;
    D_800FE450.unk30 = temp_v0;
    load_rom_asset_payload(temp_v0, &sp20);
    temp_v0_2 = heap3_alloc(get_rom_asset_allocation_size(0x51EU, &sp20));
    D_800FE450.unk0 = temp_v0_2;
    load_rom_asset_payload(temp_v0_2, &sp20);
    temp_v0_3 = heap3_alloc(get_rom_asset_allocation_size(0x70FU, &sp20));
    D_800FE450.unk4 = temp_v0_3;
    load_rom_asset_payload(temp_v0_3, &sp20);
    temp_v0_4 = heap3_alloc(get_rom_asset_allocation_size(0x720U, &sp20));
    D_800FE450.unk8 = temp_v0_4;
    load_rom_asset_payload(temp_v0_4, &sp20);
    temp_v0_5 = heap3_alloc(get_rom_asset_allocation_size(0x900U, &sp20));
    D_800FE450.unk34 = temp_v0_5;
    load_rom_asset_payload(temp_v0_5, &sp20);
    temp_v0_6 = heap3_alloc(get_rom_asset_allocation_size(0x93FU, &sp20));
    D_800FE450.unk2C = temp_v0_6;
    load_rom_asset_payload(temp_v0_6, &sp20);
    temp_v0_7 = heap3_alloc(get_rom_asset_allocation_size(0x5F7U, &sp20));
    D_800FE450.unk38 = temp_v0_7;
    load_rom_asset_payload(temp_v0_7, &sp20);
    temp_v0_8 = heap3_alloc(get_rom_asset_allocation_size(0x8EEU, &sp20));
    D_800FE450.unk3C = temp_v0_8;
    load_rom_asset_payload(temp_v0_8, &sp20);
    temp_v0_9 = heap3_alloc(get_rom_asset_allocation_size(0x8EBU, &sp20));
    D_800FE450.unk40 = temp_v0_9;
    load_rom_asset_payload(temp_v0_9, &sp20);
    temp_v0_10 = heap3_alloc(get_rom_asset_allocation_size(0x95DU, &sp20));
    D_800FE450.unk44 = temp_v0_10;
    load_rom_asset_payload(temp_v0_10, &sp20);
    temp_v0_11 = heap3_alloc(get_rom_asset_allocation_size(0x979U, &sp20));
    D_800FE450.unk54 = temp_v0_11;
    load_rom_asset_payload(temp_v0_11, &sp20);
    temp_v0_12 = heap3_alloc(get_rom_asset_allocation_size(0x70BU, &sp20));
    D_800FE450.unk4C = temp_v0_12;
    load_rom_asset_payload(temp_v0_12, &sp20);
    temp_v0_13 = heap3_alloc(get_rom_asset_allocation_size(0x96BU, &sp20));
    D_800FE450.unk50 = temp_v0_13;
    load_rom_asset_payload(temp_v0_13, &sp20);
    temp_v0_14 = heap3_alloc(get_rom_asset_allocation_size(0x978U, &sp20));
    D_800FE450.unk48 = temp_v0_14;
    load_rom_asset_payload(temp_v0_14, &sp20);
    var_s5 = 0;
    var_s6 = &sp20;
    var_s7 = 4;
    var_s4 = temp_s1 - 0x24;
    var_s3 = D_800C54BC;
    var_s2 = temp_s1 - 0x14;
    var_s1 = D_800C5498;
    D_800FEC00.unknown_90 = 0;
    D_800FEC00.unknown_94 = 0;
    do {
        temp_v0_16 = get_rom_asset_allocation_size(*var_s1, var_s6);
        var_s1 += 1;
        var_s5 += 1;
        temp_v0_15 = heap3_alloc(temp_v0_16);
        *var_s2 = temp_v0_15;
        load_rom_asset_payload(temp_v0_15, var_s6);
        open_rom_asset_slot(*var_s3, &sp10);
        var_s3 += 1;
        var_s2 += 1;
        temp_v0_16 = get_rom_load_slot_size(&sp10);
        temp_v0_17 = heap3_alloc(temp_v0_16);
        *var_s4 = temp_v0_17;
        read_rom_load_slot(temp_v0_17, temp_v0_16, &sp10);
        var_s4 += 1;
    } while (var_s5 != var_s7);
    play_bgm_by_id(0x16, 0);
    func_ovl15_801FB400(D_800FE450.unk38);
    nuContRmbForceStop();
}

void func_ovl15_801F4E70(void) {
    s32 i;

    heap3_free(&D_800FE450.unk30);
    heap3_free(&D_800FE450.unk0);
    heap3_free(&D_800FE450.unk4);
    heap3_free(&D_800FE450.unk8);
    heap3_free(&D_800FE450.unk34);
    heap3_free(&D_800FE450.unk2C);
    heap3_free(&D_800FE450.unk38);
    heap3_free(&D_800FE450.unk3C);
    heap3_free(&D_800FE450.unk40);
    heap3_free(&D_800FE450.unk44);
    heap3_free(&D_800FE450.unk54);
    heap3_free(&D_800FE450.unk48);
    heap3_free(&D_800FE450.unk4C);
    heap3_free(&D_800FE450.unk50);
    for (i = 0; i != 4; i++) {
        heap3_free(&D_800FE450.unk1C[i]);
        heap3_free(&D_800FE450.unkC[i]);
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F4F50);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F54F8);

void func_ovl15_801FA320(s32 arg0, s32 arg1);

/* Set the aux flag to 2, then for indices 0 and 2..12 start a ramp-up (state 1) recording a per-index value. Other indices do nothing. */
void func_ovl15_801F5570(s32 a0) {
    D_800FEC00.unknown_90 = 2;
    switch (a0) {
        case 2:
            func_ovl15_801FA320(1, 0x15F);
            break;
        case 3:
            func_ovl15_801FA320(1, 0x149);
            break;
        case 4:
            func_ovl15_801FA320(1, 0x14C);
            break;
        case 5:
            func_ovl15_801FA320(1, 0x14D);
            break;
        case 6:
            func_ovl15_801FA320(1, 0x148);
            break;
        case 7:
            func_ovl15_801FA320(1, 0x14A);
            break;
        case 8:
            func_ovl15_801FA320(1, 0x14F);
            break;
        case 9:
            func_ovl15_801FA320(1, 0x14E);
            break;
        case 0:
            func_ovl15_801FA320(1, 0x151);
            break;
        case 10:
            func_ovl15_801FA320(1, 0x152);
            break;
        case 11:
            func_ovl15_801FA320(1, 0x156);
            break;
        case 12:
            func_ovl15_801FA320(1, 0x153);
            break;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F5644);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F5AF0);

s32 func_ovl15_801F5EA8(void) {
    u32 *handle_slot;
    u32 handle;
    s32 checked;
    s32 all_ready;

    all_ready = 1;
    checked = 0;
    if (D_80105218 != 0) {
        handle_slot = controller_handle_array;
        do {
            handle = *handle_slot;
            handle_slot += 1;
            checked += 1;
            all_ready &= -(nuContRmbCheck(handle) == 0);
        } while (checked != D_80105218);
    }
    return all_ready;
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F5F1C);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F6388);

void func_ovl15_801F6624(void) {
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F662C);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F66EC);

void func_ovl15_801F67F4(void) {
  D_800FEC00.unknown_00 = 0;
  D_800FEC00.unknown_04 = 0;
  D_800FEC00.unknown_08 = 0;
  D_800FEC00.unknown_8C = 0;
}

void func_ovl15_801F6818(void) {
  D_800FEC00.unknown_34 = 0;
  D_800FEC00.unknown_38 = 0;
  D_800FEC00.unknown_8C = 0;
}

void func_ovl15_801F6834(void) {
  D_800FEC00.unknown_34 = 0;
  D_800FEC00.unknown_04 = 0;
  D_800FEC00.unknown_38 = 0;
  D_800FEC00.unknown_8C = 0;
}

s32 func_ovl15_801F6858(GbPakSaveRecord *record) {
  return record->checksum[0] | (record->checksum[1] << 8);
}

s32 func_ovl15_801F686C(GbPakSaveRecord *record) {
  s32 sum;
  u8 *cursor;

  sum = 0;
  cursor = record->payload;
  do {
    sum += *cursor;
    cursor++;
  } while (cursor != (u8 *)(record + 1));
  return sum;
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F6890);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F68FC);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F69B8);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F6E94);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F7074);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F71F8);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F7664);

void func_ovl15_801F77D4(Gfx** dl, s32* arg1) {
    Gfx* saved = *dl;

    func_ovl15_801F7664(&saved, arg1);
    *dl = saved;
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F780C);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F7ED4);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F8110);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F8BC8);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F8C60);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F91E4);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F93DC);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F9688);

s32 func_ovl15_801F9D80(s32 contNo, OSGbpakId *info, NUContPakFile *handle);

/* Retries the GB Pak check up to three times, re-registering the GB Pak manager after each failure. Returns 1 on success, otherwise the last failure code. */
s32 func_ovl15_801F9CEC(s32 arg0, OSGbpakId *arg1, NUContPakFile *arg2) {
    s32 result;
    s32 i;

    i = 0;
    do {
        result = func_ovl15_801F9D80(arg0, arg1, arg2);
        if (result == 1) {
            return 1;
        }
        nuContGBPakMgrInit();
        i++;
    } while (i != 3);
    return result;
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F9D80);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801FA0BC);

/* Set the overlay 15 state word. Ramp up (1) restarts the step counter at 0 and records arg1; ramp down (2) starts the counter at 4. */
void func_ovl15_801FA320(s32 arg0, s32 arg1) {
    D_ovl15_801FBB78 = arg0;
    if (arg0 == 1) {
        D_ovl15_801FBB80 = 0;
        D_ovl15_801FBB7C = arg1;
    }
    if (arg0 == 2) {
        D_ovl15_801FBB80 = 4;
    }
}

/* Return the overlay 15 state word: 0 idle, 1 ramping up, 2 ramping down, 3 active. */
s32 func_ovl15_801FA35C(void) {
    return D_ovl15_801FBB78;
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801FA368);

/* Draw ten 216x9 CI8 texture strips from asset block unk50, stacked in 9-pixel rows. */
void func_ovl15_801FA720(Gfx **gfxp) {
    Gfx *gfx;
    s32 i;

    gfx = *gfxp;
    func_8006A548(&gfx, (u32)D_800FE450.unk50 + 8);
    for (i = 0; i != 10; i++) {
        load_texture_block(&gfx, (u32)D_800FE450.unk50 + 0x208 + i * 0x798, 2, 1, 0xD8, 9, 0, 0, 0, 0, 0, 0, 0);
        func_8006A2C0(&gfx, 0xD0, 0x100 + i * 0x24, 0x430, 0x124 + i * 0x24, 0, 0, 0, 0x400, 0x400);
    }
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801FA844);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801FA990);

/* Returns x rounded to one decimal place (half up on the hundredths digit), truncated to an
 * integer. The clamp to 999 applies only when that rounding carries into the integer part. */
s32 func_ovl15_801FB348(f32 arg0) {
    f32 temp_fa0;
    s32 var_a0;
    s32 var_a1;

    var_a1 = (s32) arg0;
    temp_fa0 = (arg0 - (f32) var_a1) * 100.0f;
    var_a0 = (s32) temp_fa0;
    if (((s32) temp_fa0 % 10) >= 5) {
        var_a0 += 0xA;
    }
    var_a0 = var_a0 / 10;
    if (var_a0 >= 0xA) {
        var_a1 += 1;
        if (var_a1 >= 0x3E8) {
            var_a1 = 0x3E7;
        }
    }
    return var_a1;
}
