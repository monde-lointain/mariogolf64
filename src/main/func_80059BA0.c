#include "common.h"

typedef struct {
  s32 unk_00[26];
} Struct80131510;

extern Struct80131510 D_80131510[];
extern u8 D_801323A0[];
extern s8 D_80105DC1;
extern s8 D_80105DC2;
extern s32 D_80105DC4;
extern s32 D_800C2B28[];
extern s32 D_800FE334;
extern u8 D_801052E8;
extern u8 D_801B9410;
extern u8 D_800C2BD4;
extern s32 current_game_mode;
extern u8 D_8012F720[];
extern s32 D_801B6098;
extern s32 scenario_mode_id;
extern u8 D_8012D400[];
extern u32 D_8012F724;
extern s32 D_80105DCC;
extern s32 D_80105DC8;

extern u8 rumble_disable_flag;
extern u8 D_800FE3EC;
extern u8 D_800FE402;
extern u8 D_800FE418;
extern u8 D_800FE42E;
extern s8 D_800C1FF4;
extern s8 D_800C1FF5;
extern s8 D_800C1FF6;
extern s8 D_800C1FF7;
extern s32 D_800C2B3C;

extern s32 D_800C2BD0;
extern s32 D_80105B68;
extern u32 player_context_offset;
extern s32 D_8012F480;
extern s32 D_8012F484;
extern s32 D_8012F488;
extern s32 D_8012F48C;
extern u8 D_801B70F0;
extern u8 D_801B70F1;
extern u8 D_801B70F2;
extern u8 D_801B70F3;
extern s16 D_800FBD40;
extern s16 D_801B71CC;
extern s16 D_800FBE2C;
extern u8 D_800FBDB9;
extern s16 D_800FE33C;
extern u8 D_800FF4C0;
extern u8 D_80105268;
extern u8 D_800FBE08[];
extern u8 D_801323B8;
extern s32 D_801B60AC;
extern u8 D_801323D0;
extern s32 D_801B608C;
extern u8 D_801B5634;
extern u8 D_800C2BD5;

u8* func_8005AF74(void);
void func_8005DF54(u8*, s32);
s32 func_80099490(void);
void flag_clear(s32);
void func_8005D9A0(void);

/* func_80059BA0: fabsf(arg0) via union bit-clear of the sign bit (0x7FFFFFFF).
 * NEAR-MATCH, CARRIED (terminal sched2 coin). The faithful body
 *   union{f32 f;s32 i;}x; x.f=arg0; x.i&=0x7FFFFFFF; arg0=x.f; return arg0;
 * yields the exact 8 insns + identical regalloc, but gcc's post-reload
 * scheduler (schedule_select, sched.c:2650) front-loads the independent `li`
 * ahead of `mfc1` to fill the mov.s->mfc1 FP-load-coproc hazard slot:
 *   mine: mov.s $f0,$f12; li; mfc1; ori; and; mtc1
 *   ROM : mov.s $f0,$f12; mfc1; li; ori; and; mtc1
 * See docs/wip/func_80059BA0.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_80059BA0);

/* fdlibm single-precision acos. It reduces to the same rational approximation
 * asin(x) = x + x*x^2*R(x^2), R = P/Q, that fdlibm's asinf uses, which is why
 * the PS0-PS5 / QS1-QS4 coefficients below are asin's. Every coefficient is a
 * source literal, since this compiler materializes an SFmode constant inline
 * with lui/ori/mtc1 rather than from a rodata pool. Two constants arrive
 * pre-folded: pi + 2*pio2_lo = 3.14159274f (the acos(-1) return) and pio2_hi +
 * pio2_lo = 1.57079637f (the |x| < 2^-57 return).
 *
 * This variant returns 0.0f for |x| > 1 where stock fdlibm returns the NaN
 * (x-x)/(x-x); the two 0.0f returns cross-jump into one block.
 *
 * sqrtf() is GCC's BUILT_IN_FSQRT, so the TU carries the -ffast-math override
 * in mk/main.mk that drops the NaN guard around the bare sqrt.s. */
#define PIO2_HI 1.57079625f /* 0x3FC90FDA */
#define PIO2_LO 7.54978942e-08f
/* 0x40490FDA, truncated down; pio2_lo carries the rest */
#define PI 3.1415925f
#define PS0 0.166666672f
#define PS1 -0.325565815f
#define PS2 0.201212525f
#define PS3 -0.0400555357f
#define PS4 0.000791535014f
#define PS5 3.47933092e-05f
#define QS1 -2.40339494f
#define QS2 2.02094579f
#define QS3 -0.68828398f
#define QS4 0.077038154f

extern f32 sqrtf(f32);

f32 acosf(f32 x) {
  f32 z, p, q, r, w, s, c, df;
  s32 hx, ix, idf;

  do {
    union {
      f32 f;
      s32 i;
    } gf_u;
    gf_u.f = x;
    hx = gf_u.i;
  } while (0);
  ix = hx & 0x7FFFFFFF;
  if (ix == 0x3F800000) { /* |x| == 1 */
    if (hx > 0) {
      return 0.0f; /* acos(1) = 0 */
    }
    return 3.14159274f; /* acos(-1) = pi */
  } else if (ix > 0x3F800000) {
    return 0.0f; /* |x| > 1 */
  }
  if (ix < 0x3F000000) { /* |x| < 0.5 */
    if (ix <= 0x23000000) {
      return 1.57079637f; /* |x| < 2^-57 */
    }
    z = x * x;
    p = z * (PS0 + z * (PS1 + z * (PS2 + z * (PS3 + z * (PS4 + z * PS5)))));
    q = 1.0f + z * (QS1 + z * (QS2 + z * (QS3 + z * QS4)));
    r = p / q;
    return PIO2_HI - (x - (PIO2_LO - x * r));
  } else if (hx < 0) { /* x < -0.5 */
    z = (1.0f + x) * 0.5f;
    p = z * (PS0 + z * (PS1 + z * (PS2 + z * (PS3 + z * (PS4 + z * PS5)))));
    q = 1.0f + z * (QS1 + z * (QS2 + z * (QS3 + z * QS4)));
    s = sqrtf(z);
    r = p / q;
    w = r * s - PIO2_LO;
    return PI - 2.0f * (s + w);
  } else { /* x > 0.5 */
    z = (1.0f - x) * 0.5f;
    s = sqrtf(z);
    df = s;
    do {
      union {
        f32 f;
        s32 i;
      } gf_u;
      gf_u.f = df;
      idf = gf_u.i;
    } while (0);
    do {
      union {
        f32 f;
        s32 i;
      } gf_u;
      gf_u.i = idf & 0xFFFFF000;
      df = gf_u.f;
    } while (0);
    c = (z - df * df) / (s + df);
    p = z * (PS0 + z * (PS1 + z * (PS2 + z * (PS3 + z * (PS4 + z * PS5)))));
    q = 1.0f + z * (QS1 + z * (QS2 + z * (QS3 + z * QS4)));
    r = p / q;
    w = r * s + c;
    return 2.0f * (df + w);
  }
}

f32 atanf(f32);
f32 func_80059BA0(f32); /* fabsf */
extern f32 D_800D089C;  /* -0.0f, the `-zero` return of case 1 */

