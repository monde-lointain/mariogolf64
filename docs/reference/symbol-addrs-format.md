---
type: Reference
title: "symbol_addrs.txt format"
description: "How symbols are named and annotated for Splat: line syntax, the options this project uses, and rules for duplicates, overlays and absolutes."
tags: [symbols, splat]
resource: ../../symbol_addrs.txt
---

`symbol_addrs.txt` is an input to Splat (`symbol_addrs_path` in `mariogolf64.yaml`), not directly to the linker. Splat takes names, types and sizes from it when it disassembles, and writes `undefined_funcs_auto.txt` and `undefined_syms_auto.txt` with `name = address;` lines for referenced symbols that no extracted segment defines; the Makefile passes both to the linker with `-T` (`LINKER_SCRIPTS`). Symbols without an entry get default names: `func_<VRAM>` and `D_<VRAM>`, or `func_ovl<N>_<VRAM>` and `D_ovl<N>_<VRAM>` in overlays. Upstream reference: [Splat, Declaring symbols](https://github.com/ethteck/splat/wiki/Adding-Symbols).

# Line syntax

```text
name = 0xVRAM; // option:value option:value
```

Blank lines and lines starting with `//` are skipped. Every other line needs exactly one `;`, counted over the whole line including its comment, and the code before `//` ends with it. The name is before `=`. After `//`, the text is split on single spaces: a token with one `:` is an option, a token with two or more `:` is an error, and a token without `:` is ignored. A repeated name is an error unless both lines set `allow_duplicated:True`. A repeated VRAM is an error only when both lines also share the segment and ROM address.

# Options used here

| Option | Meaning |
|---|---|
| `type:func` | A function. Other types used here: `u8`, `s16`, `s32`, `u32`, `u64`, `f64`, `asciz`, and the capitalized type names `Color` and `Ovl8TextRecord`. Splat also accepts `s8`, `u16`, `f32`, `jtbl` and `label`. |
| `size:0x..` | Size in bytes. `tools/find_oversized_symbols.py` reports data symbols whose linked size is larger than this, so keep it equal to the real object. |
| `rom:0x..` | ROM address. Optional: [data-differ](/tools/data-differ.md) falls back to the linker map. |
| `segment:<name>` | Binds the symbol to a YAML segment. Needed when the same VRAM appears in more than one segment, as overlays do (`segment:overlay_4`). Splat rejects a name that matches no segment. |
| `absolute:True` | Not relocated with a segment: hardware addresses such as `osTvType`, and linker-generated symbols such as `asset_directory_ROM_START` ([add-asset](/procedures/add-asset.md)). Splat warns if `rom` or `segment` is also set. |
| `allow_duplicated:True` | The option needs a value: a bare `allow_duplicated` token is ignored. Both lines of a repeated name or VRAM must set it. Used on `_getRate`, `_getVol` and `_decodeChunk`. |

# Rules

* Rename by editing the name in place and updating every C reference ([rename-global](/procedures/rename-global.md)). Keep `size:` and `type:` annotations.
* For a field inside a known struct, annotate the **owning** symbol with `type:` and `size:` so the output is `symbol + offset`; never add a standalone symbol or a linker alias for the field ([match-ultralib-segment](/procedures/match-ultralib-segment.md)).
* A run of contiguous symbols accessed sequentially is probably one array, or one struct; fix the layout here even if it forces a wider refactor ([matching method](/procedures/matching-method.md)).
* Only one name per address within a segment: when two linker symbols share an address, declare one.
* Do not add libultra function or data aliases to a linker script; declare the symbol here at its runtime `0x800...` address.
