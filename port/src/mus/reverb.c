/*
 * The synthesizer's effects.
 *
 * Each side of the stereo send runs through parallel feedback delays, each
 * with a low-pass filter in its loop, whose outputs are averaged and then
 * diffused by allpass filters in series. A chorus or a flanger is one delay
 * whose length a slow sine sweeps. The right side's delays are a little
 * longer than the left's, and its sweep a quarter turn ahead, so that the
 * effect spreads across the stereo field.
 */

#include "reverb.h"

#include <SDL3/SDL.h>

#define MAX_FEEDBACK_DELAYS 4
#define MAX_ALLPASSES 2
#define SIDE_COUNT 2
// A power of two, so that positions wrap with a mask; holds 0.5 s at 32 kHz
#define DELAY_LINE_CAPACITY 16384
#define DELAY_LINE_MASK (DELAY_LINE_CAPACITY - 1)
#define RIGHT_SIDE_STRETCH 1.017f
#define RIGHT_SIDE_SWEEP_LEAD (SDL_PI_F / 2.0f)
#define MILLISECONDS_PER_SECOND 1000.0f

typedef struct ReverbDesign {
    int delayCount;
    float delayMilliseconds[MAX_FEEDBACK_DELAYS];
    float feedback;
    float damping; // 0 leaves the loops bright; toward 1, each pass darkens more
    float sweepMilliseconds;
    float sweepHertz;
    int allpassCount;
    float allpassMilliseconds[MAX_ALLPASSES];
    float allpassGain;
    float outputGain;
} ReverbDesign;

typedef struct DelayLine {
    float samples[DELAY_LINE_CAPACITY];
    uint32_t writePosition; // Kept below the capacity, so that it stays exact as a float
    float lowPass;
} DelayLine;

typedef struct ReverbSide {
    DelayLine feedbackDelays[MAX_FEEDBACK_DELAYS];
    DelayLine allpasses[MAX_ALLPASSES];
    float delayLengths[MAX_FEEDBACK_DELAYS]; // In samples
    float allpassLengths[MAX_ALLPASSES];
    float sweepDepth; // In samples
    float sweepPhaseLead;
} ReverbSide;

static const ReverbDesign f_Designs[REVERB_KIND_COUNT] = {
    [REVERB_NONE] = {0},
    [REVERB_SMALL_ROOM] =
        {
            .delayCount = 4,
            .delayMilliseconds = {19.3f, 23.9f, 27.1f, 31.7f},
            .feedback = 0.62f,
            .damping = 0.35f,
            .allpassCount = 2,
            .allpassMilliseconds = {4.9f, 1.6f},
            .allpassGain = 0.5f,
            .outputGain = 0.5f,
        },
    [REVERB_BIG_ROOM] =
        {
            .delayCount = 4,
            .delayMilliseconds = {41.3f, 47.9f, 53.1f, 61.7f},
            .feedback = 0.8f,
            .damping = 0.3f,
            .allpassCount = 2,
            .allpassMilliseconds = {6.1f, 2.3f},
            .allpassGain = 0.6f,
            .outputGain = 0.45f,
        },
    [REVERB_CHORUS] =
        {
            .delayCount = 1,
            .delayMilliseconds = {14.0f},
            .sweepMilliseconds = 2.5f,
            .sweepHertz = 0.8f,
            .outputGain = 0.8f,
        },
    [REVERB_FLANGE] =
        {
            .delayCount = 1,
            .delayMilliseconds = {2.5f},
            .feedback = 0.6f,
            .sweepMilliseconds = 2.0f,
            .sweepHertz = 0.25f,
            .outputGain = 0.6f,
        },
    [REVERB_ECHO] = {
        .delayCount = 1,
        .delayMilliseconds = {220.0f},
        .feedback = 0.38f,
        .damping = 0.45f,
        .outputGain = 0.7f,
    },
};

static int f_SampleRate = 32000;
static ReverbKind f_Kind = REVERB_NONE;
static float f_SweepPhase;
static float f_SweepStep;
static ReverbSide f_Sides[SIDE_COUNT];

static float MillisecondsToSamples(float milliseconds, float stretch) {
    const float samples = milliseconds * stretch * (float)f_SampleRate / MILLISECONDS_PER_SECOND;

    return SDL_clamp(samples, 1.0f, (float)(DELAY_LINE_CAPACITY - 2));
}

