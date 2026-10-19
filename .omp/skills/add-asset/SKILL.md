---
name: add-asset-to-yaml
description: Replacing a raw PI ROM address with a linker-generated asset symbol requires a named segment in mariogolf64.yaml. Use this skill when C code or data refers to a ROM offset (D_<hex> symbols below 0x2000000, or literal ROM addresses) that should become <name>_ROM_START/_ROM_END.
---

# Add a new asset to mariogolf64.yaml

## Overview

Every named segment in `mariogolf64.yaml` gets linker-generated `<name>_ROM_START` and `<name>_ROM_END` symbols. Code that DMA-loads data from the cartridge refers to these PI ROM addresses. When the referenced address has no named segment, Splat emits a placeholder such as `D_DAC40 = 0xDAC40;` in `undefined_syms_auto.txt`, and C code ends up with raw addresses or `D_<hex>` externs.

## When to add an asset

You need to add an asset when you see code that uses raw ROM addresses directly, such as:

```c
extern u8 D_D9B9C0[];
load_sound_effect_bank_from_rom((u32)D_D9B9C0, (u32)D_D9F450 - (u32)D_D9B9C0);
```

or

```c
.romStart = 0x00D9B9C0,
```

These should be converted to asset references:

```c
USE_ASSET(sound_effect_bank);

load_sound_effect_bank_from_rom((u32)sound_effect_bank_ROM_START,
                                (u32)sound_effect_bank_ROM_END - (u32)sound_effect_bank_ROM_START);
```

`USE_ASSET(name)` (from `common.h`) declares `name_ROM_START[]` and `name_ROM_END[]`.

Addresses that are the start of a code segment (an overlay) already have symbols: use `overlay_4_ROM_START`, `overlay_4_ROM_END`, `overlay_4_VRAM`, etc. from the generated `mariogolf64.ld`.

## Steps to add an asset

### 1. Check if the asset already exists

Search `mariogolf64.yaml` for the address and look at the generated linker script:

```bash
grep -n "0xD9B9C0" mariogolf64.yaml
grep -n "_ROM_START" mariogolf64.ld | head
```

### 2. Add a named bin segment in mariogolf64.yaml

Assets follow the overlays as top-level `bin` segments, sorted by start address:

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

Choose a descriptive name when the purpose is known; otherwise use `assets_<HEX>`. A segment ends where the next one starts, so if the code needs an exact end address (`_ROM_END`), make sure a segment starts at that address too (add an `assets_<HEX>` segment if necessary).

### 3. Name the symbol for the extracted assembly

Add the start address to `symbol_addrs.txt` as an absolute symbol so `%hi/%lo` pairs and `.word` references in the extracted assembly use the linker-generated name instead of a `D_<hex>` placeholder:

```text
sound_effect_bank_ROM_START = 0xD9B9C0; // absolute:True
```

Only one name can be declared per address. When two linker symbols share an address (`audio_wave_bank_ROM_END` and `audio_wave_table_ROM_START`), declare one; C can still use both through `USE_ASSET`. Keep two distinct C symbols when the original code materialized them separately: merging them into one symbol can let GCC reuse a register and change code generation.

### 4. Update code to use asset references

Replace the raw addresses or `D_<hex>` externs with `USE_ASSET(...)` and the `_ROM_START`/`_ROM_END` symbols.

## Common Mistakes

- **Out of order segments**: the YAML must be in ascending order by start address; Splat stops with `segments out of order`.
- **Incorrect asset size**: `_ROM_END` is the start of the next segment. If the code's end address is not a segment boundary, add a segment there.
- **Zero-sized segment**: two segments with the same start address.

## Validation

After adding assets, verify the build succeeds:

```bash
./tools/build-and-verify.sh
```

## Tips

- Raw ROM references that still need assets show up as small values in `undefined_syms_auto.txt` (for example `D_DAC40 = 0xDAC40;`).
- Null pointers (`0x00000000`) should not be converted to assets.
