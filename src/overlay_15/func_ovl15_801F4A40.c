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
    u16 a;
    u16 b;
    u8 c;
    u8 d;
} Ovl15EntryRec; /* 6 bytes */

typedef struct {
    u8 state;               /* 0x00 */
    u8 name[10];            /* 0x01 */
    u8 unk0B;               /* 0x0B */
    u8 unk0C;               /* 0x0C */
    u8 key[11];             /* 0x0D */
    u8 unk18;               /* 0x18 */
    u8 priority;            /* 0x19 */
    u8 unk1A;               /* 0x1A */
    u8 unk1B;               /* 0x1B */
    u8 unk1C;               /* 0x1C */
    u8 unk1D;               /* 0x1D */
    u8 unk1E;               /* 0x1E */
    u8 unk1F;               /* 0x1F */
    u8 mode;                /* 0x20 */
    u8 unk21[3];            /* 0x21 */
    s32 unk24;              /* 0x24 */
    s32 unk28;              /* 0x28 */
    s16 unk2C;              /* 0x2C */
    u8 unk2E[5];            /* 0x2E */
    u8 unk33;               /* 0x33 */
    s32 unk34;              /* 0x34 */
    s32 unk38;              /* 0x38 */
    Ovl15EntryRec recs[13]; /* 0x3C */
    u8 unk8A[2];            /* 0x8A */
} Ovl15Entry; /* 0x8C */

typedef struct {
    u8 a[2];
    u8 b[2];
    u8 c;
    u8 d;
} Ovl15SourceRec; /* 6 bytes, u16 fields stored little-endian */

typedef struct {
    u8 name[10];            /* 0x00 */
    u8 unk0A;               /* 0x0A */
    u8 key2[5];             /* 0x0B */
    u8 priority;            /* 0x10 */
    u8 unk11;               /* 0x11 */
    u8 unk12;               /* 0x12 */
    u8 unk13[7];            /* 0x13 */
    u8 unk1A[2];            /* 0x1A, little-endian u16 */
    u8 unk1C;               /* 0x1C */
    u8 unk1D;               /* 0x1D */
    u8 unk1E;               /* 0x1E */
    u8 unk1F;               /* 0x1F */
    u8 unk20[6];            /* 0x20 */
    u8 unk26;               /* 0x26 */
    u8 unk27;               /* 0x27 */
    u8 unk28;               /* 0x28 */
    u8 unk29[0x15];         /* 0x29 */
    u8 unk3E;               /* 0x3E */
    u8 unk3F[0xC1];         /* 0x3F */
    Ovl15SourceRec recs[13]; /* 0x100 */
} Ovl15Source; /* known extent 0x17E */

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
    Ovl15Entry unkCC[12]; /* 0xCC */
    u8 unk75C[0x44];
    s32 unk7A0;
    u8 unk7A4[4];
    s32 unk7A8;
    u8 unk7AC[4];
    s32 unk7B0;
    s32 unk7B4;
    u8 unk7B8[0x20];
    s8 unk7D8[12]; /* 0x7D8 */
    s32 unk7E4;
    s32 unk7E8;
    s32 unk7EC[1]; /* 0x7EC; indexed 0..unk7E8-1 */
} Ovl15AssetBlock; /* known extent 0x7EC */

typedef struct {
  /* 0x00 */ u8 unknown_00[0x7A8];
  /* 0x7A8 */ s32 unknown_7A8;
} Ovl15State;

typedef struct {
    /* 0x00 */ u8 unk00[0xD];
    /* 0x0D */ u8 unkD[0xB];
    /* 0x18 */ u8 unk18[0x16];
    /* 0x2E */ u8 unk2E[5];
} Ovl15PhaseRecord; /* known extent 0x33 */

