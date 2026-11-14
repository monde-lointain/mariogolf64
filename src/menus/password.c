/*
 * Encodes the score passwords and decodes the tournament passwords. Passwords
 * are strings of symbols from a 28-symbol alphabet. A symbol's character also
 * depends on its position in the password, and the last symbols are checksums
 * of the others.
 */

#include "menus/password.h"

#include <stdlib.h>
#include <ultra64.h>

#include "characters/character.h"
#include "system/c_language.h"
#include "system/compiler.h"

#define PASSWORD_SYMBOL_COUNT 28

// Symbols that hold a number of up to three bytes, in base 28
#define NUMBER_SYMBOL_COUNT 5

#define TOURNAMENT_RULES_COUNT 192

// Scores are stored with this added, so that they fit in SCORE_RANGE
#define SCORE_PASSWORD_BIAS 56
#define SCORE_RANGE 200
#define TIME_RANGE 108000
#define PASSWORD_GAME_BOY_LEVEL_LIMIT 100
// A random number in this range pads the score's number
#define SCORE_NOISE_RANGE 4600

// Added to the score password's numbers to disguise them
#define TIME_NUMBER_SALT 0x5B347F
#define SCORE_NUMBER_SALT 0x3507F

// Writes symbolCount symbols as characters. It fails with -1 on a symbol
// outside the alphabet. Either variant writes each symbol's first
// character.
Outcome WritePasswordSymbols(u8 *symbols, char *password, s32 symbolCount, s32 variant);

TournamentRules g_ClubTournamentRules;

// Each symbol's characters. The first one is the one written; the others
// are accepted because they look alike.
static char *f_PasswordAlphabet[PASSWORD_SYMBOL_COUNT] = {
    "4",
    "6",
    "X",
    "Y",
    "17I",
    "2Z",
    "5S",
    "8B",
    "0OD",
    "VU",
    "9",
    "A",
    "C",
    "E",
    "R",
    "T",
    "W",
    "L",
    "M",
    "N",
    "P",
    "J",
    "K",
    "Q",
    "3",
    "F",
    "G",
    "H",
};

// XORed with the tournament password's bytes
static u8 f_TournamentKey[] = "evi";

