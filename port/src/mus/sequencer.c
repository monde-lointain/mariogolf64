/*
 * The sequencer's channels and the Music Tools' track commands.
 *
 * A track is a stream of bytes: a byte of 0x80 or more is a command, with
 * its arguments after it, and anything less is a note, followed by its
 * velocity while velocities are inline and by its length unless a fixed
 * length is set. Lengths, wave numbers and other counts that can pass 127
 * take a second byte when the first has its top bit set. Note 96 is a rest.
 *
 * A note's volume is the product of the track's volume, its envelope, its
 * velocity and the handle's scale, then the master volume and any fade.
 * Its pitch is the note, the sample's tuning and the transposition, glided
 * from the last note if asked, plus the pitch offset, vibrato, wobble and
 * pitch bend.
 *
 * The behaviour follows what the game's songs expect of the N64's player:
 * when a channel's notes are read, how a new note fades out the one still
 * sounding before its sample starts, and the envelope's stages.
 */

#include "sequencer.h"

#include <SDL3/SDL.h>

#include "synth.h"

#define MAX_CHANNELS (SYNTH_MAX_VOICES + SEQUENCER_MASTER_TRACK_CHANNELS)
#define CLOCK_UNITS_PER_TICK 256
#define CLOCK_TICK_SHIFT 8
// The data's ticks per second at a tempo of 120 beats a minute
#define TICKS_PER_SECOND_AT_120 96
#define TEMPO_SCALE_UNITY 0x80
#define SCALE_SHIFT 7
#define MAX_TEMPO_SCALE 256
#define REST_NOTE 96
// A note this long holds until the track stops it
#define HELD_NOTE_LENGTH 0x7FFF
#define LONG_VALUE_FLAG 0x80
#define LOOP_DEPTH 4
// A loop of this many passes repeats forever
#define ENDLESS_LOOP 0xFF
#define NOT_FADING (-1)
#define MAX_LEVEL 127
#define MAX_REVERB 127
#define DEFAULT_PAN 64
#define SEMITONES_PER_OCTAVE 12.0f
// Past twice a sample's rate, the N64 couldn't play it, and the note is
// silenced
#define MAX_RATE 2.0f
#define VOLUME_PRODUCT_SHIFT 13
#define MASTER_VOLUME_SHIFT 15
#define MAX_MASTER_VOLUME 0x7FFF
#define UNKNOWN_VOLUME 0xFFFF
// A pitch bend level of 64 is no bend; the range is in 64ths of a semitone
#define BEND_CENTER 64.0f
#define BEND_RANGE_UNITS 64.0f
#define DEFAULT_BEND_RANGE 2
#define DISTORTION_UNITS 100.0f
#define VIBRATO_DEPTH_UNITS 50.0f
// The sweep moves the pan by its speed in 64ths of a step a tick
#define SWEEP_CARRY_BITS 6
#define SWEEP_CARRY_MASK ((1 << SWEEP_CARRY_BITS) - 1)
#define MAX_SWEPT_PAN 0x7F
#define FALLING_SWEEP_PAN 0x40
// Envelope steps are counted in 1024ths of a tick times its period
#define ENVELOPE_STEP_SHIFT 10
#define ENVELOPE_STEP_UNITY (1 << ENVELOPE_STEP_SHIFT)
#define INLINE_ENVELOPE_SIZE 7
// More commands than any note of the game's is preceded by; past it the
// track is taken to loop without notes, and is stopped
#define COMMAND_LIMIT 4096

typedef enum Command {
    COMMAND_END = 0x80,
    COMMAND_WAVE,
    COMMAND_GLIDE,
    COMMAND_GLIDE_OFF,
    COMMAND_ENVELOPE,
    COMMAND_TEMPO,
    COMMAND_CUTOFF,
    COMMAND_RELEASE_LEAD,
    COMMAND_VIBRATO_UP,
    COMMAND_VIBRATO_DOWN,
    COMMAND_VIBRATO_OFF,
    COMMAND_FIXED_LENGTH,
    COMMAND_NEXT_LENGTH_INLINE,
    COMMAND_TRANSPOSE,
    COMMAND_LOCK_TRANSPOSITION,
    COMMAND_DISTORT,
    COMMAND_SONG_ENVELOPE,
    COMMAND_HOLD_ENVELOPE,
    COMMAND_RETRIGGER_ENVELOPE,
    COMMAND_HOLD_SAMPLE,
    COMMAND_RETRIGGER_SAMPLE,
    COMMAND_LOOP_START,
    COMMAND_LOOP_END,
    COMMAND_WOBBLE,
    COMMAND_WOBBLE_OFF,
    COMMAND_INLINE_VELOCITY,
    COMMAND_DEFAULT_VELOCITY_ONLY,
    COMMAND_DEFAULT_VELOCITY,
    COMMAND_PAN,
    COMMAND_STEREO,
    COMMAND_DRUMS,
    COMMAND_DRUMS_OFF,
    COMMAND_PRINT,
    COMMAND_GOTO,
    COMMAND_REVERB,
    COMMAND_RANDOM_TRANSPOSE,
    COMMAND_RANDOM_VOLUME,
    COMMAND_RANDOM_PAN,
    COMMAND_VOLUME,
    COMMAND_START_EFFECT,
    COMMAND_BEND_RANGE,
    COMMAND_SWEEP,
    COMMAND_CHANGE_EFFECT,
    COMMAND_MARKER,
    COMMAND_FIXED_LENGTH_OFF,
    COMMAND_COUNT_END
} Command;

typedef enum EnvelopeStage {
    ENVELOPE_IDLE,
    ENVELOPE_ATTACK,
    ENVELOPE_DECAY,
    ENVELOPE_SUSTAIN,
    ENVELOPE_RELEASE,
    ENVELOPE_DONE
} EnvelopeStage;

// An envelope's shape, as a track sets it
typedef struct EnvelopeShape {
    uint8_t period; // Frames between updates
    int32_t stepScale; // ENVELOPE_STEP_UNITY / period
    uint8_t initial;
    uint8_t peak;
    uint8_t sustain;
    uint8_t attackTicks;
    uint8_t decayTicks;
    uint8_t releaseTicks;
    float attackSlope; // Level per step
    float decaySlope;
    float releaseShare; // Share of the level released per step
} EnvelopeShape;

// Where a note's envelope is
typedef struct EnvelopeState {
    EnvelopeStage stage;
    uint8_t countdown;
    int32_t level;
    int32_t releaseFrom;
    uint32_t releaseClock;
} EnvelopeState;

// A run-length coded stream of levels beside a track, one level a tick
typedef struct LevelStream {
    const uint8_t *start;
    const uint8_t *cursor;
    uint32_t clock;
    uint16_t runLeft;
} LevelStream;

