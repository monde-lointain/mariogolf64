# Gap Overlay Analysis

## Overview

The "gap overlay" refers to ROM region `0x11BA50-0x128580` (~50KB) that sits between overlay 3 and overlay 4 but is **not listed** in the standard 17-overlay table.

## Key Findings

### VRAM Address: `0x80451A50`

Confirmed by the first function in the gap which returns `0x80451A60`:
```asm
func_ovlgap_80451A50:
    lui   $v0, 0x8045
    jr    $ra
    addiu $v0, $v0, 0x1A60  ; returns 0x80451A60
```

### References to `0x8040xxxx`

The gap overlay makes extensive references to the `0x80400000` region:
- 77+ `j` (jump) instructions to `0x8040xxxx`
- 12+ `jal` (call) instructions to `0x8040xxxx`
- 716+ `lui 0x8041` instructions accessing data

These addresses are in the **N64 Expansion Pak memory region** (4-8MB).

## Memory Architecture

```
Standard N64 (4MB):
  0x80000000 - 0x803FFFFF

With Expansion Pak (8MB):
  0x80000000 - 0x807FFFFF
  └─ 0x80400000+: Expansion Pak RAM
```

### Mario Golf Memory Layout

| Range | Purpose |
|-------|---------|
| `0x80000000-0x80025C50` | Boot/System |
| `0x80025C50-0x800D2930` | Main code |
| `0x800D2930-0x801F4A30` | Main BSS |
| `0x801F4A30-0x803FFFFF` | Overlays + Course data |
| `0x80400000-0x8040CB30` | Terrain code/data (Expansion Pak) |
| `0x80451A50-0x80458080` | Gap overlay code |

## What is `0x80400000`?

The `0x80400000` region contains **runtime-loaded terrain data**:

1. **Terrain manipulation functions** (`func_80400C0C`, `func_804066C8`, etc.)
2. **Terrain vertex/mesh data** (accessed via `get_terrain_vertex_pointer`)
3. **Course-specific BSS** for terrain calculations

This data is loaded from compressed course assets when a golf course is initialized.

## Functions at `0x8040xxxx`

These are terrain-related functions loaded at runtime:

| Address | Purpose (inferred) |
|---------|-------------------|
| `func_80400C0C` | Terrain rendering setup |
| `func_804066C8` | Terrain vertex processing |
| `func_804079B8` | Terrain collision/height |
| `func_80405AF4` | Terrain attribute lookup |
| ... | (72+ total functions) |

## Data at `0x8040xxxx`/`0x8041xxxx`

| Symbol | Purpose |
|--------|---------|
| `D_8040C5A8` | Terrain grid X coordinate |
| `D_8040C5AC` | Terrain grid Y coordinate |
| `D_8040C5B0` | Saved terrain position X |
| `D_8040C5B4` | Saved terrain position Y |

## Loading Mechanism

1. **Gap overlay** is NOT loaded by standard `load_overlay()` function
2. Loaded separately during course/terrain initialization
3. `0x80400000` region populated by terrain decompression routines
4. Requires Expansion Pak for full functionality

## For Decompilation

### Symbol Configuration

The `func_8040xxxx` and `D_8040xxxx` symbols should be defined as external in `undefined_funcs_auto.txt` and `undefined_syms_auto.txt`:

```
func_80400C0C = 0x80400C0C;
func_804066C8 = 0x804066C8;
...
D_8040C5A8 = 0x8040C5A8;
D_8040C5AC = 0x8040C5AC;
...
```

### YAML Configuration

```yaml
- name: overlay_gap
  type: code
  start: 0x11BA50
  vram: 0x80451A50
  exclusive_ram_id: ovlgap
  symbol_name_format: ovlgap_$VRAM
  subsegments:
    - [0x11BA50, asm]
    - [0x127FD0, data]  # Data section starts here
```

## Overlay Table Context

The gap sits precisely between overlay 3 and overlay 4:

| Overlay | ROM Start | ROM End | VRAM |
|---------|-----------|---------|------|
| Overlay 3 | `0x0DE9B0` | `0x11BA50` | `0x801F4A30` |
| **Gap** | `0x11BA50` | `0x128580` | `0x80451A50` |
| Overlay 4 | `0x128580` | `0x1300A0` | `0x8024D800` |

## Open Questions

1. What triggers loading of the gap overlay?
2. Is the gap overlay used only with Expansion Pak, or is there fallback code?
3. Which specific courses/modes utilize the expanded terrain features?
4. Where in ROM is the compressed terrain code that loads to `0x80400000`?

## Related Files

- `asm/11BA50.s` - Gap overlay disassembly
- `undefined_funcs_auto.txt` - External function definitions
- `undefined_syms_auto.txt` - External symbol definitions
- `src/lz_decompress.c` - Decompression routines (may load terrain data)
