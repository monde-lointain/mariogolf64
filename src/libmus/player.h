/*
 * player.h
 *
 * Internal data structures and command-handler prototypes for the libmus
 * sequence player (player.c and player_commands.c, shared through
 * player_priv.h). It defines the per-channel playback state (channel_t), the
 * on-cartridge song and effect-bank headers (song_t, fx_header_t, with their
 * drum_t and fx_t sub-records), the channel/pointer/song status flags, and the
 * sequence-command jump-table entry type (command_func_t).
 */
#ifndef _PLAYER_H_
#define _PLAYER_H_
#include "libmus.h"
#include "libmus_data.h"

/* Maximum nesting of for/next loops on a single channel. */
#define FORNEXT_DEPTH 4

/* Channels reserved at the front of the pool for song master tracks; also the
 * offset to the effect-channel region (mus_channels2 = mus_channels + this). */
#define MAX_SONGS 4

/*
 * One drum-kit entry. A percussion note indexes song_t.drum_table to pick the
 * wave, envelope, pan and pitch the note should actually sound with.
 */
typedef struct {
  unsigned short wave; /* wave-table index for this drum sound */
  unsigned short adsr; /* envelope index (x7) into song_t.env_table */
  unsigned char pan;   /* pan 0..255 (halved to 0..127 when applied) */
  unsigned char pitch; /* note pitch to sound */
} drum_t;

/*
 * On-cartridge song header. Its pointer members are stored as file-relative
 * offsets and relocated to absolute addresses on first play (__MusIntStartSong
 * relocates SONGHDR_COUNT consecutive pointers starting at SONGHDR_ADR()).
 */
typedef struct {
  unsigned long version; /* song-format version */
  long num_channels;     /* number of channel (track) streams */
  long num_waves;        /* number of distinct waves referenced */
/* The block of consecutive pointer fields below (data_list..master_track) is
 * relocated as a unit: SONGHDR_COUNT of them, starting at SONGHDR_ADR(). */
#define SONGHDR_COUNT 7
#define SONGHDR_ADR(x) (&(x)->data_list)
  unsigned char** data_list;   /* per-channel note/command streams */
  unsigned char** volume_list; /* per-channel continuous-volume streams */
  unsigned char** pbend_list;  /* per-channel continuous-pitchbend streams */
  unsigned char* env_table;    /* ADSR parameter table, 7 bytes per entry */
  drum_t* drum_table;          /* drum-kit map */
  unsigned short* wave_table;  /* wave index -> bank wave; 0xffff = silent */
  unsigned char* master_track; /* master/conductor track stream */
  unsigned long flags;         /* SONGFLAG_* status bits */
  unsigned long reserved1;
  unsigned long reserved2;
  unsigned long reserved3;
} song_t;

/* One effect entry within an effect bank. */
typedef struct {
  unsigned char*
      fxdata;   /* effect command stream (file offset until relocated) */
  int priority; /* default priority when this effect is started */
} fx_t;

/*
 * Effect-bank header. effects[] is a variable-length array (declared [1]); its
 * true length is number_of_components.
 */
typedef struct {
  int number_of_components; /* effect entries present (each fxdata relocated) */
  int number_of_effects;    /* effects exposed to the app
                               (MusFxBankNumberOfEffects) */
  int num_waves;            /* number of distinct waves referenced */
  unsigned long flags;      /* FXFLAG_* status bits */
  ptr_bank_t* ptr_addr;     /* associated sample/pointer bank */
  unsigned short*
      wave_table;  /* wave index -> bank wave (relocated from offset) */
  fx_t effects[1]; /* per-effect entries, [number_of_components] */
} fx_header_t;

/* ptr_bank_t.flags: bank pointers have already been relocated to absolute. */
#define PTRFLAG_REMAPPED (1 << 31)

/* channel_t.channel_flag bits. */
#define CHFLAG_PAUSE (1 << 0)       /* channel is paused */
#define CHFLAG_MASTERTRACK (1 << 1) /* channel is a song's master track */

/* fx_header_t.flags / song_t.flags: header already initialised in place. */
#define FXFLAG_INITIALISED (1 << 0)
#define SONGFLAG_INITIALISED (1 << 0)

/*
 * Per-channel player state: one voice's worth of running sequence-playback
 * state. The channel pool holds max_channels of these; pdata == NULL marks a
 * free/inactive channel. All "*_frame" values are points on a 1/256-tick
 * fixed-point sequence clock (note lengths enter the clock as length * 256).
 */
