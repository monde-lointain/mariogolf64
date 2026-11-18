/*
 * Where the music player's sound goes: the host's audio device, fed stereo
 * float frames that it plays in order.
 *
 * The music player calls these from its own thread. sdl_audio_output.c is
 * the port's; the tests link their own.
 */

#ifndef PORT_MUS_AUDIO_OUTPUT_H
#define PORT_MUS_AUDIO_OUTPUT_H

#include <stdbool.h>

// Opens the device at the sample rate, and gives whether it opened; while
// it isn't open, frames queued to it are dropped
bool OpenAudioOutput(int sampleRate);
void CloseAudioOutput(void);

// Sets how loud the output plays, from 0, silent, to 1, as the player
// made it, before the music player starts
void SetAudioOutputVolume(float volume);

// Queues frameCount frames, interleaved left and right
void QueueAudioOutput(const float *frames, int frameCount);

// The frames queued that the device has yet to play
int GetQueuedAudioFrames(void);

#endif
