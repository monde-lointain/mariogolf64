#include "common.h"

#include "heap.h"

extern char D_800CA0E0[];
extern char D_800CA0F0[];
extern char D_800CA118[];
extern char D_800CA134[];
extern char D_800CA14C[];
extern char D_800CA164[];
extern HeapBlock* D_800B6568;
extern void* D_800B656C;
extern s8 D_800B6564;

extern void* heap3_alloc(s32 size);
extern void heap3_free(void** payload_ptr);

void func_800263B0(void);

/* Scenery collision cylinders: parallel global arrays, one record per 0x10
 * bytes. Each field carries a distinct symbol (collision_cylinders,
 * D_800FBEA2/A4/A6/AA/AC/AE); model each as its own 0x10-stride array so the
 * shared record index materialises each symbol with addend 0. */
typedef struct {
  /* 0x00 */ s16 val;
  /* 0x02 */ s8 pad[0xE];
} CylFieldS16; /* 0x10 */

typedef struct {
  /* 0x00 */ u16 val;
  /* 0x02 */ s8 pad[0xE];
} CylFieldU16; /* 0x10 */

typedef struct {
  /* 0x00 */ s8 val;
  /* 0x01 */ s8 pad[0xF];
} CylFieldS8; /* 0x10 */

typedef struct {
  /* 0x00 */ s32 depth;
  /* 0x04 */ s8 pad[0x84];
} SceneryTransform; /* 0x88 */

typedef struct {
  /* 0x00 */ Mtx translate;
  /* 0x40 */ Mtx scale;
  /* 0x80 */ s8 pad[8];
} SceneryMtxPair; /* 0x88 */

extern CylFieldS16 collision_cylinders[]; /* 0x800FBEA0 : x */
extern CylFieldS16 D_800FBEA2[];          /* y / stored terrain height */
extern CylFieldS16 D_800FBEA4[];          /* z */
extern CylFieldS8 D_800FBEA6[];           /* active flag */
extern CylFieldS16 D_800FBEAA[];          /* cull flag (stored) */
extern CylFieldU16 D_800FBEAC[];          /* scale x */
extern CylFieldU16 D_800FBEAE[];          /* scale y */
extern SceneryTransform g_scenery_transform_pool[]; /* 0x801EFFD0 */
extern SceneryMtxPair D_801EFFD8[];                 /* 0x801EFFD8 (pool + 8) */
extern u16 g_scenery_sort_indices[0x80];            /* 0x800B6570 */
extern s8 g_terrain_vtx_xform_mode;

extern s32 project_point_view_depth(f32 x, f32 y, f32 z);
extern s32 get_interpolated_terrain_height_wrapper(s32 x, s32 z);

/* Register the region [block, block+size) as slot-1 heap and mark slot-1 ready.
 * `end` is computed after both calls so the add lands in func_800263B0's delay
 * slot (the compiler saves `size` in a callee-saved reg across osSyncPrintf).
 */
void func_80026400(u8* block, s32 size) {
  void* end;

  osSyncPrintf(D_800CA0E0);
  func_800263B0();
  end = block + size;
  D_800B6568 = (HeapBlock*)block;
  D_800B656C = end;
  heap_add_region(1, (HeapBlock*)block, end);
  D_800B6564 = 1;
}

/* Re-register the previously recorded slot-1 region and mark slot-1 ready. */
void func_8002646C(void) {
  osSyncPrintf(D_800CA0F0);
  func_800263B0();
  heap_add_region(1, D_800B6568, D_800B656C);
  D_800B6564 = 1;
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
    if (D_800FBEA6[i].val < 0) {
      continue;
    }
    x = (f32)collision_cylinders[i].val;
    z = (f32)D_800FBEA4[i].val;
    if (g_terrain_vtx_xform_mode == 0 && (((x <= 256.0f) | (z <= 256.0f)) ||
                                          ((3840.0f <= x) | (7936.0f <= z)))) {
      D_800FBEAA[i].val = 1;
    } else {
      D_800FBEAA[i].val = 0;
    }
    h = get_interpolated_terrain_height_wrapper((s32)(x * 1024.0f),
                                                (s32)(z * 1024.0f));
    if (h < 0) {
      h += 0x3FF;
    }
    h >>= 10;
    hf = (f32)h;
    D_800FBEA2[i].val = (s16)hf;
    guTranslate(&D_801EFFD8[i].translate, x, hf, z);
    guScale(&D_801EFFD8[i].scale, (f32)D_800FBEAC[i].val,
            (f32)D_800FBEAE[i].val, 1.0f);
  }
}

/* Project every active cylinder to view depth (inactive ones get a far
 * sentinel) then insertion-sort the index permutation by descending depth. */
void project_sort_scenery_cylinders(void) {
  s32 i;
  s32 j;
  u16 key;

  for (i = 0; i != 0x80; i++) {
    if (D_800FBEA6[i].val < 0) {
      g_scenery_transform_pool[i].depth = 0x40000000;
    } else {
      g_scenery_transform_pool[i].depth = project_point_view_depth(
          (f32)collision_cylinders[i].val, (f32)D_800FBEA2[i].val,
          (f32)D_800FBEA4[i].val);
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

/* Duplicate `str` (length rounded up to a word, capped at 0xFF) into a fresh
 * slot-3 buffer with a 4-word header, then release both. Returns a status code:
 * 1 if `tag` is 0, 2 if `str` is empty, 3 if the rounded length is too large,
 * 0 on success. */
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

/* Sibling of func_80028110 with a read-back check: duplicate `str` into a
 * slot-3 buffer with a header marked 0xB, and if the stored length reads back
 * as zero clear `*out` and log. Returns 2 if `str` is empty, 3 if too large,
 * else 1. */
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
