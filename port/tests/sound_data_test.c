/*
 * Tests of the reading of the game's sound data, on small banks and songs
 * built here in the big-endian layouts that the Music Tools wrote.
 */

#include <PR/libaudio.h>
#include <string.h>

#include "sound_data.h"
#include "test_support.h"
#include "vadpcm.h"

static uint8_t f_Data[0x400];
static uint8_t f_Waves[0x100];

static void Put16(uint8_t *data, uint32_t offset, uint32_t value) {
    data[offset] = (uint8_t)(value >> 8);
    data[offset + 1] = (uint8_t)value;
}

static void Put32(uint8_t *data, uint32_t offset, uint32_t value) {
    data[offset] = (uint8_t)(value >> 24);
    data[offset + 1] = (uint8_t)(value >> 16);
    data[offset + 2] = (uint8_t)(value >> 8);
    data[offset + 3] = (uint8_t)value;
}

// One wave's entry at offset: its data's place and size, its type, and the
// offsets of its loop and codebook
static void PutWave(
    uint32_t offset,
    uint32_t data,
    uint32_t size,
    int type,
    uint32_t loop,
    uint32_t book
) {
    Put32(f_Data, offset, data);
    Put32(f_Data, offset + 4, size);
    f_Data[offset + 8] = (uint8_t)type;
    Put32(f_Data, offset + 12, loop);
    Put32(f_Data, offset + 16, book);
}

/*
 * A pointer bank of four samples:
 *   0: VADPCM, two frames at wave offset 0x10, looping from 4 to 40 forever
 *   1: 16-bit PCM, four samples at wave offset 0x40
 *   2: on the 64DD's disk
 *   3: past the end of the wave data
 */
static void BuildSampleBank(void) {
    int i;

    memset(f_Data, 0, sizeof(f_Data));
    memcpy(f_Data, "N64 PtrTablesV2", 16);
    Put32(f_Data, 32, 4);
    Put32(f_Data, 36, 0x30); // Base notes
    Put32(f_Data, 40, 0x40); // Detunes
    Put32(f_Data, 44, 0x60); // Wave entries
    f_Data[0x30] = SAMPLE_BASE_NOTE + 3;
    f_Data[0x31] = SAMPLE_BASE_NOTE - 12;
    f_Data[0x32] = SAMPLE_BASE_NOTE;
    f_Data[0x33] = SAMPLE_BASE_NOTE;
    f_Data[0x40] = 50; // +0.5 semitones
    f_Data[0x44] = 0xE7; // -0.25
    for (i = 0; i < 4; i++) {
        Put32(f_Data, 0x60 + (i * 4), 0x80 + (i * 0x20));
    }

    PutWave(0x80, 0x10, 2 * VADPCM_FRAME_SIZE, AL_ADPCM_WAVE, 0x100, 0x140);
    PutWave(0xA0, 0x40, 8, AL_RAW16_WAVE, 0, 0);
    PutWave(0xC0, 0xFF000010, 8, AL_RAW16_WAVE, 0, 0);
    PutWave(0xE0, 0xF0, 0x20, AL_RAW16_WAVE, 0, 0);
    Put32(f_Data, 0x100, 4);
    Put32(f_Data, 0x104, 40);
    Put32(f_Data, 0x108, AL_LOOP_FOREVER);
    // A first-order book of one predictor, all zero, so that each sample is
    // its residual
    Put32(f_Data, 0x140, 1);
    Put32(f_Data, 0x144, 1);

    memset(f_Waves, 0, sizeof(f_Waves));
    f_Waves[0x10] = 0x10; // A scale of 2, and predictor 0
    f_Waves[0x11] = 0x31; // Samples of 6 and 2
    Put16(f_Waves, 0x40, 0x1234);
    Put16(f_Waves, 0x42, 0xFFFE);
}

static void TestSampleBank(void) {
    SampleBank bank;
    WaveSource waves = {f_Waves, sizeof(f_Waves)};
    const Sample *samples;

    BuildSampleBank();
    EXPECT(ParseSampleBank(f_Data, waves, &bank));
    EXPECT(bank.count == 4 && bank.source == f_Data);
    samples = bank.samples;
    EXPECT(samples[0].encoding == SAMPLE_VADPCM);
    EXPECT(samples[0].data == f_Waves + 0x10);
    EXPECT(samples[0].dataSize == 2 * VADPCM_FRAME_SIZE);
    EXPECT(samples[0].predictorOrder == 1 && samples[0].predictorCount == 1);
    EXPECT(samples[0].codebook == f_Data + 0x148);
    EXPECT(samples[0].loops && samples[0].loopStart == 4);
    // The loop's end is cut to the sample's 32 samples
    EXPECT(samples[0].loopEnd == 2 * VADPCM_FRAME_SAMPLES);
    EXPECT(samples[0].loopCount == AL_LOOP_FOREVER);
    EXPECT_NEAR(samples[0].tuning, 3.5, 1e-6);
    EXPECT(samples[1].encoding == SAMPLE_PCM16 && samples[1].data == f_Waves + 0x40);
    EXPECT(!samples[1].loops);
    EXPECT_NEAR(samples[1].tuning, -12.25, 1e-6);
    EXPECT(samples[2].data == NULL);
    EXPECT(samples[3].data == NULL);
    ReleaseSampleBank(&bank);
    EXPECT(bank.count == 0 && bank.samples == NULL);
}

