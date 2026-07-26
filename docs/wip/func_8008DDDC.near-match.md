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
- Kin to memory `docs/levers.md` (loop invariant hoist order preheader regalloc) and
  `docs/levers.md` (cross call live range callee saved lever).

## S250 compiler-source crack attempt — TERMINAL verdict (pass-cited, no-lever)
Reproduced baseline 8900 / 0.610 isolated. Root-caused the residual to a SINGLE coupled
7-callee-saved-register allocation equilibrium; both builds spend exactly 7 saved regs
(s0-s6) at near-identical instr count (ref 212 vs cur 211), but on a different split:
  - REF (7): s0=&D_800C6014(tbl), **s1=&putter_mode_flag addr (kept across both calls)**,
    s2=kfA, s3=b3, s4=b4, s5=grp, s6=mfhi-tmp. const-3 (inner-loop bound) is
    **rematerialized** `li v0,3` inside the inner loop (no saved reg).
  - CUR (7): s0=tbl, s1=kfA, s2=b2, s3=b1, s4=grp, **s5=const-3 (loop.c-hoisted)**,
    s6=mfhi-tmp. `&putter_mode_flag` is **rematerialized** (`lw $3,putter` macro, address
    folded by the assembler) at both loads — no address pseudo exists.
The entire 23-diff cascade (grp s4<->s5, kfA s1<->s2, kfB t6<->t7, lane-base and inner-counter
reg permutation) is downstream of this ONE difference: which saved reg goes to putter's
address vs the hoisted const-3.

### Diverging passes (file:line)
1. **Address-materialization** — `config/mips/mips.c:1023` (`ret = "lw\t%0,%1"`). For a direct
   `(mem (symbol_ref))` the MIPS backend emits the assembler *macro* `lw reg,sym`, which the
   assembler folds to `lui reg,%hi; lw reg,%lo(reg)` — address NOT kept. Keeping `&putter` in
   s1 (`la $17,putter; ... lw $v,0($17)`) requires a separate address pseudo
   `(set p (symbol_ref))` that only CSE/reload-inheritance creates. Post-CSE flow dump
   (`t.c.flow`) confirms BOTH putter and D_800C5EEC remain inline `(mem (symbol_ref))` with
   **no** address pseudo in my compilation — cse.c did not pull either out. The reference kept
   putter's (a marginal reload-inheritance / cse address-cost coin, reload1.c
   `choose_reload_regs`), NOT D_800C5EEC's, despite the two symbols being source-identical
   (both read twice, one jal between; addrs 0x800BA9F8 vs 0x800C5EEC, non-adjacent -> no
   shared-base struct explanation).
2. **const-3 double-hoist** — `loop.c:1631` move_movables
   (`(threshold * savings * m->lifetime) >= insn_count`, threshold = 2*(1+n_non_fixed_regs)
   @ loop.c:532). The inner-loop bound `3` is invariant to both nested loops, hoisted to the
   OUTER preheader -> lives across the outer loop -> callee-saved s5. The reference does not
   hoist it because its outer-preheader pressure (putter addr in s1) makes the hoist
   unprofitable. Purely a function of `n_non_fixed_regs` (reg-availability estimate), coupled
   to decision #1.

### Why no source lever
- `s32 *pp = &putter_mode_flag; *pp` DOES create the address pseudo (-> s5 kept across both
  calls, correct in isolation) but gcc hoists its constant init to the PROLOGUE (materialized
  `lui s5;addiu s5` at fn entry, live range from entry, not ref's after-1st-jal window) AND
  loop.c still hoists const-3 to a SEPARATE reg -> 8 saved regs, frame 0x30->0x38, score
  8900->13160. No faithful C controls the address-pseudo PLACEMENT (constant addr always
  hoists maximally early) or prevents the independent const-3 hoist.
- const-3 un-hoist is defeated by constant propagation: per-outer-iteration `s32 nn=3;` bound
  is const-propagated and hoisted identically (8900, no change). Only `volatile`/a runtime
  value blocks it — both unfaithful (extra load / changed semantics).
- Operand swap (`eec + putter*4`) and descending lane-base init (b4-first) tested: 0 or
  negative (9040) delta. Ascending lane init remains best.
- The two decisions are a coupled zero-sum: fixing putter's address (pp) without
  simultaneously un-hoisting const-3 (unreachable) can only grow the reg count, never reach
  ref's 7-reg split. No lever moves both together.

### Assembler ruled OUT
binutils 2.6 faithfully expands the `lw reg,sym` macro and does not reorder/CSE addresses; the
divergence is entirely in gcc RTL (presence/placement of the address pseudo), not the
assembler.

### Verdict
TERMINAL — not source-leverable. Diverging pass: reload1.c `choose_reload_regs` /
address-pseudo creation feeding `config/mips/mips.c:1023`, coupled with `loop.c:1631`
const-hoist. Best percent reached: **0.610** (baseline; all variants tied or worse), far below
the 0.97 permuter gate, so the permuter was not run. Recommend retaining the S249 carry as a
pass-cited wall (S233 doctrine).
