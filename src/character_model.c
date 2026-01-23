#include "common.h"

// N64 SDK types (from PR/ultratypes.h and PR/gbi.h)
typedef u64 Mtx[8];  // Fixed-point 4x4 matrix (8 double-words)

typedef struct {
    s16 ob[3];  // Position
    u16 flag;
    s16 s, t;   // Texture coords accessible by name
    u8 cn[4];   // Color/normal
} Vtx_t;

typedef u64 Gfx;

// GBI macros (simplified for compilation - use separate statements)
#define G_IM_FMT_RGBA    0
#define G_IM_SIZ_16b     2
#define G_TX_LOADTILE    7
#define G_TX_NOMIRROR    0
#define G_TX_CLAMP       2
#define G_TX_NOLOD       0

#define gDPSetTextureImage(pkt, fmt, siz, width, addr) (void)(*(pkt) = 0)
#define gDPSetTile(pkt, fmt, siz, line, tmem, tile, palette, cmt, maskt, shiftt, cms, masks, shifts) (void)(*(pkt) = 0)
#define gDPLoadSync(pkt) (void)(*(pkt) = 0)
#define gDPLoadBlock(pkt, tile, uls, ult, lrs, dxt) (void)(*(pkt) = 0)
#define gDPPipeSync(pkt) (void)(*(pkt) = 0)
#define gDPSetTileSize(pkt, tile, uls, ult, lrs, lrt) (void)(*(pkt) = 0)

/**
 * Texture entry - 28 bytes (0x1C)
 * Describes a textured triangle with UV coordinates
 */
typedef struct TextureEntry {
    /* 0x00 */ u32 vertex_base_addr;    // Base vertex address for this tri
    /* 0x04 */ u32 texture_addr;        // Texture data address (+ 0x10 offset)
    /* 0x08 */ u8 pad08[0x06];
    /* 0x0E */ s16 tile_index;          // Index into tile descriptor array (-1 = use default)
    /* 0x10 */ u16 u[3];                // U coords for 3 vertices
    /* 0x16 */ u16 v[3];                // V coords for 3 vertices
} TextureEntry; // size = 0x1C

/**
 * Tile descriptor - 8 bytes
 * Contains texture DRAM address and dimensions
 */
typedef struct TileDescriptor {
    /* 0x00 */ u32 texture_dram_addr;   // DRAM address of texture (8-byte aligned)
    /* 0x04 */ u8 width;                // Texture width in texels
    /* 0x05 */ u8 height;               // Texture height in texels
    /* 0x06 */ u8 pad06[0x02];
} TileDescriptor; // size = 0x8

/**
 * Animation data entry - 88 bytes (0x58)
 * Stored in D_800CCCC8 (low LOD) and D_800CFE48 (high LOD) arrays
 */
/**
 * Bone position entry - 20 bytes (0x14)
 * Named marker position within a coordinate frame defined by matrix chain.
 * Multiple entries can share the same chain with different translations.
 */
typedef struct BonePosEntry {
    /* 0x00 */ s16* matrix_chain;    // Bone matrix indices to multiply, -1 terminated
    /* 0x04 */ f32 trans_x;          // Translation X within coordinate frame
    /* 0x08 */ f32 trans_y;          // Translation Y within coordinate frame
    /* 0x0C */ f32 trans_z;          // Translation Z within coordinate frame
    /* 0x10 */ s16 bone_id;          // Bone identifier (lookup key)
    /* 0x12 */ u8 pad12[0x02];
} BonePosEntry; // size = 0x14

typedef struct AnimData {
    /* 0x00 */ u32 rom_start;
    /* 0x04 */ u32 rom_end;
    /* 0x08 */ void* display_list;
    /* 0x0C */ u8 pad0C[0x08];
    /* 0x14 */ void* bone_anim_header;        // BoneEntry array (frame bounds per bone)
    /* 0x18 */ BonePosEntry* bone_pos_array;  // BonePosEntry array (parent chains + rest pose)
    /* 0x1C */ void* bone_list;               // BoneAnimEntry array (keyframes)
    /* 0x20 */ u8 pad20[0x04];
    /* 0x24 */ TextureEntry** texture_array;  // NULL-terminated array of texture entry ptrs
    /* 0x28 */ TileDescriptor* tile_array;    // Array of tile descriptors
    /* 0x2C */ f32* scale_x_ptr;
    /* 0x30 */ f32* scale_y_ptr;
    /* 0x34 */ f32* scale_z_ptr;
    /* 0x38 */ f32* param4_ptr;
    /* 0x3C */ f32* param5_ptr;
    /* 0x40 */ f32* param6_ptr;
    /* 0x44 */ u16 flags;
    /* 0x46 */ u8 pad46[0x02];
    /* 0x48 */ u32 extended_rom_start;
    /* 0x4C */ u32 extended_rom_end;
    /* 0x50 */ u32 vertex_start_offset;
    /* 0x54 */ u32 vertex_end_offset;
} AnimData; // size = 0x58

/**
 * Bone hierarchy entry - 12 bytes (0xC)
 */
typedef struct BoneEntry {
    /* 0x00 */ void* data;
    /* 0x04 */ s16 start_frame;
    /* 0x06 */ s16 frame_count;
    /* 0x08 */ u8 pad08[0x04];
} BoneEntry; // size = 0xC

/**
 * Quaternion stored as fixed-point s16 values (scaled by 65536)
 */
