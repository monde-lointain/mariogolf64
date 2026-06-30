/*
 * libmus_data.h
 *
 * On-stream and in-ROM data formats for the libmus sequenced-music player: the
 * pointer-bank (instrument index) layout, plus the symbolic constants a song
 * author uses to hand-build a sequence -- note pitches, note durations, and the
 * player command opcodes. The player consumes these formats internally; this
 * header lets a caller author and read them without depending on the
 * implementation.
 */
#ifndef _LIBMUS_DATA_H_
#define _LIBMUS_DATA_H_

/* ALWaveTable, referenced by ptr_bank_t::wave_list below. */
#include <libaudio.h>

/*
 * Pointer bank: the index mapping a song's wave (instrument) numbers to the
 * sample data held in a wave bank (.wbk). Loaded from a ".ptr" file; its
 * internal pointers begin as file-relative offsets and are relocated to
 * absolute addresses on first use (flagged by PTRFLAG_REMAPPED).
 */
typedef struct {
  unsigned char header_name[16]; /* file tag/identifier, 16 ASCII bytes */
  unsigned long flags; /* bit 31 (PTRFLAG_REMAPPED) = pointers relocated */
  unsigned long wbk_name[3]; /* name of the paired wave bank (.wbk), 3 words */
  int count;                 /* number of waves (instruments) in this bank */
  unsigned char*
      basenote;  /* per-wave root pitch (semitone, c4 = 48); count entries */
  float* detune; /* per-wave fine tune in semitones, added to the note */
  ALWaveTable**
      wave_list; /* per-wave sample (wavetable) pointers; count entries */
} ptr_bank_t;

/*
 * Note-duration constants, in player ticks (crotchet cr = 48 = one beat is the
 * base resolution). A note in a sequence stream is followed by a length byte;
 * these are the convenient values an author writes for it. Names follow British
 * note durations: a "d" prefix is dotted (* 3/2), "dd" double-dotted (* 7/4),
 * and a "tr" suffix is a triplet subdivision (three fill the parent value).
 */
#define qv 24    /* quaver (1/8 note) */
#define sq 12    /* semiquaver (1/16 note) */
#define dcr 72   /* dotted crotchet (cr * 3/2) */
#define ddcr 84  /* double-dotted crotchet (cr * 7/4) */
#define dsq 6    /* demisemiquaver (1/32 note) */
#define dqv 36   /* dotted quaver (qv * 3/2) */
#define cr 48    /* crotchet (1/4 note); the base resolution, one beat */
#define mn 96    /* minim (1/2 note) */
#define crtr 16  /* crotchet triplet (3 * crtr == cr) */
#define ddqv 42  /* double-dotted quaver (qv * 7/4) */
#define hdsqtr 1 /* hemidemisemiquaver triplet (3 * hdsqtr == hdsq) */
#define sqtr 4   /* semiquaver triplet (3 * sqtr == sq) */
#define qvtr 8   /* quaver triplet (3 * qvtr == qv) */
#define dtsq 18  /* dotted semiquaver (sq * 3/2) */
#define hdsq 3   /* hemidemisemiquaver (1/64 note) */
#define sb 192   /* semibreve (whole note) */
#define dmn 144  /* dotted minim (mn * 3/2) */

/*
 * Note-pitch constants: a note byte in a sequence stream is a semitone index,
 * 12 per octave, kept below 0x80 so it never collides with a command opcode.
 * c4 = 48 is the player's base reference pitch (BASEOFFSET). A sharp is named
 * "<note>s<octave>" (e.g. cs4); naturals omit the 's'. The table starts at
 * cs0 = 1 (c0 = 0 is not defined here).
 */
#define cs0 1
#define d0 2
#define ds0 3
#define e0 4
#define f0 5
#define fs0 6
#define g0 7
#define gs0 8
#define a0 9
#define as0 10
#define b0 11

#define c1 12
#define cs1 13
#define d1 14
#define ds1 15
#define e1 16
#define f1 17
#define fs1 18
#define g1 19
#define gs1 20
#define a1 21
#define as1 22
#define b1 23

#define c2 24
#define cs2 25
#define d2 26
#define ds2 27
#define e2 28
#define f2 29
#define fs2 30
#define g2 31
#define gs2 32
#define a2 33
#define as2 34
#define b2 35

#define c3 36
#define cs3 37
#define d3 38
#define ds3 39
#define e3 40
#define f3 41
#define fs3 42
#define g3 43
#define gs3 44
#define a3 45
#define as3 46
#define b3 47

