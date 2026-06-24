/*
 * Synth voice start.
 *
 * Starts an active naudio synthesizer voice playing a wavetable by queuing a
 * start update onto the voice's envelope/mixer filter, to take effect at the
 * voice's current sample position.
 */
#include <os_internal.h>
#include <ultraerror.h>
#include "n_synthInternals.h"

/* Start voice v playing wavetable table. A no-op unless the voice is physically
 * allocated. */
void n_alSynStartVoice(N_ALVoice* v, ALWaveTable* table) {
  ALStartParam* update;

  if (v->pvoice) {
    /* Take an update command off the free list; bail if it is exhausted. */
    update = (ALStartParam*)__n_allocParam();
    ALFailIf(update == 0, ERR_ALSYN_NO_UPDATE);

    /* Schedule the update for the voice's current sample position. */
#ifdef SAMPLE_ROUND
    update->delta = SAMPLE184(n_syn->paramSamples + v->pvoice->offset);
#else
    update->delta = n_syn->paramSamples + v->pvoice->offset;
#endif

    /* Carry the wavetable and its natural (unity) pitch as a single, unchained
     * update, then hand it to the voice's filter, which applies it at the
     * scheduled time. */
    update->type = AL_FILTER_START_VOICE;
    update->wave = table;
    update->next = 0;
    update->unity = v->unityPitch;
    n_alEnvmixerParam(v->pvoice, AL_FILTER_ADD_UPDATE, update);
  }
}