/* fdlibm single-precision atan2. Every other constant folds into an immediate:
 * pi_o_2 = 1.57079637f, pi_o_4 = 0.785398185f, 3*pi_o_4 = 2.3561945f,
 * pi = 3.1415925f and pi_lo = 1.50995788e-07f (this variant truncates pi down
 * and carries the remainder in pi_lo), tiny = 1.0e-30f which folds away. */
f32 atan2f(f32 y, f32 x) {
  f32 z;
  s32 k, m, hx, hy, ix, iy;

  do {
    union {
      f32 f;
      s32 i;
    } gf_u;
    gf_u.f = x;
    hx = gf_u.i;
  } while (0);
  ix = hx & 0x7FFFFFFF;
  do {
    union {
      f32 f;
      s32 i;
    } gf_u;
    gf_u.f = y;
    hy = gf_u.i;
  } while (0);
  iy = hy & 0x7FFFFFFF;
  if ((ix > 0x7F800000) || (iy > 0x7F800000)) {
    return x + y; /* x or y is NaN */
  }
  if (hx == 0x3F800000) {
    return atanf(y); /* x = 1.0 */
  }
  m = ((hy >> 31) & 1) | ((hx >> 30) & 2); /* 2*sign(x) + sign(y) */

  /* when y = 0 */
  if (iy == 0) {
    switch (m) {
      case 0:
      case 1:
        return y; /* atan(+-0, +anything) = +-0 */
      case 2:
        return 3.1415925f; /* atan(+0, -anything) = pi */
      case 3:
        return -3.1415925f; /* atan(-0, -anything) = -pi */
    }
  }
  /* when x = 0 */
  if (ix == 0) {
    return (hy < 0) ? -1.57079637f : 1.57079637f;
  }
  /* when x is INF */
  if (ix == 0x7F800000) {
    if (iy == 0x7F800000) {
      switch (m) {
        case 0:
          return 0.785398185f; /* atan(+INF, +INF) */
        case 1:
          return -0.785398185f; /* atan(-INF, +INF) */
        case 2:
          return 2.3561945f; /* atan(+INF, -INF) */
        case 3:
          return -2.3561945f; /* atan(-INF, -INF) */
      }
    } else {
      switch (m) {
        case 0:
          return 0.0f; /* atan(+..., +INF) */
        case 1:
          return D_800D089C; /* atan(-..., +INF) */
        case 2:
          return 3.1415925f; /* atan(+..., -INF) */
        case 3:
          return -3.1415925f; /* atan(-..., -INF) */
      }
    }
  }
  /* when y is INF */
  if (iy == 0x7F800000) {
    return (hy < 0) ? -1.57079637f : 1.57079637f;
  }

  /* compute y/x */
  k = (iy - ix) >> 23;
  if (k > 60) {
    z = 1.57079649f; /* |y/x| > 2**60, pi_o_2 + 0.5f*pi_lo */
  } else if ((hx < 0) && (k < -60)) {
    z = 0.0f; /* |y|/x < -2**60 */
  } else {
    z = atanf(func_80059BA0(y / x)); /* safe to do y/x */
  }
  switch (m) {
    case 0:
      return z; /* atan(+, +) */
    case 1:
      do {
        union {
          f32 f;
          s32 i;
        } gf_u;
        gf_u.f = z;
        gf_u.i ^= 0x80000000;
        z = gf_u.f;
      } while (0);
      return z; /* atan(-, +) */
    case 2:
      return 3.1415925f - (z - 1.50995788e-07f); /* atan(+, -) */
    default:                                     /* case 3 */
      return (z - 1.50995788e-07f) - 3.1415925f; /* atan(-, -) */
  }
}

/* fdlibm single-precision atan. D_800D08A0 = atanhi[4], D_800D08B0 = atanlo[4],
 * D_800D08C0 = aT[11], all in the shared main rodata blob (extern, not carved).
 */
extern f32 D_800D08A0[];
extern f32 D_800D08B0[];
extern f32 D_800D08C0[];

f32 atanf(f32 x) {
  f32 w, s1, s2, z;
  s32 ix, hx, id;

  do {
    union {
      f32 f;
      s32 i;
    } gf_u;
    gf_u.f = x;
    hx = gf_u.i;
  } while (0);
  ix = hx & 0x7FFFFFFF;
  if (ix >= 0x50800000) { /* |x| >= 2^33 */
    if (ix > 0x7F800000) {
      return x + x; /* NaN */
    }
    if (hx > 0) {
      return D_800D08A0[3] + D_800D08B0[3];
    } else {
      return -D_800D08A0[3] - D_800D08B0[3];
    }
  }
  if (ix < 0x3EE00000) {   /* |x| < 0.4375 */
    if (ix < 0x31000000) { /* |x| < 2^-29 */
      if (1.0e30f + x > 1.0f) {
        return x; /* raise inexact */
      }
    }
    id = -1;
  } else {
    x = func_80059BA0(x);
    if (ix < 0x3F980000) {   /* |x| < 1.1875 */
      if (ix < 0x3F300000) { /* 7/16 <= |x| < 11/16 */
        id = 0;
        x = (2.0f * x - 1.0f) / (2.0f + x);
      } else { /* 11/16 <= |x| < 19/16 */
        id = 1;
        x = (x - 1.0f) / (x + 1.0f);
      }
    } else {
      if (ix < 0x401C0000) { /* |x| < 2.4375 */
        id = 2;
        x = (x - 1.5f) / (1.0f + 1.5f * x);
      } else { /* 2.4375 <= |x| < 2^33 */
        id = 3;
        x = -1.0f / x;
      }
    }
  }

  /* Break the sum from i=0 to 10 of aT[i]*z**(i+1) into odd and even polys. */
  z = x * x;
  w = z * z;
  s1 = z * (D_800D08C0[0] +
            w * (D_800D08C0[2] +
                 w * (D_800D08C0[4] +
                      w * (D_800D08C0[6] +
                           w * (D_800D08C0[8] + w * D_800D08C0[10])))));
  s2 =
      w * (D_800D08C0[1] +
           w * (D_800D08C0[3] +
                w * (D_800D08C0[5] + w * (D_800D08C0[7] + w * D_800D08C0[9]))));
  if (id < 0) {
    return x - x * (s1 + s2);
  } else {
    z = D_800D08A0[id] - ((x * (s1 + s2) - D_800D08B0[id]) - x);
    return (hx < 0) ? -z : z;
  }
}

typedef struct {
  /* 0x0 */ u32 start;
  /* 0x4 */ u32 size;
  /* 0x8 */ u32 pos;
  /* 0xC */ u32 end;
} RomLoadSlot; /* 0x10 */

extern void heap3_free(void** payload_ptr);
extern s8 D_800C1FF0;
extern void* D_801F4A20;
extern void* D_8012D3AC;
extern void* D_801049A0[];
extern void* D_8012F4B0[];
extern void* D_801052C4[];
extern void* D_800FF420[];
extern void* D_801B60D0;
extern void* D_8012D420[];
extern void* D_801B5CD0[];
extern void* D_801052BC[];
extern void* D_800FBDFC[];
extern void* D_800FF4C4[];
extern void* D_800FBE38[];
extern void* D_801B93E0[];
extern void* D_8010526C[];
extern void* D_800FF180[];
extern void* D_80106100;
extern void* D_800FC898;
extern void* tile_palette_table[];
extern void* tile_texture_table[];
extern void* D_800FBE30[];
extern void* D_800FC6A0;
extern void* D_801B69EC;

