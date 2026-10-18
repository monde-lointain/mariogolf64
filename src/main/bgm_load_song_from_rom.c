#include "common.h"
#include "session.h"
#include "club.h"
#include <nusys.h>
#include <nualstl.h>

extern musHandle try_spawn_global_object(int object_id);
extern void gen_terrain_detail_texture(s32 arg0, s32 arg1);
extern void func_80407D64(void);
extern s32 get_lowest_height_at_position(s32 x, s32 z);
extern s32 D_800FBE68;

extern s32 transfer_mode_continue_slot(s32 restore);
extern void func_80216B74(s32 arg0);
extern s32 flag_is_set(s32 flag);
extern void initialize_game_mode_from_flags(void);
extern void func_8005F4AC(void);
extern void load_game_mode_record(GameModeRecord* arg0);
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
extern s32 resolve_active_terrain_club_mask(s32 category);
extern s8 D_800FBDB8;
extern s8 rumble_disable_flag;
extern s32 get_course_hole_primary_flag(s32 course_id, s32 hole_index);
extern s32 get_course_hole_secondary_flag(s32 course_id, s32 hole_index);
extern s32 func_8006D1FC(void);
extern s32 func_8006DF84(void);
extern void func_8020FFEC(s32 event);
extern s32 func_8020FFD8(void);

void bgm_load_song_from_rom(u32 rom_address, s32 byte_count) {
  nuPiReadRom(rom_address, (void*)g_bgm_song_buffer,
              byte_count + (byte_count & 1));
}

void allocate_sound_effect_bank_buffer(s32 buffer_size) {
  D_800C2FF4 = (u32)__MusIntMemMalloc(buffer_size);
}

musHandle spawn_object_simple(int object_id) {
  return try_spawn_global_object(object_id);
}

void start_sound_effect(int effect_id, int volume, int pan, int restart_flag,
                        int priority) {
  MusStartEffect2(effect_id, volume, pan, restart_flag, priority);
}

void load_sound_effect_bank_from_rom(u32 rom_address, s32 bank_size) {
  s32 padded_size = (bank_size & 1) + bank_size;

  if ((u32)padded_size >= 0x4001) {
    osSyncPrintf(D_800D1220);
  }
  nuPiReadRom(rom_address, (void*)D_800C2FF4, padded_size);
  MusFxBankInitialize((void*)D_800C2FF4);
}

