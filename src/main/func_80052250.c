#include "common.h"
#include "session.h"

enum {
  SPECIAL_COURSE_SCENARIO_MODE = 0xC,
  HOLE_HANDICAP_HOLE_COUNT = 18,
  HOLE_HANDICAP_ENTRY_STRIDE = 10,
  HOLE_HANDICAP_COURSE_STRIDE = 200,
  HOLE_HANDICAP_RANK_TABLE_SIZE = 1600,
  MAX_PLAYER_COUNT = 4,
  LOWEST_SCORE_SENTINEL = 0x40000000,
};

extern s32 scenario_mode_id;
extern u32 game_mode;
extern s32 active_player_count[];
extern s32 round_hole_index;
extern s32 D_801B60A0[];
extern s32 get_round_first_hole(void);
extern s32 get_round_hole_count(void);
extern s8 hole_handicap_ranks[];
/* The retail caller forwards its selector in a0 even though the lookup uses
 * global state. Dropping that argument changes this caller's allocation. */
extern s32 get_current_course_id(s32 course_id);
extern void rank_scores_descending(s32* scores, s32* player_order, s32 count);

s32 is_special_course_active(void) {
  return scenario_mode_id == SPECIAL_COURSE_SCENARIO_MODE;
}

s32 calculate_hole_handicap_adjustment(s32 course_id, s32 hole_index,
                                       s32 handicap) {
  return -handicap / HOLE_HANDICAP_HOLE_COUNT -
         (handicap % HOLE_HANDICAP_HOLE_COUNT >=
          hole_handicap_ranks[get_current_course_id(course_id) *
                                  HOLE_HANDICAP_COURSE_STRIDE +
                              hole_index * HOLE_HANDICAP_ENTRY_STRIDE]);
}

s32 calculate_player_adjusted_score(s32 player_index) {
  return player_records[player_index].base_score +
         calculate_hole_handicap_adjustment(
             scenario_mode_id, round_hole_index,
             player_records[player_index].handicap);
}

void initialize_hole_handicap_ranks(void) {
  s32 ranked_hole_workspace[HOLE_HANDICAP_HOLE_COUNT * 2];
  s32* ranked_hole_scores = ranked_hole_workspace;
  s32 handicap_rank_count = HOLE_HANDICAP_HOLE_COUNT;
  s32 course_rank_table_offset;
  s32 hole_rank_table_offset;
  s32 hole_index;

  for (course_rank_table_offset = 0;
       course_rank_table_offset < HOLE_HANDICAP_RANK_TABLE_SIZE;
       course_rank_table_offset += HOLE_HANDICAP_COURSE_STRIDE) {
    for (hole_index = 0; hole_index < HOLE_HANDICAP_HOLE_COUNT; hole_index++) {
      hole_rank_table_offset =
          course_rank_table_offset + hole_index * HOLE_HANDICAP_ENTRY_STRIDE;
      ranked_hole_scores[hole_index] =
          hole_handicap_ranks[hole_rank_table_offset];
    }
    rank_scores_descending(ranked_hole_workspace,
                           &ranked_hole_workspace[HOLE_HANDICAP_HOLE_COUNT],
                           HOLE_HANDICAP_HOLE_COUNT);
    for (hole_index = 0, hole_rank_table_offset = course_rank_table_offset;
         hole_index < HOLE_HANDICAP_HOLE_COUNT; hole_index++) {
      s32 ranked_hole_table_offset =
          ranked_hole_scores[HOLE_HANDICAP_HOLE_COUNT + hole_index] *
              HOLE_HANDICAP_ENTRY_STRIDE +
          hole_rank_table_offset;
      hole_handicap_ranks[ranked_hole_table_offset] =
          handicap_rank_count - hole_index;
    }
  }
}

