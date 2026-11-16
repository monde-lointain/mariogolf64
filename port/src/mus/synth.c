/*
 * The synthesizer's voices and mixer.
 *
 * A voice steps through its sample's decoded PCM at its pitch, in 32.32
 * fixed point, and reads between samples with a Catmull-Rom cubic. Its
 * volume, pan and send become four gains: left and right, dry and to the
 * effect, with equal-power curves for the pan and the send. The mix is in
 * floats, with the effect added to the dry mix before the clamp.
 *
 * The cost is small: at 32 kHz and 60 blocks a second, 24 voices are about
 * 13,000 interpolated samples a block.
 */

#include "synth.h"

#include <PR/libaudio.h>
#include <SDL3/SDL.h>

#include "reverb.h"

#define STEREO 2
#define FRACTION_BITS 32
#define MAX_PITCH 8.0f
#define LEVEL_COUNT 128

typedef struct VoiceGains {
    float left;
    float right;
    float sendLeft;
    float sendRight;
} VoiceGains;

typedef struct Voice {
    const int16_t *pcm;
    uint32_t length;
    bool loops;
    uint32_t loopStart;
    uint32_t loopEnd;
    uint32_t loopsLeft;
    uint64_t position; // 32.32 fixed point
    uint64_t step;
    uint16_t volume;
    uint8_t pan;
    uint8_t send;
    bool isPlaying;
    bool isFresh; // No block has played it since it started
    VoiceGains gains; // Where the last block left them
} Voice;

static int f_SampleRate = 32000;
static int f_VoiceCount;
static Voice f_Voices[SYNTH_MAX_VOICES];
// Equal-power curves: cosine falling and sine rising across the levels
static float f_FallingCurve[LEVEL_COUNT];
static float f_RisingCurve[LEVEL_COUNT];
static float f_Send[SYNTH_MAX_BLOCK_FRAMES * STEREO];

static Voice *GetVoice(int voice) {
    SDL_assert(voice >= 0 && voice < f_VoiceCount);
    return &f_Voices[voice];
}

void InitSynth(int sampleRate, int voiceCount) {
    int level;

    SDL_assert(sampleRate > 0);
    SDL_assert(voiceCount >= 0 && voiceCount <= SYNTH_MAX_VOICES);
    f_SampleRate = sampleRate;
    f_VoiceCount = voiceCount;
    SDL_zeroa(f_Voices);
    for (level = 0; level < LEVEL_COUNT; level++) {
        const float angle = (float)level * (SDL_PI_F / 2.0f) / (float)(LEVEL_COUNT - 1);

        f_FallingCurve[level] = SDL_cosf(angle);
        f_RisingCurve[level] = SDL_sinf(angle);
    }

    InitReverb(sampleRate);
}

void StartSynthVoice(int voice, Sample *sample) {
    Voice *state = GetVoice(voice);
    uint32_t length = 0;
    const int16_t *pcm = (sample != NULL) ? GetSamplePcm(sample, &length) : NULL;

    state->isPlaying = pcm != NULL && length != 0;
    state->isFresh = true;
    state->position = 0;
    if (!state->isPlaying) {
        return;
    }

    state->pcm = pcm;
    state->length = length;
    state->loops = sample->loops && sample->loopEnd <= length;
    state->loopStart = sample->loopStart;
    state->loopEnd = sample->loopEnd;
    state->loopsLeft = sample->loopCount;
}

void StopSynthVoice(int voice) {
    GetVoice(voice)->isPlaying = false;
}

bool IsSynthVoicePlaying(int voice) {
    return GetVoice(voice)->isPlaying;
}

void SetSynthVoicePitch(int voice, float ratio) {
    const float clamped = SDL_clamp(ratio, 0.0f, MAX_PITCH);

    GetVoice(voice)->step = (uint64_t)((double)clamped * (double)(1ull << FRACTION_BITS));
}

void SetSynthVoiceVolume(int voice, uint16_t volume) {
    GetVoice(voice)->volume = SDL_min(volume, SYNTH_MAX_VOLUME);
}

void SetSynthVoicePan(int voice, uint8_t pan) {
    GetVoice(voice)->pan = SDL_min(pan, SYNTH_MAX_PAN);
}

void SetSynthVoiceSend(int voice, uint8_t send) {
    GetVoice(voice)->send = SDL_min(send, SYNTH_MAX_SEND);
}

