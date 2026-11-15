/*
 * Loads a hole for the tests as the loader thread does
 */

#include "hole_fixture.h"

#include "course/hole_conditions.h"
#include "course/hole_loader.h"
#include "course/hole_setup.h"
#include "course/terrain.h"
#include "game/scene.h"
#include "round/game_state.h"
#include "system/asset.h"
#include "system/heap.h"
#include "system/overlay_manager.h"

#define FNV_PRIME 0x01000193

Boolean LoadTestHole(CourseIndex courseIndex, s32 holeIndex) {
    s32 overlayId;

    InitHeapRegions(FALSE);
    InitAssetFiles();
    InitOverlayManager();
    g_GameState.gameMode = GAME_MODE_STROKE;
    g_GameState.courseIndex = courseIndex;
    g_GameState.holeIndex = holeIndex;
    g_GameState.isMiniGolf = courseIndex >= MINI_GOLF_COURSE_INDEX;
    g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_START;

    // The overlay manager keeps what is loaded across InitOverlayManager, so
    // a test that leaves an overlay sharing the builder's memory loaded
    // makes this fail rather than run the code that was there before
    overlayId = GetOverlayOfModule(MODULE_HOLE_BUILDER);
    if (!LoadOverlay(overlayId)) {
        return FALSE;
    }
    LoadHole();
    PrepareHoleScene();
    UnloadOverlay(overlayId);

    g_HoleSetup.cupPosition[AXIS_X] = g_HolePoints.cupPositions[DEFAULT_CUP][AXIS_X];
    g_HoleSetup.cupPosition[AXIS_Z] = g_HolePoints.cupPositions[DEFAULT_CUP][AXIS_Z];
    g_HoleSetup.cupPosition[AXIS_Y] =
        GetGroundHeight(g_HoleSetup.cupPosition[AXIS_X], g_HoleSetup.cupPosition[AXIS_Z]);
    g_HoleSetup.teePosition[AXIS_X] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_X];
    g_HoleSetup.teePosition[AXIS_Y] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_Y];
    g_HoleSetup.teePosition[AXIS_Z] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_Z];
    return TRUE;
}

u32 HashBytes(u32 hash, const void *data, u32 size) {
    const u8 *bytes = data;
    u32 i;

    for (i = 0; i < size; i++) {
        hash = (hash ^ bytes[i]) * FNV_PRIME;
    }
    return hash;
}

// What the hashes sample at each point
typedef s32 (*GroundSampler)(s32 x, s32 z);

static u32 HashGroundSamples(GroundSampler sample) {
    s32 width = g_GameState.isMiniGolf ? MINI_GOLF_GROUND_SIZE : GROUND_WIDTH;
    s32 length = g_GameState.isMiniGolf ? MINI_GOLF_GROUND_SIZE : GROUND_LENGTH;
    u32 hash = FNV_OFFSET_BASIS;
    s32 column, row, value;

    for (row = 0; row < GROUND_POINT_ROW_COUNT; row++) {
        for (column = 0; column < GROUND_POINT_COLUMN_COUNT; column++) {
            value = sample(
                column * (width / (GROUND_POINT_COLUMN_COUNT - 1)),
                row * (length / (GROUND_POINT_ROW_COUNT - 1))
            );
            hash = HashBytes(hash, &value, sizeof(value));
        }
    }
    return hash;
}

u32 HashGroundHeights(void) {
    return HashGroundSamples(GetGroundHeight);
}

u32 HashGroundSurfaces(void) {
    return HashGroundSamples(GetSurface);
}
