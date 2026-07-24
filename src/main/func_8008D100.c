#include "common.h"

extern u32 sky_panel_cycle_mode_sel;
extern s32 D_800C5EE4;
extern s32 D_800C7304;
extern s32 D_800C730C;

extern s32 D_801B6088;
extern s32 D_801B6098;
extern s32 scenario_mode_id;
extern s32 D_801B609C;
extern s32 D_801B608C;
extern s32 putter_mode_flag;
extern s32 wind_magnitude;
extern s16 wind_angle_bam;
extern u32 D_800BA9FC;
extern s16 g_scenery_wind_angle_b;
extern s16 g_scenery_wind_angle_a;
extern s32 D_801B6090;

extern void flag_set(s32);
extern void func_80069FBC(void);
extern void func_80069F38(void);
extern void cfb_set_num(u32);

extern Gfx* glistp;

typedef struct {
  s16 ulx;
  s16 uly;
  s16 lrx;
  s16 lry;
} ScissorRect;

extern ScissorRect D_801B7F30;

/* Game/session init: zero the 0x44-byte state block, seed mode/wind globals
 * (wind magnitude = (rand() & 0x3F) * 6000, wind angle from a second rand),
 * then run the sub-init calls and select framebuffer 2. */
void init_scenario_state(void) {
  memset(&D_801B6088, 0, 0x44);
  D_801B6098 = 9;
  scenario_mode_id = 0;
  D_801B609C = 0;
  D_801B608C = 0xC;
  putter_mode_flag = 0;
  wind_magnitude = (rand() & 0x3F) * 6000;
  wind_angle_bam = rand();
  D_800BA9FC = 6;
  g_scenery_wind_angle_b = 0x1000;
  g_scenery_wind_angle_a = 0x6000;
  flag_set(7);
  D_801B6090 = 1;
  func_80069FBC();
  func_80069F38();
  cfb_set_num(2);
}

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8008D1DC);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8008D3F4);

INCLUDE_ASM("asm/nonmatchings/main/func_8008D100", func_8008DDDC);

/* Integer lerp, truncated: a0*(1-t) + a1*t, t in $a2 (o32 GPR).
 *
 * TRUE ORIGIN: this is a GCC nested function (an orphaned out-of-line child).
 * The dead 8-byte frame + `sw $v0,0(sp)` (never reloaded) is the
 * nested-function prologue homing the incoming STATIC CHAIN ($v0 ==
 * STATIC_CHAIN_REGNUM = GP_REG_FIRST+2, mips.h:1310); this child never reads a
 * parent variable so the chain is homed but unused. A clean nested child `int
 * f(int a,int b,float t){ return a*(1-t)+b*t; }` inside a parent emits these
 * exact 20 words (verified byte-for-byte). The parent inlined its call, so no
 * jal / fn-pointer to 0x8008E164 survives anywhere in the ROM (orphaned dead
 * body).
 *
 * The `volatile s32` local below is a standalone stand-in that reproduces the
 * same bytes (it cannot be written as a real nested fn without its parent,
 * which is undecompiled and would not re-emit this symbol). Move it inside the
 * parent as a nested function if that parent is ever recovered. See the memory
 * dead-frame-dead-v0-store-crack and docs/wip/func_8008E164.nested.md. */
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

/* gsDPPipeSync + gsDPSetScissor emitter for the full 320x240 (0x140 x 0xF0)
 * screen. Coords come from a persistent 4x s16 rect at D_801B7F30; the scissor
 * macro does the *4.0f 10.2 fixed-point conversion. */
void emit_fullscreen_scissor_dl(void) {
  D_801B7F30.ulx = 0;
  D_801B7F30.lrx = 0x140;
  D_801B7F30.lry = 0xF0;
  D_801B7F30.uly = 0;
  gDPPipeSync(glistp++);
  gDPSetScissor(glistp++, G_SC_NON_INTERLACE, D_801B7F30.ulx, D_801B7F30.uly,
                D_801B7F30.lrx, D_801B7F30.lry);
}

s32 func_800959F8(void) { return D_800C5EE4 < 1; }
