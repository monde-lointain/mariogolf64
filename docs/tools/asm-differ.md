---
type: Tool
title: "asm-differ"
description: "Diff a function in the built ROM against the target assembly; vendored submodule, protected."
tags: [diff, verify]
resource: ../../tools/asm-differ/diff.py
---

`tools/asm-differ/diff.py` diffs a function in the built ROM against the target assembly. It is a vendored submodule and is protected ([ground rules](/project/ground-rules.md)).

```bash
python3 tools/asm-differ/diff.py --no-pager <function name>
```

**Rebuild first.** It reads the current `build/`; after a source change, run [build-and-verify](/tools/build-and-verify.md) first or the diff shows stale code. Inside a decomp environment, use `./build.sh` and `./diff.sh` instead ([decomp-env](/tools/decomp-env.md)), which score a candidate in isolation.

# Configuration

`diff_settings.py` at the repository root sets the images, map and source directories. `diff.py` imports it from the current directory, so run it from the repository root.

| Setting | Value |
|---|---|
| `baseimg` | `baserom.z64` (target ROM) |
| `myimg` | `build/mariogolf64.z64` (built ROM) |
| `mapfile` | `build/mariogolf64.map` |
| `source_directories` | `./src`, `./include`, `./asm` |
| `objdump_flags` | `-Mreg-names=32` |
| `makeflags` | `NON_MATCHING=0`, passed to `make` by `-m` and `-w` |

# Flags

The positional `start [end]` gives a function name or start address and an optional end address.

| Flag | Effect |
|---|---|
| `-o` | Diff `.o` files instead of the whole binary. Recommended: symbol names are visible. |
| `-f, --file FILE` | Diff this file. The map file is not searched for the function. Useful for dynamically linked libraries. |
| `-F, --ref-file REF_FILE` | File to diff against. Default `expected/<diffed file>`. |
| `-e, --elf SYMBOL` | Diff a function in two ELFs, one stripped and one not. Requires binutils 2.33+ `objdump`. |
| `-c, --source` | Show source code. Only with `-o` or `-e`. |
| `-C, --source-old-binutils` | Source handling for binutils older than 2.33. Implies `--source`. |
| `-j, --section SECTION` | Restrict the diff to one output section. |
| `-L, --line-numbers` / `--no-line-numbers` | Show or hide source line numbers. |
| `--inlines` | Show inline function calls. Only with `-o` or `-e`. |
| `--base-asm FILE` | Read the target assembly from `FILE` instead of `baseimg`. |
| `--write-asm FILE` | Write the current assembly to `FILE`, for use with `--base-asm`. |
| `-m, --make` | Run `make` on the object or binary before diffing. |
| `-l, --skip-lines LINES` | Skip the first `LINES` lines of output. |
| `-s, --stop-at-ret` | Stop at the first return instruction. `-ss` stops at the second, and so on. |
| `-i, --ignore-large-imms` | Treat all large enough immediates as equal. |
| `-I, --ignore-addr-diffs` | Ignore address differences. Affects only AArch64 and ARM32. |
| `-B, --no-show-branches` | Do not draw branches and branch targets. |
| `-R, --no-show-rodata-refs` | Hide `.rodata` to `.text` references (typically jump tables). |
| `-S, --base-shift N` | Compare position N in the built image with N + shift in the target. Arithmetic is allowed. Not supported with `-o`. |
| `-w, --watch` | Rebuild and redo the diff when source or object files change. Use with `-m`. |
| `-y, --yes` | Answer yes to prompts. Useful for `-w` without `-m`. |
| `-0, --diff_mode=single_base` | Show the target assembly only. |
| `-1, --diff_mode=single` | Show the current assembly only. |
| `-3, --threeway=prev` | Three-way diff of target, current and the assembly before the last `-w` rebuild. Requires `-w`. |
| `-b, --threeway=base` | Three-way diff of target, current and the assembly when `diff.py` started. Requires `-w`. |
| `--width COLS` | Width of each column. |
| `--algorithm {levenshtein,difflib}` | Diff algorithm. Default `levenshtein`, which gives the smallest diff; `difflib` keeps long runs of equal opcodes together. |
| `--max-size, --max-lines LINES` | Maximum diff length, in lines. |
| `--no-pager` | Write to stdout and exit. Incompatible with `--watch`. |
| `--format {color,plain,html,json}` | Output format; default `color`. `html` and `json` imply `--no-pager`. |
| `-U, --compress-matching N` | Collapse runs of matching lines, leaving `N` lines of context around differences. |
| `-V, --compress-sameinstr N` | Collapse runs of identical instructions with possibly different register allocation, leaving `N` lines of context. |
| `-d, --diff-function-symbols` | Include and diff function symbols. |

`-m` runs `make NON_MATCHING=0` (the `makeflags` setting) on the object or binary before diffing. With `-w`, `-m` also rebuilds on each source change. `-w` without `-m` asks for confirmation (skipped by `-y`) and does not run make.
