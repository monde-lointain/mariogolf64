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

/* Almost certainly the same as st_unk_0x800DC6E0 */
typedef struct MemoryBlockDescriptor
{
    s32 unk_0x0;
    s32 unk_0x4;
    void *start_addr;
    void *end_addr;
    u32 size;
    s32 unk_0x1C;
} MemoryBlockDescriptor;

extern st_unk_0x800DC6E0 D_800DC6E0[];
extern s8 D_800BFEE4;
extern s8 D_800DAF60[0x4B0];
extern const char D_800CCA90;
void *D_800DC728;

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

s32 func_8004DE44(s32 arg0)
{
    return D_800DC6E0[arg0].unk_0x10;
}

s32 func_8004DE60(s32 arg0)
{
    return D_800DC6E0[arg0].unk_0x14;
}

#ifdef NON_MATCHING
/* This function appears to be a general purpose memory allocator. It searches
 * through the memory arena and identifies the first available free space block
 * which is large enough to fit the requested memory size.
 */
s32 *func_8004DE7C(s32 arg0, s32 arg1)
{
    u32 block_size;
    st_unk_0x800DC6E0 *block;
    st_unk_0x800DC6E0 *current_block;
    st_unk_0x800DC6E0 *new_block;
    st_unk_0x800DC6E0 *best_block;
    u32 mask;
    u32 requested_size;
    u32 size_difference;
    u32 best_size;
    u32 max_block;

    do
    {
        current_block = &D_800DC6E0[arg0];
        best_block = NULL;
        new_block = NULL;
        max_block = 0;
        
        requested_size = (arg1 + 0x17) & (~7);
        mask = osSetIntMask(0x00000001);
        best_size = 0x7FFFFFFF;

        block = current_block->unk_0x8;

        while (block != current_block)
        {
            if (block->unk_0x4 != 0)
            {
                block = block->unk_0x8;
                continue;
            }

            block_size = block->unk_0x0;
            if (block_size >= requested_size)
            {
                if (block_size < best_size)
                {
                    if ((max_block < best_size) && (best_size != 0x7FFFFFFF))
                    {
                        max_block = best_size;
                    }
                    best_size = block_size;
                    size_difference = best_size - requested_size;
                    if (max_block < size_difference)
                    {
                        max_block = size_difference;
                    }
                    best_block = block;
                }
                else if (max_block < block_size)
                {
                    max_block = block_size;
                }
            }
            block = block->unk_0x8;
        }

        D_800DC6E0[arg0].unk_0x14 = max_block;

        if (best_block == NULL)
        {
            osSetIntMask(mask);
            osSyncPrintf(&D_800CCA90);
            return NULL;
        }
        
        if ((best_block->unk_0x0 - requested_size) >= 0x11U)
        {
            D_800DC6E0[arg0].unk_0x10 -= requested_size;
            new_block->unk_0x0 = best_block->unk_0x0;
            new_block->unk_0x0 = new_block->unk_0x0 - requested_size;
            new_block->unk_0x4 = 0;
            best_block->unk_0x8->unk_0xC = new_block;
            new_block->unk_0x8 = best_block->unk_0x8;
            best_block->unk_0x8 = new_block;
            new_block->unk_0xC = best_block;
            best_block->unk_0x0 = requested_size;
        }
        else
        {
            D_800DC6E0[arg0].unk_0x10 -= best_size;
        }
    } while (0);
    best_block->unk_0x4 = 0x12345678;
    osSetIntMask(mask);
    return &best_block->unk_0x10;
}
#else
INCLUDE_ASM("asm/nonmatchings/28590", func_8004DE7C);
#endif

INCLUDE_ASM("asm/nonmatchings/28590", func_8004E058);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004E184);

void *func_8004E184(void *, void *); 

#ifdef NON_MATCHING
void func_8004E1E0(s32 arg0,
                    struct MemoryBlockDescriptor *a,
                    struct MemoryBlockDescriptor *b)
{
    a = (struct MemoryBlockDescriptor *)(&D_800DC728);

    a->unk_0x4 = -1;
    a->end_addr = a;
    a->unk_0x0 = 0;
    a->size = 0;
    a->start_addr = a->end_addr;

    func_8004E184((void *)0x8025D800, (void *)0x802EA000);

    if (arg0 == 1)
    {
        b = func_8004E184((void *)0x80600000, (void *)0x80800000);
    }

    a->unk_0x1C = -1;
}
#else
INCLUDE_ASM("asm/nonmatchings/28590", func_8004E1E0);
#endif

INCLUDE_ASM("asm/nonmatchings/28590", func_8004E27C);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004E288);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004E2DC);

INCLUDE_ASM("asm/nonmatchings/28590", func_8004E47C);
