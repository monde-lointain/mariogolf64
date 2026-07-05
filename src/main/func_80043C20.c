#include "common.h"

typedef struct {
  u8 unk0;      // 0x00
  u8 pad01[7];  // 0x01
  s32 unk8;     // 0x08
  s16 unkC;     // 0x0C
  s16 unkE;     // 0x0E
  u8 pad10[4];  // 0x10
  s16 unk14;    // 0x14
  u8 pad16[2];  // 0x16
  s32 dist[3];  // 0x18, 0x1C, 0x20
  u8 pad24[2];  // 0x24
  s16 unk26;    // 0x26
  s16 unk28;    // 0x28
  s16 unk2A;    // 0x2A
  s16 unk2C;    // 0x2C
  u8 pad2E[2];  // 0x2E
  s8 unk30;     // 0x30
  u8 pad31[3];  // 0x31
} ClubShot;     // 0x34

typedef struct {
  s32 w[6];  // 0x00..0x14
} ClubSrc;   // 0x18

extern ClubShot D_800BB258[];
extern ClubShot club_param_fallback;
extern s8 D_800BE5E8[];
extern s16 D_801B7222[][92];
extern s32 active_player_idx;
extern s32 active_club_id;
extern s32 active_meter_category;
extern void* D_800FBE70;

extern s32 flag_is_set(s32 flag);
extern s32 get_club_meter_extent(void* club, s32 category);
extern s32 get_club_meter_units(void* club, s32 category);
extern void* get_table_entry(void* table);
extern u8* func_80043AF0(s32 index);

// Source struct that func_80044254's arg1 points at: fixed fields at 0x1C-0x1F,
// and a 6-byte-stride array region from 0x3C (unk3C/unk3E/unk41 read via a
// running pointer stepping +6 per iteration).
typedef struct {
  u8 pad00[0x1C];
  u8 unk1C;  // 0x1C
  u8 unk1D;  // 0x1D
  u8 unk1E;  // 0x1E
  s8 unk1F;  // 0x1F
  u8 pad20[0x1C];
  u16 unk3C;  // 0x3C
  u16 unk3E;  // 0x3E
  u8 pad40;   // 0x40
  u8 unk41;   // 0x41
} ShotInput;

extern s32 active_player_idx_alt;  // 0x800FBE78 (active_club_id/idx/idx_alt are
                                   // adjacent @0x800FBE58/74/78)
extern u16 D_800FED18[];
extern u8 D_800BE5E0[];
extern u8 D_800CC510[];
extern u8 D_800CC520[];
extern s8 g_terrain_vtx_xform_mode;
extern s32 func_8021EA48(void);
extern void play_sound_effect(s32 sfx, s32 arg1, s32 arg2);
extern void* heap3_alloc(s32 size);
extern void heap3_free(void* ptr);
extern void func_80028110(void* buf, s32 len);
s32 predict_shot_distance_variant(s32 idx, s32 category);
extern char D_800CC370[], D_800CC374[], D_800CC388[], D_800CC3EC[],
    D_800CC40C[];
extern char D_800CC454[], D_800CC474[], D_800CC478[], D_800CC480[],
    D_800CC484[], D_800CC488[];

s32 resolve_club_terrain_mask(s32 arg0, void* arg1);
s32 resolve_shot_quality_table(s32 arg0, s32 arg1, s32 arg2);
void predict_shot_distance(s32 index);
void func_80043C64(void);
ClubShot* get_shot_data(void);
ClubShot* get_club_param(u32 id);