extern void* heap3_alloc(u32 need);
extern u32 func_8005062C(u16 index, void* out);
extern void func_800506D4(void* data, RomLoadSlot* slot);
extern void func_800504E8(s32 index, RomLoadSlot* slot);
extern u32 func_80050598(RomLoadSlot* slot);
extern void func_800505A0(void* dst, u32 size, RomLoadSlot* slot);
extern u8* func_8005AF50(void);
extern void func_8002A97C(s32 arg0, s32 arg1);
extern void cfb_set_num(u32 num);
extern void play_bgm_by_id(s32 id, s32 flag);

extern char D_800D08F0[];
extern u16 D_800C2014;
extern u16 D_800C2016;
extern u16 D_800C2028;
extern u16 D_800C2064[];
extern u16 D_800C20E0[][12];
extern s16 D_800C20D4[];
extern u16 D_800C21B8;
extern s16 D_800C2278[];
extern u16 D_800C2650[];
extern s16 D_800C2590[];
extern u16 D_800C28B0;

/* Allocates and loads every ROM asset the course scene needs, in one pass.
 *
 * Two asset APIs alternate throughout. The three-call block
 * (func_8005062C / heap3_alloc / func_800506D4) sizes an asset by id, takes a
 * heap block for it and reads it in; the five-call block
 * (func_800504E8 / func_80050598 / heap3_alloc / func_800505A0) does the same
 * through a pre-opened RomLoadSlot. func_8005ACF8 below releases these in the
 * same order.
 *
 * Several func_8005062C results are deliberately discarded: those buffers have
 * a fixed size (0x70A8, 0xFA0, 0x125C) and the call only opens the slot. */
void load_course_assets(void) {
  RomLoadSlot slot_a[2];
  RomLoadSlot slot_b[2];
  u8* save;
  /* Four separate pointers, not one reused local: each id global is read twice
   * across intervening calls, and a per-site pointer keeps that address in the
   * ROM's $s0. One shared pointer spans the whole function, which drops its
   * global.c priority below the array bases and permutes $s0/$s1 throughout. */
  u16* asset_id_a;
  u16* asset_id_b;
  u16* asset_id_c;
  u16* asset_id_d;
  s32 i;
  s32 off;
  u32 size;

  save = func_8005AF50();
  if (D_800C1FF0 != -1) {
    osSyncPrintf(D_800D08F0);
    return;
  }

  D_801F4A20 = heap3_alloc(func_8005062C(0x51E, slot_a));
  func_800506D4(D_801F4A20, slot_a);
  D_8012D3AC = heap3_alloc(func_8005062C(0x721, slot_a));
  func_800506D4(D_8012D3AC, slot_a);
  D_800FF420[0] = heap3_alloc(func_8005062C(0x520, slot_a));
  func_800506D4(D_800FF420[0], slot_a);
  D_800FF420[1] = heap3_alloc(func_8005062C(0x520, slot_a));
  func_800506D4(D_800FF420[1], slot_a);
  D_801B60D0 = heap3_alloc(func_8005062C(0x521, slot_a));
  func_800506D4(D_801B60D0, slot_a);

  D_8012D420[0] = heap3_alloc(0x70A8);
  D_801B5CD0[0] = heap3_alloc(0x70A8);
  D_8012D420[1] = heap3_alloc(0x70A8);
  D_801B5CD0[1] = heap3_alloc(0x70A8);
  asset_id_a = &D_800C2014;
  func_8005062C(*asset_id_a, slot_a);
  func_800506D4(D_8012D420[0], slot_a);
  func_8005062C(*asset_id_a, slot_a);
  func_800506D4(D_8012D420[1], slot_a);
  if (current_game_mode == 5) {
    func_8005062C(0x528, slot_a);
    func_800506D4(D_801B5CD0[0], slot_a);
    func_8005062C(0x528, slot_a);
    func_800506D4(D_801B5CD0[1], slot_a);
  } else {
    func_8005062C(D_800C2016, slot_a);
    func_800506D4(D_801B5CD0[0], slot_a);
    func_8005062C(D_800C2016, slot_a);
    func_800506D4(D_801B5CD0[1], slot_a);
  }

  asset_id_b = &D_800C2028;
  D_801052BC[0] = heap3_alloc(func_8005062C(*asset_id_b, slot_a));
  func_800506D4(D_801052BC[0], slot_a);
  D_801052BC[1] = heap3_alloc(func_8005062C(*asset_id_b, slot_a));
  func_800506D4(D_801052BC[1], slot_a);

  off = 0;
  for (i = 0; i != 7; i++) {
    if (current_game_mode == 5) {
      u8* row = (u8*)D_800C20E0[D_80105B68];
      size = func_8005062C(*(u16*)(i * 2 + row), slot_a);
    } else {
      size = func_8005062C(D_800C2064[i], slot_a);
    }
    D_801049A0[i] = heap3_alloc(size);
    func_800506D4(D_801049A0[i], slot_a);
    if (current_game_mode == 5) {
      u8* row = (u8*)D_800C20E0[D_80105B68];
      size = func_8005062C(*(u16*)(i * 2 + row), slot_a);
    } else {
      size = func_8005062C(D_800C2064[i], slot_a);
    }
    D_8012F4B0[i] = heap3_alloc(size);
    func_800506D4(D_8012F4B0[i], slot_a);
  }

  for (i = 0; i != 2; i++) {
    func_800504E8(D_800C20D4[i], slot_b);
    size = func_80050598(slot_b);
    D_801052C4[i] = heap3_alloc(size);
    func_800505A0(D_801052C4[i], size, slot_b);
  }

  asset_id_c = &D_800C21B8;
  func_8005062C(*asset_id_c, slot_a);
  D_800FBDFC[0] = heap3_alloc(0xFA0);
  func_800506D4(D_800FBDFC[0], slot_a);
  func_8005062C(*asset_id_c, slot_a);
  D_800FBDFC[1] = heap3_alloc(0xFA0);
  func_800506D4(D_800FBDFC[1], slot_a);

  for (i = 0; i != 2; i++) {
    func_8005062C(D_800C21B8, slot_a);
    D_800FF4C4[i] = heap3_alloc(0x125C);
    func_800506D4(D_800FF4C4[i], slot_a);
  }

  for (i = 0; i != 5; i++) {
    D_801B93E0[i] = heap3_alloc(func_8005062C(0x720, slot_a));
    func_800506D4(D_801B93E0[i], slot_a);
  }

  size = func_8005062C(0x623, slot_a);
  D_8010526C[0] = heap3_alloc(size);
  D_8010526C[1] = heap3_alloc(size);
  D_800FF180[0] = heap3_alloc(size);
  D_800FF180[1] = heap3_alloc(size);
  func_800506D4(D_8010526C[0], slot_a);
  D_80106100 = heap3_alloc(func_8005062C(0x65D, slot_a));
  func_800506D4(D_80106100, slot_a);
  D_800FC898 = heap3_alloc(func_8005062C(0x663, slot_a));
  func_800506D4(D_800FC898, slot_a);

  for (i = 0; i != 18; i++) {
    off = i * 16;
    tile_palette_table[i] =
        heap3_alloc(func_8005062C(*(u16*)((u8*)D_800C2650 + off), slot_a));
    func_800506D4(tile_palette_table[i], slot_a);
  }

  for (i = 0; i != 18; i++) {
    off = i * 8;
    func_800504E8(*(s16*)((u8*)D_800C2590 + off), slot_b);
    size = func_80050598(slot_b);
    tile_texture_table[i] = heap3_alloc(size);
    func_800505A0(tile_texture_table[i], size, slot_b);
  }

  for (i = 0; i != 3; i++) {
    func_800504E8(D_800C2278[i], slot_b);
    size = func_80050598(slot_b);
    D_800FBE38[i] = heap3_alloc(size);
    func_800505A0(D_800FBE38[i], size, slot_b);
  }

  asset_id_d = &D_800C28B0;
  D_800FBE30[0] = heap3_alloc(func_8005062C(*asset_id_d, slot_a) * 2);
  func_800506D4(D_800FBE30[0], slot_a);
  D_800FBE30[1] = heap3_alloc(func_8005062C(*asset_id_d, slot_a) * 2);
  func_800506D4(D_800FBE30[1], slot_a);
  D_800FC6A0 = heap3_alloc(func_8005062C(0x6F7, slot_a) * 2);
  func_800506D4(D_800FC6A0, slot_a);
  D_801B69EC = heap3_alloc(func_8005062C(0x709, slot_a));
  func_800506D4(D_801B69EC, slot_a);

  play_bgm_by_id(0x27, 0);
  cfb_set_num(3);
  func_8002A97C(4, 0);
  D_801323D0 = 6;
  func_80099490();
  func_8005DF54(save, 0);
}