static void TestSamplePcm(void) {
    SampleBank bank;
    WaveSource waves = {f_Waves, sizeof(f_Waves)};
    const int16_t *pcm;
    uint32_t length = 0;

    BuildSampleBank();
    EXPECT(ParseSampleBank(f_Data, waves, &bank));
    pcm = GetSamplePcm(&bank.samples[0], &length);
    EXPECT(pcm != NULL && length == 2 * VADPCM_FRAME_SAMPLES);
    if (pcm != NULL) {
        EXPECT(pcm[0] == 6 && pcm[1] == 2 && pcm[2] == 0);
    }

    // A second call gives the same decoding
    EXPECT(GetSamplePcm(&bank.samples[0], &length) == pcm);
    pcm = GetSamplePcm(&bank.samples[1], &length);
    EXPECT(pcm != NULL && length == 4);
    if (pcm != NULL) {
        EXPECT(pcm[0] == 0x1234 && pcm[1] == -2 && pcm[2] == 0);
    }

    EXPECT(GetSamplePcm(&bank.samples[2], &length) == NULL && length == 0);
    ReleaseSampleBank(&bank);
}

static void TestNotASampleBank(void) {
    SampleBank bank;
    WaveSource waves = {f_Waves, sizeof(f_Waves)};

    memset(f_Data, 0, sizeof(f_Data));
    EXPECT(!ParseSampleBank(f_Data, waves, &bank));
    EXPECT(bank.count == 0 && bank.samples == NULL);
    EXPECT(!ParseSampleBank(NULL, waves, &bank));
}

// A song of two tracks, the second with no streams
static void TestSong(void) {
    SongLayout layout;
    SongTrack track;

    memset(f_Data, 0, sizeof(f_Data));
    Put32(f_Data, 0, 0x00020015);
    Put32(f_Data, 4, 2);
    Put32(f_Data, 8, 3);
    Put32(f_Data, 12, 0x40); // Track notes
    Put32(f_Data, 16, 0x48); // Volume streams
    Put32(f_Data, 20, 0x50); // Pitch bend streams
    Put32(f_Data, 24, 0x60); // Envelopes
    Put32(f_Data, 28, 0); // No drums
    Put32(f_Data, 32, 0x70); // Waves
    Put32(f_Data, 36, 0x80); // Master track
    Put32(f_Data, 0x40, 0x90);
    Put32(f_Data, 0x44, 0xA0);
    Put32(f_Data, 0x48, 0xB0);
    Put32(f_Data, 0x50, 0xC0);
    Put16(f_Data, 0x70, 7);
    Put16(f_Data, 0x72, MISSING_WAVE);

    EXPECT(ParseSong(f_Data, &layout));
    EXPECT(layout.trackCount == 2 && layout.waveCount == 3);
    EXPECT(layout.envelopes == f_Data + 0x60 && layout.drums == NULL);
    EXPECT(layout.waves == f_Data + 0x70 && layout.masterTrack == f_Data + 0x80);
    track = GetSongTrack(&layout, 0);
    EXPECT(track.notes == f_Data + 0x90);
    EXPECT(track.volumes == f_Data + 0xB0 && track.bends == f_Data + 0xC0);
    track = GetSongTrack(&layout, 1);
    EXPECT(track.notes == f_Data + 0xA0 && track.volumes == NULL && track.bends == NULL);
    track = GetSongTrack(&layout, 2);
    EXPECT(track.notes == NULL);
    EXPECT(LookUpWave(layout.waves, 0) == 7);
    EXPECT(LookUpWave(layout.waves, 1) == MISSING_WAVE);
    EXPECT(LookUpWave(NULL, 0) == MISSING_WAVE);

    // A header with an impossible track count is refused
    Put32(f_Data, 4, 0x7FFFFFFF);
    EXPECT(!ParseSong(f_Data, &layout));
}

// A bank of two effects
static void TestEffectBank(void) {
    EffectBank bank;

    memset(f_Data, 0, sizeof(f_Data));
    Put32(f_Data, 0, 2);
    Put32(f_Data, 4, 2);
    Put32(f_Data, 20, 0x40);
    Put32(f_Data, 24, 0x50);
    Put32(f_Data, 28, 100);
    Put32(f_Data, 32, 0x58);
    Put32(f_Data, 36, 0xFFFFFFFF);
    Put16(f_Data, 0x42, 9);

    EXPECT(ParseEffectBank(f_Data, &bank));
    EXPECT(bank.effectCount == 2 && bank.source == f_Data);
    EXPECT(LookUpWave(bank.waves, 1) == 9);
    EXPECT(GetEffectData(&bank, 0) == f_Data + 0x50);
    EXPECT(GetEffectData(&bank, 1) == f_Data + 0x58);
    EXPECT(GetEffectData(&bank, 2) == NULL && GetEffectData(&bank, -1) == NULL);
    EXPECT(GetEffectPriority(&bank, 0) == 100);
    EXPECT(GetEffectPriority(&bank, 1) == -1);
}

int main(void) {
    TestSampleBank();
    TestSamplePcm();
    TestNotASampleBank();
    TestSong();
    TestEffectBank();
    return ReportTestResults("sound_data_test");
}
