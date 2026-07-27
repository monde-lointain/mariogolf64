#include "common.h"

extern s32 flag_is_set(s32 flag);
extern s32 find_keyframe_offset_by_tag(s32 id, s32 track, s32 tag);
extern void func_80054BC0(void);
extern u8* get_character_state(s32 id);
extern void* func_80056060(s32 id);
extern void animation_step(s32 index);
extern void func_800543A4(void);
extern void func_800577DC(s32 index, void* arg1);

typedef struct {
  s32 unk_00;
  u8 unk_04[0x188];
} AnimSlot;
extern AnimSlot D_801F4424[];
extern s16 D_800FBE48;
extern void func_800453E0(void* a, void* b, void* c);
extern s32 get_interpolated_terrain_height_wrapper(s32 x, s32 z);
extern s32 func_800544B4(s32 arg0, s32 arg1);
extern void func_800543DC(s32 arg0, s32 arg1);
extern s32 func_8005342C(s32 a, s32 b, s32 c, s32 d, u32 phys);
extern s32 D_800C1DEC;
extern void func_800989EC(void* a, void* b, void* c);
extern f32 per_view_camera_state;
extern f32 D_801B54F0;
extern f32 D_801B54F4;
extern f32 D_801B54F8;
extern f32 D_801B54FC;
extern f32 D_801B5500;
extern u8 polychara_buf[];
extern u8 polychara_state[];
extern u8 polychara_seg_ptr[];
extern u8 polychara_alive_msg[];
extern u8 polychara_assert_cond[];
extern u8 polychara_assert_file[];
extern void __assert(const char* cond, const char* file, s32 line);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80054900);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80054938);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80054B7C);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80054BC0);

void func_80054E20(void) {
  if (flag_is_set(0x36)) {
    func_80054BC0();
  }
}

void func_80054E4C(s32 arg0, s32 arg1) {
  u8* cs = get_character_state(arg0);
  if (cs != NULL) {
    s32 mode;
    D_800C1DEC = *(s32*)(cs + 8);
    switch (func_800544B4(arg0, arg1)) {
      case 0:
        mode = 0;
        break;
      case 1:
        mode = 1;
        break;
      case 2:
        mode = 2;
        break;
      case 3:
        mode = 3;
        break;
      default:
        mode = -1;
        break;
    }
    func_8005342C(0, 0, 0x20, 0x2000000,
                  osVirtualToPhysical(*(void**)(cs + 4)));
    func_800543DC(arg0, mode);
  }
}

/* One keyframe of an animation track: a signed offset value plus the tag that
 * selects it. Entries are packed 4 bytes apart and the list ends at val == -1.
 */
typedef struct {
  s16 val;
  u8 tag;
  u8 pad;
} KfEntry;

/* collect_keyframe_events_at: CARRIED at 54/54, ONE BIT off (S260). The whole
 * body is byte-identical to the ROM -- same registers, same annulled `bnel` and
 * `beql`, same frame -- except the loop's back edge: the ROM branches to the
 * loop-top `lh v0,0(v1)` (offset 0xfff1) and the build branches PAST it to the
 * `bnel` (0xfff2), because the bottom sentinel load leaves the same value in
 * the same register and gcc redirects the edge over the redundant re-load.
 * Reproduce with (KfEntry as below, same shape as find_keyframe_offset_by_tag):
 *
 *   u8* cs = get_character_state(id); s32 count; KfEntry *e, *q; s32 off,
 *   sentinel, v, w; u8 *tracks, *p;
 *   count = 0;                       // AFTER the call: crossing it makes it
 * callee-saved if (cs == NULL) goto done; if (track >= *(s32*)(cs + 0x1C)) goto
 * done; tracks = *(u8**)(*(u8**)cs + 0x14); off = track * 12; e =
 * *(KfEntry**)(off + (u32)tracks); if (e == NULL) goto done; if (e->val == -1)
 * goto done;     // literal here, local `sentinel` in the loop sentinel = -1; q
 * = e; p = out;   // q = e keeps the ROM's `move v1,a0` loop: v = q->val; // v
 * and w must be SEPARATE locals if (v != want) goto advance; if (count >= 0x10)
 * goto advance; p[0] = q->tag; p[1] = q->pad; count++; p += 2; advance: q++; w
 * = q->val; if (w != sentinel) goto loop; done: return count;
 *
 * Tried for the back edge: raw `*(s16*)q` on either side (MEM_IN_STRUCT_P is
 * not part of the identity test), separate v/w locals, and reading the sentinel
 * as `q[1].val` BEFORE the increment -- the last one does fix the edge but
 * costs both annulled delay slots (the advance handler stops being one
 * instruction). The two loads have to differ AFTER register allocation, which
 * no source spelling reached. */
