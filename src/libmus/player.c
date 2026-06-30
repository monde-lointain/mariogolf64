#include "include_asm.h"

// Game-embedded libmus 3.14 sequence player (player.c TU: player_api +
// player_fifo
// + player_commands, one #include-chained translation unit). Banked
// incrementally as a mixed game-region carve: stock fns as C, game-modified fns
// kept as INCLUDE_ASM.
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

// player.c file-scope statics, placed in BSS and referenced via their curated
// ghidra names while this TU is a partial mixed carve. #define-aliased to the
// upstream names so banked bodies stay verbatim (these become file-scope defs
// once the full TU banks).
extern int g_mus_channel_count;
extern channel_t* g_mus_channel_array;
extern int g_mus_vsyncs_per_sec;
extern LIBMUScb_marker g_mus_marker_callback;
#define max_channels g_mus_channel_count
#define mus_channels g_mus_channel_array
#define mus_vsyncs_per_second g_mus_vsyncs_per_sec
#define marker_callback g_mus_marker_callback

// player.c internal callees (still INCLUDE_ASM); aliased to their curated
// ghidra symbols so banked bodies call the right address verbatim.
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
float func_8009B92C(float);  // __MusIntPowerOf2 (defined below)
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
extern ptr_bank_t* g_mus_current_ptr_bank;  // mus_default_bank
extern int D_800E7074;                      // mus_last_fxtype
extern fx_header_t* D_800E7078;             // libmus_fxheader_current
void mus_remap_ptr_bank(char*, char*);  // __MusIntRemapPtrBank (defined below)
extern fx_header_t* D_800E707C;         // libmus_fxheader_single
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

// player.c file-scope macros (verbatim).
#define REST 96
#define BASEOFFSET 48
#define U8_TO_FLOAT(c) ((c) & 128) ? -(256 - (c)) : (c)
#define OFFSETTOPOINTER(base, offset) ((u32)(base) + (u32)(offset))

// Default-ADSR setup, extracted as a static helper so GCC -O3 auto-inlines it
// into mus_cmd_envelope + func_8009AB18 exactly as the upstream inlines Fdefa
// (defining it before its callers is what lets the inline reproduce; the
// standalone Fdefa lives in mus_cmd_default_adsr for the command jumptable).
static void env_set_adsr(channel_t* cp, unsigned char* ptr) {
  unsigned char value;

  value = *ptr++;
  if (value == 0) value = 1;
  cp->env_speed = value;
  cp->env_speed_calc = 1024 / value;
  cp->env_init_vol = *ptr++;
  value = *ptr++;
  cp->env_attack_speed = value;
  cp->env_max_vol = *ptr++;
  cp->env_attack_calc =
      (1.0 / ((float)value)) * ((float)(cp->env_max_vol - cp->env_init_vol));
  value = *ptr++;
  cp->env_decay_speed = value;
  cp->env_sustain_vol = *ptr++;
  cp->env_decay_calc =
      (1.0 / ((float)value)) * ((float)(cp->env_sustain_vol - cp->env_max_vol));
  value = *ptr++;
  cp->env_release_speed = value;
  cp->env_release_calc = 1.0 / ((float)value);
}

unsigned char* mus_cmd_envelope(channel_t* cp, unsigned char* ptr) {
  int tmp;

  tmp = *ptr++;
  if (tmp & 0x80) {
    tmp &= 0x7f;
    tmp <<= 8;
    tmp |= *ptr++;
  }
  env_set_adsr(cp, &cp->song_addr->env_table[tmp * 7]);
  return (ptr);
}

INCLUDE_ASM("asm/nonmatchings/libmus/player", MusInitialize);

// MusSetMasterVolume
void mus_set_master_volume(unsigned long flags, int volume) {
  if (flags & MUSFLAG_EFFECTS) mus_master_volume_effects = volume;
  if (flags & MUSFLAG_SONGS) mus_master_volume_songs = volume;
}

// MusStartSong
musHandle MusStartSong(void* addr) {
  musHandle handle;

  handle = __MusIntStartSong(addr);
  MusHandleUnPause(handle);
  return (handle);
}

// MusStartSongFromMarker
musHandle mus_start_song_from_marker(void* addr, int marker) {
  musHandle handle;
  unsigned char command, *ptr;
  int i, note;
  channel_t* cp;

  handle = __MusIntStartSong(addr);

  /* skip to correct marker */
  for (i = 0, cp = mus_channels; i < max_channels; i++, cp++) {
    if (cp->handle == handle && cp->song_addr == (song_t*)addr && (cp->pdata)) {
      /* skip to marker */
      while (cp->pdata) {
        /* commands must be processed */
        ptr = cp->pdata;
        if (*ptr >= 128) {
          if (*ptr == Cmarker && *(ptr + 1) == marker) break;

          command = *ptr++;
          cp->pdata = (jumptable[command & 0x7f].func)(cp, ptr);
          continue;
        }
        note = *(cp->pdata++);
        /* velocity must be processed (MG64 game-divergent block, matches
           func_8009AB18: velocity assigned via temp, masked into velocity not
           vel, default reloaded) */
        if (cp->velocity_on) {
          unsigned char vel = *(cp->pdata++);
          cp->velocity = vel;
          if (vel >= 0x80) {
            cp->velocity = vel & 0x7f;
            cp->velocity_on = 0;
            cp->default_velocity = cp->velocity;
          }
        } else {
          cp->velocity = cp->default_velocity;
        }
        /* get length for continuous data update */
        if (cp->fixed_length && !cp->ignore) {
          cp->length = cp->fixed_length;
        } else {
          cp->ignore = 0;
          command = *(cp->pdata++);
          if (command < 128)
            cp->length = command;
          else
            cp->length = ((int)(command & 0x7f) << 8) + *(cp->pdata++);
        }
        cp->channel_frame += cp->length * 256;
      }

      cp->note_end_frame = cp->channel_frame;
      if (cp->pdata) {
        /* get marker delay vaule */
        ptr = cp->pdata + 2;
        note = *ptr++;
        if (note >= 0x80) {
          note &= 0x7f;
          note <<= 8;
          note |= *ptr++;
        }
        cp->channel_frame -= note * 256;
        cp->count = 0;
        cp->length = note;
        cp->pdata = ptr;
      }
      cp->note_start_frame = cp->channel_frame;
      /* advance through volume data */
      if (cp->pvolume) __MusIntProcessContinuousVolume(cp);
      /* advance through pitchbend data */
      if (cp->ppitchbend) __MusIntProcessContinuousPitchBend(cp);
    }
  }
  MusHandleUnPause(handle);
  return (handle);
}

INCLUDE_ASM("asm/nonmatchings/libmus/player", try_spawn_global_object);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_80099CF0);

