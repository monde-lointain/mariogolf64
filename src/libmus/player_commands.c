/*
 * player_commands.c
 *
 * The libmus sequence player's command-opcode handlers, split out of player.c.
 * A song (or sound effect) is a byte stream of note events interleaved with
 * command bytes; a command byte has its high bit set and indexes the jump
 * table (defined in player.c) of the handlers below. Every handler receives
 * the owning channel `cp` and `ptr`, the read cursor positioned just past the
 * opcode byte: it consumes any operand bytes from the stream, updates the
 * channel's playback state, and returns the advanced cursor, which the player
 * stores as the channel's next instruction. Two handlers break that pattern --
 * Fgoto returns a computed branch target, and Fstop returns NULL to retire the
 * channel.
 */

/* Channel layout, the command jump table, and the audio-backend declarations.
 */
#include "player_priv.h"

/* Stop the channel: drop every stream/playback pointer and the handle, then
 * return NULL so the player loop sees no next instruction and retires it. */
unsigned char* Fstop(channel_t* cp, unsigned char* ptr) {
  cp->pvolume = NULL;
  cp->ppitchbend = NULL;
  cp->song_addr = NULL;
  cp->fx_addr = NULL;
  cp->handle = 0;
  cp->pending = NULL;
  return (NULL);
}

/* Select the wave (instrument voice) for following notes. The index is one
 * byte, or a 15-bit value over two bytes when the first byte's high bit is
 * set. */
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

/* Enable portamento (pitch glide between notes) over `port` steps; a nonzero
 * setting anchors the glide origin at the current base note. */
unsigned char* Fport(channel_t* cp, unsigned char* ptr) {
  cp->port = *ptr++;
  if (cp->port) {
    cp->port_base = cp->base_note;
  }
  return (ptr);
}

/* Disable portamento; notes start at their own pitch again. */
unsigned char* Fportoff(channel_t* cp, unsigned char* ptr) {
  cp->port = 0;
  return (ptr);
}

/* Define this channel's ADSR envelope inline from the stream (rather than from
 * the song's shared envelope table). Reads the attack/decay/release speeds and
 * the init/max/sustain levels, then precomputes a per-frame step for each
 * phase: env_speed_calc paces the whole envelope and the float *_calc fields
 * are the level change applied on each frame of attack, decay, and release. */
