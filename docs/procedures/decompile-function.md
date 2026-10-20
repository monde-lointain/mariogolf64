---
type: Procedure
name: decompile-function
title: "Decompile a function"
description: "Decompile one Mario Golf 64 assembly function into matching C, or improve an existing partial match; use when asked to decompile, match or improve a function."
tags: [decompile, workflow]
---

Run commands from the repository root. Work on the one function you were given.

# 1. Bootstrap

```bash
./tools/decomp-env --bootstrap-only [--id <ID>] [--refs '<names>'] <function name>
```

This creates `nonmatchings/<function name>[-N]/` ([decomp-env](/tools/decomp-env.md) lists its contents); `--id` sets the suffix (nigel tasks pass `$TASK_ID`) and `--refs` overrides the reference functions, which default to the five most similar matched ones. It renders the environment's `AGENTS.md` from `tools/decomp-env-template/AGENTS.md`, with the function's callers, declarations, reference functions, earlier ledger attempts and `base.c`'s compile result already filled in.

# 2. Follow the brief

The environment's `AGENTS.md` is the whole per-function procedure: the attempt loop, how to integrate a match, and the stop rules. nigel runs rely on it alone; approach and plateaus are in [matching-method](/procedures/matching-method.md), compiler behavior behind a diff in [KMC GCC learnings](/reference/kmc-gcc/index.md).

The brief's stop rule uses the environment's [stall limit](/tools/difficulty-ledger.md#stall-limit). `difficulty_ledger.py harvest` records matched, stalled, near-miss and timed-out runs from the environment, so the agent records only `integration_failed` and `gave_up`. A `NEAR_MISS.md` note makes [selection tools](/tools/selection-and-status.md) skip the function.

# 3. Hand runs

Under nigel the run ends at `BUILD SUCCEEDED` with the change uncommitted: [verify-candidate](/tools/verify-candidate.md) checks it, then the task's `success_command` commits it. In a hand run:

* After `BUILD SUCCEEDED`, commit with the subject `Decompile <function name>` and a body saying what the function does and how it was matched. A failing pre-commit hook means the change does not match ([git hook and CI](/harness/git-hook-and-ci.md)).
* With a near miss, keep the C next to the include, guarded so a `NON_MATCHING` build has exactly one definition; the include stays on one line ([code-quality](/project/code-quality.md#file-structure)):

```c
#ifdef NON_MATCHING
// your non-matching function
#else
// your_function best match: XX%
INCLUDE_ASM("asm/nonmatchings/main/file", your_function);
#endif
```

The function keeps its asm name even when a caller uses another; renaming is a separate task ([rename-global](/procedures/rename-global.md)).

# Harness friction

If `build.sh`, `tools/decomp-env`, another tool or the policy hook is wrong or in the way, fix it instead of working around it, commit the fix on its own as [changing the harness](/harness/changing-the-harness.md) describes, then return to the function. Files in the environment that come from `tools/decomp-env-template/` are symlinks, except `AGENTS.md`, which is rendered per environment; editing a symlink edits the template for every environment.