static VoiceGains ComputeGains(const Voice *voice) {
    const float level = (float)voice->volume / (float)SYNTH_MAX_VOLUME;
    const float dry = level * f_FallingCurve[voice->send];
    const float wet = level * f_RisingCurve[voice->send];
    VoiceGains gains;

    gains.left = dry * f_FallingCurve[voice->pan];
    gains.right = dry * f_RisingCurve[voice->pan];
    gains.sendLeft = wet * f_FallingCurve[voice->pan];
    gains.sendRight = wet * f_RisingCurve[voice->pan];
    return gains;
}

// The voice's sample at index, with the loop wrapping indices past its end,
// and silence outside the sample
static float ReadTap(const Voice *voice, int64_t index) {
    if (voice->loops && index >= (int64_t)voice->loopEnd) {
        index -= voice->loopEnd - voice->loopStart;
    }

    if (index < 0 || index >= (int64_t)voice->length) {
        return 0.0f;
    }

    return (float)voice->pcm[index];
}

static float InterpolateVoice(const Voice *voice) {
    const int64_t index = (int64_t)(voice->position >> FRACTION_BITS);
    const float t = (float)(uint32_t)voice->position * (1.0f / 4294967296.0f);
    const float p0 = ReadTap(voice, index - 1);
    const float p1 = ReadTap(voice, index);
    const float p2 = ReadTap(voice, index + 1);
    const float p3 = ReadTap(voice, index + 2);
    const float a = (3.0f * (p1 - p2)) + p3 - p0;
    const float b = (2.0f * p0) - (5.0f * p1) + (4.0f * p2) - p3;
    const float c = p2 - p0;

    return (p1 + (0.5f * t * (c + (t * (b + (t * a)))))) * (1.0f / 32768.0f);
}

// Steps the voice on, through its loop or to its end
static void AdvanceVoice(Voice *voice) {
    uint64_t index;

    voice->position += voice->step;
    index = voice->position >> FRACTION_BITS;
    if (voice->loops && voice->loopsLeft != 0 && index >= voice->loopEnd) {
        voice->position -= (uint64_t)(voice->loopEnd - voice->loopStart) << FRACTION_BITS;
        if (voice->loopsLeft != AL_LOOP_FOREVER) {
            voice->loopsLeft--;
        }

        if (voice->loopsLeft == 0) {
            voice->loops = false;
        }
    }
    else if (index >= voice->length) {
        voice->isPlaying = false;
    }
}

static void MixVoice(Voice *voice, float *dry, float *send, int frameCount) {
    const VoiceGains target = ComputeGains(voice);
    VoiceGains gains = voice->isFresh ? target : voice->gains;
    const float scale = 1.0f / (float)frameCount;
    const VoiceGains step = {
        (target.left - gains.left) * scale,
        (target.right - gains.right) * scale,
        (target.sendLeft - gains.sendLeft) * scale,
        (target.sendRight - gains.sendRight) * scale,
    };
    int frame;

    for (frame = 0; frame < frameCount && voice->isPlaying; frame++) {
        const float sample = InterpolateVoice(voice);

        gains.left += step.left;
        gains.right += step.right;
        gains.sendLeft += step.sendLeft;
        gains.sendRight += step.sendRight;
        dry[(frame * STEREO) + 0] += sample * gains.left;
        dry[(frame * STEREO) + 1] += sample * gains.right;
        send[(frame * STEREO) + 0] += sample * gains.sendLeft;
        send[(frame * STEREO) + 1] += sample * gains.sendRight;
        AdvanceVoice(voice);
    }

    voice->gains = target;
    voice->isFresh = false;
}

static void RenderBlock(float *output, int frameCount) {
    const int valueCount = frameCount * STEREO;
    int voice;
    int i;

    SDL_memset(output, 0, (size_t)valueCount * sizeof(float));
    SDL_memset(f_Send, 0, (size_t)valueCount * sizeof(float));
    for (voice = 0; voice < f_VoiceCount; voice++) {
        if (f_Voices[voice].isPlaying) {
            MixVoice(&f_Voices[voice], output, f_Send, frameCount);
        }
    }

    ApplyReverb(f_Send, output, frameCount);
    for (i = 0; i < valueCount; i++) {
        output[i] = SDL_clamp(output[i], -1.0f, 1.0f);
    }
}

void RenderSynth(float *output, int frameCount) {
    SDL_assert(output != NULL || frameCount == 0);
    SDL_assert(frameCount >= 0 && frameCount <= SYNTH_MAX_BLOCK_FRAMES);
    frameCount = SDL_clamp(frameCount, 0, SYNTH_MAX_BLOCK_FRAMES);
    if (frameCount > 0) {
        RenderBlock(output, frameCount);
    }
}
