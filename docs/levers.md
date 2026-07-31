# Source levers (index)

<role>
The named source-level levers a crack slice draws on, one line each, so a prompt can name one and an
agent can act on it. Read when filling `{{LEVERS}}` in `docs/fanout-prompt.md` and when a residual
class suggests a family of fixes.
</role>

<scope>
An index, not a transcription: derivations live in the retros and, where one exists, a
`docs/hazards.md` section.
</scope>

## Levers with a full playbook

These have a `docs/hazards.md` section; read it, not a line here. Anchors:
`#base-register-vs-displacement`, `#cross-jump-tail-merge`,
`#goto-loop--loopc-never-runs-defeating-strength-reduction-and-bound-hoisting` (keep the entry test
by wrapping in `if (n != 0)`, S306), `#loop-weight-and-live-length-regalloc-steering`,
`#named-aggregate-local-extra-block-move`, `#nested-function-static-chain-spill`,
`#signed-divide-const-v0v1-quotient-destination`, `#switch-compare-chain-layout`,
`#top-tested-loop-goto-local-hoist`.

## Register allocation and coloring

- **cross-call-live-range-callee-saved-lever** -- declare a post-call value *before* the call to force
  a callee-saved register; initializing it after is the inverse.

- **global-allocno-compare-livelength-biv-order** -- register order follows
  `floor_log2(nref)*nref/live_length` (`global.c:587`), which also tiers `local-alloc` quantities and
  spills a long-lived few-reference parameter (S289). Read it with `tools/allocno_report.py`, compute
  the window the ROM's register implies, then pick the knob landing in it: an eighth reference, a free
  bound copy `n = bound;` (S288), or `do {} while (0)` round a subset, reweighting only those refs
  (S292; also bars the sched hoist, S287). `find_reg` takes callee-saved first.
- **scope-and-live-range-steer-allocation** -- scope and live range pick a value's register and
  whether its copy survives: equal copies merge when both pseudos prefer one register
  (`global.c:790-823`), `combine_regs` ties others (`local-alloc.c:472/1290/1587`), and reuse pulls a
  load forward by anti-dependency. Knobs: name a temp to end a range early instead of inlining it
  (S293); hoist a block-local temp to function scope, or the reverse when a function-scope value set
  in two arms becomes a global allocno that misses `combine_regs` (S299); split a value spanning two
  loops, but only what must die between them (S295); give each region its own short-lived local when
  one variable stretches an allocno across both (S308). The inverse recolours one register at a time
  (S287). Terminal only when six forced registers reroute a read.
- **dead-frame-levers** -- for a frame-only diff: `s32 unused[(ROM_frame-0x18)/4]`, `str[row]` (keeps
  base and index live), or an uninit local plus `volatile s32 s = g;` (reload frame, dead `sw $v0`).
  Split point: arrays slot at `expand_decl`, an address-taken scalar only at the first `&x` and after
  every function-scope aggregate, so reserve part in a block opened after it (S296; a block-local
  aggregate declared later lands above it, S304). Slot order is ascending pseudo number
  (`reload1.c`), so an `@F<dec>(sp)`-only residual is a numbering question (S302).
- **per-region-cse-slot-base-lever** -- pass the array directly, no cached pointer, so gcc CSEs the
  base per region. The count of pointer *locals* is the pressure knob: each is a `loop.c` induction
  pointer costing a callee-saved register; an offset off a shared row pointer is not (S289).
- **fp-arg-registers-are-a-signature** -- ROM values in `$f12`/`$f14` where the build uses `$f0`/`$f2`
  mean the callee takes FP args the `extern` omits (S286).

## Scheduling

- **emission-order-placement-lever** -- placement follows source emission order: LUID in latency-1
  blocks (`sched.c`), and `loop.c` hoists invariants in loop-body order before `strength_reduce` adds
  giv inits. Remove a hoist by spelling the invariant as a statement (S300). Knobs: fold a
  pre-call compute into the call argument; `off = i*STRIDE` as its own statement (inlined, the symbol
  folds into a walking-pointer giv and loses `%hi`/`addu`/`%lo`, S290); a row origin left inline
  (`i * 6`) when the ROM births givs in body order (S294). Whether one hoists at all is the same
  lever: `loop.c:1631` wants `threshold*savings*lifetime >= insn_count`, threshold twice `1+nregs`
  falling 3 per move, equal constants merging both. So one call per if-chain arm rather than one
  after it flips which constant the ROM hoists (S308).
- **sched-tiebreak-coins** -- `rank_for_schedule` (`sched.c:2428`) sorts class then LUID (a class-1
  compute defers behind class-3 stores); `schedule_select` (`sched.c:2615`) front-loads a transfer over
  a constant load (`fabsf` sign-mask). Same rule, FP case: a constant load
  outranks a `fabs` feeding the same compare (3 vs 2); `mtc1`-zero ties via potential-hazard. Source
  order is not always the knob: S305 moved a `div` three ways for a byte-identical object.
- **sched-bottomup-loadsplit-livelength-blockmove** -- at exact count a `s32 tmp` statement split
  forces a load interleave (sched is bottom-up); moving a statement earlier shortens a qty's live
  length. Permuter-proof.
- **do-while-zero-block-break** -- an empty `do {} while (0);` emits nothing but ends the preceding
  block, so `reorg` stops reaching past a call to annul the next branch (S288).
