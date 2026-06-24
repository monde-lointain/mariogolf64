/*
 * Set up the PI handle used to DMA the cartridge's battery-backed SRAM, filling
 * in the bus address and the device's access-timing parameters and linking it
 * into the OS so EPI transfers can reach it.
 */

#include <nusys.h>

// SRAM lives on PI domain 2 at this physical base.
#define SRAM_START_ADDR 0x08000000
#define SRAM_SIZE 0x8000

// PI bus access timing for the SRAM device (latency / pulse / page-size /
// release-duration, in PI clock terms). These are the standard values for the
// cartridge SRAM part.
#define SRAM_LATENCY 0x5
#define SRAM_PULSE 0x0c
#define SRAM_PAGE_SIZE 0xd
#define SRAM_REL_DURATION 0x2

extern OSPiHandle SramHandle;

/*
 * Populate and register SramHandle on first use; later calls are no-ops once
 * the base address shows it is already initialized.
 */
void nuPiInitSram(void) {
  if (SramHandle.baseAddress == PHYS_TO_K1(SRAM_START_ADDR)) {
    return;
  }

  // Device identity and the K1 (uncached) view of its bus address.
  SramHandle.type = DEVICE_TYPE_SRAM;
  SramHandle.baseAddress = PHYS_TO_K1(SRAM_START_ADDR);

  // Program the domain-2 access timing.
  SramHandle.latency = (u8)SRAM_LATENCY;
  SramHandle.pulse = (u8)SRAM_PULSE;
  SramHandle.pageSize = (u8)SRAM_PAGE_SIZE;
  SramHandle.relDuration = (u8)SRAM_REL_DURATION;
  SramHandle.domain = PI_DOMAIN2;
  SramHandle.speed = 0;

  // Clear the per-transfer scratch state, then link the handle into the OS PI
  // chain and publish it for nuPiReadWriteSram.
  bzero((void*)&(SramHandle.transferInfo), sizeof(SramHandle.transferInfo));
  osEPiLinkHandle(&SramHandle);
  nuPiSramHandle = &SramHandle;
}
