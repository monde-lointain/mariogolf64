/*
 * n_synthesizer.c
 *
 * Core of the naudio software synthesizer: it turns the active player's timed
 * parameter updates into one frame's worth of RSP audio command list.
 * n_alSynNew builds the synthesizer state (physical-voice pool, aux/main buses,
 * parameter pool) out of the audio heap, and n_alAudioFrame is called once per
 * video frame to advance the sample clock and emit the commands that render a
 * block of output samples. The rest are small helpers managing the parameter
 * and physical-voice free lists and converting between microseconds and
 * samples. This is the naudio (n_aspMain microcode) counterpart of the stock
 * audio synthesizer.
 */
#include "n_synthInternals.h"
#ifdef _DEBUG
#include <assert.h>
#endif

/* Single-player build: selects the simpler one-client path in n_alAudioFrame
   and compiles out the multi-player __n_nextSampleTime scan. */
#define ONLY_ONE_PLAYER

/* Optional per-frame CPU-cycle profiling (osGetCount samples into counters). */
#ifdef AUD_PROFILE
#include <os.h>
extern u32 cnt_index, drvr_num, drvr_cnt, drvr_max, drvr_min, lastCnt[];
extern u32 client_num, client_cnt, client_max, client_min;
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static s32 __n_nextSampleTime(ALPlayer** client);
static s32 _n_timeToSamplesNoRound(s32 micros);

/*
 * Build the synthesizer state n_syn for the given configuration. Allocates the
 * aux/main buses, the physical-voice pool, and the parameter pool from the
 * audio heap (c->heap), and routes the main bus through the effects path when
 * an effect is configured. Runs once at audio init; emits no audio.
 */
void n_alSynNew(ALSynConfig* c) {
  /* vv, vvoices, save, sources, and m_sources are vestigial: the declaration
     block mirrors the stock alSynNew, but this variant allocates the buses and
     save/voice state inline and never uses them. */
  s32 i;
  N_ALVoice* vv;
  N_PVoice* pv;
  N_ALVoice* vvoices;
  N_PVoice* pvoices;
  ALHeap* hp = c->heap;
  ALSave* save;
  ALFilter* sources;
  N_PVoice* m_sources;
  ALParam* params;
  ALParam* paramPtr;

  /* Reset the running sample clocks and copy the static configuration. */
  n_syn->head = NULL;
  n_syn->numPVoices = c->maxPVoices;
  n_syn->curSamples = 0;
  n_syn->paramSamples = 0;
  n_syn->outputRate = c->outputRate;

  /* Max samples the RSP renders per pass: 160 normally, the fixed 184-sample
     block under N_MICRO. */
#ifndef N_MICRO
  n_syn->maxOutSamples = AL_MAX_RSP_SAMPLES;
#else
  n_syn->maxOutSamples = FIXED_SAMPLE;
#endif

  /* DMA constructor for sample loads; sv_* track the save/output buffer state.
   */
  n_syn->dma = (ALDMANew)c->dmaproc;
  n_syn->sv_dramout = 0;
  n_syn->sv_first = 1;

  /* Auxiliary effects bus, with one source slot per physical voice. */
  n_syn->auxBus = (N_ALAuxBus*)alHeapAlloc(hp, 1, sizeof(N_ALAuxBus));
  n_syn->auxBus->sourceCount = 0;
  n_syn->auxBus->maxSources = c->maxPVoices;
  n_syn->auxBus->sources =
      (N_PVoice**)alHeapAlloc(hp, c->maxPVoices, sizeof(N_PVoice*));

  /* Main bus: route through the FX (reverb) pull when an effect is configured,
     otherwise straight to the aux-bus pull. */
  n_syn->mainBus = (N_ALMainBus*)alHeapAlloc(hp, 1, sizeof(N_ALMainBus));
  if (c->fxType != AL_FX_NONE) {
    n_syn->auxBus->fx = n_alSynAllocFX(0, c, hp);
    n_syn->mainBus->filter.handler = (N_ALCmdHandler)n_alFxPull;
  } else {
    n_syn->mainBus->filter.handler = (N_ALCmdHandler)n_alAuxBusPull;
  }

  /* Physical-voice lists start empty: free = available, lame = finished and
     awaiting collection, alloc = currently in use. */
  n_syn->pFreeList.next = 0;
  n_syn->pFreeList.prev = 0;
  n_syn->pLameList.next = 0;
  n_syn->pLameList.prev = 0;
  n_syn->pAllocList.next = 0;
  n_syn->pAllocList.prev = 0;

  /* Allocate the physical voices: link each onto the free list, build its
     decoder/resampler/envmixer state, and register it as an aux-bus source. */
  pvoices = alHeapAlloc(hp, c->maxPVoices, sizeof(N_PVoice));
  for (i = 0; i < c->maxPVoices; i++) {
    pv = &pvoices[i];
    alLink((ALLink*)pv, &n_syn->pFreeList);
    pv->vvoice = 0;
    alN_PVoiceNew(pv, n_syn->dma, hp);
    n_syn->auxBus->sources[n_syn->auxBus->sourceCount++] = pv;
  }

  /* Allocate the parameter (update event) pool and thread it onto the free
     param list. */
  params = alHeapAlloc(hp, c->maxUpdates, sizeof(ALParam));
  n_syn->paramList = 0;
  for (i = 0; i < c->maxUpdates; i++) {
    paramPtr = &params[i];
    paramPtr->next = n_syn->paramList;
    n_syn->paramList = paramPtr;
  }

  n_syn->heap = hp;
}

