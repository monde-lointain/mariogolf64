/*
 * libmus.h
 *
 * Public interface to the libmus sequenced-music and sound-effect player.
 *
 * libmus is the high-level audio engine: it owns a pool of playback channels,
 * registers itself with the low-level synthesis driver, and renders songs and
 * sound effects each audio frame. A caller never touches the synthesizer
 * directly; it drives everything through the Mus* calls below.
 *
 * Typical lifecycle:
 *   1. (Optional) install a custom frame scheduler with MusSetScheduler.
 *   2. Fill in a musConfig and call MusInitialize once. This carves the
 *      channel/voice pool out of the caller-supplied heap and brings the
 *      synthesis driver online.
 *   3. Start songs with MusStartSong and sound effects with MusStartEffect /
 *      MusStartEffect2. Each start returns a musHandle naming that one playing
 *      instance.
 *   4. Use the handle to adjust or stop that instance: volume, pan, tempo,
 *      reverb, pitch offset, pause/unpause, and fade-out stop.
 *
 * Instruments and samples live in "pointer banks"; sound effects live in "fx
 * banks". The caller selects which bank is current (persistently, or for the
 * next start only) before issuing a start call. Global controls set the master
 * volume, switch the active reverb/effect type, and report how many songs or
 * effects are currently sounding.
 */
#ifndef _LIBMUS_H_
#define _LIBMUS_H_

#include <ultra64.h>

