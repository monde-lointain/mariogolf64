/*
 * libmus.h
 *
 * Public API of libmus, the music and sound-effect driver. A game includes this
 * header to start, stop, and steer songs and effects on the N64 audio hardware:
 * the one-time musConfig setup block, the pointer/effect/wave bank managers,
 * per-handle controls (volume, pan, tempo, reverb, pause), and the scheduler
 * hooks that tie playback to the audio frame. A musHandle names one playing
 * song or effect.
 */
#ifndef _LIBMUS_H_
#define _LIBMUS_H_

#include <ultra64.h>

#ifdef _LANGUAGE_C_PLUS_PLUS
extern "C" {
#endif

// One-time initialisation block passed to MusInitialize: heap, thread, synth,
// and DMA-streaming settings for the whole driver.
typedef struct {
  unsigned long control_flag;  // MUSCONTROL_* configuration flags
  int channels;                // number of mixing channels (voices)
  void* sched;                 // OS scheduler the audio thread runs under
  int thread_priority;         // audio thread priority
  unsigned char* heap;         // base of the driver's working heap
  int heap_length;             // heap size, bytes
  unsigned char* ptr;          // default pointer-bank data
  unsigned char* wbk;          // default wave-bank data
  void* default_fxbank;        // default effect (reverb) bank
  int fifo_length;             // audio command FIFO length
  int syn_updates;             // synth parameter updates per frame
  int syn_output_rate;         // output sample rate, Hz
  int syn_rsp_cmds;            // max RSP audio commands per frame
  int syn_retraceCount;        // VI retraces between synth updates
  int syn_num_dma_bufs;        // number of sample-streaming DMA buffers
  int syn_dma_buf_size;        // size of each DMA buffer, bytes
  OSPiHandle* diskrom_handle;  // PI handle for streaming samples from cart/disk
} musConfig;

typedef enum { MUSBOOL_OFF, MUSBOOL_ON } musBool;

// An RSP audio task handed to the scheduler's dotask callback each frame.
typedef struct {
  u64* data;        // audio command list
  int data_size;    // command-list size, bytes
  u64* ucode;       // audio microcode
  u64* ucode_data;  // microcode data segment
} musTask;

// Opaque identifier for a playing song or effect (0 means "none").
typedef unsigned long musHandle;

// Game-supplied callbacks: a song-marker hook plus the three scheduler hooks.
typedef void (*LIBMUScb_marker)(musHandle, int);
typedef void (*LIBMUScb_install)(void);
typedef void (*LIBMUScb_waitframe)(void);
typedef void (*LIBMUScb_dotask)(musTask*);

// Scheduler hook set: lets the game drive libmus from its own frame loop --
// install the audio thread, wait for a frame, and run the RSP task.
typedef struct {
  LIBMUScb_install install;
  LIBMUScb_waitframe waitframe;
  LIBMUScb_dotask dotask;
} musSched;

// control_flag bit: sample data lives in RAM rather than being streamed from
// ROM.
#define MUSCONTROL_RAM (1 << 0)

// `flags` bits selecting which playback to act on (volume/stop/query calls).
#define MUSFLAG_EFFECTS 1
#define MUSFLAG_SONGS 2
// Driver setup and global controls.
extern int MusInitialize(musConfig* config);
extern int MusSetFxType(int fxtype);
extern int MusSetSongFxChange(musBool onoff);
extern void MusSetMasterVolume(unsigned long flags, int volume);

// Start and stop playback. The MusStart* calls return a handle to the new
// song/effect; MusStop silences all songs and/or effects per `flags`, fading
// over `speed`.
extern musHandle MusStartSong(void* addr);
extern musHandle MusStartSongFromMarker(void* addr, int marker);
extern musHandle MusStartEffect(int number);
extern musHandle MusStartEffect2(int number, int volume, int pan,
                                 int restartflag, int priority);
extern void MusStop(unsigned long flags, int speed);

// Query whether anything selected by `flags` / the given handle is still
// playing.
extern int MusAsk(unsigned long flags);
extern int MusHandleAsk(musHandle handle);

// Per-handle playback controls (volume, pan, pitch/tempo, reverb, pause).
extern int MusHandleStop(musHandle handle, int speed);
extern int MusHandleSetVolume(musHandle handle, int volume);
extern int MusHandleSetPan(musHandle handle, int pan);
extern int MusHandleSetFreqOffset(musHandle handle, float offset);
extern int MusHandleSetTempo(musHandle handle, int tempo);
extern int MusHandleSetReverb(musHandle handle, int reverb);
extern int MusHandlePause(musHandle handle);
extern int MusHandleUnPause(musHandle handle);
extern void* MusHandleGetPtrBank(musHandle handle);

// Pointer-bank management: a pointer bank maps song/effect numbers to their
// sample data. SetSingle/SetCurrent choose the active bank for later starts.
extern void MusPtrBankInitialize(void* pbank, void* wbank);
extern void* MusPtrBankSetSingle(void* ipbank);
extern void MusPtrBankSetCurrent(void* ipbank);
extern void* MusPtrBankGetCurrent(void);

// Effect-bank management: an effect (FX) bank holds reverb/effect presets and
// links to a pointer bank for its samples.
extern void MusFxBankInitialize(void* fxbank);
extern void MusFxBankSetSingle(void* ifxbank);
extern void MusFxBankSetCurrent(void* ifxbank);
extern void* MusFxBankGetCurrent(void);
extern int MusFxBankNumberOfEffects(void* ifxbank);
extern void MusFxBankSetPtrBank(void* ifxbank, void* ipbank);
extern void* MusFxBankGetPtrBank(void* ifxbank);
extern void MusFxBankSetSingle(void* ifxbank);

// Hook the driver into the game's frame loop and song-marker notifications.
extern void MusSetScheduler(musSched* sched_list);
extern void MusSetMarkerCallback(void* callback);

// Inspect the wave (sample) data backing a playing handle.
extern int MusHandleWaveCount(musHandle handle);
extern unsigned short* MusHandleWaveAddress(musHandle handle);

// Convenience wrappers that select a pointer bank and then act in one call. The
// `(x) == (cast)MusPtrBankSetSingle(ipbank) ? (x) : (x)` form always yields x
// while forcing the bank to be selected first (a sequencing trick), so the
// start happens against the freshly chosen bank.
#define MusBankInitialize(pbank, wbank) MusPtrBankInitialize(pbank, wbank)
#define MusBankStartSong(ipbank, addr) \
  MusStartSong((addr) == (void*)MusPtrBankSetSingle(ipbank) ? (addr) : (addr))
#define MusBankStartEffect(ipbank, number)                               \
  MusStartEffect((number) == (int)MusPtrBankSetSingle(ipbank) ? (number) \
                                                              : (number))
#define MusBankStartEffect2(ipbank, number, volume, pan, restartflag,     \
                            priority)                                     \
  MusStartEffect2(                                                        \
      (number) == (int)MusPtrBankSetSingle(ipbank) ? (number) : (number), \
      volume, pan, restartflag, priority)
#ifdef _LANGUAGE_C_PLUS_PLUS
}
#endif
#endif