/*
 * Build one frame's RSP command list and return its end. Called every video
 * frame, which serves as the audio clients' time base: first it advances the
 * parameter clock, running the player's pending update handlers, then it emits
 * the commands that render outLen stereo samples into outBuf. The number of
 * commands produced is written to *cmdLen. naudio's per-frame entry point (the
 * alAudioFrame counterpart).
 */
Acmd* n_alAudioFrame(Acmd* cmdList, s32* cmdLen, s16* outBuf, s32 outLen) {
  ALPlayer* client;
  // Starting DMEM buffer offset, unused here: unlike the stock alAudioFrame
  // (which passes &tmp down the filter chain), n_alSavePull tracks it itself.
  s16 tmp = 0;
  Acmd* cmdlEnd = cmdList;
  Acmd* cmdPtr;
  s32 nOut;
  s16* lOutBuf = outBuf;
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif

  /* No player attached: nothing to render this frame. */
  if (n_syn->head == 0) {
    *cmdLen = 0;
    return cmdList;
  }
#ifdef AUD_PROFILE
  lastCnt[++cnt_index] = osGetCount();
#endif

  /* Advance the parameter clock. paramSamples is the time of the next parameter
     change and curSamples the current sample time, so any change that falls
     within this frame (outLen) is applied now by calling its client handler.
     paramSamples is floored to a 16-sample boundary (the RSP works in 16-sample
     chunks) so a handler timestamps cleanly and is processed next frame. */
#ifndef ONLY_ONE_PLAYER
  for (n_syn->paramSamples = __n_nextSampleTime(&client);
       n_syn->paramSamples - n_syn->curSamples < outLen;
       n_syn->paramSamples = __n_nextSampleTime(&client)) {
    n_syn->paramSamples &= ~0xf;
    client->samplesLeft += _n_timeToSamplesNoRound((*client->handler)(client));
  }
#else
  client = n_syn->head;
  while (client->samplesLeft - n_syn->curSamples < outLen) {
    n_syn->paramSamples = (client->samplesLeft & ~0xf);
    client->samplesLeft += _n_timeToSamplesNoRound((*client->handler)(client));
  }
#endif
  n_syn->paramSamples &= ~0xf;
#ifdef AUD_PROFILE
  PROFILE_AUD(client_num, client_cnt, client_max, client_min);
#endif

  /* Emit the render in maxOutSamples-sized chunks. Each chunk sets the segment
     base and the save buffer (sv_dramout), then n_alSavePull builds its
     commands; lOutBuf advances two s16 per sample (interleaved stereo) and
     curSamples tracks total samples emitted. */
  while (outLen > 0) {
    nOut = MIN(n_syn->maxOutSamples, outLen);
    cmdPtr = cmdlEnd;
#ifndef N_MICRO
    aSegment(cmdPtr++, 0, 0);
#endif
    n_syn->sv_dramout = (s32)lOutBuf;
    cmdlEnd = n_alSavePull(n_syn->curSamples, cmdPtr);
    outLen -= nOut;
    lOutBuf += nOut << 1;
    n_syn->curSamples += nOut;
  }

  /* Report the command-list length and reclaim voices that finished this frame.
   */
  *cmdLen = (s32)(cmdlEnd - cmdList);
  _n_collectPVoices();
#ifdef AUD_PROFILE
  PROFILE_AUD(drvr_num, drvr_cnt, drvr_max, drvr_min);
#endif
  return cmdlEnd;
}

