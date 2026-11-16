/*
 * Reading the game's songs, sample bank and sound effect bank.
 */

#include "sound_data.h"

#include <PR/libaudio.h>
#include <SDL3/SDL.h>

#include "vadpcm.h"

// Past this, an offset in the game's sound data is taken to be corrupt
#define OFFSET_LIMIT 0x1000000u

// The pointer bank's header
#define POINTER_BANK_COUNT 32
#define POINTER_BANK_BASE_NOTES 36
#define POINTER_BANK_DETUNES 40
#define POINTER_BANK_WAVES 44
// The bytes of each of its detune entries, of which only the first is used
#define DETUNE_ENTRY_SIZE 4
// Wave data at an address with all of these bits set is on the 64DD's disk
#define DISK_WAVE_MASK 0xFF000000u

// A wave's entry in the pointer bank, as libaudio's ALWaveTable laid out on
// the N64
#define WAVE_DATA 0
#define WAVE_SIZE 4
#define WAVE_TYPE 8
#define WAVE_LOOP 12
#define WAVE_BOOK 16

// A loop and a codebook, as the pointer bank holds them
#define LOOP_START 0
#define LOOP_END 4
#define LOOP_COUNT 8
#define BOOK_ORDER 0
#define BOOK_PREDICTOR_COUNT 4
#define BOOK_COEFFICIENTS 8

// A song's header
#define SONG_TRACK_COUNT 4
#define SONG_WAVE_COUNT 8
#define SONG_TRACK_NOTES 12
#define SONG_TRACK_VOLUMES 16
#define SONG_TRACK_BENDS 20
#define SONG_ENVELOPES 24
#define SONG_DRUMS 28
#define SONG_WAVES 32
#define SONG_MASTER_TRACK 36
// More tracks than any song could play at once
#define SONG_TRACK_LIMIT 256

// The sound effect bank's header, and its entries that follow it
#define EFFECT_BANK_COUNT 0
#define EFFECT_BANK_WAVES 20
#define EFFECT_BANK_ENTRIES 24
#define EFFECT_ENTRY_SIZE 8
#define EFFECT_ENTRY_DATA 0
#define EFFECT_ENTRY_PRIORITY 4
#define EFFECT_COUNT_LIMIT 0x8000

// The data at an offset from start, or NULL for a zero or corrupt offset
static const uint8_t *ResolveOffset(const uint8_t *start, uint32_t offset) {
    if (offset == 0 || offset >= OFFSET_LIMIT) {
        return NULL;
    }

    return start + offset;
}

static const uint8_t *ResolveOffsetAt(const uint8_t *start, uint32_t fieldOffset) {
    return ResolveOffset(start, ReadBig32(start + fieldOffset));
}

// A byte of the data that is a signed number
static int SignedByte(uint8_t value) {
    return (value & 0x80) ? value - 0x100 : value;
}

static uint32_t SampleCountOf(const Sample *sample) {
    if (sample->encoding == SAMPLE_VADPCM) {
        return (sample->dataSize / VADPCM_FRAME_SIZE) * VADPCM_FRAME_SAMPLES;
    }

    return sample->dataSize / sizeof(int16_t);
}

static void ReadSampleLoop(const uint8_t *loop, Sample *sample) {
    const uint32_t start = ReadBig32(loop + LOOP_START);
    const uint32_t end = SDL_min(ReadBig32(loop + LOOP_END), SampleCountOf(sample));
    const uint32_t count = ReadBig32(loop + LOOP_COUNT);

    sample->loops = count != 0 && start < end;
    sample->loopStart = start;
    sample->loopEnd = end;
    sample->loopCount = count;
}