INCLUDE_ASM("asm/nonmatchings/main/func_80054900", collect_keyframe_events_at);

/* Search track `track` of character `id` for the first keyframe tagged `tag`
 * and return its offset relative to the track's base offset, or -1.
 *
 * The walk is a goto loop and the exits are a single `goto done`, both
 * load-bearing: loop.c never sees a goto loop, so it cannot peel the first
 * iteration and let CSE cache `(cs->0)->0x14` in a caller-saved register (that
 * cache is what blocked `result` from a callee-saved register, S213), and a
 * single `return result` lets reorg replicate the return copy into each guard's
 * delay slot instead of materialising -1 per exit. The advance is written as an
 * out-of-line handler so reorg folds `e++` into the annulled `bnel`, and the
 * loop's own -1 needs its own local because a goto loop de-hoists a literal. */
s32 find_keyframe_offset_by_tag(s32 id, s32 track, s32 tag) {
  s32 result = -1;
  u8* cs = get_character_state(id);
  KfEntry* e;
  s32 off;
  s32 sentinel;
  u8* tracks;

  if (cs == NULL) {
    goto done;
  }
  if (track >= *(s32*)(cs + 0x1C)) {
    goto done;
  }
  tracks = *(u8**)(*(u8**)cs + 0x14);
  off = track * 12;
  e = *(KfEntry**)(off + (u32)tracks);
  if (e == NULL) {
    goto done;
  }
  if (e->val == result) {
    goto done;
  }
  sentinel = -1;

loop:
  if (e->tag != tag) {
    goto advance;
  }
  result = e->val - *(s16*)(off + (u32) * (u8**)(*(u8**)cs + 0x14) + 4);
  goto done;
advance:
  e++;
  if (e->val != sentinel) {
    goto loop;
  }
done:
  return result;
}

s32 func_800550B8(s32 arg0) { return find_keyframe_offset_by_tag(arg0, 2, 6); }

/* One entry of a character model's texture table: an RGBA16 image plus the
 * texel dimensions the tile has to be re-declared with each frame. */
typedef struct {
  void* image;
  u8 width;
  u8 height;
  u8 pad06[2];
} TextureDesc;

/* A model face whose texture is animated: the segmented addresses of its three
 * vertices and of its own display list, the index of the texture to bind, and
 * the s/t pair to write into each of the three vertices. */
typedef struct {
  u32 verts;
  u32 dl;
  u8 pad08[6];
  s16 textureIndex;
  u16 texS[3];
  u16 texT[3];
} TexturedFace;

#define FACE_VERTEX_COUNT 3
#define SEGMENT_OFFSET_MASK 0xFFFFFF
/* Both tiles clamp with a 64-texel wrap mask in s and t. */
#define FACE_TEXTURE_MASK 6

/* Re-binds the animated texture of every face of character `id` and rewrites
 * that face's vertex texture coordinates. Each face owns a display list whose
 * first two commands are a fixed preamble, so the seven-command texture load is
 * patched in at `dl + 0x10`; both the list and the vertices are segmented
 * addresses resolved against the character's current segment table entry. */
