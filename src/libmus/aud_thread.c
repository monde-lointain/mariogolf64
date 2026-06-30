/*
 * aud_thread.c
 *
 * The libmus audio manager thread. At startup it programs the DAC, brings up
 * the synthesizer, and allocates the RSP command list plus a set of rotating
 * output buffers sized to the per-frame sample budget. It then runs a
 * never-ending per-frame loop that renders one buffer of audio into an RSP
 * command list, hands that list to the scheduler for the RSP to execute, and
 * queues finished buffers to the audio DAC. Output is triple-buffered so the
 * RSP can render one buffer while the DAC plays another.
 */
#include "libmus_config.h"
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

#define AUDIO_STACKSIZE 0x2000 /* bytes of stack for the audio thread */

/* Headroom, as a percent of the per-frame sample budget, that the synth may
 * render past the nominal frame length to absorb retrace jitter. naudio and
 * libaudio pace differently, so each build gets its own margin. */
#define EXTRA_SAMPLES 30
#define EXTRA_SAMPLES_N 20

#define NUM_OUTPUT_BUFFERS 3 /* rotating output buffers (triple buffering) */

/* Audio microcode symbols: which RSP task image runs the synth, picked by
 * build. MICROCODE_DATA stays the audio ucode's data half; MICROCODE_CODE is
 * immediately overridden below. */
#ifndef SUPPORT_NAUDIO
#define MICROCODE_CODE aspMainTextStart
#define MICROCODE_DATA aspMainDataStart
#else
#define MICROCODE_CODE n_aspMainTextStart
#define MICROCODE_DATA n_aspMainDataStart
#endif

/* The audio microcode text is linked directly after the RSP boot stub, so its
 * first instruction sits at the boot stub's end label. Point the task's text
 * pointer there instead of at the ucode's own start symbol. */
#undef MICROCODE_CODE
#define MICROCODE_CODE rspbootTextEnd

/* One rotating output buffer: its rendered PCM and the sample count the synth
 * actually wrote into it this frame. */
typedef struct {
  short* data;
  int frame_samples;
} audio_task_t;

static void __MusIntThreadProcess(void* ignored);

/*
 * Shared audio-thread state, defined in the libmus globals TU: audio_tasks is
 * the array of NUM_OUTPUT_BUFFERS rotating output buffers; audio_command_list
 * is the RSP command-list scratch; thread/stack_addr are the audio thread and
 * its stack base; last_task is the buffer currently queued to the DAC (played
 * next frame); g_mus_audio_paused gates rendering; and
 * g_mus_audio_silence_buffer is the 16-byte run of silence emitted while
 * paused.
 */
extern audio_task_t* audio_tasks;
extern Acmd* audio_command_list;
extern OSThread thread;
extern u64* stack_addr;
extern audio_task_t* last_task;
extern u8 g_mus_audio_paused;
extern u8 g_mus_audio_silence_buffer[0x10];

/*
 * One-time startup for the audio manager: assemble the synth configuration,
 * program the DAC rate, bring up the synthesizer, allocate the RSP command list
 * and the rotating output buffers sized to the frame's sample budget, then
 * create and start the audio thread. config carries the caller's voice, update,
 * DMA, and heap choices; vsyncs_per_second and fx_type tune the sample pacing
 * and the effects unit.
 */
