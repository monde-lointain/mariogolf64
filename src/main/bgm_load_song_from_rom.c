#include "common.h"
#include "session.h"
#include "club.h"
#include <nusys.h>
#include <nualstl.h>

extern musHandle try_spawn_global_object(int number);
extern void gen_terrain_detail_texture(s32 arg0, s32 arg1);
extern void func_80407D64(void);
extern s32 get_lowest_height_at_position(s32 x, s32 z);
extern s32 D_800FBE68;

extern s32 transfer_mode_continue_slot(s32 restore);
extern void func_80216B74(s32 arg0);
extern s32 flag_is_set(s32 flag);
extern void func_8005F360(void);
extern void func_8005F4AC(void);
extern void func_80060210(GameModeRecord* arg0);
extern void initialize_hole_handicap_ranks(void);
extern void init_per_player_state(void);
extern void init_sky_panels(void);
extern void func_80054E20(void);
void setup_terrain_detail_default(void);

extern u32 scenario_mode_id;
extern s32 game_mode;
extern u8 D_800FE330;
extern const char D_800D1254[];
extern void* __MusIntMemMalloc(s32 size);
extern s32 rand(void);
extern u8 g_terrain_detail_texture[];
extern s32 D_800C3070;
extern s32 D_800C3074;
extern u16 D_800C3078;
extern u16 D_800C307A;
extern s32 D_800C307C;
extern s32 D_800C3080;
extern s32 D_800C3084;
extern s32 D_800C3088;
extern s32 D_800C308C;
extern s32 D_800C3090;

extern ScenarioState D_801B55F0;
extern GameModeRecord D_800FEDD8[];
extern CourseSelectionRecord D_80105F40[];
extern ScenarioState scenario_state_id;
extern PlayerState player_records[];
extern PlayerState D_8012D0C8[];
extern GameModeRecord D_800FED40[];
extern s8 D_800FEE10;
extern s8 D_800FEE11;
extern s8 D_801B60B8;
extern s8 D_801B72A6;
extern s32 active_player_count;
extern s32 round_hole_index;
extern s32 D_801B609C;
extern const char D_800D1240[];

extern u32 g_bgm_song_buffer;
extern u8 g_terrain_tile_cull_exclude[];
extern u8 D_800C30A4;
extern s32 D_800FBE88;
extern s32 D_800B67C4;
extern const char D_800D1238[];
extern u32 D_800C2FF4;
extern const char D_800D1220[];
extern s8 g_terrain_vtx_xform_mode;
extern u16 D_80104F78;
extern s32 D_80104F7C;
extern s32 D_80104F80;
extern s32 D_80104F84;
extern s32 D_80104F88;
extern s32 D_80104F8C;
extern s32 D_80104F90;
extern s32 D_800C306C;
/* State, game mode, player count, and scenario mode share this word block. */
/* scenario_state_id is the first field of the 0x44-byte snapshot above. */
extern s32 D_800C30A8;
extern s32 D_800C30AC;
extern s32 func_80044A8C(s32 category);
extern s8 D_800FBDB8;
extern s8 rumble_disable_flag;
extern s32 get_course_hole_primary_flag(s32 course_id, s32 hole_index);
extern s32 get_course_hole_secondary_flag(s32 course_id, s32 hole_index);
extern s32 func_8006D1FC(void);
extern s32 func_8006DF84(void);
extern void func_8020FFEC(s32 event);
extern s32 func_8020FFD8(void);

void bgm_load_song_from_rom(u32 rom_addr, s32 size) {
  nuPiReadRom(rom_addr, (void*)g_bgm_song_buffer, size + (size & 1));
}

void func_8005F0BC(s32 size) { D_800C2FF4 = (u32)__MusIntMemMalloc(size); }

musHandle spawn_object_simple(int number) {
  return try_spawn_global_object(number);
}

void func_8005F0FC(int number, int volume, int pan, int restartflag,
                   int priority) {
  MusStartEffect2(number, volume, pan, restartflag, priority);
}

void func_8005F11C(u32 rom_addr, s32 size) {
  s32 rounded = (size & 1) + size;

  if ((u32)rounded >= 0x4001) {
    osSyncPrintf(D_800D1220);
  }
  nuPiReadRom(rom_addr, (void*)D_800C2FF4, rounded);
  MusFxBankInitialize((void*)D_800C2FF4);
}

