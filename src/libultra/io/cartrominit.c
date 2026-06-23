/*
 * cartrominit.c -- build and register the PI handle for the game cartridge.
 *
 * The cartridge lives on PI domain 1. The first call to osCartRomInit probes
 * the cartridge's own preferred bus timing (stored in the header word at its
 * base address), records it in a handle, and links that handle into the global
 * PI device table so later EPI DMA calls can drive the cartridge with correct
 * timing. Subsequent calls just hand back the already-built handle.
 */
#include "PRinternal/macros.h"
#include "PR/os_version.h"
#include "PR/os_internal.h"
#include "PR/R4300.h"
#include "PR/rcp.h"
#include "PRinternal/piint.h"

extern OSPiHandle __CartRomHandle;
extern int osCartRomInitFirst;

OSPiHandle* osCartRomInit(void) {
  u32 value = 0;
  u32 saveMask;
  register u32 stat;

  // Saved domain-1 bus-timing registers, restored after the probe read below.
  u32 latency;
  u32 pulse;
  u32 pageSize;
  u32 relDuration;

  // Serialize against other PI users for the whole handle setup.
  __osPiGetAccess();

  // After the first call the handle is fully built; return it unchanged.
  if (!osCartRomInitFirst) {
    __osPiRelAccess();
    return &__CartRomHandle;
  }
  osCartRomInitFirst = 0;

  // Fixed handle fields for a cartridge on domain 1.
  __CartRomHandle.type = DEVICE_TYPE_CART;
  __CartRomHandle.baseAddress = PHYS_TO_K1(PI_DOM1_ADDR2);
  __CartRomHandle.domain = PI_DOMAIN1;
  __CartRomHandle.speed = 0;
  bzero(&__CartRomHandle.transferInfo, sizeof(__OSTranxInfo));

  // The probe read needs the bus quiet, so wait out any in-flight IO.
  WAIT_ON_IOBUSY(stat);

  // Stash the current domain-1 timing, then force conservative "slow" timing so
  // the cartridge's own timing word reads back reliably.
  latency = IO_READ(PI_BSD_DOM1_LAT_REG);
  pageSize = IO_READ(PI_BSD_DOM1_PGS_REG);
  relDuration = IO_READ(PI_BSD_DOM1_RLS_REG);
  pulse = IO_READ(PI_BSD_DOM1_PWD_REG);
  IO_WRITE(PI_BSD_DOM1_LAT_REG, 0xFF);
  IO_WRITE(PI_BSD_DOM1_PGS_REG, 0);
  IO_WRITE(PI_BSD_DOM1_RLS_REG, 3);
  IO_WRITE(PI_BSD_DOM1_PWD_REG, 0xFF);

  // Read the cartridge header word and unpack the four timing fields it encodes
  // into the handle: latency, page size, release duration, and pulse width.
  value = IO_READ(__CartRomHandle.baseAddress);
  __CartRomHandle.latency = value & 0xFF;
  __CartRomHandle.pageSize = (value >> 0x10) & 0xF;
  __CartRomHandle.relDuration = (value >> 0x14) & 0xF;
  __CartRomHandle.pulse = (value >> 8) & 0xFF;

  // Restore the timing registers we clobbered for the probe.
  IO_WRITE(PI_BSD_DOM1_LAT_REG, latency);
  IO_WRITE(PI_BSD_DOM1_PGS_REG, pageSize);
  IO_WRITE(PI_BSD_DOM1_RLS_REG, relDuration);
  IO_WRITE(PI_BSD_DOM1_PWD_REG, pulse);

  // Prepend the handle to the global PI device list under an interrupt lock so
  // the list stays consistent against the PI interrupt handler.
  saveMask = __osDisableInt();
  __CartRomHandle.next = __osPiTable;
  __osPiTable = &__CartRomHandle;
  __osRestoreInt(saveMask);

  __osPiRelAccess();
  return &__CartRomHandle;
}
