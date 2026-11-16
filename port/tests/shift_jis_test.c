/*
 * Tests of the Shift-JIS copies that the build compiles of the game's
 * sources. This file is built through the same copy, so its own strings and
 * line marker show what the game's get.
 */

#include <string.h>

#include "test_support.h"

// Half-width katakana take one byte each in Shift-JIS, against three in UTF-8
static void TestHalfWidthKatakana(void) {
    static const char text[] = "ｶﾅ";

    EXPECT(sizeof(text) == 3);
    EXPECT((unsigned char)text[0] == 0xB6);
    EXPECT((unsigned char)text[1] == 0xC5);
}

// A voiced mark is a byte of its own after its kana, as the game's font
// draws it
static void TestVoicedKana(void) {
    static const char text[] = "ﾎﾞｰﾙ";
    static const unsigned char expected[] = {0xCE, 0xDE, 0xB0, 0xD9};

    EXPECT(sizeof(text) == sizeof(expected) + 1);
    EXPECT(memcmp(text, expected, sizeof(expected)) == 0);
}

// Full-width characters take two bytes, lead byte first
static void TestFullWidthKanji(void) {
    static const char text[] = "漢";

    EXPECT(sizeof(text) == 3);
    EXPECT((unsigned char)text[0] == 0x8A);
    EXPECT((unsigned char)text[1] == 0xBF);
}

// ASCII passes through unchanged, backslash escapes included
static void TestAscii(void) {
    static const char text[] = "Hole %d\\\n";

    EXPECT(strcmp(text, "Hole %d\\\n") == 0);
    EXPECT(text[7] == 0x5C);
}

// The copy's line marker names the source and keeps its line numbers, so
// the compiler's messages and assertions point at the source
static void TestLineMarker(void) {
    EXPECT(strcmp(__FILE__, SHIFT_JIS_TEST_SOURCE) == 0);
    EXPECT(__LINE__ == 51);
}

int main(void) {
    TestHalfWidthKatakana();
    TestVoicedKana();
    TestFullWidthKanji();
    TestAscii();
    TestLineMarker();
    return ReportTestResults("shift_jis_test");
}
