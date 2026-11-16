/*
 * Tests of the TLB's mappings of virtual addresses onto the host's memory.
 */

#include <SDL3/SDL.h>
#include <ultra64.h>

#include "test_support.h"

#define PAGE_SIZE 0x1000
#define VIRTUAL_START 0x02000000u

// Two pages of the host's memory, outside osPcRdram, as a heap's block is
static _Alignas(PAGE_SIZE) u8 f_Pages[2 * PAGE_SIZE];

// A mapped page translates to the host's memory at its physical address, a
// host's address as wide as any, and osVirtualToPhysical follows it
static void TestMapsHostAddresses(void) {
    osMapTLB(
        0,
        OS_PM_4K,
        (void *)(uintptr_t)VIRTUAL_START,
        osVirtualToPhysical(f_Pages),
        osVirtualToPhysical(&f_Pages[PAGE_SIZE]),
        -1
    );
    EXPECT(osPcTranslate(VIRTUAL_START) == f_Pages);
    EXPECT(osPcTranslate(VIRTUAL_START + PAGE_SIZE + 0x10) == &f_Pages[PAGE_SIZE + 0x10]);
    EXPECT(osPcTranslate(VIRTUAL_START + (2 * PAGE_SIZE)) == NULL);
    EXPECT(
        osVirtualToPhysical((void *)(uintptr_t)(VIRTUAL_START + 0x20)) == (uintptr_t)&f_Pages[0x20]
    );
    osUnmapTLBAll();
    EXPECT(osPcTranslate(VIRTUAL_START) == NULL);
}

// A physical address of -1 leaves its page unmapped, as SetMultiTlb's lone
// odd page does
static void TestLeavesPageUnmapped(void) {
    osMapTLB(3, OS_PM_4K, (void *)(uintptr_t)VIRTUAL_START, -1, osVirtualToPhysical(f_Pages), -1);
    EXPECT(osPcTranslate(VIRTUAL_START) == NULL);
    EXPECT(osPcTranslate(VIRTUAL_START + PAGE_SIZE) == f_Pages);
    osUnmapTLB(3);
    EXPECT(osPcTranslate(VIRTUAL_START + PAGE_SIZE) == NULL);
}

int main(void) {
    TestMapsHostAddresses();
    TestLeavesPageUnmapped();
    return ReportTestResults("memory_test");
}
