# func_8005CEE0 — near-match (CARRIED, load-order + regalloc scheduling coin)

## Function
`s32 func_8005CEE0(s32 idx)` — range-check (0..14, else -1) + triple-table lookup
returning a sign predicate. 0x98, 38 instrs, jal=1 (func_8005C674).

## Faithful body (near-match 38/38, in nonmatchings/func_8005CEE0/base.c)
```c
s32 func_8005CEE0(s32 arg0) {
  s32 idx; s16 a; s16 b; u16 h; u8* base;
  if (arg0 < 0)  { goto ret_m1; }
  if (arg0 < 15) { goto ok; }
ret_m1:
  return -1;
ok:
  func_8005C674(1, 1);
  idx = D_800C2BB0[arg0];                       /* s16 index table */
  a   = *(s16*)(&D_800C29BA[idx * 4]);           /* struct{s16 f0@B8; s16 f2@BA} */
  base = D_800C28E4;                             /* explicit base ptr (see below) */
  b   = *(s16*)(&D_800C29B8[idx * 4]);
  h   = *(u16*)(base + a * 14 + b * 2);          /* stride-14 rows, halfword-index b */
  return ((s32)(h << 16)) >> 31;                 /* sign of the s16 -> -1/0 */
}
```

## Fixes already found (exact count)
1. **Guard structure**: `if(arg0<0) goto ret_m1; if(arg0<15) goto ok; ret_m1: return -1;
   ok: ...` — the `-1` block must sit BETWEEN the guards with the 2nd guard branching
   TOWARD `ok`. A `||` combined guard folds to one `sltiu` (fold-range-test); two plain
   `if...return -1` merge the `-1` into direct epilogue branches (2 short).
2. **base-vs-displacement on D_800C28E4**: an explicit `u8* base = D_800C28E4;` then
   `base + a*14 + b*2` materializes the full base register (ROM `lui+addiu; addu; 0(reg)`);
   the array-index form `D_800C28E4[...]` folds `%lo` into the load displacement (2 short).
   This lever brings the count to EXACT 38/38.

## Residual (terminal-ish)
Sole diff (lines 18-30) is a within-block scheduling/regalloc permutation:
- ROM loads `a` (D_800C29BA) -> a0, materializes the base -> a1 INTERLEAVED between the
  two field loads, then `b` (D_800C29B8) -> v1; mine loads a -> v1, b -> a0, base last.
- ROM emits `a*14` then `b*2`; mine emits `b*2` then `a*14`.
Register roles (a0<->v1) and the two-multiply emission order are swapped. Permuter
(400s, seed 777, -j8) found NO zero — it perturbs source structure but this is a
post-load scheduling/regalloc coin on an otherwise byte-exact body (kin to
[[base-register-vs-displacement]] / the S224 within-block scheduling walls).

## Verdict
CARRIED — exact-count regalloc/schedule coin. Re-open with a register-role lever
(force a into a0 / base materialized between the two field loads) or a matched-corpus
sibling. Do NOT re-grind as a fresh leaf.
