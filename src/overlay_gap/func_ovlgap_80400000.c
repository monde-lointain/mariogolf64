#include "common.h"
#include "../main/terrain.h"

extern u8 D_80451A60[];

u8* func_ovlgap_80400000(void) {
    return D_80451A60;
}

extern s32 D_800BB020;
extern u16 D_800FED18;
extern u16 D_800FED1E;
extern f32 D_80105220;
extern f32 D_80105228;
extern f32 D_8010522C;
extern f32 D_80105230;
extern f32 D_80105234;
extern f32 D_8010525C;
extern s32 D_ovlgap_8040C5A8;
extern s32 D_ovlgap_8040C5AC;
extern s32 D_ovlgap_8040C5B0;
extern s32 D_ovlgap_8040C5B4;
extern u8 D_ovlgap_8040C580[];
extern Mtx D_40CC98;
extern Mtx D_8040CC98;
extern Vtx D_ovlgap_8040C780[];
extern u16 debug_menu_pad_buttons_a;
extern char D_80105118[];
extern s32 D_ovlgap_8040C70C;
extern u8 D_ovlgap_8040C710[];
extern char D_ovlgap_8040CAE4[];
extern u16 *nuGfxCfb_ptr;
void clear_text_grid(void);
void func_ovlgap_80405AF4(s32, s32);
s16 *get_terrain_vertex_pointer(u32, u32);
s16 *get_direct_grid_vertex(s32, s32);

void func_ovlgap_8040000C(void) {
    u16 buttons;
    f32 temp_fv0;
    f32 temp_fv0_2;
    GridVertex *temp_s0;
    GridVertex *temp_v0;

    buttons = D_800FED18;
    if (buttons & 0x100) {
        D_ovlgap_8040C5A8 += 1;
    }
    if (buttons & 0x200) {
        D_ovlgap_8040C5A8 -= 1;
    }
    if (buttons & 0x400) {
        D_ovlgap_8040C5AC -= 1;
    }
    if (buttons & 0x800) {
        D_ovlgap_8040C5AC += 1;
    }
    if (D_ovlgap_8040C5A8 < 0) {
        D_ovlgap_8040C5A8 = 0;
    }
    if (D_ovlgap_8040C5A8 >= 0x20) {
        D_ovlgap_8040C5A8 = 0x1F;
    }
    if (D_ovlgap_8040C5AC < 0) {
        D_ovlgap_8040C5AC = 0;
    }
    if (D_ovlgap_8040C5AC >= 0x40) {
        D_ovlgap_8040C5AC = 0x3F;
    }
    if (debug_menu_pad_buttons_a & 0x10) {
        f32 *scale = &D_8010525C;
        temp_fv0 = *scale * 1.1f;
        *scale = temp_fv0;
        if (temp_fv0 >= 10.0f) {
            *scale = 10.0f;
        }
    }
    if (debug_menu_pad_buttons_a & 0x20) {
        f32 *scale = &D_8010525C;
        temp_fv0_2 = *scale * 0.9f;
        *scale = temp_fv0_2;
        if (temp_fv0_2 < 0.1f) {
            *scale = 0.1f;
        }
    }
    if (D_800FED18 & 0x8000) {
        func_ovlgap_80405AF4(D_ovlgap_8040C5A8, D_ovlgap_8040C5AC);
    }
    if (D_800FED1E & 0x2000) {
        D_ovlgap_8040C5B0 = D_ovlgap_8040C5A8;
        D_ovlgap_8040C5B4 = D_ovlgap_8040C5AC;
        clear_text_grid();
        D_800BB020 = 0xA;
    }
    temp_s0 = (GridVertex *) get_terrain_vertex_pointer(D_ovlgap_8040C5A8, D_ovlgap_8040C5AC);
    temp_v0 = (GridVertex *) get_terrain_vertex_pointer(D_ovlgap_8040C5A8 + 1, D_ovlgap_8040C5AC + 1);
    D_80105220 = (f32) ((temp_s0->x + temp_v0->x) / 2);
    do {
        D_80105228 = (f32) ((temp_s0->z + temp_v0->z) / 2);
    } while (0);
}

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_804002C0);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80400FBC);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80402428);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80402E1C);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80403A04);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_804045C0);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80404850);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80404B04);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80404B84);

