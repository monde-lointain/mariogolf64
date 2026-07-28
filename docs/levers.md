# Source levers (index)

<role>
The named source-level levers a crack slice draws on: what each one is, in one line, so a prompt can
name it and an agent can act on it. Consulted when filling `{{LEVERS}}` in `docs/fanout-prompt.md`
and when a residual class suggests a family of fixes.
</role>

<scope>
An index, not a transcription: full derivations live in the sprint retros and, where one exists, a
`docs/hazards.md` section.
</scope>

## Levers with a full playbook

These have a `docs/hazards.md` section; read it rather than the line here.

- base register vs displacement -- `docs/hazards.md#base-register-vs-displacement`
- cross-jump tail merge -- `docs/hazards.md#cross-jump-tail-merge`
- goto loop (last resort; defeats every `loop.c` pass) --
  `docs/hazards.md#goto-loop--loopc-never-runs-defeating-strength-reduction-and-bound-hoisting`
- loop weight and live length regalloc steering -- `docs/hazards.md#loop-weight-and-live-length-regalloc-steering`
- named aggregate local block move -- `docs/hazards.md#named-aggregate-local-extra-block-move`
- nested-function static chain spill -- `docs/hazards.md#nested-function-static-chain-spill`
- signed-divide const quotient destination -- `docs/hazards.md#signed-divide-const-v0v1-quotient-destination`
- switch compare-chain layout -- `docs/hazards.md#switch-compare-chain-layout`
- top-tested loop goto local hoist -- `docs/hazards.md#top-tested-loop-goto-local-hoist`

## Register allocation and coloring

- **cross-call-live-range-callee-saved-lever** -- declare a post-call value *before* the call to force
  a callee-saved register; initializing it after is the inverse.
- **global-allocno-compare-livelength-biv-order** -- register order follows
  `floor_log2(nref)*nref/live_length` (`global.c:587`), which also tiers `local-alloc` quantities and
  spills a long-lived few-reference parameter (S289). Read it with `tools/allocno_report.py`, compute
  the window the ROM's register implies, then pick the knob landing in it: an eighth reference, a free
  bound copy `n = bound;` (S288), or a `do {} while (0)` note round a subset, reweighting only those
  refs (S292; also bars the sched hoist, S287). `find_reg` picks callee-saved first.
- **scope-and-live-range-steer-allocation** -- scope and live range pick a value's register and
  decide whether its copy survives: equal copies merge when both pseudos prefer one register
  (`global.c:790-823`), `combine_regs` ties others (`local-alloc.c:472/1290/1587`), and reuse pulls a
  load forward by anti-dependency. Knobs: name a temp to end a live range early rather than inlining
  it (S293); hoist a block-local temp to function scope, or the reverse when a function-scope value
  set in two arms becomes a global allocno that misses `combine_regs` (S299); split a value spanning
  two loops, but only what must die between them -- one the ROM keeps in one register across both is
  one pseudo (S295). The inverse recolours one register at a time (S287). Terminal only when six
  forced registers reroute a read.
- **dead-frame-levers** -- for a frame-only diff: `s32 unused[(ROM_frame-0x18)/4]` (pure dead frame),
  `str[row]` (keeps base and index live), or an uninit local plus `volatile s32 s = g;` (reload frame,
  dead `sw $v0`; `volatile` defeats DCE). Split point is the second knob: arrays slot at
  `expand_decl`, a `(void)&x` local only at `put_var_into_stack`, so reserve part in a block opened
  after it to land it on the ROM's offset (S296). Slot *order* is the third: `reload1.c` assigns
  slots in ascending pseudo number, so an `@F<dec>(sp)`-only residual is a numbering question --
  declaring a temp in a block that opens one statement early creates its pseudo there (S302).
- **per-region-cse-slot-base-lever** -- pass the array directly, no cached pointer, so gcc CSEs the
  base per region. The count of pointer *locals* is the pressure knob: each is a `loop.c` induction
  pointer costing a callee-saved register, an offset off a shared row pointer is not (S289).
- **fp-arg-registers-are-a-signature** -- ROM values in `$f12`/`$f14` where the build uses `$f0`/`$f2`
  are FP argument registers: the callee takes FP args the `extern` omits (S286).

## Scheduling

- **emission-order-placement-lever** -- placement follows source emission order: LUID in latency-1
  blocks (`sched.c`), and `loop.c` hoists invariants in loop-body order before `strength_reduce` adds
  giv inits. Remove a hoist by spelling the invariant as its own statement first (S300). Knobs: fold
  a pre-call compute into the call argument; `off = i*STRIDE` as its own statement (inlined, gcc
  folds the symbol into a walking-pointer giv, losing `%hi`/`addu`/`%lo`, S290); leave a row origin
  inline (`i * 6`) when the ROM births givs in body order (S294).
- **sched-tiebreak-coins** -- `rank_for_schedule` (`sched.c:2428`) sorts class then LUID (a class-1
  compute defers behind class-3 stores); `schedule_select` (`sched.c:2615`) front-loads a transfer over
  a constant load (`fabsf` sign-mask).
- **sched-bottomup-loadsplit-livelength-blockmove** -- at exact instruction count, a `s32 tmp`
  statement split forces a load interleave (sched is bottom-up); moving a statement earlier shortens a
  qty's live length. Permuter-proof.
