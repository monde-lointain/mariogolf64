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

- **scope-and-live-range-steer-allocation** -- register order follows
  `floor_log2(nref)*nref/live_length` (`global.c:587`), which also tiers `local-alloc` quantities,
  spills a long-lived few-reference parameter (S289) and decides whether a copy survives
  (`global.c:790-823`, `local-alloc.c:472/1290/1587`); reuse also pulls a load forward by
  anti-dependency. Read the numbers with `tools/allocno_report.py`, compute the window the ROM's
  register implies, then pick the knob landing in it: an eighth reference, a free bound copy
  `n = bound;`, `do {} while (0)` round a subset (S292; also bars the sched hoist), a short-lived
  local per region, a split of a value spanning two loops, a temp ending a range early, or a
  block-local temp hoisted to function scope (S308). Merging is the inverse and the
  only knob when live length cannot win: one variable across two disjoint regions jumps `n_refs` a
  `floor_log2` tier, the tell being the ROM spending one register on both (S312). `find_reg` takes
  callee-saved first. **Equal `refs` and `live_length` ends this family: birth order then decides,
  so the lever is emission order** (S313). Playbook, `.greg` method and tie:
  `#loop-weight-and-live-length-regalloc-steering`.
- **aggregate-copy-scratch-clobbers** -- a struct/array copy is one `movstrsi_internal` whose four
  `match_scratch` temps take the lowest free `d` registers there, so anything live across it
  conflicts with all four; its `la` temp names the set's top (S313).
- **dead-frame-levers** -- for a frame-only diff: `s32 unused[(ROM_frame-0x18)/4]`, `str[row]` (keeps
  base and index live), or an uninit local plus `volatile s32 s = g;`. Split point: arrays slot at
  `expand_decl`, an address-taken scalar only at the first `&x` and after every function-scope
  aggregate, so reserve part in a block opened after it (S296). Slot order is ascending pseudo number
  (`reload1.c`).
- **per-region-cse-slot-base-lever** -- pass the array directly, no cached pointer, so gcc CSEs the
  base per region. The count of pointer *locals* is the pressure knob: each is a `loop.c` induction
  pointer costing a callee-saved register; an offset off a shared row pointer is not (S289).
- **fp-arg-registers-are-a-signature** -- ROM values in `$f12`/`$f14` where the build uses `$f0`/`$f2`
  mean the callee takes FP args the `extern` omits (S286).

## Scheduling

- **emission-order-placement-lever** -- placement follows source emission order: LUID in latency-1
  blocks (`sched.c`), and `loop.c` hoists invariants in loop-body order before `strength_reduce` adds
  giv inits. Knob: move the statement, or split/inline it (`off = i*stride` as its own statement
  keeps `%hi`/`addu`/`%lo`; inlined it folds into a walking-pointer giv). Whether it hoists at all is
  the same lever (`loop.c:1631`, `threshold*savings*lifetime >= insn_count`), and a use count of 2
  rather than 1 is often the whole difference (S290, S294, S300, S308, S310). `threshold` is 122 here
  less 3 per movable already moved (`loop.c:532/1719`), so movable K hoists iff
  `122 - 3K >= insn_count`; the knob is `insn_count`, from `-dL` (S312).
- **sched-tiebreak-coins** -- `rank_for_schedule` (`sched.c:2428`) sorts class then LUID (a class-1
  compute defers behind class-3 stores); `schedule_select` (`sched.c:2615`) front-loads a transfer
  over a constant load (`fabsf` sign-mask, 3 vs 2; `mtc1`-zero ties via potential-hazard). Source
  order is not always the knob: S305 moved a `div` three ways for a byte-identical object.
- **sched-bottomup-loadsplit-livelength-blockmove** -- at exact count a `s32 tmp` statement split
  forces a load interleave (sched is bottom-up); moving a statement earlier shortens a qty's live
  length. Permuter-proof.
- **do-while-zero-block-break** -- an empty `do {} while (0);` emits nothing but ends the preceding
  block, so `reorg` stops reaching past a call to annul the next branch (S288).
