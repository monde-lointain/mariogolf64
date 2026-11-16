/*
 * Tests the input scripts' parser and lookup: a nominal script, the least
 * and the boundaries of each field, and each kind of bad line.
 */

#include <SDL3/SDL.h>
#include <ultra64.h>

#include "input_script.h"
#include "test_support.h"

static char f_Error[128];

static InputScript *Parse(const char *text) {
    return ParseInputScript(text, f_Error, sizeof(f_Error));
}

// Whether a script fails on a line, and says which
static bool FailsOnLine(const char *text, int lineNumber) {
    InputScript *script = Parse(text);
    char expected[16];

    SDL_snprintf(expected, sizeof(expected), "line %d:", lineNumber);
    FreeInputScript(script);
    return script == NULL && SDL_strstr(f_Error, expected) == f_Error;
}

// Each line holds from its retrace until the next's, and nothing holds
// before the first
static void TestHoldsEachLine(void) {
    InputScript *script = Parse(
        "# Into the menus\n"
        "120 START\n"
        "\n"
        "126   # let go\n"
        "400 A B stick=-80,80\n"
    );
    OSContPad pad;

    EXPECT(script != NULL);
    EXPECT(!FindScriptedPad(script, 119, &pad));
    EXPECT(FindScriptedPad(script, 120, &pad) && pad.button == START_BUTTON);
    EXPECT(FindScriptedPad(script, 125, &pad) && pad.button == START_BUTTON);
    EXPECT(FindScriptedPad(script, 126, &pad) && pad.button == 0);
    EXPECT(pad.stick_x == 0 && pad.stick_y == 0);
    EXPECT(FindScriptedPad(script, 399, &pad) && pad.button == 0);
    EXPECT(FindScriptedPad(script, 400, &pad));
    EXPECT(pad.button == (A_BUTTON | B_BUTTON) && pad.stick_x == -80 && pad.stick_y == 80);
    EXPECT(FindScriptedPad(script, 100000, &pad) && pad.button == (A_BUTTON | B_BUTTON));
    FreeInputScript(script);
}

// An empty script holds nothing; one line from the first retrace holds
// from it on
static void TestLeastScripts(void) {
    InputScript *script = Parse("");
    OSContPad pad;

    EXPECT(script != NULL && !FindScriptedPad(script, 0, &pad));
    FreeInputScript(script);

    script = Parse("0 Z");
    EXPECT(script != NULL && FindScriptedPad(script, 0, &pad) && pad.button == Z_TRIG);
    FreeInputScript(script);
}

// Every button's name presses its own button
static void TestNamesEveryButton(void) {
    InputScript *script = Parse("1 A B Z START L R CUP CDOWN CLEFT CRIGHT UP DOWN LEFT RIGHT\r\n");
    OSContPad pad;
    u16 every = A_BUTTON | B_BUTTON | Z_TRIG | START_BUTTON | L_TRIG | R_TRIG | U_CBUTTONS |
        D_CBUTTONS | L_CBUTTONS | R_CBUTTONS | U_JPAD | D_JPAD | L_JPAD | R_JPAD;

    EXPECT(script != NULL && FindScriptedPad(script, 1, &pad) && pad.button == every);
    FreeInputScript(script);
}

// The stick's axes reach 80 either way, and no further
static void TestStickLimits(void) {
    InputScript *script = Parse("1 stick=80,-80");
    OSContPad pad;

    EXPECT(script != NULL && FindScriptedPad(script, 1, &pad));
    EXPECT(pad.stick_x == 80 && pad.stick_y == -80);
    FreeInputScript(script);

    EXPECT(FailsOnLine("1 stick=81,0", 1));
    EXPECT(FailsOnLine("1 stick=0,-81", 1));
}

// Bad lines: a retrace that isn't a number, is negative or doesn't
// increase, an unknown or misspelled word, and a malformed stick
static void TestRejectsBadLines(void) {
    EXPECT(FailsOnLine("START", 1));
    EXPECT(FailsOnLine("-1 A", 1));
    EXPECT(FailsOnLine("12x A", 1));
    EXPECT(FailsOnLine("10 A\n10 B", 2));
    EXPECT(FailsOnLine("10 A\n\n9 B", 3));
    EXPECT(FailsOnLine("1 a", 1));
    EXPECT(FailsOnLine("1 JUMP", 1));
    EXPECT(FailsOnLine("1 stick=5", 1));
    EXPECT(FailsOnLine("1 stick=5,", 1));
    EXPECT(FailsOnLine("1 stick=,5", 1));
    EXPECT(FailsOnLine("1 stick=1,2,3", 1));
    EXPECT(FailsOnLine("1 stick=99999999999999999999999999999999999,0", 1));
    EXPECT(SDL_strstr(f_Error, "stick=") != NULL);
}

// Many lines: the lookup finds each one's state at its boundaries
static void TestManyLines(void) {
    char *text = SDL_malloc(1000 * 16);
    char *end = text;
    InputScript *script;
    OSContPad pad;
    int i;
    int misses = 0;

    for (i = 0; i < 1000; i++) {
        end += SDL_snprintf(end, 16, "%d %s\n", (i * 3) + 5, (i % 2) != 0 ? "A" : "");
    }

    script = Parse(text);
    EXPECT(script != NULL);
    EXPECT(!FindScriptedPad(script, 4, &pad));
    for (i = 0; i < 1000; i++) {
        u16 expected = (i % 2) != 0 ? A_BUTTON : 0;

        misses += !FindScriptedPad(script, (i * 3) + 5, &pad) || pad.button != expected;
        misses += !FindScriptedPad(script, (i * 3) + 7, &pad) || pad.button != expected;
    }

    EXPECT(misses == 0);
    FreeInputScript(script);
    SDL_free(text);
}

int main(void) {
    TestHoldsEachLine();
    TestLeastScripts();
    TestNamesEveryButton();
    TestStickLimits();
    TestRejectsBadLines();
    TestManyLines();
    return ReportTestResults("input_script_test");
}