s32 func_8005F180(void) {
  s32 r = 0;

  if (flag_is_set(0x32)) {
    r = 1;
  } else if (flag_is_set(0x9B)) {
    r = 1;
  }
  return r;
}

void func_8005F1C8(void) {
  D_800C306C = 0;
  D_800C3070 = 0;
  D_800C3074 = 0;
  D_800C3078 = 0;
  D_800C3084 = 0;
  D_800C3088 = 0;
  D_800C308C = 0;
  D_800C3090 = 0;
  D_800C307A = 0;
  D_800C307C = 0;
  D_800C3080 = 0;
}

s32 func_8005F224(void) {
  s32 temp = D_800FBE88;

  if (temp >= 0x1F) {
    temp = 0x3C - temp;
  }
  return temp;
}

s32 func_8005F248(s32 arg0) {
  s32 x;

  if (arg0 < 0x1F) {
    x = arg0;
  } else {
    x = 0x3C - arg0;
  }
  if (x < 0) {
    x = 0;
  }
  return (x << 8) / 30;
}

s32 func_8005F290(s32 unused, s16* input, u8* output) {
  s32 i = 0;
  u8* enabled = output;
  s16* clubs = input;
  for (; i < 14;) {
    *enabled =
        (get_club_param(*clubs++)->flags & func_80044A8C(1)) != 0;
    enabled++;
    i++;
  }
  return 14;
}

s32 func_8005F30C(s32 x, s32 z) {
  s32 height = get_lowest_height_at_position(x, z);

  if ((g_terrain_vtx_xform_mode == 0) & (height > 0)) {
    s32 limit = D_800FBE68;
    s32 positive;
    /* Keep the predicate separate from the mask to preserve slt/negu/and. */
    do {
      positive = limit > 0;
    } while (0);
    height = limit & -positive;
  }
  return height;
}

void func_8005F360(void) {
  s32 flags = (flag_is_set(0x33) << 1) + flag_is_set(0x34);

  switch (flags) {
    case 0:
      scenario_mode_id = 0;
      round_hole_index = 0;
      D_801B609C = 0;
      flags = (flag_is_set(0x30) << 1) + flag_is_set(0x31) + 1;
      active_player_count = flags;
      scenario_state_id.state_id = 0;
      game_mode = 0;
      break;
    case 1:
      scenario_mode_id = 0;
      round_hole_index = 0;
      D_801B609C = 0;
      active_player_count = flags;
      scenario_state_id.state_id = 0;
      game_mode = 3;
      break;
    case 2:
      scenario_mode_id = 0;
      round_hole_index = 0;
      D_801B609C = 0;
      active_player_count = flags;
      scenario_state_id.state_id = 0;
      game_mode = flags;
      /* The original case deliberately falls through to the default setup. */
    default:
      active_player_count = 3;
      scenario_mode_id = 0;
      round_hole_index = 0;
      D_801B609C = 0;
      scenario_state_id.state_id = 0;
      game_mode = 10;
      break;
  }
}

void func_8005F4AC(void) {
  /* A byte view preserves the retail scalar-alias barrier at this store. */
  u8* mode_id = (u8*)&scenario_state_id.scenario_mode_id;
  *(s32*)mode_id = 0;
  active_player_count = 1;
  round_hole_index = 0;
  D_801B609C = 0;
  scenario_state_id.state_id = 11;
  if (flag_is_set(0x35)) {
    scenario_state_id.game_mode = 9;
  } else {
    game_mode = 0;
  }
}

void func_8005F518(s32 arg0, char* arg1) {
  sprintf(arg1, D_800D1238, arg0 + 1);
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom",
            init_per_player_state);

void func_8005F838(s32 arg0) {
  if (arg0 == 1) {
    func_8005F360();
  } else if (arg0 == 0) {
    func_8005F4AC();
  } else if (arg0 == 2) {
    func_80060210(D_800FED40);
    D_801B609C = 0;
    active_player_count = 1;
    scenario_state_id.state_id = 0;
    game_mode = 0;
    scenario_mode_id = D_800FEE10;
    round_hole_index = D_800FEE11;
  }

  initialize_hole_handicap_ranks();
  if (D_801B60B8 == 0) {
    init_per_player_state();
  }
  if (2 * flag_is_set(0x33) + flag_is_set(0x34) == 2) {
    D_801B72A6 = 1;
  }
  setup_terrain_detail_default();
  if (arg0 != 3) {
    init_sky_panels();
  }
  func_80054E20();
  osSyncPrintf(D_800D1240);
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005F964);

