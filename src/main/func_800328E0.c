#include "common.h"

/* Per-frame viewport setup and framebuffer clears. The append_* builders
 * advance a caller-owned display-list cursor; the other builders use glistp.
 * Keep the retail command order, including repeated pipeline syncs. */

enum {
  FRAMEBUFFER_WIDTH = 320,
  FRAMEBUFFER_HEIGHT = 240,
  FRAMEBUFFER_ADDRESS_ALIGNMENT = 8,
  PLAYFIELD_INSET = 8,
  COLOR_CLEAR_SCANLINES = 1,
  COLOR_CLEAR_RECTANGLE = 2
};

#define OPAQUE_BLACK_FILL_COLOR \
  ((GPACK_RGBA5551(0, 0, 0, 1) << 16) | GPACK_RGBA5551(0, 0, 0, 1))
#define DEPTH_CLEAR_FILL_COLOR \
  (((u32)GPACK_ZDZ(G_MAXFBZ, 0) << 16) | GPACK_ZDZ(G_MAXFBZ, 0))

typedef struct {
  s32 red;
  s32 green;
  s32 blue;
} Color;

/* Shared command cursor used by the frame renderer. */
extern Gfx* glistp;

extern Vp frame_viewports[];
extern Gfx rdp_setup_dl[];
extern Gfx rsp_setup_dl[];
/* Shared RGB state set by set_clear_color and consumed by clear_playfield. */
extern Color clear_color;

extern u16* nuGfxZBuffer;
extern u16* nuGfxCfb_ptr;

extern void set_camera_matrices_fixed(s32, s32, Vp*);

/* viewport_index selects an existing frame_viewports entry. The caller must
 * provide writable command space through display_list_cursor. */
void append_viewport_setup(s32 viewport_index, Gfx** display_list_cursor) {
  Gfx* next_command = *display_list_cursor;

  gDPPipeSync(next_command++);
  gSPSegment(next_command++, 0, 0);
  gSPViewport(next_command++, &frame_viewports[viewport_index]);
  set_camera_matrices_fixed(0, 0, &frame_viewports[viewport_index]);
  gSPDisplayList(next_command++, rsp_setup_dl);
  gSPDisplayList(next_command++, rdp_setup_dl);
  gDPPipeSync(next_command++);
  *display_list_cursor = next_command;
}

void setup_frame_viewport(s32 viewport_index) {
  append_viewport_setup(viewport_index, &glistp);
}

void set_clear_color(s32 red, s32 green, s32 blue) {
  clear_color.red = red;
  clear_color.green = green;
  clear_color.blue = blue;
}

/* Keep the RGB channels in a struct and pack them in the command argument.
 * Their aliasing with command stores preserves the retail color-load schedule;
 * caching the packed color or the global cursor changes generated code. */
void clear_playfield(void) {
  gDPPipeSync(glistp++);
  gDPSetRenderMode(glistp++, G_RM_NOOP, G_RM_NOOP2);
  gDPPipeSync(glistp++);
  gDPPipeSync(glistp++);
  gDPSetCycleType(glistp++, G_CYC_FILL);
  gDPSetFillColor(
      glistp++,
      ((GPACK_RGBA5551(clear_color.red, clear_color.green, clear_color.blue, 1)
        << 16) |
       GPACK_RGBA5551(clear_color.red, clear_color.green, clear_color.blue,
                      1)));
  gDPPipeSync(glistp++);
  gDPFillRectangle(glistp++, PLAYFIELD_INSET, PLAYFIELD_INSET,
                   FRAMEBUFFER_WIDTH - PLAYFIELD_INSET - 1,
                   FRAMEBUFFER_HEIGHT - PLAYFIELD_INSET - 1);
  gDPPipeSync(glistp++);
  gDPPipeSync(glistp++);
  gDPPipeSync(glistp++);
  gDPSetCycleType(glistp++, G_CYC_1CYCLE);
  gDPPipeSync(glistp++);
}

/* Always clear depth; clear color to opaque black only for the named modes.
 * Other mode values leave color unchanged. Advance the caller-owned cursor,
 * which must have room for the setup, clear commands, and cycle restoration. */
void append_framebuffer_clear(s32 color_clear_mode, Gfx** display_list_cursor) {
  Gfx* next_command = *display_list_cursor;
  u32 depth_buffer_address =
      OS_K0_TO_PHYSICAL(nuGfxZBuffer) & ~(FRAMEBUFFER_ADDRESS_ALIGNMENT - 1);

  gDPPipeSync(next_command++);
  gDPSetScissor(next_command++, G_SC_NON_INTERLACE, 0, 0, FRAMEBUFFER_WIDTH,
                FRAMEBUFFER_HEIGHT);
  gDPSetDepthImage(next_command++, depth_buffer_address);
  gDPPipeSync(next_command++);
  gDPSetRenderMode(next_command++, G_RM_NOOP, G_RM_NOOP2);
  gDPPipeSync(next_command++);
  gDPPipeSync(next_command++);
  gDPSetCycleType(next_command++, G_CYC_FILL);
  gDPSetColorImage(next_command++, G_IM_FMT_RGBA, G_IM_SIZ_16b,
                   FRAMEBUFFER_WIDTH, depth_buffer_address);
  gDPSetFillColor(next_command++, DEPTH_CLEAR_FILL_COLOR);
  gDPFillRectangle(next_command++, 0, 0, FRAMEBUFFER_WIDTH - 1,
                   FRAMEBUFFER_HEIGHT - 1);
  gDPPipeSync(next_command++);
  gDPSetColorImage(
      next_command++, G_IM_FMT_RGBA, G_IM_SIZ_16b, FRAMEBUFFER_WIDTH,
      osVirtualToPhysical(nuGfxCfb_ptr) & ~(FRAMEBUFFER_ADDRESS_ALIGNMENT - 1));

  if (color_clear_mode == COLOR_CLEAR_SCANLINES) {
    s32 row;

    /* The retail scanline path leaves the final framebuffer row untouched. */
    for (row = 0; row < FRAMEBUFFER_HEIGHT - 1; row++) {
      gDPSetFillColor(next_command++, OPAQUE_BLACK_FILL_COLOR);
      gDPFillRectangle(next_command++, 0, row, FRAMEBUFFER_WIDTH - 1, row);
    }
    gDPPipeSync(next_command++);
  } else if (color_clear_mode == COLOR_CLEAR_RECTANGLE) {
    gDPSetFillColor(next_command++, OPAQUE_BLACK_FILL_COLOR);
    gDPPipeSync(next_command++);
    gDPFillRectangle(next_command++, 0, 0, FRAMEBUFFER_WIDTH - 1,
                     FRAMEBUFFER_HEIGHT - 1);
    gDPPipeSync(next_command++);
  }

  gDPPipeSync(next_command++);
  gDPPipeSync(next_command++);
  gDPSetCycleType(next_command++, G_CYC_1CYCLE);
  gDPPipeSync(next_command++);
  *display_list_cursor = next_command;
}

void clear_framebuffers(s32 color_clear_mode) {
  append_framebuffer_clear(color_clear_mode, &glistp);
}
