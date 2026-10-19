---
type: Reference
title: "mariogolf64.yaml segments"
description: "How the Splat configuration maps ROM ranges to C, assembly, data, rodata, bss and binary assets; the entry forms this project uses and the rules that keep the build matching."
tags: [splat, segments]
resource: ../../mariogolf64.yaml
---

`mariogolf64.yaml` is the Splat configuration. It names the ROM, the generated files and every segment; Splat produces `asm/`, `assets/` and the linker script `mariogolf64.ld` from it. Procedures that edit it: [match-data-segment](/procedures/match-data-segment.md), [match-bss-segment](/procedures/match-bss-segment.md), [match-ultralib-segment](/procedures/match-ultralib-segment.md), [add-asset](/procedures/add-asset.md), [rename-file](/procedures/rename-file.md).

# Options worth knowing

* `symbol_addrs_path` and `reloc_addrs_path` are lists (`symbol_addrs.txt`, `reloc_addrs.txt`). The symbol file format is in [symbol-addrs format](/reference/symbol-addrs-format.md).
* `hasm_in_src_path: True` puts hand-written `hasm` sources under `src/`. The name in each `hasm` line gives the path: `[0x7E360, hasm, libultra/os/setintmask]` is `src/libultra/os/setintmask.s`.
* `use_legacy_include_asm: False` makes the second argument of `INCLUDE_ASM` the function name (see [Rules](/reference/splat-yaml.md#rules)).
* `generate_asm_macros_files: False` (Splat's default is `True`). Splat's template would overwrite `include/macro.inc`, `include_asm.h` and `labels.inc`, and it emits a `.internal _MACRO_INC_GUARD` directive that KMC binutils 2.6 `as` does not recognize. Those three files are maintained by hand.
* `gfx_ucode: f3dex2`, `hardware_regs: True`, `compiler: KMC`, `platform: n64`.

# Top-level layout

Upstream grammar: [Splat, Segments](https://github.com/ethteck/splat/wiki/Segments). Top-level segments, in ROM order: `header`, `ipl3`, `entry` (`[0x1000, hasm, entry/entry]`), `main` (placed after `entry` with `follows_vram`), `overlay_0` through `overlay_16` with `overlay_gap` between `overlay_3` and `overlay_4` (the `overlay_0` comment says overlays 1 and 2 share its ROM), then named segments such as `sound_effect_bank`. The list ends with `- [0x2000000]`, the ROM size: `baserom.z64` is 33554432 bytes.

| Key | Meaning |
|---|---|
| `follows_vram: entry` | Places `main` in VRAM after `entry`. Splat rejects it together with `vram`. |
| `bss_size` | Size of the segment's bss. The segment's bss entry receives it, so that entry must not set its own. |
| `exclusive_ram_id` | Overlay identifier, `ovl0`, `ovl3`, `ovlgap`, and so on. |
| `symbol_name_format` | Pattern for generated symbol names. `ovl0_$VRAM` gives the `ovl0_<VRAM>` part of names such as `func_ovl0_801F4A30`. |

# Subsegment forms

| Form | Meaning |
|---|---|
| `- [0x1130, c, main/func_80025D30]` | C file `src/main/func_80025D30.c`. Extraction creates it, with one `INCLUDE_ASM` per function, if it does not exist. |
| `- [0x7E360, hasm, libultra/os/setintmask]` | Hand-written assembly `src/libultra/os/setintmask.s`. |
| `[start, asm, name]` | Assembly-only, with no C file. Not used in this repo. |
| `- [0xA2ED0, .data, libmus/aud_sched]` | `.data` owned by the file `libmus/aud_sched`; see [Rules](/reference/splat-yaml.md#rules). |
| `- [0xA5D30, .rodata, main/get_tile_attribute]` | `.rodata` owned by the file `main/get_tile_attribute`. |
| `- [0x91310, data, main_data]` | Raw data with a name, emitted to `asm/data/main_data.data.s`. |
| `- [0x115D80, data]` | Raw data with no name, emitted to `asm/data/115D80.data.s`. Rodata works the same way: `[0xA5CE0, rodata]` gives `asm/data/A5CE0.rodata.s`. |
| `- { type: bss, vram: 0x800D2930 }` | Raw bss, as in `main`. No `start:`. |
| `- { start: 0xDE9B0, type: bss, vram: 0x802256B0 }` | Overlay bss. `start:` is the ROM address of the next segment; all 15 overlay bss entries follow this. |
| `- { type: .bss, vram: ... }` | `.bss` owned by a file, as `.data` is. Not used in this repo. |
| `- name: sound_effect_bank`, `type: bin`, `start: 0xD9B9C0` | Binary asset: `assets/sound_effect_bank.bin`, with `sound_effect_bank_ROM_START` and `_ROM_END` in the linker script. |
| `textbin`, `databin` | `[start, textbin, name]` links as `.text`; `[start, databin, name]` links as `.data`. `rodatabin` links as `.rodata` and is not used here. |

# Rules

* A segment ends where the next begins. Splat stops with `segments out of order` when a segment starts after the next one; two segments at the same address are allowed.
* A `.data` or `.rodata` entry's name is the file that owns it, relative to `src/` and without extension. When that file has code, the name equals its `c`, `hasm`, `textbin` or `databin` subsegment's name; a data-only file has no subsegment (for example `src/libultra/io/vitbl.c`, `src/libkmc/atbl.c`, `src/libultra/gu/libm_vals.s`). Renaming a file means changing every entry that names it ([rename-file](/procedures/rename-file.md)).
* Per-function assembly is extracted under `asm/nonmatchings/<segment>/<file>/<function>.s`. `INCLUDE_ASM` takes the path `asm/nonmatchings/<segment>/<file>` as its first argument and the function name as its second, for example `INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", func_8002A640);` in `src/main/func_8002A640.c`. Keep each `INCLUDE_ASM` on one line, as the existing files do.
* After editing the YAML, run [build-and-verify](/tools/build-and-verify.md). `make extract` runs `rm -rf asm` before Splat, so hand edits under `asm/` are lost.
