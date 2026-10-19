#include "common.h"
#include "../main/terrain.h"

extern u8 D_80451A60[];

u8* func_ovlgap_80400000(void) {
    return D_80451A60;
}

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_8040000C);

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

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80407D64);

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
