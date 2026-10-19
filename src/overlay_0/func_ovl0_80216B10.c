#include "common.h"
#include "../main/rom_asset.h"

extern s32 D_800C1B70;
extern s32 D_ovl0_802246F8;
extern s32 D_ovl0_80224860;
extern u8 D_ovl0_802291D8;
extern s8 D_ovl0_80229236;
extern s32 D_800FBE64;
extern s32 D_800FBE68;
extern s32 target_z;
extern s32 D_800C1B74;
extern u8 D_ovl0_80229240;
extern s32 D_ovl0_80228F80[30][2];
extern f32 D_ovl0_80229070[30][2];
extern f32 D_ovl0_80229160[30];
extern u8 D_ovl0_802291F8[30];
extern u8 D_ovl0_80229218[30];
extern f64 D_ovl0_80225428;
extern f64 D_ovl0_80225430;

void func_ovl0_80216B10(void) {
    s32 i;

    i = 0;
    D_ovl0_80224860 = 0;
    D_800C1B70 = 0;
    D_ovl0_802246F8 = -1;
    do {
        *(&D_ovl0_80229236 + i) = -1;
        i += 1;
    } while (i != 5);
    i = 0;
    do {
        *(&D_ovl0_802291D8 + i) = 0;
        i += 1;
    } while (i != 0x1E);
}

extern void func_80078D94(void);
extern void func_8008679C(s32 mode);
extern void func_ovl0_801FD8DC(void);
extern void func_ovl0_8020D9F0(void);
extern void func_ovl0_8020E7B4(s32 arg0);
extern void func_ovl0_8020FFEC(s32 arg0);
extern void func_ovl0_80213D70(s32 arg0);
extern void func_ovl0_80216130(s32 arg0);
extern void func_ovl0_80217F5C(s32 arg0);
extern s8 D_ovl0_80225410;

void func_ovl0_80216B74(s32 arg0) {
    osSyncPrintf(&D_ovl0_80225410, arg0);
    func_ovl0_80216130(-1);
    func_8008679C(-1);
    func_80078D94();
    D_ovl0_80224860 = 0;
    D_800C1B70 = 0;
    func_ovl0_8020D9F0();
    func_ovl0_80213D70(-1);
    if (arg0 == 2) {
        func_ovl0_8020E7B4(-2);
    }
    if (arg0 == 1) {
        func_ovl0_8020FFEC(-1);
        func_ovl0_8020E7B4(-1);
    }
    func_ovl0_801FD8DC();
    func_ovl0_80217F5C(-1);
}

extern void update_object_group_by_id(s32 index);

void func_ovl0_80216C18(void) {
    s32 i;

    i = 0;
    do {
        update_object_group_by_id(i);
        i += 1;
    } while (i != 0x10);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80216B10", func_ovl0_80216C58);

s32 func_ovl0_80216C58(void);

void func_ovl0_80216CAC(void) {
    s32 spawned;
    s32 slot;
    s32 i;
    s32 off;
    f32 speed;

    spawned = 0;
    D_ovl0_80224860 = 1;
    slot = func_ovl0_80216C58();
    if (slot != -1) {
        off = slot * 12;
        *(s32 *)(&D_ovl0_80229240 + off) = D_800FBE64;
        *(s32 *)(&D_ovl0_80229240 + off + 4) = D_800FBE68;
        *(s32 *)(&D_ovl0_80229240 + off + 8) = target_z;
        i = 0;
        do {
            if (*(&D_ovl0_802291D8 + i) == 0) {
                D_ovl0_80228F80[i][0] = 0;
                D_ovl0_80228F80[i][1] = 0;
                D_ovl0_80229160[i] = (f32)((f64)(f32)spawned * D_ovl0_80225428 +
                                           (f64)((guRandom() % 20) - 10) * D_ovl0_80225430);
                D_ovl0_802291F8[i] = 1;
                speed = (f32)(guRandom() % 16) + 8.0f;
                D_ovl0_80229070[i][0] = sinf(D_ovl0_80229160[i]) * speed;
                spawned += 1;
                D_ovl0_80229070[i][1] = -cosf(D_ovl0_80229160[i]) * speed;
                *(&D_ovl0_802291D8 + i) = 10;
                D_ovl0_80229218[i] = slot;
            }
            if (spawned == 0x14) {
                break;
            }
            i += 1;
        } while (i != 0x1E);
    }
    D_800C1B70 = 2;
    D_800C1B74 = 0xFF;
}

extern void* heap3_alloc(u32 size);
extern void* D_ovl0_8022927C;
extern void* D_ovl0_80229280;
extern u8* D_8012D3C0;

void func_ovl0_80216EDC(void) {
  RomAssetLoad asset;
  void* temp_v0;
  void* temp_v0_2;

  temp_v0 = heap3_alloc(get_rom_asset_allocation_size(0x561, &asset));
  D_ovl0_8022927C = temp_v0;
  load_rom_asset_payload(temp_v0, &asset);
  temp_v0_2 = heap3_alloc(get_rom_asset_allocation_size(0x973, &asset));
  D_ovl0_80229280 = temp_v0_2;
  load_rom_asset_payload(temp_v0_2, &asset);
  D_8012D3C0 = heap3_alloc(0x2180);
  get_rom_asset_allocation_size(0x6AD, &asset);
  load_rom_asset_payload(D_8012D3C0, &asset);
  get_rom_asset_allocation_size(0x969, &asset);
  load_rom_asset_payload(D_8012D3C0 + 0x648, &asset);
}

extern void heap3_free(void** payload_ptr);

void func_ovl0_80216F8C(void) {
  heap3_free(&D_ovl0_8022927C);
  heap3_free(&D_ovl0_80229280);
  heap3_free((void**)&D_8012D3C0);
}

void func_ovl0_80216FC4(void) {
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80216B10", func_ovl0_80216FCC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80216B10", func_ovl0_80217164);

void func_ovl0_80217F44(void) {
}

void func_ovl0_80217F4C(void) {
}

void func_ovl0_80217F54(void) {
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80216B10", func_ovl0_80217F5C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80216B10", func_ovl0_80218238);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80216B10", func_ovl0_8021857C);