// MusStop
void mus_stop(unsigned long flags, int speed) {
  int i, speed2;
  channel_t* cp;

  speed2 = speed ? speed : 1;
  for (i = 0, cp = mus_channels; i < max_channels; i++, cp++) {
    if ((cp->fx_addr && flags & MUSFLAG_EFFECTS) ||
        (!cp->fx_addr && flags & MUSFLAG_SONGS)) {
      if (cp->pdata && cp->stopping == -1) {
        if (cp->channel_flag & CHFLAG_PAUSE) {
          cp->stopping_speed = 1;
          cp->stopping = 0;
          cp->channel_flag &= ~CHFLAG_PAUSE;
        } else {
          cp->stopping_speed = speed2;
          cp->stopping = speed;
        }
      }
    }
  }
}

// MusAsk
int mus_ask(unsigned long flags) {
  int i, count;
  channel_t* cp;

  for (i = 0, cp = mus_channels, count = 0; i < max_channels; i++, cp++) {
    if (cp->pdata) {
      if ((cp->fx_addr && flags & MUSFLAG_EFFECTS) ||
          (!cp->fx_addr && flags & MUSFLAG_SONGS))
        count++;
    }
  }
  return (count);
}

// MusHandleStop
int mus_handle_stop(musHandle handle, int speed) {
  int i, speed2, count;
  channel_t* cp;

  if (!handle) return (0);

  speed2 = speed ? speed : 1;
  for (i = 0, cp = mus_channels, count = 0; i < max_channels; i++, cp++) {
    if (cp->handle == handle && cp->stopping == -1) {
      if (cp->channel_flag & CHFLAG_PAUSE) {
        cp->stopping_speed = 1;
        cp->stopping = 0;
        cp->channel_flag &= ~CHFLAG_PAUSE;
      } else {
        cp->stopping_speed = speed2;
        cp->stopping = speed;
      }
      count++;
    }
  }
  return (count);
}

// MusHandleAsk
int mus_handle_ask(musHandle handle) {
  channel_t* cp;
  int i, count;

  if (!handle) return (0);
  for (i = 0, cp = mus_channels, count = 0; i < max_channels; i++, cp++)
    if (cp->handle == handle) count++;
  return (count);
}

// MusHandleSetVolume
int mus_handle_set_volume(musHandle handle, int volume) {
  channel_t* cp;
  int i, count;

  if (!handle) return (0);
  for (i = 0, cp = mus_channels, count = 0; i < max_channels; i++, cp++) {
    if (cp->handle == handle) {
      cp->volscale = volume;
      count++;
    }
  }
  return (count);
}

// MusHandleSetPan
int mus_handle_set_pan(musHandle handle, int pan) {
  channel_t* cp;
  int i, count;

  if (!handle) return (0);
  for (i = 0, cp = mus_channels, count = 0; i < max_channels; i++, cp++) {
    if (cp->handle == handle) {
      cp->panscale = pan;
      cp->old_pan = 0xff;
      count++;
    }
  }
  return (count);
}

// MusHandleSetFreqOffset
int mus_handle_set_freq_offset(musHandle handle, float offset) {
  channel_t* cp;
  int i, count;

  if (!handle) return (0);
  for (i = 0, cp = mus_channels, count = 0; i < max_channels; i++, cp++) {
    if (cp->handle == handle) {
      cp->freqoffset = offset + cp->distort;
      count++;
    }
  }
  return (count);
}

// MusHandleSetTempo
int mus_handle_set_tempo(musHandle handle, int tempo) {
  channel_t* cp;
  int i, count;

  if (!handle) return (0);

  if (tempo < 1)
    tempo = 1;
  else if (tempo > 256)
    tempo = 256;

  for (i = 0, cp = mus_channels, count = 0; i < max_channels; i++, cp++) {
    if (cp->handle == handle) {
      cp->temscale = tempo;
      cp->channel_tempo = (cp->channel_tempo_save * tempo) >> 7;
      count++;
    }
  }
  return (count);
}

// MusHandleSetReverb
int mus_handle_set_reverb(musHandle handle, int reverb) {
  channel_t* cp;
  int i, count;

  if (!handle) return (0);

  if (reverb < 0)
    reverb = 0;
  else if (reverb > 127)
    reverb = 127;

  for (i = 0, cp = mus_channels, count = 0; i < max_channels; i++, cp++) {
    if (cp->handle == handle) {
      cp->reverb_base = reverb;
      cp->old_reverb = 0xff;
      count++;
    }
  }
  return (count);
}

// MusPtrBankInitialize
void mus_ptr_bank_initialize(void* pbank, void* wbank) {
  __MusIntRemapPtrBank(pbank, wbank);
  if (!mus_default_bank) mus_default_bank = pbank;
}

// MusPtrBankSetSingle
void* func_8009A2F0(void* ipbank) {
  ptr_bank_t* pptr;
  if (ipbank) {
    pptr = (ptr_bank_t*)ipbank;
    if (pptr->flags & PTRFLAG_REMAPPED) mus_init_bank = pptr;
  }
  return (mus_init_bank);
}

// MusPtrBankSetCurrent
void func_8009A318(void* ipbank) {
  ptr_bank_t* pptr;
  if (ipbank) {
    pptr = (ptr_bank_t*)ipbank;
    if (pptr->flags & PTRFLAG_REMAPPED) mus_default_bank = pptr;
  }
}

// MusPtrBankGetCurrent
void* func_8009A33C(void) { return (mus_default_bank); }

// MusHandleGetPtrBank
void* mus_handle_get_ptr_bank(musHandle handle) {
  channel_t* cp;
  int i, count;

  if (!handle) return (NULL);
  for (i = 0, cp = mus_channels, count = 0; i < max_channels; i++, cp++)
    if (cp->handle == handle) return (cp->sample_bank);
  return (NULL);
}

// MusHandlePause
int mus_handle_pause(musHandle handle) {
  fifo_t fifo_command;

  fifo_command.command = FIFOCMD_PAUSE;
  fifo_command.data = handle;
  return (__MusIntFifoAddCommand(&fifo_command));
}

// MusHandleUnPause
int mus_cmd_start_song(musHandle handle) {
  fifo_t fifo_command;

  fifo_command.command = FIFOCMD_UNPAUSE;
  fifo_command.data = handle;
  return (__MusIntFifoAddCommand(&fifo_command));
}

// MusSetFxType
int mus_set_fx_type(int fxtype) {
  fifo_t fifo_command;

  fifo_command.command = FIFOCMD_CHANGEFX;
  fifo_command.data = fxtype;
  mus_last_fxtype = fxtype;
  return (__MusIntFifoAddCommand(&fifo_command));
}

// MusSetSongFxChange
int mus_set_song_fx_change(musBool onoff) {
  int changed;

  changed = 1;
  if (onoff == MUSBOOL_OFF) changed = mus_set_fx_type(mus_last_fxtype);
  if (changed) mus_songfxchange_flag = onoff;
  return (changed);
}

