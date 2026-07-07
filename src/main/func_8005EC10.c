#include "common.h"
#include <nusys.h>
#include <nualstl.h>

extern musConfig D_800C3010;

extern void bgm_alloc_song_buffer(s32 size);
extern void func_8005F0BC(s32 size);

void func_8005EC48(s16 arg0, s32 arg1);

void func_8005EC10(NUAuPreNMIFunc func) {
  OSIntMask mask = osSetIntMask(OS_IM_NONE);

  nuAuPreNMIFunc = func;
  osSetIntMask(mask);
}

INCLUDE_ASM("asm/nonmatchings/main/func_8005EC10", func_8005EC48);

INCLUDE_ASM("asm/nonmatchings/main/func_8005EC10", func_8005ECC4);

void audio_system_boot(musConfig* config) {
  if (config == NULL) {
    config = &D_800C3010;
  }

  nuAuStlMgrInit(config);
  bgm_alloc_song_buffer(0xC000);
  func_8005F0BC(0x4000);
  func_8005EC10(func_8005EC48);
}
