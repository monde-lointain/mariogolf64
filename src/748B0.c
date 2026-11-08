#include "common.h"

/* Placeholder for the object passed to the 0x8009A5xx accessors; real type
 * unknown (their callers are not direct jal sites). Rename once identified. */
typedef struct Unk8009A500 {
    /* 0x00 */ u8 pad0[0x4];
    /* 0x04 */ s32 unk4;
    /* 0x08 */ u8 pad8[0x8];
    /* 0x10 */ s32 unk10;
} Unk8009A500;

INCLUDE_ASM("asm/nonmatchings/748B0", main);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009951C);

INCLUDE_ASM("asm/nonmatchings/748B0", func_800995F0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009973C);

extern s16 D_800E705C;
extern s16 D_800E705E;

void func_800999C4(s32 arg0, s16 arg1) {
    if (arg0 & 1) {
        D_800E705C = arg1;
    }
    if (arg0 & 2) {
        D_800E705E = arg1;
    }
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_800999F0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_80099A20);

INCLUDE_ASM("asm/nonmatchings/748B0", func_80099C70);

INCLUDE_ASM("asm/nonmatchings/748B0", func_80099CF0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_80099E08);

INCLUDE_ASM("asm/nonmatchings/748B0", func_80099ED4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_80099F54);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A008);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A060);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A0C0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A130);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A1A0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A22C);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A2B4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A2F0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A318);

extern s32 D_800E706C;

s32 func_8009A33C(void) {
    return D_800E706C;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A348);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A3A4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A3C8);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A3F0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A420);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A47C);

s32 func_8009A500(Unk8009A500 *arg0) {
    return arg0->unk4;
}

extern s32 D_800E7078;

void func_8009A508(s32 arg0) {
    D_800E7078 = arg0;
}

extern s32 D_800E707C;

void func_8009A514(s32 arg0) {
    D_800E707C = arg0;
}

s32 func_8009A520(void) {
    return D_800E7078;
}

void func_8009A52C(Unk8009A500 *arg0, s32 arg1) {
    arg0->unk10 = arg1;
}

s32 func_8009A534(Unk8009A500 *arg0) {
    return arg0->unk10;
}

extern s32 __libmus_current_sched;

void func_8009A53C(s32 arg0) {
    __libmus_current_sched = arg0;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A548);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A5AC);

extern s32 D_800E7080;

void func_8009A624(s32 arg0) {
    D_800E7080 = arg0;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A630);

s32 __MusIntMemMalloc(s32);
extern s32 D_800E7084;
extern s32 D_800E7088;
extern s32 D_800E708C;
extern s32 D_800E7090;

void func_8009A64C(s32 arg0) {
    s32 var_s0;

    var_s0 = arg0;
    if (var_s0 < 0x40) {
        var_s0 = 0x40;
    } else if (var_s0 >= 0x401) {
        var_s0 = 0x400;
    }
    D_800E7090 = __MusIntMemMalloc(var_s0 * 8);
    D_800E708C = var_s0;
    D_800E7088 = 0;
    D_800E7084 = 0;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A6B0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A730);

extern s32 D_800C7770;

s32 func_8009A7C8(void) {
    return D_800C7770;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A7D4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009AB18);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B060);

extern s32 D_800C76B8;

void func_8009B0DC(s32 arg0) {
    D_800C76B8 = arg0 == 1;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B0F0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B254);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B360);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B3D0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B5C4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B5E0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B698);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B6F0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B754);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B818);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B92C);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009BAC0);

extern s32 D_800E7064;

s32 func_8009BC58(s32 arg0) {
    s32 temp_a1;
    s32 temp_v0;
    s32 temp_v1;
    s32 var_a2;

    var_a2 = 0;
    do {
        temp_a1 = D_800E7064 * 2;
        temp_v0 = D_800E7064 & 0x48000000;
        temp_v1 = (temp_v0 == 0x48000000) | (temp_v0 == 0x08000000);
        D_800E7064 = temp_a1;
        var_a2 += 1;
        if (temp_v1 != 0) {
            D_800E7064 = temp_a1 | 1;
        }
    } while (var_a2 < 8);
    return (s32) ((f32) arg0 * (((f32) D_800E7064 / 65536.0f) / 65536.0f));
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009BCEC);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009BE28);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009BFF0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C028);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C0D8);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C280);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C4EC);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C540);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C560);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C590);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C5B0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C5BC);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C6E4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C7C8);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C7DC);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C800);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C870);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C8E4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C8F4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C924);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C934);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C944);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C954);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C9AC);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C9BC);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C9C8);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C9D8);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C9E4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CA50);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CAEC);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CB14);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CB20);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CB30);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CB3C);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CB50);

s32 func_8009CB64(s32 arg0, s32 arg1) {
    return arg1 + 2;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CB6C);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CBB4);

s32 func_8009CBC0(s32 arg0, s32 arg1) {
    return arg1 + 1;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CBC8);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CC38);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CC48);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CC98);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CCE8);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CD38);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CD48);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CE18);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CE54);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CE64);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CEA8);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CF18);
