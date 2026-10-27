#include "common.h"
#include "menu_memory.h"

extern s16 D_8023B640_ovl_01[][5];

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_802178B0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80217B88_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80217D20_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021826C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80219460_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80219948_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80219BD4_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021B1E0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021B618_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021B87C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021BE88_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021C200_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021C67C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021CBDC_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021CF10_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021CFAC_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021D0B8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021D3AC_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E01C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E340_ovl_01);

s16 func_8021E468_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();

    return D_8023B640_ovl_01[memory->unk3A][memory->unk39];
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E4B0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E97C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021EF64_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021F124_ovl_01);