static TournamentRules f_TournamentRules[TOURNAMENT_RULES_COUNT] = {
    {-1, 0, 0},
    {-1, 0, 1},
    {-1, 0, 2},
    {-1, 0, 3},
    {-1, 0, 4},
    {-1, 0, 5},
    {-1, 0, 6},
    {-1, 0, 7},
    {-1, 0, 8},
    {-1, 0, 9},
    {-1, 0, 10},
    {-1, 0, 11},
    {-1, 1, 0},
    {-1, 1, 1},
    {-1, 1, 2},
    {-1, 1, 3},
    {-1, 1, 4},
    {-1, 1, 5},
    {-1, 1, 6},
    {-1, 1, 7},
    {-1, 1, 8},
    {-1, 1, 9},
    {-1, 1, 10},
    {-1, 1, 11},
    {0, 0, 0},
    {0, 0, 1},
    {0, 0, 2},
    {0, 0, 3},
    {0, 0, 4},
    {0, 0, 5},
    {0, 1, 0},
    {0, 1, 1},
    {0, 1, 2},
    {0, 1, 3},
    {0, 1, 4},
    {0, 1, 5},
    {1, 0, 0},
    {1, 0, 1},
    {1, 0, 2},
    {1, 0, 3},
    {1, 0, 4},
    {1, 0, 5},
    {1, 1, 0},
    {1, 1, 1},
    {1, 1, 2},
    {1, 1, 3},
    {1, 1, 4},
    {1, 1, 5},
    {2, 0, 0},
    {2, 0, 1},
    {2, 0, 2},
    {2, 0, 3},
    {2, 0, 4},
    {2, 0, 5},
    {2, 1, 0},
    {2, 1, 1},
    {2, 1, 2},
    {2, 1, 3},
    {2, 1, 4},
    {2, 1, 5},
    {3, 0, 0},
    {3, 0, 1},
    {3, 0, 2},
    {3, 0, 3},
    {3, 0, 4},
    {3, 0, 5},
    {3, 1, 0},
    {3, 1, 1},
    {3, 1, 2},
    {3, 1, 3},
    {3, 1, 4},
    {3, 1, 5},
    {4, 0, 0},
    {4, 0, 1},
    {4, 0, 2},
    {4, 0, 3},
    {4, 0, 4},
    {4, 0, 5},
    {4, 1, 0},
    {4, 1, 1},
    {4, 1, 2},
    {4, 1, 3},
    {4, 1, 4},
    {4, 1, 5},
    {5, 0, 0},
    {5, 0, 1},
    {5, 0, 2},
    {5, 0, 3},
    {5, 0, 4},
    {5, 0, 5},
    {5, 1, 0},
    {5, 1, 1},
    {5, 1, 2},
    {5, 1, 3},
    {5, 1, 4},
    {5, 1, 5},
    {6, 0, 0},
    {6, 0, 1},
    {6, 0, 2},
    {6, 0, 3},
    {6, 0, 4},
    {6, 0, 5},
    {6, 1, 0},
    {6, 1, 1},
    {6, 1, 2},
    {6, 1, 3},
    {6, 1, 4},
    {6, 1, 5},
    {7, 0, 0},
    {7, 0, 1},
    {7, 0, 2},
    {7, 0, 3},
    {7, 0, 4},
    {7, 0, 5},
    {7, 1, 0},
    {7, 1, 1},
    {7, 1, 2},
    {7, 1, 3},
    {7, 1, 4},
    {7, 1, 5},
    {8, 0, 0},
    {8, 0, 1},
    {8, 0, 2},
    {8, 0, 3},
    {8, 0, 4},
    {8, 0, 5},
    {8, 1, 0},
    {8, 1, 1},
    {8, 1, 2},
    {8, 1, 3},
    {8, 1, 4},
    {8, 1, 5},
    {9, 0, 0},
    {9, 0, 1},
    {9, 0, 2},
    {9, 0, 3},
    {9, 0, 4},
    {9, 0, 5},
    {9, 1, 0},
    {9, 1, 1},
    {9, 1, 2},
    {9, 1, 3},
    {9, 1, 4},
    {9, 1, 5},
    {10, 0, 0},
    {10, 0, 1},
    {10, 0, 2},
    {10, 0, 3},
    {10, 0, 4},
    {10, 0, 5},
    {10, 1, 0},
    {10, 1, 1},
    {10, 1, 2},
    {10, 1, 3},
    {10, 1, 4},
    {10, 1, 5},
    {11, 0, 0},
    {11, 0, 1},
    {11, 0, 2},
    {11, 0, 3},
    {11, 0, 4},
    {11, 0, 5},
    {11, 1, 0},
    {11, 1, 1},
    {11, 1, 2},
    {11, 1, 3},
    {11, 1, 4},
    {11, 1, 5},
    {12, 0, 0},
    {12, 0, 1},
    {12, 0, 2},
    {12, 0, 3},
    {12, 0, 4},
    {12, 0, 5},
    {12, 1, 0},
    {12, 1, 1},
    {12, 1, 2},
    {12, 1, 3},
    {12, 1, 4},
    {12, 1, 5},
    {13, 0, 0},
    {13, 0, 1},
    {13, 0, 2},
    {13, 0, 3},
    {13, 0, 4},
    {13, 0, 5},
    {13, 1, 0},
    {13, 1, 1},
    {13, 1, 2},
    {13, 1, 3},
    {13, 1, 4},
    {13, 1, 5},
};

static u8 ReverseBits(u8 bits) {
    u8 reversed = 0;
    s32 bitIndex;

    for (bitIndex = 0; bitIndex < BITS_PER_BYTE; bitIndex++) {
        reversed *= 2;
        if (bits & 1) {
            reversed |= 1;
        }

        bits >>= 1;
    }

    return reversed;
}

