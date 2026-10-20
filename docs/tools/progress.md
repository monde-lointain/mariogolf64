---
type: Tool
title: "Progress report"
description: "tools/progress.py: code and data matched in the last build, by bytes, functions and data symbols, per category or segment; `-o` writes the objdiff report.json."
tags: [status, workflow, ci]
resource: ../../tools/progress.py
---

```bash
python3 tools/progress.py                  # Game, Libraries and Total rows
python3 tools/progress.py --segments       # plus one row per ROM segment
python3 tools/progress.py -u               # plus unmatched functions, largest first
python3 tools/progress.py -o report.json   # writes the objdiff report
```

Reads the linker map and each object the map links. The figures describe whatever `build/` holds, so run [build-and-verify](/tools/build-and-verify.md) first.

| Flag | Effect |
|---|---|
| `--map MAP` | Linker map to measure. Default: the `versions[0]` map in `decomp.yaml` (`build/mariogolf64.map`). |
| `--segments` | One row per ROM segment, in link order, above the category rows. |
| `-u`, `--unmatched` | Print the unmatched functions, largest first. See [Unmatched functions](#unmatched-functions). |
| `-o PATH`, `--report PATH` | Write the objdiff progress report. See [decomp.dev report](#decompdev-report). |

Exit `1` with `error: <map> not found; run ./tools/build-and-verify.sh first` when the map is missing.

# Measures

| Column | Matched / total |
|---|---|
| Code bytes | matched bytes / bytes of executable input sections (`.text`) |
| Functions | matched functions / all functions in those sections |
| Data bytes | matched bytes / bytes of every other input section (`.data`, `.rodata`, `.bss`) |
| Data symbols | matched data objects / all data objects |

Rows are Game, Libraries and Total, plus one per segment with `--segments`.

## Counting

- A function or data object is one address, however many symbols name it. A symbol with no `.size` runs to the next symbol, or to the section end.
- Functions are `STT_FUNC` symbols. In hand-written assembly, global untyped or `OBJECT` labels also count as entry points.
- Data objects are `STT_OBJECT` symbols only. C string literals have no symbol, so they are not counted.
- In a C object, code bytes are the section size minus the sizes of its unmatched items. Bytes no symbol covers count as matched.
- A hand-written assembly section counts in full as matched; an `asm/` section counts zero.

## What counts as matched

| Object source | Matched |
|---|---|
| `src/**/*.c` | everything except symbols with a `<name>.NON_MATCHING` marker: `INCLUDE_ASM` functions and the rodata included with them |
| `src/**/*.s` | everything: hand-written assembly (the `hasm` subsegments in `mariogolf64.yaml`, such as `entry/entry`) |
| `asm/**/*.s` | nothing: splat output not yet in C |

Binary objects are neither code nor data to decompile. They are left out of every total and summarised on the `Not counted` line: `build/assets/`, `asm/` sources that `.incbin` a binary (the RSP microcode), and the ROM header object `asm/header.o`.

**Libraries** are objects built from `src/libultra`, `src/libnusys`, `src/libnaudio`, `src/libmus`, `src/libkmc` and `src/mgu`. **Game** is everything else, including every `asm/` object, so library code and data still in `asm/` count as Game.

# Unmatched functions

`-u` lists every unmatched function item in an executable section: `NON_MATCHING`-marked C functions and every function in an `asm/` object. Data is not listed.

- Header `Unmatched functions, largest first (N):`, then one row per function: `0x<size hex>  <name>  (<source>)`.
- Sorted by size, then name. Prints `All functions are matched.` when there are none.

# decomp.dev report

`-o PATH` writes an [objdiff](https://github.com/encounter/objdiff) progress report (`report.proto`, version 2, proto3 JSON).

- Top level: `version`, `measures` (whole build), `categories` (`game` and `libs`) and `units`.
- One unit per linked object, named by its path under `build/` without the extension (`src/main/audio_mgr`). Its `metadata` holds `complete` (every code and data byte matched), `module_name` (ROM segment), `module_id` (the segment's index in link order), `source_path` (the `.c` or `.s` source) and `progress_categories` (`[game]` or `[libs]`).
- A unit's `sections` give the size and byte match percent of each section. Its `functions` cover code items only; `fuzzy_match_percent` is `100.0` when matched and `0.0` otherwise, since no fuzzy comparison is made.
- The report has no data-symbol counts, so they appear only in the table. Binary objects are not units.

CI (`.github/workflows/build.yml`) runs `progress.py --segments -o report.json`, writes the table to the job summary, and uploads `report.json` as the artifact `us_report`.

# Relation to other figures

[Pipeline metrics](/tools/pipeline-metrics.md) keeps its own progress trend, computed from the source tree, so it needs no build. It counts only `src/` C functions and `asm/nonmatchings` stubs and skips hand-written assembly and data. Its totals therefore differ from this report's.
