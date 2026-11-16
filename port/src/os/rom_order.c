/*
 * The registry of memory that holds data in the ROM's byte order.
 *
 * Every read from the cartridge marks the bytes that it fills, so the
 * renderer can tell the textures, display lists, vertices and matrices that
 * the game loaded from ROM from those that its code built in the host's
 * order. Lookups come once per vertex load, matrix and texture image, from
 * any host address: RDRAM, the game's variables or a thread's stack.
 *
 * The registry keeps a bitmap of each host page that ever held a marked
 * byte, one bit a byte, found through an open-addressed hash table of the
 * pages' numbers. A lookup is a hash, a probe or two and a bit test, and
 * marks are exact to the byte, so a load that overlaps part of an older one
 * leaves the rest of the older one's mark. Pages stay in the table once
 * added; the game loads into the same few buffers again and again.
 *
 * Only one of the game's threads runs at a time, and the renderer runs on
 * the graphics thread, so the registry needs no lock.
 */

#include <SDL3/SDL.h>

#include "os_internal.h"
#include "port/os_host.h"

#define PAGE_SHIFT 12
#define PAGE_SIZE ((uintptr_t)1 << PAGE_SHIFT)
#define PAGE_BITMAP_WORDS (PAGE_SIZE / 64)
#define INITIAL_SLOT_COUNT 1024
// The table grows before more than half its slots are used, which keeps
// probe chains short
#define MAX_LOAD_NUMERATOR 1
#define MAX_LOAD_DENOMINATOR 2

typedef struct PageBitmap {
    uint64_t bits[PAGE_BITMAP_WORDS];
} PageBitmap;

typedef struct PageSlot {
    uintptr_t pageNumber;
    PageBitmap *bitmap; // NULL for an empty slot
} PageSlot;

static PageSlot *f_Slots;
static size_t f_SlotCount; // A power of two, or 0 before the first mark
static size_t f_UsedSlotCount;

// Fibonacci hashing spreads the consecutive pages of a buffer
static size_t HashPage(uintptr_t pageNumber, size_t slotCount) {
    return (size_t)(((uint64_t)pageNumber * 0x9E3779B97F4A7C15ull) >> 20) & (slotCount - 1);
}

static PageBitmap *FindPageBitmap(uintptr_t pageNumber) {
    size_t index;

    if (f_SlotCount == 0) {
        return NULL;
    }

    for (index = HashPage(pageNumber, f_SlotCount);; index = (index + 1) & (f_SlotCount - 1)) {
        const PageSlot *slot = &f_Slots[index];

        if (slot->bitmap == NULL || slot->pageNumber == pageNumber) {
            return slot->bitmap;
        }
    }
}

static void InsertSlot(
    PageSlot *slots,
    size_t slotCount,
    uintptr_t pageNumber,
    PageBitmap *bitmap
) {
    size_t index = HashPage(pageNumber, slotCount);

    while (slots[index].bitmap != NULL) {
        index = (index + 1) & (slotCount - 1);
    }

    slots[index].pageNumber = pageNumber;
    slots[index].bitmap = bitmap;
}

static void GrowSlots(void) {
    size_t newCount = f_SlotCount == 0 ? INITIAL_SLOT_COUNT : f_SlotCount * 2;
    PageSlot *newSlots = SDL_calloc(newCount, sizeof(*newSlots));
    size_t i;

    if (newSlots == NULL) {
        PcFatalError("Out of memory for the ROM's byte order's registry");
    }

    for (i = 0; i < f_SlotCount; i++) {
        if (f_Slots[i].bitmap != NULL) {
            InsertSlot(newSlots, newCount, f_Slots[i].pageNumber, f_Slots[i].bitmap);
        }
    }

    SDL_free(f_Slots);
    f_Slots = newSlots;
    f_SlotCount = newCount;
}

static PageBitmap *AddPageBitmap(uintptr_t pageNumber) {
    PageBitmap *bitmap = FindPageBitmap(pageNumber);

    if (bitmap != NULL) {
        return bitmap;
    }

    if ((f_UsedSlotCount + 1) * MAX_LOAD_DENOMINATOR > f_SlotCount * MAX_LOAD_NUMERATOR) {
        GrowSlots();
    }

    bitmap = SDL_calloc(1, sizeof(*bitmap));
    if (bitmap == NULL) {
        PcFatalError("Out of memory for the ROM's byte order's registry");
    }

    InsertSlot(f_Slots, f_SlotCount, pageNumber, bitmap);
    f_UsedSlotCount++;
    return bitmap;
}

// Sets or clears the bits of bytes first to last of one page
static void SetPageBits(PageBitmap *bitmap, size_t first, size_t last, bool isMarked) {
    size_t firstWord = first / 64;
    size_t lastWord = last / 64;
    size_t word;

    for (word = firstWord; word <= lastWord; word++) {
        size_t low = word == firstWord ? first % 64 : 0;
        size_t high = word == lastWord ? last % 64 : 63;
        uint64_t mask = (~(uint64_t)0 >> (63 - high)) & (~(uint64_t)0 << low);

        if (isMarked) {
            bitmap->bits[word] |= mask;
        }
        else {
            bitmap->bits[word] &= ~mask;
        }
    }
}

static void SetRangeOrder(const void *start, u32 size, bool isMarked) {
    uintptr_t first = (uintptr_t)start;
    uintptr_t last = first + size - 1;
    uintptr_t page;

    if (size == 0) {
        return;
    }

    SDL_assert(first <= last);
    for (page = first >> PAGE_SHIFT; page <= last >> PAGE_SHIFT; page++) {
        uintptr_t pageStart = page << PAGE_SHIFT;
        size_t low = page == first >> PAGE_SHIFT ? first - pageStart : 0;
        size_t high = page == last >> PAGE_SHIFT ? last - pageStart : PAGE_SIZE - 1;
        PageBitmap *bitmap = isMarked ? AddPageBitmap(page) : FindPageBitmap(page);

        if (bitmap != NULL) {
            SetPageBits(bitmap, low, high, isMarked);
        }
    }
}

void osPcMarkRomOrder(const void *start, u32 size) {
    SDL_assert(start != NULL || size == 0);
    SetRangeOrder(start, size, true);
}

void osPcUnmarkRomOrder(const void *start, u32 size) {
    SetRangeOrder(start, size, false);
}

int osPcIsRomOrder(const void *address) {
    uintptr_t byte = (uintptr_t)address;
    const PageBitmap *bitmap = FindPageBitmap(byte >> PAGE_SHIFT);
    size_t offset = byte & (PAGE_SIZE - 1);

    return bitmap != NULL && ((bitmap->bits[offset / 64] >> (offset % 64)) & 1) != 0;
}
