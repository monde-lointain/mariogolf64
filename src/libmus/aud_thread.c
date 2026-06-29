/*
 * aud_thread.c
 *
 * libmus audio manager and its OS thread. __MusIntAudManInit builds the audio
 * synthesis driver (alInit), sizes per-frame sample timing, allocates the RSP
 * command list, the round-robin output buffers, and the thread stack, then
 * creates and starts the audio thread. __MusIntThreadProcess is that thread:
 * once per video retrace it hands the buffer it synthesized last frame to the
 * audio interface, builds the next frame's audio command list, and dispatches
 * the RSP audio task.
 */
#include "libmus_config.h"
#include <ultra64.h>
#ifndef SUPPORT_NAUDIO
#include <libaudio.h>
#else
#include <n_libaudio_sc.h>
#include <n_libaudio_sn_sc.h>
#endif
#include "libmus.h"
#include "lib_memory.h"
#include "aud_sched.h"
#include "aud_dma.h"
#include "aud_samples.h"
#ifdef SUPPORT_FXCHANGE
#include "player_fx.h"
#endif
#include "aud_thread.h"

// Audio thread stack size, in bytes.
#define AUDIO_STACKSIZE 0x2000

// Per-frame synth headroom, expressed as a percentage of the nominal sample
// count (applied as samples * rate / 100); the libaudio and naudio builds use
// different margins.
#define EXTRA_SAMPLES 30
#define EXTRA_SAMPLES_N 20

// Triple-buffered output: one playing, one queued, one being filled.
#define NUM_OUTPUT_BUFFERS 3

// Audio synthesis microcode symbols, selected by build (libaudio vs n_audio).
#ifndef SUPPORT_NAUDIO
#define MICROCODE_CODE aspMainTextStart
#define MICROCODE_DATA aspMainDataStart
#else
#define MICROCODE_CODE n_aspMainTextStart
#define MICROCODE_DATA n_aspMainDataStart
#endif

// Build-specific override: this ROM links the audio microcode text immediately
// after the rspboot text, so the task's microcode pointer is taken as the end
// of rspboot rather than the aspMain start symbol. Only the code pointer is
// redirected; MICROCODE_DATA still names the aspMain data symbol.
#undef MICROCODE_CODE
#define MICROCODE_CODE rspbootTextEnd

/* One synthesized output frame: its sample buffer and the count it holds. */
typedef struct {
  short* data;
  int frame_samples;
} audio_task_t;

static void __MusIntThreadProcess(void* ignored);

extern audio_task_t* g_mus_audio_tasks;
extern Acmd* g_mus_audio_command_list;
extern OSThread g_mus_audio_thread;
extern u64* g_mus_audio_stack;
extern audio_task_t* g_mus_audio_last_task;
extern u8 g_mus_audio_paused;
extern u8 g_mus_audio_silence_buffer[0x10];

/*
 * Audio manager setup: stand up the audio subsystem from the caller's
 * musConfig. Configure and initialize the synthesis driver, work out the
 * per-frame sample budget, allocate the command list / output buffers / thread
 * stack, then create and start the audio manager thread.
 */
