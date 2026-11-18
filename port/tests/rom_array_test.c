/*
 * Tests of filling the game's arrays from the cartridge's ROM: each must
 * hold the values that the N64 build's initializer gave it, in the host's
 * order, and nothing else of the ROM or of memory.
 */

#include <SDL3/SDL.h>
#include <ultra64.h>

#include "port/os_host.h"
#include "test_support.h"

#define ROM_SIZE 64
// Bytes beside an array that a fill must leave alone
#define GUARD 0xA5

static u8 f_Rom[ROM_SIZE];

static void InsertNumberedRom(void) {
    int i;

    for (i = 0; i < ROM_SIZE; i++) {
        f_Rom[i] = (u8)(i + 1);
    }

    PcInsertCartridge(f_Rom, sizeof(f_Rom));
}

// Bytes are the ROM's from the offset on, and the bytes either side are
// untouched
static void TestFillsBytes(void) {
    u8 memory[1 + 4 + 1];
    OSPcRomArray array = {&memory[1], 4, sizeof(u8), 9};

    SDL_memset(memory, GUARD, sizeof(memory));
    osPcFillRomArray(&array);
    EXPECT(memory[0] == GUARD);
    EXPECT(memory[1] == 10 && memory[2] == 11 && memory[3] == 12 && memory[4] == 13);
    EXPECT(memory[5] == GUARD);
}

// Halves and words are the ROM's big-endian values
static void TestFillsHalvesAndWords(void) {
    u16 halves[3];
    u32 words[2];
    OSPcRomArray halfArray = {halves, sizeof(halves), sizeof(u16), 1};
    OSPcRomArray wordArray = {words, sizeof(words), sizeof(u32), 3};

    osPcFillRomArray(&halfArray);
    EXPECT(halves[0] == 0x0203 && halves[1] == 0x0405 && halves[2] == 0x0607);
    osPcFillRomArray(&wordArray);
    EXPECT(words[0] == 0x04050607 && words[1] == 0x08090A0B);
}

// An array may end at the ROM's last byte
static void TestFillsToRomEnd(void) {
    u16 halves[2];
    OSPcRomArray array = {halves, sizeof(halves), sizeof(u16), ROM_SIZE - sizeof(halves)};

    osPcFillRomArray(&array);
    EXPECT(halves[0] == 0x3D3E && halves[1] == 0x3F40);
}

// The array holds the host's values, which the renderer must not read in
// the ROM's order, even where the memory held data in that order before
static void TestLeavesHostOrder(void) {
    u16 halves[4] = {0};
    OSPcRomArray array = {halves, sizeof(halves), sizeof(u16), 0};

    osPcMarkRomOrder(halves, sizeof(halves));
    osPcFillRomArray(&array);
    EXPECT(!osPcIsRomOrder(&halves[0]));
    EXPECT(!osPcIsRomOrder((const u8 *)&halves[4] - 1));
}

int main(void) {
    InsertNumberedRom();
    TestFillsBytes();
    TestFillsHalvesAndWords();
    TestFillsToRomEnd();
    TestLeavesHostOrder();
    return ReportTestResults("rom_array_test");
}