void func_8005FB58(void) {
  /* Preserve the ROM's eight bytes of local stack space. */
  s32 unused[2];
  s32 i;
  if (D_800FBDB8 == 0) {
    i = 0;
    if (active_player_count > 0) {
      s32 count = active_player_count;
      do {
        s32 state_code;
        if (player_records[i].base_score == 0) {
          break;
        }
        state_code = player_records[i].state_code;
        if ((state_code != 0x11) & (state_code != 0x14)) {
          break;
        }
        i++;
      } while (i < count);
    }
    if (rumble_disable_flag == 0 && i == active_player_count) {
      if (get_course_hole_primary_flag(scenario_mode_id, round_hole_index) &&
          func_8006D1FC() != -1) {
        func_8020FFEC(0xC);
      }
      if (get_course_hole_secondary_flag(scenario_mode_id, round_hole_index) &&
          func_8006D1FC() != -1) {
        func_8020FFEC(0xD);
      }
      if (game_mode == 5 && func_8006DF84()) {
        func_8020FFEC(0xE);
      }
      D_800FBDB8 = 0x3C;
    }
  } else {
    if (func_8020FFD8()) {
      D_800FBDB8 = -1;
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005FCB8);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005FE8C);

/* The selector advances eight bytes through auxiliary entries, not whole records. */
void func_800600C0(s32 arg0) {
  ModeAuxFields* rows = (ModeAuxFields*)D_800FEDD8;
  GameModeRecord* record = (GameModeRecord*)&rows[arg0];
  ModeAuxFields* fields = &record->mode_aux_fields[0];

  D_80104F78 = fields->unknown_00;
  D_80104F7C = fields->unknown_02[0];
  D_80104F80 = fields->unknown_02[1];
  D_80104F84 = fields->unknown_02[2];
  D_80104F88 = fields->unknown_02[3];
  D_80104F8C = fields->unknown_02[4];
  D_80104F90 = fields->unknown_02[5];
}

void func_80060128(s32 arg0) {
  ModeAuxFields* rows = (ModeAuxFields*)D_80105F40;
  CourseSelectionRecord* record = (CourseSelectionRecord*)&rows[arg0];
  ModeAuxFields* fields = &record->mode_aux_fields[0];

  D_80104F78 = fields->unknown_00;
  D_80104F7C = fields->unknown_02[0];
  D_80104F80 = fields->unknown_02[1];
  D_80104F84 = fields->unknown_02[2];
  D_80104F88 = fields->unknown_02[3];
  D_80104F8C = fields->unknown_02[4];
  D_80104F90 = fields->unknown_02[5];
}

extern s8 D_800BAA04;

s32 func_80060190(s32 score) {
  s32 difference = score - D_800BAA04;
  s32 result;
  s32 mode;

  if (score == 1) {
    result = 0x80;
  } else {
    switch (difference) {
      case -3:
        result = 0x81;
        break;
      case -2:
        result = 0x82;
        break;
      case -1:
        result = 0x83;
        break;
      case 0:
        mode = game_mode;
        if (mode != 9) {
          goto zero;
        }
        result = 0x84;
        break;
      default:
        goto zero;
    }
  }
  goto done;
zero:
  result = 0;
done:
  return result;
}

void func_80060210(GameModeRecord* arg0) {
  D_800FEDD8[0] = *arg0;
  game_mode = arg0->game_mode;
  scenario_mode_id = arg0->scenario_mode_id;
  round_hole_index = arg0->round_hole_index;
  D_800FE330 = arg0->unknown_3E;
  init_per_player_state();
  osSyncPrintf(D_800D1254);
}

void func_800602B4(CourseSelectionRecord* arg0) {
  D_80105F40[0] = *arg0;
  game_mode = 9;
  scenario_mode_id = arg0->scenario_mode_id;
  round_hole_index = arg0->round_hole_index;
  D_800FE330 = arg0->unknown_3A;
  init_per_player_state();
}

void func_8006034C(u8 arg0) { D_800C30A4 = arg0; }

void func_80060358(void) {
  D_800C306C = 0;
  transfer_mode_continue_slot(0);
  func_80216B74(0);
  scenario_state_id.state_id = 7;
  D_800C30A8 = 3;
  D_800C30AC = 9;
}

s32 func_800603A8(void) {
  s32 f82 = flag_is_set(0x82);
  s32 f83 = flag_is_set(0x83);
  s32 f84 = flag_is_set(0x84);
  s32 f85 = flag_is_set(0x85);
  s32 f86 = flag_is_set(0x86);

  return (f82 << 4) | (f83 << 3) | (f84 << 2) | (f85 << 1) | f86;
}

void func_80060434(void) {
  s32 i;

  D_801B55F0 = scenario_state_id;
  for (i = 0; i < 4; i++) {
    D_8012D0C8[i] = player_records[i];
  }
}

typedef struct {
  s8 id;
  u8 unknown_01[4];
  u8 rank;
} RankedRosterEntry;

extern RankedRosterEntry ranked_roster_ids[];
extern s32 roster_random_seed;
extern s32 get_handicap_match_play_result(s32 player_a, s32 player_b);
extern s32 get_round_first_hole(void);
extern s32 get_round_hole_count(void);
extern s32 func_8006CD50(s32 hole_limit, s32 player);
extern u8 roster_sort_and_rank(s32 initial_hole, s32 hole_adjustment,
                               s32 initial_score, s32 random_seed);

s32 func_800604F4(void) {
  s32 mode = game_mode;
  s32 first_hole;
  s32 i;
  RankedRosterEntry* entry;

  /* Both successful paths share this return; a terminal constant select would
   * otherwise collapse the match-play character check into arithmetic. */
  if (mode == 4) {
    if (get_handicap_match_play_result(0, 1) != 1) {
      goto no;
    }
    if (player_records[1].character_index != 12) {
      goto no;
    }
    goto yes;
  }
  if (mode != 5) {
    goto no;
  }
  if ((u32)(scenario_mode_id - 4) >= 2) {
    goto no;
  }
  first_hole = get_round_first_hole();
  roster_sort_and_rank(18, 0,
                       func_8006CD50(first_hole + get_round_hole_count(), 0),
                       roster_random_seed);
  i = 0;
  do {
    entry = &ranked_roster_ids[i];
    if (entry->id == -1) {
      if ((entry->rank & 0x7F) == 1) {
        goto yes;
      }
    }
    i++;
  } while (i != 30);
no:
  return 0;
yes:
  return 1;
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_800605EC);

void gen_terrain_detail_texture(s32 range, s32 minimum) {
  s32 index = 0;
  s32 row = 0;
  s32 column;
  u8* pixel;
  while (row < 32) {
    column = 0;
    pixel = &g_terrain_detail_texture[index];
    while (column < 32) {
      s32 value = rand() % range + minimum;
      if (value >= 256) {
        value = 255;
      }
      *pixel++ = value;
      column++;
      index++;
    }
    row++;
  }
}

void setup_terrain_detail_default(void) {
  if (D_800B67C4 != 0) {
    func_80407D64();
  }
  gen_terrain_detail_texture(0x60, 0xBE);
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom",
            emit_per_phase_fog_state);

void func_800627F8(f64* src, f64* dst) {
  dst[0] = src[0];
  dst[1] = src[1];
}

void func_8006280C(s32 arg0, s32 arg1) {
  s32 tx, ty;
  s32 dx, dy;

  if (g_terrain_vtx_xform_mode != 0) {
    return;
  }
  tx = (u32)arg0 >> 19;
  ty = (u32)arg1 >> 19;
  if ((u32)tx >= 8) {
    return;
  }
  if ((u32)ty >= 0x10) {
    return;
  }
  for (dy = -1; dy < 2; dy++) {
    s32 yy = ty + dy;
    for (dx = -1; dx < 2; dx++) {
      s32 xx = tx + dx;
      if ((u32)xx < 8) {
        if ((u32)yy < 0x10) {
          g_terrain_tile_cull_exclude[yy * 8 + xx] = 1;
        }
      }
    }
  }
}

void clear_terrain_emit_scratch(void) {
  bzero(g_terrain_tile_cull_exclude, 0x80);
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom",
            init_terrain_vertex_texcoords);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom",
            emit_terrain_state_prefix_block);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom",
            emit_course_terrain_dl);