void update_vertex_texture_coords(s32 id) {
  f32 unused[16];
  u8* cs;
  TexturedFace* face;
  TextureDesc* texture;
  Gfx* gfx;
  s32 faceIndex;
  s32 vertexIndex;

  faceIndex = 0;
  /* `cs` has to live in the frame. The walk re-reads the character state at its
   * bottom, and only a stack-resident pointer keeps loop.c from hoisting that
   * re-read out past the display-list stores. */
  (void)&cs;
  cs = get_character_state(id);
  if (cs == NULL) {
    return;
  }
  func_8005342C(0, 0, 0x20, 0x2000000, osVirtualToPhysical(*(void**)(cs + 4)));

  /* The face list is NULL-terminated, so the fetch belongs in the condition: it
   * is then the loop test that gcc rotates to the bottom of the body, which is
   * what keeps the character-state re-read inside the walk. */
  while ((face = (*(TexturedFace***)(*(u8**)cs + 0x24))[faceIndex]) != NULL) {
    texture = *(TextureDesc**)(*(u8**)cs + 0x28);
    if (face->textureIndex >= 0) {
      texture += face->textureIndex;
    }
    gfx = (Gfx*)(((face->dl + 0x10) & SEGMENT_OFFSET_MASK) +
                 ((u32*)(cs + 0x20))[*(s32*)(cs + 0x30)]);
    gDPLoadTextureBlock(gfx++, (u32)texture->image & ~7, G_IM_FMT_RGBA,
                        G_IM_SIZ_16b, texture->width, texture->height, 0,
                        G_TX_CLAMP, G_TX_CLAMP, FACE_TEXTURE_MASK,
                        FACE_TEXTURE_MASK, G_TX_NOLOD, G_TX_NOLOD);

    for (vertexIndex = 0; vertexIndex < FACE_VERTEX_COUNT; vertexIndex++) {
      Vtx* vertex = (Vtx*)(((vertexIndex * sizeof(Vtx) + face->verts) &
                            SEGMENT_OFFSET_MASK) +
                           ((u32*)(cs + 0x20))[*(s32*)(cs + 0x30)]);
      vertex->v.tc[0] = face->texS[vertexIndex];
      vertex->v.tc[1] = face->texT[vertexIndex];
    }
    faceIndex++;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900",
            update_vertex_texture_coords_per_frame);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80055738);

void func_80055788(s32 id) {
  u8* base = get_character_state(id);
  s32 i = 0x1F;
  u8* p = base + 0xF8;
  for (; i >= 0; i--) {
    p[0x8C] = 0;
    p -= 8;
  }
}

