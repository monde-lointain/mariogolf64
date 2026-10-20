#include "common.h"
#include "../main/rom_asset.h"
#include "../main/session.h"
#include "../main/club.h"
#include "../main/terrain.h"

typedef struct {
    f32 target_z;
    f32 angle;
} CameraLookTarget;

extern s8 player_base_scores[];
extern s8 D_800BAA04;
extern s32 game_mode;
extern void func_ovl0_8020E7B4(s32 arg0);
extern s32 func_ovl0_80207048(PlayerState *player);
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);
extern f32 D_801B54F8;
extern CameraLookTarget D_801B5500;
extern s32 D_800C308C;
extern s32 active_club_id;
extern s32 active_meter_category;
extern s32 active_terrain_index;

void func_ovl0_80207F70(s32 player_index) {
    s32 diff;
    s32 score;
    s32 sfx;
    s32 s0;

    score = player_base_scores[player_index * sizeof(PlayerState)];
    diff = score - D_800BAA04;
    s0 = -1;
    if (game_mode == 7) {
        func_ovl0_8020E7B4(9);
        sfx = 0x90;
        goto play;
    }
    if (game_mode == 9) {
        if (func_ovl0_80207048(&player_records[player_index]) == 0) {
            goto done;
        }
        func_ovl0_8020E7B4(0xA);
        sfx = 0x96;
        goto play;
    }
    if (score == 1) {
        s0 = 0xB;
        sfx = 0x84;
        goto play;
    }
    switch (diff + 3) {
    case 2:
        s0 = 0xE;
        sfx = 0x87;
        goto play;
    case 1:
        s0 = 0xD;
        sfx = 0x86;
        goto play;
    case 0:
        s0 = 0xC;
        sfx = 0x85;
        goto play;
    case 3:
        s0 = 0xF;
        sfx = 0x88;
        goto play;
    case 4:
        s0 = 0x10;
        sfx = 0x98;
    play:
        play_sound_effect(sfx, 3, 0x7F);
        goto done;
    case 5:
        s0 = 0x12;
        goto done;
    case 6:
        s0 = 0x13;
        goto done;
    default:
        s0 = diff + 0x10;
        goto done;
    }
done:
    if (s0 >= 0) {
        func_ovl0_8020E7B4(s0);
    }
}

/* First four bytes of the "BOGEY\n" string at DE3C4. KMC pads .rodata to 8 bytes,
 * so the jump table above (ends at DE3C4) needs real data here to avoid a 4-byte pad.
 * The rest of the string stays raw in asm/data. func_ovl0_802088B0 takes its address. */
const char D_ovl0_802250C4[4] = { 'B', 'O', 'G', 'E' };

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

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80207F70", func_ovl0_802080D4);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80207F70", func_ovl0_80208120);

extern void func_8005637C(s32 id, u8 value);

/* Nested-function original (parent func_ovl0_802088B0 not recovered): the
 * uninitialized `chain` stands in for the static chain in $v0, saved by the
 * dead `unused` store and dereferenced for the parent's player index. Reusing
 * `chain` for the flag test ties it to $v0. Sets the active player's
 * character unknown_60 flag from the club's flag bit 0 unless D_800C308C is 4. */
void func_ovl0_8020816C(void) {
    s32 *chain;
    s32 * volatile unused = chain;
    s32 flags;

    flags = get_club_param(player_records[*chain].club_ids[active_club_id])->flags;
    chain = (s32 *)(flags & 1);
    if (chain && (D_800C308C != 4)) {
        func_8005637C(0, 1);
    } else {
        func_8005637C(0, 0);
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80207F70", func_ovl0_802081F8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80207F70", func_ovl0_80208354);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80207F70", func_ovl0_8020845C);

extern s8 D_801061BF;
extern s8 D_801061C0;
extern s8 D_801061C1;
extern s8 D_801061C2;
extern s8 D_801061C3;
extern s8 D_801061C5;

/* Dead 8-byte frame and `sw $v0,0(sp)`: static-chain home of the nested-function original (parent func_ovl0_802088B0 is not recovered); `unused` reproduces them, as in func_ovl7_802068B4. */
void func_ovl0_802087D0(s8 arg0) {
    s32 r;
    volatile s32 unused = r;

    D_801061BF = arg0;
    D_801061C0 = arg0;
    D_801061C1 = arg0;
    D_801061C2 = arg0;
    D_801061C3 = arg0;
    D_801061C5 = arg0;
}

extern s32 D_ovl0_8022715C;
extern u8* func_80037E50(ClubShot* club, s32 arg1, s32 arg2, s32 arg3);
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);
extern s32 D_800C0E60;