unsigned char* Fdefa(channel_t* cp, unsigned char* ptr) {
  unsigned char value;
  value = *ptr++;
  if (value == 0) {
    value = 1; /* overall pace; also guards 1024/value below */
  }
  cp->env_speed = value;
  cp->env_speed_calc = 1024 / value;
  cp->env_init_vol = *ptr++;

  /* Attack: climb from env_init_vol to env_max_vol over `value` frames. */
  value = *ptr++;
#ifdef _AUDIODEBUG
  if (value == 0) { /* a zero speed would divide by zero in the slope below */
    osSyncPrintf(
        "PLAYER_COMMANDS.C: Fdefa() attempting to set speed of zero.\n");
    value = 1;
  }
#endif
  cp->env_attack_speed = value;
  cp->env_max_vol = *ptr++;
  cp->env_attack_calc =
      (1.0 / ((float)value)) * ((float)(cp->env_max_vol - cp->env_init_vol));

  /* Decay: fall from env_max_vol to env_sustain_vol over `value` frames. */
  value = *ptr++;
#ifdef _AUDIODEBUG
  if (value == 0) {
    osSyncPrintf(
        "PLAYER_COMMANDS.C: Fdefa() attempting to set decay speed of zero.\n");
    value = 1;
  }
#endif
  cp->env_decay_speed = value;
  cp->env_sustain_vol = *ptr++;
  cp->env_decay_calc =
      (1.0 / ((float)value)) * ((float)(cp->env_sustain_vol - cp->env_max_vol));

  /* Release: fall toward silence at 1/value per frame. */
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

/* Set the playback tempo from a one-byte BPM-style value, converted to the
 * player's internal per-frame units and normalized to the video frame rate.
 * On an effect channel the value applies locally; on a song channel it is
 * pushed to every channel of the same song, storing the channel-scaled tempo
 * (temscale is a /128 fixed-point factor) alongside the unscaled original. */
unsigned char* Ftempo(channel_t* cp, unsigned char* ptr) {
  channel_t* sp;
  int i;
  int temp;
  int temp2;
  temp = (*ptr++) * 256 * 96 / 120 / mus_vsyncs_per_second;
  temp2 =
      (temp * cp->temscale) >> 7; /* apply channel tempo scale (>>7 == /128) */
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

/* Gate the note relative to its natural end (release it `endit` ticks early).
 * Mutually exclusive with cutoff, so clear cutoff. */
unsigned char* Fendit(channel_t* cp, unsigned char* ptr) {
  cp->endit = *ptr++;
  cp->cutoff = 0;
  return (ptr);
}

/* Gate the note as a fixed 16-bit span measured from its start. Mutually
 * exclusive with endit, so clear endit. */
unsigned char* Fcutoff(channel_t* cp, unsigned char* ptr) {
  short tmp;
  tmp = (*ptr++) << 8;
  tmp |= *ptr++;
  cp->cutoff = tmp;
  cp->endit = 0;
  return (ptr);
}

/* Configure vibrato that sweeps pitch upward: onset delay, oscillation speed,
 * and depth (amount/50). vib_precalc caches one cycle's angular step
 * (2*pi/speed) so the per-frame update avoids the division. */
unsigned char* Fvibup(channel_t* cp, unsigned char* ptr) {
  cp->vib_delay = *ptr++;
  cp->vib_speed = *ptr++;
  cp->vib_amount = ((float)*ptr++) / 50.0;
  cp->vib_precalc = (2 * 3.1415926) / (float)cp->vib_speed;
  return (ptr);
}

/* Same as Fvibup but with negative depth, so the vibrato leans pitch downward.
 */
unsigned char* Fvibdown(channel_t* cp, unsigned char* ptr) {
  cp->vib_delay = *ptr++;
  cp->vib_speed = *ptr++;
  cp->vib_amount = (-((float)*ptr++)) / 50.0;
  cp->vib_precalc = (2 * 3.1415926) / (float)cp->vib_speed;
  return (ptr);
}

/* Turn vibrato off. */
unsigned char* Fviboff(channel_t* cp, unsigned char* ptr) {
  cp->vib_speed = 0;
  cp->vibrato = 0;
  return (ptr);
}

/* Set a fixed note length for following notes (7- or 15-bit), so notes in the
 * stream need not carry their own duration. */
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

/* Skip the next note event on this channel; the flag self-clears once the
 * skipped note is reached. */
unsigned char* Fignore(channel_t* cp, unsigned char* ptr) {
  cp->ignore = 1;
  return (ptr);
}

/* Set the transpose offset (in semitones) added to following note numbers. */
unsigned char* Ftrans(channel_t* cp, unsigned char* ptr) {
  cp->transpose = *ptr++;
  return (ptr);
}

/* Suppress transposition for this channel (cancels the transpose offset). */
unsigned char* Fignore_trans(channel_t* cp, unsigned char* ptr) {
  cp->ignore_transpose = 1;
  return (ptr);
}

/* Apply a persistent frequency detune. The signed byte (in hundredths of a
 * semitone) replaces the previous distort: back the old value out of
 * freqoffset before folding the new one in, so freqoffset tracks only the
 * current detune. */
unsigned char* Fdistort(channel_t* cp, unsigned char* ptr) {
  int c;
  float f;
  c = (int)(*ptr++);
  if (c & 0x80) {
    c |= 0xffffff00; /* sign-extend the 8-bit operand */
  }
  f = (float)(c) / 100.0;
  cp->freqoffset -= cp->distort;
  cp->freqoffset += f;
  cp->distort = f;
  return (ptr);
}

/* Stop the envelope from re-triggering on each new note. */
unsigned char* Fenvoff(channel_t* cp, unsigned char* ptr) {
  cp->env_trigger_off = 1;
  return (ptr);
}

/* Allow the envelope to re-trigger on each new note again. */
unsigned char* Fenvon(channel_t* cp, unsigned char* ptr) {
  cp->env_trigger_off = 0;
  return (ptr);
}

/* Suppress note re-triggering (hold the current voice across notes). */
unsigned char* Ftroff(channel_t* cp, unsigned char* ptr) {
  cp->trigger_off = 1;
  return (ptr);
}

/* Allow notes to re-trigger normally again. */
unsigned char* Ftron(channel_t* cp, unsigned char* ptr) {
  cp->trigger_off = 0;
  return (ptr);
}

/* FOR: open a repeat block. Push the iteration count (from the stream) and a
 * full snapshot of the restart state -- the loop-back pointer plus the volume
 * and pitch-bend envelope cursors and their repeat counts -- onto the FOR/NEXT
 * stack, so a later Fnext can rewind to exactly here. */
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

/* NEXT: close the innermost repeat block. Decrement the loop counter on top of
 * the FOR/NEXT stack (a count of 0xff loops forever). While iterations remain,
 * restore the snapshot Ffor took -- code pointer and envelope state -- and jump
 * back; once exhausted, pop the frame and fall through past the block. */
unsigned char* Fnext(channel_t* cp, unsigned char* ptr) {
  int index;
  index = cp->for_stack_count - 1;
  if (cp->for_count[index] != 0xff) {
    if (--(cp->for_count[index]) == 0) {
      cp->for_stack_count = index;
      index = -1; /* sentinel: loop finished, do not rewind */
    }
  }
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

/* Configure wobble modulation: depth plus separate ramp-on and ramp-off
 * speeds. */
unsigned char* Fwobble(channel_t* cp, unsigned char* ptr) {
  cp->wobble_amount = *ptr++;
  cp->wobble_on_speed = *ptr++;
  cp->wobble_off_speed = *ptr++;
  return (ptr);
}

/* Stop wobble by zeroing its ramp-on speed. */
unsigned char* Fwobbleoff(channel_t* cp, unsigned char* ptr) {
  cp->wobble_on_speed = 0;
  return (ptr);
}

/* Enable per-note velocity: following notes carry their own velocity byte. */
unsigned char* Fvelon(channel_t* cp, unsigned char* ptr) {
  cp->velocity_on = 1;
  return (ptr);
}

/* Disable per-note velocity. */
unsigned char* Fveloff(channel_t* cp, unsigned char* ptr) {
  cp->velocity_on = 0;
  return (ptr);
}

/* Set a fixed default velocity for following notes and stop reading per-note
 * velocity bytes. */
unsigned char* Fvelocity(channel_t* cp, unsigned char* ptr) {
  cp->default_velocity = *ptr++;
  cp->velocity_on = 0;
  return (ptr);
}

/* Set the stereo pan; the stream value is halved into the player's pan range.
 */
unsigned char* Fpan(channel_t* cp, unsigned char* ptr) {
  cp->pan = (*ptr++) / 2;
  return (ptr);
}

/* Reserved stereo opcode: no state change, just skip its two operand bytes. */
unsigned char* Fstereo(channel_t* cp, unsigned char* ptr) { return (ptr + 2); }

/* Point the channel at a drum-kit entry in the song's drum table (7- or 15-bit
 * index), so following notes play percussion voices. */
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

/* Leave drum mode; following notes play the melodic wave again. */
unsigned char* Fdrumsoff(channel_t* cp, unsigned char* ptr) {
  cp->pdrums = NULL;
  return (ptr);
}

/* Debug trace opcode: print the operand byte (with the channel frame) when
 * audio debugging is built in; otherwise just consume the byte. */
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

/* Branch (loop/jump) within the sequence. Reads three big-endian 16-bit
 * offsets: the new code position, plus fresh positions for the volume and
 * pitch-bend envelope streams relative to their bases. Resets the
 * continuous-envelope repeat counters and returns the new code pointer. */
unsigned char* Fgoto(channel_t* cp, unsigned char* ptr) {
  int off;
  int off1;

  /* Branch target into the channel's code stream. */
  off1 = *ptr++ << 8;
  off1 += *ptr++;

  /* Reposition the volume envelope cursor to its base + offset. */
  off = *ptr++ << 8;
  off += *ptr++;
  cp->pvolume = cp->pvolumebase + off;
  cp->cont_vol_repeat_count = 1;

  /* Reposition the pitch-bend envelope cursor likewise. */
  off = *ptr++ << 8;
  off += *ptr++;
  cp->ppitchbend = cp->ppitchbendbase + off;
  cp->cont_pb_repeat_count = 1;

  return (cp->pbase + off1);
}

/* Set the channel's reverb send level (0 = dry). */
unsigned char* Freverb(channel_t* cp, unsigned char* ptr) {
  cp->reverb = *ptr++;
  return (ptr);
}

/* Randomize the transpose: a random spread (0..arg) plus a base offset. */
unsigned char* FrandNote(channel_t* cp, unsigned char* ptr) {
  cp->transpose = __MusIntRandom(*ptr++);
  cp->transpose += *ptr++;
  return (ptr);
}

/* Randomize the volume: a random spread (0..arg) plus a base level. */
unsigned char* FrandVolume(channel_t* cp, unsigned char* ptr) {
  cp->volume = __MusIntRandom(*ptr++);
  cp->volume += *ptr++;
  return (ptr);
}

/* Randomize the pan: a random spread (0..arg) plus a base position. */
unsigned char* FrandPan(channel_t* cp, unsigned char* ptr) {
  cp->pan = __MusIntRandom(*ptr++);
  cp->pan += *ptr++;
  return (ptr);
}

/* Set the channel volume directly. */
unsigned char* Fvolume(channel_t* cp, unsigned char* ptr) {
  cp->volume = *ptr++;
  return (ptr);
}

/* Launch a sound effect from inside a song. Reads the effect number (7- or
 * 15-bit), then starts it on a free channel using this channel's volume/pan
 * scaling at a momentarily raised priority. If a channel was claimed, retag
 * its handle and sample bank to this channel's, so the effect plays under the
 * song's identity. */
unsigned char* Fstartfx(channel_t* cp, unsigned char* ptr) {
  int i;
  int number;
  channel_t* sp;
  unsigned long new_handle;
  number = *ptr++;
  if (number >= 0x80) {
    number = ((number & 0x7f) << 8) + *ptr++;
  }
  cp->priority++; /* spawn one priority above the song channel */
  new_handle = __MusIntFindChannelAndStart(cp->fx_addr, number, cp->volscale,
                                           cp->panscale, cp->priority);
  cp->priority--;
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

/* Set the pitch-bend range in semitones (arg/64) and refresh the cached bend
 * amount (pitchbend * range) used by the per-frame pitch update. */
unsigned char* Fbendrange(channel_t* cp, unsigned char* ptr) {
  cp->bendrange = (float)(*ptr++) * (1.0 / 64.0);
  cp->pitchbend_precalc = cp->pitchbend * cp->bendrange;
  return (ptr);
}

/* Set the pitch-sweep rate. */
unsigned char* Fsweep(channel_t* cp, unsigned char* ptr) {
  cp->sweep_speed = *ptr++;
  return (ptr);
}

/* Switch the active custom effect type, when effect-change support is built in
 * and globally enabled. The type byte is always consumed so the stream stays
 * aligned regardless of the build flag. */
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

/* Sequence marker: read the marker number and the following rest (a 7- or
 * 15-bit delay). Only the song's master track fires the user callback, and
 * only while the channel is not paused. */
unsigned char* Fmarker(channel_t* cp, unsigned char* ptr) {
  int rest;
  int number;
  number = *ptr++;
  rest = *ptr++;
  if (rest & 0x80) {
    rest &= 0x7f;
    rest <<= 8;
    rest |= *ptr++;
  }
  if ((cp->channel_flag & CHFLAG_MASTERTRACK) &&
      !(cp->channel_flag & CHFLAG_PAUSE)) {
    if (marker_callback) {
      marker_callback(cp->handle, number);
    }
  }
  return (ptr);
}

/* Clear the fixed note length; notes carry their own duration again. */
unsigned char* Flength0(channel_t* cp, unsigned char* ptr) {
  cp->fixed_length = 0;
  return (ptr);
}