// MusFxBankInitialize
void mus_fx_bank_initialize(void* fxbank) {
  int i;
  fx_header_t* header;

  header = (fx_header_t*)fxbank;
  if (header->flags & FXFLAG_INITIALISED) {
    return;
  }
  if (!libmus_fxheader_current) libmus_fxheader_current = header;
  header->flags = FXFLAG_INITIALISED;
  header->ptr_addr = NULL;
  header->wave_table =
      (unsigned short*)OFFSETTOPOINTER(fxbank, header->wave_table);
  for (i = 0; i < header->number_of_components; i++)
    header->effects[i].fxdata =
        (unsigned char*)OFFSETTOPOINTER(fxbank, header->effects[i].fxdata);
}

// MusFxBankNumberOfEffects
int func_8009A500(void* ifxbank) {
  return (((fx_header_t*)ifxbank)->number_of_effects);
}

// MusFxBankSetCurrent
void func_8009A508(void* ifxbank) {
  libmus_fxheader_current = (fx_header_t*)ifxbank;
}

// MusFxBankSetSingle
void func_8009A514(void* ifxbank) {
  libmus_fxheader_single = (fx_header_t*)ifxbank;
}

// MusFxBankGetCurrent
void* func_8009A520(void) { return (libmus_fxheader_current); }

// MusFxBankSetPtrBank
void func_8009A52C(void* ifxbank, void* ipbank) {
  ((fx_header_t*)ifxbank)->ptr_addr = (ptr_bank_t*)ipbank;
}

// MusFxBankGetPtrBank
void* func_8009A534(void* ifxbank) {
  return (((fx_header_t*)ifxbank)->ptr_addr);
}

// MusSetScheduler
void MusSetScheduler(musSched* sched_list) {
  __libmus_current_sched = sched_list;
}

// MusHandleWaveCount
int mus_handle_wave_count(musHandle handle) {
  channel_t* cp;
  int i;

  if (!handle) return (0);
  for (i = 0, cp = mus_channels; i < max_channels; i++, cp++) {
    if (cp->handle == handle) {
      if (cp->song_addr)
        return (cp->song_addr->num_waves);
      else
        return (cp->fx_addr->num_waves);
    }
  }
  return (0);
}

// MusHandleWaveAddress
unsigned short* mus_handle_wave_address(musHandle handle) {
  channel_t* cp;
  int i;

  if (!handle) return (NULL);
  for (i = 0, cp = mus_channels; i < max_channels; i++, cp++) {
    if (cp->handle == handle) {
      if (cp->song_addr)
        return (cp->song_addr->wave_table);
      else
        return (cp->fx_addr->wave_table);
    }
  }
  return (NULL);
}

// MusSetMarkerCallback
void func_8009A624(void* callback) {
  marker_callback = (LIBMUScb_marker)callback;
}

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A630);

// __MusIntFifoOpen
void func_8009A64C(int commands) {
  if (commands < MIN_FIFO_COMMANDS) {
    commands = MIN_FIFO_COMMANDS;
  } else if (commands > MAX_FIFO_COMMANDS) {
    commands = MAX_FIFO_COMMANDS;
  }
  fifo_addr = __MusIntMemMalloc(commands * sizeof(fifo_t));
  fifo_limit = commands;
  fifo_start = fifo_current = 0;
}

// __MusIntFifoProcessCommand
void mus_fifo_dispatch(fifo_t* command) {
  switch (command->command) {
    case FIFOCMD_PAUSE:
      __MusIntHandleSetFlag(command->data, ~CHFLAG_PAUSE, CHFLAG_PAUSE);
      break;
    case FIFOCMD_UNPAUSE:
      __MusIntHandleSetFlag(command->data, ~CHFLAG_PAUSE, 0);
      break;
    case FIFOCMD_CHANGEFX:
#ifdef SUPPORT_FXCHANGE
      ChangeCustomEffect(command->data);
#endif
      break;
  }
}

// __MusIntFifoAddCommand
int mus_fifo_enqueue(fifo_t* command) {
  int index;

  index = (fifo_current + 1) % fifo_limit;
  if (index == fifo_start) {
    return (0);
  }
  __MusIntMemMove(&fifo_addr[fifo_current], command, sizeof(fifo_t));
  fifo_current = index;
  return (1);
}

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A7C8);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_player_frame_handler);

// __MusIntGetNewNote: advance past commands (jumptable dispatch), then fetch
// the next note + length + velocity, set up the wave/drum sample, envelope and
// reverb.
void func_8009AB18(channel_t* cp, int x) {
  unsigned char* ptr;
  unsigned char command;

  ptr = cp->pdata;
  while (ptr && (command = *ptr) > 127) {
#ifdef _AUDIODEBUG
    if (command >= Clast) {
      osSyncPrintf("PLAYER.C: Channel %d is corrupt (command=%02x)\n", x,
                   command);
      cp->pdata = NULL;
      break;
    }
#endif
    /* Execute the relevant code for the token.  */
    ptr = (jumptable[command & 0x7f].func)(cp, ptr + 1);
  }
  cp->pdata = ptr;

  /* new note */
  if (ptr) {
    int note;

    cp->last_note = cp->port_base;
    note = *(cp->pdata++);

    if (cp->velocity_on) {
      u8 vel = *cp->pdata++;
      /* NOTE: MG64's libmus predates the 98.12.15 "rests don't store velocity"
         change, so velocity is assigned unconditionally (no note!=REST guard).
       */
      cp->velocity = vel;
      if (vel >= 0x80) {
        cp->velocity = vel & 0x7F;
        cp->velocity_on = 0;
        cp->default_velocity = cp->velocity;
      }
    } else
      cp->velocity = cp->default_velocity;

    if (cp->fixed_length) {
      if (!cp->ignore)
        cp->length = cp->fixed_length;
      else {
        cp->ignore = 0;
        command = *(cp->pdata++);
        if (command < 128)
          cp->length = command;
        else
          cp->length = ((int)(command & 0x7f) << 8) + *(cp->pdata++);
      }
    } else {
      command = *(cp->pdata++);
      if (command < 128)
        cp->length = command;
      else
        cp->length = ((int)(command & 0x7f) << 8) + *(cp->pdata++);
    }

    /* set length and timer */
    cp->note_start_frame = cp->note_end_frame;
    cp->note_end_frame += cp->length * 256;
    cp->count = 0;
    /* initialise wobble */
    cp->wobble_count = cp->wobble_off_speed;
    cp->wobble_current = 0;

    /* check to see if wave is valid */
    if (cp->song_addr && !cp->pdrums) {
      if (cp->song_addr->wave_table[cp->wave] == 0xffff) note = REST;
    }

    if (note != REST) {
      int wave;
      ptr_bank_t* bank;

      /* get current sample bank */
      bank = cp->sample_bank;

      /* check for drums */
      if (cp->pdrums != NULL) {
        cp->wave = cp->pdrums[note].wave;
        cp->pan = cp->pdrums[note].pan / 2;
        env_set_adsr(cp, &cp->song_addr->env_table[cp->pdrums[note].adsr * 7]);
        note = cp->pdrums[note].pitch;
      }

      /* initialise envelope */
      if (!cp->env_trigger_off) __MusIntInitEnvelope(cp);

      /* initialise sweep */
      if (cp->sweep_speed) __MusIntInitSweep(cp);

      /* get current wave number */
      wave = cp->wave;

      if (cp->song_addr)
        wave = cp->song_addr->wave_table[wave];
      else
        wave = cp->fx_addr->wave_table[wave];

      /* start relevant sample if required */
      if (!cp->trigger_off) {
        ALWaveTable* wave_addr;

        wave_addr = bank->wave_list[wave];
        cp->pending = wave_addr;
        if (cp->playing && cp->old_volume) {
          cp->old_volume = 0;
          alSynSetVol(&__libmus_alglobals.drvr, mus_voices + x, 0,
                      mus_next_frame_time);
        } else
          __MusIntFlushPending(cp, x);
      }
      cp->base_note = (float)note + bank->detune[wave];
      command = cp->transpose * (1 - cp->ignore_transpose);
      cp->base_note += (float)U8_TO_FLOAT(command);

      /* set reverb level if required */
      if (cp->reverb != cp->old_reverb) {
        unsigned char work;

        work = cp->reverb_base;
        work += ((128 - work) * cp->reverb) >> 7;
        cp->old_reverb = cp->reverb;
        alSynSetFXMix(&__libmus_alglobals.drvr, mus_voices + x, work);
      }
    } else {
      /* rest allows previous notes release to finish */
      if (cp->env_phase < 4) {
        cp->env_phase = 4; /* Start Release */
        cp->release_frame = cp->channel_frame;
        cp->env_count = 1;
        cp->release_start_vol = cp->env_current;
      }
    }
  } else /* must have hit a Cstop so stop its voice */
  {
    if (cp->playing) {
      cp->playing = 0;
      alSynSetVol(&__libmus_alglobals.drvr, mus_voices + x, 0,
                  mus_next_frame_time);
      alSynStopVoice(&__libmus_alglobals.drvr, mus_voices + x);
    }
  }
}

