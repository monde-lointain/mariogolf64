---
type: Tool
title: "build-and-verify"
description: "Clean re-extract, rebuild and SHA1 check of the whole ROM; the only command that decides a project-wide match."
tags: [build, verify, comparison]
resource: ../../tools/build-and-verify.sh
---

Runs the steps below in order and stops at the first failing one. Its final SHA-1 check is what decides whether the ROM matches.

```bash
./tools/build-and-verify.sh
```

It takes no arguments and exits 2 when given any. Environment: `JOBS` (parallel jobs, default `nproc`) and `MAKE` (make binary, default `make`).

# What it does

1. `make clean` removes `asm/`, `assets/`, `build/`, the generated linker script, the `.d` dependency files and the undefined-symbol lists.
2. `make extract` runs Splat to re-extract `baserom.z64`. Its output is discarded.
3. `make -j<JOBS> all`: compiles and links. The `verify` target then runs `shasum --check mariogolf64.sha1`, unless `NON_MATCHING=1`.

The Make targets and their dependencies are described in [setup-and-build](/project/setup-and-build.md).

Because it always rebuilds from scratch, state in `asm/` or `build/` never leaks into a result. Tools that read build outputs ([asm-differ](/tools/asm-differ.md), [data-differ](/tools/data-differ.md)) need a build first.

# Reading the result

Success prints `build/mariogolf64.z64: OK` followed by `BUILD SUCCEEDED`. A non-zero exit prints `BUILD HAS FAILED`. That message says warnings and checksum mismatches count as failures, but the script itself only fails on a non-zero exit. Existing sources emit compiler warnings on a successful build, so judge by the exit code and the two success lines, and do not add new warnings.

On a checksum failure, find the cause in this order:

1. [asm-differ](/tools/asm-differ.md) on every function in the files you changed, especially users of structs you modified.
2. [data-differ](/tools/data-differ.md) `--find-first-mismatch` when data may be responsible.
3. `make diff-line` (human use) writes a byte diff of the base and built ROM to `romdiff`.

This script is the last step of [verify-candidate](/tools/verify-candidate.md), which is nigel's `verify_command`. The [git pre-commit hook and CI](/harness/git-hook-and-ci.md) also run it. Agents must call this script rather than `make` ([policy hook](/harness/policy-hook.md)).

# Build helpers

| Tool | Use |
|---|---|
| `tools/kmc-cc [gcc flags…] <input.c> -o <output.o>` | Compiles one C file exactly as the ROM build does. It runs KMC GCC 2.7.2 (`tools/gcc_kmc/gcc -S`), assembles with KMC binutils (`as -EB -mips2 -G 0 -I include`), then strips the placeholder symbol KMC GCC leaves behind (`$STRIP`, default `mips-linux-gnu-strip`, with `-N dummy-symbol-name`). `-o` is required and the script exits 2 without it. Run it from the repository root so include paths and `INCLUDE_ASM` paths resolve. Used by the Makefile (`KMC_CC`), the decomp environments' `build.sh`, the permuter's `compiler_command`, and `tools/m2c_one_shot.py`. Compiler flags are in [compiler-profiles](/project/compiler-profiles.md). |
| `tools/n64crc.py <in> [out]` | Writes the two N64 header CRCs (at 0x10) over the ROM range 0x1000–0x101000. Without `out` it rewrites `in` in place, and it prints `crc1: 0x…, crc2: 0x…`. Exits 1 with no arguments, 2 when the input is missing, and 3 when the input is shorter than 0x101000 bytes. The Makefile's `nonmatching` target runs it on its ROM. |
