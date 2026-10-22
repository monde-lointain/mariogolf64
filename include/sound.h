#ifndef SOUND_H
#define SOUND_H

#include "common.h"

/* Sound effects play on channels, each holding one effect at a time */
#define SOUND_CHANNEL_COUNT 16

/* PlaySong's flags */
#define SONG_FLAG_UNK10 0x10   /* Stops the song instead when SAVE_OPTION_UNK0 is set */
#define SONG_FLAG_RESTART 0x80 /* Restarts the song if it is already playing */

/* The audio channel for the next sound effect, alternating between two */
extern s32 g_NextSoundChannel;

void PlaySound(s32 soundId, s32 channel, s32 volume);
void PlaySoundWithEffects(s32 soundId, s32 channel, s32 volume, s32 reverb, s32 pan);
void StopSound(s32 channel);
void SetSoundPan(s32 channel, s32 pan);
void SetSoundVolume(s32 channel, s32 volume);
void SetSoundReverb(s32 channel, s32 reverb);
void FadeOutSound(s32 channel, u32 frameCount);
void SetSoundTempo(s32 channel, s32 tempo);
void QueueCharacterSound(s32 character, s32 slot, s32 kind, s32 volume, s32 pan);
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
