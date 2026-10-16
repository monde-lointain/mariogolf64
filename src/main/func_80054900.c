#include "animation.h"
#include "session.h"

extern s32 flag_is_set(s32 flag);
extern s32 find_keyframe_offset_by_tag(s32 id, s32 track, s32 tag);
extern void func_80054BC0(void);
extern KeyframeTrack* func_80056060(s32 id);
extern void animation_step(s32 index);
extern void func_800543A4(void);
extern void func_800577DC(s32 index, void* arg1);

extern s16 D_800FBE48;
extern void func_800453E0(void* a, void* b, void* c);
extern s32 get_interpolated_terrain_height_wrapper(s32 x, s32 z);
extern s32 func_800544B4(s32 arg0, s32 arg1);
extern void func_800543DC(s32 arg0, s32 arg1);
extern s32 kSetMultiTLB(s32 a, s32 b, s32 c, s32 d, u32 phys);
extern s32 D_800C1DEC;
extern void func_800989EC(void* a, void* b, void* c);
extern f32 view_camera_eye_x;
extern f32 D_801B54F0;
extern f32 view_camera_eye_z;
extern f32 D_801B54F8;
extern f32 D_801B54FC;
extern f32 D_801B5500;
extern const u8 polychara_alive_msg[];
extern u8 polychara_assert_cond[];
extern u8 polychara_assert_file[];
extern void __assert(const char* cond, const char* file, s32 line);

extern s32 func_80054550(s32 id, s32 tag, f32* position);
extern void check_and_print_grid(char* text, s32 column, s32 row);
extern s32 D_800D04A0[][5];
extern char D_800D0608[], D_800D0618[], D_800D061C[];
extern f32 sqrtf(f32 value);

s32 func_80054938(s32 id, s32 mode, s32 direct, s32 variant) {
  f32 position[3];
  char line[0x80];
  CharacterState* cs;
  s32 column;
  s32 tag = 3;
  s32 result;
  f32 magnitude;
  s32 selected;

  /* This unused nested helper retains the ROM's static-chain spill. The TU's
   * fast-math profile emits the bare sqrt.s rather than a NaN fallback call. */
  f32 func_80054900(f32 x, f32 y, f32 z) {
    return sqrtf(x * x + y * y + z * z);
  }

  cs = get_character_state(id);
  column = 0;
  if (!direct) {
    selected = func_800544B4(id, mode);
  } else {
    selected = mode;
  }
  switch (selected) {
    case 0:
      result = 0x3600;
      tag = 3;
      column = (variant == 2) * 4;
      break;
    case 1:
      result = 0x2400;
      tag = 4;
      column = 1;
      break;
    case 2:
      result = 0x2400;
      tag = 5;
      column = 2;
      break;
    case 3:
      result = 0x1800;
      tag = 6;
      column = 3;
      break;
  }
  if (!flag_is_set(0x36) && cs != NULL) {
    s32 index = cs->resource_index;
    if (index >= 0) {
      index %= 36;
      if (index < 18) {
        /* Base-before-offset preserves the ROM's arithmetic register lifetimes.
         */
        u8* base = (u8*)D_800D04A0;
        s32 offset = index * 20;
        s32* row = (s32*)(base + offset);
        result = row[column];
        goto done;
      }
    }
  }
  if (func_80054550(id, tag, position) == 0) {
    sprintf(line, D_800D0608, position[0], position[1], position[2]);
    check_and_print_grid(line, 2, 13);
    magnitude = position[2] * 1228.8f;
    if (magnitude < 0.0f) {
      magnitude = -magnitude;
    }
    result = magnitude;
    osSyncPrintf(D_800D0618);
  } else {
    osSyncPrintf(D_800D061C);
  }
done:
  return result;
}

