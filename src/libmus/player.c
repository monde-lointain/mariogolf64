#include "player_priv.h"

// Default-ADSR setup, extracted as a static helper so GCC -O3 auto-inlines it
// into Fenvelope + __MusIntGetNewNote exactly as the upstream inlines Fdefa
// (defining it before its callers is what lets the inline reproduce; the
// standalone Fdefa lives in Fdefa for the command jumptable).
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

unsigned char* Fenvelope(channel_t* cp, unsigned char* ptr) {
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

// MusInitialize deps (callees defined later; globals).
extern OSPiHandle* diskrom_handle;       // @0x800FEF78
extern unsigned long __muscontrol_flag;  // __muscontrol_flag @0x80132368
extern ALPlayer plr_player;              // @0x800E7030
extern void __MusIntFifoOpen(int);       // __MusIntFifoOpen
extern void MusPtrBankInitialize(void*, void*);      // MusPtrBankInitialize
extern void MusFxBankInitialize(void*);              // MusFxBankInitialize
extern void MusSetMasterVolume(unsigned long, int);  // MusSetMasterVolume
extern ALMicroTime __MusIntMain(void*);              // __MusIntMain

int MusInitialize(musConfig* config) {
  ALVoiceConfig vc;
  int i;

  diskrom_handle = config->diskrom_handle;
  __muscontrol_flag = config->control_flag;
  max_channels = config->channels + MAX_SONGS;

  if (osTvType == 0)
    mus_vsyncs_per_second = 50;
  else
    mus_vsyncs_per_second = 60;
  mus_next_frame_time = 1000000 / mus_vsyncs_per_second;

  __MusIntMemInit(config->heap, config->heap_length);
  __MusIntSchedInit(config->sched);

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

  plr_player.next = NULL;
  plr_player.handler = __MusIntMain;
  plr_player.clientData = &plr_player;
  alSynAddPlayer(&__libmus_alglobals.drvr, &plr_player);

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

// MusSetMasterVolume
void MusSetMasterVolume(unsigned long flags, int volume) {
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
musHandle MusStartSongFromMarker(void* addr, int marker) {
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
           __MusIntGetNewNote: velocity assigned via temp, masked into velocity
           not vel, default reloaded) */
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

// MusStartEffect
musHandle try_spawn_global_object(int number) {
  musHandle handle;
  fx_header_t* header;

  if (libmus_fxheader_single) {
    header = libmus_fxheader_single;
    libmus_fxheader_single = NULL;
  } else {
    header = libmus_fxheader_current;
    /* are effects present? */
    if (!header) {
      mus_init_bank = NULL;
      return (0);
    }
  }

  /* set FX default sample bank */
  if (!mus_init_bank) mus_init_bank = header->ptr_addr;
  /* start effect default priority */
  handle = __MusIntFindChannelAndStart(header, number, 0x80, 0x80, -1);
  /* reset any single sample bank override */
  mus_init_bank = NULL;
  return (handle);
}

// MusStartEffect2
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
    /* are effects present? */
    if (!header) {
      mus_init_bank = NULL;
      return (0);
    }
  }

  /* set FX default sample bank */
  if (!mus_init_bank) mus_init_bank = header->ptr_addr;
  /* overwrite same effect if it exists */
  if (restartflag) {
    for (i = MAX_SONGS, cp = mus_channels2; i < max_channels; i++, cp++) {
      if (cp->fx_number == number && cp->fx_addr == header) {
        /* get default priority if required */
        if (priority == -1) priority = header->effects[number].priority;
        handle = __MusIntStartEffect(cp, header, number, volume, pan, priority);
        /* reset any single sample bank override */
        mus_init_bank = NULL;
        return (handle);
      }
    }
  }
  /* start effect using parameters */
  handle = __MusIntFindChannelAndStart(header, number, volume, pan, priority);
  /* reset any single sample bank override */
  mus_init_bank = NULL;
  return (handle);
}

// MusStop
void MusStop(unsigned long flags, int speed) {
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

// MusHandleStop
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

// MusHandleAsk
int MusHandleAsk(musHandle handle) {
  channel_t* cp;
  int i, count;

  if (!handle) return (0);
  for (i = 0, cp = mus_channels, count = 0; i < max_channels; i++, cp++)
    if (cp->handle == handle) count++;
  return (count);
}

// MusHandleSetVolume
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

// MusHandleSetPan
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

// MusHandleSetFreqOffset
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

// MusHandleSetTempo
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

// MusHandleSetReverb
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

// MusPtrBankInitialize
void MusPtrBankInitialize(void* pbank, void* wbank) {
  __MusIntRemapPtrBank(pbank, wbank);
  if (!mus_default_bank) mus_default_bank = pbank;
}

// MusPtrBankSetSingle
void* MusPtrBankSetSingle(void* ipbank) {
  ptr_bank_t* pptr;
  if (ipbank) {
    pptr = (ptr_bank_t*)ipbank;
    if (pptr->flags & PTRFLAG_REMAPPED) mus_init_bank = pptr;
  }
  return (mus_init_bank);
}

// MusPtrBankSetCurrent
void MusPtrBankSetCurrent(void* ipbank) {
  ptr_bank_t* pptr;
  if (ipbank) {
    pptr = (ptr_bank_t*)ipbank;
    if (pptr->flags & PTRFLAG_REMAPPED) mus_default_bank = pptr;
  }
}

// MusPtrBankGetCurrent
void* MusPtrBankGetCurrent(void) { return (mus_default_bank); }

// MusHandleGetPtrBank
void* MusHandleGetPtrBank(musHandle handle) {
  channel_t* cp;
  int i, count;

  if (!handle) return (NULL);
  for (i = 0, cp = mus_channels, count = 0; i < max_channels; i++, cp++)
    if (cp->handle == handle) return (cp->sample_bank);
  return (NULL);
}

// MusHandlePause
int MusHandlePause(musHandle handle) {
  fifo_t fifo_command;

  fifo_command.command = FIFOCMD_PAUSE;
  fifo_command.data = handle;
  return (__MusIntFifoAddCommand(&fifo_command));
}

// MusHandleUnPause
int MusHandleUnPause(musHandle handle) {
  fifo_t fifo_command;

  fifo_command.command = FIFOCMD_UNPAUSE;
  fifo_command.data = handle;
  return (__MusIntFifoAddCommand(&fifo_command));
}

// MusSetFxType
int MusSetFxType(int fxtype) {
  fifo_t fifo_command;

  fifo_command.command = FIFOCMD_CHANGEFX;
  fifo_command.data = fxtype;
  mus_last_fxtype = fxtype;
  return (__MusIntFifoAddCommand(&fifo_command));
}

// MusSetSongFxChange
int MusSetSongFxChange(musBool onoff) {
  int changed;

  changed = 1;
  if (onoff == MUSBOOL_OFF) changed = MusSetFxType(mus_last_fxtype);
  if (changed) mus_songfxchange_flag = onoff;
  return (changed);
}

// MusFxBankInitialize
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

// MusFxBankNumberOfEffects
int MusFxBankNumberOfEffects(void* ifxbank) {
  return (((fx_header_t*)ifxbank)->number_of_effects);
}

// MusFxBankSetCurrent
void MusFxBankSetCurrent(void* ifxbank) {
  libmus_fxheader_current = (fx_header_t*)ifxbank;
}

// MusFxBankSetSingle
void MusFxBankSetSingle(void* ifxbank) {
  libmus_fxheader_single = (fx_header_t*)ifxbank;
}

// MusFxBankGetCurrent
void* MusFxBankGetCurrent(void) { return (libmus_fxheader_current); }

// MusFxBankSetPtrBank
void MusFxBankSetPtrBank(void* ifxbank, void* ipbank) {
  ((fx_header_t*)ifxbank)->ptr_addr = (ptr_bank_t*)ipbank;
}

// MusFxBankGetPtrBank
void* MusFxBankGetPtrBank(void* ifxbank) {
  return (((fx_header_t*)ifxbank)->ptr_addr);
}

// MusSetScheduler
void MusSetScheduler(musSched* sched_list) {
  __libmus_current_sched = sched_list;
}

// MusHandleWaveCount
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

// MusHandleWaveAddress
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

// MusSetMarkerCallback
void MusSetMarkerCallback(void* callback) {
  marker_callback = (LIBMUScb_marker)callback;
}

void func_8009A630(void) { func_8009DBA0(); }

// __MusIntFifoOpen
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

// __MusIntFifoProcess: drain the command fifo. Defined BEFORE
// __MusIntFifoProcessCommand so the dispatch is only forward-declared here ->
// GCC cannot inline it -> it stays a jal. __MusIntFifoProcess is static +
// called once (by __MusIntMain), so GCC -O3 inlines this drain loop INTO
// __MusIntMain while keeping
// __MusIntFifoProcessCommand out-of-line. That definition-order trick (NOT a
// hand-inlined loop) is what reproduces the ROM -- confirmed byte-exact in PPL
// (-O3) and drmario64 (-O2). The canonical libmus has the same order
// (player_fifo.inc.c: drain @73, dispatch @101).
void __MusIntFifoProcessCommand(
    fifo_t* command);  // forward decl (defined just below)
static void __MusIntFifoProcess(void) {
  if (fifo_start == fifo_current) return;
  while (fifo_start != fifo_current) {
    __MusIntFifoProcessCommand(&fifo_addr[fifo_start]);
    fifo_start++;
    if (fifo_start == fifo_limit) fifo_start = 0;
  }
}

// __MusIntFifoProcessCommand
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

// __MusIntFifoAddCommand
int __MusIntFifoAddCommand(fifo_t* command) {
  int index;

  index = (fifo_current + 1) % fifo_limit;
  if (index == fifo_start) {
    return (0);
  }
  __MusIntMemMove(&fifo_addr[fifo_current], command, sizeof(fifo_t));
  fifo_current = index;
  return (1);
}

int func_8009A7C8(void) { return g_mus_frame_counter; }

// __MusIntMain callees defined later in this TU (forward-declared so they emit
// as jal -- defined-after-caller means not visible for inlining at the call
// site).
extern void __MusIntGetNewNote(channel_t*, int);       // __MusIntGetNewNote
extern void __MusIntSetPitch(channel_t*, int, float);  // __MusIntSetPitch
extern void __MusIntProcessEnvelope(channel_t*);  // __MusIntProcessEnvelope
extern void __MusIntProcessSweep(channel_t*);     // __MusIntProcessSweep
extern float __MusIntProcessWobble(channel_t*);   // __MusIntProcessWobble
extern float __MusIntProcessVibrato(channel_t*);  // __MusIntProcessVibrato
extern void __MusIntSetVolumeAndPan(channel_t*,
                                    int);  // __MusIntSetVolumeAndPan
extern u32 _mus_cpu_last;  // _mus_cpu_last  (SUPPORT_PROFILER timing)
extern u32 _mus_cpu_worst;
// __MusIntMain: per-frame sequence-player handler. SUPPORT_PROFILER is on (the
// osGetCount bracket + _mus_cpu_last/worst); g_mus_frame_counter++ and the
// inline channel-stop (Fstop is cross-TU in player_commands.c, so it cannot be
// GCC-inlined) are MG64 customizations.
ALMicroTime __MusIntMain(void* node) {
  int x;
  channel_t* cp;
  u32 start = osGetCount();

  g_mus_frame_counter++;
  __MusIntFifoProcess();

  for (x = -MAX_SONGS, cp = mus_channels; x < max_channels - MAX_SONGS;
       x++, cp++) {
    if (cp->pdata == NULL || (cp->channel_flag & CHFLAG_PAUSE)) continue;
    if (cp->pending) __MusIntFlushPending(cp, x);
    cp->channel_frame += cp->channel_tempo;
    if (cp->length != 0x7fff) {
      while ((s32)(cp->note_end_frame - cp->channel_frame) < 0 &&
             cp->pdata != NULL)
        __MusIntGetNewNote(cp, x);
      if (!cp->pdata) continue;
    }
    if (cp->pvolume && (s32)(cp->volume_frame - cp->channel_frame) < 0)
      __MusIntProcessContinuousVolume(cp);
    if (cp->ppitchbend && (s32)(cp->pitchbend_frame - cp->channel_frame) < 0)
      __MusIntProcessContinuousPitchBend(cp);
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
    cp->count = (cp->channel_frame - cp->note_start_frame) >> 8;
  }

  _mus_cpu_last = osGetCount() - start;
  if (_mus_cpu_last > _mus_cpu_worst) _mus_cpu_worst = _mus_cpu_last;
  return (mus_next_frame_time);
}

// __MusIntGetNewNote: advance past commands (jumptable dispatch), then fetch
// the next note + length + velocity, set up the wave/drum sample, envelope and
// reverb.
void __MusIntGetNewNote(channel_t* cp, int x) {
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
void __MusIntFlushPending(channel_t* cp, int x) {
  if (cp->playing) alSynStopVoice(&__libmus_alglobals.drvr, mus_voices + x);
  cp->playing = 1;
  /* start sample */
  alSynStartVoice(&__libmus_alglobals.drvr, mus_voices + x, cp->pending);
  cp->pending = NULL;
}

// MG64-added: enable/disable stereo pan processing.
void func_8009B0DC(int mode) { g_mus_pan_enabled = (mode == 1); }

// __MusIntSetVolumeAndPan (MG64 pan-enable mod: pan defaults to centre 0x40,
// the real pan is computed only when g_mus_pan_enabled is set)
void __MusIntSetVolumeAndPan(channel_t* cp, int x) {
  u32 volume;

  /* process volume */
  volume = ((u32)(cp->volume) * (u32)(cp->env_current) * (u32)(cp->velocity) *
            (u32)(cp->volscale)) >>
           13;
  if (volume > 32767) volume = 32767;

  if (!cp->fx_addr)
    volume *= mus_master_volume_songs;
  else
    volume *= mus_master_volume_effects;
  volume >>= 15;
  if (cp->stopping != -1) volume = (volume * cp->stopping) / cp->stopping_speed;

  if (volume != cp->old_volume) {
    cp->old_volume = volume;
    alSynSetVol(&__libmus_alglobals.drvr, mus_voices + x, volume,
                mus_next_frame_time);
  }

  /* process pan */
  volume = 0x40;
  if (g_mus_pan_enabled) volume = ((cp->pan * cp->panscale) >> 7) & 0x7f;
  if (volume != cp->old_pan) {
    cp->old_pan = volume;
    alSynSetPan(&__libmus_alglobals.drvr, mus_voices + x, volume);
  }
}

// __MusIntSetPitch: compute the channel frequency (portamento + pitchbend +
// 2^(semitones/12)) and push it to the synth voice.
void __MusIntSetPitch(channel_t* cp, int x, float offset) {
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
void __MusIntInitEnvelope(channel_t* cp) {
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

// __MusIntProcessEnvelope (MG64 restores the upstream-removed 99.01.29
// env_count guard; uses an early-return rather than wrapping the switch in an
// if-block)
void __MusIntProcessEnvelope(channel_t* cp) {
  int env_phase_count;

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
    case 1:
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
    case 2:
      /* MG64 subtracts env_attack_speed BEFORE the multiply (upstream does it
       * after) */
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
    case 3:
      return;
    case 4:
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

// __MusIntInitSweep
void __MusIntInitSweep(channel_t* cp) {
  cp->sweep_frame = cp->note_start_frame;
  cp->sweep_timer = 0;
  cp->sweep_dir = cp->pan & 0x40;
}

// __MusIntProcessSweep
void __MusIntProcessSweep(channel_t* cp) {
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

// __MusIntProcessVibrato
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

// __MusIntProcessContinuousVolume
void __MusIntProcessContinuousVolume(channel_t* cp) {
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
void __MusIntProcessContinuousPitchBend(channel_t* cp) {
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

// __MusIntRemapPtrBank: convert pointer-bank file offsets to RAM pointers.
void __MusIntRemapPtrBank(char* pptr, char* wptr) {
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

// __MusIntRandom. Banked now that player_commands.c is a separate TU: the rand
// command handlers call it cross-TU (jal), so GCC -O3 keeps it out-of-line.
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

// __MusIntInitialiseChannel
void __MusIntInitialiseChannel(channel_t* cp) {
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
int __MusIntFindChannel(song_t* addr, int song_chan) {
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
void __MusIntRemapPtrs(void* addr, void* offset, int count) {
  unsigned long *dest, add;
  int i;

  dest = (unsigned long*)addr;
  add = (unsigned long)offset;
  for (i = 0; i < count; i++)
    if (dest[i]) dest[i] += add;
}

// __MusIntStartEffect
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

  /* set sample bank pointer */
  if (header->ptr_addr) cp->sample_bank = header->ptr_addr;

  /* pdata must be set last to avoid processing clash */
  cp->pdata = cp->pbase = header->effects[number].fxdata;

  return (cp->handle);
}

// __MusIntFindChannelAndStart. Banked: its caller Fstartfx is now in
// the separate player_commands.c TU (cross-TU jal), so GCC -O3 keeps this
// out-of-line. The ROM inlines __MusIntStartEffect at both sites (manual inline
// reproduced).
musHandle __MusIntFindChannelAndStart(fx_header_t* header, int number,
                                      int volume, int pan, int priority) {
  int i, current_priority;
  channel_t *cp, *current_cp;

  if (priority == -1) priority = header->effects[number].priority;
  current_priority = priority + 1;

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

// __MusIntStartSong
musHandle __MusIntStartSong(void* addr) {
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