extern s32 D_ovl15_801FBB78;
extern s32 D_ovl15_801FBB7C;
extern s32 D_ovl15_801FBB80;
extern void func_ovl15_801F7664(Gfx** dl, Ovl15AssetBlock* arg1);
extern s32 D_80105218;
extern u32 controller_handle_array[];
extern s32 nuContRmbCheck(u32 contNo);
extern u16 D_800C5498[];
extern s16 D_800C54BC[];
extern Ovl15AssetBlock D_800FE450;
extern s32 D_800FE4B8[];
extern char D_80105118[];
extern char D_ovl15_801FBE4C[];
extern void func_80071608(Gfx **dl);
extern void func_ovl15_801F69B8(Gfx **dl, void *entry, Ovl15AssetBlock *blk);
extern char D_ovl15_801FBC40[];
extern char D_ovl15_801FBC4C[];
extern char D_ovl15_801FBC54[];
extern char D_ovl15_801FBC5C[];
extern char D_ovl15_801FBC78[];
extern char D_ovl15_801FBDE4[];
extern char D_ovl15_801FBDC8[];
extern char D_ovl15_801FBDD8[];
extern s32 flag_is_set(s32 flag);
extern char D_ovl15_801FBDFC[];
extern char D_ovl15_801FBE08[];
extern char D_ovl15_801FBE14[];
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
extern void func_8006A4A0(Gfx** pgfx, u32 pal, u32 addr);
extern void load_texture_block_4b(Gfx **pgfx, u32 timg, s32 fmt, s32 width, s32 height, s32 pal, s32 cms, s32 cmt, s32 masks, s32 maskt, s32 shifts, s32 shiftt);
extern void func_ovl15_801FB510(Gfx **dl, s32 a, s32 b);
extern void func_800719A0(s32 x, s32 y, s32 run, s32 size, s32 index, s32 flags,
                          u8 red, u8 green, u8 blue, u8 alpha);
extern void func_80071CE4(Gfx **dl, s32 i);

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

void func_ovl15_801F6388(Ovl15Source *src, s32 arg1, s32 arg2, s32 arg3) {
    Ovl15Entry *entry;
    s32 i;

    entry = &D_800FE450.unkCC[arg1 * 3 + arg2];
    entry->state = 1;
    for (i = 0; i != 10; i++) {
        entry->name[i] = src->name[i];
        entry->key[i] = src->name[i];
        if ((u32)(entry->name[i] - 0x61) < 0x1A) {
            entry->name[i] = entry->name[i] + 0x80;
        }
    }
    entry->unk0B = 0;
    entry->key[10] = 0;
    for (i = 0; i != 5; i++) {
        entry->unk2E[i] = i[src->key2];
    }
    entry->priority = src->priority;
    if (entry->priority >= 4) {
        entry->priority = 3;
    }
    if (flag_is_set(0x6F)) {
        entry->unk1A = 1;
    } else {
        entry->unk1A = src->unk11;
    }
    entry->unk1B = src->unk12;
    entry->unk1C = src->unk1C;
    entry->unk1D = src->unk1D;
    entry->unk1E = src->unk1E;
    entry->unk1F = src->unk1F;
    if (flag_is_set(0x88)) {
        entry->mode = 1;
    } else if (flag_is_set(0x89)) {
        entry->mode = 2;
    } else if (flag_is_set(0x8A)) {
        entry->mode = 3;
    } else if (flag_is_set(0x8B)) {
        entry->mode = 4;
    } else if (flag_is_set(0x8C)) {
        entry->mode = 5;
    } else if (flag_is_set(0x8D)) {
        entry->mode = 6;
    } else if (flag_is_set(0x8E)) {
        entry->mode = 7;
    } else {
        entry->mode = src->unk3E;
    }
    osSyncPrintf(D_ovl15_801FBDC8, (s8)entry->mode);
    entry->unk24 = (src->unk28 << 16) | (src->unk27 << 8) | src->unk26;
    entry->unk28 = 0;
    entry->unk2C = (s16)(src->unk1A[0] | (src->unk1A[1] << 8));
    osSyncPrintf(D_ovl15_801FBDD8, entry->unk28, (s8)entry->unk1C, (s8)entry->unk1D);
    for (i = 0; i != 13; i++) {
        entry->recs[i].a = src->recs[i].a[0] | (src->recs[i].a[1] << 8);
        entry->recs[i].b = src->recs[i].b[0] | (src->recs[i].b[1] << 8);
        entry->recs[i].c = src->recs[i].c;
        entry->recs[i].d = src->recs[i].d;
        osSyncPrintf(D_ovl15_801FBDD8, i, entry->recs[i].a, entry->recs[i].b);
    }
    entry->unk34 = arg3;
    entry->unk38 = arg2;
}

void func_ovl15_801F6624(void) {
}

s32 func_ovl15_801F662C(Ovl15PhaseRecord *record, u8 *key) {
    s32 i;

    osSyncPrintf(D_ovl15_801FBDE4);
    for (i = 0; i != 11; i++) {
        if (record->unkD[i] != key[i]) {
            return 0;
        }
    }
    osSyncPrintf(D_ovl15_801FBDFC);
    for (i = 0; i != 5; i++) {
        if (record->unk2E[i] != key[i + 0xB]) {
            osSyncPrintf(D_ovl15_801FBE08);
            return 0;
        }
    }
    osSyncPrintf(D_ovl15_801FBE14);
    return 1;
}

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

