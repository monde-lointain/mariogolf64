/*
 * gbpakinit.c -- power up and initialize a Game Boy Transfer Pak.
 *
 * Brings the pak through a power-off then power-on cycle (writing then reading
 * back the power block to confirm each step took), waits out the cartridge's
 * boot time, and primes the OSPfs handle for subsequent Transfer Pak calls.
 */
#include "PR/os_internal.h"
#include "controller.h"
#include "controller_gbpak.h"

s32 osGbpakInit(OSMesgQueue* mq, OSPfs* pfs, int channel) {
  int i;
  s32 ret;
  u8 temp[BLOCKSIZE];
  pfs->status = 0;

  /* Power the pak OFF first to force a known starting state. Retry once if the
   * controller reports a new pack mid-write. */
  for (i = 0; i < BLOCKSIZE; temp[i++] = GB_POWER_OFF) {
    ;
  }
  ret = __osContRamWrite(mq, channel, CONT_BLOCK_GB_POWER, temp, FALSE);
  if (ret == PFS_ERR_NEW_PACK) {
    ret = __osContRamWrite(mq, channel, CONT_BLOCK_GB_POWER, temp, FALSE);
  }
  if (ret != 0) {
    return ret;
  }

  /* Read the power block back; a pak that is really there echoes the OFF state
   * we just wrote. If it still reads ON, no Transfer Pak is connected. */
  ret = __osContRamRead(mq, channel, CONT_BLOCK_GB_POWER, temp);
  if (ret == PFS_ERR_NEW_PACK) {
    ret = PFS_ERR_CONTRFAIL;
  }
  if (ret != 0) {
    return ret;
  } else {
    if (temp[BLOCKSIZE - 1] == GB_POWER_OFF) {
      return PFS_ERR_DEVICE;
    }
  }

  /* Now power the pak ON and confirm the read-back, same handshake. */
  for (i = 0; i < BLOCKSIZE; temp[i++] = GB_POWER_ON) {
    ;
  }
  ret = __osContRamWrite(mq, channel, CONT_BLOCK_GB_POWER, temp, FALSE);
  if (ret == PFS_ERR_NEW_PACK) {
    ret = PFS_ERR_CONTRFAIL;
  }
  if (ret != 0) {
    return ret;
  }
  ret = __osContRamRead(mq, channel, CONT_BLOCK_GB_POWER, temp);
  if (ret == PFS_ERR_NEW_PACK) {
    ret = PFS_ERR_CONTRFAIL;
  }
  if (ret != 0) {
    return ret;
  } else {
    if (temp[BLOCKSIZE - 1] != GB_POWER_ON) {
      return PFS_ERR_DEVICE;
    }
  }

  /* Let the just-powered Game Boy cartridge finish booting before any access:
   * arm a one-shot timer and block on it. */
  ERRCK(__osPfsGetStatus(mq, channel));
  osCreateMesgQueue(&__osGbpakTimerQ, &__osGbpakTimerMsg, 1);
  osSetTimer(&__osGbpakTimer, 9000000, 0, &__osGbpakTimerQ, &__osGbpakTimerMsg);
  osRecvMesg(&__osGbpakTimerQ, NULL, OS_MESG_BLOCK);

  /* Mark the handle initialized and seed the cartridge-geometry fields with
   * sentinels (0xFF = unknown until osGbpakReadId fills them in). */
  pfs->queue = mq;
  pfs->status = PFS_GBPAK_INITIALIZED;
  pfs->channel = channel;
  pfs->activebank = 0x84;
  pfs->banks = 0xFF;
  pfs->version = 0xFF;
  pfs->dir_size = 0xFF;
  return 0;
}
