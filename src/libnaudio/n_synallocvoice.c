/*
 * n_synallocvoice.c
 *
 * naudio synthesizer driver: allocate a physical voice for a logical voice.
 * Binds a logical N_ALVoice to a physical voice, drawing from the free/lame
 * pools when possible and otherwise stealing the lowest-priority active voice.
 * Stealing fades the displaced voice out and schedules its stop so the swap is
 * click-free.
 */
#include "n_synthInternals.h"

static s32 _allocatePVoice(N_PVoice** pvoice, s16 priority);

/*
 * Allocate and configure a physical voice for logical voice `voice` from the
 * settings in `vc`. On success the two are cross-linked and the function
 * returns nonzero; if no voice could be obtained it returns 0. Stealing an
 * active voice queues two update params (ramp-to-silence, then stop), so a free
 * param slot for each is required; debug builds check this up front.
 */
s32 n_alSynAllocVoice(N_ALVoice* voice, ALVoiceConfig* vc) {
  N_PVoice* pvoice = 0;  // physical voice we end up with (0 if none available)
  ALFilter* f;      // unused; vestige of the stock synth's per-voice filter
  ALParam* update;  // scratch param node for the ramp-down / stop events
  s32 stolen;       // nonzero when an active voice had to be stolen

  // Stealing needs two free update params; bail early if they are unavailable.
#ifdef _DEBUG
  if (n_syn->paramList == 0) {
    __osError(ERR_ALSYN_NO_UPDATE, 0);
    return 0;
  } else if (n_syn->paramList->next == 0) {
    __osError(ERR_ALSYN_NO_UPDATE, 0);
    return 0;
  }
#endif

  // Seed the logical voice from the caller's config; it starts out stopped.
  voice->priority = vc->priority;
  voice->unityPitch = vc->unityPitch;
  voice->table = 0;
  voice->fxBus = vc->fxBus;
  voice->state = AL_STOPPED;
  voice->pvoice = 0;

  stolen = _allocatePVoice(&pvoice, vc->priority);

  if (pvoice) {
    if (stolen) {
      // Hold the stolen voice 552 more samples before its hard stop, and
      // detach it from its old logical voice. The fade queued below runs for
      // 552 - 184 samples, ending one 184-sample frame before that stop.
      pvoice->offset = 552;
      pvoice->vvoice->pvoice = 0;
      pvoice->vvoice = voice;
      voice->pvoice = pvoice;

      // Ramp the stolen voice to silence (volume 0) over 368 samples (the
      // 552-sample offset less one 184-sample frame) so it fades out instead
      // of cutting off abruptly.
      update = __n_allocParam();
#ifdef SAMPLE_ROUND
      update->delta = SAMPLE184(n_syn->paramSamples);
#else
      update->delta = n_syn->paramSamples;
#endif
      update->type = AL_FILTER_SET_VOLUME;
      update->data.i = 0;
      update->moredata.i = 368;
      n_alEnvmixerParam(voice->pvoice, AL_FILTER_ADD_UPDATE, update);

      // Then hard-stop it at paramSamples + offset (552), one frame after the
      // fade above completes.
      update = __n_allocParam();
      if (update) {
#ifdef SAMPLE_ROUND
        update->delta = SAMPLE184(n_syn->paramSamples + pvoice->offset);
#else
        update->delta = n_syn->paramSamples + pvoice->offset;
#endif
        update->type = AL_FILTER_STOP_VOICE;
        update->next = 0;
        n_alEnvmixerParam(voice->pvoice, AL_FILTER_ADD_UPDATE, update);
      } else {
#ifdef _DEBUG
        __osError(ERR_ALSYN_NO_UPDATE, 0);
#endif
      }
    } else {
      // Fresh voice: no fade needed, just cross-link it.
      pvoice->offset = 0;
      pvoice->vvoice = voice;
      voice->pvoice = pvoice;
    }
  }

  return (pvoice != 0);
}

/*
 * Find a physical voice for the given priority and return it through `pvoice`.
 * Prefers the lame list (voices finished but not yet collected), then the free
 * list; failing both it steals the lowest-priority active voice whose priority
 * is at or below `priority` and that is not already being stolen (offset == 0).
 * Returns nonzero only when a voice was stolen.
 */
s32 _allocatePVoice(N_PVoice** pvoice, s16 priority) {
  ALLink* dl;
  N_PVoice* pv;
  s32 stolen = 0;

  if ((dl = n_syn->pLameList.next) != 0) {
    // Reuse a spent voice still sitting on the lame list.
    *pvoice = (N_PVoice*)dl;
    alUnlink(dl);
    alLink(dl, &n_syn->pAllocList);
  } else if ((dl = n_syn->pFreeList.next) != 0) {
    // Otherwise take an unused voice from the free list.
    *pvoice = (N_PVoice*)dl;
    alUnlink(dl);
    alLink(dl, &n_syn->pAllocList);
  } else {
    // None spare: scan the allocated voices for the lowest-priority steal
    // candidate at or below the requested priority that is not already
    // mid-steal. Re-narrowing `priority` each hit keeps the lowest one.
    for (dl = n_syn->pAllocList.next; dl != 0; dl = dl->next) {
      pv = (N_PVoice*)dl;
      if ((pv->vvoice->priority <= priority) && (pv->offset == 0)) {
        *pvoice = pv;
        priority = pv->vvoice->priority;
        stolen = 1;
      }
    }
  }

  return stolen;
}
