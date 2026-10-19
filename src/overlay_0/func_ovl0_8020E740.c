#include "common.h"
#include "../main/rom_asset.h"

extern void* heap3_alloc(u32 size_bytes);
extern void heap3_free(void** payload_ptr);
extern s32 D_ovl0_80224114;
extern s32 D_ovl0_80224120;
extern s32 D_ovl0_80224124;
extern void* D_ovl0_8022D650;

void func_ovl0_8020E740(void) {
  RomAssetLoad asset;

  D_ovl0_80224114 = -1;
  D_ovl0_80224124 = 1;
  D_ovl0_8022D650 = heap3_alloc(0x6270);
  D_ovl0_80224120 = -1;
}

void func_ovl0_8020E78C(void) {
  D_ovl0_80224124 = 0;
  heap3_free(&D_ovl0_8022D650);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8020E7B4);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8020EBBC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8020ED54);

s32 func_ovl0_8020EEEC(s32* arg0) { return *arg0 = *arg0; }

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8020EEF8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8020FAE8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8020FC10);

extern s32 D_ovl0_80224410;

s32 func_ovl0_8020FFD8(void) { return D_ovl0_80224410 == -1; }

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8020FFEC);

extern s32 D_ovl0_80224418;

s32 func_ovl0_8021035C(void) { return D_ovl0_80224418 >= 0xC; }

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80210370);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_802104BC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80210F40);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8021169C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80211D60);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80212590);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_802129C0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80212CE0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80213038);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80213634);

extern s32 D_ovl0_802246F8;

s32 func_ovl0_80213C64(void) { return D_ovl0_802246F8 != -1; }

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80213C78);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80213D70);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_8021419C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80214250);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_80216130);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8020E740", func_ovl0_802161D4);
