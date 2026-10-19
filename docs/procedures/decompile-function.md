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

Replace the function's `INCLUDE_ASM("asm/nonmatchings/<seg>/<file>", <function name>);` line in its C file with the real code.

* Update the rest of the project to build. If a header declares the function, update it; callers teach the correct argument and return types. Never fall back to `void *`.
* Search `src/` and `include/` for existing declarations of every function and struct involved; do not add duplicates ([redundant declarations](/project/code-quality.md)).
* If the function lives in an assembly-only segment (`asm/<rom offset>.s`, no C file), first split that segment into a C subsegment in `mariogolf64.yaml` and keep the other functions as `INCLUDE_ASM` stubs ([segment kinds](/reference/splat-yaml.md)).
* Commit any harness fix first and separately (see below), while `src/` still builds.

Then run `./tools/build-and-verify.sh`. The match is not done until it succeeds. If the checksum fails, rebuild and diff every function in the modified files with `python3 tools/asm-differ/diff.py --no-pager <function>`, focusing on functions that use structs you changed. See [asm-differ](/tools/asm-differ.md).

# 3. Commit

* **Perfect match:** commit with a message that says what was matched and how. Respect the pre-commit hook; a failure means the C change is wrong.
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

If `build.sh`, `tools/decomp-env`, another tool or the policy hook is wrong or in the way, fix it instead of working around it ([changing the harness](/harness/changing-the-harness.md)). Files in the environment that come from `tools/decomp-env-template/` are symlinks, so editing them edits the template for every environment. Commit the fix by itself with the `harness:` prefix, then continue. A harness fix does not reset or extend the attempt count.

# Stop rules

**Mandatory.**

* Stop at a verified 100% match (`Verified exact match: yes`).
* `build.sh` prints `No progress in N attempts (best: X% at <file>)` once the best score is 20 or more `base_<N>.c` attempts old. Treat that line as the stop signal; do not make another attempt. (The script words it as "consider stopping"; the project rule is that you stop.)
* After the stall line, report: best score and which file, approaches tried, analysis of remaining differences, and whether the remainder looks solvable or needs a different strategy.
