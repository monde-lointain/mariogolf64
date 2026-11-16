/*
 * Tests of the synthesizer: what its voices play at a pitch, a volume, a pan
 * and a send, how they loop and stop, and how the effect delays the send.
 */

#include <PR/libaudio.h>
#include <SDL3/SDL.h>
#include <string.h>

#include "reverb.h"
#include "synth.h"
#include "test_support.h"

#define SAMPLE_RATE 32000
#define STEREO 2
#define FULL_SCALE 32768.0
#define TOLERANCE 1e-5

static int16_t f_Pcm[16];
static Sample f_Sample;
static float f_Output[SYNTH_MAX_BLOCK_FRAMES * STEREO];

// A sample of 16 PCM values, 1000 apart, already decoded
static void MakeRampSample(void) {
    int i;

    for (i = 0; i < 16; i++) {
        f_Pcm[i] = (int16_t)(1000 * (i + 1));
    }

    memset(&f_Sample, 0, sizeof(f_Sample));
    f_Sample.data = (const uint8_t *)f_Pcm;
    f_Sample.encoding = SAMPLE_PCM16;
    f_Sample.pcm = f_Pcm;
    f_Sample.pcmLength = 16;
}

static void StartVoice(int voice, float pitch, uint8_t pan) {
    StartSynthVoice(voice, &f_Sample);
    SetSynthVoicePitch(voice, pitch);
    SetSynthVoiceVolume(voice, SYNTH_MAX_VOLUME);
    SetSynthVoicePan(voice, pan);
    SetSynthVoiceSend(voice, 0);
}

static double Left(int frame) {
    return f_Output[frame * STEREO];
}

static double Right(int frame) {
    return f_Output[(frame * STEREO) + 1];
}

// At a pitch of 1 a voice plays its samples as they are, then stops
static void TestPlaysAtPitch(void) {
    int frame;

    InitSynth(SAMPLE_RATE, 4);
    MakeRampSample();
    StartVoice(0, 1.0f, 0);
    RenderSynth(f_Output, 20);
    for (frame = 0; frame < 16; frame++) {
        EXPECT_NEAR(Left(frame), f_Pcm[frame] / FULL_SCALE, TOLERANCE);
        EXPECT_NEAR(Right(frame), 0.0, TOLERANCE);
    }

    EXPECT_NEAR(Left(16), 0.0, TOLERANCE);
    EXPECT(!IsSynthVoicePlaying(0));
}

// At a pitch of 2 it skips every other sample and ends twice as soon; at a
// half it reads between them
static void TestPitchSteps(void) {
    InitSynth(SAMPLE_RATE, 4);
    MakeRampSample();
    StartVoice(0, 2.0f, 0);
    RenderSynth(f_Output, 4);
    EXPECT_NEAR(Left(1), f_Pcm[2] / FULL_SCALE, TOLERANCE);
    EXPECT_NEAR(Left(3), f_Pcm[6] / FULL_SCALE, TOLERANCE);
    RenderSynth(f_Output, 8);
    EXPECT(!IsSynthVoicePlaying(0));

    StartVoice(0, 0.5f, 0);
    RenderSynth(f_Output, 4);
    // Midway along a straight line, the cubic is on the line
    EXPECT_NEAR(Left(3), (f_Pcm[1] + 500) / FULL_SCALE, TOLERANCE);
}

// Panned, a voice keeps its power: wholly on one side at the end, and
// shared by cosine and sine between
static void TestPanKeepsPower(void) {
    const double angle = (SYNTH_CENTER_PAN / (double)SYNTH_MAX_PAN) * (SDL_PI_D / 2.0);
    const double level = 1000 / FULL_SCALE;

    InitSynth(SAMPLE_RATE, 4);
    MakeRampSample();
    StartVoice(0, 1.0f, SYNTH_MAX_PAN);
    RenderSynth(f_Output, 1);
    EXPECT_NEAR(Left(0), 0.0, TOLERANCE);
    EXPECT_NEAR(Right(0), level, TOLERANCE);
    StartVoice(0, 1.0f, SYNTH_CENTER_PAN);
    RenderSynth(f_Output, 1);
    EXPECT_NEAR(Left(0), SDL_cos(angle) * level, TOLERANCE);
    EXPECT_NEAR(Right(0), SDL_sin(angle) * level, TOLERANCE);
}

