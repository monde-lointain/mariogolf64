/*
 * nualstl.h
 *
 * Public NuSYS audio-manager interface for the N64 Sound Tools (libmus)
 * sequence library plus the n_audio synthesizer. Declares the nuAuStl* entry
 * points a game calls to start audio and play music sequences and sound
 * effects, the configuration constants that size the audio heap, DMA buffers,
 * synthesizer channels and command list, and the handle/struct types and
 * convenience macros that wrap the underlying mus library. This is the
 * include-side public interface only; there is no matching .c file in this
 * tree.
 */
#ifndef _NUALSTL_H_
#define _NUALSTL_H_
#ifdef _LANGUAGE_C_PLUS_PLUS
extern "C" {
#endif

// Compile-time configuration: the sizes and identifiers used to build the
// default audio-manager configuration (nuAuStlConfig) and lay out the heap.
#define NU_AU_SEQPLAYER_NUM 2     // number of sequence players started
#define NU_AU_FIFO_LENGTH 64      // synthesizer command FIFO length (entries)
#define NU_AU_CHANNELS 24         // synthesizer voices (simultaneous channels)
#define NU_AU_SYN_UPDATE_MAX 256  // max synth voice-state updates per frame

// Audio DMA: buffers that stream sample data from ROM into the synthesizer.
#define NU_AU_DMA_BUFFER_NUM 64     // number of sample-DMA transfer buffers
#define NU_AU_DMA_BUFFER_SIZE 1024  // size of each DMA buffer, in bytes

// Audio-manager thread, command list, output rate, and message queue.
#define NU_AU_MGR_THREAD_PRI 70  // audio-manager thread priority
#define NU_AU_MGR_THREAD_ID 6    // audio-manager thread id
#define NU_AU_CLIST_LEN 0x800    // audio command-list buffer length (entries)
#define NU_AU_OUTPUT_RATE 32000  // synthesizer output sample rate, in Hz
#define NU_AU_MESG_MAX 2         // capacity of the audio-manager message queue

// Audio heap: default 320 KB carved from just below the NuSYS frame buffer,
// plus the default per-buffer sizes for song, sample, and sound-effect data.
#define NU_AU_HEAP_SIZE 0x50000  // default audio heap size, in bytes (320 KB)
// Audio heap base: placed just below the graphics framebuffer.
#define NU_AU_HEAP_ADDR (NU_GFX_FRAMEBUFFER_ADDR - NU_AU_HEAP_SIZE)
#define NU_AU_SONG_SIZE 0x4000  // default binary song-data buffer size (bytes)
// Default sample/wave-bank buffer size (bytes).
#define NU_AU_SAMPLE_SIZE 0x4000
#define NU_AU_SE_SIZE 0x4000  // default sound-effect-bank buffer size (bytes)

// Audio-task run state for nuAuTaskStop: when STOP, the manager skips the task.
#define NU_AU_TASK_STOP 0  // do not run the audio task this frame
#define NU_AU_TASK_RUN 1   // audio task may run

// Sequence-player index constants (two players are available).
#define NU_AU_SEQ_PLAYER0 0
#define NU_AU_SEQ_PLAYER1 1

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
// libultra base types plus the mus library (musHandle/musConfig and the Mus*
// playback API the control macros below wrap).
#include <ultra64.h>
#include <PR/libmus.h>
// n_audio synthesizer headers, included only for the n_audio build variant.
#ifdef N_AUDIO
#include <PR/n_libaudio_sc.h>
#include <PR/n_libaudio_sn_sc.h>
#endif

/*
 * One sequence player: a mus playback handle plus the heap buffer holding its
 * binary song data.
 */
typedef struct st_Seqence {
  musHandle handle;
  u8* data_ptr;
} NUAuSeqPlayer;

// Callback invoked on a PreNMI (reset-pending) scheduler event; receives the
// scheduler message type and the current frame counter.
typedef void (*NUAuPreNMIFunc)(NUScMsg, u32);

// Audio-manager state shared with the game.
extern u8 nuAuTaskStop;  // audio-task run state (NU_AU_TASK_STOP/RUN)
extern u8 nuAuPreNMI;    // set when a PreNMI reset is pending
extern NUAuPreNMIFunc nuAuPreNMIFunc;  // user PreNMI callback (NULL if none)
extern void* nuAuEffect_ptr;   // sound-effect bank buffer in the audio heap
extern void* nuAuPtrBank_ptr;  // sample pointer-bank buffer in the audio heap
extern NUScTask nuAuTask;      // audio task handed to the NuSYS scheduler
extern OSMesgQueue nuAuMesgQ;  // audio-manager message queue
extern NUAuSeqPlayer nuAuSeqPlayer[];  // the sequence players
extern musConfig nuAuStlConfig;  // default config used to init the manager
extern NUScClient nuAuClient;    // scheduler-client registration for audio

// Audio-manager lifecycle.

// Initialize the audio manager with the default configuration (nuAuStlConfig):
// set up both sequence players, the sound player, and the sample pointer bank,
// then register the default PreNMI handler. Returns the heap bytes used.
extern s32 nuAuStlInit(void);

// Initialize the audio manager from `config` and bring up the mus library;
// returns the heap size used.
extern s32 nuAuStlMgrInit(musConfig* config);

// Sample pointer bank (instrument-sample lookup table).

// Reserve a pbk_size-byte sample pointer-bank buffer from the audio heap.
extern void nuAuStlPtrBankInit(u32 pbk_size);

// Load the pointer bank at pbk_addr, associate it with the wave bank at
// wbk_addr, and register it with the mus library.
extern void nuAuStlPtrBankSet(u8* pbk_addr, u32 pbk_size, u8* wbk_addr);

// Sequence (music) player.

// Initialize sequence player player_no and reserve a size-byte song-data
// buffer for it from the audio heap.
extern void nuAuStlSeqPlayerInit(u32 player_no, u32 size);

// Load seq_size bytes of binary song data from seq_addr into player_no's
// buffer.
extern void nuAuStlSeqPlayerDataSet(u32 player_no, u8* seq_addr, u32 seq_size);

// Start playback on sequence player player_no; returns its mus handle for later
// tempo/volume/pan control.
extern musHandle nuAuStlSeqPlayerPlay(u32 player_no);

// Sound-effect player.

// Initialize the sound player and reserve a size-byte effect-bank buffer from
// the audio heap.
extern void nuAuStlSndPlayerInit(u32 size);

// Play sound number sndNo with default parameters; returns its mus handle.
extern u32 nuAuStlSndPlayerPlay(u32 sndNo);

// Play sound sndNo with explicit volume (0-0x100), pan (0-0x100), restart flag
// (nonzero overwrites an already-playing instance), and priority; returns its
// mus handle.
extern u32 nuAuStlSndPlayerPlay2(u32 sndNo, s32 volume, s32 pan,
                                 s32 restartflag, s32 priority);

// Load the binary effect bank at snd_addr (snd_size bytes) into the sound
// player and initialize it.
extern void nuAuStlSndPlayerDataSet(u8* snd_addr, u32 snd_size);

// PreNMI (reset) handling.

// Register func as the PreNMI callback (see nuAuPreNMIFuncRemove to clear it).
extern void nuAuPreNMIFuncSet(NUAuPreNMIFunc func);

// Default PreNMI handler: invoked with the scheduler message type and frame
// counter when a reset becomes pending.
extern void nuAuPreNMIProc(NUScMsg mesg_type, u32 frameCounter);

// Older same-named entry; the compatibility macro below shadows this name and
// forwards calls to nuAuStlSndPlayerDataSet.
extern void nuAuStlSndPlayerSetData(u8* snd_addr, u32 snd_size);

// Audio heap.

// Allocate length bytes from the audio heap; returns a pointer to the block.
extern void* nuAuStlHeapAlloc(s32 length);

// Return the number of free bytes remaining in the audio heap.
extern s32 nuAuStlHeapGetFree(void);

// Return the number of bytes currently used in the audio heap.
extern s32 nuAuStlHeapGetUsed(void);

// Convenience macros wrapping the mus library. MUSFLAG_SONGS targets all
// sequence (music) channels; MUSFLAG_EFFECTS targets all sound-effect channels.

// Clear the PreNMI callback registered with nuAuPreNMIFuncSet.
#define nuAuPreNMIFuncRemove() nuAuPreNMIFuncSet(NULL)

// Channel mask currently in use by the sequence players.
#define nuAuStlSeqPlayerGetState() MusAsk(MUSFLAG_SONGS)

// Stop all sequences, fading out over `speed` frames.
#define nuAuStlSeqPlayerStop(speed) MusStop(MUSFLAG_SONGS, speed)

// Set the master volume for all sequences (vol 0-0x7fff).
#define nuAuStlSeqPlayerSetMasterVol(vol) MusSetMasterVolume(MUSFLAG_SONGS, vol)

// Query the state of the sequence on player player_no.
#define nuAuStlSeqPlayerGetSeqState(player_no) \
  MusHandleAsk(nuAuSeqPlayer[player_no].handle)

// Stop the sequence on player player_no, fading over `speed` frames.
#define nuAuStlSeqPlayerSeqStop(player_no, speed) \
  MusHandleStop(nuAuSeqPlayer[player_no].handle, speed)

// Set the tempo of the sequence on player player_no (tempo 0-0x100).
#define nuAuStlSeqPlayerSetSeqTempo(player_no, tempo) \
  MusHandleSetTempo(nuAuSeqPlayer[player_no].handle, tempo)

// Set the volume of the sequence on player player_no (vol 0-0x100).
#define nuAuStlSeqPlayerSetSeqVol(player_no, vol) \
  MusHandleSetVolume(nuAuSeqPlayer[player_no].handle, vol)

// Set the pan of the sequence on player_no (0=left, 0x80=center, 0x100=right).
#define nuAuStlSeqPlayerSetSeqPan(player_no, pan) \
  MusHandleSetPan(nuAuSeqPlayer[player_no].handle, pan)

// Channel mask currently in use by the sound-effect player.
#define nuAuStlSndPlayerGetState() MusAsk(MUSFLAG_EFFECTS)

// Stop all sound effects, fading out over `speed` frames.
#define nuAuStlSndPlayerStop(speed) MusStop(MUSFLAG_EFFECTS, speed)

// Set the master volume for all sound effects (vol 0-0x7fff).
#define nuAuStlSndPlayerSetMasterVol(vol) \
  MusSetMasterVolume(MUSFLAG_EFFECTS, vol)

// Query the state of the sound playing on `handle`.
#define nuAuStlSndPlayerGetSndState(handle) MusHandleAsk(handle)

// Stop the sound on `handle`, fading over `speed` frames.
#define nuAuStlSndPlayerSndStop(handle, speed) MusHandleStop(handle, speed)

// Set the volume of the sound on `handle` (vol 0-0x100).
#define nuAuStlSndPlayerSetSndVol(handle, vol) MusHandleSetVolume(handle, vol)

// Set the pan of the sound on `handle` (0=left, 0x80=center, 0x100=right).
#define nuAuStlSndPlayerSetSndPan(handle, pan) MusHandleSetPan(handle, pan)

// Set the pitch (frequency offset) of the sound on `handle` (-6.0 to +6.0).
#define nuAuStlSndPlayerSetSndPitch(handle, pitch) \
  MusHandleSetFreqOffset(handle, pitch)

// Names kept for compatibility with the beta API; prefer the current entry
// points above.

// Reserve and load the sample pointer bank in one step.
#define nuAuStlBankSet(pbk_addr, pbk_size, wbk_addr) \
  {                                                  \
    nuAuStlPtrBankInit(pbk_size);                    \
    nuAuStlPtrBankSet(pbk_addr, pbk_size, wbk_addr); \
  }

// Old name for nuAuStlSeqPlayerDataSet.
#define nuAuStlSeqPlayerSetData(player_no, seq_addr, seq_size) \
  nuAuStlSeqPlayerDataSet(player_no, seq_addr, seq_size)

// Old name for nuAuStlSndPlayerDataSet (shadows the same-named extern above).
#define nuAuStlSndPlayerSetData(snd_addr, snd_size) \
  nuAuStlSndPlayerDataSet(snd_addr, snd_size)

// Obsolete no-op retained so old call sites still compile.
#define nuAuStlPlayerInit(c, size) ((void)0);
#endif  // _LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS
#ifdef _LANGUAGE_C_PLUS_PLUS
}
#endif
#endif  // _NUALSTL_H_