extern void* heap3_alloc(s32 size);
extern void heap3_free(void* ptr);
extern void func_80028110(void* data, s32 size);
extern void load_character_model(s32 id, s32 model, s32 arg2);
extern void func_800562F4(s32 id, f32 value);
extern void animation_play(s32 id, s32 animation);
extern void func_80056264(s32 id);
extern s32 clear_animation_slot(s32 id);
extern char D_800D0644[], D_800D06B4[], D_800D06B8[], D_800D06D0[],
    D_800D06D4[];

void func_80054BC0(void) {
  char line[0x100];
  u8* buf_pos;
  u8* buf_end;
  u8* buf_start;
  s32 model;
  s32 column;
  s32 animation;
  s32 kind;

  void func_80054B7C(u8 * s) {
    for (;;) {
      u8 c = *s++;
      if (c == 0) {
        break;
      }
      if (buf_pos < buf_end) {
        *buf_pos++ = c;
      }
    }
  }

  /* The nested definition allocates its captured pointers before this array. */
  s32 values[5];
  buf_start = heap3_alloc(0x8000);
  buf_pos = buf_start;
  buf_end = buf_start + 0x8000;
  if (buf_start == NULL) {
    __assert(polychara_assert_cond, polychara_assert_file, 0x5F5);
  }
  sprintf(line, D_800D0644);
  func_80054B7C((u8*)line);
  model = 0;
  do {
    load_character_model(0, model, 0);
    D_800C1DEC = D_801F43F0[0].resource_index;
    for (column = 0; column < 5; column++) {
      switch ((u32)column) {
        case 0:
          animation = 0;
          kind = 0;
          break;
        case 1:
          animation = 3;
          kind = 1;
          break;
        case 2:
          animation = 6;
          kind = 2;
          break;
        case 3:
          animation = 9;
          kind = 3;
          break;
        case 4:
          animation = 2;
          kind = 0;
          break;
      }
      func_800562F4(0, 0.0f);
      animation_play(0, animation);
      if (column < 4) {
        func_80056264(0);
      }
      animation_step(0);
      values[column] = func_80054938(0, kind, 1, 1);
    }
    osSyncPrintf(D_800D06B4);
    sprintf(line, D_800D06B8, values[0], values[1], values[2], values[3],
            values[4]);
    osSyncPrintf(line);
    func_80054B7C((u8*)line);
    clear_animation_slot(0);
    model++;
  } while (model < 18);
  sprintf(line, D_800D06D0);
  func_80054B7C((u8*)line);
  line[0] = 0x1A;
  line[1] = 0;
  func_80054B7C((u8*)line);
  sprintf(line, D_800D06D4);
  func_80028110(line, buf_pos - buf_start);
  heap3_free(&buf_start);
}

/* The four-byte-ended jump table and later constants share one rodata carve;
 * unbanked siblings retain their own generated diagnostic strings. */
const char D_800D06F4[0x18] = "TA %d : FRAME %d    ";

void func_80054E20(void) {
  if (flag_is_set(0x36)) {
    func_80054BC0();
  }
}

void func_80054E4C(s32 arg0, s32 arg1) {
  CharacterState* cs = get_character_state(arg0);
  if (cs != NULL) {
    s32 mode;
    D_800C1DEC = cs->resource_index;
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
    kSetMultiTLB(0, 0, 0x20, 0x2000000,
                  osVirtualToPhysical(cs->segment_data));
    func_800543DC(arg0, mode);
  }
}

/* KfEntry and KeyframeTrack layouts are shared in animation.h. */

