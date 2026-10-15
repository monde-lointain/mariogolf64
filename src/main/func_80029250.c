#include "common.h"

enum { CFB_TRIPLE_BUFFER_COUNT = 3, CFB_NO_BUFFER_INDEX = 0xFF };

#define CFB_SEPARATE_AUXILIARY_BUFFER ((u16*)0x802EA000)

// NuSystem state shared with the graphics task manager.
extern u16** nuGfxCfb;
extern u16* nuGfxCfb_ptr;
extern u32 nuGfxCfbNum;
extern u32 nuGfxCfbCounter;

// Shared with the renderer and task manager. The successor table links the
// active buffers into a ring; the reserved index is CFB_NO_BUFFER_INDEX when
// all three buffers are active.
extern u16* g_auxiliary_framebuffer;
extern u8 g_next_framebuffer_indices[];
extern u8 g_reserved_framebuffer_index;

// Initialize the drawing ring at buffer zero. framebuffers must remain valid
// with three entries, even for double buffering; buffer_count must be 2 or 3.
void initialize_color_framebuffers(u16** framebuffers, u32 buffer_count) {
  nuGfxCfb = framebuffers;
  nuGfxCfbNum = buffer_count;
  nuGfxCfbCounter = 0;
  nuGfxCfb_ptr = framebuffers[0];

  if (buffer_count == CFB_TRIPLE_BUFFER_COUNT) {
    // All buffers rotate; rendering uses a separate auxiliary surface.
    g_auxiliary_framebuffer = CFB_SEPARATE_AUXILIARY_BUFFER;
    g_next_framebuffer_indices[0] = 1;
    g_next_framebuffer_indices[1] = 2;
    g_next_framebuffer_indices[2] = 0;
    g_reserved_framebuffer_index = CFB_NO_BUFFER_INDEX;
  } else {
    // Capture the reserved surface before changing shared rotation state.
    u16* reserved_framebuffer = framebuffers[2];

    g_next_framebuffer_indices[0] = 1;
    g_next_framebuffer_indices[2] = CFB_NO_BUFFER_INDEX;
    g_next_framebuffer_indices[1] = 0;
    g_reserved_framebuffer_index = 2;
    g_auxiliary_framebuffer = reserved_framebuffer;
  }
}

// Switch an initialized ring between two and three buffers; buffer_count must
// be 2 or 3. An unchanged count preserves the drawing and auxiliary buffers.
void set_color_framebuffer_count(u32 buffer_count) {
  if (nuGfxCfbNum == buffer_count) {
    return;
  }

  if (buffer_count == CFB_TRIPLE_BUFFER_COUNT) {
    u8 next_buffer_index;

    // Reinsert the reserved buffer before the current buffer, then select it.
    nuGfxCfbNum = buffer_count;
    g_next_framebuffer_indices[g_reserved_framebuffer_index] = nuGfxCfbCounter;
    next_buffer_index = g_next_framebuffer_indices[nuGfxCfbCounter];
    g_next_framebuffer_indices[next_buffer_index] =
        g_reserved_framebuffer_index;
    nuGfxCfb_ptr = nuGfxCfb[g_reserved_framebuffer_index];
    g_auxiliary_framebuffer = CFB_SEPARATE_AUXILIARY_BUFFER;
    nuGfxCfbCounter = g_reserved_framebuffer_index;
    g_reserved_framebuffer_index = CFB_NO_BUFFER_INDEX;
  } else {
    u8 next_buffer_index;

    // Reserve the current buffer as the auxiliary surface and advance drawing.
    nuGfxCfbNum = buffer_count;
    g_reserved_framebuffer_index = nuGfxCfbCounter;
    nuGfxCfbCounter = g_next_framebuffer_indices[nuGfxCfbCounter];
    next_buffer_index = g_next_framebuffer_indices[nuGfxCfbCounter];
    g_next_framebuffer_indices[next_buffer_index] = nuGfxCfbCounter;
    nuGfxCfb_ptr = nuGfxCfb[nuGfxCfbCounter];
    g_auxiliary_framebuffer = nuGfxCfb[g_reserved_framebuffer_index];
  }
}
