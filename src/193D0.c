#include "ultra64.h"
#include "include_asm.h"

// 45400.c
extern u32 func_8006A0CC(); 
// 28590.c
extern void func_8004E47C(u32*);                                

extern u32 D_800DAD00;
extern u32 D_800DAD04;
extern u32 D_800DAD08;
extern u32 D_800DAD0C;


//INCLUDE_ASM("asm/nonmatchings/193D0", func_8003DFD0);

u32 func_8003DFD0(void) {
    
    u32 ret;

    ret = func_8006A0CC();
    
    if (ret < 1689) {
        ret = 0;
    } else {
        ret -= 1689;
    }
        
    return ret;

}

INCLUDE_ASM("asm/nonmatchings/193D0", func_8003E004);

INCLUDE_ASM("asm/nonmatchings/193D0", func_8003E314);

INCLUDE_ASM("asm/nonmatchings/193D0", func_8003E400);

//INCLUDE_ASM("asm/nonmatchings/193D0", func_8003E4B4);

void func_8003E4B4(void) {
    func_8004E47C(&D_800DAD00);
    func_8004E47C(&D_800DAD04);
    func_8004E47C(&D_800DAD08);
    func_8004E47C(&D_800DAD0C);
}

INCLUDE_ASM("asm/nonmatchings/193D0", func_8003E4F8);

INCLUDE_ASM("asm/nonmatchings/193D0", func_8003E628);

INCLUDE_ASM("asm/nonmatchings/193D0", func_8003E638);

INCLUDE_ASM("asm/nonmatchings/193D0", func_8003E648);

INCLUDE_ASM("asm/nonmatchings/193D0", func_8003E6C0);