/* collect_keyframe_events_at: CARRIED at 54/54, ONE BIT off (S260). The whole
 * body is byte-identical to the ROM -- same registers, same annulled `bnel` and
 * `beql`, same frame -- except the loop's back edge: the ROM branches to the
 * loop-top `lh v0,0(v1)` (offset 0xfff1) and the build branches PAST it to the
 * `bnel` (0xfff2), because the bottom sentinel load leaves the same value in
 * the same register and gcc redirects the edge over the redundant re-load.
 * Reproduce with (KfEntry as below, same shape as find_keyframe_offset_by_tag):
 *
 *   CharacterState* cs = get_character_state(id);
 *   count = 0;  // AFTER the call, to keep it caller-saved.
 *   if (cs == NULL) goto done;
 *   if (track >= cs->keyframe_track_count) goto done;
 *   KeyframeTrack* tracks = cs->model_resource->keyframe_tracks;
 *   KfEntry* e = tracks[track].entries;
 *   if (e == NULL || e->value == -1) goto done;
 *   q = e; p = out;
 *   ... read q->value/tag/reserved_03; advance q and test its -1 sentinel ...
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
  CharacterState* cs = get_character_state(id);
  KfEntry* e;
  s32 off;
  s32 sentinel;
  KeyframeTrack* tracks;

  if (cs == NULL) {
    goto done;
  }
  if (track >= cs->keyframe_track_count) {
    goto done;
  }
  tracks = cs->model_resource->keyframe_tracks;
  off = track * sizeof(KeyframeTrack);
  e = *(KfEntry**)(off + (u32)&tracks[0].entries);
  if (e == NULL) {
    goto done;
  }
  if (e->value == result) {
    goto done;
  }
  sentinel = -1;

loop:
  if (e->tag != tag) {
    goto advance;
  }
  result = e->value - *(s16*)(off + (u32)&cs->model_resource->keyframe_tracks[0].start_frame);
  goto done;
advance:
  e++;
  if (e->value != sentinel) {
    goto loop;
  }
done:
  return result;
}

s32 func_800550B8(s32 arg0) { return find_keyframe_offset_by_tag(arg0, 2, 6); }

/* One entry of a character model's texture table: an RGBA16 image plus the
 * texel dimensions the tile has to be re-declared with each frame. */

/* One step of a face's texture animation: the frame count at which it becomes
 * current, plus the texture and the s/t triple to bind from there on. The walk
 * takes the last keyframe whose threshold the slot's frame counter has reached,
 * so the list is scanned in full rather than stopped at the first hit. */

/* One binding of a model face to an animation slot. `slot` selects the entry of
 * the character's slot table that drives it; the keyframe list is what the
 * per-frame path steps through, and is unused on the reset path. */

/* One entry of the character's animation play order: which slot to advance and
 * how many frames it runs for. A negative slot ends the list. */

/* Each texture slot is 8 bytes and there are 0x20 per character. */
#define TEXTURE_ANIM_SLOT_COUNT 0x20

/* A model face whose texture is animated: the segmented addresses of its three
 * vertices and of its own display list, the slots that drive it, the index of
 * the texture to bind, and the s/t pair to write into each of the three
 * vertices. */

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
  CharacterState* cs;
  TexturedFace* face;
  TextureDesc* texture;
  Gfx* gfx;
  s32 faceIndex;
  s32 stepIndex;

  faceIndex = 0;
  /* `cs` has to live in the frame. The walk re-reads the character state at its
   * bottom, and only a stack-resident pointer keeps loop.c from hoisting that
   * re-read out past the display-list stores. */
  (void)&cs;
  cs = get_character_state(id);
  if (cs == NULL) {
    return;
  }
  kSetMultiTLB(0, 0, 0x20, 0x2000000, osVirtualToPhysical(cs->segment_data));

  /* The face list is NULL-terminated, so the fetch belongs in the condition: it
   * is then the loop test that gcc rotates to the bottom of the body, which is
   * what keeps the character-state re-read inside the walk. */
  while ((face = cs->model_resource->textured_face_table[faceIndex]) != NULL) {
    texture = cs->model_resource->texture_table;
    if (face->texture_index >= 0) {
      texture += face->texture_index;
    }
    gfx = (Gfx*)(((face->display_list + 0x10) & SEGMENT_OFFSET_MASK) +
                 cs->segment_bases[cs->segment_index]);
    gDPLoadTextureBlock(gfx++, (u32)texture->image & ~7, G_IM_FMT_RGBA,
                        G_IM_SIZ_16b, texture->width, texture->height, 0,
                        G_TX_CLAMP, G_TX_CLAMP, FACE_TEXTURE_MASK,
                        FACE_TEXTURE_MASK, G_TX_NOLOD, G_TX_NOLOD);

    for (stepIndex = 0; stepIndex < FACE_VERTEX_COUNT; stepIndex++) {
      Vtx* vertex = (Vtx*)(((stepIndex * sizeof(Vtx) + face->vertices) &
                            SEGMENT_OFFSET_MASK) +
                           cs->segment_bases[cs->segment_index]);
      vertex->v.tc[0] = face->tex_s[stepIndex];
      vertex->v.tc[1] = face->tex_t[stepIndex];
    }
    faceIndex++;
  }
}

