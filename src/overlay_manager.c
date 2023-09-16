#include "include_asm.h"
#include "ultra64.h"
#include "unknown_structs.h"

typedef struct st_OverlayInfo
{
    /* 0x0 */  s8 active;
    /* 0x4 */  s8 *unk_0x4;
    /* 0x8 */  u32 rom_start_addr;
    /* 0xC */  u32 rom_end_addr;
    /* 0x10 */ void *vram_start_addr;
    /* 0x14 */ void *vram_text_end_addr;
    /* 0x18 */ void *vram_end_addr;
    /* 0x1C */ void *unk_0x1C;
    /* 0x20 */ void *unk_0x20;
    /* 0x24 */ void *unk_0x24; /* function pointer */
} OverlayInfo;

extern void func_80080DCC();
extern void func_801FD624();
extern s32 func_800299F4(u32);

extern OverlayInfo overlays[17];
extern s32 D_800B67F0;
extern const char D_800C9F88;
extern const char D_800C9F98;
extern const char D_800C9FD0;
extern const char D_800C9F60;
extern const char D_800C9F70;
extern s8 D_800D2930[64];
extern u8 D_800FC858[64];
extern u32 D_8012CFC0[64];

#define OVERLAY_COUNT 17U

void func_80025D30(void)
{
    func_80080DCC();
    func_801FD624();
}

INCLUDE_ASM("asm/nonmatchings/overlay_manager", func_80025D54);

s32 func_80025D78(s32 index)
{
    return D_800D2930[index] | 0x100;
}

void func_80025D8C(void)
{
    s32 i;
    s8 *byte_arr_ptr;
    s8 *byte_ptr;

    /* Initialize arrays to their default values */
    for (i = 0; i < 0x40; i++)
    {
        D_800D2930[i] = -1;
        D_800FC858[i] = 0;
        D_8012CFC0[i] = 0x80000000;
    }

    /* Process structs */
    for (i = 0; (u32)i < OVERLAY_COUNT; i++)
    {
        const s32 overlay_index = i;

        /* Get address of data pointed to by the next overlay */
        byte_arr_ptr = overlays[overlay_index].unk_0x4;

        /* Check if data valid */
        if (*byte_arr_ptr != -1)
        {
            /* Process bytes pointed to by pointer */
            for (byte_ptr = byte_arr_ptr; *byte_ptr != -1; byte_ptr++)
            {
                if ((D_800D2930[*byte_ptr] != -1) && (func_800299F4(0x4D) == 0))
                {
                    osSyncPrintf(&D_800C9F60); /* "OverlayManager\n " */
                }

                D_800D2930[*byte_ptr] = i;
            }
        }
    }

    for (i = 0; i < 0x40; i++)
    {
        if (D_800D2930[i] == -1)
        {
            /* Flip bit if not valid */
            D_800FC858[i] = 1;
        }
    }
}

s32 func_80025EC8(s32 module_set_num)
{
    s32 var_v0;

    if (module_set_num < 0x100 || (u32)module_set_num >= 0x111U)
    {
        if (!func_800299F4(0x4D))
        {
            osSyncPrintf(&D_800C9F70); /* "Illegal ModuleSetNo.\n" */
        }

        var_v0 = 0;
    }
    else
    {
        var_v0 = module_set_num & 0xFF;
    }

    return var_v0;
}

s32 func_80025F18(s32 arg0)
{
    u32 overlay_index;
    u32 new_overlay_index;
    u32 byte_index;
    s8 *bytes;
    s8 byte;

    new_overlay_index = func_80025EC8(arg0);

    if (overlays[new_overlay_index].active)
    {
        if (func_800299F4(0x4DU) == 0)
        {
            osSyncPrintf(&D_800C9F88); /* "Already Loaded\n" */
        }
        return 1;
    }

    for (overlay_index = 0; overlay_index < OVERLAY_COUNT; overlay_index++)
    {
        if (overlay_index == new_overlay_index
            || overlays[overlay_index].vram_start_addr > overlays[new_overlay_index].unk_0x1C
            || overlays[overlay_index].unk_0x1C < overlays[new_overlay_index].vram_start_addr)
        {
            /* discard */
            continue;
        }
        
        if (overlays[overlay_index].active)
        {
            /* Another overlay is still active */
            if (func_800299F4(0x4DU) == 0)
            {
                osSyncPrintf(&D_800C9F98, /* "Conflicting %d m.s. and other moduleset[%x] is alive\n" */
                             new_overlay_index,
                             overlay_index);
            }
            return 0;
        }

        /* ??? */
        bytes = overlays[overlay_index].unk_0x4;
        
        for (byte_index = 0; bytes[byte_index] != -1; byte_index++)
        {
            byte = bytes[byte_index];

            if (D_800B67F0 - D_8012CFC0[byte] < 2)
            {
                if (func_800299F4(0x4DU) == 0)
                {
                    osSyncPrintf(&D_800C9FD0, /* "Conflicting %d m.s. and other module[%x-%x]'s RDP request\n" */
                                 new_overlay_index,
                                 overlay_index,
                                 byte_index);
                }
                return 0;
            }
        }
    }

    return 1;
}

INCLUDE_ASM("asm/nonmatchings/overlay_manager", func_800260C4);

INCLUDE_ASM("asm/nonmatchings/overlay_manager", func_80026230);

INCLUDE_ASM("asm/nonmatchings/overlay_manager", func_80026358);
