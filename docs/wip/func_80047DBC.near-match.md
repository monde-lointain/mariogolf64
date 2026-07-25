# func_80047DBC — near-match carry (terminal sched.c const-load-vs-fabs coin)

**Status:** CARRY. Body CONFIRMED CORRECT (byte-exact under `-fno-schedule-insns
-fno-schedule-insns2`; off by exactly one gas nop under the real -O2 profile).
Source-invariant, cpu-invariant, permuter-ineligible. S277.

## Function
3-axis "snap toward target" lerp over three consecutive f32 globals
(D_800DAF24/28/2C). For each axis k: `d = in[k] - G; if (fabsf(d) > 1.0f) G += d*0.2f;`

```c
extern f32 D_800DAF24, D_800DAF28, D_800DAF2C;
void func_80047DBC(f32* arg0) {
  f32* p = &D_800DAF24;
  f32 d;
  d = arg0[0] - *p;         if (fabsf(d) > 1.0f) *p += d * 0.2f;
  d = arg0[1] - D_800DAF28; if (fabsf(d) > 1.0f) D_800DAF28 += d * 0.2f;
  d = arg0[2] - D_800DAF2C; if (fabsf(d) > 1.0f) D_800DAF2C += d * 0.2f;
}
```
Builds at plain `-O2` (no FASTMATH — `fabsf` is an unconditional gcc-2.7.2 builtin,
c-decl.c:3217, so `abs.s` comes out without `-ffast-math`).

## What is already solved
- **Base-reg asymmetry** (block 0 holds `&D_800DAF24` in $v0 for both its load and
  store; blocks 1/2 use inline `%hi/%lo`): reproduced register-for-register by the
  `f32* p = &D_800DAF24;` pointer-for-the-first-only spelling. Plain
  two-use-vs-direct-reference effect, NOT cse use_related_value.
- Register allocation, frame, branch targets, const materialisation, blocks 1/2:
  all byte-exact (proven by the schedulers-off build being word-identical bar the
  10 unresolved %hi/%lo reloc words).

## The single residual (terminal)
Block 0 only:
```
ROM:  sub.s; abs.s; lui 0x3F80; mtc1 $f6; nop(gas); c.lt.s $f6,$f0
mine: sub.s; lui 0x3F80; mtc1 $f6; abs.s;         c.lt.s $f6,$f0
```
gcc's sched.c places the 1.0f `li.s` (mtc1) BEFORE the `abs.s` that feeds the same
compare, so gas fills the mtc1->c.lt.s COPROC_MOVE hazard slot with `abs.s` and
emits no nop — one instruction short of the ROM.

## Root cause (named pass + file:line)
- `insn_cost` (sched.c:1362) = `result_ready_cost`.
- 1.0f load = `movsf_internal1` alt 3 (`=f <- Fm`), type **load** (mips.md:3449-3460)
  -> "memory" ready-delay **3** (mips.md:153).
- `abssf2` = type **fabs** (mips.md:1578) -> "adder" ready-delay **2** (mips.md:226).
- `schedule_insn` (sched.c:2589-2600) queues each predecessor of the compare at
  clock+cost; the const is ready one clock later, and the scheduler emits by
  decreasing clock, so the const always lands one slot EARLIER in the code.
- INSN_LUID (source emission order) is a same-clock tiebreak only — never reached,
  because the two producers are ready at different clocks.

Both sched passes independently swap (`-fno-schedule-insns` alone still swaps;
`-fno-schedule-insns2` alone still swaps; both off = ROM order).

## Invariance evidence
- Source-invariant: `fabsf(d) > 1.0f`, `1.0f < fabsf(d)`, `!(fabsf(d) <= 1.0f)`,
  explicit `f32 m = fabsf(d);` temp, `static inline` 3-call helper, `-ffast-math`
  — all emit `li.s` first.
- CPU-invariant: `-mips1/2/3` × `-mcpu=r3000/r4300/r4600/r4650/r6000/r8000` (9
  builds) — all emit `li.s` first (load ready-delay > fabs ready-delay by 1 in
  every mips.md row).
- Binutils exonerated: gas supplies the ROM's nop automatically once gcc's order is
  right (proven by the schedulers-off build). The residual is 100% gcc insn ORDER.

## Not permuter-eligible
One instruction SHORT (not exact-count-plus-one-operand); the residual is a fixed
function-unit-latency consequence with no priority/LUID tie to perturb. asm-differ
would also mis-score it as a 2-instruction permutation.

## Only untried mechanism (for a future re-attack)
A C shape that puts the 1.0f materialisation in a DIFFERENT basic block from the
compare (const ready-cost <=1 needs an anti/output dep or an unrecognised user insn;
neither is producible by a plain FP compare). The identical lerp triple recurs
verbatim inside func_80048D7C @0x8004A144 (0.5f threshold, 0.1f factor) with the
same abs.s-then-li.s order — a crack banks BOTH.
