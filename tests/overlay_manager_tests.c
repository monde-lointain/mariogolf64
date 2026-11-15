/*
 * Characterizes overlay_manager.c, which loads the overlays that hold the
 * game's modules into memory that they share. Each overlay is loaded or
 * not, and the tests take each input in each state: loading and unloading
 * it, loading an overlay that overlaps it, and reinitializing the manager.
 * The overlay ID's range has edges, which the tests take both sides of.
 * An overlay can't load while one that overlaps it is loaded, or while the
 * RDP may still read one of that overlay's modules' display lists; the
 * tests take frames on both sides of that window. They use only overlays
 * without load hooks, which change nothing but their own memory.
 */

#include <ultra64.h>

#include "debug/game_flags.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"
#include "system/segment_symbols.h"
#include "unity_fixture.h"

// Indices in overlay_manager.c's table. The play modules are in the first
// three entries, and map to the last of them, which loads the scenery.
#define PLAY_OVERLAY_INDEX 2
#define HOLE_BUILDER_OVERLAY_INDEX 4
#define SHOT_SIMULATION_OVERLAY_INDEX 6
#define STAFF_ROLL_OVERLAY_INDEX 16

// As overlay_manager.c's RDP_REQUEST_FRAMES_IN_USE: a module's display
// lists may be read for this many frames after its request
#define RDP_REQUEST_FRAMES_IN_USE 2
// Any frame well past the first, so that the frames before it are too
#define TEST_FRAME 100

// The hole builder's and the shot simulation's overlays overlap; the
// scorecard's lies apart from both
static s32 f_HoleBuilderId;
static s32 f_ShotSimulationId;
static s32 f_ScorecardId;

TEST_GROUP(OverlayManager);

TEST_SETUP(OverlayManager) {
    ClearAllGameFlags();
    SetGameFlag(GAME_FLAG_QUIET_OVERLAY_LOG);
    InitOverlayManager();
    f_HoleBuilderId = GetOverlayOfModule(MODULE_HOLE_BUILDER);
    f_ShotSimulationId = GetOverlayOfModule(MODULE_SHOT_SIMULATION);
    f_ScorecardId = GetOverlayOfModule(MODULE_SCORECARD);
    g_FrameCount = TEST_FRAME;
}

// Overlays stay loaded across InitOverlayManager, so each test unloads what
// it may have loaded
TEST_TEAR_DOWN(OverlayManager) {
    UnloadOverlay(f_HoleBuilderId);
    UnloadOverlay(f_ShotSimulationId);
    UnloadOverlay(f_ScorecardId);
    g_FrameCount = 0;
    ClearAllGameFlags();
}

TEST(OverlayManager, ModulesInNoOverlayAreAlive) {
    TEST_ASSERT_TRUE(g_IsModuleAlive[0]);
    TEST_ASSERT_TRUE(g_IsModuleAlive[MODULE_UNUSED_TRANSMIT_B + 1]);
    TEST_ASSERT_TRUE(g_IsModuleAlive[MODULE_COUNT - 1]);
    TEST_ASSERT_FALSE(g_IsModuleAlive[MODULE_GOLF_BALLS]);
    TEST_ASSERT_FALSE(g_IsModuleAlive[MODULE_UNUSED_TRANSMIT_B]);
}

TEST(OverlayManager, ModulesMapToTheirOverlays) {
    TEST_ASSERT_EQUAL_HEX32(OVERLAY_ID_BASE + PLAY_OVERLAY_INDEX, GetOverlayOfModule(MODULE_PLAY));
    TEST_ASSERT_EQUAL_HEX32(
        OVERLAY_ID_BASE + HOLE_BUILDER_OVERLAY_INDEX,
        GetOverlayOfModule(MODULE_HOLE_BUILDER)
    );
    TEST_ASSERT_EQUAL_HEX32(
        OVERLAY_ID_BASE + HOLE_BUILDER_OVERLAY_INDEX,
        GetOverlayOfModule(MODULE_UNUSED_HOLE_BUILDER)
    );
    TEST_ASSERT_EQUAL_HEX32(
        OVERLAY_ID_BASE + SHOT_SIMULATION_OVERLAY_INDEX,
        GetOverlayOfModule(MODULE_SHOT_SIMULATION)
    );
    TEST_ASSERT_EQUAL_HEX32(
        OVERLAY_ID_BASE + STAFF_ROLL_OVERLAY_INDEX,
        GetOverlayOfModule(MODULE_STAFF_ROLL)
    );
}

