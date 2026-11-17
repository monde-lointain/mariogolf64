/*
 * Tests the port's KMC random numbers (port/src/kmc/random.c) against
 * numbers worked out from the generator's definition: each state is the
 * last times 1103515245 plus 12345, modulo 2^32, and each number is bits
 * 16 to 30 of one more than the state.
 */

#include "port/kmc.h"
#include "test_support.h"

static void ExpectSequence(unsigned int seed, const int *expected, int count) {
    int i;

    SeedKmcRandom(seed);
    for (i = 0; i < count; i++) {
        EXPECT(KmcRandom() == expected[i]);
    }
}

// The game never seeds the generator, so it starts from zero
static void TestStartsFromZero(void) {
    static const int expected[] = {0, 21468, 9988, 22117, 3498, 16927};

    ExpectSequence(0, expected, sizeof(expected) / sizeof(expected[0]));
}

// Seeding again starts the same sequence again
static void TestRestartsFromSeed(void) {
    static const int expected[] = {21468, 9988};

    SeedKmcRandom(0);
    KmcRandom();
    KmcRandom();
    ExpectSequence(12345, expected, sizeof(expected) / sizeof(expected[0]));
}

// The largest seed's state wraps past 32 bits, to 0xBE39E1CC, as the
// N64's register did
static void TestWrapsState(void) {
    static const int expected[] = {15929, 4409};

    ExpectSequence(0xFFFFFFFFu, expected, sizeof(expected) / sizeof(expected[0]));
}

// This seed's next state is 0x0000FFFF, whose bits give 0, but one more
// carries into them and gives 1
static void TestTakesBitsFromOneMore(void) {
    static const int expected[] = {1, 3238};

    ExpectSequence(0xF922BB1Eu, expected, sizeof(expected) / sizeof(expected[0]));
}

static void TestStaysInRange(void) {
    int i;
    int lowest = KMC_RANDOM_MAX;
    int highest = 0;

    SeedKmcRandom(12345);
    for (i = 0; i < 100000; i++) {
        int value = KmcRandom();

        EXPECT(value >= 0 && value <= KMC_RANDOM_MAX);
        lowest = value < lowest ? value : lowest;
        highest = value > highest ? value : highest;
    }
    EXPECT(lowest == 0);
    EXPECT(highest == KMC_RANDOM_MAX);
}

int main(void) {
    TestStartsFromZero();
    TestRestartsFromSeed();
    TestWrapsState();
    TestTakesBitsFromOneMore();
    TestStaysInRange();
    return ReportTestResults("kmc_random_test");
}
