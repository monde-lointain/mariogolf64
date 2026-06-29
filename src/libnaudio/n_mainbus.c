/*
 * n_mainbus.c
 *
 * naudio main (dry) output bus filter, the naudio counterpart of the base
 * library's mainbus. It clears the MAIN stereo buffers, runs the bus handler
 * (the aux-bus pull, or the reverb-FX pull that wraps it), then folds the AUX
 * (wet/effects) buffers into MAIN to form the final mix saved out to the DAC.
 * Unlike the base mainbus, which loops over its own sources and folds AUX after
 * each one, the naudio main bus owns no sources: it runs one handler that
 * drives every voice, then folds AUX in once. naudio runs a fixed-length frame,
 * so the buffer sizes are compile-time constants rather than a runtime count.
 */
#include "n_synthInternals.h"

/*
 * Command-list handler for the main bus. Clears the MAIN L/R buffers, then runs
 * mainBus->filter.handler (the aux-bus pull, or the reverb-FX pull that wraps
 * it), which sums every voice's dry signal into MAIN and its wet signal into
 * AUX. Finally it folds the AUX L/R buffers into MAIN at near-unity gain
 * (0x7fff = max s16), adding the wet mix to the dry. FIXED_SAMPLE is the
 * per-frame sample count; "<< 1" makes it an s16 byte count. Returns the
 * advanced command pointer.
 */
Acmd* n_alMainBusPull(s32 sampleOffset, Acmd* p) {
  Acmd* ptr = p;

  /* Clear the MAIN buffers (the compact N_MICRO build clears both contiguous
   * MAIN blocks in one double-width command). */
#ifndef N_MICRO
  aClearBuffer(ptr++, AL_MAIN_L_OUT, FIXED_SAMPLE << 1);
  aClearBuffer(ptr++, AL_MAIN_R_OUT, FIXED_SAMPLE << 1);
#else
  aClearBuffer(ptr++, N_AL_MAIN_L_OUT, N_AL_DIVIDED << 1);
#endif

  /* Run the aux/FX sub-chain: dry voices land in MAIN, wet sends in AUX. */
  ptr = (n_syn->mainBus->filter.handler)(sampleOffset, ptr);

  /* Fold the AUX (wet) buffers into MAIN at near-unity gain to finish the mix.
   * The full ABI sets the mix byte count with a separate aSetBuffer; the
   * compact N_MICRO build issues self-contained mix commands, so that setup is
   * dropped there. */
#ifndef N_MICRO
  aSetBuffer(ptr++, 0, 0, 0, FIXED_SAMPLE << 1);
  aMix(ptr++, 0, 0x7fff, AL_AUX_L_OUT, AL_MAIN_L_OUT);
  aMix(ptr++, 0, 0x7fff, AL_AUX_R_OUT, AL_MAIN_R_OUT);
#else
  aMix(ptr++, 0, 0x7fff, N_AL_AUX_L_OUT, N_AL_MAIN_L_OUT);
  aMix(ptr++, 0, 0x7fff, N_AL_AUX_R_OUT, N_AL_MAIN_R_OUT);
#endif
  return ptr;
}
