#include "include_asm.h"
#include "ultra64.h"

extern OSThread boot_thread;
extern void boot_thread_entry(void *args);
extern u64 boot_thread_sp;

void mg64_main(void)
{
    __osInitialize_common();
    osAiSetFrequency(32000);
    osCreateThread(&boot_thread, 1, &boot_thread_entry, NULL, &boot_thread_sp, 10);
    osStartThread(&boot_thread);
}

INCLUDE_ASM("asm/nonmatchings/main", boot_thread_entry);

INCLUDE_ASM("asm/nonmatchings/main", func_800995F0);

INCLUDE_ASM("asm/nonmatchings/main", func_8009973C);

INCLUDE_ASM("asm/nonmatchings/main", func_800999C4);
