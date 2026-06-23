/*
 * gbpaksetbank.c -- Transfer Pak (GB Pak) bank selection.
 *
 * The Transfer Pak exposes the inserted Game Boy cartridge through a small
 * address window, paging the cartridge in 0x4000-byte banks. This helper points
 * that window at a bank by writing the bank-select block, recovering once if
 * the write reports the pak was just (re)inserted.
 */
#include "PR/os_internal.h"
#include "controller.h"

/* Select cartridge `bank` (0..2) on the Transfer Pak behind `pfs`. Returns
 * PFS_ERR_INVALID for an out-of-range bank, otherwise the controller-write
 * status; updates the cached bank on success. */
s32 __osGbpakSetBank(OSPfs* pfs, u8 bank) {
  int i;
  s32 ret;
  u8 temp[BLOCKSIZE];

  if (bank > 2) {
    return PFS_ERR_INVALID;
  }

  /* The bank number is broadcast across every byte of the write block. */
  for (i = 0; i < BLOCKSIZE; temp[i++] = bank) {
    ;
  }

  ret = __osContRamWrite(pfs->queue, pfs->channel, CONT_BLOCK_GB_BANK, temp,
                         FALSE);

  /* A "new pak" status means the pak was reinserted: re-initialize it and
   * retry the bank write once before giving up. */
  if (ret == PFS_ERR_NEW_PACK) {
    ret = osGbpakInit(pfs->queue, pfs, pfs->channel);
    if (ret == 0) {
      ret = __osContRamWrite(pfs->queue, pfs->channel, CONT_BLOCK_GB_BANK, temp,
                             FALSE);
      if (ret == PFS_ERR_NEW_PACK) {
        ret = PFS_ERR_CONTRFAIL;
      }
    }
  }

  if (ret == 0) {
    pfs->banks = bank;
  }
  return ret;
}
