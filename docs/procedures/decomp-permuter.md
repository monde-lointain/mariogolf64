---
type: Procedure
name: decomp-permuter
title: "Run the permuter"
description: "Decomp-Permuter is a tool that automatically permutes C files to better match a target binary. Use this skill when you are decompiling a function and it is almost matching except for some register differences (i.e. 95%+)."
tags: [decompile, permuter]
---

# Sanity check

Stop before running it unless both hold:

* No significant control-flow or functional difference remains between your C and the target. The permuter does not change control flow, and trying anyway risks a local optimum.
* You have already improved the function yourself. A 95%+ match handed to you has probably been permuted already; you were given it *because* the permuter failed.

# Run

From the repository root, compute the time budget first when running under nigel (`NIGEL_TIMEOUT_DEADLINE_UNIX` is set), then run with a **literal numeric** timeout:

```bash
printf 'max permuter timeout: %ss\n' "$((NIGEL_TIMEOUT_DEADLINE_UNIX - $(date +%s) - 120))"
timeout 300s ./tools/permuter --source-file nonmatchings/<function>/base_N.c <function>
```

* The [policy hook](/harness/policy-hook.md) blocks any permuter run not under `timeout`, and checks the command before shell variables expand, so write `300s`, not `$T`. It also requires the timeout to end at least 120 s before `NIGEL_TIMEOUT_DEADLINE_UNIX`. If the remaining budget is too small for a useful run, do not start it. With no deadline set, choose a bounded value such as 300 s.
* Add `--preserve-f3dex` for functions that build display lists so the importer keeps the `gDP*`/`gSP*` macros.
* The permuter compiles with the same profile as `./build.sh` (from the environment's `.source_file`). See [permuter](/tools/permuter.md).

The source file must compile, should already be 95%+, may implement only one function body (externs for others are fine), and must include its headers.

Improvements appear in `nonmatchings/<function>[-N]/output-<score>[-N]/`: full code in `source.c`, changes in `diff.txt`.

# Iterating

Read the changes and apply the sensible ones in a **new** attempt file (`base_N+1.c`); do not edit an old attempt in place. Re-run with `--source-file` pointing at the new file.

The permuter finds local optima. Take only changes that make logical sense for the function. If it moves or deletes a statement the function needs, the score bump cannot lead to a 100% match. Discard results that remove `gbi.h` macros.