// What a loop goes back to on each pass
typedef struct LoopMark {
    const uint8_t *notes;
    const uint8_t *volumes;
    const uint8_t *bends;
    uint16_t volumeRunLeft;
    uint16_t bendRunLeft;
    uint8_t volume;
    uint8_t passesLeft;
    float bend;
} LoopMark;

typedef struct Channel {
    // What it plays
    const uint8_t *cursor; // NULL while the channel is idle
    const uint8_t *trackStart;
    LevelStream volumes;
    LevelStream bends;
    const uint8_t *song; // The song's start, for a song's channel
    const uint8_t *envelopes;
    const uint8_t *drumTable;
    const uint8_t *drums; // The drum map in use, if any
    const uint8_t *waves;
    const EffectBank *effectBank; // For an effect's channel
    int effectNumber;
    SampleBank *samples;
    uint32_t handle;
    int32_t priority;
    bool isMasterTrack;

    // Time
    uint32_t clock;
    uint32_t noteStartClock;
    uint32_t noteEndClock;
    int32_t ticksIntoNote;
    uint16_t tempo;
    uint16_t unscaledTempo;
    uint16_t tempoScale;
    uint16_t length;
    uint16_t fixedLength;
    bool hasInlineLength; // The next note gives its length despite a fixed one

    // The note
    uint16_t wave;
    uint8_t transposition;
    bool isTranspositionLocked;
    bool hasInlineVelocity;
    uint8_t velocity;
    uint8_t defaultVelocity;
    float note;
    float glideFrom;
    float glideNote;
    uint8_t glideTicks;
    bool isEnvelopeHeld;
    bool isSampleHeld;
    uint16_t cutoffTicks;
    uint8_t releaseLeadTicks;
    EnvelopeShape envelope;
    EnvelopeState envelopeState;

    // Modulation
    uint8_t vibratoDelay;
    uint8_t vibratoPeriod;
    float vibratoDepth;
    uint8_t wobbleAmount;
    uint8_t wobbleOnTicks;
    uint8_t wobbleOffTicks;
    uint8_t wobbleCountdown;
    uint8_t wobbleValue;
    uint8_t sweepSpeed;
    uint8_t sweepCarry;
    bool isSweepFalling;
    uint32_t sweepClock;
    float bend;
    float bendRange;
    float pitchOffset;
    float distortion;

    // Levels
    uint8_t volume;
    uint8_t pan;
    uint8_t reverb;
    uint8_t reverbBase;
    int32_t volumeScale;
    int32_t panScale;
    LoopMark loops[LOOP_DEPTH];
    int loopDepth;
    int32_t fadeLeft;
    int32_t fadeLength;

    // The voice
    Sample *pendingSample;
    bool isSounding;
    uint16_t lastVolume;
} Channel;

static Channel f_Channels[MAX_CHANNELS];
static int f_ChannelCount;
static int f_TicksPerSecond = 60;
static uint32_t f_NextHandle = 1;
static uint32_t f_RandomState = 0x2545F491u;
static int32_t f_SongMasterVolume = MAX_MASTER_VOLUME;
static int32_t f_EffectMasterVolume = MAX_MASTER_VOLUME;
static bool f_IsStereo = true;
static MarkerHandler f_OnMarker;
static EffectChangeHandler f_OnEffectChange;

static void ReadNextNote(Channel *channel, int voice);

// Whether clock a is before clock b, across the counters' wrap
static bool IsBefore(uint32_t a, uint32_t b) {
    return (int32_t)(a - b) < 0;
}

static int SignedByte(uint8_t value) {
    return (value & 0x80) ? value - 0x100 : value;
}

static int VoiceOf(int channelIndex) {
    return channelIndex - SEQUENCER_MASTER_TRACK_CHANNELS;
}

static uint16_t DefaultTempo(void) {
    return (uint16_t)(TICKS_PER_SECOND_AT_120 * CLOCK_UNITS_PER_TICK / f_TicksPerSecond);
}

static uint8_t ReadByte(const uint8_t **cursor) {
    return *(*cursor)++;
}

// A count of one byte, or of fifteen bits over two
static uint16_t ReadCount(const uint8_t **cursor) {
    uint16_t value = ReadByte(cursor);

    if (value & LONG_VALUE_FLAG) {
        value = (uint16_t)(((value & ~LONG_VALUE_FLAG) << 8) | ReadByte(cursor));
    }

    return value;
}

// A whole number in [0, range), from a xorshift generator
static int RandomBelow(int range) {
    f_RandomState ^= f_RandomState << 13;
    f_RandomState ^= f_RandomState >> 17;
    f_RandomState ^= f_RandomState << 5;
    if (range <= 0) {
        return 0;
    }

    return (int)(f_RandomState % (uint32_t)range);
}

float SemitonesToRate(float semitones) {
    // The N64's player took 2^x as the first seven terms of its Taylor
    // series, inverted for negative x, which runs flat far from 0, and the
    // game's music was tuned by ear on it
    static const float terms[] = {
        0.6931471806f,
        0.2402265070f,
        0.0555041087f,
        0.0096181291f,
        0.0013333558f,
        0.0001540353f,
    };
    const float octaves = semitones / SEMITONES_PER_OCTAVE;
    const float x = SDL_fabsf(octaves);
    float power = x;
    float sum = 1.0f;
    size_t i;

    for (i = 0; i < SDL_arraysize(terms); i++) {
        sum += terms[i] * power;
        power *= x;
    }

    return (octaves < 0.0f) ? 1.0f / sum : sum;
}

// --- Channels ---

static void SetDefaultEnvelope(EnvelopeShape *shape) {
    shape->period = 1;
    shape->stepScale = ENVELOPE_STEP_UNITY;
    shape->initial = 0;
    shape->attackTicks = 1;
    shape->attackSlope = 1.0f;
    shape->peak = MAX_LEVEL;
    shape->decayTicks = 255;
    shape->decaySlope = 0.0f;
    shape->sustain = MAX_LEVEL;
    shape->releaseTicks = 15;
    shape->releaseShare = 1.0f / 15.0f;
}

