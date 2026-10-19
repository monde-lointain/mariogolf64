---
type: Tool
title: "decomp-env and per-function scoring"
description: "tools/decomp-env creates nonmatchings/<function>/ with target assembly, an m2c starting point and build.sh, which compiles an attempt with the owning file's profile and scores it."
tags: [decompile, build, comparison]
resource: ../../tools/decomp-env
---

# Bootstrap

```bash
./tools/decomp-env --bootstrap-only <function> [--id <ID>]
```

Needs `asm/` (run [build-and-verify](build-and-verify.md) first). `<function>` may be a function name, a `.s` path, or a decomp.me scratch ID (alphanumeric, 5-10 chars; function names win when both match). Flags:

| Flag | Effect |
|---|---|
| `--bootstrap` | Generate `base.c` with m2c. Without it, `base.c` is copied from `src/temp.c`. Not allowed with a scratch ID. |
| `--bootstrap-only` | Implies `--bootstrap`; create the directory and exit. Without it, the script launches `omp --thinking xhigh` inside the directory. |
| `--id ID` | Directory is `nonmatchings/<function>-<ID>` and is only created if absent. Requires `--bootstrap`; not allowed with a scratch ID. |

Without `--id`, the directory is `nonmatchings/<function>` or the first free `<function>-2`, `-3`, ...

# What the directory contains

| File | Source |
|---|---|
| `target.s`, `target.o` | `prelude.inc` plus the function's assembly, assembled with the VR4300 settings |
| `base.c` | m2c output (with a context built from the owning C file via `tools/m2ctx.py`), the scratch's source, or `src/temp.c` |
| `.source_file` | The C file whose [compiler profile](/project/compiler-profiles.md) applies. For assembly-only segments: `src/main/__candidate__.c` below VRAM `0x80100000`, else `src/__candidate__.c`. |
| `.diff_algorithm` | Only for scratches that used levenshtein diffing |
| `AGENTS.md` | Rendered from `tools/decomp-env-template/AGENTS.md` with the function name substituted |
| `build.sh`, `diff.sh`, `dist.py`, `normalize_asm.py`, `objdump.py`, `map_asm_to_c.py`, `prelude.inc` | Symlinks into `tools/decomp-env-template/`: editing one edits the template for every environment |
| `.omp` | Symlink to the repository's `.omp` (skills, policy hook, rules) |
| `match_log.txt`, `.matches/`, `LEARNINGS.md` | Created while working |

`nonmatchings/` is git-ignored. [decomp-env-cleanup](selection-and-status.md) removes environments for functions that are already matched.

# Scoring an attempt

```bash
./build.sh base_3.c [opt_flag]
```

Run inside the environment. It serializes builds with `flock`, rejects a candidate that contains `INCLUDE_ASM`, `INCLUDE_RODATA` or `GLOBAL_ASM`, reads the profile flags from `make print-cflags` for `.source_file`, compiles, dumps and normalizes both objects, and scores them with `dist.py` (penalty weights copied from decomp-permuter's scorer). An optional second argument replaces the `-O` level.

Output files next to the attempt: `<attempt>.o`, `<attempt>_object_dump.s`, `_object_dump_normalized.s`, `_annotated.s` (objdump with C annotations), and `<attempt>_diff`.

`Verified exact match: yes` requires all three of: scorer reports an exact match, zero differences, and byte-identical normalized dumps. On a match the source is archived as `.matches/<stem>-<sha256>.c`. For attempts named `base_<N>.c` the result is appended to `match_log.txt` (`<file> <score>%`, or `.matches/... 100.000% exact sha256=...`), and the script prints `No progress in N attempts (best: X% at <file>)` once the best entry is 20 or more attempts old. That line is the project's stop signal ([stop rules](/procedures/decompile-function.md)).

| Helper | Use |
|---|---|
| `./diff.sh <attempt>.o` | Normalized diff of a built attempt |
| `./objdump.py <attempt>.o` | Normalized MIPS dump |
| `./map_asm_to_c.py <attempt>.o <line>` | Map an assembly line back to its C |

A symbol-only difference such as `%lo(D_800B67A5)` versus `%lo(array+1)` is naming noise, not an instruction difference.

Per-function exact matches are certified from `match_log.txt` by `tools/nonmatching_match_log.py` ([selection-and-status](selection-and-status.md)). If any of these helpers is wrong, fix it at its source ([changing the harness](/harness/changing-the-harness.md)).
