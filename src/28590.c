#include "include_asm.h"
#include "mg_type.h"
#include "mg.h"
#include "ultra64.h"

typedef struct st_unk_0x800DC6E0
{
    /* 0x0 */  s32 unk_0x0;
    /* 0x4 */  s32 unk_0x4;
    /* 0x8 */  struct st_unk_0x800DC6E0 *unk_0x8;
    /* 0xC */  struct st_unk_0x800DC6E0 *unk_0xC;
    /* 0x10 */ s32 unk_0x10;
    /* 0x14 */ s32 unk_0x14;
} st_unk_0x800DC6E0;

extern st_unk_0x800DC6E0 D_800DC6E0[];
extern s8 D_800BFEE4;
extern s8 D_800DAF60[0x4B0];

void func_8004D9C0();
void func_8004DA24(s8*, u32, u32);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D190);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D4B8);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D580);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D5F0);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D794);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004D7B8);

void func_8004D99C(s32 arg0)
{
    if (arg0 != 0)
    {
        D_800BFEE4 = -0x80;
    }
    else
    {
        D_800BFEE4 = 0;
    }
}

void func_8004D9C0(s8 *src, u32 arg1, u32 arg2)
{
    s32 val;
    s8 *dest;
    s8 mask;

    mask = D_800BFEE4;
    dest = &D_800DAF60[(arg2 * 0x28) + arg1];
    
    while (true)
    {
        val = (s32)*src++;
        if (val == 0)
        {
            break;
        }
        
        if (dest >= (s8 *)&D_800DAF60[0] && dest < (s8 *)(&D_800DAF60[0] + 0x4B0))
        {
            *dest = mask | val;
        }
        
        dest++;
    }

    D_800BFEE4 = 0;
}

void func_8004DA24(s8 *arg0, u32 arg1, u32 arg2)
{
    if (debug_mode)
    {
        func_8004D9C0(arg0, arg1, arg2);
    }
}

INCLUDE_ASM("asm/nonmatchings/28590", func_8004DA4C);

void func_8004DA9C(u32 arg0, u32 arg1, u32 arg2)
{
    s8 sp10[8];
    s8 sp18;
    u32 var_a1;
    s32 var_v1;
    s8 *var_a0;
    
    var_a0 = &sp18;
    var_a1 = arg1 + 8;
    sp18 = 0;
    do
    {
        var_v1 = (arg0 & 0xF) + 0x30;
        
        if (var_v1 >= 0x3A)
        {
            var_v1 = (arg0 & 0xF) + 0x37;
        }
        
        var_a0--;
        *var_a0 = var_v1;
        var_a1--;
        arg0 = arg0 >> 4;
    } while (var_a0 != (s8 *)(&sp10));
    
    func_8004DA24(var_a0, var_a1, arg2);
}

INCLUDE_ASM("asm/nonmatchings/28590", func_8004DAF4);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004DC44);

void func_8004DD70(s32 arg0, st_unk_0x800DC6E0 *arg1, s32 arg2)
{
    s32 temp_a2;

    temp_a2 = arg2 - (s32)arg1;
    arg1->unk_0x0 = temp_a2;
    D_800DC6E0[arg0].unk_0x10 = D_800DC6E0[arg0].unk_0x10 + temp_a2;
    arg1->unk_0x4 = 0;
    arg1->unk_0x8 = &D_800DC6E0[arg0];
    arg1->unk_0xC = D_800DC6E0[arg0].unk_0xC;
    D_800DC6E0[arg0].unk_0xC->unk_0x8 = arg1;
    D_800DC6E0[arg0].unk_0xC = arg1;
}

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