// Clears the channel for a new sound, leaving its voice as it is, so that a
// note it still sounds fades out before the next one starts
static void ResetChannel(Channel *channel, SampleBank *samples) {
    const bool isSounding = channel->isSounding;

    SDL_zerop(channel);
    channel->isSounding = isSounding;
    channel->lastVolume = UNKNOWN_VOLUME;
    channel->tempo = DefaultTempo();
    channel->unscaledTempo = channel->tempo;
    channel->tempoScale = TEMPO_SCALE_UNITY;
    channel->length = 1;
    channel->defaultVelocity = MAX_LEVEL;
    channel->volume = MAX_LEVEL;
    channel->pan = DEFAULT_PAN;
    channel->bendRange = DEFAULT_BEND_RANGE / BEND_RANGE_UNITS;
    channel->volumes.runLeft = 1;
    channel->bends.runLeft = 1;
    channel->fadeLeft = NOT_FADING;
    channel->volumeScale = TEMPO_SCALE_UNITY;
    channel->panScale = TEMPO_SCALE_UNITY;
    channel->samples = samples;
    SetDefaultEnvelope(&channel->envelope);
}

// Ends what the channel plays, and silences its voice
static void EndChannel(Channel *channel, int voice) {
    channel->cursor = NULL;
    channel->volumes.cursor = NULL;
    channel->bends.cursor = NULL;
    channel->song = NULL;
    channel->effectBank = NULL;
    channel->handle = NO_SOUND;
    channel->pendingSample = NULL;
    if (channel->isSounding) {
        channel->isSounding = false;
        if (voice >= 0) {
            StopSynthVoice(voice);
        }
    }
}

static bool IsEffectChannel(const Channel *channel) {
    return channel->effectBank != NULL;
}

static bool IsOfKinds(const Channel *channel, uint32_t kinds) {
    return IsEffectChannel(channel) ? (kinds & SOUND_KIND_EFFECTS) != 0 :
                                      (kinds & SOUND_KIND_SONGS) != 0;
}

static void StartFade(Channel *channel, int frames) {
    frames = SDL_max(frames, 0);
    channel->fadeLeft = frames;
    channel->fadeLength = SDL_max(frames, 1);
}

// --- Envelopes ---

// An envelope's shape from its seven bytes: period, initial level, attack
// ticks, peak, decay ticks, sustain level, release ticks
static void ReadEnvelopeShape(const uint8_t *bytes, EnvelopeShape *shape) {
    const uint8_t period = SDL_max(bytes[0], 1);
    const uint8_t attackTicks = SDL_max(bytes[2], 1);
    const uint8_t decayTicks = SDL_max(bytes[4], 1);
    const uint8_t releaseTicks = SDL_max(bytes[6], 1);

    shape->period = period;
    shape->stepScale = ENVELOPE_STEP_UNITY / period;
    shape->initial = bytes[1];
    shape->attackTicks = attackTicks;
    shape->peak = bytes[3];
    shape->attackSlope = (float)(shape->peak - shape->initial) / (float)attackTicks;
    shape->decayTicks = decayTicks;
    shape->sustain = bytes[5];
    shape->decaySlope = (float)(shape->sustain - shape->peak) / (float)decayTicks;
    shape->releaseTicks = releaseTicks;
    shape->releaseShare = 1.0f / (float)releaseTicks;
}

static void UseSongEnvelope(Channel *channel, int index) {
    if (channel->envelopes != NULL) {
        ReadEnvelopeShape(channel->envelopes + (index * ENVELOPE_ENTRY_SIZE), &channel->envelope);
    }
}

static void StartEnvelope(Channel *channel) {
    EnvelopeState *state = &channel->envelopeState;

    if (channel->length == HELD_NOTE_LENGTH) {
        state->releaseClock = channel->noteStartClock + INT32_MAX;
    }
    else if (channel->cutoffTicks != 0) {
        state->releaseClock = channel->noteStartClock + (channel->cutoffTicks << CLOCK_TICK_SHIFT);
    }
    else {
        state->releaseClock =
            channel->noteEndClock - (channel->releaseLeadTicks << CLOCK_TICK_SHIFT);
    }

    state->level = channel->envelope.initial;
    state->countdown = channel->envelope.period;
    state->stage = ENVELOPE_ATTACK;
}

static void ReleaseEnvelope(Channel *channel, uint32_t clock) {
    EnvelopeState *state = &channel->envelopeState;

    if (state->stage < ENVELOPE_RELEASE) {
        state->stage = ENVELOPE_RELEASE;
        state->releaseClock = clock;
        state->countdown = 1;
        state->releaseFrom = state->level;
    }
}

// The envelope's steps since a clock, at its period
static int32_t EnvelopeStepsSince(const Channel *channel, uint32_t clock, int32_t lessTicks) {
    const int32_t ticks = (int32_t)((channel->clock - clock) >> CLOCK_TICK_SHIFT) - lessTicks;

    return (ticks * channel->envelope.stepScale) >> ENVELOPE_STEP_SHIFT;
}

static void AdvanceEnvelope(Channel *channel) {
    const EnvelopeShape *shape = &channel->envelope;
    EnvelopeState *state = &channel->envelopeState;
    int32_t steps;

    if (IsBefore(state->releaseClock, channel->clock) && state->stage < ENVELOPE_RELEASE) {
        state->stage = ENVELOPE_RELEASE;
        state->countdown = 1;
        state->releaseFrom = state->level;
    }

    if (--state->countdown != 0) {
        return;
    }

    state->countdown = shape->period;
    switch (state->stage) {
        case ENVELOPE_ATTACK:
            steps = EnvelopeStepsSince(channel, channel->noteStartClock, 0);
            if (steps < shape->attackTicks) {
                state->level = shape->initial + (int32_t)(shape->attackSlope * (float)steps);
            }
            else {
                state->stage = ENVELOPE_DECAY;
                state->level = shape->peak;
            }

            break;
        case ENVELOPE_DECAY:
            steps = EnvelopeStepsSince(channel, channel->noteStartClock, shape->attackTicks);
            if (steps < shape->decayTicks) {
                state->level = shape->peak + (int32_t)(shape->decaySlope * (float)steps);
            }
            else {
                state->stage = ENVELOPE_SUSTAIN;
                state->level = shape->sustain;
            }

            break;
        case ENVELOPE_RELEASE:
            steps = EnvelopeStepsSince(channel, state->releaseClock, 0);
            if (steps < shape->releaseTicks) {
                const float released =
                    shape->releaseShare * (float)steps * (float)state->releaseFrom;

                state->level = state->releaseFrom - (int32_t)released;
            }
            else {
                state->stage = ENVELOPE_DONE;
                state->level = 0;
            }

            break;
        default:
            break;
    }
}

// --- Level streams ---

// Reads levels until the stream catches up with the channel's clock, and
// gives the last one read, or -1 if the run in progress went on
static int AdvanceLevelStream(LevelStream *stream, uint32_t clock) {
    int level = -1;

    do {
        stream->clock += CLOCK_UNITS_PER_TICK;
        if (--stream->runLeft == 0) {
            const uint8_t value = ReadByte(&stream->cursor);

            level = value & ~LONG_VALUE_FLAG;
            if (value & LONG_VALUE_FLAG) {
                // A run of the level follows, less the two that make a run
                stream->runLeft = (uint16_t)(ReadCount(&stream->cursor) + 2);
            }
            else {
                stream->runLeft = 1;
            }
        }
    }
    while (IsBefore(stream->clock, clock));

    return level;
}

