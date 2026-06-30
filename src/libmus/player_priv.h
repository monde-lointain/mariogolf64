/*
 * player_priv.h
 *
 * Private interface shared by the libmus sequence player's two translation
 * units, player.c (the engine and the public Mus* API) and player_commands.c
 * (the Fxxx sequence-command handlers). It pulls in the player's own struct
 * definitions and then declares every symbol the two files share: the channel
 * pool, the deferred-command FIFO, the active banks, and the internal
 * __MusInt* engine routines. Nothing here is part of the public API; each
 * extern resolves to a definition that lives in one of the two TUs.
 */
#ifndef PLAYER_PRIV_H
#define PLAYER_PRIV_H

/* INCLUDE_ASM macro: raw-asm fallback for any not-yet-decompiled function. */
#include "include_asm.h"

/* Build-feature switches (SUPPORT_FXCHANGE, ...) consulted below. */
#include "libmus_config.h"
#include <ultra64.h>

/*
 * Synthesizer driver headers. SUPPORT_NAUDIO selects the "n audio" sequence-
 * player synth (the n_al* SC variant this build ships) instead of stock
 * libaudio; the choice changes the voice/synth types named below (N_ALVoice,
 * etc.).
 */
#ifndef SUPPORT_NAUDIO
#include <libaudio.h>
#else
#include <n_libaudio_sc.h>
#include <n_libaudio_sn_sc.h>
#endif

/* Public API plus shared libmus types (musHandle, ptr_bank_t, musSched). */
#include "libmus.h"

/* Internal subsystems the player calls into. */
#include "lib_memory.h"
#include "aud_sched.h"
#include "aud_thread.h"

/* Player-local types: channel_t, song_t, fx_header_t, the FIFO record. */
#include "player.h"
#include "player_fifo.h"

#ifdef SUPPORT_FXCHANGE
#include "player_fx.h" /* runtime reverb/effect-type switching */
#endif

/* Total allocated channel slots: config->channels voices plus the MAX_SONGS
 * master-track slots reserved at the front of mus_channels. */
extern int max_channels;

/* Base of the channel array (max_channels entries). Slots [0, MAX_SONGS) are
 * song master tracks; mus_channels2 points past them at the effect channels. */
extern channel_t* mus_channels;

/* Video field rate, 50 (PAL) or 60 (NTSC); scales tempo and frame timing. */
extern int mus_vsyncs_per_second;

/* User callback fired by the Fmarker command: marker_callback(handle, number).
 * NULL when no callback is installed. */
extern LIBMUScb_marker marker_callback;

/* Pseudo-random int in [0, range) from the mus_random_seed LFSR. */
extern int __MusIntRandom(int range);

/* Allocate an effect channel (stealing the lowest-priority busy one if none is
 * free) and start effect `number` from the given fx bank with volume/pan/
 * priority; returns its handle, or 0 if no channel could be claimed. */
extern musHandle __MusIntFindChannelAndStart(fx_header_t* header, int number,
                                             int volume, int pan, int priority);

/* Switch the active custom effect (reverb) preset to the given fx type. */
extern int ChangeCustomEffect(s32);

/* When set, a song's embedded Fchangefx command is honored; cleared by
 * MusSetSongFxChange to lock the effect type. */
extern musBool mus_songfxchange_flag;

/* Relocate an array of `count` pointer-sized words at the first arg in place,
 * adding the second arg to each non-null entry (file-relative -> absolute). */
extern void __MusIntRemapPtrs(void* addr, void* offset, int count);

/* Synth voice array: one N_ALVoice per effect channel (max_channels minus the
 * MAX_SONGS master slots), indexed by a channel's voice index. */
extern N_ALVoice* mus_voices;

/* Microseconds between audio frames (1000000 / mus_vsyncs_per_second); the
 * player handler returns it to schedule its next call. */
extern ALMicroTime mus_next_frame_time;

/* Command dispatch table indexed by (opcode & 0x7f); each entry is the Fxxx
 * handler that consumes one sequence command's bytes. */
extern command_func_t jumptable[];

/* Arm the ADSR envelope at note start. */
extern void __MusIntInitEnvelope(channel_t* cp);

/* Arm the pan sweep at note start. */
extern void __MusIntInitSweep(channel_t* cp);

/* Start the channel's queued (pending) wave on voice x (2nd arg), stopping any
 * wave already playing there. */
extern void __MusIntFlushPending(channel_t* cp, int x);

/* 2^x via a 7-term polynomial; maps a semitone offset to a freq ratio. */
extern float __MusIntPowerOf2(float x);

/* Master volume scalers (0..0x7fff), multiplied into every voice. */
extern unsigned short mus_master_volume_effects; /* all sound effects */
extern unsigned short mus_master_volume_songs;   /* all songs */

