/*
 * gbpakgetstatus.c -- read the Game Boy Transfer Pak power/insertion status.
 *
 * The Transfer Pak exposes its state through controller-pak RAM blocks. This
 * coalesces the status block into a single flag byte and maps the hardware
 * conditions (no cartridge, freshly reinserted cartridge, controller failure)
 * onto the pfs error codes.
 */
#include "PR/os_internal.h"
#include "controller.h"

s32 osGbpakGetStatus(OSPfs* pfs, u8* status) {
  s32 ret;
  s32 i;
  u32 temp[BLOCKSIZE / sizeof(u32)];

  /* If the pak reports it lost power (or a new pack was hot-swapped), power it
   * back up before trusting any status read. */
  ret =
      __osContRamRead(pfs->queue, pfs->channel, CONT_BLOCK_GB_POWER, (u8*)temp);
  if ((ret == PFS_ERR_NEW_PACK) ||
      (((u8*)temp)[BLOCKSIZE - 1] != GB_POWER_ON)) {
    ERRCK(osGbpakInit(pfs->queue, pfs, pfs->channel));
  }

  ret = __osContRamRead(pfs->queue, pfs->channel, CONT_BLOCK_GB_STATUS,
                        (u8*)temp);
  if (ret == 0) {
    ERRCK(__osPfsGetStatus(pfs->queue, pfs->channel));

    /* OR every byte of the block together: the meaningful status bits can land
     * in any byte, so fold them into one, keep only the cartridge-pull and
     * reset-detect bits, then add back the last byte (the live power/insertion
     * flags). */
    *status = ((u8*)temp)[0];
    for (i = 1; i < BLOCKSIZE; i++) {
      *status |= ((u8*)temp)[i];
    }
    *status &= (OS_GBPAK_GBCART_PULL | OS_GBPAK_RSTB_DETECTION);
    *status |= ((u8*)temp)[BLOCKSIZE - 1];

    /* Translate the flags into an error: no cartridge present, or one that was
     * just (re)inserted and therefore not yet trustworthy. */
    if (!(*status & OS_GBPAK_GBCART_ON)) {
      ret = PFS_ERR_NO_GBCART;
    } else if (*status & OS_GBPAK_GBCART_PULL) {
      ret = PFS_ERR_NEW_GBCART;
    }
  } else if (ret == 2) {
    ret = PFS_ERR_CONTRFAIL;
  }
  return ret;
}
