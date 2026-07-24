# func_80047CAC — pure-FP interpolation stepper (FP scheduler coin, near-match)

**Status:** fully RE'd behavior, near-match. Two independent residuals: an FP block-local
scheduler coin (mine 1 instr SHORTER than ROM = ROM lost a hazard-fill coin, S233 terminal class)
and a 1-ULP float constant. CARRIED S274.

## Semantics (verified from .s, 0xBC / 47 instr, 0 jal)
Three lerp-style updates to float globals from s32 inputs scaled by 1/1024:
```c
void func_80047CAC(void) {
  D_800DAF30 += ((f32)D_800DAEEC * (1.0f/1024.0f) - D_800DAF30) * 0.2f;
  D_800DAF34 += ((f32)D_800DAEF0 * (1.0f/1024.0f) - K - D_800DAF34) * 0.5f;
  D_800DAF38 += ((f32)D_800DAEF4 * (1.0f/1024.0f) - D_800DAF38) * 0.2f;
}
```
Behavior is correct (both the sequential and the batched diff-then-scale spelling compile to the
same schedule). Externs already typed in func_800453E0.c.

## Residual 1 — the middle constant K is 1 ULP off
`19.65f` compiles to `0x419D3333` (19.64999962). ROM has `0x419D3334` (19.65000153). Use a literal
that rounds up, e.g. `19.6500015f` / `19.650002f` -> `0x419D3334`. (Original was likely
19.6500015f, not a clean 19.65.)

## Residual 2 — FP scheduler coin (the wall)
Mine = 46 instr, ROM = 47. ROM emits TWO `mtc1;nop` hazard bubbles (after the 19.65 mtc1 AND
after the 0.2 `0x3E4CCCCD` mtc1). Mine fills the second bubble with an independent `lwc1`
(loads D_800DAF34 right after the 0.2 mtc1), so mine is strictly tighter by one nop.
Root: ROM keeps `1/1024` and `0.2` in the SAME FPR (reuses `$f8/ft2` across the two phases,
since 1/1024 dies before 0.2 is needed); mine splits them ($f0 for 1/1024, $f8 for 0.2), which
frees the scheduler to hoist the 0.2 load early and fill the hazard. To match, D_800DAF34 must be
loaded EARLY (like ROM, before the 0.2 mtc1) so it is not available as the filler -> nop appears.
Tried: sequential `+=` form and batched diff-then-scale form -> identical 46-instr schedule; decl
reorder did not move the D_800DAF34 load. This is a block-local FP scheduler/regalloc coin
(mips.md hazard fill), the S233 "ROM lost a coin, mine is 1 shorter" terminal class — not
global.c ref-count steerable.

## Next action
PERMUTER (FP schedule reorder is its domain) — BLOCKED on the same `import.py` main-include gap as
[[get_terrain_vertex_pointer]] (`<PR/ultratypes.h>` not on import.py's hardcoded `-I include`).
Isolated base.c compiled clean via `decomp_loop` (score 6240, 47/50 rows, isolation-noisy negative
percent). Fix the constant first, then permuter the schedule once import.py learns the main
includes.
