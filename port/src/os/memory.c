/*
 * The N64's memory as the host holds it: osPcRdram for the addresses that
 * the game fixes, the TLB's mappings as ranges of the host's memory, the
 * caches, which the host doesn't need, and the overlays' variables.
 */

#include <SDL3/SDL.h>

#include "os_internal.h"
#include "port/os_host.h"
#include "port/overlay_sections.h"

// The TLB's 32 entries, each of which maps an even and an odd page
#define TLB_ENTRY_COUNT 32
#define TLB_PAGE_COUNT (TLB_ENTRY_COUNT * 2)
// The ranges that osPcMapRange keeps, beyond the TLB's
#define MAPPED_RANGE_COUNT 8

typedef struct MappedRange {
    u32 virtualStart;
    u32 size; // 0 for an unused range
    u8 *host;
} MappedRange;

// Aligned for the textures and matrices that the game places in it
_Alignas(64) u8 osPcRdram[PC_RDRAM_SIZE];

static MappedRange f_TlbPages[TLB_PAGE_COUNT];
static MappedRange f_MappedRanges[MAPPED_RANGE_COUNT];

// AddressSanitizer pads each variable with poisoned bytes, which the
// copies of an overlay's sections cover, as they take the sections whole
#if defined(__SANITIZE_ADDRESS__)
#    define IS_ADDRESS_SANITIZED 1
#elif defined(__has_feature)
#    if __has_feature(address_sanitizer)
#        define IS_ADDRESS_SANITIZED 1
#    endif
#endif

// Each overlay's variables as they were at start-up
static u8 *f_OverlaySnapshots;
static const OverlaySections *f_OverlaySections;
static int f_OverlayCount;

static void *TranslateInRanges(const MappedRange *ranges, int count, u32 virtualAddress) {
    int i;

    for (i = 0; i < count; i++) {
        u32 offset = virtualAddress - ranges[i].virtualStart;

        if (ranges[i].size != 0 && offset < ranges[i].size) {
            return ranges[i].host + offset;
        }
    }

    return NULL;
}

void *osPcTranslate(u32 virtualAddress) {
    void *host = TranslateInRanges(f_MappedRanges, MAPPED_RANGE_COUNT, virtualAddress);

    if (host == NULL) {
        host = TranslateInRanges(f_TlbPages, TLB_PAGE_COUNT, virtualAddress);
    }

    return host;
}

void osPcMapRange(u32 virtualAddress, void *hostAddress, u32 size) {
    int freeIndex = -1;
    int i;

    for (i = 0; i < MAPPED_RANGE_COUNT; i++) {
        if (f_MappedRanges[i].size != 0 && f_MappedRanges[i].virtualStart == virtualAddress) {
            freeIndex = i;
            break;
        }

        if (f_MappedRanges[i].size == 0 && freeIndex < 0) {
            freeIndex = i;
        }
    }

    if (freeIndex < 0) {
        PcFatalError("More than %d mapped ranges", MAPPED_RANGE_COUNT);
    }

    f_MappedRanges[freeIndex].virtualStart = virtualAddress;
    f_MappedRanges[freeIndex].size = size;
    f_MappedRanges[freeIndex].host = hostAddress;
}

void osPcUnmapAll(void) {
    SDL_memset(f_MappedRanges, 0, sizeof(f_MappedRanges));
    SDL_memset(f_TlbPages, 0, sizeof(f_TlbPages));
}

uintptr_t osPcVirtualToPhysical(const void *address) {
    uintptr_t number = (uintptr_t)address;

    // Only an address that fits in 32 bits can be one that the TLB maps
    if (number <= 0xFFFFFFFFu) {
        void *host = osPcTranslate((u32)number);

        if (host != NULL) {
            return (uintptr_t)host;
        }
    }

    return number;
}

// The page sizes of the OSPageMask values, from 4 KiB to 16 MiB in steps of
// four
static u32 PageSize(OSPageMask pageMask) {
    switch (pageMask) {
        case OS_PM_4K:
            return 4 * 1024;
        case OS_PM_16K:
            return 16 * 1024;
        case OS_PM_64K:
            return 64 * 1024;
        case OS_PM_256K:
            return 256 * 1024;
        case OS_PM_1M:
            return 1024 * 1024;
        case OS_PM_4M:
            return 4 * 1024 * 1024;
        case OS_PM_16M:
            return 16 * 1024 * 1024;
        default:
            PcFatalError("Invalid TLB page mask %#x", (unsigned)pageMask);
    }
}

// A page of physical memory, which is the host's memory at its address
static void MapTlbPage(MappedRange *page, u32 virtualStart, u32 pageSize, uintptr_t physical) {
    if (physical == PC_UNMAPPED_PAGE) {
        page->size = 0;
        return;
    }

    page->virtualStart = virtualStart;
    page->size = pageSize;
    page->host = (u8 *)physical;
}

