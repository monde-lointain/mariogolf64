/*
 * gbpakcheckconnector.c -- detect a faulty Game Boy Transfer Pak connection.
 *
 * A poorly seated cartridge often fails to drive its address lines, so reads at
 * different addresses return identical data. This routine exploits that: it
 * reads the cartridge across a doubling range of addresses and verifies the
 * data actually changes from one address to the next (and against the base
 * address). If reads that should differ come back identical, the connector is
 * judged unreliable (PFS_ERR_CONTRFAIL).
 *
 * It ping-pongs between two scratch buffer slots (1 and 2) so each new address
 * can be compared against the previous one, while slot 0 holds the base-address
 * reference; buf_status[] tracks which slots already hold valid data so a block
 * is never re-read needlessly.
 */
#include "PR/os_internal.h"
#include "controller.h"
#include "controller_gbpak.h"

s32 osGbpakCheckConnector(OSPfs* pfs, u8* status) {
  s32 ret;
  s32 bufn = 1;
  s32 oldbufn = 0;
  u16 address = 0;
  u16 oldaddr = 0;
  u16 daddr = 0;
  u16 num;
  u8 buf[3][4][BLOCKSIZE];
  u8 buf_status[3][4];

  /* Need a powered, present cartridge; one retry covers a fresh reinsertion. */
  ret = osGbpakGetStatus(pfs, status);
  if (ret == PFS_ERR_NEW_GBCART) {
    ret = osGbpakGetStatus(pfs, status);
  }
  if (ret == PFS_ERR_NEW_GBCART) {
    return PFS_ERR_CONTRFAIL;
  }
  if (ret == 0) {
    if (!(*status & OS_GBPAK_POWER)) {
      ERRCK(osGbpakPower(pfs, OS_GBPAK_POWER_ON));
    }
    bzero(buf_status, sizeof(buf_status));

    /* Walk the probe addresses, doubling each step (0x80, 0x100, ... 0x4000).
     */
    for (address = 0x80; address <= 0x4000; address <<= 1) {
      num = 0;
      daddr = 0;

      /* Compare this address's blocks against the previous address's. Read the
       * current block, lazily fill the previous block if not already cached,
       * and flag a fault the moment two consecutive addresses read identically
       * (num reset to 0 forces the failure check below). */
      do {
        ERRCK(osGbpakReadWrite(pfs, OS_READ, address + daddr, buf[bufn][num],
                               BLOCKSIZE));
        buf_status[bufn][num] = 1;
        if (buf_status[oldbufn][num] == 0) {
          ret = osGbpakReadWrite(pfs, OS_READ, oldaddr + daddr,
                                 buf[oldbufn][num], BLOCKSIZE);
          if (ret != 0) {
            return ret;
          }
          buf_status[oldbufn][num] = 1;
        }
        if (bcmp(buf[bufn][num], buf[oldbufn][num], BLOCKSIZE) != 0) {
          num = 0;
          break;
        }
        daddr += BLOCKSIZE;
      } while (num++ < ARRLEN(buf[0]) - 1);
      if (num != 0) {
        return PFS_ERR_CONTRFAIL;
      }

      /* From the second address on, also compare against the base address
       * (slot 0): a good cartridge differs from its base reading too. */
      if (oldbufn != 0) {
        num = 0;
        daddr = 0;
        do {
          if (buf_status[bufn][num] == 0) {
            ERRCK(osGbpakReadWrite(pfs, OS_READ, address + daddr,
                                   buf[bufn][num], BLOCKSIZE));
            buf_status[bufn][num] = 1;
          }
          if (buf_status[0][num] == 0) {
            ret = osGbpakReadWrite(pfs, OS_READ, daddr, buf[0][num], BLOCKSIZE);
            if (ret != 0) {
              return ret;
            }
            buf_status[0][num] = 1;
          }
          if (bcmp(buf[bufn][num], buf[0][num], BLOCKSIZE)) {
            num = 0;
            break;
          }
          daddr += BLOCKSIZE;
        } while (num++ < ARRLEN(buf_status[0]) - 1);
      }
      if (num != 0) {
        return PFS_ERR_CONTRFAIL;
      }

      /* Advance the ping-pong: retire the slot we just compared against, then
       * make the current slot the new "previous" and flip to the other slot. */
      if (oldbufn != 0) {
        bzero(buf_status[oldbufn], ARRLEN(buf_status[oldbufn]));
      }
      oldaddr = address;
      oldbufn = bufn;
      bufn ^= 3;
    }

    /* Larger carts (multi-block directory, or version 2) also mirror their RAM:
     * 0xA000 and 0x2000 should read alike on a good connection. */
    if ((pfs->dir_size >= 2) || (pfs->version == 2)) {
      num = 0;
      daddr = 0;
      do {
        ERRCK(osGbpakReadWrite(pfs, OS_READ, daddr + 0xA000, buf[bufn][num],
                               BLOCKSIZE));
        ERRCK(osGbpakReadWrite(pfs, OS_READ, daddr + 0x2000, buf[oldbufn][num],
                               BLOCKSIZE));
        if (bcmp(buf[bufn][num], buf[oldbufn][num], BLOCKSIZE)) {
          num = 0;
          break;
        }
        daddr += BLOCKSIZE;
      } while (num++ < ARRLEN(buf[0]) - 1);
      if (num != 0) {
        return PFS_ERR_CONTRFAIL;
      }
    }

    /* Re-read status last so the caller sees the current state; a "new cart"
     * here means the cartridge moved mid-check, which is itself a failure. */
    ret = osGbpakGetStatus(pfs, status);
    if (ret == PFS_ERR_NEW_GBCART) {
      ret = PFS_ERR_CONTRFAIL;
    }
  }
  return ret;
}
