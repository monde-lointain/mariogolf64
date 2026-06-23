/*
 * gbpakreadwrite.c -- Transfer Pak (GB Pak) bulk read/write.
 *
 * Moves a run of data between RDRAM and the inserted Game Boy cartridge through
 * the pak's paged address window. Addresses, sizes, and the bank wrap are all
 * worked in 32-byte controller blocks, switching banks transparently when a
 * transfer crosses a bank boundary.
 */
#include "PR/os_internal.h"
#include "controller.h"
#include "controller_gbpak.h"

/* Read (flag == 0) or write (flag == 1) `size` bytes at cartridge `address`
 * through `pfs`, using `buffer` in RDRAM. Returns 0 on success or the first
 * controller error encountered. address selects a bank in its top bits; size
 * and address are rescaled to block units before the transfer loop. */
s32 osGbpakReadWrite(OSPfs* pfs, u16 flag, u16 address, u8* buffer, u16 size) {
  s32 i;
  s32 ret;
  u8 bank;

  /* The cartridge bank lives in the high bits of the address; page it in first
   * if it differs from the bank currently selected on the pak. */
  bank = (u8)(address >> 0xE);
  if (bank != pfs->banks) {
    ret = __osGbpakSetBank(pfs, bank);
    if (ret != 0) {
      return ret;
    }
  }

  /* Convert byte size and address into 32-byte block counts; the address is
   * folded into the pak's read/write window (0xC000) before scaling. */
  size = (u16)(size >> 5);
  address = (u16)((address | 0xC000) >> 5);

  if (flag == 1) {
    for (i = 0; i < (s32)size; i++, buffer += BLOCKSIZE) {
      ret = __osContRamWrite(pfs->queue, pfs->channel, address, buffer, 0);
      if (ret != 0) {
        break;
      }

      /* Past the end of the window, advance to the next bank and rewind the
       * address to where that bank's window resumes (0x600). */
      if ((++address >= 0x800) && (i < (s32)(size - 1))) {
        ret = __osGbpakSetBank(pfs, ++bank);
        if (ret != 0) {
          break;
        }
        address = 0x600U;
      }
    }
  } else {
    for (i = 0; i < (s32)size; i++, buffer += BLOCKSIZE) {
      ret = __osContRamRead(pfs->queue, pfs->channel, address, buffer);
      if (ret != 0) {
        break;
      }

      /* Same bank-wrap handling as the write path above. */
      if (++address >= 0x800 && (i < (s32)(size - 1))) {
        ret = __osGbpakSetBank(pfs, ++bank);
        if (ret != 0) {
          break;
        }
        address = 0x600U;
      }
    }
  }

  /* A mid-transfer reinsertion surfaces as a hard controller failure here. */
  if (ret == PFS_ERR_NEW_PACK) {
    ret = PFS_ERR_CONTRFAIL;
  }
  return ret;
}
