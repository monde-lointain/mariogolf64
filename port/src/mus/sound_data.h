/*
 * The game's sound data, as the Music Tools wrote it: the sample bank, the
 * songs and the sound effect bank.
 *
 * Each is big-endian, with 32-bit offsets where the N64's player wrote
 * pointers over them. The port leaves the data as the game read it and
 * keeps what it finds in host structures instead, so 64-bit pointers never
 * have to fit in the data's 32-bit fields.
 *
 * The data comes from the game's own memory, which gives no sizes for
 * songs or banks, so offsets are only checked against a generous limit;
 * the sample bank's samples, which are read from the ROM, are checked
 * against where they come from.
 */

#ifndef PORT_MUS_SOUND_DATA_H
#define PORT_MUS_SOUND_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The note that plays a sample at its own rate, before its tuning
#define SAMPLE_BASE_NOTE 48
// A song's wave table entry for a wave it doesn't have
#define MISSING_WAVE 0xFFFF
// The bytes of a song's envelope and drum entries
#define ENVELOPE_ENTRY_SIZE 7
#define DRUM_ENTRY_SIZE 6

typedef enum SampleEncoding {
    SAMPLE_VADPCM,
    SAMPLE_PCM16
} SampleEncoding;

// A sample of a bank. One with no data is silent.
typedef struct Sample {
    const uint8_t *data;
    uint32_t dataSize;
    SampleEncoding encoding;
    const uint8_t *codebook; // Big-endian, for VADPCM
    uint8_t predictorOrder;
    uint8_t predictorCount;
    bool loops;
    uint32_t loopStart; // In samples
    uint32_t loopEnd;
    uint32_t loopCount; // AL_LOOP_FOREVER for a loop that never ends
    float tuning; // Semitones that every note played with it is raised by
    // The decoded samples, filled in on first use by GetSamplePcm
    int16_t *pcm;
    uint32_t pcmLength;
} Sample;

typedef struct SampleBank {
    const void *source; // The pointer bank it was read from
    int count;
    Sample *samples;
} SampleBank;

// Where a sample bank's wave data is: the ROM, or the game's memory
typedef struct WaveSource {
    const uint8_t *start;
    size_t size;
} WaveSource;

// A song's header. Each table is NULL when the song has none.
typedef struct SongLayout {
    const uint8_t *start;
    int trackCount;
    int waveCount;
    const uint8_t *envelopes;
    const uint8_t *drums;
    const uint8_t *waves;
    const uint8_t *masterTrack;
} SongLayout;

// A song's track: its notes and commands, and its streams of volume and
// pitch bend levels, each NULL when it has none
typedef struct SongTrack {
    const uint8_t *notes;
    const uint8_t *volumes;
    const uint8_t *bends;
} SongTrack;

typedef struct EffectBank {
    const void *source;
    int effectCount;
    const uint8_t *waves;
    const uint8_t *entries;
} EffectBank;

static inline uint16_t ReadBig16(const uint8_t *bytes) {
    return (uint16_t)((bytes[0] << 8) | bytes[1]);
}

static inline uint32_t ReadBig32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) |
        bytes[3];
}

// Reads a pointer bank and the samples it describes in waves. Gives false,
// and an empty bank, for one that isn't a pointer bank.
bool ParseSampleBank(const void *pointerBank, WaveSource waves, SampleBank *bank);
void ReleaseSampleBank(SampleBank *bank);

// The sample's 16-bit samples, decoded on first use; NULL for a silent one
const int16_t *GetSamplePcm(Sample *sample, uint32_t *length);

bool ParseSong(const void *song, SongLayout *layout);
// The song's track, all NULL for a track number it doesn't have
SongTrack GetSongTrack(const SongLayout *layout, int track);
// The bank's number for one of a song's or a bank's waves, or MISSING_WAVE
uint16_t LookUpWave(const uint8_t *waves, int wave);

bool ParseEffectBank(const void *effectBank, EffectBank *bank);
// The effect's notes and commands, or NULL for a number the bank doesn't have
const uint8_t *GetEffectData(const EffectBank *bank, int number);
int32_t GetEffectPriority(const EffectBank *bank, int number);

#endif
