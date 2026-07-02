#include "common.h"

typedef struct {
  u8 pad00[8];
  u32 flags;    // 0x08
  s16 power_a;  // 0x0C
} ClubParam;

extern u8 D_800CABD0[];

u8* func_80043AF0(s32 index) { return &D_800CABD0[index * 24]; }

void func_80043B0C(void) {}

// Meter extent (yards) for the active club and category selector
// (0 = first meter, 1 = second meter, 2 = power-curve kind).
//
// The category dispatch is written as `if (category == K) goto ...;` to a set
// of out-of-line body labels, NOT as a switch or if/else chain. This is
// REQUIRED to match the original object byte-for-byte: KMC gcc 2.7.2 lowers a
// structured `if (category == 2) extent = 30;` (extent provably 0, all-constant
// arm) to a BRANCHLESS `mask & 30`, and every if/else/switch idiom branches
// AROUND the body (leaving it inline). The ROM instead branches TOWARD each
// body with a branch-likely `beql`, which gcc only emits when the then-clause
// is a bare jump (jump.c "condjump over an unconditional jump" -> invert to
// branch-on-true). The body ORDER is also load-bearing: the non-putter power_a
// case is a load, which cannot ride a branch-likely delay slot, so it must be
// declared LAST and fall through into the shared return. Do not "simplify" this
// to a switch/if-else.
s32 get_club_meter_extent(ClubParam* club, s32 category) {
  s32 extent = 0;

  if (club->flags & 1) {
    if (category == 1) goto putter_meter2;
    if (category == 0) goto putter_meter1;
    if (category == 2) goto putter_curve;
    goto end;
  putter_meter2:
    extent = 100;
    goto end;
  putter_meter1:
    extent = 200;
    goto end;
  putter_curve:
    extent = 30;
    goto end;
  } else {
    if (category == 1) goto normal_meter2;
    if (category == 0) goto normal_meter1;
    if (category == 2) goto normal_curve;
    goto end;
  normal_meter1:
    extent = (s32)((f32)club->power_a * 1.1f);
    goto end;
  normal_curve:
    extent = 60;
    goto end;
  normal_meter2:
    extent = club->power_a;
    goto end;
  }
end:
  return extent;
}

// Meter extent converted to game-space units (per-yard factor differs for
// putter). One reused `units` assigned in both arms is deliberate: it makes the
// value a single allocno that conflicts with $v0 instead of coalescing into it,
// so the int/float chain runs in $a0 and matches the ROM. gcc tail-merges the
// two arms back into one factor-select + one multiply.
s32 get_club_meter_units(ClubParam* club, s32 category) {
  s32 units = get_club_meter_extent(club, category);

  if (club->flags & 1) {
    units = (s32)((f32)units * 4681.728f);
  } else {
    units = (s32)((f32)units * 14045.184f);
  }
  return units;
}
