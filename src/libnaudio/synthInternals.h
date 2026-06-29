/*
 * synthInternals.h
 *
 * Private interface of the stock libaudio software synthesizer (the SGI "AL"
 * audio library). Declares the pull-model filter graph that turns voices into
 * an RSP audio command list: the per-stage filter structs (ADPCM decoder,
 * resampler, envelope mixer, FX, main/aux bus, save), the control-parameter
 * message types queued to those filters, the fixed DMEM scratch-buffer layout,
 * and the private driver prototypes. The n_audio variant flattens these same
 * stages; see n_synthInternals.h.
 */
#ifndef __audioInternals__
#define __audioInternals__
#include <libaudio.h>

/* Parameter-message ids: the "set" operations a filter understands. */
enum {
  AL_FILTER_FREE_VOICE,
  AL_FILTER_SET_SOURCE,
  AL_FILTER_ADD_SOURCE,
  AL_FILTER_ADD_UPDATE,
  AL_FILTER_RESET,
  AL_FILTER_SET_WAVETABLE,
  AL_FILTER_SET_DRAM,
  AL_FILTER_SET_PITCH,
  AL_FILTER_SET_UNITY_PITCH,
  AL_FILTER_START,
  AL_FILTER_SET_STATE,
  AL_FILTER_SET_VOLUME,
  AL_FILTER_SET_PAN,
  AL_FILTER_START_VOICE_ALT,
  AL_FILTER_START_VOICE,
  AL_FILTER_STOP_VOICE,
  AL_FILTER_SET_FXAMT
};

/* Most samples the RSP processes in one frame; sets the scratch-buffer size. */
#define AL_MAX_RSP_SAMPLES 160

/*
 * Fixed DMEM scratch-buffer layout, as byte offsets. Each half-buffer holds
 * AL_MAX_RSP_SAMPLES (160) s16 samples = 320 bytes; the pipeline reuses the
 * low region in place (decoder-in, resampler-out and temp 0 all start at 0).
 */
#define AL_DECODER_IN 0
#define AL_RESAMPLER_OUT 0
#define AL_TEMP_0 0
#define AL_DECODER_OUT 320
#define AL_TEMP_1 320
#define AL_TEMP_2 640
#define AL_MAIN_L_OUT 1088
#define AL_MAIN_R_OUT 1408
#define AL_AUX_L_OUT 1728
#define AL_AUX_R_OUT 2048

/* Filter (pipeline-stage) type tags, stored in ALFilter.type. */
enum {
  AL_ADPCM,
  AL_RESAMPLE,
  AL_BUFFER,
  AL_SAVE,
  AL_ENVMIX,
  AL_FX,
  AL_AUXBUS,
  AL_MAINBUS
};

/*
 * Control message queued to a filter, applied `delta` samples in the future.
 * `type` is one of the AL_FILTER_* ids; the four unions carry up to four
 * typed payload words (float or int) whose meaning depends on `type`. The
 * Start/Free param structs below overlay this same node with named fields.
 */
typedef struct ALParam_s {
  struct ALParam_s* next;
  s32 delta;  // ticks from the previous event to this one
  s16 type;   // parameter selector (an AL_FILTER_* value)
  union {
    f32 f;
    s32 i;
  } data;
  union {
    f32 f;
    s32 i;
  } moredata;
  union {
    f32 f;
    s32 i;
  } stillmoredata;
  union {
    f32 f;
    s32 i;
  } yetstillmoredata;
} ALParam;

/* Start a voice with full parameters (pitch, volume, pan, fx mix, wavetable).
 */
typedef struct {
  struct ALParam_s* next;
  s32 delta;
  s16 type;
  s16 unity;  // nonzero: play at unity pitch (resampler disabled)
  f32 pitch;
  s16 volume;
  ALPan pan;
  u8 fxMix;  // effect-send (wet) amount
  s32 samples;
  struct ALWaveTable_s* wave;
} ALStartParamAlt;

/* Start a voice from just a wavetable (pitch/volume left at current state). */
typedef struct {
  struct ALParam_s* next;
  s32 delta;
  s16 type;
  s16 unity;  // nonzero: play at unity pitch (resampler disabled)
  struct ALWaveTable_s* wave;
} ALStartParam;

/* Return a physical voice to the free pool. */
typedef struct {
  struct ALParam_s* next;
  s32 delta;
  s16 type;
  struct PVoice_s* pvoice;
} ALFreeParam;

/* Pull entry point: a filter appends its RSP commands and returns the new tail.
 */
typedef Acmd* (*ALCmdHandler)(void*, s16*, s32, s32, Acmd*);
/* Apply one AL_FILTER_* parameter to a filter. */
typedef s32 (*ALSetParam)(void*, s32, void*);

