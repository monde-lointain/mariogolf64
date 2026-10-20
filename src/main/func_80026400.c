#include "common.h"

#include "heap.h"
#include "terrain.h"
#include "rom_asset.h"

extern char D_800CA0E0[];
extern char D_800CA0F0[];
extern char D_800CA118[];
extern char D_800CA134[];
extern char D_800CA14C[];
extern char D_800CA164[];
extern HeapBlock* D_800B6568;
extern void* D_800B656C;
extern s8 g_scenery_heap_ready;

extern void* heap3_alloc(s32 size);
extern void heap3_free(void** payload_ptr);

void reset_scenery_billboards(void);

typedef struct {
  /* 0x00 */ s32 depth;
  /* 0x04 */ s8 unknown_04[4];
  /* 0x08 */ Mtx translate;
  /* 0x48 */ Mtx scale;
} SceneryTransform; /* 0x88 */

extern SceneryCylinder collision_cylinders[];       /* 0x800FBEA0 */
extern SceneryTransform g_scenery_transform_pool[]; /* 0x801EFFD0 */
extern u16 g_scenery_sort_indices[0x80];            /* 0x800B6570 */
extern s8 g_terrain_vtx_xform_mode;

extern s32 project_point_view_depth(f32 x, f32 y, f32 z);
extern s32 get_interpolated_terrain_height_wrapper(s32 x, s32 z);


/* Register the region [heap_region_start, heap_region_end) as the scenery heap
 * and mark that heap as ready. */
void register_scenery_heap_region(u8* heap_region_start, s32 heap_region_size) {
  void* heap_region_end;

  osSyncPrintf(D_800CA0E0);
  reset_scenery_billboards();
  heap_region_end = heap_region_start + heap_region_size;
  D_800B6568 = (HeapBlock*)heap_region_start;
  D_800B656C = heap_region_end;
  heap_add_region(SCENERY_HEAP_SLOT, (HeapBlock*)heap_region_start,
                  heap_region_end);
  g_scenery_heap_ready = 1;
}

/* Re-register the previously recorded scenery heap region. */
void reregister_scenery_heap_region(void) {
  osSyncPrintf(D_800CA0F0);
  reset_scenery_billboards();
  heap_add_region(SCENERY_HEAP_SLOT, D_800B6568, D_800B656C);
  g_scenery_heap_ready = 1;
}

/* Rebuild the per-cylinder matrices and culling flags, then reset the sort
 * indices to an identity permutation. */
void update_scenery_cylinder_transforms(void) {
  s32 cylinder_index;

  for (cylinder_index = 0; cylinder_index != SCENERY_CYLINDER_COUNT;
       cylinder_index++) {
    g_scenery_sort_indices[cylinder_index] = cylinder_index;
  }

  for (cylinder_index = 0; cylinder_index < SCENERY_CYLINDER_COUNT;
       cylinder_index++) {
    f32 world_x;
    f32 world_z;
    f32 terrain_height;
    s32 terrain_height_fixed;

    if (collision_cylinders[cylinder_index].sprite_index < 0) {
      continue;
    }
    world_x = (f32)collision_cylinders[cylinder_index].x;
    world_z = (f32)collision_cylinders[cylinder_index].z;
    if (g_terrain_vtx_xform_mode == 0 &&
        (((world_x <= 256.0f) | (world_z <= 256.0f)) ||
         ((3840.0f <= world_x) | (7936.0f <= world_z)))) {
      collision_cylinders[cylinder_index].cull_state = 1;
    } else {
      collision_cylinders[cylinder_index].cull_state = 0;
    }

    terrain_height_fixed = get_interpolated_terrain_height_wrapper(
        (s32)(world_x * 1024.0f), (s32)(world_z * 1024.0f));
    if (terrain_height_fixed < 0) {
      terrain_height_fixed += 0x3FF;
    }
    terrain_height_fixed >>= 10;
    terrain_height = (f32)terrain_height_fixed;
    collision_cylinders[cylinder_index].terrain_y = (s16)terrain_height;
    guTranslate(&g_scenery_transform_pool[cylinder_index].translate, world_x,
                terrain_height, world_z);
    guScale(&g_scenery_transform_pool[cylinder_index].scale,
            (f32)collision_cylinders[cylinder_index].radius,
            (f32)collision_cylinders[cylinder_index].scale_y, 1.0f);
  }
}

