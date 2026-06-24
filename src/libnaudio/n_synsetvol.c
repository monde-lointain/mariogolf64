/*
 * Synth voice volume control.
 *
 * Sets the volume of an active naudio synthesizer voice over a transition time
 * by queuing a parameter update onto the voice's envelope/mixer filter, to take
 * effect at the voice's current sample position.
 */
#include <os_internal.h>
#include <ultraerror.h>
#include "n_synthInternals.h"

/* Set the volume of voice v, ramping over transition time t. A no-op unless the
 * voice is physically allocated. */
void n_alSynSetVol(N_ALVoice* v, s16 volume, ALMicroTime t) {
  ALParam* update;
  ALFilter* f;

  if (v->pvoice) {
    /* Take an update command off the free list; bail if it is exhausted. */
    update = __n_allocParam();
    ALFailIf(update == 0, ERR_ALSYN_NO_UPDATE);

    /* Schedule the update for the voice's current sample position. */
#ifdef SAMPLE_ROUND
    update->delta = SAMPLE184(n_syn->paramSamples + v->pvoice->offset);
#else
    update->delta = n_syn->paramSamples + v->pvoice->offset;
#endif

    /* Carry the new volume as a single, unchained update. */
    update->type = AL_FILTER_SET_VOLUME;
    update->data.i = volume;

    /* Carry the ramp transition time alongside it, converted from microseconds
     * to samples. */
#ifdef SAMPLE_ROUND
    update->moredata.i = SAMPLE184(_n_timeToSamples(t));
#else
    update->moredata.i = _n_timeToSamples(t);
#endif

    /* Hand the update to the voice's filter, which applies it at the scheduled
     * time. */
    update->next = 0;
    n_alEnvmixerParam(v->pvoice, AL_FILTER_ADD_UPDATE, update);
  }
}