void func_ovlgap_8040582C(s32 *arg0, s32 arg1) {
    s32 *var_a0;
    s32 var_v1;
    s32 temp_v0;
    s32 tmp[2];
    s32 uninit;

    tmp[0] = uninit;
    var_a0 = arg0;
    var_v1 = 0;
    do {
        temp_v0 = *var_a0;
        if (temp_v0 == arg1) {
            return;
        }
        var_v1++;
        if (temp_v0 == -1) {
            *var_a0 = arg1;
            return;
        }
        var_a0++;
    } while (var_v1 < 2);
}

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80405870);

/* Toggles one cell of the 32x64 grid at (x, y); cells outside 0..0x1F x 0..0x3F are ignored.
 * Each 4x4 block is one terrain tile, and its 16-bit mask is unknown_992. */
void func_ovlgap_80405AF4(s32 x, s32 y) {
    s32 tile;
    u16 mask;

    if ((((u32) x >= 0x20U) | (y >= 0x40)) == 0) {
        if (y >= 0) {
            tile = (y >> 2) * 8 + x / 4;
            mask = g_terrain_tile_cache[tile].unknown_992;
            g_terrain_tile_cache[tile].unknown_992 = mask ^ (1 << (((y & 3) * 4) + (x & 3)));
        }
    }
}

/* Blends grid vertices (col, row) and (col + 1, row) by t/16 into the five edge slots of out.
 * Only the x and texture_s fields are written; slot indices come from D_ovlgap_8040C580. */
void func_ovlgap_80405B80(GridVertex *out, s32 col, s32 row, s32 t) {
    GridVertex *edge[2];
    s32 inv_t;
    s32 value;
    s32 i;
    s32 parity_off;
    u8 *entry;
    GridVertex *slot;

    inv_t = 0x10 - t;
    edge[0] = (GridVertex *) get_direct_grid_vertex(col, row);
    edge[1] = (GridVertex *) get_direct_grid_vertex(col + 1, row);
    i = 0;
    parity_off = (col & 1) * 2;
    entry = D_ovlgap_8040C580;
    do {
        value = (edge[0]->x * inv_t) + (edge[1]->x * t);
        slot = &out[entry[1 + parity_off]];
        if (value < 0) {
            value += 0xF;
        }
        slot->x = (s16) (value >> 4);
        slot->texture_s = (s16) (((edge[0]->texture_s * inv_t) + (edge[1]->texture_s * t)) / 16);
        i += 1;
        entry += 5;
    } while (i < 5);
}

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80405C94);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80405DB0);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80406570);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_804066C8);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80406818);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80406C3C);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_804077B0);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_804079B8);

extern f64 sin(f64);
extern f64 cos(f64);
extern u8 D_80491A60[];
extern f64 D_ovlgap_8040CA70;
extern f64 D_ovlgap_8040CA78;

void func_ovlgap_80407D64(void) {
    s32 texture_index = 0;
    s32 texture_row = 0;
    s32 bias = 0xFFFFF;
    s32 texture_column;
    s32 sine_phase;
    u8* texture_pixel;
    f64 angle_step = D_ovlgap_8040CA70;
    f64 amplitude = D_ovlgap_8040CA78;
    f64 cosine;
    s32 intensity;
    s32 scaled;

    while (texture_row < 32) {
        texture_column = 0;
        sine_phase = texture_row * 4;
        texture_pixel = &D_80491A60[texture_index];
        while (texture_column < 32) {
            cosine = cos((f64) (f32) (texture_column * 4) * angle_step);
            intensity = (s32) (cosine * sin((f64) (f32) sine_phase * angle_step) * amplitude);
            intensity = intensity * (intensity * intensity);
            scaled = intensity;
            if (intensity < 0) {
                scaled = intensity + bias;
            }
            intensity = scaled >> 0x14;
            if (intensity <= 0) {
                intensity = 1;
            }
            if (intensity >= 0x10) {
                intensity = 0xF;
            }
            *texture_pixel++ = (intensity << 4) | intensity;
            texture_column++;
            texture_index++;
        }
        texture_row++;
    }
}

