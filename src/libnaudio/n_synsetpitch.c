/*
 * Synth voice pitch control.
 *
 * Sets the playback rate of an active naudio synthesizer voice by queuing a
 * parameter update onto the voice's envelope/mixer filter, to take effect at
 * the voice's current sample position.
 */
#include <os_internal.h>
#include <ultraerror.h>
#include "n_synthInternals.h"

/* Set the playback pitch of voice v. A no-op unless the voice is physically
 * allocated. */
void n_alSynSetPitch(N_ALVoice* v, f32 pitch) {
  ALParam* update;

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

    /* Carry the new pitch as a single, unchained update, then hand it to the
     * voice's filter, which applies it at the scheduled time. */
    update->type = AL_FILTER_SET_PITCH;
    update->data.f = pitch;
    update->next = 0;
    n_alEnvmixerParam(v->pvoice, AL_FILTER_ADD_UPDATE, update);
  }
}
