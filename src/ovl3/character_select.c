/**
 * @file character_select.c
 * @brief Character selection screen tile rendering for overlay 3
 *
 * Renders a 3x7 grid of character selection tiles with animated highlights.
 * Each tile can be either:
 *   - A normal character portrait (tile_id >= 0)
 *   - A highlight/selection effect (tile_id < 0)
 *
 * The system supports multiple players with separate texture coordinate tables
 * and includes animation state for the selected character highlight.
 */

#include "common.h"
#include <PR/ultratypes.h>
#include <PR/gbi.h>

//=============================================================================
// External function declarations
//=============================================================================
extern void* func_8005AF74(void);
extern void func_800504E8(s16 lookup_id, void* out_coords);
extern s16 func_80050598(void* coords);
extern void func_800505A0(void* texture_ptr, s16 format, void* coords);
extern void func_8005062C(u16 lookup_id, void* out_entry);
extern void func_800506D4(void* palette_ptr, void* entry);
extern s32 func_800299F4(s32 probability);
extern s16 func_ovl3_8020C2D8(s16 tile_id);

//=============================================================================
// External data declarations
//=============================================================================

// Character slot info array - 3 rows x 7 columns, 2 bytes per entry
// Row stride = 14 bytes
extern s16 D_800C28E4[];        // g_character_slot_info

// Game state
extern s32 D_801EFFA0;          // g_current_game_mode (0x10 = special mode)
extern s8 D_801B71ED;           // g_selected_character_limit
extern s32 D_801B7070;          // g_player_context_offset

// Texture/palette lookup tables (indexed by tile_id * 4)
extern void* D_8012D430[];      // g_tile_palette_table
extern void* D_80105B78[];      // g_tile_texture_table

// Highlight texture/palette tables (for negative tile IDs)
extern void* D_8012D4F8[];      // g_highlight_palette_table (D_8012D478 + 0x80)
extern void* D_80105C40[];      // g_highlight_texture_table

// Per-player texture coordinate tables (4 bytes per entry, 4 players)
extern s16 D_800C2590[];        // g_texture_coords_p0
extern s16 D_800C2592[];        // g_texture_coords_p1
extern s16 D_800C2594[];        // g_texture_coords_p2
extern s16 D_800C2596[];        // g_texture_coords_p3

// Palette animation lookup tables
extern u16 D_800C2650[];        // g_palette_anim_table_a (standard)
extern u16 D_800C2780[];        // g_palette_anim_table_b (alternate, 1% chance)

// Character unlock mapping table
extern u16 D_800FE3D8[];        // g_character_unlock_table

//=============================================================================
// Overlay 3 BSS variables (0x8022FFxx)
//=============================================================================
extern u8 D_ovl3_8022FF04;      // g_anim_frame_major (0-7)
extern u8 D_ovl3_8022FF05;      // g_anim_flip_flag (0/1 toggle)
extern u8 D_ovl3_8022FF06;      // g_anim_frame_minor (0-3)
extern s8 D_ovl3_8022FF09;      // g_current_player_index (0-3, or 4+ for multiplayer)
extern s16 D_ovl3_8022FFC6;     // g_selected_slot_x
extern s16 D_ovl3_8022FFC8;     // g_selected_slot_y

//=============================================================================
// Constants
//=============================================================================

// Grid dimensions
#define GRID_ROWS           3
#define GRID_COLS           7
#define ROW_STRIDE          14      // bytes per row in slot array

// Screen coordinates (in pixels, converted to 10.2 fixed point for GBI)
#define TILE_Y_BASE         0x38    // Y start = 56
#define TILE_Y_SPAN         0x20    // Y height = 32
#define TILE_X_BASE         0x18    // X start = 24
#define TILE_X_SPAN         0x20    // X width = 32
#define TILE_STRIDE         0x28    // spacing = 40

// Tile ID limits
#define MAX_TILE_ID         0x11    // 17
#define HIGHLIGHT_THRESHOLD (-0xA)  // -10

// GBI command sizes
#define GFX_NORMAL_SIZE     0x80    // bytes for normal tile commands
#define GFX_HIGHLIGHT_SIZE  0x88    // bytes for highlight tile commands

//=============================================================================
// Helper macros for 10.2 fixed-point coordinate clamping
//=============================================================================

// Clamp coordinate to valid range and convert to 10.2 format bits 11:5
#define CLAMP_COORD(x) ({ \
    s32 _x = (x); \
    s32 _x4 = _x << 2; \
    s32 _xh = (_x << 18) >> 16; \
    s32 _mask = ~_xh >> 31; \
    (_x4 & _mask) & 0xFE0; \
})

//=============================================================================
// Main render function
//=============================================================================