void func_8005ACF8(void) {
  s32 i;

  D_800C1FF0 = -1;
  heap3_free(&D_801F4A20);
  heap3_free(&D_8012D3AC);
  i = 0;
  do {
    heap3_free(&D_801049A0[i]);
    heap3_free(&D_8012F4B0[i]);
    i++;
  } while (i != 7);
  heap3_free(&D_801052C4[0]);
  heap3_free(&D_801052C4[1]);
  heap3_free(&D_800FF420[0]);
  heap3_free(&D_800FF420[1]);
  heap3_free(&D_801B60D0);
  heap3_free(&D_8012D420[0]);
  heap3_free(&D_801B5CD0[0]);
  heap3_free(&D_8012D420[1]);
  heap3_free(&D_801B5CD0[1]);
  heap3_free(&D_801052BC[0]);
  heap3_free(&D_801052BC[1]);
  heap3_free(&D_800FBDFC[0]);
  heap3_free(&D_800FBDFC[1]);
  i = 0;
  do {
    heap3_free(&D_800FF4C4[i]);
    i++;
  } while (i != 2);
  i = 0;
  do {
    heap3_free(&D_800FBE38[i]);
    i++;
  } while (i != 3);
  i = 0;
  do {
    heap3_free(&D_801B93E0[i]);
    i++;
  } while (i != 5);
  heap3_free(&D_8010526C[0]);
  heap3_free(&D_8010526C[1]);
  heap3_free(&D_800FF180[0]);
  heap3_free(&D_800FF180[1]);
  heap3_free(&D_80106100);
  heap3_free(&D_800FC898);
  i = 0;
  do {
    heap3_free(&tile_palette_table[i]);
    i++;
  } while (i != 18);
  i = 0;
  do {
    heap3_free(&tile_texture_table[i]);
    i++;
  } while (i != 18);
  heap3_free(&D_800FBE30[0]);
  heap3_free(&D_800FBE30[1]);
  heap3_free(&D_800FC6A0);
  heap3_free(&D_801B69EC);
}

u8* func_8005AF50(void) {
  func_8005AF74();
  return D_8012F720;
}

u8* func_8005AF74(void) { return D_801323A0; }

void* memset(void*, s32, u32);

void func_8005AF80(void) {
  u8* base = func_8005AF50();
  s8 i;
  s8 j;
  memset(base, 0, 0x2A78);
  *(s32*)base = 0x12345678;
  i = 0;
  do {
    s32* row;
    j = 0;
    /* (s32)base drops REG_POINTER so the addu keeps i*12 as rs (addu v0,v0,s0),
     * matching the ROM's commutative operand order. */
    row = (s32*)(i * 12 + (s32)base + 0xDC0);
    do {
      row[j] = -1;
      j++;
    } while (j != 3);
    i++;
  } while (i != 6);
  func_8005DF54(base, 1);
}

void func_8005B03C(void) {
  u8* temp = func_8005AF50();
  func_80099490();
  func_8005DF54(temp, 0);
}

s32 func_8005B070(s32 arg0) {
  s32 i = 5;
  s32* p = &D_800C2B3C;
  while (i != 0) {
    if (arg0 >= *p) {
      break;
    }
    i--;
    p--;
  }
  return i + 1;
}

s32 func_8005B0A0(s32 arg0) { return D_800C2B28[arg0]; }

/* func_8005B0B4: selector (flags&0xF) dispatch -> strength value. NEAR-MATCH
 * 37/39 (faithful body in nonmatchings/func_8005B0B4/base.c). Residual = S263
 * value-select-branch-likely: ROM emits the const early-returns as `beql
 * sel,K,end` with the value in the annulled delay slot; gcc-from-C branches
 * away + `j;li`. Terminal / permuter-denied. See
 * docs/wip/func_8005B0B4.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B0B4);

void rank_scores_descending(s32* scores, s32* order, s32 count) {
  s32 keys[count];
  s32 i;
  s32 j;
  s32 k;
  s32 key;
  s32 rank;

  for (i = 0; i < count; i++) {
    order[i] = i;
  }
  for (i = 0; i < count; i++) {
    keys[i] = scores[i];
  }
  for (i = 0; i < count; i++) {
    key = keys[i];
    rank = order[i];
    for (j = i - 1; j >= 0 && keys[j] < key; j--) {
    }
    if (j < i - 1) {
      for (k = i; j + 1 < k; k--) {
        keys[k] = keys[k - 1];
        order[k] = order[k - 1];
      }
      keys[k] = key;
      order[k] = rank;
    }
  }
}

void func_8005B28C(s32 idx, Struct80131510* src) {
  D_80131510[idx] = *src;
  func_8005DF54(func_8005AF50(), 1);
}

/* D_80130CF0 is the four-category leaderboard, 4 blocks of 5 Struct80131510
 * records; the block base comes from the low nibble of the record's flag byte
 * at 0x3A, whose high bit gates the whole insert. D_80131510 is the record
 * immediately past it. rank_scores_descending ranks `scores` into the index
 * permutation `order`. Byte 0x3A is read twice, once signed for the high-bit
 * test and once unsigned for the nibble, which is what the ROM's `lb`/`lbu`
 * pair shows. */
extern Struct80131510 D_80130CF0[];
extern char D_800D0904[]; /* "Checking SuperShot" */
extern char D_800D0918[]; /* "1:prior=%d:" */
extern char D_800D0924[]; /* "2" */
extern char D_800D0928[]; /* "\n" */
extern void osSyncPrintf(const char*, ...);
s32 func_8005B0B4(Struct80131510*);
void rank_scores_descending(s32* scores, s32* order, s32 count);

