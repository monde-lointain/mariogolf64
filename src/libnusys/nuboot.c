/*
 * NuSYS boot: the cartridge entry point and the idle thread.
 *
 * This is the game's copy of the nusys nuboot.c template, compiled into the
 * main segment at -O0 and merged with the cart entry: nuBoot (the symbol the
 * entry stub jumps to) brings up the OS and spawns idle, the nusys idle thread.
 *
 * It is a SEPARATE, game-embedded nusys instance from the mapped libnusys
 * library, so its IdleThread / MainThread / nuIdleFunc are this instance's own
 * objects (a distinct address set); the symbol names are shared with the
 * library copy via allow_duplicated in symbol_addrs.txt.
 *
 * Customizations vs stock nuboot: nuBoot sets the audio DAC to 32 kHz before
 * spawning idle, and the idle/main thread stacks reuse existing data regions
 * (plr_player / nuContNum); the original relocs name them, so they are
 * referenced as-is.
 */
#include "common.h"

/* os_host.h aliases __osInitialize_common() to osInitialize(); the cart calls
 * the real symbol directly, so drop the alias before declaring it. */
#undef __osInitialize_common
extern void __osInitialize_common(void);

extern void nuPiInit(void);
extern void nuScCreateScheduler(u8 video_mode, u8 num_fields);
extern void mainproc(void* arg);

extern OSThread IdleThread;
extern OSThread MainThread;
extern void (*nuIdleFunc)(void); /* idle-loop callback */
extern u8 plr_player[];          /* reused as the idle-thread boot stack */
extern u32 nuContNum; /* its address is reused as the main-thread stack */

void idle(void* arg);

void nuBoot(void) {
  __osInitialize_common();
  osAiSetFrequency(0x7D00);
  osCreateThread(&IdleThread, 1, idle, NULL, plr_player, 0xA);
  osStartThread(&IdleThread);
}

void idle(void* arg) {
  nuIdleFunc = NULL;
  nuPiInit();
  nuScCreateScheduler(OS_VI_NTSC_LAN1, 1);
  osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON | OS_VI_GAMMA_OFF |
                         OS_VI_GAMMA_DITHER_OFF | OS_VI_DIVOT_ON);
  osCreateThread(&MainThread, 3, mainproc, NULL, &nuContNum, 0xA);
  osStartThread(&MainThread);
  osSetThreadPri(&IdleThread, 0);
  while (1) {
    if (nuIdleFunc != NULL) {
      (*nuIdleFunc)();
    }
  }
}