// The original keeps the fourth parameter live through the loop: no
// pseudo there takes $a3, so i is in $t0 and each later value one
// register higher, as global-alloc's find_reg skips a register that a
// conflicting pseudo prefers, and only a copy of $a3 prefers it. Testing
// the parameter around the store, with the same store in both arms, does
// that: jump2 merges the arms after register allocation and then deletes
// the test, a branch to the next instruction, leaving no trace in the
// code. The arms reach the character through a pointer to the alphabet's
// entry, which gives the entry's address a register of its own ($v0) as
// the original does.
//
// The identical arms are the one form here with no other reason to
// exist, kept as the last resort: without the test 34 words differ, 2 in
// shape, and returning the parameter in place of 0, which also keeps it
// live, copies $a3 to $v0. With the test and the entry read directly, 4
// words differ, none in shape; the parameter's type, pointer walks,
// symbol locals, helpers and u8 or u16 counters gave no closer form.
Outcome WritePasswordSymbols(u8 *symbols, char *password, s32 symbolCount, s32 variant) {
    s32 symbolIndex;
    char **spellings;

    for (symbolIndex = 0; symbolIndex < symbolCount; symbolIndex++) {
        if (symbols[symbolIndex] >= PASSWORD_SYMBOL_COUNT) {
            return OUTCOME_FAILED;
        }

        spellings =
            &f_PasswordAlphabet[(symbols[symbolIndex] + symbolIndex) % PASSWORD_SYMBOL_COUNT];
        if (variant) {
            password[symbolIndex] = (*spellings)[0];
        }
        else {
            password[symbolIndex] = (*spellings)[0];
        }
    }

    return OUTCOME_OK;
}

// Reads symbolCount characters as symbols. It fails with -1 on a character
// outside the alphabet.
static Outcome ReadPasswordSymbols(u8 *symbols, char *password, s32 symbolCount) {
    s32 symbolIndex;
    s32 symbol;
    s32 spellingIndex;

    for (symbolIndex = 0; symbolIndex < symbolCount; symbolIndex++) {
        for (symbol = 0; symbol < PASSWORD_SYMBOL_COUNT; symbol++) {
            char *spellings = f_PasswordAlphabet[symbol];
            Boolean isFound = FALSE;

            for (spellingIndex = 0; spellings[spellingIndex] != '\0'; spellingIndex++) {
                if (spellings[spellingIndex] == password[symbolIndex]) {
                    isFound = TRUE;
                    break;
                }
            }

            if (isFound) {
                break;
            }
        }

        if (symbol >= PASSWORD_SYMBOL_COUNT) {
            return OUTCOME_FAILED;
        }

        symbols[symbolIndex] =
            (symbol + PASSWORD_SYMBOL_COUNT - symbolIndex) % PASSWORD_SYMBOL_COUNT;
    }

    return OUTCOME_OK;
}

Outcome DecodeTournamentPassword(u8 *code, char *password) {
    u8 bytes[TOURNAMENT_CODE_SIZE];
    u8 symbols[TOURNAMENT_PASSWORD_LENGTH];
    u32 number;
    s32 i;

    if (ReadPasswordSymbols(symbols, password, TOURNAMENT_PASSWORD_LENGTH) != OUTCOME_OK) {
        return OUTCOME_FAILED;
    }

    symbols[0] = (symbols[0] + PASSWORD_SYMBOL_COUNT - symbols[5]) % PASSWORD_SYMBOL_COUNT;
    symbols[1] = (symbols[1] + PASSWORD_SYMBOL_COUNT - symbols[6]) % PASSWORD_SYMBOL_COUNT;
    symbols[2] = (symbols[2] + PASSWORD_SYMBOL_COUNT - symbols[7]) % PASSWORD_SYMBOL_COUNT;
    symbols[3] = (symbols[3] + PASSWORD_SYMBOL_COUNT - symbols[7]) % PASSWORD_SYMBOL_COUNT;
    symbols[4] = (symbols[4] + PASSWORD_SYMBOL_COUNT - symbols[5]) % PASSWORD_SYMBOL_COUNT;
    if (symbols[5] != (symbols[4] + symbols[0] + symbols[1]) % (u32)PASSWORD_SYMBOL_COUNT) {
        return OUTCOME_FAILED;
    }

    if (symbols[6] != (symbols[1] + symbols[2] + symbols[3]) % (u32)PASSWORD_SYMBOL_COUNT) {
        return OUTCOME_FAILED;
    }

    if (symbols[7] != (symbols[2] + symbols[3] + symbols[4]) % (u32)PASSWORD_SYMBOL_COUNT) {
        return OUTCOME_FAILED;
    }

    number = 0;
    for (i = NUMBER_SYMBOL_COUNT - 1; i >= 0; i--) {
        number *= PASSWORD_SYMBOL_COUNT;
        number += symbols[i];
    }

    bytes[0] = number >> 16;
    bytes[1] = number >> 8;
    bytes[2] = number;
    for (i = 0; i < TOURNAMENT_CODE_SIZE; i++) {
        code[i] = ReverseBits(bytes[i] ^ f_TournamentKey[i]);
    }

    return OUTCOME_OK;
}