/* Pop a free parameter (update event) node off the pool, or return NULL when
   the pool is exhausted. */
ALParam* __n_allocParam(void) {
  ALParam* update = 0;
  if (n_syn->paramList) {
    update = n_syn->paramList;
    n_syn->paramList = n_syn->paramList->next;
    update->next = 0;
  }
  return update;
}

/* Return a parameter node to the pool (push onto the head of the free list). */
void _n_freeParam(ALParam* param) {
  param->next = n_syn->paramList;
  n_syn->paramList = param;
}

/* Move every physical voice on the lame list (retired last frame) back onto the
   free list, making it available for reuse. */
void _n_collectPVoices(void) {
  ALLink* dl;
  N_PVoice* pv;
  while ((dl = n_syn->pLameList.next) != 0) {
    pv = (N_PVoice*)dl;
    alUnlink(dl);
    alLink(dl, &n_syn->pFreeList);
  }
}

/* Retire a physical voice: unlink it and queue it on the lame list, where
   _n_collectPVoices reclaims it next frame (deferred so the RSP finishes with
   the voice's buffers first). */
void _n_freePVoice(N_PVoice* pvoice) {
  alUnlink((ALLink*)pvoice);
  alLink((ALLink*)pvoice, &n_syn->pLameList);
}

/* Convert a duration in microseconds to a sample count at the current output
   rate, with no frame-boundary alignment. The +0.5 offsets the average error
   of truncating the float-to-int cast. */
s32 _n_timeToSamplesNoRound(s32 micros) {
  f32 tmp = ((f32)micros) * n_syn->outputRate / 1000000.0 + 0.5;
  return (s32)tmp;
}

/* As _n_timeToSamplesNoRound, but floored to a whole 16-sample RSP chunk. */
s32 _n_timeToSamples(s32 micros) {
  return _n_timeToSamplesNoRound(micros) & ~0xf;
}

/* Multi-player variant (disabled by ONLY_ONE_PLAYER): scan every attached
   player and return the soonest one's next parameter time, reporting that
   player through *client. The 0x7fffffff seed is the largest s32 delta, acting
   as +infinity. */
#ifndef ONLY_ONE_PLAYER
static s32 __n_nextSampleTime(ALPlayer** client) {
  ALMicroTime delta = 0x7fffffff;
  ALPlayer* cl;
  u32 tt;
#ifdef _DEBUG
  assert(n_syn->head);
#endif
  *client = 0;
  for (cl = n_syn->head; cl != 0; cl = cl->next) {
    if ((cl->samplesLeft - n_syn->curSamples) < delta) {
      *client = cl;
      delta = cl->samplesLeft - n_syn->curSamples;
    }
  }
  return (*client)->samplesLeft;
}
#endif
