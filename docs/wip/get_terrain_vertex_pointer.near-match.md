# get_terrain_vertex_pointer — value-select + tail register-permutation (EXACT-COUNT near-match)

**Status:** fully RE'd, 40/40 instruction count, byte-exact except a tail register permutation
+ one `bne`/`bnel` annul coin. Permuter-class (exact-count residual). CARRIED S274.

## Semantics (verified from .s, 0xA0 / 40 instr)
```c
s16* get_terrain_vertex_pointer(u32 col, u32 row) {
  if ((col | row) & 1) {
    u32 rowHi, rowLo, colHi, colLo;
    s32 bigIdx; u8* rowp;
    if (row == 0x40) { rowHi = 0xF; rowLo = 4; }
    else            { rowHi = row >> 2; rowLo = row & 3; }
    if (col == 0x20) { colHi = 7; colLo = 4; }
    else            { colHi = col >> 2; colLo = col & 3; }
    bigIdx = (rowHi * 8 + colHi) << 8;
    rowp = &D_800BA9B0[rowLo * 8];
    return (s16*)(D_80185220 + bigIdx + (rowp[colLo] << 4));
  }
  return get_direct_grid_vertex(col >> 1, row >> 1);
}
```
This C is CORRECT in behavior and reaches exact count.

## Levers already applied (each removed real diffs)
- `u32` params -> ROM's `srl` (logical) shifts on row/col, not `sra`. Load-bearing.
- Left-assoc `D_80185220 + bigIdx + tableterm` -> ROM grouping `(base+bigIdx)+tableterm`.
- `rowp = &D_800BA9B0[rowLo*8]; rowp[colLo]` -> forces ROM's FULL-base materialization
  (`lui;addiu;addu;addu;lbu 0`) instead of the %lo-fold-into-load (`lui;addu;lbu %lo`).

## Residual (the wall)
1. **Register permutation in the tail.** ROM: `colHi`->$v0 (reuses the `li v0,0x20` compare
   reg), `bigIdx`->$v1 (reuses `rowHi`'s reg after consumption). My build: `colHi`->$a3,
   `bigIdx`->$a0. Global allocno assignment; the ROM's tight reuse of the compare register for
   `colHi` is the crux and drives the cascade.
2. **`bne` vs `bnel` on the col select.** ROM annuls the col-select delay slot
   (`bnel a0,v0; srl v0` annulled); row select is plain `bne a1,v0; srl v1` non-annulled. Both
   selects are source-identical; the annul is a reorg.c coin coupled to residual (1) — colHi
   reusing $v0 (live into the fall-through's `li v0,7`) is what makes reorg annul. Fix (1) and
   (2) very likely resolves with it.

Class: value-select-if-else-vs-branch-likely + local/global-alloc register permutation. Not
source-steerable by the levers tried (decl-order reshuffles did not move colHi into $v0).

## Next action = PERMUTER (exact-count residual, ideal input), BLOCKED on tooling
`tools/decomp-permuter/import.py` fails to preprocess a `main`-segment src file: its hardcoded
cpp step uses only `-I include` and cannot find `<PR/ultratypes.h>` (lives at
`include/libultra/PR/`). `nonmatching-func`/`decomp_loop` DO compile the isolated base.c (score
3060, 44/46 rows, isolation-inflated percent 0.30 — the reloc addends dominate; real residual is
just the ~4 tail rows above). Retry once import.py learns the main include set (add
`-I include/libultra/PR` to its preprocess + generated compile command), or hand-build the
permuter workdir. Isolated base.c is preserved inline above.

Retro suggestion filed: teach import.py the MAIN include dirs (`include/libultra/PR`, etc.) so
main-segment exact-count residuals become permuter-reachable.
