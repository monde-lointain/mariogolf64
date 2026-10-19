---
type: Procedure
name: match-data-segment
title: "Match a data or rodata segment"
description: "Match a data segment by mapping it to an existing C file and replacing extern declarations with ordered definitions. Use when asked to match, add, split, or fix a .data or .rodata segment in mariogolf64.yaml."
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

The third field of a `.data`/`.rodata` entry must equal the code segment's name, including its directory (`main/get_tile_attribute` in `- [0xA5D30, .rodata, main/get_tile_attribute]`). Take start and end offsets from the `/* ROM VRAM */` comments in the data assembly under `asm/data/` and from `symbol_addrs.txt`.

An unowned `- [<rom>, rodata]` entry: find which functions reference its labels (jump tables `jtbl_...`, float constants `D_...`; search `asm/`). If they are all C, change it to `- [<rom>, .rodata, <dir>/<file>]`, split any trailing range that belongs to another file into its own `rodata` entry, and define the constants in the C file in ROM order (KMC GCC emits a function's switch jump tables and literals into `.rodata` itself). The linker emits every `.data` input before every `.rodata` input, so a carved `.rodata` table only lands at its ROM offset when every raw entry after it is also `rodata`-typed; a `data` entry after the table pushes the table into the wrong VRAM (overlay_11: `[0x15B8F8, rodata]`, commit `3b0c5b05`). If the owning file still has `INCLUDE_ASM` stubs that reference this rodata, use `INCLUDE_RODATA("asm/...", symbol)` for those symbols or leave the entry alone.

# 2. Rebuild and read the failures

```bash
./tools/build-and-verify.sh
```

It fails because the data is not defined yet; the errors tell you what is needed.

# 3. Define the data in the C file

Open the labeled data in `asm/data/` (before the split it is in `asm/data/main_data.data.s`; after re-extraction the carved range is no longer emitted there). The ROM is big endian. The C file already has `extern` declarations for these symbols; replace them with definitions **in the order they appear in the data file**, not in the order of the externs.

# 4. Build and fix

Run `./tools/build-and-verify.sh` again and work through errors and checksum mismatches. If the checksum fails and data is suspected, use [data-differ](/tools/data-differ.md) (`--find-first-mismatch`). For bss, see [match-bss-segment](/procedures/match-bss-segment.md).
