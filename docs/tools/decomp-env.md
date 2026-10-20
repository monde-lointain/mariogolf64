---
type: Tool
title: "decomp-env and per-function scoring"
description: "tools/decomp-env creates nonmatchings/<function>/ with the target assembly, a starting base.c and build.sh, which compiles an attempt with the owning file's profile and scores it."
tags: [decompile, build, comparison]
resource: ../../tools/decomp-env
---

`tools/decomp-env` creates a working directory for one function under `nonmatchings/`. The directory's `build.sh` compiles and scores each attempt. The per-function workflow and stop rules are in [decompile-function](/procedures/decompile-function.md); selection and cleanup are in [selection-and-status](/tools/selection-and-status.md).

# Bootstrap

```bash
./tools/decomp-env [--bootstrap] [--bootstrap-only] [--id ID] [--refs LIST] <function_name_or_path_or_scratch_id>
```

The argument is a function name, a `.s` path, or a decomp.me scratch ID. It is a scratch ID when it contains no `/`, does not end in `.s`, names no assembly function under `asm/`, and is either all digits or 5–10 alphanumeric characters. Function names win.

| Flag | Effect |
|---|---|
| `--bootstrap` | Generate `base.c` with m2c from the function's assembly. Without it, `base.c` is a copy of `src/temp.c`, or the imported source for a scratch ID. Not allowed with a scratch ID. |
| `--bootstrap-only` | Implies `--bootstrap`. Create the directory and exit, printing `Bootstrap-only mode: environment ready at <dir>`. Without it, the script launches `omp --thinking xhigh` inside the directory. |
| `--id ID` | Use the directory `nonmatchings/<function>-<ID>`. Requires `--bootstrap`; not allowed with a scratch ID. If the directory exists, the script prints `Directory already exists: <dir>`, launches `omp` there unless `--bootstrap-only`, and exits 0 without re-creating it. |
| `--refs LIST` | Function names whose C definitions go into the context block. A JSON array (`'["func_a", "func_b"]'`) or comma or space separated names; an empty value gives no references. Absent: the five matched functions most similar to the target's assembly (`find_similar_functions.py`). Works with `--bootstrap-only --id ID`. See Context block below. |

Without `--id`, the directory is `nonmatchings/<function>`, or the first free `<function>-2`, `-3`, ….

Run [build-and-verify](/tools/build-and-verify.md) first. Without `asm/`, the script exits with `asm/ does not exist. Run ./tools/build-and-verify.sh first.`

# Requirements

| Requirement | Used for |
|---|---|
| `asm/` | Source of the target assembly |
| `mips-linux-gnu-as`, `mips64-linux-gnu-as` or `mips64-elf-as` | Assembling `target.o`; the first one found is used |
| `make` | `make -s print-cflags` in `build.sh` |
| `flock` | Serializing builds in `build.sh` |
| `.venv/bin/m2c` when present, else `m2c` | `--bootstrap`. A missing or failing m2c leaves a blank `base.c` with a warning. |
| `.venv/bin/python3` when present, else `python3` | `tools/m2ctx.py` and `tools/difficulty_ledger.py write-run` |
| `omp` | Launched in the environment. When absent, the script prints the directory to open. |
| `curl` and `.decomp_dev_secret` | Fetching scratches. The first line of `.decomp_dev_secret` is the User-Agent header. The file is git-ignored. |

# What the directory contains

| File | Source |
|---|---|
| `function.s` | Whole-file assembly targets only (an `asm/` file outside `asm/nonmatchings/`): the lines from `nonmatching <name>,` through `endlabel <name>`. Fails with `Error: Could not isolate <name> from <file>` when that range is empty. |
| `target.s`, `target.o` | `prelude.inc` plus the function's assembly (`function.s` for a whole-file target, else the file under `asm/nonmatchings/`), assembled with `-EB -march=vr4300 -mtune=vr4300 -Iinclude` |
| `base.c` | With `--bootstrap`: m2c output, with a context from `tools/m2ctx.py` when the owning C file exists. For a scratch ID: the imported scratch source. Otherwise: a copy of `src/temp.c`. |
| `.source_file` | The C file whose [compiler profile](/project/compiler-profiles.md) applies. Hand-written assembly with no owning C file uses `src/main/__candidate__.c`. |
| `.diff_algorithm` | `levenshtein`, only for a scratch whose diff flags include `-DIFFlevenshtein` |
| `AGENTS.md` | Rendered from the template with `$functionName`, `$root` and `$env` replaced by the function name, the absolute repository root and the absolute environment directory. The `$context` line is replaced by the context block described below. |
| Template files | Symlinks to every file in `tools/decomp-env-template/` (`build.sh`, `diff.sh`, `dist.py`, `normalize_asm.py`, `objdump.py`, `map_asm_to_c.py`, `prelude.inc`, `__pycache__`). Editing one edits the template for every environment. |
| `.omp` | Symlink to the repository's `.omp` configuration |
| `.run.json` | Written by `tools/difficulty_ledger.py write-run`: function, task, tier, model, run id, deadline and `stall_limit`. See [difficulty-ledger](/tools/difficulty-ledger.md). If the write fails, the script warns and `build.sh` uses the default stall limit. |
| `.build.lock`, `match_log.txt`, `.matches/`, `NEAR_MISS.md` | Created during work: the build lock, per-attempt scores, archived exact matches and notes |

`nonmatchings/` is git-ignored. `tools/decomp-env-cleanup` removes environments for functions that are already matched; see [selection-and-status](/tools/selection-and-status.md).

# Context block

`decomp-env` renders the `$context` line of `AGENTS.md` with the output of `tools/decomp_env_context.py`. It runs from the repository root after `.run.json` is written:

