---
type: Tool
title: "data-differ"
description: "Byte-compare data symbols between the built ROM and baserom.z64; find the first mismatching symbol after a checksum failure."
tags: [data, diff, verify]
resource: ../../tools/data-differ/data_diff.py
---

Like [asm-differ](asm-differ.md) but for data. Either entry point works from anywhere in the repo:

```bash
python3 tools/data-differ/data_diff.py <symbol>
./tools/diff-data <symbol>          # thin wrapper
```

Needs a completed build ([build-and-verify](build-and-verify.md)): it reads `build/mariogolf64.z64`, `build/mariogolf64.elf` and `build/mariogolf64.map`.

# Common uses

```bash
# One symbol; override the size, or show matching bytes too
python3 tools/data-differ/data_diff.py <symbol> --size 0x10
python3 tools/data-differ/data_diff.py <symbol> --show-all

# After a checksum failure: scan symbols in ROM order, stop at the first mismatch
python3 tools/data-differ/data_diff.py --find-first-mismatch
python3 tools/data-differ/data_diff.py --find-first-mismatch --filter course
```

Use `--find-first-mismatch` after a failed build, after changing a data structure, or while working through a large file with many data symbols. Other flags: `--target-rom`, `--compiled-rom`, `--elf`, `--symbol-addrs`, `--map-file`.

# How it resolves a symbol

* Reads `symbol_addrs.txt` lines of the form `name = 0xVRAM; // rom:0xROM size:0xSIZE type:TYPE` ([format](/reference/symbol-addrs-format.md)). `rom:` is optional; otherwise the ROM address comes from the linker map.
* Size priority: `--size`, then `size:` in `symbol_addrs.txt`, then the ELF symbol size, then a `.size` directive in `asm/data/`. If none exists, add `size:0x...` to the symbol.
* Output shows VRAM, ROM address, size, the target and compiled bytes, and each differing offset.

# Failure modes

* *Symbol not found*: it is not in `symbol_addrs.txt`.
* *Could not determine ROM address*: the map has no section covering the VRAM; add `rom:0x...` to the symbol.
* *No size*: add `size:` or pass `--size`.
* *Compiled ROM not found*: build first.
* Needs `readelf` (binutils) for ELF parsing; optional if the symbol has a size elsewhere.