// __MusIntFlushPending
void func_8009B060(channel_t* cp, int x) {
  if (cp->playing) alSynStopVoice(&__libmus_alglobals.drvr, mus_voices + x);
  cp->playing = 1;
  /* start sample */
  alSynStartVoice(&__libmus_alglobals.drvr, mus_voices + x, cp->pending);
  cp->pending = NULL;
}

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B0DC);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_set_volume_and_pan);

// __MusIntSetPitch: compute the channel frequency (portamento + pitchbend +
// 2^(semitones/12)) and push it to the synth voice.
void func_8009B254(channel_t* cp, int x, float offset) {
  float frequency, temp;

  /* base frequency */
  frequency = cp->base_note;

  /* incorporate portamento */
  if (cp->port != 0) {
    if (cp->count <= cp->port) {
      temp = (frequency - cp->last_note) / (float)(cp->port);
      temp *= (float)cp->count;
      frequency = cp->last_note + temp;
    }
    cp->port_base = frequency;
  }

  /* incorporate offsets */
  frequency += offset + cp->pitchbend_precalc;

  /* only output it if it's changed! */
  if (frequency == cp->old_frequency) return;
  cp->old_frequency = frequency;
  frequency = __MusIntPowerOf2(frequency * (1.0 / 12.0));
#ifdef _AUDIODEBUG
  if (frequency <= 0) {
    osSyncPrintf("PLAYER.C: frequency underflow.\n");
    frequency = 1.0;
  }
#endif
  if (frequency > 2.0) {
#ifdef _AUDIODEBUG
    osSyncPrintf("PLAYER.C: frequency overflow (note silenced).\n");
#endif
    frequency = 2.0;
    cp->velocity = 0;
  }
  alSynSetPitch(&__libmus_alglobals.drvr, mus_voices + x, frequency);
}

// __MusIntInitEnvelope
void func_8009B360(channel_t* cp) {
  if (cp->length != 0x7fff) {
    if (cp->cutoff != 0) /* release time is from start of note */
      cp->release_frame = cp->note_start_frame + (cp->cutoff << 8);
    else /* release time is from end of note */
      cp->release_frame = cp->note_end_frame - (cp->endit << 8);
#ifdef _FX_FULL_RELEASE_MODE
    if (cp->fx_addr)
      cp->note_end_frame +=
          (((cp->env_release_speed << 10) / cp->env_speed_calc) << 8);
#endif
  } else {
    cp->release_frame =
        cp->note_start_frame + 0x7fffffff; /* release tomorrow please! */
  }
  cp->env_current = cp->env_init_vol;
  cp->env_count = cp->env_speed;
  cp->env_phase = 1;
}

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B3D0);

// __MusIntInitSweep
void func_8009B5C4(channel_t* cp) {
  cp->sweep_frame = cp->note_start_frame;
  cp->sweep_timer = 0;
  cp->sweep_dir = cp->pan & 0x40;
}

// __MusIntProcessSweep
void func_8009B5E0(channel_t* cp) {
  unsigned long calc;

  do {
    cp->sweep_frame += 256;
    calc = cp->sweep_timer + cp->sweep_speed;
    if (calc < 64) {
      cp->sweep_timer = calc;
      continue;
    }
    cp->sweep_timer = calc & 63;
    calc >>= 6;
    if (!cp->sweep_dir) {
      cp->pan += calc;
      if (cp->pan > 0x7f) {
        cp->pan = 0x7f;
        cp->sweep_dir = 1;
      }
    } else {
      cp->pan -= calc;
      if (cp->pan >= 0x80 || cp->pan == 0) {
        cp->pan = 0;
        cp->sweep_dir = 0;
      }
    }
  } while ((long)(cp->sweep_frame - cp->channel_frame) < 0);
}

// __MusIntProcessWobble
float func_8009B698(channel_t* cp) {
  cp->wobble_count--;
  if (!cp->wobble_count) {
    if (cp->wobble_current == 0) {
      cp->wobble_current = cp->wobble_amount;
      cp->wobble_count = cp->wobble_on_speed;
    } else {
      cp->wobble_current = 0;
      cp->wobble_count = cp->wobble_off_speed;
    }
  }
  return ((float)cp->wobble_current);
}

// __MusIntProcessVibrato
float func_8009B6F0(channel_t* cp) {
  int temp;
  float temp1;

  temp = cp->count - cp->vib_delay;
  if (temp > 0) {
    temp1 = sinf((float)temp * cp->vib_precalc) * cp->vib_amount;
    cp->vibrato = temp1;
  } else {
    return (0);
  }
  return (cp->vibrato);
}

