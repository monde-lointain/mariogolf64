---
type: Tool
title: "Progress report"
description: "tools/progress.py: code and data matched in the last build, by bytes and by functions or data symbols, per category or segment; writes the objdiff report.json that decomp.dev reads."
tags: [status, workflow, ci]
resource: ../../tools/progress.py
---

`tools/progress.py` measures the last build: it reads the linker map named in `decomp.yaml` (`build/mariogolf64.map`) and each object file the map links. Run `./tools/build-and-verify.sh` first; the figures describe whatever `build/` holds.

```bash
python3 tools/progress.py                  # Game, Libraries and Total rows
python3 tools/progress.py --segments       # plus one row per ROM segment
python3 tools/progress.py -u               # plus unmatched functions, largest first
python3 tools/progress.py -o report.json   # plus the decomp.dev report
```

# Measures

| Column | Matched / total |
|---|---|
| Code bytes | `.text` bytes of matched functions / all `.text` bytes |
| Functions | matched functions / all functions |
| Data bytes | matched `.data`, `.rodata` and `.bss` bytes / all of them (objdiff counts `.bss` as data too) |
| Data symbols | matched data objects / all data objects |

A function or data object is one address, however many aliases name it. A symbol without a `.size` runs to the next one. Data symbols are named objects only: string literals in C have no symbol, so the count drops a little as `asm/data` strings move into C.

# What counts as matched

| Object source | Matched |
|---|---|
| `src/**/*.c` | everything except symbols with a `<name>.NON_MATCHING` marker: `INCLUDE_ASM` functions and the rodata included with them |
| `src/**/*.s` | everything: original hand-written assembly (`hasm` in `mariogolf64.yaml`), including `entry.s` |
| `asm/**/*.s` | nothing: splat data not yet moved into C |

Binary blobs are neither code nor data to decompile, so they are listed on the `Not counted` line and left out of every total: `build/assets/`, `asm/` sources that `.incbin` a binary (the RSP microcode), and the ROM header `asm/header.s`.

**Libraries** are units under `src/libultra`, `libnusys`, `libnaudio`, `libmus`, `libkmc` and `mgu`. **Game** is everything else, including `asm/data` files: they have no owning source file, so library data still in `asm/data` counts as game data.

# decomp.dev report

`-o PATH` writes an [objdiff](https://github.com/encounter/objdiff) progress report (`report.proto`, version 2, proto3 JSON). There is one unit per object, named by its source path without the extension (`src/main/audio_mgr`), with its segment as `module_name`. A unit is `complete` when all its code and data are matched. The `game` and `libs` categories match the table rows. The report has no field for data symbol counts, so they appear only in the table.

CI (`.github/workflows/build.yml`) runs `progress.py --segments -o report.json` after the ROM verifies. It writes the table to the job summary and uploads `report.json` as the artifact `us_report`, the name [decomp.dev](https://decomp.dev) looks for to read version `us`.

# Relation to other figures

The [pipeline-metrics](pipeline-metrics.md) progress trend counts code from the source tree, so it needs no build. It leaves out hand-written assembly and data, which makes its function and byte totals smaller than these. Its unmatched figures equal this report's.
