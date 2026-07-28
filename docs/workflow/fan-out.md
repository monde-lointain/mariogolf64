# Subagent fan-out

<role>
When to dispatch subagents over a decomp pack, how many, and what a fan-out is expected to return.
Read this at the plan gate when choosing between another smallest-first continuation and an
escalation slice, and in the execution loop when opening a crack slice. The prompt each dispatched
agent receives is `docs/fanout-prompt.md`.
</role>

## When to fan out

| Situation | Action | Heads | Expected yield |
| --- | --- | --- | --- |
| One tractable smallest-first leaf | Work inline | 0 | -- |
| Context gathering: `BACKLOG.md`, `pick_target.py`, hazard sections, `.s` reads | Read in parallel, inline | 0 | -- |
| Confirming a result | `tools/verify-rom.sh` | 0 | The ROM oracle is the verifier. Never delegate a check the oracle settles. |
| A large classical pack's hard tail, cheap leaves already banked | Isolated per-function iterators | one per hard standalone, or per nested pair | matching C plus struct/extern additions per crack; a `docs/wip/<fn>.near-match.md` per carry |
| An all-FP one-tu tail | Isolated iterators with the FP lever set | one per function | mostly characterization, occasionally an unexpected match |
| A c-stub continuation whose next vein is suspected regalloc- or ABI-walled | gcc-source crackers, dispatched up front rather than iterate-then-permuter sequentially | one per target | a definitive per-function verdict: byte-match, or a wall with the diverging pass named at `file:line` |
| A plateaued tail whose smallest functions carry fully-RE'd near-match comments or a `file:line` "proven wall" verdict, especially when a new structural lever landed this sprint or the carry reads as a register-allocation wall | gcc-source crackers, to crack. This outranks "prefer a fresh pack". | one per documented wall | a sorted tail: cracks with a cited lever, plus terminal verdicts with a cited pass |
| One hard function with several coupled residuals (count deficit, register permutation, schedule coin, frame delta) | Crackers from distinct angles on the same function | one per residual angle, cap 2 | each sub-residual cracked with a cited lever, leaving at most one terminal coin |
| Any fan-out whose residual could be assembler-side | Re-run the build's own compile line with `-S` into a scratch dir first: if the divergence is already in gcc's text, the assembler is out with no agent spent. Only if that is inconclusive, add exactly one binutils rule-out | 0 or 1 | the assembler exonerated or implicated; either answer redirects the gcc agents |
| A plateaued mined pack whose smallest remaining functions carry wall-class tells but no prior documentation | Do not fan out. Prefer a fresh pack or an escalation slice. | 0 | another smallest-first continuation yields characterizations, not banks |

## How many

One agent per independently-compilable unit of work, plus at most one binutils rule-out.

For distinct functions the unit is the function: `make nonmatching-func FUNC=<f>` compiles a
per-function `nonmatchings/<f>/base.c` to its own `nonmatchings/<f>/current.o` and reads the shared
`build/asm/<seg>.o` reference read-only, so `tools/decomp_loop.py` on distinct functions is
parallel-safe. For a single hard function the unit is the residual angle, capped at two.

Never put two agents on one `nonmatchings/<fn>/` directory: it is the permuter's scratch dir and is
not shareable. Never spawn an agent to verify.

Each agent costs a full context and serializes at integration: the orchestrator holds every `src/`
edit and every full `make`, and never runs a full `make` while an isolated compile or a background
permuter is in flight, because that races the shared `build/`. Spawn only for a compile-isolated unit
you cannot fold into the current iteration.

## What a fan-out is worth

<!-- load-bearing: expected-value guard, do not compress -->

**Do not price a fan-out at three-for-three.** S232's tail was `global.c` reference-count steerable
and cracked 3/3; S233's constant walls were block-local `local-alloc.c` scheduler coins, which the
`||` reference-count lever does not reach, and that fan-out returned 1 crack plus 2 pass-cited
terminal verdicts. A no-lever verdict carrying a `file:line` citation is a real deliverable: it
retires the wall so no later sprint re-grinds it. The job is to sort the tail into {crack,
terminal-verdict}, not to crack everything.

<!-- load-bearing: expected-value guard, do not compress -->

**A prior wall verdict is a hypothesis, including one carrying a pass citation.** The citation may
name the right pass and still miss the source lever that steers it. S260's
`find_keyframe_offset_by_tag` carried the tree's strongest terminal verdict, a source dive citing
`loop.c:505-545`'s first-iteration peel, and it banked: a goto loop removes `loop.c` from the picture
entirely, peel included. The more source-dive backing a verdict has, the more precisely it names the
pass a new structural lever can now sidestep. So when a sprint gains a new structural lever, sort the
carries whose cited pass that lever disables to the front, however strongly worded their verdict.

**A verdict returned mid-sprint gets one orchestrator read for the unmeasured side of its own claim,
before it is accepted.** The rule above covers inherited verdicts; this one covers the agent
still running. S300's `func_8006BC80` came back at 2/474 words with correct citations
(`loop.c:1706` hoist placement, a `sched.c rank_for_schedule` LUID tie, the giv escape closed at
`loop.c:3805`) and twelve alternatives measured -- but it had only proved that the *other* insn
cannot move after the hoist, never that the hoist could be removed. It could: one source statement
took the leaf to 0. Ask what the verdict did not measure; it costs one message against three rounds
of the agent's work.

**Reach exact instruction count before arguing pressure.** A register permutation called terminal on a
body that was never at exact count is usually not terminal. Materialize every value the ROM hoists or
reorders as a source temp -- a product, a difference, a loop-invariant, a deferred param copy -- and
only quote a live-length or "needs an Nth register" argument once the count matches. Two S272 crack
slices banked this way against strongly-worded terminal verdicts. See
`docs/hazards.md#pervasive-regalloc-classical-main`.

**An apparent pressure or spill wall is often a GCC nested function.** Nested `inline` helpers that
reference the parent's params as free variables force `put_var_into_stack` plus the arg pointer, which
reads exactly like a spill. Rule it out with `pick_target.py --nested-check` before pricing such a body
a pressure wall. See `docs/hazards.md#nested-function-static-chain-spill`.

## Wall classes a fan-out commonly returns

These are the classes a plateaued mid-logic tail concentrates on. Each iterates to a fully-RE'd
near-match that does not bank and is not permuter-reachable, so recognize them early and carry:
`docs/hazards.md#base-register-vs-displacement`,
`docs/hazards.md#indexed-vs-pointer-loop-strength-reduction`,
`docs/hazards.md#value-select-if-else-vs-branch-likely`,
`docs/hazards.md#local-alloc-qty-permutation`.

Two that a fan-out has repeatedly cracked rather than confirmed:
`docs/hazards.md#delay-slot-fill-across-call` (a `void` to `s32` return-reg liveness change) and
`docs/hazards.md#cross-jump-tail-merge` (goto-split the return tails).

The named source levers a crack slice draws on are in `docs/levers.md`.

## Case law

The reasoning above was derived sprint by sprint. Each sprint's digest is richer than any summary of
it, so the digests are the record rather than a retelling here.

Case law: `RETRO.md`, sprints 184, 206, 218, 224, 232, 233, 235, 250, 260, 272, 274, 275, 276, 280,
282. Read one with `grep -n "^## Sprint <N> " RETRO.md`, then `Read` at that offset with `limit: 40`.
`RETRO.md` is prepend-only, so a low sprint number is deep in the file; do not read it whole.