// Reads one wave's entry, leaving the sample silent if its data isn't there
static void ReadSample(const uint8_t *bank, const uint8_t *wave, WaveSource waves, Sample *sample) {
    const uint32_t dataOffset = ReadBig32(wave + WAVE_DATA);
    const uint32_t dataSize = ReadBig32(wave + WAVE_SIZE);
    const uint8_t type = wave[WAVE_TYPE];
    const uint8_t *loop = ResolveOffset(bank, ReadBig32(wave + WAVE_LOOP));
    const uint8_t *book = ResolveOffset(bank, ReadBig32(wave + WAVE_BOOK));

    if ((dataOffset & DISK_WAVE_MASK) == DISK_WAVE_MASK ||
        waves.start == NULL ||
        dataOffset > waves.size ||
        dataSize > waves.size - dataOffset) {
        return;
    }

    if (type == AL_ADPCM_WAVE) {
        if (book == NULL) {
            return;
        }

        sample->encoding = SAMPLE_VADPCM;
        sample->predictorOrder = (uint8_t)SDL_min(ReadBig32(book + BOOK_ORDER), 0xFFu);
        sample->predictorCount = (uint8_t)SDL_min(ReadBig32(book + BOOK_PREDICTOR_COUNT), 0xFFu);
        sample->codebook = book + BOOK_COEFFICIENTS;
    }
    else if (type == AL_RAW16_WAVE) {
        sample->encoding = SAMPLE_PCM16;
    }
    else {
        return;
    }

    sample->data = waves.start + dataOffset;
    sample->dataSize = dataSize;
    if (loop != NULL) {
        ReadSampleLoop(loop, sample);
    }
}

bool ParseSampleBank(const void *pointerBank, WaveSource waves, SampleBank *bank) {
    const uint8_t *start = pointerBank;
    const uint8_t *baseNotes;
    const uint8_t *detunes;
    const uint8_t *waveEntries;
    uint32_t count;
    uint32_t i;

    SDL_assert(bank != NULL);
    SDL_zerop(bank);
    if (start == NULL) {
        return false;
    }

    count = ReadBig32(start + POINTER_BANK_COUNT);
    baseNotes = ResolveOffsetAt(start, POINTER_BANK_BASE_NOTES);
    detunes = ResolveOffsetAt(start, POINTER_BANK_DETUNES);
    waveEntries = ResolveOffsetAt(start, POINTER_BANK_WAVES);
    if (count == 0 ||
        count >= OFFSET_LIMIT ||
        baseNotes == NULL ||
        detunes == NULL ||
        waveEntries == NULL) {
        return false;
    }

    bank->samples = SDL_calloc(count, sizeof(Sample));
    if (bank->samples == NULL) {
        return false;
    }

    bank->source = pointerBank;
    bank->count = (int)count;
    for (i = 0; i < count; i++) {
        Sample *sample = &bank->samples[i];
        const uint8_t *wave = ResolveOffset(start, ReadBig32(waveEntries + (i * 4)));
        const int baseNote = SignedByte((uint8_t)(baseNotes[i] - SAMPLE_BASE_NOTE));
        const int cents = SignedByte(detunes[i * DETUNE_ENTRY_SIZE]);

        sample->tuning = (float)baseNote + ((float)cents / 100.0f);
        if (wave != NULL) {
            ReadSample(start, wave, waves, sample);
        }
    }

    return true;
}

void ReleaseSampleBank(SampleBank *bank) {
    int i;

    SDL_assert(bank != NULL);
    for (i = 0; i < bank->count; i++) {
        SDL_free(bank->samples[i].pcm);
    }

    SDL_free(bank->samples);
    SDL_zerop(bank);
}

static int16_t *DecodeSample(const Sample *sample, uint32_t length) {
    int16_t *pcm = SDL_malloc(length * sizeof(int16_t));
    uint32_t i;

    if (pcm == NULL) {
        return NULL;
    }

    if (sample->encoding == SAMPLE_VADPCM) {
        VadpcmBook book;

        if (!ReadVadpcmBook(
                sample->codebook,
                sample->predictorOrder,
                sample->predictorCount,
                &book
            )) {
            SDL_free(pcm);
            return NULL;
        }

        DecodeVadpcm(&book, sample->data, length / VADPCM_FRAME_SAMPLES, pcm);
    }
    else {
        for (i = 0; i < length; i++) {
            pcm[i] = (int16_t)ReadBig16(sample->data + (i * sizeof(int16_t)));
        }
    }

    return pcm;
}