// __MusIntProcessContinuousVolume
void func_8009B754(channel_t* cp) {
  unsigned char work_vol;

  do {
    cp->volume_frame += 256;
    cp->cont_vol_repeat_count--;
    if (cp->cont_vol_repeat_count == 0) /* already repeating? */
    {
      work_vol = *(cp->pvolume++);
      if (work_vol > 127) /* does count follow? */
      {
        /* yes  volume is followed by run length data */
        cp->volume = work_vol & 0x7f;
        work_vol = *(cp->pvolume++);
        if (work_vol > 127) {
          cp->cont_vol_repeat_count = ((int)(work_vol & 0x7f) * 256);
          cp->cont_vol_repeat_count += (int)*(cp->pvolume++) + 2;
        } else
          cp->cont_vol_repeat_count = (int)work_vol + 2;
      } else {
        cp->volume = work_vol;
        cp->cont_vol_repeat_count = 1;
      }
    }
  } while ((long)(cp->volume_frame - cp->channel_frame) < 0);
}

// __MusIntProcessContinuousPitchBend
void func_8009B818(channel_t* cp) {
  unsigned char work_pb;

  do {
    cp->pitchbend_frame += 256;
    cp->cont_pb_repeat_count--;
    if (cp->cont_pb_repeat_count == 0) /* already repeating? */
    {
      work_pb = *(cp->ppitchbend++);
      if (work_pb > 127) /* does count follow? */
      {
        /* yes  pitchbend is followed by run length data */
        cp->pitchbend = ((float)(work_pb & 0x7f)) - 64.0;
        cp->pitchbend_precalc = cp->pitchbend * cp->bendrange;
        work_pb = *(cp->ppitchbend++);
        if (work_pb > 127) {
          cp->cont_pb_repeat_count = ((int)(work_pb & 0x7f) * 256);
          cp->cont_pb_repeat_count += (int)*(cp->ppitchbend++) + 2;
        } else
          cp->cont_pb_repeat_count = (int)work_pb + 2;
      } else {
        cp->pitchbend = ((float)work_pb) - 64.0;
        cp->pitchbend_precalc = cp->pitchbend * cp->bendrange;
        cp->cont_pb_repeat_count = 1;
      }
    }
  } while ((long)(cp->pitchbend_frame - cp->channel_frame) < 0);
}

// __MusIntPowerOf2: 2^x via a 6-term polynomial (x>0) or its reciprocal (x<0).
float func_8009B92C(float x) {
  float x2;

  if (x == 0) return 1;

  if (x > 0) {
    x2 = x * x;
    return (1 + (x * .693147180559945) + (x2 * .240226506959101) +
            (x2 * x * 5.55041086648216E-02) + (x2 * x2 * 9.61812910762848E-03) +
            (x2 * x2 * x * 1.33335581464284E-03) +
            (x2 * x2 * x2 * 1.54035303933816E-04));
  } else {
    x = -x;
    x2 = x * x;
    return (1 / (1 + (x * .693147180559945) + (x2 * .240226506959101) +
                 (x2 * x * 5.55041086648216E-02) +
                 (x2 * x2 * 9.61812910762848E-03) +
                 (x2 * x2 * x * 1.33335581464284E-03) +
                 (x2 * x2 * x2 * 1.54035303933816E-04)));
  }
}

// __MusIntRemapPtrBank: convert pointer-bank file offsets to RAM pointers.
void mus_remap_ptr_bank(char* pptr, char* wptr) {
  int i;
  ptr_bank_t* ptrfile_addr;
  unsigned char *chardetune, charwork;
  float *floatdetune, floatwork;
  unsigned long base;

  ptrfile_addr = (ptr_bank_t*)pptr;
  /* return if already remapped */
  if (ptrfile_addr->flags & PTRFLAG_REMAPPED) return;
  /* set remapped flag */
  ptrfile_addr->flags |= PTRFLAG_REMAPPED;

  /* remap first set of pointers */
  __MusIntRemapPtrs(&ptrfile_addr->basenote, pptr, 3);
  /* remap wave list pointers */
  __MusIntRemapPtrs(&ptrfile_addr->wave_list[0], pptr, ptrfile_addr->count);

  /* now calculate detune values and remap wave list */
  for (i = 0; i < ptrfile_addr->count; i++) {
    floatdetune = &ptrfile_addr->detune[i];
    chardetune = (unsigned char*)floatdetune;
    charwork = *chardetune;

    floatwork = U8_TO_FLOAT(charwork);
    *floatdetune = floatwork / 100.0;

    charwork = ptrfile_addr->basenote[i] - BASEOFFSET;
    floatwork = U8_TO_FLOAT(charwork);
    *floatdetune += floatwork;

    /* remap pointers inside ALWaveTable structures */
    if (!ptrfile_addr->wave_list[i]->flags) {
      base = (unsigned long)ptrfile_addr->wave_list[i]->base;
      if ((base & 0xff000000) != 0xff000000) /* not n64dd sample */
      {
        base += (unsigned long)wptr;
        ptrfile_addr->wave_list[i]->base = (u8*)base;
      }
      ptrfile_addr->wave_list[i]->flags = 1;

      if (ptrfile_addr->wave_list[i]->waveInfo.adpcmWave.loop)
        ptrfile_addr->wave_list[i]->waveInfo.adpcmWave.loop =
            (ALADPCMloop*)((u32)(ptrfile_addr->wave_list[i]
                                     ->waveInfo.adpcmWave.loop) +
                           (u32)(pptr));
      if (ptrfile_addr->wave_list[i]->type == AL_ADPCM_WAVE)
        ptrfile_addr->wave_list[i]->waveInfo.adpcmWave.book =
            (ALADPCMBook*)((u32)(ptrfile_addr->wave_list[i]
                                     ->waveInfo.adpcmWave.book) +
                           (u32)(pptr));
    }
  }
  /* flush data cache so the new sample pointers are visible to the RSP */
  osWritebackDCacheAll();
}

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009BC58);

// __MusIntInitialiseChannel
void init_struct_defaults(channel_t* cp) {
  unsigned char old_playing, *work_ptr;
  int i;

  /* disable channel processing 1st!!! */
  cp->pdata = NULL;
  old_playing = cp->playing;

  /* zero out channel */
  work_ptr = (unsigned char*)cp;
  for (i = 0; i < sizeof(channel_t); i++) *work_ptr++ = 0;

  /* set none zero values */
  cp->old_volume = 0xffff;
  cp->old_reverb = 0xff;
  cp->old_pan = 0xff;
  cp->old_frequency = 99.9;

  cp->channel_tempo = cp->channel_tempo_save = 96 * 256 / mus_vsyncs_per_second;

  cp->length = 1;

  cp->default_velocity = 127;
  cp->volume = 127;
  cp->bendrange = 2 * (1.0 / 64.0);
  cp->pan = 64;

  cp->cont_vol_repeat_count = 1;
  cp->cont_pb_repeat_count = 1;

  cp->stopping = -1;

  /* new volume and pan scales */
  cp->volscale = 0x80;
  cp->panscale = 0x80;
  cp->temscale = 0x80;

  /* setup a default envelope */
  cp->env_speed = 1;
  cp->env_attack_speed = 1;
  cp->env_attack_calc = 1.0F;
  cp->env_max_vol = 127;
  cp->env_decay_speed = 255;
  cp->env_decay_calc = 1.0 / 255.0;
  cp->env_sustain_vol = 127;
  cp->env_release_speed = 15;
  cp->env_release_calc = 1.0 / 15.0;

  /* set current sample bank */
  cp->sample_bank = mus_init_bank ? mus_init_bank : mus_default_bank;

  /* restore channel status flag */
  cp->playing = old_playing;
}

