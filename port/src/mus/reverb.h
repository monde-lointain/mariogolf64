/*
 * The effect that the synthesizer puts on the voices' effect sends: a room's
 * reverberation, a chorus, a flanger or an echo.
 *
 * The N64's effects came from libaudio's tables, which the port doesn't
 * have; these are its own, built from damped feedback delays and allpass
 * diffusers, and meant to sound like the kinds they stand for rather than
 * to match them sample for sample.
 */

#ifndef PORT_MUS_REVERB_H
#define PORT_MUS_REVERB_H

typedef enum ReverbKind {
    REVERB_NONE,
    REVERB_SMALL_ROOM,
    REVERB_BIG_ROOM,
    REVERB_CHORUS,
    REVERB_FLANGE,
    REVERB_ECHO,
    REVERB_KIND_COUNT
} ReverbKind;

// Sets the sample rate and clears the effect, which starts as none
void InitReverb(int sampleRate);

// Changes the effect, clearing what the last one still held
void SelectReverb(ReverbKind kind);
ReverbKind GetReverbKind(void);

// Adds the effect's output for frameCount frames of the stereo send to
// output, both interleaved left and right
void ApplyReverb(const float *send, float *output, int frameCount);

#endif
