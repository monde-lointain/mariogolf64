/*
 * Tests of MenuIconImage, which finds a buffer of g_MenuIconImages by its
 * index across all the pairs, as the main menu indexes them.
 */

#include <ultra64.h>

#include "menus/menu.h"
#include "system/c_language.h"
#include "test_support.h"

// A buffer list of its own, linked in place of the game's
ImageAsset *g_MenuIconImages[MENU_ICON_COUNT];
static u8 f_Buffers[MENU_ICON_COUNT];

static void *FindImage(s32 index) {
    volatile s32 volatileIndex = index;

    return MenuIconImage(volatileIndex);
}

static void FillImages(void) {
    s32 index;

    for (index = 0; index != MENU_ICON_COUNT; index++) {
        g_MenuIconImages[index] = (ImageAsset *)&f_Buffers[index];
    }
}

// The first pair's buffers are the first two
static void TestFindsTheFirstPair(void) {
    EXPECT(FindImage(0) == &f_Buffers[0]);
    EXPECT(FindImage(1) == &f_Buffers[1]);
}

// The buffers past the first pair, as the courses and the second mini-golf
// buffer use them, up to the last
static void TestFindsTheBuffersPastTheFirstPair(void) {
    EXPECT(FindImage(MINI_GOLF_DISC_IMAGE_COUNT) == &f_Buffers[MINI_GOLF_DISC_IMAGE_COUNT]);
    EXPECT(FindImage(MENU_ICON_COUNT - 1) == &f_Buffers[MENU_ICON_COUNT - 1]);
}

int main(void) {
    FillImages();
    TestFindsTheFirstPair();
    TestFindsTheBuffersPastTheFirstPair();
    return ReportTestResults("menu_icon_image_test");
}