void insert_supershot_record(Struct80131510* entry) {
  s32 scores[20];
  s32 order[20];
  Struct80131510 records[20];
  s32 base;
  s32 best;
  s32 min;
  s32 first;
  /* `count` is initialized here rather than next to the loops so that loop.c
   * cannot see the initial value: that is what keeps the first loop's entry
   * guard and gives the frame its ROM size. `n` then carries the bound, which
   * moves its live range to the second osSyncPrintf and gets it the ROM's $s3
   * (with one variable, global.c's live-length priority rotates it against
   * `first` and `best`). The copy folds away, so it costs nothing. */
  s32 count = 5;
  s32 i;
  s32 n;
  Struct80131510* board;

  osSyncPrintf(D_800D0904);
  if (*(s8*)((u8*)entry + 0x3A) & 0x80) {
    base = func_8005B0B4(entry);
    board = D_80130CF0;
    min = 0x40000000;
    best = 0;
    /* Cases are listed in descending order: that is what makes emit_case_nodes
     * give the `case 3` / default leaf the ROM's `bne`, with the default value
     * in the delay slot. Ascending order emits the same four instructions with
     * the two arms swapped. */
    switch (*(u8*)((u8*)entry + 0x3A) & 0xF) {
      default:
        first = 15;
        break;
      case 3:
        first = 0;
        break;
      case 2:
        first = 5;
        break;
      case 1:
        first = 15;
        break;
      case 0:
        first = 10;
        break;
    }
    n = count;
    osSyncPrintf(D_800D0918, base);
    /* Empty statement, but not dead: it ends the call's basic block, so reorg
     * fills the jal's delay slot with the bound's `li` instead of reaching past
     * the call for the loop's `i = 0` and then having to annul the guard. */
    do {
    } while (0);
    for (i = 0; i < n; i++) {
      scores[i] = func_8005B0B4(&board[first + i]);
    }
    for (i = 0; i < n; i++) {
      if (!(min < scores[i])) {
        best = i;
        min = scores[i];
      }
    }
    if (!(base < min)) {
      osSyncPrintf(D_800D0924);
      scores[best] = 0;
      rank_scores_descending(scores, order, n);
      for (i = 0; i < n; i++) {
        records[i] = board[first + i];
      }
      for (i = 0; i < n - 1; i++) {
        board[first + i + 1] = records[order[i]];
        scores[i + 1] = func_8005B0B4(&board[first + i + 1]);
      }
      scores[0] = base;
      board[first] = *entry;
      rank_scores_descending(scores, order, n);
      for (i = 0; i < n; i++) {
        records[i] = board[first + i];
      }
      for (i = 0; i < n; i++) {
        board[first + i] = records[order[i]];
      }
      func_8005DF54(func_8005AF50(), 1);
    }
  }
  osSyncPrintf(D_800D0928);
}

typedef struct {
  u32 data[0xD];
} Rec34;
typedef struct {
  u32 data[0x11];
} Rec44;
typedef struct {
  u32 data[0x7];
} Rec1C;
typedef struct {
  u32 data[0x1A];
} Rec68;
typedef struct {
  u32 data[0x2E];
} RecB8;

/* One continue slot: the saved copy of the global match state. `state` mirrors
 * the 0x44 block at D_801B6088 (whose word 2 is the player count), `camera`
 * the 0x34 block at camera_position_x and `misc` the 0x1C block at
 * D_800FF4D0. The per-player records live in a sibling array. */
typedef struct {
  s32 valid;    /* 0x00 */
  s32 unk_04;   /* 0x04 */
  Rec34 camera; /* 0x08 */
  Rec44 state;  /* 0x3C */
  Rec1C misc;   /* 0x80 */
  u8 pad9C[4];
} ContinueSlot; /* 0xA0 */

extern RecB8 D_801B71D0[];
extern Rec68 D_800FEDD8[];
extern RecB8 D_8012FB60[];
extern RecB8 D_8012FAA8[];
extern RecB8 D_8012F938[];
extern s32 D_801B6088;
extern s32 D_801B6090;
extern s8 D_801B60B8;
extern s32 camera_position_x;
extern void flag_set(s32 flag);
extern void func_8005DF54(u8* save, s32 arg1);
extern const char D_800D092C[];
extern const char D_800D0944[];
extern const char D_800D0958[];
extern const char D_800D0970[];

/* Per-game-mode twin of transfer_continue_slot: saves the live match state
 * into the continue block that D_801B608C selects (restore == 0), or restores
 * that block back. Each mode owns one contiguous region, so its record array,
 * its 0x68-stride mode records and the continue slot 0x2C8 / 0x2B0 / 0x1E0
 * bytes before it all come off the one base symbol. Modes with no continue
 * block fall through with a zero count. Returns -1 on failure, 0 otherwise. */
s32 transfer_mode_continue_slot(s32 restore) {
  RecB8* recs;
  Rec68* modes;
  ContinueSlot* slot;
  s32 count = 0;
  s32 bad;
  s32 mode;
  s32 i;

  switch (D_801B608C) {
    case 0:
    case 6:
    case 10:
      recs = D_8012FB60;
      modes = (Rec68*)((u8*)D_8012FB60 + 0x3B0);
      slot = (ContinueSlot*)((u8*)D_8012FB60 - 0x2C8);
      count = 4;
      break;
    case 5:
      recs = D_8012FAA8;
      modes = (Rec68*)((u8*)D_8012FAA8 + 0x400);
      slot = (ContinueSlot*)((u8*)D_8012FAA8 - 0x2B0);
      osSyncPrintf(D_800D092C);
      count = 1;
      break;
    case 4:
      recs = D_8012F938;
      modes = (Rec68*)((u8*)D_8012F938 + 0x508);
      slot = (ContinueSlot*)((u8*)D_8012F938 - 0x1E0);
      count = 2;
      break;
  }

  if (restore == 0) {
    if (count < D_801B6090) {
      osSyncPrintf(D_800D0944);
      return -1;
    }
    bad = 0;
    count = D_801B6090;
    slot->valid = 0;
    for (i = 0; i < count; i++) {
      recs[i] = D_801B71D0[i];
      switch (recs[i].data[3]) {
        case 0:
        case 2:
        case 3:
        case 21:
          recs[i].data[3] = 0;
          break;
        case 17:
        case 20:
          break;
        default:
          osSyncPrintf(D_800D0958);
          bad = 1;
          break;
      }
      modes[i] = D_800FEDD8[i];
    }
    slot->state = *(Rec44*)&D_801B6088;
    slot->camera = *(Rec34*)&camera_position_x;
    if (bad == 0) {
      mode = 1;
      slot->valid = mode;
    } else {
      return -1;
    }
  } else {
    mode = 2;
    if (slot->valid == 1) {
      if (count < (s32)slot->state.data[2]) {
        osSyncPrintf(D_800D0944);
        return -1;
      }
      count = slot->state.data[2];
      for (i = 0; i < count; i++) {
        D_801B71D0[i] = recs[i];
        D_800FEDD8[i] = modes[i];
      }
      *(Rec44*)&D_801B6088 = slot->state;
      D_801B6088 = 1;
      flag_set(0x1E);
      *(Rec34*)&camera_position_x = slot->camera;
    }
    mode = 2;
    D_801B60B8 = mode;
    osSyncPrintf(D_800D0970);
  }
  return 0;
}

extern RecB8 D_801318F8[][4];
extern RecB8 D_8012D0C8[];
extern u32 D_801B55F0;
extern u32 D_800FF4D0;

