#ifndef PLAYER_PRIV_H
#define PLAYER_PRIV_H

// Shared preamble for the game-embedded libmus sequence player, split into its
// original translation units: player.c (the internal __MusInt* engine + the FX/
// envelope command Fenvelope + the inlined-into-it API & fifo helpers) and
// player_commands.c (the bytecode command-handler jumptable bodies). The split
// is load-bearing for matching: at the game's -O3, GCC inlines small static
// callees only WITHIN a TU, so e.g. __MusIntRandom (player.c) is kept
// out-of-line because its only callers are the rand command handlers in the
// separate player_commands.c TU (cross-TU -> jal). Both TUs reference the same
// curated ghidra symbols via the aliases below.
#include "include_asm.h"
#include "libmus_config.h"
#include <ultra64.h>
#ifndef SUPPORT_NAUDIO
#include <libaudio.h>
#else
#include <n_libaudio_sc.h>
#include <n_libaudio_sn_sc.h>
#endif
#include "libmus.h"
#include "lib_memory.h"
#include "aud_sched.h"
#include "aud_thread.h"
#include "player.h"
#include "player_fifo.h"
#ifdef SUPPORT_FXCHANGE
#include "player_fx.h"
#endif

extern int max_channels;
extern channel_t* mus_channels;
extern int mus_vsyncs_per_second;
extern LIBMUScb_marker marker_callback;

extern int __MusIntRandom(int);  // __MusIntRandom
extern musHandle __MusIntFindChannelAndStart(
    fx_header_t*, int, int, int,
    int);                            // __MusIntFindChannelAndStart
extern int ChangeCustomEffect(s32);  // ChangeCustomEffect
extern musBool mus_songfxchange_flag;
extern void __MusIntRemapPtrs(void*, void*, int);  // __MusIntRemapPtrs
extern N_ALVoice* mus_voices;
extern ALMicroTime mus_next_frame_time;
extern command_func_t jumptable[];
extern void __MusIntInitEnvelope(channel_t*);       // __MusIntInitEnvelope
extern void __MusIntInitSweep(channel_t*);          // __MusIntInitSweep
extern void __MusIntFlushPending(channel_t*, int);  // __MusIntFlushPending
extern float __MusIntPowerOf2(float);               // __MusIntPowerOf2
extern unsigned short mus_master_volume_effects;
extern unsigned short mus_master_volume_songs;
extern musSched* __libmus_current_sched;     // @0x800C7ADC (symbol_addrs)
extern int __MusIntFifoAddCommand(fifo_t*);  // __MusIntFifoAddCommand
extern int fifo_start;
extern int fifo_current;
extern int fifo_limit;
extern fifo_t* fifo_addr;
extern void __MusIntMemMove(void*, void*, int);  // @0x8009E30C (symbol_addrs)
extern void __MusIntHandleSetFlag(unsigned long, unsigned long,
                                  unsigned long);  // __MusIntHandleSetFlag
extern ptr_bank_t* mus_default_bank;
extern int mus_last_fxtype;
extern fx_header_t* libmus_fxheader_current;
extern void __MusIntRemapPtrBank(char*, char*);  // __MusIntRemapPtrBank
extern fx_header_t* libmus_fxheader_single;
extern channel_t* mus_channels2;
extern unsigned long mus_current_handle;
extern ptr_bank_t* mus_init_bank;
extern int __MusIntFindChannel(song_t*, int);       // __MusIntFindChannel
extern void __MusIntInitialiseChannel(channel_t*);  // __MusIntInitialiseChannel
extern long mus_random_seed;
extern int MusHandleUnPause(musHandle);     // MusHandleUnPause
extern musHandle __MusIntStartSong(void*);  // __MusIntStartSong
extern unsigned long __MusIntStartEffect(channel_t*, fx_header_t*, int, int,
                                         int,
                                         int);  // __MusIntStartEffect
extern void __MusIntProcessContinuousVolume(
    channel_t*);  // __MusIntProcessContinuousVolume
extern void __MusIntProcessContinuousPitchBend(
    channel_t*);                  // __MusIntProcessContinuousPitchBend
extern int g_mus_pan_enabled;     // @0x800C76B8 (MG64-added pan-enable flag)
extern int g_mus_frame_counter;   // @0x800C7770
extern void func_8009DBA0(void);  // empty routine (aud_dma.c)

#define REST 96
#define BASEOFFSET 48
#define U8_TO_FLOAT(c) ((c) & 128) ? -(256 - (c)) : (c)
#define OFFSETTOPOINTER(base, offset) ((u32)(base) + (u32)(offset))

#endif  // PLAYER_PRIV_H
