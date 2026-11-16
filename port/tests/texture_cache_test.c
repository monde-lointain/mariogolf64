/*
 * Tests of the texture atlas's cache: finding, placing and evicting.
 */

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

int main(void) {
    TestFindAndPlace();
    TestZeroKey();
    TestEviction();
    return ReportTestResults("texture_cache_test");
}