void __MusIntAudManInit(musConfig* config, int vsyncs_per_second, int fx_type) {
  u32 i;
  ALSynConfig syn_config;
  u32 extra_rate;
  u32 samples_per_frame;

  /* Both the virtual and physical voice counts track the requested channels. */
  syn_config.maxVVoices = syn_config.maxPVoices = config->channels;
  syn_config.maxUpdates = config->syn_updates;

  /* Stand up the sample-streaming DMA subsystem; it returns the callback the
   * synth invokes to fetch wavetable data. */
  syn_config.dmaproc =
      __MusIntDmaInit(config->syn_num_dma_bufs, config->syn_dma_buf_size);
  syn_config.fxType = fx_type;

  /* osAiSetFrequency programs the DAC and returns the nearest rate it could
   * actually achieve, which the synth must render at. */
  syn_config.outputRate = osAiSetFrequency(config->syn_output_rate);
  syn_config.heap = __MusIntMemGetHeapAddr();
  alInit(&__libmus_alglobals, &syn_config);

#ifndef SUPPORT_NAUDIO
  extra_rate = EXTRA_SAMPLES;
#else
  extra_rate = EXTRA_SAMPLES_N;
#endif

  /* Derive the per-frame sample budget from the retrace rate versus the DAC
   * rate, plus the jitter headroom. */
  samples_per_frame = __MusIntSamplesInit((u32)config->syn_retraceCount,
                                          (u32)syn_config.outputRate,
                                          (u32)vsyncs_per_second, extra_rate);

  audio_command_list =
      (Acmd*)__MusIntMemMalloc(config->syn_rsp_cmds * sizeof(Acmd));
  audio_tasks = __MusIntMemMalloc(NUM_OUTPUT_BUFFERS * sizeof(audio_task_t));

  /* 4 bytes per sample (16-bit stereo) times the frame budget per buffer. */
  for (i = 0; i < NUM_OUTPUT_BUFFERS; i++) {
    audio_tasks[i].data = __MusIntMemMalloc(4 * samples_per_frame);
  }

  /* Launch the thread with its stack pointer at the top of the block, since
   * the MIPS stack grows downward. */
  stack_addr = __MusIntMemMalloc(AUDIO_STACKSIZE);
  osCreateThread(&thread, 3, __MusIntThreadProcess, 0,
                 (void*)(stack_addr + (AUDIO_STACKSIZE / sizeof(u64))),
                 config->thread_priority);
  osStartThread(&thread);
}

/*
 * Audio thread body; never returns. Each scheduler frame it renders one buffer
 * of audio, submits the resulting RSP command list as an audio task, and queues
 * the previous frame's finished buffer to the DAC. The output buffers cycle so
 * the RSP can work on one while the DAC plays another. While paused it emits
 * silence and skips rendering.
 */
static void __MusIntThreadProcess(void* ignored) {
  Acmd* cmdp;
  s32 commands;
  u32 task_count;
  u32 samples;
  u32 status;
  audio_task_t* task;
  musTask sched_task;

  /* Per-frame task template; the ucode and command-list pointers stay constant
   * across frames, so only data_size is refilled each iteration. */
  sched_task.ucode = (u64*)MICROCODE_CODE;
  sched_task.ucode_data = (u64*)MICROCODE_DATA;
  sched_task.data = (u64*)audio_command_list;
  task_count = 0;

  __MusIntSched_install();
  while (1) {
    /* Block until the scheduler reports a VI retrace (one audio frame). */
    __MusIntSched_waitframe();

    /* While paused, keep the DAC fed with a short run of silence and render
     * nothing this frame. */
    if (g_mus_audio_paused) {
      osAiSetNextBuffer(g_mus_audio_silence_buffer, 0x10);
      continue;
    }

    /* Samples still queued in the AI DMA: byte length / 4 (16-bit stereo). */
    status = osAiGetStatus();
    samples = osAiGetLength() >> 2;

    /* Both AI DMA slots are busy, so there is no room to enqueue; skip. */
    if (status & AI_STATUS_FIFO_FULL) {
      continue;
    }

    /* Retire completed sample DMAs and age the wavetable keep-counts. */
    __MusIntDmaProcess();

    /* Once a frame has been rendered, hand the previous frame's finished buffer
     * to the DAC (frame_samples << 2 = byte length). last_task stays NULL until
     * the first task is submitted, so commands is always set by alAudioFrame
     * before it is read here. */
    if (last_task && commands) {
      osAiSetNextBuffer(last_task->data, last_task->frame_samples << 2);
    }

    /* Pick this frame's buffer, then choose its render length to track the
     * DAC's consumption: grow or shrink toward the budget by how many samples
     * remain queued (drift control). */
    task = &audio_tasks[task_count];
    task->frame_samples = __MusIntSamplesCurrent(samples);

    /* Render the frame: build the command list into audio_command_list,
     * targeting the buffer's physical address. commands is set nonzero when the
     * synth produced work, and cmdp points just past the last command. */
    cmdp = alAudioFrame(audio_command_list, &commands,
                        (short*)osVirtualToPhysical(task->data),
                        task->frame_samples);

    /* Submit the command list to the scheduler for the RSP to run, and remember
     * this buffer so the next frame queues it to the DAC. */
    if (commands) {
      sched_task.data_size = (cmdp - audio_command_list) * sizeof(Acmd);
      __MusIntSched_dotask(&sched_task);
      last_task = task;
    }

    /* Advance to the next output buffer. */
    task_count = (task_count + 1) % NUM_OUTPUT_BUFFERS;
  }
}
