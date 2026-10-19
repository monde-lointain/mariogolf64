#include "common.h"
#include "../main/terrain.h"
#include "../main/club.h"

extern TerrainAttrEntry D_ovl7_8020B564[27];
extern s32 rand(void);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206680);

TerrainAttrEntry* func_ovl7_802066F0(u32 arg0) {
  u32 i = (arg0 < 0x1BU) ? arg0 : 0;
  return &D_ovl7_8020B564[i];
}

void func_ovl7_80206718(void) {
  s32 i;

  for (i = 0; i <= 0; i++) {
    D_800B7868[i].unknown_00 = 0x800;
    D_800B7868[i].unknown_04 = -1000;
    rand();
    D_800B7868[i].unknown_08 = 0x1000;
    D_800B7868[i].unknown_0C = rand() & 0x3FF;
    D_800B7868[i].unknown_10 = -100;
    D_800B7868[i].unknown_14 = rand() & 0x3FF;
    D_800B7868[i].unknown_18 = 0;
    D_800B7868[i].unknown_74 = 1;
  }
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206800);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_802068B4);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_802068EC);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206910);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206968);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206A50);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206AA4);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_8020A474);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_8020A488);
