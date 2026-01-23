#include "common.h"
#include <PR/ultratypes.h>
#include <PR/gbi.h>
#include <PR/os_convert.h>

// G_SETHALF (0xEE) - not in standard GBI, game-specific
#define G_SETHALF 0xEE
#define gDPSetHalf(pkt, val)                                    \
{                                                               \
    Gfx* _g = (Gfx*)(pkt);                                      \
    _g->words.w0 = (G_SETHALF << 24);                           \
    _g->words.w1 = (val);                                       \
}

// External functions
extern s32 project_point_to_screen(f32 x, f32 y, f32 z, void* out);
extern void mtx_from_rts(Mtx* mtx, void* rotation, Vec3f* translation, Vec3f* scale);
extern void convert_and_pack_floats_to_fixed(Mtx* src, Mtx* dst);

// BSS data - particle system state
extern s32 g_particle_frame_counter;      // D_ovl0_80224860 - wraps at 100
extern s32 g_particle_fade_state;         // D_800C1B70 - 0=idle, 1=fade_in, 2=fade_out
extern s32 g_particle_alpha;              // D_800C1B74 - current alpha (0-255)
extern s32 g_frame_buffer_index;          // D_800C54E0 - double buffer index

// Emitter data (5 emitters)
extern s8 g_emitter_lifetimes[5];         // D_ovl0_80229236 - lifetime per emitter (-1=inactive)
extern s8 g_emitter_countdown[5];         // D_ovl0_8022923B - countdown timers
extern s32 g_emitter_pos_x[5];            // D_ovl0_80229240 - world X (as int, scaled)
extern s32 g_emitter_pos_y[5];            // D_ovl0_80229244 - world Y
extern s32 g_emitter_pos_z[5];            // D_ovl0_80229248 - world Z

// Particle data (30 particles)
extern u8 g_particle_flags[30];           // D_ovl0_802291D8 - active flags
extern u8 g_particle_needs_matrix[30];    // D_ovl0_802291F8 - matrix update flag
extern u8 g_particle_emitter_index[30];   // D_ovl0_80229218 - which emitter owns this particle
extern f32 g_particle_rotation_z[30];     // D_ovl0_80229160 - Z rotation per particle

// Sprite offset/velocity arrays
extern f32 g_sprite_offset_x[30];         // D_ovl0_80228F80
extern f32 g_sprite_offset_y[30];         // D_ovl0_80228F84
extern f32 g_sprite_velocity_y[30];       // D_ovl0_80229074

// Texture/matrix data
extern u32 g_particle_texture_addr;       // D_ovl0_8022927C
extern Mtx g_particle_matrices[30];       // D_ovl0_80228080 - RSP matrices
extern Vtx g_joint_vertex_data[];         // D_ovl0_80224868

// ROM vertex data
extern Vtx D_1B5638[];

// Local particle state structure (on stack)
typedef struct {
    s32 pad[4];                 // 0x00-0x0F: rotation params
    Vec3f translation;          // 0x10-0x1B
    s32 pad2;                   // 0x1C
    Vec3f position;             // 0x20-0x2B
    s32 pad3;                   // 0x2C
    Vec3f scale;                // 0x30-0x3B
} ParticleTransform;

typedef struct {
    s32 visible;                // 0xA0 offset from base
    f32 depth_scale;            // 0x88 offset
    s32 half_size;              // 0xF8 offset
    s16 tex_step;               // 0x110 offset (as s32, but s16 at 0x112)
} EmitterState;

#define MAX_EMITTERS 5
#define MAX_PARTICLES 30
#define SCREEN_CENTER_X 160.0f
#define SCREEN_CENTER_Y 120.0f
#define VELOCITY_DAMPING 0.98f  // 0x3F7AE148

