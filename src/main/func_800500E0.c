#include "common.h"

/* Translucent rectangles that open from the center and close toward it.
 * Queue caller-owned boxes before draw_queued_wipe_boxes each frame; drawing
 * advances their animation and consumes the queue, not the boxes themselves. */
typedef enum {
  WIPE_BOX_OPENING = 0,
  WIPE_BOX_OPEN = 1,
  WIPE_BOX_CLOSING = 2,
  WIPE_BOX_CLOSED = 3
} WipeBoxState;

enum {
  WIPE_BOX_TRANSITION_FRAMES = 4,
  WIPE_BOX_QUEUE_CAPACITY = 10,
  WIPE_BOX_SCREEN_FRAC_BITS = 2,
  WIPE_BOX_TEXTURE_STEP_ONE = 1 << 10
};

typedef struct {
  /* Coordinates are in screen pixels; color channels range from 0 to 255. */
  /* 0x00 */ s16 left;
  /* 0x02 */ s16 top;
  /* 0x04 */ s16 right;
  /* 0x06 */ s16 bottom;
  /* 0x08 */ u8 red;
  /* 0x09 */ u8 green;
  /* 0x0A */ u8 blue;
  /* 0x0B */ u8 alpha;
  /* 0x0C */ WipeBoxState state;
  /* Cycles through 0..3 while visible, including when fully open. */
  /* 0x10 */ s32 transition_frame;
} WipeBox; /* 0x14 */

extern Gfx* glistp;
/* Shared by queue_wipe_box and draw_queued_wipe_boxes; boxes remain
 * caller-owned. */
extern u32 queued_wipe_box_count;
extern WipeBox* queued_wipe_boxes[WIPE_BOX_QUEUE_CAPACITY];

/* Advance a box with a frame in 0..3 and append its visible rectangle to the
 * valid display_list cursor. Closed boxes neither advance nor append commands.
 */
void advance_and_draw_wipe_box(Gfx** display_list, WipeBox* box) {
  /* The entry-time cursor load is required for the retail instruction order. */
  Gfx* gfx = *display_list;
  s32 half_width;
  s32 pixels_per_frame;
  s32 edge_offset;
  u16 draw_left;
  s16 draw_right;

  if (box->state == WIPE_BOX_CLOSED) {
    return;
  }
  box->transition_frame += 1;
  if (box->transition_frame == WIPE_BOX_TRANSITION_FRAMES) {
    box->transition_frame = 0;
    if (box->state == WIPE_BOX_CLOSING) {
      box->state = WIPE_BOX_CLOSED;
      return;
    }
    if (box->state == WIPE_BOX_OPENING) {
      box->state = WIPE_BOX_OPEN;
    }
  }

  /* Preserve the retail edge widths and signed half-width truncation. */
  half_width = (box->right - box->left) / 2;
  pixels_per_frame = (s16)half_width / WIPE_BOX_TRANSITION_FRAMES;
  edge_offset = pixels_per_frame * box->transition_frame;
  if (box->state == WIPE_BOX_CLOSING) {
    draw_left = box->left + edge_offset;
    draw_right = box->right - edge_offset;
  } else if (box->state == WIPE_BOX_OPENING) {
    draw_left = box->left + half_width - edge_offset;
    draw_right = edge_offset + (box->left + half_width);
  } else {
    draw_left = box->left;
    draw_right = box->right;
  }

  gDPSetPrimColor(gfx++, 0, 0, box->red, box->green, box->blue, box->alpha);
  gSPTextureRectangle(gfx++, draw_left << WIPE_BOX_SCREEN_FRAC_BITS,
                      box->top << WIPE_BOX_SCREEN_FRAC_BITS,
                      draw_right << WIPE_BOX_SCREEN_FRAC_BITS,
                      box->bottom << WIPE_BOX_SCREEN_FRAC_BITS, G_TX_RENDERTILE,
                      0, 0, WIPE_BOX_TEXTURE_STEP_ONE,
                      WIPE_BOX_TEXTURE_STEP_ONE);
  *display_list = gfx;
}

/* Queue a non-null box that remains valid through drawing. The caller must
 * leave room in the ten-entry queue and requeue the box for subsequent frames.
 */
void queue_wipe_box(WipeBox* box) {
  queued_wipe_boxes[queued_wipe_box_count] = box;
  queued_wipe_box_count += 1;
}

/* Append the shared primitive-color setup, advance/draw queued boxes in order,
 * and empty the queue. Keep the retail synchronization commands in order. */
void draw_queued_wipe_boxes(void) {
  u32 box_index;

  gDPPipeSync(glistp++);
  gDPPipeSync(glistp++);
  gDPSetCycleType(glistp++, G_CYC_1CYCLE);
  gDPPipeSync(glistp++);
  gDPSetAlphaCompare(glistp++, G_AC_NONE);
  gDPPipeSync(glistp++);
  gDPSetRenderMode(glistp++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPSetCombineMode(glistp++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
  gDPPipeSync(glistp++);

  for (box_index = 0; box_index != queued_wipe_box_count; box_index++) {
    advance_and_draw_wipe_box(&glistp, queued_wipe_boxes[box_index]);
  }
  queued_wipe_box_count = 0;
}
