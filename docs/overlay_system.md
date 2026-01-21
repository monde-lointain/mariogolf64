# Mario Golf 64 Overlay System

## Overview

Mario Golf 64 uses a dynamic overlay system to load code segments on demand. The game has 17 registered overlays plus one unregistered "gap" overlay.

## MGOverlayInfo Structure

```c
typedef struct {
    /* 0x00 */ s8 active;
    /* 0x04 */ s8 *module_indices;    // Pointer to byte array of module IDs
    /* 0x08 */ u32 rom_start;
    /* 0x0C */ u32 rom_end;
    /* 0x10 */ void *vram_start;      // Text section start
    /* 0x14 */ void *data_start;      // Text end / Data start
    /* 0x18 */ void *bss_start;       // Data end / BSS start
    /* 0x1C */ void *vram_end;        // BSS end
    /* 0x20 */ void (*constructor)(void);
    /* 0x24 */ void (*destructor)(void);
} MGOverlayInfo; // size: 0x28
```

## Overlay Table

Located at ROM `0x091358`, containing 17 entries:

| ID | ROM Range | VRAM | BSS End | Constructor | Purpose |
|----|-----------|------|---------|-------------|---------|
| 0 | `0xADD30-0xDE9B0` | `0x801F4A30` | `0x8022D6A0` | - | Shared with 1,2 |
| 1 | `0xADD30-0xDE9B0` | `0x801F4A30` | `0x8022D6A0` | - | Shared with 0,2 |
| 2 | `0xADD30-0xDE9B0` | `0x801F4A30` | `0x8022D6A0` | `0x80025D30` | Shared with 0,1 |
| 3 | `0xDE9B0-0x11BA50` | `0x801F4A30` | `0x8023EDF0` | `0x8005A580` | Course/Gameplay |
| 4 | `0x128580-0x1300A0` | `0x8024D800` | `0x8025D360` | - | |
| 5 | `0x1300A0-0x136BA0` | `0x8024D800` | `0x8025C8B0` | - | |
| 6 | `0x136BA0-0x1370B0` | `0x8024D800` | `0x80250B70` | - | |
| 7 | `0x1370B0-0x14EA70` | `0x801F4A30` | `0x8022C2B0` | `0x80025D54` | |
| 8 | `0x14EA70-0x14FB50` | `0x801F4A30` | `0x8022BE30` | `0x801F4C84` | |
| 9 | `0x14FB50-0x1508D0` | `0x801F4A30` | `0x801FFD40` | `0x801F4A40` | |
| 10 | `0x1508D0-0x150990` | `0x801F4A30` | `0x801F4B00` | - | |
| 11 | `0x150990-0x15BB50` | `0x80241800` | `0x8024CF40` | - | |
| 12 | `0x15BB50-0x15F250` | `0x80241800` | `0x8024AD00` | - | |
| 13 | `0x15F250-0x1619A0` | `0x80241800` | `0x80244000` | - | |
| 14 | `0x1619A0-0x165BC0` | `0x80241800` | `0x80245AF0` | - | |
| 15 | `0x165BC0-0x16D650` | `0x801F4A30` | `0x801FC5D0` | `0x801F4B20` | |
| 16 | `0x16D650-0x171770` | `0x801F4A30` | `0x80223810` | `0x801F4B9C` | |

## VRAM Regions

Three primary VRAM load addresses are used:

| VRAM | Overlays |
|------|----------|
| `0x801F4A30` | 0-3, 7-10, 15-16 |
| `0x80241800` | 11-14 |
| `0x8024D800` | 4-6 |

Overlays sharing the same VRAM are **mutually exclusive** - only one can be loaded at a time.

## Loading Process

```c
bool load_overlay(s32 overlay_index) {
    // 1. Check for conflicts with currently loaded overlays
    // 2. DMA from ROM to VRAM
    nuPiReadRom(overlay->rom_start, overlay->vram_start, size);

    // 3. Zero BSS section
    bzero(overlay->bss_start, bss_size);

    // 4. Flush caches
    osWritebackDCache(...);
    osInvalICache(...);

    // 5. Call constructor if present
    if (overlay->constructor != NULL) {
        overlay->constructor();
    }

    overlay->active = true;
}
```

## Gap Overlay

ROM `0x11BA50-0x128580` is NOT in the overlay table but contains valid code.

See [gap_overlay_analysis.md](gap_overlay_analysis.md) for details.

## Overlay Constructors

Some overlays have constructor/destructor functions:

| Overlay | Constructor | Purpose |
|---------|-------------|---------|
| 2 | `0x80025D30` | Basic init |
| 3 | `0x8005A580` | Course data loading (large, 0x778 bytes) |
| 7 | `0x80025D54` | |
| 8 | `0x801F4C84` | Within overlay itself |
| 9 | `0x801F4A40` | Within overlay itself |
| 15 | `0x801F4B20` | Within overlay itself |
| 16 | `0x801F4B9C` | Within overlay itself |

## Module Index System

Each overlay has a `module_indices` pointer to a byte array. These indices map logical module IDs to overlays and are used for:

1. Tracking which modules are active
2. Preventing conflicting overlays from loading
3. Managing RDP request conflicts

```c
// D_800D2930[module_id] = overlay_index that provides this module
// D_800FC858[module_id] = 1 if module is currently available
```

## Splat Configuration

For proper disassembly, use `exclusive_ram_id` to isolate symbol namespaces:

```yaml
- name: overlay_3
  type: code
  start: 0xDE9B0
  vram: 0x801F4A30
  exclusive_ram_id: ovl3
  symbol_name_format: ovl3_$VRAM
  bss_size: 0xD320
  subsegments:
    - [0xDE9B0, asm]
    - [0x117DA0, data]
    - { start: 0x11BA50, type: bss, vram: 0x80231AD0 }
```
