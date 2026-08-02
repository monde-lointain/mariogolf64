# func_8005CEE0 — near-match (CARRIED, load-order + regalloc scheduling coin)

**BANKED S315** (`fb12a92`). The S268 "TERMINAL / do not re-grind" verdict is retired. The load-order-vs-`idx*4`-colouring coupling below was real for the shape it was measured on — flat `u8` externs with hand-written `idx * 4` / `a * 14` / `b * 2` stride arithmetic. Typing the externs as the arrays they are (`extern s16 D_800C29B8[10][2];` and `extern s16 D_800C28E4[3][7];`, both already declared in the host file for `build_roster_grid`) and writing `D_800C29B8[idx][1]` / `D_800C29B8[idx][0]` / `D_800C28E4[row][col]` gives 38/38 byte-clean on the first build. The two pair loads then share one index register with a `%lo` displacement each, and the grid address is a two-term register sum so the symbol materialises into a register — which is the whole residual. Kept for the refuted-lever table.


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
`docs/hazards.md#base-register-vs-displacement` / the S224 within-block scheduling walls).

## S268 re-open — COUPLED, terminal (order-fix ⟂ index-coloring)
Correction to the S266 note: register roles are NOT swapped in the warm base.c. With
`u8* base = D_800C28E4;` explicit + `base + a*14 + b*2` the roles ALREADY match ROM
(a→a0, base→a1, b→v1, idx→v0). The sole residual is a LOAD-ORDER coin:
- ROM loads the a-table (29BA) BEFORE the b-table (29B8), base materialized between;
  emits `a*14` then `b*2`.
- Mine loads the b-table (29B8) FIRST (addresses ascending: 29B8<29BA), emits `b*2`
  early. This ordering is **fold-canonical**: `b*2 + (base + a*14)`, explicit parens,
  every operand-spelling variant folds to the identical RTL, so the order is
  spelling-invariant (score stays 640 / 0.84).

The ONLY way to reorder the two independent loads is a statement-split (pointer/accum
temp) that evaluates `base + a*14` first. But every split perturbs the idx*4 index
register coloring:
- `u8* p = base + a*14; p + b*2` (block or top-decl): perfect order + roles, but
  idx*4 recolors v0→v1 (1800-1860).
- `base = base + a*14` / `base += a*14` (RMW, no new pseudo): swaps a↔base roles
  (a→a1, base→a0) (3320 / 0.17).
- reuse dead `idx` as accumulator (`idx = (s32)(base+a*14); idx + b*2`): keeps idx→v0
  but recolors idx*4 into a0 (1800).

So load-order and idx*4-coloring are COUPLED through local-alloc: the inline fold keeps
idx*4 in v0 but forces b-first; any split that forces a-first adds/moves a pseudo that
recolors idx*4. No source form yields {a-first load order AND idx*4 in v0}. The S265
`docs/levers.md` (commutative operand order statement split) and `docs/levers.md` (one variable reuse reorders loads)
(incl. the reuse-dead-var inverse) were the new levers — both tried, neither cracks it.

## Verdict
CARRIED — TERMINAL (S268). Exact-count local-alloc coloring coin coupled to a
fold-canonical load order; not source-steerable by any statement-split (each recolors
the idx*4 index reg). Not permuter-reachable (dry 400s). Re-open only via a
matched-corpus sibling that fixes the pack's local-alloc pressure, not a source lever.
Do NOT re-grind.