void osPcMapTLB(
    s32 index,
    u32 pageMask,
    void *virtualAddress,
    uintptr_t evenPhysical,
    uintptr_t oddPhysical,
    s32 asid
) {
    u32 pageSize = PageSize(pageMask);
    u32 virtualStart = (u32)(uintptr_t)virtualAddress;

    (void)asid;
    SDL_assert_always(index >= 0 && index < TLB_ENTRY_COUNT);
    MapTlbPage(&f_TlbPages[index * 2], virtualStart, pageSize, evenPhysical);
    MapTlbPage(&f_TlbPages[index * 2 + 1], virtualStart + pageSize, pageSize, oddPhysical);
}

void osUnmapTLB(s32 index) {
    SDL_assert_always(index >= 0 && index < TLB_ENTRY_COUNT);
    f_TlbPages[index * 2].size = 0;
    f_TlbPages[index * 2 + 1].size = 0;
}

void osUnmapTLBAll(void) {
    SDL_memset(f_TlbPages, 0, sizeof(f_TlbPages));
}

// The host's caches are coherent, and its code isn't written at run time
void osInvalDCache(void *address, s32 size) {
    (void)address;
    (void)size;
}

void osInvalICache(void *address, s32 size) {
    (void)address;
    (void)size;
}

void osWritebackDCache(void *address, s32 size) {
    (void)address;
    (void)size;
}

void osWritebackDCacheAll(void) {
}

#ifdef IS_ADDRESS_SANITIZED
// Copies a section's bytes unchecked, padding included. Byte by byte, as
// memcpy is checked whatever its caller; neither GCC nor Clang makes this
// loop a call of it, at any level.
__attribute__((no_sanitize_address)) static void CopySectionBytes(
    void *to,
    const void *from,
    size_t size
) {
    u8 *out = to;
    const u8 *in = from;
    size_t i;

    for (i = 0; i < size; i++) {
        out[i] = in[i];
    }
}
#else
static void CopySectionBytes(void *to, const void *from, size_t size) {
    SDL_memcpy(to, from, size);
}
#endif

// A section's bounds are the linker's, outside any one of C's objects, which
// C leaves the difference of undefined: their addresses' difference isn't
static size_t MeasureSection(const unsigned char *start, const unsigned char *end) {
    return (uintptr_t)end - (uintptr_t)start;
}

static size_t MeasureDataSection(const OverlaySections *sections) {
    return MeasureSection(sections->dataStart, sections->dataEnd);
}

static size_t MeasureBssSection(const OverlaySections *sections) {
    return MeasureSection(sections->bssStart, sections->bssEnd);
}

// The snapshot holds the bss as well as the data: what start-up has
// written into either before it, such as the arrays that the port fills
// from the ROM, a compiler may put in either, where the arrays' initializers
// hold only zeros
void PcInitSegments(void) {
    size_t snapshotSize = 0;
    u8 *cursor;
    int i;

    f_OverlaySections = GetOverlaySections(&f_OverlayCount);
    for (i = 0; i < f_OverlayCount; i++) {
        snapshotSize +=
            MeasureDataSection(&f_OverlaySections[i]) + MeasureBssSection(&f_OverlaySections[i]);
    }

    f_OverlaySnapshots = SDL_malloc(snapshotSize + 1);
    if (f_OverlaySnapshots == NULL) {
        PcFatalError("Can't keep the overlays' variables");
    }

    cursor = f_OverlaySnapshots;
    for (i = 0; i < f_OverlayCount; i++) {
        const OverlaySections *sections = &f_OverlaySections[i];

        CopySectionBytes(cursor, sections->dataStart, MeasureDataSection(sections));
        cursor += MeasureDataSection(sections);
        CopySectionBytes(cursor, sections->bssStart, MeasureBssSection(sections));
        cursor += MeasureBssSection(sections);
    }
}

void osPcResetSegment(u32 segmentNumber) {
    const u8 *snapshot = f_OverlaySnapshots;
    int i;

    for (i = 0; i < f_OverlayCount; i++) {
        const OverlaySections *sections = &f_OverlaySections[i];

        if (sections->segmentNumber == segmentNumber) {
            CopySectionBytes(sections->dataStart, snapshot, MeasureDataSection(sections));
            CopySectionBytes(
                sections->bssStart,
                snapshot + MeasureDataSection(sections),
                MeasureBssSection(sections)
            );
            return;
        }

        snapshot += MeasureDataSection(sections) + MeasureBssSection(sections);
    }

    // The main segment and the hole editor are never reloaded, and have no
    // sections of their own
}
