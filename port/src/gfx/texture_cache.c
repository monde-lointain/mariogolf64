/*
 * The atlas of the GPU's textures and the table that finds them.
 */

#include "texture_cache.h"

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_stdinc.h>

// Twice the textures that the cache holds, so that the table is at most
// half full and its probes stay short
#define TABLE_SIZE (TEXTURE_CACHE_ENTRY_MAX * 2)
#define TABLE_MASK (TABLE_SIZE - 1)
#define LAYER_ENTRY_MAX (TEXTURE_CACHE_ENTRY_MAX / ATLAS_LAYER_COUNT)
#define SHELF_COUNT_MAX 256
// A shelf takes textures down to half its height before opening another
#define SHELF_FIT_DIVISOR 2

typedef struct TableEntry {
    uint64_t key; // 0 for an empty entry
    AtlasPlace place;
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
    uint32_t entryCount; // The textures placed in it
    uint32_t lastUsedFrame;
} AtlasLayer;

static TableEntry f_Table[TABLE_SIZE];
// The entries that a rebuild of the table keeps
static TableEntry f_KeptEntries[TEXTURE_CACHE_ENTRY_MAX];
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

// The key as the table stores it: zero marks an empty entry, so a texture
// whose key is zero moves
static uint64_t StoreKey(uint64_t key) {
    return key != 0 ? key : 1;
}

// The entry that holds the stored key, or the empty one where it would go.
// The table is never more than half full, so there is always one.
static TableEntry *ProbeTable(uint64_t storedKey) {
    uint32_t index = (uint32_t)storedKey & TABLE_MASK;

    while (f_Table[index].key != storedKey && f_Table[index].key != 0) {
        index = (index + 1) & TABLE_MASK;
    }

    return &f_Table[index];
}

static void InsertEntry(uint64_t storedKey, const AtlasPlace *place) {
    TableEntry *entry = ProbeTable(storedKey);

    SDL_assert(entry->key == 0);
    entry->key = storedKey;
    entry->place = *place;
}

static bool PlaceInLayer(AtlasLayer *layer, uint16_t width, uint16_t height, AtlasPlace *place) {
    Shelf *shelf;
    int i;

    if (layer->entryCount == LAYER_ENTRY_MAX) {
        return false;
    }

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

// Empties the layer, and rebuilds the table without its textures. Taking
// their entries out one by one would leave gaps in other keys' probes.
static void EmptyLayer(AtlasLayer *layer) {
    uint16_t layerIndex = (uint16_t)(layer - f_Layers);
    uint32_t keptCount = 0;
    uint32_t i;

    for (i = 0; i < TABLE_SIZE; i++) {
        if (f_Table[i].key != 0 && f_Table[i].place.layer != layerIndex) {
            f_KeptEntries[keptCount++] = f_Table[i];
        }
    }

    SDL_zeroa(f_Table);
    for (i = 0; i < keptCount; i++) {
        InsertEntry(f_KeptEntries[i].key, &f_KeptEntries[i].place);
    }

    layer->shelfCount = 0;
    layer->usedHeight = 0;
    layer->entryCount = 0;
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
    uint64_t storedKey = StoreKey(key);
    const TableEntry *entry = ProbeTable(storedKey);

    SDL_assert(width != 0 && height != 0 && width <= ATLAS_SIDE && height <= ATLAS_SIDE);
    if (entry->key == storedKey) {
        *place = entry->place;
        f_Layers[place->layer].lastUsedFrame = f_FrameNumber;
        return TEXTURE_FOUND;
    }

    if (!PlaceTexture(width, height, place)) {
        return TEXTURE_NEEDS_FLUSH;
    }

    // Emptying a layer rebuilds the table, so the key is probed for again
    InsertEntry(storedKey, place);
    f_Layers[place->layer].entryCount++;
    f_Layers[place->layer].lastUsedFrame = f_FrameNumber;
    return TEXTURE_PLACED;
}
