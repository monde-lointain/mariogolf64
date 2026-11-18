/*
 * What the native port adds to libultra's interface: the memory that stands
 * in for the N64's RDRAM, addresses as wide as the host's, and the few
 * routines that the game's code calls only in a native build.
 *
 * ultra64.h includes this after libultra's headers, so that its macros
 * replace theirs.
 */

#ifndef _OS_PC_H_
#define _OS_PC_H_

#include <PR/ultratypes.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The N64's memory with the Expansion Pak: the game places its heaps, frame
   buffers and some buffers at fixed addresses in it, which a native build
   finds at the same offsets in this array. */
#define PC_RDRAM_SIZE 0x800000
#define PC_RDRAM_BASE 0x80000000u
extern u8 osPcRdram[PC_RDRAM_SIZE];

/* The memory at an address of the N64's KSEG0, an address constant */
#define PcRdramAddress(address) (&osPcRdram[(u32)(address) - PC_RDRAM_BASE])

/* The RCP reads the host's memory, so a physical address is the host's
   address. Virtual addresses that a mapping set by osPcMapRange covers are
   translated through it, as the TLB did. */
uintptr_t osPcVirtualToPhysical(const void *address);
#undef osVirtualToPhysical
#define osVirtualToPhysical(address) osPcVirtualToPhysical(address)

#undef OS_K0_TO_PHYSICAL
#undef OS_K1_TO_PHYSICAL
#undef OS_PHYSICAL_TO_K0
#undef OS_PHYSICAL_TO_K1
#define OS_K0_TO_PHYSICAL(address) ((uintptr_t)(address))
#define OS_K1_TO_PHYSICAL(address) ((uintptr_t)(address))
#define OS_PHYSICAL_TO_K0(address) ((void *)(uintptr_t)(address))
#define OS_PHYSICAL_TO_K1(address) ((void *)(uintptr_t)(address))

/* A segmented address that holds a host's address: the RSP adds the
   segment's base to it, as the N64's adds the base to the low 24 bits of a
   segmented address. The segment goes in the top byte, which no host's
   address in user space uses. */
#define PC_SEGMENT_SHIFT 56
#define osPcSegmentAddress(segment, address) \
    ((void *)((uintptr_t)(address) + ((uintptr_t)(segment) << PC_SEGMENT_SHIFT)))

/* Maps size bytes of virtual addresses from virtualAddress onto the host's
   memory at hostAddress, as osMapTLB mapped them onto physical memory, in
   place of any mapping there before. */
void osPcMapRange(u32 virtualAddress, void *hostAddress, u32 size);

/* osMapTLB with the host's physical addresses: maps a TLB entry's even and
   odd pages onto the host's memory at evenPhysical and oddPhysical, which
   osVirtualToPhysical gives, or leaves a page unmapped where its address is
   PC_UNMAPPED_PAGE, as the N64's -1 did. */
#define PC_UNMAPPED_PAGE ((uintptr_t)-1)
void osPcMapTLB(
    s32 index,
    u32 pageMask,
    void *virtualAddress,
    uintptr_t evenPhysical,
    uintptr_t oddPhysical,
    s32 asid
);
#define osMapTLB(index, pageMask, virtualAddress, evenPhysical, oddPhysical, asid) \
    osPcMapTLB((index), (pageMask), (virtualAddress), (evenPhysical), (oddPhysical), (asid))

/* Removes every mapping, as osUnmapTLBAll does */
void osPcUnmapAll(void);

/* The host's address of a virtual address that a mapping covers, or NULL */
void *osPcTranslate(u32 virtualAddress);

/* Marks size bytes from start as holding data in the ROM's big-endian
   order, as the game loaded it, for the renderer to read their display
   lists, vertices, matrices and textures so. It reads all other memory in
   the host's order. Every read from the cartridge marks the bytes that it
   fills; code that writes values of its own over them unmarks them. Marks
   are exact to the byte. */
void osPcMarkRomOrder(const void *start, u32 size);
void osPcUnmarkRomOrder(const void *start, u32 size);
/* Whether the byte at address lies in a range marked as in the ROM's
   order */
int osPcIsRomOrder(const void *address);

/* Puts the variables of the code segment with the number that
   rom_file_offsets.h gives it back to their values at start-up, as reading
   the segment from ROM and clearing its bss did on the N64. */
void osPcResetSegment(u32 segmentNumber);

/* Blocks the calling thread until the next interrupt, for a thread that
   would otherwise spin until an interrupt or another thread changes
   something */
void osPcWaitForInterrupt(void);

/* Set when an interrupt has made a thread of higher priority than the
   running one runnable. The running thread gives up the CPU at its next
   libultra call, or at a long loop's osPcReachPreemptionPoint. The host
   thread that raises the interrupt sets it while the running thread reads
   it, so both go through atomics; osPcPreempt then takes the scheduler's
   lock, which orders everything else. */
extern int osPcPreemptPending;
void osPcPreempt(void);
#define osPcIsPreemptPending() __atomic_load_n(&osPcPreemptPending, __ATOMIC_RELAXED)
#define osPcReachPreemptionPoint() (osPcIsPreemptPending() ? osPcPreempt() : (void)0)

#ifdef __cplusplus
}
#endif

#endif
