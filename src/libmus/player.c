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

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B254);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B360);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B3D0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B5C4);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B5E0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B698);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B6F0);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B754);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B818);

INCLUDE_ASM("asm/nonmatchings/libmus/player", func_8009B92C);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_remap_ptr_bank);

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

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_tempo);

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

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_drums_on);

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

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_rand_note);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_rand_volume);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_rand_pan);

unsigned char* mus_cmd_volume(channel_t* cp, unsigned char* ptr) {
  cp->volume = *ptr++;
  return (ptr);
}

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_start_fx);

unsigned char* mus_cmd_bend_range(channel_t* cp, unsigned char* ptr) {
  cp->bendrange = (float)(*ptr++) * (1.0 / 64.0);
  cp->pitchbend_precalc = cp->pitchbend * cp->bendrange;
  return (ptr);
}

unsigned char* mus_cmd_sweep(channel_t* cp, unsigned char* ptr) {
  cp->sweep_speed = *ptr++;
  return (ptr);
}

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_change_fx);

INCLUDE_ASM("asm/nonmatchings/libmus/player", mus_cmd_marker);

unsigned char* mus_cmd_length0(channel_t* cp, unsigned char* ptr) {
  cp->fixed_length = 0;
  return (ptr);
}
