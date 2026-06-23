/*
 * contpfs.c -- Controller Pak filesystem (PFS): on-Pak identity and inode
 * management.
 *
 * A Controller Pak stores files in a small flash-like filesystem. This file
 * holds the lowest layer: checksum helpers, the device-ID block (which records
 * the Pak's serial/bank count and whose redundant copies must agree), and the
 * inode-table read/write routine with its mirror-and-repair logic. Higher PFS
 * operations build on these. The Pak is unreliable hardware, so almost every
 * routine reads back what it wrote and falls back to a redundant copy on a
 * mismatch.
 */
#include "PRinternal/macros.h"
#include "PR/os_internal.h"
#include "PR/os_version.h"
#include "controller.h"
#include "PR/rmon.h"
#if BUILD_VERSION >= VERSION_J

// One-entry inode cache so a repeated read of the same (channel, bank) inode
// page skips the slow Pak round-trip. Bank 250 is an impossible value used as
// "empty" so the first real read always misses.
extern __OSInode __osPfsInodeCache;
s32 __osPfsInodeCacheChannel = -1;
u8 __osPfsInodeCacheBank = 250;
#endif

/* 16-bit additive checksum over length bytes, used to validate inode pages. */
u16 __osSumcalc(u8* ptr, int length) {
  int i;
  u32 sum = 0;
  u8* tmp = ptr;
  for (i = 0; i < length; i++) {
    sum += *tmp++;
#if BUILD_VERSION < VERSION_J
    sum = sum & 0xFFFF;
#endif
  }
#if BUILD_VERSION >= VERSION_J
  return sum & 0xFFFF;
#else
  return sum;
#endif
}

/* Compute the ID-block checksum pair: a plain 16-bit sum and the sum of the
 * bit-inverted words. Storing both lets a reader detect corruption that a
 * single sum could miss. The trailing u32 (the stored checksums themselves) is
 * excluded from the range. */
s32 __osIdCheckSum(u16* ptr, u16* csum, u16* icsum) {
  u16 data = 0;
  u32 j;
  *csum = *icsum = 0;
  for (j = 0; j < ((sizeof(__OSPackId) - sizeof(u32)) / sizeof(u8)); j += 2) {
    data = *(u16*)((u32)ptr + j);
    *csum += data;
    *icsum += ~data;
  }
  return 0;
}

/* Rebuild a corrupt device-ID block. Carries the serial number over from the
 * damaged ID, re-discovers how many banks the Pak actually has by probing each
 * bank (write a marker, read it back, stop at the first bank that does not
 * verify), recomputes the checksums, and writes the fresh ID to all four
 * redundant ID slots, verifying the first. Returns 0, or a fatal/device error
 * if the rewrite cannot be confirmed. */
s32 __osRepairPackId(OSPfs* pfs, __OSPackId* badid, __OSPackId* newid) {
  s32 ret = 0;
  u8 temp[BLOCKSIZE];
  u8 comp[BLOCKSIZE];
  u8 mask = 0;
  int i;
  int j;
  u16 index[4];
#if BUILD_VERSION >= VERSION_J
  j = 0;
#else
  SET_ACTIVEBANK_TO_ZERO();
#endif

  // Seed the new ID: mark it repaired, give it a fresh random tag, and inherit
  // the old serial so the Pak keeps its identity.
  newid->repaired = -1;
  newid->random = osGetCount();
  newid->serial_mid = badid->serial_mid;
  newid->serial_low = badid->serial_low;
#if BUILD_VERSION >= VERSION_J
  SET_ACTIVEBANK_TO_ZERO();
#else
  j = 0;
#endif

  // Count usable banks by writing a per-bank marker (bank index with high bit
  // set, rest of the block inverted) to each bank's block 0 and reading it
  // back. The first bank whose readback fails to match is one past the last
  // good bank. For banks beyond 0, also confirm bank 0 still reads its own
  // marker, guarding against banks that silently alias bank 0.
  do {
    ERRCK(SELECT_BANK(pfs, j));
    ERRCK(__osContRamRead(pfs->queue, pfs->channel, 0, temp));
    temp[0] = j | 0x80;
    for (i = 1; i < BLOCKSIZE; i++) {
      temp[i] = ~temp[i];
    }
    ERRCK(__osContRamWrite(pfs->queue, pfs->channel, 0, temp, FALSE));
    ERRCK(__osContRamRead(pfs->queue, pfs->channel, 0, comp));
    for (i = 0; i < BLOCKSIZE; i++) {
      if (comp[i] != temp[i]) {
        break;
      }
    }
    if (i != BLOCKSIZE) {
      break;
    }
    if (j > 0) {
      ERRCK(SELECT_BANK(pfs, 0));
      ERRCK(__osContRamRead(pfs->queue, pfs->channel, 0, (u8*)temp));
      if (temp[0] != 0x80) {
        break;
      }
    }
    j++;
  } while (j < PFS_MAX_BANKS);
#if BUILD_VERSION >= VERSION_J
  SET_ACTIVEBANK_TO_ZERO();
#else
  ERRCK(SELECT_BANK(pfs, 0));
#endif

  // Finalize the ID: low device-id bit flags multi-bank Paks, record the bank
  // count, carry the version, and stamp the freshly computed checksums.
  mask = (j > 0) ? 1 : 0;
  newid->deviceid = (badid->deviceid & (u16)~1) | mask;
  newid->banks = j;
  newid->version = badid->version;
  __osIdCheckSum((u16*)newid, &newid->checksum, &newid->inverted_checksum);

  // Write the new ID to all four redundant ID areas, then read the first back
  // and require an exact match before declaring the repair successful.
  index[0] = PFS_ID_0AREA;
  index[1] = PFS_ID_1AREA;
  index[2] = PFS_ID_2AREA;
  index[3] = PFS_ID_3AREA;
  for (i = 0; i < ARRLEN(index); i++) {
    ERRCK(
        __osContRamWrite(pfs->queue, pfs->channel, index[i], (u8*)newid, TRUE));
  }
  ERRCK(__osContRamRead(pfs->queue, pfs->channel, PFS_ID_0AREA, (u8*)temp));
  for (i = 0; i < BLOCKSIZE; i++) {
    if (temp[i] != ((u8*)newid)[i]) {
#if BUILD_VERSION >= VERSION_J
      return PFS_ERR_DEVICE;
#else
      return PFS_ERR_ID_FATAL;
#endif
    }
  }
  return 0;
}

