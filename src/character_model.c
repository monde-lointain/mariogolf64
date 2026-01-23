#include "common.h"
#include "PR/gbi.h"

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
typedef struct AnimData {
    /* 0x00 */ u32 rom_start;
    /* 0x04 */ u32 rom_end;
    /* 0x08 */ void* display_list;
    /* 0x0C */ u8 pad0C[0x08];
    /* 0x14 */ void* bone_hierarchy;
    /* 0x18 */ u8 pad18[0x04];
    /* 0x1C */ void* bone_list;
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
 * Bone list entry - 52 bytes (0x34)
 */
typedef struct BoneListEntry {
    /* 0x00 */ s16 bone_id;
    /* 0x02 */ u8 pad02[0x1E];
    /* 0x20 */ s16 cache0;
    /* 0x22 */ s16 cache1;
    /* 0x24 */ u8 pad24[0x02];
    /* 0x26 */ s16 cache2;
    /* 0x28 */ s16 cache3;
    /* 0x2A */ u8 pad2A[0x02];
    /* 0x2C */ s16 cache4;
    /* 0x2E */ s16 cache5;
    /* 0x30 */ u8 pad30[0x04];
} BoneListEntry; // size = 0x34

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
extern s32 D_800C1DEC;   // g_current_anim_index
extern void* D_800C1DF4; // g_vertex_buffer
extern s32 D_800C1DF0;   // g_current_bone
extern void* D_80105210; // g_current_vtx_buffer
extern f32 D_800FBDF8;   // g_anim_frame
extern s32 D_80104B80;   // g_start_frame
extern s32 D_800FE430;   // g_end_frame

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
extern void func_80053604(s32 character_id);
extern CharacterAnimState* get_character_anim_state(s32 character_id);  // func_80054354

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
        D_800C1DEC = anim_index;
    } else {
        // High detail LOD - use D_800CFE48 bank
        // Animation index = model_variant % 18
        anim_index = model_variant % 18;
        anim_data = &D_800CFE48[anim_index];
        D_800C1DEC = anim_index;
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
    anim_state->current_anim = D_800C1DEC;
    D_800C1DF4 = anim_state->vertex_buffer;

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
    heap_ptr = func_800A7720(D_800C1DF4);

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
    D_800C1DF0 = anim_state->bone_index;
    D_80105210 = anim_state->vtx_buffer_0;  // vtx_buffers[buffer_index]

    // Debug output
    if (func_800299F4(0x41)) {
        anim_data = anim_state->anim_data;
        func_800AAE80(D_800D076C, anim_data->bone_hierarchy);
    }

    // Read animation frame bounds from bone entry
    anim_data = anim_state->anim_data;
    bone_hierarchy = (BoneEntry*)anim_data->bone_hierarchy;
    anim_state->start_frame = bone_hierarchy[D_800C1DF0].start_frame;
    anim_state->current_frame = (f32)anim_state->start_frame;
    anim_state->end_frame = anim_state->start_frame + bone_hierarchy[D_800C1DF0].frame_count;

    // Store frame info to globals
    D_800FBDF8 = anim_state->current_frame;
    D_80104B80 = anim_state->start_frame;
    D_800FE430 = anim_state->end_frame;

    if (func_800299F4(0x41)) {
        func_800AAE80(D_800D0788, D_800C1DF0, D_80104B80, D_800FE430);
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
    func_80053604(character_id);

    // Initialize bone attachment point cache to -1
    anim_data = anim_state->anim_data;
    bone_list = (BoneListEntry*)anim_data->bone_list;
    if (bone_list->bone_id != -1) {
        do {
            bone_list->cache0 = -1;
            bone_list->cache1 = -1;
            bone_list->cache2 = -1;
            bone_list->cache3 = -1;
            bone_list->cache4 = -1;
            bone_list->cache5 = -1;
            bone_list++;
        } while (bone_list->bone_id != -1);
    }

    // Copy render parameters from anim_data to anim_state
    anim_state->vtx_ptr = D_80105210;
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
    bone_hierarchy = (BoneEntry*)anim_data->bone_hierarchy;
    bone_count = 0;
    while (bone_hierarchy->data != NULL) {
        bone_count++;
        bone_hierarchy++;
    }
    anim_state->bone_count = bone_count;
    anim_state->playback_speed = 1.0f;

    if (func_800299F4(0x41)) {
        func_800AAE80(D_800D07B0, D_800C1DEC);
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
            vertex = (Vtx*)(((tex_entry->vertex_base_addr + (i * 16)) & ADDRESS_MASK) + (u32)vtx_buffer);
            vertex->s = tex_entry->u[i];
            vertex->t = tex_entry->v[i];
        }

        // Move to next texture entry
        texture_index++;
        tex_entry = anim_state->anim_data->texture_array[texture_index];
    } while (tex_entry != NULL);
}
