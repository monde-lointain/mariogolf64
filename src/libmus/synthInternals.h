/*
 * synthInternals.h
 *
 * Internal interface of the N64 audio-library synthesis driver. It declares the
 * RSP DMEM scratch-buffer offsets, the parameter-event and per-filter state
 * structs that form the synthesis pipeline (decoder -> resampler -> envelope
 * mixer -> buses -> save), and the driver-private constructors and per-stage
 * pull/param routines. Game/library clients drive playback through the public
 * libaudio API; this header is for the driver itself and its custom-effect
 * extensions.
 */
#ifndef __audioInternals__
#define __audioInternals__

#include <libaudio.h>

// Parameter/command selectors passed to a filter's setParam handler: each names
// the voice operation to apply (set source, set pitch, start/stop a voice,
// ...).
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

// Samples the RSP processes per pass (the stock block size).
#define AL_MAX_RSP_SAMPLES 160

// Byte offsets of the audio scratch buffers within RSP DMEM. Each slot holds
// one 160-sample block (320 bytes); names sharing an offset mark a buffer
// reused between pipeline stages (decoder input == resampler output == temp 0).
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

// Filter type tags (the `type` field of ALFilter), one per pipeline stage.
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
// A scheduled parameter-change event in a filter's control list. Events are
// kept time-ordered as a singly linked list: `delta` is the tick count from the
// previous event to this one, `type` selects the parameter, and the four
// generic data slots carry its operands (each readable as float or int).
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

// Start-voice event (full form): hands the decoder its initial pitch, volume,
// pan, effect-send level, and wavetable when a note begins.
typedef struct {
  struct ALParam_s* next;
  s32 delta;
  s16 type;
  s16 unity;  // unity-pitch flag
  f32 pitch;
  s16 volume;
  ALPan pan;
  u8 fxMix;  // effect-send (wet) amount
  s32 samples;
  struct ALWaveTable_s* wave;
} ALStartParamAlt;

// Start-voice event (short form): wavetable and unity pitch only.
typedef struct {
  struct ALParam_s* next;
  s32 delta;
  s16 type;
  s16 unity;
  struct ALWaveTable_s* wave;
} ALStartParam;

// Free-voice event: release the named physical voice when it fires.
typedef struct {
  struct ALParam_s* next;
  s32 delta;
  s16 type;
  struct PVoice_s* pvoice;
} ALFreeParam;

// A filter's per-block command emitter and its parameter handler.
typedef Acmd* (*ALCmdHandler)(void*, s16*, s32, s32, Acmd*);
typedef s32 (*ALSetParam)(void*, s32, void*);

// Base filter: one node in the synthesis pipeline. Nodes chain via `source`;
// `inp`/`outp` are the DMEM buffers it reads from and writes to.
typedef struct ALFilter_s {
  struct ALFilter_s* source;  // upstream node feeding this one
  ALCmdHandler handler;       // per-block command emitter
  ALSetParam setParam;        // parameter-change handler
  s16 inp;                    // input DMEM offset
  s16 outp;                   // output DMEM offset
  s32 type;                   // filter type tag (an AL_* value)
} ALFilter;

// Initialise the base fields common to every filter.
void alFilterNew(ALFilter* f, ALCmdHandler h, ALSetParam s, s32 type);
#define AL_MAX_ADPCM_STATES 3

// The decode ("load") stage: DMAs compressed samples from DRAM and
// ADPCM-decodes them, tracking loop bounds and the running decoder history.
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

// The resample stage: rate-converts the decoded stream to the requested pitch,
// keeping a fractional position (`delta`) and a list of pending pitch changes.
typedef struct ALResampler_s {
  ALFilter filter;
  RESAMPLE_STATE* state;
  f32 ratio;          // output/input rate ratio (pitch)
  s32 upitch;         // unity-pitch (no resample) flag
  f32 delta;          // fractional sample position
  s32 first;          // first-block flag
  ALParam* ctrlList;  // pending parameter-change events
  ALParam* ctrlTail;
  s32 motion;  // play state: AL_PLAYING or AL_STOPPED
} ALResampler;

// One-pole low-pass filter used on the reverb feedback path. The coefficient
// vector is unioned with an s64 only to force 8-byte alignment for the RSP.
typedef struct {
  s16 fc;     // cutoff coefficient
  s16 fgain;  // filter gain
  union {
    s16 fccoef[16];     // computed pole-filter coefficients
    s64 force_aligned;  // alignment padding only
  } fcvec;
  POLEF_STATE* fstate;
  s32 first;  // first-block flag
} ALLowPass;