/* Advances every animated texture on character `id` by one frame. Each face
 * binding names a slot; the slot's frame counter selects the keyframe whose
 * texture and s/t triple get patched into the face's own display list at
 * `dl + 0x10`, the same seven-command block `update_vertex_texture_coords`
 * writes. The tail pass then steps each running slot's counter and retires it
 * to state 3 once it reaches the length its play-order entry declares. */
void update_vertex_texture_coords_per_frame(s32 id) {
  f32 unused[16];
  CharacterState* cs;
  TexturedFace* face;
  TextureDesc* texture;
  FaceTexAnim* anim;
  Gfx* gfx;
  s32 faceIndex;
  s32 animIndex;
  s32 keyframeIndex;
  /* One counter serves the vertex loop and the slot-advance loop: their live
   * ranges do not overlap, and keeping them one pseudo is what lets the tail
   * loop's init stay in the join block instead of being duplicated onto the
   * entry guard's skip path. */
  s32 stepIndex;
  s32 frame;
  /* Separate from `frame`: this one lives only across the slot-advance loop,
   * carrying the counter on the running path and the state on the winding-down
   * one. Folding the two together lifts `frame` above the segment mask in
   * global.c's priority order and rotates five registers. */
  s32 counter;
  /* A byte-wide copy of the state: read unsigned so gcc cannot prove it equal
   * to the signed `state`, which keeps both live and lets the copy fill the
   * dispatch's delay slot instead of dying at the compare. */
  u8 hold;
  s32 slotIndex;
  s32 state;

  faceIndex = 0;
  (void)&cs;
  cs = get_character_state(id);
  if (cs == NULL) {
    return;
  }
  {
    f32 unused2[31];
    (void)unused2;
  }
  kSetMultiTLB(0, 0, 0x20, 0x2000000, osVirtualToPhysical(cs->segment_data));

  while ((face = cs->model_resource->textured_face_table[faceIndex]) != NULL) {
    for (animIndex = 0; animIndex < face->animation_count; animIndex++) {
      /* Keep the slot displacement in the load, not the address root. */
      CharacterState* slot_view;
      s8* bound_state;
      s32* bound_frame;
      anim = face->animations + animIndex;
      slot_view = (CharacterState*)((u8*)cs + anim->slot_index * sizeof(TextureAnimSlot));
      bound_state = (s8*)&slot_view->texture_slots[0].state;
      bound_frame = &slot_view->texture_slots[0].frame;
      if (*bound_state == 0) {
        continue;
      }
      frame = *bound_frame;

      for (keyframeIndex = 0; keyframeIndex < anim->keyframe_count;
           keyframeIndex++) {
        TexKeyframe* keyframe = anim->keyframes + keyframeIndex;
        if (frame < keyframe->threshold) {
          continue;
        }

        gfx = (Gfx*)(((face->display_list + 0x10) & SEGMENT_OFFSET_MASK) +
                     cs->segment_bases[cs->segment_index]);
        texture = cs->model_resource->texture_table + keyframe->texture_index;
        gDPLoadTextureBlock(gfx++, (u32)texture->image & ~7, G_IM_FMT_RGBA,
                            G_IM_SIZ_16b, texture->width, texture->height, 0,
                            G_TX_CLAMP, G_TX_CLAMP, FACE_TEXTURE_MASK,
                            FACE_TEXTURE_MASK, G_TX_NOLOD, G_TX_NOLOD);

        for (stepIndex = 0; stepIndex < FACE_VERTEX_COUNT; stepIndex++) {
          Vtx* vertex = (Vtx*)(((stepIndex * sizeof(Vtx) + face->vertices) &
                                SEGMENT_OFFSET_MASK) +
                               cs->segment_bases[cs->segment_index]);
          vertex->v.tc[0] = keyframe->tex_s[stepIndex];
          vertex->v.tc[1] = keyframe->tex_t[stepIndex];
        }
      }
    }
    faceIndex++;
  }
  for (stepIndex = 0; stepIndex < TEXTURE_ANIM_SLOT_COUNT; stepIndex++) {
    CharacterState* slot_view;
    s8* slot_state;
    s32* slot_frame;
    slotIndex = cs->model_resource->texture_animation_order[stepIndex]
                    .slot_index;
    if (slotIndex < 0) {
      break;
    }
    slot_view = (CharacterState*)((u8*)cs + slotIndex * sizeof(TextureAnimSlot));
    slot_state = (s8*)&slot_view->texture_slots[0].state;
    slot_frame = &slot_view->texture_slots[0].frame;
    state = *slot_state;
    hold = *(u8*)slot_state;
    if (state >= 4) {
      counter = *slot_frame + 1;
      *slot_frame = counter;
      if (counter < cs->model_resource->texture_animation_order[stepIndex]
                        .frame_length) {
        continue;
      }
      *slot_state = 3;
      /* The state store above aliases the frame-resident `cs`, so both arms
       * re-read it; spelling the base out re-derives it rather than reusing the
       * pointer the loop head already built. */
      if (slotIndex == 0) {
        s32* first_frame = &cs->texture_slots[0].frame;
        *first_frame = 0;
      } else {
        CharacterState* reslot_view =
            (CharacterState*)((u8*)cs + slotIndex * sizeof(TextureAnimSlot));
        s32* reslot_frame = &reslot_view->texture_slots[0].frame;
        *reslot_frame =
            cs->model_resource->texture_animation_order[stepIndex]
                .frame_length;
      }
    } else if (state != 0) {
      *slot_state = hold - 1;
    }
  }
}

