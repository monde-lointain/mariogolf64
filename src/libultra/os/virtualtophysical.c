/*
 * Virtual-to-physical address translation.
 *
 * Maps a CPU virtual address to its physical (RDRAM/bus) address. The two
 * unmapped kernel windows (KSEG0 cached, KSEG1 uncached) translate by a simple
 * fixed offset; anything else is a mapped region and must be resolved through
 * the TLB.
 */

#include "PR/os_internal.h"
#include "PR/R4300.h"
#include "osint.h"

u32 osVirtualToPhysical(void* addr) {
  if (IS_KSEG0(addr)) {
    return K0_TO_PHYS(addr);
  } else if (IS_KSEG1(addr)) {
    return K1_TO_PHYS(addr);
  } else {
    return __osProbeTLB(addr);
  }
}
