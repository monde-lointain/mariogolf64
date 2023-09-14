#include "include_asm.h"
#include "ultra64.h"

extern OSThread main_thread;
extern OSThread boot_thread;
extern void boot_thread_entry(void *args);
extern void main_thread_entry(void *);
extern void (*D_80132398)();
extern void func_800A16E0(void);
extern void func_800283B0(u32, u32);
extern u64 boot_thread_sp;
extern u64 main_thread_sp;

void mg64_main(void)
{
    __osInitialize_common();
    osAiSetFrequency(32000);
    osCreateThread(&boot_thread, 1, &boot_thread_entry,
                   NULL, &boot_thread_sp, 10);
    osStartThread(&boot_thread);
}

void boot_thread_entry(void *args)
{
    D_80132398 = NULL;
    func_800A16E0();
    func_800283B0(2, 1);
    osViSetSpecialFeatures(0x5AU);
    osCreateThread(&main_thread, 3, main_thread_entry, NULL, &main_thread_sp, 10);
    osStartThread(&main_thread);
    osSetThreadPri(&boot_thread, 0);

    while (1)
    {
        if (D_80132398 != NULL)
        {
            D_80132398();
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/main", func_800995F0);

INCLUDE_ASM("asm/nonmatchings/main", func_8009973C);

INCLUDE_ASM("asm/nonmatchings/main", func_800999C4);