// IDs outside the table give its first entry
TEST(OverlayManager, OverlayIdsOutsideTheTableGiveTheFirst) {
    TEST_ASSERT_EQUAL_INT(0, GetOverlayIndex(-1));
    TEST_ASSERT_EQUAL_INT(0, GetOverlayIndex(OVERLAY_ID_BASE - 1));
    TEST_ASSERT_EQUAL_INT(0, GetOverlayIndex(OVERLAY_ID_BASE));
    TEST_ASSERT_EQUAL_INT(1, GetOverlayIndex(OVERLAY_ID_BASE + 1));
    TEST_ASSERT_EQUAL_INT(OVERLAY_COUNT - 1, GetOverlayIndex(OVERLAY_ID_BASE + OVERLAY_COUNT - 1));
    TEST_ASSERT_EQUAL_INT(0, GetOverlayIndex(OVERLAY_ID_BASE + OVERLAY_COUNT));
    TEST_ASSERT_EQUAL_INT(0, GetOverlayIndex(2 * OVERLAY_ID_BASE + 1));
}

TEST(OverlayManager, LoadingMarksItsModulesAlive) {
    TEST_ASSERT_TRUE(LoadOverlay(f_HoleBuilderId));

    TEST_ASSERT_TRUE(g_IsModuleAlive[MODULE_HOLE_BUILDER]);
    TEST_ASSERT_TRUE(g_IsModuleAlive[MODULE_UNUSED_HOLE_BUILDER]);
    TEST_ASSERT_FALSE(g_IsModuleAlive[MODULE_SHOT_SIMULATION]);
}

TEST(OverlayManager, UnloadingMarksItsModulesDead) {
    TEST_ASSERT_TRUE(LoadOverlay(f_HoleBuilderId));
    UnloadOverlay(f_HoleBuilderId);

    TEST_ASSERT_FALSE(g_IsModuleAlive[MODULE_HOLE_BUILDER]);
    TEST_ASSERT_FALSE(g_IsModuleAlive[MODULE_UNUSED_HOLE_BUILDER]);
}

// Loading a loaded overlay reads it again, which clears its bss
TEST(OverlayManager, LoadingALoadedOverlayReloadsIt) {
    u8 *bss = ovl_02_BSS_START;

    TEST_ASSERT_TRUE_MESSAGE(
        BytesBetween(ovl_02_BSS_START, ovl_02_BSS_END) > 0,
        "The hole builder's overlay has no bss"
    );
    TEST_ASSERT_TRUE(LoadOverlay(f_HoleBuilderId));
    *bss = 0xA5;

    TEST_ASSERT_TRUE(LoadOverlay(f_HoleBuilderId));
    TEST_ASSERT_TRUE(g_IsModuleAlive[MODULE_HOLE_BUILDER]);
    TEST_ASSERT_EQUAL_HEX8(0, *bss);
}

TEST(OverlayManager, UnloadingAnUnloadedOverlayChangesNothing) {
    TEST_ASSERT_TRUE(LoadOverlay(f_ShotSimulationId));
    UnloadOverlay(f_HoleBuilderId);

    TEST_ASSERT_TRUE(g_IsModuleAlive[MODULE_SHOT_SIMULATION]);
    TEST_ASSERT_FALSE(g_IsModuleAlive[MODULE_HOLE_BUILDER]);
    TEST_ASSERT_FALSE(CanLoadOverlay(f_HoleBuilderId));
}

