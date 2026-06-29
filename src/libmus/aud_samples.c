/*
 * aud_samples.c
 *
 * Per-frame audio sample budgeting for the libmus sequence player. The synth
 * runs off the video retrace clock but feeds a DAC running on its own sample
 * clock, so the two drift relative to each other. SamplesInit sizes the output
 * buffers once at startup; SamplesCurrent then decides, frame by frame, how
 * many samples to synthesize so the queued depth stays near a target band.
 * Sizes are quantized to a fixed block of samples (16 for stock libaudio,
 * N_SAMPLES for the naudio build).
 */

#include "libmus_config.h"
#include <ultra64.h>
#include "aud_samples.h"

/* Sample-count quantization block for the naudio build; the stock libaudio
 * path uses a block of 16 instead. All buffer sizes are rounded to this. */
#define N_SAMPLES 184

/* Audio queue-depth state, in DAC samples (naudio path). Written by
 * SamplesInit, read each frame by SamplesCurrent. */
extern u32 g_mus_frame_samples;      // nominal samples synthesized per frame
extern u32 g_mus_frame_samples_min;  // low-water target (nominal - one block)
extern u32 g_mus_frame_samples_max;  // high-water target (nominal + one block)
extern u32 g_mus_extra_samples;      // headroom = nominal * extra_rate / 100

/*
 * Size the audio sample buffers at startup and return the worst-case buffer
 * size, in samples, the caller must allocate. retrace_count is the number of
 * video retraces per audio update, output_rate the DAC rate in Hz,
 * vsyncs_per_sec the video field rate, and extra_rate a headroom percentage.
 * Establishes the nominal/min/max per-frame counts used by SamplesCurrent.
 */
#ifndef SUPPORT_NAUDIO
u32 __MusIntSamplesInit(u32 retrace_count, u32 output_rate, u32 vsyncs_per_sec,
                        u32 extra_rate) {
  u32 calc;

  // Samples produced across retrace_count video frames, rounded up.
  calc = (retrace_count * output_rate + vsyncs_per_sec - 1) / vsyncs_per_sec;

  // Round up to a 16-sample block; the +1 always leaves headroom above calc.
  frame_samples = ((calc / 16) + 1) * 16;
  frame_samples_min = frame_samples - 16;
  extra_samples = frame_samples * extra_rate / 100;
  return (frame_samples + 16 + extra_samples);
}
#else
u32 __MusIntSamplesInit(u32 retrace_count, u32 output_rate, u32 vsyncs_per_sec,
                        u32 extra_rate) {
  u32 calc;

  // Samples produced across retrace_count video frames, rounded up.
  calc = (retrace_count * output_rate + vsyncs_per_sec - 1) / vsyncs_per_sec;

  // Round up to an N_SAMPLES block (the +1 leaves headroom above calc), then
  // derive the low/high water marks and the percentage headroom.
  g_mus_frame_samples = ((calc / N_SAMPLES) + 1) * N_SAMPLES;
  g_mus_frame_samples_min = g_mus_frame_samples - N_SAMPLES;
  g_mus_frame_samples_max = g_mus_frame_samples + N_SAMPLES;
  g_mus_extra_samples = g_mus_frame_samples * extra_rate / 100;
  return (g_mus_frame_samples + N_SAMPLES + g_mus_extra_samples);
}
#endif

/*
 * Return how many samples to synthesize this frame to hold the DAC queue near
 * its target depth. samples is the count still queued in the DAC. Because the
 * synth and DAC clocks drift, production is nudged up or down by one block when
 * the queue strays outside the normal band.
 */
#ifndef SUPPORT_NAUDIO
u32 __MusIntSamplesCurrent(u32 samples) {
  // Amount to top the queue back up to the target depth, aligned down to a
  // 16-sample block; never fall below the low-water mark.
  samples = (frame_samples + extra_samples + 16 - samples) & (~15);
  if (samples < frame_samples_min) return (frame_samples_min);
  return (samples);
}
#else
u32 __MusIntSamplesCurrent(u32 samples) {
  // Fire each correction only once per excursion; re-armed on return to band.
  static u32 only_one_flag = 1;

  if (samples > N_SAMPLES + g_mus_extra_samples) {
    // Queue too deep: synthesize one short frame to let it drain.
    if (only_one_flag) {
      only_one_flag = 0;
      return (g_mus_frame_samples_min);
    }
  } else if (samples < g_mus_extra_samples) {
    // Queue too shallow: synthesize one long frame to refill it.
    if (only_one_flag) {
      only_one_flag = 0;
      return (g_mus_frame_samples_max);
    }
  } else {
    // Within the normal band: re-arm and run at the nominal rate.
    only_one_flag = 1;
  }
  return (g_mus_frame_samples);
}
#endif