void func_ovl15_801F6890(void) {
    Ovl15AssetBlock *base;
    s32 var_v1;
    s32 var_a0;

    base = &D_800FE450;
    var_v1 = 0;
    var_a0 = 0;
    do {
        if (base->unkCC[var_a0].state == 1) {
            base->unk7D8[var_v1] = var_a0;
            var_v1 += 1;
        }
        var_a0 += 1;
    } while (var_a0 != 0xC);
    if (var_v1 != 0xC) {
        do {
            base->unk7D8[var_v1] = -1;
            var_v1 += 1;
        } while (var_v1 != 0xC);
    }
}

void func_ovl15_801F68FC(Gfx **dl, Ovl15State *state) {
    Gfx *gfx = *dl;
    s32 run = -1;

    switch (state->unknown_7A8) {
    case 1:
        run = 0x160;
        break;
    case 3:
        run = 0x161;
        break;
    case 5:
        run = 0x162;
        break;
    }
    if (run != -1) {
        func_800719A0(0x1C, 0xBA, run, 1, 3, 0x20, 0xFF, 0xFF, 0xFF, 0xFF);
        func_80071CE4(&gfx, 3);
    }
    *dl = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F69B8);

void func_ovl15_801F6E94(Gfx **arg0, Ovl15AssetBlock *arg1) {
    Gfx *sp28;
    void *var_s2;
    s32 var_s0;
    s32 var_a2;

    var_s2 = &D_800FF1E8;
    sp28 = *arg0;
    func_80071608(&sp28);
    gDPSetPrimColor(sp28++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
    var_s0 = 0;
    sprintf(D_80105118, D_ovl15_801FBE4C, arg1->unk7A8);
    check_and_print_grid(D_80105118, 0x14, 2);
    if (arg1->unk7A8 == 8) {
        var_s0 = 1;
        var_s2 = &D_800FF1E8[arg1->unk7B4];
    }
    /* unk7B0 holds a byte offset to another asset block; the block is reached through that offset. */
    if (((u32)(arg1->unk7A8 - 2) < 2U) && (D_800FE4B8[0x1CD] > 0)) {
        var_s0 = 1;
        var_s2 = &arg1->unkCC[((Ovl15AssetBlock *)&((u8 *)arg1)[arg1->unk7B0])->unk7D8[0]];
    }
    if (var_s0 != 0) {
        func_ovl15_801F69B8(&sp28, var_s2, arg1);
    }
    func_80071608(&sp28);
    if (arg1->unk7A0 == ((D_80105218 * 4) - 1)) {
        var_a2 = -1;
        switch (arg1->unk7A8) {
        case 2:
            var_a2 = 0x15C;
            break;
        case 8:
            var_a2 = 0x15E;
            break;
        case 7:
            var_a2 = 0x15D;
            break;
        }
        if (var_a2 != -1) {
            func_800719A0(0x1C, 0xBA, var_a2, 1, 3, 0x20, 0xFF, 0xFF, 0xFF, 0xFF);
            func_80071CE4(&sp28, 3);
        }
    }
    *arg0 = sp28;
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F7074);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F71F8);

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F7664);

void func_ovl15_801F77D4(Gfx** dl, Ovl15AssetBlock* arg1) {
    Gfx* saved = *dl;

    func_ovl15_801F7664(&saved, arg1);
    *dl = saved;
}

INCLUDE_ASM("asm/nonmatchings/overlay_15/func_ovl15_801F4A40", func_ovl15_801F780C);

void func_ovl15_801F7ED4(Gfx **dl, Ovl15AssetBlock *blk) {
    Gfx *gfx = *dl;
    s32 i;
    s32 j;

    func_ovl15_801F7664(&gfx, blk);
    if (blk->unk7A8 == 0) {
        func_ovl15_801FB510(&gfx, blk->unk7B4 * 0x28 + 0xF, 0x7F);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    }
    func_8006A4A0(&gfx, 0, (u32)blk->unk34 + 8);
    load_texture_block_4b(&gfx, (u32)blk->unk34 + 0x28, G_IM_FMT_CI, 0x20, 0x80, 0, 0, 0, 0, 0, 0, 0);
    j = 0;
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
    i = 0;
    if (blk->unk7E8 != 0) {
        do {
            if (blk->unk7A8 == 3 && j != blk->unk7E4) {
                gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0x7F, 0x7F, 0xFF);
            } else {
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
            }
            func_8006A2C0(&gfx, 0xF0 + j * 0xE0, 0x120, 0x170 + j * 0xE0, 0x1A0, 0, 0, blk->unk7EC[i] << 10, 0x400, 0x400);
            j++;
            i++;
        } while (i != blk->unk7E8);
    }
    *dl = gfx;
}

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