// Debug club-data dump. func_80043C20 is a GCC nested string-appender (static
// chain = &line, with buf_pos/buf_end reached at chain+0x100/+0x104); GCC 2.7.2
// emits it immediately before this parent, landing it at the pack-lead vram
// 0x80043C20.
void func_80043C64(void) {
  char line[0x100];
  u8* buf_pos;
  u8* buf_end;
  u8* buf_start;
  ClubShot* cs;
  s32 club;
  s32 category;
  s32 power;

  void func_80043C20(u8 * s) {
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

  buf_start = heap3_alloc(0x8000);
  buf_pos = buf_start;
  buf_end = buf_start + 0x8000;
  if (buf_start == NULL) {
    __assert(D_800CC370, D_800CC374, 0x21F);
  }
  sprintf(line, D_800CC388);
  func_80043C20((u8*)line);
  club = 0;
  do {
    cs = &D_800BB258[club];
    sprintf(line, D_800CC3EC, cs->dist[0], cs->dist[1], cs->dist[2], cs->unk28,
            cs->unk2A, cs->unk2C);
    club++;
    func_80043C20((u8*)line);
  } while (club < 0xFC);
  sprintf(line, D_800CC40C);
  func_80043C20((u8*)line);
  func_80043C20((u8*)D_800CC454);
  category = 0;
  do {
    func_80043C20((u8*)D_800CC474);
    power = 1;
    do {
      sprintf(line, D_800CC478,
              predict_shot_distance_variant((power << 8) / 30, category));
      func_80043C20((u8*)line);
      power++;
    } while (power < 0x1F);
    func_80043C20((u8*)D_800CC480);
    category++;
  } while (category < 3);
  func_80043C20((u8*)D_800CC484);
  line[0] = 0x1A;
  line[1] = 0;
  func_80043C20((u8*)line);
  sprintf(line, D_800CC488);
  func_80028110(line, buf_pos - buf_start);
  heap3_free(&buf_start);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", lerp_int_v2);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", predict_shot_distance);

void func_80044254(s32 arg0, ShotInput* arg1) {
  s32 i;
  s32 club;
  ShotInput* p;
  ClubShot* cs;
  u16 temp;
  s32 var_v1;
  s16 temp_v0_2;
  s32 hundred;
  u8 temp_v0_3;
  u8 temp_v1_2;
  u8 temp_v0_4;

  i = 0;
  hundred = 100;
  club = arg0 * 0xE;
  do {
    p = (ShotInput*)((u8*)arg1 + i * 6);
    cs = get_club_param(club);
    temp = p->unk3C;
    cs->unkC = temp;
    if ((s16)temp >= 0x191) {
      cs->unkC = 0x190;
    }
    osSyncPrintf(&D_800CC510, cs->unkC);
    var_v1 = p->unk3E * 0x168;
    if (var_v1 < 0) {
      var_v1 += 0xFFFF;
    }
    temp_v0_2 = (s8)arg1->unk1C + (var_v1 >> 0x10);
    cs->unkE = -temp_v0_2;
    osSyncPrintf(&D_800CC520, (s16)-temp_v0_2);
    if (cs->unkE >= -6) {
      cs->unkE = -7;
    }
    if (cs->unkE < -0x2D) {
      cs->unkE = -0x2D;
    }
    temp_v0_3 = p->unk41;
    cs->unk26 = (s8)temp_v0_3;
    if (cs->unk26 >= 0x65) {
      cs->unk26 = hundred;
    }
    if (cs->unk26 < -0x64) {
      cs->unk26 = -0x64;
    }
    temp_v1_2 = arg1->unk1E;
    if ((u32)((temp_v1_2 + 4) & 0xFF) >= 9U) {
      cs->unk14 = hundred;
    } else {
      cs->unk14 = D_800BE5E0[(s8)temp_v1_2];
    }
    cs->unk0 = 0x6E - (arg1->unk1F * 9);
    temp_v0_4 = arg1->unk1D;
    cs->unk30 = temp_v0_4;
    if ((s8)temp_v0_4 < -4) {
      cs->unk30 = -4;
    }
    if (cs->unk30 >= 5) {
      cs->unk30 = 4;
    }
    if (cs->unk0 >= 0x92) {
      cs->unk0 = 0x91;
    }
    if (cs->unk0 < 0x46) {
      cs->unk0 = 0x46;
    }
    g_terrain_vtx_xform_mode = 0;
    predict_shot_distance(club);
    club += 1;
    i += 1;
  } while (i < 0xE);
}

// func_80044470 is a GCC nested function of func_800444B8 (static chain in
// $v0); GCC 2.7.2 emits it immediately before the parent. Defined inside
// func_800444B8 below.

void func_800444B8(void) {
  s32 i;
  s8 func_80044470(s32 x) {
    s32 q = x / 14;
    if ((u32)q >= 0x12) {
      q = 0;
    }
    return D_800BE5E8[q];
  }
  if (!flag_is_set(0x1F)) {
    for (i = 0; i < 0xC4; i++) {
      ClubShot* dst = &D_800BB258[i];
      ClubSrc* src = (ClubSrc*)func_80043AF0(i);
      dst->dist[0] = src->w[0];
      dst->dist[1] = src->w[1];
      dst->dist[2] = src->w[2];
      dst->unk28 = src->w[3];
      dst->unk2A = src->w[4];
      dst->unk2C = src->w[5];
      dst->unk30 = func_80044470(i);
    }
    return;
  }
  if (flag_is_set(0x1F)) {
    for (i = 0; i < 0xFC; i++) {
      predict_shot_distance(i);
    }
    func_80043C64();
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", lerp_int);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20",
            predict_shot_distance_variant);

s32 get_club_distance_slot(ClubShot* c, s32 i) { return c->dist[i]; }

ClubShot* get_club_param(u32 id) {
  ClubShot* result;
  if (id < 0xFC) {
    result = &D_800BB258[id];
  } else {
    result = &club_param_fallback;
  }
  return result;
}

ClubShot* get_shot_data(void) {
  return &D_800BB258[D_801B7222[active_player_idx][active_club_id]];
}

s32 get_shot_param(void) {
  return get_club_meter_extent(get_shot_data(), active_meter_category);
}

s32 get_shot_progress(void) {
  return get_club_meter_units(get_shot_data(), active_meter_category);
}

// The switch funnels through `r` + a single trailing `return r` (NOT per-case
// returns): this gives its cases a distinct switch-exit label from the early
// `if (g_terrain) return 1`, so GCC 2.7.2's jump.c cross-jump (find_cross_jump,
// rtx_renumbered_equal_p on the jump target) does not tail-merge case 11's
// `return 1` with the g_terrain `return 1`. Merging them would give g_terrain's
// return block 2 predecessors, blocking reorg from folding it to the ROM's
// tight `bnez end`+delay. The three early `if` returns stay direct (they must
// jump to the true epilogue for that fold). See
// docs/hazards.md#cross-jump-tail-merge (S185 compiler-source fan-out).
s32 resolve_club_terrain_mask(s32 arg0, void* arg1) {
  s32 terrain = *(s32*)arg1;
  s32 r;

  if (flag_is_set(0x98)) {
    return -1;
  }
  if (g_terrain_vtx_xform_mode != 0) {
    return 1;
  }
  if (arg0 == 0) {
    return (terrain == 0xB) ? 1 : -1;
  }
  switch (terrain) {
    case 3:
    case 16:
    case 18:
    case 19:
    case 23:
      r = 2;
      break;
    case 11:
      r = 1;
      break;
    case 4:
    case 6:
    case 7:
    case 8:
      r = 4;
      break;
    case 15:
      r = 8;
      break;
    case 0:
    case 1:
    case 2:
      r = 0x10;
      break;
    case 17:
      r = 0x20;
      break;
    default:
      r = -1;
      break;
  }
  return r;
}

s32 func_80044A8C(s32 arg0) {
  return resolve_club_terrain_mask(arg0, get_table_entry(D_800FBE70));
}

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", resolve_shot_quality_table);

void func_80044C7C(s32 arg0, s32 arg1, s32* out, s32 arg3) {
  s32 n = resolve_shot_quality_table(arg0, arg1, arg3);
  if (n > 0) {
    out[0] = n * 2 - 1;
    out[1] = n - 1;
  } else {
    out[0] = 0;
    out[1] = 0;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_80044CCC);

void func_80044FDC(void) {
  s32 club;
  u16 flags;

  if (func_8021EA48() != 0) {
    s32* alt =
        &active_player_idx_alt;  // 0x800FBE78; base=alt-1=&active_player_idx,
                                 // base[-7]=active_club_id
    flags = D_800FED18[*alt * 4];
    if (flags & 0x400) {
      s32* base = alt - 1;
      for (club = active_club_id + 1; club < 0x10; club++) {
        if (D_801B7222[base[0]][club] != -1) {
          if (D_800BB258[D_801B7222[base[0]][club]].unk8 & func_80044A8C(0)) {
            base[-7] = club;
            play_sound_effect(0x5B, 0xF, 0x7F);
            return;
          }
        }
      }
    } else if (flags & 0x800) {
      s32* base = alt - 1;
      for (club = active_club_id - 1; club >= 0; club--) {
        if (D_801B7222[base[0]][club] != -1) {
          if (D_800BB258[D_801B7222[base[0]][club]].unk8 & func_80044A8C(0)) {
            base[-7] = club;
            play_sound_effect(0x5B, 0xF, 0x7F);
            return;
          }
        }
      }
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_800451E4);
