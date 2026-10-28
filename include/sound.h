#ifndef SOUND_H
#define SOUND_H

#include "common.h"

/* Sound effects play on channels, each holding one effect at a time */
#define SOUND_CHANNEL_COUNT 16

/* A ball being struck by a plain club or bouncing on hard ground */
#define SOUND_CONTACT_PLAIN 0x51

/* The sound of confirming a choice */
#define SOUND_CONFIRM 0x5D

/* The sound of a menu choice that cannot be made, or a cursor that cannot move
 * that way */
#define SOUND_MENU_BLOCKED 0x63

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