typedef struct {
  /* Sequence clock and command-stream cursor. */
  unsigned long channel_flag; /* CHFLAG_* bits */
  unsigned char* pdata;       /* cursor in note/command stream; NULL=inactive */
  ALWaveTable* pending;       /* wave queued to start on the voice next flush */
  unsigned long channel_frame;   /* playback clock (1/256-tick units) */
  int stopping;                  /* fade-out countdown; -1 = not stopping */
  unsigned long volume_frame;    /* clock of next continuous-volume step */
  unsigned long pitchbend_frame; /* clock of next continuous-pitchbend step */
  int stopping_speed;            /* fade divisor while stopping */

  /* Pitch working values and continuous-controller cursors. */
  float vib_amount;        /* vibrato depth */
  float pitchbend_precalc; /* pitchbend * bendrange (semitone offset) */
  float old_frequency;     /* last pitch sent (skip redundant updates) */
  float base_note;         /* current note pitch (semitone scale) */
  float freqoffset; /* fixed frequency offset added to pitch (incl distort) */
  unsigned char* ppitchbend; /* cursor in continuous-pitchbend stream */
  unsigned char* pvolume;    /* cursor in continuous-volume stream */

  /* Note timing, owning handle, portamento and release schedule. */
  unsigned long note_end_frame;   /* clock at which the current note ends */
  unsigned long note_start_frame; /* clock at which the current note began */
  unsigned long handle;           /* owning musHandle; 0 = free */
  int priority;                   /* effect priority */
  float last_note;                /* glide-start pitch (portamento) */
  float port_base;                /* current portamento pitch */
  unsigned long release_frame;    /* clock at which envelope release begins */

  /* Precomputed envelope rates and pitch ranges. */
  float env_attack_calc;  /* attack level increment per step */
  float env_decay_calc;   /* decay level increment per step */
  float env_release_calc; /* release level factor per step */
  int env_speed_calc;     /* 1024 / env_speed envelope timing scale */
  float vibrato;          /* current vibrato output */
  float bendrange;        /* pitchbend range, semitones per unit */
  float pitchbend;        /* current pitchbend (centered at 64) */

  /* Owning song/effect bank and the stream bases used for goto offsets. */
  song_t* song_addr;             /* owning song, NULL for effects */
  fx_header_t* fx_addr;          /* owning effect bank, NULL for songs */
  ptr_bank_t* sample_bank;       /* wave/sample bank for this channel */
  unsigned char* pbase;          /* base of note stream (goto target base) */
  drum_t* pdrums;                /* active drum map, or NULL */
  unsigned char* ppitchbendbase; /* base of pitchbend stream (goto base) */
  unsigned char* pvolumebase;    /* base of volume stream (goto base) */

  /* Distort/sweep timing, tempo, note length, scaling, and counters. */
  float distort; /* frequency distort offset (folded into freqoffset) */
  unsigned long sweep_frame; /* clock of next pan-sweep step */
  short temscale;            /* tempo scale, /128 fixed point (128 = 1.0) */
  unsigned short length;     /* current note length (ticks); 0x7fff = sustain */
  unsigned short channel_tempo; /* clock advance per audio frame (scaled) */
  short volscale;               /* volume scale, /128 */
  unsigned short old_volume;    /* last volume sent (cache) */
  unsigned short cont_vol_repeat_count; /* ticks until next volume byte */
  unsigned short cont_pb_repeat_count;  /* ticks until next pitchbend byte */
  unsigned short fx_number;          /* effect index playing on this channel */
  unsigned short channel_tempo_save; /* unscaled base tempo */
  unsigned short count;              /* ticks elapsed in the current note */
  unsigned short fixed_length;       /* forced note length (0 = use stream) */
  unsigned short wave;               /* current wave index into wave_table */
  short panscale;                    /* pan scale, /128 */
  unsigned short cutoff; /* release offset measured from note start (ticks) */
  unsigned short
      endit; /* release lead-in measured back from note end (ticks) */

  /* Per-note byte parameters. */
  unsigned char vib_delay;        /* ticks before vibrato starts */
  unsigned char ignore;           /* skip the next fixed-length override */
  unsigned char port;             /* portamento time (ticks); 0 = off */
  unsigned char transpose;        /* transpose, signed semitones */
  unsigned char ignore_transpose; /* suppress transpose for the next note */
  unsigned char velocity;         /* current note velocity */
  unsigned char volume;           /* channel volume 0..127 */
  unsigned char pan;              /* pan 0..127 (64 = center) */
  unsigned char old_pan;          /* last pan sent; 0xff = force update */

  /* ADSR envelope (8-bit working state). */
  unsigned char env_speed;       /* envelope step period (frames per step) */
  unsigned char env_init_vol;    /* attack-start level */
  unsigned char env_max_vol;     /* attack-peak level */
  unsigned char env_sustain_vol; /* sustain level */
  unsigned char
      env_phase; /* 0=off 1=attack 2=decay 3=sustain 4=release 5=done */
  unsigned char env_current;       /* current envelope level */
  unsigned char env_count;         /* frames until next envelope step */
  unsigned char env_attack_speed;  /* attack duration */
  unsigned char env_decay_speed;   /* decay duration */
  unsigned char env_release_speed; /* release duration */

  /* Voice/reverb state, wobble (tremolo-style toggle) and misc toggles. */
  unsigned char playing;           /* voice currently allocated/sounding */
  unsigned char reverb;            /* per-note reverb amount */
  unsigned char reverb_base;       /* base reverb (MusHandleSetReverb) */
  unsigned char old_reverb;        /* last reverb sent; 0xff = force update */
  unsigned char release_start_vol; /* envelope level when release began */
  unsigned char wobble_on_speed;   /* frames at full wobble; 0 = wobble off */
  unsigned char wobble_off_speed;  /* frames at zero wobble */
  unsigned char wobble_count;      /* frames until wobble toggles */
  signed char wobble_current;      /* current wobble value */
  unsigned char velocity_on;      /* read per-note velocity bytes from stream */
  unsigned char default_velocity; /* velocity used while velocity_on is off */
  unsigned char sweep_speed;      /* pan-sweep speed; 0 = off */
  unsigned char vib_speed;        /* vibrato period; 0 = off */
  unsigned char env_trigger_off;  /* do not retrigger envelope per note */
  unsigned char trigger_off;      /* do not retrigger the sample per note */
  signed char wobble_amount;      /* wobble depth */
  unsigned char sweep_timer;      /* pan-sweep accumulator */
  unsigned char sweep_dir;        /* pan-sweep direction: 0 = up, 1 = down */

  /* for/next loop stack (FORNEXT_DEPTH levels deep). Each level snapshots the
   * stream cursors, controller counters and volume/pitch state at the loop top
   * so Fnext can rewind to it. */
  unsigned char for_stack_count; /* current loop-stack depth */
  float vib_precalc;             /* vibrato angular step = 2pi/vib_speed */
  unsigned char* for_stack[FORNEXT_DEPTH];     /* saved note-stream cursor */
  unsigned char* for_stackvol[FORNEXT_DEPTH];  /* saved volume cursor */
  unsigned char* for_stackpb[FORNEXT_DEPTH];   /* saved pitchbend cursor */
  unsigned short for_vol_count[FORNEXT_DEPTH]; /* saved cont_vol_repeat_count */
  unsigned short for_pb_count[FORNEXT_DEPTH];  /* saved cont_pb_repeat_count */
  unsigned char
      for_count[FORNEXT_DEPTH]; /* iterations left (0xff = infinite) */
  unsigned char for_volume[FORNEXT_DEPTH]; /* saved volume */
  float for_pitchbend[FORNEXT_DEPTH];      /* saved pitchbend */
#ifndef SUPPORT_EFFECTS
  /* Keep the struct size stable when the special-effect fields are compiled
   * out (this build: only SUPPORT_FXCHANGE is defined). */
  unsigned char padding[4];
#else
  /* Extended special-effect state, present only when SUPPORT_EFFECTS is built.
   */
  unsigned long effect_type;      /* active special-effect selector */
  float specialvib_amount;        /* special-vibrato depth */
  unsigned char specialvib_speed; /* special-vibrato speed */
  unsigned char specialvib_delay; /* special-vibrato start delay */
  unsigned char last_volume;      /* previous volume, for effect ramps */
  unsigned char vol_effect[4];    /* per-step volume effect table */
  unsigned char pitch_effect[4];  /* per-step pitch effect table */
#endif
} channel_t;

/*
 * One sequence-command jump-table entry. Each handler consumes its operand
 * bytes from the channel's stream and returns the next read position.
 */
typedef struct {
  unsigned char* (*func)(channel_t* cp, unsigned char* ptr);
} command_func_t;
#endif
