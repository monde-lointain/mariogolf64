/*
 * The sequencer: the channels that play the Music Tools' songs and sound
 * effects, one tick per video frame, on the synthesizer's voices.
 *
 * Each voice has a channel, and a few more channels play songs' master
 * tracks, which carry the tempo and markers but no notes. A song takes a
 * channel for each of its tracks, and an effect one for itself; every
 * channel that a song or effect starts shares its handle, through which
 * the game changes or stops them together.
 *
 * Time on a channel runs in 256ths of the data's ticks, at its tempo per
 * video frame, so that tempos between whole ticks keep their pace.
 */

#ifndef PORT_MUS_SEQUENCER_H
#define PORT_MUS_SEQUENCER_H

#include <stdbool.h>
#include <stdint.h>

#include "sound_data.h"

#define SEQUENCER_MASTER_TRACK_CHANNELS 4
// The kinds of sound that StopSoundKinds and CountSoundKinds take, as libmus
// numbered them
#define SOUND_KIND_EFFECTS 1u
#define SOUND_KIND_SONGS 2u
// An effect started at this priority takes the priority from its bank
#define EFFECT_BANK_PRIORITY (-1)
#define NO_SOUND 0u

typedef void (*MarkerHandler)(uint32_t handle, int marker);
typedef void (*EffectChangeHandler)(int effect);

// Sets up the channels for the synthesizer's voices, all idle, at a tick
// rate of ticksPerSecond
void InitSequencer(int voiceCount, int ticksPerSecond);

// Plays one video frame's worth of every channel
void TickSequencer(void);

// Starts a song with the sample bank, and gives its handle
uint32_t StartSong(const SongLayout *layout, SampleBank *samples);

// Starts one of the bank's effects with the sample bank, scaled in volume and
// pan, and gives its handle; NO_SOUND if it has no channel to play on. With
// restarts, an effect of the same number that is playing is started over on
// its channel.
uint32_t StartEffect(
    const EffectBank *bank,
    int number,
    SampleBank *samples,
    int volumeScale,
    int panScale,
    int priority,
    bool restarts
);

// Fades out every sound of the kinds over frames video frames, and gives how
// many channels it started fading
int StopSoundKinds(uint32_t kinds, int frames);
// How many channels play sounds of the kinds
int CountSoundKinds(uint32_t kinds);

// Each of these acts on the channels of one handle, and gives how many
// there were
int StopSound(uint32_t handle, int frames);
int CountSoundChannels(uint32_t handle);
int SetSoundVolumeScale(uint32_t handle, int scale);
int SetSoundPanScale(uint32_t handle, int scale);
int SetSoundPitchOffset(uint32_t handle, float semitones);
int SetSoundTempoScale(uint32_t handle, int scale);
int SetSoundReverbBase(uint32_t handle, int level);

void SetMasterVolume(uint32_t kinds, int volume);
void SetStereoOutput(bool isStereo);

// The handlers for songs' markers, on their master tracks, and their
// requests to change the effect, which are ignored while there is none
void SetSequencerHandlers(MarkerHandler onMarker, EffectChangeHandler onEffectChange);

// The semitones above a sample's own pitch as a playback rate, as the N64's
// player worked it out
float SemitonesToRate(float semitones);

#endif
