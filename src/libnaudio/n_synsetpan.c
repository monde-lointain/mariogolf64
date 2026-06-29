/*
 * n_synsetpan.c
 *
 * naudio synthesizer driver: set a voice's stereo pan position. Part of the
 * n_alSyn* per-voice parameter API. Builds a timestamped update record and
 * enqueues it on the voice's envmixer so the new pan takes effect at the next
 * synthesis frame.
 */
#include <os_internal.h>
#include <ultraerror.h>
#include "n_synthInternals.h"

/*
 * Set the stereo pan for voice v (0 = full left, 64 = center, 127 = full
 * right). No-op when v has no physical voice. Side effect: enqueues a parameter
 * update timestamped to the next frame.
 */
void n_alSynSetPan(N_ALVoice* v, u8 pan) {
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

    update->type = AL_FILTER_SET_PAN;
    update->data.i = pan;
    update->next = 0;

    // Queue the one-element update list onto the voice's envmixer.
    n_alEnvmixerParam(v->pvoice, AL_FILTER_ADD_UPDATE, update);
  }
}
