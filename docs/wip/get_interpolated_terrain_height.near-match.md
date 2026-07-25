# get_interpolated_terrain_height — exact-count register-permutation carry (S280)

## Verdict
Body is FULLY RE'd and structurally BYTE-EXACT: exact instruction count (0x208 / 130 instrs), every
instruction mnemonic + immediate + memory offset matches the ROM. The ONLY residual is a terminal
local-alloc register-coloring permutation in the integer cross-product block (an `a1<->a2` core swap
that cascades through the scratch registers). asm-differ score 270 (from 3188 at first build); the
permuter plateaus at 170 over 220+ iters (best-only, --stop-on-zero), never reaching 0. This is the
documented multi-register-permutation NON-payoff shape.

## What the function does
Barycentric / plane-equation terrain-height interpolation. `s32 get_interpolated_terrain_height(s32 x,
s32 z)`:
1. Clamp x,z into range (mode-dependent: `g_terrain_vtx_xform_mode != 0` -> both [0,0xFFFFF]; else
   x in [0,0x3FFFFF], z in [0,0x7FFFFF]). The clamp is `if((u32)v>MAX){ if(v<0) v=0; else v=MAX; }`
   (negative-test-first is load-bearing: reproduces the ROM's `bgez`->MAX-target / `bltz`->zero-target
   polarity mix exactly).
2. `detect_terrain_collision(x, z, tri)` fills a local `s16 tri[11]` (tri[2..4]=x[3], tri[5..7]=y[3],
   tri[8..10]=z[3]; same buffer layout as `func_800432E4`). If it returns <0, return -300.
3. Plane normal N = E1 x E2 where E1 = V1-V0, E2 = V2-V0 (V0=(tri[2],tri[5],tri[8]) etc):
   nx = e1y*e2z - e1z*e2y, ny = e1z*e2x - e1x*e2z, nz = e1x*e2y - e1y*e2x.
4. Height Y at (x,z) = (y0<<10) + (s32)((-nx_f*(x-x0<<10) - nz_f*(z-z0<<10)) / ny_f); if (s32)ny_f==0
   return 0.

## The matching-structure body (score 270, exact count)
See the S280 diff / git history. Load-bearing source levers that got it to exact-count byte-structure:
- Clamp as nested `if(v<0)v=0;else v=MAX;` (branches, not ternary -> ternary if-converts to
  nor/sra/and cmov; ROM branches). Negative-test-first for the ROM's bgez/bltz polarity.
- Cross products written `p1 + operand*-factor` (explicit negated OPERAND) -> reproduces the ROM's
  `negu; mult; ...; addu` accumulate form; the plain `p1 - p2` spelling emits `subu` (3 instr shorter,
  wrong). Negated operand per ROM: nx negates e1z, ny negates e2z, nz negates e2x; SECOND product
  term is the negated+added one, and its ORDER within the expression matches ROM's just-in-time eval.
- Inline the three normals directly into the `(f32)(...)` casts (no `s32 nx,ny,nz` intermediates) ->
  interleaves each cross-product `addu` right before its `mtc1`/`cvt` (ROM schedule); the intermediate
  form bunches all three adds then converts.
- Hoist `y0s/x0s/z0s = tri[k]<<10` + `dx = x-x0s` + `dz = z-z0s` as explicit temps BEFORE the
  `(s32)fny==0` guard (dz is computed unconditionally = it lands in the always-executed `bnez` delay
  slot, matching ROM).
- Guard as `if((s32)fny != 0){ return <expr>; } return 0;` (compute in the then-arm) -> ROM's
  `bnez v0, compute` with return-0 INLINE + dz in the branch delay; the `if(==0) return 0;` then-arm
  form hoisted return-0 out of line (beqz) instead.
- Edge decl order `e1y` before `e2z` (loads tri[6] first, like ROM) -> 311 to 270.

## The wall (terminal residual)
From the very first load the scratch-register assignment diverges: ROM loads tri[6](y1) into `a2`,
mine into `a1`; the whole integer block is a consistent register RENAMING from there (a1<->a2 +
cascade). Every instruction is otherwise identical. gcc's REG_ALLOC_ORDER would take a1 before a2
(mine), so the ROM prefers a2 for a reason local-alloc encodes that no faithful C reordering reproduced:
- edge/expression reorder (e2z-first vs e1y-first): 311 vs 270, never 0.
- explicit ordered temps for all 9 tri reads in ROM's exact load order: 257, load order closer but the
  a1<->a2 assignment persists.
- permuter (import.py --settings permuter_settings_main.toml): base 250, plateau 170 over 220+ iters,
  no zero. Its best-170 diffs are noise (`do{}while(0)`, a redundant copy).

Class: `#local-alloc-qty-permutation` / terminal register-coloring coin at exact count (kin to
[[copy-coalesce-cse-signext-terminal]] global.c:790-823 set_preferences; the S232 `||`-ref-count lever
reaches global.c allocno priority, NOT local-alloc's first-fit coloring, so it likely does not apply).

## Re-open lead (gcc-source dive)
The single question is WHY gcc's local-alloc assigns `a2` (not the reg_alloc_order-earlier `a1`) to the
first cross-product operand e1y. A gcc-2.7.2 `local-alloc.c` / `reg_alloc_order` dive on the a1-vs-a2
first-fit (does an incoming-arg `a1` copy / a `detect_terrain_collision` return-value liveness reserve
a1 across the block in the ROM build?) is the crack lead. Per S272 exact-count-first: the body IS
exact-count, so any lever that flips a1->a2 for that one allocno likely banks it whole.