extern void *heap3_alloc(s32 size);
extern void heap3_free(void *ptr);
extern s32 validate_heap_string(char *source_string, s32 allocation_tag);
extern void __assert(const char *expr, const char *file, int line);
extern char D_ovlgap_8040C9B0[], D_ovlgap_8040C9B4[];
extern char D_ovlgap_8040CA80[], D_ovlgap_8040CA88[], D_ovlgap_8040CA90[];
extern u8 D_ovlgap_8040C6D4[];
extern s32 D_ovlgap_8040C6D0;
extern char D_ovlgap_8040CAC0[];
void mark_scenery_collision_cells(void);
extern u8 D_ovlgap_8040C748[];
extern u16 *nuGfxCfb_ptr;
extern char D_80105118[];
extern char D_ovlgap_8040CB04[];


void func_ovlgap_80407F08(s32 *arg0, s32 arg1, char *arg2) {
    char line_buffer[0x100];
    u8 *buffer_cursor;
    u8 *buffer_end;
    u8 *buffer_start;
    s32 row;
    s32 col;

    void func_ovlgap_80407EC4(u8 *text) {
        for (;;) {
            u8 character = *text++;
            if (character == 0) {
                break;
            }
            if (buffer_cursor < buffer_end) {
                *buffer_cursor++ = character;
            }
        }
    }

    buffer_start = heap3_alloc(0x40000);
    buffer_cursor = buffer_start;
    buffer_end = buffer_start + 0x40000;
    if (buffer_start == NULL) {
        __assert(D_ovlgap_8040C9B0, D_ovlgap_8040C9B4, 0xBD3);
    }
    for (row = 0; row < (arg1 + 0xF) / 0x10; row++) {
        s32 *words = arg0;
        sprintf(line_buffer, D_ovlgap_8040CA80, row * 0x10);
        func_ovlgap_80407EC4((u8 *)line_buffer);
        for (col = 0; col < 4; col++) {
            sprintf(line_buffer, D_ovlgap_8040CA88, words[row * 4 + col]);
            func_ovlgap_80407EC4((u8 *)line_buffer);
        }
        sprintf(line_buffer, D_ovlgap_8040CA90);
        func_ovlgap_80407EC4((u8 *)line_buffer);
    }
    line_buffer[0] = 0x1A;
    line_buffer[1] = 0;
    func_ovlgap_80407EC4((u8 *)line_buffer);
    sprintf(line_buffer, arg2);
    validate_heap_string(line_buffer, buffer_cursor - buffer_start);
    heap3_free(&buffer_start);
}

/* Writes the midpoint of two grid vertices to ov; the flags halfword is cleared. */
void func_ovlgap_80408080(GridVertex* av, GridVertex* bv, GridVertex* ov) {
    s32 tmp[2];
    s32 uninit;

    tmp[0] = uninit;
    ov->x = (av->x + bv->x) / 2;
    ov->height = (av->height + bv->height) / 2;
    ov->z = (av->z + bv->z) / 2;
    ov->vertex_flags = 0;
    ov->texture_s = (av->texture_s + bv->texture_s) / 2;
    ov->texture_t = (av->texture_t + bv->texture_t) / 2;
    ov->color_r = (av->color_r + bv->color_r) / 2;
    ov->color_g = (av->color_g + bv->color_g) / 2;
    ov->color_b = (av->color_b + bv->color_b) / 2;
    ov->color_a = (av->color_a + bv->color_a) / 2;
}

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_80408170);

INCLUDE_ASM("asm/nonmatchings/overlay_gap/func_ovlgap_80400000", func_ovlgap_804083E0);