/* The non-void fall-through keeps the guard delay slot empty; callers ignore
 * the unspecified result. */
s32 func_80055738(s32 id, s32 slot, s32 frame) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    cs->texture_slots[slot].frame = frame;
    cs->texture_slots[slot].state = 3;
  }
}

void func_80055788(s32 id) {
  CharacterState* base = get_character_state(id);
  s32 i = 0x1F;
  /* This is an address projection, not another CharacterState record. */
  CharacterState* p = (CharacterState*)((u8*)base + 31 * sizeof(TextureAnimSlot));
  for (; i >= 0; i--) {
    p->texture_slots[0].state = 0;
    p = (CharacterState*)((u8*)p - sizeof(TextureAnimSlot));
  }
}

void activate_texture_anim_slot(s32 id, s32 slot) {
  CharacterState* cs = get_character_state(id);
  if (slot >= 0x20) {
    __assert((const char*)polychara_assert_cond,
             (const char*)polychara_assert_file, 0x7A4);
  }
  if (cs != NULL) {
    u8 val = 4;
    cs->texture_slots[slot].state = val;
    cs->texture_slots[slot].frame = 0;
  }
}

/* Rebinds every face driven by animation slot `slot` back to that face's own
 * default texture and s/t pair, undoing whatever keyframe the per-frame walk
 * last wrote. Shares the seven-command patch at `dl + 0x10` and the segmented
 * address resolve with `update_vertex_texture_coords`. */
