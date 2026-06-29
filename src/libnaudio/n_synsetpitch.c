/*
 * n_synsetpitch.c
 *
 * naudio synthesizer driver: set a voice's playback pitch. Part of the
 * n_alSyn* per-voice parameter API. Builds a timestamped update record and
 * enqueues it on the voice's envmixer so the new pitch takes effect at the next
 * synthesis frame.
 */
#include <os_internal.h>
#include <ultraerror.h>
#include "n_synthInternals.h"

/*
 * Set the playback pitch for voice v. pitch is a ratio applied on top of the
 * wave's unity rate (1.0 = play at the recorded rate, 2.0 = an octave up).
 * No-op when v has no physical voice. Side effect: enqueues a parameter update
 * timestamped to the next frame.
 */
void n_alSynSetPitch(N_ALVoice* v, f32 pitch) {
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

    // Pitch is a float ratio, so use the float arm of the value union.
    update->type = AL_FILTER_SET_PITCH;
    update->data.f = pitch;
    update->next = 0;

    // Queue the one-element update list onto the voice's envmixer.
    n_alEnvmixerParam(v->pvoice, AL_FILTER_ADD_UPDATE, update);
  }
}
