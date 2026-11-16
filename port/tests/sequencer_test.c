/*
 * Tests of the sequencer: effects and songs written in the Music Tools'
 * format, played on the real synthesizer, and heard through its output.
 *
 * The one sample is a steady level that loops forever, so what a voice
 * gives at the end of a block is its volume, after the block's glide.
 */

#include <PR/libaudio.h>
#include <SDL3/SDL.h>
#include <string.h>

#include "sequencer.h"
#include "synth.h"
#include "test_support.h"

#define SAMPLE_RATE 32000
#define TICKS_PER_SECOND 60
#define STEREO 2
#define BLOCK_FRAMES 64
#define SAMPLE_LEVEL 16384
#define BANK_SIZE 512
#define EFFECT_DATA_START 128
#define FULL_VOLUME_SCALE 0x80

static int16_t f_Pcm[BLOCK_FRAMES];
static Sample f_Sample;
static SampleBank f_Samples = {NULL, 1, &f_Sample};
static uint8_t f_BankBytes[BANK_SIZE];
static EffectBank f_Bank;
static float f_Output[BLOCK_FRAMES * STEREO];

static void WriteBig32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static void MakeSteadySample(void) {
    int i;

    for (i = 0; i < BLOCK_FRAMES; i++) {
        f_Pcm[i] = SAMPLE_LEVEL;
    }

    memset(&f_Sample, 0, sizeof(f_Sample));
    f_Sample.data = (const uint8_t *)f_Pcm;
    f_Sample.encoding = SAMPLE_PCM16;
    f_Sample.pcm = f_Pcm;
    f_Sample.pcmLength = BLOCK_FRAMES;
    f_Sample.loops = true;
    f_Sample.loopEnd = BLOCK_FRAMES;
    f_Sample.loopCount = AL_LOOP_FOREVER;
}

// An effect bank of the given effects, each at its priority, whose wave 0 is
// the one sample
static void MakeEffectBank(
    const uint8_t *const *effects,
    const int *sizes,
    const int *priorities,
    int count
) {
    uint32_t offset = EFFECT_DATA_START;
    int i;

    memset(f_BankBytes, 0, sizeof(f_BankBytes));
    WriteBig32(f_BankBytes + 0, (uint32_t)count);
    WriteBig32(f_BankBytes + 20, 120); // The wave table, mapping wave 0 to sample 0
    for (i = 0; i < count; i++) {
        WriteBig32(f_BankBytes + 24 + (i * 8), offset);
        WriteBig32(f_BankBytes + 28 + (i * 8), (uint32_t)priorities[i]);
        memcpy(f_BankBytes + offset, effects[i], (size_t)sizes[i]);
        offset += (uint32_t)sizes[i];
    }

    EXPECT(offset <= BANK_SIZE);
    EXPECT(ParseEffectBank(f_BankBytes, &f_Bank));
}

static void MakeOneEffect(const uint8_t *effect, int size) {
    const int priority = 10;

    MakeEffectBank(&effect, &size, &priority, 1);
}

static void Reset(int voiceCount) {
    InitSynth(SAMPLE_RATE, voiceCount);
    InitSequencer(voiceCount, TICKS_PER_SECOND);
    MakeSteadySample();
}

// Plays a frame, and gives the level heard at its end, as a share of the
// sample's
static double PlayFrame(void) {
    const int last = (BLOCK_FRAMES - 1) * STEREO;

    TickSequencer();
    RenderSynth(f_Output, BLOCK_FRAMES);
    return SDL_sqrt((f_Output[last] * f_Output[last]) + (f_Output[last + 1] * f_Output[last + 1])) /
        (SAMPLE_LEVEL / 32768.0);
}

// Plays frames until the handle has no channels left, and gives how many
// it took, or -1 if it outlasted limit
static int FramesUntilDone(uint32_t handle, int limit) {
    int frame;

    for (frame = 1; frame <= limit; frame++) {
        PlayFrame();
        if (CountSoundChannels(handle) == 0) {
            return frame;
        }
    }

    return -1;
}

static uint32_t StartTheEffect(int number, int priority) {
    return StartEffect(
        &f_Bank,
        number,
        &f_Samples,
        FULL_VOLUME_SCALE,
        FULL_VOLUME_SCALE,
        priority,
        false
    );
}

// A note of four ticks at full levels plays at the product of its track's
// volume, its velocity and its envelope, squared by the synthesizer, and the
// effect ends after it
static void TestEffectPlaysNote(void) {
    static const uint8_t effect[] = {0, 4, 0x80};
    const double volume = (127.0 * 127.0 * 127.0 * FULL_VOLUME_SCALE / 8192.0) / 32767.0;
    const double full = volume * volume;
    uint32_t handle;

    Reset(2);
    MakeOneEffect(effect, sizeof(effect));
    handle = StartTheEffect(0, EFFECT_BANK_PRIORITY);
    EXPECT(handle != NO_SOUND);
    EXPECT(CountSoundChannels(handle) == 1);
    EXPECT(CountSoundKinds(SOUND_KIND_EFFECTS) == 1);
    EXPECT(CountSoundKinds(SOUND_KIND_SONGS) == 0);
    EXPECT_NEAR(PlayFrame(), full, 1e-3);
    EXPECT(IsSynthVoicePlaying(0));

    // At 96 ticks a second and 60 frames, four ticks end in the third frame
    EXPECT(FramesUntilDone(handle, 10) == 2);
    EXPECT(!IsSynthVoicePlaying(0));
}