const int16_t *GetSamplePcm(Sample *sample, uint32_t *length) {
    SDL_assert(sample != NULL && length != NULL);
    if (sample->pcm == NULL && sample->data != NULL) {
        const uint32_t sampleCount = SampleCountOf(sample);

        if (sampleCount != 0) {
            sample->pcm = DecodeSample(sample, sampleCount);
        }

        if (sample->pcm == NULL) {
            // Not worth trying again on every note
            sample->data = NULL;
        }
        else {
            sample->pcmLength = sampleCount;
        }
    }

    *length = sample->pcmLength;
    return sample->pcm;
}

bool ParseSong(const void *song, SongLayout *layout) {
    const uint8_t *start = song;
    uint32_t trackCount;
    uint32_t waveCount;

    SDL_assert(layout != NULL);
    SDL_zerop(layout);
    if (start == NULL) {
        return false;
    }

    trackCount = ReadBig32(start + SONG_TRACK_COUNT);
    waveCount = ReadBig32(start + SONG_WAVE_COUNT);
    if (trackCount > SONG_TRACK_LIMIT || waveCount >= OFFSET_LIMIT) {
        return false;
    }

    layout->start = start;
    layout->trackCount = (int)trackCount;
    layout->waveCount = (int)waveCount;
    layout->envelopes = ResolveOffsetAt(start, SONG_ENVELOPES);
    layout->drums = ResolveOffsetAt(start, SONG_DRUMS);
    layout->waves = ResolveOffsetAt(start, SONG_WAVES);
    layout->masterTrack = ResolveOffsetAt(start, SONG_MASTER_TRACK);
    return true;
}

// Entry track of the song's list of offsets in the header field at listField
static const uint8_t *ResolveTrackList(const SongLayout *layout, uint32_t listField, int track) {
    const uint8_t *list = ResolveOffsetAt(layout->start, listField);

    if (list == NULL) {
        return NULL;
    }

    return ResolveOffset(layout->start, ReadBig32(list + (track * 4)));
}

SongTrack GetSongTrack(const SongLayout *layout, int track) {
    SongTrack result = {NULL, NULL, NULL};

    SDL_assert(layout != NULL);
    if (track >= 0 && track < layout->trackCount) {
        result.notes = ResolveTrackList(layout, SONG_TRACK_NOTES, track);
        result.volumes = ResolveTrackList(layout, SONG_TRACK_VOLUMES, track);
        result.bends = ResolveTrackList(layout, SONG_TRACK_BENDS, track);
    }

    return result;
}

uint16_t LookUpWave(const uint8_t *waves, int wave) {
    if (waves == NULL || wave < 0) {
        return MISSING_WAVE;
    }

    return ReadBig16(waves + (wave * sizeof(uint16_t)));
}

bool ParseEffectBank(const void *effectBank, EffectBank *bank) {
    const uint8_t *start = effectBank;
    uint32_t count;

    SDL_assert(bank != NULL);
    SDL_zerop(bank);
    if (start == NULL) {
        return false;
    }

    count = ReadBig32(start + EFFECT_BANK_COUNT);
    if (count > EFFECT_COUNT_LIMIT) {
        return false;
    }

    bank->source = effectBank;
    bank->effectCount = (int)count;
    bank->waves = ResolveOffsetAt(start, EFFECT_BANK_WAVES);
    bank->entries = start + EFFECT_BANK_ENTRIES;
    return true;
}

const uint8_t *GetEffectData(const EffectBank *bank, int number) {
    SDL_assert(bank != NULL);
    if (number < 0 || number >= bank->effectCount) {
        return NULL;
    }

    return ResolveOffset(
        bank->source,
        ReadBig32(bank->entries + (number * EFFECT_ENTRY_SIZE) + EFFECT_ENTRY_DATA)
    );
}

int32_t GetEffectPriority(const EffectBank *bank, int number) {
    SDL_assert(bank != NULL);
    if (number < 0 || number >= bank->effectCount) {
        return 0;
    }

    return (int32_t)ReadBig32(bank->entries + (number * EFFECT_ENTRY_SIZE) + EFFECT_ENTRY_PRIORITY);
}