static void ConfigureSide(ReverbSide *side, const ReverbDesign *design, float stretch, float lead) {
    int i;

    SDL_zerop(side);
    for (i = 0; i < design->delayCount; i++) {
        side->delayLengths[i] = MillisecondsToSamples(design->delayMilliseconds[i], stretch);
    }

    for (i = 0; i < design->allpassCount; i++) {
        side->allpassLengths[i] = MillisecondsToSamples(design->allpassMilliseconds[i], stretch);
    }

    side->sweepDepth = design->sweepMilliseconds * (float)f_SampleRate / MILLISECONDS_PER_SECOND;
    side->sweepPhaseLead = lead;
}

void InitReverb(int sampleRate) {
    SDL_assert(sampleRate > 0);
    f_SampleRate = sampleRate;
    SelectReverb(REVERB_NONE);
}

void SelectReverb(ReverbKind kind) {
    const ReverbDesign *design;

    SDL_assert(kind >= 0 && kind < REVERB_KIND_COUNT);
    f_Kind = kind;
    design = &f_Designs[kind];
    f_SweepPhase = 0.0f;
    f_SweepStep = 2.0f * SDL_PI_F * design->sweepHertz / (float)f_SampleRate;
    ConfigureSide(&f_Sides[0], design, 1.0f, 0.0f);
    ConfigureSide(&f_Sides[1], design, RIGHT_SIDE_STRETCH, RIGHT_SIDE_SWEEP_LEAD);
}

ReverbKind GetReverbKind(void) {
    return f_Kind;
}

// The line's sample delay samples ago, between samples by linear
// interpolation
static float ReadDelayLine(const DelayLine *line, float delay) {
    float position = (float)line->writePosition - delay;
    float whole;
    float fraction;
    uint32_t index;
    float earlier;
    float later;

    if (position < 0.0f) {
        position += (float)DELAY_LINE_CAPACITY;
    }

    whole = SDL_floorf(position);
    fraction = position - whole;
    index = (uint32_t)whole;
    earlier = line->samples[index & DELAY_LINE_MASK];
    later = line->samples[(index + 1) & DELAY_LINE_MASK];
    return earlier + ((later - earlier) * fraction);
}

static void WriteDelayLine(DelayLine *line, float sample) {
    line->samples[line->writePosition] = sample;
    line->writePosition = (line->writePosition + 1) & DELAY_LINE_MASK;
}

static float RunSide(ReverbSide *side, const ReverbDesign *design, float input, float sweep) {
    float sum = 0.0f;
    int i;

    for (i = 0; i < design->delayCount; i++) {
        DelayLine *line = &side->feedbackDelays[i];
        const float delayed = ReadDelayLine(line, side->delayLengths[i] + sweep);

        line->lowPass = delayed + (design->damping * (line->lowPass - delayed));
        WriteDelayLine(line, input + (line->lowPass * design->feedback));
        sum += delayed;
    }

    sum /= (float)design->delayCount;
    for (i = 0; i < design->allpassCount; i++) {
        DelayLine *line = &side->allpasses[i];
        const float delayed = ReadDelayLine(line, side->allpassLengths[i]);

        WriteDelayLine(line, sum + (design->allpassGain * delayed));
        sum = delayed - (design->allpassGain * sum);
    }

    return sum * design->outputGain;
}

void ApplyReverb(const float *send, float *output, int frameCount) {
    const ReverbDesign *design = &f_Designs[f_Kind];
    int frame;
    int side;

    SDL_assert((send != NULL && output != NULL) || frameCount == 0);
    if (design->delayCount == 0) {
        return;
    }

    for (frame = 0; frame < frameCount; frame++) {
        for (side = 0; side < SIDE_COUNT; side++) {
            ReverbSide *state = &f_Sides[side];
            float sweep = 0.0f;

            if (state->sweepDepth != 0.0f) {
                sweep = state->sweepDepth * (1.0f + SDL_sinf(f_SweepPhase + state->sweepPhaseLead));
            }

            output[(frame * SIDE_COUNT) + side] +=
                RunSide(state, design, send[(frame * SIDE_COUNT) + side], sweep);
        }

        f_SweepPhase += f_SweepStep;
        if (f_SweepPhase >= 2.0f * SDL_PI_F) {
            f_SweepPhase -= 2.0f * SDL_PI_F;
        }
    }
}
