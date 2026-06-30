/*
 * player.c
 *
 * The core libmus sequence player: the engine that turns compiled music data
 * into a stream of synthesizer voice commands, one audio frame at a time.
 *
 * The file is organized in four bands:
 *
 *   - Public API (the Mus* routines). The game's interface for starting and
 *     stopping songs and effects, adjusting per-handle volume/pan/tempo/reverb/
 *     pitch, registering sample and effect banks, and querying player state.
 *     These run on the game thread.
 *
 *   - The control FIFO. A single-producer/single-consumer ring that carries
 *     messages (pause, unpause, change-fx) from the game thread across to the
 *     audio thread, so channel state is only ever mutated on the audio side.
 *
 *   - The frame handler (__MusIntMain) and its per-note helpers. Called by the
 *     synth driver once per audio frame; it advances every active channel's
 *     clock, pulls new notes, runs the ADSR / vibrato / wobble / sweep /
 *     portamento processors, and pushes the resulting pitch, volume and pan to
 *     the synthesizer voices.
 *
 *   - The sequence interpreter and channel bookkeeping. __MusIntGetNewNote
 *     decodes the byte-coded sequence stream (note and length events plus the
 *     0x80+ command bytes dispatched through `jumptable`), while the find /
 *     start / initialise routines allocate and arm channel slots. A small LFSR
 *     PRNG and the pointer-bank relocator round it out.
 *
 * Sequence timing runs on a 32-bit channel_frame accumulator measured in
 * 1/256-frame ticks; frame stamps are compared with the wraparound-safe idiom
 * (s32)(a - b) < 0 throughout. The individual 0x80+ command handlers live in
 * player_commands.c.
 */

#include "player_priv.h"

/*
 * Unpack a 7-byte ADSR descriptor into a channel's envelope state. Alongside
 * the raw speed and level bytes, this precomputes the per-tick slopes the
 * envelope processor needs, keeping the frame loop multiply-light:
 * env_speed_calc is the reciprocal pace, and the *_calc floats are the volume
 * deltas covered per envelope step of each phase.
 */
