#include "common.h"

#include "heap.h"

#define SCENERY_SPRITE_COUNT 46
#define SCENERY_HEAP_SLOT 1

/* Texture metadata is retained when the decoded buffers are discarded. */
typedef struct ScenerySpriteDescriptor {
  /* 0x00 */ u8 texture_metadata[0xC];
  /* 0x0C */ void* decoded_texture;
} ScenerySpriteDescriptor; /* 0x10 */

extern const char SCENERY_RESET_MESSAGE[];
extern ScenerySpriteDescriptor g_scenery_sprites[SCENERY_SPRITE_COUNT];
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
