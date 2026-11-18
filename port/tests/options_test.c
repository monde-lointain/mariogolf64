/*
 * Tests of the program's command line: each option read into its field,
 * the limits of the numbers, and what isn't an option.
 */

#include <SDL3/SDL.h>

#include "config.h"
#include "options.h"
#include "test_support.h"

// Parses the words given after the program's name, which ends with NULL
static bool ParseWords(Options *options, const char *const *words) {
    char *argv[16];
    int argc = 1;

    argv[0] = "mariogolf64";
    while (words[argc - 1] != NULL) {
        SDL_assert(argc < (int)SDL_arraysize(argv));
        argv[argc] = (char *)words[argc - 1];
        argc++;
    }

    return ParseOptions(argc, argv, options);
}

// A macro's value as a string
#define STRING_OF(macro) SDL_STRINGIFY_ARG(macro)

#define PARSE(options, ...) ParseWords((options), (const char *const[]) {__VA_ARGS__, NULL})

// With no options, each is unset, whatever the structure held before
static void TestNoOptionsLeavesAllUnset(void) {
    Options options;
    char *argv[] = {"mariogolf64"};

    SDL_memset(&options, 0xA5, sizeof(options));
    EXPECT(ParseOptions(1, argv, &options));
    EXPECT(options.romPath == NULL);
    EXPECT(options.savePath == NULL);
    EXPECT(options.configPath == NULL);
    EXPECT(options.screenshotPath == NULL);
    EXPECT(options.inputScriptPath == NULL);
    EXPECT(options.recordingPath == NULL);
    EXPECT(options.renderScale == 0);
    EXPECT(options.frameLimit == 0);
    EXPECT(options.gameFrameLimit == 0);
    EXPECT(!options.isFullscreen);
    EXPECT(!options.isGpuDebug);
    EXPECT(!options.isHeadless);
}

// Each path goes to its own field, and a word that isn't an option is the ROM
static void TestPathsGoToTheirFields(void) {
    Options options;

    EXPECT(PARSE(
        &options,
        "--save",
        "a.sra",
        "--config",
        "b.ini",
        "--screenshot",
        "c.png",
        "--input-script",
        "d.txt",
        "--record",
        "e.txt",
        "rom.z64"
    ));
    EXPECT_STRING(options.savePath, "a.sra");
    EXPECT_STRING(options.configPath, "b.ini");
    EXPECT_STRING(options.screenshotPath, "c.png");
    EXPECT_STRING(options.inputScriptPath, "d.txt");
    EXPECT_STRING(options.recordingPath, "e.txt");
    EXPECT_STRING(options.romPath, "rom.z64");
}

// Each switch sets its own field alone
static void TestSwitchesSetTheirFields(void) {
    Options options;

    EXPECT(PARSE(&options, "--headless"));
    EXPECT(options.isHeadless && !options.isFullscreen && !options.isGpuDebug);
    EXPECT(PARSE(&options, "--fullscreen"));
    EXPECT(!options.isHeadless && options.isFullscreen && !options.isGpuDebug);
    EXPECT(PARSE(&options, "--gpu-debug"));
    EXPECT(!options.isHeadless && !options.isFullscreen && options.isGpuDebug);
}

// The scale goes from 1 to CONFIG_SCALE_MAX
static void TestScaleIsWithinItsLimits(void) {
    Options options;

    EXPECT(!PARSE(&options, "--scale", "0"));
    EXPECT(PARSE(&options, "--scale", "1"));
    EXPECT(options.renderScale == 1);
    EXPECT(PARSE(&options, "--scale", STRING_OF(CONFIG_SCALE_MAX)));
    EXPECT(options.renderScale == CONFIG_SCALE_MAX);
    EXPECT(!PARSE(&options, "--scale", "9"));
}

// Both counts of frames start at 1, each going to its own field
static void TestFrameCountsStartAtOne(void) {
    Options options;

    EXPECT(!PARSE(&options, "--frames", "0"));
    EXPECT(!PARSE(&options, "--frames", "-1"));
    EXPECT(PARSE(&options, "--frames", "1"));
    EXPECT(options.frameLimit == 1 && options.gameFrameLimit == 0);
    EXPECT(!PARSE(&options, "--game-frames", "0"));
    EXPECT(!PARSE(&options, "--game-frames", "-1"));
    EXPECT(PARSE(&options, "--game-frames", "1"));
    EXPECT(options.gameFrameLimit == 1 && options.frameLimit == 0);
    EXPECT(PARSE(&options, "--frames", "29478", "--game-frames", "12345"));
    EXPECT(options.frameLimit == 29478 && options.gameFrameLimit == 12345);
}

// A number must be the option's whole word: not empty, and with nothing
// after its digits
static void TestNumbersAreWholeWords(void) {
    Options options;

    EXPECT(!PARSE(&options, "--scale", "2x"));
    EXPECT(!PARSE(&options, "--scale", ""));
    EXPECT(!PARSE(&options, "--frames", "10abc"));
    EXPECT(!PARSE(&options, "--frames", "1.5"));
    EXPECT(!PARSE(&options, "--frames", ""));
    EXPECT(!PARSE(&options, "--game-frames", "7 8"));
    EXPECT(!PARSE(&options, "--game-frames", "x"));
}

// The counts end at the largest 32-bit number on every host, the largest
// long where it is 32 bits wide, as on Windows
static void TestFrameCountsEndAtLargestInt32(void) {
    Options options;

    EXPECT(PARSE(&options, "--frames", "2147483647"));
    EXPECT(options.frameLimit == 2147483647L);
    EXPECT(!PARSE(&options, "--frames", "2147483648"));
    EXPECT(!PARSE(&options, "--frames", "99999999999999999999"));
    EXPECT(PARSE(&options, "--game-frames", "2147483647"));
    EXPECT(options.gameFrameLimit == 2147483647L);
    EXPECT(!PARSE(&options, "--game-frames", "2147483648"));
}

// An unknown option, a second ROM or an option without its value is refused
static void TestOtherWordsAreRefused(void) {
    Options options;

    EXPECT(!PARSE(&options, "--sale", "a.sra"));
    EXPECT(!PARSE(&options, "-h"));
    EXPECT(!PARSE(&options, "one.z64", "two.z64"));
    EXPECT(!PARSE(&options, "--save"));
    EXPECT(!PARSE(&options, "--frames"));
    EXPECT(!PARSE(&options, "--scale"));
}

int main(void) {
    TestNoOptionsLeavesAllUnset();
    TestPathsGoToTheirFields();
    TestSwitchesSetTheirFields();
    TestScaleIsWithinItsLimits();
    TestFrameCountsStartAtOne();
    TestNumbersAreWholeWords();
    TestFrameCountsEndAtLargestInt32();
    TestOtherWordsAreRefused();
    return ReportTestResults("options_test");
}
