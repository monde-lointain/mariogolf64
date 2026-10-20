#include "common.h"
#include "../main/controller.h"
#include "../main/rom_asset.h"
#include "../main/session.h"

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
extern void func_80051A88(s32 volume);
extern void func_800719A0(s32 x, s32 y, s32 run, s32 size, s32 index, s32 flags,
                          u8 red, u8 green, u8 blue, u8 alpha);
extern s32 D_ovl14_802457E4;
extern s32 D_ovl14_802457DC;
extern void* D_ovl14_80245A38[];
extern void* D_ovl14_80245A68[];
extern void* D_ovl14_80245A98[3][2];
extern void* D_ovl14_80245AB8[6];
extern void* D_ovl14_80245AD0;
extern void* D_ovl14_80245AD4;
extern u16 D_800C547C[];
extern s16 D_800C54A0[];
extern SupershotRecord D_80105F40[];
extern u8* func_8005AF50(void);
extern void func_8005B28C(s32 idx, SupershotRecord* src);
extern void func_8005D0D8(void);
extern void func_8005DE60(void);

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

void func_ovl14_80241E54(void) {
    s32 i;

    func_80051A88(0x7F);
    heap3_free(&D_ovl14_80245AB0);
    heap3_free(&D_ovl14_80245AD4);
    if (D_ovl14_802457DC == 1) {
        for (i = 0; i != 3; i++) {
            heap3_free(&D_ovl14_80245A98[i][0]);
            heap3_free(&D_ovl14_80245A98[i][1]);
        }
        i = 0;
    } else {
        for (i = 0; i != 6; i++) {
            heap3_free(&D_ovl14_80245AB8[i]);
        }
        heap3_free(&D_ovl14_80245AD0);
        i = 0;
    }
    for (i = 0; i != 12; i++) {
        heap3_free(&D_ovl14_80245A68[i]);
        heap3_free(&D_ovl14_80245A38[i]);
    }
    D_ovl14_802457DC = 0;
    func_800719A0(0xA0, 0x72, -1, 1, 0, 0, 0, 0, 0, 0);
    func_800719A0(0xA0, 0x72, -1, 1, 1, 0, 0, 0, 0, 0);
    func_800719A0(0xA0, 0x72, -1, 1, 2, 0, 0, 0, 0, 0);
    func_800719A0(0xA0, 0x72, -1, 1, 3, 0, 0, 0, 0, 0);
    func_800719A0(0xA0, 0x72, -1, 1, 4, 0, 0, 0, 0, 0);
}

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80242050);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80242294);

INCLUDE_ASM("asm/nonmatchings/overlay_14/func_ovl14_80241800", func_ovl14_80243698);

void func_ovl14_80243880(void) {
    D_ovl14_802457E8 = 0;
    D_ovl14_802457EC = 5;
}

/* Selects the supershot entry at D_ovl14_802457E4. When its bit 7 validation
 * flag is set, loads that entry's asset payload and reads its ROM slot. */
void func_ovl14_80243898(void) {
    RomAssetLoad asset_load;
    RomLoadSlot rom_slot;
    SupershotRecord* record;
    u8* base;
    u32 size;
    s32 state;

    base = func_8005AF50();
    record = &((SupershotRecord*)(base + 0x1DF0))[D_ovl14_802457E4];
    func_8005B28C(D_ovl14_802457E4, D_80105F40);
    D_ovl14_802457E8 = 0;
    func_8005D0D8();
    if ((s8)record->validation_flags & 0x80) {
        get_rom_asset_allocation_size(
            D_800C547C[(s8)record->character_index] + 9, &asset_load);
        load_rom_asset_payload(D_ovl14_80245A68[D_ovl14_802457E4], &asset_load);
        open_rom_asset_slot(
            D_800C54A0[(s8)record->character_index] + (s8)record->asset_variant,
            &rom_slot);
        size = get_rom_load_slot_size(&rom_slot);
        read_rom_load_slot(D_ovl14_80245A38[D_ovl14_802457E4], size, &rom_slot);
        state = 4;
    } else {
        state = 1;
    }
    D_ovl14_802457EC = state;
    func_8005DE60();
}

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
