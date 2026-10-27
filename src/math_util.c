#include "common.h"

#include "math_util.h"

/* The integer lengths halve their components until they are below this
 * magnitude */
#define INT_LENGTH_COMPONENT_LIMIT 0x20000000
/* Whether -INT_LENGTH_COMPONENT_LIMIT <= value < INT_LENGTH_COMPONENT_LIMIT */
#define IS_SMALL_COMPONENT(value)               \
  ((u32)(value) - INT_LENGTH_COMPONENT_LIMIT >= \
      (u32)(-2 * INT_LENGTH_COMPONENT_LIMIT))

/* CRC-16/X.25: reflected CCITT polynomial, all ones in and inverted out */
#define CRC16_REFLECTED_POLYNOMIAL 0x8408
#define CRC16_INITIAL_VALUE 0xFFFF

extern s32 f_RandomSeed;
extern f32 f_ZeroDivisor;

/* Halves the components until they are small enough, and doubles the length
 * back */
u32 IntLength3D(s32 x, s32 y, s32 z) {
  s32 shiftCount;

  shiftCount = 0;
  while (TRUE) {
    if (IS_SMALL_COMPONENT(x)) {
      if (IS_SMALL_COMPONENT(y)) {
        if (IS_SMALL_COMPONENT(z)) {
          break;
        }
      }
    }
    shiftCount++;
    x >>= 1;
    y >>= 1;
    z >>= 1;
  }
  return (u32)sqrt((f64)x * x + (f64)y * y + (f64)z * z) << shiftCount;
}

u32 IntLength2D(s32 x, s32 y) {
  s32 shiftCount;

  shiftCount = 0;
  while (TRUE) {
    if (IS_SMALL_COMPONENT(x)) {
      if (IS_SMALL_COMPONENT(y)) {
        break;
      }
    }
    shiftCount++;
    x >>= 1;
    y >>= 1;
  }
  return (u32)sqrt((f64)x * x + (f64)y * y) << shiftCount;
}

void SetRandomSeed(s32 seed) {
  f_RandomSeed = seed;
}

s32 NextRandom(void) {
  return f_RandomSeed = f_RandomSeed * 0x5D588B65 + 1;
}

f32 Length2D(f32 x, f32 y) {
  return sqrtf(x * x + y * y);
}

f32 Length3D(f32 x, f32 y, f32 z) {
  return sqrtf(x * x + y * y + z * z);
}

u16 ComputeCrc16(u8* data, u32 size) {
  u32 crc;
  u32 bitIndex;
  u8* end;

  crc = CRC16_INITIAL_VALUE;
  if (size != 0) {
    end = (u8*)(size + (u32)data);
    do {
      crc ^= *data;
      for (bitIndex = 0; bitIndex < 8; bitIndex++) {
        if (crc & 1) {
          crc = (crc >> 1) ^ CRC16_REFLECTED_POLYNOMIAL;
        } else {
          crc >>= 1;
        }
      }
      data++;
    } while (data < end);
  }
  crc = ~crc;
  crc &= 0xFFFF;
  osSyncPrintf("CRC = %x\n", crc);
  return crc;
}

/*
 * Prints the caller, then forces a CPU exception: truncating the infinite
 * quotient to an integer is an unimplemented operation, which the FPU cannot
 * mask
 */
s32 ReportDivisionError(void) {
  u32 returnAddress;

  asm volatile("move %0, $31" : "=r"(returnAddress));
  osSyncPrintf("kDivError %x\n", returnAddress);
  return 1.0f / f_ZeroDivisor;
}