/* Active scheduler vtable (owned by aud_sched); MusSetScheduler swaps it. */
extern musSched* __libmus_current_sched;

/* Deferred-command FIFO. The public Mus* calls enqueue here; the player drains
 * the ring once per frame, so a command takes effect between frame updates
 * rather than mid-update. */
extern int __MusIntFifoAddCommand(fifo_t* command); /* enqueue; 0 if full */
extern int fifo_start;    /* read cursor: next command to run */
extern int fifo_current;  /* write cursor: next free slot */
extern int fifo_limit;    /* ring capacity in fifo_t slots */
extern fifo_t* fifo_addr; /* ring buffer base */

/* memmove (definition shared with lib_memory.h). */
extern void __MusIntMemMove(void*, void*, int);

/* For every channel matching `handle`, set channel_flag = (flag & clear) | set;
 * the FIFO uses it to pause/unpause. Args: handle, clear-mask, set-mask. */
extern void __MusIntHandleSetFlag(unsigned long handle, unsigned long clear,
                                  unsigned long set);

/* Current default sample/pointer bank; a new channel uses it unless mus_init_
 * bank overrides. */
extern ptr_bank_t* mus_default_bank;

/* Most recently selected effect (reverb) type; defaults to AL_FX_BIGROOM. */
extern int mus_last_fxtype;

/* Current (sticky) effect bank header that effects start from. */
extern fx_header_t* libmus_fxheader_current;

/* One-time relocation of a pointer (sample) bank and its wave data: fixes the
 * wave_list pointers, detune/base-note, and wave base addresses (guarded by
 * PTRFLAG_REMAPPED). Args: pointer-bank file, wave-bank file. */
extern void __MusIntRemapPtrBank(char* pptr, const char* wptr);

/* Single-shot effect bank: overrides libmus_fxheader_current for the next
 * effect, then clears itself. */
extern fx_header_t* libmus_fxheader_single;

/* mus_channels + MAX_SONGS: the first non-master (effect) channel. */
extern channel_t* mus_channels2;

/* Next handle to hand out; bumped on each allocation. Handle 0 means "none". */
extern unsigned long mus_current_handle;

/* One-shot bank applied to the next channel(s) initialised, then reset to NULL;
 * overrides mus_default_bank for that init. */
extern ptr_bank_t* mus_init_bank;

/* Choose a channel slot for a song channel (song_chan < 0 selects a master
 * slot), voice-stealing if needed; returns the slot index. */
extern int __MusIntFindChannel(song_t* addr, int song_chan);

/* Zero a channel and load its default state (volumes, envelope, bank). */
extern void __MusIntInitialiseChannel(channel_t* cp);

/* LFSR state for __MusIntRandom; seeded with 0x12345678. */
extern long mus_random_seed;

/* Resume a paused handle (public Mus API; enqueues an unpause via the FIFO). */
extern int MusHandleUnPause(musHandle handle);

/* Load and relocate a song (one-time pointer fixups), allocate its channels,
 * and return a new (paused) handle. */
extern musHandle __MusIntStartSong(void* addr);

/* (Re)start effect `number` from a header on the already-chosen channel with
 * the given volume/pan/priority; returns its handle. Args: channel, header,
 * number, volume, pan, priority. */
extern unsigned long __MusIntStartEffect(channel_t* cp, fx_header_t* header,
                                         int number, int volume, int pan,
                                         int priority);

/* Advance a channel's continuous (per-frame) controller streams embedded in the
 * sequence data: the volume envelope and the pitch-bend envelope. */
extern void __MusIntProcessContinuousVolume(channel_t* cp);
extern void __MusIntProcessContinuousPitchBend(channel_t* cp);

/* MG64-specific additions (not in stock libmus). */
extern int g_mus_pan_enabled;   /* 0 forces center pan (0x40); 1 applies pan */
extern int g_mus_frame_counter; /* bumped each __MusIntMain frame */
/* MG64-specific; called from func_8009A630, purpose not yet identified. */
extern void func_8009DBA0(void);

/* Sequence note value reserved to mean a rest (no sound this note). */
#define REST 96

/* Bias on a wave's stored base note; subtract it to recover the signed semitone
 * offset (see __MusIntRemapPtrBank). */
#define BASEOFFSET 48

/* Reinterpret byte `c` as a signed 8-bit value: high bit set -> c - 256, else
 * c. Recovers signed transpose/detune from an unsigned byte. Caveat: no outer
 * parentheses, so it is only safe in the cast contexts the callers use. */
#define U8_TO_FLOAT(c) ((c) & 128) ? -(256 - (c)) : (c)

/* Resolve a file-relative byte `offset` against `base` into an absolute
 * address; used to fix up bank pointers after a bank is loaded. */
#define OFFSETTOPOINTER(base, offset) ((u32)(base) + (u32)(offset))

#endif
