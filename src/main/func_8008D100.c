#include "common.h"

extern u32 sky_panel_cycle_mode_sel;
extern s32 D_800C5EE4;
extern s32 D_800C7304;
extern s32 D_800C730C;

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8008D100);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8008D1DC);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8008D3F4);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8008DDDC);

/* Integer lerp, truncated: a0*(1-t) + a1*t, t in $a2 (o32 GPR).
 * The dead `volatile s32` local (seeded from an uninitialized local) reproduces
 * the ROM's dead 8-byte frame + `sw $v0` store of the incoming (uninit) return
 * register: the volatile addressed local forces the frame (compute_frame_size,
 * mips.c:4444) and its store survives DCE (flow.c volatile exemption), while
 * the uninitialized source emits no load, storing $v0 as-is. */
s32 lerp_s32(s32 a0, s32 a1, f32 t) {
  s32 r;
  volatile s32 unused = r;
  return a0 * (1.0f - t) + a1 * t;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100",
            per_hole_skybox_palette_load);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8008E82C);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", update_sky_panel_verts);

void set_sky_panel_cycle_mode_sel(u32 arg0) { sky_panel_cycle_mode_sel = arg0; }

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", emit_sky_bg_panel_dl);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", emit_sky_panorama_strips_dl);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8009226C);

void func_80092324(void) {}

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8009232C);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_80092E10);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_80092F18);

void func_800934CC(s32 arg0) {
  if (arg0 == -1) {
    D_800C7304 = 0;
  } else if (arg0 == 3) {
    D_800C730C = arg0;
    D_800C7304 = 2;
  } else {
    D_800C7304 = 1;
    D_800C730C = arg0;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8009351C);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_800939E8);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_80094228);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_800947A8);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_80095150);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8009548C);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_800957F0);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_800958D8);

s32 func_800959F8(void) { return D_800C5EE4 < 1; }
