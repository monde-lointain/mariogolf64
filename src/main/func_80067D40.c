#include "common.h"

typedef struct {
  s8 value;
  s8 padding[3];
} RosterScoreAdjustment;

typedef struct {
  s8 value;
  s8 padding[3];
} RosterHoleAdjustment;

typedef struct {
  s8 id;
  s8 padding0;
  s16 padding1;
  s16 padding2;
} RankedRosterEntry;

typedef struct {
  s16 score;
  s8 padding[4];
} ScoredRosterEntry;

typedef struct {
  s8 hole;
  s8 padding[5];
} RosterHoleEntry;

typedef struct {
  u8 rank;
  s8 padding[5];
} RosterRankEntry;

extern s32 scenario_mode_id;
extern s32 roster_rng_state;
extern RosterScoreAdjustment roster_score_adjustments[];
extern RosterHoleAdjustment roster_hole_adjustments[];
extern char roster_name_table[];
extern f64 roster_radians_per_hole;
extern s16 roster_name_indices[];
extern RankedRosterEntry ranked_roster_ids[];
extern ScoredRosterEntry ranked_roster_scores[];
extern RosterHoleEntry ranked_roster_holes[];
extern RosterRankEntry ranked_roster_ranks[];
extern char player_records[];

extern f64 sin(f64);
extern s32 strcmp(const char*, const char*);
extern void rank_scores_descending(s32* keys, s32* order, s32 count);

s32 roster_next_random_value(void);
void initialize_roster_name_indices(void);

void roster_set_random_seed(s32 seed) { roster_rng_state = seed; }

s32 roster_next_random_value(void) {
  roster_rng_state = roster_rng_state * 0x5D588B65 + 1;
  return roster_rng_state;
}

/* The legacy return register and fifth argument are unused by the caller.
 * Retain the scalar return type: void lets GCC fill a retail nop delay slot. */
s32 roster_generate_scores(s32 initial_hole, s32 hole_adjustment,
                           s32 start_entry, s32 end_entry,
                           s32 unused_player_score) {
  s32 high_bit;
  s32 entry_index;
  s32 score_curve_kind;
  s32 score_wave_amplitude;
  s32 hole;
  s32 score_delta;
  f64 radians_per_hole;
  s32 score;

  if (initial_hole > 0x12) {
    initial_hole = 0x12;
  }
  entry_index = start_entry;
  if (entry_index < end_entry) {
    high_bit = (u32)hole_adjustment >> 31;
    radians_per_hole = roster_radians_per_hole;
    for (; entry_index < end_entry; entry_index++) {
      score_curve_kind = entry_index % 3;
      score_wave_amplitude = entry_index % 10;
      if (scenario_mode_id < 2) {
        score_wave_amplitude = 0xD - score_wave_amplitude;
        if (score_wave_amplitude > 9) {
          score_wave_amplitude = 9;
        }
      }
      {
        s32 prior_hole_adjustment =
            roster_hole_adjustments[entry_index - 1].value;
        s32 carry = high_bit;
        carry |= hole_adjustment >= entry_index;
        hole = initial_hole + (prior_hole_adjustment + carry);
      }
      if (hole > 0x12) {
        hole = 0x12;
      }
      score_delta = roster_score_adjustments[entry_index - 1].value -
                    scenario_mode_id * 2;
      {
        s32 first_random_adjustment =
            (s32)((u32)roster_next_random_value() >> 28 & 5) - 2;
        s32 second_random_adjustment =
            (s32)((u32)roster_next_random_value() >> 28 & 5) - 2;
        score_delta += (first_random_adjustment + second_random_adjustment) / 2;
      }
      if (hole == 0x12) {
        s32 minimum_final_hole_score = scenario_mode_id * -2 + 1;
        if (score_delta <= minimum_final_hole_score) {
          score_delta = minimum_final_hole_score;
        }
      }
      if (score_delta < -0x12) {
        score_delta = -0x12;
      } else if (score_delta > 0x12) {
        score_delta = 0x12;
      }
      switch (score_curve_kind) {
        case 0:
          score = (score_delta * hole) / 18;
          break;
        case 1:
          score = (s32)((f64)((score_delta * hole) / 18) +
                        sin((f64)hole * radians_per_hole) *
                            (f64)score_wave_amplitude);
          break;
        case 2:
          score = (s32)((f64)((score_delta * hole) / 18) -
                        sin((f64)hole * radians_per_hole) *
                            (f64)score_wave_amplitude);
          break;
      }
      ranked_roster_ids[entry_index].id = entry_index - 1;
      ranked_roster_scores[entry_index].score = score;
      ranked_roster_holes[entry_index].hole = hole;
    }
  }
}