/* Project active cylinders to view depth, then insertion-sort their indices by
 * descending depth. Inactive cylinders receive a far-depth sentinel. */
void project_sort_scenery_cylinders(void) {
  s32 cylinder_index;

  for (cylinder_index = 0; cylinder_index != SCENERY_CYLINDER_COUNT;
       cylinder_index++) {
    if (collision_cylinders[cylinder_index].sprite_index < 0) {
      g_scenery_transform_pool[cylinder_index].depth = 0x40000000;
    } else {
      g_scenery_transform_pool[cylinder_index].depth = project_point_view_depth(
          (f32)collision_cylinders[cylinder_index].x,
          (f32)collision_cylinders[cylinder_index].terrain_y,
          (f32)collision_cylinders[cylinder_index].z);
    }
  }

  for (cylinder_index = 1; cylinder_index != SCENERY_CYLINDER_COUNT;
       cylinder_index++) {
    u16 cylinder_key = g_scenery_sort_indices[cylinder_index];
    s32 insertion_position = cylinder_index - 1;

    while (insertion_position >= 0 &&
           g_scenery_transform_pool[
               g_scenery_sort_indices[insertion_position]]
                   .depth <
               g_scenery_transform_pool[cylinder_key].depth) {
      g_scenery_sort_indices[insertion_position + 1] =
          g_scenery_sort_indices[insertion_position];
      insertion_position--;
    }
    g_scenery_sort_indices[insertion_position + 1] = cylinder_key;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80026400", emit_scenery_billboard);

INCLUDE_ASM("asm/nonmatchings/main/func_80026400", draw_scenery_opaque_pass);

typedef union {
  u32 words[3];
  struct {
    /* 0x00 */ u8 pad_00[3];
    /* 0x03 */ u8 r;
    /* 0x04 */ u8 pad_04[3];
    /* 0x07 */ u8 g;
    /* 0x08 */ u8 pad_08[3];
    /* 0x0B */ u8 b;
  } bytes;
} SceneryColorRecord; /* 0x0C */

typedef struct {
  /* 0x00 */ s32 state;
  /* 0x04 */ u8 unknown_04[0x4C];
} SceneryAlphaSlot; /* 0x50 */

extern SceneryAlphaSlot D_801B5534[];
extern SceneryColorRecord D_800CA100;
extern SceneryColorRecord D_800CA10C;
extern s32 D_800FBE64;
extern s32 D_800FBE68;
extern s32 target_z;
extern s32 g_last_billboard_sprite_id;
extern Gfx* glistp;
extern s32 flag_is_set(s32 flag);
extern s32 func_80045AC0(void);
extern void emit_scenery_billboard(Gfx** gfx, SceneryCylinder* cylinder,
                                   SceneryTransform* transform, s32 sprite_index,
                                   s32 arg0);

void draw_scenery_alpha_pass(s32 arg0, s32 arg1) {
  Gfx* gfx;
  SceneryColorRecord unused_record;
  SceneryColorRecord fill_color;
  s32 i;
  s32 first;
  s32 threshold;

  gfx = glistp;
  unused_record = D_800CA100;
  fill_color = D_800CA10C;
  if (flag_is_set(0xE) == 0 && g_scenery_heap_ready != 0 &&
      func_80045AC0() == 0) {
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPPipeSync(gfx++);
    gDPSetEnvColor(gfx++, fill_color.bytes.r, fill_color.bytes.g,
                   fill_color.bytes.b, 0x20);
    gSPClipRatio(gfx++, FRUSTRATIO_1);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_FOG | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0xFE);
    gDPPipeSync(gfx++);
    gDPSetTextureLOD(gfx++, G_TL_TILE);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);

    if (D_801B5534[arg1].state == 1) {
      threshold = project_point_view_depth(
          (f32)D_800FBE64 * 0.0009765625f, (f32)D_800FBE68 * 0.0009765625f,
          (f32)target_z * 0.0009765625f);
      if (threshold > 0) {
        threshold = -0xFFFF;
      }
    } else {
      threshold = -0xFFFF;
    }
    g_last_billboard_sprite_id = -1;

    gDPSetPrimColor(gfx++, 0, 0, 0x3F, 0xB4, 0, 0xFF);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    first = 0;
    gDPPipeSync(gfx++);
    gDPSetRenderMode(
        gfx++,
        Z_CMP | Z_UPD | IM_RD | CVG_DST_FULL | ZMODE_XLU | FORCE_BL |
            G_RM_FOG_SHADE_A,
        Z_CMP | Z_UPD | IM_RD | CVG_DST_FULL | ZMODE_XLU | FORCE_BL |
            GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA));
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0xFF, 0xFF, 0xFF, 0x3F);

    for (i = 0x7F; i >= 0; i--) {
      s32 cylinder_index = g_scenery_sort_indices[i];

      if (collision_cylinders[cylinder_index].cull_state == 0) {
        s32 sprite_index = collision_cylinders[cylinder_index].sprite_index;

        if (sprite_index >= 0) {
          s32 depth = g_scenery_transform_pool[cylinder_index].depth;

          if (depth < -0x800) {
            continue;
          }
          if (depth >= 0) {
            continue;
          }
          {
            s32 alpha = -depth;

            if (alpha >= 0x100) {
              alpha = 0xFF;
            }
            if (alpha < 0x80) {
              alpha = 0x80;
            }
            if (depth < threshold) {
              alpha = 0xFF;
            }
            if (alpha != 0xFF) {
              if ((first == 0) && (alpha < 0xFF)) {
                first = 1;
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_FOG_SHADE_A, G_RM_ZB_XLU_SURF2);
              }
              if (alpha < 0xFF) {
                gDPSetPrimColor(gfx++, 0, 0, 0x3F, 0xB4, 0, alpha);
              }
              emit_scenery_billboard(
                  &gfx, &collision_cylinders[cylinder_index],
                  &g_scenery_transform_pool[cylinder_index], sprite_index,
                  arg0);
            }
          }
        }
      }
    }

    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPPipeSync(gfx++);
    glistp = gfx;
  }
}