/* Recover the device ID when its primary copy (area 0) is bad. Scans the
 * remaining redundant ID areas for one whose stored checksums verify; if found,
 * it copies that good ID back over every other area to re-establish redundancy.
 * Returns the recovered ID in temp, or PFS_ERR_ID_FATAL if no copy is intact.
 */
s32 __osCheckPackId(OSPfs* pfs, __OSPackId* temp) {
  u16 index[4];
  s32 ret = 0;
  u16 sum;
  u16 isum;
  int i;
  int j;
  SET_ACTIVEBANK_TO_ZERO();
  index[0] = PFS_ID_0AREA;
  index[1] = PFS_ID_1AREA;
  index[2] = PFS_ID_2AREA;
  index[3] = PFS_ID_3AREA;

  // Start at area 1 (area 0 is the one already known bad) and take the first
  // copy whose checksum pair validates.
  for (i = 1; i < ARRLEN(index); i++) {
    ERRCK(__osContRamRead(pfs->queue, pfs->channel, index[i], (u8*)temp));
    __osIdCheckSum((u16*)temp, &sum, &isum);
    if (temp->checksum == sum && temp->inverted_checksum == isum) {
      break;
    }
  }
  if (i == ARRLEN(index)) {
    return PFS_ERR_ID_FATAL;
  }

  // Heal the other areas by writing the good copy back over each of them.
  for (j = 0; j < ARRLEN(index); j++) {
    if (j != i) {
      ERRCK(__osContRamWrite(pfs->queue, pfs->channel, index[j], (u8*)temp,
                             TRUE));
    }
  }
  return 0;
}

/* Read and validate the Pak's device ID, then derive the filesystem geometry
 * from it into the OSPfs handle. Falls back to ID recovery/repair when the
 * primary copy fails its checksum or the device-id bit says the Pak was never
 * fully initialized. On success the handle's bank count and the inode/mirror/
 * directory table offsets are all computed from the banks field. */
