#ifndef SOUND_H
#define SOUND_H

#include "common.h"

/* Sound effects play on channels, each holding one effect at a time */
#define SOUND_CHANNEL_COUNT 16

/*
 * The largest volume or pan scale: libmus keeps a sound's own value at 0x80,
 * and nearly all of it at this
 */
#define SOUND_FULL_SCALE 0x7F

/*
 * The sound effects that PlaySound and PlaySoundWithEffects play. Many play
 * in more than one place; each is named for one use, and the comments name
 * the others.
 */
enum Sound {
  SOUND_FLAG_SWISH = 0x1F,
  SOUND_TICK, /* A tally's tick and each click of the club slots' reels */
  SOUND_REEL_STOP,
  /* Also the club slots' turn and a sound of the scorecard */
  SOUND_TALLY_DONE,
  SOUND_START_LIGHT_RED,
  SOUND_START_LIGHT_GO,
  SOUND_WIND_FLY_GUY = 0x28,
  SOUND_GOOMBA = 0x2A, /* Also the Monty Mole's */
  SOUND_CONTACT_MARIO = 0x2E, /* Also Bowser's */
  SOUND_CONTACT_HEAVY_HITTER,
  SOUND_CONTACT_WARIO, /* Also played when the game halts on a fault */
  SOUND_SAVED = 0x3B, /* Also played by the save prompt */
  SOUND_SHRINKING_IMAGE = 0x43,
  SOUND_BANNER,
  SOUND_SLICE, /* Also played when the holes of a round are chosen */
  /* The scorecard and the save menu opening and closing */
  SOUND_PANEL_OPEN = 0x47,
  SOUND_PANEL_CLOSE,
  /*
   * Also played when a contest's winner appears, an erasure is declined and
   * a tournament is declined
   */
  SOUND_READY,
  /*
   * Also played when a contest's winner vanishes, the erase window opens and
   * a club tournament code is accepted
   */
  SOUND_GO,
  SOUND_CAPTION_MEET, /* Also played when the save data is erased */
  SOUND_MENU_START = 0x50,
  /* A ball being struck by a plain club or bouncing on hard ground */
  SOUND_CONTACT_PLAIN,
  SOUND_ENTER_TOURNAMENT = 0x54,
  SOUND_UNK90 = 0x5A,
  SOUND_LOOK_DOWN_RANGE, /* Also played when a scorecard row flips */
  SOUND_MENU_MOVE, /* Also the prompts' cursor and the scorecard's pages */
  SOUND_MENU_CONFIRM, /* The sound of confirming a choice */
  SOUND_MENU_CANCEL = 0x5F,
  SOUND_LOOK_AT_CUP,
  SOUND_LOOK_BACK, /* Also a sound of the scorecard */
  /* The swing meter's click for an impact on the mark */
  SOUND_IMPACT_ON_MARK,
  /*
   * A menu choice that cannot be made, or a cursor that cannot move that way.
   * Also a mishit impact and an incorrect club tournament code.
   */
  SOUND_MENU_BLOCKED,
  /*
   * Also the inset view opening, a wipe closing, the aim camera lowering and
   * the club slots' reels starting
   */
  SOUND_PROMPT_OPEN,
  /* Also the inset view closing, a wipe opening and the aim camera rising */
  SOUND_PROMPT_CLOSE,
  SOUND_THREE_STARS = 0x67, /* Also a sound of the scorecard */
  SOUND_UNK106 = 0x6A,
  SOUND_SKIN_COUNTED,
  SOUND_TEXT_TYPING,
  /* The swing meter's click for an impact off the mark */
  SOUND_IMPACT_OFF_MARK = 0x6E,
  SOUND_OPTION_TOGGLED,
  SOUND_LONG_SHOT,
  SOUND_LONG_SHOT_HEAVY,
  SOUND_CONTROLS_OPEN,
  /* Also played when the main menu shows the transmit option */
  SOUND_CONTROLS_CLOSE,
  SOUND_OUT_OF_BOUNDS = 0x77,
  SOUND_RING_SHOT_RETRY = 0x8E,
  SOUND_HOLE_INTRO,
  SOUND_UNK154 = 0x9A,
  SOUND_NEW_BEST_SCORE = 0x9C,
  /* The opening's ambient sounds */
  SOUND_AMBIENT_ALTERNATING_FIRST = 0x184,
  SOUND_AMBIENT_ALTERNATING_SECOND,
  SOUND_AMBIENT_REPEATING = 0x18B,
  SOUND_AMBIENT_RANDOM_FIRST = 0x1A3,
  SOUND_AMBIENT_RANDOM_SECOND = 0x1BA
};