// A reverb delay line: feeds samples through a DMEM delay buffer with
// feed-forward/feedback taps, optionally resampled and low-pass filtered for a
// pitch-modulated, damped echo.
typedef struct {
  u32 input;        // delay-line input DMEM offset
  u32 output;       // delay-line output DMEM offset
  s16 ffcoef;       // feed-forward coefficient
  s16 fbcoef;       // feedback coefficient
  s16 gain;         // output gain
  f32 rsinc;        // chorus/flange modulation step per sample (sweep rate)
  f32 rsval;        // current modulation phase (triangle wave)
  s32 rsdelta;      // running sample offset of the modulated read tap
  f32 rsgain;       // modulation depth: scales the phase into a delay offset
  ALLowPass* lp;    // optional low-pass on the feedback
  ALResampler* rs;  // optional resampler for modulated delay
} ALDelay;
typedef s32 (*ALSetFXParam)(void*, s32, void*);

// The effects (reverb) filter: runs `section_count` delay lines over a working
// buffer to build the wet signal mixed back into the buses.
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

// The main bus: sums its source filters into the final stereo output.
#define AL_MAX_MAIN_BUS_SOURCES 1
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

// An aux bus: sums its sources and applies its built-in effect (reverb).
#define AL_MAX_AUX_BUS_SOURCES 8
#define AL_MAX_AUX_BUS_FX 1
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
// The save stage: writes the finished block from DMEM out to a DRAM output
// buffer (`dramout`) for DMA to the audio DAC.
typedef struct ALSave_s {
  ALFilter filter;
  s32 dramout;  // DRAM address of the output buffer
  s32 first;    // first-block flag
} ALSave;
void alSaveNew(ALSave* r);
Acmd* alSavePull(void* f, s16* outp, s32 outCount, s32 sampleOffset, Acmd* p);
s32 alSaveParam(void* f, s32 paramID, void* param);

// The envelope/pan mixer: ramps left/right gains toward their targets across
// the block and splits the signal into dry and wet (effect-send) amounts. The
// l/r ramp rates are split into low (u16) and mid (s16) words for the RSP.
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
  s32 motion;  // play state: AL_PLAYING or AL_STOPPED
} ALEnvMixer;
void alEnvmixerNew(ALEnvMixer* e, ALHeap* hp);
Acmd* alEnvmixerPull(void* f, s16* outp, s32 out, s32 sampleOffset, Acmd* p);
s32 alEnvmixerParam(void* filter, s32 paramID, void* param);

// Per-allocation header prepended to each block in the audio heap; the
// file/line and magic support leak/overrun debugging of the allocator.
typedef struct {
  s32 magic;  // sentinel marking a valid block
  s32 size;   // payload size, bytes
  u8* file;   // source file of the allocation call
  s32 line;   // source line of the allocation call
  s32 count;  // allocation sequence number
  s32 pad0;
  s32 pad1;
  s32 pad2;
} HeapInfo;

// Mask to round an allocation up to a 16-byte data-cache line.
#define AL_CACHE_ALIGN 15

// A physical voice: one sounding sample. It embeds the three pipeline stages a
// sample flows through each block -- decoder, resampler, then envelope mixer.
typedef struct PVoice_s {
  ALLink node;               // free/active list link
  struct ALVoice_s* vvoice;  // virtual voice driving this physical voice
  ALFilter* channelKnob;
  ALLoadFilter decoder;   // ADPCM decode stage
  ALResampler resampler;  // pitch/rate-convert stage
  ALEnvMixer envmixer;    // envelope/pan mix stage
  s32 offset;             // DMEM offset of this voice's scratch buffer
} PVoice;

// Parameter-list node pool and physical-voice lifetime management.
ALParam* __allocParam(void);
void __freeParam(ALParam* param);
void _freePVoice(ALSynth* drvr, PVoice* pvoice);
void _collectPVoices(ALSynth* drvr);

// Convert between wall-clock microseconds and sample counts at the output rate.
s32 _timeToSamples(ALSynth* ALSynth, s32 micros);
ALMicroTime _samplesToTime(ALSynth* synth, s32 samples);

// Reverb low-pass initialiser, present only in the current audio library.
#ifndef _OLD_AUDIO_LIBRARY
void _init_lpfilter(ALLowPass* lp);
#endif

#endif
