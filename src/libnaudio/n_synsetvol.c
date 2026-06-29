/*
 * n_synsetvol.c
 *
 * naudio synthesizer driver: set a voice's target volume with a ramp time.
 * Part of the n_alSyn* per-voice parameter API. Builds a timestamped update
 * record and enqueues it on the voice's envmixer, which slews the level toward
 * the target over the requested interval starting at the next synthesis frame.
 */
#include "n_synthInternals.h"

/*
 * Set the target volume for voice v, ramped over t microseconds. The envmixer
 * interpolates from the current level to volume across that interval. No-op
 * when v has no physical voice. Side effect: enqueues a parameter update
 * timestamped to the next frame.
 */
void n_alSynSetVol(N_ALVoice* v, s16 volume, ALMicroTime t) {
  ALParam* update;
  ALFilter* f;  // unused

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

    update->type = AL_FILTER_SET_VOLUME;
    update->data.i = volume;

    // moredata carries the ramp length: convert t (microseconds) to samples
    // so the envmixer knows how long to slew toward the target.
#ifdef SAMPLE_ROUND
    update->moredata.i = SAMPLE184(_n_timeToSamples(t));
#else
    update->moredata.i = _n_timeToSamples(t);
#endif
    update->next = 0;

    // Queue the one-element update list onto the voice's envmixer.
    n_alEnvmixerParam(v->pvoice, AL_FILTER_ADD_UPDATE, update);
  }
}
