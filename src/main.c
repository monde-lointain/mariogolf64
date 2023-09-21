#include "include_asm.h"
#include "overlay_manager.h"
#include "mg_type.h"

INCLUDE_ASM("asm/nonmatchings/main", func_800298E0);

INCLUDE_ASM("asm/nonmatchings/main", func_80029900);

INCLUDE_ASM("asm/nonmatchings/main", func_800299D0);

extern u8 debug_flags[32]; /* 0x100 long bitfield */

bool flag_is_set(u32 index)
{
    if (index >= 0x100U)
    {
        return false;
    }

    /* Find the correct byte in the array by shifting the index right, then
     * extract the correct bit with a mask. Return true if the bit is set. 
     */
    return (debug_flags[index >> 3] & (0x80 >> (index & 0x7))) != 0;
}

INCLUDE_ASM("asm/nonmatchings/main", func_80029A30);

INCLUDE_ASM("asm/nonmatchings/main", func_80029A6C);

INCLUDE_ASM("asm/nonmatchings/main", func_80029AA8);

INCLUDE_ASM("asm/nonmatchings/main", func_80029AE4);

INCLUDE_ASM("asm/nonmatchings/main", func_80029B08);

INCLUDE_ASM("asm/nonmatchings/main", func_80029B58);

INCLUDE_ASM("asm/nonmatchings/main", func_80029BD8);

INCLUDE_ASM("asm/nonmatchings/main", func_80029C00);

INCLUDE_ASM("asm/nonmatchings/main", mainproc);

INCLUDE_ASM("asm/nonmatchings/main", func_80029EEC);

INCLUDE_ASM("asm/nonmatchings/main", func_80029F28);

INCLUDE_ASM("asm/nonmatchings/main", func_80029F6C);

INCLUDE_ASM("asm/nonmatchings/main", func_8002A144);

INCLUDE_ASM("asm/nonmatchings/main", func_8002A310);

INCLUDE_ASM("asm/nonmatchings/main", func_8002A434);
