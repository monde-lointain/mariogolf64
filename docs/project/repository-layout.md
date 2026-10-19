---
type: Reference
title: "Repository layout"
description: "Directories and root files, which are hand-written, which are generated, and where each kind of source lives."
tags: [project, layout]
---

# Source and headers

| Path | Contents |
|---|---|
| `src/main/` | Main game code (`-DF3DEX_GBI_2`). Project headers live next to their sources (`src/main/*.h`). |
| `src/overlay_*/` | Overlays (`-DF3DEX_GBI_2`, same F3DEX2 microcode as `src/main`). |
| `src/mgu/` | Remaining game code; base compiler profile. |
| `src/libultra/` | libultra, mirroring the upstream `ultralib` subtree (`os/`, `io/`, `gu/`, ...). |
| `src/libnusys/`, `src/libmus/`, `src/libnaudio/`, `src/libkmc/` | Other libraries, each with its own [compiler profile](compiler-profiles.md). |
| `src/entry/entry.s` | Entry stub, assembled with modern GAS. Other hand-written library assembly sits next to its C (`src/libultra/**/*.s`, `src/libkmc/*.s`). |
| `include/` | `common.h`, `include_asm.h`, `macro.inc`, `labels.inc` (hand-maintained; Splat's regeneration is disabled) and vendored SDK headers (`include/libultra`, `libnusys`, `libmus`, `libnaudio`, `libkmc`, ...). |

# Extracted and generated (git-ignored)

| Path | Contents |
|---|---|
| `asm/nonmatchings/<seg>/<file>/<func>.s` | Unmatched functions, one file each, pulled into C by `INCLUDE_ASM`. |
| `asm/<dir>/<file>.s` | Full disassembly of each C file, for reference. |
| `asm/data/` | Extracted data assembly. |
| `assets/` | Binary blobs extracted from the ROM. |
| `build/` | Objects, ELF, map, and `build/mariogolf64.z64`. |
| `mariogolf64.ld`, `mariogolf64.d`, `undefined_funcs_auto.txt`, `undefined_syms_auto.txt` | Generated linker script, dependency file, and Splat's undefined-symbol lists. |
| `nonmatchings/<func>[-N]/` | Per-function [decomp environments](/tools/decomp-env.md). |
| `ctx.c`, `ctx.c.m2c`, `src/temp.c`, `src/ctx.c` | Scratch files for m2c and decomp.me; never read or edit them. |
| `tools/gcc_kmc/`, `tools/scratches.json` | Downloaded toolchain; fetched decomp.me scratches. |

`make extract` deletes and regenerates `asm/`. Never edit it.

# Configuration

| File | Purpose |
|---|---|
| `mariogolf64.yaml` | Splat segment layout: [format reference](/reference/splat-yaml.md). |
| `symbol_addrs.txt` | Symbol names, types and sizes: [format reference](/reference/symbol-addrs-format.md). |
| `reloc_addrs.txt` | Splat relocation overrides (entrypoint `main_BSS_START`/`main_BSS_SIZE` relocs). |
| `linker_scripts/` | Hand-written `hardware_regs.ld` and `n64_os_syms.ld` (hardware registers, fixed OS addresses). |
| `Makefile` | Build rules and the `COMPILER PROFILES` section. |
| `requirements.txt` | Python dependencies (Splat, spimdisasm, rabbitizer, mapfile_parser, permuter, asm-differ, m2c). |
| `diff_settings.py` | asm-differ configuration (images, map file, source directories). |
| `permuter_settings.toml` | decomp-permuter configuration; `tools/permuter` supplies the compiler profile. |
| `decomp.yaml` | Project metadata for mapfile_parser progress reports (`report.json`). |
| `.clang-format`, `.clang-tidy`, `.clangd` | Formatting, advisory lint, editor settings. |
| `.envrc` | direnv hook that activates `.venv`. |
| `mariogolf64.sha1`, `baserom.z64` | Reference checksum and base ROM. Protected. |

# Tooling and automation

| Path | Purpose |
|---|---|
| `tools/` | Scripts, the per-function environment template, vendored submodules. See [tools](/tools/) and the [harness code map](/harness/code-map.md). |
| `task-runner/` | [nigel](/tools/task-runner.md) task definitions. |
| `.omp/` | omp config, sticky rules, skills, policy hook. |
| `docs/` | This bundle. |
| `docker/`, `.github/workflows/build.yml` | [CI](/harness/git-hook-and-ci.md). |
