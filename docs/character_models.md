# Character Model System

## Overview

Mario Golf 64 uses a skeletal animation system with compressed model data stored in ROM. Models are decompressed on-demand to RAM buffers, with segment-relative addressing used for internal pointers.

## Memory Layout

### Key Data Structures

#### CharacterAnimState (0x18C bytes)
Array at `D_801F43F0`, 4 slots for concurrent characters.

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| 0x00 | AnimData* | anim_data | Pointer to animation/model metadata |
| 0x04 | void* | vertex_buffer | Base address of decompressed model data |
| 0x08 | s32 | current_anim | Current animation index |
| 0x0C | s32 | bone_index | Current bone being processed |
| 0x10 | f32 | current_frame | Animation playback position |
| 0x14 | s32 | start_frame | Animation start frame |
| 0x18 | s32 | end_frame | Animation end frame |
| 0x1C | s32 | bone_count | Number of bones in skeleton |
| 0x20 | void* | vtx_buffer_0 | Triple-buffer vertex slot 0 |
| 0x24 | void* | vtx_buffer_1 | Triple-buffer vertex slot 1 |
| 0x28 | void* | vtx_buffer_2 | Triple-buffer vertex slot 2 |
| 0x2C | void* | vtx_ptr | Current vertex buffer (bone matrices) |
| 0x30 | s32 | buffer_index | Current triple-buffer index (0-2) |
| 0x5C | f32 | playback_speed | Animation speed multiplier |
| 0x60 | u8 | flags_60 | State flags |
| 0x61 | u8 | anim_speed | Animation speed (default 0x60) |
| 0x64 | void* | display_list | RDP display list pointer |
| 0x68 | f32 | scale_x | Model X scale |
| 0x6C | f32 | scale_y | Model Y scale |
| 0x70 | f32 | scale_z | Model Z scale |
| 0x80 | u16 | flags | Render flags |
| 0x83 | u8 | is_playing | Animation playing flag |
| 0x85 | u8 | is_enabled | Character enabled flag |
| 0x86 | u8 | blend_frames | Animation blend frame count |

#### AnimData (0x58 bytes)
Two banks exist in ROM data section:
- `D_800CCCC8` - Low LOD bank (144 entries: 4 LOD levels * 36 variants)
- `D_800CFE48` - High LOD bank (18 entries)

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| 0x00 | u32 | rom_start | Compressed data start in ROM |
| 0x04 | u32 | rom_end | Compressed data end in ROM |
| 0x08 | void* | display_list | Segment-relative display list ptr |
| 0x14 | BoneEntry* | bone_anim_header | Frame bounds per bone (segment addr) |
| 0x18 | BonePosEntry* | bone_pos_array | Bone rest pose + parent chains (segment addr) |
| 0x1C | BoneAnimEntry* | bone_list | Keyframe data per bone (segment addr) |
| 0x24 | TextureEntry** | texture_array | Texture entry pointers (segment addr) |
| 0x28 | TileDescriptor* | tile_array | Texture tile descriptors (segment addr) |
| 0x2C | f32* | scale_x_ptr | Pointer to X scale value |
| 0x30 | f32* | scale_y_ptr | Pointer to Y scale value |
| 0x34 | f32* | scale_z_ptr | Pointer to Z scale value |
| 0x44 | u16 | flags | Model flags |
| 0x48 | u32 | extended_rom_start | Extended compression aux data start |
| 0x4C | u32 | extended_rom_end | Extended compression aux data end |
| 0x50 | u32 | vertex_start_offset | Vertex data start within decompressed |
| 0x54 | u32 | vertex_end_offset | Vertex data end within decompressed |

#### BonePosEntry (0x14 bytes)
Named marker positions within coordinate frames defined by matrix chains.

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| 0x00 | s16* | matrix_chain | Array of bone matrix indices to multiply, -1 terminated (segment addr) |
| 0x04 | f32 | trans_x | Local X translation within the coordinate frame |
| 0x08 | f32 | trans_y | Local Y translation within the coordinate frame |
| 0x0C | f32 | trans_z | Local Z translation within the coordinate frame |
| 0x10 | s16 | bone_id | Bone identifier (used to look up this entry) |
| 0x12 | u8[2] | padding | Alignment padding |

**Important**: `matrix_chain` is NOT a parent hierarchy. It's a list of bone matrix indices
(into the 12-slot `bone_matrices` array) that get multiplied together to define a coordinate
frame. Multiple BonePosEntry records can share the same chain, placing different markers
at different translations within that shared frame.

Example from a character model:
```
bone_id=3: chain=[0,1,3,4,5,6,7,8] trans=(33.87, -67.68, 866.32)
bone_id=4: chain=[0,1,3,4,5,6,7,8] trans=(46.26, -33.63, 679.86)
bone_id=5: chain=[0,1,3,4,5,6,7,8] trans=(44.93, -19.88, 617.94)
```
These bones share the same matrix chain but have different translations.

