#include "common.h"

extern s32 D_800E4C5C;
extern s8 D_800E4CC8;
/* The best candidate position found so far: three consecutive words, and one
 * array rather than three scalars. The array form is load-bearing -- gcc's
 * aliasing rule lets a load through an s32* pointer float past a store to a
 * plain scalar global, but not past a store into an aggregate, which is what
 * keeps the load/store pairs interleaved here and in func_800989C4. */
extern s32 D_800E4C98[3];
extern u8 D_800C7464[];
extern s32 D_800E4C50;
extern s32 D_800E4C54;
extern s32 D_800E4C58;
extern u8 D_800E4C90;
extern u8 D_800E4C91;
extern s32 D_800C73A0;
extern s32 D_800C73E0;
extern u8* D_800C73F0[];
extern Light D_800C73B0[3];

extern s32 D_800E4CA4;

/* One scenery collision cylinder record. The sibling decomp in
 * src/main/func_80026400.c models these as parallel per-field arrays so each
 * field materialises its own symbol; here a single walking record is the right
 * view, because the ROM keeps one base register and reaches every field by
 * displacement. */
typedef struct {
  /* 0x00 */ s16 x;
  /* 0x02 */ s16 unk_02;
  /* 0x04 */ s16 z;
  /* 0x06 */ s8 unk_06[6];
  /* 0x0C */ u16 radius;
  /* 0x0E */ s8 unk_0E[2];
} CollisionCyl; /* 0x10 */

extern CollisionCyl collision_cylinders[];

typedef struct {
  /* 0x00 */ f32 x;
  /* 0x04 */ f32 y;
  /* 0x08 */ f32 z;
} Vec3f; /* 0x0C */

/* The camera record the shot-view builders fill in: the eye position followed
 * by the point it looks at. func_80095A68 takes both, so the two vectors are
 * one record rather than two globals. */
typedef struct {
  /* 0x00 */ Vec3f eye;
  /* 0x0C */ Vec3f at;
} GolfCamera; /* 0x18 */

extern s32 D_800BB020;
extern s32 D_801B60A0;

/* Two adjacent literal-pool doubles, both 1.57079632679489656 (pi/2), one per
 * using function. They stay extern refs into the shared main rodata blob rather
 * than source literals: spelling them out would emit a fresh pool entry into
 * this TU and force a rodata carve the still-asm siblings share. */
extern f64 D_800D1EC8;
extern f64 D_800D1ED0;

extern s32 func_80056494(s32 arg0, s32 arg1);
extern u8* get_character_state(s32 id);
extern s32 flag_is_set(s32 flag);
extern f32 func_80095A10(f32 near, f32 far, f32 t);
extern void func_80095A68(GolfCamera* cam, Vec3f* at, f32 arg2, s32 arg3);
extern void func_8009676C(GolfCamera* cam);
extern void func_80079358(s32 player, s32 arg1);
extern void func_8005483C(s32 player, s32 arg1, Vec3f* out);
extern void play_sound_effect(s32 sfx, s32 arg1, s32 arg2);
extern s32 get_interpolated_terrain_height_wrapper(s32 x, s32 z);
extern u32 calculate_hypotenuse_safe(s32 x, s32 y);
extern f32 atan2f(f32 dz, f32 dx);
extern void func_80078FA8(s32 player, s32 arg1, f32 angle);
extern void func_80078DC0(s32 arg0, s32 arg1);
extern void set_anim_flag_82(s32 id, u8 value);
extern f32 sqrtf(f32 x);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80095A10);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80095A68);

s32 func_80095C10(Gfx** gdlp, Lights3* src) {
  Gfx* gdl = *gdlp;
  if (D_800E4C5C != 0) {
    D_800C73B0[0] = src->l[0];
    D_800C73B0[1] = src->l[1];
    gSPNumLights(gdl++, NUMLIGHTS_3);
    gSPLight(gdl++, &D_800C73B0[0], 1);
    gSPLight(gdl++, &D_800C73B0[1], 2);
    gSPLight(gdl++, &D_800C73B0[2], 3);
    gSPLight(gdl++, (Light*)((u8*)D_800C73B0 - 8), 4);
    *gdlp = gdl;
    return 1;
  }
  *gdlp = gdl;
  return 0;
}