s32 func_8005F180(void) {
  s32 is_mode_flag_set = 0;

  if (flag_is_set(0x32)) {
    is_mode_flag_set = 1;
  } else if (flag_is_set(0x9B)) {
    is_mode_flag_set = 1;
  }
  return is_mode_flag_set;
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

s32 get_reflected_phase(void) {
  s32 phase_value = D_800FBE88;

  if (phase_value >= 0x1F) {
    phase_value = 0x3C - phase_value;
  }
  return phase_value;
}

s32 scale_reflected_phase(s32 phase) {
  s32 reflected_phase;

  if (phase < 0x1F) {
    reflected_phase = phase;
  } else {
    reflected_phase = 0x3C - phase;
  }
  if (reflected_phase < 0) {
    reflected_phase = 0;
  }
  return (reflected_phase << 8) / 30;
}

s32 populate_terrain_club_availability(s32 unused, s16* club_ids,
                                       u8* enabled_club_slots) {
  s32 club_index;

  for (club_index = 0; club_index < 14; club_index++) {
    enabled_club_slots[club_index] =
        (get_club_param(club_ids[club_index])->flags &
         resolve_active_terrain_club_mask(1)) != 0;
  }
  return 14;
}

s32 get_terrain_height_at_position(s32 x_coordinate, s32 z_coordinate) {
  s32 height = get_lowest_height_at_position(x_coordinate, z_coordinate);

  if ((g_terrain_vtx_xform_mode == 0) & (height > 0)) {
    s32 height_limit = D_800FBE68;
    s32 has_positive_limit;
    /* Keep the predicate separate from the mask to preserve slt/negu/and. */
    has_positive_limit = height_limit > 0;
    height = height_limit & -has_positive_limit;
  }
  return height;
}

void initialize_game_mode_from_flags(void) {
  s32 mode_flags = (flag_is_set(0x33) << 1) + flag_is_set(0x34);

  switch (mode_flags) {
    case 0:
      scenario_mode_id = 0;
      round_hole_index = 0;
      D_801B609C = 0;
      mode_flags = (flag_is_set(0x30) << 1) + flag_is_set(0x31) + 1;
      active_player_count = mode_flags;
      scenario_state_id.state_id = 0;
      game_mode = 0;
      break;
    case 1:
      scenario_mode_id = 0;
      round_hole_index = 0;
      D_801B609C = 0;
      active_player_count = mode_flags;
      scenario_state_id.state_id = 0;
      game_mode = 3;
      break;
    case 2:
      scenario_mode_id = 0;
      round_hole_index = 0;
      D_801B609C = 0;
      active_player_count = mode_flags;
      scenario_state_id.state_id = 0;
      game_mode = mode_flags;
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
  u8* scenario_mode_id_bytes = (u8*)&scenario_state_id.scenario_mode_id;
  *(s32*)scenario_mode_id_bytes = 0;
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

void format_hole_number(s32 hole_index, char* output_buffer) {
  sprintf(output_buffer, D_800D1238, hole_index + 1);
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", init_per_player_state);

void initialize_game_setup(s32 setup_mode) {
  if (setup_mode == 1) {
    initialize_game_mode_from_flags();
  } else if (setup_mode == 0) {
    func_8005F4AC();
  } else if (setup_mode == 2) {
    load_game_mode_record(D_800FED40);
    D_801B609C = 0;
    active_player_count = 1;
    scenario_state_id.state_id = 0;
    game_mode = 0;
    scenario_mode_id = D_800FEDD8[0].scenario_mode_id;
    round_hole_index = D_800FEDD8[0].round_hole_index;
  }

  initialize_hole_handicap_ranks();
  if (D_801B60B8 == 0) {
    init_per_player_state();
  }
  if (2 * flag_is_set(0x33) + flag_is_set(0x34) == 2) {
    D_801B72A6 = 1;
  }
  setup_terrain_detail_default();
  if (setup_mode != 3) {
    init_sky_panels();
  }
  func_80054E20();
  osSyncPrintf(D_800D1240);
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_8005F964);

void func_8005FB58(void) {
  /* Preserve the ROM's eight bytes of local stack space. */
  s32 stack_reserve[2];
  s32 player_index;
  if (D_800FBDB8 == 0) {
    player_index = 0;
    if (active_player_count > 0) {
      s32 players_to_check = active_player_count;
      do {
        s32 player_state_code;
        if (player_records[player_index].base_score == 0) {
          break;
        }
        player_state_code = player_records[player_index].state_code;
        if ((player_state_code != 0x11) & (player_state_code != 0x14)) {
          break;
        }
        player_index++;
      } while (player_index < players_to_check);
    }
    if (rumble_disable_flag == 0 && player_index == active_player_count) {
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

/* The selector advances eight bytes through auxiliary entries, not whole records.
 * These are address projections: direct array indexing folds the member offset
 * into the base and changes the retail instruction order. */
void load_game_mode_auxiliary_fields(s32 mode_selector_index) {
  ModeAuxFields* mode_aux_rows = (ModeAuxFields*)D_800FEDD8;
  GameModeRecord* mode_record = (GameModeRecord*)&mode_aux_rows[mode_selector_index];
  ModeAuxFields* auxiliary_fields = &mode_record->mode_aux_fields[0];

  D_80104F78 = auxiliary_fields->unknown_00;
  D_80104F7C = auxiliary_fields->unknown_02[0];
  D_80104F80 = auxiliary_fields->unknown_02[1];
  D_80104F84 = auxiliary_fields->unknown_02[2];
  D_80104F88 = auxiliary_fields->unknown_02[3];
  D_80104F8C = auxiliary_fields->unknown_02[4];
  D_80104F90 = auxiliary_fields->unknown_02[5];
}

void load_course_selection_auxiliary_fields(s32 course_selector_index) {
  ModeAuxFields* course_aux_rows = (ModeAuxFields*)D_80105F40;
  CourseSelectionRecord* course_record =
      (CourseSelectionRecord*)&course_aux_rows[course_selector_index];
  ModeAuxFields* auxiliary_fields = &course_record->mode_aux_fields[0];

  D_80104F78 = auxiliary_fields->unknown_00;
  D_80104F7C = auxiliary_fields->unknown_02[0];
  D_80104F80 = auxiliary_fields->unknown_02[1];
  D_80104F84 = auxiliary_fields->unknown_02[2];
  D_80104F88 = auxiliary_fields->unknown_02[3];
  D_80104F8C = auxiliary_fields->unknown_02[4];
  D_80104F90 = auxiliary_fields->unknown_02[5];
}


s32 get_hole_score_result_code(s32 hole_score) {
  s32 score_difference = hole_score - hole_environment.unknown_2C;
  s32 score_code;
  s32 current_game_mode;

  if (hole_score == 1) {
    score_code = 0x80;
  } else {
    switch (score_difference) {
      case -3:
        score_code = 0x81;
        break;
      case -2:
        score_code = 0x82;
        break;
      case -1:
        score_code = 0x83;
        break;
      case 0:
        current_game_mode = game_mode;
        if (current_game_mode != 9) {
          goto zero;
        }
        score_code = 0x84;
        break;
      default:
        goto zero;
    }
  }
  /* These shared exits preserve the ROM's branch layout. */
  goto done;
zero:
  score_code = 0;
done:
  return score_code;
}

void load_game_mode_record(GameModeRecord* mode_record) {
  D_800FEDD8[0] = *mode_record;
  game_mode = mode_record->game_mode;
  scenario_mode_id = mode_record->scenario_mode_id;
  round_hole_index = mode_record->round_hole_index;
  D_800FE330 = mode_record->unknown_3E;
  init_per_player_state();
  osSyncPrintf(D_800D1254);
}

void load_course_selection_record(CourseSelectionRecord* course_record) {
  D_80105F40[0] = *course_record;
  game_mode = 9;
  scenario_mode_id = course_record->scenario_mode_id;
  round_hole_index = course_record->round_hole_index;
  D_800FE330 = course_record->unknown_3A;
  init_per_player_state();
}

void func_8006034C(u8 value) { D_800C30A4 = value; }

void func_80060358(void) {
  D_800C306C = 0;
  transfer_mode_continue_slot(0);
  func_80216B74(0);
  scenario_state_id.state_id = 7;
  D_800C30A8 = 3;
  D_800C30AC = 9;
}

s32 get_mode_flags_mask(void) {
  s32 mode_flag_0x82 = flag_is_set(0x82);
  s32 mode_flag_0x83 = flag_is_set(0x83);
  s32 mode_flag_0x84 = flag_is_set(0x84);
  s32 mode_flag_0x85 = flag_is_set(0x85);
  s32 mode_flag_0x86 = flag_is_set(0x86);

  return (mode_flag_0x82 << 4) | (mode_flag_0x83 << 3) |
         (mode_flag_0x84 << 2) | (mode_flag_0x85 << 1) | mode_flag_0x86;
}

void save_scenario_state_snapshot(void) {
  s32 player_index;

  D_801B55F0 = scenario_state_id;
  for (player_index = 0; player_index < 4; player_index++) {
    D_8012D0C8[player_index] = player_records[player_index];
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
  s32 current_game_mode = game_mode;
  s32 first_hole;
  s32 roster_index;
  RankedRosterEntry* roster_entry;

  /* Both successful paths share this return; a terminal constant select would
   * otherwise collapse the match-play character check into arithmetic. */
  if (current_game_mode == 4) {
    if (get_handicap_match_play_result(0, 1) != 1) {
      goto no;
    }
    if (player_records[1].character_index != 12) {
      goto no;
    }
    goto yes;
  }
  if (current_game_mode != 5) {
    goto no;
  }
  if ((u32)(scenario_mode_id - 4) >= 2) {
    goto no;
  }
  first_hole = get_round_first_hole();
  roster_sort_and_rank(18, 0,
                       func_8006CD50(first_hole + get_round_hole_count(), 0),
                       roster_random_seed);
  roster_index = 0;
  do {
    roster_entry = &ranked_roster_ids[roster_index];
    if (roster_entry->id == -1) {
      if ((roster_entry->rank & 0x7F) == 1) {
        goto yes;
      }
    }
    roster_index++;
  } while (roster_index != 30);
no:
  return 0;
yes:
  return 1;
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", func_800605EC);

void gen_terrain_detail_texture(s32 random_value_range, s32 minimum_value) {
  s32 texture_index = 0;
  s32 texture_row = 0;
  s32 texture_column;
  u8* texture_pixel;

  /* The pointer/index traversal preserves the ROM's register allocation. */
  while (texture_row < 32) {
    texture_column = 0;
    texture_pixel = &g_terrain_detail_texture[texture_index];
    while (texture_column < 32) {
      s32 pixel_value = rand() % random_value_range + minimum_value;
      if (pixel_value >= 256) {
        pixel_value = 255;
      }
      *texture_pixel++ = pixel_value;
      texture_column++;
      texture_index++;
    }
    texture_row++;
  }
}

void setup_terrain_detail_default(void) {
  if (D_800B67C4 != 0) {
    func_80407D64();
  }
  gen_terrain_detail_texture(0x60, 0xBE);
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", emit_per_phase_fog_state);

void copy_two_double_values(f64* source_values, f64* destination_values) {
  destination_values[0] = source_values[0];
  destination_values[1] = source_values[1];
}

void exclude_terrain_tiles_near_position(s32 x_coordinate, s32 z_coordinate) {
  s32 tile_x;
  s32 tile_z;
  s32 x_offset;
  s32 z_offset;

  if (g_terrain_vtx_xform_mode != 0) {
    return;
  }
  tile_x = (u32)x_coordinate >> 19;
  tile_z = (u32)z_coordinate >> 19;
  if ((u32)tile_x >= 8) {
    return;
  }
  if ((u32)tile_z >= 0x10) {
    return;
  }
  for (z_offset = -1; z_offset < 2; z_offset++) {
    s32 neighbor_z = tile_z + z_offset;
    for (x_offset = -1; x_offset < 2; x_offset++) {
      s32 neighbor_x = tile_x + x_offset;
      if ((u32)neighbor_x < 8) {
        if ((u32)neighbor_z < 0x10) {
          g_terrain_tile_cull_exclude[neighbor_z * 8 + neighbor_x] = 1;
        }
      }
    }
  }
}

void clear_terrain_emit_scratch(void) {
  bzero(g_terrain_tile_cull_exclude, 0x80);
}

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", init_terrain_vertex_texcoords);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", emit_terrain_state_prefix_block);

INCLUDE_ASM("asm/nonmatchings/main/bgm_load_song_from_rom", emit_course_terrain_dl);