s32 __osGetId(OSPfs* pfs) {
#if BUILD_VERSION < VERSION_J
  int k;
#endif
  u16 sum;
  u16 isum;
  u8 temp[BLOCKSIZE];
  __OSPackId newid;
  s32 ret;
  __OSPackId* id;
  SET_ACTIVEBANK_TO_ZERO();
  ERRCK(__osContRamRead(pfs->queue, pfs->channel, PFS_ID_0AREA, (u8*)temp));
  __osIdCheckSum((u16*)temp, &sum, &isum);
  id = (__OSPackId*)temp;

  // Primary ID failed its checksum: try to recover a good redundant copy, and
  // if none survives, rebuild the ID from scratch.
  if (id->checksum != sum || id->inverted_checksum != isum) {
    ret = __osCheckPackId(pfs, id);
    if (ret == PFS_ERR_ID_FATAL) {
      ERRCK(__osRepairPackId(pfs, id, &newid));
      id = &newid;
    } else if (ret != 0) {
      return ret;
    }
  }

  // An even device-id marks an uninitialized Pak: repair it, and if it still
  // comes back uninitialized the device is unusable.
  if ((id->deviceid & 1) == 0) {
    ERRCK(__osRepairPackId(pfs, id, &newid));
    id = &newid;
    if ((id->deviceid & 1) == 0) {
      return PFS_ERR_DEVICE;
    }
  }

  // Cache the validated ID and unpack version/bank count into the handle.
#if BUILD_VERSION >= VERSION_J
  bcopy(id, pfs->id, BLOCKSIZE);
#else
  for (k = 0; k < ARRLEN(pfs->id); k++) {
    pfs->id[k] = ((u8*)id)[k];
  }
#endif
  pfs->version = id->version;
  pfs->banks = id->banks;

  // Lay out the filesystem: the inode table, its mirror, and the directory
  // table are placed back-to-back, each sized by the bank count.
  pfs->inode_start_page = 1 + DEF_DIR_PAGES + (2 * pfs->banks);
  pfs->dir_size = 16;
  pfs->inode_table = PFS_ONE_PAGE;
  pfs->minode_table = (1 + pfs->banks) * PFS_ONE_PAGE;
  pfs->dir_table = pfs->minode_table + pfs->banks * PFS_ONE_PAGE;
  ERRCK(__osContRamRead(pfs->queue, pfs->channel, PFS_LABEL_AREA, pfs->label));
  return 0;
}

/* Confirm the Pak in the slot is still the one this handle was opened against.
 * Re-reads the current ID block and compares it to the cached id; a difference
 * means the user swapped Paks (PFS_ERR_NEW_PACK), which higher layers treat as
 * "remount before trusting cached geometry". */
s32 __osCheckId(OSPfs* pfs) {
#if BUILD_VERSION < VERSION_J
  int k;
#endif
  u8 temp[BLOCKSIZE];
  s32 ret;

  // Selecting a bank also re-arms the "new pack" detection; the second select
  // retries once past a transient swap signal.
#if BUILD_VERSION >= VERSION_J
  if (pfs->activebank != 0) {
    ret = __osPfsSelectBank(pfs, 0);
    if (ret == PFS_ERR_NEW_PACK) {
      ret = __osPfsSelectBank(pfs, 0);
    }
    if (ret != 0) {
      return ret;
    }
  }
#else
  SET_ACTIVEBANK_TO_ZERO();
#endif

  // A new-pack signal on the read is non-fatal here; read once more to get the
  // current ID, then compare against the cached one.
  ret = __osContRamRead(pfs->queue, pfs->channel, PFS_ID_0AREA, (u8*)temp);
  if (ret != 0) {
    if (ret != PFS_ERR_NEW_PACK) {
      return ret;
    }
    ERRCK(__osContRamRead(pfs->queue, pfs->channel, PFS_ID_0AREA, (u8*)temp));
  }
#if BUILD_VERSION >= VERSION_J
  if (bcmp(pfs->id, temp, BLOCKSIZE) != 0) {
    return PFS_ERR_NEW_PACK;
  }
#else
  for (k = 0; k < ARRLEN(temp); k++) {
    if (pfs->id[k] != temp[k]) return PFS_ERR_NEW_PACK;
  }
#endif
  return 0;
}

/* Read or write a bank's inode table, kept in two copies (primary + mirror) for
 * resilience. On write, both copies are updated and the page checksum stamped.
 * On read, the checksum is verified; a failure falls back to the mirror, and if
 * the mirror is good the primary is rewritten from it (self-healing). A read
 * whose result still fails the checksum returns PFS_ERR_INCONSISTENT (the Pak
 * needs the filesystem checker). The result populates the inode cache. */
