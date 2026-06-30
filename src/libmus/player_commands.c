// player_commands.c -- the bytecode command-handler jumptable bodies (Fxxx).
// Separate TU from player.c so the internal __MusInt* callees stay out-of-line
// (cross-TU jal) under the game -O3, matching the ROM.
#include "player_priv.h"

unsigned char* Fstop(channel_t* cp, unsigned char* ptr) {
  cp->pvolume = NULL;
  cp->ppitchbend = NULL;
  cp->song_addr = NULL;
  cp->fx_addr = NULL;
  cp->handle = 0;
  cp->pending = NULL;
  return (NULL);
}

unsigned char* Fwave(channel_t* cp, unsigned char* ptr) {
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

unsigned char* Fport(channel_t* cp, unsigned char* ptr) {
  cp->port = *ptr++;
  if (cp->port) cp->port_base = cp->base_note;
  return (ptr);
}

unsigned char* Fportoff(channel_t* cp, unsigned char* ptr) {
  cp->port = 0;
  return (ptr);
}

unsigned char* Fdefa(channel_t* cp, unsigned char* ptr) {
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

unsigned char* Ftempo(channel_t* cp, unsigned char* ptr) {
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

unsigned char* Fendit(channel_t* cp, unsigned char* ptr) {
  cp->endit = *ptr++;
  cp->cutoff = 0;
  return (ptr);
}

unsigned char* Fcutoff(channel_t* cp, unsigned char* ptr) {
  short tmp;

  tmp = (*ptr++) << 8;
  tmp |= *ptr++;

  cp->cutoff = tmp;
  cp->endit = 0;
  return (ptr);
}

unsigned char* Fvibup(channel_t* cp, unsigned char* ptr) {
  cp->vib_delay = *ptr++;
  cp->vib_speed = *ptr++;
  cp->vib_amount = ((float)*ptr++) / 50.0;
  cp->vib_precalc = (2 * 3.1415926) / (float)cp->vib_speed;
  return (ptr);
}

unsigned char* Fvibdown(channel_t* cp, unsigned char* ptr) {
  cp->vib_delay = *ptr++;
  cp->vib_speed = *ptr++;
  cp->vib_amount = (-((float)*ptr++)) / 50.0;
  cp->vib_precalc = (2 * 3.1415926) / (float)cp->vib_speed;
  return (ptr);
}

unsigned char* Fviboff(channel_t* cp, unsigned char* ptr) {
  cp->vib_speed = 0;
  cp->vibrato = 0;
  return (ptr);
}

unsigned char* Flength(channel_t* cp, unsigned char* ptr) {
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

unsigned char* Fignore(channel_t* cp, unsigned char* ptr) {
  cp->ignore = 1;
  return (ptr);
}

unsigned char* Ftrans(channel_t* cp, unsigned char* ptr) {
  cp->transpose = *ptr++;
  return (ptr);
}

unsigned char* Fignore_trans(channel_t* cp, unsigned char* ptr) {
  cp->ignore_transpose = 1;
  return (ptr);
}

unsigned char* Fdistort(channel_t* cp, unsigned char* ptr) {
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

unsigned char* Fenvoff(channel_t* cp, unsigned char* ptr) {
  cp->env_trigger_off = 1;
  return (ptr);
}

unsigned char* Fenvon(channel_t* cp, unsigned char* ptr) {
  cp->env_trigger_off = 0;
  return (ptr);
}

unsigned char* Ftroff(channel_t* cp, unsigned char* ptr) {
  cp->trigger_off = 1;
  return (ptr);
}

unsigned char* Ftron(channel_t* cp, unsigned char* ptr) {
  cp->trigger_off = 0;
  return (ptr);
}

unsigned char* Ffor(channel_t* cp, unsigned char* ptr) {
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

unsigned char* Fnext(channel_t* cp, unsigned char* ptr) {
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

unsigned char* Fwobble(channel_t* cp, unsigned char* ptr) {
  cp->wobble_amount = *ptr++;
  cp->wobble_on_speed = *ptr++;
  cp->wobble_off_speed = *ptr++;
  return (ptr);
}

unsigned char* Fwobbleoff(channel_t* cp, unsigned char* ptr) {
  cp->wobble_on_speed = 0;
  return (ptr);
}

unsigned char* Fvelon(channel_t* cp, unsigned char* ptr) {
  cp->velocity_on = 1;
  return (ptr);
}

unsigned char* Fveloff(channel_t* cp, unsigned char* ptr) {
  cp->velocity_on = 0;
  return (ptr);
}

unsigned char* Fvelocity(channel_t* cp, unsigned char* ptr) {
  cp->default_velocity = *ptr++;
  cp->velocity_on = 0;
  return (ptr);
}

unsigned char* Fpan(channel_t* cp, unsigned char* ptr) {
  cp->pan = (*ptr++) / 2;
  return (ptr);
}

unsigned char* Fstereo(channel_t* cp, unsigned char* ptr) { return (ptr + 2); }

unsigned char* Fdrums(channel_t* cp, unsigned char* ptr) {
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

unsigned char* Fdrumsoff(channel_t* cp, unsigned char* ptr) {
  cp->pdrums = NULL;
  return (ptr);
}

unsigned char* Fprint(channel_t* cp, unsigned char* ptr) {
#ifdef _AUDIODEBUG
  osSyncPrintf("PLAYER_COMMANDS.C: Fprint() -  %d (channel frame=%d)\n", *ptr++,
               cp->channel_frame);
  return (ptr);
#else
  ptr++;
  return (ptr);
#endif
}

unsigned char* Fgoto(channel_t* cp, unsigned char* ptr) {
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

unsigned char* Freverb(channel_t* cp, unsigned char* ptr) {
  cp->reverb = *ptr++;
  return (ptr);
}

unsigned char* FrandNote(channel_t* cp, unsigned char* ptr) {
  // rand_amount,rand_base  -- 20,-3 would give -3 to 16 as the value
  cp->transpose = __MusIntRandom(*ptr++);
  cp->transpose += *ptr++;
  return (ptr);
}

unsigned char* FrandVolume(channel_t* cp, unsigned char* ptr) {
  // rand_amount,base
  cp->volume = __MusIntRandom(*ptr++);
  cp->volume += *ptr++;
  return (ptr);
}

unsigned char* FrandPan(channel_t* cp, unsigned char* ptr) {
  // rand_amount,base
  cp->pan = __MusIntRandom(*ptr++);
  cp->pan += *ptr++;
  return (ptr);
}

unsigned char* Fvolume(channel_t* cp, unsigned char* ptr) {
  cp->volume = *ptr++;
  return (ptr);
}

unsigned char* Fstartfx(channel_t* cp, unsigned char* ptr) {
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

unsigned char* Fbendrange(channel_t* cp, unsigned char* ptr) {
  cp->bendrange = (float)(*ptr++) * (1.0 / 64.0);
  cp->pitchbend_precalc = cp->pitchbend * cp->bendrange;
  return (ptr);
}

unsigned char* Fsweep(channel_t* cp, unsigned char* ptr) {
  cp->sweep_speed = *ptr++;
  return (ptr);
}

unsigned char* Fchangefx(channel_t* cp, unsigned char* ptr) {
  int fxtype;

  fxtype = *ptr++;
#ifdef SUPPORT_FXCHANGE
  if (mus_songfxchange_flag == MUSBOOL_ON) {
    ChangeCustomEffect(fxtype);
  }
#endif

  return (ptr);
}

unsigned char* Fmarker(channel_t* cp, unsigned char* ptr) {
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

unsigned char* Flength0(channel_t* cp, unsigned char* ptr) {
  cp->fixed_length = 0;
  return (ptr);
}
