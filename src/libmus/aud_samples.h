/*
 * aud_samples.h
 *
 * Interface to libmus's per-frame audio sample budgeting (see aud_samples.c):
 * sizing the output buffers at startup and choosing how many samples to
 * synthesize each frame to keep the DAC queue at a steady depth.
 */
#ifndef _LIBMUS_AUD_SAMPLES_H_
#define _LIBMUS_AUD_SAMPLES_H_

// Size the sample buffers at startup; returns the worst-case buffer size in
// samples. See aud_samples.c for the argument meanings.
u32 __MusIntSamplesInit(u32 retrace_count, u32 output_rate, u32 vsyncs_per_sec,
                        u32 extra_rate);

// Samples to synthesize this frame given the count still queued in the DAC.
u32 __MusIntSamplesCurrent(u32 samples);

#endif