typedef struct Quat16 {
    s16 x, y, z, w;
} Quat16; // size = 0x8

/**
 * Position keyframe - 12 bytes (3 floats)
 */
typedef struct PosKeyframe {
    f32 x, y, z;
} PosKeyframe; // size = 0xC

/**
 * Scale keyframe - 12 bytes (3 floats)
 */
typedef struct ScaleKeyframe {
    f32 x, y, z;
} ScaleKeyframe; // size = 0xC

/**
 * Bone animation data entry - 52 bytes (0x34)
 * Contains keyframe data and interpolation cache for one bone
 */
typedef struct BoneAnimEntry {
    /* 0x00 */ s16 bone_id;                    // -1 = end of list sentinel
    /* 0x02 */ s16 num_rot_keyframes;          // Number of rotation keyframes
    /* 0x04 */ s16 num_scale_keyframes;        // Number of scale keyframes
    /* 0x06 */ u8 pad06[0x02];
    /* 0x08 */ s16* pos_keyframe_times;        // Array of position keyframe times
    /* 0x0C */ s16* rot_keyframe_times;        // Array of rotation keyframe times
    /* 0x10 */ s16* scale_keyframe_times;      // Array of scale keyframe times
    /* 0x14 */ PosKeyframe* pos_data;          // Position keyframe data (x,y,z floats)
    /* 0x18 */ Quat16* rot_data;               // Rotation keyframe data (quaternions)
    /* 0x1C */ ScaleKeyframe* scale_data;      // Scale keyframe data (x,y,z floats)
    /* 0x20 */ s16 cache_pos_start;            // Cached position keyframe start time
    /* 0x22 */ s16 cache_pos_end;              // Cached position keyframe end time
    /* 0x24 */ s16 cache_pos_index;            // Cached position keyframe index
    /* 0x26 */ s16 cache_rot_start;            // Cached rotation keyframe start time
    /* 0x28 */ s16 cache_rot_end;              // Cached rotation keyframe end time
    /* 0x2A */ s16 cache_rot_index;            // Cached rotation keyframe index
    /* 0x2C */ s16 cache_scale_start;          // Cached scale keyframe start time
    /* 0x2E */ s16 cache_scale_end;            // Cached scale keyframe end time
    /* 0x30 */ s16 cache_scale_index;          // Cached scale keyframe index
    /* 0x32 */ u8 pad32[0x02];
} BoneAnimEntry; // size = 0x34

/**
 * Bone list entry - 52 bytes (0x34)
 * Alias for backwards compatibility
 */
typedef BoneAnimEntry BoneListEntry;

/**
 * Character animation state - 396 bytes (0x18C)
 * Array at D_801F43F0
 */
typedef struct CharacterAnimState {
    /* 0x00 */ AnimData* anim_data;
    /* 0x04 */ void* vertex_buffer;
    /* 0x08 */ s32 current_anim;
    /* 0x0C */ s32 bone_index;
    /* 0x10 */ f32 current_frame;
    /* 0x14 */ s32 start_frame;
    /* 0x18 */ s32 end_frame;
    /* 0x1C */ s32 bone_count;
    /* 0x20 */ void* vtx_buffer_0;
    /* 0x24 */ void* vtx_buffer_1;
    /* 0x28 */ void* vtx_buffer_2;
    /* 0x2C */ void* vtx_ptr;
    /* 0x30 */ s32 buffer_index;
    /* 0x34 */ u8 pad34[0x28];
    /* 0x5C */ f32 playback_speed;
    /* 0x60 */ u8 flags_60;
    /* 0x61 */ u8 anim_speed;
    /* 0x62 */ u8 pad62[0x02];
    /* 0x64 */ void* display_list;
    /* 0x68 */ f32 scale_x;
    /* 0x6C */ f32 scale_y;
    /* 0x70 */ f32 scale_z;
    /* 0x74 */ f32 param_4;
    /* 0x78 */ f32 param_5;
    /* 0x7C */ f32 param_6;
    /* 0x80 */ u16 flags;
    /* 0x82 */ u8 flags_82;
    /* 0x83 */ u8 is_playing;
    /* 0x84 */ s8 prev_anim;
    /* 0x85 */ u8 is_enabled;
    /* 0x86 */ u8 blend_frames;
    /* 0x87 */ u8 pad87[0x101];
    /* 0x188 */ u8 extra_flags_0;
    /* 0x189 */ u8 extra_flags_1;
    /* 0x18A */ u8 pad18A[0x02];
} CharacterAnimState; // size = 0x18C

// External data
extern CharacterAnimState D_801F43F0[];
extern AnimData D_800CCCC8[];  // Low LOD animation bank
extern AnimData D_800CFE48[];  // High LOD animation bank

// Global state
extern s32 g_current_anim_index;    // D_800C1DEC
extern void* g_vertex_buffer;       // D_800C1DF4
extern s32 g_current_bone;          // D_800C1DF0
extern void* g_bone_matrix_buffer;  // D_80105210
extern f32 g_anim_frame;            // D_800FBDF8
extern s32 g_start_frame;           // D_80104B80
extern s32 g_end_frame;             // D_800FE430
extern void* D_800C1DF4;            // Decompression buffer