void func_80095D10(s32 arg0, s32 arg1, s32 arg2) {
  D_800E4C50 = arg1;
  D_800E4C5C = 0;
  D_800C73A0 = 0;
  D_800E4C90 = 0;
  D_800E4C91 = 0;
  D_800E4C54 = arg2;
  D_800E4C58 = func_80056494(arg0, arg2);
}

void func_80095D64(void) { D_800E4C5C = 0; }

void func_80095D70(s32 arg0, s32 arg1) {
  if (arg1 == -1) {
    return;
  }
  if (D_800C73A0 - 1 != arg0) {
    return;
  }
  play_sound_effect(arg1, D_800C73E0, 0x7F);
  D_800C73E0 = D_800C73E0 + 1;
  if (D_800C73E0 >= 8) {
    D_800C73E0 = 4;
  }
}

/* Cue the scripted sound effects for the current cutscene beat: the outer
 * switch selects the scene (D_800E4C50), the inner one the camera/shot variant
 * (D_800E4C54), and each arm queues one or more (frame, sound_id) pairs.
 *
 * The s32 return type is load-bearing and the value is never produced: a
 * non-void return blocks reorg.c's fall-through delay-slot steal, which is what
 * leaves the switch range-check branch's slot empty here. Declared void, gcc
 * fills that slot with the following sll and the body comes out one nop short.
 */
