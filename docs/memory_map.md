# Mario Golf 64 Memory Map

## N64 Hardware

### Without Expansion Pak (4MB)
```
0x80000000 - 0x803FFFFF  RDRAM (4MB, cached via KSEG0)
0xA0000000 - 0xA03FFFFF  RDRAM (4MB, uncached via KSEG1)
```

### With Expansion Pak (8MB)
```
0x80000000 - 0x807FFFFF  RDRAM (8MB, cached)
0xA0000000 - 0xA07FFFFF  RDRAM (8MB, uncached)
```

## Mario Golf Memory Layout

### Main Segments

| Start | End | Size | Description |
|-------|-----|------|-------------|
| `0x80000000` | `0x80001000` | 4KB | Exception vectors, boot |
| `0x80001000` | `0x80025C00` | ~150KB | System/libultra |
| `0x80025C00` | `0x80025C50` | 80B | Entry point |
| `0x80025C50` | `0x800D2930` | ~700KB | Main code |
| `0x800D2930` | `0x801F4A30` | ~1.2MB | Main BSS |

### Overlay Region

| Start | End | Size | Description |
|-------|-----|------|-------------|
| `0x801F4A30` | `0x80260000` | ~430KB | Overlay code/data/BSS |
| `0x80241800` | `0x8024D800` | ~54KB | Secondary overlay slot |
| `0x8024D800` | `0x80260000` | ~74KB | Tertiary overlay slot |

### Expansion Pak Region (if present)

| Start | End | Size | Description |
|-------|-----|------|-------------|
| `0x80400000` | `0x8040CB30` | ~50KB | Terrain code/data |
| `0x80451A50` | `0x80458080` | ~26KB | Gap overlay |

## ROM Layout

| ROM Start | ROM End | Size | Description |
|-----------|---------|------|-------------|
| `0x000000` | `0x000040` | 64B | N64 header |
| `0x000040` | `0x001000` | ~4KB | IPL3 bootcode |
| `0x001000` | `0x001050` | 80B | Entry code |
| `0x001050` | `0x0ADD30` | ~700KB | Main segment |
| `0x0ADD30` | `0x171770` | ~800KB | Overlays |
| `0x171770` | `0x2000000` | ~30MB | Assets |

### Overlay ROM Ranges

| Overlay | ROM Start | ROM End | Size |
|---------|-----------|---------|------|
| 0-2 | `0x0ADD30` | `0x0DE9B0` | 196KB |
| 3 | `0x0DE9B0` | `0x11BA50` | 245KB |
| **Gap** | `0x11BA50` | `0x128580` | 50KB |
| 4 | `0x128580` | `0x1300A0` | 31KB |
| 5 | `0x1300A0` | `0x136BA0` | 27KB |
| 6 | `0x136BA0` | `0x1370B0` | 2KB |
| 7 | `0x1370B0` | `0x14EA70` | 97KB |
| 8 | `0x14EA70` | `0x14FB50` | 4KB |
| 9 | `0x14FB50` | `0x1508D0` | 3KB |
| 10 | `0x1508D0` | `0x150990` | 0.2KB |
| 11 | `0x150990` | `0x15BB50` | 45KB |
| 12 | `0x15BB50` | `0x15F250` | 14KB |
| 13 | `0x15F250` | `0x1619A0` | 10KB |
| 14 | `0x1619A0` | `0x165BC0` | 17KB |
| 15 | `0x165BC0` | `0x16D650` | 31KB |
| 16 | `0x16D650` | `0x171770` | 16KB |

## Special Addresses

### Hardware Registers

| Address | Description |
|---------|-------------|
| `0xA4600000` | PI_DRAM_ADDR |
| `0xA4600004` | PI_CART_ADDR |
| `0xA4600008` | PI_RD_LEN |
| `0xA460000C` | PI_WR_LEN |
| `0xA4600010` | PI_STATUS |

### Key Data Structures

| Address | Description |
|---------|-------------|
| `0x800B5F58` | Overlay table (17 entries) |
| `0x800D2930` | Module-to-overlay mapping |
| `0x800FC858` | Module availability flags |
| `0x8012CFC0` | Module RDP request timestamps |

## BSS Sizes

| Segment | BSS Size | BSS Range |
|---------|----------|-----------|
| Main | `0x122100` | `0x800D2930-0x801F4A30` |
| Overlay 0-2 | `0x7FF0` | `0x802256B0-0x8022D6A0` |
| Overlay 3 | `0xD320` | `0x80231AD0-0x8023EDF0` |
| Overlay 4 | `0x8040` | `0x80255320-0x8025D360` |
| Overlay 7 | `0x1FEC0` | `0x8020C3F0-0x8022C2B0` |

## Expansion Pak Detection

The game likely checks for Expansion Pak at boot:
```c
// Pseudocode
if (osMemSize > 0x400000) {
    expansion_pak_present = true;
    // Enable enhanced terrain features
}
```

Without Expansion Pak, the gap overlay and `0x8040xxxx` functions are never loaded/called.
