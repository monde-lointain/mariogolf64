/*
 * n_synstopvoice.c
 *
 * naudio synthesizer driver: stop playback on a voice. Part of the n_alSyn*
 * per-voice parameter API. Builds a timestamped stop-voice update record and
 * enqueues it on the voice's envmixer so playback ends at the next synthesis
 * frame.
 */
#include <os_internal.h>
#include <ultraerror.h>
#include "n_synthInternals.h"

/*
 * Stop playback on voice v. No-op when v has no physical voice. Side effect:
 * enqueues a stop-voice update timestamped to the next frame.
 */
void n_alSynStopVoice(N_ALVoice* v) {
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

    update->type = AL_FILTER_STOP_VOICE;
    update->next = 0;

    // Queue the one-element update list onto the voice's envmixer.
    n_alEnvmixerParam(v->pvoice, AL_FILTER_ADD_UPDATE, update);
  }
}
