#ifndef FOG_H
#define FOG_H

#include "common.h"

/* One 0x20-byte table entry. Debug-page headings identify the first three
 * color sets as T, B, and C, and the fourth as BG; BG's fourth byte is the
 * blend percentage. The lens-flare cap pointer and per-hole fog RGB/Z values
 * are additional fields of this same shared entry, not independent views. */
typedef struct LensFlareCap {
  /* 0x00 */ u8 unknown_00[0x18];
  /* 0x18 */ f32 flare_alpha_max;
} LensFlareCap; /* 0x1C */

typedef struct FogPreset {
  /* 0x00 */ s32 cc;
  /* 0x04 */ u8 color_set_t[3];
  /* 0x07 */ u8 color_set_b[3];
  /* 0x0A */ u8 color_set_c[3];
  /* 0x0D */ u8 color_set_bg_and_blend[4];
  /* 0x11 */ u8 lens_flare_enabled;
  /* 0x12 */ u8 unknown_12[2];
  /* 0x14 */ const LensFlareCap* flare_cap;
  /* 0x18 */ u8 fog_color_rgb[3];
  /* 0x1B */ u8 unknown_1B;
  /* 0x1C */ s16 fog_z_min;
  /* 0x1E */ s16 fog_z_max;
} FogPreset; /* 0x20 */

typedef struct FogColorRecord {
  /* 0x00 */ u8 rgb[3];
  /* 0x03 */ u8 unknown_03[0x0D];
} FogColorRecord; /* 0x10 */

/* D_800C6014 spans nine 0x100-byte outer rows (0x900 bytes in the map), each
 * containing eight 0x20-byte records. Call sites select the outer row by
 * course, mode, or preset bank. D_800C69CC is a 0x280-byte aggregate; symbols
 * D_800C6A1C (+0x50) and sky_strip_params (+0x274) name interior aliases, not
 * independent allocations. */
extern FogPreset D_800C6014[9][8];
extern FogColorRecord D_800C69CC[4][10];

#endif /* FOG_H */
