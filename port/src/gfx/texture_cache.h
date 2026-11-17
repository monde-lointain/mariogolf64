/*
 * The textures that the GPU holds, in the layers of one atlas, found by the
 * key of the texels that they decode to. A texture that the frame uses is
 * decoded and added to the frame's uploads the first time; after that its
 * place is found again in the cache.
 *
 * Each layer is filled shelf by shelf. When no layer has room, the one used
 * least recently is emptied, unless the frame being recorded uses it, in
 * which case the frame must be flushed first.
 */

#ifndef PORT_GFX_TEXTURE_CACHE_H
#define PORT_GFX_TEXTURE_CACHE_H

#include <stdbool.h>
#include <stdint.h>

#define ATLAS_SIDE 2048
#define ATLAS_LAYER_COUNT 4
// The most textures that the cache holds at once. Each layer holds at most
// its share, and is full when it does, however much room it has left.
#define TEXTURE_CACHE_ENTRY_MAX 32768

typedef struct AtlasPlace {
    uint16_t x;
    uint16_t y;
    uint16_t layer;
} AtlasPlace;

typedef enum TextureLookup {
    TEXTURE_FOUND, // Where the texture already is
    TEXTURE_PLACED, // Where it is to be uploaded
    TEXTURE_NEEDS_FLUSH // No room until the frame is flushed
} TextureLookup;

void InitTextureCache(void);

// Starts counting a new frame's use of the layers
void BeginTextureCacheFrame(void);

TextureLookup FindTexture(uint64_t key, uint16_t width, uint16_t height, AtlasPlace *place);

#endif