#ifdef _LANGUAGE_C_PLUS_PLUS
extern "C" {
#endif

/* Configuration block describing the whole player; passed once to
 * MusInitialize. The memory it points at need not outlive that call. */
typedef struct {
  unsigned long control_flag; /* MUSCONTROL_* feature bits */
  int channels;         /* simultaneous song/effect channels to allocate */
  void* sched;          /* OS scheduler the audio thread drives */
  int thread_priority;  /* priority assigned to allocated synth voices */
  unsigned char* heap;  /* base of the block libmus sub-allocates from */
  int heap_length;      /* size of that heap, in bytes */
  unsigned char* ptr;   /* default pointer (instrument) bank, or NULL */
  unsigned char* wbk;   /* wavetable bank paired with `ptr`, or NULL */
  void* default_fxbank; /* fx bank made current at init, or NULL */
  int fifo_length;      /* depth of the pause/unpause/fx command queue */
  int syn_updates;      /* synth driver: max parameter updates per frame */
  int syn_output_rate;  /* synth driver: requested DAC sample rate (Hz) */
  int syn_rsp_cmds;     /* synth driver: max RSP audio command list size */
  int syn_retraceCount; /* vertical retraces buffered before playback */
  int syn_num_dma_bufs; /* number of sample-streaming DMA buffers */
  int syn_dma_buf_size; /* size of each DMA buffer, in bytes */
  OSPiHandle* diskrom_handle; /* PI handle for streaming audio off disk/ROM */
} musConfig;

/* Boolean argument type for the Mus* on/off switches. */
typedef enum { MUSBOOL_OFF, MUSBOOL_ON } musBool;

/* One RSP audio task handed to a custom scheduler's dotask callback. */
typedef struct {
  u64* data;       /* command list to execute */
  int data_size;   /* length of `data`, in bytes */
  u64* ucode;      /* audio microcode text */
  u64* ucode_data; /* audio microcode data */
} musTask;

/* Opaque identifier for one playing song or effect, returned by the start
 * calls and accepted by every MusHandle* control. Zero means "no instance". */
typedef unsigned long musHandle;

/* Caller hook invoked when a song reaches a marker; receives the song's handle
 * and the marker value. */
typedef void (*LIBMUScb_marker)(musHandle, int);

/* Custom-scheduler hooks: bring-up, per-frame wait, and task dispatch. */
typedef void (*LIBMUScb_install)(void);
typedef void (*LIBMUScb_waitframe)(void);
typedef void (*LIBMUScb_dotask)(musTask*);

/* Scheduler callback set installed with MusSetScheduler to override the
 * built-in frame loop. */
typedef struct {
  LIBMUScb_install install;     /* called once to start the scheduler */
  LIBMUScb_waitframe waitframe; /* called to block until the next audio frame */
  LIBMUScb_dotask dotask;       /* called to submit one RSP audio task */
} musSched;

/* musConfig.control_flag bit: song/sample data is already resident in RAM, so
 * no DMA streaming is performed. */
#define MUSCONTROL_RAM (1 << 0)

/* Selector bits for the bulk calls (MusSetMasterVolume, MusStop, MusAsk):
 * choose sound effects, songs, or both (bitwise-OR). */
#define MUSFLAG_EFFECTS 1
#define MUSFLAG_SONGS 2

/* Bring the player online: validate the config, allocate the channel and voice
 * pool from config->heap, open the command FIFO, install the default banks,
 * set master volume to full, and register with the synthesis driver. Call once
 * before any other Mus* function. Returns the number of heap bytes left
 * unused. */
extern int MusInitialize(musConfig* config);

/* Select the global reverb/effect type (an AL_FX_* value). The change is
 * queued and applied on the next audio frame. Returns nonzero if queued, 0 if
 * the command FIFO was full. */
extern int MusSetFxType(int fxtype);

/* Enable (MUSBOOL_ON) or disable (MUSBOOL_OFF) songs being allowed to change
 * the global effect type themselves. Returns nonzero if the setting took
 * effect. */
extern int MusSetSongFxChange(musBool onoff);

/* Set the overall output level for effects and/or songs (per `flags`).
 * `volume` ranges 0..0x7fff, where 0x7fff is full scale. */
extern void MusSetMasterVolume(unsigned long flags, int volume);

/* Start the song whose data begins at `addr` and begin playback immediately.
 * Returns a handle identifying the new song instance. */
extern musHandle MusStartSong(void* addr);

/* Start the song at `addr` but silently fast-forward every channel to the
 * given marker before playback begins. Returns the song's handle. */
extern musHandle MusStartSongFromMarker(void* addr, int marker);

/* Start sound effect `number` from the current fx bank at default volume and
 * pan (0x80 = unaltered) and the effect's stored priority. Returns a handle, or
 * 0 if no channel was free. */
extern musHandle MusStartEffect(int number);

/* Start sound effect `number` with explicit settings. `volume` and `pan` are
 * each 0..0x80 (0x80 = unaltered). If `restartflag` is nonzero an already
 * playing copy of the effect is retriggered instead of allocating a new
 * channel. `priority` arbitrates channel stealing; pass -1 to use the effect's
 * stored priority. Returns a handle, or 0 if no channel could be claimed. */
extern musHandle MusStartEffect2(int number, int volume, int pan,
                                 int restartflag, int priority);

/* Stop every song and/or effect selected by `flags`, fading out over `speed`
 * audio frames (0 fades as fast as possible). */
extern void MusStop(unsigned long flags, int speed);

/* Return how many channels are currently playing the categories named in
 * `flags` (songs and/or effects). */
extern int MusAsk(unsigned long flags);

/* Return how many channels the given handle still owns; 0 means the instance
 * has finished or `handle` is 0. */
extern int MusHandleAsk(musHandle handle);

/* Stop the instance named by `handle`, fading out over `speed` audio frames.
 * Returns the number of channels affected (0 if `handle` is 0). */
extern int MusHandleStop(musHandle handle, int speed);

/* Set the per-instance volume scale for `handle`'s channels; `volume` is
 * 0..0x80 (0x80 = unaltered). Returns the count of channels updated (0 if
 * `handle` is 0). */
extern int MusHandleSetVolume(musHandle handle, int volume);

/* Set the per-instance pan scale for `handle`'s channels; `pan` is 0..0x80
 * (0x80 = unaltered). Returns the count of channels updated (0 if `handle` is
 * 0). */
extern int MusHandleSetPan(musHandle handle, int pan);

/* Shift the playback pitch of `handle`'s channels by `offset` semitones
 * (combined with any per-channel distortion). Returns the count of channels
 * updated (0 if `handle` is 0). */
extern int MusHandleSetFreqOffset(musHandle handle, float offset);

/* Set the playback tempo of `handle`'s channels. `tempo` is clamped to
 * 1..256, where 128 is the song's authored speed. Returns the count of
 * channels updated (0 if `handle` is 0). */
extern int MusHandleSetTempo(musHandle handle, int tempo);

/* Set the reverb amount for `handle`'s channels, clamped to 0..127. Returns
 * the count of channels updated (0 if `handle` is 0). */
extern int MusHandleSetReverb(musHandle handle, int reverb);

/* Queue a pause for `handle`'s channels; applied on the next audio frame.
 * Returns nonzero if queued, 0 if the command FIFO was full. */
extern int MusHandlePause(musHandle handle);

/* Queue an unpause for `handle`'s channels; applied on the next audio frame.
 * Returns nonzero if queued, 0 if the command FIFO was full. */
extern int MusHandleUnPause(musHandle handle);

/* Return the pointer (instrument) bank the instance named by `handle` is
 * playing from, or NULL if the handle is invalid. */
extern void* MusHandleGetPtrBank(musHandle handle);

/* Remap a pointer (instrument) bank in place against its wavetable bank so it
 * is ready to play, and adopt it as the default bank if none is set yet. */
extern void MusPtrBankInitialize(void* pbank, void* wbank);

/* Select `ipbank` as the pointer bank used for the very next song/effect start
 * only. Returns the pointer bank that will actually be used. */
extern void* MusPtrBankSetSingle(void* ipbank);

/* Make `ipbank` the persistent current/default pointer bank for subsequent
 * starts. */
extern void MusPtrBankSetCurrent(void* ipbank);

/* Return the current/default pointer bank. */
extern void* MusPtrBankGetCurrent(void);

/* Initialize an fx bank in place (relocating its internal offsets) and adopt
 * it as the current fx bank if none is set yet. */
extern void MusFxBankInitialize(void* fxbank);

/* Select `ifxbank` as the fx bank used for the very next effect start only. */
extern void MusFxBankSetSingle(void* ifxbank);

/* Make `ifxbank` the persistent current fx bank for subsequent effect
 * starts. */
extern void MusFxBankSetCurrent(void* ifxbank);

/* Return the current fx bank. */
extern void* MusFxBankGetCurrent(void);

/* Return the number of effects defined in `ifxbank`. */
extern int MusFxBankNumberOfEffects(void* ifxbank);

/* Associate pointer (instrument) bank `ipbank` with fx bank `ifxbank`, so
 * effects from that bank draw their samples from it. */
extern void MusFxBankSetPtrBank(void* ifxbank, void* ipbank);

/* Return the pointer bank associated with `ifxbank`. */
extern void* MusFxBankGetPtrBank(void* ifxbank);

/* Select `ifxbank` for the next effect start only (re-declared from above;
 * the duplicate prototype is identical). */
extern void MusFxBankSetSingle(void* ifxbank);

/* Install a custom frame scheduler in place of the built-in loop. Call before
 * MusInitialize. */
extern void MusSetScheduler(musSched* sched_list);

/* Register the callback invoked whenever a song executes a marker command;
 * pass NULL to remove it. */
extern void MusSetMarkerCallback(void* callback);

/* Return the number of distinct waves (samples) the instance named by `handle`
 * uses, or 0 if the handle is invalid. */
extern int MusHandleWaveCount(musHandle handle);

/* Return the wavetable the instance named by `handle` plays from, or NULL if
 * the handle is invalid. */
extern unsigned short* MusHandleWaveAddress(musHandle handle);

/* Convenience: alias for MusPtrBankInitialize. */
#define MusBankInitialize(pbank, wbank) MusPtrBankInitialize(pbank, wbank)

/* Convenience: select `ipbank` as the one-shot pointer bank, then start the
 * song at `addr`. */
#define MusBankStartSong(ipbank, addr) \
  MusStartSong((addr) == (void*)MusPtrBankSetSingle(ipbank) ? (addr) : (addr))

/* Convenience: select `ipbank` as the one-shot pointer bank, then start effect
 * `number`. */
#define MusBankStartEffect(ipbank, number)                               \
  MusStartEffect((number) == (int)MusPtrBankSetSingle(ipbank) ? (number) \
                                                              : (number))

/* Convenience: select `ipbank` as the one-shot pointer bank, then start effect
 * `number` with explicit volume/pan/restart/priority. */
#define MusBankStartEffect2(ipbank, number, volume, pan, restartflag,     \
                            priority)                                     \
  MusStartEffect2(                                                        \
      (number) == (int)MusPtrBankSetSingle(ipbank) ? (number) : (number), \
      volume, pan, restartflag, priority)

#ifdef _LANGUAGE_C_PLUS_PLUS
}
#endif

#endif
