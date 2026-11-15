/*
 * Starts the test ROM's tests in place of the game. The test ROM links the
 * game with --wrap=mainproc, so NuSystem's boot starts the main thread here
 * instead of in the game's mainproc. This prepares the machine as mainproc
 * does, loads the tests' segment from ROM into the Expansion Pak's memory
 * that the game leaves free (see segment.ld), and runs them.
 *
 * This file goes into main's code, with what the boot segment loads, so it
 * must not have a bss: the layout keeps none from an archive.
 */

#include <nusys.h>

#include "system/compiler.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "system/segment_symbols.h"

DECLARE_SEGMENT(target_tests);

void DisableFloatExceptions(void);
void RunTargetTests(void);
void __wrap_mainproc(void *arg);

void __wrap_mainproc(void *arg UNUSED) {
    u32 testsSize = BytesBetween(target_tests_ROM_START, target_tests_ROM_END);

    // As mainproc does: clear the memory after main's, which the game takes
    // for zeroed, and take IEEE results from float errors instead of traps
    bzero(main_BSS_END, BytesBetween(main_BSS_END, BASE_RDRAM_END));
    osWritebackDCacheAll();
    DisableFloatExceptions();

    nuPiReadRom((u32)target_tests_ROM_START, target_tests_VRAM, testsSize);
    bzero(target_tests_BSS_START, BytesBetween(target_tests_BSS_START, target_tests_BSS_END));
    osWritebackDCacheAll();
    osInvalICache(target_tests_VRAM, testsSize);

    RunTargetTests();
}
