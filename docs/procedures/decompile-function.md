---
type: Procedure
name: decompile-function
title: "Decompile a function"
description: "Decompile one Mario Golf 64 assembly function into matching C, or improve an existing partial match; use when asked to decompile, match or improve a function."
tags: [decompile, workflow]
---

Run commands from the repository root unless a step says otherwise. Work on the one function you were given, and stop at the first stop rule (section 4) that applies.

# 1. Bootstrap and match

```bash
./tools/decomp-env --bootstrap-only <function name>
```

This creates `nonmatchings/<function name>[-N]/`; `--id <ID>` sets the suffix (nigel tasks pass `$TASK_ID`), and [decomp-env](/tools/decomp-env.md) lists what it contains. Its `AGENTS.md` carries the attempt loop: follow it there. Approach and plateaus: [matching-method](/procedures/matching-method.md); compiler behavior behind a diff you cannot explain: [KMC GCC learnings](/reference/kmc-gcc/index.md).

# 2. Integrate a 100% match

On a match `build.sh` prints `Verified exact match: yes` and a `NEXT:` line.

1. Replace the function's `INCLUDE_ASM("asm/nonmatchings/<seg>/<file>", <function name>);` line in its C file with the matching code, defined under exactly `<function name>` even when a caller uses another name; renaming is a separate task (the rename-global skill).
2. Use the argument and return types the assembly shows. A prototype or a caller's call that disagrees (often a guessed `(void)`) is wrong: correct it, and `build-and-verify.sh` proves the callers still match. Reuse the declarations and structs already in `src/` and `include/`, and update a header that declares the function rather than adding a second declaration ([code quality](/project/code-quality.md)).
3. Run `./tools/build-and-verify.sh`. If the checksum fails, diff each function in the changed files with `python3 tools/asm-differ/diff.py --no-pager <function>`, starting with those that use structs you changed ([asm-differ](/tools/asm-differ.md)).

`BUILD SUCCEEDED` completes the match. Under nigel, stop there: nigel verifies, commits and records it. In a hand run, commit with the subject `Decompile <function name>` and a body saying what the function does and how it was matched; a pre-commit hook failure means the C change is wrong.

# 3. Record a partial match

When you stop without a match and your best attempt beats the score recorded for the function (or none is recorded), write `NEAR_MISS.md` in the environment: the best file and score, the remaining differences, and what to try next. The note makes [selection tools](/tools/selection-and-status.md) skip the function. In a hand run, also keep the C next to the include, guarded so a `NON_MATCHING` build has exactly one definition; the include stays on one line because Splat reads the function name from it:

```c
#ifdef NON_MATCHING
// your non-matching function
#else
// your_function best match: XX%
INCLUDE_ASM("asm/nonmatchings/main/file", your_function);
#endif
```

# 4. Stop rules

Stop at the first of these and make no further attempt:

* **Matched:** `build-and-verify.sh` succeeds after integration (section 2).
* **Stalled:** `build.sh` prints a line starting `STOP:`. The best score has not improved for the stall limit of `base_<N>.c` attempts. `MG64_STALL_LIMIT` overrides the limit; otherwise it is `stall_limit` in `.run.json`, default 20 ([difficulty ledger](/tools/difficulty-ledger.md) has tiers and fields).
* **Integration failed:** the environment matches but `build-and-verify.sh` cannot be made to pass. Restore the C file.
* **Gave up:** the only change left is one this procedure forbids: inline assembly, or an unused or `volatile` local other than the two in [stack frames](/reference/kmc-gcc/registers-and-scheduling.md#stack-frames) (a nested function's static-chain stand-in, unused frame space). Register allocation, scheduling or a low `Time left` are not reasons: keep attempting until `STOP:`.

`difficulty_ledger.py harvest` records matched, stalled, near-miss and timed-out runs from the environment. Record the other two yourself, from the repository root:

```bash
python3 tools/difficulty_ledger.py record <function> --outcome <integration_failed|gave_up> --reason "<one line>"
```

Then report in at most 10 lines: best score and file, approaches tried, the remaining differences, and whether they look solvable.

# Harness friction

If `build.sh`, `tools/decomp-env`, another tool or the policy hook is wrong or in the way, fix it instead of working around it, commit the fix on its own as [changing the harness](/harness/changing-the-harness.md) describes, then return to the function. Files in the environment that come from `tools/decomp-env-template/` are symlinks, except `AGENTS.md`, which is rendered per environment; editing a symlink edits the template for every environment. A harness fix does not reset or extend the attempt count.
