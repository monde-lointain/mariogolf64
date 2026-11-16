/*
 * The synthesizer: voices that play the sample bank's samples at a pitch,
 * a volume, a place in the stereo field and a share sent to the effect,
 * mixed into blocks of stereo output.
 *
 * The sequencer changes the voices between blocks, and each block is one
 * of its ticks. A change of volume, pan or send glides across the next
 * block, as the N64's envelope mixer ramped it, except on a voice's first
 * block, which starts at its levels.
 */

#ifndef PORT_MUS_SYNTH_H
#define PORT_MUS_SYNTH_H

#include <stdbool.h>
#include <stdint.h>

#include "sound_data.h"

#define SYNTH_MAX_VOICES 64
// The levels the sequencer gives, as libaudio took them
#define SYNTH_MAX_VOLUME 0x7FFF
#define SYNTH_MAX_PAN 127
#define SYNTH_CENTER_PAN 64
#define SYNTH_MAX_SEND 127
// The most frames that one block can have
#define SYNTH_MAX_BLOCK_FRAMES 2048

// Sets the output's sample rate and the voices there are, all silent. A
// pitch of 1 plays a sample at the output's rate.
void InitSynth(int sampleRate, int voiceCount);

// Plays the sample from its start, cutting off what the voice played
void StartSynthVoice(int voice, Sample *sample);
void StopSynthVoice(int voice);
bool IsSynthVoicePlaying(int voice);

void SetSynthVoicePitch(int voice, float ratio);
// The gain is the square of the volume's share of SYNTH_MAX_VOLUME
void SetSynthVoiceVolume(int voice, uint16_t volume);
void SetSynthVoicePan(int voice, uint8_t pan);
void SetSynthVoiceSend(int voice, uint8_t send);

// Mixes frameCount frames of every voice, and the effect, into output,
// interleaved left and right and clamped to [-1, 1]
void RenderSynth(float *output, int frameCount);

#endif
