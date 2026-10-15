#include "common.h"

extern u8* func_8005AF50(void);
extern void func_8005DF54(u8*, s32);
extern void func_8007FE44(s32);

extern s8 D_801B60C5;
extern s32 game_mode;
extern s32 round_hole_index;
extern s32 scenario_mode_id;
extern char D_800D1570[];

typedef struct {
  u8 padding[0xF4];
  s8 result_counts[6][0x12];
  u8 outcome_counts[13][0x12][2];
} SaveBlock;

void save_record_hole_result(s32 course_index, s32 hole_index,
                             s32 result_kind) {
  SaveBlock* save_block = (SaveBlock*)(func_8005AF50() + 0x990);
  s32 updated_count;

  if (result_kind != 0) {
    s32 old_count_and_flag =
        save_block->outcome_counts[course_index][hole_index][1];
    u8 count = old_count_and_flag & 0x7F;

    if (count < 99) {
      count = count + 1;
    }
    updated_count = count | (old_count_and_flag & 0x80);
    old_count_and_flag = updated_count;
    if (D_801B60C5 == 0) {
      old_count_and_flag = updated_count | 0x80;
    }
    save_block->outcome_counts[course_index][hole_index][1] =
        old_count_and_flag;
    osSyncPrintf(D_800D1570, old_count_and_flag);
  } else {
    updated_count = save_block->outcome_counts[course_index][hole_index][0] + 1;
    save_block->outcome_counts[course_index][hole_index][0] = updated_count;
    if (99 < (u8)updated_count) {
      save_block->outcome_counts[course_index][hole_index][0] = 99;
    }
  }

  func_8005DF54(func_8005AF50(), 1);
}

void save_increment_scenario_result_count(void) {
  SaveBlock* save_block = (SaveBlock*)(func_8005AF50() + 0x990);

  if (game_mode == 5) {
    save_block->result_counts[scenario_mode_id][round_hole_index] += 1;
    if (99 < save_block->result_counts[scenario_mode_id][round_hole_index]) {
      save_block->result_counts[scenario_mode_id][round_hole_index] = 99;
    }
    func_8005DF54(func_8005AF50(), 1);
  }
}

void save_count_completed_scenario_results(void) {
  SaveBlock* save_block = (SaveBlock*)(func_8005AF50() + 0x990);

  if (game_mode == 5 &&
      save_block->result_counts[scenario_mode_id][round_hole_index] == 0) {
    s32 completed_result_count = 0;
    s32 course_index;
    for (course_index = 0; course_index != 6; course_index++) {
      s32 result_index;
      for (result_index = 0; result_index != 0x12; result_index++) {
        s32 result_count =
            save_block->result_counts[course_index][result_index];
        completed_result_count += (result_count != 0);
      }
    }
    func_8007FE44(completed_result_count);
  }
}