// __MusIntFindChannel
int mus_alloc_channel(song_t* addr, int song_chan) {
  channel_t* cp;
  int i, current, current_channel;

  /* master tacks can use channels reserved for them */
  if (song_chan < 0) {
    for (i = 0, cp = mus_channels; i < MAX_SONGS; i++, cp++)
      if (!cp->pdata) return (i);
  }

  /* 1st scan for empty channel */
  for (i = MAX_SONGS, cp = mus_channels2; i < max_channels; i++, cp++)
    if (!cp->pdata) return (i);

  /* 2nd scan for sfx channel to override */
  for (i = MAX_SONGS, cp = mus_channels2, current = 0x7fffffff,
      current_channel = MAX_SONGS - 1;
       i < max_channels; i++, cp++) {
    if (cp->fx_addr) {
      if (cp->priority <= current) {
        current = cp->priority;
        current_channel = i;
      }
    }
  }
  if (current_channel >= MAX_SONGS) return (current_channel);

  /* 3rd scan for song channel (not this song) */
  for (i = MAX_SONGS, cp = mus_channels2; i < max_channels; i++)
    if (!cp->fx_addr && cp->song_addr != addr) return (i);

  /* 4th scan for same tune, same channel */
  for (i = MAX_SONGS, cp = mus_channels2; i < max_channels; i++, cp++)
    if (cp->song_addr == addr && addr->data_list[song_chan] == cp->pbase)
      return (i);

  /* get any channel */
  return ((song_chan % (max_channels - MAX_SONGS)) + MAX_SONGS);
}

// __MusIntRemapPtrs
void func_8009BFF0(void* addr, void* offset, int count) {
  unsigned long *dest, add;
  int i;

  dest = (unsigned long*)addr;
  add = (unsigned long)offset;
  for (i = 0; i < count; i++)
    if (dest[i]) dest[i] += add;
}

// __MusIntStartEffect
unsigned long func_8009C028(channel_t* cp, fx_header_t* header, int number,
                            int volume, int pan, int priority) {
  __MusIntInitialiseChannel(cp);

  cp->fx_number = number;
  cp->fx_addr = header;
  cp->volscale = volume;
  cp->panscale = pan;
  cp->handle = mus_current_handle++;
  cp->priority = priority;

  /* set sample bank pointer */
  if (header->ptr_addr) cp->sample_bank = header->ptr_addr;

  /* pdata must be set last to avoid processing clash */
  cp->pdata = cp->pbase = header->effects[number].fxdata;

  return (cp->handle);
}

INCLUDE_ASM("asm/nonmatchings/libmus/player", allocate_object_slot);

// __MusIntStartSong
musHandle mus_start_song(void* addr) {
  song_t* song_addr;
  int i, channels;
  channel_t* cp;
  unsigned long handle;

  song_addr = addr;
  channels = song_addr->num_channels;

  if (!song_addr->flags & SONGFLAG_INITIALISED) {
    song_addr->flags |= SONGFLAG_INITIALISED;
    /* convert header offsets to pointers */
    __MusIntRemapPtrs(SONGHDR_ADR(song_addr), addr, SONGHDR_COUNT);
    /* convert pointer tables */
    __MusIntRemapPtrs(song_addr->data_list, addr, channels);
    __MusIntRemapPtrs(song_addr->volume_list, addr, channels);
    __MusIntRemapPtrs(song_addr->pbend_list, addr, channels);
  }
  /* get next handle */
  handle = mus_current_handle++;

  /* start master track last - always gets started! */
  cp = mus_channels + __MusIntFindChannel(song_addr, -1);
  __MusIntInitialiseChannel(cp);
  cp->velocity_on = 1; /* enable velocity for songs */
  cp->channel_flag |= CHFLAG_PAUSE | CHFLAG_MASTERTRACK;
  cp->song_addr = song_addr;
  cp->pdata = cp->pbase = song_addr->master_track;
  cp->handle = handle;

  for (i = 0; i < channels; i++) {
    if (song_addr->data_list[i]) /* should never happen but just in case! */
    {
      cp = mus_channels + __MusIntFindChannel(song_addr, i);
      __MusIntInitialiseChannel(cp);
      cp->velocity_on = 1; /* enable velocity for songs */
      cp->channel_flag |= CHFLAG_PAUSE;
      cp->song_addr = song_addr;
      cp->pvolume = cp->pvolumebase = song_addr->volume_list[i];
      cp->ppitchbend = cp->ppitchbendbase = song_addr->pbend_list[i];
      /* pdata must be set last to avoid processing clash */
      cp->pdata = cp->pbase = song_addr->data_list[i];
      cp->handle = handle;
    }
  }
  /* reset any single sample bank override */
  mus_init_bank = NULL;
  return (handle);
}

// __MusIntHandleSetFlag
void mus_handle_set_flag(unsigned long handle, unsigned long clear,
                         unsigned long set) {
  int i;
  channel_t* cp;

  for (i = 0, cp = mus_channels; i < max_channels; i++, cp++) {
    if (cp->handle == handle) {
      cp->channel_flag &= clear;
      cp->channel_flag |= set;
    }
  }
}

unsigned char* mus_cmd_stop(channel_t* cp, unsigned char* ptr) {
  cp->pvolume = NULL;
  cp->ppitchbend = NULL;
  cp->song_addr = NULL;
  cp->fx_addr = NULL;
  cp->handle = 0;
  cp->pending = NULL;
  return (NULL);
}

unsigned char* mus_cmd_wave(channel_t* cp, unsigned char* ptr) {
  unsigned short wave;

  wave = *ptr++;
  if (wave & 0x80) {
    wave &= 0x7f;
    wave <<= 8;
    wave |= *ptr++;
  }
  cp->wave = wave;
  return (ptr);
}

unsigned char* mus_cmd_port_on(channel_t* cp, unsigned char* ptr) {
  cp->port = *ptr++;
  if (cp->port) cp->port_base = cp->base_note;
  return (ptr);
}

unsigned char* mus_cmd_port_off(channel_t* cp, unsigned char* ptr) {
  cp->port = 0;
  return (ptr);
}

