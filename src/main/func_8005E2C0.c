#include "common.h"

extern OSMesgQueue D_800E1270;
extern OSThread* D_800DF0B0;
extern s32 D_800DF0B4;
extern const char D_800D0AE4[];
extern const char D_800D0AF8[];
extern const char D_800FF428[];

extern OSThread* __osGetCurrFaultedThread(void);
extern void play_sound_effect(s32 arg0, s32 arg1, s32 arg2);
extern void func_8005E380(OSThread* thread);

void func_8005E2C0(void) { volatile u8 unused[0x18]; }

void func_8005E2CC(void) {
  OSMesg msg;

  osSetEventMesg(12, &D_800E1270, (OSMesg)0x10);
  D_800DF0B4 = 0;
  while (TRUE) {
    osSyncPrintf(D_800D0AE4);
    osRecvMesg(&D_800E1270, &msg, OS_MESG_BLOCK);
    osSyncPrintf(D_800D0AF8);
    D_800DF0B0 = __osGetCurrFaultedThread();
    if (D_800DF0B0 != NULL) {
      play_sound_effect(0x30, 0xB, 0x7F);
      func_8005E380(D_800DF0B0);
    }
  }
}

void func_8005E360(void) { osSyncPrintf(D_800FF428); }
