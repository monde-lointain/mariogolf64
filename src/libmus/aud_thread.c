
/*************************************************************

  aud_thread.c : Nintendo 64 Music Tools Programmers Library
  (c) Copyright 1997/1998, Software Creations (Holdings) Ltd.

  Version 3.14

  Music library thread base audio manager.

**************************************************************/

/* include configuartion */
#include "libmus_config.h"

/* include system headers */
#include <ultra64.h>
#ifndef SUPPORT_NAUDIO
#include <libaudio.h>
#else
#include <n_libaudio_sc.h>
#include <n_libaudio_sn_sc.h>
#endif

/* include other header files */
#include "libmus.h"
#include "lib_memory.h"
#include "aud_sched.h"
#include "aud_dma.h"
#include "aud_samples.h"

#ifdef SUPPORT_FXCHANGE
#include "player_fx.h"
#endif

/* include current header file */
#include "aud_thread.h"

/* __MusIntThreadProcess is CARRIED as INCLUDE_ASM (MG64-custom body); needs the
 * macro */
#include "include_asm.h"

/* internal macros */
#define AUDIO_STACKSIZE 0x2000 /* size of stack for thread (in bytes) */
#define EXTRA_SAMPLES 30       /* ratio[%] of extra samples per frame */
#define EXTRA_SAMPLES_N 20     /* ratio[%] of extra eamples (if n_audio) */
#define NUM_OUTPUT_BUFFERS 3   /* number of output buffers */

#ifndef SUPPORT_NAUDIO
#define MICROCODE_CODE aspMainTextStart
#define MICROCODE_DATA aspMainDataStart
#else
#define MICROCODE_CODE n_aspMainTextStart
#define MICROCODE_DATA n_aspMainDataStart
#endif

/* internal data structures */
typedef struct {
  short* data;
  int frame_samples;
} audio_task_t;

/* internal function prototypes (carried, see INCLUDE_ASM at end of file) */
void __MusIntThreadProcess(void* ignored);

/* internal workspace (drop-static-mirror: placed at curated/recovered main_bss
 * vrams) */
extern audio_task_t* g_mus_audio_tasks; /* audio_tasks        @ 0x800E72A4 */
extern Acmd* g_mus_audio_command_list;  /* audio_command_list @ 0x800E72A8 */
extern OSThread g_mus_audio_thread;     /* thread (func-static) @ 0x800E70F0 */
extern u64* g_mus_audio_stack; /* stack_addr (func-static) @ 0x800E72A0 */

/* global workspace (drop-def: placed at recovered main_bss vram 0x801B56A0) */
extern ALGlobals __libmus_alglobals;

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
  [EXTERNAL FUNCTION]
  __MusIntAudManInit(config, vsyncs_per_second, fx_type)

  [Parameters]
  config       address of library configuration structure
  vsyncs_per_second  number of video refreshes per seconds
  fx_type      audio library effect type

  [Explanation]
  Initialise audio thread and all audio managment functionality.

  [Return value]
  NONE
*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

void __MusIntAudManInit(musConfig* config, int vsyncs_per_second, int fx_type) {
  u32 i;
  ALSynConfig syn_config;
  u32 extra_rate;
  u32 samples_per_frame;

  /* setup syn configuartion structure */
  syn_config.maxVVoices = syn_config.maxPVoices = config->channels;
  syn_config.maxUpdates = config->syn_updates;
  syn_config.dmaproc =
      __MusIntDmaInit(config->syn_num_dma_bufs, config->syn_dma_buf_size);
  syn_config.fxType = fx_type;
  syn_config.outputRate = osAiSetFrequency(config->syn_output_rate);
  syn_config.heap = __MusIntMemGetHeapAddr();

  /* initialise syn driver */
  alInit(&__libmus_alglobals, &syn_config);

  /* initialise sample buffer manager */
#ifndef SUPPORT_NAUDIO
  extra_rate = EXTRA_SAMPLES;
#else
  extra_rate = EXTRA_SAMPLES_N;
#endif

  samples_per_frame = __MusIntSamplesInit((u32)config->syn_retraceCount,
                                          (u32)syn_config.outputRate,
                                          (u32)vsyncs_per_second, extra_rate);

  /* allocate audio command list */
  g_mus_audio_command_list =
      (Acmd*)__MusIntMemMalloc(config->syn_rsp_cmds * sizeof(Acmd));

  /* initialise task control structures */
  g_mus_audio_tasks =
      __MusIntMemMalloc(NUM_OUTPUT_BUFFERS * sizeof(audio_task_t));
  for (i = 0; i < NUM_OUTPUT_BUFFERS; i++)
    g_mus_audio_tasks[i].data = __MusIntMemMalloc(4 * samples_per_frame);

  /* create and start thread */
  g_mus_audio_stack = __MusIntMemMalloc(AUDIO_STACKSIZE);
  osCreateThread(&g_mus_audio_thread, 3, __MusIntThreadProcess, 0,
                 (void*)(g_mus_audio_stack + AUDIO_STACKSIZE / sizeof(u64)),
                 config->thread_priority);
  osStartThread(&g_mus_audio_thread);
}

/* __MusIntThreadProcess: MG64-customized body (a pause/mute block on the byte
   flag @0x800C7AE0 + sched-state globals @0x800C7AE4/AE8 not in the libmus 3.14
   source) -> classical, carried S143. */
INCLUDE_ASM("asm/nonmatchings/libmus/aud_thread", __MusIntThreadProcess);
