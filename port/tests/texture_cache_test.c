/*
 * Tests of the texture atlas's cache: finding, placing and evicting.
 */

#include <stdbool.h>

#include "test_support.h"
#include "texture_cache.h"

// A texture found again keeps its place; a new one goes elsewhere
static void TestFindAndPlace(void) {
    AtlasPlace first;
    AtlasPlace again;
    AtlasPlace second;

    InitTextureCache();
    BeginTextureCacheFrame();
    EXPECT(FindTexture(100, 32, 32, &first) == TEXTURE_PLACED);
    EXPECT(FindTexture(100, 32, 32, &again) == TEXTURE_FOUND);
    EXPECT(again.x == first.x && again.y == first.y && again.layer == first.layer);
    EXPECT(FindTexture(101, 32, 32, &second) == TEXTURE_PLACED);
    EXPECT(second.x != first.x || second.y != first.y || second.layer != first.layer);
}

// Key 0 is a texture like any other
static void TestZeroKey(void) {
    AtlasPlace place;

    InitTextureCache();
    BeginTextureCacheFrame();
    EXPECT(FindTexture(0, 16, 16, &place) == TEXTURE_PLACED);
    EXPECT(FindTexture(0, 16, 16, &place) == TEXTURE_FOUND);
}

// Filling every layer in one frame asks for a flush; in a later frame the
// least recently used layer is emptied instead, and its textures are lost
static void TestEviction(void) {
    AtlasPlace place;
    uint64_t key;
    TextureLookup lookup = TEXTURE_PLACED;

    InitTextureCache();
    BeginTextureCacheFrame();
    // Each layer holds four textures of a quarter of its side
    for (key = 1; key <= 4 * ATLAS_LAYER_COUNT; key++) {
        lookup = FindTexture(key, ATLAS_SIDE / 2, ATLAS_SIDE / 2, &place);
        EXPECT(lookup == TEXTURE_PLACED);
    }

    EXPECT(FindTexture(1000, ATLAS_SIDE / 2, ATLAS_SIDE / 2, &place) == TEXTURE_NEEDS_FLUSH);

    BeginTextureCacheFrame();
    EXPECT(FindTexture(1000, ATLAS_SIDE / 2, ATLAS_SIDE / 2, &place) == TEXTURE_PLACED);
    // The emptied layer was the first filled, so its first texture is gone
    EXPECT(place.layer == 0);
    EXPECT(FindTexture(1, ATLAS_SIDE / 2, ATLAS_SIDE / 2, &place) != TEXTURE_FOUND);
}

#define LAYER_SHARE (TEXTURE_CACHE_ENTRY_MAX / ATLAS_LAYER_COUNT)

// A layer is full once it holds its share of the textures, however small
// they are. With every layer full, emptying the least recently used one
// loses only its textures.
static void TestLayerHoldsItsShare(void) {
    AtlasPlace place;
    uint64_t key;
    bool isEachPlaced = true;

    InitTextureCache();
    BeginTextureCacheFrame();
    for (key = 1; key <= LAYER_SHARE; key++) {
        isEachPlaced =
            isEachPlaced && FindTexture(key, 1, 1, &place) == TEXTURE_PLACED && place.layer == 0;
    }

    EXPECT(isEachPlaced);
    EXPECT(FindTexture(key, 1, 1, &place) == TEXTURE_PLACED && place.layer == 1);
    for (key++; key <= TEXTURE_CACHE_ENTRY_MAX; key++) {
        isEachPlaced = isEachPlaced && FindTexture(key, 1, 1, &place) == TEXTURE_PLACED;
    }

    EXPECT(isEachPlaced);
    EXPECT(FindTexture(key, 1, 1, &place) == TEXTURE_NEEDS_FLUSH);

    BeginTextureCacheFrame();
    EXPECT(FindTexture(key, 1, 1, &place) == TEXTURE_PLACED && place.layer == 0);
    EXPECT(FindTexture(1, 1, 1, &place) == TEXTURE_PLACED);
    EXPECT(FindTexture(LAYER_SHARE + 1, 1, 1, &place) == TEXTURE_FOUND && place.layer == 1);
    EXPECT(FindTexture(TEXTURE_CACHE_ENTRY_MAX, 1, 1, &place) == TEXTURE_FOUND);
}

// New textures every frame, as when the game draws into a texture each
// frame, cycle through the layers without filling the table: each frame's
// are placed and then found, however many came before
static void TestTexturesCycleThroughLayers(void) {
    enum {
        FRAME_COUNT = 200,
        FRAME_TEXTURE_COUNT = 1000
    };

    AtlasPlace place;
    uint64_t key = 1;
    bool isEachPlaced = true;
    bool isEachFound = true;
    int frame;
    int i;

    InitTextureCache();
    for (frame = 0; frame < FRAME_COUNT; frame++) {
        uint64_t frameFirstKey = key;

        BeginTextureCacheFrame();
        for (i = 0; i < FRAME_TEXTURE_COUNT; i++, key++) {
            isEachPlaced = isEachPlaced && FindTexture(key, 8, 1, &place) == TEXTURE_PLACED;
        }

        for (i = 0; i < FRAME_TEXTURE_COUNT; i++) {
            isEachFound = isEachFound &&
                FindTexture(frameFirstKey + (uint64_t)i, 8, 1, &place) == TEXTURE_FOUND;
        }
    }

    EXPECT(isEachPlaced);
    EXPECT(isEachFound);
}

int main(void) {
    TestFindAndPlace();
    TestZeroKey();
    TestEviction();
    TestLayerHoldsItsShare();
    TestTexturesCycleThroughLayers();
    return ReportTestResults("texture_cache_test");
}
