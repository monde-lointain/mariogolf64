/*
 * aud_samples.c
 *
 * Sizes libmus's audio output budget. The synth runs one buffer per scheduler
 * frame, so the driver must decide how many samples that buffer should hold.
 * __MusIntSamplesInit turns the host's retrace timing into a nominal per-frame
 * sample count once at startup; __MusIntSamplesCurrent then nudges that count
 * up or down every frame so the AI output queue neither drains (a gap) nor
 * overflows (a dropped frame). The audio thread in aud_thread.c is the only
 * caller.
 */

#include <ultra64.h>

#include "aud_samples.h"

/*
 * Sample quantum for the SUPPORT_NAUDIO build; the stock build quantizes to a
 * fixed 16 samples instead (see the per-branch arithmetic below).
 */
#define N_SAMPLES 184

/*
 * The per-frame budget, published by __MusIntSamplesInit and read back by
 * __MusIntSamplesCurrent (and by the audio thread). Defined and placed
 * elsewhere; this module only computes and consumes them.
 *   frame_samples     - nominal samples to synthesize per frame
 *   frame_samples_min - lower bound the running count may fall to
 *   frame_samples_max - upper bound (SUPPORT_NAUDIO build only)
 *   extra_samples     - jitter headroom, a percentage of frame_samples
 */
extern u32 frame_samples;
extern u32 frame_samples_min;
extern u32 frame_samples_max;
extern u32 extra_samples;

/*
 * Compute the per-frame sample budget from the scheduler's audio timing and
 * publish it through the module globals. retrace_count retraces are serviced
 * per scheduler frame at output_rate samples/sec against a vsyncs_per_sec
 * refresh; extra_rate is the jitter-headroom percentage. Returns the worst-case
 * sample count a single frame can need, which the caller uses to size its
 * output buffers.
 */
#ifndef SUPPORT_NAUDIO
u32 __MusIntSamplesInit(u32 retrace_count, u32 output_rate, u32 vsyncs_per_sec,
                        u32 extra_rate) {
  u32 calc;

  /* Samples produced across the serviced retraces, rounded up. */
  calc = (retrace_count * output_rate + vsyncs_per_sec - 1) / vsyncs_per_sec;

  /* Snap up to the next 16-sample boundary, always one quantum clear of calc.
   */
  frame_samples = ((calc / 16) + 1) * 16;
  frame_samples_min = frame_samples - 16;
  extra_samples = frame_samples * extra_rate / 100;

  return (frame_samples + 16 + extra_samples);
}
#else
/*
 * SUPPORT_NAUDIO build: the same budget quantized to N_SAMPLES, and an explicit
 * upper bound is tracked since this synth corrects in whole-quantum steps.
 */
u32 __MusIntSamplesInit(u32 retrace_count, u32 output_rate, u32 vsyncs_per_sec,
                        u32 extra_rate) {
  u32 calc;

  calc = ((retrace_count * output_rate) + vsyncs_per_sec - 1) / vsyncs_per_sec;

  frame_samples = ((calc / N_SAMPLES) + 1) * N_SAMPLES;
  frame_samples_min = frame_samples - N_SAMPLES;
  frame_samples_max = frame_samples + N_SAMPLES;
  extra_samples = frame_samples * extra_rate / 100;

  return (frame_samples + N_SAMPLES + extra_samples);
}
#endif

/*
 * Decide how many samples to synthesize for the current frame, given how many
 * are still queued in the AI output (samples). Tracks the nominal budget while
 * counter-steering toward the target fill, so the output queue stays between
 * underrun and overflow.
 */
#ifndef SUPPORT_NAUDIO
u32 __MusIntSamplesCurrent(u32 samples) {
  /*
   * Refill the gap up to the target (budget + headroom + one quantum), snapped
   * down to a 16-sample boundary: produce more when the queue is drained, less
   * when it is full.
   */
  samples = (frame_samples + extra_samples + 16 - samples) & (~15);
  if (samples < frame_samples_min) return (frame_samples_min);
  return (samples);
}
#else
/*
 * SUPPORT_NAUDIO build: rather than a continuous gap fill, emit a single
 * corrective frame whenever the queue drifts outside its band, then hold
 * nominal. only_one_flag arms the one-shot and re-arms only once the queue
 * returns to the band, so the correction cannot oscillate.
 */
u32 __MusIntSamplesCurrent(u32 samples) {
  static u32 only_one_flag = 1;

  if (samples > N_SAMPLES + extra_samples) {
    /* Queue overfull: undershoot once to let it drain. */
    if (only_one_flag) {
      only_one_flag = 0;
      return (frame_samples_min);
    }
  } else if (samples < extra_samples) {
    /* Queue running dry: overshoot once to refill it. */
    if (only_one_flag) {
      only_one_flag = 0;
      return (frame_samples_max);
    }
  } else {
    /* Back inside the band: re-arm the one-shot. */
    only_one_flag = 1;
  }

  return (frame_samples);
}
#endif