/*
 * Base of every pipeline stage. `source` is the upstream filter pulled from;
 * `inp`/`outp` are its DMEM in/out buffer offsets; `type` is an AL_* tag.
 */
typedef struct ALFilter_s {
  struct ALFilter_s* source;  // upstream node feeding this one
  ALCmdHandler handler;       // per-block command emitter
  ALSetParam setParam;        // parameter-change handler
  s16 inp;                    // input DMEM offset
  s16 outp;                   // output DMEM offset
  s32 type;                   // filter type tag (an AL_* value)
} ALFilter;

void alFilterNew(ALFilter* f, ALCmdHandler h, ALSetParam s, s32 type);

// Concurrent decoder states needed (depends on subframes per frame + loop len).
#define AL_MAX_ADPCM_STATES 3

/* ADPCM decoder stage: streams compressed samples from DRAM and decodes them.
 */
typedef struct {
  ALFilter filter;
  ADPCM_STATE* state;   // decoder history (current)
  ADPCM_STATE* lstate;  // decoder history saved at the loop point
  ALRawLoop loop;
  struct ALWaveTable_s* table;
  s32 bookSize;   // ADPCM codebook size, bytes
  ALDMAproc dma;  // DMA callback feeding the decoder
  void* dmaState;
  s32 sample;  // current decode position
  s32 lastsam;
  s32 first;  // first-block flag
  s32 memin;
} ALLoadFilter;

void alLoadNew(ALLoadFilter* f, ALDMANew dma, ALHeap* hp);
Acmd* alAdpcmPull(void* f, s16* outp, s32 byteCount, s32 sampleOffset, Acmd* p);
Acmd* alRaw16Pull(void* f, s16* outp, s32 byteCount, s32 sampleOffset, Acmd* p);
s32 alLoadParam(void* filter, s32 paramID, void* param);

/* Pitch resampler stage: ratio/delta drive the fractional sample step. */
typedef struct ALResampler_s {
  ALFilter filter;
  RESAMPLE_STATE* state;
  f32 ratio;          // output/input rate ratio (pitch)
  s32 upitch;         // unity-pitch (no resample) flag
  f32 delta;          // fractional sample position
  s32 first;          // first-block flag
  ALParam* ctrlList;  // pending parameter-change events
  ALParam* ctrlTail;
  s32 motion;
} ALResampler;

/* One-pole low-pass filter used by the FX (reverb) path. */
typedef struct {
  s16 fc;     // cutoff coefficient
  s16 fgain;  // filter gain
  union {
    s16 fccoef[16];     // computed pole-filter coefficients
    s64 force_aligned;  // forces 8-byte alignment of the coefficient vector
  } fcvec;
  POLEF_STATE* fstate;
  s32 first;  // first-block flag
} ALLowPass;

/* One tap of the reverb delay line: a delayed, filtered, resampled feedback. */
typedef struct {
  u32 input;   // delay-line input DMEM offset
  u32 output;  // delay-line output DMEM offset
  s16 ffcoef;  // feed-forward coefficient
  s16 fbcoef;  // feedback coefficient
  s16 gain;    // output gain
  f32 rsinc;   // resampler increment (delay modulation)
  f32 rsval;
  s32 rsdelta;
  f32 rsgain;
  ALLowPass* lp;    // optional low-pass on the feedback
  ALResampler* rs;  // optional resampler for modulated delay
} ALDelay;

typedef s32 (*ALSetFXParam)(void*, s32, void*);

/* Effects (reverb) stage: a ring buffer of `section_count` delay taps. */
typedef struct {
  struct ALFilter_s filter;
  s16* base;              // base of the reverb delay buffer
  s16* input;             // current input pointer
  u32 length;             // delay buffer length, samples
  ALDelay* delay;         // array of delay-line sections
  u8 section_count;       // number of delay sections
  ALSetFXParam paramHdl;  // effect-specific parameter handler
} ALFx;

void alFxNew(ALFx* r, ALSynConfig* c, ALHeap* hp);
Acmd* alFxPull(void* f, s16* outp, s32 out, s32 sampleOffset, Acmd* p);
s32 alFxParam(void* filter, s32 paramID, void* param);
s32 alFxParamHdl(void* filter, s32 paramID, void* param);

#define AL_MAX_MAIN_BUS_SOURCES 1

/* Main bus: sums its source filters into the final stereo output. */
typedef struct ALMainBus_s {
  ALFilter filter;
  s32 sourceCount;  // active sources
  s32 maxSources;   // capacity
  ALFilter** sources;
} ALMainBus;

void alMainBusNew(ALMainBus* m, void* ptr, s32 len);
Acmd* alMainBusPull(void* f, s16* outp, s32 outCount, s32 sampleOffset,
                    Acmd* p);
s32 alMainBusParam(void* filter, s32 paramID, void* param);

