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

INCLUDE_ASM("asm/nonmatchings/748B0", func_800999C4);

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

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A33C);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A348);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A3A4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A3C8);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A3F0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A420);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A47C);

s32 func_8009A500(Unk8009A500 *arg0) {
    return arg0->unk4;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A508);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A514);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A520);

void func_8009A52C(Unk8009A500 *arg0, s32 arg1) {
    arg0->unk10 = arg1;
}

s32 func_8009A534(Unk8009A500 *arg0) {
    return arg0->unk10;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A53C);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A548);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A5AC);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A624);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A630);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A64C);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A6B0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A730);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A7C8);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A7D4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009AB18);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B060);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B0DC);

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

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009BC58);

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
