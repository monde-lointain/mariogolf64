#include "common.h"
#include "session.h"

extern u8* func_8005AF50(void);
extern s32 get_round_last_hole(void);
extern s32 func_8006CD50(s32, s32);
extern u8 roster_sort_and_rank(s32, s32, s32, s32);
extern void func_8005DF54(u8*, s32);

extern s32 scenario_mode_id;
extern s32 game_mode;
extern s32 roster_random_seed;

typedef struct {
  s8 id;
  u8 padding[4];
  u8 rank;
} RankedRosterEntry;
extern RankedRosterEntry ranked_roster_ids[];


void save_scenario_roster_rank(void) {
  ScenarioStatistics* scenario_record =
      (ScenarioStatistics*)(func_8005AF50() + 0xE98) + scenario_mode_id;
  s32 has_podium_finish = 0;

  if (game_mode == 5) {
    s32 hole_limit = get_round_last_hole() + 1;
    s32 last_hole_index = get_round_last_hole();
    s32 initial_score = func_8006CD50(last_hole_index + 1, 0);

    roster_sort_and_rank(hole_limit, 0, initial_score, roster_random_seed);
    {
      RankedRosterEntry* roster_entries = ranked_roster_ids;
      s32 roster_index;
      s32 selected_rank = 4;
      s32* placement_achievement;

      for (roster_index = 0; roster_index != 0x1E; roster_index++) {
        if (roster_entries[roster_index].id == -1) {
          selected_rank = roster_entries[roster_index].rank & 0x7F;
          break;
        }
      }

      switch (selected_rank) {
        case 1:
          placement_achievement = &scenario_record->first_place_achieved;
          has_podium_finish = 1;
          break;
        case 2:
          placement_achievement = &scenario_record->second_place_achieved;
          has_podium_finish = 1;
          break;
        case 3:
          placement_achievement = &scenario_record->third_place_achieved;
          has_podium_finish = 1;
          break;
        default:
          placement_achievement = &scenario_record->third_place_achieved;
          break;
      }
      if (has_podium_finish) {
        *placement_achievement = 1;
      }
    }

    func_8005DF54(func_8005AF50(), 1);
  }
}