// A looping voice goes back to its loop's start for as long as it plays
static void TestLoops(void) {
    int frame;

    InitSynth(SAMPLE_RATE, 4);
    MakeRampSample();
    f_Sample.loops = true;
    f_Sample.loopStart = 4;
    f_Sample.loopEnd = 8;
    f_Sample.loopCount = AL_LOOP_FOREVER;
    StartVoice(0, 1.0f, 0);
    RenderSynth(f_Output, 40);
    for (frame = 8; frame < 40; frame++) {
        EXPECT_NEAR(Left(frame), f_Pcm[4 + (frame % 4)] / FULL_SCALE, TOLERANCE);
    }

    EXPECT(IsSynthVoicePlaying(0));

    // Between the loop's last sample and the next, the cubic reads on from
    // the loop's start: halfway from 8000 to 5000
    StartVoice(0, 0.5f, 0);
    RenderSynth(f_Output, 16);
    EXPECT_NEAR(Left(15), 6500 / FULL_SCALE, TOLERANCE);

    // A loop of one pass goes back once, then plays on to the end
    f_Sample.loopCount = 1;
    StartVoice(0, 1.0f, 0);
    RenderSynth(f_Output, 30);
    EXPECT_NEAR(Left(8), f_Pcm[4] / FULL_SCALE, TOLERANCE);
    EXPECT_NEAR(Left(19), f_Pcm[15] / FULL_SCALE, TOLERANCE);
    EXPECT_NEAR(Left(20), 0.0, TOLERANCE);
}

// A voice starts at its volume, but a change glides across the next block
static void TestVolumeGlides(void) {
    const int frames = 8;
    int frame;

    InitSynth(SAMPLE_RATE, 4);
    MakeRampSample();
    f_Sample.loops = true;
    f_Sample.loopStart = 0;
    f_Sample.loopEnd = 16;
    f_Sample.loopCount = AL_LOOP_FOREVER;
    StartVoice(0, 0.0f, 0);
    RenderSynth(f_Output, frames);
    EXPECT_NEAR(Left(0), f_Pcm[0] / FULL_SCALE, TOLERANCE);
    SetSynthVoiceVolume(0, 0);
    RenderSynth(f_Output, frames);
    for (frame = 0; frame < frames; frame++) {
        const double share = 1.0 - ((frame + 1) / (double)frames);

        EXPECT_NEAR(Left(frame), share * f_Pcm[0] / FULL_SCALE, TOLERANCE);
    }

    RenderSynth(f_Output, frames);
    EXPECT_NEAR(Left(0), 0.0, TOLERANCE);
}

// Voices add up, and the sum is clamped
static void TestMixClamps(void) {
    int voice;

    InitSynth(SAMPLE_RATE, 4);
    MakeRampSample();
    f_Pcm[0] = INT16_MAX;
    for (voice = 0; voice < 2; voice++) {
        StartVoice(voice, 1.0f, 0);
    }

    RenderSynth(f_Output, 2);
    EXPECT_NEAR(Left(0), 1.0, TOLERANCE);
    EXPECT_NEAR(Left(1), 2 * f_Pcm[1] / FULL_SCALE, TOLERANCE);
    StopSynthVoice(0);
    StopSynthVoice(1);
    RenderSynth(f_Output, 2);
    EXPECT_NEAR(Left(0), 0.0, TOLERANCE);
}

// A voice sent wholly to the echo is heard only once the echo's delay has
// passed, and none of it reaches a side its pan keeps it from
static void TestEchoDelaysTheSend(void) {
    const int blocks = 8;
    int block;
    int frame;
    int firstHeard = -1;
    double rightPeak = 0.0;

    InitSynth(SAMPLE_RATE, 4);
    MakeRampSample();
    SelectReverb(REVERB_ECHO);
    EXPECT(GetReverbKind() == REVERB_ECHO);
    StartVoice(0, 1.0f, 0);
    SetSynthVoiceSend(0, SYNTH_MAX_SEND);
    for (block = 0; block < blocks; block++) {
        RenderSynth(f_Output, 1000);
        for (frame = 0; frame < 1000; frame++) {
            if (firstHeard < 0 && SDL_fabs(Left(frame)) > 1e-4) {
                firstHeard = (block * 1000) + frame;
            }

            rightPeak = SDL_max(rightPeak, SDL_fabs(Right(frame)));
        }
    }

    // 220 ms at 32 kHz
    EXPECT(firstHeard >= 7039 && firstHeard <= 7041);
    EXPECT(rightPeak < 1e-4);
}

int main(void) {
    TestPlaysAtPitch();
    TestPitchSteps();
    TestPanKeepsPower();
    TestLoops();
    TestVolumeGlides();
    TestMixClamps();
    TestEchoDelaysTheSend();
    return ReportTestResults("synth_test");
}