void render_particle_effects(Gfx** gfxp) {
    Gfx* gfx = *gfxp;
    s32 i, j;
    s32 emitter_idx;

    // Local state arrays (simplified from actual stack layout)
    s32 emitter_visible[MAX_EMITTERS];
    s32 screen_coords[MAX_EMITTERS][3];  // x, y, z per emitter
    f32 depth_scale[MAX_EMITTERS];
    s32 half_size[MAX_EMITTERS];
    s32 tex_step[MAX_EMITTERS];

    Vec3f rotation;
    Vec3f translation;
    Vec3f scale;
    Mtx temp_mtx;
    s32 screen_out[4];

    // Update frame counter (wraps at 100)
    if (g_particle_frame_counter != 0) {
        g_particle_frame_counter++;
        if (g_particle_frame_counter == 100) {
            g_particle_frame_counter = 0;
        }

        // Handle fade animation state machine
        if (g_particle_fade_state == 1) {
            // Fade in: increase alpha by 64
            g_particle_alpha += 64;
            if (g_particle_alpha >= 256) {
                g_particle_alpha = 255;
                g_particle_fade_state = 2;  // transition to fade out
            }
        } else if (g_particle_fade_state == 2) {
            // Fade out: decrease alpha by 32
            g_particle_alpha -= 32;
            if (g_particle_alpha < 0) {
                g_particle_alpha = 1;
                g_particle_fade_state = 0;  // back to idle
            }
        }
    }

    // Process 5 emitters - project world positions to screen
    for (i = 0; i < MAX_EMITTERS; i++) {
        if (g_emitter_lifetimes[i] == -1) {
            emitter_visible[i] = -1;
            continue;
        }

        // Convert int positions to float and scale
        f32 x = (f32)g_emitter_pos_x[i] * 0.001f;
        f32 y = (f32)g_emitter_pos_y[i] * 0.001f;
        f32 z = (f32)g_emitter_pos_z[i] * 0.001f;

        // Project to screen
        s32 result = project_point_to_screen(x, y, z, screen_out);

        if (result >= 0) {
            // Off-screen
            emitter_visible[i] = -1;
        } else {
            // Visible - store screen coords
            emitter_visible[i] = 1;
            screen_coords[i][0] = screen_out[0];
            screen_coords[i][1] = screen_out[1];
            screen_coords[i][2] = screen_out[2];

            // Calculate depth-based scale
            f32 raw_scale = *(f32*)&screen_out[3] * 0.3f;
            f32 life_scale = (f32)g_emitter_lifetimes[i] * 0.0625f + 0.3f;

            // Clamp scale to max 2.0
            if (raw_scale > 2.0f) {
                raw_scale = 2.0f;
            }
            depth_scale[i] = raw_scale;

            // Calculate half-size for sprite rendering
            s32 size = (s32)(raw_scale * life_scale * 8.0f);
            if (size <= 0) {
                tex_step[i] = 0xFFFF;
            } else {
                half_size[i] = size;
                tex_step[i] = (s32)(2048.0f / (raw_scale * life_scale));
            }

            // Increment lifetime (max 127)
            if (g_emitter_lifetimes[i] < 127) {
                g_emitter_lifetimes[i]++;
            }
        }
    }

    // Build display list - initial RDP state setup
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gSPSetOtherMode(gfx++, G_SETOTHERMODE_L, 0, 10, 0x00000A01);
    gDPPipeSync(gfx++);
    gSPSetOtherMode(gfx++, G_SETOTHERMODE_L, 0, 26, 0x00000030);
    gDPPipeSync(gfx++);
    gSPSetOtherMode(gfx++, G_SETOTHERMODE_L, 0, 16, 0);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gSPSetOtherMode(gfx++, G_SETOTHERMODE_L, 0, 12, 0);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0x00, 0xFF);
    gDPSetCombineMode(gfx++, G_CC_MODULATERGBA, G_CC_MODULATERGBA);
    gDPPipeSync(gfx++);
    gSPSetOtherMode(gfx++, G_SETOTHERMODE_H, 30, 1, 1);
    gDPSetBlendColor(gfx++, 0, 0, 0, 1);
    gDPPipeSync(gfx++);

    // Setup render mode for particles
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gSPSetOtherMode(gfx++, G_SETOTHERMODE_L, 0, 18, 0x00001201);

    // Load particle texture
    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 1,
                       (u8*)((g_particle_texture_addr + 8) & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 0, 0, G_TX_LOADTILE, 0,
               G_TX_WRAP, 0, 0, G_TX_WRAP, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x3FF, 0x400);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 8, 0, G_TX_RENDERTILE, 0,
               G_TX_WRAP, 5, 0, G_TX_WRAP, 5, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 31 << 2, 31 << 2);
    gDPPipeSync(gfx++);

    // Render 30 particles
    for (i = 0; i < MAX_PARTICLES; i++) {
        if (g_particle_flags[i] == 0) {
            continue;
        }

        // Update sprite position with velocity
        g_sprite_offset_x[i] += g_sprite_velocity_y[i - 1];  // note: offset by -4 bytes
        g_sprite_offset_y[i] += g_sprite_velocity_y[i];

        // Apply velocity damping
        g_sprite_velocity_y[i - 1] *= VELOCITY_DAMPING;
        g_sprite_velocity_y[i] *= VELOCITY_DAMPING;

        // Decrement particle lifetime
        g_particle_flags[i]--;

        // Get parent emitter
        emitter_idx = g_particle_emitter_index[i];
        if (emitter_visible[emitter_idx] != 1) {
            continue;
        }

        // Setup transform
        rotation.x = 0.0f;
        rotation.y = 0.0f;
        rotation.z = g_particle_rotation_z[i];

        translation.x = g_sprite_offset_x[i] * depth_scale[emitter_idx]
                       + (f32)screen_coords[emitter_idx][0] - SCREEN_CENTER_X;
        translation.y = g_sprite_offset_y[i] * depth_scale[emitter_idx]
                       - (f32)screen_coords[emitter_idx][1] + SCREEN_CENTER_Y;
        translation.z = -100.0f;

        scale.x = scale.y = scale.z = depth_scale[emitter_idx] * 0.1f;

        // Build matrix if needed
        if (g_particle_needs_matrix[i] == 1) {
            mtx_from_rts(&temp_mtx, &rotation, &translation, &scale);

            // Convert to fixed point and store in matrix buffer
            s32 buf_offset = (g_frame_buffer_index * 15) << 7;
            convert_and_pack_floats_to_fixed(&temp_mtx,
                (Mtx*)((u8*)g_particle_matrices + buf_offset + (i << 6)));
        }

        if (g_particle_needs_matrix[i] != 0) {
            continue;
        }

        // Screen bounds check
        s32 sx = screen_coords[emitter_idx][0];
        s32 sy = screen_coords[emitter_idx][1];
        if ((u32)(sx + 511) >= 1023 || (u32)(sy + 511) >= 1023) {
            continue;
        }

        // Calculate final sprite position
        f32 cx = g_sprite_offset_x[i] * depth_scale[emitter_idx] + (f32)sx;
        f32 cy = g_sprite_offset_y[i] * depth_scale[emitter_idx] + (f32)sy;
        s32 icx = (s32)cx;
        s32 icy = (s32)cy;
        s32 hs = half_size[emitter_idx];

        // gSPTextureRectangle - emit sprite
        s32 xh = (icx + hs) << 2;
        s32 yh = (icy + hs) << 2;
        s32 xl = (icx - hs) << 2;
        s32 yl = (icy - hs) << 2;

        // Clamp coordinates
        if (xh < 0) xh = 0;
        if (yh < 0) yh = 0;
        if (xl < 0) xl = 0;
        if (yl < 0) yl = 0;
        xh &= 0xFFC;
        yh &= 0xFFC;
        xl &= 0xFFC;
        yl &= 0xFFC;

        // Build texture rectangle command
        gfx->words.w0 = (G_TEXRECT << 24) | (xh << 12) | yh;
        gfx->words.w1 = (xl << 12) | yl;
        gfx++;

        // S/T coordinates and step
        s32 s_coord = 0;
        s32 t_coord = 0;
        s32 ts = tex_step[emitter_idx];

        gfx->words.w0 = G_RDPHALF_1 << 24;
        gfx->words.w1 = (s_coord << 16) | t_coord;
        gfx++;

        gfx->words.w0 = G_RDPHALF_2 << 24;
        gfx->words.w1 = (ts << 16) | ts;
        gfx++;
    }

    // Second pass: Setup for 3D joint rendering
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gSPSetOtherMode(gfx++, G_SETOTHERMODE_H, 30, 1, 0);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_OFF);
    gSPGeometryMode(gfx++, 0, G_SHADE | G_SHADING_SMOOTH);
    gSPSetOtherMode(gfx++, G_SETOTHERMODE_L, 0, 0, 0);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    gDPPipeSync(gfx++);
    gSPSetOtherMode(gfx++, G_SETOTHERMODE_H, 29, 1, 4);

    // Render 30 joints with matrices
    s32 mtx_buf_offset = (g_frame_buffer_index * 15) << 7;

    for (i = 0; i < MAX_PARTICLES; i++) {
        if (g_particle_needs_matrix[i] != 1) {
            continue;
        }

        emitter_idx = g_particle_emitter_index[i];
        if (emitter_visible[emitter_idx] != 1) {
            continue;
        }

        // Set depth
        gDPSetHalf(gfx++, (u32)screen_coords[emitter_idx][2] << 16);

        if (g_particle_flags[i] == 0) {
            continue;
        }

        // Load matrix and draw
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL((u8*)g_particle_matrices + mtx_buf_offset + (i << 6)),
                  G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
        gSPVertex(gfx++, g_joint_vertex_data, 6, 0);
        gSP1Triangle(gfx++, 0, 1, 2, 0);
    }

    // Cleanup pass
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gSPSetOtherMode(gfx++, G_SETOTHERMODE_H, 29, 1, 0);
    gDPPipeSync(gfx++);

    // Decrement emitter countdowns
    for (i = 0; i < MAX_EMITTERS; i++) {
        if (g_emitter_countdown[i] == 0) {
            g_emitter_lifetimes[i] = -1;  // mark inactive
        } else {
            g_emitter_countdown[i]--;
        }
    }

    *gfxp = gfx;
}
