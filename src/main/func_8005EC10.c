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

void func_8005EC48(s16 msg_type, s32 frame_count) {
  if (msg_type == NU_SC_RETRACE_MSG) {
    goto retrace;
  }

  if (msg_type == NU_SC_PRENMI_MSG) {
    MusStop(1, 0);
    MusSetFxType(0);
  }
  return;

retrace:
  if (frame_count == 20) {
    MusSetFxType(0);
  }

  if (frame_count == 28) {
    nuAuTaskStop = NU_AU_TASK_STOP;
  }
}

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
