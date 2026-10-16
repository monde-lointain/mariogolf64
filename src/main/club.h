#ifndef CLUB_H
#define CLUB_H

#include "common.h"

/* Runtime shot parameters. Meter helpers consume this record, not the separate
 * distance-preset table returned by func_80043AF0. */
typedef struct {
  /* 0x00 */ u8 unknown_00;
  /* 0x01 */ u8 unknown_01[7];
  /* 0x08 */ s32 flags;
  /* 0x0C */ s16 target_distance;
  /* 0x0E */ s16 launch_angle_degrees;
  /* 0x10 */ u8 unknown_10[4];
  /* 0x14 */ s16 quality_scale_percent;
  /* 0x16 */ s16 club_index;
  /* 0x18 */ s32 distances[3];
  /* 0x24 */ s16 kind;
  /* 0x26 */ s16 unknown_26;
  /* 0x28 */ s16 unknown_28;
  /* 0x2A */ s16 unknown_2A;
  /* 0x2C */ s16 unknown_2C;
  /* 0x2E */ u16 asset_id;
  /* 0x30 */ s8 unknown_30;
  /* 0x31 */ u8 unknown_31[3];
} ClubShot; /* 0x34 */

/* Generated CPARA records contain two groups of three words. Only the first
 * group's distance meaning is established by the recovered consumers. */
typedef struct {
  /* 0x00 */ s32 distances[3];
  /* 0x0C */ s32 unknown_0C[3];
} ClubDistancePreset; /* 0x18 */

ClubShot* get_club_param(u32 id);
ClubShot* get_shot_data(void);
ClubDistancePreset* func_80043AF0(s32 index);
s32 get_club_meter_extent(ClubShot* club, s32 category);
s32 get_club_meter_units(ClubShot* club, s32 category);
s32 resolve_shot_quality_table(ClubShot* club, s32 terrain, s32 category);

#endif /* CLUB_H */