void __MusIntAudManInit(musConfig* config, int vsyncs_per_second, int fx_type) {
  u32 i;
  ALSynConfig syn_config;
  u32 extra_rate;
  u32 samples_per_frame;

  // Translate the public config into a synthesis config. Virtual and physical
  // voice counts are both the requested channel count; osAiSetFrequency
  // programs the audio interface and returns the actual rate it could achieve.
  syn_config.maxVVoices = syn_config.maxPVoices = config->channels;
  syn_config.maxUpdates = config->syn_updates;
  syn_config.dmaproc =
      __MusIntDmaInit(config->syn_num_dma_bufs, config->syn_dma_buf_size);
  syn_config.fxType = fx_type;
  syn_config.outputRate = osAiSetFrequency(config->syn_output_rate);
  syn_config.heap = __MusIntMemGetHeapAddr();
  alInit(&__libmus_alglobals, &syn_config);

  // The headroom percentage differs between the libaudio and n_audio builds.
#ifndef SUPPORT_NAUDIO
  extra_rate = EXTRA_SAMPLES;
#else
  extra_rate = EXTRA_SAMPLES_N;
#endif
  samples_per_frame = __MusIntSamplesInit((u32)config->syn_retraceCount,
                                          (u32)syn_config.outputRate,
                                          (u32)vsyncs_per_second, extra_rate);

  // Allocate the RSP command list and the output buffers. Each output buffer
  // holds samples_per_frame stereo 16-bit samples (4 bytes apiece).
  g_mus_audio_command_list =
      (Acmd*)__MusIntMemMalloc(config->syn_rsp_cmds * sizeof(Acmd));
  g_mus_audio_tasks =
      __MusIntMemMalloc(NUM_OUTPUT_BUFFERS * sizeof(audio_task_t));
  for (i = 0; i < NUM_OUTPUT_BUFFERS; i++) {
    g_mus_audio_tasks[i].data = __MusIntMemMalloc(4 * samples_per_frame);
  }

  // Allocate the stack and launch the audio thread. The stack grows down, so
  // the entry stack pointer is the top of the freshly allocated block.
  g_mus_audio_stack = __MusIntMemMalloc(AUDIO_STACKSIZE);
  osCreateThread(&g_mus_audio_thread, 3, __MusIntThreadProcess, 0,
                 (void*)(g_mus_audio_stack + (AUDIO_STACKSIZE / sizeof(u64))),
                 config->thread_priority);
  osStartThread(&g_mus_audio_thread);
}

/*
 * Audio manager thread body: after installing the scheduler it loops forever,
 * synthesizing one audio frame per video retrace: output the previous frame's
 * buffer, build the next command list, dispatch the RSP audio task, and rotate
 * through the output buffers. The argument is unused.
 */
static void __MusIntThreadProcess(void* ignored) {
  Acmd* cmdp;
  s32 commands;
  u32 task_count;
  u32 samples;
  u32 status;
  audio_task_t* task;
  musTask sched_task;

  // Constant parts of every dispatched task: the microcode and command list.
  sched_task.ucode = (u64*)MICROCODE_CODE;
  sched_task.ucode_data = (u64*)MICROCODE_DATA;
  sched_task.data = (u64*)g_mus_audio_command_list;
  task_count = 0;
  __MusIntSched_install();
  while (1) {
    __MusIntSched_waitframe();

    // While paused, keep the audio interface fed with a buffer of silence.
    if (g_mus_audio_paused) {
      osAiSetNextBuffer(g_mus_audio_silence_buffer, 0x10);
      continue;
    }

    // AI backlog: length in bytes, converted to samples (4 bytes each).
    status = osAiGetStatus();
    samples = osAiGetLength() >> 2;
    if (status & AI_STATUS_FIFO_FULL) {
      continue;
    }
    __MusIntDmaProcess();

    // Queue the buffer synthesized last frame for playback. commands carries
    // the previous frame's alAudioFrame count; g_mus_audio_last_task is NULL
    // until the first task runs, so this is skipped on the first frame.
    if (g_mus_audio_last_task && commands) {
      osAiSetNextBuffer(g_mus_audio_last_task->data,
                        g_mus_audio_last_task->frame_samples << 2);
    }

    // Pick this frame's output buffer and decide how many samples to make.
    task = &g_mus_audio_tasks[task_count];
    task->frame_samples = __MusIntSamplesCurrent(samples);
    cmdp = alAudioFrame(g_mus_audio_command_list, &commands,
                        (short*)osVirtualToPhysical(task->data),
                        task->frame_samples);

    // Dispatch the RSP task only if synthesis actually produced commands.
    if (commands) {
      sched_task.data_size = (cmdp - g_mus_audio_command_list) * sizeof(Acmd);
      __MusIntSched_dotask(&sched_task);
      g_mus_audio_last_task = task;
    }
    task_count = (task_count + 1) % NUM_OUTPUT_BUFFERS;
  }
}