#define c4 48 /* base reference pitch (BASEOFFSET) */
#define cs4 49
#define d4 50
#define ds4 51
#define e4 52
#define f4 53
#define fs4 54
#define g4 55
#define gs4 56
#define a4 57
#define as4 58
#define b4 59

#define c5 60
#define cs5 61
#define d5 62
#define ds5 63
#define e5 64
#define f5 65
#define fs5 66
#define g5 67
#define gs5 68
#define a5 69
#define as5 70
#define b5 71

#define c6 72
#define cs6 73
#define d6 74
#define ds6 75
#define e6 76
#define f6 77
#define fs6 78
#define g6 79
#define gs6 80
#define a6 81
#define as6 82
#define b6 83

#define c7 84
#define cs7 85
#define d7 86
#define ds7 87
#define e7 88
#define f7 89
#define fs7 90
#define g7 91
#define gs7 92
#define a7 93
#define as7 94
#define b7 95

/*
 * Song/effect command opcodes. In a sequence stream a byte with its high bit
 * set (>= 0x80) is a command; (opcode & 0x7f) indexes the player's handler jump
 * table, and the handler consumes any operand bytes that follow. Bytes below
 * 0x80 are note pitches (above). Clast bounds the table: an opcode >= Clast is
 * invalid.
 */
#define Cstop 0x80    /* end channel: clear all pointers, retire it */
#define Cwave 0x81    /* select wave/instrument for following notes */
#define Cport 0x82    /* portamento on: glide pitch over N steps */
#define Cportoff 0x83 /* portamento off */
#define Cdefa 0x84    /* define an ADSR envelope inline from the stream */
#define Ctempo 0x85   /* set tempo (BPM-style), propagated across the song */
#define Ccutoff 0x86  /* gate note to a fixed span measured from its start */
#define Cendit 0x87   /* gate note: release N ticks before its natural end */
#define Cvibup 0x88   /* vibrato sweeping pitch up: delay, speed, depth */
#define Cvibdown 0x89 /* vibrato sweeping pitch down */
#define Cviboff 0x8a  /* vibrato off */
#define Clength 0x8b  /* set a fixed note length for following notes */
#define Cignore 0x8c  /* skip the next note event */
#define Ctrans 0x8d   /* set the transpose offset (semitones) */
#define Cignoretrans 0x8e /* suppress transposition on this channel */
#define Cdistort 0x8f     /* persistent detune, signed, in 1/100 semitone */
#define Cadsr 0x90        /* select an ADSR envelope from the song's table */
#define Cenvoff 0x91      /* stop the envelope re-triggering on each note */
#define Cenvon 0x92       /* allow the envelope to re-trigger on each note */
#define Ctroff 0x93       /* suppress note re-trigger (hold the voice) */
#define Ctron 0x94        /* allow notes to re-trigger */
#define Cfor 0x95         /* open a FOR repeat block (push count + state) */
#define Cnext 0x96        /* close the block (loop back or fall through) */
#define Cwobble 0x97      /* wobble modulation: depth, ramp-on, ramp-off */
#define Cwobbleoff 0x98   /* wobble off */
#define Cvelon 0x99    /* per-note velocity on (notes carry a velocity byte) */
#define Cveloff 0x9a   /* per-note velocity off */
#define Cvelocity 0x9b /* set a fixed default velocity */
#define Cpan 0x9c      /* set the stereo pan */
#define Cstereo 0x9d   /* reserved stereo opcode (skips 2 operand bytes) */
#define Cdrums 0x9e    /* select a drum-kit entry; play percussion voices */
#define Cdrumsoff 0x9f /* leave drum mode */
#define Cprint 0xa0    /* debug trace opcode (prints under _AUDIODEBUG) */
#define Cgoto 0xa1   /* branch within the sequence (code + envelope offsets) */
#define Creverb 0xa2 /* set the reverb send level (0 = dry) */
#define Crandnote 0xa3   /* randomize transpose: random spread + base */
#define Crandvolume 0xa4 /* randomize volume: random spread + base */
#define Crandpan 0xa5    /* randomize pan: random spread + base */
#define Cvolume 0xa6     /* set the channel volume */
#define Cstartfx 0xa7    /* launch a sound effect from inside a song */
#define Cbendrange 0xa8  /* set the pitch-bend range in semitones (arg/64) */
#define Csweep 0xa9      /* set the pitch-sweep rate */
#define Cchangefx 0xaa   /* switch the active custom effect type */
#define Cmarker 0xab     /* fire the marker callback, then rest */
#define Clength0 0xac /* clear fixed length; notes carry their own duration */
#define Clast 0xad    /* end-of-table sentinel: first invalid opcode */
#endif
