#include "common.h"

extern s32 D_800BB03C;
extern s32 D_800B7770;
extern s32 D_800B7774;
extern s32 D_800B7778;
extern s32 D_800B777C;
extern s32 D_800B680C;
extern s32 D_800B6810;
extern s32 D_800B6818;
extern s32 D_801B552C;
extern s32 D_801B608C;
extern s32 D_800BB038;

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002A640);

void func_8002A90C(Mtx *arg0, f32 arg1, f32 arg2, f32 arg3) {
    Mtx sp10;
    guTranslate(&sp10, arg1, arg2, arg3);
    guMtxCatL(arg0, &sp10, arg0);
}

void func_8002A944(Mtx *arg0, f32 arg1, f32 arg2, f32 arg3) {
    Mtx sp10;
    guScale(&sp10, arg1, arg2, arg3);
    guMtxCatL(arg0, &sp10, arg0);
}

void func_8002A97C(s32 arg0, s32 arg1) {
    D_800B680C = arg0;
    D_800B6810 = arg1;
    if (arg0 == 4) {
        D_801B552C = 1;
        D_800B6818 = -0x280;
    } else {
        D_801B552C = 3;
    }
}

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002A9C4);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", emit_sky_dome_dl);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", emit_sky_horizon_compositor_dl);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", init_rdp_and_draw_sky_background);

void func_8002BD9C(void) {
}

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", setup_view_by_camera_mode);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002BE78);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", draw_ground_shadow_decals);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002CDA8);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002DAC0);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", render_frame);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_80031450);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_800318A8);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_80031AF4);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_80032520);

s32 func_80032658(void) {
    return D_800BB03C == 0;
}

void func_80032668(void) {
    u32 v = D_801B608C;
    s32 r;

    D_800BB03C = 1;
    switch (v) {
    case 3:
    case 9:
        r = 0x15;
        break;
    case 11:
        r = 0x1A;
        break;
    default:
        r = 5;
        break;
    }
    D_800BB038 = r;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_800326C4);

void func_800326FC(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    D_800B7778 = arg2;
    D_800B777C = arg3;
    D_800B7770 = arg0;
    D_800B7774 = arg1;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_80032720);
