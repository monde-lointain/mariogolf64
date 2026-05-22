/**
 * @file spark_effects.c
 * @brief Spark/trail particle effects rendering with physics simulation
 *
 * This file implements a spark particle system used for visual effects.
 * Each spark has position, velocity, lifetime, and animated RGB color.
 * The system supports multiple emitters, each managing multiple sparks.
 */

#include "common.h"
#include <PR/ultratypes.h>
#include <PR/gbi.h>

//=============================================================================
// External function declarations
//=============================================================================
extern void func_80077E94(void* emitter, s32 index);
extern void update_item_status(s32 sound_id, s32 volume, s32 pan);
extern void convert_and_pack_floats_to_fixed(f32* src_mtx, void* dst);
extern f32 sinf2(f32 angle);

//=============================================================================
// External data declarations (BSS)
//=============================================================================

// Global state
extern s32 g_num_emitters;           // D_800C4664 - total number of active emitters
extern s32 g_spark_enabled;          // D_800C4660 - master enable flag
extern s32 g_matrix_buffer_index;    // D_800C54E0 - matrix double-buffer index
extern f32 g_spark_anim_time;        // D_800C46A8 - animation time accumulator
extern f64 g_spark_time_delta;       // D_800D1908 - time delta (double precision)

// Emitter array (stride 0x2C = 44 bytes per emitter)
// Layout per emitter:
//   +0x00: pointer to spark particle array
//   +0x04: spark count
//   +0x08: animation frame counter
//   +0x25: active flag (byte)
extern u8 g_spark_emitters[];        // D_80105140 - emitter data array
extern s32 g_spark_counts[];         // D_80105144 - spark count per emitter (offset +4)
extern s32 g_spark_lifetimes[];      // D_80105148 - lifetime/fade counter per emitter (offset +8)
extern u8 g_spark_active_flags[];    // D_80105165 - active flag per emitter (offset +0x25)

// Vertex data
extern Vtx g_spark_billboard_vtx[];  // D_1B5638 - billboard quad vertices (ROM)
extern Vtx g_spark_vertex_buffer[];  // D_800C4668 - vertex buffer

//=============================================================================
// Spark particle structure
// Each spark is 0x98 (152) bytes. Key offsets:
//   +0x80: pos_x (f32)
//   +0x84: pos_y (f32)
//   +0x88: vel_x (f32)
//   +0x8C: vel_y (f32)
//   +0x94: lifetime (s16)
//=============================================================================

#define SPARK_POS_X_OFFSET   0x80
#define SPARK_POS_Y_OFFSET   0x84
#define SPARK_VEL_X_OFFSET   0x88
#define SPARK_VEL_Y_OFFSET   0x8C
#define SPARK_LIFETIME_OFFSET 0x94

#define SPARK_STRIDE         0x98
#define EMITTER_STRIDE       0x2C

// Float constants
#define VELOCITY_DAMPING     0.94f      // 0x3F70A3D7
#define GRAVITY              0.0005f    // 0x3A03126F
#define SCALE_FACTOR         0.1f       // 0x3DCCCCCD
#define Z_OFFSET            -100.0f     // 0xC2C80000
#define COLOR_AMPLITUDE      64.0f      // 0x42800000

// Animation frame thresholds
#define FRAME_WRAP_A         160        // 0xA0
#define FRAME_WRAP_B         220        // 0xDC

// Sound IDs
#define SOUND_SPARK_A        0x31
#define SOUND_SPARK_B        0x32

//=============================================================================
// Helper macros to access spark particle data
//=============================================================================
#define SPARK_POS_X(spark)    (*(f32*)((u8*)(spark) + SPARK_POS_X_OFFSET))
#define SPARK_POS_Y(spark)    (*(f32*)((u8*)(spark) + SPARK_POS_Y_OFFSET))
#define SPARK_VEL_X(spark)    (*(f32*)((u8*)(spark) + SPARK_VEL_X_OFFSET))
#define SPARK_VEL_Y(spark)    (*(f32*)((u8*)(spark) + SPARK_VEL_Y_OFFSET))
#define SPARK_LIFETIME(spark) (*(s16*)((u8*)(spark) + SPARK_LIFETIME_OFFSET))

