# Source levers (index)

<role>
The named source-level levers a crack slice draws on: what each one is, in one line, so a prompt can
name it and an agent can act on it. Consulted when filling `{{LEVERS}}` in `docs/fanout-prompt.md`
and when a residual class suggests a family of fixes.
</role>

<scope>
An index, deliberately not a transcription. These levers were discovered sprint by sprint and their
full derivations live in the sprint retros and, where one exists, a `docs/hazards.md` section. This
file exists because the names were previously cited as double-bracketed wiki links into a store only
one harness can read: a Codex session or a dispatched subagent could resolve none of them. A name plus
a line of what it does is enough to act on or to look up.

When a lever here has no `docs/hazards.md` section and its line is too thin to act on, that is a
signal the lever should get a section -- write it the next time a sprint actually uses the lever, as
the execution loop already requires for a wall characterization. Levers nobody uses never need
writing up.
</scope>

## Levers with a full playbook

These have a `docs/hazards.md` section; read it rather than the line here.

- base register vs displacement -- `docs/hazards.md#base-register-vs-displacement`
- cross-jump tail merge -- `docs/hazards.md#cross-jump-tail-merge`
- loop weight and live length regalloc steering -- `docs/hazards.md#loop-weight-and-live-length-regalloc-steering`
- nested-function static chain spill -- `docs/hazards.md#nested-function-static-chain-spill`
- signed-divide const quotient destination -- `docs/hazards.md#signed-divide-const-v0v1-quotient-destination`
- top-tested loop goto local hoist -- `docs/hazards.md#top-tested-loop-goto-local-hoist`

## Register allocation and coloring

- **cross-call-live-range-callee-saved-lever** -- declare a post-call value *before* the call to force
  a callee-saved register; initializing it after is the inverse.
- **global-allocno-compare-livelength-biv-order** -- s-register order follows
  `floor_log2(nref)*nref/live_length` (`global.c:587`); flip it by adding an eighth reference.
- **local-alloc-combine-regs-block-local-temp** -- hoist a block-local temp to function scope to
  defeat the `combine_regs` tie (`local-alloc.c:472/1290/1587`).
- **do-while-doubles-reg-n-refs-qty-tier** -- a `do {} while(0)` macro loop doubles `REG_N_REFS` and
  flips the qty tier; a plain block fixes the resulting 2-register permutation.
- **one-variable-reuse-reorders-loads** -- reusing one variable for two values pulls a load forward via
  anti-dependency; split it. The inverse: reuse a dead variable to recolour.
- **reuse-one-pointer-across-loops-coloring-trap** -- one walking pointer spanning two loops permutes
  the earlier loop's registers; split it per loop.
- **abs-compare-form-steers-allocno** -- `(x <= -1) ? -x : x` versus `(x < 0)` flips a single magnitude
  allocno; a cheap lever to try before the permuter.
- **dead-frame-live-index-pressure-lever** -- an `str[row]` rewrite keeps base and index live and
  reproduces a small truly-dead reserved frame.
- **pure-dead-frame-clean-crack** -- a frame-only diff cracks via `s32 unused[(ROM_frame-0x18)/4]`.
- **dead-frame-dead-v0-store-crack** -- an uninitialized local plus `volatile s32 s = g;` reproduces a
  reload frame and a dead `sw $v0`; `volatile` defeats DCE.
- **per-region-cse-slot-base-lever** -- pass the array directly, with no cached pointer, so gcc CSEs the
  base per region.
- **copy-coalesce-cse-signext-terminal** -- `global.c:790-823` merges equal copies; forcing six
  registers reroutes a later read and the residual goes terminal.

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
- **empty-asm-volatile-sched-barrier** -- an empty `asm volatile` is the only zero-byte way to end a
  scheduling region; the operand form also lengthens a live range for regalloc.
- **loop-invariant-hoist-order-preheader-regalloc** -- `loop.c` hoists invariants in loop-body emission
  order, so precomputing a division early changes preheader allocation.

## Control flow and branch shape

- **goto-loop-defeats-loop-strength-reduction** -- a goto loop defeats every `loop.c` pass including
  the first-iteration peel; keep the bound in a variable, one per loop.
- **goto-loop-vs-structured-loop-codegen** -- a structured loop emits `NOTE_INSN_LOOP_BEG`, adding
  register weight and enabling `loop.c` induction-variable work; a forward goto fixes placement.
- **goto-is-last-resort** -- try a natural `for`/`while`/`do-while` first; reach for `goto` only when a
  structured loop provably cannot match.
- **do-while-not-equal-loop-exit-form** -- rewrite a bounded `for (i < N)` as `do {} while (i != N)` for
  a `bne` with a register bound and a tight frame. The symptom is `slti` where the ROM has `bne`.
- **out-of-line-handler-block-branch-likely** -- a one-instruction `goto` handler folds into an
  annulled `beql`; a single `goto done` replicates the return copy.
- **else-arm-return-vs-then-arm** -- for a far early return the else-arm form yields a plain `bc1t`
  (`jump.c:1737` cannot invert across a set-retval); keep the final `return 0` last.
- **nonvoid-return-blocks-fallthrough-delay-steal** -- a lone epilogue-branch nop means the ROM function
  returns `s32` (`reorg.c:3375/4274`).
- **value-select-branch-likely-on-switch-default** -- a constant-select branch-likely extends to a
  switch arm, and is goto-proof.
- **store-flag-single-bit-terminal-wall** -- a terminal `(x & bit) ? 1 : 0` folds to `lhu; srl` where the
  ROM keeps `andi; bnez`. Permuter-denied.
- **gcc272-fold-range-test-slti-merge** -- adjacent `slti` tests fold to `(u32)(x-lo) < span`; a per-test
  goto defeats the merge when the ROM keeps both.

## Addressing, types and expression shape

- **commutative-operand-order-statement-split** -- flip `addu`/`xor` operand order via `x = a; x ^= b;`
  or `T *p = base + i; p[C] = ...`. RTL-canonical: change structure, not spelling.
- **fold-associate-constant-side** -- `GLOBAL + (elem + CONST)` reproduces `addiu rX, globreg, C`; the
  natural spelling reassociates onto the element instead.
- **byte-offset-cast-defeats-base-ptr-cse** -- `*(s32 *)((u8 *)SYM + off)` forces a per-access
  `%hi`/`%lo`. Terminal only for a param-base fold.
- **mem-in-struct-index-global-cse** -- `extern s32 G[]` plus `G[0]` defeats CSE of both the load and
  the `G << 2`, forcing a per-access re-read.
- **array-element-form-for-multilevel-bound** -- the same form forces a re-read at every nesting level;
  a cached `s32 *cnt = &G` comes out short.
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
