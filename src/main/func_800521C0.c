#include "common.h"

extern s32 flag_is_set(s32 flag);
extern s32 round_hole_flags;

s32 get_round_first_hole(void) { return (round_hole_flags & 2) ? 9 : 0; }

s32 get_round_hole_count(void) {
  if (flag_is_set(0x3D)) {
    return 2;
  }
  if (round_hole_flags & 6) {
    return 9;
  }
  return 18;
}

s32 get_round_last_hole(void) {
  return get_round_first_hole() + get_round_hole_count() - 1;
}
