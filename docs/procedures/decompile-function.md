---
type: Procedure
name: decompile-function
title: "Decompile a function"
description: "Decompile a specified Mario Golf 64 assembly function into matching C, or improve an existing partial match. Use for per-function matching, integration and verification; not general build or tooling changes."
tags: [decompile, workflow]
---

Run commands from the repository root unless a step says otherwise. The attempt and stop rules below are mandatory.

# 1. Bootstrap and match

```bash
./tools/decomp-env --bootstrap-only <function name>
```

This creates `nonmatchings/<function name>[-N]/`; pass `--id <ID>` to control the suffix (nigel tasks pass `$TASK_ID`). Details of what the directory contains: [decomp-env](/tools/decomp-env.md). Move into it and read its `AGENTS.md`, which carries the attempt loop.

In that directory run `./build.sh <attempt>.c`. It compiles with the profile of the file that owns the function and scores the object against the target. Put each attempt in a new self-contained file (`base_1.c`, `base_2.c`, ...); only `base_<N>.c` names are counted in `match_log.txt`. How to approach the function: [matching-method](matching-method.md). Kinds of mismatch and compiler behavior: [KMC GCC learnings](/reference/kmc-gcc/index.md).

# 2. Integrate a 100% match

Replace the function's `INCLUDE_ASM("asm/nonmatchings/<seg>/<file>", <function name>);` line in its C file with the real code. Define it under exactly `<function name>`, even when a caller declares the same address under another name; renaming is a separate task (the rename-global skill).

* Update the rest of the project to build. If a header declares the function, update it; callers teach the correct argument and return types. Never fall back to `void *`.
* Search `src/` and `include/` for existing declarations of every function and struct involved; do not add duplicates ([redundant declarations](/project/code-quality.md)).
* Commit any harness fix first and separately (see below), while `src/` still builds.

Then run `./tools/build-and-verify.sh`. The match is not done until it succeeds. If the checksum fails, rebuild and diff every function in the modified files with `python3 tools/asm-differ/diff.py --no-pager <function>`, focusing on functions that use structs you changed. See [asm-differ](/tools/asm-differ.md).

# 3. Commit

* **Perfect match:** commit with the subject `Decompile <function name>` and a body saying what the function does and how it was matched. Respect the pre-commit hook; a failure means the C change is wrong.
* **Improved partial match** (better than any recorded one, or none recorded): record it next to the include and keep the include on one line, because Splat reads the function name from it.

```c
// your_function best match: XX%
INCLUDE_ASM("asm/nonmatchings/main/file", your_function);

#ifdef NON_MATCHING
// your non-matching function
#endif
```

A `NEAR_MISS.md` handoff note in `nonmatchings/<function>/` makes `list_decomp_candidates.py` skip the function ([selection tools](/tools/selection-and-status.md)). You are then done; do not hunt for the next function.

# Harness friction

If `build.sh`, `tools/decomp-env`, another tool or the policy hook is wrong or in the way, fix it instead of working around it ([changing the harness](/harness/changing-the-harness.md)). Files in the environment that come from `tools/decomp-env-template/` are symlinks, so editing them edits the template for every environment. Commit the fix by itself with a subject of at most 50 characters starting `harness: ` (lowercase, present tense), then continue. A harness fix does not reset or extend the attempt count.

# Stop rules

**Mandatory.** Stop at each point below and make no further attempt after it.

* A verified 100% match (`Verified exact match: yes`). Integrate it (section 2) and record it as `matched`.
* A line starting `STOP:` from `build.sh`. It prints once the best score has not improved for the stall limit of `base_<N>.c` attempts: 20 by default, or the tier's `stall_limit` in `.run.json`; `MG64_STALL_LIMIT` overrides both. It repeats on every later attempt. Record it as `stalled`.
* The end of your time budget. Do not record it: `difficulty_ledger.py harvest` writes a `timeout` record for an environment that has none.
* Any other point where you cannot make progress without a change this procedure does not allow. Record it as `gave_up` with the reason.

At every stop, record the outcome with the command the `STOP:` line prints (run from the environment directory). Otherwise run this from the repository root:

```bash
python3 tools/difficulty_ledger.py record <function> --outcome <outcome> --reason "<one line>"
```

Outcomes: `matched` (100% and `./tools/build-and-verify.sh` passes after integration); `near_miss` (the best partial match is recorded and `NEAR_MISS.md` written, section 3); `stalled` (the `STOP:` line); `integration_failed` (the environment matches but the integrated build fails); `gave_up` (any other stop above). `timeout` comes from `harvest`.

After a stop, report: best score and which file, approaches tried, analysis of remaining differences, and whether the remainder looks solvable or needs a different strategy.

The permuter is not part of this procedure; it is an offline tool for people, see [decomp-permuter](decomp-permuter.md).
