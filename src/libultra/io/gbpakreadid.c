/*
 * gbpakreadid.c -- read and validate a Game Boy cartridge's header ID.
 *
 * Reads the cartridge header through the Transfer Pak, verifies it is a genuine
 * Game Boy cartridge (the boot ROM's Nintendo logo must match and the header
 * checksum must be correct), copies the ID out, and records the cartridge's
 * memory-controller type and RAM size in the OSPfs handle.
 */
#include "macros.h"
#include "PR/os_internal.h"
#include "controller.h"

/* The Nintendo logo bitmap every licensed Game Boy cartridge stores at header
 * offset 0x04; the boot ROM refuses to run a cartridge whose copy differs, so
 * matching it is the canonical "is this a real GB cart" test. */
// clang-format off: hand-aligned 48-byte logo bitmap, 12 bytes per row
static u8 nintendo[] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83,
    0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63,
    0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E};
// clang-format on

/* Maps a cartridge's raw header cartridge-type byte to the memory-controller
 * generation the driver cares about (0xFF = unsupported). */
// clang-format off: hand-aligned cartridge-type -> MMC-generation lookup
static u8 mmc_type[] = {0x00, 0x01, 0x01, 0x01, 0xFF, 0x02, 0x02,
                        0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0x03, 0x03, 0x03, 0x03, 0x03};
// clang-format on

s32 osGbpakReadId(OSPfs* pfs, OSGbpakId* id, u8* status) {
  s32 i;
  s32 ret;
  u8 isum;
  u8 buf[96];

  /* Confirm the pak is alive; one retry covers a just-reinserted cartridge. */
  ret = osGbpakGetStatus(pfs, status);
  if (ret == PFS_ERR_NEW_GBCART) {
    ret = osGbpakGetStatus(pfs, status);
  }
  if (ret == PFS_ERR_NEW_GBCART) {
    return PFS_ERR_CONTRFAIL;
  } else if (ret == 0) {
    if (!(*status & OS_GBPAK_POWER)) {
      ERRCK(osGbpakPower(pfs, OS_GBPAK_POWER_ON));
    }

    /* Pull the cartridge header (it lives at GB address 0x100). */
    ERRCK(osGbpakReadWrite(pfs, OS_READ, 0x100U, buf, ARRLEN(buf)));
    ret = osGbpakGetStatus(pfs, status);
    if (ret == PFS_ERR_NEW_GBCART) {
      ret = PFS_ERR_CONTRFAIL;
    }
    if (ret != 0) {
      return ret;
    }

    /* If the cartridge's reset line never asserted, the read is unreliable. */
    if (!(*status & OS_GBPAK_RSTB_STATUS)) {
      return PFS_ERR_CONTRFAIL;
    }

    /* Authenticity check 1: the logo at offset 4 must match exactly. */
    if (bcmp(nintendo, buf + 4, ARRLEN(nintendo))) {
      return 4;
    }

    /* Authenticity check 2: the header checksum. Summing bytes 0x34..0x4D and
     * adding 0x19 must wrap to zero (the GB boot ROM's header-check formula).
     */
    for (i = 0x34, isum = 0; i < 0x4E; i++) {
      isum += buf[i];
    }
    if ((isum + 0x19) & 0xFF) {
      return 4;
    }

    /* Header is valid: hand the caller the ID and remember the cartridge's
     * controller type and RAM size for later bank handling. */
    bcopy(buf, id, 0x50);
    if (id->cart_type < 0x14) {
      pfs->version = (int)mmc_type[id->cart_type];
    }
    pfs->dir_size = (int)id->ram_size;
  }
  return ret;
}
