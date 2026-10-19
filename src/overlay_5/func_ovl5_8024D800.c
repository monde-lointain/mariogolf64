#include "common.h"
#include "../main/session.h"
#include "../main/club.h"
#include "../main/vector.h"

extern s8 D_ovl5_8025C848;
extern s32 D_ovl5_80253C64;
extern OSThread D_ovl5_80254300;
extern u64 D_ovl5_8025A640[0x2000 / sizeof(u64)];
extern s32 D_ovl5_8025C874;
extern s32 D_ovl5_8025C87C;
extern s8 D_ovl5_80253B52[];
extern s32 D_ovl5_8025C884;
extern s8 D_800BAA04;
extern s32 D_ovl5_8025C640;
extern s32 camera_position_x;
extern s32 camera_position_z;
extern u32 calculate_hypotenuse_safe(s32, s32);
void func_ovl5_8024D870(void *);

typedef struct {
    s32 unknown_00[22];
} OverlayRecord58;

extern OverlayRecord58 D_ovl5_802544B0;

s32 func_ovl5_8024DBF8(OverlayRecord58 *);
s32 func_ovl5_80251378(Vec3i *from, Vec3i *to, Vec3i *out);

void func_ovl5_8024D800(void) { D_ovl5_8025C848 = 0; }

void func_ovl5_8024D80C(void) {
    func_ovl5_8024D800();
    D_ovl5_80253C64 = 0;
    osCreateThread(&D_ovl5_80254300, 0xB, func_ovl5_8024D870, NULL,
                   &D_ovl5_8025A640, 0x14);
    osStartThread(&D_ovl5_80254300);
}

void func_ovl5_8024D870(void *arg0) {
    while (func_ovl5_8024DBF8(&D_ovl5_802544B0) == -1) {
    }
    D_ovl5_80253C64 = 2;
    while (1) {
    }
}

s32 func_ovl5_8024D8B4(OverlayRecord58 *arg0) {
    if (D_ovl5_80253C64 != 0) {
        *arg0 = D_ovl5_802544B0;
        if (D_ovl5_80253C64 == 2) {
            osDestroyThread(&D_ovl5_80254300);
            D_ovl5_80253C64 = 1;
        }
        return 0;
    }
    return -1;
}

s8 func_ovl5_8024D958(void) {
    return D_ovl5_80253B52[14 *
                           player_records[D_ovl5_8025C884].character_index];
}

s32 func_ovl5_8024D99C(void) {
    return player_records[D_ovl5_8025C884].base_score >=
           (D_800BAA04 + D_ovl5_8025C640) - 1;
}

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_8024D9E8);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_8024DBF8);

s32 func_ovl5_8024F070(void) {
    return calculate_hypotenuse_safe(D_ovl5_8025C874 - camera_position_x,
                                     D_ovl5_8025C87C - camera_position_z);
}

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_8024F0B0);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_8024F320);

/* Unused local: reserves the 8-byte stack frame the original code uses. */
void func_ovl5_8024F688(s32 count, s32 *parallel_a, s32 *parallel_b, s32 *keys, s32 descending) {
    s32 reserved_frame[1];
    s32 i;
    s32 j;
    s32 best;
    s32 temp;

    for (i = 0; i < count - 1; i++) {
        best = i;
        for (j = i + 1; j < count; j++) {
            if (descending == 1) {
                if (keys[j] > keys[best]) {
                    best = j;
                }
            } else if (keys[j] < keys[best]) {
                best = j;
            }
        }
        if (parallel_a != NULL) {
            temp = parallel_a[i];
            parallel_a[i] = parallel_a[best];
            parallel_a[best] = temp;
        }
        if (parallel_b != NULL) {
            temp = parallel_b[i];
            parallel_b[i] = parallel_b[best];
            parallel_b[best] = temp;
        }
        temp = keys[i];
        keys[i] = keys[best];
        keys[best] = temp;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_8024F790);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_8024F844);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_802504A0);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80250768);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_802507D8);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_802510D0);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80251198);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80251378);

s32 func_ovl5_80251570(Vec3i *arg0, Vec3i *arg1) {
    Vec3i out;
    s32 dist;

    if (func_ovl5_80251378(arg0, arg1, &out) != 1) {
        dist = calculate_hypotenuse_safe(arg1->x - arg0->x, arg1->z - arg0->z);
    } else {
        dist = calculate_hypotenuse_safe(out.x - arg0->x, out.z - arg0->z);
    }
    return dist;
}

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_802515DC);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_802517F0);

/* Dead 8-byte frame and `sw $v0,0(sp)`: static-chain home of the nested-function original (caller sets $v0 = sp+0x20 before its jal, func_ovl5_80251B20.s:909); `unused` reproduces them, as in lerp_s32 (func_8008D100.c). */
s32 func_ovl5_80251AC0(s32 arg0) {
    s32 r;
    volatile s32 unused = r;
    s32 var_v1;

    var_v1 = (s32) (((f32) arg0 * 0.1171875f) + 0.5f);
    if (var_v1 < 3) {
        var_v1 = 3;
    }
    if (var_v1 >= 0x1E) {
        var_v1 = 0x1E;
    }
    return var_v1;
}

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80251B20);

s32 func_ovl5_80252EE4(s32 arg0, s32 arg1) {
    s32 slot_index;

    for (slot_index = 0; slot_index < 16; slot_index++) {
        if (get_club_param(player_records[D_ovl5_8025C884].club_ids[slot_index])->club_index == arg1) {
            return slot_index;
        }
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80252F70);

s32 func_ovl5_80253180(s32 arg0, s32 arg1) {
    s32 var_v0;

    var_v0 = (s16)(arg0 - arg1);
    if (var_v0 < 0) {
        var_v0 = -var_v0;
    }
    return var_v0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_8025319C);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80253574);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80253754);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80253978);
