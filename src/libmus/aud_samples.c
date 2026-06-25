
/*************************************************************

  aud_samples.c : Nintendo 64 Music Tools Programmers Library
  (c) Copyright 1997/1998, Software Creations (Holdings) Ltd.

  Version 3.11

  Music library frame sample calculations.

**************************************************************/

/* include configuartion */
#include "libmus_config.h"

/* include system headers */
#include <ultra64.h>

/* include current header file */
#include "aud_samples.h"

/* internal macros */
#define N_SAMPLES 184

/* internal workspace (drop-static-mirror: placed at curated main_bss vrams) */
extern u32 g_mus_frame_samples;
extern u32 g_mus_frame_samples_min;
extern u32 g_mus_frame_samples_max;
extern u32 g_mus_extra_samples;

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
  [EXTERNAL FUNCTION]
  __MusIntSamplesInit(retrace_count, output_rate, vsyncs_per_sec, extra_rate)

  [Parameters]
  retrace_count		number of vsyncs per vsync message
  output_rate		audio libary output rate value
  vsyncs_per_sec	number of vsyncs per second
  extra_rate		ratio of extra samples to download

  [Explanation]
  Calculate constants for the number of samples required per frame.

  [Return value]
  Maximum number of samples downloaded per frame
*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef SUPPORT_NAUDIO
u32 __MusIntSamplesInit(u32 retrace_count, u32 output_rate, u32 vsyncs_per_sec,
                        u32 extra_rate) {
  u32 calc;

  calc = (retrace_count * output_rate + vsyncs_per_sec - 1) / vsyncs_per_sec;
  frame_samples = ((calc / 16) + 1) * 16;
  frame_samples_min = frame_samples - 16;
  extra_samples = frame_samples * extra_rate / 100;
  return (frame_samples + 16 + extra_samples);
}
#else  /* SUPPORT_NAUDIO */
u32 __MusIntSamplesInit(u32 retrace_count, u32 output_rate, u32 vsyncs_per_sec,
                        u32 extra_rate) {
  u32 calc;

  calc = (retrace_count * output_rate + vsyncs_per_sec - 1) / vsyncs_per_sec;
  g_mus_frame_samples = ((calc / N_SAMPLES) + 1) * N_SAMPLES;
  g_mus_frame_samples_min = g_mus_frame_samples - N_SAMPLES;
  g_mus_frame_samples_max = g_mus_frame_samples + N_SAMPLES;
  g_mus_extra_samples = g_mus_frame_samples * extra_rate / 100;
  return (g_mus_frame_samples + N_SAMPLES + g_mus_extra_samples);
}
#endif /* SUPPORT_NAUDIO */

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
  [EXTERNAL FUNCTION]
  __MusIntSamplesCurrent()

  [Parameters]
  samples		number of samples left.

  [Explanation]
  Calculate number of samples required per frame.

  [Return value]
  Number of samples currently required
*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef SUPPORT_NAUDIO
u32 __MusIntSamplesCurrent(u32 samples) {
  samples = (frame_samples + extra_samples + 16 - samples) & (~15);
  if (samples < frame_samples_min) return (frame_samples_min);
  return (samples);
}
#else  /* SUPPORT_NAUDIO */
u32 __MusIntSamplesCurrent(u32 samples) {
  static u32 only_one_flag = 1;

  if (samples > N_SAMPLES + g_mus_extra_samples) {
    if (only_one_flag) {
      only_one_flag = 0;
      return (g_mus_frame_samples_min);
    }
  } else if (samples < g_mus_extra_samples) {
    if (only_one_flag) {
      only_one_flag = 0;
      return (g_mus_frame_samples_max);
    }
  } else {
    only_one_flag = 1;
  }
  return (g_mus_frame_samples);
}
#endif /* SUPPORT_NAUDIO */

/* end of file */