- **fp-const-load-before-fabs** -- an FP constant load schedules before a `fabs` feeding the same
  compare (load 3 outranks fabs 2); an mtc1-zero ties via potential-hazard.
- **do-while-zero-block-break** -- an empty `do {} while (0);` emits nothing but ends the preceding
  block, so `reorg` stops reaching past a call for a later insn and annulling the next branch to
  compensate (S288).
- **aggregate-store-pins-pointer-load** -- a scalar-global store does not constrain a later load
  through a pointer parameter (`true_dependence`, `sched.c:817`); typing those globals as one array
  restores the dependence and the ROM's load/store interleave. Same on the `loop.c` side: as scalars
  they read loop-invariant against `Gfx *` stores, so the load and its expressions hoist into
  `$f20`/`$f22`; `G[]` + `[0]` restores the reload (S301). Inverse: a global read in a loop storing
  through a pointer is not invariant, so the reload forces a second IV -- copy it to a preheader
  local (S294).
- **two-argument-call-temp-split** -- when both arguments of a call each cross another call, compute
  them into temps first; as one expression gcc evaluates argument 0 fully and holds it across the
  second call.

## Control flow and branch shape

- **do-while-not-equal-loop-exit-form** -- rewrite a bounded `for (i < N)` as `do {} while (i != N)` for
  a `bne` with a register bound and a tight frame. The symptom is `slti` where the ROM has `bne`.
  Choose per loop (S287: 3 do-while, 6 structured).
- **out-of-line-handler-block-branch-likely** -- a one-instruction `goto` handler folds into an
  annulled `beql`; a single `goto done` replicates the return copy.
- **else-arm-return-vs-then-arm** -- for a far early return the else-arm form yields a plain `bc1t`
  (`jump.c:1737` cannot invert across a set-retval); keep the final `return 0` last.
- **nonvoid-return-blocks-fallthrough-delay-steal** -- a lone epilogue-branch nop means the ROM function
  returns `s32` (`reorg.c:3375/4274`).
- **value-select-branch-likely-on-switch-default** -- a constant-select branch-likely extends to a
  switch arm, and is goto-proof.
- **cross-jump-merge-point-before-store** -- the delay-slot filler names the pre-reorg order; a merge
  point at the argument move, not the store, means each arm held its own copy of the trailing calls --
  duplicate them per arm (S286).
- **store-flag-single-bit-terminal-wall** -- a terminal `(x & bit) ? 1 : 0` folds to `lhu; srl` where the
  ROM keeps `andi; bnez`. Permuter-denied.
- **gcc272-fold-range-test-slti-merge** -- adjacent `slti` tests fold to `(u32)(x-lo) < span`; a per-test
  goto defeats the merge when the ROM keeps both.

## Addressing, types and expression shape

- **commutative-operand-order-statement-split** -- flip `addu`/`xor` operand order via `x = a; x ^= b;`
  or `T *p = base + i; p[C] = ...`. RTL-canonical: change structure, not spelling.
  `*(p+off+C)` gives `addu rd,base,off`; `p[off+C]` reverses it.
- **fold-associate-constant-side** -- `GLOBAL + (elem + CONST)` reproduces `addiu rX, globreg, C`; the
  natural spelling reassociates onto the element instead.
- **defeat-global-base-cse** -- per-access re-read: `*(s32 *)((u8 *)SYM + off)` on the address
  (param-base fold aside, terminal), `extern s32 G[]` + `G[0]` on the value. Inverse: per-site
  `T *p = &SYM;`, never reused (S291).
- **himode-shortening-cse-neg-imm-addiu** -- `(f32)(u16)(x + 0x8000)` shortens to HImode and CSE folds it
  to one negative-immediate `addiu`; a separate `s32 t =` keeps SImode.
- **u8-s32-char-split-zero-extend** -- `u8` for the `!= 0` test plus `s32` for compares puts the
  zero-extend in another basic block and keeps `slti` signed.
- **divmod-order-quotient-coalescing** -- `x % K` before `x / K` emits the ROM's non-coalesced `move`;
  division first coalesces.
- **ifelse-not-ternary-cse-reset** -- a multi-predecessor join resets CSE's table and re-loads live
  memory; inverted, a temp holding the arm result keeps the ROM's single load (S299).
- **add-s-negative-const-lever** -- for a ROM `add.s` with a negative FP constant, spell it `x + -C`;
  subtraction emits `sub.s` with a positive constant.
- **pre-temp-defer-rowadd-lever** -- precompute the index offset and the `u8 *` base into separate temps
  (`expr.c:6457`, operand 0 before operand 1).
- **negative-displacement-neighbour-needs-one-symbol** -- `lw t0, -0x2B(a0)` reads global B off A's base
  only when A and B are one symbol.
- **same-field-sentinel-loop-peels-top-load** -- a loop testing one field at the top with a bottom
  sentinel makes gcc peel the top load; the back edge is then a terminal coin.
- **block-scoped-record-pointer-single-giv** -- for three or more fields of `ARR[i]`, a block-scoped
  `T *p = &ARR[i];` gives one base register plus displacements; `p++` splits into two induction
  variables, and a bare `ARR[i].field` gives a byte-offset one with a per-access `%hi`.
