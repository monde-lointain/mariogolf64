/*
 * n_synsetfxmix.c
 *
 * naudio synthesizer driver: set a voice's effects (wet) send level. Part of
 * the n_alSyn* per-voice parameter API. Builds a timestamped update record and
 * enqueues it on the voice's envmixer so the new level takes effect at the next
 * synthesis frame.
 */
#include "n_synthInternals.h"

/*
 * Set the effects-send (wet) level for voice v to fxmix, clamped to 0..127.
 * No-op when v has no physical voice. Side effect: enqueues a parameter update
 * timestamped to the next frame.
 */
void n_alSynSetFXMix(N_ALVoice* v, u8 fxmix) {
  ALParam* update;

  // Parameter changes apply only to a voice bound to a physical voice.
  if (v->pvoice) {
    // Take a record from the parameter free pool; fail if it is empty.
    update = __n_allocParam();
    ALFailIf(update == 0, ERR_ALSYN_NO_UPDATE);

    // Stamp the update with the sample time the envmixer should apply it:
    // the start of this voice's next frame. (SAMPLE_ROUND, when enabled,
    // snaps the delta to a whole number of 184-sample frames.)
#ifdef SAMPLE_ROUND
    update->delta = SAMPLE184(n_syn->paramSamples + v->pvoice->offset);
#else
    update->delta = n_syn->paramSamples + v->pvoice->offset;
#endif

    // Effects (wet) send is a 7-bit level; clamp to its 0..127 range.
    update->type = AL_FILTER_SET_FXAMT;
    if (fxmix > 127) {
      fxmix = 127;
    }
    update->data.i = fxmix;
    update->next = 0;

    // Queue the one-element update list onto the voice's envmixer.
    n_alEnvmixerParam(v->pvoice, AL_FILTER_ADD_UPDATE, update);
  }
}
