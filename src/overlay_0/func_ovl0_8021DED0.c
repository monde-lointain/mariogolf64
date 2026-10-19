#include "common.h"

extern u16 D_800FBE48;
extern s32 D_800FBE64;
extern s32 D_800FBE68;
extern s8 D_80105118;
extern f32 D_801B54F0;
extern f32 D_801B5548;
extern f32 D_801B554C;
extern f32 D_801B5550;
extern s32 D_ovl0_80224B10;
extern f32 D_ovl0_80224B14;
extern f32 D_ovl0_80224B18;
extern s8 D_ovl0_80225658;
extern f64 D_ovl0_80225660;
extern f64 D_ovl0_80225668;
extern f64 D_ovl0_80225670;
extern f64 D_ovl0_80225678;
extern f64 D_ovl0_80225680;
extern s32 round_hole_index;
extern s32 target_z;
extern f32 view_camera_eye_x;
extern f32 view_camera_eye_z;

extern void check_and_print_grid(char *str, s32 col, s32 row);
extern void func_ovl0_8020FFEC(s32 arg0);
extern s32 func_ovl0_8021035C(void);
extern s32 get_round_first_hole(void);
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8021DED0", func_ovl0_8021DED0);

s32 func_ovl0_8021E090(void) {
    f32 eye_offset[3];
    f32 temp_fv0;
    f32 temp_fv0_2;
    f32 temp_fv1;
    f32 temp_fv2;
    f32 var_fs1_2;
    f32 *eye_x_ptr;

    if (D_ovl0_80224B10 < 0x3E8) {
        D_ovl0_80224B10 += 1;
    }
    if (round_hole_index == get_round_first_hole()) {
        if (D_ovl0_80224B10 < 0x5A) {
            D_ovl0_80224B14 = (f32) (-307200 - (-(D_ovl0_80224B10 * 0x3C000) / 90));
            D_ovl0_80224B18 = (f32) (-122880 - (-(D_ovl0_80224B10 * 0x19200) / 90));
        } else {
            D_ovl0_80224B14 = -61440.0f;
            D_ovl0_80224B18 = -19968.0f;
        }
        var_fs1_2 = (f32) ((f64) -(f32) D_ovl0_80224B10 * D_ovl0_80225660);
        if ((f64) var_fs1_2 < D_ovl0_80225668) {
            var_fs1_2 = -3.1415927f;
        }
        var_fs1_2 = (f32) ((f64) var_fs1_2 + ((f64) ((f32) D_800FBE48 * 0.0000958738f) + D_ovl0_80225670));
    } else {
        if (D_ovl0_80224B10 < 0x28) {
            D_ovl0_80224B14 = (f32) (-276480 - (-(D_ovl0_80224B10 * 0x34800) / 40));
            D_ovl0_80224B18 = (f32) (-107520 - (-(D_ovl0_80224B10 * 0x15600) / 40));
        } else {
            D_ovl0_80224B14 = -61440.0f;
            D_ovl0_80224B18 = -19968.0f;
        }
        var_fs1_2 = (f32) ((f64) ((f32) D_800FBE48 * 0.0000958738f) + D_ovl0_80225678 + D_ovl0_80225680);
    }
    eye_x_ptr = &D_801B5548;
    *eye_x_ptr = (f32) D_800FBE64 * 0.0009765625f;
    temp_fv2 = ((f32) D_800FBE68 - 17664.0f) * 0.0009765625f;
    D_801B5550 = (f32) target_z * 0.0009765625f;
    D_801B554C = temp_fv2;
    temp_fv0 = -sinf(var_fs1_2) * D_ovl0_80224B14 * 0.0009765625f;
    eye_offset[1] = ((f32) D_800FBE68 + D_ovl0_80224B18) * 0.0009765625f;
    eye_offset[0] = temp_fv0;
    temp_fv0_2 = cosf(var_fs1_2) * D_ovl0_80224B14 * 0.0009765625f;
    temp_fv1 = D_801B5550 + temp_fv0_2;
    D_801B54F0 = eye_offset[1];
    view_camera_eye_x = *eye_x_ptr + eye_offset[0];
    eye_offset[2] = temp_fv0_2;
    view_camera_eye_z = temp_fv1;
    if (round_hole_index == get_round_first_hole()) {
        if (D_ovl0_80224B10 == 0x3C) {
            func_ovl0_8020FFEC(1);
        }
        if ((D_ovl0_80224B10 >= 0x3D) && (func_ovl0_8021035C() != 0)) {
            return 1;
        }
        goto block_24;
    }
    if (D_ovl0_80224B10 == 4) {
        func_ovl0_8020FFEC(0xF);
    }
    if (D_ovl0_80224B10 == 8) {
        play_sound_effect(0x8F, 0xD, 0x7F);
    }
    if (D_ovl0_80224B10 == 0x28) {
        return 1;
    }
block_24:
    sprintf(&D_80105118, &D_ovl0_80225658, D_ovl0_80224B10);
    check_and_print_grid(&D_80105118, 0x14, 2);
    return 0;
}
