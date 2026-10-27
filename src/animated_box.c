#include "common.h"

#include "animated_box.h"

#include "variables.h"

/* Frames that opening and closing each take */
#define BOX_ANIMATION_FRAME_COUNT 4
#define QUEUED_BOX_CAPACITY 10
/* Texture rectangle steps in s5.10: the boxes are flat colour, so any step will
 * do */
#define ONE_TEXEL_PER_PIXEL (1 << 10)

extern s32 f_QueuedBoxCount;
extern AnimatedBox* f_QueuedBoxes[QUEUED_BOX_CAPACITY];

static void DrawAnimatedBox(Gfx** gfxHead, AnimatedBox* box) {
  Gfx* gfx;
  s32 halfWidth;
  s16 step;
  s16 left;
  s16 right;
  s16 middle;
  s16 boxLeft;
  s16 boxRight;

  gfx = *gfxHead;
  if (box->state == BOX_CLOSED) {
    return;
  }
  box->frameIndex++;
  if (box->frameIndex == BOX_ANIMATION_FRAME_COUNT) {
    box->frameIndex = 0;
    if (box->state == BOX_CLOSING) {
      box->state = BOX_CLOSED;
      return;
    }
    if (box->state == BOX_OPENING) {
      box->state = BOX_OPEN;
    }
  }
  boxRight = box->right;
  boxLeft = box->left;
  halfWidth = (boxRight - boxLeft) / 2;
  step = ((s16)halfWidth / BOX_ANIMATION_FRAME_COUNT) * box->frameIndex;
  if (box->state == BOX_CLOSING) {
    left = boxLeft + step;
    right = boxRight - step;
  } else if (box->state == BOX_OPENING) {
    middle = boxLeft + halfWidth;
    left = middle - step;
    right = step + middle;
  } else {
    left = boxLeft;
    right = boxRight;
  }
  gDPSetPrimColor(gfx++, 0, 0, box->red, box->green, box->blue, box->alpha);
  gSPTextureRectangle(gfx++, left << 2, box->top << 2, right << 2,
      box->bottom << 2, G_TX_RENDERTILE, 0, 0, ONE_TEXEL_PER_PIXEL,
      ONE_TEXEL_PER_PIXEL);
  *gfxHead = gfx;
}

/* DrawAnimatedBoxes empties the queue, so a box is queued again for each draw;
 * nothing checks the capacity */
void QueueAnimatedBox(AnimatedBox* box) {
  f_QueuedBoxes[f_QueuedBoxCount] = box;
  f_QueuedBoxCount++;
}

void DrawAnimatedBoxes(void) {
  s32 i;

  gDPPipeSync(g_DisplayListHead++);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetAlphaCompare(g_DisplayListHead++, G_AC_NONE);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetRenderMode(g_DisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPSetCombineMode(g_DisplayListHead++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
  gDPPipeSync(g_DisplayListHead++);
  for (i = 0; i != f_QueuedBoxCount; i++) {
    DrawAnimatedBox(&g_DisplayListHead, f_QueuedBoxes[i]);
  }
  f_QueuedBoxCount = 0;
}