// External functions
extern s32 func_800542A0(s32 character_id);
extern void func_800AAE80(const char* fmt, ...);
extern void lz_decompress_dma(u32 rom_addr, u32 size);
extern void lz_compress_extended_dma(u32 rom_addr, void* dest, u32 size);
extern void* func_800A7720(void* ptr);
extern void func_8005342C(s32 a0, s32 a1, s32 a2, s32 a3);
extern void func_800AADC4(void* dest, void* src, s32 size);
extern s32 func_800299F4(s32 debug_flag);
extern void func_80055788(s32 character_id);
extern CharacterAnimState* get_character_anim_state(s32 character_id);  // func_80054354

// Matrix functions
extern void guTranslateF(f32* out, f32 x, f32 y, f32 z);
extern void guScaleF(f32* out, f32 x, f32 y, f32 z);
extern void guMtxCatF(f32* a, f32* b, f32* out);
extern void guMtxF2L(f32* mtxF, void* mtx);
extern void guMtxIdent(Mtx* m);
extern void guTranslate(Mtx* m, f32 x, f32 y, f32 z);
extern void guMtxCatL(Mtx* a, Mtx* b, Mtx* out);
extern void guMtxXFML(Mtx* m, f32 x, f32 y, f32 z, f32* ox, f32* oy, f32* oz);
extern u32 osVirtualToPhysical(void* addr);
extern f32 acosf_approx(f32 x);                                          // func_80059BC0
extern f32 sinf(f32 angle);

// Constants
extern f64 D_800D0478;          // 1.0 double constant
extern f64 SLERP_EPSILON;       // D_800D0480 - threshold for nlerp vs slerp

// Forward declarations
void update_vertex_texture_coords(s32 character_id);
void calculate_bone_matrices(s32 character_id);
s32 get_bone_world_position(s32 character_id, s16 bone_id, f32* out_pos);

// Debug strings
extern const char D_800D070C[];  // "model already loaded"
extern const char D_800D076C[];  // bone info format
extern const char D_800D0788[];  // frame info format
extern const char D_800D07B0[];  // "loaded model %d"

#define MAX_VERTEX_BUFFER_SIZE 0x780  // 1920 bytes (120 vertices * 16)

/**
 * Load and initialize a character model with animations
 *
 * @param character_id Character slot index
 * @param model_variant Model/animation variant index
 * @param lod_level LOD level (0-3 = low detail, >=4 = high detail)
 */
