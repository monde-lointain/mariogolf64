#include "common.h"
#include "../main/rom_asset.h"

extern s32 flag_is_set(s32 flag);
extern void check_and_print_grid(char* str, s32 col, s32 row);
extern char D_80105118[];
extern s32 D_ovl0_80224BD0;
extern char D_ovl0_80225690[];
extern u16 debug_menu_pad_buttons_c;
extern u8 D_ovl0_80224B23;
extern s32 D_ovl0_80224B24;
extern void* heap3_alloc(u32 size);
extern void heap3_free(void** payload_ptr);
extern void* D_801B5CD0[];
extern void* D_ovl0_8022D690;
extern void* D_ovl0_8022D694;
extern u8 D_ovl0_80224B20;
extern u8 D_ovl0_80224B21;
extern u8 D_ovl0_80224B22;
extern s32 D_ovl0_80224B2C;
extern s8 D_801061C4;
void func_ovl0_8021EA58(Gfx** gfxp);

void func_ovl0_8021E550(void) {
    u16 buttons;

    if (flag_is_set(0x6E) != 0) {
        buttons = debug_menu_pad_buttons_c;
        if (buttons & 0x800) {
            D_ovl0_80224BD0 -= 1;
        }
        if (buttons & 0x400) {
            D_ovl0_80224BD0 += 1;
        }
        sprintf(D_80105118, D_ovl0_80225690, D_ovl0_80224BD0);
        check_and_print_grid(D_80105118, 0x14, 5);
    }
}

void func_ovl0_8021E5F4(void) {
    RomAssetLoad asset;
    void* temp_v0;
    void* temp_v0_2;
    void* temp_v0_3;

    temp_v0 = heap3_alloc(get_rom_asset_allocation_size(0x4FF, &asset));
    D_ovl0_8022D690 = temp_v0;
    load_rom_asset_payload(temp_v0, &asset);
    temp_v0_2 = heap3_alloc(get_rom_asset_allocation_size(0x56C, &asset));
    D_801B5CD0[12] = temp_v0_2;
    load_rom_asset_payload(temp_v0_2, &asset);
    temp_v0_3 = heap3_alloc(get_rom_asset_allocation_size(0x56C, &asset));
    D_801B5CD0[13] = temp_v0_3;
    load_rom_asset_payload(temp_v0_3, &asset);
    D_ovl0_8022D694 = heap3_alloc(0x808);
    D_ovl0_80224B20 = 0xFF;
    D_ovl0_80224B21 = 0xFF;
    D_ovl0_80224B22 = 0xFF;
    D_ovl0_80224B2C = 1;
}

void func_ovl0_8021E6B8(void) {
    D_ovl0_80224B2C = -1;
    heap3_free(&D_ovl0_8022D690);
    heap3_free(&D_ovl0_8022D694);
    heap3_free(&D_801B5CD0[12]);
    heap3_free(&D_801B5CD0[13]);
}

void func_ovl0_8021E710(void) {
    D_ovl0_80224B24 = 0;
    D_ovl0_80224B23 = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8021E550", func_ovl0_8021E724);

extern u8 D_ovl0_80224B23;

s32 func_ovl0_8021EA48(void) {
    return D_ovl0_80224B23 == 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8021E550", func_ovl0_8021EA58);

void func_ovl0_8021FC20(Gfx** gfxp) {
    Gfx* gfx;

    gfx = *gfxp;
    if (D_801061C4 != 0) {
        func_ovl0_8021E550();
        func_ovl0_8021EA58(&gfx);
        *gfxp = gfx;
    }
}