s32 __osPfsRWInode(OSPfs* pfs, __OSInode* inode, u8 flag, u8 bank) {
  u8 sum;
  int j;
  s32 ret;
  int offset;
  u8* addr;

  // Fast path: a cached read for the same channel/bank avoids the Pak entirely.
#if BUILD_VERSION >= VERSION_J
  if (flag == PFS_READ && bank == __osPfsInodeCacheBank &&
      (pfs->channel == __osPfsInodeCacheChannel)) {
    bcopy(&__osPfsInodeCache, inode, sizeof(__OSInode));
    return 0;
  }
#endif
  SET_ACTIVEBANK_TO_ZERO();

  // Bank 0's inode table starts after the directory pages; later banks start at
  // page 1. The checksum covers the inode entries past that start offset.
  offset = (bank > 0) ? 1 : pfs->inode_start_page;
  if (flag == PFS_WRITE) {
    inode->inode_page[0].inode_t.page =
        __osSumcalc((u8*)&inode->inode_page[offset],
                    (PFS_INODE_SIZE_PER_PAGE - offset) * 2);
  }

  // Transfer the whole inode page block-by-block. A write updates both the
  // primary table and its mirror; a read pulls only from the primary for now.
  for (j = 0; j < PFS_ONE_PAGE; j++) {
    addr = ((u8*)inode->inode_page + j * BLOCKSIZE);
    if (flag == PFS_WRITE) {
      ret = __osContRamWrite(pfs->queue, pfs->channel,
                             pfs->inode_table + bank * PFS_ONE_PAGE + j, addr,
                             FALSE);
      ret = __osContRamWrite(pfs->queue, pfs->channel,
                             pfs->minode_table + bank * PFS_ONE_PAGE + j, addr,
                             FALSE);
    } else {
      ret = __osContRamRead(pfs->queue, pfs->channel,
                            pfs->inode_table + bank * PFS_ONE_PAGE + j, addr);
    }
    if (ret != 0) {
      return ret;
    }
  }

  // Validate a read against the stored checksum; on mismatch fall back to the
  // mirror table.
  if (flag == PFS_READ) {
    sum = __osSumcalc((u8*)&inode->inode_page[offset],
                      (PFS_INODE_SIZE_PER_PAGE - offset) * 2);
    if (sum != inode->inode_page[0].inode_t.page) {
      for (j = 0; j < PFS_ONE_PAGE; j++) {
        addr = ((u8*)inode->inode_page + j * BLOCKSIZE);
        ret =
            __osContRamRead(pfs->queue, pfs->channel,
                            pfs->minode_table + bank * PFS_ONE_PAGE + j, addr);
      }
#if BUILD_VERSION >= VERSION_J
      sum = __osSumcalc((u8*)&inode->inode_page[offset],
                        (PFS_INODE_SIZE_PER_PAGE - offset) * 2);
#endif

      // If the mirror is also bad, the filesystem is corrupt.
      if (sum != inode->inode_page[0].inode_t.page) {
        return PFS_ERR_INCONSISTENT;
      }

      // Mirror was good: repair the primary table from it.
      for (j = 0; j < PFS_ONE_PAGE; j++) {
        addr = ((u8*)inode->inode_page + j * BLOCKSIZE);
        ret = __osContRamWrite(pfs->queue, pfs->channel,
                               pfs->inode_table + bank * PFS_ONE_PAGE + j, addr,
                               FALSE);
      }
    }
#if BUILD_VERSION < VERSION_J
    else {
      // Primary was good: refresh the mirror so it stays in sync.
      for (j = 0; j < PFS_ONE_PAGE; j++) {
        addr = ((u8*)inode->inode_page + j * 32);
        ret = __osContRamWrite(pfs->queue, pfs->channel,
                               pfs->minode_table + bank * PFS_ONE_PAGE + j,
                               addr, FALSE);
      }
    }
#endif
  }

  // Populate the single-entry inode cache with what we just read/wrote.
#if BUILD_VERSION >= VERSION_J
  __osPfsInodeCacheBank = bank;
  bcopy(inode, &__osPfsInodeCache, sizeof(__OSInode));
  __osPfsInodeCacheChannel = pfs->channel;
#endif
  return 0;
}

/* Legacy bank selection: a multi-bank Pak picks its active bank by writing the
 * bank number to the special detect block. The block is filled uniformly so the
 * Pak latches it regardless of which byte it samples. */
#if BUILD_VERSION < VERSION_J
s32 __osPfsSelectBank(OSPfs* pfs) {
  u8 temp[BLOCKSIZE];
  int i;
  s32 ret = 0;
  for (i = 0; i < BLOCKSIZE; i++) {
    temp[i] = pfs->activebank;
  }
  ret = __osContRamWrite(pfs->queue, pfs->channel, CONT_BLOCK_DETECT, temp,
                         FALSE);
  return ret;
}
#endif

/* Debug aid: dump the raw device-ID block field-by-field to the rmon console.
 */
#ifdef _DEBUG
s32 __osDumpId(OSPfs* pfs) {
  u8 id[BLOCKSIZE];
  __OSPackId* temp;
  s32 ret;
  ERRCK(__osContRamRead(pfs->queue, pfs->channel, PFS_ID_0AREA, id));
  temp = (__OSPackId*)id;
  rmonPrintf("repaired %x\n", temp->repaired);
  rmonPrintf("random %x\n", temp->random);
  rmonPrintf("serial_mid %llu\n", temp->serial_mid);
  rmonPrintf("serial_low %llu\n", temp->serial_low);
  rmonPrintf("deviceid %x\n", temp->deviceid);
  rmonPrintf("banks %x\n", temp->banks);
  rmonPrintf("version %x\n", temp->version);
  rmonPrintf("checksum %x\n", temp->checksum);
  rmonPrintf("inverted_checksum %x\n", temp->inverted_checksum);
  return 0;
}
#endif