void reset_face_textures_for_anim_slot(s32 id, s32 slot) {
  f32 unused[16];
  CharacterState* cs;
  TexturedFace* face;
  TextureDesc* texture;
  Gfx* gfx;
  s32 faceIndex;
  s32 animIndex;
  s32 stepIndex;

  faceIndex = 0;
  (void)&cs;
  cs = get_character_state(id);
  if (cs == NULL) {
    return;
  }
  kSetMultiTLB(0, 0, 0x20, 0x2000000, osVirtualToPhysical(cs->segment_data));

  while ((face = cs->model_resource->textured_face_table[faceIndex]) != NULL) {
    texture = cs->model_resource->texture_table + face->texture_index;

    for (animIndex = 0; animIndex < face->animation_count; animIndex++) {
      FaceTexAnim* anim = face->animations + animIndex;
      if (anim->slot_index != slot) {
        continue;
      }

      gfx = (Gfx*)(((face->display_list + 0x10) & SEGMENT_OFFSET_MASK) +
                   cs->segment_bases[cs->segment_index]);
      gDPLoadTextureBlock(gfx++, (u32)texture->image & ~7, G_IM_FMT_RGBA,
                          G_IM_SIZ_16b, texture->width, texture->height, 0,
                          G_TX_CLAMP, G_TX_CLAMP, FACE_TEXTURE_MASK,
                          FACE_TEXTURE_MASK, G_TX_NOLOD, G_TX_NOLOD);

      for (stepIndex = 0; stepIndex < FACE_VERTEX_COUNT; stepIndex++) {
        Vtx* vertex = (Vtx*)(((stepIndex * sizeof(Vtx) + face->vertices) &
                              SEGMENT_OFFSET_MASK) +
                             cs->segment_bases[cs->segment_index]);
        vertex->v.tc[0] = face->tex_s[stepIndex];
        vertex->v.tc[1] = face->tex_t[stepIndex];
      }
    }
    faceIndex++;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", load_character_model);

s32 lookup_animation_by_id(s32 id, s32 target) {
  CharacterState* cs = get_character_state(id);
  s32 result = 0;
  if (cs != NULL) {
    KeyframeTrack* e = cs->model_resource->keyframe_tracks;
    s32 idx = 0;
    while (e->entries != NULL) {
      if (e->animation_id == target) {
        result = idx;
        goto done;
      }
      e++;
      idx++;
    }
  }
done:
  return result;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80056060);

typedef struct {
  s16 unknown_00;
  u8 unknown_02[0x1E];
  s16 unknown_20;
  s16 unknown_22;
  s16 unknown_24;
  s16 unknown_26;
  s16 unknown_28;
  s16 unknown_2A;
  s16 unknown_2C;
  s16 unknown_2E;
  u8 unknown_30[4];
} AnimationPartState; /* 0x34 */

void animation_play(s32 id, s32 animation) {
  CharacterState* cs = get_character_state(id);
  KeyframeTrack* tracks;
  s32 index;
  AnimationPartState* parts;
  if (cs != NULL) {
    kSetMultiTLB(0, 0, 0x20, 0x2000000, osVirtualToPhysical(cs->segment_data));
    index = lookup_animation_by_id(id, animation);
    cs->keyframe_track_index = index;
    tracks = cs->model_resource->keyframe_tracks;
    if (tracks[index].entries == NULL) {
      cs->keyframe_track_index = 0;
    }
    cs->current_frame =
        (f32)(cs->start_frame =
                  cs->model_resource->keyframe_tracks[cs->keyframe_track_index]
                      .start_frame);
    cs->end_frame =
        cs->start_frame +
        cs->model_resource->keyframe_tracks[cs->keyframe_track_index]
            .frame_span;
    /* The model's part-list pointer is read-only; the pointed-to state is not.
     */
    parts = *(AnimationPartState* const*)cs->model_resource->unknown_1C;
    while (parts->unknown_00 != -1) {
      parts->unknown_22 = -1;
      parts->unknown_20 = -1;
      parts->unknown_28 = -1;
      parts->unknown_26 = -1;
      parts->unknown_2E = -1;
      parts->unknown_2C = -1;
      parts++;
    }
  }
}

void func_80056238(s32 id, u8 value) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    cs->auto_slot_zero_enabled = value;
  }
}