void load_character_model(s32 character_id, s32 model_variant, s32 lod_level) {
    CharacterAnimState* anim_state;
    AnimData* anim_data;
    s32 anim_index;
    s32 needs_extended_decompress;
    s32 vtx_size;
    void* heap_ptr;
    BoneEntry* bone_hierarchy;
    BoneListEntry* bone_list;
    s32 bone_count;

    needs_extended_decompress = 0;

    // Get animation state for this character (396 bytes per character)
    anim_state = &D_801F43F0[character_id];

    if (lod_level < 4) {
        // Low detail LOD - use D_800CCCC8 bank
        // Animation index = (lod_level & 3) * 36 + (model_variant % 72)
        anim_index = ((lod_level & 3) * 36) + (model_variant % 72);
        anim_data = &D_800CCCC8[anim_index];
        g_current_anim_index = anim_index;
    } else {
        // High detail LOD - use D_800CFE48 bank
        // Animation index = model_variant % 18
        anim_index = model_variant % 18;
        anim_data = &D_800CFE48[anim_index];
        g_current_anim_index = anim_index;
    }

    anim_state->anim_data = anim_data;

    // Check if model is already loaded
    if (func_800542A0(character_id) != 0) {
        // Model already loaded - print debug messages
        func_800AAE80(D_800D070C, character_id);
        func_800AAE80(D_800D070C, character_id);
        func_800AAE80(D_800D070C, character_id);
        func_800AAE80(D_800D070C, character_id);
        func_800AAE80(D_800D07B0, character_id);
        return;
    }

    // Check if vertex buffer is allocated
    if (anim_state->vertex_buffer == NULL) {
        return;
    }

    // Check if model uses extended compression
    anim_data = anim_state->anim_data;
    if (anim_data->rom_start != anim_data->extended_rom_start) {
        needs_extended_decompress = 1;
    }

    // Initialize animation state fields
    anim_state->buffer_index = 0;
    anim_state->bone_index = 0;
    anim_state->flags_60 = 0;
    anim_state->anim_speed = 0x60;  // 96
    anim_state->flags_82 = 0;
    anim_state->is_playing = 1;
    anim_state->prev_anim = -1;
    anim_state->is_enabled = 1;
    anim_state->blend_frames = 27;
    anim_state->extra_flags_0 = 0;
    anim_state->extra_flags_1 = 0;
    anim_state->current_anim = g_current_anim_index;
    g_vertex_buffer = anim_state->vertex_buffer;

    // Decompress model data from ROM
    if (needs_extended_decompress) {
        // Extended compression - decompress auxiliary data first
        anim_data = anim_state->anim_data;
        lz_decompress_dma(anim_data->extended_rom_start,
                          anim_data->extended_rom_end - anim_data->extended_rom_start);

        anim_data = anim_state->anim_data;
        lz_compress_extended_dma(anim_data->rom_start, D_800C1DF4,
                                 anim_data->rom_end - anim_data->rom_start);
    } else {
        // Standard decompression
        anim_data = anim_state->anim_data;
        lz_decompress_dma(anim_data->rom_start,
                          anim_data->rom_end - anim_data->rom_start);
    }

    // Relocate pointers in decompressed data
    heap_ptr = func_800A7720(g_vertex_buffer);

    // Initialize bone matrix buffer
    func_8005342C(0, 0, 0x20, 0x2000000);

    // Calculate vertex buffer size (clamped to max)
    anim_data = anim_state->anim_data;
    vtx_size = anim_data->vertex_end_offset - anim_data->vertex_start_offset;
    if (vtx_size < MAX_VERTEX_BUFFER_SIZE) {
        vtx_size = MAX_VERTEX_BUFFER_SIZE;
    }

    // Allocate triple-buffered vertex memory
    heap_ptr = func_800A7720(NULL);
    heap_ptr = (void*)((u32)heap_ptr - 0x80000000);  // Convert to KSEG0

    anim_state->vtx_buffer_0 = heap_ptr;
    anim_state->vtx_buffer_1 = (u8*)heap_ptr + vtx_size;
    anim_state->vtx_buffer_2 = (u8*)heap_ptr + vtx_size * 2;

    // Copy initial vertex data to buffers
    func_800AADC4(anim_state->vtx_buffer_1, anim_state->vtx_buffer_0, vtx_size);
    func_800AADC4(anim_state->vtx_buffer_2, anim_state->vtx_buffer_0, vtx_size);

    // Set current vertex buffer
    g_current_bone = anim_state->bone_index;
    g_bone_matrix_buffer = anim_state->vtx_buffer_0;  // vtx_buffers[buffer_index]

    // Debug output
    if (func_800299F4(0x41)) {
        anim_data = anim_state->anim_data;
        func_800AAE80(D_800D076C, anim_data->bone_anim_header);
    }

    // Read animation frame bounds from bone entry
    anim_data = anim_state->anim_data;
    bone_hierarchy = (BoneEntry*)anim_data->bone_anim_header;
    anim_state->start_frame = bone_hierarchy[g_current_bone].start_frame;
    anim_state->current_frame = (f32)anim_state->start_frame;
    anim_state->end_frame = anim_state->start_frame + bone_hierarchy[g_current_bone].frame_count;

    // Store frame info to globals
    g_anim_frame = anim_state->current_frame;
    g_start_frame = anim_state->start_frame;
    g_end_frame = anim_state->end_frame;

    if (func_800299F4(0x41)) {
        func_800AAE80(D_800D0788, g_current_bone, g_start_frame, g_end_frame);
    }

    // Initialize vertex buffers for triple buffering
    update_vertex_texture_coords(character_id);
    anim_state->buffer_index = 1;
    update_vertex_texture_coords(character_id);
    anim_state->buffer_index = 2;
    update_vertex_texture_coords(character_id);
    anim_state->buffer_index = 0;

    // Reset animation state
    func_80055788(character_id);

    // Calculate initial bone matrices
    calculate_bone_matrices(character_id);

    // Initialize bone keyframe cache to -1
    anim_data = anim_state->anim_data;
    bone_list = (BoneListEntry*)anim_data->bone_list;
    if (bone_list->bone_id != -1) {
        do {
            bone_list->cache_pos_start = -1;
            bone_list->cache_pos_end = -1;
            bone_list->cache_rot_start = -1;
            bone_list->cache_rot_end = -1;
            bone_list->cache_scale_start = -1;
            bone_list->cache_scale_end = -1;
            bone_list++;
        } while (bone_list->bone_id != -1);
    }

    // Copy render parameters from anim_data to anim_state
    anim_state->vtx_ptr = g_bone_matrix_buffer;
    anim_data = anim_state->anim_data;
    anim_state->display_list = anim_data->display_list;
    anim_state->scale_x = *anim_data->scale_x_ptr;
    anim_state->scale_y = *anim_data->scale_y_ptr;
    anim_state->scale_z = *anim_data->scale_z_ptr;
    anim_state->param_4 = *anim_data->param4_ptr;
    anim_state->param_5 = *anim_data->param5_ptr;
    anim_state->param_6 = *anim_data->param6_ptr;
    anim_state->flags = anim_data->flags;

    // Count bones in hierarchy
    bone_hierarchy = (BoneEntry*)anim_data->bone_anim_header;
    bone_count = 0;
    while (bone_hierarchy->data != NULL) {
        bone_count++;
        bone_hierarchy++;
    }
    anim_state->bone_count = bone_count;
    anim_state->playback_speed = 1.0f;

    if (func_800299F4(0x41)) {
        func_800AAE80(D_800D07B0, g_current_anim_index);
    }
}

#define ADDRESS_MASK    0x00FFFFFF
#define G_TX_LOADTILE   7

/**
 * Update vertex texture coordinates for a character model
 *
 * Iterates through texture entries and:
 * 1. Generates RDP display list commands to load/configure textures
 * 2. Copies UV coordinates from texture entries into vertex buffer
 *
 * @param character_id Character slot index (0-3)
 */
