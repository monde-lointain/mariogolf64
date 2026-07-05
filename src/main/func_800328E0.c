#include "common.h"

/* Display-list builders for the per-frame RCP viewport/scissor setup and the
 * full-screen fill clears. func_800328E0 emits the segment/viewport preamble and
 * splices the camera matrices in; func_800329F4 clears the play field to the RGB
 * set by func_800329D8; func_80032B78 clears the color/Z framebuffers with a
 * mode selector. func_800329B8/func_80032E34 are the &glistp wrappers. */

extern Gfx* glistp;

extern Vp D_800B7790[];
extern Gfx D_800B77F0[];
extern Gfx D_800B7820[];
extern s32 D_800B7840;
extern s32 D_800B7844;
extern s32 D_800B7848;

extern u16* nuGfxZBuffer;
extern u16* nuGfxCfb_ptr;

extern void set_camera_matrices_fixed(s32, s32, Vp*);
extern u32 osVirtualToPhysical(void*);

void func_800328E0(s32 arg0, Gfx** glistp) {
  Gfx* gfx = *glistp;

  gDPPipeSync(gfx++);
  gSPSegment(gfx++, 0, 0);
  gSPViewport(gfx++, &D_800B7790[arg0]);
  set_camera_matrices_fixed(0, 0, &D_800B7790[arg0]);
  gSPDisplayList(gfx++, D_800B7820);
  gSPDisplayList(gfx++, D_800B77F0);
  gDPPipeSync(gfx++);
  *glistp = gfx;
}

void func_800329B8(s32 arg0) {
  func_800328E0(arg0, &glistp);
}

void func_800329D8(s32 arg0, s32 arg1, s32 arg2) {
  D_800B7840 = arg0;
  D_800B7844 = arg1;
  D_800B7848 = arg2;
}

INCLUDE_ASM("asm/nonmatchings/main/func_800328E0", func_800329F4);

void func_80032B78(s32 arg0, Gfx** glistp) {
  Gfx* gfx = *glistp;
  s32 i;
  u32 zbuf = OS_K0_TO_PHYSICAL(nuGfxZBuffer) & ~7;

  gDPPipeSync(gfx++);
  gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, 320, 240);
  gDPSetDepthImage(gfx++, zbuf);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_FILL);
  gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, zbuf);
  gDPSetFillColor(gfx++, 0xFFFCFFFC);
  gDPFillRectangle(gfx++, 0, 0, 319, 239);
  gDPPipeSync(gfx++);
  gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                   osVirtualToPhysical(nuGfxCfb_ptr) & ~7);

  if (arg0 == 1) {
    for (i = 0; i < 239; i++) {
      gDPSetFillColor(gfx++, 0x00010001);
      gDPFillRectangle(gfx++, 0, i, 319, i);
    }
    gDPPipeSync(gfx++);
  } else if (arg0 == 2) {
    gDPSetFillColor(gfx++, 0x00010001);
    gDPPipeSync(gfx++);
    gDPFillRectangle(gfx++, 0, 0, 319, 239);
    gDPPipeSync(gfx++);
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  *glistp = gfx;
}

void func_80032E34(s32 arg0) {
  func_80032B78(arg0, &glistp);
}