void func_80056264(s32 id) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    cs->current_frame = (f32)cs->end_frame;
  }
}

void seek_current_frame_by(s32 id, s32 delta) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    f32 newval = cs->current_frame + (f32)delta;
    f32 limit = (f32)cs->end_frame;
    cs->current_frame = newval;
    if (limit <= newval) {
      cs->current_frame = limit;
    }
  }
}

void func_800562F4(s32 id, f32 value) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    cs->frame_step = value;
  }
}

void func_80056324(s32 id, u8 value) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    cs->loop_enabled = value;
  }
}

void func_80056350(s32 id, u8 value) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    cs->next_animation_id = value;
  }
}

void func_8005637C(s32 id, u8 value) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    cs->unknown_60 = value;
  }
}

void set_anim_flag_82(s32 id, u8 value) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    cs->unknown_82 = value;
  }
}

void set_anim_speed_byte(s32 id, u8 value) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    cs->unknown_61 = value;
  }
}

s32 func_80056400(s32 id) {
  CharacterState* cs = get_character_state(id);
  s32 result = -1;
  if (cs != NULL) {
    result = cs->end_frame;
  }
  return result;
}

s32 func_8005642C(s32 id) {
  KeyframeTrack* p = func_80056060(id);
  s32 result;
  if (p == NULL) {
    result = -1;
  } else {
    result = p->start_frame + p->frame_span;
  }
  return result;
}

s32 func_80056464(s32 id) {
  KeyframeTrack* p = func_80056060(id);
  s32 result;
  if (p == NULL) {
    result = -1;
  } else {
    result = p->start_frame;
  }
  return result;
}

s32 func_80056494(s32 id) {
  KeyframeTrack* p = func_80056060(id);
  s32 result;
  if (p == NULL) {
    result = -1;
  } else {
    result = p->frame_span;
  }
  return result;
}

void set_extra_flag_0(s32 id, u8 value) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    cs->unknown_188[0] = value;
  }
}

/* The non-void fall-through keeps the guard delay slot empty; callers ignore
 * the unspecified result. */
s32 func_800564F0(s32 id) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    cs->unknown_188[1] = 1;
  }
}

s32 func_80056520(s32 id) {
  CharacterState* cs = get_character_state(id);
  s32 result = -1;
  if (cs != NULL) {
    result = (s32)cs->current_frame;
  }
  return result;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", animation_step);
const u32 D_800D07E8 = 0x3FF00000;
const u32 D_800D07EC = 0;

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_800568CC);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", animation_step_no_rotate);

void animation_tick_all_players(void) {
  s32 i;
  for (i = 0; i < 4; i++) {
    animation_step(i);
  }
}

s32 func_800577D0(void) { return 0x23680; }
const u8 polychara_alive_msg[0x34] =
    "Polychara %d still alive. You can't change buffer.\n";

void func_800577DC(s32 index, void* arg1) {
  u32 aligned = ((u32)arg1 + 0xFFF) & 0xFFFFF000;
  if (D_801F43F0[index].resource_index != -1) {
    s32 j;
    for (j = 0; j < 10; j++) {
      osSyncPrintf((const char*)polychara_alive_msg, index);
    }
  } else {
    s32 i;
    D_801F43F0[index].segment_data = (void*)aligned;
    for (i = 0; i < 3; i++) {
      D_801F43F0[index].segment_bases[i] =
          (u32)D_801F43F0[index].segment_data + 0x20000 + i * 0x780;
    }
  }
}

void func_800578AC(void) {
  s32 i;
  func_800543A4();
  for (i = 0; i < 4; i++) {
    D_801F43F0[i].last_update_frame = -4;
    func_800577DC(i, (void*)0x8030F800);
  }
}