s32 update_cutscene_sound_cues(void) {
  if (D_800BB020 == 0x1E) {
    return;
  }

  switch (D_800E4C50) {
    case 0:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x8, 0x1AC);
          func_80095D70(0x3E, 0x3B);
          break;
        case 0xD:
          func_80095D70(0x20, 0x1B9);
          break;
        case 0xE:
          func_80095D70(0x1E, 0x1A3);
          break;
        case 0xF:
          func_80095D70(0x19, 0x1B7);
          break;
      }
      break;

    case 1:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x0, 0xCC);
          break;
        case 0xD:
          func_80095D70(0x0, 0xE1);
          func_80095D70(0xA, 0x3F);
          break;
        case 0xE:
          func_80095D70(0xA, 0xD6);
          break;
        case 0xF:
          func_80095D70(0x22, 0xE0);
          break;
      }
      break;

    case 2:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x2C, 0x15D);
          func_80095D70(0x5C, 0x3D);
          break;
        case 0xD:
          func_80095D70(0xC, 0x168);
          break;
        case 0xE:
          func_80095D70(0xC, 0x169);
          break;
        case 0xF:
          func_80095D70(0x8, 0x159);
          break;
      }
      break;

    case 3:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0xF, 0xC6);
          break;
        case 0xD:
          func_80095D70(0x9, 0xC9);
          func_80095D70(0x39, 0xCA);
          break;
        case 0xE:
          func_80095D70(0x20, 0xC9);
          break;
        case 0xF:
          func_80095D70(0x28, 0xCB);
          break;
      }
      break;

    case 4:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x2, 0x150);
          break;
        case 0xD:
          func_80095D70(0x0, 0x13F);
          break;
        case 0xE:
          func_80095D70(0x12, 0x14E);
          break;
        case 0xF:
          func_80095D70(0x7, 0x140);
          break;
      }
      break;

    case 5:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x0, 0x1A0);
          func_80095D70(0x38, 0x3B);
          break;
        case 0xD:
          func_80095D70(0x6, 0x194);
          func_80095D70(0x6, 0x3C);
          break;
        case 0xE:
          func_80095D70(0xC, 0x18E);
          func_80095D70(0x2A, 0x3B);
          break;
        case 0xF:
          func_80095D70(0x10, 0x19F);
          break;
      }
      break;

    case 6:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0xE, 0x1C4);
          break;
        case 0xD:
          func_80095D70(0x2, 0x1D0);
          break;
        case 0xE:
          func_80095D70(0x7, 0x1D1);
          break;
        case 0xF:
          func_80095D70(0x0, 0x1C1);
          break;
      }
      break;

    case 7:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x0, 0x1FF);
          func_80095D70(0x15, 0x3C);
          func_80095D70(0x58, 0x3B);
          break;
        case 0xD:
          func_80095D70(0x0, 0x1F2);
          break;
        case 0xE:
          func_80095D70(0x11, 0x1F3);
          break;
        case 0xF:
          func_80095D70(0x0, 0x1EE);
          func_80095D70(0x1C, 0x39);
          break;
      }
      break;

    case 8:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x14, 0x105);
          func_80095D70(0x37, 0x3B);
          break;
        case 0xD:
          func_80095D70(0x1B, 0xFF);
          break;
        case 0xE:
          func_80095D70(0x18, 0x103);
          break;
        case 0xF:
          func_80095D70(0x5, 0xF5);
          break;
      }
      break;

    case 9:
    case 13:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x0, 0x180);
          func_80095D70(0x0, 0x3C);
          func_80095D70(0x44, 0x38);
          break;
        case 0xD:
          func_80095D70(0x0, 0x173);
          break;
        case 0xE:
          func_80095D70(0xD, 0x182);
          break;
        case 0xF:
          func_80095D70(0x1A, 0x17F);
          break;
      }
      break;

    case 10:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x42, 0x200);
          func_80095D70(0x26, 0x3E);
          func_80095D70(0x26, 0x35);
          func_80095D70(0x3, 0x4F);
          func_80095D70(0xB, 0x4F);
          func_80095D70(0x12, 0x4F);
          func_80095D70(0x19, 0x4F);
          func_80095D70(0x20, 0x4F);
          break;
        case 0xD:
          func_80095D70(0x28, 0x208);
          func_80095D70(0x14, 0x4F);
          func_80095D70(0x26, 0x4F);
          break;
        case 0xE:
          func_80095D70(0x1C, 0x209);
          break;
        case 0xF:
          func_80095D70(0x0, 0x206);
          break;
      }
      break;

    case 11:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x7, 0x42);
          func_80095D70(0x1E, 0xE2);
          func_80095D70(0x23, 0x41);
          func_80095D70(0x28, 0x41);
          func_80095D70(0x2D, 0x41);
          func_80095D70(0x32, 0x41);
          func_80095D70(0x37, 0x41);
          func_80095D70(0x3C, 0x41);
          func_80095D70(0x46, 0xEA);
          break;
        case 0xD:
          func_80095D70(0x23, 0xEA);
          func_80095D70(0x16, 0x41);
          break;
        case 0xE:
          func_80095D70(0x23, 0xEA);
          func_80095D70(0x11, 0x41);
          func_80095D70(0x1F, 0x41);
          break;
        case 0xF:
          func_80095D70(0x0, 0xE8);
          break;
      }
      break;

    case 12:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x1D, 0x12F);
          break;
        case 0xD:
          func_80095D70(0x2A, 0x136);
          break;
        case 0xE:
          func_80095D70(0x0, 0x138);
          break;
        case 0xF:
          func_80095D70(0x0, 0x135);
          func_80095D70(0xA, 0x3A);
          func_80095D70(0x18, 0x3A);
          func_80095D70(0x21, 0x3A);
          break;
      }
      break;

    case 14:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x0, 0x123);
          func_80095D70(0x42, 0x12E);
          break;
        case 0xD:
          func_80095D70(0x0, 0x12E);
          break;
        case 0xE:
          func_80095D70(0x20, 0x11E);
          break;
        case 0xF:
          func_80095D70(0x19, 0x120);
          break;
      }
      break;

    case 15:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x0, 0x1DE);
          func_80095D70(0x6, 0x3C);
          break;
        case 0xD:
          func_80095D70(0x14, 0x1D7);
          break;
        case 0xE:
          func_80095D70(0x0, 0x1D4);
          break;
        case 0xF:
          func_80095D70(0x18, 0x1E8);
          break;
      }
      break;

    case 16:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0x19, 0xA7);
          break;
        case 0xD:
          func_80095D70(0x5, 0xB2);
          break;
        case 0xE:
          func_80095D70(0x26, 0xAE);
          break;
        case 0xF:
          func_80095D70(0x0, 0xB1);
          break;
      }
      break;

    case 17:
      switch (D_800E4C54) {
        case 0xC:
          func_80095D70(0xF, 0x11B);
          break;
        case 0xD:
          func_80095D70(0x6, 0x11A);
          break;
        case 0xE:
          func_80095D70(0x0, 0x109);
          break;
        case 0xF:
          func_80095D70(0x0, 0x10C);
          break;
      }
      break;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_8009676C);