/* 8-byte frame with `sw $v0,0(sp)`: static-chain home of the nested-function original, as in func_ovl7_802068B4. */
void func_ovlgap_8040BFC8(void) {
    char file_name[0x80];
    u8 *buffer;
    s32 pos;
    s32 x;
    s32 z;

    u16 func_ovlgap_8040BF64(u32 packed_xz) {
        u32 gx;
        u32 gz;
        u32 block;
        u32 sx;
        u32 sz;
        u16* base;

        gx = packed_xz & 0xFF;
        gz = (packed_xz >> 8) & 0x1FF;
        block = ((gx >> 5) + (gz >> 5) * 8) * sizeof(TerrainTileCache);
        sx = gx & 0x1F;
        sz = gz & 0x1F;
        /* Preserve the retail member-rooted scalar address and 36-halfword stride. */
        base = &g_terrain_tile_cache[0].colors[1][1];
        return *(u16*)((u8*)(base + (sx + sz * 36)) + block);
    }

    mark_scenery_collision_cells();
    buffer = heap3_alloc(0x60036);
    for (pos = 0; pos != 0x36; pos++) {
        buffer[pos] = D_ovlgap_8040C6D4[pos];
    }
    for (z = 0x1FF; z != -1; z--) {
        for (x = 0; x != 0x100; x++) {
            u32 packed_xz = (z << 8) + x;

            buffer[pos] = ((u32)func_ovlgap_8040BF64(packed_xz) << 2) & 0xF8;
            pos++;
            buffer[pos] = (func_ovlgap_8040BF64(packed_xz) >> 3) & 0xF8;
            pos++;
            buffer[pos] = (func_ovlgap_8040BF64(packed_xz) >> 8) & 0xF8;
            pos++;
        }
    }
    sprintf(file_name, D_ovlgap_8040CAC0, D_ovlgap_8040C6D0);
    validate_heap_string(file_name, 0x60036);
    heap3_free(&buffer);
    D_ovlgap_8040C6D0 += 1;
}

void func_ovlgap_8040C124(void) {
  u8 *buffer;
  volatile u16 *framebuffer;
  s32 position;
  s32 row;
  s32 row_end;
  s32 row_offset;
  s32 column;
  s32 column_end;

  framebuffer = (volatile u16*)((u32)nuGfxCfb_ptr + 0x20000000);
  buffer = heap3_alloc(0x38436);
  for (position = 0; position != 0x36; position++) {
    buffer[position] = D_ovlgap_8040C710[position];
  }
  row = 0xEF;
  column_end = 0x140;
  row_end = -1;
  row_offset = 0x12AC0;
  do {
    column = 0;
    do {
      buffer[position] = (u8)(framebuffer[row_offset + column] * 4) & 0xF8;
      position++;
      buffer[position] = (framebuffer[row_offset + column] >> 3) & 0xF8;
      position++;
      buffer[position] = (framebuffer[row_offset + column] >> 11) << 3;
      position++;
      column++;
    } while (column != column_end);
    row--;
    row_offset -= 0x140;
  } while (row != row_end);
  sprintf(D_80105118, D_ovlgap_8040CAE4, D_ovlgap_8040C70C, row_offset);
  validate_heap_string(D_80105118, 0x38436);
  heap3_free(&buffer);
  D_ovlgap_8040C70C++;
}

void func_ovlgap_8040C260(s32 file_index) {
    volatile u16 *framebuffer;
    u8 *bitmap;
    u8 *output;
    s32 position;
    s32 column;
    s32 source_row;
    s32 row;
    s32 column_end;
    s32 row_end;
    s32 pixel_index;

    bitmap = (u8 *)g_terrain_tile_cache;
    framebuffer = (volatile u16 *)((u32)nuGfxCfb_ptr + 0x20000000);
    for (position = 0; position != 0x36; position++) {
        bitmap[position] = D_ovlgap_8040C748[position];
    }
    row = 0x7F;
    column_end = 0x40;
    row_end = -1;
    source_row = 0x9EC0;
    for (; row != row_end; row--) {
        column = 0;
        output = (u8 *)((u32)position + (u32)bitmap);
        do {
            pixel_index = source_row + column;
            *output++ = ((u32)framebuffer[pixel_index] << 2) & 0xF8;
            position += 3;
            *output++ = (framebuffer[pixel_index] >> 3) & 0xF8;
            column++;
            *output++ = (framebuffer[pixel_index] >> 11) << 3;
        } while (column != column_end);
        source_row -= 0x140;
    }
    sprintf(D_80105118, D_ovlgap_8040CB04, file_index, column);
    validate_heap_string(D_80105118, 0x38436);
}

void func_ovlgap_8040C360(s32 unused, Gfx **display_list_cursor) {
    Gfx *gfx = *display_list_cursor;

    guTranslate(&D_8040CC98, D_8010522C, D_80105230, D_80105234);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    gSPMatrix(gfx++, &D_40CC98, G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(gfx++, D_ovlgap_8040C780, 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 1, 3, 2, 0);
    gSPVertex(gfx++, &D_ovlgap_8040C780[4], 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 1, 3, 2, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);

    *display_list_cursor = gfx;
}