//=============================================================================
// Main render function
//=============================================================================
#ifdef NONMATCHING
void render_spark_effects(Gfx** gfxp) {
    Gfx* gfx = *gfxp;
    s32 emitter_idx;
    s32 spark_idx;
    s32 frame_counter;
    u8* emitter_base;
    u8* spark;
    s32 spark_count;
    s32 lifetime;
    f32 pos_x, pos_y;
    f32 vel_x, vel_y;
    f32 magnitude;
    f32 norm_x, norm_y;
    f32 scale;
    f32 mtx_local[16];  // 2x3 transform matrix on stack
    s32 red, green, blue, alpha;
    s32 rgb_packed;

    // Early exit if no emitters
    if (g_num_emitters <= 0) {
        return;
    }

    //=========================================================================
    // PART 1: Animation frame update loop
    //=========================================================================
    if (g_spark_enabled != 0) {
        emitter_base = g_spark_emitters;

        for (emitter_idx = 0; emitter_idx < g_spark_enabled; emitter_idx++) {
            // Check if emitter is active
            if (g_spark_active_flags[emitter_idx * EMITTER_STRIDE] != 1) {
                emitter_base += EMITTER_STRIDE;
                continue;
            }

            // Get and increment frame counter (at offset +8)
            frame_counter = *(s32*)(emitter_base + 8);
            frame_counter++;
            *(s32*)(emitter_base + 8) = frame_counter;

            if (frame_counter == FRAME_WRAP_A) {
                // Trigger sound A
                update_item_status(SOUND_SPARK_A, 8, 0x7F);
            } else if (frame_counter == FRAME_WRAP_B) {
                // Reset counter and spawn new particles
                *(s32*)(emitter_base + 8) = 0;
                func_80077E94(emitter_base, emitter_idx);
                update_item_status(SOUND_SPARK_B, 8, 0x7F);
            }

            emitter_base += EMITTER_STRIDE;
        }
    }

    //=========================================================================
    // PART 2: Initialize transform constants
    //=========================================================================
    // Initialize local matrix to identity-ish
    mtx_local[2] = 0.0f;   // [0][1]
    mtx_local[3] = 0.0f;
    mtx_local[6] = 0.0f;
    mtx_local[7] = 0.0f;
    mtx_local[8] = 0.0f;
    mtx_local[9] = 0.0f;
    mtx_local[11] = 0.0f;
    mtx_local[10] = SCALE_FACTOR;  // default scale
    mtx_local[15] = 1.0f;

    if (g_spark_enabled == 0) {
        goto setup_gbi;
    }

    //=========================================================================
    // PART 3: Physics simulation - update spark positions
    //=========================================================================
    for (emitter_idx = 0; emitter_idx < g_spark_enabled; emitter_idx++) {
        // Check if emitter is active
        if (g_spark_active_flags[emitter_idx * EMITTER_STRIDE] != 1) {
            continue;
        }

        spark_count = g_spark_counts[emitter_idx];
        if (spark_count == 0) {
            continue;
        }

        // Get pointer to spark array for this emitter
        spark = *(u8**)(g_spark_emitters + emitter_idx * EMITTER_STRIDE);

        for (spark_idx = 0; spark_idx < spark_count; spark_idx++) {
            // Check if spark is alive
            lifetime = SPARK_LIFETIME(spark);
            if (lifetime <= 0) {
                spark += SPARK_STRIDE;
                continue;
            }

            // Load current state
            pos_x = SPARK_POS_X(spark);
            pos_y = SPARK_POS_Y(spark);
            vel_x = SPARK_VEL_X(spark);
            vel_y = SPARK_VEL_Y(spark);

            // Update position
            pos_x += vel_x;
            pos_y += vel_y;

            // Apply velocity damping
            vel_x *= VELOCITY_DAMPING;
            vel_y *= VELOCITY_DAMPING;

            // Apply gravity
            vel_y -= GRAVITY;

            // Decrement lifetime
            lifetime--;

            // Faster decay when lifetime < 128
            if (lifetime < 128) {
                lifetime -= 4;
            }

            // Store updated values
            SPARK_POS_X(spark) = pos_x;
            SPARK_POS_Y(spark) = pos_y;
            SPARK_VEL_X(spark) = vel_x;
            SPARK_VEL_Y(spark) = vel_y;
            SPARK_LIFETIME(spark) = (s16)lifetime;

            //=================================================================
            // Compute normalized velocity direction for sprite rotation
            //=================================================================
            vel_x = SPARK_VEL_X(spark);
            vel_y = SPARK_VEL_Y(spark);
            magnitude = sqrtf(vel_x * vel_x + vel_y * vel_y);

            if (magnitude == 0.0f) {
                // Default direction when stationary
                norm_x = 0.0f;
                norm_y = SCALE_FACTOR;
                scale = 0.5f;
            } else {
                // Normalize velocity
                norm_x = vel_x / magnitude;
                norm_y = vel_y / magnitude;

                //=============================================================
                // Compute scale based on emitter lifetime
                //=============================================================
                s32 emit_lifetime = g_spark_lifetimes[emitter_idx];

                if (emit_lifetime < 8) {
                    // Growing phase: scale increases with lifetime
                    scale = norm_y * 0.5f * (f32)emit_lifetime;
                } else if (emit_lifetime < 65) {
                    // Shrinking phase
                    scale = norm_y * (4.0f - (f32)(emit_lifetime - 8) * 0.5f);
                } else {
                    // Stable phase
                    scale = 1.0f + (f32)(emit_lifetime - 64) * 0.04f;
                }

                // Clamp scale to minimum 1.0
                if (scale < 1.0f) {
                    scale = 1.0f;
                }
            }

            //=================================================================
            // Build 2x3 transform matrix
            //=================================================================
            mtx_local[1] = norm_x * SCALE_FACTOR;        // m[0][0]
            mtx_local[0] = -norm_y * SCALE_FACTOR;       // m[1][0]
            mtx_local[4] = -norm_x * scale * SCALE_FACTOR; // m[0][1]
            mtx_local[5] = -norm_y * scale * SCALE_FACTOR; // m[1][1]

            // Translation
            mtx_local[12] = SPARK_POS_X(spark);  // trans_x
            mtx_local[13] = SPARK_POS_Y(spark);  // trans_y
            mtx_local[14] = Z_OFFSET;            // trans_z

            // Convert to fixed-point and store in output matrix
            convert_and_pack_floats_to_fixed(
                mtx_local,
                spark + (g_matrix_buffer_index << 6)
            );

            spark += SPARK_STRIDE;
        }
    }

setup_gbi:
    //=========================================================================
    // PART 4: Setup GBI rendering pipeline
    //=========================================================================

    // gDPPipeSync x2
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);

    // gSPSetGeometryMode - enable lighting (0xE3000A01)
    gSPSetGeometryMode(gfx++, G_LIGHTING);

    // gDPPipeSync
    gDPPipeSync(gfx++);

    // gDPSetRenderMode - XLU surface (0xE200001C, 0x504240)
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);

    // gDPPipeSync
    gDPPipeSync(gfx++);

    gDPSetAlphaCompare(gfx++, G_AC_NONE)

    // gSPTexture (0xD7000000, 0x80008000)
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);

    // gSPGeometryMode - clear all, set new (0xD9000000, 0xD9FFFFFF -> 0x200004)
    gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
    gSPSetGeometryMode(gfx++, 0x200004);

    // gDPPipeSync
    gDPPipeSync(gfx++);

    // gSPSetGeometryMode - cull none (0xE3001001)
    gSPSetGeometryMode(gfx++, G_CULL_FRONT);

    // gDPPipeSync
    gDPPipeSync(gfx++);

    // gSPSetGeometryMode - cull back (0xE3001A01)
    gSPSetGeometryMode(gfx++, G_CULL_BACK | G_CULL_FRONT);

    // gDPSetCombineMode (0xFC323864, 0xFF73FFFF) - shade * prim color
    gDPSetCombineLERP(gfx++, PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0, SHADE, 0, PRIMITIVE, 0, SHADE, 0)

    // gDPPipeSync
    gDPPipeSync(gfx++);

    // gSPMatrix - load projection (0xDA380007)
    gSPMatrix(gfx++, g_spark_billboard_vtx, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);

    //=========================================================================
    // PART 5: Animated color calculation using sine waves
    //=========================================================================
    g_spark_anim_time = (f32)((f64)g_spark_anim_time + g_spark_time_delta);

    // Red channel: sin(time)
    red = (s32)(sinf2(g_spark_anim_time) * COLOR_AMPLITUDE) + 127;

    // Green channel: sin(time + 2.0) - phase offset
    green = (s32)(sinf2(g_spark_anim_time + 2.0f) * COLOR_AMPLITUDE) + 127;

    // Blue channel: sin(time + 4.0) - larger phase offset
    blue = (s32)(sinf2(g_spark_anim_time + 4.0f) * COLOR_AMPLITUDE) + 127;

    if (g_spark_enabled == 0) {
        *gfxp = gfx;
        return;
    }

    //=========================================================================
    // PART 6: Emit per-spark rendering commands
    //=========================================================================
    // Pack RGB (alpha filled in per-spark)
    rgb_packed = (red << 24) | ((green & 0xFF) << 16) | ((blue & 0xFF) << 8);

    for (emitter_idx = 0; emitter_idx < g_spark_enabled; emitter_idx++) {
        // Check if emitter is active
        if (g_spark_active_flags[emitter_idx * EMITTER_STRIDE] != 1) {
            continue;
        }

        // Compute alpha based on emitter lifetime
        lifetime = g_spark_lifetimes[emitter_idx];
        if (lifetime < 40) {
            alpha = 255;
        } else {
            // Fade out
            alpha = 255 - (lifetime - 40) * 8;

            // Flicker effect on odd frames
            if (lifetime & 1) {
                alpha += 24;
            }
        }

        // Clamp alpha
        if (alpha < 0) alpha = 0;
        if (alpha > 255) alpha = 255;

        // Emit G_SETPRIMCOLOR
        gDPSetPrimColor(gfx++, 0, 0, red, green, blue, alpha);

        spark_count = g_spark_counts[emitter_idx];
        if (spark_count == 0) {
            continue;
        }

        // Get spark array pointer
        spark = *(u8**)(g_spark_emitters + emitter_idx * EMITTER_STRIDE);

        for (spark_idx = 0; spark_idx < spark_count; spark_idx++) {
            lifetime = SPARK_LIFETIME(spark);
            if (lifetime <= 0) {
                spark += SPARK_STRIDE;
                continue;
            }

            // gDPPipeSync
            gDPPipeSync(gfx++);

            // gSPMatrix - load modelview (0xDA380003)
            // Matrix is stored at spark + buffer_offset, converted to physical addr
            gSPMatrix(gfx++,
                (Mtx*)((u32)(spark + (g_matrix_buffer_index << 6)) - 0x80000000),
                G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);

            // gSPVertex (0x01003006) - load 6 vertices starting at index 0
            gSPVertex(gfx++, g_spark_vertex_buffer, 6, 0);

            // gSP1Triangle (0x05000204) - triangle with vertices 0, 2, 4
            gSP1Triangle(gfx++, 0, 2, 4, 0);

            spark += SPARK_STRIDE;
        }
    }

    *gfxp = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/spark_effects", render_spark_effects);
#endif
