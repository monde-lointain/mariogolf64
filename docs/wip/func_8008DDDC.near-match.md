# func_8008DDDC (0x388/904B) — near-match carry (S249)

Fog/material color-preset interpolation. Advances a progress counter (`D_800C5EF4`, clamped
`< D_800C5EF8`), fetches two keyframe records from `D_800C6014` (base + `putter_mode_flag`
row + `func_80051FCC()<<8` frame, the second +0x20), and blends them by `progress/1000`
(signed div magic 0x10624DD3). Writes 5 material color lanes (3 rgb channels each) into
`D_800C69CC`, then 3 fog rgb (`g_fog_presetA_color_r[0..2]`) and 2 fog Z bounds
(`g_fog_presetA_z_min/max`). 2 calls to `func_80051FCC` (void), fp=0.

## Status: STRUCTURAL-COMPLETE, pervasive-regalloc near-match
- Fully RE'd C in `docs/wip/func_8008DDDC.base.c` (== `nonmatchings/func_8008DDDC/base.c`).
- decomp_loop: score **8900**, percent **0.610**, match_count 228, cur=211 vs ref=212 instr.
- 23 opcode-level diffs remain (not pure register permutation): constant/address
  materialization placement + the resulting s-register permutation.

## Levers applied (score progression 11420 -> 8900)
- Compute the 5 lane bases AFTER the second `func_80051FCC()` call so 3 land in temporaries
  (t7/t8/t9) instead of crossing the call as callee-saved — removed the extra `s7` spill
  (frame 0x38 -> 0x30). 11420 -> 9340 (biggest lever).
- Shared advancing `tbl = D_800C6014; ... tbl += 0x20;` pointer reproduces the reference's
  `s0` running-base (kept across the call, mutated +0x20 for kfB) vs rematerialized address.
  9340 -> 9040.
- Lane-base init ascending (b0 first) nudges the anchor toward base+0. 9040 -> 8900.

## Residual (why carried) — #pervasive-regalloc-classical-main / constant-materialization
- `&putter_mode_flag` (and `&D_800C6014`-derived offsets): reference keeps the address in a
  callee-saved reg (`s1`) and reloads the value twice; my build rematerializes the %hi/%lo.
  Caching as `s32 *pp = &putter_mode_flag` BACKFIRED (8900 -> 13160): GCC then also
  rematerialized `D_800C5EEC`. Natural global ref is correct; the keep-vs-remat is an
  unsteerable regalloc coin here.
- div-by-1000 magic (0x10624DD3) placement + loop-bound constant `3` hoisted to a saved reg
  vs reference's `li v0,3` re-materialized at the compare.
- percent 0.610 << 0.97 gate; residual is regalloc/materialization, not source-leverable.

## Next-attempt ideas (if re-opened)
- Compiler-source fan-out (gcc-2.7.2 global.c allocno-order + local-alloc.c) to name the
  keep-vs-rematerialize decision on `&putter_mode_flag` across the call — the closest lever
  target (this fn is the most tractable of the S249 pair at 0.610).
- Kin to memory [[loop-invariant-hoist-order-preheader-regalloc]] and
  [[cross-call-live-range-callee-saved-lever]].