#### BoneAnimEntry (0x34 bytes)
Keyframe animation data for each bone.

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| 0x00 | s16 | bone_id | Bone ID (-1 = end sentinel) |
| 0x02 | s16 | num_rot_keyframes | Rotation keyframe count |
| 0x04 | s16 | num_scale_keyframes | Scale keyframe count |
| 0x08 | s16* | pos_keyframe_times | Position keyframe timestamps |
| 0x0C | s16* | rot_keyframe_times | Rotation keyframe timestamps |
| 0x10 | s16* | scale_keyframe_times | Scale keyframe timestamps |
| 0x14 | PosKeyframe* | pos_data | Position keyframes (x,y,z floats) |
| 0x18 | Quat16* | rot_data | Rotation keyframes (s16 quaternions) |
| 0x1C | ScaleKeyframe* | scale_data | Scale keyframes (x,y,z floats) |
| 0x20-0x32 | s16 | cache_* | Keyframe lookup cache fields |

## Segment-Relative Addressing

All pointers within AnimData that reference decompressed model data use **segment-relative addressing**:

- Segment base: `0x02000000`
- Actual RAM address = `vertex_buffer + (segment_addr - 0x02000000)`

Example:
```
vertex_buffer = 0x80310000
bone_pos_array (segment) = 0x020111CC
bone_pos_array (RAM) = 0x80310000 + (0x020111CC - 0x02000000) = 0x803211CC
```

This scheme allows the same compressed model data to be decompressed to different RAM locations without pointer patching.

## Model Loading Process

### load_character_model(character_id, model_variant, lod_level)

1. **Select AnimData bank**
   - LOD < 4: Use `D_800CCCC8` (low LOD), index = `(lod & 3) * 36 + (variant % 72)`
   - LOD >= 4: Use `D_800CFE48` (high LOD), index = `variant % 18`

2. **Check if already loaded** via `func_800542A0()`

3. **Decompress model data from ROM**
   - Standard: `lz_decompress_dma(rom_start, size)`
   - Extended: First decompress aux data, then main data with `lz_compress_extended_dma()`

4. **Relocate pointers** via `func_800A7720()` (osVirtualToPhysical for DMA setup)

5. **Allocate triple-buffered vertex memory**
   - Three buffers of `vertex_end_offset - vertex_start_offset` bytes (min 0x780)
   - Used for double/triple buffering vertex transforms

6. **Initialize animation state**
   - Set playback parameters from BoneEntry frame bounds
   - Initialize keyframe cache to -1

7. **Copy render parameters** from AnimData to CharacterAnimState

## Skeleton Structure

Characters use a 12-bone skeleton. The bone matrix indices map to:

```
         [1] Spine/Torso
              |
    +---------+---------+
    |                   |
[2] Head            [3] Spine2
                        |
                    [4] Shoulder
                        |
                    [5] Upper Arm
                        |
                    [6] Elbow
                        |
                    [7] Forearm
                        |
                    [8] Hand/Wrist
```

| Index | Bone | Typical Movement |
|-------|------|------------------|
| 0 | Root | Base transform |
| 1 | Spine/Torso | Upper body tilt |
| 2 | Head | Head orientation |
| 3 | Spine2 | Torso twist (follow-through) |
| 4 | Shoulder | Arm adduction/abduction |
| 5 | Upper Arm | Main shoulder rotation |
| 6 | Elbow | Arm extension/flexion |
| 7 | Forearm | Wrist roll/pronation |
| 8 | Hand/Wrist | Club grip, release |
| 9-11 | (varies) | Character-specific |

**During a golf swing** (downswing/follow-through):
- Bones 5, 6, 8 show most movement (~60-70° rotation)
- Bone 8 (hand) shows club release: RotY drops ~55° through impact
- Bone 6 (elbow) extends: RotX increases ~67°
- Animation often freezes briefly at impact pose

The arm chain `[0,1,3,4,5,6,7,8]` builds the full transform from root to hand.

## Bone Matrix Calculation

**Important**: The system uses a two-stage approach:
1. `calculate_bone_matrices` computes **local transforms** for each bone (no parent concatenation)
2. `get_bone_world_position` applies the hierarchy by multiplying matrices via `matrix_chain`

This means `bone_matrices[i]` contains only that bone's local TRS transform, NOT the accumulated
world transform. The hierarchy is only resolved on-demand when querying world positions.

### calculate_bone_matrices(character_id)

Iterates through `bone_list` (BoneAnimEntry array) and for each bone:

1. **Position interpolation** - Linear interpolation between position keyframes
2. **Rotation interpolation** - Quaternion SLERP between rotation keyframes
   - Falls back to NLERP when quaternions nearly parallel (dot product > threshold)
