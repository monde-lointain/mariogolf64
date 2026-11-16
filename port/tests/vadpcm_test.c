/*
 * Tests of the VADPCM decoder, on frames built by hand with codebooks whose
 * output is easy to work out.
 */

#include <string.h>

#include "test_support.h"
#include "vadpcm.h"

// A frame whose sixteen residuals all hold the same nibble
static void FillFrame(uint8_t *frame, int scaleShift, int predictor, int nibble) {
    frame[0] = (uint8_t)((scaleShift << 4) | predictor);
    memset(frame + 1, (nibble << 4) | nibble, VADPCM_FRAME_SIZE - 1);
}

// A book of one predictor that predicts nothing: every sample is its residual
static void MakeSilentBook(VadpcmBook *book, int order) {
    memset(book, 0, sizeof(*book));
    book->order = order;
    book->predictorCount = 1;
}

// A book whose predictor carries the last sample over, so that the samples
// are the running sum of the residuals
static void MakeSumBook(VadpcmBook *book) {
    int i;

    MakeSilentBook(book, 2);
    for (i = 0; i < VADPCM_HALF_FRAME_SAMPLES; i++) {
        book->coefficients[0][1][i] = 2048;
    }
}

// The scale shifts each sign-extended residual left
static void TestResidualsAreScaled(void) {
    VadpcmBook book;
    uint8_t frame[VADPCM_FRAME_SIZE] = {0x20, 0x78, 0xF0, 0x12, 0x34, 0x56, 0x7F, 0x8E, 0xD1};
    int16_t samples[VADPCM_FRAME_SAMPLES];
    const int16_t expected[VADPCM_FRAME_SAMPLES] =
        {28, -32, -4, 0, 4, 8, 12, 16, 20, 24, 28, -4, -32, -8, -12, 4};

    MakeSilentBook(&book, 2);
    DecodeVadpcm(&book, frame, 1, samples);
    EXPECT(memcmp(samples, expected, sizeof(expected)) == 0);
}

// The prediction runs across half frames and frames, and its sums saturate
static void TestPredictionCarriesAcrossFrames(void) {
    VadpcmBook book;
    uint8_t frames[2 * VADPCM_FRAME_SIZE];
    int16_t samples[2 * VADPCM_FRAME_SAMPLES];
    int i;

    MakeSumBook(&book);
    FillFrame(frames, 0, 0, 1);
    FillFrame(frames + VADPCM_FRAME_SIZE, 12, 0, 7);
    DecodeVadpcm(&book, frames, 2, samples);
    for (i = 0; i < VADPCM_FRAME_SAMPLES; i++) {
        EXPECT(samples[i] == i + 1);
    }

    // 16 + 7 * 4096 = 28688, then the next sum passes 32767
    EXPECT(samples[VADPCM_FRAME_SAMPLES] == 16 + (7 << 12));
    EXPECT(samples[VADPCM_FRAME_SAMPLES + 1] == INT16_MAX);
    EXPECT(samples[(2 * VADPCM_FRAME_SAMPLES) - 1] == INT16_MAX);
}

// The header's predictor picks the coefficients; one past the book's last
// uses the last
static void TestPredictorSelection(void) {
    VadpcmBook book;
    uint8_t frames[2 * VADPCM_FRAME_SIZE];
    int16_t samples[2 * VADPCM_FRAME_SAMPLES];

    MakeSumBook(&book);
    book.predictorCount = 2;
    // Predictor 1 predicts nothing
    FillFrame(frames, 0, 1, 2);
    // Predictor 5 doesn't exist, so it is predictor 1 as well
    FillFrame(frames + VADPCM_FRAME_SIZE, 0, 5, 3);
    DecodeVadpcm(&book, frames, 2, samples);
    EXPECT(samples[0] == 2);
    EXPECT(samples[VADPCM_FRAME_SAMPLES - 1] == 2);
    EXPECT(samples[VADPCM_FRAME_SAMPLES] == 3);
}

// A second-order coefficient row looks two samples back
static void TestSecondOrderHistory(void) {
    VadpcmBook book;
    uint8_t frames[2 * VADPCM_FRAME_SIZE];
    int16_t samples[2 * VADPCM_FRAME_SAMPLES];
    int i;

    MakeSilentBook(&book, 2);
    for (i = 0; i < VADPCM_HALF_FRAME_SAMPLES; i++) {
        // Half of the sample two before, at the start of each half frame
        book.coefficients[0][0][i] = (i == 0) ? 1024 : 0;
    }

    FillFrame(frames, 4, 0, 1);
    FillFrame(frames + VADPCM_FRAME_SIZE, 0, 0, 0);
    DecodeVadpcm(&book, frames, 2, samples);
    // The second half frame's first sample takes half of the first's
    // seventh, 16, as well as its own residual
    EXPECT(samples[VADPCM_HALF_FRAME_SAMPLES] == 16 + 8);
    // The next frame's first takes half of the 16 two samples back
    EXPECT(samples[VADPCM_FRAME_SAMPLES] == 8);
    EXPECT(samples[VADPCM_FRAME_SAMPLES + 1] == 0);
}

// Books are read from big-endian coefficients, and bad shapes are refused
static void TestReadBook(void) {
    VadpcmBook book;
    uint8_t coefficients[2 * 2 * VADPCM_HALF_FRAME_SAMPLES * 2];
    size_t i;

    for (i = 0; i < sizeof(coefficients); i += 2) {
        coefficients[i] = (uint8_t)(i == 0 ? 0xFF : 0x01);
        coefficients[i + 1] = (uint8_t)(i / 2);
    }

    EXPECT(ReadVadpcmBook(coefficients, 2, 2, &book));
    EXPECT(book.order == 2 && book.predictorCount == 2);
    EXPECT(book.coefficients[0][0][0] == -256);
    EXPECT(book.coefficients[0][0][1] == 0x0101);
    EXPECT(book.coefficients[1][1][7] == 0x0100 + 31);
    EXPECT(!ReadVadpcmBook(coefficients, 0, 2, &book));
    EXPECT(!ReadVadpcmBook(coefficients, 2, VADPCM_MAX_PREDICTORS + 1, &book));
    EXPECT(!ReadVadpcmBook(NULL, 2, 2, &book));
}

int main(void) {
    TestResidualsAreScaled();
    TestPredictionCarriesAcrossFrames();
    TestPredictorSelection();
    TestSecondOrderHistory();
    TestReadBook();
    return ReportTestResults("vadpcm_test");
}