/* The shot-view builder that also re-aims the scene's third light: in mode 12,
 * once the shot is 10 frames in, it takes the vector between two sampled points
 * of player 0, normalises it to the 127 range a Light direction wants, and
 * writes it into D_800C73B0[2] along with a fixed colour pair. */
void func_800967F4(s32 player, GolfCamera* cam) {
  Vec3f pos;
  f32 unused[16];
  Vec3f dir;
  Vec3f base;
  u8* cs;
  f32 follow_dist;
  f32 ground;
  f32 len;
  f32 scale;

  cs = get_character_state(player);
  if (D_800E4C54 == 14) {
    follow_dist = func_80095A10(107520.0f, 30720.0f, D_800C73A0 * 0.025f);
  } else {
    follow_dist = func_80095A10(107520.0f, 38400.0f, D_800C73A0 * 0.025f);
  }

  if (D_800E4C54 == 12) {
    if (D_800C73A0 >= 10) {
      func_80079358(player, 3);
      func_8005483C(0, 3, &dir);
      func_8005483C(0, 1, &base);
      dir.x = dir.x - base.x;
      dir.y = dir.y - base.y;
      dir.z = dir.z - base.z;
      len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
      if (len > 1.0f) {
        scale = 127.0f / len;
        dir.x = dir.x * scale;
        dir.y = dir.y * scale;
        dir.z = dir.z * scale;
        D_800E4C5C = 1;
        set_anim_flag_82(player, 2);
        D_800C73B0[2].l.col[0] = 0xFF;
        D_800C73B0[2].l.col[1] = 0xFF;
        D_800C73B0[2].l.col[2] = 0x7F;
        D_800C73B0[2].l.colc[0] = 0xFF;
        D_800C73B0[2].l.colc[1] = 0xFF;
        D_800C73B0[2].l.colc[2] = 0x7F;
        D_800C73B0[2].l.dir[0] = dir.x;
        D_800C73B0[2].l.dir[1] = dir.y;
        D_800C73B0[2].l.dir[2] = dir.z;
      }
    }
  }

  func_8005483C(player, 1, &pos);
  ground = (get_interpolated_terrain_height_wrapper((s32)(pos.x * 1024.0f),
                                                    (s32)(pos.z * 1024.0f)) -
            0x1E00) *
           (1.0f / 1024.0f);
  if (ground < pos.y) {
    pos.y = ground;
  }
  if (D_800E4C54 == 12) {
    pos.y -= 4.5f;
  }

  cam->at.x = pos.x;
  cam->at.y = pos.y;
  cam->at.z = pos.z;
  cam->eye.x = pos.x + cosf(-*(f32*)(cs + 0x48) - 1.57079637f) * follow_dist *
                           (1.0f / 1024.0f);
  cam->eye.y = pos.y + -1.5f;
  if (D_800E4C54 == 14) {
    cam->eye.y = pos.y + -7.5f;
  }
  cam->eye.z = pos.z + sinf(-*(f32*)(cs + 0x48) - 1.57079637f) * follow_dist *
                           (1.0f / 1024.0f);
  func_8009676C(cam);
  func_80095A68(cam, &cam->at, 0.1f, 3);

  if (flag_is_set(0x7F) || (D_800BB020 != 30 && D_801B60A0 == 1)) {
    cam->at.y -= 4.5f;
    cam->eye.y += 4.5f;
  }
}

/* The mode-zoomed variant of the shot-view builder: the near/far pair the
 * follow distance interpolates between is chosen per camera mode, the ball
 * position is dropped 15 units before the terrain clamp, and the eye rides
 * below the look-at by an amount that is again mode-specific. */
