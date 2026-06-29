/*
 * n_save.c
 *
 * n_audio (microcode-variant) save filter: the tail of the synthesis chain.
 * It drives the main bus to build the final mix, interleaves the separate left
 * and right mix buffers into one stereo buffer, and DMAs the result out to the
 * DRAM output buffer (n_syn->sv_dramout) where the audio interface plays it.
 */

#include "n_synthInternals.h"
#include <os.h>

/*
 * Builds the command list that produces one frame of finished stereo audio:
 * pulls the main-bus mix, interleaves the left/right mix buffers, and saves
 * (DMAs) the stereo result out to DRAM. sampleOffset, the frame's start time in
 * samples, is forwarded to the main-bus pull. Returns the advanced command
 * pointer.
 */
Acmd* n_alSavePull(s32 sampleOffset, Acmd* p) {
  Acmd* ptr = p;

  // Let the main bus build the left/right mix in DMEM.
  ptr = n_alMainBusPull(sampleOffset, ptr);
  // Interleave the separate left and right mix buffers into one stereo buffer
  // (<<1 = the per-channel byte count, <<2 = the interleaved-stereo byte
  // count), then DMA it out to the DRAM output buffer.
#ifndef N_MICRO
  aSetBuffer(ptr++, 0, 0, 0, FIXED_SAMPLE << 1);
  aInterleave(ptr++, AL_MAIN_L_OUT, AL_MAIN_R_OUT);
  aSetBuffer(ptr++, 0, 0, 0, FIXED_SAMPLE << 2);
  aSaveBuffer(ptr++, n_syn->sv_dramout);
#else
#include "inc/n_save_add01.inc"
#endif
  return ptr;
}
