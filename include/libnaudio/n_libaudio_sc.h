/*
 * n_libaudio_sc.h
 *
 * Public client interface of the n_audio ("single command") audio library: the
 * 184-sample-per-frame synthesizer driven by the n_aspMain RSP microcode. A
 * game includes this to allocate and control virtual voices, drive the synth
 * driver, and build each video frame's audio command list. The n_al* entry
 * points here replace the stock al* synth API; n_libaudio_sn_sc.h provides
 * source-compatibility macros that map the old al* names onto them.
 */
#ifndef __N_LIBAUDIO__
#define __N_LIBAUDIO__
#include <PR/libaudio.h>
#ifdef _LANGUAGE_C_PLUS_PLUS
extern "C" {
#endif
#include <PR/ultratypes.h>
#include <PR/mbi.h>

/*
 * Virtual voice: the client-visible handle for one playing sound. It is bound
 * to a physical voice (`pvoice`) only while audible; `state`, `priority`,
 * `fxBus`, and `unityPitch` track how and where it is mixed.
 */
typedef struct N_ALVoice_s {
  ALLink node;
  struct N_PVoice_s* pvoice;
  ALWaveTable* table;
  void* clientPrivate;
  s16 state;
  s16 priority;
  s16 fxBus;
  s16 unityPitch;
} N_ALVoice;

/*
 * n_audio synthesis driver state: the client player list, the physical-voice
 * pools, the heap and DMA callback, the bus graph, and the running sample
 * clock that schedules parameter changes.
 */
typedef struct {
  ALPlayer* head;     // client (sequence player) list head
  ALLink pFreeList;   // free physical voices
  ALLink pAllocList;  // allocated (in-use) physical voices
  ALLink pLameList;   // voices finished and waiting to be freed
  s32 paramSamples;   // sample time of the next scheduled parameter update
  s32 curSamples;     // running sample count since synth start
  ALDMANew dma;
  ALHeap* heap;
  struct ALParam_s* paramList;
  struct N_ALMainBus_s* mainBus;
  struct N_ALAuxBus_s* auxBus;
  s32 numPVoices;
  s32 maxAuxBusses;
  s32 outputRate;     // output sample rate, in Hz
  s32 maxOutSamples;  // max samples produced per frame
  s32 sv_dramout;     // save-stage DRAM output address
  s32 sv_first;       // save-stage first-frame flag
} N_ALSynth;

/* Synth-driver control API; all operate on the single global synth (n_syn). */
void n_alSynAddPlayer(ALPlayer* client);
ALFxRef n_alSynAllocFX(s16 bus, ALSynConfig* c, ALHeap* hp);
s32 n_alSynAllocVoice(N_ALVoice* voice, ALVoiceConfig* vc);
void n_alSynDelete(void);
void n_alSynFreeVoice(N_ALVoice* voice);
ALFxRef n_alSynGetFXRef(s16 bus, s16 index);
s16 n_alSynGetPriority(N_ALVoice* voice);
void n_alSynRemovePlayer(ALPlayer* client);
void n_alSynSetFXMix(N_ALVoice* v, u8 fxmix);
void n_alSynSetFXParam(ALFxRef fx, s16 paramID, void* param);
void n_alSynSetFXtype(s16 bus, ALFxId fxid, s32 rate);
void n_alSynSetPan(N_ALVoice* v, u8 pan);
void n_alSynSetPitch(N_ALVoice* v, f32 pitch);
void n_alSynSetPriority(N_ALVoice* voice, s16 priority);
void n_alSynSetVol(N_ALVoice* v, s16 volume, ALMicroTime t);
void n_alSynStartVoice(N_ALVoice* v, ALWaveTable* table);
void n_alSynStartVoiceParams(N_ALVoice* v, ALWaveTable* w, f32 pitch, s16 vol,
                             ALPan pan, u8 fxmix, ALMicroTime t);
void n_alSynStopVoice(N_ALVoice* v);
void n_alSynNew(ALSynConfig* c);

/* Top-level library globals: the one synth driver wrapped for client access. */
typedef struct {
  N_ALSynth drvr;
} N_ALGlobals;

extern N_ALGlobals* n_alGlobals;
extern N_ALSynth* n_syn;

/* Bring up / tear down the audio library and build one frame of RSP commands.
 */
void n_alInit(N_ALGlobals* g, ALSynConfig* c);
void n_alClose(void);
Acmd* n_alAudioFrame(Acmd* cmdList, s32* cmdLen, s16* outBuf, s32 outLen);

/* Linker symbols bounding the n_audio RSP microcode (n_aspMain) text and data.
 */
extern long long int n_aspMainTextStart[], n_aspMainTextEnd[];
extern long long int n_aspMainDataStart[], n_aspMainDataEnd[];
#ifdef _LANGUAGE_C_PLUS_PLUS
}
#endif
#endif