void func_80096C04(s32 player, GolfCamera* cam) {
  Vec3f pos;
  f32 unused[16];
  u8* cs;
  f32 follow_dist;
  f32 ground;

  cs = get_character_state(player);
  if (D_800E4C54 == 12) {
    follow_dist = func_80095A10(107520.0f, 46080.0f, D_800C73A0 * 0.025f);
  } else if (D_800E4C54 == 13) {
    follow_dist = func_80095A10(107520.0f, 23040.0f, D_800C73A0 * 0.025f);
  } else {
    follow_dist = func_80095A10(92160.0f, 38400.0f, D_800C73A0 * 0.025f);
  }

  if (D_800E4C54 == 12) {
    if (D_800C73A0 >= 20) {
      func_80079358(player, 3);
    }
  }

  func_8005483C(player, 2, &pos);
  pos.y -= 15.0f;
  if (D_800E4C54 == 13) {
    pos.y += 1.5f;
  }

  ground = (get_interpolated_terrain_height_wrapper((s32)(pos.x * 1024.0f),
                                                    (s32)(pos.z * 1024.0f)) -
            0x1E00) *
           (1.0f / 1024.0f);
  if (ground < pos.y) {
    pos.y = ground;
  }

  cam->at.x = pos.x;
  cam->at.y = pos.y;
  cam->at.z = pos.z;
  cam->eye.x = pos.x + cosf(-*(f32*)(cs + 0x48) - 1.57079637f) * follow_dist *
                           (1.0f / 1024.0f);
  cam->eye.z = pos.z + sinf(-*(f32*)(cs + 0x48) - 1.57079637f) * follow_dist *
                           (1.0f / 1024.0f);
  switch (D_800E4C54) {
    case 12:
      cam->eye.y = pos.y;
      break;
    case 13:
      cam->eye.y = pos.y + -9.0f;
      break;
    default:
      cam->eye.y = pos.y + -4.5f;
      break;
  }
  func_8009676C(cam);
  func_80095A68(cam, &cam->at, 0.1f, 3);

  if (flag_is_set(0x7F) || (D_800BB020 != 30 && D_801B60A0 == 1)) {
    cam->at.y -= 4.5f;
    cam->eye.y += 4.5f;
  }
}

/* The overhead-follow variant of the same shot-view builder: the eye sits level
 * with the ball (no height offset) and, once the shot is well under way in mode
 * 15, the camera hands the horizontal aim it has drifted to back to the player
 * heading, alternating the two call flavours on the frame parity. */
void func_80096F44(s32 player, GolfCamera* cam) {
  Vec3f pos;
  f32 unused[16];
  u8* cs;
  f32 follow_dist;
  f32 ground;
  f32 aim;
  f32 dx;
  f32 dz;

  cs = get_character_state(player);
  follow_dist = func_80095A10(61440.0f, 30720.0f, D_800C73A0 * 0.0125f);

  func_8005483C(player, 1, &pos);
  ground = (get_interpolated_terrain_height_wrapper((s32)(pos.x * 1024.0f),
                                                    (s32)(pos.z * 1024.0f)) -
            0x2400) *
           (1.0f / 1024.0f);
  if (ground < pos.y) {
    pos.y = ground;
  }

  cam->at.x = pos.x;
  cam->at.y = pos.y;
  cam->at.z = pos.z;
  cam->eye.x = pos.x + cosf(-*(f32*)(cs + 0x48) - 1.57079637f) * follow_dist *
                           (1.0f / 1024.0f);
  cam->eye.y = pos.y;
  cam->eye.z = pos.z + sinf(-*(f32*)(cs + 0x48) - 1.57079637f) * follow_dist *
                           (1.0f / 1024.0f);
  func_8009676C(cam);
  func_80095A68(cam, &cam->at, 0.1f, 3);

  if (D_800E4C54 == 15 && D_800C73A0 >= 44) {
    /* Both deltas are computed before the test, not short-circuited inside it:
     * the ROM loads all four coordinates up front and lets the z subtraction
     * fill the first branch's delay slot. */
    dx = cam->at.x - cam->eye.x;
    dz = cam->at.z - cam->eye.z;
    if ((s32)dx != 0 || (s32)dz != 0) {
      aim = atan2f(dz, dx);
    } else {
      aim = 0.0f;
    }
    if ((D_800C73A0 & 1) == 0) {
      func_80078FA8(player, 3, (f32)(aim + D_800D1EC8));
    } else {
      func_80078FA8(player, 4, (f32)(aim - D_800D1ED0));
    }
  }

  if (flag_is_set(0x7F) || (D_800BB020 != 30 && D_801B60A0 == 1)) {
    cam->at.y -= 4.5f;
    cam->eye.y += 4.5f;
  }
}

/* A third shot-view camera builder, sharing the eye/at construction with
 * func_8009806C and func_80098310. What is specific to this one: the eye rides
 * 15 units below the ball outside mode 15, and the mode-15 fixup lifts the eye
 * by 3 while every other mode drops the look-at and lifts the eye by 7.5. */
