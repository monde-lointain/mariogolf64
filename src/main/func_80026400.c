#include "common.h"

#include "heap.h"
#include "terrain.h"

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

/* Register the region [block, block+size) as slot-1 heap and mark slot-1 ready.
 * `end` is computed after both calls so the add lands in the reset call's delay
 * slot (the compiler saves `size` in a callee-saved reg across osSyncPrintf).
 */
void func_80026400(u8* block, s32 size) {
  void* end;

  osSyncPrintf(D_800CA0E0);
  reset_scenery_billboards();
  end = block + size;
  D_800B6568 = (HeapBlock*)block;
  D_800B656C = end;
  heap_add_region(1, (HeapBlock*)block, end);
  g_scenery_heap_ready = 1;
}

/* Re-register the previously recorded slot-1 region and mark slot-1 ready. */
void func_8002646C(void) {
  osSyncPrintf(D_800CA0F0);
  reset_scenery_billboards();
  heap_add_region(1, D_800B6568, D_800B656C);
  g_scenery_heap_ready = 1;
}

/* Rebuild the per-cylinder translate+scale matrix pair from the terrain and set
 * the visibility-cull flags; resets the sort-index identity permutation. */
void update_scenery_cylinder_transforms(void) {
  s32 i;
  f32 x;
  f32 z;
  f32 hf;
  s32 h;

  for (i = 0; i != 0x80; i++) {
    g_scenery_sort_indices[i] = i;
  }

  for (i = 0; i < 0x80; i++) {
    if (collision_cylinders[i].sprite_index < 0) {
      continue;
    }
    x = (f32)collision_cylinders[i].x;
    z = (f32)collision_cylinders[i].z;
    if (g_terrain_vtx_xform_mode == 0 && (((x <= 256.0f) | (z <= 256.0f)) ||
                                          ((3840.0f <= x) | (7936.0f <= z)))) {
      collision_cylinders[i].cull_state = 1;
    } else {
      collision_cylinders[i].cull_state = 0;
    }
    h = get_interpolated_terrain_height_wrapper((s32)(x * 1024.0f),
                                                (s32)(z * 1024.0f));
    if (h < 0) {
      h += 0x3FF;
    }
    h >>= 10;
    hf = (f32)h;
    collision_cylinders[i].terrain_y = (s16)hf;
    guTranslate(&g_scenery_transform_pool[i].translate, x, hf, z);
    guScale(&g_scenery_transform_pool[i].scale,
            (f32)collision_cylinders[i].radius,
            (f32)collision_cylinders[i].scale_y, 1.0f);
  }
}

/* Project every active cylinder to view depth (inactive ones get a far
 * sentinel) then insertion-sort the index permutation by descending depth. */
void project_sort_scenery_cylinders(void) {
  s32 i;
  s32 j;
  u16 key;

  for (i = 0; i != 0x80; i++) {
    if (collision_cylinders[i].sprite_index < 0) {
      g_scenery_transform_pool[i].depth = 0x40000000;
    } else {
      g_scenery_transform_pool[i].depth = project_point_view_depth(
          (f32)collision_cylinders[i].x, (f32)collision_cylinders[i].terrain_y,
          (f32)collision_cylinders[i].z);
    }
  }

  for (i = 1; i != 0x80; i++) {
    key = g_scenery_sort_indices[i];
    for (j = i - 1;
         j >= 0 && g_scenery_transform_pool[g_scenery_sort_indices[j]].depth <
                       g_scenery_transform_pool[key].depth;
         j--) {
      g_scenery_sort_indices[j + 1] = g_scenery_sort_indices[j];
    }
    g_scenery_sort_indices[j + 1] = key;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80026400", emit_scenery_billboard);

INCLUDE_ASM("asm/nonmatchings/main/func_80026400", draw_scenery_opaque_pass);

INCLUDE_ASM("asm/nonmatchings/main/func_80026400", draw_scenery_alpha_pass);

/* Copy the word-rounded `str` bytes into a slot-3 buffer and allocate a
 * separate 4-byte header. That header word is overwritten with 0xA, then the
 * rounded length, then `tag`. Returns: 1 if `tag` is 0, 2 if `str` is empty,
 * 3 if the rounded length is too large, 0 on success. */
s32 func_80028110(char* str, s32 tag) {
  s32 len;
  void* hdr;
  void* buf;

  if (tag == 0) {
    osSyncPrintf(D_800CA118);
    return 1;
  }
  len = strlen(str);
  if (len == 0) {
    osSyncPrintf(D_800CA134);
    return 2;
  }
  len = (len + 3) & ~3;
  if ((u32)len >= 0x100) {
    osSyncPrintf(D_800CA14C);
    return 3;
  }
  hdr = heap3_alloc(4);
  buf = heap3_alloc(len);
  memcpy(buf, str, len);
  *(s32*)hdr = 0xA;
  *(s32*)hdr = len;
  *(s32*)hdr = tag;
  heap3_free(&hdr);
  heap3_free(&buf);
  return 0;
}

/* Sibling of func_80028110: copies the word-rounded `str` bytes into a slot-3
 * buffer and allocates a separate 4-byte header. It writes 0xB, then
 * overwrites that word with the rounded length; if the stored length reads
 * back as zero, clears `*out` and logs. Returns 2 if `str` is empty, 3 if too
 * large, else 1. */
s32 func_80028204(char* str, s32* out) {
  s32 len;
  void* hdr;
  void* buf;

  len = strlen(str);
  if (len == 0) {
    osSyncPrintf(D_800CA134);
    return 2;
  }
  len = (len + 3) & ~3;
  if ((u32)len >= 0x100) {
    osSyncPrintf(D_800CA14C);
    return 3;
  }
  hdr = heap3_alloc(4);
  buf = heap3_alloc(len);
  memcpy(buf, str, len);
  *(s32*)hdr = 0xB;
  *(s32*)hdr = len;
  if (*(s32*)hdr == 0) {
    *out = 0;
    osSyncPrintf(D_800CA164);
  }
  heap3_free(&hdr);
  heap3_free(&buf);
  return 1;
}