void EncodeScorePassword(ScoreReport *report, char *password) {
    // Pads the frame to match; without it 80 words differ
    u8 unusedBuffer[8] UNUSED;
    u8 symbols[SCORE_PASSWORD_LENGTH];
    ScoreReport clampedReport;
    ScoreReport *clamped = &clampedReport;
    s32 symbolCount = 0;
    u32 number;
    s32 symbolIndex;

    clampedReport = *report;

    if (clamped->characterIndex >= CHARACTER_COUNT) {
        clamped->characterIndex = 0;
    }

    clamped->score += SCORE_PASSWORD_BIAS;
    if (clamped->score >= SCORE_RANGE) {
        clamped->score = SCORE_RANGE - 1;
    }

    clamped->time--;
    if (clamped->time > TIME_RANGE - 1) {
        clamped->time = TIME_RANGE - 1;
    }

    if (clamped->gameBoyLevel >= PASSWORD_GAME_BOY_LEVEL_LIMIT) {
        clamped->gameBoyLevel = PASSWORD_GAME_BOY_LEVEL_LIMIT - 1;
    }

    number = (clamped->tournamentCode[0] << 16) | (clamped->tournamentCode[1] << 8) |
        clamped->tournamentCode[2];
    for (symbolIndex = 0; symbolIndex < NUMBER_SYMBOL_COUNT; symbolIndex++) {
        symbols[symbolCount++] = number % PASSWORD_SYMBOL_COUNT;
        number /= PASSWORD_SYMBOL_COUNT;
    }

    number = (clamped->gameBoyLevel * TIME_RANGE) + clamped->time + TIME_NUMBER_SALT;
    for (symbolIndex = 0; symbolIndex < NUMBER_SYMBOL_COUNT; symbolIndex++) {
        symbols[symbolCount++] = number % PASSWORD_SYMBOL_COUNT;
        number /= PASSWORD_SYMBOL_COUNT;
    }

    number = (clamped->characterIndex * SCORE_RANGE) + (s16)clamped->score +
        ((rand() % SCORE_NOISE_RANGE) * (CHARACTER_COUNT * SCORE_RANGE)) + SCORE_NUMBER_SALT;
    for (symbolIndex = 0; symbolIndex < NUMBER_SYMBOL_COUNT; symbolIndex++) {
        symbols[symbolCount++] = number % PASSWORD_SYMBOL_COUNT;
        number /= PASSWORD_SYMBOL_COUNT;
    }

    for (symbolIndex = 0; symbolIndex < NUMBER_SYMBOL_COUNT; symbolIndex++) {
        symbols[symbolIndex] =
            (symbols[symbolIndex] + symbols[(2 * NUMBER_SYMBOL_COUNT) + symbolIndex]) %
            PASSWORD_SYMBOL_COUNT;
        symbols[NUMBER_SYMBOL_COUNT + symbolIndex] =
            (symbols[NUMBER_SYMBOL_COUNT + symbolIndex] +
                symbols[(2 * NUMBER_SYMBOL_COUNT) + symbolIndex]) %
            PASSWORD_SYMBOL_COUNT;
    }

    for (symbolIndex = 0; symbolIndex < NUMBER_SYMBOL_COUNT; symbolIndex++) {
        symbols[symbolCount++] =
            (symbols[symbolIndex] + symbols[NUMBER_SYMBOL_COUNT + symbolIndex] +
                symbols[(2 * NUMBER_SYMBOL_COUNT) + symbolIndex]) %
            PASSWORD_SYMBOL_COUNT;
    }

    WritePasswordSymbols(symbols, password, SCORE_PASSWORD_LENGTH, 0);
}

TournamentRules *GetTournamentRules(u8 rulesIndex) {
    if (rulesIndex >= TOURNAMENT_RULES_COUNT) {
        return &f_TournamentRules[0];
    }

    return &f_TournamentRules[rulesIndex];
}
