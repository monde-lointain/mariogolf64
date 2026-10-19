#include "common.h"
#include "../main/rom_asset.h"

extern u16 D_800C2064[];
extern void** D_ovl3_8023AA40;

void func_ovl3_80213390(void** destinations) {
    RomAssetLoad asset;
    s16 i;

    D_ovl3_8023AA40 = destinations;
    for (i = 0; i != 6; i++) {
        get_rom_asset_allocation_size(D_800C2064[i], &asset);
        load_rom_asset_payload(D_ovl3_8023AA40[i], &asset);
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_8021342C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_80213464);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_80213558);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_80213748);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_802144E4);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_802149A8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_8021508C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80213390", func_ovl3_8021585C);