static void SetBend(Channel *channel, float bend) {
    channel->bend = bend;
}

static void AdvanceStreams(Channel *channel) {
    if (channel->volumes.cursor != NULL && IsBefore(channel->volumes.clock, channel->clock)) {
        const int level = AdvanceLevelStream(&channel->volumes, channel->clock);

        if (level >= 0) {
            channel->volume = (uint8_t)level;
        }
    }

    if (channel->bends.cursor != NULL && IsBefore(channel->bends.clock, channel->clock)) {
        const int level = AdvanceLevelStream(&channel->bends, channel->clock);

        if (level >= 0) {
            SetBend(channel, (float)level - BEND_CENTER);
        }
    }
}

// --- Commands ---

static void PushLoop(Channel *channel, uint8_t passes, const uint8_t *body) {
    LoopMark *mark;

    if (channel->loopDepth >= LOOP_DEPTH) {
        return;
    }

    mark = &channel->loops[channel->loopDepth++];
    mark->notes = body;
    mark->volumes = channel->volumes.cursor;
    mark->bends = channel->bends.cursor;
    mark->volumeRunLeft = channel->volumes.runLeft;
    mark->bendRunLeft = channel->bends.runLeft;
    mark->volume = channel->volume;
    mark->bend = channel->bend;
    mark->passesLeft = passes;
}

// Goes back to the innermost loop's start for another pass, or leaves it
static const uint8_t *EndLoopPass(Channel *channel, const uint8_t *after) {
    LoopMark *mark;

    if (channel->loopDepth == 0) {
        return after;
    }

    mark = &channel->loops[channel->loopDepth - 1];
    if (mark->passesLeft != ENDLESS_LOOP && --mark->passesLeft == 0) {
        channel->loopDepth--;
        return after;
    }

    channel->volumes.cursor = mark->volumes;
    channel->bends.cursor = mark->bends;
    channel->volumes.runLeft = mark->volumeRunLeft;
    channel->bends.runLeft = mark->bendRunLeft;
    channel->volume = mark->volume;
    SetBend(channel, mark->bend);
    return mark->notes;
}

// Jumps the track and its streams to offsets from their starts
static const uint8_t *JumpTrack(Channel *channel, const uint8_t *arguments) {
    const uint16_t notesOffset = ReadBig16(arguments);
    const uint16_t volumesOffset = ReadBig16(arguments + 2);
    const uint16_t bendsOffset = ReadBig16(arguments + 4);

    if (channel->volumes.start != NULL) {
        channel->volumes.cursor = channel->volumes.start + volumesOffset;
        channel->volumes.runLeft = 1;
    }

    if (channel->bends.start != NULL) {
        channel->bends.cursor = channel->bends.start + bendsOffset;
        channel->bends.runLeft = 1;
    }

    return channel->trackStart + notesOffset;
}

static void SetTempo(Channel *channel, uint8_t beatsPerMinute) {
    const int tempo =
        beatsPerMinute * CLOCK_UNITS_PER_TICK * TICKS_PER_SECOND_AT_120 / 120 / f_TicksPerSecond;
    const int scaled = (tempo * channel->tempoScale) >> SCALE_SHIFT;
    int i;

    if (IsEffectChannel(channel)) {
        channel->tempo = (uint16_t)tempo;
        return;
    }

    // A song's tempo is the same on all of its tracks
    for (i = 0; i < f_ChannelCount; i++) {
        Channel *other = &f_Channels[i];

        if (other->song == channel->song) {
            other->unscaledTempo = (uint16_t)tempo;
            other->tempo = (uint16_t)scaled;
        }
    }
}

static void SetVibrato(Channel *channel, const uint8_t *arguments, float direction) {
    channel->vibratoDelay = arguments[0];
    channel->vibratoPeriod = arguments[1];
    channel->vibratoDepth = direction * (float)arguments[2] / VIBRATO_DEPTH_UNITS;
}

static void SetDistortion(Channel *channel, uint8_t value) {
    const float distortion = (float)SignedByte(value) / DISTORTION_UNITS;

    channel->pitchOffset += distortion - channel->distortion;
    channel->distortion = distortion;
}

static uint32_t StartEffectOnChannel(
    Channel *channel,
    const EffectBank *bank,
    int number,
    SampleBank *samples,
    int volumeScale,
    int panScale,
    int priority
);
static Channel *FindEffectChannel(int priority);

// Starts another of the channel's bank's effects as part of its sound, a
// step above it in priority
static void StartNestedEffect(Channel *channel, int number) {
    const int priority = channel->priority + 1;
    Channel *target;
    uint32_t handle;
    int i;

    if (channel->effectBank == NULL || GetEffectData(channel->effectBank, number) == NULL) {
        return;
    }

    target = FindEffectChannel(priority);
    if (target == NULL) {
        return;
    }

    handle = StartEffectOnChannel(
        target,
        channel->effectBank,
        number,
        channel->samples,
        channel->volumeScale,
        channel->panScale,
        priority
    );
    for (i = 0; i < f_ChannelCount; i++) {
        if (f_Channels[i].handle == handle) {
            f_Channels[i].handle = channel->handle;
        }
    }
}

static void ReachMarker(Channel *channel, int marker) {
    if (channel->isMasterTrack && f_OnMarker != NULL) {
        f_OnMarker(channel->handle, marker);
    }
}