u8 roster_sort_and_rank(s32 initial_hole, s32 hole_adjustment,
                        s32 initial_score, s32 random_seed) {
  s32 sort_keys[30];
  s32 sorted_indices[30];
  RankedRosterEntry original_entries[30];
  s32 entry_index;
  s32 prior_entry_index;
  u8 player_rank;

  player_rank = 1;
  roster_set_random_seed(random_seed);
  roster_generate_scores(initial_hole, hole_adjustment, 1, 30, initial_score);
  initialize_roster_name_indices();

  ranked_roster_scores[0].score = initial_score;
  ranked_roster_ids[0].id = -1;
  ranked_roster_holes[0].hole = initial_hole;
  for (entry_index = 0; entry_index < 30; entry_index++) {
    sort_keys[entry_index] = -ranked_roster_scores[entry_index].score;
  }
  rank_scores_descending(sort_keys, sorted_indices, 30);
  for (entry_index = 0; entry_index < 30; entry_index++) {
    original_entries[entry_index] = ranked_roster_ids[entry_index];
  }
  for (entry_index = 0; entry_index < 30; entry_index++) {
    ranked_roster_ids[entry_index] =
        original_entries[sorted_indices[entry_index]];
  }

  for (entry_index = 0; entry_index < 30; entry_index++) {
    u8 rank = 1;
    u8 tie_flag = 0;
    prior_entry_index = 0;
    if (prior_entry_index < entry_index) {
      RosterRankEntry* prior_ranks = ranked_roster_ranks;
      for (; prior_entry_index < entry_index; prior_entry_index++) {
        if (ranked_roster_scores[entry_index].score >
            ranked_roster_scores[prior_entry_index].score) {
          rank++;
        } else {
          tie_flag = 0x80;
          prior_ranks[prior_entry_index].rank |= 0x80;
        }
      }
    }
    ranked_roster_ranks[entry_index].rank = tie_flag | rank;
    if (ranked_roster_ids[entry_index].id == -1) {
      player_rank = tie_flag | rank;
    }
  }
  return player_rank;
}

void initialize_roster_name_indices(void) {
  s16* name_index_cursor;
  s16* name_index_limit;
  char* roster_names;
  s32 last_roster_name_index;
  s32 remaining_names;
  s32 course_index;
  s32 player_name_found;
  s32 is_before_limit;
  s16* name_indices;
  s32 scenario;

  scenario = scenario_mode_id;
  course_index = 0x1C;
  remaining_names = 0x1C;
  player_name_found = 0;
  roster_names = roster_name_table;
  name_indices = roster_name_indices;
  name_index_cursor = name_indices + 0x1C;
  name_index_limit = name_indices + 1;
  last_roster_name_index = (scenario + 1) * 30 - 2;
  for (; course_index >= 0; course_index--) {
    if (!player_name_found) {
      if (strcmp(player_records,
                 (char*)((last_roster_name_index - remaining_names) * 13 +
                         (s32)roster_names)) == 0) {
        player_name_found = 1;
        remaining_names--;
      }
    }
    is_before_limit = name_index_cursor < name_index_limit;
    if (player_name_found & is_before_limit) {
      *name_index_cursor = (scenario + 1) * 30 - 1;
    } else {
      *name_index_cursor = last_roster_name_index - remaining_names;
      remaining_names--;
    }
    name_index_cursor--;
  }
}