unsigned char* mus_cmd_default_adsr(channel_t* cp, unsigned char* ptr) {
  unsigned char value;

  // get envelope speed...
  value = *ptr++;
  if (value == 0)  // cannot be zero!!!
    value = 1;
  cp->env_speed = value;
  cp->env_speed_calc = 1024 / value;

  // get envelope initial volume level...
  cp->env_init_vol = *ptr++;

  // get attack speed...
  value = *ptr++;
#ifdef _AUDIODEBUG
  if (value == 0) {
    osSyncPrintf(
        "PLAYER_COMMANDS.C: Fdefa() attempting to set speed of zero.\n");
    value = 1;
  }
#endif
  cp->env_attack_speed = value;

  // get peak volume...
  cp->env_max_vol = *ptr++;

  // get attack precalc value...
  cp->env_attack_calc =
      (1.0 / ((float)value)) * ((float)(cp->env_max_vol - cp->env_init_vol));

  // get decay speed...
  value = *ptr++;
#ifdef _AUDIODEBUG
  if (value == 0) {
    osSyncPrintf(
        "PLAYER_COMMANDS.C: Fdefa() attempting to set decay speed of zero.\n");
    value = 1;
  }
#endif
  cp->env_decay_speed = value;

  // get sustain volume level...
  cp->env_sustain_vol = *ptr++;

  // get sustain precalc value...
  cp->env_decay_calc =
      (1.0 / ((float)value)) * ((float)(cp->env_sustain_vol - cp->env_max_vol));

  // get release speed...
  value = *ptr++;
#ifdef _AUDIODEBUG
  if (value == 0) {
    osSyncPrintf(
        "PLAYER_COMMANDS.C: Fdefa() attempting to set release speed of "
        "zero.\n");
    value = 1;
  }
#endif
  cp->env_release_speed = value;
  cp->env_release_calc = 1.0 / ((float)value);

  return (ptr);
}

unsigned char* mus_cmd_tempo(channel_t* cp, unsigned char* ptr) {
  // tempo   = bpm
  // fps     = mus_vsyncs_per_second
  // 120 bpm = 96 fps
  // therefore tempo = bmp(required)/120*96/mus_vsyncs_per_second

  channel_t* sp;
  int i;
  int temp, temp2;

  temp = (*ptr++) * 256 * 96 / 120 / mus_vsyncs_per_second;
  temp2 = (temp * cp->temscale) >> 7;
  if (cp->fx_addr) {
    cp->channel_tempo = temp;
  } else {
    for (i = 0, sp = mus_channels; i < max_channels; i++, sp++) {
      if (sp->song_addr == cp->song_addr) {
        sp->channel_tempo_save = temp;
        sp->channel_tempo = temp2;
      }
    }
  }
  return (ptr);
}

unsigned char* mus_cmd_endit(channel_t* cp, unsigned char* ptr) {
  cp->endit = *ptr++;
  cp->cutoff = 0;
  return (ptr);
}

unsigned char* mus_cmd_cutoff(channel_t* cp, unsigned char* ptr) {
  short tmp;

  tmp = (*ptr++) << 8;
  tmp |= *ptr++;

  cp->cutoff = tmp;
  cp->endit = 0;
  return (ptr);
}

unsigned char* mus_cmd_vibrato_up(channel_t* cp, unsigned char* ptr) {
  cp->vib_delay = *ptr++;
  cp->vib_speed = *ptr++;
  cp->vib_amount = ((float)*ptr++) / 50.0;
  cp->vib_precalc = (2 * 3.1415926) / (float)cp->vib_speed;
  return (ptr);
}

unsigned char* mus_cmd_vibrato_down(channel_t* cp, unsigned char* ptr) {
  cp->vib_delay = *ptr++;
  cp->vib_speed = *ptr++;
  cp->vib_amount = (-((float)*ptr++)) / 50.0;
  cp->vib_precalc = (2 * 3.1415926) / (float)cp->vib_speed;
  return (ptr);
}

unsigned char* mus_cmd_vibrato_off(channel_t* cp, unsigned char* ptr) {
  cp->vib_speed = 0;
  cp->vibrato = 0;
  return (ptr);
}

unsigned char* mus_cmd_length(channel_t* cp, unsigned char* ptr) {
  int length;

  length = *ptr++;
  if (length >= 0x80) {
    length &= 0x7f;
    length <<= 8;
    length |= *ptr++;
  }
  cp->fixed_length = length;
  return (ptr);
}

unsigned char* mus_cmd_ignore(channel_t* cp, unsigned char* ptr) {
  cp->ignore = 1;
  return (ptr);
}

unsigned char* mus_cmd_transpose(channel_t* cp, unsigned char* ptr) {
  cp->transpose = *ptr++;
  return (ptr);
}

unsigned char* mus_cmd_ignore_transpose(channel_t* cp, unsigned char* ptr) {
  cp->ignore_transpose = 1;
  return (ptr);
}

unsigned char* mus_cmd_distort(channel_t* cp, unsigned char* ptr) {
  int c;
  float f;

  c = (int)(*ptr++);
  if (c & 0x80) c |= 0xffffff00;  // signed chars don't work
  f = (float)(c) / 100.0;

  cp->freqoffset -= cp->distort;
  cp->freqoffset += f;
  cp->distort = f;
  return (ptr);
}

unsigned char* mus_cmd_env_off(channel_t* cp, unsigned char* ptr) {
  cp->env_trigger_off = 1;
  return (ptr);
}

unsigned char* mus_cmd_env_on(channel_t* cp, unsigned char* ptr) {
  cp->env_trigger_off = 0;
  return (ptr);
}

unsigned char* mus_cmd_trigger_off(channel_t* cp, unsigned char* ptr) {
  cp->trigger_off = 1;
  return (ptr);
}

unsigned char* mus_cmd_trigger_on(channel_t* cp, unsigned char* ptr) {
  cp->trigger_off = 0;
  return (ptr);
}

unsigned char* mus_cmd_for(channel_t* cp, unsigned char* ptr) {
  int index;

  index = cp->for_stack_count;
  cp->for_count[index] = *ptr++;
  cp->for_stack[index] = ptr;
  cp->for_stackvol[index] = cp->pvolume;
  cp->for_stackpb[index] = cp->ppitchbend;
  cp->for_volume[index] = cp->volume;
  cp->for_pitchbend[index] = cp->pitchbend;
  cp->for_vol_count[index] = cp->cont_vol_repeat_count;
  cp->for_pb_count[index] = cp->cont_pb_repeat_count;
  cp->for_stack_count++;
  return (ptr);
}

unsigned char* mus_cmd_next(channel_t* cp, unsigned char* ptr) {
  int index;

  index = cp->for_stack_count - 1;
  /* infinite loop? */
  if (cp->for_count[index] != 0xff) { /* still looping? */
    if (--(cp->for_count[index]) == 0) {
      cp->for_stack_count = index;
      index = -1;
    }
  }
  /* unstack pointers if necessary */
  if (index > -1) {
    ptr = cp->for_stack[index];
    cp->pvolume = cp->for_stackvol[index];
    cp->ppitchbend = cp->for_stackpb[index];
    cp->volume = cp->for_volume[index];
    cp->pitchbend = cp->for_pitchbend[index];
    cp->cont_vol_repeat_count = cp->for_vol_count[index];
    cp->cont_pb_repeat_count = cp->for_pb_count[index];
    cp->pitchbend_precalc = cp->pitchbend * cp->bendrange;
  }
  return (ptr);
}