s32 get_match_play_result(s32 player_a_index, s32 player_b_index) {
  s32 round_hole_limit;
  s32 remaining_hole_count;
  s32 player_a_match_lead;
  s32 ending_lead_magnitude;
  s32 lead_magnitude;
  s32 closing_lead_magnitude;
  s32 hole_index;

  round_hole_limit = get_round_first_hole() + get_round_hole_count();
  player_a_match_lead = 0;

  if (D_801B60A0[0] != 0) {
    return 3;
  }

  remaining_hole_count = round_hole_limit - round_hole_index - 1;
  hole_index = get_round_first_hole();
  if (hole_index < round_hole_index) {
    s32 final_hole_index;

    do {
      s32 player_a_hole_score;
      s32 player_b_hole_score;

      final_hole_index = D_801B60A0[-2];
      player_a_hole_score =
          player_records[player_a_index].hole_scores[hole_index].score;
      player_b_hole_score =
          player_records[player_b_index].hole_scores[hole_index].score;
      if (player_a_hole_score < player_b_hole_score) {
        player_a_match_lead++;
      } else {
        player_a_match_lead -= (player_b_hole_score < player_a_hole_score);
      }
      hole_index++;
    } while (hole_index < final_hole_index);
  }

  closing_lead_magnitude = player_a_match_lead;
  if (closing_lead_magnitude < 0) {
    closing_lead_magnitude = -closing_lead_magnitude;
  }
  if (closing_lead_magnitude == remaining_hole_count + 1) {
    if (player_a_match_lead < 0) {
      return 2;
    }
    return 1;
  }
  lead_magnitude = player_a_match_lead;
  if (lead_magnitude < 0) {
    lead_magnitude = -lead_magnitude;
  }
  if (remaining_hole_count + 1 < lead_magnitude) {
    return -1;
  }
  ending_lead_magnitude = player_a_match_lead;
  if (ending_lead_magnitude < 0) {
    ending_lead_magnitude = -ending_lead_magnitude;
  }
  if (ending_lead_magnitude == remaining_hole_count) {
    if (remaining_hole_count == 0) {
      return 3;
    }
  }
  return 0;
}

s32 get_handicap_match_play_result(s32 player_a_index, s32 player_b_index) {
  s32 round_hole_limit;
  s32 remaining_hole_count;
  s32 player_a_match_lead;
  s32 lead_magnitude;
  s32 player_a_hole_score;
  s32 player_b_hole_score;
  s32 hole_index;
  s32* tournament_state = D_801B60A0;
  s32* course_config;
  s32* course_score_config;

  round_hole_limit = get_round_first_hole() + get_round_hole_count();
  player_a_match_lead = 0;

  if (tournament_state[0] != 0) {
    hole_index = get_round_first_hole();
    if (hole_index >= round_hole_index) {
      return 0;
    }
    course_config = tournament_state - 3;
    do {
      player_a_hole_score =
          player_records[player_a_index].hole_scores[hole_index].score +
          calculate_hole_handicap_adjustment(
              course_config[0], hole_index,
              player_records[player_a_index].handicap);
      player_b_hole_score =
          player_records[player_b_index].hole_scores[hole_index].score +
          calculate_hole_handicap_adjustment(
              course_config[0], hole_index,
              player_records[player_b_index].handicap);
      if (player_a_hole_score < player_b_hole_score) {
        return 1;
      }
      if (player_b_hole_score < player_a_hole_score) {
        return 2;
      }
      hole_index++;
    } while (hole_index < course_config[1]);
    return 0;
  }

  remaining_hole_count = round_hole_limit - round_hole_index - 1;
  hole_index = get_round_first_hole();
  if (hole_index < round_hole_index) {
    course_score_config = tournament_state - 3;
    do {
      player_a_hole_score =
          player_records[player_a_index].hole_scores[hole_index].score +
          calculate_hole_handicap_adjustment(
              course_score_config[0], hole_index,
              player_records[player_a_index].handicap);
      player_b_hole_score =
          player_records[player_b_index].hole_scores[hole_index].score +
          calculate_hole_handicap_adjustment(
              course_score_config[0], hole_index,
              player_records[player_b_index].handicap);
      player_a_match_lead += (player_a_hole_score < player_b_hole_score);
      player_a_match_lead -= (player_b_hole_score < player_a_hole_score);
      hole_index++;
    } while (hole_index < course_score_config[1]);
  }
  lead_magnitude =
      (player_a_match_lead < 0) ? -player_a_match_lead : player_a_match_lead;
  if (remaining_hole_count + 1 < lead_magnitude) {
    if (player_a_match_lead > 0) {
      return 1;
    }
    return 2;
  }
  return 0;
}

