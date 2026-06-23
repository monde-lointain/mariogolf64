/*
 * crc.c -- Controller Pak CRCs: the address word and the 32-byte data block
 * sent over the joybus each carry a CRC the Pak verifies, so these checks must
 * reproduce the Pak's polynomial division bit-for-bit. Two implementations
 * exist; the VERSION_J build uses the first, restructured to process the
 * message MSB-first with explicit named parameters.
 */
#include "PR/os_internal.h"
#if BUILD_VERSION >= VERSION_J

/* Address CRC: a 5-bit CRC over the 11-bit Pak block address. The generator is
 * the joybus address polynomial; the result occupies the low 5 bits. */
#define ADDRESS_CRC_MESSAGE_LENGTH 10
#define ADDRESS_CRC_LENGTH 5
#define ADDRESS_CRC_GENERATOR 0x15
#define ADDRESS_CRC_MASK ((1 << ADDRESS_CRC_LENGTH) - 1)

u8 __osContAddressCrc(u16 addr) {
  u32 temp = 0;

  // Feed the address bits MSB-first into the shift-register division. When the
  // bit pushed out is set, XOR in the generator (the -1 variant folds in the
  // simultaneous +1 the set address bit would contribute).
  u32 i = (1 << ADDRESS_CRC_MESSAGE_LENGTH);
  do {
    temp <<= 1;
    if ((u32)addr & i) {
      if (temp & (1 << ADDRESS_CRC_LENGTH)) {
        temp ^= ADDRESS_CRC_GENERATOR - 1;
      } else {
        ++temp;
      }
    } else if (temp & (1 << ADDRESS_CRC_LENGTH)) {
      temp ^= ADDRESS_CRC_GENERATOR;
    }
    i >>= 1;
  } while (i != 0);

  // Flush the register with CRC_LENGTH trailing zero bits to finish the
  // division.
  i = ADDRESS_CRC_LENGTH;
  do {
    temp <<= 1;
    if (temp & (1 << ADDRESS_CRC_LENGTH)) {
      temp ^= ADDRESS_CRC_GENERATOR;
    }
  } while (--i != 0);
  return temp & ADDRESS_CRC_MASK;
}

/* Data CRC: an 8-bit CRC over the 32-byte data block read from or written to a
 * Pak. The generator is the joybus data polynomial. */
#define DATA_CRC_MESSAGE_BYTES 32
#define DATA_CRC_LENGTH 8
#define DATA_CRC_GENERATOR 0x85

u8 __osContDataCrc(u8* data) {
  u32 temp = 0;
  u32 i;
  u32 j;

  // Process all 32 bytes, each MSB-first, through the same shift-register
  // division used for the address CRC.
  for (i = DATA_CRC_MESSAGE_BYTES; i; --i) {
    for (j = (1 << (DATA_CRC_LENGTH - 1)); j; j >>= 1) {
      temp <<= 1;
      if ((*data & j) != 0) {
        if ((temp & (1 << DATA_CRC_LENGTH)) != 0) {
          temp ^= DATA_CRC_GENERATOR - 1;
        } else {
          ++temp;
        }
      } else if (temp & (1 << DATA_CRC_LENGTH)) {
        temp ^= DATA_CRC_GENERATOR;
      }
    }
    data++;
  }

  // Flush with CRC_LENGTH trailing zero bits; i has wrapped to 0 from the loop
  // above, so this runs exactly DATA_CRC_LENGTH times.
  do {
    temp <<= 1;
    if (temp & (1 << DATA_CRC_LENGTH)) {
      temp ^= DATA_CRC_GENERATOR;
    }
  } while (++i < DATA_CRC_LENGTH);
  return temp;
}
#else

/* Pre-VERSION_J equivalents: same polynomials (0x15 / 0x85), expressed as a
 * straight bit-at-a-time CRC that shifts each message bit in before
 * XOR-folding. */
u8 __osContAddressCrc(u16 addr) {
  u8 temp = 0;
  u8 temp2;
  int i;
  for (i = 0; i < 16; i++) {
    temp2 = (temp & 0x10) ? 0x15 : 0;
    temp <<= 1;
    temp |= (u8)((addr & 0x400) ? 1 : 0);
    addr <<= 1;
    temp ^= temp2;
  }
  return temp & 0x1f;
}

/* Pre-VERSION_J data CRC, matching the address-CRC bit-serial form above. */
u8 __osContDataCrc(u8* data) {
  u8 temp = 0;
  u8 temp2;
  int i;
  int j;

  // 33 iterations: 32 data bytes plus a final pass of trailing zeros that
  // flushes the shift register.
  for (i = 0; i <= 32; i++) {
    for (j = 7; j > -1; j--) {
      temp2 = (temp & 0x80) ? 0x85 : 0;
      temp <<= 1;
      if (i == 32) {
        temp &= -1;
      } else {
        temp |= ((*data & (1 << j)) ? 1 : 0);
      }
      temp ^= temp2;
    }
    data++;
  }
  return temp;
}
#endif
