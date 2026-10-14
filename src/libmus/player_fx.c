/*
 * Bundled libmus custom-effect synthesizer. Preset tables remain in main data;
 * allocation and preset-switch paths reuse a single worst-case-sized effect.
 * The default initialization and effect-update routines still use assembly.
 */
#include "player_fx.h"

#include "include_asm.h"

extern s32 D_800C77E8[];
extern s32* D_800C7A8C[];
extern s32 D_800C7A88;
extern N_ALGlobals* n_alGlobals;
extern REVERB_MEM D_801B74B0;

void func_8009D004(ALSynConfig* config);
ALFxRef func_8009D274(s16 bus, ALSynConfig* config, ALHeap* heap);
void func_8009D2C4(ALFx** effect, ALSynConfig* config, ALHeap* heap);
void func_8009D40C(s32* params);
void func_8009D800(ALSynConfig* config, s32 index);
s32 func_8009D820(void);
s32 func_8009D864(void);

INCLUDE_ASM("asm/nonmatchings/libmus/player_fx", CustomInit);

void func_8009CF98(N_ALGlobals* globals, ALSynConfig* config, s32 index) {
  if (n_alGlobals == NULL) {
    n_alGlobals = globals;
    if (n_syn == NULL) {
      func_8009D800(config, index);
      n_syn = (N_ALSynth*)(D_801B74B0.synth = (ALSynth*)&n_alGlobals->drvr);
      func_8009D004(config);
    }
  }
}

void func_8009D004(ALSynConfig* c) {
  s32 i;
  N_PVoice* pv;
  N_PVoice* pvoices;
  ALHeap* hp = c->heap;
  ALParam* params;
  ALParam* paramPtr;

  n_syn->head = NULL;
  n_syn->numPVoices = c->maxPVoices;
  n_syn->curSamples = 0;
  n_syn->paramSamples = 0;
  n_syn->outputRate = c->outputRate;
  n_syn->maxOutSamples = FIXED_SAMPLE;
  n_syn->dma = (ALDMANew)c->dmaproc;
  n_syn->sv_dramout = 0;
  n_syn->sv_first = 1;

  n_syn->auxBus = alHeapAlloc(hp, 1, sizeof(N_ALAuxBus));
  n_syn->auxBus->sourceCount = 0;
  n_syn->auxBus->maxSources = c->maxPVoices;
  n_syn->auxBus->sources = alHeapAlloc(hp, c->maxPVoices, sizeof(N_PVoice*));
  n_syn->mainBus = alHeapAlloc(hp, 1, sizeof(N_ALMainBus));
  if (c->fxType != AL_FX_NONE) {
    n_syn->auxBus->fx = func_8009D274(0, c, hp);
    n_syn->mainBus->filter.handler = (N_ALCmdHandler)n_alFxPull;
  } else {
    n_syn->mainBus->filter.handler = (N_ALCmdHandler)n_alAuxBusPull;
  }
  n_syn->pFreeList.next = 0;
  n_syn->pFreeList.prev = 0;
  n_syn->pLameList.next = 0;
  n_syn->pLameList.prev = 0;
  n_syn->pAllocList.next = 0;
  n_syn->pAllocList.prev = 0;

  pvoices = alHeapAlloc(hp, c->maxPVoices, sizeof(N_PVoice));
  for (i = 0; i < c->maxPVoices; i++) {
    pv = &pvoices[i];
    alLink((ALLink*)pv, &n_syn->pFreeList);
    pv->vvoice = 0;
    alN_PVoiceNew(pv, n_syn->dma, hp);
    n_syn->auxBus->sources[n_syn->auxBus->sourceCount++] = pv;
  }
  params = alHeapAlloc(hp, c->maxUpdates, sizeof(ALParam));
  n_syn->paramList = 0;
  for (i = 0; i < c->maxUpdates; i++) {
    paramPtr = &params[i];
    paramPtr->next = n_syn->paramList;
    n_syn->paramList = paramPtr;
  }
  n_syn->heap = hp;
}

ALFxRef func_8009D274(s16 bus, ALSynConfig* config, ALHeap* heap) {
  func_8009D2C4(&n_syn->auxBus->fx_array[bus], config, heap);
  return n_syn->auxBus->fx_array[bus];
}

void func_8009D2C4(ALFx** effect, ALSynConfig* config, ALHeap* heap) {
  *effect = alHeapAlloc(heap, 1, sizeof(ALFx));
  (*effect)->paramHdl = alFxParamHdl;
  D_801B74B0.pDelay = alHeapAlloc(heap, func_8009D820(), sizeof(ALDelay));
  D_801B74B0.pBase = alHeapAlloc(heap, func_8009D864(), sizeof(s16));
  D_801B74B0.pResampler = alHeapAlloc(heap, 1, sizeof(ALResampler));
  D_801B74B0.pResampleState = alHeapAlloc(heap, 1, sizeof(RESAMPLE_STATE));
  D_801B74B0.pLowPass = alHeapAlloc(heap, 1, sizeof(ALLowPass));
  D_801B74B0.pLpfState = alHeapAlloc(heap, 1, sizeof(POLEF_STATE));
  func_8009D40C(config->params);
}

INCLUDE_ASM("asm/nonmatchings/libmus/player_fx", func_8009D40C);

INCLUDE_ASM("asm/nonmatchings/libmus/player_fx", func_8009D6EC);

INCLUDE_ASM("asm/nonmatchings/libmus/player_fx", func_8009D774);

/* Select the default custom-effect parameter block. */
void func_8009D7E8(ALSynConfig* config) {
  config->fxType = AL_FX_CUSTOM;
  config->params = D_800C77E8;
}

/* Select a preset; the caller supplies a valid table index. */
void func_8009D800(ALSynConfig* config, s32 index) {
  config->fxType = AL_FX_CUSTOM;
  config->params = D_800C7A8C[index];
}

/* Size the reusable effect's delay-descriptor allocation for every preset. */
s32 func_8009D820(void) {
  s32 index;
  s32 max_sections = 0;

  for (index = 0; D_800C7A8C[index] != NULL; index++) {
    if (D_800C7A8C[index][0] > max_sections) {
      max_sections = D_800C7A8C[index][0];
    }
  }
  return max_sections;
}

/* Size the reusable effect's sample buffer for every preset. */
s32 func_8009D864(void) {
  s32 index;
  s32 max_length = 0;

  for (index = 0; D_800C7A8C[index] != NULL; index++) {
    if (D_800C7A8C[index][1] > max_length) {
      max_length = D_800C7A8C[index][1];
    }
  }
  return max_length;
}

int ChangeCustomEffect(s32 index) {
  if (index >= D_800C7A88) {
    return 1;
  } else {
    func_8009D40C(D_800C7A8C[index]);
    alFxParam(((N_ALSynth*)D_801B74B0.synth)->auxBus->fx, 1,
              ((N_ALSynth*)D_801B74B0.synth)->auxBus);
    return 0;
  }
}
