# Source levers (index)

<role>
The named source-level levers a crack slice draws on: what each one is, in one line, so a prompt can
name it and an agent can act on it. Consulted when filling `{{LEVERS}}` in `docs/fanout-prompt.md`
and when a residual class suggests a family of fixes.
</role>

<scope>
An index, deliberately not a transcription. These levers were discovered sprint by sprint and their
full derivations live in the sprint retros and, where one exists, a `docs/hazards.md` section. A name
plus a line of what it does is enough to act on or to look up.

When a lever here has no `docs/hazards.md` section and its line is too thin to act on, that is a
signal the lever should get a section -- write it the next time a sprint actually uses the lever, as
the execution loop already requires for a wall characterization. Levers nobody uses never need
writing up.
</scope>

## Levers with a full playbook

These have a `docs/hazards.md` section; read it rather than the line here.

- base register vs displacement -- `docs/hazards.md#base-register-vs-displacement`
- cross-jump tail merge -- `docs/hazards.md#cross-jump-tail-merge`
- goto loop (last resort; defeats every `loop.c` pass) --
  `docs/hazards.md#goto-loop--loopc-never-runs-defeating-strength-reduction-and-bound-hoisting`
- loop weight and live length regalloc steering -- `docs/hazards.md#loop-weight-and-live-length-regalloc-steering`
- nested-function static chain spill -- `docs/hazards.md#nested-function-static-chain-spill`
- signed-divide const quotient destination -- `docs/hazards.md#signed-divide-const-v0v1-quotient-destination`
- switch compare-chain layout -- `docs/hazards.md#switch-compare-chain-layout`
- top-tested loop goto local hoist -- `docs/hazards.md#top-tested-loop-goto-local-hoist`

## Register allocation and coloring

- **cross-call-live-range-callee-saved-lever** -- declare a post-call value *before* the call to force
  a callee-saved register; initializing it after is the inverse.
- **global-allocno-compare-livelength-biv-order** -- s-register order follows
  `floor_log2(nref)*nref/live_length` (`global.c:587`); flip it with an eighth reference, or with the
  bound-copy below.
- **bound-copy-for-live-range-placement** -- when a loop bound must be initialized early (a
  declaration initializer keeps `loop.c` from folding the entry guard), add `n = bound;` where the
  ROM's live range starts and loop on `n`. gcc folds the copy, so it is free, and the priority above
  then ranks it as the ROM does (S288).
- **local-alloc-combine-regs-block-local-temp** -- hoist a block-local temp to function scope to
  defeat the `combine_regs` tie (`local-alloc.c:472/1290/1587`).
- **do-while-doubles-reg-n-refs-qty-tier** -- a `do {} while(0)` macro loop doubles `REG_N_REFS` and
  flips the qty tier; a plain block fixes the resulting 2-register permutation. Loop notes also bar
  the sched hoist (fdlibm `GET_FLOAT_WORD`, S287).
- **variable-reuse-is-a-per-register-lever** -- reuse pulls a load forward by anti-dependency, and a
  pointer spanning two loops permutes the earlier one: split those. The inverse recolours, and is the
  main tool for a callee-saved permutation, one register at a time (S287: four reuses, one each).
- **abs-compare-form-steers-allocno** -- `(x <= -1) ? -x : x` versus `(x < 0)` flips a single magnitude
  allocno; a cheap lever to try before the permuter.
- **dead-frame-levers** -- for a frame-only diff: `s32 unused[(ROM_frame-0x18)/4]` (pure dead frame),
  `str[row]` (keeps base and index live), or an uninit local plus `volatile s32 s = g;` (reload frame,
  dead `sw $v0`; `volatile` defeats DCE).
- **per-region-cse-slot-base-lever** -- pass the array directly, with no cached pointer, so gcc CSEs the
  base per region.
- **copy-coalesce-cse-signext-terminal** -- `global.c:790-823` merges equal copies; forcing six
  registers reroutes a later read and the residual goes terminal.
- **fp-arg-registers-are-a-signature** -- ROM values in `$f12`/`$f14` where the build uses `$f0`/`$f2`
  are the FP argument registers: the callee takes FP args the `extern` omits (S286).

## Scheduling

- **sched-luid-order-inline-arg-subexpr** -- LUID is emission order in latency-1 blocks; fold a
  pre-call compute statement into the call argument to reorder register setup.
