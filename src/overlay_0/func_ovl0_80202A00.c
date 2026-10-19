#include "common.h"
#include "../main/rom_asset.h"
#include "../main/session.h"

extern s32 nuSiSendMesg(s16 mesg, void* dataPtr);

void func_ovl0_80202A00(void) {
    nuSiSendMesg(0x305, 0);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80202A20);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80202D04);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802038D0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80203B4C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80203DAC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80203E58);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80203EEC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80203F30);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80204104);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802041C8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80204210);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020439C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80204460);

extern s8 D_801B55D9;
extern s32 D_800FBE7C[];

void func_ovl0_80204568(void) {
    s8 input;
    s32 magnitude;
    s32 *value;

    input = D_801B55D9;
    magnitude = input;
    if (input < 0) {
        magnitude = -input;
    }
    if (magnitude >= 9) {
        value = &D_800FBE7C[2];
        *value -= (input / 16) * 0x600;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802045BC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020466C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80204910);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802054CC);

extern u32 D_ovl0_80227198;

void func_ovl0_802055F8(void) {
    D_ovl0_80227198 = osGetCount();
}

s32 func_ovl0_8020561C(s32 arg0) {
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80205624);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802059C0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80205AE4);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80205BB8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80205C78);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80205D44);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80205DE8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80206354);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80206BD0);

extern s32 game_mode;
extern s8 rumble_disable_flag;

s32 func_ovl0_80206CC8(void) {
    if (rumble_disable_flag == 0) {
        return game_mode == 9;
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80206CF0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80206E88);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207014);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207048);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207080);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802071B8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207250);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802072AC);

extern void animation_play(s32 id, s32 animation);
s32 func_ovl0_80207250(void);
s32 func_ovl0_802072AC(s32 arg0, s32 arg1, s32 arg2);

void func_ovl0_80207328(s32 arg0) {
    animation_play(arg0, func_ovl0_80207250());
}

void func_ovl0_80207358(s32 arg0, s32 arg1, s32 arg2) {
    animation_play(arg0, func_ovl0_802072AC(arg0, arg1, arg2));
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207388);

void func_ovl0_80207660(void) {
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207668);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802078C4);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802078F4);

extern s32 clear_animation_slot(s32 index);
extern void func_80095D64(void);

void func_ovl0_80207A10(s32 arg0) {
    clear_animation_slot(0);
    func_80095D64();
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207A34);

void func_ovl0_80207B8C(s32 player_index) {
    if (player_records[player_index].unknown_80 == -2) {
        player_records[player_index].unknown_80 = -1;
    }
    if (player_records[player_index].unknown_84 == -2) {
        player_records[player_index].unknown_84 = -1;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207BEC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207CAC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207E88);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207F70);

/* Returns whether bit 0x4000 of D_80104990 is set. The dead 8-byte frame with
 * the never-reloaded `sw $v0,0(sp)` is the same nested-function stand-in as
 * lerp_s32 (func_8008D100.c); parent func_ovl0_802088B0 is not recovered. */
extern u16 D_80104990;

s32 func_ovl0_802080B4(void) {
  s32 r;
  volatile s32 unused = r;
  r = D_80104990;
  r &= 0x4000;
  return r != 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802080D4);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80208120);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020816C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802081F8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80208354);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020845C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802087D0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80208810);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80208850);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802088B0);

extern void* heap3_alloc(u32 size);
extern void* D_ovl0_80227C90[2];
extern void* D_ovl0_80227C98;
extern s32 D_ovl0_80224034;

void func_ovl0_8020D6F0(void) {
    RomAssetLoad asset;
    void* temp_v0;
    void* temp_v0_2;
    void* temp_v0_3;

    temp_v0 = heap3_alloc(get_rom_asset_allocation_size(0x6E0, &asset));
    D_ovl0_80227C90[0] = temp_v0;
    load_rom_asset_payload(temp_v0, &asset);
    temp_v0_2 = heap3_alloc(get_rom_asset_allocation_size(0x6E5, &asset));
    D_ovl0_80227C90[1] = temp_v0_2;
    load_rom_asset_payload(temp_v0_2, &asset);
    temp_v0_3 = heap3_alloc(get_rom_asset_allocation_size(0x6F0, &asset));
    D_ovl0_80227C98 = temp_v0_3;
    load_rom_asset_payload(temp_v0_3, &asset);
    D_ovl0_80224034 = 0;
}

extern void heap3_free(void** payload_ptr);

void func_ovl0_8020D784(void) {
    heap3_free(&D_ovl0_80227C90[0]);
    heap3_free(&D_ovl0_80227C90[1]);
    heap3_free(&D_ovl0_80227C98);
}

extern f32 atan2f(f32 y, f32 x);
extern f32 D_801B54F8;
extern f32 view_camera_eye_x;
extern f32 D_801B5500;
extern f32 view_camera_eye_z;

f32 func_ovl0_8020D7C4(void) {
    f32 dx = D_801B54F8 - view_camera_eye_x;
    f32 angle = 0.0f;
    f32 dz = D_801B5500 - view_camera_eye_z;

    if ((s32)dx != 0 || (s32)dz != 0) {
        angle = atan2f(dx, dz);
    }
    return angle;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020D838);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020D874);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020D9F0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020DA18);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020DCA4);
