#include "common.h"
#include "../main/controller.h"
#include "../main/rom_asset.h"

extern s32 D_ovl14_802457E8;
extern s32 D_ovl14_802457EC;
extern void heap3_free(void** payload_address);
extern void* heap3_alloc(u32 size);
extern s8 g_terrain_vtx_xform_mode;
extern s32 func_80051BFC(void);
extern void play_bgm_by_id(s32 id, s32 flags);
extern s32 D_ovl14_802457E0;
extern s32 D_ovl14_80245860;
extern void* D_ovl14_80245AB0;
extern s32 D_ovl14_80245A20;
extern s32 D_800C0E60;
extern s32 D_800C306C;
extern s8 D_801061BD;
extern s8 D_801061BE;
extern s8 D_801061BF;
extern s8 D_801061C0;
extern s8 D_801061C1;
extern s8 D_801061C2;
extern s8 D_801061C3;
extern s8 D_801061C4;
extern s8 D_801061C5;
extern void func_8002A97C(s32 sky_mode, s32 blend_countdown);
extern void func_80050DA0(s32 sfx, s32 channel, s32 volume, s32 reverb, s32 pan);
extern void func_800934CC(s32 arg0);
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);
extern void set_color_framebuffer_count(u32 buffer_count);

typedef struct {
    /* 0x00 */ u8 unknown_00[4];
    /* 0x04 */ s32 kind;
    /* 0x08 */ u8 unknown_08[0x32];
    /* 0x3A */ s8 flag;
} Overlay14Entry;

s32 func_ovl14_80241800(Overlay14Entry *entry) {
    if (entry->flag != 0) {
        return 5;
    }
    switch (entry->kind) {
    case 5:
        return 0;
    case 4:
        return 1;
    case 0:
        return 2;
    case 2:
        return 6;
    case 10:
        return 7;
    case 11:
        return 10;
    default:
        return -1;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_8024186C);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80241E54);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80242050);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80242294);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80243698);

void func_ovl14_80243880(void) {
    D_ovl14_802457E8 = 0;
    D_ovl14_802457EC = 5;
}

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80243898);

void func_ovl14_802439B0(void) {
    D_ovl14_802457E8 = 0;
    D_ovl14_802457EC = 5;
}

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_802439C8);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80243E28);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80244134);

void func_ovl14_80245260(void) {
    RomAssetLoad asset;
    void* payload;

    if (g_terrain_vtx_xform_mode != 0) {
        payload = heap3_alloc(get_rom_asset_allocation_size(0x96A, &asset));
        D_ovl14_80245AB0 = payload;
        load_rom_asset_payload(payload, &asset);
    } else {
        payload = heap3_alloc(get_rom_asset_allocation_size(0x92A, &asset));
        D_ovl14_80245AB0 = payload;
        load_rom_asset_payload(payload, &asset);
    }
    D_ovl14_80245A20 = 0xF0;
    D_ovl14_802457E0 = 1;
    D_ovl14_802457E8 = 0;
    D_ovl14_80245860 = func_80051BFC();
    play_bgm_by_id(0x17, 0x11);
}

void func_ovl14_802452E4(void) {
    heap3_free(&D_ovl14_80245AB0);
    D_ovl14_802457E0 = 0;
    play_bgm_by_id(D_ovl14_80245860, 0x11);
}

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80245320);

void func_ovl14_80245598(ControllerState *input) {
    D_ovl14_802457E8 += 1;
    switch (D_ovl14_802457EC) {
    case 0: {
        s32 *a20 = &D_ovl14_80245A20;

        if (*a20 > 0) {
            if (*a20 == 1) {
                *a20 = 0;
            } else {
                *a20 += (s32) -*a20 / 2;
            }
        }
        if (D_ovl14_802457E8 == 3) {
            func_80050DA0(0x72, D_800C0E60, 0x64, 0x14, 0x7F);
        }
        if (D_ovl14_802457E8 == 0xE) {
            D_ovl14_802457EC = 1;
            return;
        }
        return;
    }
    case 1:
        if (input->pressed & 0xC000) {
            play_sound_effect(0x5D, D_800C0E60, 0x64);
            D_ovl14_802457E8 = 0;
            D_ovl14_802457EC = 2;
            return;
        }
        break;
    case 2: {
        s32 *a20 = &D_ovl14_80245A20;

        if (*a20 >= -1) {
            *a20 = *a20 - 4;
        } else {
            *a20 = *a20 + *a20 / 2;
        }
        if (D_ovl14_802457E8 == 3) {
            func_80050DA0(0x73, D_800C0E60, 0x64, 0x14, 0x7F);
        }
        if (D_ovl14_802457E8 == 0xE) {
            D_ovl14_802457EC = 3;
            return;
        }
        break;
    }
    case 3:
        func_ovl14_802452E4();
        D_800C306C = 0;
        func_800934CC(-1);
        set_color_framebuffer_count(3);
        func_8002A97C(4, 0);
        D_801061BD = 1;
        D_801061C4 = 1;
        D_801061BF = 1;
        D_801061BE = 0;
        D_801061C0 = 1;
        D_801061C1 = 1;
        D_801061C2 = 1;
        D_801061C3 = 1;
        D_801061C5 = 1;
        break;
    }
}