/*
 * The songs that PlaySong plays. SONG_HOLE_* are a hole's result: Stroke play
 * picks one by the score against par, Get Character plays the one for how the
 * first player did, and match games and skins matches play SONG_HOLE_WON for
 * any hole that somebody won. SONG_ROUND_* play through a hole.
 */
enum Song {
  SONG_HOLE_OUT_EAGLE, /* Also for an albatross */
  SONG_HOLE_OUT_BIRDIE, /* Also for a hole-in-one */
  SONG_HOLE_OUT_PAR,
  SONG_RESULTS, /* The round results' song, outside tournaments */
  SONG_HOLE_OUT_BOGEY,
  SONG_HOLE_OUT_OVER_BOGEY,
  SONG_HOLE_WON,
  SONG_HOLE_LOST,
  SONG_HOLE_HALVED,
  SONG_MATCH_RESULT = 10,
  SONG_ROUND_TOURNAMENT,
  SONG_ROUND_FINAL = 20, /* A hole that can decide the round */
  SONG_TRANSMIT_MENU = 22,
  /* The controls screen's song; the main menu also plays it as it enters
   * MENU_SCREEN_UNK22 */
  SONG_CONTROLS,
  SONG_ROUND_UNK24,
  SONG_ROUND_MATCH,
  SONG_ROUND_UNK26,
  SONG_ROUND_COURSE,
  SONG_ROUND_RING_SHOT = 29,
  SONG_ROUND_UNK31 = 31,
  SONG_TOURNAMENT_RESULTS = 37,
  SONG_TITLE = 39, /* The title screen's song */
  SONG_ROUND_CLUB_SLOTS = 43,
  SONG_ROUND_MINI_GOLF = 45,
  SONG_ROUND_SKINS_MATCH,
  SONG_STAFF_ROLL = 49,
  SONG_CLOSING
};

/* PlaySong's flags */
/* Passed by some callers, but PlaySong never tests it */
#define SONG_FLAG_UNREAD 0x01
/* Stops the song instead when SAVE_OPTION_MUSIC_OFF is set */
#define SONG_FLAG_UNK10 0x10
/* Restarts the song if it is already playing */
#define SONG_FLAG_RESTART 0x80

/* The song id when no song is playing */
#define NO_SONG -1
/* libmus scales a song's own tempo by SetSongTempo's value over this */
#define NORMAL_SONG_TEMPO 0x80

/* The audio channel for the next sound effect, alternating between two */
extern s32 g_NextSoundChannel;

void PlaySound(s32 soundId, s32 channel, s32 volume);
void PlaySoundWithEffects(
    s32 soundId, s32 channel, s32 volume, s32 reverb, s32 pan);
void StopSound(s32 channel);
void SetSoundPan(s32 channel, s32 pan);
void SetSoundVolume(s32 channel, s32 volume);
void SetSoundReverb(s32 channel, s32 reverb);
void FadeOutSound(s32 channel, u32 frameCount);
void SetSoundTempo(s32 channel, s32 tempo);
void PlayCharacterSound(
    s32 character, s32 slot, s32 soundIndex, s32 volume, s32 pan);
void QueueCharacterSound(
    s32 character, s32 slot, s32 kind, s32 volume, s32 pan);
void SetSoundFrequencyOffset(s32 channel, f32 offset);
s32 UpdateAudio(void);
void StopSong(s32 fadeFrameCount);
void RequestSongStop(void);
void SetSongVolume(s32 volume);
void SetSongReverb(s32 reverb);
void SetSongTempo(s32 tempo);
void SetSongFrequencyOffset(f32 offset);
s32 GetSongId(void);
void PlaySong(s32 songId, s32 flags);
void InitAudio(void);

#endif
