---
type: Procedure
name: match-data-segment
title: "Match a data or rodata segment"
description: "Map a data or rodata segment to a C file and replace externs with ordered definitions; use when asked to match, add, split or fix one in mariogolf64.yaml."
tags: [data, segments, splat]
---

Code, data, rodata and bss are separate segments in `mariogolf64.yaml` ([format](/reference/splat-yaml.md)). Matching data means telling Splat that a C file owns a byte range, then defining that data in the C file.

# 1. Carve the range in `mariogolf64.yaml`

Most data still lives in large raw blobs such as `- [0x91310, data, main_data]`. Carve the C file's range out and leave the remainder raw. The existing `libmus/aud_sched` entry shows the shape:

```yaml
- [0x91310, data, main_data]
- [0xA2ED0, .data, libmus/aud_sched]   # owned by src/libmus/aud_sched.c
- [0xA2EE0, data, main_data_1a]        # raw tail; numbered like the other tails
```

The third field of a `.data`/`.rodata` entry names the owning source file (`main/get_tile_attribute` in `- [0xA5D30, .rodata, main/get_tile_attribute]`); the naming rule is in [splat-yaml](/reference/splat-yaml.md#rules). Take start and end offsets from the `/* ROM VRAM */` comments in the data assembly under `asm/data/` and from `symbol_addrs.txt`.

An unowned `- [<rom>, rodata]` entry: find which functions reference its labels (jump tables `jtbl_...`, float constants `D_...`; search `asm/`). If they are all C, change it to `- [<rom>, .rodata, <dir>/<file>]`, split any trailing range that belongs to another file into its own `rodata` entry, and define the constants in the C file in ROM order. KMC GCC places a function's jump tables and literals in `.rodata` itself ([data and jump tables](/reference/kmc-gcc/data-and-jump-tables.md)). Within a segment the generated linker script places all `.data` inputs before all `.rodata` inputs (`<seg>_DATA_START` … `<seg>_RODATA_START` in `mariogolf64.ld`), so a carved `.rodata` table lands at its ROM offset only when every later raw entry in the segment is also `rodata`-typed (overlay_11's `- [0x15B8F8, rodata]` stays `rodata` for this reason). If the owning file still has `INCLUDE_ASM` stubs that reference this rodata, use `INCLUDE_RODATA("asm/...", symbol)` for those symbols or leave the entry alone.

Overlays share VRAM, so before widening a `.rodata` entry, grep `asm/data/` for `.word .L<prefix>_…` entries pointing into the new range. Splat moves an asm-only jump table into its function's `.s`, where its labels become local. Any other overlay's raw data that splat named with those labels then fails to link (`undefined reference to .Lovl9_801F5548` from `16CCB0.data.o`). Such a table has to stay raw, so it ends the owned range.

# 2. Rebuild and read the failures

```bash
./tools/build-and-verify.sh
```

It fails because the data is not defined yet; the errors tell you what is needed.

# 3. Define the data in the C file

Open the labeled data in `asm/data/` (the `main_data` range is in `asm/data/main_data.data.s`). The ROM is big endian. The C file already has `extern` declarations for these symbols; replace them with definitions **in the order they appear in the data file**, not in the order of the externs.

# 4. Build and fix

Run `./tools/build-and-verify.sh` again and work through errors and checksum mismatches. If the checksum fails and data is suspected, use [data-differ](/tools/data-differ.md) (`--find-first-mismatch`). For bss, see [match-bss-segment](/procedures/match-bss-segment.md).
