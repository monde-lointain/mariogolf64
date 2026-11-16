/*
 * The atlas of the GPU's textures and the table that finds them.
 */

#include "texture_cache.h"

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_stdinc.h>

// Enough entries that the table stays sparse with every layer full of the
// smallest textures that the game uses
#define TABLE_SIZE (1 << 16)
#define TABLE_MASK (TABLE_SIZE - 1)
#define SHELF_COUNT_MAX 256
// A shelf takes textures down to half its height before opening another
#define SHELF_FIT_DIVISOR 2

typedef struct TableEntry {
    uint64_t key; // 0 for an entry never used
    AtlasPlace place;
    uint32_t generation; // Of its layer when it was placed
} TableEntry;

typedef struct Shelf {
    uint16_t top;
    uint16_t height;
    uint16_t used; // Width filled from the left
} Shelf;

typedef struct AtlasLayer {
    Shelf shelves[SHELF_COUNT_MAX];
    int shelfCount;
    uint16_t usedHeight;
    uint32_t generation; // Counts the times that the layer was emptied
    uint32_t lastUsedFrame;
} AtlasLayer;

static TableEntry f_Table[TABLE_SIZE];
static AtlasLayer f_Layers[ATLAS_LAYER_COUNT];
static uint32_t f_FrameNumber;

void InitTextureCache(void) {
    SDL_zeroa(f_Table);
    SDL_zeroa(f_Layers);
    f_FrameNumber = 1;
}

void BeginTextureCacheFrame(void) {
    f_FrameNumber++;
}

static bool IsCurrent(const TableEntry *entry) {
    return entry->key != 0 && entry->generation == f_Layers[entry->place.layer].generation;
}

static TableEntry *ProbeTable(uint64_t key) {
    uint32_t index = (uint32_t)key & TABLE_MASK;
    TableEntry *reusable = NULL;
    uint32_t step;

    // A zero key marks an empty entry, so a texture whose key is zero moves
    key = key != 0 ? key : 1;
    for (step = 0; step < TABLE_SIZE; step++) {
        TableEntry *entry = &f_Table[(index + step) & TABLE_MASK];

        if (entry->key == key) {
            return entry;
        }

        if (entry->key == 0) {
            return reusable != NULL ? reusable : entry;
        }

        if (reusable == NULL && !IsCurrent(entry)) {
            reusable = entry;
        }
    }

    SDL_assert_always(reusable != NULL);
    return reusable;
}

static bool PlaceInLayer(AtlasLayer *layer, uint16_t width, uint16_t height, AtlasPlace *place) {
    Shelf *shelf;
    int i;

    for (i = 0; i < layer->shelfCount; i++) {
        shelf = &layer->shelves[i];
        if (height <= shelf->height &&
            height >= shelf->height / SHELF_FIT_DIVISOR &&
            shelf->used + width <= ATLAS_SIDE) {
            place->x = shelf->used;
            place->y = shelf->top;
            shelf->used += width;
            return true;
        }
    }

    if (layer->shelfCount == SHELF_COUNT_MAX || layer->usedHeight + height > ATLAS_SIDE) {
        return false;
    }

    shelf = &layer->shelves[layer->shelfCount++];
    shelf->top = layer->usedHeight;
    shelf->height = height;
    shelf->used = width;
    layer->usedHeight += height;
    place->x = 0;
    place->y = shelf->top;
    return true;
}

static void EmptyLayer(AtlasLayer *layer) {
    layer->shelfCount = 0;
    layer->usedHeight = 0;
    layer->generation++;
}

static AtlasLayer *LeastRecentlyUsedLayer(void) {
    AtlasLayer *oldest = &f_Layers[0];
    int i;

    for (i = 1; i < ATLAS_LAYER_COUNT; i++) {
        if (f_Layers[i].lastUsedFrame < oldest->lastUsedFrame) {
            oldest = &f_Layers[i];
        }
    }

    return oldest;
}

static bool PlaceTexture(uint16_t width, uint16_t height, AtlasPlace *place) {
    AtlasLayer *layer;
    int i;

    for (i = 0; i < ATLAS_LAYER_COUNT; i++) {
        if (PlaceInLayer(&f_Layers[i], width, height, place)) {
            place->layer = (uint16_t)i;
            return true;
        }
    }

    layer = LeastRecentlyUsedLayer();
    if (layer->lastUsedFrame == f_FrameNumber) {
        return false;
    }

    EmptyLayer(layer);
    place->layer = (uint16_t)(layer - f_Layers);
    return PlaceInLayer(layer, width, height, place);
}

TextureLookup FindTexture(uint64_t key, uint16_t width, uint16_t height, AtlasPlace *place) {
    TableEntry *entry = ProbeTable(key);

    SDL_assert(width != 0 && height != 0 && width <= ATLAS_SIDE && height <= ATLAS_SIDE);
    if (entry->key == (key != 0 ? key : 1) && IsCurrent(entry)) {
        *place = entry->place;
        f_Layers[place->layer].lastUsedFrame = f_FrameNumber;
        return TEXTURE_FOUND;
    }

    if (!PlaceTexture(width, height, place)) {
        return TEXTURE_NEEDS_FLUSH;
    }

    // Emptying a layer can have made the entry probed above stale, which is
    // as good a place for the key as any
    entry->key = key != 0 ? key : 1;
    entry->place = *place;
    entry->generation = f_Layers[place->layer].generation;
    f_Layers[place->layer].lastUsedFrame = f_FrameNumber;
    return TEXTURE_PLACED;
}
