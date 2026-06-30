/*
 * aud_samples.h
 *
 * libmus internal interface for per-frame audio sample-count bookkeeping. The
 * synth produces one block of samples per video frame to feed the audio (AI)
 * DMA output. These routines size that buffer from the video/output timing, and
 * each frame decide how many samples to generate so the AI backlog stays near a
 * steady target. Both share a set of file-scope range globals (frame_samples
 * and its min/max/extra bounds). Implemented in aud_samples.c.
 */
#ifndef _LIBMUS_AUD_SAMPLES_H_
#define _LIBMUS_AUD_SAMPLES_H_

/*
 * Establish the per-frame sample budget and return the maximum samples one
 * frame buffer must hold (frame size plus a block of headroom plus the extra
 * margin). retrace_count is the number of video retraces per synthesis frame,
 * output_rate the achieved DAC rate in Hz, vsyncs_per_sec the video refresh
 * rate in Hz, and extra_rate a headroom margin as a percent of the frame size.
 * Called once at startup; seeds the frame_samples range globals that
 * __MusIntSamplesCurrent reads.
 */
u32 __MusIntSamplesInit(u32 retrace_count, u32 output_rate, u32 vsyncs_per_sec,
                        u32 extra_rate);

/*
 * Given the samples still pending in the AI output buffer (its current
 * backlog), return how many new samples to synthesize this frame to refill
 * toward the target level, block-aligned and clamped to frame_samples_min.
 */
u32 __MusIntSamplesCurrent(u32 samples);

#endif