// Carries out the command at cursor, and gives where the track goes on; NULL
// once it ends
static const uint8_t *RunCommand(Channel *channel, const uint8_t *cursor) {
    const uint8_t command = ReadByte(&cursor);
    int value;

    switch (command) {
        case COMMAND_END:
            return NULL;
        case COMMAND_WAVE:
            channel->wave = ReadCount(&cursor);
            break;
        case COMMAND_GLIDE:
            channel->glideTicks = ReadByte(&cursor);
            if (channel->glideTicks != 0) {
                channel->glideNote = channel->note;
            }

            break;
        case COMMAND_GLIDE_OFF:
            channel->glideTicks = 0;
            break;
        case COMMAND_ENVELOPE:
            ReadEnvelopeShape(cursor, &channel->envelope);
            cursor += INLINE_ENVELOPE_SIZE;
            break;
        case COMMAND_TEMPO:
            SetTempo(channel, ReadByte(&cursor));
            break;
        case COMMAND_CUTOFF:
            channel->cutoffTicks = ReadBig16(cursor);
            channel->releaseLeadTicks = 0;
            cursor += 2;
            break;
        case COMMAND_RELEASE_LEAD:
            channel->releaseLeadTicks = ReadByte(&cursor);
            channel->cutoffTicks = 0;
            break;
        case COMMAND_VIBRATO_UP:
        case COMMAND_VIBRATO_DOWN:
            SetVibrato(channel, cursor, (command == COMMAND_VIBRATO_UP) ? 1.0f : -1.0f);
            cursor += 3;
            break;
        case COMMAND_VIBRATO_OFF:
            channel->vibratoPeriod = 0;
            break;
        case COMMAND_FIXED_LENGTH:
            channel->fixedLength = ReadCount(&cursor);
            break;
        case COMMAND_NEXT_LENGTH_INLINE:
            channel->hasInlineLength = true;
            break;
        case COMMAND_TRANSPOSE:
            channel->transposition = ReadByte(&cursor);
            break;
        case COMMAND_LOCK_TRANSPOSITION:
            channel->isTranspositionLocked = true;
            break;
        case COMMAND_DISTORT:
            SetDistortion(channel, ReadByte(&cursor));
            break;
        case COMMAND_SONG_ENVELOPE:
            UseSongEnvelope(channel, ReadCount(&cursor));
            break;
        case COMMAND_HOLD_ENVELOPE:
        case COMMAND_RETRIGGER_ENVELOPE:
            channel->isEnvelopeHeld = command == COMMAND_HOLD_ENVELOPE;
            break;
        case COMMAND_HOLD_SAMPLE:
        case COMMAND_RETRIGGER_SAMPLE:
            channel->isSampleHeld = command == COMMAND_HOLD_SAMPLE;
            break;
        case COMMAND_LOOP_START:
            value = ReadByte(&cursor);
            PushLoop(channel, (uint8_t)value, cursor);
            break;
        case COMMAND_LOOP_END:
            return EndLoopPass(channel, cursor);
        case COMMAND_WOBBLE:
            channel->wobbleAmount = cursor[0];
            channel->wobbleOnTicks = cursor[1];
            channel->wobbleOffTicks = cursor[2];
            cursor += 3;
            break;
        case COMMAND_WOBBLE_OFF:
            channel->wobbleOnTicks = 0;
            break;
        case COMMAND_INLINE_VELOCITY:
        case COMMAND_DEFAULT_VELOCITY_ONLY:
            channel->hasInlineVelocity = command == COMMAND_INLINE_VELOCITY;
            break;
        case COMMAND_DEFAULT_VELOCITY:
            channel->defaultVelocity = ReadByte(&cursor);
            channel->hasInlineVelocity = false;
            break;
        case COMMAND_PAN:
            channel->pan = ReadByte(&cursor) / 2;
            break;
        case COMMAND_STEREO:
            cursor += 2;
            break;
        case COMMAND_DRUMS:
            value = ReadCount(&cursor);
            if (channel->drumTable != NULL) {
                channel->drums = channel->drumTable + (value * DRUM_ENTRY_SIZE);
            }

            break;
        case COMMAND_DRUMS_OFF:
            channel->drums = NULL;
            break;
        case COMMAND_PRINT:
            cursor++;
            break;
        case COMMAND_GOTO:
            return JumpTrack(channel, cursor);
        case COMMAND_REVERB:
            channel->reverb = ReadByte(&cursor);
            break;
        case COMMAND_RANDOM_TRANSPOSE:
            channel->transposition = (uint8_t)(RandomBelow(cursor[0]) + cursor[1]);
            cursor += 2;
            break;
        case COMMAND_RANDOM_VOLUME:
            channel->volume = (uint8_t)(RandomBelow(cursor[0]) + cursor[1]);
            cursor += 2;
            break;
        case COMMAND_RANDOM_PAN:
            channel->pan = (uint8_t)(RandomBelow(cursor[0]) + cursor[1]);
            cursor += 2;
            break;
        case COMMAND_VOLUME:
            channel->volume = ReadByte(&cursor);
            break;
        case COMMAND_START_EFFECT:
            StartNestedEffect(channel, ReadCount(&cursor));
            break;
        case COMMAND_BEND_RANGE:
            channel->bendRange = (float)ReadByte(&cursor) / BEND_RANGE_UNITS;
            break;
        case COMMAND_SWEEP:
            channel->sweepSpeed = ReadByte(&cursor);
            break;
        case COMMAND_CHANGE_EFFECT:
            value = ReadByte(&cursor);
            if (f_OnEffectChange != NULL) {
                f_OnEffectChange(value);
            }

            break;
        case COMMAND_MARKER:
            value = ReadByte(&cursor);
            // The rest that follows matters only when starting at a marker
            (void)ReadCount(&cursor);
            ReachMarker(channel, value);
            break;
        case COMMAND_FIXED_LENGTH_OFF:
            channel->fixedLength = 0;
            break;
        default:
            // Not a command of the Music Tools': the track is corrupt
            return NULL;
    }

    return cursor;
}

// --- Notes ---

// Starts the sample waiting for the voice, cutting off the one it played
static void StartPendingSample(Channel *channel, int voice) {
    if (voice >= 0) {
        StartSynthVoice(voice, channel->pendingSample);
    }

    channel->isSounding = true;
    channel->pendingSample = NULL;
}

// The bank's sample for the channel's current wave, or NULL
static Sample *LookUpSample(const Channel *channel, int wave) {
    const uint16_t index = LookUpWave(channel->waves, wave);

    if (channel->samples == NULL || index == MISSING_WAVE || index >= channel->samples->count) {
        return NULL;
    }

    return &channel->samples->samples[index];
}

static void SetReverbSend(Channel *channel, int voice) {
    const int send = channel->reverbBase +
        (((MAX_REVERB + 1 - channel->reverbBase) * channel->reverb) >> SCALE_SHIFT);

    if (voice >= 0) {
        SetSynthVoiceSend(voice, (uint8_t)SDL_min(send, MAX_REVERB));
    }
}

