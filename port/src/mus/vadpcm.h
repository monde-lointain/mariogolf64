/*
 * The N64's VADPCM sample encoding.
 *
 * A frame of nine bytes holds sixteen samples: a header byte, whose high
 * nibble is a scale's power of two and whose low nibble picks a predictor,
 * then sixteen signed 4-bit residuals. Each half frame of eight samples is
 * predicted from the samples before it with the predictor's coefficients,
 * in 5.11 fixed point, and the scaled residuals correct the prediction.
 */

#ifndef PORT_MUS_VADPCM_H
#define PORT_MUS_VADPCM_H

#include <stdbool.h>
#include <stdint.h>

#define VADPCM_FRAME_SIZE 9
#define VADPCM_FRAME_SAMPLES 16
// The samples that share one set of predictions
#define VADPCM_HALF_FRAME_SAMPLES 8
#define VADPCM_MAX_ORDER 8
#define VADPCM_MAX_PREDICTORS 16

// A sample's codebook, in the host's order: for each predictor, a row of
// eight coefficients for each of the earlier samples it looks back on
typedef struct VadpcmBook {
    int order;
    int predictorCount;
    int16_t coefficients[VADPCM_MAX_PREDICTORS][VADPCM_MAX_ORDER][VADPCM_HALF_FRAME_SAMPLES];
} VadpcmBook;

// Reads a codebook of big-endian coefficients, and tells whether its order
// and predictor count are ones the decoder handles
bool ReadVadpcmBook(const uint8_t *coefficients, int order, int predictorCount, VadpcmBook *book);

// Decodes frameCount frames into frameCount * VADPCM_FRAME_SAMPLES samples,
// starting from silence. A header naming a predictor past the book's last
// uses the last.
void DecodeVadpcm(
    const VadpcmBook *book,
    const uint8_t *frames,
    uint32_t frameCount,
    int16_t *samples
);

#endif
