#include "common.h"

#include "heap.h"

#include "terrain.h"

extern const char SCENERY_RESET_MESSAGE[];
extern s8 g_scenery_heap_ready;

/* Discard cached textures and empty their heap. A caller must register a heap
 * region again before scenery rendering can resume. */
void reset_scenery_billboards(void) {
  u32 sprite_index;

  osSyncPrintf(SCENERY_RESET_MESSAGE);

  for (sprite_index = 0; sprite_index < SCENERY_SPRITE_COUNT; sprite_index++) {
    g_scenery_sprites[sprite_index].decoded_texture = NULL;
  }

  heap_init(SCENERY_HEAP_SLOT);
  g_scenery_heap_ready = 0;
}
