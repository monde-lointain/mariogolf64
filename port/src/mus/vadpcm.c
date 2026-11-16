/*
 * Decoding VADPCM frames into 16-bit samples.
 *
 * For a half frame, sample i is the sum of three parts, in 5.11 fixed point:
 * the earlier samples times their coefficients for i, the residuals before i
 * in this half frame times the last row's coefficients, as though each were a
 * sample of its own, and residual i itself. The sums are kept in 64 bits,
 * which the largest scales and coefficients need, and clamped to 16.
 */

#include "vadpcm.h"

#include <SDL3/SDL.h>

#define COEFFICIENT_FRACTION_BITS 11
#define RESIDUALS_PER_BYTE 2

bool ReadVadpcmBook(const uint8_t *coefficients, int order, int predictorCount, VadpcmBook *book) {
    int predictor;
    int row;
    int column;
    const uint8_t *next = coefficients;

    SDL_assert(book != NULL);
    if (coefficients == NULL ||
        order < 1 ||
        order > VADPCM_MAX_ORDER ||
        predictorCount < 1 ||
        predictorCount > VADPCM_MAX_PREDICTORS) {
        return false;
    }

    SDL_zerop(book);
    book->order = order;
    book->predictorCount = predictorCount;
    for (predictor = 0; predictor < predictorCount; predictor++) {
        for (row = 0; row < order; row++) {
            for (column = 0; column < VADPCM_HALF_FRAME_SAMPLES; column++) {
                book->coefficients[predictor][row][column] = (int16_t)((next[0] << 8) | next[1]);
                next += 2;
            }
        }
    }

    return true;
}

// The residual in a nibble, sign-extended and scaled
static int32_t ScaleResidual(int nibble, int scaleShift) {
    int32_t residual = (nibble & 0x8) ? nibble - 0x10 : nibble;

    return residual * (1 << scaleShift);
}

static int16_t ClampToSample(int64_t value) {
    return (int16_t)SDL_clamp(value, INT16_MIN, INT16_MAX);
}

// Decodes one half frame. history holds the order samples before it, the
// latest last, and is left holding this half frame's last ones.
static void DecodeHalfFrame(
    const VadpcmBook *book,
    int predictor,
    const int32_t residuals[VADPCM_HALF_FRAME_SAMPLES],
    int32_t history[VADPCM_MAX_ORDER],
    int16_t samples[VADPCM_HALF_FRAME_SAMPLES]
) {
    const int order = book->order;
    const int16_t (*rows)[VADPCM_HALF_FRAME_SAMPLES] = book->coefficients[predictor];
    const int16_t *lastRow = rows[order - 1];
    int i;
    int j;
    int row;

    for (i = 0; i < VADPCM_HALF_FRAME_SAMPLES; i++) {
        int64_t sum = (int64_t)residuals[i] * (1 << COEFFICIENT_FRACTION_BITS);

        for (row = 0; row < order; row++) {
            sum += (int64_t)rows[row][i] * history[row];
        }

        for (j = 0; j < i; j++) {
            sum += (int64_t)lastRow[i - 1 - j] * residuals[j];
        }

        samples[i] = ClampToSample(sum >> COEFFICIENT_FRACTION_BITS);
    }

    for (row = 0; row < order; row++) {
        history[row] = samples[VADPCM_HALF_FRAME_SAMPLES - order + row];
    }
}

void DecodeVadpcm(
    const VadpcmBook *book,
    const uint8_t *frames,
    uint32_t frameCount,
    int16_t *samples
) {
    int32_t history[VADPCM_MAX_ORDER] = {0};
    int32_t residuals[VADPCM_HALF_FRAME_SAMPLES];
    uint32_t frame;
    int half;
    int i;

    SDL_assert(book != NULL && book->order >= 1 && book->order <= VADPCM_MAX_ORDER);
    SDL_assert(frames != NULL || frameCount == 0);
    SDL_assert(samples != NULL || frameCount == 0);
    for (frame = 0; frame < frameCount; frame++) {
        const uint8_t *bytes = frames + (frame * VADPCM_FRAME_SIZE);
        const int scaleShift = bytes[0] >> 4;
        const int predictor = SDL_min(bytes[0] & 0xF, book->predictorCount - 1);

        for (half = 0; half < 2; half++) {
            const uint8_t *packed =
                bytes + 1 + (half * VADPCM_HALF_FRAME_SAMPLES / RESIDUALS_PER_BYTE);

            for (i = 0; i < VADPCM_HALF_FRAME_SAMPLES; i += RESIDUALS_PER_BYTE) {
                residuals[i] = ScaleResidual(packed[i / 2] >> 4, scaleShift);
                residuals[i + 1] = ScaleResidual(packed[i / 2] & 0xF, scaleShift);
            }

            DecodeHalfFrame(book, predictor, residuals, history, samples);
            samples += VADPCM_HALF_FRAME_SAMPLES;
        }
    }
}
