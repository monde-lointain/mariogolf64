#include "ultra64.h"
#include "include_asm.h"

// 1B4A0.c
s32 func_80040E20(u32, u32); 
void func_800407E8(u32, u32, u32); 

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_800400A0);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_8004018C);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80040234);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_800402F4);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80040380);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80040428);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_800407D8);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_800407E8);

//INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80040E20);

// matches, but should have return value based on its usage
s32 func_80040E20(u32 arg0, u32 arg1) {
    func_800407E8(arg0, arg1, 0);
}

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80040E3C);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80040E60);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80040EF8);

//INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80041058);

s32 func_80041058(u32 arg0, u32 arg1, u32 arg2) {
    return func_80040E20(arg0, arg2) == 5;
}

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_8004107C);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80041160);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80041344);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_800413A0);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_800413C0);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80041464);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80041490);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_800414C0);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80041524);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_800415C4);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80041878);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80041AA8);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80041B98);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80041D44);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80041E8C);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80041EC0);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80042228);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80042318);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_800425C8);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80042720);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80042738);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_800427E8);

//INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80042DF4);

// something probably wrong here, but matches
s32 func_80042DF4(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {

    if (arg3 & 0x80) {
        // no return value?
        func_80041344((arg1 * 2) + ((arg3 >> 2) & 3), (arg2 * 2) + (arg3 & 3));        
    } else {
        return arg0 + (arg3 * 0x10);
    }
    
}

//INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80042E40);

s32 func_80042E40(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5) {
    return ((arg2 - arg0) * (arg5 - arg1)) - ((arg3 - arg1) * (arg4 - arg0));
}

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80042E78);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_800432E4);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_8004333C);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80043620);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_800437FC);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80043A04);

INCLUDE_ASM("asm/nonmatchings/1B4A0", func_80043A20);
