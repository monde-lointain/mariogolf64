#include "club.h"

extern ClubDistancePreset D_800CABD0[];

ClubDistancePreset* func_80043AF0(s32 clubIndex) { return &D_800CABD0[clubIndex]; }

void func_80043B0C(void) {}

// Meter extent (yards) for the active club and category selector
// (0 = first meter, 1 = second meter, 2 = power-curve kind).
//
// The category dispatch is written as `if (meterCategory == K) goto ...;` to a set
// of out-of-line body labels, NOT as a switch or if/else chain. This is
// REQUIRED to match the original object byte-for-byte: KMC gcc 2.7.2 lowers a
// structured `if (meterCategory == 2) extentYards = 30;` (extentYards provably
// 0, all-constant arm) to a BRANCHLESS `mask & 30`, and every if/else/switch idiom
// AROUND the body (leaving it inline). The ROM instead branches TOWARD each
// body with a branch-likely `beql`, which gcc only emits when the then-clause
// is a bare jump (jump.c "condjump over an unconditional jump" -> invert to
// branch-on-true). The body ORDER is also load-bearing: the non-putter power_a
// case is a load, which cannot ride a branch-likely delay slot, so it must be
// declared LAST and fall through into the shared return. Do not "simplify" this
// to a switch/if-else.
s32 get_club_meter_extent(ClubShot* clubShot, s32 meterCategory) {
  s32 extentYards = 0;

  if (clubShot->flags & 1) {
    if (meterCategory == 1) goto putter_meter2;
    if (meterCategory == 0) goto putter_meter1;
    if (meterCategory == 2) goto putter_curve;
    goto end;
  putter_meter2:
    extentYards = 100;
    goto end;
  putter_meter1:
    extentYards = 200;
    goto end;
  putter_curve:
    extentYards = 30;
    goto end;
  } else {
    if (meterCategory == 1) goto normal_meter2;
    if (meterCategory == 0) goto normal_meter1;
    if (meterCategory == 2) goto normal_curve;
    goto end;
  normal_meter1:
    extentYards = (s32)((f32)clubShot->target_distance * 1.1f);
    goto end;
  normal_curve:
    extentYards = 60;
    goto end;
  normal_meter2:
    extentYards = clubShot->target_distance;
    goto end;
  }
end:
  return extentYards;
}

// Meter extent converted to game-space units (per-yard factor differs for
// putter). One reused `meterUnits` assigned in both arms is deliberate: it makes the
// value a single allocno that conflicts with $v0 instead of coalescing into it,
// so the int/float chain runs in $a0 and matches the ROM. gcc tail-merges the
// two arms back into one factor-select + one multiply.
s32 get_club_meter_units(ClubShot* clubShot, s32 meterCategory) {
  s32 meterUnits = get_club_meter_extent(clubShot, meterCategory);

  if (clubShot->flags & 1) {
    meterUnits = (s32)((f32)meterUnits * 4681.728f);
  } else {
    meterUnits = (s32)((f32)meterUnits * 14045.184f);
  }
  return meterUnits;
}