/* Saves the live match state into continue slot `id` (restore == 0), or
 * restores that slot back over the live state. The three continue slots sit
 * immediately before the per-slot record sets, so both bases come off the one
 * D_801318F8 symbol and the slot base is reached by a negative displacement. A
 * record whose state code is not one of the resumable ones is reported and
 * fails the save. Only the restore side announces itself. Returns -1 on
 * failure, 0 otherwise.
 *
 * `mode` is set twice in the restore arm, and the second set belongs AFTER the
 * inner if, not at the end of its body: that is what colours it $v0 rather than
 * $a0 (S309, found by the permuter at exact instruction count). */
s32 transfer_continue_slot(s32 id, s32 restore) {
  RecB8* recs = D_801318F8[id];
  ContinueSlot* slot = &((ContinueSlot*)((u8*)D_801318F8 - 0x1E0))[id];
  s32 count = 4;
  s32 bad;
  s32 mode;
  s32 i;

  if (restore == 0) {
    if (count < D_801B6090) {
      osSyncPrintf(D_800D0944);
      return -1;
    }
    bad = 0;
    count = D_801B6090;
    slot->valid = 0;
    for (i = restore; i < count; i++) {
      recs[i] = D_8012D0C8[i];
      switch (recs[i].data[3]) {
        case 0:
        case 2:
        case 3:
        case 21:
          recs[i].data[3] = 0;
          break;
        case 17:
        case 20:
          break;
        default:
          osSyncPrintf(D_800D0958);
          bad = 1;
          break;
      }
    }
    slot->state = *(Rec44*)&D_801B55F0;
    if (((s8*)slot)[0x79] < 0x63) {
      ((s8*)slot)[0x79]++;
    }
    slot->camera = *(Rec34*)&camera_position_x;
    slot->misc = *(Rec1C*)&D_800FF4D0;
    if (bad != 0) {
      return -1;
    }
    mode = 1;
    slot->valid = mode;
  } else {
    mode = 2;
    if (slot->valid == 1) {
      if (count < (s32)slot->state.data[2]) {
        osSyncPrintf(D_800D0944);
        return -1;
      }
      count = slot->state.data[2];
      for (i = 0; i < count; i++) {
        D_801B71D0[i] = recs[i];
      }
      *(Rec44*)&D_801B6088 = slot->state;
      D_801B6088 = 1;
      flag_set(0x1E);
      *(Rec34*)&camera_position_x = slot->camera;
      *(Rec1C*)&D_800FF4D0 = slot->misc;
    }
    mode = 2;
    D_801B60B8 = mode;
    osSyncPrintf(D_800D0970);
  }
  if (restore == 0) {
    func_8005DF54(func_8005AF50(), 1);
  }
  return 0;
}

void func_8005C018(u32 arg0) {
  u32* p = &D_8012F724;
  if (*p < arg0) {
    *p = arg0;
  }
}

/* func_8005C038: insert one entry into a category's three-slot record table
 * (four parallel s32[6][3] arrays at 0xD78/0xDC0/0xE08/0xE50 off the save
 * block, sorted ascending by score with the key as tie-break). NEAR-MATCH
 * 268/264, frame -0x60 exact, every control-flow edge and both value-select
 * `beql` clamps reproduced. Residual: my build spills `score` and the fifth
 * argument to their home slots while the ROM keeps them in $s6/$fp and pays to
 * re-materialise `category * 12 + base` instead -- a callee-saved colouring
 * equilibrium, not a spelling. Full RE, the ten measured variants and the
 * attempt body are in docs/wip/func_8005C038.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C038);

s32 func_8005C458(void) {
  u8* ret = func_8005AF50();
  s32 count = 0, i = 0, n = 6;
  u8* base = ret;
  do {
    u32 p = (u32)base + 0x1320;
    u32 end = n + p;
    do {
      count += (*(s8*)p != 0);
      p++;
    } while (p != end);
    base += 6;
    i++;
  } while (i != n);
  return count == 36;
}

s32 func_8005C4B4(void) {
  u8* ret = func_8005AF50();
  s32 count = 0, i = 0, n = 6;
  u8* base = ret;
  do {
    u32 p = (u32)base + 0x1320;
    u32 end = n + p;
    do {
      count += (*(s8*)p != 0);
      p++;
    } while (p != end);
    base += 6;
    i++;
  } while (i != n);
  return count >= 30;
}

s32 func_8005C510(void) {
  u8* ret = func_8005AF50();
  s32 count = 0, i = 0, n = 6;
  u8* base = ret;
  do {
    u32 p = (u32)base + 0x1320;
    u32 end = n + p;
    do {
      count += (*(s8*)p != 0);
      p++;
    } while (p != end);
    base += 6;
    i++;
  } while (i != n);
  if (count == 4) {
    return 1;
  }
  if (count == 8) {
    return 2;
  }
  if (count == 12) {
    return 3;
  }
  if (count == 16) {
    return 4;
  }
  return (count == 20) ? 5 : 0;
}

s32 func_8005C5B4(void) {
  u8* ret = func_8005AF50();
  s32 count = 0, i = 0, n = 18, lim = 6;
  u8* base = ret;
  do {
    u32 p = (u32)base + 0xA84;
    u32 end = n + p;
    do {
      count += (*(s8*)p != 0);
      p++;
    } while (p != end);
    base += 18;
    i++;
  } while (i != lim);
  return count >= 50;
}

s32 func_8005C614(void) {
  u8* ret = func_8005AF50();
  s32 count = 0, i = 0, n = 18, lim = 6;
  u8* base = ret;
  do {
    u32 p = (u32)base + 0xA84;
    u32 end = n + p;
    do {
      count += (*(s8*)p != 0);
      p++;
    } while (p != end);
    base += 18;
    i++;
  } while (i != lim);
  return count == 108;
}

extern s16 D_800C28E4[3][7];
extern s16 D_800C298C[3][7];
extern s16 D_800C2910[3][7];
extern s16 D_800C293C[3][7];
extern s16 g_abRosterUnlockOrder[];
extern s16 g_abRosterUnlockFlags[];
extern s16 D_800C29B8[10][2];
extern u16 D_800C2650[];
extern s16 D_800C2590[];
extern s16 D_800FE3D8[];
extern void* D_8012D468[];
extern void* D_80105BB0[];
extern s32 flag_is_set(s32 flag);
extern void func_8005CA48(void);
extern s32 func_8005C4B4(void);
extern s32 func_8005C5B4(void);
extern s32 func_8005C614(void);
extern u32 func_8005062C(u16 index, void* out);
extern void func_800506D4(void* data, RomLoadSlot* slot);
extern void func_800504E8(s32 index, RomLoadSlot* slot);
extern u32 func_80050598(RomLoadSlot* slot);
extern void func_800505A0(void* dst, u32 size, RomLoadSlot* slot);

/* Builds the character-select roster grid D_800C28E4[3][7] from its template
 * tables, patches it against the unlock state, then loads the four selected
 * characters' assets through the standard five-call RomLoadSlot block.
 *
 * The variable reuse below is deliberate and load-bearing: the ROM keeps arg0,
 * the unlock-scan counter and the asset index in one register, and reads the
 * unlock flag into the same register as the scan counter. Giving any of them
 * its own local permutes the whole allocation. */
