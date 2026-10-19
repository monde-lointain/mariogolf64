#ifndef SESSION_H
#define SESSION_H

#include "common.h"
#include "vector.h"

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
  /* 0x7C */ u8 unknown_7C[4];
  /* 0x80 */ s32 unknown_80; /* -2 pending, -1 no result */
  /* 0x84 */ s32 unknown_84; /* -2 pending, -1 no result */
  /* 0x88 */ u8 unknown_88[0x14];
  /* 0x9C */ s8 unknown_9C;
  /* 0x9D */ u8 unknown_9D[3];
  /* 0xA0 */ s32 pending_result;
  /* 0xA4 */ u8 unknown_A4[0x14];
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
  /* 0x3A */ s8 terrain_transform_mode; /* g_terrain_vtx_xform_mode interior label */
  /* 0x3B */ u8 unknown_3B[2];
  /* 0x3D */ s8 continue_count;
  /* 0x3E */ u8 unknown_3E[6];
} ScenarioState; /* 0x44 */

/* Live hole target, wind, and rendering environment; copied into saved modes. */
typedef struct {
  /* 0x00 */ Vec3i pin_position; /* game-space coordinates, 1024 units per world unit */
  /* 0x0C */ u32 unknown_0C[3];
  /* 0x18 */ s32 wind_magnitude;
  /* 0x1C */ s16 wind_angle_bam;
  /* 0x1E */ u8 unknown_1E[2];
  /* 0x20 */ s32 putter_mode;
  /* 0x24 */ u32 unknown_24;
  /* 0x28 */ s16 scenery_wind_angles[2];
  /* 0x2C */ s8 unknown_2C;
  /* 0x2D */ u8 unknown_2D[7];
} HoleEnvironmentState; /* 0x34 */

/* Keep the retail linker label; its historical name describes neither the
 * pin position nor the enclosing environment record accurately. */
extern HoleEnvironmentState hole_environment __asm__("camera_position_x");

/* Per-round accumulators saved with the live session. Result units remain
 * unknown; include_in_average selects the subset accumulated at +0x10/+0x14. */
typedef struct {
  /* 0x00 */ s32 unknown_00;
  /* 0x04 */ s32 unknown_04;
  /* 0x08 */ s32 result_count;
  /* 0x0C */ s32 result_max;
  /* 0x10 */ s32 result_total;
  /* 0x14 */ s32 average_sample_count;
  /* 0x18 */ s32 include_in_average;
} RoundStatistics; /* 0x1C */

extern RoundStatistics D_800FF4D0;

typedef struct {
  u16 unknown_00;
  s8 unknown_02[6];
} ModeAuxFields; /* 0x08 */

typedef struct {
  /* 0x00 */ HoleEnvironmentState environment;
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
  /* 0x00 */ HoleEnvironmentState environment;
  /* 0x34 */ s8 scenario_mode_id;
  /* 0x35 */ s8 round_hole_index;
  /* 0x36 */ u8 unknown_36[4];
  /* 0x3A */ u8 unknown_3A;
  /* 0x3B */ u8 unknown_3B[5];
  /* 0x40 */ ModeAuxFields mode_aux_fields[4];
  /* 0x60 */ u8 unknown_60[8];
} CourseSelectionRecord; /* 0x68 */

typedef struct {
  /* 0x00 */ s8 par;
  /* 0x01 */ u8 unknown_01[9];
} CourseHoleDefinition; /* 0x0A */

typedef struct {
  /* 0x00 */ char name[20];
  /* 0x14 */ CourseHoleDefinition holes[18];
} CourseDefinition; /* 0xC8 */

extern CourseDefinition D_800C1420[];

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

/* Zeroed by func_8006F24C in both branches and by the overlay_15 reset
 * func_ovl15_801F67F4. Only the words those resets touch are named; the rest
 * of the 0x98-byte block is not yet recovered. */
typedef struct {
  /* 0x00 */ s32 unknown_00;
  /* 0x04 */ s32 unknown_04;
  /* 0x08 */ s32 unknown_08;
  /* 0x0C */ u8 unknown_0C[0x28];
  /* 0x34 */ s32 unknown_34;
  /* 0x38 */ s32 unknown_38;
  /* 0x3C */ u8 unknown_3C[0x50];
  /* 0x8C */ s32 unknown_8C;
  /* 0x90 */ s32 unknown_90;
  /* 0x94 */ s32 unknown_94;
} SessionAuxState; /* 0x98 */

extern SessionAuxState D_800FEC00;

#endif /* SESSION_H */
