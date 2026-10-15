#include "common.h"

extern u32 rng_seed;
extern f32 division_error_denominator;

double sqrt(double);
extern f32 sqrtf(f32);

#define RANGE_OFF 0xE0000000
#define RANGE_BOUND 0xBFFFFFFF

/* Scale into [-2^29, 2^29) before squaring. Ordered exit tests preserve the
 * retail branches; combining them into a while predicate rotates the loop.
 * This translation unit needs the main profile's -ffast-math override. */
u32 vector_magnitude_safe(s32 x, s32 y, s32 z) {
  u32 shift_count = 0;
  s32 range_offset = RANGE_OFF;
  u32 range_bound = RANGE_BOUND;

  while (1) {
    if (range_bound < (u32)(x + range_offset)) {
      if (range_bound < (u32)(y + range_offset)) {
        if (range_bound < (u32)(z + range_offset)) {
          break;
        }
      }
    }
    shift_count++;
    x >>= 1;
    y >>= 1;
    z >>= 1;
  }
  return (u32)sqrt((f64)x * (f64)x + (f64)y * (f64)y + (f64)z * (f64)z)
         << shift_count;
}

u32 calculate_hypotenuse_safe(s32 x, s32 y) {
  u32 shift_count = 0;
  s32 range_offset = RANGE_OFF;
  u32 range_bound = RANGE_BOUND;

  while (1) {
    if (range_bound < (u32)(x + range_offset)) {
      if (range_bound < (u32)(y + range_offset)) {
        break;
      }
    }
    shift_count++;
    x >>= 1;
    y >>= 1;
  }
  return (u32)sqrt((f64)x * (f64)x + (f64)y * (f64)y) << shift_count;
}

void set_rng_seed(u32 seed) { rng_seed = seed; }

void update_rng_seed(void) { rng_seed = rng_seed * 0x5D588B65 + 1; }

f32 hypotf_2d(f32 x, f32 y) { return sqrtf(x * x + y * y); }

f32 calc_vec3_magnitude(f32 x, f32 y, f32 z) {
  return sqrtf(x * x + y * y + z * z);
}

u16 crc16_ccitt(u8* data, u32 byte_count) {
  u32 crc = 0xFFFF;
  u32 byte_index;
  /* Function scope keeps GCC 2.7.2 from reversing this counted bit loop. */
  u32 bit_index;

  for (byte_index = 0; byte_index < byte_count; byte_index++) {
    crc ^= data[byte_index];
    for (bit_index = 0; bit_index < 8; bit_index++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0x8408;
      } else {
        crc >>= 1;
      }
    }
  }
  crc = ~crc;
  crc &= 0xFFFF;
  osSyncPrintf("CRC = %x\n", crc);
  return crc;
}

s32 report_div_error(void) {
  u32 return_address;
  // Preserve retail prologue/delay-slot order. This compiler cannot use
  // __builtin_return_address(0); the volatile barrier is intentional.
  __asm__ __volatile__("addu %0, $31, $0" : "=r"(return_address));
  osSyncPrintf("kDivError %x\n", return_address);
  return (s32)(1.0f / division_error_denominator);
}