void func_80097218(s32 player, GolfCamera* cam) {
  Vec3f pos;
  f32 unused[16];
  u8* cs;
  f32 follow_dist;
  f32 ground;

  cs = get_character_state(player);
  follow_dist = func_80095A10(107520.0f, 30720.0f, D_800C73A0 * 0.025f);

  if (D_800E4C54 == 12) {
    if (D_800C73A0 % 3 == 0) {
      func_80079358(player, 3);
    }
  }

  func_8005483C(player, 1, &pos);
  ground = (get_interpolated_terrain_height_wrapper((s32)(pos.x * 1024.0f),
                                                    (s32)(pos.z * 1024.0f)) -
            0x1E00) *
           (1.0f / 1024.0f);
  if (ground < pos.y) {
    pos.y = ground;
  }

  cam->at.x = pos.x;
  cam->at.y = pos.y;
  cam->at.z = pos.z;
  cam->eye.x = pos.x + cosf(-*(f32*)(cs + 0x48) - 1.57079637f) * follow_dist *
                           (1.0f / 1024.0f);
  if (D_800E4C54 == 15) {
    cam->eye.y = pos.y;
  } else {
    cam->eye.y = pos.y + -15.0f;
  }
  cam->eye.z = pos.z + sinf(-*(f32*)(cs + 0x48) - 1.57079637f) * follow_dist *
                           (1.0f / 1024.0f);
  func_8009676C(cam);
  func_80095A68(cam, &cam->at, 0.1f, 3);

  if (flag_is_set(0x7F) || (D_800BB020 != 30 && D_801B60A0 == 1)) {
    if (D_800E4C54 == 15) {
      cam->at.y -= 4.5f;
      cam->eye.y += 3.0f;
    } else {
      cam->at.y -= 7.5f;
      cam->eye.y += 7.5f;
    }
  }
}

/* The replay-style variant: the eye height and the follow smoothing both depend
 * on the camera mode, and mode 13 runs the aim step twice -- once at full
 * weight, then again through the shared 0.1 pass every mode takes. */
void func_800974D8(s32 player, GolfCamera* cam) {
  Vec3f pos;
  f32 unused[16];
  u8* cs;
  f32 follow_dist;
  f32 ground;

  cs = get_character_state(player);
  follow_dist = func_80095A10(107520.0f, 30720.0f, D_800C73A0 * 0.025f);

  if (D_800E4C54 == 15) {
    if (D_800C73A0 == 10 || D_800C73A0 == 24 || D_800C73A0 == 33) {
      func_80078DC0(0, 3);
    }
  }

  func_8005483C(player, 1, &pos);
  ground = (get_interpolated_terrain_height_wrapper((s32)(pos.x * 1024.0f),
                                                    (s32)(pos.z * 1024.0f)) -
            0x1E00) *
           (1.0f / 1024.0f);
  if (ground < pos.y) {
    pos.y = ground;
  }

  cam->at.x = pos.x;
  cam->at.y = pos.y;
  cam->at.z = pos.z;
  cam->eye.x = pos.x + cosf(-*(f32*)(cs + 0x48) - 1.57079637f) * follow_dist *
                           (1.0f / 1024.0f);
  cam->eye.z = pos.z + sinf(-*(f32*)(cs + 0x48) - 1.57079637f) * follow_dist *
                           (1.0f / 1024.0f);

  /* The 0.1 aim pass is spelled out in both the mode-12 arm and the default arm
   * rather than written once after the switch. That is codegen-load-bearing:
   * with the calls inside each arm, the eye.y store sits in the same block as
   * the add that feeds it, so the scheduler covers the latency with the call's
   * own argument setup, and gcc then cross-jumps the two identical tails into
   * one -- which is why the ROM's merge point is the argument move rather than
   * the store. */
  switch (D_800E4C54) {
    case 12:
      cam->eye.y = pos.y + -15.0f;
      func_8009676C(cam);
      func_80095A68(cam, &cam->at, 0.1f, 3);
      break;
    case 13:
      cam->eye.y = pos.y + 1.5f;
      func_8009676C(cam);
      func_80095A68(cam, &cam->at, 1.0f, 3);
      /* fallthrough */
    default:
      cam->eye.y = pos.y + 1.5f;
      func_8009676C(cam);
      func_80095A68(cam, &cam->at, 0.1f, 3);
      break;
  }

  if (flag_is_set(0x7F) || (D_800BB020 != 30 && D_801B60A0 == 1)) {
    if (D_800E4C54 == 12) {
      cam->at.y -= 7.5f;
      cam->eye.y += 9.0f;
    } else {
      cam->at.y -= 3.0f;
      cam->eye.y += 3.0f;
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_800977E0);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80097A08);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80097C18);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80097E30);