extern s32 render_buffer_index;
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
 * one, both indexed by render_buffer_index. */
void build_scenery_light_sets(void) {
  u8 amb[3];
  u8 col[3];
  s8 dir[3];
  f32 pitch;
  f32 yaw;

  /* Two separate range tests, not `9 <= x && x < 11`: fold_range_test merges
   * the adjacent sltis into one `(u32)(x - 9) < 2`, where the ROM keeps both.
   */
  if (hole_environment.unknown_24 < 11) {
    if (hole_environment.unknown_24 < 9) {
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

  pitch = (u16)hole_environment.scenery_wind_angles[1] * 9.58737992e-05f;
  yaw = (u16)hole_environment.scenery_wind_angles[0] * 9.58737992e-05f;
  dir[0] = -sinf(pitch) * cosf(yaw) * 127.0f;
  dir[1] = -cosf(pitch) * 127.0f;
  dir[2] = -sinf(pitch) * sinf(yaw) * 127.0f;

  /* Cast-to-struct, not a named `Lights2` local: with a local, gcc builds the
   * constructor into a temporary and then block-moves it into the local, so
   * each set costs one extra 40-byte copy and 0x28 of frame. Assigning the
   * constructor straight to the array element leaves the ROM's single copy. */
  D_800C1EA0[render_buffer_index] = (Lights2)gdSPDefLights2(
      amb[0], amb[1], amb[2], col[0], col[1], col[2], dir[0], dir[1], dir[2],
      col[0], col[1], col[2], D_800C1F90, D_800C1F94, D_800C1F98);
  D_800C1E50[render_buffer_index] = (Lights2)gdSPDefLights2(
      100, 100, 100, 0x9B, 0x78, 0x78, dir[0], dir[1], dir[2], 0x96, 0x96, 0x96,
      D_800C1F90, D_800C1F94, D_800C1F98);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80057FFC);

void func_80058ACC(s32 id) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    s32 angle;
    func_800453E0(&cs->position.x, &cs->rotation.x, &cs->scale.x);
    angle = -D_800FBE48;
    cs->rotation.y = (f32)(u16)angle * 9.5873802e-05f;
  }
}

void func_80058B34(s32 id) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    f32* position_x = &cs->position.x;
    f32* position_y = &cs->position.y;
    f32* position_z = &cs->position.z;
    f32* heading = &cs->rotation.y;
    func_800989EC(&cs->position.x, &cs->rotation.x, &cs->scale.x);
    D_801B54F8 = *position_x * (1.0f / 1024.0f);
    D_801B54FC = (*position_y - 15360.0f) * (1.0f / 1024.0f);
    D_801B5500 = *position_z * (1.0f / 1024.0f);
    view_camera_eye_x =
        (*position_x + cosf(-*heading - 1.57079637f) * 61440.0f) *
        (1.0f / 1024.0f);
    D_801B54F0 = (*position_y - 15360.0f) * (1.0f / 1024.0f);
    view_camera_eye_z = (*position_z +
                         sinf(-*heading - 1.57079637f) * 61440.0f) *
                        (1.0f / 1024.0f);
  }
}

void func_80058C58(s32 id, s32* arg1, f32 arg2) {
  CharacterState* cs = get_character_state(id);
  if (cs != NULL) {
    s32 h;
    f32 t38;
    f32 t2;
    cs->position.x = (f32)arg1[0];
    t38 = *(volatile f32*)&cs->position.x;
    t2 = (f32)arg1[2];
    cs->position.z = t2;
    h = get_interpolated_terrain_height_wrapper((s32)t38, (s32)t2);
    cs->position.y = (f32)h;
    cs->rotation.x = 0.0f;
    cs->rotation.z = 0.0f;
    cs->rotation.y = -arg2;
    cs->scale.x = 1.0f;
    cs->scale.y = 1.0f;
    cs->scale.z = 1.0f;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80058D04);