#define AL_MAX_AUX_BUS_SOURCES 8
#define AL_MAX_AUX_BUS_FX 1

/* Aux bus: sums its sources, then runs them through its FX before the main bus.
 */
typedef struct ALAuxBus_s {
  ALFilter filter;
  s32 sourceCount;  // active sources
  s32 maxSources;   // capacity
  ALFilter** sources;
  ALFx fx[AL_MAX_AUX_BUS_FX];  // effect applied to the summed signal
} ALAuxBus;

void alAuxBusNew(ALAuxBus* m, void* ptr, s32 len);
Acmd* alAuxBusPull(void* f, s16* outp, s32 outCount, s32 sampleOffset, Acmd* p);
s32 alAuxBusParam(void* filter, s32 paramID, void* param);
void alResampleNew(ALResampler* r, ALHeap* hp);
Acmd* alResamplePull(void* f, s16* outp, s32 out, s32 sampleOffset, Acmd* p);
s32 alResampleParam(void* f, s32 paramID, void* param);

/* Save stage: DMAs the finished frame from DMEM out to the DRAM output buffer.
 */
typedef struct ALSave_s {
  ALFilter filter;
  s32 dramout;  // DRAM address of the output buffer
  s32 first;    // first-block flag
} ALSave;

void alSaveNew(ALSave* r);
Acmd* alSavePull(void* f, s16* outp, s32 outCount, s32 sampleOffset, Acmd* p);
s32 alSaveParam(void* f, s32 paramID, void* param);

/*
 * Envelope mixer stage: ramps left/right volume toward target at a given rate
 * and splits each voice into dry (main) and wet (aux/fx) amounts. Each left
 * and right rate is split into mantissa (m) and low (l) words for the RSP.
 */
typedef struct ALEnvMixer_s {
  ALFilter filter;
  ENVMIX_STATE* state;
  s16 pan;
  s16 volume;
  s16 cvolL;   // current left volume
  s16 cvolR;   // current right volume
  s16 dryamt;  // dry (unreverbed) mix amount
  s16 wetamt;  // wet (reverb send) mix amount
  u16 lratl;   // left gain ramp rate, low word
  s16 lratm;   // left gain ramp rate, mid word
  s16 ltgt;    // left gain target
  u16 rratl;   // right gain ramp rate, low word
  s16 rratm;   // right gain ramp rate, mid word
  s16 rtgt;    // right gain target
  s32 delta;
  s32 segEnd;         // end of the current envelope segment
  s32 first;          // first-block flag
  ALParam* ctrlList;  // pending parameter-change events
  ALParam* ctrlTail;
  ALFilter** sources;
  s32 motion;
} ALEnvMixer;

void alEnvmixerNew(ALEnvMixer* e, ALHeap* hp);
Acmd* alEnvmixerPull(void* f, s16* outp, s32 out, s32 sampleOffset, Acmd* p);
s32 alEnvmixerParam(void* filter, s32 paramID, void* param);

/* Per-allocation header prepended to each debug-heap block. */
typedef struct {
  s32 magic;  // sentinel used to check block integrity
  s32 size;   // size of this allocated block, in bytes
  u8* file;   // source file the allocation was requested from
  s32 line;   // source line the allocation was requested from
  s32 count;  // sequential heap-call number
  s32 pad0;
  s32 pad1;
  s32 pad2;  // pads the header out to 32 bytes
} HeapInfo;

// Heap allocations are rounded/aligned to (this + 1) = 16-byte cache lines.
#define AL_CACHE_ALIGN 15

/*
 * Physical voice: one hardware playback channel, built as a decoder ->
 * resampler -> envelope-mixer chain. `vvoice` links back to the virtual voice
 * that currently owns it; `offset` is its sample offset within the frame.
 */
typedef struct PVoice_s {
  ALLink node;               // free/active list link
  struct ALVoice_s* vvoice;  // virtual voice driving this physical voice
  ALFilter* channelKnob;
  ALLoadFilter decoder;   // ADPCM decode stage
  ALResampler resampler;  // pitch/rate-convert stage
  ALEnvMixer envmixer;    // envelope/pan mix stage
  s32 offset;             // DMEM offset of this voice's scratch buffer
} PVoice;

/* Private driver routines: parameter pool, voice reclaim, and time conversion.
 */
ALParam* __allocParam(void);
void __freeParam(ALParam* param);
void _freePVoice(ALSynth* drvr, PVoice* pvoice);
void _collectPVoices(ALSynth* drvr);
s32 _timeToSamples(ALSynth* ALSynth, s32 micros);
ALMicroTime _samplesToTime(ALSynth* synth, s32 samples);
#ifndef _OLD_AUDIO_LIBRARY
void _init_lpfilter(ALLowPass* lp);
#endif
#endif
