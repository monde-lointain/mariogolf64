#include "ultra64.h"
#include "include_asm.h"

#include "structs.h"

// 4560.c
extern void func_800292FC(s32);       

// main.c
extern void func_80029A30(s32);      

// 43810.c
extern void func_80069F38();                                  
extern void func_80069FBC();       

// 8E550.c       
extern void func_800B3150(u32*, s32, s32);     

// 8E620.c
extern s16 func_800B3220();   

// ADD30.c
extern unk_801B6088 D_801B6088;

// 8F250.c
extern unk_800BA9F0 D_800BA9F0;


//INCLUDE_ASM("asm/nonmatchings/68500", func_8008D100);

void func_8008D100(void) {
    
    func_800B3150(&D_801B6088.unk_0, 0, 0x44);
    
    D_801B6088.unk_10 = 9;
    D_801B6088.unk_C = 0;
    D_801B6088.unk_14 = 0;
    D_801B6088.unk_4 = 0xC;
    
    D_800BA9F0.unk_8 = 0;
    D_800BA9F0.unk_0 = (func_800B3220() & 0x3F) * 0x1770;
    D_800BA9F0.unk_4 = func_800B3220(); // wind direction
    D_800BA9F0.unk_C = 6;
    D_800BA9F0.unk_12 = 0x1000;
    D_800BA9F0.unk_10 = 0x6000;
    
    func_80029A30(7);
    
    D_801B6088.unk_8 = 1;
    
    func_80069FBC();
    func_80069F38();
    func_800292FC(2);

}

INCLUDE_ASM("asm/nonmatchings/68500", func_8008D1DC);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008D3F4);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008DDDC);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008E164);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008E1B4);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008E82C);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008F30C);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008FF14);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008FF20);

INCLUDE_ASM("asm/nonmatchings/68500", func_80091110);

INCLUDE_ASM("asm/nonmatchings/68500", func_8009226C);

void func_80092324(void) {
}

INCLUDE_ASM("asm/nonmatchings/68500", func_8009232C);

INCLUDE_ASM("asm/nonmatchings/68500", func_80092E10);

INCLUDE_ASM("asm/nonmatchings/68500", func_80092F18);

INCLUDE_ASM("asm/nonmatchings/68500", func_800934CC);

INCLUDE_ASM("asm/nonmatchings/68500", func_8009351C);

INCLUDE_ASM("asm/nonmatchings/68500", func_800939E8);

INCLUDE_ASM("asm/nonmatchings/68500", func_80094228);

INCLUDE_ASM("asm/nonmatchings/68500", func_800947A8);

INCLUDE_ASM("asm/nonmatchings/68500", func_80095150);

INCLUDE_ASM("asm/nonmatchings/68500", func_8009548C);

INCLUDE_ASM("asm/nonmatchings/68500", func_800957F0);

INCLUDE_ASM("asm/nonmatchings/68500", func_800958D8);

INCLUDE_ASM("asm/nonmatchings/68500", func_800959F8);