```bash
tools/decomp_env_context.py --env nonmatchings/<dir> --function <name> [--refs LIST]
```

The sections appear in this order. Each is omitted when empty.

| Section | Content |
|---|---|
| `<prior_attempts>` | This function's records in the [difficulty ledger](/tools/difficulty-ledger.md), newest first, at most 5: `tier <n> <outcome> best <pct>%: <reason>`. The ledger is `$MG64_LEDGER` if set, else `$(git rev-parse --git-common-dir)/mg64/difficulty.jsonl`. |
| `<callers>` | `path:line: text` for each C caller in `src/` and `include/`, sorted by path and line, at most 12, then `(N more)`. Skipped: comments, `INCLUDE_ASM`, `INCLUDE_RODATA` and `GLOBAL_ASM` lines, the function's own definition, declarations, and files under `nonmatchings/` or named `ctx.c` or `temp.c`. `Undecompiled callers (asm):` names the `asm/nonmatchings` stubs that `jal` the function and have no C body, at most 8. |
| `<declarations>` | Prototypes and externs of the function, at most 5. |
| `<references>` | The C definition of each reference (`--refs`, else the five most similar matched functions) that has one. Bodies over 80 lines are `<reference ... />` elements with the path and line range only. Unknown names are skipped. |
| `<baseline>` | `./build.sh base.c` run in the environment: its `Score:` line and any `NEXT:` line, or, when it fails, its first 15 output lines with errors before warnings. Times out after 120 s. Omitted when there is no `build.sh`. |

`base.c` is not a `base_<N>.c` attempt. Its result is not appended to `match_log.txt` and does not count toward the stall limit.

The block is capped at 10,000 bytes. Reference bodies are dropped largest first, then trailing references, until it fits.

If the context script fails, `decomp-env` warns and leaves the context empty; environment creation still succeeds. An unreadable ledger warns on stderr and omits `<prior_attempts>`.

# Scoring an attempt

```bash
./build.sh base_3.c [opt_flag]
```

Run inside the environment. `build.sh`:

- rejects a candidate that contains `INCLUDE_ASM`, `INCLUDE_RODATA` or `GLOBAL_ASM` (`ERROR: The C file contains an assembly include.`);
- serializes builds with `flock` on `.build.lock`;
- reads the profile flags from `make -s print-cflags SRC=<.source_file>`; an optional second argument replaces the `-O` level;
- compiles with `tools/kmc-cc` ([build-and-verify](/tools/build-and-verify.md)), dumps and normalizes both objects, and scores them with `dist.py` using `.diff_algorithm` (default `sequence`). The penalty weights are copied from decomp-permuter's scorer.

| Output | Content |
|---|---|
| `<attempt>.o` | Object file |
| `<attempt>_annotated.s` | Disassembly with C line annotations |
| `<attempt>_object_dump.s`, `<attempt>_object_dump_normalized.s` | Dump and normalized dump |
| `<attempt>_diff` | Unified diff of the normalized dump against `target_object_dump_normalized.s` |

`Verified exact match: yes` requires all three of: the scorer reports an exact match, zero differences, and byte-identical normalized dumps. On a match the source is archived as `.matches/<stem>-<sha256>.c`, and a `NEXT:` line names the integration step ([decompile-function](/procedures/decompile-function.md)).

For attempts named `base_<N>.c`, the result is appended to `match_log.txt`: `<file> <score>%`, or `.matches/... 100.000% exact sha256=...` for a match. Once the best entry is `N` or more attempts old, the script prints `STOP: No progress in <attempts> attempts (best: X% at <file>; stall limit N).` and repeats that line on every later attempt. `N` is `MG64_STALL_LIMIT` if set, else `stall_limit` from `.run.json`, else 20. A value that is not a positive integer fails before compiling. When `.run.json` has a deadline, every build ends with `Time left: <minutes> min`. The stop rules that act on `STOP:` are in [decompile-function](/procedures/decompile-function.md).

# Helpers

| Helper | Use |
|---|---|
| `./build.sh <file.c> [opt_flag]` | Compile, score and archive one attempt |
| `./diff.sh <attempt>.o` | Diff a built attempt's normalized dump against `target_object_dump_normalized.s` |
| `./dist.py <target.o> <attempt.o> [--stack-diffs] [--symbol SYMBOL] [--algorithm {sequence,levenshtein}] [--debug]` | Score two objects. Prints `Score: X% (N differences)`, `Algorithm`, and `Exact match: yes\|no`. |
| `./objdump.py <file.o> [--symbol SYMBOL]` | Normalized MIPS dump of an object |
| `./normalize_asm.py <file>` | Normalized dump to stdout; `jtbl_*` labels are rewritten to `.rodata` |
| `./map_asm_to_c.py <attempt>.o <n>` | Print the source location and C code for the n-th instruction (decimal, counted from 1 within its function) |

`prelude.inc` sets `noat`, `noreorder` and `gp=64`, includes `macro.inc`, and redefines `jlabel` as a local label.

Relocations compare by resolved VRAM. An auto-named `D_<VRAM>` or `func_<VRAM>` symbol encodes its address; any other name resolves through `symbol_addrs.txt`. So `%lo(D_800B67A5)` and `%lo(array+1)` compare equal when `array+1` is `0x800B67A5`. A name declared twice in `symbol_addrs.txt` is unresolved and compares by name.

Per-function exact matches are certified from `match_log.txt` by `tools/nonmatching_match_log.py` ([selection-and-status](/tools/selection-and-status.md)). If any of these helpers is wrong, fix it at its source ([changing the harness](/harness/changing-the-harness.md)).
