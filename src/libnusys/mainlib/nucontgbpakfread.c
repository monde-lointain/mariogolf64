/*
 * GB Pak unaligned cartridge read.
 *
 * Reads an arbitrary address/length range from a Game Boy Game Pak, hiding the
 * underlying 32-byte block constraint of nuContGBPakReadWrite. It also enables
 * MBC RAM access when the range hits cartridge SRAM, checks the connector
 * first, and verifies power afterward. Supports MBC 1/2/3 carts and unmanaged
 * carts.
 */
#include <nusys.h>

/*
 * Read size bytes starting at the cartridge address into buffer.
 *
 * Power is assumed to be on already (the caller turns it on and reads the ID
 * first). Returns a PFS_ERR_* code on failure, otherwise the final status read.
 */
s32 nuContGBPakFread(NUContPakFile* handle, u16 address, u8* buffer, u16 size) {
  s32 rtn;
  s32 readsize;
  s32 offset;
  s32 ram;
  u16 readAdd;
  u8 data[32];
  u8 status;

  // The SRAM window (0xA000-0xBFFF) is normally write-protected. If the range
  // touches it, send the MBC RAM-enable code so the cartridge exposes it, and
  // remember to re-protect it afterward.
  ram = 0;
  if ((0xa000 <= address) && (address < 0xc000)) {
    bzero(data, 32);
    data[31] = NU_CONT_GBPAK_MBC_RAM_ENABLE_CODE;
    rtn = nuContGBPakWrite(handle, NU_CONT_GBPAK_MBC_RAM_REG0_ADDR, data, 32);
    if (rtn) return rtn;
    ram++;
  }

  rtn = nuContGBPakCheckConnector(handle, &status);
  if (rtn) return rtn;

  // Walk the request in 32-byte blocks. A block is handled the slow way (read a
  // full aligned block into a scratch buffer, then copy out the wanted slice)
  // whenever the address is misaligned or fewer than 32 bytes remain; otherwise
  // the aligned bulk is read straight into the caller's buffer.
  while (size) {
    offset = address & 0x001f;
    if (offset || size < 32) {
      readAdd = address & 0xffe0;
      rtn = nuContGBPakRead(handle, readAdd, data, 32);
      if (rtn) return rtn;
      if (size < 32) {
        readsize = size;
      } else {
        readsize = 32 - offset;
      }
      bcopy((data + offset), buffer, readsize);
    } else {
      readsize = size & 0xffe0;
      rtn = nuContGBPakRead(handle, address, buffer, readsize);
      if (rtn) return rtn;
    }
    address += readsize;
    buffer += readsize;
    size -= readsize;
  }

  // Re-protect the SRAM window if this read enabled it.
  if (ram) {
    bzero(data, 32);
    rtn = nuContGBPakWrite(handle, NU_CONT_GBPAK_MBC_RAM_REG0_ADDR, data, 32);
    if (rtn) return rtn;
  }

  // A power drop mid-transfer means the data can't be trusted.
  rtn = nuContGBPakGetStatus(handle, &status);
  if (rtn) return rtn;
  if (!(status & OS_GBPAK_POWER)) {
    return PFS_ERR_CONTRFAIL;
  }
  return rtn;
}
