---
type: Procedure
name: decomp-permuter
title: "Run the permuter (offline, by a person)"
description: "Offline tool a person runs with decomp-permuter on a 95%+ register-only near match. Not part of the agent decompile loop: agents stop at the build.sh STOP line and never run it."
tags: [decompile, permuter]
---

# Why agents do not run it

The permuter reorders and rewrites expressions. It does not change control flow, so it cannot fix a control-flow mismatch. It also finds local optima and tends to loop: `do { } while (0)` rewrites, reused variables, dropped calls, and `gbi.h` macros expanded into raw words. The agent loop stops at the `STOP:` line from `build.sh` and records the function in the difficulty ledger instead ([decompile-function](decompile-function.md), "Stop rules").

# Sanity check

Run it only when both hold:

* No significant control-flow or functional difference remains between your C and the target. The permuter does not change control flow, and trying anyway risks a local optimum.
* You have already improved the function yourself. A 95%+ match handed to you has probably been permuted already; you were given it *because* the permuter failed.

# Run

From the repository root, with a **literal numeric** timeout:

```bash
timeout 300s ./tools/permuter --source-file nonmatchings/<function>/base_N.c <function>
```

* Write the timeout as a literal such as `300s`, not `$T`: the [policy hook](/harness/policy-hook.md) checks the command before shell variables expand. Under nigel the timeout must also end at least 120 s before `NIGEL_TIMEOUT_DEADLINE_UNIX`; if the remaining budget is too small for a useful run, do not start it. With no deadline set, choose a bounded value such as 300 s.
* Add `--preserve-f3dex` for functions that build display lists so the importer keeps the `gDP*`/`gSP*` macros.
* The permuter compiles with the same profile as `./build.sh` (from the environment's `.source_file`). See [permuter](/tools/permuter.md).

The source file must compile, should already be 95%+, may implement only one function body (externs for others are fine), and must include its headers.

Improvements appear in `nonmatchings/<function>[-N]/output-<score>[-N]/`: full code in `source.c`, changes in `diff.txt`.

# Iterating

Read the changes and apply the sensible ones in a **new** attempt file (`base_N+1.c`); do not edit an old attempt in place. Re-run with `--source-file` pointing at the new file.

The permuter finds local optima. Take only changes that make logical sense for the function. If it moves or deletes a statement the function needs, the score bump cannot lead to a 100% match. Discard results that remove `gbi.h` macros.
