#include "common.h"
#include "../main/session.h"

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

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_8024F688);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_8024F790);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_8024F844);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_802504A0);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80250768);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_802507D8);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_802510D0);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80251198);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80251378);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80251570);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_802515DC);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_802517F0);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80251AC0);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80251B20);

INCLUDE_ASM("asm/nonmatchings/overlay_5/func_ovl5_8024D800", func_ovl5_80252EE4);

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