// Sets up a note that sounds: its drum, envelope, sample and pitch
static void StrikeNote(Channel *channel, int voice, int note) {
    Sample *sample;

    if (channel->drums != NULL) {
        const uint8_t *drum = channel->drums + (note * DRUM_ENTRY_SIZE);

        channel->wave = ReadBig16(drum);
        channel->pan = drum[4] / 2;
        UseSongEnvelope(channel, ReadBig16(drum + 2));
        note = drum[5];
    }

    if (!channel->isEnvelopeHeld) {
        StartEnvelope(channel);
    }

    if (channel->sweepSpeed != 0) {
        channel->sweepClock = channel->noteStartClock;
        channel->sweepCarry = 0;
        channel->isSweepFalling = (channel->pan & FALLING_SWEEP_PAN) != 0;
    }

    sample = LookUpSample(channel, channel->wave);
    if (!channel->isSampleHeld) {
        channel->pendingSample = sample;
        if (channel->isSounding && channel->lastVolume != 0) {
            // Fade out the note still sounding first; the sample starts on
            // the next frame
            channel->lastVolume = 0;
            if (voice >= 0) {
                SetSynthVoiceVolume(voice, 0);
            }
        }
        else {
            StartPendingSample(channel, voice);
        }
    }

    channel->note = (float)note + ((sample != NULL) ? sample->tuning : 0.0f);
    if (!channel->isTranspositionLocked) {
        channel->note += (float)SignedByte(channel->transposition);
    }

    SetReverbSend(channel, voice);
}

static bool IsRest(const Channel *channel, int note) {
    if (note == REST_NOTE) {
        return true;
    }

    // A song's wave that the bank lacks plays as rests
    return channel->song != NULL &&
        channel->drums == NULL &&
        LookUpWave(channel->waves, channel->wave) == MISSING_WAVE;
}

// Reads the note's velocity and length, and times it from the last note's end
static void ReadNoteTiming(Channel *channel) {
    if (channel->hasInlineVelocity) {
        const uint8_t velocity = ReadByte(&channel->cursor);

        channel->velocity = velocity & ~LONG_VALUE_FLAG;
        if (velocity & LONG_VALUE_FLAG) {
            // The last inline velocity, which becomes the default
            channel->hasInlineVelocity = false;
            channel->defaultVelocity = channel->velocity;
        }
    }
    else {
        channel->velocity = channel->defaultVelocity;
    }

    if (channel->fixedLength != 0 && !channel->hasInlineLength) {
        channel->length = channel->fixedLength;
    }
    else {
        channel->hasInlineLength = false;
        channel->length = ReadCount(&channel->cursor);
    }

    channel->noteStartClock = channel->noteEndClock;
    channel->noteEndClock += channel->length << CLOCK_TICK_SHIFT;
    channel->ticksIntoNote = 0;
    channel->wobbleCountdown = channel->wobbleOffTicks;
    channel->wobbleValue = 0;
}

// Runs the track's commands up to its next note and starts it, or ends the
// channel at the track's end
static void ReadNextNote(Channel *channel, int voice) {
    const uint8_t *cursor = channel->cursor;
    int commands = 0;
    int note;

    while (cursor != NULL && *cursor >= COMMAND_END && commands++ < COMMAND_LIMIT) {
        cursor = RunCommand(channel, cursor);
    }

    if (commands > COMMAND_LIMIT) {
        cursor = NULL;
    }

    channel->cursor = cursor;
    if (cursor == NULL) {
        EndChannel(channel, voice);
        return;
    }

    channel->glideFrom = channel->glideNote;
    note = ReadByte(&channel->cursor);
    ReadNoteTiming(channel);
    if (!IsRest(channel, note)) {
        StrikeNote(channel, voice, note);
    }
    else {
        // A rest lets the last note's release play out
        ReleaseEnvelope(channel, channel->clock);
    }
}

// --- Each frame ---

static float GlideNote(Channel *channel) {
    float note = channel->note;

    if (channel->glideTicks != 0) {
        if (channel->ticksIntoNote <= channel->glideTicks) {
            const float step = (note - channel->glideFrom) / (float)channel->glideTicks;

            note = channel->glideFrom + (step * (float)channel->ticksIntoNote);
        }

        channel->glideNote = note;
    }

    return note;
}

static float VibratoOffset(const Channel *channel) {
    const int32_t ticks = channel->ticksIntoNote - channel->vibratoDelay;
    const float radiansPerTick = 2.0f * SDL_PI_F / (float)channel->vibratoPeriod;

    if (channel->vibratoPeriod == 0 || ticks <= 0) {
        return 0.0f;
    }

    return SDL_sinf((float)ticks * radiansPerTick) * channel->vibratoDepth;
}

static float WobbleOffset(Channel *channel) {
    if (channel->wobbleOnTicks == 0) {
        return 0.0f;
    }

    if (--channel->wobbleCountdown == 0) {
        if (channel->wobbleValue == 0) {
            channel->wobbleValue = channel->wobbleAmount;
            channel->wobbleCountdown = channel->wobbleOnTicks;
        }
        else {
            channel->wobbleValue = 0;
            channel->wobbleCountdown = channel->wobbleOffTicks;
        }
    }

    return (float)channel->wobbleValue;
}

// Moves the pan back and forth between the sides at the sweep's speed
static void AdvanceSweep(Channel *channel) {
    do {
        unsigned carry = channel->sweepCarry + channel->sweepSpeed;
        unsigned step;

        channel->sweepClock += CLOCK_UNITS_PER_TICK;
        channel->sweepCarry = (uint8_t)(carry & SWEEP_CARRY_MASK);
        step = carry >> SWEEP_CARRY_BITS;
        if (step == 0) {
            continue;
        }

        if (!channel->isSweepFalling) {
            channel->pan = (uint8_t)(channel->pan + step);
            if (channel->pan > MAX_SWEPT_PAN) {
                channel->pan = MAX_SWEPT_PAN;
                channel->isSweepFalling = true;
            }
        }
        else {
            channel->pan = (uint8_t)(channel->pan - step);
            if (channel->pan > MAX_SWEPT_PAN || channel->pan == 0) {
                channel->pan = 0;
                channel->isSweepFalling = false;
            }
        }
    }
    while (IsBefore(channel->sweepClock, channel->clock));
}

static void UpdatePitch(Channel *channel, int voice, float offset) {
    const float semitones = GlideNote(channel) + offset + (channel->bend * channel->bendRange);
    float rate = SemitonesToRate(semitones);

    if (rate > MAX_RATE) {
        rate = MAX_RATE;
        channel->velocity = 0;
    }

    if (voice >= 0) {
        SetSynthVoicePitch(voice, rate);
    }
}

