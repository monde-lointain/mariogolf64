---
type: Reference
title: "symbol_addrs.txt format"
description: "How symbols are named and annotated for Splat: line syntax, the options this project uses, and rules for duplicates, overlays and absolutes."
tags: [symbols, splat]
resource: ../../symbol_addrs.txt
sources:
  - id: splat-symbols
    resource: https://github.com/ethteck/splat/wiki/Adding-Symbols
    title: Splat, Declaring symbols
---

`symbol_addrs.txt` is an input to Splat (`symbol_addrs_path` in `mariogolf64.yaml`), not to the linker. Splat takes names, types and sizes from it when it disassembles; the linker gets `undefined_syms_auto.txt`.[^splat-symbols] Symbols without an entry get default names: `func_<VRAM>` and `D_<VRAM>` (`func_ovl<N>_<VRAM>`, `D_ovl<N>_<VRAM>` in overlays).

# Line syntax

```text
name = 0xVRAM; // option:value option:value
```

Options are whitespace-separated `key:value` pairs with no spaces inside a pair. `//` comments on their own line are allowed. Each name appears once, and each address has one name.

# Options used here

| Option | Meaning |
|---|---|
| `type:func` | A function. Other values: `s8/u8`, `s16/u16`, `s32/u32` (default for data), `f32`, `f64`, `asciz`, `jtbl`, `label`, or a custom capitalized struct name such as `type:OSDevMgr`. |
| `size:0x..` | Size in bytes. Needed by [data-differ](/tools/data-differ.md) and checked by `find_oversized_symbols.py` against the linked ELF size; keep it equal to the real object. |
| `rom:0x..` | ROM address. Optional; data-differ falls back to the linker map. |
| `segment:<name>` | Binds the symbol to a YAML segment. Required for overlay symbols because overlays share VRAM (`segment:overlay_4`). |
| `absolute:True` | Not relocated with a segment: hardware addresses and linker-generated symbols such as `<name>_ROM_START` ([add-asset](/procedures/add-asset.md)). |
| `allow_duplicated:True` | Permits the same name in two places. Only for same-named file-static functions in different libraries. |

# Rules

* Rename by editing the name in place and updating every C reference ([rename-global](/procedures/rename-global.md)). Keep `size:` and `type:` annotations.
* For a field inside a known struct, annotate the **owning** symbol with `type:` and `size:` so the output is `symbol + offset`; never add a standalone symbol or a linker alias for the field ([match-ultralib-segment](/procedures/match-ultralib-segment.md)).
* A run of contiguous symbols accessed sequentially is probably one array, or one struct; fix the layout here even if it forces a wider refactor ([matching method](/procedures/matching-method.md)).
* Only one name per address: when two linker symbols share an address, declare one; C can use both through `USE_ASSET`.
* Do not add libultra function or data aliases to a linker script; declare the symbol here at its runtime `0x800...` address.

[^splat-symbols]: Splat, Declaring symbols
