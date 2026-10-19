#include "common.h"
#include "../main/rom_asset.h"

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80216B10", func_ovl0_80216B10);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80216B10", func_ovl0_80216B74);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80216B10", func_ovl0_80216C18);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80216B10", func_ovl0_80216C58);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80216B10", func_ovl0_80216CAC);

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
