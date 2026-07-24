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

extern s32 D_800BB020;
extern s32 D_800BB024;
extern s32 D_800BB02C;
extern s32 D_800BB030;
extern s32 D_800BB034;
extern s32 D_800B7768;
extern s32 D_800B776C;

extern s32 func_80025D78(s32);
extern s32 func_80025F18(s32);
extern s32 load_overlay(s32);
extern void unload_overlay(s32);

extern Gfx *glistp;

void init_rdp_and_draw_sky_background(Gfx **gfxp, s32 arg1);
void func_8002A9C4(Gfx **gfxp, s32 arg1);
void emit_sky_horizon_compositor_dl(Gfx **gfxp);

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

void func_8002BD9C(Gfx **gfxp) {
}

void setup_view_by_camera_mode(u8 *arg0, s32 arg1) {
    if (D_800B680C == 0) {
        init_rdp_and_draw_sky_background(&glistp, arg1);
    } else {
        if (D_800B680C == 2) {
            func_8002A9C4(&glistp, arg1);
            return;
        }
        if (D_800B680C != 1) {
            goto check3;
        }
        func_8002BD9C(&glistp);
    }
    gSPPopMatrix(glistp++, G_MTX_MODELVIEW);
    gSPMatrix(glistp++, (Mtx *)(arg0 + 0x1C0),
              G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    return;
check3:
    if (D_800B680C == 3) {
        emit_sky_horizon_compositor_dl(&glistp);
    }
}

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002BE78);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", draw_ground_shadow_decals);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002CDA8);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002DAC0);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", render_frame);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_80031450);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_800318A8);

INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_80031AF4);

void func_80032520(void) {
    if (D_800BB020 == 0x19) {
        if (D_800B7768 == 0) {
            D_800B7768 = 1;
            unload_overlay(func_80025D78(D_800BB02C));
        } else if (func_80025F18(func_80025D78(D_800BB030)) != 0) {
            load_overlay(func_80025D78(D_800BB030));
            D_800B7768 = 0;
            D_800BB020 = D_800BB024;
        }
    }
    if (D_800BB03C == 1) {
        if (D_800B776C == 0) {
            D_800B776C = 1;
            unload_overlay(func_80025D78(D_800BB034));
        } else if (func_80025F18(func_80025D78(D_800BB038)) != 0) {
            load_overlay(func_80025D78(D_800BB038));
            D_800BB03C = 0;
            D_800B776C = 0;
            D_800BB034 = D_800BB038;
        }
    }
}

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
