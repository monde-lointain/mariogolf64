# Mario Golf 64 ROM Data Regions

ROM: `baserom.z64` (32 MB / 0x2000000 bytes)
SHA1: `e2c4e7a905b29529b49a1619a401fe699224829b`

## Compression Status

**No standard compression detected.** The ROM does not use MIO0, Yay0, Yaz0, LZ77, or other common N64 compression formats. High-entropy regions are raw texture/audio data.

## Region Map

| Start | End | Size | Entropy | Description |
|-------|-----|------|---------|-------------|
| `0x00000000` | `0x00001000` | 4 KB | - | Header + IPL3 |
| `0x00001000` | `0x000ADD30` | 709 KB | 5.9 | Code segment (main) |
| `0x000ADD30` | `0x00170000` | 793 KB | 5.9-6.0 | Extended code/data |
| `0x00170000` | `0x00280000` | 1.1 MB | 7.0-7.4 | Textures/models (high entropy) |
| `0x00280000` | `0x00290000` | 64 KB | 5.9 | Structured data |
| `0x00290000` | `0x00680000` | 4.0 MB | 7.0-7.4 | Textures/models (high entropy) |
| `0x00680000` | `0x006C0000` | 256 KB | 6.8-6.9 | Mixed assets |
| `0x006C0000` | `0x00800000` | 1.3 MB | 7.0-7.4 | Textures/models |
| `0x00800000` | `0x00DA0000` | 5.6 MB | 7.0-7.4 | Textures/models |
| `0x00DA0000` | `0x01050000` | 2.8 MB | 5.8-6.0 | **Music sequence data** |
| `0x01050000` | `0x01070000` | 128 KB | 2.5-5.3 | Sparse/padding |
| `0x01070000` | `0x010C0000` | 320 KB | **7.5-7.9** | **VADPCM audio samples** |
| `0x010C0000` | `0x01720000` | 6.4 MB | 5.5-7.5 | Mixed assets |
| `0x01720000` | `0x02000000` | 14.9 MB | 0.0 | **Empty** (zeros) |

## Notable Structures

### Asset Size Table (0xAB900)

40 entries of 32-bit sizes, possibly referencing character model data:
```
0x002B1C (11036 bytes)
0x003017 (12311 bytes)
0x0024B5 (9397 bytes)
...
Total: ~486 KB
```

### Music Sequence Data (0xDA0000)

N64 sequence format similar to MIDI. Pattern structure:
```
[cmd] [note] [duration] [flags]
Example: 02 31 0a 60
```
- `0x02`: Note command
- `0x31`: Note value (MIDI-like)
- `0x0a 60`: Duration/velocity

### Debug Strings

Original dev path found at `0xABA20`:
```
c:/usgolf64/polychara/pcsub.c
```

GB Transfer Pak strings at `0xAC8C0`:
```
GB PACK OK %d
GB NOPACK %d
GB DEVICE ER %d
```

## Texture Data Characteristics

High-entropy regions (7.0-7.4) are consistent with:
- **CI8**: 8-bit color-indexed textures
- **RGBA16**: 16-bit RGBA (5551 format)
- **IA8/IA16**: Intensity + Alpha formats

No compression headers found; textures appear stored raw.

## VADPCM Audio Region (0x1070000)

Entropy 7.5-7.9 with characteristics:
- High byte diversity (256/256 unique in 8KB sample)
- 48% adjacent byte similarity (spatial coherence)
- Top bytes: `0x02` (3.7%), `0x00`/`0xFF` (1.3% each)

Likely VADPCM-compressed audio samples used by the N64 audio system.

## Summary

| Content Type | Approximate Size | Notes |
|--------------|------------------|-------|
| Code | ~1.5 MB | MIPS assembly |
| Textures/Models | ~12 MB | Raw, uncompressed |
| Music Sequences | ~2.8 MB | N64 seq format |
| Audio Samples | ~320 KB | VADPCM |
| Unused | ~15 MB | Zero padding |
