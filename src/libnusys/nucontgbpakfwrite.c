/*
 * GB Pak unaligned cartridge write.
 *
 * The write counterpart of nuContGBPakFread: writes an arbitrary address/length
 * range to a Game Boy Game Pak, hiding the 32-byte block constraint. A partial
 * block is handled read-modify-write so surrounding bytes are preserved. It
 * also toggles MBC RAM access for cartridge SRAM and verifies power afterward.
 */
#include <nusys.h>

/*
 * Write size bytes from buffer to the cartridge at address.
 *
 * Power is assumed to be on already. Returns a PFS_ERR_* code on failure,
 * otherwise the final status read.
 */
s32 nuContGBPakFwrite(NUContPakFile* handle, u16 address, u8* buffer,
                      u16 size) {
  s32 rtn;
  s32 writesize;
  s32 offset;
  s32 ram;
  u16 writeAdd;
  u8 data[32];
  u8 status;

  // Without a registered GB Pak Manager there is nothing to dispatch to.
  if (nuContGBPakCallBack.func == NULL) {
#ifdef NU_DEBUG
    osSyncPrintf("nuContGBPakReadWrite: no 64GBPack manager!!\n");
#endif
    return 0;
  }

  // The SRAM window (0xA000-0xBFFF) is normally write-protected. If the range
  // touches it, send the MBC RAM-enable code so the cartridge accepts writes,
  // and remember to re-protect it afterward.
  ram = 0;
  if ((0xa000 <= address) && (address < 0xc000)) {
    bzero(data, 32);
    data[31] = NU_CONT_GBPAK_MBC_RAM_ENABLE_CODE;
    rtn = nuContGBPakWrite(handle, NU_CONT_GBPAK_MBC_RAM_REG0_ADDR, data, 32);
    if (rtn) {
      return rtn;
    }
    ram++;
  }

  rtn = nuContGBPakCheckConnector(handle, &status);
  if (rtn) {
    return rtn;
  }

  // Walk the request in 32-byte blocks. A misaligned or short tail block is a
  // read-modify-write: pull the aligned block in, splice the new bytes over the
  // wanted slice, and write the whole block back so neighboring bytes survive.
  // A fully aligned run is written straight from the caller's buffer.
  while (size) {
    offset = address & 0x001f;
    if (offset || size < 32) {
      writeAdd = address & 0xffe0;
      rtn = nuContGBPakRead(handle, writeAdd, data, 32);
      if (rtn) {
        return rtn;
      }
      if (size < 32) {
        writesize = size;
      } else {
        writesize = 32 - offset;
      }
      bcopy(buffer, (data + offset), writesize);
      rtn = nuContGBPakWrite(handle, writeAdd, data, 32);
      if (rtn) {
        return rtn;
      }
    } else {
      writesize = size & 0xffe0;
      rtn = nuContGBPakWrite(handle, address, buffer, writesize);
      if (rtn) {
        return rtn;
      }
    }
    address += writesize;
    buffer += writesize;
    size -= writesize;
  }

  // Re-protect the SRAM window if this write enabled it.
  if (ram) {
    bzero(data, 32);
    rtn = nuContGBPakWrite(handle, NU_CONT_GBPAK_MBC_RAM_REG0_ADDR, data, 32);
    if (rtn) {
      return rtn;
    }
  }

  // A power drop mid-transfer means the write may not have landed.
  rtn = nuContGBPakGetStatus(handle, &status);
  if (rtn) {
    return rtn;
  }
  if (!(status & OS_GBPAK_POWER)) {
    return PFS_ERR_CONTRFAIL;
  }
  return rtn;
}
