---
type: Tool
title: "data-differ"
description: "Byte-compare data symbols between the built ROM and baserom.z64; find the first mismatching symbol after a checksum failure."
tags: [data, diff, verify]
resource: ../../tools/data-differ/data_diff.py
---

`tools/data-differ/data_diff.py` byte-compares one data symbol, or scans every symbol, between the built ROM and `baserom.z64`. It is the data counterpart of [asm-differ](/tools/asm-differ.md). `./tools/diff-data <symbol>` is a thin wrapper around the same script and works from anywhere in the repository.

```bash
python3 tools/data-differ/data_diff.py [--find-first-mismatch | <symbol>] [flags]
./tools/diff-data <symbol>
```

The script changes to the repository root before it resolves paths, so the default paths below are relative to that root. It needs a completed build ([build-and-verify](/tools/build-and-verify.md)): the built ROM, ELF and linker map.

# Common uses

```bash
# One symbol; override the size, or show matching bytes too
python3 tools/data-differ/data_diff.py <symbol> --size 0x10
python3 tools/data-differ/data_diff.py <symbol> --show-all

# After a checksum failure: scan symbols in ROM order, stop at the first mismatch
python3 tools/data-differ/data_diff.py --find-first-mismatch
python3 tools/data-differ/data_diff.py --find-first-mismatch --filter course
```

Use `--find-first-mismatch` after a failed build, after changing a data structure, or while working through a large file with many data symbols.

# Flags

| Flag | Default | Effect |
|---|---|---|
| `symbol` | none | Symbol name from `symbol_addrs.txt`. Required unless `--find-first-mismatch` is given. |
| `--find-first-mismatch` | off | Scan all symbols in ROM address order and report the first mismatch. Ignores `symbol`, with a warning. Needs the map and ELF, not `symbol_addrs.txt`. |
| `--filter PATTERN` | none | With `--find-first-mismatch`, check only symbols matching `PATTERN`. |
| `--size SIZE` | from the symbol | Override the size. Accepts `0x10`, `16` or `0b10000`. |
| `--show-all` | off | Show all bytes, not only the differences. |
| `--target-rom` | `baserom.z64` | Target ROM. |
| `--compiled-rom` | `build/mariogolf64.z64` | Built ROM. |
| `--elf` | `build/mariogolf64.elf` | ELF, used for symbol sizes and object types. |
| `--symbol-addrs` | `symbol_addrs.txt` | Symbol table. |
| `--map-file` | `build/mariogolf64.map` | Linker map, used for ROM addresses. |

The exit code is 0 when the data matches and 1 when it differs or the lookup fails.

# How it resolves a symbol

* Each `symbol_addrs.txt` line has the form `name = 0xVRAM; // rom:0xROM size:0xSIZE type:TYPE` ([format](/reference/symbol-addrs-format.md)). `rom:` is optional.
* ROM address: `rom:` from `symbol_addrs.txt`; otherwise the VRAM is mapped to a ROM address through the linker map.
* Size, in priority order: `--size`, then `size:` in `symbol_addrs.txt`, then the ELF symbol size read with `readelf`, then a `.size` directive in `asm/data/`. If none of these gives a size, the script stops with an error.
* Output shows the VRAM, ROM address, size and where the size came from, the target and compiled bytes, and each differing offset.

# Failure modes

| Message | Cause and fix |
|---|---|
| `Target ROM not found` / `Compiled ROM not found` | The file named by `--target-rom` or `--compiled-rom` is missing. Run [build-and-verify](/tools/build-and-verify.md) for the compiled ROM. |
| `Symbol addresses file not found` | Single-symbol mode needs `--symbol-addrs`, which defaults to `symbol_addrs.txt`. |
| `Symbol '<name>' not found in symbol_addrs.txt` | Not in the table. If partial name matches exist, the script lists up to 10 as "Did you mean" on stdout instead. |
| `Could not determine ROM address` | No `rom:` and the linker map has no section covering the VRAM. Add `rom:0x...` to the symbol. |
| `No size specified` | No `size:`, no ELF size and no `.size` directive. Add `size:0x...`, pass `--size`, or make sure the symbol is in the ELF. |
| `Map file not found` / `ELF file not found` | Required by `--find-first-mismatch`. |
| `Warning: '<name>' appears to be a function (FUNC)` | The name refers to code, not data. Use [asm-differ](/tools/asm-differ.md). |
| `Warning: Could not run readelf` | `readelf` (binutils) is missing or failed. This is not fatal; the size falls back to the other sources. |