void activate_texture_anim_slot(s32 id, s32 slot) {
  u8* cs = get_character_state(id);
  if (slot >= 0x20) {
    __assert((const char*)polychara_assert_cond,
             (const char*)polychara_assert_file, 0x7A4);
  }
  if (cs != NULL) {
    u8 val = 4;
    u8* p = cs + slot * 8;
    *(u8*)(p + 0x8c) = val;
    *(s32*)(p + 0x88) = 0;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80055828);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", load_character_model);

s32 lookup_animation_by_id(s32 id, s32 target) {
  u8* cs = get_character_state(id);
  s32 result = 0;
  if (cs != NULL) {
    u8* e = *(u8**)(*(u8**)cs + 0x14);
    s32 idx = 0;
    while (*(s32*)e != 0) {
      if (*(u8*)(e + 0x8) == target) {
        result = idx;
        goto done;
      }
      e += 0xC;
      idx++;
    }
  }
done:
  return result;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80056060);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", animation_play);

void func_80056238(s32 id, u8 value) {
  u8* cs = get_character_state(id);
  if (cs != NULL) {
    cs[0x85] = value;
  }
}

void func_80056264(s32 id) {
  u8* cs = get_character_state(id);
  if (cs != NULL) {
    *(f32*)(cs + 0x10) = (f32) * (s32*)(cs + 0x18);
  }
}

void seek_current_frame_by(s32 id, s32 delta) {
  u8* cs = get_character_state(id);
  if (cs != NULL) {
    f32 newval = *(f32*)(cs + 0x10) + (f32)delta;
    f32 limit = (f32) * (s32*)(cs + 0x18);
    *(f32*)(cs + 0x10) = newval;
    if (limit <= newval) {
      *(f32*)(cs + 0x10) = limit;
    }
  }
}

void func_800562F4(s32 id, f32 value) {
  u8* cs = get_character_state(id);
  if (cs != NULL) {
    *(f32*)(cs + 0x5C) = value;
  }
}

void func_80056324(s32 id, u8 value) {
  u8* cs = get_character_state(id);
  if (cs != NULL) {
    cs[0x83] = value;
  }
}

void func_80056350(s32 id, u8 value) {
  u8* cs = get_character_state(id);
  if (cs != NULL) {
    cs[0x84] = value;
  }
}

void func_8005637C(s32 id, u8 value) {
  u8* cs = get_character_state(id);
  if (cs != NULL) {
    cs[0x60] = value;
  }
}

void set_anim_flag_82(s32 id, u8 value) {
  u8* cs = get_character_state(id);
  if (cs != NULL) {
    cs[0x82] = value;
  }
}

void set_anim_speed_byte(s32 id, u8 value) {
  u8* cs = get_character_state(id);
  if (cs != NULL) {
    cs[0x61] = value;
  }
}

s32 func_80056400(s32 id) {
  u8* cs = get_character_state(id);
  s32 result = -1;
  if (cs != NULL) {
    result = *(s32*)(cs + 0x18);
  }
  return result;
}

s32 func_8005642C(s32 id) {
  void* p = func_80056060(id);
  s32 result;
  if (p == NULL) {
    result = -1;
  } else {
    result = *(s16*)((u8*)p + 4) + *(s16*)((u8*)p + 6);
  }
  return result;
}

s32 func_80056464(s32 id) {
  void* p = func_80056060(id);
  s32 result;
  if (p == NULL) {
    result = -1;
  } else {
    result = *(s16*)((u8*)p + 4);
  }
  return result;
}

s32 func_80056494(s32 id) {
  void* p = func_80056060(id);
  s32 result;
  if (p == NULL) {
    result = -1;
  } else {
    result = *(s16*)((u8*)p + 6);
  }
  return result;
}

void set_extra_flag_0(s32 id, u8 value) {
  u8* cs = get_character_state(id);
  if (cs != NULL) {
    cs[0x188] = value;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_800564F0);

s32 func_80056520(s32 id) {
  u8* cs = get_character_state(id);
  s32 result = -1;
  if (cs != NULL) {
    result = (s32) * (f32*)(cs + 0x10);
  }
  return result;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", animation_step);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_800568CC);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", animation_step_no_rotate);

void animation_tick_all_players(void) {
  s32 i;
  for (i = 0; i < 4; i++) {
    animation_step(i);
  }
}

s32 func_800577D0(void) { return 0x23680; }

void func_800577DC(s32 index, void* arg1) {
  u32 aligned = ((u32)arg1 + 0xFFF) & 0xFFFFF000;
  s32 stride = index * 0x18C;
  if (*(s32*)(polychara_state + stride) != -1) {
    s32 j = 0;
    do {
      osSyncPrintf((const char*)polychara_alive_msg, index);
      j += 1;
    } while (j < 10);
  } else {
    s32 i;
    s32* p;
    *(u32*)(polychara_buf + stride) = aligned;
    i = 0;
    p = (s32*)(polychara_seg_ptr + stride);
    for (; i < 3; i++) {
      *p = *(s32*)(polychara_buf + stride) + 0x20000 + i * 0x780;
      p++;
    }
  }
}

void func_800578AC(void) {
  s32 i;
  func_800543A4();
  for (i = 0; i < 4; i++) {
    D_801F4424[i].unk_00 = -4;
    func_800577DC(i, (void*)0x8030F800);
  }
}

extern u32 D_800BA9FC;
extern s32 D_800B7780;
extern u16 g_scenery_wind_angle_a;
extern u16 g_scenery_wind_angle_b;
extern s32 D_800C1F90;
extern s32 D_800C1F94;
extern s32 D_800C1F98;
extern Lights2 D_800C1E50[];
extern Lights2 D_800C1EA0[];
extern f32 sinf(f32);
extern f32 cosf(f32);

/* Rebuilds the two scenery light sets for the current course from the wind
 * angles. Both angles are u16 binary angles, so 2*pi/65536 converts them to
 * radians; the direction vector is the unit vector of (yaw, pitch) scaled to
 * the s8 range. D_800C1EA0 takes the course-tinted set and D_800C1E50 a fixed
 * one, both indexed by D_800B7780. */
void build_scenery_light_sets(void) {
  u8 amb[3];
  u8 col[3];
  s8 dir[3];
  f32 pitch;
  f32 yaw;

  /* Two separate range tests, not `9 <= x && x < 11`: fold_range_test merges
   * the adjacent sltis into one `(u32)(x - 9) < 2`, where the ROM keeps both.
   */
  if (D_800BA9FC < 11) {
    if (D_800BA9FC < 9) {
      goto dull;
    }
    amb[0] = 0x78;
    amb[1] = 0x78;
    amb[2] = 0x78;
    col[0] = 0xC8;
    col[1] = 0x50;
    col[2] = 0x0A;
  } else {
  dull:
    /* amb[2] is set between amb[0] and amb[1] so both constants are live
     * before the first store; grouping the 0x82s together defers the 0x78
     * `li` past them and costs a reload. */
    amb[0] = 0x82;
    amb[2] = 0x78;
    amb[1] = 0x82;
    col[0] = 0x82;
    col[1] = 0x78;
    col[2] = 0x6E;
  }

  pitch = g_scenery_wind_angle_b * 9.58737992e-05f;
  yaw = g_scenery_wind_angle_a * 9.58737992e-05f;
  dir[0] = -sinf(pitch) * cosf(yaw) * 127.0f;
  dir[1] = -cosf(pitch) * 127.0f;
  dir[2] = -sinf(pitch) * sinf(yaw) * 127.0f;

  /* Cast-to-struct, not a named `Lights2` local: with a local, gcc builds the
   * constructor into a temporary and then block-moves it into the local, so
   * each set costs one extra 40-byte copy and 0x28 of frame. Assigning the
   * constructor straight to the array element leaves the ROM's single copy. */
  D_800C1EA0[D_800B7780] = (Lights2)gdSPDefLights2(
      amb[0], amb[1], amb[2], col[0], col[1], col[2], dir[0], dir[1], dir[2],
      col[0], col[1], col[2], D_800C1F90, D_800C1F94, D_800C1F98);
  D_800C1E50[D_800B7780] = (Lights2)gdSPDefLights2(
      100, 100, 100, 0x9B, 0x78, 0x78, dir[0], dir[1], dir[2], 0x96, 0x96, 0x96,
      D_800C1F90, D_800C1F94, D_800C1F98);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80057FFC);

void func_80058ACC(s32 id) {
  u8* cs = get_character_state(id);
  if (cs != NULL) {
    s32 angle;
    func_800453E0(cs + 0x38, cs + 0x44, cs + 0x50);
    angle = -D_800FBE48;
    *(f32*)(cs + 0x48) = (f32)(u16)angle * 9.5873802e-05f;
  }
}

void func_80058B34(s32 id) {
  u8* cs = get_character_state(id);
  if (cs != NULL) {
    func_800989EC(cs + 0x38, cs + 0x44, cs + 0x50);
    D_801B54F8 = *(f32*)(cs + 0x38) * (1.0f / 1024.0f);
    D_801B54FC = (*(f32*)(cs + 0x3C) - 15360.0f) * (1.0f / 1024.0f);
    D_801B5500 = *(f32*)(cs + 0x40) * (1.0f / 1024.0f);
    per_view_camera_state =
        (*(f32*)(cs + 0x38) +
         cosf(-*(f32*)(cs + 0x48) - 1.57079637f) * 61440.0f) *
        (1.0f / 1024.0f);
    D_801B54F0 = (*(f32*)(cs + 0x3C) - 15360.0f) * (1.0f / 1024.0f);
    D_801B54F4 = (*(f32*)(cs + 0x40) +
                  sinf(-*(f32*)(cs + 0x48) - 1.57079637f) * 61440.0f) *
                 (1.0f / 1024.0f);
  }
}

void func_80058C58(s32 id, s32* arg1, f32 arg2) {
  u8* cs = get_character_state(id);
  if (cs != NULL) {
    s32 h;
    f32 t38;
    f32 t2;
    *(f32*)(cs + 0x38) = (f32)arg1[0];
    t38 = *(volatile f32*)(cs + 0x38);
    t2 = (f32)arg1[2];
    *(f32*)(cs + 0x40) = t2;
    h = get_interpolated_terrain_height_wrapper((s32)t38, (s32)t2);
    *(f32*)(cs + 0x3C) = (f32)h;
    *(f32*)(cs + 0x44) = 0.0f;
    *(f32*)(cs + 0x4C) = 0.0f;
    *(f32*)(cs + 0x48) = -arg2;
    *(f32*)(cs + 0x50) = 1.0f;
    *(f32*)(cs + 0x54) = 1.0f;
    *(f32*)(cs + 0x58) = 1.0f;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80058D04);
