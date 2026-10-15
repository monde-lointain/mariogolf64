#include "common.h"

extern s32 scenario_mode_id;
extern u32 game_mode;
extern s32 active_player_count[];
extern s32 round_hole_index;
extern s8 player_base_scores[];
extern s8 player_hole_status[];
extern s8 player_handicaps[];
extern s8 player_hole_scores[];
extern s32 D_801B60A0[];
extern s32 get_match_play_result(s32 player_a, s32 player_b);
extern s32 get_round_first_hole(void);
extern s32 get_round_hole_count(void);
extern s8 hole_handicap_ranks[];
/* The retail caller forwards its selector in a0 even though the lookup uses
 * global state. Dropping that argument changes this caller's allocation. */
extern s32 get_current_course_id(s32 course_id);
extern void rank_scores_descending(s32* scores, s32* player_order, s32 count);
extern s32 calculate_hole_handicap_adjustment(s32 course_id, s32 hole_index,
                                              s32 score);

s32 is_special_course_active(void) { return scenario_mode_id == 0xC; }

s32 calculate_hole_handicap_adjustment(s32 course_id, s32 hole_index,
                                       s32 score) {
  return -score / 18 -
         (score % 18 >=
          hole_handicap_ranks[get_current_course_id(course_id) * 200 +
                              hole_index * 10]);
}

s32 calculate_player_adjusted_score(s32 player_index) {
  return player_base_scores[player_index * 0xB8] +
         calculate_hole_handicap_adjustment(
             scenario_mode_id, round_hole_index,
             player_handicaps[player_index * 0xB8]);
}

void initialize_hole_handicap_ranks(void) {
  s32 scores_and_order[36];
  s32* score_order = scores_and_order;
  s32 rank_count = 18;
  s32 course_offset;
  s32 table_offset;
  s32 hole_index;

  for (course_offset = 0; course_offset < 1600; course_offset += 200) {
    for (hole_index = 0; hole_index < 18; hole_index++) {
      table_offset = course_offset + hole_index * 10;
      score_order[hole_index] = hole_handicap_ranks[table_offset];
    }
    rank_scores_descending(scores_and_order, &scores_and_order[18], 18);
    for (hole_index = 0, table_offset = course_offset; hole_index < 18;
         hole_index++) {
      s32 rank_offset = score_order[18 + hole_index] * 10 + table_offset;
      hole_handicap_ranks[rank_offset] = rank_count - hole_index;
    }
  }
}

s32 get_match_play_result(s32 player_a, s32 player_b) {
  s32 score_span;
  s32 remaining_holes;
  s32 score_difference;
  s32 score_difference_magnitude;
  s32 difference_magnitude;
  s32 first_difference_magnitude;
  s32 hole_index;

  score_span = get_round_first_hole() + get_round_hole_count();
  score_difference = 0;

  if (D_801B60A0[0] != 0) {
    return 3;
  }

  remaining_holes = score_span - round_hole_index - 1;
  hole_index = get_round_first_hole();
  if (hole_index < round_hole_index) {
    s32 final_hole;

    do {
      s32 player_a_score;
      s32 player_b_score;
      s32 player_a_offset = player_a * 184 + hole_index * 2;
      s32 player_b_offset = player_b * 184 + hole_index * 2;

      final_hole = D_801B60A0[-2];
      player_a_score = player_hole_scores[player_a_offset];
      player_b_score = player_hole_scores[player_b_offset];
      if (player_a_score < player_b_score) {
        score_difference++;
      } else {
        score_difference -= (player_b_score < player_a_score);
      }
      hole_index++;
    } while (hole_index < final_hole);
  }

  first_difference_magnitude = score_difference;
  if (first_difference_magnitude < 0) {
    first_difference_magnitude = -first_difference_magnitude;
  }
  if (first_difference_magnitude == remaining_holes + 1) {
    if (score_difference < 0) {
      return 2;
    }
    return 1;
  }
  difference_magnitude = score_difference;
  if (difference_magnitude < 0) {
    difference_magnitude = -difference_magnitude;
  }
  if (remaining_holes + 1 < difference_magnitude) {
    return -1;
  }
  score_difference_magnitude = score_difference;
  if (score_difference_magnitude < 0) {
    score_difference_magnitude = -score_difference_magnitude;
  }
  if (score_difference_magnitude == remaining_holes) {
    if (remaining_holes == 0) {
      return 3;
    }
  }
  return 0;
}