static void UpdateLevels(Channel *channel, int voice) {
    const int32_t master = IsEffectChannel(channel) ? f_EffectMasterVolume : f_SongMasterVolume;
    uint32_t volume =
        ((uint32_t)channel->volume * (uint32_t)SDL_max(channel->envelopeState.level, 0) *
            channel->velocity * (uint32_t)channel->volumeScale) >>
        VOLUME_PRODUCT_SHIFT;
    uint8_t pan = DEFAULT_PAN;

    volume = SDL_min(volume, (uint32_t)SYNTH_MAX_VOLUME);
    volume = (volume * (uint32_t)master) >> MASTER_VOLUME_SHIFT;
    if (channel->fadeLeft != NOT_FADING) {
        volume = (volume * (uint32_t)channel->fadeLeft) / (uint32_t)channel->fadeLength;
    }

    if (f_IsStereo) {
        pan = (uint8_t)(((channel->pan * channel->panScale) >> SCALE_SHIFT) & SYNTH_MAX_PAN);
    }

    channel->lastVolume = (uint16_t)volume;
    if (voice >= 0) {
        SetSynthVoiceVolume(voice, (uint16_t)volume);
        SetSynthVoicePan(voice, pan);
    }
}

// Plays the sounding note's envelope, modulation and levels for a frame
static void PlaySoundingNote(Channel *channel, int voice) {
    float offset;

    if (channel->envelopeState.stage != ENVELOPE_IDLE) {
        AdvanceEnvelope(channel);
    }

    if (channel->sweepSpeed != 0 && IsBefore(channel->sweepClock, channel->clock)) {
        AdvanceSweep(channel);
    }

    offset = channel->pitchOffset + VibratoOffset(channel) + WobbleOffset(channel);
    if (channel->pendingSample == NULL) {
        UpdatePitch(channel, voice, offset);
        UpdateLevels(channel, voice);
    }
}

static void TickChannel(Channel *channel, int voice) {
    if (channel->pendingSample != NULL) {
        StartPendingSample(channel, voice);
    }

    channel->clock += channel->tempo;
    if (channel->length != HELD_NOTE_LENGTH) {
        while (channel->cursor != NULL && IsBefore(channel->noteEndClock, channel->clock)) {
            ReadNextNote(channel, voice);
        }

        if (channel->cursor == NULL) {
            return;
        }
    }

    AdvanceStreams(channel);
    if (channel->fadeLeft != NOT_FADING && --channel->fadeLeft == NOT_FADING) {
        EndChannel(channel, voice);
    }

    if (channel->isSounding) {
        PlaySoundingNote(channel, voice);
    }

    channel->ticksIntoNote =
        (int32_t)((channel->clock - channel->noteStartClock) >> CLOCK_TICK_SHIFT);
}

void TickSequencer(void) {
    int i;

    for (i = 0; i < f_ChannelCount; i++) {
        if (f_Channels[i].cursor != NULL) {
            TickChannel(&f_Channels[i], VoiceOf(i));
        }
    }
}

// --- Starting sounds ---

void InitSequencer(int voiceCount, int ticksPerSecond) {
    int i;

    SDL_assert(voiceCount >= 0 && voiceCount <= SYNTH_MAX_VOICES);
    SDL_assert(ticksPerSecond > 0);
    f_TicksPerSecond = ticksPerSecond;
    f_ChannelCount = voiceCount + SEQUENCER_MASTER_TRACK_CHANNELS;
    f_NextHandle = 1;
    f_SongMasterVolume = MAX_MASTER_VOLUME;
    f_EffectMasterVolume = MAX_MASTER_VOLUME;
    f_IsStereo = true;
    f_OnMarker = NULL;
    f_OnEffectChange = NULL;
    SDL_zeroa(f_Channels);
    for (i = 0; i < f_ChannelCount; i++) {
        ResetChannel(&f_Channels[i], NULL);
    }
}

static uint32_t TakeHandle(void) {
    const uint32_t handle = f_NextHandle++;

    if (f_NextHandle == NO_SOUND) {
        f_NextHandle = 1;
    }

    return handle;
}

static bool IsVoiceChannel(const Channel *channel) {
    return channel >= &f_Channels[SEQUENCER_MASTER_TRACK_CHANNELS];
}

// A channel for one of a song's tracks, or its master track: an idle one,
// else the lowest effect, else one of another song's, else the one that
// plays this track already, else one picked by the track's number
static Channel *FindSongChannel(const SongLayout *layout, int track, const uint8_t *notes) {
    const int voiceCount = f_ChannelCount - SEQUENCER_MASTER_TRACK_CHANNELS;
    Channel *lowestEffect = NULL;
    Channel *channel;
    int i;

    if (track < 0) {
        for (i = 0; i < SEQUENCER_MASTER_TRACK_CHANNELS; i++) {
            if (f_Channels[i].cursor == NULL) {
                return &f_Channels[i];
            }
        }
    }

    if (voiceCount == 0) {
        return NULL;
    }

    for (i = SEQUENCER_MASTER_TRACK_CHANNELS; i < f_ChannelCount; i++) {
        channel = &f_Channels[i];
        if (channel->cursor == NULL) {
            return channel;
        }

        if (IsEffectChannel(channel) &&
            (lowestEffect == NULL || channel->priority <= lowestEffect->priority)) {
            lowestEffect = channel;
        }
    }

    if (lowestEffect != NULL) {
        return lowestEffect;
    }

    for (i = SEQUENCER_MASTER_TRACK_CHANNELS; i < f_ChannelCount; i++) {
        if (f_Channels[i].song != layout->start) {
            return &f_Channels[i];
        }
    }

    for (i = SEQUENCER_MASTER_TRACK_CHANNELS; i < f_ChannelCount; i++) {
        if (f_Channels[i].trackStart == notes) {
            return &f_Channels[i];
        }
    }

    return &f_Channels[SEQUENCER_MASTER_TRACK_CHANNELS + (SDL_max(track, 0) % voiceCount)];
}

static void StartSongTrack(
    Channel *channel,
    const SongLayout *layout,
    SampleBank *samples,
    uint32_t handle,
    SongTrack track
) {
    ResetChannel(channel, samples);
    channel->hasInlineVelocity = true;
    channel->song = layout->start;
    channel->envelopes = layout->envelopes;
    channel->drumTable = layout->drums;
    channel->waves = layout->waves;
    channel->handle = handle;
    channel->volumes.start = track.volumes;
    channel->volumes.cursor = track.volumes;
    channel->bends.start = track.bends;
    channel->bends.cursor = track.bends;
    channel->trackStart = track.notes;
    channel->cursor = track.notes;
}

uint32_t StartSong(const SongLayout *layout, SampleBank *samples) {
    const uint32_t handle = TakeHandle();
    const SongTrack master = {layout->masterTrack, NULL, NULL};
    Channel *channel;
    int track;

    SDL_assert(layout != NULL);
    if (layout->masterTrack != NULL) {
        channel = FindSongChannel(layout, -1, layout->masterTrack);
        if (channel != NULL) {
            StartSongTrack(channel, layout, samples, handle, master);
            channel->isMasterTrack = true;
        }
    }

    for (track = 0; track < layout->trackCount; track++) {
        const SongTrack data = GetSongTrack(layout, track);

        if (data.notes == NULL) {
            continue;
        }

        channel = FindSongChannel(layout, track, data.notes);
        if (channel != NULL) {
            StartSongTrack(channel, layout, samples, handle, data);
        }
    }

    return handle;
}