/* Probe whether the rounded string copy fits in the slot-3 heap. Returns 1 for
 * a zero allocation tag, 2 for an empty string, 3 for a rounded length of at
 * least 256 bytes, and 0 after a successful temporary allocation and copy. */
s32 validate_heap_string(char* source_string, s32 allocation_tag) {
  s32 string_length;

  if (allocation_tag == 0) {
    osSyncPrintf(D_800CA118);
    return 1;
  }
  string_length = strlen(source_string);
  if (string_length == 0) {
    osSyncPrintf(D_800CA134);
    return 2;
  }
  string_length = (string_length + 3) & ~3;
  if ((u32)string_length >= 0x100) {
    osSyncPrintf(D_800CA14C);
    return 3;
  }
  {
    void* allocation_header = heap3_alloc(4);
    void* string_copy = heap3_alloc(string_length);

    memcpy(string_copy, source_string, string_length);
    *(s32*)allocation_header = 0xA;
    *(s32*)allocation_header = string_length;
    *(s32*)allocation_header = allocation_tag;
    heap3_free(&allocation_header);
    heap3_free(&string_copy);
  }
  return 0;
}

/* Probe a temporary rounded string copy and clear result_out only if the
 * allocated header reads back as zero. Returns 2 for empty, 3 for a rounded
 * length of at least 256 bytes, and 1 after a successful allocation and copy. */
s32 validate_heap_string_with_output(char* source_string, s32* result_out) {
  s32 string_length;

  string_length = strlen(source_string);
  if (string_length == 0) {
    osSyncPrintf(D_800CA134);
    return 2;
  }
  string_length = (string_length + 3) & ~3;
  if ((u32)string_length >= 0x100) {
    osSyncPrintf(D_800CA14C);
    return 3;
  }
  {
    void* allocation_header = heap3_alloc(4);
    void* string_copy = heap3_alloc(string_length);

    memcpy(string_copy, source_string, string_length);
    *(s32*)allocation_header = 0xB;
    *(s32*)allocation_header = string_length;
    if (*(s32*)allocation_header == 0) {
      *result_out = 0;
      osSyncPrintf(D_800CA164);
    }
    heap3_free(&allocation_header);
    heap3_free(&string_copy);
  }
  return 1;
}