s32 get_handicap_match_play_result(s32 player_a, s32 player_b) {
  s32 score_span;
  s32 remaining_holes;
  s32 score_difference;
  s32 difference_magnitude;
  s32 player_a_score;
  s32 player_b_score;
  s32 hole_index;
  s32* tournament_state = D_801B60A0;
  s32* tournament_config;
  s32* tournament_config_for_scores;

  score_span = get_round_first_hole() + get_round_hole_count();
  score_difference = 0;

  if (tournament_state[0] != 0) {
    hole_index = get_round_first_hole();
    if (hole_index >= round_hole_index) {
      return 0;
    }
    tournament_config = tournament_state - 3;
    do {
      s32 player_a_base = player_a * 184;
      s32 player_b_base = player_b * 184;
      s32 player_a_offset = player_a_base + hole_index * 2;
      s32 player_b_offset = player_b_base + hole_index * 2;

      player_a_score =
          player_hole_scores[player_a_offset] +
          calculate_hole_handicap_adjustment(tournament_config[0], hole_index,
                                             player_handicaps[player_a_base]);
      player_b_score =
          player_hole_scores[player_b_offset] +
          calculate_hole_handicap_adjustment(tournament_config[0], hole_index,
                                             player_handicaps[player_b_base]);
      if (player_a_score < player_b_score) {
        return 1;
      }
      if (player_b_score < player_a_score) {
        return 2;
      }
      hole_index++;
    } while (hole_index < tournament_config[1]);
    return 0;
  }

  remaining_holes = score_span - round_hole_index - 1;
  hole_index = get_round_first_hole();
  if (hole_index < round_hole_index) {
    tournament_config_for_scores = tournament_state - 3;
    do {
      s32 player_a_base = player_a * 184;
      s32 player_b_base = player_b * 184;
      s32 player_a_offset = player_a_base + hole_index * 2;
      s32 player_b_offset = player_b_base + hole_index * 2;

      player_a_score = player_hole_scores[player_a_offset] +
                       calculate_hole_handicap_adjustment(
                           tournament_config_for_scores[0], hole_index,
                           player_handicaps[player_a_base]);
      player_b_score = player_hole_scores[player_b_offset] +
                       calculate_hole_handicap_adjustment(
                           tournament_config_for_scores[0], hole_index,
                           player_handicaps[player_b_base]);
      score_difference += (player_a_score < player_b_score);
      score_difference -= (player_b_score < player_a_score);
      hole_index++;
    } while (hole_index < tournament_config_for_scores[1]);
  }
  difference_magnitude =
      (score_difference < 0) ? -score_difference : score_difference;
  if (remaining_holes + 1 < difference_magnitude) {
    if (score_difference > 0) {
      return 1;
    }
    return 2;
  }
  return 0;
}

s32 compare_player_hole_results(s32 player_a, s32 player_b) {
  s32 comparison_result;
  s32 player_record_offset;

  player_record_offset = player_a * 184;
  if (player_hole_status[player_record_offset] == 0) {
    player_record_offset = player_b * 184;
    if (player_hole_status[player_record_offset] == 0) {
      /* A direct return changes the retail branch polarity and delay slot. */
      goto undecided;
    }
  } else {
    player_record_offset = player_b * 184;
    if (player_hole_status[player_record_offset] != 0) {
      if (calculate_player_adjusted_score(player_a) <
          calculate_player_adjusted_score(player_b)) {
        return 1;
      }
      return (calculate_player_adjusted_score(player_b) >=
              calculate_player_adjusted_score(player_a))
                 ? 3
                 : 2;
    }
    if (calculate_player_adjusted_score(player_a) <
        calculate_player_adjusted_score(player_b) + 1) {
      return 1;
    }
  }

  player_record_offset = player_b * 184;
  if (player_hole_status[player_record_offset] != 0 &&
      calculate_player_adjusted_score(player_b) <
          calculate_player_adjusted_score(player_a) + 1) {
    return 2;
  }

  comparison_result = get_match_play_result(player_a, player_b);
  if (comparison_result == 1) {
    player_record_offset = player_a * 184;
    if (player_hole_status[player_record_offset] != 0 &&
        calculate_player_adjusted_score(player_a) <=
            calculate_player_adjusted_score(player_b) + 1) {
      return 3;
    }
  }
  if (comparison_result == 2) {
    player_record_offset = player_b * 184;
    if (player_hole_status[player_record_offset] != 0 &&
        calculate_player_adjusted_score(player_b) <=
            calculate_player_adjusted_score(player_a) + 1) {
      return 3;
    }
  }
undecided:
  return 0;
}

