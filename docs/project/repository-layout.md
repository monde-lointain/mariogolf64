---
type: Reference
title: "Repository layout"
description: "Directories and root files, which are hand-written, which are generated, and where each kind of source lives."
tags: [project, layout]
---

Hand-written sources live in `src/` and `include/`. The build generates `asm/`, `assets/` and `build/`, which git ignores. Configuration and tooling sit at the root and in `tools/`, `task-runner/` and `.omp/`.

# Source and headers

| Path | Contents |
|---|---|
| `src/main/` | Main game code. Project headers sit next to their sources (`src/main/*.h`). Compile flags: [compiler profiles](/project/compiler-profiles.md). |
| `src/overlay_*/` | Overlays, one directory each (`overlay_0`, `overlay_3` to `overlay_16`, and `overlay_gap`). Compile flags: [compiler profiles](/project/compiler-profiles.md). |
| `src/mgu/` | Remaining game code, built with the base profile. |
| `src/libultra/` | libultra, mirroring the upstream `ultralib` subtree (`audio/`, `debug/`, `gu/`, `io/`, `libc/`, `os/`, `sched/`, `vimodes/`). |
| `src/libnusys/`, `src/libmus/`, `src/libnaudio/`, `src/libkmc/` | Libraries, each with its own [compiler profile](/project/compiler-profiles.md). |
| `src/entry/entry.s` | Hand-written entry stub. Assembled as described in [compiler profiles](/project/compiler-profiles.md#assembly-and-object-layout). |
| `src/libultra/**/*.s`, `src/libkmc/*.s` | Hand-written library assembly, next to its C. Assembled by KMC; see [compiler profiles](/project/compiler-profiles.md#assembly-and-object-layout). |
| `include/` | Hand-maintained `common.h`, `include_asm.h`, `macro.inc` and `labels.inc`. Splat does not regenerate them ([splat config](/reference/splat-yaml.md)). Also `assert.h`, `dbgdefs.h`, `dbgproto.h`, `macros.h`, `stdarg.h`, `ultra64.h`, `ultrahost.h`, `sys/`, and the SDK header trees `libkmc/`, `libmus/`, `libnaudio/`, `libnualstl/`, `libnusys/` and `libultra/`. |

# Extracted and generated (git-ignored)

| Path | Contents |
|---|---|
| `asm/nonmatchings/<seg>/<file>/<func>.s` | Per-function disassembly for functions not yet matched. |
| `asm/<dir>/<file>.s` | Disassembly of each source file. |
| `asm/data/`, `asm/entry/`, `asm/header.s` | Extracted data, entry and header assembly. |
| `assets/` | Binary data extracted from the ROM. |
| `build/` | `build/mariogolf64.elf`, `build/mariogolf64.map`, `build/mariogolf64.z64`, and object files. |
| `mariogolf64.ld`, `mariogolf64.d`, `undefined_funcs_auto.txt`, `undefined_syms_auto.txt` | Generated linker script, `mariogolf64.d`, and Splat's undefined-symbol lists. |
| `nonmatchings/<func>[-N]/` | Per-function [decomp environments](/tools/decomp-env.md). |
| `ctx.c`, `ctx.c.m2c`, `src/temp.c`, `src/ctx.c` | Scratch files for m2c and decomp.me. Agents never read or write them; see [ground rules](/project/ground-rules.md) §2. |
| `tools/gcc_kmc/`, `tools/scratches.json` | Downloaded KMC toolchain, and scratch data. |
| `romdiff`, `report.json`, `__pycache__/`, `.venv/` | Output of `make diff-line`, progress output from `tools/progress.py`, the Python bytecode cache, and the virtualenv. |
| `private/`, `scratchpad/` | Local-only research workspaces (`private/ghidra` holds Ghidra project data). Excluded through `.git/info/exclude`, not `.gitignore`. |

`make extract` deletes and regenerates `asm/`. Never edit it.

# Configuration

| Path | Purpose |
|---|---|
| `Makefile` | Build rules and the `COMPILER PROFILES` section. |
| `mariogolf64.yaml` | Splat segment layout ([format reference](/reference/splat-yaml.md)). |
| `symbol_addrs.txt` | Symbol names, types and addresses ([format reference](/reference/symbol-addrs-format.md)). |
| `reloc_addrs.txt` | Splat entrypoint relocations. |
| `linker_scripts/` | `hardware_regs.ld` and `n64_os_syms.ld`, linked with the generated script. |
| `requirements.txt` | Python packages: Splat (`splat64`) with `spimdisasm`, `rabbitizer` and `pyyaml`; `toml` and `pycparser` for the permuter; `colorama`, `watchdog`, `levenshtein` and `cxxfilt` for asm-differ; `m2c`, pinned to a git commit. |
| `diff_settings.py` | asm-differ configuration: map file `build/mariogolf64.map`, source directories `src`, `include` and `asm`. |
| `permuter_settings.toml` | decomp-permuter configuration. `tools/permuter` reads flags from `make print-cflags`. |
| `decomp.yaml` | Project metadata: ROM SHA1 and build paths. [Progress](/tools/progress.md) reads the map path from it. |
| `.clang-format`, `.clang-tidy`, `.clangd` | Formatting, lint and editor settings. Per-directory overrides: `src/{libnaudio,libmus,libnusys,libkmc,libultra,mgu}/` have `.clang-format` and `.clang-tidy`; `src/main/` has `.clang-format`; `include/{libnaudio,libmus,libnualstl}/` have `.clang-format`. |
| `.envrc` | direnv hook that activates `.venv`. |
| `mariogolf64.sha1`, `baserom.z64` | Reference checksum and base ROM. Protected; see [ground rules](/project/ground-rules.md) §2. |
| `README.md`, `AGENTS.md`, `LICENSE` | Project front page, short agent rules, and the MIT license. |
| `.gitignore`, `.gitattributes`, `.gitmodules`, `.dockerignore` | Ignore rules; merge drivers (`src/**/*.c` uses `decomp-c`, `docs/log.md` uses `union`); the two submodules; the Docker build context, which contains only `baserom.z64`. |

# Tooling and automation

| Path | Contents |
|---|---|
| `tools/` | Scripts, the decomp environment template (`decomp-env-template/`), and the `decomp-permuter` and `asm-differ` submodules. See [tools](/tools/) and the [harness code map](/harness/code-map.md). |
| `task-runner/` | [nigel](/tools/task-runner.md) task definitions and configuration. |
| `.omp/` | omp configuration (`config.yml`); `skills/<name>/SKILL.md` symlinks into `docs/procedures/`; the policy hook at `hooks/pre/mg64-policy.ts`, with tests in `hooks/tests/`. |
| `docs/` | This bundle. |
| `docker/`, `.github/workflows/build.yml` | Docker image and [CI](/harness/git-hook-and-ci.md). |