void update_vertex_texture_coords(s32 character_id) {
    CharacterAnimState* anim_state;
    AnimData* anim_data;
    TextureEntry* tex_entry;
    TileDescriptor* tile_desc;
    Gfx* gfx;
    void* vtx_buffer;
    Vtx_t* vertex;
    s32 texture_index;
    s32 i;
    s32 width, height;
    s32 line;
    s32 lrs;
    s32 dxt;

    texture_index = 0;

    // Get animation state for this character
    anim_state = get_character_anim_state(character_id);
    if (anim_state == NULL) {
        return;
    }

    // Relocate vertex buffer pointer
    func_800A7720(anim_state->vertex_buffer);

    // Initialize bone matrix buffer
    func_8005342C(0, 0, 0x20, 0x2000000);

    // Get texture entry array from anim data
    anim_data = anim_state->anim_data;
    tex_entry = anim_data->texture_array[0];
    if (tex_entry == NULL) {
        return;
    }

    // Process each texture entry
    do {
        // Get tile descriptor (offset by tile_index if >= 0)
        anim_data = anim_state->anim_data;
        tile_desc = anim_data->tile_array;
        if (tex_entry->tile_index >= 0) {
            tile_desc = &tile_desc[tex_entry->tile_index];
        }

        // Calculate display list address in current vertex buffer
        vtx_buffer = (&anim_state->vtx_buffer_0)[anim_state->buffer_index];
        gfx = (Gfx*)(((tex_entry->texture_addr + 0x10) & ADDRESS_MASK) + (u32)vtx_buffer);

        width = tile_desc->width;
        height = tile_desc->height;

        // 0xFD100000 XXXXXXXX - G_SETTIMG (fmt=RGBA, siz=16b, addr=tile_desc->texture_dram_addr & ~7)
        gDPSetTextureImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1,
                           tile_desc->texture_dram_addr & ~0x7);

        // 0xF5100000 07098260 - G_SETTILE for loading (tile 7, tmem=0x260, palette=9)
        gDPSetTile(gfx++,G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0x0000, G_TX_LOADTILE, 0, G_TX_NOMIRROR | G_TX_CLAMP, 6, G_TX_NOLOD, G_TX_NOMIRROR | G_TX_CLAMP, 6, G_TX_NOLOD);

        // 0xE6000000 00000000 - G_RDPLOADSYNC
        gDPLoadSync(gfx++);

        // Calculate lrs (number of texels - 1), clamped to 0x7FF
        lrs = (width * height) - 1;
        if (lrs >= 0x800) {
            lrs = 0x7FF;
        }

        // Calculate dxt
        if ((width / 4) > 0) {
            dxt = 0x800 / (width / 4);
        } else {
            dxt = 0x800;
        }

        // 0xF3000000 07LLLDDD - G_LOADBLOCK (tile 7, lrs=LLL, dxt=DDD)
        gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, lrs, dxt);

        // 0xE7000000 00000000 - G_RDPPIPESYNC
        gDPPipeSync(gfx++);

        // 0xF51LLL00 00098260 - G_SETTILE for rendering (tile 0, line=LLL, tmem=0x260, palette=9)
        line = ((width * 2) + 7) / 8;
        gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, line, 0,
                   0, 0, 0, 0, 0, 0, 0, 0);

        // 0xF2000000 00WWWHH0 - G_SETTILESIZE (tile 0, lrs=(w-1)<<2, lrt=(h-1)<<2)
        gDPSetTileSize(gfx++, 0, 0, 0,
                       (width - 1) << 2, (height - 1) << 2);

        // Copy UV coordinates to vertices (3 per triangle)
        for (i = 0; i < 3; i++) {
            vtx_buffer = (&anim_state->vtx_buffer_0)[anim_state->buffer_index];
            vertex = (Vtx_t*)(((tex_entry->vertex_base_addr + (i * 16)) & ADDRESS_MASK) + (u32)vtx_buffer);
            vertex->s = tex_entry->u[i];
            vertex->t = tex_entry->v[i];
        }

        // Move to next texture entry
        texture_index++;
        tex_entry = anim_state->anim_data->texture_array[texture_index];
    } while (tex_entry != NULL);
}

#define FIXED_TO_FLOAT (1.0f / 65536.0f)

/**
 * Calculate transformation matrices for all bones in a character's skeleton.
 *
 * For each bone, interpolates position, rotation (quaternion SLERP), and scale
 * keyframes based on current animation frame. Builds local transform matrix
 * and concatenates with parent bone's matrix.
 *
 * @param character_id Character slot index
 */
