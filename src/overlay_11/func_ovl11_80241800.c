#include "common.h"
#include "overlay_11.h"

extern s32 clear_animation_slot(s32 index);
extern void func_80050DA0(s32 sfx, s32 channel, s32 volume, s32 reverb, s32 pan);
extern s32 D_ovl11_8024C4A4;
extern s32 game_mode;
extern s32 D_800C4140;
extern s32 D_800C0E60;
void func_ovl11_80242FC0(s32 arg0);
void func_ovl11_80243050(void);
void func_ovl11_802433B4(u16 arg0);
void func_ovl11_802434A0(u16 arg0);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80241800);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80242074);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80242334);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80242B74);

void func_ovl11_80242FC0(s32 arg0) {
    if (arg0 == 0) {
        func_80050DA0(0x47, D_800C0E60, 0x64, 0x14, 0xDF);
    } else if (arg0 == 1) {
        func_80050DA0(0x48, D_800C0E60, 0x64, 0x14, 0x20);
    }
    if (arg0 == 0) {
        D_80104FD0.unkC8 = 0x140;
        D_80104FD0.unkCC = 0;
    }
    D_800C4140 = arg0;
    D_80104FD0.unkD4 = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80243050);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_802433B4);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_802434A0);

void func_ovl11_80243634(u16 arg0, u16 arg1) {
    func_ovl11_80243050();
    if (D_800C4140 == 2) {
        if (arg1 & 0xC000) {
            func_ovl11_80242FC0(1);
        } else {
            switch (game_mode) {
                case 5:
                    func_ovl11_802434A0(arg0);
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                default:
                    func_ovl11_802433B4(arg0);
                    break;
            }
        }
    }
}

s32 func_ovl11_802436C8(u16 arg0) {
    func_ovl11_80243050();
    if (D_800C4140 != 2) {
        return 0;
    }
    switch (game_mode) {
        case 5:
            func_ovl11_802434A0(arg0);
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        default:
            func_ovl11_802433B4(arg0);
            break;
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_8024373C);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_802438D0);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80243C48);

void func_ovl11_8024407C(void) {
    clear_animation_slot(0);
    clear_animation_slot(1);
    D_ovl11_8024C4A4 = -1;
}

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_802440AC);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80244244);
