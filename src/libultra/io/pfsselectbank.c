/*
 * pfsselectbank.c -- Controller Pak bank selection.
 *
 * A Controller Pak addresses its memory in banks; the active bank is chosen by
 * writing a probe block whose every byte holds the desired bank number. This
 * helper performs that write and records the new bank in the pfs handle.
 */
#include "PR/os_internal.h"
#include "controller.h"

#if BUILD_VERSION >= VERSION_J

/* Select `bank` on the pak behind `pfs` by writing the detect block filled with
 * the bank number. On success the handle's active bank is updated; the raw
 * write status (0 = ok) is returned. */
s32 __osPfsSelectBank(OSPfs* pfs, u8 bank) {
  u8 temp[BLOCKSIZE];
  int i;
  s32 ret = 0;

  for (i = 0; i < BLOCKSIZE; i++) {
    temp[i] = bank;
  }
  ret = __osContRamWrite(pfs->queue, pfs->channel, CONT_BLOCK_DETECT, temp,
                         FALSE);
  if (ret == 0) {
    pfs->activebank = bank;
  }
  return ret;
}
#endif
