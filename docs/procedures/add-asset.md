---
type: Procedure
name: add-asset
title: "Add an asset and convert ROM-address references"
description: "Replace raw ROM addresses and D_<hex> externs with <name>_ROM_START/_ROM_END symbols by adding named bin segments to mariogolf64.yaml; use when code or asm references a ROM asset by address."
tags: [assets, segments, splat]
---

Every named segment in `mariogolf64.yaml` gets linker-generated `<name>_ROM_START` and `<name>_ROM_END` symbols. When code refers to a ROM address that has no named segment, Splat emits a placeholder such as `D_DAC40 = 0xDAC40;` in `undefined_syms_auto.txt` and C ends up with raw addresses or `D_<hex>` externs. Those placeholders are the to-do list.

# When

Code that uses raw ROM addresses:

```c
extern u8 D_D9B9C0[];
load_sound_effect_bank_from_rom((u32)D_D9B9C0, (u32)D_D9F450 - (u32)D_D9B9C0);

AssetEntry g_asset_table[] = {
    { 0x00224420, 0x00226BA0 },
    { 0x0023CB80, 0x00240520 },
};
```

or `.word D_224420` / `.word 0x00224420` in `asm/data/*.data.s` that a C file is about to own. They become:

```c
USE_ASSET(sound_effect_bank);
load_sound_effect_bank_from_rom((u32)sound_effect_bank_ROM_START,
                                (u32)sound_effect_bank_ROM_END - (u32)sound_effect_bank_ROM_START);
// a segment also starts at 0xD9F450, so sound_effect_bank_ROM_END lands there

USE_ASSET(assets_224420);
USE_ASSET(assets_23CB80);
AssetEntry g_asset_table[] = {
    { (u32)assets_224420_ROM_START, (u32)assets_224420_ROM_END },
    { (u32)assets_23CB80_ROM_START, (u32)assets_23CB80_ROM_END },
};
// segments also start at 0x226BA0 and 0x240520 so the two _ROM_END symbols land there
```

`USE_ASSET(name)` (from `common.h`) declares `name_ROM_START[]` and `name_ROM_END[]`. Code-segment (overlay) addresses already have symbols (`overlay_4_ROM_START`, `_ROM_END`, `_VRAM`, `_VRAM_END`, `_BSS_START`, ...) in the generated `mariogolf64.ld`; use those. Never convert null pointers (`0x00000000`).

# Steps

1. **Check whether the asset exists.** Search `mariogolf64.yaml` for the address (for example `0xD9B9C0`) and search `mariogolf64.ld` for `_ROM_START`.

   For an array, collect every start and end address and sort the unique ones.

2. **Add a named `bin` segment per address** after the overlays, in ascending address order. A segment ends where the next starts, so if the code needs an exact `_ROM_END`, a segment must start at that address too (add `assets_<HEX>` where needed):

   ```yaml
   - name: audio_wave_table
     type: bin
     start: 0x18BE00

   - name: sound_effect_bank
     type: bin
     start: 0xD9B9C0

   - name: assets_D9F450
     type: bin
     start: 0xD9F450
   ```

   Use a descriptive name when the purpose is known, otherwise `assets_<HEX>`.

3. **Name the symbol for the extracted assembly.** Add the start address to `symbol_addrs.txt` as an absolute symbol so `%hi/%lo` pairs and `.word`s use the linker name instead of `D_<hex>`:

   ```text
   sound_effect_bank_ROM_START = 0xD9B9C0; // absolute:True
   ```

   One name per address. When two linker symbols share an address (`audio_wave_bank_ROM_END` and `audio_wave_table_ROM_START`), declare one; C can use both through `USE_ASSET`. Keep two distinct C symbols when the original materialized them separately: merging can let GCC reuse a register and change codegen. See [format](/reference/symbol-addrs-format.md).

4. **Update the code.** Add `USE_ASSET(...)` declarations (sorted by address) after the includes and replace raw addresses or `D_<hex>` externs with the `_ROM_START`/`_ROM_END` symbols. Leave size fields unchanged.

5. **Validate.**

   ```bash
   ./tools/build-and-verify.sh
   python3 tools/data-differ/data_diff.py <symbol>
   ```

   `<symbol>` is a data symbol listed in `symbol_addrs.txt`.

# Common mistakes

* Out-of-order segments: Splat stops with `segments out of order`.
* Zero-size segment: two segments with the same start.
* Wrong `_ROM_END`: it is the next segment's start. Check boundaries in `build/mariogolf64.map`.
* `undefined reference to <name>_ROM_START`: missing segment or `USE_ASSET`.
* Data mismatch in an array: an end symbol resolves to the wrong address.