static uint32_t StartEffectOnChannel(
    Channel *channel,
    const EffectBank *bank,
    int number,
    SampleBank *samples,
    int volumeScale,
    int panScale,
    int priority
) {
    const uint8_t *data = GetEffectData(bank, number);

    ResetChannel(channel, samples);
    channel->effectBank = bank;
    channel->effectNumber = number;
    channel->waves = bank->waves;
    channel->volumeScale = volumeScale;
    channel->panScale = panScale;
    channel->priority = priority;
    channel->handle = TakeHandle();
    channel->trackStart = data;
    channel->cursor = data;
    return channel->handle;
}

// An idle voice channel, or else the effect of the lowest priority below
// priority, which the new effect takes over; NULL if there is neither
static Channel *FindEffectChannel(int priority) {
    Channel *lowest = NULL;
    int i;

    for (i = SEQUENCER_MASTER_TRACK_CHANNELS; i < f_ChannelCount; i++) {
        Channel *channel = &f_Channels[i];

        if (channel->cursor == NULL) {
            return channel;
        }

        if (IsEffectChannel(channel) &&
            channel->priority < priority &&
            (lowest == NULL || channel->priority < lowest->priority)) {
            lowest = channel;
        }
    }

    return lowest;
}

static Channel *FindPlayingEffect(const EffectBank *bank, int number) {
    int i;

    for (i = SEQUENCER_MASTER_TRACK_CHANNELS; i < f_ChannelCount; i++) {
        if (f_Channels[i].effectBank == bank && f_Channels[i].effectNumber == number) {
            return &f_Channels[i];
        }
    }

    return NULL;
}

uint32_t StartEffect(
    const EffectBank *bank,
    int number,
    SampleBank *samples,
    int volumeScale,
    int panScale,
    int priority,
    bool restarts
) {
    Channel *channel = NULL;

    SDL_assert(bank != NULL);
    if (GetEffectData(bank, number) == NULL) {
        return NO_SOUND;
    }

    if (priority == EFFECT_BANK_PRIORITY) {
        priority = GetEffectPriority(bank, number);
    }

    if (restarts) {
        channel = FindPlayingEffect(bank, number);
    }

    if (channel == NULL) {
        channel = FindEffectChannel(priority);
    }

    if (channel == NULL || !IsVoiceChannel(channel)) {
        return NO_SOUND;
    }

    return StartEffectOnChannel(channel, bank, number, samples, volumeScale, panScale, priority);
}

// --- Handles ---

int StopSoundKinds(uint32_t kinds, int frames) {
    int count = 0;
    int i;

    for (i = 0; i < f_ChannelCount; i++) {
        Channel *channel = &f_Channels[i];

        if (channel->cursor != NULL &&
            channel->fadeLeft == NOT_FADING &&
            IsOfKinds(channel, kinds)) {
            StartFade(channel, frames);
            count++;
        }
    }

    return count;
}

int CountSoundKinds(uint32_t kinds) {
    int count = 0;
    int i;

    for (i = 0; i < f_ChannelCount; i++) {
        if (f_Channels[i].cursor != NULL && IsOfKinds(&f_Channels[i], kinds)) {
            count++;
        }
    }

    return count;
}

int StopSound(uint32_t handle, int frames) {
    int count = 0;
    int i;

    if (handle == NO_SOUND) {
        return 0;
    }

    for (i = 0; i < f_ChannelCount; i++) {
        if (f_Channels[i].handle == handle && f_Channels[i].fadeLeft == NOT_FADING) {
            StartFade(&f_Channels[i], frames);
            count++;
        }
    }

    return count;
}

// The channels of the handle, one by one: gives the next after *index, or
// NULL when there are no more
static Channel *NextChannelOf(uint32_t handle, int *index) {
    if (handle == NO_SOUND) {
        return NULL;
    }

    while (++*index < f_ChannelCount) {
        if (f_Channels[*index].handle == handle) {
            return &f_Channels[*index];
        }
    }

    return NULL;
}

int CountSoundChannels(uint32_t handle) {
    int index = -1;
    int count = 0;

    while (NextChannelOf(handle, &index) != NULL) {
        count++;
    }

    return count;
}

int SetSoundVolumeScale(uint32_t handle, int scale) {
    int index = -1;
    int count = 0;
    Channel *channel;

    while ((channel = NextChannelOf(handle, &index)) != NULL) {
        channel->volumeScale = scale;
        count++;
    }

    return count;
}

int SetSoundPanScale(uint32_t handle, int scale) {
    int index = -1;
    int count = 0;
    Channel *channel;

    while ((channel = NextChannelOf(handle, &index)) != NULL) {
        channel->panScale = scale;
        count++;
    }

    return count;
}

int SetSoundPitchOffset(uint32_t handle, float semitones) {
    int index = -1;
    int count = 0;
    Channel *channel;

    while ((channel = NextChannelOf(handle, &index)) != NULL) {
        channel->pitchOffset = semitones + channel->distortion;
        count++;
    }

    return count;
}

int SetSoundTempoScale(uint32_t handle, int scale) {
    int index = -1;
    int count = 0;
    Channel *channel;

    scale = SDL_clamp(scale, 1, MAX_TEMPO_SCALE);
    while ((channel = NextChannelOf(handle, &index)) != NULL) {
        channel->tempoScale = (uint16_t)scale;
        channel->tempo = (uint16_t)((channel->unscaledTempo * scale) >> SCALE_SHIFT);
        count++;
    }

    return count;
}

int SetSoundReverbBase(uint32_t handle, int level) {
    int index = -1;
    int count = 0;
    Channel *channel;

    level = SDL_clamp(level, 0, MAX_REVERB);
    while ((channel = NextChannelOf(handle, &index)) != NULL) {
        channel->reverbBase = (uint8_t)level;
        count++;
    }

    return count;
}

void SetMasterVolume(uint32_t kinds, int volume) {
    if (kinds & SOUND_KIND_EFFECTS) {
        f_EffectMasterVolume = volume;
    }

    if (kinds & SOUND_KIND_SONGS) {
        f_SongMasterVolume = volume;
    }
}

void SetStereoOutput(bool isStereo) {
    f_IsStereo = isStereo;
}

void SetSequencerHandlers(MarkerHandler onMarker, EffectChangeHandler onEffectChange) {
    f_OnMarker = onMarker;
    f_OnEffectChange = onEffectChange;
}
