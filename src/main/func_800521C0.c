#include "common.h"

extern s32 flag_is_set(s32 flag);
extern s32 round_hole_flags;

enum {
  ROUND_HOLE_FLAG_NINE_HOLES = 2,
  ROUND_HOLE_FLAGS_NINE_HOLE_ROUND = 6,
  TWO_HOLE_ROUND_FLAG = 0x3D,
  BACK_NINE_FIRST_HOLE = 9,
  TWO_HOLE_ROUND_HOLE_COUNT = 2,
  NINE_HOLE_ROUND_HOLE_COUNT = 9,
  FULL_ROUND_HOLE_COUNT = 18,
};

s32 get_round_first_hole(void) {
  return (round_hole_flags & ROUND_HOLE_FLAG_NINE_HOLES)
             ? BACK_NINE_FIRST_HOLE
             : 0;
}

s32 get_round_hole_count(void) {
  if (flag_is_set(TWO_HOLE_ROUND_FLAG)) {
    return TWO_HOLE_ROUND_HOLE_COUNT;
  }
  if (round_hole_flags & ROUND_HOLE_FLAGS_NINE_HOLE_ROUND) {
    return NINE_HOLE_ROUND_HOLE_COUNT;
  }
  return FULL_ROUND_HOLE_COUNT;
}

s32 get_round_last_hole(void) {
  return get_round_first_hole() + get_round_hole_count() - 1;
}
