---
type: Reference
title: "symbol_addrs.txt format"
description: "Line syntax of symbol_addrs.txt as Splat parses it, the options and types this repo uses, and rules for duplicates, overlay labels and absolutes; open before editing symbols."
tags: [symbols, splat]
resource: ../../symbol_addrs.txt
---

`symbol_addrs.txt` is a Splat input (`symbol_addrs_path` in `mariogolf64.yaml`), not a linker input. Splat reads names, types and sizes from it. Splat also writes `undefined_funcs_auto.txt` and `undefined_syms_auto.txt` with `name = address;` lines for referenced symbols that no extracted segment defines. The Makefile passes both to the linker (`LINKER_SCRIPTS`). Upstream reference: [Splat, Declaring symbols](https://github.com/ethteck/splat/wiki/Adding-Symbols).

Splat code paths below are in the pinned package `.venv/lib/python3.14/site-packages/splat/` (see [mariogolf64.yaml segments](/reference/splat-yaml.md)).

# Line syntax

```text
name = 0xVRAM; // option:value option:value
```

* Blank lines and lines starting with `//` are skipped (`util/symbols.py`).
* Each other line must contain exactly one `;`, counted over the whole line.
* The text after `//` is split on single spaces. A token with one `:` is an option. A token with more than one `:` is an error. A token with no `:` is ignored.
* An unknown option name is ignored without an error.
* `allow_duplicated` is set by its presence; its value is not read.

# Default names

A symbol without a line here gets a name from `default_name` (`util/symbols.py`): `func_<suffix>`, `jtbl_<suffix>`, `D_<suffix>` for data, and `.L<suffix>` for labels. The suffix comes from the segment's `symbol_name_format`, which defaults to `$VRAM` (`util/options.py`). Overlay segments set `symbol_name_format: ovl<N>_$VRAM` in `mariogolf64.yaml` (the `overlay_0` entry), so a default name there looks like `func_ovl0_801F4A30`.

# Options used here

Counts are for the current 1463-line file.

| Option | Form | Effect |
|---|---|---|
| `type:` | 1012 lines | Type of the symbol. The accepted set is in [Types](#types). |
| `size:` | 406 lines | Size in bytes. `tools/find_oversized_symbols.py` flags a non-`func` symbol when its linked object size is greater than `size:`. |
| `segment:` | 77 lines | Binds the symbol to a segment in `mariogolf64.yaml`. `main` is used 73 times. The overlay uses are `overlay_3`, `overlay_4`, `overlay_8` and `overlay_9`, once each. An unknown name is an error (`Cannot find segment`). |
| `rom:` | 24 lines | ROM address. [data-differ](/tools/data-differ.md) reads it, and falls back to the linker map when it is absent. |
| `absolute:True` | 10 lines | Not tied to a segment. Splat warns if `rom` or `segment` is also set. Examples: `tlb_refill`, `osTvType`, `osRomBase`, `osResetType`, and the linker-generated `asset_directory_ROM_START` (`asset_directory_ROM_START = 0xE473F0; // absolute:True`, see [add-asset](/procedures/add-asset.md)). |
| `allow_duplicated:True` | 6 lines | Permits a repeated name or a repeated ROM and segment address. Both lines must set it. Used on `_getRate`, `_getVol` and `_decodeChunk`. |
| `ignore:` | 0 lines | Bans the VRAM address (`util/symbols.py`). Not used. |

# Types

Splat accepts any of these as `type:` (`util/symbols.py`):

* Splat's own types: `func`, `jtbl`, `jtbl_label`, `label`, `alabel`.
* Any type known to spimdisasm 1.42.4, for example `s16`, `s32`, `u8`, `u32`, `u64`, `f32`, `f64`, `asciz`.
* A name that starts with an uppercase letter, which Splat treats as a custom type.

Any other lowercase name is an error (`log.error`). Types used in this file: `func` 880, `u32` 85, `s16` 13, `u64` 15, `s32` 5, `u8` 9, `f64` 1, `asciz` 1, `label` 1, and the custom types `Color` 1 and `Ovl8TextRecord` 1.

# Rules

* Rename by editing the name in place and updating every C reference ([rename-global](/procedures/rename-global.md)). Keep the existing `size:` and `type:` annotations.
* For a field inside a known struct, annotate the **owning** symbol with `type:` and `size:` so the output is `symbol + offset`. Do not add a standalone symbol or a linker alias for the field ([match-ultralib-segment](/procedures/match-ultralib-segment.md)). The layout decision itself is in [types and layouts](/reference/kmc-gcc/types-and-layouts.md).
* A run of contiguous symbols accessed in sequence is probably one array or one struct. Fix the layout here even if that forces a wider refactor ([matching method](/procedures/matching-method.md)).
* Two lines with the same `rom` and `segment` at the same address are an error unless both set `allow_duplicated:True`.
* Bind a label that lies in an overlay's VRAM range to that overlay with `segment:` and `type:label`. `ovl9_801F5548 = 0x801F5548; // type:label segment:overlay_9` does this. The comment directly above it gives the reason: `jtbl_ovl9_801F5798` entries 2 and 4 jump to `0x801F5548`, and the binding keeps the label from resolving to overlay_15's leaked `jlabel` at the same VRAM. `jtbl_ovl9_801F5798` itself is not defined in this file.
* Do not add libultra function or data aliases to a linker script. Declare the symbol here at its runtime `0x800...` address.