// Stopping fades a sound out over the frames asked, then ends it
static void TestStopFades(void) {
    static const uint8_t effect[] = {0, 0xFF, 0xFF, 0x80};
    double last;
    uint32_t handle;
    int frame;

    Reset(2);
    MakeOneEffect(effect, sizeof(effect));
    handle = StartTheEffect(0, EFFECT_BANK_PRIORITY);
    last = PlayFrame();
    EXPECT(last > 0.9);
    EXPECT(StopSequencedSound(handle, 4) == 1);
    for (frame = 1; frame <= 4; frame++) {
        const double level = PlayFrame();

        const double share = (4 - frame) / 4.0;

        EXPECT_NEAR(level, last * share * share, 1e-3);
    }

    EXPECT(CountSoundChannels(handle) == 1);
    PlayFrame();
    EXPECT(CountSoundChannels(handle) == 0);
}

// A loop of three passes plays its notes three times
static void TestLoopRepeats(void) {
    static const uint8_t threeTimes[] = {0x95, 3, 0, 4, 0x96, 0x80};
    static const uint8_t once[] = {0x95, 1, 0, 4, 0x96, 0x80};

    Reset(2);
    MakeOneEffect(threeTimes, sizeof(threeTimes));
    EXPECT(FramesUntilDone(StartTheEffect(0, EFFECT_BANK_PRIORITY), 40) == 8);
    Reset(2);
    MakeOneEffect(once, sizeof(once));
    EXPECT(FramesUntilDone(StartTheEffect(0, EFFECT_BANK_PRIORITY), 40) == 3);
}

// An effect takes the channel of a lower one when none is idle, but not of
// an equal or higher one
static void TestPriorityTakesChannels(void) {
    static const uint8_t effect[] = {0, 0xFF, 0xFF, 0x80};
    uint32_t middle;
    uint32_t high;

    Reset(1);
    MakeOneEffect(effect, sizeof(effect));
    middle = StartTheEffect(0, 10);
    EXPECT(middle != NO_SOUND);
    EXPECT(StartTheEffect(0, 5) == NO_SOUND);
    EXPECT(StartTheEffect(0, 10) == NO_SOUND);
    EXPECT(CountSoundChannels(middle) == 1);
    high = StartTheEffect(0, 20);
    EXPECT(high != NO_SOUND && high != middle);
    EXPECT(CountSoundChannels(middle) == 0);
    EXPECT(CountSoundChannels(high) == 1);
}

// The handle's volume scale and the master volume scale the volume, whose
// square is the level
static void TestVolumeScales(void) {
    static const uint8_t effect[] = {0, 0xFF, 0xFF, 0x80};
    uint32_t handle;
    double full;

    Reset(2);
    MakeOneEffect(effect, sizeof(effect));
    handle = StartTheEffect(0, EFFECT_BANK_PRIORITY);
    full = PlayFrame();
    EXPECT(SetSoundVolumeScale(handle, FULL_VOLUME_SCALE / 2) == 1);
    EXPECT_NEAR(PlayFrame(), full / 4, 1e-3);
    SetMasterVolume(SOUND_KIND_EFFECTS, 0x4000);
    EXPECT_NEAR(PlayFrame(), full / 16, 1e-3);
    SetMasterVolume(SOUND_KIND_SONGS, 0);
    EXPECT_NEAR(PlayFrame(), full / 16, 1e-3);
}

// The pitch rises an octave with twelve semitones of transposition, and
// past twice the sample's rate the note is silenced
static void TestTransposition(void) {
    static const uint8_t octave[] = {0x8D, 12, 0, 0xFF, 0xFF, 0x80};
    static const uint8_t tooHigh[] = {0x8D, 13, 0, 0xFF, 0xFF, 0x80};

    EXPECT_NEAR(SemitonesToRate(12.0f), 2.0, 1e-3);
    EXPECT_NEAR(SemitonesToRate(-12.0f), 0.5, 1e-3);
    EXPECT_NEAR(SemitonesToRate(0.0f), 1.0, 1e-6);
    Reset(2);
    MakeOneEffect(octave, sizeof(octave));
    StartTheEffect(0, EFFECT_BANK_PRIORITY);
    EXPECT(PlayFrame() > 0.9);
    Reset(2);
    MakeOneEffect(tooHigh, sizeof(tooHigh));
    StartTheEffect(0, EFFECT_BANK_PRIORITY);
    PlayFrame();
    EXPECT_NEAR(PlayFrame(), 0.0, 1e-3);
}

// An effect can start another of its bank's, which shares its handle
static void TestNestedEffectSharesHandle(void) {
    static const uint8_t outer[] = {0xA7, 1, 0, 0xFF, 0xFF, 0x80};
    static const uint8_t inner[] = {0, 0xFF, 0xFF, 0x80};
    const uint8_t *const effects[] = {outer, inner};
    const int sizes[] = {sizeof(outer), sizeof(inner)};
    const int priorities[] = {10, 10};
    uint32_t handle;

    Reset(2);
    MakeEffectBank(effects, sizes, priorities, 2);
    handle = StartTheEffect(0, EFFECT_BANK_PRIORITY);
    PlayFrame();
    EXPECT(CountSoundChannels(handle) == 2);
    EXPECT(StopSoundKinds(SOUND_KIND_EFFECTS, 0) == 2);
    PlayFrame();
    EXPECT(CountSoundChannels(handle) == 0);
}

int main(void) {
    TestEffectPlaysNote();
    TestStopFades();
    TestLoopRepeats();
    TestPriorityTakesChannels();
    TestVolumeScales();
    TestTransposition();
    TestNestedEffectSharesHandle();
    return ReportTestResults("sequencer_test");
}