s32 compare_player_hole_results(s32 player_a_index, s32 player_b_index) {
  s32 match_result;

  if (player_records[player_a_index].hole_status == 0) {
    if (player_records[player_b_index].hole_status == 0) {
      /* A direct return changes the retail branch polarity and delay slot. */
      goto undecided;
    }
  } else {
    if (player_records[player_b_index].hole_status != 0) {
      if (calculate_player_adjusted_score(player_a_index) <
          calculate_player_adjusted_score(player_b_index)) {
        return 1;
      }
      return (calculate_player_adjusted_score(player_b_index) >=
              calculate_player_adjusted_score(player_a_index))
                 ? 3
                 : 2;
    }
    if (calculate_player_adjusted_score(player_a_index) <
        calculate_player_adjusted_score(player_b_index) + 1) {
      return 1;
    }
  }

  if (player_records[player_b_index].hole_status != 0 &&
      calculate_player_adjusted_score(player_b_index) <
          calculate_player_adjusted_score(player_a_index) + 1) {
    return 2;
  }

  match_result = get_match_play_result(player_a_index, player_b_index);
  if (match_result == 1) {
    if (player_records[player_a_index].hole_status != 0 &&
        calculate_player_adjusted_score(player_a_index) <=
            calculate_player_adjusted_score(player_b_index) + 1) {
      return 3;
    }
  }
  if (match_result == 2) {
    if (player_records[player_b_index].hole_status != 0 &&
        calculate_player_adjusted_score(player_b_index) <=
            calculate_player_adjusted_score(player_a_index) + 1) {
      return 3;
    }
  }
undecided:
  return 0;
}

s32 resolve_hole_winners(s8* player_marks) {
  s8 fallback_player_marks[MAX_PLAYER_COUNT];
  s32 lowest_adjusted_score;
  s32 lowest_scoring_player_index;
  s32 lowest_score_tie_count;
  s32 status_three_player_count;
  s32 can_resolve_hole_result;
  s32 player_index;

  lowest_adjusted_score = LOWEST_SCORE_SENTINEL;
  lowest_scoring_player_index = -1;
  can_resolve_hole_result = 0;
  lowest_score_tie_count = 0;
  status_three_player_count = 0;

  if (player_marks == NULL) {
    player_marks = fallback_player_marks;
  }

  for (player_index = 0; player_index < active_player_count[0];
       player_index++) {
    s32 player_record_index = player_index;
    if (player_records[player_record_index].hole_status == 1) {
      if (calculate_player_adjusted_score(player_index) <
          lowest_adjusted_score) {
        lowest_adjusted_score = calculate_player_adjusted_score(player_index);
        lowest_scoring_player_index = player_index;
        lowest_score_tie_count = 0;
      } else {
        lowest_score_tie_count +=
            (lowest_adjusted_score ==
             calculate_player_adjusted_score(player_index));
      }
    } else {
      status_three_player_count +=
          (player_records[player_record_index].hole_status == 3);
    }
  }

  if (lowest_scoring_player_index == -1) {
    for (player_index = 0; player_index < active_player_count[0];
         player_index++) {
      player_marks[player_index] = 0;
    }
    if (status_three_player_count == active_player_count[0]) {
      return 2;
    }
    return can_resolve_hole_result;
  }

  can_resolve_hole_result = 1;
  for (player_index = 0; player_index < active_player_count[0];
       player_index++) {
    s32 player_record_index = player_index;
    player_marks[player_index] = 0;
    if (player_records[player_record_index].hole_status == 0) {
      if (calculate_player_adjusted_score(player_index) >=
          lowest_adjusted_score) {
        player_marks[player_index] = 2;
      } else if (calculate_player_adjusted_score(player_index) <
                 lowest_adjusted_score - 1) {
        can_resolve_hole_result = 0;
      } else {
        can_resolve_hole_result =
            lowest_score_tie_count ? can_resolve_hole_result : 0;
      }
    } else if (lowest_adjusted_score <
               calculate_player_adjusted_score(player_index)) {
      player_marks[player_index] = 2;
    } else if (player_records[player_record_index].hole_status >= 2) {
      player_marks[player_index] = 2;
    }
  }

  if (can_resolve_hole_result == 0) {
    return 0;
  }
  if (lowest_score_tie_count > 0) {
    return 2;
  }

  for (player_index = 0; player_index < active_player_count[0];
       player_index++) {
    s32 player_record_index = player_index;
    if (player_records[player_record_index].hole_status == 1 &&
        calculate_player_adjusted_score(player_index) ==
            lowest_adjusted_score) {
      player_marks[player_index] = 1;
    }
  }
  return 1;
}