- **global-reread-vs-cse**, absorbing defeat-global-base-cse -- a scalar-global store does not
  constrain a later load through a pointer parameter (`true_dependence`, `sched.c:817`), so the load
  hoists. `extern s32 G[]` read as `G[0]`, a neighbour read at a known displacement, or a `*(s32 *)`
  cast on a `u8 *` byte offset restores the re-read (the cast is terminal where a param-base folds);
  the inverses are a preheader local, and a per-site `T *p = &SYM;` never reused (S301).
- **dual-offset-temps-around-a-call** -- when the ROM recomputes an index chain on both sides of a
  `jal`, write `off1 = i*K; <call>; off2 = i*K;`: gcc cannot CSE across the call, so both are
  emitted, each keeping the form that holds the symbol in the `MEM` (S310).
- **two-argument-call-temp-split** -- when both arguments of a call cross another call, compute them
  into temps first; as one expression gcc evaluates argument 0 fully and holds it across the call.


## Control flow and branch shape

- **do-while-not-equal-loop-exit-form** -- rewrite a bounded `for (i < N)` as `do {} while (i != N)`
  for a `bne` with a register bound and a tight frame; the symptom is `slti` where the ROM has `bne`.
  Choose per loop (S287: 3 do-while, 6 structured).
- **out-of-line-handler-block-branch-likely** -- a one-instruction `goto` handler folds into an
  annulled `beql`; one `goto done` replicates the return copy. Same for a switch arm's constant
  select, which is goto-proof (S275).
- **else-arm-return-vs-then-arm** -- for a far early return the else-arm form yields a plain `bc1t`
  (`jump.c:1737` cannot invert across a set-retval), and merges it into a *shared* error epilogue
  (S309); keep the final `return 0` last.
- **nonvoid-return-blocks-fallthrough-delay-steal** -- an unfilled delay slot on any branch whose
  target is the function's return block means the ROM fn returns `s32` (`reorg.c:3375/4274`),
  including a compiler-generated `switch` range check (S311). Diagnostic: `rom=N+1 mine=N` whose only
  mnemonic delta is `nop`, every later row shifted by one.
- **and-equals-zero-not-negated-and** -- `if (!(a & b))` De Morgans into a branch per operand plus
  `or`; `(a & b) == 0` keeps the value-selects and the `and`, which ROM `bc1t`+`and`+`bnez` means.
- **cross-jump-merge-point-before-store** -- the delay-slot filler names the pre-reorg order; a merge
  point at the argument move, not the store, means each arm held its own copy of the trailing calls;
  duplicate them per arm (S286).
- **jump-c-store-flag-conversions** -- jump.c:1139-1250 makes a two-value select branchless and
  re-fires every pre-reload pass, so no spelling escapes it and a ROM `andi; bnez` or `beq` plus two
  `li` reads terminal. Only escape: a `CODE_LABEL` between assignment and test, so the value is born
  a block earlier (S310).
- **gcc272-fold-range-test-slti-merge** -- adjacent `slti` tests fold to `(u32)(x-lo) < span`; a
  per-test goto defeats the merge when the ROM keeps both.

## Addressing, types and expression shape

- **operand-order-statement-split** -- which operand is computed first is structural, not spelling:
  flip `addu`/`xor` via `x = a; x ^= b;` or `T *p = base + i; p[C] = ...` (`*(p+off+C)` gives
  `addu rd,base,off`, `p[off+C]` reverses it), and precompute an index offset and a `u8 *` base into
  temps to defer a row add (`expr.c:6457`, operand 0 before operand 1).
- **division-codegen** -- no cross-block CSE, so an inline `K / x` in a branching macro emits one
  `div` per using block; one ROM `div` means a variable. `x % K` before `x / K` emits the ROM's
  non-coalesced `move`; division first coalesces. Under `-ffast-math` (`mk/main.mk`) `x * C1 / C2`
  folds to one `mul.s`, so a ROM `mul.s`+`div.s` pair needs a non-constant divisor (S305, S306).
- **fold-associate-constant-side** -- `GLOBAL + (elem + CONST)` reproduces `addiu rX, globreg, C`;
  the natural spelling reassociates onto the element.
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
