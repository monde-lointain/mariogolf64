#include "common.h"
#include "../main/terrain.h"

extern TerrainAttrEntry D_ovl7_8020B564[27];

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206680);

TerrainAttrEntry* func_ovl7_802066F0(u32 arg0) {
  u32 i = (arg0 < 0x1BU) ? arg0 : 0;
  return &D_ovl7_8020B564[i];
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206718);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206800);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_802068B4);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_802068EC);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206910);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206968);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206A50);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206AA4);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_8020A474);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_8020A488);