- **sched-class-tiebreak-order-coin** -- `rank_for_schedule` (`sched.c:2428`) sorts class then LUID, so
  a class-1 compute defers behind class-3 stores.
- **sched-coin-loop-preheader-order-lever** -- a prologue init or save-order coin is a `loop.c`
  preheader *placement* effect; crack it with an `off = i*STRIDE` giv.
- **sched-select-potential-hazard-coin** -- `schedule_select` (`sched.c:2615`) front-loads a transfer
  over a constant load; the `fabsf` sign-mask is the canonical case.
- **sched-bottomup-loadsplit-livelength-blockmove** -- at exact instruction count, a `s32 tmp`
  statement split forces a load interleave (sched is bottom-up); moving a statement earlier shortens a
  qty's live length. Permuter-proof.
- **fp-const-load-before-fabs** -- an FP constant load schedules before a `fabs` feeding the same
  compare (load 3 outranks fabs 2); an mtc1-zero ties via potential-hazard.
- **do-while-zero-block-break** -- an empty `do {} while (0);` emits nothing but ends the preceding
  block, so `reorg` stops reaching past a call for a later insn and annulling the next branch to
  compensate (S288). Prefer it to an empty `asm volatile`, the other zero-byte region-ender, which
  also lengthens a live range in its operand form but which the PO declined to bank (S282).
- **loop-invariant-hoist-order-preheader-regalloc** -- `loop.c` hoists invariants in loop-body emission
  order, so precomputing a division early changes preheader allocation.
- **aggregate-store-pins-pointer-load** -- a store to a scalar global does not constrain a later load
  through a pointer parameter (`true_dependence`, `sched.c:817`); typing the destination globals as
  one array restores the dependence and the ROM's load/store interleave.
- **two-argument-call-temp-split** -- when both arguments of a call each cross another call, compute
  them into temps first; as one call expression gcc evaluates argument 0 fully, `trunc.w.s` included,
  and holds it across the second call.

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
- **byte-offset-cast-defeats-base-ptr-cse** -- `*(s32 *)((u8 *)SYM + off)` forces a per-access
  `%hi`/`%lo`. Terminal only for a param-base fold.
- **mem-in-struct-index-global-cse** -- `extern s32 G[]` plus `G[0]` defeats CSE of the load and the
  `G << 2`, forcing a per-access re-read at every loop-nesting level; a cached `s32 *cnt = &G` is
  short.
- **himode-shortening-cse-neg-imm-addiu** -- `(f32)(u16)(x + 0x8000)` shortens to HImode and CSE folds it
  to one negative-immediate `addiu`; a separate `s32 t =` keeps SImode.
- **u8-s32-char-split-zero-extend** -- `u8` for the `!= 0` test plus `s32` for compares puts the
  zero-extend in another basic block and keeps `slti` signed.
- **divmod-order-quotient-coalescing** -- `x % K` before `x / K` emits the ROM's non-coalesced `move`;
  division first coalesces.
- **ifelse-not-ternary-cse-reset** -- cross-jump tail merge plus a multi-predecessor join resets CSE's
  table and re-loads live memory.
- **add-s-negative-const-lever** -- for a ROM `add.s` with a negative FP constant, spell it `x + -C`;
  subtraction emits `sub.s` with a positive constant.
- **pre-temp-defer-rowadd-lever** -- precompute the index offset and the `u8 *` base into separate temps
  (`expr.c:6457`, operand 0 before operand 1).
- **rodata-strings-as-literals-via-tu-combine** -- prefer string literals over `extern D_` symbols;
  recombine a split one-tu at `OBJCOPY_ALIGN` 4-alignment.
- **negative-displacement-neighbour-needs-one-symbol** -- `lw t0, -0x2B(a0)` reads global B off A's base
  only when A and B are one symbol.
- **same-field-sentinel-loop-peels-top-load** -- a loop testing one field at the top with a bottom
  sentinel makes gcc peel the top load; the back edge is then a terminal coin.
- **block-scoped-record-pointer-single-giv** -- for three or more fields of `ARR[i]`, a block-scoped
  `T *p = &ARR[i];` gives one base register plus displacements; `p++` splits into two induction
  variables, and a bare `ARR[i].field` gives a byte-offset one with a per-access `%hi`.
