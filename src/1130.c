#include "ultra64.h"
#include "include_asm.h"
#include "unknown_structs.h"

extern s8 D_800D2930[64];

INCLUDE_ASM("asm/nonmatchings/1130", func_80025D30);

INCLUDE_ASM("asm/nonmatchings/1130", func_80025D54);

s32 func_80025D78(s32 index)
{
    return D_800D2930[index] | 0x100;
}


s32 func_800299F4(u32);                               /* extern */
extern st_800B5F5C D_800B5F5C[11];
extern const char D_800C9F60;
extern u8 D_800FC858[64];
extern u32 D_8012CFC0[64];

void func_80025D8C(void)
{
    s32 index;
    s8 *byte_arr_ptr;
    s8 *byte_ptr;

    /* Initialize arrays to their default values */
    for (index = 0; index < 0x40; index++)
    {
        D_800D2930[index] = -1;
        D_800FC858[index] = 0;
        D_8012CFC0[index] = 0x80000000;
    }
    
    /* Process structs */
    for (index = 0; index < 0x11U; index++)
    {
        /* Get address of data pointed to by the next struct */
        byte_arr_ptr = D_800B5F5C[index].pointer;
        
        /* Check if data valid */
        if (*byte_arr_ptr != -1)
        {
            /* Process bytes pointed to by pointer
             */
            for (byte_ptr = byte_arr_ptr; *byte_ptr != -1; byte_ptr++)
            {
                if ((D_800D2930[*byte_ptr] != -1) && (func_800299F4(0x4D) == 0))
                {
                    osSyncPrintf(&D_800C9F60); /*OverlayManager\n*/
                }
                
                D_800D2930[*byte_ptr] = index;
            }
        }
    }
    
    for (index = 0; index < 0x40; index++)
    {
        if (D_800D2930[index] == -1)
        {
            /* Flip bit if not valid */
            D_800FC858[index] = 1;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/1130", func_80025EC8);

INCLUDE_ASM("asm/nonmatchings/1130", func_80025F18);

INCLUDE_ASM("asm/nonmatchings/1130", func_800260C4);

INCLUDE_ASM("asm/nonmatchings/1130", func_80026230);

INCLUDE_ASM("asm/nonmatchings/1130", func_80026358);
