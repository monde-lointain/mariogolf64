/*
 * n_auxbus.c
 *
 * naudio auxiliary (wet/effects) bus filter, the naudio counterpart of the base
 * library's auxbus. It clears the AUX stereo buffers and pulls every physical
 * voice attached to it. In naudio every voice is an aux-bus source, so this is
 * where the whole voice mix is driven: each voice sends its dry signal to MAIN
 * and its wet signal to the AUX buffers cleared here, which the effects unit
 * and main bus then read. naudio runs a fixed-length frame, so the buffer sizes
 * are compile-time constants rather than a runtime sample count.
 */
#include "n_synthInternals.h"

/*
 * Command-list handler for the aux bus. Clears the AUX L/R output buffers, then
 * pulls each attached voice through its envmixer so every voice is mixed in
 * (its dry signal into MAIN, its wet signal into AUX). The sources are always
 * envmixers, so the pull calls n_alEnvmixerPull directly instead of dispatching
 * through a per-source handler pointer the way the base library does.
 * FIXED_SAMPLE is the per-frame sample count; "<< 1" turns it into a byte count
 * for the s16 buffers. Returns the advanced command pointer.
 */
Acmd* n_alAuxBusPull(s32 sampleOffset, Acmd* p) {
  Acmd* ptr = p;
  N_ALAuxBus* m = (N_ALAuxBus*)n_syn->auxBus;
  N_PVoice** sources = m->sources;
  s32 i;

  /* Clear the AUX buffers (the compact N_MICRO build clears both contiguous
   * AUX blocks in one double-width command). */
#ifndef N_MICRO
  aClearBuffer(ptr++, AL_AUX_L_OUT, FIXED_SAMPLE << 1);
  aClearBuffer(ptr++, AL_AUX_R_OUT, FIXED_SAMPLE << 1);
#else
  aClearBuffer(ptr++, N_AL_AUX_L_OUT, N_AL_DIVIDED << 1);
#endif

  /* Pull every attached source voice into the mix. */
  for (i = 0; i < m->sourceCount; i++) {
    ptr = n_alEnvmixerPull(sources[i], sampleOffset, ptr);
  }
  return ptr;
}
