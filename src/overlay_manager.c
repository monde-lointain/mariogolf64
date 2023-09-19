#include "include_asm.h"
#include "mg_type.h"
#include "mg_log.h"
#include "ultra64.h"

typedef struct
{
    /* 0x0 */  s8 active;
    /* 0x4 */  s8 *unk_0x4;
    /* 0x8 */  u32 rom_start_addr;
    /* 0xC */  u32 rom_end_addr;
    /* 0x10 */ void *overlay_start_addr;
    /* 0x14 */ void *data_start_addr;
    /* 0x18 */ void *bss_start_addr;
    /* 0x1C */ void *overlay_end_addr;
    /* 0x20 */ void (*constructor)(void); /* These likely take some void ptr arguments... */
    /* 0x24 */ void (*destructor)(void);
} MGOverlayInfo;

extern void func_80080DCC();
extern void func_801FD624();
extern s32 func_800299F4(u32);
extern void nuPiReadRom(u32 rom_addr, void* buf_ptr, u32 size);

MGOverlayInfo overlays[17];
extern u32 D_8012CFC0[64];
extern s32 D_800B67F0;
extern u8 D_800FC858[64];
extern s8 D_800D2930[64];
extern s8 D_800B67C0;

#define OVERLAY_COUNT 17U

void func_80025D30(void)
{
    func_80080DCC();
    func_801FD624();
}

/* Constructor of overlay #6 */
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
                if (D_800D2930[*byte_ptr] != -1)
                {
                    LOG_INFO("OverlayManager\n");
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

s32 compute_overlay_index(s32 overlay_num)
{
    s32 overlay_index;

    if (overlay_num < 0x100 || (u32)overlay_num >= 0x111U)
    {
        LOG_INFO("Illegal ModuleSetNo.\n");
        overlay_index = 0;
    }
    else
    {
        overlay_index = overlay_num & 0xFF;
    }

    return overlay_index;
}

bool func_80025F18(s32 new_overlay_index)
{
    s32 overlay_index;
    u32 byte_index;
    s8 *bytes;
    s8 byte;

    new_overlay_index = compute_overlay_index(new_overlay_index);

    if (overlays[new_overlay_index].active)
    {
        LOG_INFO("Already Loaded\n");
        return true;
    }

    for (overlay_index = 0; (u32)overlay_index < OVERLAY_COUNT; overlay_index++)
    {
        if (overlay_index == new_overlay_index
            || overlays[overlay_index].overlay_start_addr > overlays[new_overlay_index].overlay_end_addr
            || overlays[overlay_index].overlay_end_addr < overlays[new_overlay_index].overlay_start_addr)
        {
            /* discard */
            continue;
        }

        if (overlays[overlay_index].active)
        {
            LOG_INFO2("Conflicting %d m.s. and other moduleset[%x] is alive\n", 
                      new_overlay_index, 
                      overlay_index);
            return false;
        }

        /* ??? */
        bytes = overlays[overlay_index].unk_0x4;

        for (byte_index = 0; bytes[byte_index] != -1; byte_index++)
        {
            byte = bytes[byte_index];

            if (D_800B67F0 - D_8012CFC0[byte] < 2)
            {
                LOG_INFO3("Conflicting %d m.s. and other module[%x-%x]'s RDP request\n",
                          new_overlay_index,
                          overlay_index,
                          byte_index);
                return false;
            }
        }
    }

    return true;
}

bool load_overlay(s32 overlay_index)
{
    bool success;
    MGOverlayInfo *overlay;
    u32 byte_index;
    s8 *byte_arr_ptr;
    s8 byte;

    if (func_80025F18(overlay_index))
    {
        overlay_index = compute_overlay_index(overlay_index);
        LOG_INFO1("Load ModuleSet %x (absolute No.)\n", overlay_index);

        overlay = &overlays[overlay_index];
        byte_arr_ptr = overlay->unk_0x4;

        osWritebackDCacheAll();
        nuPiReadRom(overlay->rom_start_addr,
                    overlay->overlay_start_addr,
                    overlay->rom_end_addr - overlay->rom_start_addr);
        bzero(overlay->bss_start_addr, overlay->overlay_end_addr - overlay->bss_start_addr);
        osWritebackDCache(overlay->bss_start_addr, overlay->overlay_end_addr - overlay->bss_start_addr);
        osInvalICache(overlay->overlay_start_addr, overlay->data_start_addr - overlay->overlay_start_addr);

        if (overlay->constructor != NULL)
        {
            overlay->constructor();
        }
        
        if (*byte_arr_ptr != -1)
        {  
            for(byte_index = 0; byte_arr_ptr[byte_index] != -1; byte_index++)
            {
                byte = byte_arr_ptr[byte_index];
                D_800FC858[byte] = 1;
            }
        }
        
        overlay->active = true;

        LOG_INFO1("Complete Load ModuleSet %x (absolute No.)\n", overlay_index);
        success = true;
    }
    else
    {
        LOG_INFO1("Can't Load ModuleSet %x (logical No.)\n", overlay_index);
        success = false;
    }

    return success;
}

void unload_overlay(s32 overlay_index)
{
    MGOverlayInfo *overlay;
    u32 byte_index;
    s8 *byte_arr_ptr;
    s8 byte;

    overlay_index = compute_overlay_index(overlay_index);
    overlay = &overlays[overlay_index];
    if (!overlay->active)
    {
        return;
    }
    
    if (overlay->destructor != NULL)
    {
        overlay->destructor();
    }
    
    byte_arr_ptr = overlay->unk_0x4;
    overlay->active = false;
    if (!func_800299F4(0x4DU)) /* Doesn't match with the macro :( */
    {
        osSyncPrintf("Moduleset %x Disposed\n", overlay_index);
    }

    if (*byte_arr_ptr != -1)
    {  
        for(byte_index = 0; byte_arr_ptr[byte_index] != -1; byte_index++)
        {
            byte = byte_arr_ptr[byte_index];
            D_800FC858[byte] = 0;
        }
    }
    
    osWritebackDCacheAll();
    
    if ((D_800B67C0 != 0) && (osVirtualToPhysical(overlay->overlay_start_addr) <= 0x3FFFFFU))
    {
        bzero(overlay->overlay_start_addr, overlay->data_start_addr - overlay->overlay_start_addr);
        osWritebackDCache(overlay->overlay_start_addr, overlay->data_start_addr - overlay->overlay_start_addr);
        osInvalICache(overlay->overlay_start_addr, overlay->data_start_addr - overlay->overlay_start_addr);
    }
}

/* Likely unused */
s32 func_80026358(void)
{
    LOG_INFO("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    LOG_INFO("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    return 0;
}