static void env_set_adsr(channel_t* cp, unsigned char* ptr) {
  unsigned char value;

  value = *ptr++;
  if (value == 0) value = 1; /* never divide by zero below */
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

/*
 * Sequence command handler for the envelope-select opcode: read an envelope-
 * table index from the stream (7-bit, or 15-bit when the first byte's high bit
 * is set) and apply that entry from the song's env_table. Returns the advanced
 * cursor.
 */
unsigned char* Fenvelope(channel_t* cp, unsigned char* ptr) {
  int tmp;

  tmp = *ptr++;
  if (tmp & 0x80) { /* high bit: a second index byte follows */
    tmp &= 0x7f;
    tmp <<= 8;
    tmp |= *ptr++;
  }
  env_set_adsr(cp, &cp->song_addr->env_table[tmp * 7]);
  return (ptr);
}

/* Globals seeded at startup, and the routines MusInitialize wires together. */
extern OSPiHandle* diskrom_handle;
extern unsigned long __muscontrol_flag;
extern ALPlayer plr_player;
extern void __MusIntFifoOpen(int);
extern void MusPtrBankInitialize(void*, void*);
extern void MusFxBankInitialize(void*);
extern void MusSetMasterVolume(unsigned long, int);
extern ALMicroTime __MusIntMain(void*);

/*
 * Bring the whole music driver up from a caller-supplied configuration. Records
 * the disk-ROM handle and control flags, derives the audio frame period from
 * the TV standard (50 Hz PAL / 60 Hz NTSC), lays down the heap and scheduler,
 * allocates the synthesizer voices and the channel array, opens the command
 * FIFO, installs the default sample and effect banks, registers __MusIntMain as
 * the synth driver's per-frame callback, and gives every non-song channel a
 * voice. Returns the number of heap bytes still free.
 */
int MusInitialize(musConfig* config) {
  ALVoiceConfig vc;
  int i;

  diskrom_handle = config->diskrom_handle;
  __muscontrol_flag = config->control_flag;

  /* The first MAX_SONGS channels are reserved for song master (timing) tracks;
   * the audio frame rate follows the TV standard. */
  max_channels = config->channels + MAX_SONGS;
  if (osTvType == 0)
    mus_vsyncs_per_second = 50;
  else
    mus_vsyncs_per_second = 60;
  mus_next_frame_time = 1000000 / mus_vsyncs_per_second;

  __MusIntMemInit(config->heap, config->heap_length);
  __MusIntSchedInit(config->sched);

  /* Voices back only the non-song channels; channel slots cover all of them. */
  mus_voices =
      __MusIntMemMalloc((max_channels - MAX_SONGS) * sizeof(N_ALVoice));
  mus_channels = __MusIntMemMalloc(max_channels * sizeof(channel_t));
  mus_channels2 = mus_channels + MAX_SONGS;
  __MusIntFifoOpen(config->fifo_length);

  mus_default_bank = mus_init_bank = NULL;
  if (config->ptr && config->wbk)
    MusPtrBankInitialize(config->ptr, config->wbk);
  libmus_fxheader_current = libmus_fxheader_single = NULL;
  if (config->default_fxbank) MusFxBankInitialize(config->default_fxbank);

  marker_callback = NULL;
  mus_last_fxtype = AL_FX_BIGROOM;
  __MusIntAudManInit(config, mus_vsyncs_per_second, mus_last_fxtype);
  MusSetMasterVolume(MUSFLAG_EFFECTS | MUSFLAG_SONGS, 0x7fff);
  mus_current_handle = 1;
  mus_random_seed = 0x12345678;

  /* Register the per-frame handler with the synth driver. */
  plr_player.next = NULL;
  plr_player.handler = __MusIntMain;
  plr_player.clientData = &plr_player;
  alSynAddPlayer(&__libmus_alglobals.drvr, &plr_player);

  /* Reset each channel and allocate a voice for every non-song slot. */
  for (i = 0; i < max_channels; i++) {
    mus_channels[i].playing = 0;
    __MusIntInitialiseChannel(&mus_channels[i]);
    vc.unityPitch = 0;
    vc.priority = config->thread_priority;
    vc.fxBus = 0;
    if (i >= MAX_SONGS)
      alSynAllocVoice(&__libmus_alglobals.drvr, &mus_voices[i - MAX_SONGS],
                      &vc);
  }
  return (__MusIntMemRemaining());
}

/* Set the master volume applied to effects and/or songs, selected by flags. */
void MusSetMasterVolume(unsigned long flags, int volume) {
  if (flags & MUSFLAG_EFFECTS) mus_master_volume_effects = volume;
  if (flags & MUSFLAG_SONGS) mus_master_volume_songs = volume;
}

/* Start a song from the top and release it from its initial pause so it begins
 * playing at once. */
musHandle MusStartSong(void* addr) {
  musHandle handle;

  handle = __MusIntStartSong(addr);
  MusHandleUnPause(handle);
  return (handle);
}

/*
 * Start a song but begin playback at a named marker instead of the top. After
 * arming the song's channels, this silently fast-forwards each one through the
 * sequence stream -- executing command bytes and accumulating note lengths into
 * channel_frame -- until the marker opcode carrying `marker` is reached, then
 * primes the note current there so audio resumes exactly at that point.
 */
musHandle MusStartSongFromMarker(void* addr, int marker) {
  musHandle handle;
  unsigned char command, *ptr;
  int i, note;
  channel_t* cp;

  handle = __MusIntStartSong(addr);
  for (i = 0, cp = mus_channels; i < max_channels; i++, cp++) {
    if (cp->handle == handle && cp->song_addr == (song_t*)addr && (cp->pdata)) {
      /* Walk the stream up to the marker without producing any sound. */
      while (cp->pdata) {
        ptr = cp->pdata;
        if (*ptr >= 128) { /* a command byte, not a note */
          if (*ptr == Cmarker && *(ptr + 1) == marker) break;
          command = *ptr++;
          cp->pdata = (jumptable[command & 0x7f].func)(cp, ptr);
          continue;
        }
        note = *(cp->pdata++);
        if (cp->velocity_on) {
          unsigned char vel = *(cp->pdata++);
          cp->velocity = vel;
          if (vel >= 0x80) { /* high bit latches a new default velocity */
            cp->velocity = vel & 0x7f;
            cp->velocity_on = 0;
            cp->default_velocity = cp->velocity;
          }
        } else {
          cp->velocity = cp->default_velocity;
        }
        if (cp->fixed_length && !cp->ignore) {
          cp->length = cp->fixed_length;
        } else {
          cp->ignore = 0;
          command = *(cp->pdata++);
          /* note length: one byte, or 15 bits when the high bit is set */
          if (command < 128)
            cp->length = command;
          else
            cp->length = ((int)(command & 0x7f) << 8) + *(cp->pdata++);
        }
        cp->channel_frame += cp->length * 256;
      }

      /* Prime the note that straddles the marker so playback resumes here. */
      cp->note_end_frame = cp->channel_frame;
      if (cp->pdata) {
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
      if (cp->pvolume) __MusIntProcessContinuousVolume(cp);
      if (cp->ppitchbend) __MusIntProcessContinuousPitchBend(cp);
    }
  }
  MusHandleUnPause(handle);
  return (handle);
}

/*
 * Start effect `number` from the active effect bank at default volume and pan.
 * Prefers a one-shot bank set with MusFxBankSetSingle, otherwise the current
 * bank; with no bank selected there is nothing to play. Returns the new handle.
 */
musHandle try_spawn_global_object(int number) {
  musHandle handle;
  fx_header_t* header;

  if (libmus_fxheader_single) { /* a one-shot bank consumes itself */
    header = libmus_fxheader_single;
    libmus_fxheader_single = NULL;
  } else {
    header = libmus_fxheader_current;
    if (!header) {
      mus_init_bank = NULL;
      return (0);
    }
  }
  if (!mus_init_bank) mus_init_bank = header->ptr_addr;
  handle = __MusIntFindChannelAndStart(header, number, 0x80, 0x80, -1);
  mus_init_bank = NULL;
  return (handle);
}

/*
 * Start effect `number` with explicit volume, pan and priority. When
 * restartflag is set and the same effect is already sounding from this bank, it
 * is retriggered on its existing channel rather than allocating a new one.
 * Returns the new handle (or 0 if no channel was available).
 */
musHandle MusStartEffect2(int number, int volume, int pan, int restartflag,
                          int priority) {
  int i;
  channel_t* cp;
  musHandle handle;
  fx_header_t* header;

  if (libmus_fxheader_single) {
    header = libmus_fxheader_single;
    libmus_fxheader_single = NULL;
  } else {
    header = libmus_fxheader_current;
    if (!header) {
      mus_init_bank = NULL;
      return (0);
    }
  }
  if (!mus_init_bank) mus_init_bank = header->ptr_addr;

  /* When asked, retrigger an already-playing instance of this effect. */
  if (restartflag) {
    for (i = MAX_SONGS, cp = mus_channels2; i < max_channels; i++, cp++) {
      if (cp->fx_number == number && cp->fx_addr == header) {
        if (priority == -1) priority = header->effects[number].priority;
        handle = __MusIntStartEffect(cp, header, number, volume, pan, priority);
        mus_init_bank = NULL;
        return (handle);
      }
    }
  }
  handle = __MusIntFindChannelAndStart(header, number, volume, pan, priority);
  mus_init_bank = NULL;
  return (handle);
}

/*
 * Begin stopping songs and/or effects, fading them out over `speed` frames
 * (speed 0 stops on the next frame). A channel that is currently paused is
 * released immediately rather than ramped. Channels already stopping
 * (stopping != -1) are left alone.
 */
void MusStop(unsigned long flags, int speed) {
  int i, speed2;
  channel_t* cp;

  speed2 = speed ? speed : 1;
  for (i = 0, cp = mus_channels; i < max_channels; i++, cp++) {
    /* effect channels for EFFECTS, song channels for SONGS */
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

/* Count the channels currently sounding in the requested categories. */
int MusAsk(unsigned long flags) {
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

/*
 * Stop every channel belonging to `handle`, fading over `speed` frames; a
 * paused channel is released at once. Returns how many channels were affected.
 */
int MusHandleStop(musHandle handle, int speed) {
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

/* Count the channels still alive under `handle`. */
int MusHandleAsk(musHandle handle) {
  channel_t* cp;
  int i, count;

  if (!handle) return (0);
  for (i = 0, cp = mus_channels, count = 0; i < max_channels; i++, cp++)
    if (cp->handle == handle) count++;
  return (count);
}

/* Scale the playback volume of every channel under `handle`. */
int MusHandleSetVolume(musHandle handle, int volume) {
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

/* Re-pan every channel under `handle`; old_pan is invalidated to force the new
 * value out on the next update. */
int MusHandleSetPan(musHandle handle, int pan) {
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

/* Apply a frequency (detune) offset to every channel under `handle`, biased by
 * each channel's own distort amount. */
int MusHandleSetFreqOffset(musHandle handle, float offset) {
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

/* Re-scale tempo (clamped to 1..256, where 128 is normal speed) for every
 * channel under `handle`. */
int MusHandleSetTempo(musHandle handle, int tempo) {
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

/* Set the reverb send level (clamped to 0..127) for every channel under
 * `handle`; old_reverb is invalidated to force a resend on the next note. */
int MusHandleSetReverb(musHandle handle, int reverb) {
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

/* Relocate a sample (pointer) bank in place and adopt it as the default bank if
 * none has been chosen yet. */
void MusPtrBankInitialize(void* pbank, void* wbank) {
  __MusIntRemapPtrBank(pbank, wbank);
  if (!mus_default_bank) mus_default_bank = pbank;
}

/* Arm a sample bank for the very next start only (a one-shot override). Banks
 * that have not been relocated yet are ignored. Returns the armed bank. */
void* MusPtrBankSetSingle(void* ipbank) {
  ptr_bank_t* pptr;

  if (ipbank) {
    pptr = (ptr_bank_t*)ipbank;
    if (pptr->flags & PTRFLAG_REMAPPED) mus_init_bank = pptr;
  }
  return (mus_init_bank);
}

/* Make `ipbank` the default sample bank for subsequent starts. */
void MusPtrBankSetCurrent(void* ipbank) {
  ptr_bank_t* pptr;

  if (ipbank) {
    pptr = (ptr_bank_t*)ipbank;
    if (pptr->flags & PTRFLAG_REMAPPED) mus_default_bank = pptr;
  }
}

/* Return the current default sample bank. */
void* MusPtrBankGetCurrent(void) { return (mus_default_bank); }

/* Return the sample bank in use by `handle`'s first channel, or NULL. */
void* MusHandleGetPtrBank(musHandle handle) {
  channel_t* cp;
  int i, count;

  if (!handle) return (NULL);
  for (i = 0, cp = mus_channels, count = 0; i < max_channels; i++, cp++)
    if (cp->handle == handle) return (cp->sample_bank);
  return (NULL);
}

/* Request that `handle` pause. The actual state change happens on the audio
 * thread: this only enqueues a FIFO command for __MusIntFifoProcess to apply
 * there. Returns 0 if the FIFO was full. */
int MusHandlePause(musHandle handle) {
  fifo_t fifo_command;

  fifo_command.command = FIFOCMD_PAUSE;
  fifo_command.data = handle;
  return (__MusIntFifoAddCommand(&fifo_command));
}

/* Request that `handle` resume; queued for the audio thread like pause. */
int MusHandleUnPause(musHandle handle) {
  fifo_t fifo_command;

  fifo_command.command = FIFOCMD_UNPAUSE;
  fifo_command.data = handle;
  return (__MusIntFifoAddCommand(&fifo_command));
}

/* Queue a global effect-type (reverb preset) change for the audio thread and
 * remember it as the last type set. */
int MusSetFxType(int fxtype) {
  fifo_t fifo_command;

  fifo_command.command = FIFOCMD_CHANGEFX;
  fifo_command.data = fxtype;
  mus_last_fxtype = fxtype;
  return (__MusIntFifoAddCommand(&fifo_command));
}

/* Enable or disable a song's ability to change the effect type. Turning it off
 * first restores the last app-selected effect type. */
int MusSetSongFxChange(musBool onoff) {
  int changed;

  changed = 1;
  if (onoff == MUSBOOL_OFF) changed = MusSetFxType(mus_last_fxtype);
  if (changed) mus_songfxchange_flag = onoff;
  return (changed);
}

/*
 * Prepare an effect bank the first time it is seen: mark it initialised, then
 * convert its file-relative wave-table and per-effect data offsets into
 * absolute pointers. An already-initialised bank is left untouched, and the
 * first bank seen becomes the current one.
 */
void MusFxBankInitialize(void* fxbank) {
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

/* Report how many effects a bank defines. */
int MusFxBankNumberOfEffects(void* ifxbank) {
  return (((fx_header_t*)ifxbank)->number_of_effects);
}

/* Select the current effect bank. */
void MusFxBankSetCurrent(void* ifxbank) {
  libmus_fxheader_current = (fx_header_t*)ifxbank;
}

/* Arm an effect bank for the next start only. */
void MusFxBankSetSingle(void* ifxbank) {
  libmus_fxheader_single = (fx_header_t*)ifxbank;
}

/* Return the current effect bank. */
void* MusFxBankGetCurrent(void) { return (libmus_fxheader_current); }

/* Bind a sample (pointer) bank to an effect bank. */
void MusFxBankSetPtrBank(void* ifxbank, void* ipbank) {
  ((fx_header_t*)ifxbank)->ptr_addr = (ptr_bank_t*)ipbank;
}

/* Return the sample bank bound to an effect bank. */
void* MusFxBankGetPtrBank(void* ifxbank) {
  return (((fx_header_t*)ifxbank)->ptr_addr);
}

/* Install the caller's audio scheduler callback set. */
void MusSetScheduler(musSched* sched_list) {
  __libmus_current_sched = sched_list;
}

/* Return the number of waves in the bank backing `handle` (song or effect). */
int MusHandleWaveCount(musHandle handle) {
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

/* Return the wave-table address of the bank backing `handle` (song or effect).
 */
unsigned short* MusHandleWaveAddress(musHandle handle) {
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

/* Register the callback invoked when a master track passes a marker opcode. */
void MusSetMarkerCallback(void* callback) {
  marker_callback = (LIBMUScb_marker)callback;
}

/* MG64-specific hook: forwards to a game-side audio routine. */
void func_8009A630(void) { func_8009DBA0(); }

/*
 * Allocate the control FIFO: a ring of command slots sized by the caller and
 * clamped to [MIN_FIFO_COMMANDS, MAX_FIFO_COMMANDS]. The ring starts empty.
 */
void __MusIntFifoOpen(int commands) {
  if (commands < MIN_FIFO_COMMANDS) {
    commands = MIN_FIFO_COMMANDS;
  } else if (commands > MAX_FIFO_COMMANDS) {
    commands = MAX_FIFO_COMMANDS;
  }
  fifo_addr = __MusIntMemMalloc(commands * sizeof(fifo_t));
  fifo_limit = commands;
  fifo_start = fifo_current = 0;
}

void __MusIntFifoProcessCommand(fifo_t* command); /* defined just below */

/*
 * Drain the control FIFO on the audio thread, dispatching every queued command
 * and wrapping the read cursor around the ring. Runs once at the top of each
 * audio frame so game-thread requests take effect in step with playback.
 */
static void __MusIntFifoProcess(void) {
  if (fifo_start == fifo_current) return;
  while (fifo_start != fifo_current) {
    __MusIntFifoProcessCommand(&fifo_addr[fifo_start]);
    fifo_start++;
    if (fifo_start == fifo_limit) fifo_start = 0;
  }
}

/* Apply one queued control command: pause, unpause, or change effect type. */
void __MusIntFifoProcessCommand(fifo_t* command) {
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

/*
 * Producer side of the control FIFO, called from the game thread: copy one
 * command into the ring and advance the write cursor. Returns 0 (dropping the
 * command) when the ring is full, so the write cursor never overruns the read
 * cursor.
 */
int __MusIntFifoAddCommand(fifo_t* command) {
  int index;

  index = (fifo_current + 1) % fifo_limit;
  if (index == fifo_start) { /* next slot is the consumer's: ring is full */
    return (0);
  }
  __MusIntMemMove(&fifo_addr[fifo_current], command, sizeof(fifo_t));
  fifo_current = index;
  return (1);
}

/* MG64-specific accessor: return the player's running frame counter. */
int func_8009A7C8(void) { return g_mus_frame_counter; }

/* Per-note processors the frame loop calls into, plus the CPU-time gauges it
 * updates on each pass. */
extern void __MusIntGetNewNote(channel_t*, int);
extern void __MusIntSetPitch(channel_t*, int, float);
extern void __MusIntProcessEnvelope(channel_t*);
extern void __MusIntProcessSweep(channel_t*);
extern float __MusIntProcessWobble(channel_t*);
extern float __MusIntProcessVibrato(channel_t*);
extern void __MusIntSetVolumeAndPan(channel_t*, int);
extern u32 _mus_cpu_last;
extern u32 _mus_cpu_worst;

/*
 * The player's heartbeat: the synth driver calls this once per audio frame. It
 * drains pending control commands, then walks every active channel -- advancing
 * its clock by the current tempo, pulling new notes as their time arrives,
 * running the continuous volume and pitch-bend streams, servicing stop fades,
 * and (for sounding channels) stepping the envelope, sweep, vibrato and wobble
 * before committing pitch and volume/pan to the channel's voice. Returns the
 * delay until the next frame, and tracks the worst-case CPU time spent here.
 */
ALMicroTime __MusIntMain(void* node) {
  int x;
  channel_t* cp;
  u32 start = osGetCount();

  g_mus_frame_counter++;
  __MusIntFifoProcess();

  /* x is the voice index; the MAX_SONGS song-master slots sit at negative
   * indices and never start a voice. */
  for (x = -MAX_SONGS, cp = mus_channels; x < max_channels - MAX_SONGS;
       x++, cp++) {
    if (cp->pdata == NULL || (cp->channel_flag & CHFLAG_PAUSE)) continue;
    if (cp->pending) __MusIntFlushPending(cp, x);

    cp->channel_frame += cp->channel_tempo;
    /* 0x7fff is a held/infinite note that never auto-advances. */
    if (cp->length != 0x7fff) {
      /* Pull notes until the channel clock catches up (wraparound-safe). */
      while ((s32)(cp->note_end_frame - cp->channel_frame) < 0 &&
             cp->pdata != NULL)
        __MusIntGetNewNote(cp, x);
      if (!cp->pdata) continue;
    }

    if (cp->pvolume && (s32)(cp->volume_frame - cp->channel_frame) < 0)
      __MusIntProcessContinuousVolume(cp);
    if (cp->ppitchbend && (s32)(cp->pitchbend_frame - cp->channel_frame) < 0)
      __MusIntProcessContinuousPitchBend(cp);

    /* Count down an in-progress stop fade; at -1 the channel is fully torn
     * down and its voice silenced. */
    if (cp->stopping != -1) {
      cp->stopping--;
      if (cp->stopping == -1) {
        cp->pvolume = NULL;
        cp->ppitchbend = NULL;
        cp->song_addr = NULL;
        cp->fx_addr = NULL;
        cp->handle = 0;
        cp->pending = NULL;
        cp->pdata = NULL;
        if (cp->playing) {
          cp->playing = 0;
          alSynStopVoice(&__libmus_alglobals.drvr, mus_voices + x);
        }
      }
    }

    /* Update a sounding voice: envelope, modulation, then pitch and level. */
    if (cp->playing) {
      float total;
      if (cp->env_phase) __MusIntProcessEnvelope(cp);
      if (cp->sweep_speed && (s32)(cp->sweep_frame - cp->channel_frame) < 0)
        __MusIntProcessSweep(cp);
      total = cp->freqoffset;
      if (cp->vib_speed) total += __MusIntProcessVibrato(cp);
      if (cp->wobble_on_speed) total += __MusIntProcessWobble(cp);
      if (!cp->pending) {
        __MusIntSetPitch(cp, x, total);
        __MusIntSetVolumeAndPan(cp, x);
      }
    }

    /* whole frames elapsed since the note began (ticks are 1/256 frame) */
    cp->count = (cp->channel_frame - cp->note_start_frame) >> 8;
  }

  _mus_cpu_last = osGetCount() - start;
  if (_mus_cpu_last > _mus_cpu_worst) _mus_cpu_worst = _mus_cpu_last;
  return (mus_next_frame_time);
}

/*
 * Advance one channel's sequence interpreter to its next note. First runs out
 * any pending command bytes (>= 128) through `jumptable`, then decodes the note
 * value, its velocity and its length, schedules the note's frame window, and --
 * unless the channel is resting -- resolves the wave (or drum-mapped wave),
 * arms the voice, and computes the base pitch from the note, the bank detune
 * and the channel transpose. A REST starts the release phase; running off the
 * end of the data silences and stops the voice.
 */
void __MusIntGetNewNote(channel_t* cp, int x) {
  unsigned char* ptr;
  unsigned char command;

  /* Execute every leading command byte; the first byte < 128 is the note. */
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
    ptr = (jumptable[command & 0x7f].func)(cp, ptr + 1);
  }
  cp->pdata = ptr;

  if (ptr) {
    int note;
    cp->last_note = cp->port_base;
    note = *(cp->pdata++);

    /* Velocity: a high bit latches it as the new default and turns off the
     * per-note velocity byte. */
    if (cp->velocity_on) {
      u8 vel = *cp->pdata++;
      cp->velocity = vel;
      if (vel >= 0x80) {
        cp->velocity = vel & 0x7F;
        cp->velocity_on = 0;
        cp->default_velocity = cp->velocity;
      }
    } else
      cp->velocity = cp->default_velocity;

    /* Length: a fixed length unless told to read one (one byte, or 15 bits
     * when the high bit is set). */
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

    /* Schedule the note's frame window and reset its modulation state. */
    cp->note_start_frame = cp->note_end_frame;
    cp->note_end_frame += cp->length * 256;
    cp->count = 0;
    cp->wobble_count = cp->wobble_off_speed;
    cp->wobble_current = 0;

    /* An empty (0xffff) wave-table slot means "no instrument" -> rest. */
    if (cp->song_addr && !cp->pdrums) {
      if (cp->song_addr->wave_table[cp->wave] == 0xffff) note = REST;
    }

    if (note != REST) {
      int wave;
      ptr_bank_t* bank;
      bank = cp->sample_bank;

      /* A drum kit remaps the note index to its own wave, pan, ADSR and
       * pitch. */
      if (cp->pdrums != NULL) {
        cp->wave = cp->pdrums[note].wave;
        cp->pan = cp->pdrums[note].pan / 2;
        env_set_adsr(cp, &cp->song_addr->env_table[cp->pdrums[note].adsr * 7]);
        note = cp->pdrums[note].pitch;
      }
      if (!cp->env_trigger_off) __MusIntInitEnvelope(cp);
      if (cp->sweep_speed) __MusIntInitSweep(cp);

      wave = cp->wave;
      if (cp->song_addr)
        wave = cp->song_addr->wave_table[wave];
      else
        wave = cp->fx_addr->wave_table[wave];

      /* Arm the voice with the new wave. If a note is already sounding, fade
       * it out this frame and swap on the next; otherwise start at once. */
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

      /* Base pitch = note + bank detune, plus transpose unless opted out. */
      cp->base_note = (float)note + bank->detune[wave];
      command = cp->transpose * (1 - cp->ignore_transpose);
      cp->base_note += (float)U8_TO_FLOAT(command);

      /* Blend channel reverb toward the base level, then push if it moved. */
      if (cp->reverb != cp->old_reverb) {
        unsigned char work;
        work = cp->reverb_base;
        work += ((128 - work) * cp->reverb) >> 7;
        cp->old_reverb = cp->reverb;
        alSynSetFXMix(&__libmus_alglobals.drvr, mus_voices + x, work);
      }
    } else {
      /* A rest: enter the release phase so the current note decays away. */
      if (cp->env_phase < 4) {
        cp->env_phase = 4;
        cp->release_frame = cp->channel_frame;
        cp->env_count = 1;
        cp->release_start_vol = cp->env_current;
      }
    }
  } else {
    /* End of sequence data: silence and stop the voice. */
    if (cp->playing) {
      cp->playing = 0;
      alSynSetVol(&__libmus_alglobals.drvr, mus_voices + x, 0,
                  mus_next_frame_time);
      alSynStopVoice(&__libmus_alglobals.drvr, mus_voices + x);
    }
  }
}

/*
 * Commit a channel's pending wave to its voice: stop whatever the voice was
 * playing and start the new wave. The two-step "pending then flush" lets the
 * frame loop ramp the previous note down before the swap.
 */
void __MusIntFlushPending(channel_t* cp, int x) {
  if (cp->playing) alSynStopVoice(&__libmus_alglobals.drvr, mus_voices + x);
  cp->playing = 1;
  alSynStartVoice(&__libmus_alglobals.drvr, mus_voices + x, cp->pending);
  cp->pending = NULL;
}

/* MG64-specific: enable stereo panning when mode == 1 (else center). */
void func_8009B0DC(int mode) { g_mus_pan_enabled = (mode == 1); }

/*
 * Compute and push a channel's final voice volume and pan. Volume folds the
 * channel volume, envelope level, velocity and volume scale into a fixed-point
 * product, clamps it, scales by the appropriate master volume, and applies the
 * stop-fade ramp; pan is the channel pan times its scale (or center when stereo
 * panning is off). Each is sent to the synth only when it actually changed.
 */
void __MusIntSetVolumeAndPan(channel_t* cp, int x) {
  u32 volume;

  /* Combine the four 7-bit gains, then renormalize and clamp. */
  volume = ((u32)(cp->volume) * (u32)(cp->env_current) * (u32)(cp->velocity) *
            (u32)(cp->volscale)) >>
           13;
  if (volume > 32767) volume = 32767;
  if (!cp->fx_addr)
    volume *= mus_master_volume_songs;
  else
    volume *= mus_master_volume_effects;
  volume >>= 15; /* fold the 15-bit master volume back down */
  if (cp->stopping != -1) volume = (volume * cp->stopping) / cp->stopping_speed;
  if (volume != cp->old_volume) {
    cp->old_volume = volume;
    alSynSetVol(&__libmus_alglobals.drvr, mus_voices + x, volume,
                mus_next_frame_time);
  }

  volume = 0x40; /* 0x40 is dead center */
  if (g_mus_pan_enabled) volume = ((cp->pan * cp->panscale) >> 7) & 0x7f;
  if (volume != cp->old_pan) {
    cp->old_pan = volume;
    alSynSetPan(&__libmus_alglobals.drvr, mus_voices + x, volume);
  }
}

/*
 * Resolve a channel's final pitch and push it to the voice. Applies any active
 * portamento glide from the previous note toward the target, adds the caller's
 * offset (vibrato/wobble) and the pitch-bend, converts the result from
 * semitones to a frequency ratio via __MusIntPowerOf2(x / 12), clamps the
 * ratio, and sends it only when it changed. An overflowing ratio silences the
 * note.
 */
void __MusIntSetPitch(channel_t* cp, int x, float offset) {
  float frequency, temp;

  /* Portamento: glide linearly from last_note to the target over `port`
   * frames, then hold. */
  frequency = cp->base_note;
  if (cp->port != 0) {
    if (cp->count <= cp->port) {
      temp = (frequency - cp->last_note) / (float)(cp->port);
      temp *= (float)cp->count;
      frequency = cp->last_note + temp;
    }
    cp->port_base = frequency;
  }
  frequency += offset + cp->pitchbend_precalc;
  if (frequency == cp->old_frequency) return;
  cp->old_frequency = frequency;

  frequency =
      __MusIntPowerOf2(frequency * (1.0 / 12.0)); /* semitones -> ratio */
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

/*
 * Arm a channel's ADSR envelope for a new note: choose the release frame from
 * the note's cutoff (absolute) or endit (trim from the end) setting, seed the
 * level at the initial volume, and enter the attack phase. A held note pushes
 * its release effectively out of reach.
 */
void __MusIntInitEnvelope(channel_t* cp) {
  if (cp->length != 0x7fff) {
    if (cp->cutoff != 0)
      cp->release_frame = cp->note_start_frame + (cp->cutoff << 8);
    else
      cp->release_frame = cp->note_end_frame - (cp->endit << 8);
#ifdef _FX_FULL_RELEASE_MODE
    if (cp->fx_addr)
      cp->note_end_frame +=
          (((cp->env_release_speed << 10) / cp->env_speed_calc) << 8);
#endif
  } else {
    cp->release_frame = cp->note_start_frame + 0x7fffffff;
  }
  cp->env_current = cp->env_init_vol;
  cp->env_count = cp->env_speed;
  cp->env_phase = 1;
}

/*
 * Step the ADSR envelope one tick and update env_current. Phase 1 ramps attack
 * up to the max level, phase 2 decays to the sustain level, phase 3 holds, and
 * phase 4 releases to silence; passing the release frame forces an early jump
 * to release. env_count paces the steps so the work runs only every env_speed
 * ticks.
 */
void __MusIntProcessEnvelope(channel_t* cp) {
  int env_phase_count;

  /* Past the release frame? Drop straight into the release phase. */
  if (((long)(cp->release_frame - cp->channel_frame)) < 0 &&
      cp->env_phase < 4) {
    cp->env_phase = 4;
    cp->env_count = 1;
    cp->release_start_vol = cp->env_current;
  }
  cp->env_count--;
  if (cp->env_count) return;
  cp->env_count = cp->env_speed;

  switch (cp->env_phase) {
    case 1: /* attack: ramp from the initial volume up to the max level */
      env_phase_count = (((cp->channel_frame - cp->note_start_frame) >> 8) *
                         cp->env_speed_calc) >>
                        10;
      if (env_phase_count < cp->env_attack_speed) {
        cp->env_current =
            (int)cp->env_init_vol +
            (int)((float)(cp->env_attack_calc * (float)env_phase_count));
        return;
      } else {
        cp->env_phase++;
        cp->env_current = cp->env_max_vol;
        return;
      }
    case 2: /* decay: fall from the max level toward the sustain level */
      env_phase_count = (((cp->channel_frame - cp->note_start_frame) >> 8) -
                         cp->env_attack_speed) *
                            cp->env_speed_calc >>
                        10;
      if (env_phase_count < cp->env_decay_speed) {
        cp->env_current =
            (int)cp->env_max_vol +
            (int)((float)(cp->env_decay_calc * (float)env_phase_count));
        return;
      } else {
        cp->env_phase++;
        cp->env_current = cp->env_sustain_vol;
        return;
      }
    case 3: /* sustain: hold the current level */
      return;
    case 4: /* release: fall from the held level to silence */
      env_phase_count = (((cp->channel_frame - cp->release_frame) >> 8) *
                         cp->env_speed_calc) >>
                        10;
      if (env_phase_count < cp->env_release_speed) {
        cp->env_current =
            (int)cp->release_start_vol -
            (int)((float)(cp->env_release_calc * (float)env_phase_count *
                          (float)cp->release_start_vol));
        return;
      } else {
        cp->env_phase++;
        cp->env_current = 0;
        return;
      }
  }
}

/* Arm a pan sweep: start at the note's frame, reset the timer, and take the
 * initial direction from the pan's top bit. */
void __MusIntInitSweep(channel_t* cp) {
  cp->sweep_frame = cp->note_start_frame;
  cp->sweep_timer = 0;
  cp->sweep_dir = cp->pan & 0x40;
}

/*
 * Advance an automatic pan sweep, catching up to the current frame. Each step
 * accumulates sweep_speed into a 6-bit timer; on overflow it moves pan by the
 * carry and bounces direction at the 0 and 0x7f limits.
 */
void __MusIntProcessSweep(channel_t* cp) {
  unsigned long calc;
  do {
    cp->sweep_frame += 256;
    calc = cp->sweep_timer + cp->sweep_speed;
    if (calc < 64) { /* no whole pan step yet */
      cp->sweep_timer = calc;
      continue;
    }
    cp->sweep_timer = calc & 63;
    calc >>= 6;
    if (!cp->sweep_dir) {
      cp->pan += calc;
      if (cp->pan > 0x7f) { /* hit the right edge: turn around */
        cp->pan = 0x7f;
        cp->sweep_dir = 1;
      }
    } else {
      cp->pan -= calc;
      if (cp->pan >= 0x80 || cp->pan == 0) { /* hit/underflowed left edge */
        cp->pan = 0;
        cp->sweep_dir = 0;
      }
    }
  } while ((long)(cp->sweep_frame - cp->channel_frame) < 0);
}

/*
 * Square-wave tremolo: toggle the wobble amount on and off at its configured
 * on/off speeds. Returns the current wobble offset for the frame's pitch sum.
 */
float __MusIntProcessWobble(channel_t* cp) {
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

/*
 * Sine vibrato: after an initial delay, return a sinusoidal pitch offset whose
 * phase tracks frames since the note began and whose depth is vib_amount.
 * Returns 0 while still inside the delay.
 */
float __MusIntProcessVibrato(channel_t* cp) {
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

/*
 * Step a channel's continuous-volume stream up to the current frame. The stream
 * is run-length coded: a value byte sets the volume and is followed by a repeat
 * count (1, 2 or 3 bytes) of frames to hold it. Catches up frame by frame.
 */
void __MusIntProcessContinuousVolume(channel_t* cp) {
  unsigned char work_vol;
  do {
    cp->volume_frame += 256;
    cp->cont_vol_repeat_count--;
    if (cp->cont_vol_repeat_count == 0) {
      work_vol = *(cp->pvolume++);
      if (work_vol > 127) { /* high bit: a new volume plus a repeat count */
        cp->volume = work_vol & 0x7f;
        work_vol = *(cp->pvolume++);
        if (work_vol > 127) { /* 15-bit repeat count spans two bytes */
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

/*
 * Step a channel's continuous pitch-bend stream up to the current frame. Same
 * run-length format as the volume stream; each value is centered on 64 and
 * scaled by the channel's bend range into pitchbend_precalc.
 */
void __MusIntProcessContinuousPitchBend(channel_t* cp) {
  unsigned char work_pb;
  do {
    cp->pitchbend_frame += 256;
    cp->cont_pb_repeat_count--;
    if (cp->cont_pb_repeat_count == 0) {
      work_pb = *(cp->ppitchbend++);
      if (work_pb > 127) { /* high bit: a new bend value plus a repeat count */
        cp->pitchbend = ((float)(work_pb & 0x7f)) - 64.0;
        cp->pitchbend_precalc = cp->pitchbend * cp->bendrange;
        work_pb = *(cp->ppitchbend++);
        if (work_pb > 127) { /* 15-bit repeat count spans two bytes */
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

/*
 * Evaluate 2^x for the pitch path without a math library, via a sixth-order
 * polynomial in x (the Taylor series of 2^x). A negative exponent is handled by
 * reciprocating the positive-exponent result.
 */
float __MusIntPowerOf2(float x) {
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

/*
 * Relocate a sample (pointer) bank in place the first time it is loaded, since
 * its file is stored with self-relative offsets. Turns the base-note and
 * wave-list offset tables into absolute pointers, folds each wave's coarse
 * base-note and fine detune bytes into a single floating detune, and rebases
 * each wave's sample, loop and ADPCM-book pointers against the wave-data file.
 * Finishes by writing the dirty cache back so the RSP sees the fixed-up bank.
 */
void __MusIntRemapPtrBank(char* pptr, char* wptr) {
  int i;
  ptr_bank_t* ptrfile_addr;
  unsigned char *chardetune, charwork;
  float *floatdetune, floatwork;
  unsigned long base;

  ptrfile_addr = (ptr_bank_t*)pptr;
  if (ptrfile_addr->flags & PTRFLAG_REMAPPED) return; /* relocate once only */
  ptrfile_addr->flags |= PTRFLAG_REMAPPED;
  __MusIntRemapPtrs(&ptrfile_addr->basenote, pptr, 3);
  __MusIntRemapPtrs(&ptrfile_addr->wave_list[0], pptr, ptrfile_addr->count);

  for (i = 0; i < ptrfile_addr->count; i++) {
    /* Detune = fine cents/100 plus the offset from the standard base note. */
    floatdetune = &ptrfile_addr->detune[i];
    chardetune = (unsigned char*)floatdetune;
    charwork = *chardetune;
    floatwork = U8_TO_FLOAT(charwork);
    *floatdetune = floatwork / 100.0;
    charwork = ptrfile_addr->basenote[i] - BASEOFFSET;
    floatwork = U8_TO_FLOAT(charwork);
    *floatdetune += floatwork;

    if (!ptrfile_addr->wave_list[i]->flags) {
      base = (unsigned long)ptrfile_addr->wave_list[i]->base;
      /* 0xff......: already an absolute RAM address; leave it alone. */
      if ((base & 0xff000000) != 0xff000000) {
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
  osWritebackDCacheAll(); /* flush so the RSP reads the relocated bank */
}

/*
 * Pseudo-random integer in [0, range). Runs eight steps of a linear-feedback
 * shift register on the shared seed, then maps the seed to a float in [0, 1)
 * and scales it. The 0x48000000 tap mask with the feedback test xors the high
 * taps back into the low bit.
 */
int __MusIntRandom(int range) {
  unsigned int seed_shifted;
  unsigned int seed_masked;
  int x;
  float f;

  for (x = 0; x < 8; x++) {
    seed_shifted = mus_random_seed << 1;
    seed_masked = mus_random_seed & 0x48000000;
    mus_random_seed = seed_shifted;
    if (seed_masked == 0x48000000 || seed_masked == 0x08000000)
      mus_random_seed = seed_shifted | 1;
  }
  f = (float)(mus_random_seed) / (1 << 16);
  f /= (1 << 16);
  return ((int)((float)(range)*f));
}

/*
 * Reset a channel to its idle defaults. Zeroes the whole structure, then sets
 * the "last sent" sentinels (old_volume / old_pan / old_reverb / old_frequency)
 * so the first real update is always forwarded, installs the default tempo,
 * velocity, volume, pan, scales and a flat envelope, and points the channel at
 * the active sample bank. The playing flag is preserved across the wipe.
 */
void __MusIntInitialiseChannel(channel_t* cp) {
  unsigned char old_playing, *work_ptr;
  int i;

  /* Wipe every field, then re-establish the defaults below. */
  cp->pdata = NULL;
  old_playing = cp->playing;
  work_ptr = (unsigned char*)cp;
  for (i = 0; i < sizeof(channel_t); i++) *work_ptr++ = 0;

  /* Sentinels chosen so the first volume/pan/reverb/pitch update is sent. */
  cp->old_volume = 0xffff;
  cp->old_reverb = 0xff;
  cp->old_pan = 0xff;
  cp->old_frequency = 99.9;

  /* 96 ticks/beat at the standard tempo, scaled to this region's frame rate. */
  cp->channel_tempo = cp->channel_tempo_save = 96 * 256 / mus_vsyncs_per_second;
  cp->length = 1;
  cp->default_velocity = 127;
  cp->volume = 127;
  cp->bendrange = 2 * (1.0 / 64.0);
  cp->pan = 64;
  cp->cont_vol_repeat_count = 1;
  cp->cont_pb_repeat_count = 1;
  cp->stopping = -1;
  cp->volscale = 0x80;
  cp->panscale = 0x80;
  cp->temscale = 0x80;

  /* A flat, full-volume default envelope until a note sets a real one. */
  cp->env_speed = 1;
  cp->env_attack_speed = 1;
  cp->env_attack_calc = 1.0F;
  cp->env_max_vol = 127;
  cp->env_decay_speed = 255;
  cp->env_decay_calc = 1.0 / 255.0;
  cp->env_sustain_vol = 127;
  cp->env_release_speed = 15;
  cp->env_release_calc = 1.0 / 15.0;
  cp->sample_bank = mus_init_bank ? mus_init_bank : mus_default_bank;
  cp->playing = old_playing;
}

/*
 * Pick a channel slot for a new song track. A master track (song_chan < 0) may
 * take one of the reserved song slots; any track then prefers a free effect
 * slot, and failing that evicts the lowest-priority effect. When no slot frees
 * up it reuses the channel that last played this song's track, or falls back to
 * a fixed slot derived from the track index.
 */
int __MusIntFindChannel(song_t* addr, int song_chan) {
  channel_t* cp;
  int i, current, current_channel;

  /* A master track may use one of the reserved song slots. */
  if (song_chan < 0) {
    for (i = 0, cp = mus_channels; i < MAX_SONGS; i++, cp++)
      if (!cp->pdata) return (i);
  }

  /* Otherwise take the first free effect slot. */
  for (i = MAX_SONGS, cp = mus_channels2; i < max_channels; i++, cp++)
    if (!cp->pdata) return (i);

  /* None free: find the lowest-priority effect to evict. */
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

  /* No effect to evict: take a slot used by a different song, else reuse the
   * one that previously held this exact song track. */
  for (i = MAX_SONGS, cp = mus_channels2; i < max_channels; i++)
    if (!cp->fx_addr && cp->song_addr != addr) return (i);
  for (i = MAX_SONGS, cp = mus_channels2; i < max_channels; i++, cp++)
    if (cp->song_addr == addr && addr->data_list[song_chan] == cp->pbase)
      return (i);
  return ((song_chan % (max_channels - MAX_SONGS)) + MAX_SONGS);
}

/* Relocate a table of `count` self-relative offsets into absolute pointers by
 * adding `offset` to each nonzero entry. */
void __MusIntRemapPtrs(void* addr, void* offset, int count) {
  unsigned long *dest, add;
  int i;

  dest = (unsigned long*)addr;
  add = (unsigned long)offset;
  for (i = 0; i < count; i++)
    if (dest[i]) dest[i] += add;
}

/*
 * Arm an already-chosen channel to play effect `number` from `header`: reset
 * it, record the effect number, volume, pan and priority, assign a fresh
 * handle, point pdata at the effect's data, and prefer the bank's own sample
 * bank when it has one. Returns the new handle.
 */
unsigned long __MusIntStartEffect(channel_t* cp, fx_header_t* header,
                                  int number, int volume, int pan,
                                  int priority) {
  __MusIntInitialiseChannel(cp);
  cp->fx_number = number;
  cp->fx_addr = header;
  cp->volscale = volume;
  cp->panscale = pan;
  cp->handle = mus_current_handle++;
  cp->priority = priority;
  if (header->ptr_addr) cp->sample_bank = header->ptr_addr;
  cp->pdata = cp->pbase = header->effects[number].fxdata;
  return (cp->handle);
}

/*
 * Find a channel for effect `number` and start it. Takes the first free effect
 * slot; if none is free it evicts the lowest-priority effect, but only when
 * that priority is below the new effect's. Returns the new handle, or 0 if
 * every channel outranks the request.
 */
musHandle __MusIntFindChannelAndStart(fx_header_t* header, int number,
                                      int volume, int pan, int priority) {
  int i, current_priority;
  channel_t *cp, *current_cp;

  if (priority == -1) priority = header->effects[number].priority;
  current_priority = priority + 1;

  /* Use the first free slot; meanwhile track the weakest effect playing. */
  for (i = MAX_SONGS, cp = mus_channels2; i < max_channels; i++, cp++) {
    if (cp->pdata == NULL) {
      __MusIntInitialiseChannel(cp);
      cp->fx_number = number;
      cp->fx_addr = header;
      cp->volscale = volume;
      cp->panscale = pan;
      cp->handle = mus_current_handle++;
      cp->priority = priority;
      if (header->ptr_addr) cp->sample_bank = header->ptr_addr;
      cp->pdata = cp->pbase = header->effects[number].fxdata;
      return (cp->handle);
    }
    if (cp->fx_addr && cp->priority < current_priority) {
      current_priority = cp->priority;
      current_cp = cp;
    }
  }

  /* No free slot: evict the weakest effect only if we outrank it. */
  if (current_priority < priority) {
    __MusIntInitialiseChannel(current_cp);
    current_cp->fx_number = number;
    current_cp->fx_addr = header;
    current_cp->volscale = volume;
    current_cp->panscale = pan;
    current_cp->handle = mus_current_handle++;
    current_cp->priority = priority;
    if (header->ptr_addr) current_cp->sample_bank = header->ptr_addr;
    current_cp->pdata = current_cp->pbase = header->effects[number].fxdata;
    return (current_cp->handle);
  }
  return (0);
}

/*
 * Start a song and return its handle. On first use it relocates the song's
 * channel / volume / pitch-bend offset tables, then claims a channel for the
 * master (timing) track -- paused and flagged as master -- and one channel per
 * data track, wiring each to its data, volume and pitch-bend streams. Every
 * channel shares the handle and starts paused so the caller can release them
 * together.
 */
musHandle __MusIntStartSong(void* addr) {
  song_t* song_addr;
  int i, channels;
  channel_t* cp;
  unsigned long handle;

  song_addr = addr;
  channels = song_addr->num_channels;

  /* Relocate the song's offset tables once (precedence quirk: this tests
   * (!flags) & 1, i.e. true only when flags == 0 -- the first-use case). */
  if (!song_addr->flags & SONGFLAG_INITIALISED) {
    song_addr->flags |= SONGFLAG_INITIALISED;
    __MusIntRemapPtrs(SONGHDR_ADR(song_addr), addr, SONGHDR_COUNT);
    __MusIntRemapPtrs(song_addr->data_list, addr, channels);
    __MusIntRemapPtrs(song_addr->volume_list, addr, channels);
    __MusIntRemapPtrs(song_addr->pbend_list, addr, channels);
  }
  handle = mus_current_handle++;

  /* Master track: drives timing, produces no sound, owns the master flag. */
  cp = mus_channels + __MusIntFindChannel(song_addr, -1);
  __MusIntInitialiseChannel(cp);
  cp->velocity_on = 1;
  cp->channel_flag |= CHFLAG_PAUSE | CHFLAG_MASTERTRACK;
  cp->song_addr = song_addr;
  cp->pdata = cp->pbase = song_addr->master_track;
  cp->handle = handle;

  /* One channel per populated data track, with its volume/bend streams. */
  for (i = 0; i < channels; i++) {
    if (song_addr->data_list[i]) {
      cp = mus_channels + __MusIntFindChannel(song_addr, i);
      __MusIntInitialiseChannel(cp);
      cp->velocity_on = 1;
      cp->channel_flag |= CHFLAG_PAUSE;
      cp->song_addr = song_addr;
      cp->pvolume = cp->pvolumebase = song_addr->volume_list[i];
      cp->ppitchbend = cp->ppitchbendbase = song_addr->pbend_list[i];
      cp->pdata = cp->pbase = song_addr->data_list[i];
      cp->handle = handle;
    }
  }
  mus_init_bank = NULL;
  return (handle);
}

/*
 * Clear then set channel-flag bits on every channel under `handle`. This is how
 * the audio thread applies the pause/unpause requests delivered over the FIFO.
 */
void __MusIntHandleSetFlag(unsigned long handle, unsigned long clear,
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
