# get_surface_type (0x80039F7C) — near-match carry (S233)

Pack: `src/main/raycast_terrain.c` (fresh 13-fn main collision pack).
Status: **structurally complete, score 450 in-tree** (`tools/asm-differ/diff.py get_surface_type`).
Wall class: **#local-alloc-qty-permutation** (a single register-assignment of the `15` constant).

## The function
FP tile-coords -> 16x16 grid cell lookup into a per-tile column table, then surface-type
post-processing. Signature `s32 get_surface_type(s32 tile, f32 x, f32 z)` (x,z are float args in
GPRs a1/a2 per o32 mixed-arg rule). Externs: `u8 D_800BA6B0[]` (per-tile column selector, indexed
`tile & 0x3F`), `s8 D_800B7DB0[]` (flat `[col][256]` grid table), `s32 flag_is_set(s32)`.

## Structurally-complete C (score 450; everything aligns except one instruction)
```c
s32 get_surface_type(s32 tile, f32 x, f32 z) {
    s32 cx;
    s32 cz;
    s32 type;
    s8 *col;

    cx = (s32) (x * 16.0f);
    cz = (s32) (z * 16.0f);
    if (cx < 0) { cx = 0; } else if (cx >= 16) { cx = 15; }
    if (cz < 0) { cz = 0; } else if (cz >= 16) { cz = 15; }

    col = D_800B7DB0 + (D_800BA6B0[tile & 0x3F] << 8);
    cz = 15 - cz;
    type = col[(cz << 4) | cx];
    type = type - 0x30;
    if (type < 0) { type = 0; }
    if (type >= 16) { type = 15; }
    if (type == 3) { type = 0; }
    if (flag_is_set(0xA)) { type = 1; }
    return type;
}
```

## Two divergences already cracked (levers that hold)
1. **#base-register-vs-displacement**: `D_800B7DB0[(sel<<8) + idx]` folds `%lo` into the load
   displacement; ROM materializes the full base. FIX: split the base as an explicit pointer add
   `col = D_800B7DB0 + (sel << 8); col[idx]` (forces `lui+addiu`, then `addu`, `lb 0(col)`).
2. **C89 top-of-function declaration is load-bearing** here: `s8 *col` declared mid-block (after
   statements) compiled to score 1010; hoisting the decl to the top block dropped it to 660, then
   the `cz = 15 - cz;` + separate `type - 0x30;` split (permuter-found, source-expressible) dropped
   it to 450. GCC 2.7.2 is C89; mid-block decls change scheduling.

## The residual wall (not source-leverable found so far)
ROM saves `tile` (a0) into `v1` at the top (`move v1,a0`, before the FP ops) so it can put the
`15` constant of `15 - cz` into a0 (`li a0,0xf`; `subu a1,a0,a1`). My build keeps `tile` in a0
(its arg reg, via preference) and puts `15` in v0 (`li v0,0xf`) — 1 instruction SHORTER, i.e. my
codegen is *more* optimal than the ROM. Both `tile` and the `15` const are single-use, so their
allocno priorities tie and GCC's local-alloc respects `tile`'s a0-preference; the ROM's allocator
made the worse choice. Tried and FAILED to flip it: col/idx eval order swap, `cz=15-cz` reassign,
tile-use-after-const reorder, and a 4-min `--best-only` permuter run (best 310 isolated, never 0).
Classic #local-alloc-qty-permutation (project history: 0 permuter cracks).

## Compiler-source verdict (S233 fan-out): NO LEVER — terminal
Root-caused end-to-end (gcc-2.7.2), ~35 source variants scored against a real `target.o` (all
floor at 316; 0=match):
1. **`config/mips/mips.md:153-155`** — R4000 `load` function-unit READY-DELAY is 3. The pre-reload
   scheduler (`sched.c`) front-loads the table `lbu D_800BA6B0[tile&0x3F]` to hide that 3-cycle
   latency, filling the load-delay window with the independent `li 15`/`subu` chain. So `li 15`
   lands AFTER the load. `rank_for_schedule` (sched.c:2385-2430) gives `andi` and `li15` EQUAL
   priority; the tie falls to load-latency stall logic → the latency-OPTIMAL order.
2. **`local-alloc.c`** — const-15 is a block-LOCAL quantity; materialized late, v0 is the lowest
   free hard reg → it takes `$v0`.
3. **`global.c`** — `tile` (cross-block allocno) has an a0 copy-preference; since 15 is in v0 (not
   a0), tile coalesces into a0 with no conflict → my build ELIDES `move v1,a0` and is 1 instr
   SHORTER. The ROM lost the scheduler coin (li-15 BEFORE the load → 15 lives across andi+lbu →
   local-alloc must use a non-v0 reg → tile's a0-pref conflicts → `move v1,a0` survives).
Why no lever: the choice is downstream of the source-invariant R4000 load-latency model. The
algorithm intrinsically needs the table load (giving that chain latency-hiding priority) and the
index chain has no load, so faithful C cannot make the scheduler defer the load. The S232
`global.c:591-595 floor_log2(n_refs)` ref-count lever is INAPPLICABLE — it steers GLOBAL allocno
priority, but const-15 is a block-LOCAL quantity that never enters that sort. `-fno-schedule-insns`
scores worse (956), confirming the ROM was built WITH the scheduler. This is the
`#local-alloc-qty-permutation` class (0 permuter cracks; the 4-min run here reached 310, never 0).
**Terminal carry.** My compiler produces strictly better code than the ROM here.