#ifdef NONMATCHING
/**
 * Renders the character selection grid tiles.
 *
 * @param gfxp   Pointer to graphics context (Gfx** at offset 0)
 * @param mode   Render mode: 0 = normal tiles only, 1 = include highlight tiles
 */
void render_character_selector_tiles(void** gfxp, u8 mode) {
    Gfx* gfx;
    void* player_ctx;
    s32 row, col;
    s16 row_s, col_s;
    s16* slot_row_ptr;
    s16 tile_id;
    s32 y_bottom, y_top;
    s32 x_left, x_right;
    s32 y_top_fixed, y_offset;
    void* texture_ptr;
    void* palette_ptr;
    Gfx* texrect_params;
    s32 s_coord, t_coord;

    // GBI command constants
    u32 cmd_loadsync = 0xE6000000;
    u32 cmd_pipesync = 0xE7000000;

    gfx = *(Gfx**)gfxp;
    player_ctx = func_8005AF74();

    // Outer loop: iterate rows
    for (row = 0; row < GRID_ROWS; row++) {
        row_s = (s16)row;

        // Calculate row pointer into slot array
        // slot_row_ptr = &D_800C28E4[row * 7]
        slot_row_ptr = &D_800C28E4[row_s * GRID_COLS];

        // Calculate Y coordinates for this row
        // y = row * 40 (stride)
        y_bottom = row_s * TILE_STRIDE + TILE_Y_BASE + TILE_Y_SPAN;  // row*40 + 88
        y_top = row_s * TILE_STRIDE + TILE_Y_BASE;                    // row*40 + 56

        // Convert to 10.2 fixed point and build G_TEXRECT base command
        u32 y_bottom_bits = CLAMP_COORD(y_bottom);
        u32 texrect_base = y_bottom_bits | 0xE4000000;  // G_TEXRECT with Y_bottom

        y_top_fixed = CLAMP_COORD(y_top);
        y_offset = (s16)y_top << 10;  // for T coordinate adjustment

        // Inner loop: iterate columns
        for (col = 0; col < GRID_COLS; col++) {
            col_s = (s16)col;
            tile_id = slot_row_ptr[col_s];

            // Check for special game mode (0x10) - highlight current selection
            if (D_801EFFA0 == 0x10) {
                s8 sel_row = *(s8*)(player_ctx + 0x7);
                s8 sel_col = *(s8*)(player_ctx + 0x3);

                if (row_s == sel_row && col_s == sel_col) {
                    // This is the selected slot
                    if (D_801B71ED >= 0xE) {
                        tile_id = col_s + 0xE;  // Use alternate tile
                    }
                }
            }

            // Validate and clamp tile ID
            if (tile_id < 0) {
                // Negative tile ID - render highlight effect
                if (mode != 1) {
                    continue;  // Skip highlight if mode != 1
                }

                if (tile_id >= HIGHLIGHT_THRESHOLD) {
                    continue;  // Not a highlight tile
                }

                // Load highlight texture/palette
                s32 idx = tile_id;  // negative index
                palette_ptr = D_8012D4F8[idx];
                texture_ptr = D_80105C40[idx];

                // Emit GBI commands for highlight tile
                // [Similar to normal tile but with different combiner mode]
                // Uses G_SETPRIMCOLOR for tint

                gfx += GFX_HIGHLIGHT_SIZE / 8;
                texrect_params = gfx;
                gfx++;

                // gDPSetTextureImage - CI 4-bit
                gfx[-17].words.w0 = 0xFD100000;
                gfx[-17].words.w1 = (u32)texture_ptr & ~7;

                // gDPTileSync
                gfx[-16].words.w0 = 0xE8000000;
                gfx[-16].words.w1 = 0;

                // gDPSetTile - IA 4-bit for loading
                gfx[-15].words.w0 = 0xF5000100;
                gfx[-15].words.w1 = 0x07000000;

                // gDPLoadSync
                gfx[-14].words.w0 = cmd_loadsync;
                gfx[-14].words.w1 = 0;

                // gDPLoadTLUT_pal16
                gfx[-13].words.w0 = 0xF0000000;
                gfx[-13].words.w1 = 0x073FC000;

                // gDPPipeSync
                gfx[-12].words.w0 = cmd_pipesync;
                gfx[-12].words.w1 = 0;

                // gDPSetCombineMode - highlight mode
                gfx[-11].words.w0 = 0xFC119623;
                gfx[-11].words.w1 = 0xFF2FFFFF;

                // gDPSetPrimColor - light gray tint
                gfx[-10].words.w0 = 0xFA000000;
                gfx[-10].words.w1 = 0x0A0A0AFF;

                // gDPSetTextureImage - palette
                gfx[-9].words.w0 = 0xFD500000;
                gfx[-9].words.w1 = (u32)palette_ptr & ~7;

                // gDPSetTile - CI 16-bit
                gfx[-8].words.w0 = 0xF5500000;
                gfx[-8].words.w1 = 0x07000000;

                // gDPLoadSync
                gfx[-7].words.w0 = cmd_loadsync;
                gfx[-7].words.w1 = 0;

                // gDPLoadBlock
                gfx[-6].words.w0 = 0xF3000000;
                gfx[-6].words.w1 = 0x071FF200;

                // gDPPipeSync
                gfx[-5].words.w0 = cmd_pipesync;
                gfx[-5].words.w1 = 0;

                // gDPSetTile - palette descriptor
                gfx[-4].words.w0 = 0xF5480800;
                gfx[-4].words.w1 = 0;

                // gDPSetTileSize - 32x32
                gfx[-3].words.w0 = 0xF2000000;
                gfx[-3].words.w1 = 0x0007C07C;

                // gDPPipeSync
                gfx[-2].words.w0 = cmd_pipesync;
                gfx[-2].words.w1 = 0;

            } else {
                // Normal tile
                if (tile_id > MAX_TILE_ID) {
                    tile_id = MAX_TILE_ID;
                }

                // Load texture/palette from tables
                palette_ptr = D_8012D430[tile_id];
                texture_ptr = D_80105B78[tile_id];

                // Emit GBI commands for normal tile
                gfx += GFX_NORMAL_SIZE / 8;
                texrect_params = gfx;
                gfx++;

                // gDPSetTextureImage - CI 4-bit
                gfx[-16].words.w0 = 0xFD100000;
                gfx[-16].words.w1 = (u32)texture_ptr & ~7;

                // gDPTileSync
                gfx[-15].words.w0 = 0xE8000000;
                gfx[-15].words.w1 = 0;

                // gDPSetTile - IA 4-bit for loading
                gfx[-14].words.w0 = 0xF5000100;
                gfx[-14].words.w1 = 0x07000000;

                // gDPLoadSync
                gfx[-13].words.w0 = cmd_loadsync;
                gfx[-13].words.w1 = 0;

                // gDPLoadTLUT_pal16
                gfx[-12].words.w0 = 0xF0000000;
                gfx[-12].words.w1 = 0x073FC000;

                // gDPPipeSync
                gfx[-11].words.w0 = cmd_pipesync;
                gfx[-11].words.w1 = 0;

                // gDPSetCombineMode - normal mode
                gfx[-10].words.w0 = 0xFCFFFFFF;
                gfx[-10].words.w1 = 0xFFFCF279;

                // gDPSetTextureImage - palette
                gfx[-9].words.w0 = 0xFD500000;
                gfx[-9].words.w1 = (u32)palette_ptr & ~7;

                // gDPSetTile - CI 16-bit
                gfx[-8].words.w0 = 0xF5500000;
                gfx[-8].words.w1 = 0x07000000;

                // gDPLoadSync
                gfx[-7].words.w0 = cmd_loadsync;
                gfx[-7].words.w1 = 0;

                // gDPLoadBlock
                gfx[-6].words.w0 = 0xF3000000;
                gfx[-6].words.w1 = 0x071FF200;

                // gDPPipeSync
                gfx[-5].words.w0 = cmd_pipesync;
                gfx[-5].words.w1 = 0;

                // gDPSetTile - palette descriptor
                gfx[-4].words.w0 = 0xF5480800;
                gfx[-4].words.w1 = 0;

                // gDPSetTileSize - 32x32
                gfx[-3].words.w0 = 0xF2000000;
                gfx[-3].words.w1 = 0x0007C07C;

                // gDPPipeSync
                gfx[-2].words.w0 = cmd_pipesync;
                gfx[-2].words.w1 = 0;
            }

            // Calculate X coordinates for texture rectangle
            x_left = col_s * TILE_STRIDE + TILE_X_BASE;      // col*40 + 24
            x_right = col_s * TILE_STRIDE + TILE_X_BASE + TILE_X_SPAN;  // col*40 + 56

            // Build G_TEXRECT command words
            u32 x_right_bits = CLAMP_COORD(x_right);
            u32 x_left_bits = CLAMP_COORD(x_left);

            // G_TEXRECT word 0: command | xh | yh
            gfx[-1].words.w0 = texrect_base | (x_right_bits << 12);
            // G_TEXRECT word 1: xl | yl | tile
            gfx[-1].words.w1 = y_top_fixed | (x_left_bits << 12);

            // G_RDPHALF_1 - S/T coordinates
            gfx[0].words.w0 = 0xE1000000;

            // Calculate S coordinate adjustment for negative X
            s_coord = 0;
            if ((s16)x_left < 0) {
                s32 adj = (s16)x_left << 3;
                if (adj < 1) {
                    adj = -adj;
                }
                s_coord = adj << 16;
            }

            // Calculate T coordinate adjustment for negative Y
            t_coord = 0;
            if (y_top_fixed < 0) {
                s32 adj = y_offset >> 7;
                if (adj < 1) {
                    adj = -adj;
                }
                t_coord = adj & 0xFFFF;
            }

            texrect_params->words.w1 = s_coord | t_coord;

            // G_RDPHALF_2 - dsdx/dtdy (1.0, 1.0 in 10.5 format)
            gfx++;
            gfx[-1].words.w0 = 0xF1000000;
            gfx[-1].words.w1 = 0x04000400;

            // Final gDPPipeSync
            gfx++;
            gfx[-1].words.w0 = cmd_pipesync;
            gfx[-1].words.w1 = 0;

            gfx += 2;  // Account for additional spacing
        }
    }

    //=========================================================================
    // Update animation state after full grid render
    //=========================================================================
    D_ovl3_8022FF06++;  // Increment minor frame

    if ((s8)D_ovl3_8022FF06 == 4) {
        D_ovl3_8022FF06 = 0;  // Reset minor frame
        D_ovl3_8022FF04++;    // Increment major frame

        if ((s8)D_ovl3_8022FF04 >= 8) {
            D_ovl3_8022FF04 = 0;  // Wrap major frame
        }

        //=====================================================================
        // Update highlight position for selected character
        //=====================================================================
        D_ovl3_8022FF05 ^= 1;  // Toggle flip flag

        // Get selected slot coordinates from player context
        void* ctx = player_ctx + D_801B7070;
        s8 sel_row = *(s8*)(ctx + 0x7);
        s8 sel_col = *(s8*)(ctx + 0x3);

        // Look up tile ID for selected slot
        s16* slot_ptr = &D_800C28E4[sel_row * GRID_COLS + sel_col];
        s16 sel_tile = *slot_ptr;
        u16 sel_tile_u = *(u16*)slot_ptr;

        // Handle special character unlock mappings
        if (sel_tile < -0x14) {
            sel_tile_u += 0x32;  // Unlock offset
        } else if (sel_tile >= 0xE) {
            // Map through unlock table
            s32 unlock_idx = (sel_tile - 0xE) * 22;  // stride = 22
            sel_tile_u = D_800FE3D8[unlock_idx / 2];
        }

        // Load texture/palette for selected character
        palette_ptr = D_8012D430[sel_tile_u];
        texture_ptr = D_80105B78[sel_tile_u];

        // Get texture coordinates based on player index
        s16 tex_coord;
        s8 player_idx = D_ovl3_8022FF09;

        if (player_idx == 0) {
            tex_coord = D_800C2590[sel_tile_u * 4];
        } else if (player_idx == 1) {
            tex_coord = D_800C2592[sel_tile_u * 4];
        } else if (player_idx == 2) {
            tex_coord = D_800C2594[sel_tile_u * 4];
        } else if (player_idx == 3) {
            tex_coord = D_800C2596[sel_tile_u * 4];
        } else {
            // Multiplayer mode - use dynamic offset
            s16 offset = func_ovl3_8020C2D8(sel_tile_u);
            tex_coord = D_800C2590[sel_tile_u * 4 + offset];
        }

        // Setup texture for selected character display
        void* coords_buf[8];  // sp+0x30
        void* entry_buf[8];   // sp+0x10

        func_800504E8(tex_coord, coords_buf);
        s16 format = func_80050598(coords_buf);
        func_800505A0(texture_ptr, format, coords_buf);

        // Random 1% chance for alternate palette table
        void* palette_table;
        if (func_800299F4(100) != 0) {
            palette_table = D_800C2780;
        } else {
            palette_table = D_800C2650;
        }

        // Lookup palette animation entry
        u16 palette_id = ((u16*)palette_table)[sel_tile_u * 4 + D_ovl3_8022FF04];
        func_8005062C(palette_id, entry_buf);
        func_800506D4(palette_ptr, entry_buf);

        // Store selected slot coordinates for external reference
        ctx = player_ctx + D_801B7070;
        D_ovl3_8022FFC6 = (s8)*(u8*)(ctx + 0x3);
        D_ovl3_8022FFC8 = (s8)*(u8*)(ctx + 0x7);
    }

    // Update output pointer
    *(Gfx**)gfxp = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl3/character_select", render_character_selector_tiles);
#endif
