#include "include_asm.h"
#include "mg_type.h"
#include "ultra64.h"

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D190);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D4B8);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D580);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D5F0);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D794);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D7B8);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D99C);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D9C0);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004DA24);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004DA4C);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004DA9C);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004DAF4);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004DC44);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004DD70);

typedef struct
{
    s32 unk_0x0;
    s32 unk_0x4;
    void *unk_0x8;
    void *unk_0xC;
    s32 unk_0x10;
    s32 unk_0x14;
} st_unk_0x800DC6E0;

extern st_unk_0x800DC6E0 D_800DC6E0[];

void func_8004DDE4(s32 arg0)
{
    D_800DC6E0[arg0].unk_0x10 = 0;
    D_800DC6E0[arg0].unk_0x0 = 0;
    D_800DC6E0[arg0].unk_0x4 = -1;
    D_800DC6E0[arg0].unk_0x8 = D_800DC6E0[arg0].unk_0xC = &D_800DC6E0[arg0];
    D_800DC6E0[arg0].unk_0x14 = -1;
}

INCLUDE_ASM("asm/nonmatchings/28590", func_8004DE44);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004DE60);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004DE7C);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004E058);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004E184);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004E1E0);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004E27C);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004E288);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004E2DC);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004E47C);
