#include "common.h"

extern s32 calculate_hypotenuse_safe(s32, s32);

s32 func_8003DFD0(s32 x, s32 z) {
  s32 distance;

  distance = calculate_hypotenuse_safe(x, z);
  if ((u32)distance < 0x699U) {
    distance = 0;
  } else {
    distance -= 0x699;
  }
  return distance;
}

/* func_8003E004: wind-indicator vertex generator. Fills the 18-entry Vtx array
 * D_800BA888 (drawn by func_8003E314) with a fan of positions computed from the
 * scenery wind angle via sinf/cosf, projected onto terrain height. Structure and
 * math are fully solved (see nonmatchings/func_8003E004/base.c); carried as
 * INCLUDE_ASM. ROOT CAUSE (S204 compiler-source dive, gcc-2.7.2 + binutils-2.6):
 * loop.c move_movables HOISTS the DFmode invariant (f64)base out of the vertex
 * loop, because threshold(~60)*savings(1)*lifetime(5) >= insn_count(~90)
 * (loop.c:1630-1631). Hoisted, (f64)base is live across every iteration, but all
 * 6 callee-saved FP regs hold per-iter temps, so it spills to a callee GPR PAIR
 * (the mfc1/mtc1 round-trip); that steals 2 of the 9 s-regs, so buf-base(sp+16)
 * spills to stack (frame 320->328) and the loop bound 9 rematerializes (li ,9).
 * The ROM instead computes cvt.d.s $f24=(f64)base IN-loop each iteration (never
 * hoisted). Reaching that needs (f64)base lifetime~1, but CSE re-shares the two
 * angle uses into one longer-lived movable; inlining forces lifetime-1 but
 * perturbs the whole allocation. 9 clean valid C variants plateau at 12439-14020
 * (pscore); permuter best 3510 is SEMANTICALLY INVALID (pre-init base). gas is
 * exonerated (faithful under .set noreorder). Not blind-retryable; needs a
 * source form that keeps base precomputed-invariant yet (f64)base in-loop, or a
 * from-scratch permuter run seeded past the hoist. All rodata/data refs
 * (D_800CA8E0/E8/F0 pi/2 pool, D_800BA888, g_scenery_wind_angle_a) stay extern
 * in the shared blob. */
INCLUDE_ASM("asm/nonmatchings/main/func_8003DFD0", func_8003E004);

extern Gfx* glistp;
extern Vtx D_800BA888[];

void func_8003E314(void) {
  s32 i;

  gDPSetCombineLERP(glistp++, 0, 0, 0, SHADE, SHADE, 0, TEXEL0, 0, 0, 0, 0, SHADE,
                    SHADE, 0, TEXEL0, 0);
  gDPPipeSync(glistp++);
  gSPVertex(glistp++, D_800BA888, 18, 0);
  for (i = 0; i != 8; i++) {
    gSP2Triangles(glistp++, i * 2, i * 2 + 2, i * 2 + 1, 0, i * 2 + 1, i * 2 + 2,
                  i * 2 + 3, 0);
  }
}
