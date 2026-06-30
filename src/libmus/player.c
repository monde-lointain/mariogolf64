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
float func_8009B92C(float);  // __MusIntPowerOf2 (defined below)
#define mus_voices g_mus_voice_array
#define __MusIntPowerOf2 func_8009B92C
#define __MusIntRandom func_8009BC58
#define __MusIntFindChannelAndStart allocate_object_slot
#define ChangeCustomEffect func_8009D8A8
#define mus_songfxchange_flag g_mus_fx_enabled
#define __MusIntRemapPtrs func_8009BFF0

// player.c file-scope macros (verbatim).
#define REST 96
#define BASEOFFSET 48
#define U8_TO_FLOAT(c) ((c) & 128) ? -(256 - (c)) : (c)

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_envelope);

INCLUDE_ASM("asm/nonmatchings/libmus/player", MusInitialize);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_set_master_volume);

INCLUDE_ASM("asm/nonmatchings/libmus/player", MusStartSong);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_start_song_from_marker);

INCLUDE_ASM("asm/nonmatchings/libmus/player", try_spawn_global_object);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_80099CF0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_stop);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_ask);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_stop);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_ask);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_set_volume);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_set_pan);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_set_freq_offset);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_set_tempo);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_set_reverb);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_ptr_bank_initialize);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A2F0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A318);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A33C);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_get_ptr_bank);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_pause);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_start_song);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_set_fx_type);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_set_song_fx_change);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_fx_bank_initialize);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A500);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A508);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A514);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A520);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A52C);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A534);

INCLUDE_ASM("asm/nonmatchings/libmus/player", MusSetScheduler);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_wave_count);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_wave_address);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A624);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A630);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A64C);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_fifo_dispatch);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_fifo_enqueue);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009A7C8);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_player_frame_handler);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009AB18);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B060);

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

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B360);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B3D0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B5C4);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B5E0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B698);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B6F0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B754);

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

INCLUDE_ASM("asm/nonmatchings/libmus/player", init_struct_defaults);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_alloc_channel);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009BFF0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009C028);

INCLUDE_ASM("asm/nonmatchings/libmus/player", allocate_object_slot);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_start_song);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_handle_set_flag);

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
