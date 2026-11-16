/*
 * Tests of the registry of memory that holds data in the ROM's byte order.
 */

#include <SDL3/SDL.h>
#include <ultra64.h>

#include "port/os_host.h"
#include "test_support.h"

static u8 f_Memory[256];

// A marked range covers its first byte to its last, and nothing beyond
static void TestMarkedRangeBounds(void) {
    osPcMarkRomOrder(&f_Memory[16], 32);
    EXPECT(!osPcIsRomOrder(&f_Memory[15]));
    EXPECT(osPcIsRomOrder(&f_Memory[16]));
    EXPECT(osPcIsRomOrder(&f_Memory[47]));
    EXPECT(!osPcIsRomOrder(&f_Memory[48]));
    osPcUnmarkRomOrder(f_Memory, sizeof(f_Memory));
    EXPECT(!osPcIsRomOrder(&f_Memory[16]));
}

// Marks are exact to the byte: a load over part of an older one leaves the
// rest of the older one's mark, and an unmark clears only its own bytes
static void TestMarksAreExact(void) {
    osPcMarkRomOrder(&f_Memory[0], 64);
    osPcUnmarkRomOrder(&f_Memory[32], 64);
    EXPECT(osPcIsRomOrder(&f_Memory[31]));
    EXPECT(!osPcIsRomOrder(&f_Memory[32]));
    osPcMarkRomOrder(&f_Memory[40], 2);
    EXPECT(!osPcIsRomOrder(&f_Memory[39]));
    EXPECT(osPcIsRomOrder(&f_Memory[40]) && osPcIsRomOrder(&f_Memory[41]));
    EXPECT(!osPcIsRomOrder(&f_Memory[42]));
    osPcUnmarkRomOrder(f_Memory, sizeof(f_Memory));
}

// A range may span many pages and start and end within them, as a load
// into a large buffer does
static void TestRangesAcrossPages(void) {
    u8 *buffer = SDL_malloc(6 * 4096);
    // Three bytes before a page's end
    u8 *start = (u8 *)((((uintptr_t)buffer + 4095) & ~(uintptr_t)4095) + 4096 - 3);
    u32 size = 2 * 4096 + 10;

    osPcMarkRomOrder(start, size);
    EXPECT(!osPcIsRomOrder(start - 1));
    EXPECT(osPcIsRomOrder(start));
    EXPECT(osPcIsRomOrder(start + 2) && osPcIsRomOrder(start + 3));
    EXPECT(osPcIsRomOrder(start + 4096 + 2) && osPcIsRomOrder(start + 4096 + 3));
    EXPECT(osPcIsRomOrder(start + size - 1));
    EXPECT(!osPcIsRomOrder(start + size));
    osPcUnmarkRomOrder(buffer, 6 * 4096);
    EXPECT(!osPcIsRomOrder(start + 4096));
    SDL_free(buffer);
}

// The registry grows as many pages are marked, and still finds each
static void TestManyPages(void) {
    enum {
        PAGE_COUNT = 3000
    };

    u8 *buffer = SDL_malloc(PAGE_COUNT * 4096);
    int page;
    int misses = 0;

    for (page = 0; page < PAGE_COUNT; page += 2) {
        osPcMarkRomOrder(buffer + (page * 4096) + 100, 1);
    }

    for (page = 0; page < PAGE_COUNT; page++) {
        int isMarked = osPcIsRomOrder(buffer + (page * 4096) + 100);

        misses += isMarked != (page % 2 == 0);
    }

    EXPECT(misses == 0);
    osPcUnmarkRomOrder(buffer, PAGE_COUNT * 4096);
    SDL_free(buffer);
}

// A read from the cartridge marks what it fills
static void TestCartridgeReadsMark(void) {
    static const u8 rom[64] = {0x80, 0x37, 0x12, 0x40};

    PcInsertCartridge(rom, sizeof(rom));
    osPcUnmarkRomOrder(f_Memory, sizeof(f_Memory));
    PcReadCartridge(0, &f_Memory[8], 16);
    EXPECT(f_Memory[8] == 0x80);
    EXPECT(!osPcIsRomOrder(&f_Memory[7]));
    EXPECT(osPcIsRomOrder(&f_Memory[8]) && osPcIsRomOrder(&f_Memory[23]));
    EXPECT(!osPcIsRomOrder(&f_Memory[24]));
    osPcUnmarkRomOrder(f_Memory, sizeof(f_Memory));
}

// A segment tag sits above any address and leaves the address below it
static void TestSegmentAddress(void) {
    uintptr_t tagged = (uintptr_t)osPcSegmentAddress(6, 0x1234);

    EXPECT((tagged >> PC_SEGMENT_SHIFT) == 6);
    EXPECT((tagged & (((uintptr_t)1 << PC_SEGMENT_SHIFT) - 1)) == 0x1234);
}

int main(void) {
    TestMarkedRangeBounds();
    TestMarksAreExact();
    TestRangesAcrossPages();
    TestManyPages();
    TestCartridgeReadsMark();
    TestSegmentAddress();
    return ReportTestResults("rom_order_test");
}
