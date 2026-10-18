---
name: convert-asset-array
description: Convert arrays with hardcoded PI ROM addresses to use <name>_ROM_START/<name>_ROM_END symbols. Covers adding USE_ASSET declarations, updating array initialization, and configuring mariogolf64.yaml segments.
---

# Convert Asset Arrays to ROM Symbols

## Overview

When working with data arrays that contain hardcoded ROM addresses (asset tables, overlay tables, DMA descriptors), you need to:

1. Add a named segment to `mariogolf64.yaml` for each unique address (see the `add-asset-to-yaml` skill)
2. Add `USE_ASSET` declarations for each segment
3. Convert the array to use `_ROM_START`/`_ROM_END` symbols

## When to use

Use this skill when you see arrays with hardcoded addresses like:

```c
AssetEntry g_asset_table[] = {
    { 0x00224420, 0x00226BA0 },
    { 0x0023CB80, 0x00240520 },
};
```

or extracted data such as `.word D_224420` / `.word 0x00224420` in `asm/data/*.data.s` that a C file is about to own. These should become:

```c
USE_ASSET(assets_224420);
USE_ASSET(assets_23CB80);

AssetEntry g_asset_table[] = {
    { (u32)assets_224420_ROM_START, (u32)assets_224420_ROM_END },
    { (u32)assets_23CB80_ROM_START, (u32)assets_23CB80_ROM_END },
};
```

Overlay entries use the existing code-segment symbols (`overlay_4_ROM_START`, `overlay_4_ROM_END`, `overlay_4_VRAM`, `overlay_4_VRAM_END`, `overlay_4_BSS_START`, ...) from `mariogolf64.ld`.

## Steps

### Step 1: Extract the pattern

For each array entry, collect the start address and the end address. Collect all unique addresses, sorted.

### Step 2: Update mariogolf64.yaml

For each start address, add a named `bin` segment after the overlays, in ascending address order. If an end address is not the start of the next segment, add a segment that starts there so the previous segment's `_ROM_END` lands on it:

```yaml
  - name: assets_224420
    type: bin
    start: 0x224420

  - name: assets_226BA0
    type: bin
    start: 0x226BA0
```

### Step 3: Name the symbols for the extracted assembly

Add each start address to `symbol_addrs.txt` as `<name>_ROM_START = 0x<addr>; // absolute:True` so code and data that still live in assembly reference the linker symbol.

### Step 4: Update the array initialization

Add `USE_ASSET(<name>);` declarations (sorted by address) after the includes, and replace each raw address with the matching symbol. Keep any size fields unchanged.

### Step 5: Validate

```bash
./tools/build-and-verify.sh
python3 tools/data-differ/data_diff.py g_asset_table
```

Watch for errors:

- **Segment has zero size** - a segment followed by another at the same address
- **Segments out of order** - YAML entries not in ascending address order
- **Undefined reference to `<name>_ROM_START`** - missing segment or `USE_ASSET`
- **Data mismatch** - an end symbol resolves to the wrong address; check segment boundaries with `grep <name> build/mariogolf64.map`
