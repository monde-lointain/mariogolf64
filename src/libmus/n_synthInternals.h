/*
 * n_synthInternals.h
 *
 * naudio synthesis-driver internals: the RSP DMEM scratch-buffer offsets, the
 * per-voice and per-filter state structs, and the driver's internal pull /
 * parameter entry points. This is the naudio counterpart of synthInternals.h,
 * sized for the naudio 184-sample processing block (the stock library uses
 * 160). Reached only when SUPPORT_NAUDIO is built.
 */
#ifndef __N_SYNTHINTERNALS__
#define __N_SYNTHINTERNALS__

// Build on the stock synth structs plus the naudio audio and command-ABI heads.
#include <synthInternals.h>
#include "n_libaudio_sc.h"
#include "n_abi.h"

// Resampler/mixer rounding-mode switches: intermediate-sample rounding is left
// disabled, final-output rounding enabled.
#define SAMPLE_ROUND
#undef SAMPLE_ROUND
#define FINAL_ROUND

// naudio processes audio one fixed block at a time.
#define SAMPLES 184  // samples per block
#define SAMPLE184(delta) \
  (((delta) + (SAMPLES / 2)) / SAMPLES) * SAMPLES  // round to nearest block
#define FIXED_SAMPLE SAMPLES

// Byte offsets of the audio scratch buffers within RSP DMEM. Each slot is 368
// bytes (184 samples x 2 bytes); names that share an offset mark a buffer
// reused between pipeline stages (e.g. the decoder input doubles as the
// resampler output and temp 0).
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
// A "free this voice" event queued on a parameter list: after `delta` ticks the
// driver reclaims the named physical voice.
typedef struct {
  struct ALParam_s* next;
  s32 delta;                  // ticks until the event fires
  s16 type;                   // event tag
  struct N_PVoice_s* pvoice;  // physical voice to free
} N_ALFreeParam;

// A naudio physical voice: one currently-sounding sample. It carries, inline,
// the state of the three pipeline stages a sample passes through each block --
// the ADPCM decoder (dc_*), the resampler (rs_*), and the envelope/pan mixer
// (em_*).
typedef struct N_PVoice_s {
  ALLink node;                 // free/active list link
  struct N_ALVoice_s* vvoice;  // virtual voice driving this physical voice

  // ADPCM decoder state.
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

  // Resampler state.
  RESAMPLE_STATE* rs_state;
  f32 rs_ratio;   // output/input rate ratio (pitch)
  s32 rs_upitch;  // unity-pitch (no resample) flag
  f32 rs_delta;   // fractional sample position
  s32 rs_first;   // first-block flag

  // Envelope / pan mixer state.
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
  s32 em_motion;  // play state: AL_PLAYING or AL_STOPPED
  s32 offset;     // DMEM offset of this voice's scratch buffer
} N_PVoice;
// Per-block "pull" callback for a naudio filter: given the block's sample
// offset and the current command-list pointer, it emits its RSP commands and
// returns the advanced pointer.
typedef Acmd* (*N_ALCmdHandler)(s32, Acmd*);

// Base naudio filter: one node in the synthesis pipeline. Nodes chain via
// `source`; `inp`/`outp` are the DMEM buffers it reads and writes.
typedef struct N_ALFilter_s {
  struct N_ALFilter_s* source;  // upstream node feeding this one
  N_ALCmdHandler handler;       // per-block command emitter
  ALSetParam setParam;          // parameter-change handler
  s16 inp;                      // input DMEM offset
  s16 outp;                     // output DMEM offset
  s32 type;                     // filter type tag
} N_ALFilter;

// The main (final) output bus -- just a filter at the end of the chain.
typedef struct N_ALMainBus_s {
  N_ALFilter filter;
} N_ALMainBus;

// An auxiliary/effects bus: mixes a set of source voices and applies an effect.
typedef struct N_ALAuxBus_s {
  ALFilter filter;
  s32 sourceCount;                         // active sources
  s32 maxSources;                          // capacity
  N_PVoice** sources;                      // source voices feeding the bus
  ALFx* fx;                                // current effect
  ALFx* fx_array[AL_MAX_AUX_BUS_SOURCES];  // effect slots
} N_ALAuxBus;

// Construct a physical voice in the supplied heap, wiring up its DMA source.
void alN_PVoiceNew(N_PVoice* mv, ALDMANew dmaNew, ALHeap* hp);

// Parameter-list node pool and physical-voice lifetime management.
ALParam* __n_allocParam(void);
void _n_freeParam(ALParam* param);
void _n_freePVoice(N_PVoice* pvoice);
void _n_collectPVoices(void);

// Convert between wall-clock microseconds and sample counts at the output rate.
s32 _n_timeToSamples(s32 micros);
ALMicroTime _n_samplesToTime(s32 samples);

// Per-stage command emitters ("Pull") and parameter handlers ("Param"): each
// appends its stage's RSP commands to the list and returns the advanced
// pointer.
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

// Reverb low-pass initialiser, present only in the legacy audio library.
#ifdef _OLD_AUDIO_LIBRARY
void init_lpfilter(ALLowPass* lp);
#endif

#endif
