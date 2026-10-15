#ifndef SESSION_H
#define SESSION_H

#include "common.h"

/* Live player state and its saved copies have the same 0xB8-byte layout.
 * player_base_scores/status/handicaps/hole_scores are interior linker labels,
 * not separately allocated arrays. */
typedef struct {
  s8 score;
  s8 unknown_01;
} PlayerHoleScore; /* 0x02 */

typedef struct {
  /* 0x00 */ u8 unknown_00[0x0C];
  /* 0x0C */ s32 state_code;
  /* 0x10 */ u8 unknown_10[0x0D];
  /* 0x1D */ s8 character_index;
  /* 0x1E */ u8 unknown_1E[5];
  /* 0x23 */ s8 base_score;
  /* 0x24 */ u8 unknown_24[2];
  /* 0x26 */ s8 hole_status;
  /* 0x27 */ u8 unknown_27[2];
  /* 0x29 */ s8 handicap;
  /* 0x2A */ u8 unknown_2A;
  /* 0x2B */ PlayerHoleScore hole_scores[18];
  /* 0x4F */ u8 unknown_4F[3];
  /* 0x52 */ s16 club_ids[16];
  /* 0x72 */ u8 unknown_72[9];
  /* 0x7B */ u8 asset_variant;
  /* 0x7C */ u8 unknown_7C[0x3C];
} PlayerState; /* 0xB8 */

/* The live 0x44-byte scenario block is also copied into continue slots. */
typedef struct {
  /* 0x00 */ s32 state_id;
  /* 0x04 */ s32 game_mode;
  /* 0x08 */ s32 active_player_count;
  /* 0x0C */ s32 scenario_mode_id;
  /* 0x10 */ s32 round_hole_index;
  /* 0x14 */ u32 unknown_14;
  /* 0x18 */ u32 unknown_18[3];
  /* 0x24 */ s32 hole_flags;
  /* 0x28 */ s32 elapsed_frames;
  /* 0x2C */ s32 roster_random_seed;
  /* 0x30 */ u8 unknown_30;
  /* 0x31 */ s8 rumble_disabled;
  /* 0x32 */ u8 unknown_32[8];
  /* 0x3A */ s8 unknown_3A;
  /* 0x3B */ u8 unknown_3B[2];
  /* 0x3D */ s8 continue_count;
  /* 0x3E */ u8 unknown_3E[6];
} ScenarioState; /* 0x44 */

typedef struct {
  /* 0x00 */ s32 x;
  /* 0x04 */ s32 y;
  /* 0x08 */ s32 z;
  /* 0x0C */ u32 unknown_0C[10];
} CameraSnapshot; /* 0x34 */

typedef struct {
  u16 unknown_00;
  s8 unknown_02[6];
} ModeAuxFields; /* 0x08 */

typedef struct {
  /* 0x00 */ u8 unknown_00[0x34];
  /* 0x34 */ s32 game_mode;
  /* 0x38 */ s8 scenario_mode_id;
  /* 0x39 */ s8 round_hole_index;
  /* 0x3A */ u8 unknown_3A[4];
  /* 0x3E */ u8 unknown_3E;
  /* 0x3F */ u8 unknown_3F[5];
  /* 0x44 */ ModeAuxFields mode_aux_fields[4];
  /* 0x64 */ u8 unknown_64[4];
} GameModeRecord; /* 0x68 */

typedef struct {
  /* 0x00 */ u32 unknown_00[13];
  /* 0x34 */ s8 scenario_mode_id;
  /* 0x35 */ s8 round_hole_index;
  /* 0x36 */ u8 unknown_36[4];
  /* 0x3A */ u8 unknown_3A;
  /* 0x3B */ u8 unknown_3B[5];
  /* 0x40 */ ModeAuxFields mode_aux_fields[4];
  /* 0x60 */ u8 unknown_60[8];
} CourseSelectionRecord; /* 0x68 */

/* SRAM statistics at save + 0xE98, indexed by scenario mode. The podium
 * fields and round totals belong to this same record, not the player array. */
typedef struct {
  /* 0x00 */ s32 sample_count;
  /* 0x04 */ s32 best_round_score;
  /* 0x08 */ s32 hole_score_total;
  /* 0x0C */ s32 high_water_mark;
  /* 0x10 */ s32 weighted_average;
  /* 0x14 */ s32 average_sample_count;
  /* 0x18 */ s32 unknown_18;
  /* 0x1C */ s32 unknown_1C;
  /* 0x20 */ s32 unknown_counter_20;
  /* 0x24 */ s32 unknown_counter_24;
  /* 0x28 */ s32 unknown_28;
  /* 0x2C */ s32 unknown_2C;
  /* 0x30 */ s32 third_place_achieved;
  /* 0x34 */ s32 second_place_achieved;
  /* 0x38 */ s32 first_place_achieved;
  /* 0x3C */ s16 best_round_score_by_character[18];
  /* 0x60 */ u8 character_has_record[18];
  /* 0x72 */ u8 unknown_72[2];
} ScenarioStatistics; /* 0x74 */

extern PlayerState player_records[];
extern ScenarioState scenario_state_id;

#endif /* SESSION_H */