void build_roster_grid(s32 arg0, u8 arg1) {
  RomLoadSlot buf_a[2];
  RomLoadSlot buf_b;
  u8* stats;
  s32 row;
  s32 col;
  s32 i;
  s32 k;
  u32 size;
  s32 off;
  s32 wide;
  s32 narrow;

  stats = func_8005AF50();
  row = 0;
  do {
    col = 0;
    do {
      D_800C28E4[row][col] = D_800C298C[row][col];
      col++;
    } while (col != 7);
    row++;
  } while (row != 3);
  if (flag_is_set(0x54)) {
    row = 0;
    do {
      col = 0;
      do {
        D_800C28E4[row][col] = D_800C2910[row][col];
        col++;
      } while (col != 7);
      row++;
    } while (row != 2);
    func_8005CA48();
    return;
  }
  if (arg0 >= 2) {
    row = 0;
    do {
      col = 0;
      do {
        D_800C28E4[row][col] = D_800C2910[row][col];
        col++;
      } while (col != 5);
      row++;
    } while (row != 2);
  }
  i = 0;
  do {
    k = 0;
    arg0 = 0;
    for (; k != 18; k++) {
      s32 slot = k * 0x24 + i * 2;
      if ((*(stats + slot + 0xAF1) & 0x7F) != 0) {
        arg0++;
        break;
      }
    }
    if (arg0 > 0) {
      for (row = 0; row != 2; row++) {
        for (col = 0; col != 7; col++) {
          if (D_800C293C[row][col] == i) {
            D_800C28E4[row][col] = g_abRosterUnlockOrder[i];
          }
        }
      }
    }
    i++;
  } while (i != 13);
  if (func_8005C5B4()) {
    D_800C28E4[0][5] = 10;
  }
  if (func_8005C4B4()) {
    D_800C28E4[1][5] = 11;
  }
  if (func_8005C614()) {
    D_800C28E4[1][6] = 13;
  }
  func_8005CA48();
  for (i = 0; i != 10; i++) {
    row = D_800C29B8[i][1];
    if (D_800C28E4[row][D_800C29B8[i][0]] < 0) {
      D_800C28E4[row][D_800C29B8[i][0]] = i - 0x32;
      break;
    }
  }
  if (i == 10) {
    if (D_800C28E4[0][6] != 12) {
      D_800C28E4[0][6] = -0x26;
    }
  }
  if (arg1 == 1) {
    return;
  }
  for (i = 0; i != 4; i++) {
    k = g_abRosterUnlockFlags[i];
    if (k >= 0) {
      off = i * 0x16;
      arg0 = *(s16*)((u8*)D_800FE3D8 + off);
      wide = arg0 * 16;
      func_8005062C(*(u16*)((u8*)D_800C2650 + wide), buf_a);
      func_800506D4(D_8012D468[i], &buf_a[0]);
      narrow = arg0 * 8;
      func_800504E8(*(s16*)((u8*)D_800C2590 + narrow), &buf_b);
      size = func_80050598(&buf_b);
      func_800505A0(D_80105BB0[i], size, &buf_b);
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005CA48);

extern s16 D_800C2BB0[];

/* Rebuilds the roster grid, then reports whether the grid cell that entry
 * `arg0` maps to holds a negative value. D_800C2BB0 maps the entry to a row of
 * the D_800C29B8 coordinate-pair table, whose second halfword is the grid row
 * and whose first is the column. */
s32 is_roster_entry_locked(s32 arg0) {
  s32 idx;
  s32 row;
  s32 col;

  if (arg0 < 0) {
    goto out_of_range;
  }
  if (arg0 < 15) {
    goto in_range;
  }
out_of_range:
  return -1;
in_range:
  build_roster_grid(1, 1);
  idx = D_800C2BB0[arg0];
  row = D_800C29B8[idx][1];
  col = D_800C29B8[idx][0];
  return ((s32)((u16)D_800C28E4[row][col] << 16)) >> 31;
}

typedef struct {
  /* 0x00 */ s32 active;
  /* 0x04 */ s8 pad04[0x3C - 0x04];
  /* 0x3C */ s32 unk_3C;
  /* 0x40 */ s32 sel;
  /* 0x44 */ s32 count;
  /* 0x48 */ s32 unk_48;
  /* 0x4C */ s32 unk_4C;
  /* 0x50 */ s8 pad50[0x76 - 0x50];
  /* 0x76 */ s8 f76;
  /* 0x77 */ s8 pad77[0xA0 - 0x77];
} Cf78Section;

typedef struct {
  /* 0x00 */ s8 pad00[0x1D];
  /* 0x1D */ u8 f1D;
  /* 0x1E */ s8 pad1E[0x7B - 0x1E];
  /* 0x7B */ u8 f7B;
  /* 0x7C */ s8 pad7C[0xB8 - 0x7C];
} Cf78Row;

void func_8005CF78(void) {
  u8* base = func_8005AF50();
  s32 flags[3];
  s32 k;

  k = 0;
  do {
    flags[k] = 0;
    k++;
  } while (k != 3);

  k = 0;
  do {
    if (*(s32*)(base + k * 0xA0 + 0x1FF8) != 0) {
      Cf78Section* sect = (Cf78Section*)(base + 0x1FF8 + k * 0xA0);
      s32* a3 = &sect->unk_3C;

      if (sect->f76 == 0) {
        switch (sect->sel) {
          case 0:
          case 2:
          case 4:
          case 5:
          case 10:
          case 11:
            break;
          default:
            flags[k] = 1;
            break;
        }
      }
      if ((u32)(a3[2] - 1) >= 4U) {
        flags[k] = 1;
      }
      if ((u32)a3[3] >= 0xCU) {
        flags[k] = 1;
      }
      if ((u32)a3[4] >= 0x12U) {
        flags[k] = 1;
      }
      {
        s32 i = 0;
        Cf78Row* rows;
        while (i != a3[2]) {
          rows = (Cf78Row*)(base + 0x21D8 + k * 0x2E0);
          if (rows[i].f1D >= 0x12) {
            flags[k] = 1;
          }
          if (rows[i].f7B >= 0x4) {
            flags[k] = 1;
          }
          i++;
        }
      }
    }
    k++;
  } while (k != 3);

  k = 0;
  do {
    if (flags[k] == 1) {
      *(s32*)(base + k * 0xA0 + 0x1FF8) = 0;
    }
    k++;
  } while (k != 3);
}

/* func_8005D0D8: validates the 5 game-state entries at base + 0x1DF0, stride
 * 0x68, and toggles bit 7 of byte 0x36 on every entry that fails. CARRIED at
 * 67/67 with the ROM's exact -0x38 frame; the residual is a three-register
 * rotation (i, the flags giv, the entry pointer). Body in
 * nonmatchings/func_8005D0D8/base.c, analysis in
 * docs/wip/func_8005D0D8.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D0D8);

void func_8005D1E4(void) {
  u8* p = func_8005AF74();
  p[0x2E] = D_801B6098;
  p[0x2F] = scenario_mode_id;
}

s32 func_8005D218(void) {
  switch (D_80105DC1) {
    case 0:
    default:
      return 0;
    case 1:
      return 7;
  }
}

s32 func_8005D23C(void) { return D_80105DC2; }

s32 func_8005D248(void) { return D_80105DC4 != 0; }

s32 func_8005D258(void) {
  s32 x = D_80105DC8;
  return (x & (~x >> 31)) + 0x16F;
}

s32 func_8005D274(void) {
  s32 v = func_8005D23C();
  return (v & (~v >> 31)) + 0x1EB;
}

s32 func_8005D2A0(void) { return (D_80105DCC > 0) ? D_80105DCC : 1; }

void func_8005D2B8(u8* arg0) {
  s32 i = 0;
  do {
    *arg0++ = D_8012D400[i];
  } while (++i != 3);
}

s32 func_8005D2E4(void) {
  switch (D_80105DC1) {
    case 0:
    default:
      return 158;
    case 1:
      return 157;
  }
}

s32 func_8005D308(void) {
  s32 x = D_80105DC2;
  s32 r;

  if (x >= 6) {
    if (x >= 9) {
      r = 0xC8;
    } else {
      r = 0xC7;
    }
  } else {
    r = x + 0x1EB;
  }
  return r;
}

/* Zeroes all four active-player slots: the four s16 fields, the 12-byte name,
 * and the trailing flag of each 0x16-stride record at D_800FE3D8.
 *
 * Two shapes here are codegen levers, not style. The outer loop is a goto loop
 * because a structured one lets loop.c hoist the `!= 4` bound into its own
 * register, where the ROM rematerializes it inline each iteration. The
 * do {} while (0) is a loop note that reweights the refs inside it, which is
 * what colours `end` below `row` and above the counter ($a1/$a2 as the ROM has
 * them); without it the counter outranks `end` and the two swap. */
void clear_player_slots(void) {
  s32 i;
  s32 n;
  s32 off;
  u8* row;
  u8* p;
  u8* end;

  i = 0;
  do {
    n = 12;
    off = 0;
    row = (u8*)D_800FE3D8 + 8;
  loop:
    p = row;
    end = (u8*)(n + (s32)row);
    *(s16*)((u8*)D_800FE3D8 + off) = 0;
    *(s16*)((u8*)D_800FE3D8 + off + 2) = 0;
    *(s16*)((u8*)D_800FE3D8 + off + 4) = 0;
    *(s16*)((u8*)D_800FE3D8 + off + 6) = 0;
    do {
      *p = 0;
      p++;
    } while (p != end);
  } while (0);
  *((u8*)D_800FE3D8 + off + 0x14) = 0;
  off += 0x16;
  i++;
  row += 0x16;
  if (i != 4) {
    goto loop;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D3B8);

void func_8005D9A0(void) {
  s8 i;

  D_800C2BD0 = 6;
  D_800FE334 = 0;
  current_game_mode = 0;
  D_80105B68 = 0;
  player_context_offset = 0;
  D_8012F480 = -1;
  D_8012F484 = -1;
  D_8012F488 = -1;
  D_8012F48C = -1;
  D_801B70F0 = 0;
  D_801B70F1 = 0;
  D_801B70F2 = 0;
  D_801B70F3 = 0;
  memset(D_801323A0, 0, 0x49);
  D_800FBD40 = 0;
  D_801B71CC = 0;
  D_800FBE2C = 0;
  D_800FBDB9 = 0;
  D_800FE33C = 0;
  D_800FF4C0 = 0;
  D_80105268 = 0;
  i = 0;
  do {
    D_800FBE08[i] = 0;
  } while (++i != 4);
  D_801323B8 = 3;
  D_801B60AC = 0;
  D_801323D0 = 6;
  D_801052E8 = 0;
  D_800C2BD4 = 0;
}

void func_8005DAD4(void) {
  D_800FE334 = 0;
  D_801052E8 = 1;
  D_801B9410 = 0;
  D_800C2BD4 = 0;
}

void func_8005DAFC(void) {
  switch (D_801B608C) {
    case 5:
      D_800FF4C0 = 2;
      D_80105268 = 0;
      D_801B5634 = 0;
      break;
    case 0:
      D_800FF4C0 = 1;
      D_80105268 = 1;
      if (D_80105B68 == 4) {
        D_801B5634 = 2;
      } else {
        D_801B5634 = 1;
      }
      D_800C2BD5 = 1;
      break;
    case 2:
      D_800FF4C0 = 1;
      D_80105268 = 1;
      D_801B5634 = 3;
      break;
    case 10:
      D_800FF4C0 = 1;
      D_80105268 = 1;
      D_801B5634 = 4;
      break;
    case 3:
    case 4:
    case 6:
    case 7:
    case 9:
      D_800FF4C0 = 1;
      D_80105268 = 2;
      D_801B5634 = 0;
      break;
    case 11:
      D_800FF4C0 = 1;
      D_80105268 = 1;
      D_801B5634 = 5;
      break;
    default:
      break;
  }
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 6;
}

void func_8005DC50(void) {
  func_8005D9A0();
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  D_800FE3EC = 0;
  D_800FE402 = 0;
  D_800FE418 = 0;
  D_800FE42E = 0;
  D_800C1FF4 = -1;
  D_800C1FF5 = -1;
  D_800C1FF6 = -1;
  D_800C1FF7 = -1;
  current_game_mode = 0;
}

void func_8005DCDC(void) {
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 0x17;
}

void func_8005DD10(void) {
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 5;
}

void func_8005DD44(void) {
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 0x12;
}

void func_8005DD78(void) {
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 0xD;
}

void func_8005DDAC(void) {
  D_800FE334 = 0;
  D_801052E8 = 1;
  D_800C2BD4 = 0;
  D_801B9410 = 1;
  rumble_disable_flag = 0;
  flag_clear(0x1E);
  current_game_mode = 0xC;
}

s8 func_8005DE00(void) { return ((s8*)func_8005AF50())[0x28]; }

s8 func_8005DE20(void) { return ((s8*)func_8005AF50())[0x29]; }

s8 func_8005DE40(void) { return ((s8*)func_8005AF50())[0x2B]; }

void func_8005DE60(void) { func_8005DF54(func_8005AF50(), 1); }

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DE88);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DF54);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DFE8);

