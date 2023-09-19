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
    /* 0x10 */ void *segment_start_addr;
    /* 0x14 */ void *data_start_addr;
    /* 0x18 */ void *bss_start_addr;
    /* 0x1C */ void *segment_end_addr;
    /* 0x20 */ void (*constructor)(void); /* These likely take some void ptr arguments... */
    /* 0x24 */ void (*destructor)(void);
} MGSegmentInfo;

extern void func_80080DCC();
extern void func_801FD624();
extern s32 func_800299F4(u32);
extern void nuPiReadRom(u32 rom_addr, void* buf_ptr, u32 size);

MGSegmentInfo segments[17];
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
        const s32 segment_index = i;

        /* Get address of data pointed to by the next overlay */
        byte_arr_ptr = segments[segment_index].unk_0x4;

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

s32 get_segment_index(s32 segment_num)
{
    s32 segment_index;

    if (segment_num < 0x100 || (u32)segment_num >= 0x111U)
    {
        LOG_INFO("Illegal ModuleSetNo.\n");
        segment_index = 0;
    }
    else
    {
        segment_index = segment_num & 0xFF;
    }

    return segment_index;
}

bool func_80025F18(s32 arg0)
{
    u32 segment_index;
    u32 new_segment_index;
    u32 byte_index;
    s8 *bytes;
    s8 byte;

    new_segment_index = get_segment_index(arg0);

    if (segments[new_segment_index].active)
    {
        LOG_INFO("Already Loaded\n");
        return true;
    }

    for (segment_index = 0; segment_index < OVERLAY_COUNT; segment_index++)
    {
        if (segment_index == new_segment_index
            || segments[segment_index].segment_start_addr > segments[new_segment_index].segment_end_addr
            || segments[segment_index].segment_end_addr < segments[new_segment_index].segment_start_addr)
        {
            /* discard */
            continue;
        }
        
        if (segments[segment_index].active)
        {
            LOG_INFO2("Conflicting %d m.s. and other moduleset[%x] is alive\n",
                      new_segment_index,
                      segment_index);
            return false;
        }

        /* ??? */
        bytes = segments[segment_index].unk_0x4;
        
        for (byte_index = 0; bytes[byte_index] != -1; byte_index++)
        {
            byte = bytes[byte_index];

            if (D_800B67F0 - D_8012CFC0[byte] < 2)
            {
                LOG_INFO3("Conflicting %d m.s. and other module[%x-%x]'s RDP request\n",
                          new_segment_index,
                          segment_index,
                          byte_index);
                return false;
            }
        }
    }

    return true;
}

#ifdef NON_MATCHING
bool func_800260C4(s32 arg0)
{
    void (*initialize_segment)(void);
    bool success;
    u32 segment_index;
    MGSegmentInfo *segment;
    s8 *byte_arr_ptr;
    s8 *cursor;

    if (func_80025F18(arg0))
    {
        segment_index = get_segment_index(arg0);
        LOG_INFO1("Load ModuleSet %x (absolute No.)\n", segment_index);

        segment = &segments[segment_index];
        byte_arr_ptr = segment->unk_0x4;

        osWritebackDCacheAll();
        nuPiReadRom(segment->rom_start_addr,
                    segment->segment_start_addr,
                    segment->rom_end_addr - segment->rom_start_addr);
        bzero(segment->bss_start_addr, segment->segment_end_addr - segment->bss_start_addr);
        osWritebackDCache(segment->bss_start_addr, segment->segment_end_addr - segment->bss_start_addr);
        osInvalICache(segment->segment_start_addr, segment->data_start_addr - segment->segment_start_addr);

        initialize_segment = segment->constructor;
        if (initialize_segment != NULL)
        {
            initialize_segment();
        }

        if (*byte_arr_ptr != -1)
        {
            for (cursor = byte_arr_ptr; *cursor != -1; cursor++)
            {
                D_800FC858[*cursor] = 1;
            }
        }

        segment->active = true;

        LOG_INFO1("Complete Load ModuleSet %x (absolute No.)\n", segment_index);
        success = true;
    }
    else
    {
        LOG_INFO1("Can't Load ModuleSet %x (logical No.)\n", arg0);
        success = false;
    }

    return success;
}
#else
INCLUDE_ASM("asm/nonmatchings/overlay_manager", func_800260C4);
#endif

#ifdef NON_MATCHING
void func_80026230(s32 arg0)
{
    void (*segment_destructor)();
    s32 segment_index;
    MGSegmentInfo *segment;
    s8 *byte_arr_ptr;
    s8 *cursor;

    segment_index = get_segment_index(arg0);

    segment = &segments[segment_index];
    if (!segment->active)
    {
        return;
    }
    
    segment_destructor = segment->destructor;
    if (segment_destructor != NULL)
    {
        segment_destructor();
    }
    
    byte_arr_ptr = segment->unk_0x4;
    segment->active = false;
    LOG_INFO1("Moduleset %x Disposed\n", arg0);
    
    if (*byte_arr_ptr != -1)
    {
        for (cursor = byte_arr_ptr; *cursor != -1; cursor++)
        {
            D_800FC858[*cursor] = 0;
        }
    }
    
    osWritebackDCacheAll();
    
    if ((D_800B67C0 != 0) && (osVirtualToPhysical(segment->segment_start_addr) <= 0x3FFFFFU))
    {
        bzero(segment->segment_start_addr, segment->data_start_addr - segment->segment_start_addr);
        osWritebackDCache(segment->segment_start_addr, segment->data_start_addr - segment->segment_start_addr);
        osInvalICache(segment->segment_start_addr, segment->data_start_addr - segment->segment_start_addr);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/overlay_manager", func_80026230);
#endif

s32 func_80026358(void)
{
    LOG_INFO("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    LOG_INFO("Module %d not alive !!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    return 0;
}
