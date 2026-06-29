/*
 * n_synstartvoice.c
 *
 * naudio synthesizer driver: begin playback of a wavetable on a voice. Part of
 * the n_alSyn* per-voice parameter API. Builds a timestamped start-voice update
 * record and enqueues it on the voice's envmixer so playback begins at the next
 * synthesis frame.
 */
#include "n_synthInternals.h"

/*
 * Start playing wavetable table on voice v at the voice's unity pitch. No-op
 * when v has no physical voice. Side effect: enqueues a start-voice update
 * timestamped to the next frame.
 */
void n_alSynStartVoice(N_ALVoice* v, ALWaveTable* table) {
  ALStartParam* update;

  // Parameter changes apply only to a voice bound to a physical voice.
  if (v->pvoice) {
    // Start-voice carries extra fields, so view the freed record as an
    // ALStartParam; fail if the param pool is empty.
    update = (ALStartParam*)__n_allocParam();
    ALFailIf(update == 0, ERR_ALSYN_NO_UPDATE);

    // Stamp the update with the sample time the envmixer should apply it:
    // the start of this voice's next frame. (SAMPLE_ROUND, when enabled,
    // snaps the delta to a whole number of 184-sample frames.)
#ifdef SAMPLE_ROUND
    update->delta = SAMPLE184(n_syn->paramSamples + v->pvoice->offset);
#else
    update->delta = n_syn->paramSamples + v->pvoice->offset;
#endif

    // Wavetable to play, plus the voice's unity (base) pitch for resampling.
    update->type = AL_FILTER_START_VOICE;
    update->wave = table;
    update->next = 0;
    update->unity = v->unityPitch;

    // Queue the one-element update list onto the voice's envmixer.
    n_alEnvmixerParam(v->pvoice, AL_FILTER_ADD_UPDATE, update);
  }
}
