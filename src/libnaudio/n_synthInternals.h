/*
 * Private internals of the naudio (Software Creations) synthesizer variant.
 *
 * Parallels the base synthInternals.h, adding the N_-prefixed structs whose
 * voice pipeline is flattened into one N_PVoice rather than chained ALFilter
 * stages, the naudio frame size (184 samples vs the base 160) and its DMEM
 * buffer offsets, and the n_al* driver prototypes the n_synset*.c setters call.
 */

#ifndef __N_SYNTHINTERNALS__
#define __N_SYNTHINTERNALS__

#include <synthInternals.h>
#include "n_libaudio_sc.h"
#include "n_abi.h"

#define SAMPLE_ROUND
#undef SAMPLE_ROUND
#define FINAL_ROUND

#define SAMPLES 184 /* samples processed per audio frame */
#define SAMPLE184(delta) (((delta) + (SAMPLES / 2)) / SAMPLES) * SAMPLES
#define FIXED_SAMPLE SAMPLES

/* Byte offsets of each stage's scratch buffer in RSP DMEM, sized for the
 * naudio 184-sample frame (cf. the base AL_* offsets sized for 160). */
#define N_AL_DECODER_IN 0
#define N_AL_RESAMPLER_OUT 0
#define N_AL_TEMP_0 0
#define N_AL_DECODER_OUT 368
#define N_AL_TEMP_1 368
#define N_AL_TEMP_2 736
#define N_AL_MAIN_L_OUT 1248
#define N_AL_MAIN_R_OUT 1616
#define N_AL_AUX_L_OUT 1984
#define N_AL_AUX_R_OUT 2352
#define N_AL_DIVIDED 368

typedef struct {
  struct ALParam_s* next;
  s32 delta;
  s16 type;
  struct N_PVoice_s* pvoice;
} N_ALFreeParam;

/* Physical voice with the decode/resample/envmix stages flattened inline; the
 * field groups below mirror the base ALLoadFilter / ALResampler / ALEnvMixer.
 */
typedef struct N_PVoice_s {
  ALLink node;
  struct N_ALVoice_s* vvoice;
  /* decoder stage (ALLoadFilter) */
  ADPCM_STATE* dc_state;
  ADPCM_STATE* dc_lstate;
  ALRawLoop dc_loop;
  struct ALWaveTable_s* dc_table;
  s32 dc_bookSize;
  ALDMAproc dc_dma;
  void* dc_dmaState;
  s32 dc_sample;
  s32 dc_lastsam;
  s32 dc_first;
  s32 dc_memin;
  /* resampler stage (ALResampler) */
  RESAMPLE_STATE* rs_state;
  f32 rs_ratio;
  s32 rs_upitch;
  f32 rs_delta;
  s32 rs_first;
  /* envelope-mixer stage (ALEnvMixer) */
  ENVMIX_STATE* em_state;
  s16 em_pan;
  s16 em_volume;
  s16 em_cvolL;
  s16 em_cvolR;
  s16 em_dryamt;
  s16 em_wetamt;
  u16 em_lratl;
  s16 em_lratm;
  s16 em_ltgt;
  u16 em_rratl;
  s16 em_rratm;
  s16 em_rtgt;
  s32 em_delta;
  s32 em_segEnd;
  s32 em_first;
  ALParam* em_ctrlList;
  ALParam* em_ctrlTail;
  s32 em_motion;
  s32 offset;
} N_PVoice;

typedef Acmd* (*N_ALCmdHandler)(s32, Acmd*);

typedef struct N_ALFilter_s {
  struct N_ALFilter_s* source;
  N_ALCmdHandler handler;
  ALSetParam setParam;
  s16 inp;
  s16 outp;
  s32 type;
} N_ALFilter;

typedef struct N_ALMainBus_s {
  N_ALFilter filter;
} N_ALMainBus;

typedef struct N_ALAuxBus_s {
  ALFilter filter;
  s32 sourceCount;
  s32 maxSources;
  N_PVoice** sources;
  ALFx* fx;
  ALFx* fx_array[AL_MAX_AUX_BUS_SOURCES];
} N_ALAuxBus;

void alN_PVoiceNew(N_PVoice* mv, ALDMANew dmaNew, ALHeap* hp);

ALParam* __n_allocParam(void);
void _n_freeParam(ALParam* param);
void _n_freePVoice(N_PVoice* pvoice);
void _n_collectPVoices(void);
s32 _n_timeToSamples(s32 micros);
ALMicroTime _n_samplesToTime(s32 samples);

Acmd* n_alAdpcmPull(N_PVoice* f, s16* outp, s32 byteCount, Acmd* p);
s32 n_alLoadParam(N_PVoice* filter, s32 paramID, void* param);
Acmd* n_alResamplePull(N_PVoice* f, s16* outp, Acmd* p);
s32 n_alResampleParam(N_PVoice* f, s32 paramID, void* param);
Acmd* n_alEnvmixerPull(N_PVoice* f, s32 sampleOffset, Acmd* p);
s32 n_alEnvmixerParam(N_PVoice* p, s32 paramID, void* param);
Acmd* n_alAuxBusPull(s32 sampleOffset, Acmd* p);
Acmd* n_alFxPull(s32 sampleOffset, Acmd* p);
s32 n_alFxParamHdl(void* filter, s32 paramID, void* param);
void n_alFxNew(ALFx** r, ALSynConfig* c, ALHeap* hp);
Acmd* n_alMainBusPull(s32 sampleOffset, Acmd* p);
Acmd* n_alSavePull(s32 sampleOffset, Acmd* p);
#ifdef _OLD_AUDIO_LIBRARY
void init_lpfilter(ALLowPass* lp);
#endif
#endif