TEST(OverlayManager, OverlappingOverlayCannotLoad) {
    TEST_ASSERT_TRUE(LoadOverlay(f_HoleBuilderId));

    TEST_ASSERT_FALSE(LoadOverlay(f_ShotSimulationId));
    TEST_ASSERT_FALSE(g_IsModuleAlive[MODULE_SHOT_SIMULATION]);
    TEST_ASSERT_TRUE(g_IsModuleAlive[MODULE_HOLE_BUILDER]);
}

TEST(OverlayManager, UnloadingMakesRoomForAnOverlap) {
    TEST_ASSERT_TRUE(LoadOverlay(f_HoleBuilderId));
    UnloadOverlay(f_HoleBuilderId);

    TEST_ASSERT_TRUE(LoadOverlay(f_ShotSimulationId));
}

TEST(OverlayManager, OverlaysApartLoadTogether) {
    TEST_ASSERT_TRUE_MESSAGE(
        (u32)ovl_09_BSS_END < (u32)ovl_02_VRAM,
        "The scorecard's overlay now overlaps the hole builder's"
    );

    TEST_ASSERT_TRUE(LoadOverlay(f_HoleBuilderId));
    TEST_ASSERT_TRUE(LoadOverlay(f_ScorecardId));
}

// Reinitializing marks a loaded overlay's modules dead but leaves it
// loaded, so an overlap still can't load
TEST(OverlayManager, InitKeepsLoadedOverlays) {
    TEST_ASSERT_TRUE(LoadOverlay(f_HoleBuilderId));
    InitOverlayManager();

    TEST_ASSERT_FALSE(g_IsModuleAlive[MODULE_HOLE_BUILDER]);
    TEST_ASSERT_FALSE(LoadOverlay(f_ShotSimulationId));
}

TEST(OverlayManager, RecentRdpRequestBlocksAnOverlap) {
    g_ModuleRdpRequestFrames[MODULE_HOLE_BUILDER] = TEST_FRAME;

    TEST_ASSERT_FALSE(CanLoadOverlay(f_ShotSimulationId));
    g_FrameCount = TEST_FRAME + RDP_REQUEST_FRAMES_IN_USE - 1;
    TEST_ASSERT_FALSE(CanLoadOverlay(f_ShotSimulationId));
    g_FrameCount = TEST_FRAME + RDP_REQUEST_FRAMES_IN_USE;
    TEST_ASSERT_TRUE(CanLoadOverlay(f_ShotSimulationId));
}

TEST(OverlayManager, OwnOrDistantRdpRequestsDontBlock) {
    g_ModuleRdpRequestFrames[MODULE_SHOT_SIMULATION] = TEST_FRAME;
    g_ModuleRdpRequestFrames[MODULE_SCORECARD] = TEST_FRAME;

    TEST_ASSERT_TRUE(CanLoadOverlay(f_ShotSimulationId));
}

TEST_GROUP_RUNNER(OverlayManager) {
    RUN_TEST_CASE(OverlayManager, ModulesInNoOverlayAreAlive);
    RUN_TEST_CASE(OverlayManager, ModulesMapToTheirOverlays);
    RUN_TEST_CASE(OverlayManager, OverlayIdsOutsideTheTableGiveTheFirst);
    RUN_TEST_CASE(OverlayManager, LoadingMarksItsModulesAlive);
    RUN_TEST_CASE(OverlayManager, UnloadingMarksItsModulesDead);
    RUN_TEST_CASE(OverlayManager, LoadingALoadedOverlayReloadsIt);
    RUN_TEST_CASE(OverlayManager, UnloadingAnUnloadedOverlayChangesNothing);
    RUN_TEST_CASE(OverlayManager, OverlappingOverlayCannotLoad);
    RUN_TEST_CASE(OverlayManager, UnloadingMakesRoomForAnOverlap);
    RUN_TEST_CASE(OverlayManager, OverlaysApartLoadTogether);
    RUN_TEST_CASE(OverlayManager, InitKeepsLoadedOverlays);
    RUN_TEST_CASE(OverlayManager, RecentRdpRequestBlocksAnOverlap);
    RUN_TEST_CASE(OverlayManager, OwnOrDistantRdpRequestsDontBlock);
}