void calculate_bone_matrices(s32 character_id) {
    CharacterAnimState* state;
    AnimData* anim_data;
    BoneAnimEntry* bone;
    BoneAnimEntry* bone_base;
    void* out_mtx;
    s32 needs_interp;
    s32 keyframe_idx;
    s32 next_idx;
    s16 num_keyframes;
    s16* times;
    f32 current_frame;
    f32 lerp_t;
    f32 pos_x, pos_y, pos_z;
    f32 scale_x, scale_y, scale_z;
    f32 quat[4];  // x, y, z, w
    f32 translation_mtx[16];
    f32 rotation_mtx[16];
    f32 scale_mtx[16];
    f32 dot, omega, sin_omega, weight1, weight2;
    f32 one_minus_t;
    PosKeyframe* pos_curr;
    PosKeyframe* pos_next;
    ScaleKeyframe* scale_curr;
    ScaleKeyframe* scale_next;
    Quat16* rot_curr;
    Quat16* rot_next;

    state = get_character_anim_state(character_id);
    anim_data = state->anim_data;
    bone_base = (BoneAnimEntry*)anim_data->bone_list;

    // Check for empty bone list
    if (bone_base->bone_id == -1) {
        return;
    }

    out_mtx = g_bone_matrix_buffer;
    bone = bone_base;

    // Process each bone
    do {
        current_frame = g_anim_frame;

        // ====================================================================
        // POSITION INTERPOLATION
        // ====================================================================
        needs_interp = 1;

        // Check if frame is within cached keyframe range
        if ((f32)bone->cache_pos_end < current_frame ||
            current_frame < (f32)bone->cache_pos_start) {

            // Search for correct keyframe
            times = bone->pos_keyframe_times;
            num_keyframes = times[0];

            if (current_frame < (f32)num_keyframes) {
                // Before first keyframe - use keyframe 0 directly
                pos_curr = &bone->pos_data[0];
                pos_x = pos_curr->x;
                pos_y = pos_curr->y;
                pos_z = pos_curr->z;
                needs_interp = 0;
            } else {
                // Search for keyframe
                keyframe_idx = 1;
                while (keyframe_idx < bone->bone_id) {
                    if (current_frame < (f32)times[keyframe_idx]) {
                        // Found it - update cache
                        bone->cache_pos_index = keyframe_idx - 1;
                        bone->cache_pos_start = times[keyframe_idx - 1];
                        bone->cache_pos_end = times[keyframe_idx];
                        goto pos_interpolate;
                    }
                    keyframe_idx++;
                }

                // At or past last keyframe
                if (keyframe_idx == bone_base->bone_id) {
                    keyframe_idx--;
                    pos_curr = &bone->pos_data[keyframe_idx];
                    pos_x = pos_curr->x;
                    pos_y = pos_curr->y;
                    pos_z = pos_curr->z;
                    needs_interp = 0;
                }
            }
        }

pos_interpolate:
        if (needs_interp) {
            // Check if past animation end
            if (g_end_frame < bone->cache_pos_end) {
                keyframe_idx = bone->cache_pos_index;
                pos_curr = &bone->pos_data[keyframe_idx];
                pos_x = pos_curr->x;
                pos_y = pos_curr->y;
                pos_z = pos_curr->z;
            } else {
                // Interpolate between keyframes
                lerp_t = (current_frame - (f32)bone->cache_pos_start) /
                         (f32)(bone->cache_pos_end - bone->cache_pos_start);
                one_minus_t = 1.0 - (f64)lerp_t;

                keyframe_idx = bone->cache_pos_index;
                next_idx = keyframe_idx + 1;
                pos_curr = &bone->pos_data[keyframe_idx];
                pos_next = &bone->pos_data[next_idx];

                pos_x = one_minus_t * pos_curr->x + lerp_t * pos_next->x;
                pos_y = one_minus_t * pos_curr->y + lerp_t * pos_next->y;
                pos_z = one_minus_t * pos_curr->z + lerp_t * pos_next->z;
            }
        }

        // Build translation matrix
        guTranslateF(translation_mtx, pos_x, pos_y, pos_z);

        // ====================================================================
        // ROTATION INTERPOLATION (Quaternion SLERP)
        // ====================================================================
        needs_interp = 1;

        if ((f32)bone->cache_rot_end < current_frame ||
            current_frame < (f32)bone->cache_rot_start) {

            times = bone->rot_keyframe_times;
            num_keyframes = times[0];

            if (current_frame < (f32)num_keyframes) {
                // Use first keyframe directly
                rot_curr = &bone->rot_data[0];
                quat[0] = (f32)rot_curr->x * FIXED_TO_FLOAT;
                quat[1] = (f32)rot_curr->y * FIXED_TO_FLOAT;
                quat[2] = (f32)rot_curr->z * FIXED_TO_FLOAT;
                quat[3] = (f32)rot_curr->w * FIXED_TO_FLOAT;
                needs_interp = 0;
            } else {
                // Search for keyframe
                keyframe_idx = 1;
                while (keyframe_idx < bone->num_rot_keyframes) {
                    if (current_frame < (f32)times[keyframe_idx]) {
                        bone->cache_rot_index = keyframe_idx - 1;
                        bone->cache_rot_start = times[keyframe_idx - 1];
                        bone->cache_rot_end = times[keyframe_idx];
                        goto rot_interpolate;
                    }
                    keyframe_idx++;
                }

                // At last keyframe
                if (keyframe_idx == bone->num_rot_keyframes) {
                    keyframe_idx--;
                    rot_curr = &bone->rot_data[keyframe_idx];
                    quat[0] = (f32)rot_curr->x * FIXED_TO_FLOAT;
                    quat[1] = (f32)rot_curr->y * FIXED_TO_FLOAT;
                    quat[2] = (f32)rot_curr->z * FIXED_TO_FLOAT;
                    quat[3] = (f32)rot_curr->w * FIXED_TO_FLOAT;
                    needs_interp = 0;
                }
            }
        }

rot_interpolate:
        if (needs_interp) {
            if (g_end_frame < bone->cache_rot_end) {
                // Past end - use cached keyframe
                keyframe_idx = bone->cache_rot_index;
                rot_curr = &bone->rot_data[keyframe_idx];
                quat[0] = (f32)rot_curr->x * FIXED_TO_FLOAT;
                quat[1] = (f32)rot_curr->y * FIXED_TO_FLOAT;
                quat[2] = (f32)rot_curr->z * FIXED_TO_FLOAT;
                quat[3] = (f32)rot_curr->w * FIXED_TO_FLOAT;
            } else {
                // SLERP between quaternions
                keyframe_idx = bone->cache_rot_index;
                next_idx = keyframe_idx + 1;
                rot_curr = &bone->rot_data[keyframe_idx];
                rot_next = &bone->rot_data[next_idx];

                // Calculate dot product
                dot = ((f32)rot_curr->x * (f32)rot_next->x +
                       (f32)rot_curr->y * (f32)rot_next->y +
                       (f32)rot_curr->z * (f32)rot_next->z +
                       (f32)rot_curr->w * (f32)rot_next->w) * (FIXED_TO_FLOAT * FIXED_TO_FLOAT);

                lerp_t = (current_frame - (f32)bone->cache_rot_start) /
                         (f32)(bone->cache_rot_end - bone->cache_rot_start);

                // Check if quaternions are nearly parallel
                if ((1.0 - (f64)dot) < SLERP_EPSILON) {
                    // Linear interpolation (nlerp)
                    weight1 = 1.0 - (f64)lerp_t;
                    weight2 = lerp_t;
                } else {
                    // Full SLERP
                    omega = acosf_approx(dot);
                    sin_omega = sinf(omega);
                    weight1 = sinf((1.0f - lerp_t) * omega) / sin_omega;
                    weight2 = sinf(lerp_t * omega) / sin_omega;
                }

                // Apply weights
                quat[0] = weight1 * (f32)rot_curr->x * FIXED_TO_FLOAT +
                          weight2 * (f32)rot_next->x * FIXED_TO_FLOAT;
                quat[1] = weight1 * (f32)rot_curr->y * FIXED_TO_FLOAT +
                          weight2 * (f32)rot_next->y * FIXED_TO_FLOAT;
                quat[2] = weight1 * (f32)rot_curr->z * FIXED_TO_FLOAT +
                          weight2 * (f32)rot_next->z * FIXED_TO_FLOAT;
                quat[3] = weight1 * (f32)rot_curr->w * FIXED_TO_FLOAT +
                          weight2 * (f32)rot_next->w * FIXED_TO_FLOAT;
            }
        }

        // Convert quaternion to rotation matrix
        {
            f32 qx = quat[0], qy = quat[1], qz = quat[2], qw = quat[3];
            f32 mag_sq = qx*qx + qy*qy + qz*qz + qw*qw;
            f32 s = (mag_sq > 0.0f) ? (2.0f / mag_sq) : 0.0f;
            f32 xs = qx * s, ys = qy * s, zs = qz * s;
            f32 wx = qw * xs, wy = qw * ys, wz = qw * zs;
            f32 xx = qx * xs, xy = qx * ys, xz = qx * zs;
            f32 yy = qy * ys, yz = qy * zs, zz = qz * zs;

            rotation_mtx[0]  = 1.0f - (yy + zz);  // M[0][0]
            rotation_mtx[1]  = xy + wz;            // M[1][0]
            rotation_mtx[2]  = xz - wy;            // M[2][0]
            rotation_mtx[3]  = 0.0f;               // M[3][0]
            rotation_mtx[4]  = xy - wz;            // M[0][1]
            rotation_mtx[5]  = 1.0f - (xx + zz);  // M[1][1]
            rotation_mtx[6]  = yz + wx;            // M[2][1]
            rotation_mtx[7]  = 0.0f;               // M[3][1]
            rotation_mtx[8]  = xz + wy;            // M[0][2]
            rotation_mtx[9]  = yz - wx;            // M[1][2]
            rotation_mtx[10] = 1.0f - (xx + yy);  // M[2][2]
            rotation_mtx[11] = 0.0f;               // M[3][2]
            rotation_mtx[12] = 0.0f;               // M[0][3]
            rotation_mtx[13] = 0.0f;               // M[1][3]
            rotation_mtx[14] = 0.0f;               // M[2][3]
            rotation_mtx[15] = 1.0f;               // M[3][3]
        }

        // ====================================================================
        // SCALE INTERPOLATION
        // ====================================================================
        needs_interp = 1;

        if ((f32)bone->cache_scale_end < current_frame ||
            current_frame < (f32)bone->cache_scale_start) {

            times = bone->scale_keyframe_times;
            num_keyframes = times[0];

            if (current_frame < (f32)num_keyframes) {
                scale_curr = &bone->scale_data[0];
                scale_x = scale_curr->x;
                scale_y = scale_curr->y;
                scale_z = scale_curr->z;
                needs_interp = 0;
            } else {
                keyframe_idx = 1;
                while (keyframe_idx < bone->num_scale_keyframes) {
                    if (current_frame < (f32)times[keyframe_idx]) {
                        bone->cache_scale_index = keyframe_idx - 1;
                        bone->cache_scale_start = times[keyframe_idx - 1];
                        bone->cache_scale_end = times[keyframe_idx];
                        goto scale_interpolate;
                    }
                    keyframe_idx++;
                }

                if (keyframe_idx == bone->num_scale_keyframes) {
                    keyframe_idx--;
                    scale_curr = &bone->scale_data[keyframe_idx];
                    scale_x = scale_curr->x;
                    scale_y = scale_curr->y;
                    scale_z = scale_curr->z;
                    needs_interp = 0;
                }
            }
        }

scale_interpolate:
        if (needs_interp) {
            if (g_end_frame < bone->cache_scale_end) {
                keyframe_idx = bone->cache_scale_index;
                scale_curr = &bone->scale_data[keyframe_idx];
                scale_x = scale_curr->x;
                scale_y = scale_curr->y;
                scale_z = scale_curr->z;
            } else {
                lerp_t = (current_frame - (f32)bone->cache_scale_start) /
                         (f32)(bone->cache_scale_end - bone->cache_scale_start);
                one_minus_t = 1.0 - (f64)lerp_t;

                keyframe_idx = bone->cache_scale_index;
                next_idx = keyframe_idx + 1;
                scale_curr = &bone->scale_data[keyframe_idx];
                scale_next = &bone->scale_data[next_idx];

                scale_x = one_minus_t * scale_curr->x + lerp_t * scale_next->x;
                scale_y = one_minus_t * scale_curr->y + lerp_t * scale_next->y;
                scale_z = one_minus_t * scale_curr->z + lerp_t * scale_next->z;
            }
        }

        // Build scale matrix
        guScaleF(scale_mtx, scale_x, scale_y, scale_z);

        // ====================================================================
        // COMBINE MATRICES: out = parent * translation * rotation * scale
        // ====================================================================
        // rotation = scale * rotation
        guMtxCatF(scale_mtx, rotation_mtx, rotation_mtx);

        // rotation = rotation * translation
        guMtxCatF(rotation_mtx, translation_mtx, rotation_mtx);

        // Convert float matrix to fixed-point and output
        guMtxF2L(rotation_mtx, out_mtx);

        // Advance to next bone
        bone++;
        out_mtx = (void*)((u8*)out_mtx + 0x40);  // sizeof(Mtx) = 64
    } while (bone->bone_id != -1);
}

