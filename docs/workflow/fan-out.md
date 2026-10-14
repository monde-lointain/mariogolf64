---
type: Workflow
title: Subagent fan-out
description: Defines when to delegate decompilation work, how to isolate it, and what outcomes justify a fan-out.
---

# Subagent fan-out

Work inline unless at least two independent analysis slices justify separate contexts. Read the
selected functions and their carried evidence before dispatch; do not fan out routine selection,
small leaves, or checks settled by the ROM verifier.

**Navigate:** [Dispatch contract](#dispatch-contract) ·
[Expected-value guard](#what-a-fan-out-is-worth) · [Wall classes](#wall-classes-a-fan-out-commonly-returns).

## Dispatch contract

Use the active native `task` schema. Put the shared goal, resource ownership, and interfaces in
`context`; give each task its target files, non-goals, prior residual hypothesis, relevant source
levers, and required evidence. `solutionSpace` describes the open reasoning, not workload size.
Do not copy a fixed prompt or create project agents for roles the harness already provides.

The parent owns candidate/source edits, compiles, builds, Ghidra MCP, integration, and ROM
verification. Workers analyze independent functions, coupled nested pairs, or distinct residual
angles (at most two per function), and return proposed changes. They do not build, run checks,
format, use MCP, or launch background permuters. The parent measures proposals serially with the
function's ROM compiler profile and returns evidence when another analysis iteration is needed.
Candidate artifacts stay under `nonmatchings/<fn>/`; never share scratch directories.

Require a usable hand-off, not an iteration transcript:

- Best candidate or proposed patch with every required type, extern, and helper; measured base,
  instruction counts, frame, residual, and the command/object behind each measurement.
- A crack or a carry hypothesis supported by measurements and compiler `file:line` citations where
  available. Separate observations from attribution; the parent records unresolved evidence in
  `docs/wip/<fn>.near-match.md`. Only `tools/verify-rom.sh` can authorize banking.
- Integration hazards: conflicting host declarations, actual relocations (`objdump -r`, not
  guessed from `lui`), emitted `.rodata` and duplicate pools, and nested-function coupling.
- Source quality: no empty-asm barrier as a bank candidate; decode raw display-list words against
  SDK macros before integration. See the [oracle table](loop.md#oracles) and
  [source levers](../levers.md) for measurement limits and transformations.

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
282. Locate the cited sprint heading and read only that excerpt; the prepend-only record is not
an onboarding document.