- **global-reread-vs-cse**, absorbing defeat-global-base-cse -- a scalar-global store does not
  constrain a later load through a pointer parameter (`true_dependence`, `sched.c:817`), and as
  scalars those globals read invariant against `Gfx *` stores, so the load hoists into `$f20`/`$f22`.
  Typing them `extern s32 G[]` and reading `G[0]` restores both the dependence and the reload
  (S301); a `*(s32 *)` cast on a `u8 *` byte offset does it on the address, terminal where a
  param-base folds. Inverses: a global read in a loop that stores through a pointer is not invariant,
  so the reload forces a second IV -- copy it to a preheader local (S294); a per-site `T *p = &SYM;`,
  never reused, suppresses the re-read (S291).
- **two-argument-call-temp-split** -- when both arguments of a call cross another call, compute them
  into temps first; as one expression gcc evaluates argument 0 fully and holds it across the call.


## Control flow and branch shape

- **do-while-not-equal-loop-exit-form** -- rewrite a bounded `for (i < N)` as `do {} while (i != N)`
  for a `bne` with a register bound and a tight frame; the symptom is `slti` where the ROM has `bne`.
  Choose per loop (S287: 3 do-while, 6 structured).
- **out-of-line-handler-block-branch-likely** -- a one-instruction `goto` handler folds into an
  annulled `beql`; one `goto done` replicates the return copy.
- **else-arm-return-vs-then-arm** -- for a far early return the else-arm form yields a plain `bc1t`
  (`jump.c:1737` cannot invert across a set-retval); keep the final `return 0` last.
- **nonvoid-return-blocks-fallthrough-delay-steal** -- a lone epilogue-branch nop means the ROM fn
  returns `s32` (`reorg.c:3375/4274`).
- **and-equals-zero-not-negated-and** -- `if (!(a & b))` De Morgans into a branch per operand plus
  `or`; `(a & b) == 0` keeps the value-selects and the `and`, which ROM `bc1t`+`and`+`bnez` means.
- **value-select-branch-likely-on-switch-default** -- a constant-select branch-likely extends to a
  switch arm, and it is goto-proof.
- **cross-jump-merge-point-before-store** -- the delay-slot filler names the pre-reorg order; a merge
  point at the argument move, not the store, means each arm held its own copy of the trailing calls;
  duplicate them per arm (S286).
- **store-flag-single-bit-terminal-wall** -- `(x & bit) ? 1 : 0` folds to `lhu; srl` where the ROM
  keeps `andi; bnez`. Terminal.
- **gcc272-fold-range-test-slti-merge** -- adjacent `slti` tests fold to `(u32)(x-lo) < span`; a
  per-test goto defeats the merge when the ROM keeps both.

## Addressing, types and expression shape

- **operand-order-statement-split** -- which operand is computed first is structural, not spelling:
  flip `addu`/`xor` via `x = a; x ^= b;` or `T *p = base + i; p[C] = ...` (`*(p+off+C)` gives
  `addu rd,base,off`, `p[off+C]` reverses it), and precompute an index offset and a `u8 *` base into
  temps to defer a row add (`expr.c:6457`, operand 0 before operand 1).
- **division-codegen** -- no cross-block CSE, so an inline `K / x` in a branching macro emits one
  `div` per using block (+38 insns, S305); one ROM `div` means a variable. `x % K` before `x / K`
  emits the ROM's non-coalesced `move`; division first coalesces. Under `-ffast-math` (six `src/main`
  TUs, `mk/main.mk`) `x * C1 / C2` folds to one `mul.s` and `x / C` to a reciprocal, so a ROM pairing
  `mul.s` with `div.s` needs a non-constant divisor: `(f32)n` off an `s32` local, which `cse` folds
  back unless a sibling arm assigns a shared variable last (S306).
- **fold-associate-constant-side** -- `GLOBAL + (elem + CONST)` reproduces `addiu rX, globreg, C`; the
  natural spelling reassociates onto the element instead.
- **narrow-type-spelling** -- `(f32)(u16)(x + 0x8000)` shortens to HImode and CSE folds it to one
  negative-immediate `addiu`, while a separate `s32 t =` keeps SImode; `u8` for a `!= 0` test plus
  `s32` for compares puts the zero-extend in another block and keeps `slti` signed.
- **ifelse-not-ternary-cse-reset** -- a multi-predecessor join resets CSE's table and re-loads live
  memory; inverted, a temp holding the arm result keeps the ROM's one load (S299).
- **add-s-negative-const-lever** -- for a ROM `add.s` with a negative FP constant, spell it `x + -C`;
  subtraction emits `sub.s` with a positive constant.
- **negative-displacement-neighbour-needs-one-symbol** -- `lw t0,-0x2B(a0)` reads global B off A's
  base only when A and B are one symbol.
- **same-field-sentinel-loop-peels-top-load** -- a loop testing one field at the top with a bottom
  sentinel peels that load; the back edge is then a terminal coin.
- **block-scoped-record-pointer-single-giv** -- for three or more fields of `ARR[i]`, a block-scoped
  `T *p = &ARR[i];` gives one base register plus displacements; `p++` splits it into two IVs, and a
  bare `ARR[i].field` gives a byte-offset one with a per-access `%hi`.