/* Dead `sw $v0,0x10($sp)` in the first jal's delay slot: the `unused` local is the stand-in, as in lerp_s32 (func_8008D100.c). */
s32 func_ovl0_80208810(void) {
    s32 r;
    volatile s32 unused = r;
    return ((s8 *)func_80037E50(get_shot_data(), active_terrain_index, active_meter_category, D_ovl0_8022715C))[5];
}

/* Dead `sw $v0,0x10($sp)` in the first jal's delay slot: the `unused` local is the stand-in, as in func_ovl0_80208810 above. */
void func_ovl0_80208850(void) {
    s32 r;
    volatile s32 unused = r;

    if (func_ovl0_80208810() != 0) {
        play_sound_effect(0x63, D_800C0E60, 0x64);
    } else if (D_ovl0_8022715C == 0) {
        play_sound_effect(0x62, D_800C0E60, 0x64);
    } else {
        play_sound_effect(0x6E, D_800C0E60, 0x64);
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80207F70", func_ovl0_802088B0);

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
extern f32 view_camera_eye_x;
extern f32 view_camera_eye_z;

f32 func_ovl0_8020D7C4(void) {
    f32 dx = D_801B54F8 - view_camera_eye_x;
    f32 angle = 0.0f;
    f32 dz = D_801B5500.target_z - view_camera_eye_z;

    if ((s32)dx != 0 || (s32)dz != 0) {
        angle = atan2f(dx, dz);
    }
    return angle;
}

typedef struct {
    Mtx matrix_a; /* 0x00 */
    Mtx matrix_b; /* 0x40 */
    s32 unk80;
    s32 unk84;
    s32 unk88;
    u8 unk8C[4];
    s32 unk90;
    u8 unk94[8];
    f32 unk9C;
    u8 unkA0[4];
    u8 unkA4;
    u8 unkA5;
    u8 unkA6;
    u8 kind; /* 0xA7; 0 = unused slot, 2 = set by func_ovl0_8020D874 */
} OverlaySlot; /* size 0xA8 */

extern OverlaySlot D_ovl0_8022B5D0[10];

s32 func_ovl0_8020D838(void) {
    s32 i;

    for (i = 0; i != 10; i++) {
        if ((D_ovl0_8022B5D0 + i)->kind == 2) {
            return 1;
        }
    }
    return 0;
}

extern void convert_and_pack_floats_to_fixed(f32 matrix[4][4], Mtx* fixed_matrix);
extern void mtx_from_rts(f32 matrix[4][4], f32 rotation[3], f32 translation[3], f32 scale[3]);
extern u16 D_ovl0_8022402C;
extern f64 D_ovl0_80225270;

void func_ovl0_8020D874(f32 arg0) {
    RomAssetLoad asset;
    f32 translation[3];
    f32 rotation[3];
    f32 scale[3];
    f32 matrix[4][4];
    s32 i;
    OverlaySlot* slot;

    if (func_ovl0_8020D838() == 0) {
        get_rom_asset_allocation_size(D_ovl0_8022402C, &asset);
        load_rom_asset_payload(D_ovl0_80227C98, &asset);
        for (i = 0; i != 10; i++) {
            slot = &D_ovl0_8022B5D0[i];
            if (slot->kind == 0) {
                slot->kind = 2;
                slot->unkA6 = 0;
                slot->unkA5 = 0;
                slot->unkA4 = 0;
                slot->unk90 = 0;
                slot->unk80 = hole_environment.pin_position.x;
                slot->unk84 = hole_environment.pin_position.y - 0x9600;
                slot->unk88 = hole_environment.pin_position.z;
                slot->unk9C = (f32)((f64)-arg0 + D_ovl0_80225270);
                translation[0] = (f32)slot->unk80 * 0.0009765625f;
                translation[1] = (f32)slot->unk84 * 0.0009765625f;
                translation[2] = (f32)slot->unk88 * 0.0009765625f;
                rotation[0] = 0.0f;
                rotation[1] = slot->unk9C;
                rotation[2] = 0.0f;
                scale[0] = 1.0f;
                scale[1] = 1.0f;
                scale[2] = 1.0f;
                mtx_from_rts(matrix, rotation, translation, scale);
                convert_and_pack_floats_to_fixed(matrix, &slot->matrix_a);
                convert_and_pack_floats_to_fixed(matrix, &slot->matrix_b);
                return;
            }
        }
    }
}

void func_ovl0_8020D9F0(void) {
    s32 i;

    for (i = 0; i != 10; i++) {
        (D_ovl0_8022B5D0 + i)->kind = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80207F70", func_ovl0_8020DA18);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80207F70", func_ovl0_8020DCA4);