s32 get_player_hole_result(s32 player_index) {
  s32 hole_result;

  hole_result = 3;

  switch (game_mode) {
    case 2:
    case 4: {
      s32 opponent_index;
      opponent_index = player_index ^ 1;
      if (calculate_player_adjusted_score(player_index) ==
              calculate_player_adjusted_score(opponent_index) - 1 &&
          player_records[opponent_index].hole_status != 0) {
        hole_result = 4;
        if (get_match_play_result(player_index, opponent_index) == 1) {
          hole_result = 2;
        }
        break;
      }
      if (calculate_player_adjusted_score(player_index) <
          calculate_player_adjusted_score(opponent_index)) {
        hole_result =
            (get_match_play_result(player_index, opponent_index) == 3) * 2;
        break;
      }
      if (calculate_player_adjusted_score(player_index) >=
          calculate_player_adjusted_score(opponent_index) + 1) {
        break;
      }
      if (player_records[opponent_index].hole_status == 1) {
        break;
      }
      if (get_match_play_result(player_index, opponent_index) == 1) {
        hole_result = 2;
      }
      break;
    }

    case 10:
    case 11: {
      s32 lowest_adjusted_score;
      s32 has_finished_player;
      s32 candidate_player_index;
      has_finished_player = 0;
      lowest_adjusted_score = LOWEST_SCORE_SENTINEL;
      hole_result = 0;
      for (candidate_player_index = 0;
           candidate_player_index < active_player_count[0];
           candidate_player_index++) {
        if (player_records[candidate_player_index].hole_status == 1) {
          has_finished_player = 1;
          if (calculate_player_adjusted_score(candidate_player_index) <
              lowest_adjusted_score) {
            lowest_adjusted_score =
                calculate_player_adjusted_score(candidate_player_index);
          }
        }
      }
      for (candidate_player_index = 0;
           candidate_player_index < active_player_count[0];
           candidate_player_index++) {
        if (candidate_player_index != player_index &&
            player_records[candidate_player_index].hole_status == 0) {
          if (calculate_player_adjusted_score(candidate_player_index) <
              calculate_player_adjusted_score(player_index)) {
            hole_result = 3;
            break;
          }
          if (calculate_player_adjusted_score(candidate_player_index) ==
              calculate_player_adjusted_score(player_index)) {
            hole_result = 1;
          }
        }
      }
      if (hole_result == 3) {
        break;
      }
      if (has_finished_player != 0) {
        if (calculate_player_adjusted_score(player_index) + 1 ==
            lowest_adjusted_score) {
          hole_result = 1;
        }
        if (calculate_player_adjusted_score(player_index) + 1 >=
            lowest_adjusted_score) {
          return hole_result;
        }
      }
      if (hole_result == 1) {
        hole_result = 3;
      }
      return hole_result;
    }
  }
  return hole_result;
}
