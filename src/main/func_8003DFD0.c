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
 * scenery wind angle via sinf/cosf, projected onto terrain height. Structure
 * and math fully solved (see nonmatchings/func_8003E004/base.c); carried as
 * INCLUDE_ASM.
 *
 * S204 "move_movables DFmode-hoist wall / not blind-retryable" is REFUTED (S235
 * gcc-2.7.2 source dive). The hoist + the prologue schedule are SOLVED by 3
 * deterministic C levers (17500 -> 5380 decomp_loop, structurally 100%, all 206
 * rows align, top_mismatches empty):
 *   1. HOIST fix (loop.c:1631): reuse ONE f64 for (f64)base then ang_hi -- a
 * pseudo SET TWICE in the loop is not a single-set invariant, so move_movables
 * cannot hoist the cvt.d.s. Frame 328->320, the mfc1/mtc1 GPR-pair spill and
 * the loop- bound rematerialization both vanish.
 *   2. CALL-ORDER: inline the angle assignment into the sinf() arg so the
 * conversion lands in the 2nd BB (calls are schedule barriers), matching the
 * ROM.
 *   3. PROLOGUE: share one pointer for the angle load and base_pos so sched2
 * groups all register saves first (ROM layout) instead of interleaving them.
 * RESIDUAL (permuter territory, NOT a compiler wall): a global.c live-length RA
 * tie. global.c:587 allocno_compare gives a-pointer (pseudo 233, n_refs 8)
 * strictly higher priority than loop counter i (pseudo 75, n_refs 6); priority
 * = floor_log2(refs)*refs/ live_length, so the ROM's ordering needs
 * live_length(a) >= 2*live_length(i), a ratio only reachable by a scheduling
 * shift -- explicit a/b pointers vs direct buf[i*2] indexing compile
 * BYTE-IDENTICAL (both 5380), so it is NOT faithful-C controllable. Cascades
 * ~12 insns ($s1/$s2) + a scratch-vs-callee double-const nit + one 2nd-loop
 * store addressing mode. gas EXONERATED (aligned rows byte-identical;
 * divergence is pre-gas RTL regalloc). Permuter-reachable in principle but
 * empirically STUBBORN: ~158k iters across two runs, 0 breaks, stuck at
 * permuter-floor 1255. Re-attempt only with a materially different strategy
 * (much longer/wider run, seed diversity to escape the 1255 minimum, or a
 * hand-perturbed seed that shifts i's/a's live-length). All rodata/data refs
 * (D_800CA8E0/E8/F0 pi/2 pool, D_800BA888, g_scenery_wind_angle_a) stay extern
 * in the shared blob. */
INCLUDE_ASM("asm/nonmatchings/main/func_8003DFD0", func_8003E004);

extern Gfx* glistp;
extern Vtx D_800BA888[];

void func_8003E314(void) {
  s32 i;

  gDPSetCombineLERP(glistp++, 0, 0, 0, SHADE, SHADE, 0, TEXEL0, 0, 0, 0, 0,
                    SHADE, SHADE, 0, TEXEL0, 0);
  gDPPipeSync(glistp++);
  gSPVertex(glistp++, D_800BA888, 18, 0);
  for (i = 0; i != 8; i++) {
    gSP2Triangles(glistp++, i * 2, i * 2 + 2, i * 2 + 1, 0, i * 2 + 1,
                  i * 2 + 2, i * 2 + 3, 0);
  }
}
