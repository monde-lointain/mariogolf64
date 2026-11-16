/*
 * A fast 64-bit hash for the renderer's caches. It needs only to make
 * collisions between different textures and states vanishingly rare, not to
 * stand up to an adversary.
 */

#ifndef PORT_GFX_HASH_H
#define PORT_GFX_HASH_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define HASH_SEED 0x243F6A8885A308D3ull

#define HASH_MULTIPLIER 0x9E3779B97F4A7C15ull

static inline uint64_t HashWord(uint64_t hash, uint64_t word) {
    hash ^= word;
    hash *= HASH_MULTIPLIER;
    return hash ^ (hash >> 29);
}

// Folds size bytes into the hash, eight at a time. Two streams of words run
// side by side so that their multiplies overlap.
static inline uint64_t HashBytes(uint64_t hash, const void *bytes, size_t size) {
    const uint8_t *cursor = bytes;
    uint64_t second = hash ^ HASH_MULTIPLIER;
    uint64_t word;
    uint64_t other;

    while (size >= 16) {
        memcpy(&word, cursor, 8);
        memcpy(&other, cursor + 8, 8);
        hash = HashWord(hash, word);
        second = HashWord(second, other);
        cursor += 16;
        size -= 16;
    }

    if (size >= 8) {
        memcpy(&word, cursor, 8);
        hash = HashWord(hash, word);
        cursor += 8;
        size -= 8;
    }

    if (size != 0) {
        word = 0;
        memcpy(&word, cursor, size);
        hash = HashWord(hash, word ^ ((uint64_t)size << 56));
    }

    return HashWord(hash, second);
}

// Mixes the bits of a finished hash so that every one depends on every input
static inline uint64_t FinishHash(uint64_t hash) {
    hash ^= hash >> 33;
    hash *= 0xFF51AFD7ED558CCDull;
    hash ^= hash >> 33;
    hash *= 0xC4CEB9FE1A85EC53ull;
    return hash ^ (hash >> 33);
}

#endif
