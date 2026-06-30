/*
 * NuSYS boot: the cartridge entry point and the idle thread.
 *
 * This is the game's copy of the nusys nuboot.c template, compiled into the
 * main segment at -O0 and merged with the cart entry: nuBoot (the symbol the
 * entry stub jumps to) brings up the OS and spawns idle, the nusys idle thread.
 *
 * It is a SEPARATE, game-embedded nusys instance from the mapped libnusys
 * library, so its IdleThread / MainThread / IdleStack / nuIdleFunc are this
 * instance's own objects (a distinct address set); the symbol names are shared
 * with the library copy via allow_duplicated in symbol_addrs.txt.
 *
 * Customizations vs stock nuboot: nuBoot sets the audio DAC to 32 kHz before
 * spawning idle. (The main-thread stack passed in idle resolves to nuContNum,
 * the game's reused boot-stack region.)
 */
#include <nusys.h>

extern void mainproc(void* arg);

/* This instance's own thread/stack objects (static in stock nuboot). */
extern OSThread IdleThread;
extern OSThread MainThread;
extern u64 IdleStack[NU_IDLE_STACK_SIZE / sizeof(u64)];

void idle(void* arg);

void nuBoot(void) {
  osInitialize();
  osAiSetFrequency(32000);
  osCreateThread(&IdleThread, NU_IDLE_THREAD_ID, idle, NULL,
                 IdleStack + NU_IDLE_STACK_SIZE / 8, 10);
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
