/*
 * Characterizes password.c: the score password that records a tournament
 * round, which rand() disguises, and the tournament password that a player
 * enters to choose a club tournament's rules.
 */

#include <stdlib.h>
#include <string.h>
#include <ultra64.h>

#include "menus/password.h"
#include "unity_fixture.h"

// The characters that the encoder writes, one for each symbol
#define WRITTEN_CHARACTERS "46XY12580V9ACERTWLMNPJKQ3FGH"
#define SYMBOL_COUNT (sizeof(WRITTEN_CHARACTERS) - 1)

// The seed of rand(), which disguises the score passwords below
#define RANDOM_SEED 1

// Tournament passwords drawn from the written characters, of which this
// many decode, to codes whose FNV-1a hash is VALID_CODE_HASH
#define RANDOM_PASSWORD_COUNT 20000
#define VALID_PASSWORD_COUNT 5
#define VALID_CODE_HASH 0xC8AEA195
#define FNV_OFFSET_BASIS 2166136261u
#define FNV_PRIME 16777619u

// The first of them, and its code
#define VALID_PASSWORD "5CJ6K2E4"
#define VALID_CODE 0xF31DFD

// The state of the generator that draws the random passwords, one of
// Numerical Recipes' linear congruential generators. The tests don't use
// rand(), which the code under test also calls.
static u32 f_PasswordRandomState;

TEST_GROUP(Password);

TEST_SETUP(Password) {
    srand(RANDOM_SEED);
    f_PasswordRandomState = 1;
}

TEST_TEAR_DOWN(Password) {
}

static u32 NextPasswordRandom(void) {
    f_PasswordRandomState = (f_PasswordRandomState * 1664525u) + 1013904223u;
    return f_PasswordRandomState >> 8;
}

// Encodes each report in turn, as rand()'s state carries from one to the
// next, and compares each password with the next of expected's
static void AssertScorePasswords(ScoreReport *reports, const char *expected) {
    char password[SCORE_PASSWORD_LENGTH];
    s32 i;

    for (i = 0; i < 3; i++) {
        EncodeScorePassword(&reports[i], password);
        TEST_ASSERT_EQUAL_STRING_LEN(
            &expected[i * SCORE_PASSWORD_LENGTH],
            password,
            SCORE_PASSWORD_LENGTH
        );
    }
}

TEST(Password, ScorePasswordsOfTypicalAndExtremeRounds) {
    ScoreReport reports[3] = {
        {{0x12, 0x34, 0x56}, 0, 72, 30 * 60 * 60, 1},
        {{0x12, 0x34, 0x56}, 0xFF, 0xFFFF, 0xFFFFFFFF, 0xFF},
        {{0x00, 0x00, 0x00}, 0, 0, 0, 0},
    };

    AssertScorePasswords(
        reports,
        "6HR2QMGERVLLTL10C9X4"
        "1521CJ20NTP35WJL2T2C"
        "ARNKVMC63QJ361NKPLWT"
    );
}

TEST(Password, TournamentPasswordDecodesToItsCode) {
    char password[] = VALID_PASSWORD;
    u8 code[TOURNAMENT_CODE_SIZE];

    TEST_ASSERT_EQUAL(0, DecodeTournamentPassword(code, password));
    TEST_ASSERT_EQUAL_HEX32(VALID_CODE, (code[0] << 16) | (code[1] << 8) | code[2]);
}

// A character that looks like a symbol's reads as that symbol: S as 5
TEST(Password, LookalikeCharacterReadsAsItsSymbol) {
    char password[] = VALID_PASSWORD;
    u8 code[TOURNAMENT_CODE_SIZE];

    password[0] = 'S';
    TEST_ASSERT_EQUAL(0, DecodeTournamentPassword(code, password));
    TEST_ASSERT_EQUAL_HEX32(VALID_CODE, (code[0] << 16) | (code[1] << 8) | code[2]);
}

TEST(Password, TournamentPasswordOutsideTheAlphabetIsInvalid) {
    char password[TOURNAMENT_PASSWORD_LENGTH];
    u8 code[TOURNAMENT_CODE_SIZE];

    memset(password, 'Z' + 1, sizeof(password));
    TEST_ASSERT_EQUAL(-1, DecodeTournamentPassword(code, password));
}

// The checksum symbols reject all but a few, and the rest decode to codes
// whose hash pins every byte
TEST(Password, RandomTournamentPasswordsDecodeAsTheOriginalDecodes) {
    char password[TOURNAMENT_PASSWORD_LENGTH];
    u8 code[TOURNAMENT_CODE_SIZE];
    u32 hash = FNV_OFFSET_BASIS;
    s32 validCount = 0;
    s32 n;
    s32 i;

    for (n = 0; n < RANDOM_PASSWORD_COUNT; n++) {
        for (i = 0; i < TOURNAMENT_PASSWORD_LENGTH; i++) {
            password[i] = WRITTEN_CHARACTERS[NextPasswordRandom() % SYMBOL_COUNT];
        }

        if (DecodeTournamentPassword(code, password) == 0) {
            validCount++;
            for (i = 0; i < TOURNAMENT_CODE_SIZE; i++) {
                hash = (hash ^ code[i]) * FNV_PRIME;
            }
        }
    }

    TEST_ASSERT_EQUAL(VALID_PASSWORD_COUNT, validCount);
    TEST_ASSERT_EQUAL_HEX32(VALID_CODE_HASH, hash);
}

TEST(Password, TournamentRulesOutOfRangeAreTheFirst) {
    TEST_ASSERT_EQUAL_PTR(GetTournamentRules(0), GetTournamentRules(0xFF));
}

TEST_GROUP_RUNNER(Password) {
    RUN_TEST_CASE(Password, ScorePasswordsOfTypicalAndExtremeRounds);
    RUN_TEST_CASE(Password, TournamentPasswordDecodesToItsCode);
    RUN_TEST_CASE(Password, LookalikeCharacterReadsAsItsSymbol);
    RUN_TEST_CASE(Password, TournamentPasswordOutsideTheAlphabetIsInvalid);
    RUN_TEST_CASE(Password, RandomTournamentPasswordsDecodeAsTheOriginalDecodes);
    RUN_TEST_CASE(Password, TournamentRulesOutOfRangeAreTheFirst);
}