extern u8 D_801B5588[];
extern u32 D_800C2BE0;
extern u32 D_800C2BE4;
extern u32 D_800C2BE8;
extern u32 D_800C2BEC;
extern s32 func_80028110(u32 addr, s32 len);

s32 func_8005E180(u8* buf, s32 save_flag) {
  u8 raw[0xC0];
  u16* stat = (u16*)(((u32)raw + 0xF) & ~0xF);
  s32 i;
  s32 j;
  s32 flag;
  u16 crc;
  s32 off;

  for (j = 0; j < 0x20; j++) {
    u8* dst = buf + j;
    dst[8] = ~D_801B5588[j];
  }
  crc = crc16_ccitt(buf, 0x2A78);
  i = 0;
  flag = save_flag & 0xFF;
  off = 0;
  do {
    s32 r = rand();
    stat[0] = r;
    stat[1] = r;
    stat[1] = stat[1] ^ crc;
    if (flag == 0) {
      if (func_80028110(*(u32*)((u8*)&D_800C2BE0 + off), 0x2A78) == 0) {
        func_80028110(*(u32*)((u8*)&D_800C2BE4 + off), 4);
      }
    } else {
      nuPiReadWriteSram(*(u32*)((u8*)&D_800C2BE8 + off), buf, 0x2A78, 1);
      nuPiReadWriteSram(*(u32*)((u8*)&D_800C2BEC + off), stat, 4, 1);
    }
    i++;
    off += 0x10;
  } while (i < 2);
  return 1;
}