s32 resolve_hole_winners(s8* player_marks) {
  s8 local_marks[4];
  s32 minimum_score;
  s32 best_player;
  s32 tie_count;
  s32 status_three_count;
  s32 can_resolve;
  s32 player;

  minimum_score = 0x40000000;
  best_player = -1;
  can_resolve = 0;
  tie_count = 0;
  status_three_count = 0;

  if (player_marks == NULL) {
    player_marks = local_marks;
  }

  for (player = 0; player < active_player_count[0]; player++) {
    s32 player_record_offset = player * 184;

    if (player_hole_status[player_record_offset] == 1) {
      if (calculate_player_adjusted_score(player) < minimum_score) {
        minimum_score = calculate_player_adjusted_score(player);
        best_player = player;
        tie_count = 0;
      } else {
        tie_count += (minimum_score == calculate_player_adjusted_score(player));
      }
    } else {
      status_three_count += (player_hole_status[player_record_offset] == 3);
    }
  }

  if (best_player == -1) {
    for (player = 0; player < active_player_count[0]; player++) {
      player_marks[player] = 0;
    }
    if (status_three_count == active_player_count[0]) {
      return 2;
    }
    return can_resolve;
  }

  can_resolve = 1;
  for (player = 0; player < active_player_count[0]; player++) {
    s32 player_record_offset = player * 184;

    player_marks[player] = 0;
    if (player_hole_status[player_record_offset] == 0) {
      if (calculate_player_adjusted_score(player) >= minimum_score) {
        player_marks[player] = 2;
      } else if (calculate_player_adjusted_score(player) < minimum_score - 1) {
        can_resolve = 0;
      } else {
        can_resolve = tie_count ? can_resolve : 0;
      }
    } else if (minimum_score < calculate_player_adjusted_score(player)) {
      player_marks[player] = 2;
    } else if (player_hole_status[player_record_offset] >= 2) {
      player_marks[player] = 2;
    }
  }

  if (can_resolve == 0) {
    return 0;
  }
  if (tie_count > 0) {
    return 2;
  }

  for (player = 0; player < active_player_count[0]; player++) {
    s32 player_record_offset = player * 184;

    if (player_hole_status[player_record_offset] == 1 &&
        calculate_player_adjusted_score(player) == minimum_score) {
      player_marks[player] = 1;
    }
  }
  return 1;
}

s32 get_player_hole_result(s32 player) {
  s32 result;
  s32 opponent;
  s32 minimum_score;
  s32 any_finished_player;
  s32 player_index;

  result = 3;

  switch (game_mode) {
    case 2:
    case 4:
      opponent = player ^ 1;
      if (calculate_player_adjusted_score(player) ==
              calculate_player_adjusted_score(opponent) - 1 &&
          player_hole_status[opponent * 184] != 0) {
        result = 4;
        if (get_match_play_result(player, opponent) == 1) {
          result = 2;
        }
        break;
      }
      if (calculate_player_adjusted_score(player) <
          calculate_player_adjusted_score(opponent)) {
        result = (get_match_play_result(player, opponent) == 3) * 2;
        break;
      }
      if (calculate_player_adjusted_score(player) >=
          calculate_player_adjusted_score(opponent) + 1) {
        break;
      }
      if (player_hole_status[opponent * 184] == 1) {
        break;
      }
      if (get_match_play_result(player, opponent) == 1) {
        result = 2;
      }
      break;

    case 10:
    case 11:
      any_finished_player = 0;
      minimum_score = 0x40000000;
      result = 0;
      for (player_index = 0; player_index < active_player_count[0];
           player_index++) {
        s32 player_record_offset = player_index * 184;

        if (player_hole_status[player_record_offset] == 1) {
          any_finished_player = 1;
          if (calculate_player_adjusted_score(player_index) < minimum_score) {
            minimum_score = calculate_player_adjusted_score(player_index);
          }
        }
      }
      for (player_index = 0; player_index < active_player_count[0];
           player_index++) {
        s32 player_record_offset = player_index * 184;

        if (player_index != player &&
            player_hole_status[player_record_offset] == 0) {
          if (calculate_player_adjusted_score(player_index) <
              calculate_player_adjusted_score(player)) {
            result = 3;
            break;
          }
          if (calculate_player_adjusted_score(player_index) ==
              calculate_player_adjusted_score(player)) {
            result = 1;
          }
        }
      }
      if (result == 3) {
        break;
      }
      if (any_finished_player != 0) {
        if (calculate_player_adjusted_score(player) + 1 == minimum_score) {
          result = 1;
        }
        if (calculate_player_adjusted_score(player) + 1 >= minimum_score) {
          return result;
        }
      }
      if (result == 1) {
        result = 3;
      }
      return result;
  }
  return result;
}