void func_8009806C(s32 player, GolfCamera* cam) {
  Vec3f pos;
  f32 unused[16];
  u8* cs;
  f32 follow_dist;
  f32 ground;

  cs = get_character_state(player);
  follow_dist = func_80095A10(76800.0f, 46080.0f, D_800C73A0 * 0.025f);

  if (D_800E4C54 == 12) {
    if (D_800C73A0 >= 15 && D_800C73A0 <= 51) {
      func_80079358(player, D_800C73A0 % 3 + 3);
      func_80079358(player, D_800C73A0 % 3 + 6);
    }
  }

  func_8005483C(player, 1, &pos);
  ground = (get_interpolated_terrain_height_wrapper((s32)(pos.x * 1024.0f),
                                                    (s32)(pos.z * 1024.0f)) -
            0x1E00) *
           (1.0f / 1024.0f);
  if (ground < pos.y) {
    pos.y = ground;
  }

  cam->at.x = pos.x;
  cam->at.y = pos.y;
  cam->at.z = pos.z;
  cam->eye.x = pos.x + cosf(-*(f32*)(cs + 0x48) - 1.57079637f) * follow_dist *
                           (1.0f / 1024.0f);
  cam->eye.y = pos.y + 1.5f;
  cam->eye.z = pos.z + sinf(-*(f32*)(cs + 0x48) - 1.57079637f) * follow_dist *
                           (1.0f / 1024.0f);
  func_8009676C(cam);
  func_80095A68(cam, &cam->at, 0.1f, 3);

  if (flag_is_set(0x7F) || (D_800BB020 != 30 && D_801B60A0 == 1)) {
    cam->at.y -= 4.5f;
    cam->eye.y += 3.0f;
  }
}

