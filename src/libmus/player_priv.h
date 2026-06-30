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

extern int g_mus_channel_count;
extern channel_t* g_mus_channel_array;
extern int g_mus_vsyncs_per_sec;
extern LIBMUScb_marker g_mus_marker_callback;
#define max_channels g_mus_channel_count
#define mus_channels g_mus_channel_array
#define mus_vsyncs_per_second g_mus_vsyncs_per_sec
#define marker_callback g_mus_marker_callback

extern int func_8009BC58(int);  // __MusIntRandom
extern musHandle allocate_object_slot(fx_header_t*, int, int, int,
                                      int);    // __MusIntFindChannelAndStart
extern int func_8009D8A8(s32);                 // ChangeCustomEffect
extern musBool g_mus_fx_enabled;               // mus_songfxchange_flag
extern void func_8009BFF0(void*, void*, int);  // __MusIntRemapPtrs
extern N_ALVoice* g_mus_voice_array;           // mus_voices
extern ALMicroTime g_mus_usec_per_frame;       // mus_next_frame_time
extern command_func_t D_800C76BC[];            // jumptable
extern void func_8009B360(channel_t*);         // __MusIntInitEnvelope
extern void func_8009B5C4(channel_t*);         // __MusIntInitSweep
extern void func_8009B060(channel_t*, int);    // __MusIntFlushPending
extern float func_8009B92C(float);             // __MusIntPowerOf2
#define mus_voices g_mus_voice_array
#define mus_next_frame_time g_mus_usec_per_frame
#define jumptable D_800C76BC
#define __MusIntInitEnvelope func_8009B360
#define __MusIntInitSweep func_8009B5C4
#define __MusIntFlushPending func_8009B060
#define __MusIntPowerOf2 func_8009B92C
#define __MusIntRandom func_8009BC58
#define __MusIntFindChannelAndStart allocate_object_slot
#define ChangeCustomEffect func_8009D8A8
#define mus_songfxchange_flag g_mus_fx_enabled
#define __MusIntRemapPtrs func_8009BFF0
extern unsigned short g_mus_master_vol_left;     // mus_master_volume_effects
extern unsigned short g_mus_master_vol_right;    // mus_master_volume_songs
extern musSched* __libmus_current_sched;         // @0x800C7ADC (symbol_addrs)
extern int mus_fifo_enqueue(fifo_t*);            // __MusIntFifoAddCommand
extern int g_mus_fifo_read_idx;                  // fifo_start
extern int g_mus_fifo_write_idx;                 // fifo_current
extern int g_mus_fifo_size;                      // fifo_limit
extern fifo_t* g_mus_fifo_base;                  // fifo_addr
extern void __MusIntMemMove(void*, void*, int);  // @0x8009E30C (symbol_addrs)
extern void mus_handle_set_flag(unsigned long, unsigned long,
                                unsigned long);  // __MusIntHandleSetFlag
#define mus_master_volume_effects g_mus_master_vol_left
#define mus_master_volume_songs g_mus_master_vol_right
#define __MusIntFifoAddCommand mus_fifo_enqueue
#define fifo_start g_mus_fifo_read_idx
#define fifo_current g_mus_fifo_write_idx
#define fifo_limit g_mus_fifo_size
#define fifo_addr g_mus_fifo_base
#define __MusIntHandleSetFlag mus_handle_set_flag
extern ptr_bank_t* g_mus_current_ptr_bank;     // mus_default_bank
extern int D_800E7074;                         // mus_last_fxtype
extern fx_header_t* D_800E7078;                // libmus_fxheader_current
extern void mus_remap_ptr_bank(char*, char*);  // __MusIntRemapPtrBank
extern fx_header_t* D_800E707C;                // libmus_fxheader_single
#define mus_default_bank g_mus_current_ptr_bank
#define mus_last_fxtype D_800E7074
#define libmus_fxheader_current D_800E7078
#define libmus_fxheader_single D_800E707C
#define __MusIntRemapPtrBank mus_remap_ptr_bank
extern channel_t* g_mus_sound_channel_base;    // mus_channels2
extern unsigned long g_mus_handle_counter;     // mus_current_handle
extern ptr_bank_t* g_mus_last_started_handle;  // mus_init_bank
extern int mus_alloc_channel(song_t*, int);    // __MusIntFindChannel
extern void init_struct_defaults(channel_t*);  // __MusIntInitialiseChannel
#define mus_channels2 g_mus_sound_channel_base
#define mus_current_handle g_mus_handle_counter
#define mus_init_bank g_mus_last_started_handle
#define __MusIntFindChannel mus_alloc_channel
#define __MusIntInitialiseChannel init_struct_defaults
extern long g_mus_rng_seed;  // mus_random_seed
#define mus_random_seed g_mus_rng_seed
extern int mus_cmd_start_song(musHandle);  // MusHandleUnPause
extern musHandle mus_start_song(void*);    // __MusIntStartSong
extern unsigned long func_8009C028(channel_t*, fx_header_t*, int, int, int,
                                   int);  // __MusIntStartEffect
extern void func_8009B754(channel_t*);    // __MusIntProcessContinuousVolume
extern void func_8009B818(channel_t*);    // __MusIntProcessContinuousPitchBend
#define MusHandleUnPause mus_cmd_start_song
#define __MusIntStartSong mus_start_song
#define __MusIntStartEffect func_8009C028
#define __MusIntProcessContinuousVolume func_8009B754
#define __MusIntProcessContinuousPitchBend func_8009B818
extern int g_mus_pan_enabled;     // @0x800C76B8 (MG64-added pan-enable flag)
extern int g_mus_frame_counter;   // @0x800C7770
extern void func_8009DBA0(void);  // empty routine (aud_dma.c)

#define REST 96
#define BASEOFFSET 48
#define U8_TO_FLOAT(c) ((c) & 128) ? -(256 - (c)) : (c)
#define OFFSETTOPOINTER(base, offset) ((u32)(base) + (u32)(offset))

#endif  // PLAYER_PRIV_H
