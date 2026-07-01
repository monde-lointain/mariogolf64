#include "common.h"

/* A translucent textured rectangle that wipes its horizontal extent open/closed
 * over a few frames. func_8005029C emits the shared render setup then draws
 * every registered box; func_800500E0 advances one box's wipe and emits its
 * texrect; func_80050274 registers a box for the next frame. */
typedef struct {
  /* 0x00 */ s16 x0;
  /* 0x02 */ s16 y0;
  /* 0x04 */ s16 x1;
  /* 0x06 */ s16 y1;
  /* 0x08 */ u8 r;
  /* 0x09 */ u8 g;
  /* 0x0A */ u8 b;
  /* 0x0B */ u8 a;
  /* 0x0C */ s32 state;
  /* 0x10 */ s32 timer;
} WipeBox; /* 0x14 */

extern Gfx* glistp;
extern u32 D_800C0E50;
extern WipeBox* D_80132370[];

void func_800500E0(Gfx** glistp, WipeBox* box) {
  Gfx* gfx = *glistp;
  s32 half;
  s32 step;
  s32 offset;
  u16 left;
  s16 right;

  if (box->state == 3) {
    return;
  }
  box->timer += 1;
  if (box->timer == 4) {
    box->timer = 0;
    if (box->state == 2) {
      box->state = 3;
      return;
    }
    if (box->state == 0) {
      box->state = 1;
    }
  }

  half = (box->x1 - box->x0) / 2;
  step = (s16)half / 4;
  offset = step * box->timer;
  if (box->state == 2) {
    left = box->x0 + offset;
    right = box->x1 - offset;
  } else if (box->state == 0) {
    left = box->x0 + half - offset;
    right = offset + (box->x0 + half);
  } else {
    left = box->x0;
    right = box->x1;
  }

  gDPSetPrimColor(gfx++, 0, 0, box->r, box->g, box->b, box->a);
  gSPTextureRectangle(gfx++, left << 2, box->y0 << 2, right << 2, box->y1 << 2,
                      G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
  *glistp = gfx;
}

void func_80050274(WipeBox* box) {
  D_80132370[D_800C0E50] = box;
  D_800C0E50 += 1;
}

void func_8005029C(void) {
  u32 i;

  gDPPipeSync(glistp++);
  gDPPipeSync(glistp++);
  gDPSetCycleType(glistp++, G_CYC_1CYCLE);
  gDPPipeSync(glistp++);
  gDPSetAlphaCompare(glistp++, G_AC_NONE);
  gDPPipeSync(glistp++);
  gDPSetRenderMode(glistp++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPSetCombineMode(glistp++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
  gDPPipeSync(glistp++);

  for (i = 0; i != D_800C0E50; i++) {
    func_800500E0(&glistp, D_80132370[i]);
  }
  D_800C0E50 = 0;
}
