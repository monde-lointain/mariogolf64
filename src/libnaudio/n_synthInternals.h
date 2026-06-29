/*
 * n_synthInternals.h
 *
 * Private interface of the n_audio ("single command") synthesizer, the variant
 * that runs a fixed 184-sample frame with DMEM addresses packed into each RSP
 * command (see n_abi.h). It mirrors the stock synthInternals.h but flattens the
 * per-stage filter structs into one N_PVoice and uses leaner pull handlers.
 * Declares the 184-sample frame constants, the n_audio DMEM buffer layout, the
 * flattened voice/filter/bus structs, and the private n_* driver prototypes.
 */
#ifndef __N_SYNTHINTERNALS__
#define __N_SYNTHINTERNALS__
#include <synthInternals.h>
#include "n_libaudio_sc.h"
#include "n_abi.h"

// Rounding build toggles. SAMPLE_ROUND, when defined, would snap
// parameter-event scheduling times to the next 184-sample frame (via
// SAMPLE184); it is left disabled here -- defined, then immediately undefined.
// FINAL_ROUND is left enabled, selecting the envelope mixer's rounded
// segment-timing path.
#define SAMPLE_ROUND
#undef SAMPLE_ROUND
#define FINAL_ROUND

#define SAMPLES 184  // fixed samples processed per n_audio frame
// Round `delta` samples to the nearest whole multiple of SAMPLES (184).
#define SAMPLE184(delta) (((delta) + (SAMPLES / 2)) / SAMPLES) * SAMPLES
#define FIXED_SAMPLE SAMPLES  // fixed frame length, in samples

/*
 * n_audio DMEM scratch layout, as byte offsets. Buffers are spaced one
 * 184-sample s16 block (368 bytes) apart; decoder-in, resampler-out and temp 0
 * all share offset 0 so the early pipeline runs in place.
 */
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

#define N_AL_DIVIDED 368  // one 184-sample s16 block, in bytes

/* Return an n_audio physical voice to the free pool. */
typedef struct {
  struct ALParam_s* next;
  s32 delta;                  // ticks until the event fires
  s16 type;                   // event tag
  struct N_PVoice_s* pvoice;  // physical voice to free
} N_ALFreeParam;

/*
 * n_audio physical voice. Instead of nesting the decoder/resampler/envmixer
 * sub-structs the way stock PVoice does, it inlines all their fields into one
 * flat struct, prefixed by stage: dc_ = ADPCM decoder (ALLoadFilter), rs_ =
 * resampler (ALResampler), em_ = envelope mixer (ALEnvMixer). `node` links it
 * on a voice list and `vvoice` points back at the owning virtual voice.
 */
typedef struct N_PVoice_s {
  ALLink node;                 // free/active list link
  struct N_ALVoice_s* vvoice;  // virtual voice driving this physical voice

  // decoder stage (flattened ALLoadFilter)
  ADPCM_STATE* dc_state;   // decoder history (current)
  ADPCM_STATE* dc_lstate;  // decoder history (loop point)
  ALRawLoop dc_loop;       // raw-sample loop bounds
  struct ALWaveTable_s* dc_table;
  s32 dc_bookSize;   // ADPCM codebook size, bytes
  ALDMAproc dc_dma;  // DMA callback feeding the decoder
  void* dc_dmaState;
  s32 dc_sample;  // current decode position
  s32 dc_lastsam;
  s32 dc_first;  // first-block flag
  s32 dc_memin;

  // resampler stage (flattened ALResampler)
  RESAMPLE_STATE* rs_state;
  f32 rs_ratio;   // output/input rate ratio (pitch)
  s32 rs_upitch;  // unity-pitch (no resample) flag
  f32 rs_delta;   // fractional sample position
  s32 rs_first;   // first-block flag

  // envelope-mixer stage (flattened ALEnvMixer)
  ENVMIX_STATE* em_state;
  s16 em_pan;
  s16 em_volume;
  s16 em_cvolL;   // current left volume
  s16 em_cvolR;   // current right volume
  s16 em_dryamt;  // dry (unreverbed) mix amount
  s16 em_wetamt;  // wet (reverb send) mix amount
  u16 em_lratl;   // left gain ramp rate, low word
  s16 em_lratm;   // left gain ramp rate, mid word
  s16 em_ltgt;    // left gain target
  u16 em_rratl;   // right gain ramp rate, low word
  s16 em_rratm;   // right gain ramp rate, mid word
  s16 em_rtgt;    // right gain target
  s32 em_delta;
  s32 em_segEnd;         // end of the current envelope segment
  s32 em_first;          // first-block flag
  ALParam* em_ctrlList;  // pending parameter-change events
  ALParam* em_ctrlTail;
  s32 em_motion;
  s32 offset;  // DMEM offset of this voice's scratch buffer
} N_PVoice;

/* n_audio pull handler: leaner than the stock one (buffers are implicit). */
typedef Acmd* (*N_ALCmdHandler)(s32, Acmd*);

/* n_audio filter base; like ALFilter but with the leaner N_ALCmdHandler. */
typedef struct N_ALFilter_s {
  struct N_ALFilter_s* source;  // upstream node feeding this one
  N_ALCmdHandler handler;       // per-block command emitter
  ALSetParam setParam;          // parameter-change handler
  s16 inp;                      // input DMEM offset
  s16 outp;                     // output DMEM offset
  s32 type;                     // filter type tag
} N_ALFilter;

/* Main bus: a single n_audio filter that produces the final output. */
typedef struct N_ALMainBus_s {
  N_ALFilter filter;
} N_ALMainBus;

/* Aux bus: sums its voice sources and runs them through the attached FX. */
typedef struct N_ALAuxBus_s {
  ALFilter filter;
  s32 sourceCount;                         // active sources
  s32 maxSources;                          // capacity
  N_PVoice** sources;                      // source voices feeding the bus
  ALFx* fx;                                // current effect
  ALFx* fx_array[AL_MAX_AUX_BUS_SOURCES];  // effect slots
} N_ALAuxBus;

/* Initialize an n_audio physical voice and its DMA proc. */
void alN_PVoiceNew(N_PVoice* mv, ALDMANew dmaNew, ALHeap* hp);

/* Private driver routines: parameter pool, voice reclaim, time conversion. */
ALParam* __n_allocParam(void);
void _n_freeParam(ALParam* param);
void _n_freePVoice(N_PVoice* pvoice);
void _n_collectPVoices(void);
s32 _n_timeToSamples(s32 micros);
ALMicroTime _n_samplesToTime(s32 samples);

/* Per-stage pull/param handlers that append RSP commands for one voice. */
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