3. **Scale interpolation** - Linear interpolation between scale keyframes
4. **Build LOCAL transform** - `out = scale * rotation * translation` (no parent multiplication)
5. **Convert to fixed-point** - `guMtxF2L()` converts float matrix to N64 Mtx format
6. **Output to bone matrix buffer** at `g_bone_matrix_buffer + bone_index * 64`

**No hierarchy traversal occurs here** — bones are processed sequentially from `bone_list`,
not in parent-child order. Each matrix is independent.

### get_bone_world_position(character_id, bone_id, out_pos)

Calculates world-space position of a named bone marker by resolving the hierarchy:

1. Get `CharacterAnimState` for character
2. Search `bone_pos_array` for matching `bone_id`
3. **Walk `matrix_chain` array** — multiply bone matrices via `guMtxCatL()` to build world transform:
   ```
   accumulated = identity
   for each idx in matrix_chain:  // e.g., [0, 1, 3, 4, 5, 6, 7, 8]
       accumulated = bone_matrices[idx] * accumulated
   ```
4. Apply bone's local translation via `guTranslate()` and concatenate
5. Transform origin (0,0,0) through accumulated matrix via `guMtxXFML()`
6. Scale result by 0.01 (internal units to world units)

Returns: 0 = success, 1 = bone not found, -1 = invalid character

**Why this design?** Storing local transforms is more memory-efficient and allows selective
hierarchy resolution. The RDP likely uses the local matrices directly for skinning, while
world positions are only computed when needed (e.g., for attaching the golf club).

## Triple Buffering

The system uses triple-buffered vertex data for smooth animation:

- `vtx_buffer_0`, `vtx_buffer_1`, `vtx_buffer_2` hold transformed vertices
- `buffer_index` cycles 0 -> 1 -> 2 -> 0
- `vtx_ptr` points to current buffer being rendered
- Allows CPU to write next frame while RDP renders current frame

## Compression

Model data uses a custom LZ compression scheme (see `lz_decompress.c`):

- Standard decompression via `lz_decompress_dma()`
- Extended variant for larger models using `lz_compress_extended_dma()`
- Decompressed data lands at `vertex_buffer` address

## Texture System

### TextureEntry (0x1C bytes)

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| 0x00 | u32 | vertex_base_addr | Base vertex address (segment-relative) |
| 0x04 | u32 | texture_addr | Texture data address + 0x10 offset |
| 0x0E | s16 | tile_index | Index into TileDescriptor array (-1 = default) |
| 0x10 | u16[3] | u | U texture coords for 3 vertices |
| 0x16 | u16[3] | v | V texture coords for 3 vertices |

### TileDescriptor (0x8 bytes)

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| 0x00 | u32 | texture_dram_addr | DRAM address (8-byte aligned) |
| 0x04 | u8 | width | Texture width in texels |
| 0x05 | u8 | height | Texture height in texels |

## Tools

### mg64_bone_dump.py

Dumps bone hierarchy from RAM dump:

```bash
# Dump RAM in GDB while character is loaded:
dump binary memory ram.bin 0x80000000 0x80800000

# List loaded characters:
mg64_bone_dump.py ram.bin --list

# Dump specific character's bones:
mg64_bone_dump.py ram.bin 0
```

Output shows bone IDs, parent chains, and rest pose translations.

## Key Addresses

| Symbol | Address | Description |
|--------|---------|-------------|
| D_801F43F0 | 0x801F43F0 | CharacterAnimState[4] array |
| D_800CCCC8 | 0x800CCCC8 | Low LOD AnimData bank |
| D_800CFE48 | 0x800CFE48 | High LOD AnimData bank |
| g_bone_matrix_buffer | D_80105210 | Current bone matrix output buffer |
| g_anim_frame | D_800FBDF8 | Current animation frame (global) |
| g_start_frame | D_80104B80 | Animation start frame (global) |
| g_end_frame | D_800FE430 | Animation end frame (global) |

## Key Functions

| Function | Address | Description |
|----------|---------|-------------|
| load_character_model | 0x80052C60 | Load and decompress character model |
| calculate_bone_matrices | 0x80053604 | Calculate bone transforms for frame |
| get_bone_world_position | 0x80054550 | Get world position of specific bone |
| get_character_anim_state | 0x80054354 | Get CharacterAnimState for character ID |
| update_vertex_texture_coords | 0x80052XXX | Update vertex UV coords from texture entries |
| lz_decompress_dma | 0x80043810 | Decompress LZ data from ROM via DMA |
| guMtxCatL | 0x800A9558 | Concatenate fixed-point matrices |
| guMtxXFML | 0x800A6574 | Transform point by fixed-point matrix |
