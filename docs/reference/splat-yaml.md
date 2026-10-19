---
type: Reference
title: "mariogolf64.yaml segments"
description: "How the Splat configuration maps ROM ranges to C, assembly, data, rodata, bss and binary assets; the entry forms this project uses and the rules that keep the build matching."
tags: [splat, segments]
resource: ../../mariogolf64.yaml
sources:
  - id: splat-segments
    resource: https://github.com/ethteck/splat/wiki/Segments
    title: Splat, Segments
---

`mariogolf64.yaml` is the Splat configuration. It names the ROM, the generated files, and every segment; Splat produces `asm/`, `assets/` and the linker script `mariogolf64.ld` from it.[^splat-segments] Procedures that edit it: [match-data-segment](/procedures/match-data-segment.md), [match-bss-segment](/procedures/match-bss-segment.md), [match-ultralib-segment](/procedures/match-ultralib-segment.md), [add-asset](/procedures/add-asset.md), [rename-file](/procedures/rename-file.md).

# Options worth knowing

* `symbol_addrs_path: symbol_addrs.txt` ([format](symbol-addrs-format.md)) and `reloc_addrs_path: reloc_addrs.txt`.
* `hasm_in_src_path: True`: `hasm` sources live under `src/` next to the C, with a `<dir>/<stem>` name.
* `generate_asm_macros_files: False`: `include/macro.inc`, `include_asm.h` and `labels.inc` are hand-maintained, because Splat's template emits a directive KMC `as` rejects. Do not turn this on.
* `gfx_ucode: f3dex2`, `hardware_regs: True`, `compiler: KMC`.

# Top-level layout

Top-level segments, in ROM order: `header`, `ipl3`, `entry` (`hasm` `entry/entry`), the big `code` segments (`main`, overlays, ...) each with `subsegments`, then named `bin` asset segments after the overlays. Entries must be in ascending address order, or Splat stops with `segments out of order`.

# Subsegment forms

| Form | Meaning |
|---|---|
| `- [0x1130, c, main/func_80025D30]` | C file `src/main/func_80025D30.c`. Extraction creates it with one single-line `INCLUDE_ASM` per function if it does not exist. |
| `- [0x7E360, hasm, libultra/os/setintmask]` | Hand-written assembly `src/libultra/os/setintmask.s`. |
| `- [0x..., asm]` | Assembly-only; no C file yet (`asm/<rom offset>.s`). |
| `- [0xA2ED0, .data, libmus/aud_sched]` | Data owned by the C file of that name. Third field = the code segment name. |
| `- [0xA5D30, .rodata, main/get_tile_attribute]` | Same for rodata. |
| `- [0x91310, data, main_data]`, `- [0xA5CE0, rodata]` | Raw (unowned) data / rodata, emitted as assembly under `asm/data/`. |
| `- { type: .bss, vram: 0x800D2930, name: main/func_80025D30 }` | Bss owned by a C file. `- { type: bss, vram: ... }` is raw bss. |
| `- name: sound_effect_bank` + `type: bin` + `start: 0xD9B9C0` | Named asset; generates `<name>_ROM_START` / `<name>_ROM_END`. |
| `textbin`, `databin` | Binary blobs included as text or data. |

# Rules

* A segment ends where the next begins; two segments at one address make a zero-size segment.
* A `.data`/`.rodata`/`.bss` entry's name must equal its code segment's name, directory included; renaming one means renaming all.
* Per-function assembly is extracted under `asm/nonmatchings/<segment>/<file>/<function>.s`; the `INCLUDE_ASM("asm/nonmatchings/<segment>/<file>", <function>)` path must match, and the whole macro must stay on **one line** because Splat reads the function name from its second argument.
* After editing the YAML run [build-and-verify](/tools/build-and-verify.md); `make extract` regenerates everything.

[^splat-segments]: Splat, Segments