void func_80098310(s32 player, GolfCamera* cam) {
  Vec3f pos;
  f32 unused[16];
  u8* cs;
  f32 follow_dist;
  f32 swing;
  f32 ground;

  cs = get_character_state(player);
  follow_dist = func_80095A10(76800.0f, 30720.0f, D_800C73A0 * 0.025f);
  swing = (D_800C73A0 - D_800E4C58) * 0.049087387f;
  if (swing > 0.0f) {
    swing = 0.0f;
  }

  func_8005483C(player, 1, &pos);
  ground = (get_interpolated_terrain_height_wrapper((s32)(pos.x * 1024.0f),
                                                    (s32)(pos.z * 1024.0f)) -
            0x1E00) *
           (1.0f / 1024.0f);
  if (ground < pos.y) {
    pos.y = ground;
  }

  cam->at.x = pos.x;
  cam->at.y = pos.y;
  cam->at.z = pos.z;
  if (flag_is_set(0x7F) || (D_800BB020 != 30 && D_801B60A0 == 1)) {
    swing = 0.0f;
  }

  cam->eye.x = pos.x + cosf(swing - *(f32*)(cs + 0x48) - 1.57079637f) *
                           follow_dist * (1.0f / 1024.0f);
  cam->eye.y = pos.y + 1.5f;
  cam->eye.z = pos.z + sinf(swing - *(f32*)(cs + 0x48) - 1.57079637f) *
                           follow_dist * (1.0f / 1024.0f);
  func_8009676C(cam);
  func_80095A68(cam, &cam->at, 0.3f, 3);

  if (flag_is_set(0x7F) || (D_800BB020 != 30 && D_801B60A0 == 1)) {
    cam->at.y -= 4.5f;
    cam->eye.y += 4.5f;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_800985B4);

void func_8009874C(void) { D_800E4CC8 = 0; }

/* Scores how flat the terrain is around a candidate position, and keeps the
 * position when it beats the best seen so far. The score sums the absolute
 * height difference from the centre sample over four rings of sixteen samples,
 * weighting an outer ring less by dividing by the ring index. A position
 * sitting inside any scenery collision cylinder is forced to a large score so
 * it loses.
 *
 * Declared s32 with valueless returns, as the ROM has it: no path sets $v0, but
 * marking it live at the epilogue is what stops the early-out branches from
 * stealing their delay slots from the fall-through path. */
s32 func_80098758(s32* pos) {
  s32 score;
  s32 centre_height;
  s32 ring;
  s32 step;
  s32 blocked;
  s32 i;
  s32 height;
  s32 distance;
  f32 angle;
  f32 offset_x;
  f32 offset_z;

  centre_height = get_interpolated_terrain_height_wrapper(pos[0], pos[2]);
  score = 0;

  for (ring = 1; ring < 5; ring++) {
    for (step = 0; step < 16; step++) {
      angle = (f32)step * 0.39269909f;
      offset_x = cosf(angle) * (f32)ring * 15360.0f;
      offset_z = sinf(angle) * (f32)ring * 15360.0f;
      height = get_interpolated_terrain_height_wrapper(
          (s32)((f32)pos[0] + offset_x), (s32)((f32)pos[2] + offset_z));
      height -= centre_height;
      if (height < 0) {
        height = -height;
      }
      score += height / (ring + 1);
    }
  }

  score <<= 1;

  if (D_800E4CC8 == 0) {
    blocked = 0;
    for (i = 0; i < 0x80; i++) {
      CollisionCyl* cyl = &collision_cylinders[i];
      distance = calculate_hypotenuse_safe((cyl->x << 10) - pos[0],
                                           (cyl->z << 10) - pos[2]);
      distance -= cyl->radius << 10;
      if (distance <= 0x12BFF) {
        blocked = 1;
      }
    }
    if (blocked != 0) {
      score = 0x465000;
    }
  }

  /* Re-read rather than reuse the test above: the cylinder pass runs only on
   * the first call, and this second test also has to see the value it set. */
  if (D_800E4CC8 != 0) {
    if (score * 2 >= D_800E4CA4) {
      return;
    }
    if (0xB400 >= D_800E4CA4) {
      return;
    }
  }

  D_800E4CA4 = score;
  D_800E4C98[0] = pos[0];
  D_800E4C98[1] = pos[1];
  D_800E4CC8 = 1;
  D_800E4C98[2] = pos[2];
}

void func_800989C4(s32* arg0) {
  s32 v;
  v = D_800E4C98[0];
  arg0[0] = v;
  v = D_800E4C98[1];
  arg0[1] = v;
  v = D_800E4C98[2];
  arg0[2] = v;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_800989EC);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80098C6C);

s32 func_80098CA0(s32 arg0) {
  u16 temp;
  s32 result = 0;
  s32 i = 7;

  do {
    temp = (result & 0xFF) << 1;
    result = temp;
    if (arg0 & 1) {
      result = temp | 1;
    }
    arg0 = (u32)(arg0 & 0xFF) >> 1;
    i--;
  } while (i >= 0);

  return result & 0xFF;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80098CD8);

s32 func_80098D70(u8* dst, u8* src, s32 count) {
  s32 offset;
  s32 i;
  for (i = 0; i < count; i++) {
    s32 k;
    s32 matched;
    for (k = 0; k < 28; k++) {
      u8* p = D_800C73F0[k];
      matched = 0;
      if (*p != 0) {
        u8 c = src[i];
        while (*p != 0) {
          /* Empty block is codegen-load-bearing: the basic-block boundary
             breaks the CSE extended-basic-block so `*p` reloads at the loop
             top each iteration (matches ROM; permuter-confirmed). */
          if (1) {
          }
          if (*p == c) {
            matched = 1;
            break;
          }
          p++;
        }
      }
      if (matched) {
        break;
      }
    }
    if (k >= 28) {
      return -1;
    }
    offset = i - 28;
    dst[i] = (k - offset) % 28;
  }
  return 0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_80098E48);

INCLUDE_ASM("asm/nonmatchings/main/func_80095A10", func_800990D0);

u8* func_80099454(s32 arg0) {
  u8 index = arg0 & 0xFF;

  if (index >= 0xC0) {
    return &D_800C7464[0];
  }
  return &D_800C7464[index * 3];
}
