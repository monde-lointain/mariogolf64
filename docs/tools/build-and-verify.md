---
type: Tool
title: "build-and-verify"
description: "Clean re-extract, rebuild and SHA1 check of the whole ROM; the only command that decides a project-wide match."
tags: [build, verify, comparison]
resource: ../../tools/build-and-verify.sh
---

```bash
./tools/build-and-verify.sh
```

Takes no arguments (it exits 2 on any). Environment: `JOBS` (parallelism, default `nproc`), `MAKE` (make binary).

# What it does

1. `make clean`, which removes `asm/`, `assets/`, `build/`, the generated linker script and undefined-symbol lists.
2. `make extract`: Splat re-extracts `baserom.z64`.
3. `make -j<JOBS> all`: compiles, links, then `shasum --check mariogolf64.sha1`.

Because it always rebuilds from scratch, state in `asm/` or `build/` never leaks into a result, and tools that read build outputs ([asm-differ](asm-differ.md), [data-differ](data-differ.md)) need a build first.

# Reading the result

Success prints `build/mariogolf64.z64: OK` followed by `BUILD SUCCEEDED`. A non-zero exit prints `BUILD HAS FAILED` (its text says warnings and checksum mismatches count as failures; the script itself only fails on a non-zero exit). Existing sources emit compiler warnings on a successful build, so judge by the exit code and the two success lines, and do not add new warnings.

On a checksum failure, find the cause in this order:

1. [asm-differ](asm-differ.md) on every function in the files you changed, especially users of structs you modified.
2. [data-differ](data-differ.md) `--find-first-mismatch` when data may be responsible.
3. `make diff-line` (human use) writes a byte diff of base and built ROM to `romdiff`.

It is also the `verify_command` for [nigel](task-runner.md) and the body of the [git pre-commit hook](/harness/git-hook-and-ci.md). Agents must call this script rather than `make` ([policy hook](/harness/policy-hook.md)).