#define BONE_TO_WORLD_SCALE 0.01f

/**
 * Get the world-space position of a specific bone in a character's skeleton.
 *
 * Searches the bone hierarchy for the specified bone, accumulates parent
 * transforms, applies the bone's local translation, then transforms the
 * origin through the matrix to get the final world position.
 *
 * @param character_id Character slot index
 * @param bone_id Target bone ID to find in hierarchy
 * @param out_pos Pointer to Vec3f to receive world position
 * @return 0 on success, 1 if bone not found, -1 if invalid character
 */
s32 get_bone_world_position(s32 character_id, s16 bone_id, f32* out_pos) {
    CharacterAnimState* anim_state;
    AnimData* anim_data;
    BonePosEntry* bone_hierarchy;
    BonePosEntry* bone_entry;
    s16* matrix_chain;
    Mtx accumulated_mtx;
    Mtx translation_mtx;
    s16 parent_idx;
    Mtx* bone_matrices;

    anim_state = get_character_anim_state(character_id);
    if (anim_state == NULL) {
        out_pos[0] = 0.0f;
        out_pos[1] = 0.0f;
        out_pos[2] = 0.0f;
        return -1;
    }

    // Convert vertex buffer to physical address for DMA
    osVirtualToPhysical(anim_state->vertex_buffer);

    // Initialize bone matrix buffer
    func_8005342C(0, 0, 0x20, 0x2000000);

    // Get bone position array from AnimData (offset 0x18)
    anim_data = anim_state->anim_data;
    bone_hierarchy = anim_data->bone_pos_array;

    // Check for empty hierarchy
    if (bone_hierarchy->matrix_chain == NULL) {
        out_pos[0] = 0.0f;
        out_pos[1] = 0.0f;
        out_pos[2] = 0.0f;
        return 1;
    }

    // Search for matching bone_id
    bone_entry = bone_hierarchy;
    while (bone_entry->matrix_chain != NULL) {
        if (bone_entry->bone_id == bone_id) {
            break;
        }
        bone_entry++;
    }

    // Check if bone was found
    if (bone_entry->matrix_chain == NULL) {
        out_pos[0] = 0.0f;
        out_pos[1] = 0.0f;
        out_pos[2] = 0.0f;
        return 1;
    }

    // Initialize accumulated matrix to identity
    guMtxIdent(&accumulated_mtx);

    // Get parent chain and accumulate parent bone matrices
    matrix_chain = bone_entry->matrix_chain;
    parent_idx = matrix_chain[0];

    if (parent_idx != -1) {
        bone_matrices = (Mtx*)anim_state->vtx_ptr;

        // Walk parent chain, concatenating bone matrices
        while (parent_idx != -1) {
            guMtxCatL(&bone_matrices[parent_idx], &accumulated_mtx, &accumulated_mtx);
            matrix_chain++;
            parent_idx = matrix_chain[0];
        }
    }

    // Build translation matrix from bone's local position
    guMtxIdent(&translation_mtx);
    guTranslate(&translation_mtx, bone_entry->trans_x, bone_entry->trans_y, bone_entry->trans_z);

    // Concatenate: accumulated = translation * accumulated
    guMtxCatL(&translation_mtx, &accumulated_mtx, &accumulated_mtx);

    // Transform origin (0,0,0) through accumulated matrix to get world position
    guMtxXFML(&accumulated_mtx, 0.0f, 0.0f, 0.0f, &out_pos[0], &out_pos[1], &out_pos[2]);

    // Scale from internal units to world units
    out_pos[0] *= BONE_TO_WORLD_SCALE;
    out_pos[1] *= BONE_TO_WORLD_SCALE;
    out_pos[2] *= BONE_TO_WORLD_SCALE;

    return 0;
}
