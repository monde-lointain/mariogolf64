#include "common.h"

extern u8 D_801323A0[];
extern s8 D_80105DC2;
extern s32 D_80105DC4;
extern s32 D_800C2B28[];
extern s32 D_800FE334;
extern u8 D_801052E8;
extern u8 D_801B9410;
extern u8 D_800C2BD4;
extern s32 current_game_mode;

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_80059BA0);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_80059BC0);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_80059FAC);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005A2AC);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005A580);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005ACF8);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005AF50);

u8 *func_8005AF74(void) {
    return D_801323A0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005AF80);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B03C);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B070);

s32 func_8005B0A0(s32 arg0) {
    return D_800C2B28[arg0];
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B0B4);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B150);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B28C);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B314);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B7BC);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005BC10);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C018);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C038);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C458);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C4B4);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C510);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C5B4);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C614);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C674);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005CA48);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005CEE0);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005CF78);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D0D8);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D1E4);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D218);

s8 func_8005D23C(void) {
    return D_80105DC2;
}

s32 func_8005D248(void) {
    return D_80105DC4 != 0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D258);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D274);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D2A0);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D2B8);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D2E4);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D308);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D334);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D3B8);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D9A0);

void func_8005DAD4(void) {
    D_800FE334 = 0;
    D_801052E8 = 1;
    D_801B9410 = 0;
    D_800C2BD4 = 0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DAFC);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DC50);

void func_8005DCDC(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    current_game_mode = 0x17;
}

void func_8005DD10(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    current_game_mode = 5;
}

void func_8005DD44(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    current_game_mode = 0x12;
}

void func_8005DD78(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    current_game_mode = 0xD;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DDAC);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DE00);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DE20);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DE40);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DE60);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DE88);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DF54);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DFE8);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005E180);
