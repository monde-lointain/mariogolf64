#include "common.h"
#include <nusys.h>
#include <nualstl.h>

void func_8005EC10(NUAuPreNMIFunc func) {
  OSIntMask mask = osSetIntMask(OS_IM_NONE);

  nuAuPreNMIFunc = func;
  osSetIntMask(mask);
}

INCLUDE_ASM("asm/nonmatchings/main/func_8005EC10", func_8005EC48);

INCLUDE_ASM("asm/nonmatchings/main/func_8005EC10", func_8005ECC4);

INCLUDE_ASM("asm/nonmatchings/main/func_8005EC10", audio_system_boot);