unsigned char* mus_cmd_wobble(channel_t* cp, unsigned char* ptr) {
  cp->wobble_amount = *ptr++;
  cp->wobble_on_speed = *ptr++;
  cp->wobble_off_speed = *ptr++;
  return (ptr);
}

unsigned char* mus_cmd_wobble_off(channel_t* cp, unsigned char* ptr) {
  cp->wobble_on_speed = 0;
  return (ptr);
}

unsigned char* mus_cmd_velocity_on(channel_t* cp, unsigned char* ptr) {
  cp->velocity_on = 1;
  return (ptr);
}

unsigned char* mus_cmd_velocity_off(channel_t* cp, unsigned char* ptr) {
  cp->velocity_on = 0;
  return (ptr);
}

unsigned char* mus_cmd_velocity(channel_t* cp, unsigned char* ptr) {
  cp->default_velocity = *ptr++;
  cp->velocity_on = 0;
  return (ptr);
}

unsigned char* mus_cmd_pan(channel_t* cp, unsigned char* ptr) {
  cp->pan = (*ptr++) / 2;
  return (ptr);
}

unsigned char* mus_cmd_stereo(channel_t* cp, unsigned char* ptr) {
  return (ptr + 2);
}

unsigned char* mus_cmd_drums_on(channel_t* cp, unsigned char* ptr) {
  int index;

  index = *ptr++;
  if (index >= 0x80) {
    index &= 0x7f;
    index <<= 8;
    index |= *ptr++;
  }
  cp->pdrums = &cp->song_addr->drum_table[index];
  return (ptr);
}

unsigned char* mus_cmd_drums_off(channel_t* cp, unsigned char* ptr) {
  cp->pdrums = NULL;
  return (ptr);
}

unsigned char* mus_cmd_print(channel_t* cp, unsigned char* ptr) {
#ifdef _AUDIODEBUG
  osSyncPrintf("PLAYER_COMMANDS.C: Fprint() -  %d (channel frame=%d)\n", *ptr++,
               cp->channel_frame);
  return (ptr);
#else
  ptr++;
  return (ptr);
#endif
}

unsigned char* mus_cmd_goto(channel_t* cp, unsigned char* ptr) {
  int off, off1;

  /* 2 bytes for song offset */
  off1 = *ptr++ << 8;
  off1 += *ptr++;

  /* get volume offset BEFORE updating pointer */
  /* 2 bytes for volume offset (never inside a run length bit) */
  off = *ptr++ << 8;
  off += *ptr++;
  cp->pvolume = cp->pvolumebase + off;
  cp->cont_vol_repeat_count = 1;

  /* get pitchbend offset BEFORE updating pointer */
  /* 2 bytes for pitchbend offset (never inside a run length bit) */
  off = *ptr++ << 8;
  off += *ptr++;
  cp->ppitchbend = cp->ppitchbendbase + off;
  cp->cont_pb_repeat_count = 1;

  return (cp->pbase + off1);
}

unsigned char* mus_cmd_reverb(channel_t* cp, unsigned char* ptr) {
  cp->reverb = *ptr++;
  return (ptr);
}

unsigned char* mus_cmd_rand_note(channel_t* cp, unsigned char* ptr) {
  // rand_amount,rand_base  -- 20,-3 would give -3 to 16 as the value
  cp->transpose = __MusIntRandom(*ptr++);
  cp->transpose += *ptr++;
  return (ptr);
}

unsigned char* mus_cmd_rand_volume(channel_t* cp, unsigned char* ptr) {
  // rand_amount,base
  cp->volume = __MusIntRandom(*ptr++);
  cp->volume += *ptr++;
  return (ptr);
}

unsigned char* mus_cmd_rand_pan(channel_t* cp, unsigned char* ptr) {
  // rand_amount,base
  cp->pan = __MusIntRandom(*ptr++);
  cp->pan += *ptr++;
  return (ptr);
}

unsigned char* mus_cmd_volume(channel_t* cp, unsigned char* ptr) {
  cp->volume = *ptr++;
  return (ptr);
}

unsigned char* mus_cmd_start_fx(channel_t* cp, unsigned char* ptr) {
  int i, number;
  channel_t* sp;
  unsigned long new_handle;

  number = *ptr++;
  if (number >= 0x80) number = ((number & 0x7f) << 8) + *ptr++;

  /* increase priority */
  cp->priority++;
  /* start sub effect */
  new_handle = __MusIntFindChannelAndStart(cp->fx_addr, number, cp->volscale,
                                           cp->panscale, cp->priority);
  /* decrease priority back to normal */
  cp->priority--;
  /* copy handle and sample bank setting */
  if (new_handle) {
    for (i = 0, sp = mus_channels; i < max_channels; i++, sp++) {
      if (sp->handle == new_handle) {
        sp->handle = cp->handle;
        sp->sample_bank = cp->sample_bank;
      }
    }
  }
  return (ptr);
}

unsigned char* mus_cmd_bend_range(channel_t* cp, unsigned char* ptr) {
  cp->bendrange = (float)(*ptr++) * (1.0 / 64.0);
  cp->pitchbend_precalc = cp->pitchbend * cp->bendrange;
  return (ptr);
}

unsigned char* mus_cmd_sweep(channel_t* cp, unsigned char* ptr) {
  cp->sweep_speed = *ptr++;
  return (ptr);
}

unsigned char* mus_cmd_change_fx(channel_t* cp, unsigned char* ptr) {
  int fxtype;

  fxtype = *ptr++;
#ifdef SUPPORT_FXCHANGE
  if (mus_songfxchange_flag == MUSBOOL_ON) {
    ChangeCustomEffect(fxtype);
  }
#endif

  return (ptr);
}

unsigned char* mus_cmd_marker(channel_t* cp, unsigned char* ptr) {
  int rest;
  int number;

  number = *ptr++; /* marker number */
  rest = *ptr++;
  if (rest & 0x80) {
    rest &= 0x7f;
    rest <<= 8;
    rest |= *ptr++;
  }
  /* if not going to a marker but marker is found on the mastertrack try
   * callback */
  if ((cp->channel_flag & CHFLAG_MASTERTRACK) &&
      !(cp->channel_flag & CHFLAG_PAUSE)) {
    if (marker_callback) marker_callback(cp->handle, number);
  }
  return (ptr);
}

unsigned char* mus_cmd_length0(channel_t* cp, unsigned char* ptr) {
  cp->fixed_length = 0;
  return (ptr);
}
