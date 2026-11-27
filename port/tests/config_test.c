/*
 * Tests of the player's settings and the text file that holds them: each
 * setting's values at its limits and just past them, the problems that the
 * reader reports, and that what the writer writes reads back the same.
 */

#include <SDL3/SDL.h>

#include "config.h"
#include "test_support.h"

#define PROBLEM_LIMIT 8

// The problems that a parse reported
typedef struct Problems {
    int count;
    int lines[PROBLEM_LIMIT];
    char texts[PROBLEM_LIMIT][256];
} Problems;

static void NoteProblem(void *context, int line, const char *problem) {
    Problems *problems = context;

    if (problems->count < PROBLEM_LIMIT) {
        problems->lines[problems->count] = line;
        SDL_strlcpy(problems->texts[problems->count], problem, sizeof(problems->texts[0]));
    }

    problems->count++;
}

// Parses a NUL-ended text over the defaults, noting its problems
static int ParseOverDefaults(const char *text, Config *config, Problems *problems) {
    SetDefaultConfig(config);
    SDL_zerop(problems);
    return ParseConfig(text, SDL_strlen(text), config, NoteProblem, problems);
}

static bool IsSameConfig(const Config *a, const Config *b) {
    return SDL_strcmp(a->romPath, b->romPath) == 0 &&
        a->renderScale == b->renderScale &&
        a->isFullscreen == b->isFullscreen &&
        a->present == b->present &&
        a->antialiasing == b->antialiasing &&
        a->windowWidth == b->windowWidth &&
        a->windowHeight == b->windowHeight &&
        a->hasWindowPosition == b->hasWindowPosition &&
        a->windowX == b->windowX &&
        a->windowY == b->windowY &&
        a->pausesWhenUnfocused == b->pausesWhenUnfocused &&
        a->volume == b->volume &&
        SDL_memcmp(&a->bindings, &b->bindings, sizeof(a->bindings)) == 0;
}

// Whether a one-line setting under a section parses, and with no problem
static bool AcceptsSetting(const char *section, const char *line, Config *config) {
    char text[512];
    Problems problems;

    (void)SDL_snprintf(text, sizeof(text), "[%s]\n%s\n", section, line);
    return ParseOverDefaults(text, config, &problems) == 0 && problems.count == 0;
}

// --- Defaults ---

static void TestDefaults(void) {
    Config config;
    PadBindings bindings;

    SDL_memset(&config, 0xA5, sizeof(config));
    SetDefaultConfig(&config);
    SetDefaultPadBindings(&bindings);
    EXPECT(config.romPath[0] == '\0');
    EXPECT(config.renderScale == 0);
    EXPECT(!config.isFullscreen);
    EXPECT(config.present == PRESENT_MAILBOX);
    EXPECT(config.antialiasing == ANTIALIASING_OFF);
    EXPECT(config.windowWidth == 0 && config.windowHeight == 0);
    EXPECT(!config.hasWindowPosition);
    EXPECT(config.pausesWhenUnfocused);
    EXPECT(config.volume == 100);
    EXPECT(SDL_memcmp(&config.bindings, &bindings, sizeof(bindings)) == 0);
}

// --- Reading back what's written ---

// The defaults' text reads back as the defaults over any other settings
static void TestDefaultsReadBack(void) {
    Config defaults;
    Config config;
    char *text;

    SetDefaultConfig(&defaults);
    text = FormatConfig(&defaults);
    EXPECT(text != NULL);
    if (text == NULL) {
        return;
    }

    SetDefaultConfig(&config);
    config.renderScale = 3;
    config.volume = 7;
    config.hasWindowPosition = true;
    SDL_strlcpy(config.romPath, "other.z64", sizeof(config.romPath));
    ClearPadSources(&config.bindings, PAD_CONTROL_A);
    EXPECT(ParseConfig(text, SDL_strlen(text), &config, NULL, NULL) == 0);
    EXPECT(IsSameConfig(&config, &defaults));
    // Empty values leave no space at the ends of their lines
    EXPECT(SDL_strstr(text, " \n") == NULL);
    EXPECT(SDL_strstr(text, "\nrom =\n") != NULL);
    SDL_free(text);
}

// Every setting changed from its default, at its limits, reads back
static void TestEverySettingReadsBack(void) {
    Config written;
    Config config;
    char *text;
    int control;

    SetDefaultConfig(&written);
    SDL_strlcpy(written.romPath, "/roms/Mario Golf (U) [!].z64", sizeof(written.romPath));
    written.renderScale = CONFIG_SCALE_MAX;
    written.isFullscreen = true;
    written.present = PRESENT_IMMEDIATE;
    written.antialiasing = ANTIALIASING_8X;
    written.windowWidth = CONFIG_WINDOW_SIZE_MIN;
    written.windowHeight = CONFIG_WINDOW_SIZE_MAX;
    written.hasWindowPosition = true;
    written.windowX = -CONFIG_WINDOW_POSITION_LIMIT;
    written.windowY = CONFIG_WINDOW_POSITION_LIMIT;
    written.pausesWhenUnfocused = false;
    written.volume = 0;
    for (control = 0; control < PAD_CONTROL_COUNT; control++) {
        ClearPadSources(&written.bindings, (PadControl)control);
    }

    (void)AddPadSource(
        &written.bindings,
        PAD_CONTROL_A,
        (PadSource) {PAD_SOURCE_KEY, SDL_SCANCODE_COMMA}
    );
    (void)AddPadSource(
        &written.bindings,
        PAD_CONTROL_A,
        (PadSource) {PAD_SOURCE_KEY, SDL_SCANCODE_KP_COMMA}
    );
    (void)AddPadSource(
        &written.bindings,
        PAD_CONTROL_A,
        (PadSource) {PAD_SOURCE_KEY, SDL_SCANCODE_LSHIFT}
    );
    (void)AddPadSource(
        &written.bindings,
        PAD_CONTROL_A,
        (PadSource) {PAD_SOURCE_BUTTON, SDL_GAMEPAD_BUTTON_MISC6}
    );
    (void)AddPadSource(
        &written.bindings,
        PAD_CONTROL_STICK_LEFT,
        (PadSource) {PAD_SOURCE_AXIS_MINUS, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER}
    );
    (void)AddPadSource(
        &written.bindings,
        PAD_CONTROL_D_RIGHT,
        (PadSource) {PAD_SOURCE_AXIS_PLUS, SDL_GAMEPAD_AXIS_LEFTX}
    );
    (void)AddPadSource(
        &written.bindings,
        PAD_CONTROL_START,
        (PadSource) {PAD_SOURCE_BUTTON, SDL_GAMEPAD_BUTTON_SOUTH}
    );
    written.bindings.stick.deadZone = PAD_AXIS_MAX - 1;
    written.bindings.stick.fullThrow = PAD_AXIS_MAX;
    written.bindings.stick.range = 1;

    text = FormatConfig(&written);
    EXPECT(text != NULL);
    if (text == NULL) {
        return;
    }

    SetDefaultConfig(&config);
    EXPECT(ParseConfig(text, SDL_strlen(text), &config, NULL, NULL) == 0);
    EXPECT(IsSameConfig(&config, &written));
    SDL_free(text);
}

// --- Each setting's values ---

static void TestBooleans(void) {
    Config config;

    EXPECT(AcceptsSetting("video", "fullscreen = true", &config) && config.isFullscreen);
    EXPECT(AcceptsSetting("video", "fullscreen = YES", &config) && config.isFullscreen);
    EXPECT(AcceptsSetting("video", "fullscreen = On", &config) && config.isFullscreen);
    EXPECT(AcceptsSetting("game", "pause_when_unfocused = false", &config));
    EXPECT(!config.pausesWhenUnfocused);
    EXPECT(AcceptsSetting("game", "pause_when_unfocused = no", &config));
    EXPECT(!config.pausesWhenUnfocused);
    EXPECT(AcceptsSetting("game", "pause_when_unfocused = OFF", &config));
    EXPECT(!config.pausesWhenUnfocused);
    EXPECT(!AcceptsSetting("video", "fullscreen = 1", &config));
    EXPECT(!AcceptsSetting("video", "fullscreen = truer", &config));
    EXPECT(!AcceptsSetting("video", "fullscreen =", &config));
}

// The scale and the volume at each end, and one past
static void TestIntegerLimits(void) {
    Config config;

    EXPECT(!AcceptsSetting("video", "scale = -1", &config));
    EXPECT(AcceptsSetting("video", "scale = 0", &config) && config.renderScale == 0);
    EXPECT(AcceptsSetting("video", "scale = 8", &config) && config.renderScale == 8);
    EXPECT(!AcceptsSetting("video", "scale = 9", &config));
    EXPECT(config.renderScale == 0);
    EXPECT(AcceptsSetting("audio", "volume = 0", &config) && config.volume == 0);
    EXPECT(AcceptsSetting("audio", "volume = 100", &config) && config.volume == 100);
    EXPECT(!AcceptsSetting("audio", "volume = 101", &config));
    EXPECT(!AcceptsSetting("audio", "volume = 5x", &config));
    EXPECT(!AcceptsSetting("audio", "volume = 99999999999999999999", &config));
    EXPECT(!AcceptsSetting("audio", "volume = ", &config));
    EXPECT(config.volume == 100);
}

// A window's size is 0 or from its least to its greatest
static void TestWindowSizeLimits(void) {
    Config config;

    EXPECT(AcceptsSetting("window", "width = 0", &config) && config.windowWidth == 0);
    EXPECT(!AcceptsSetting("window", "width = 1", &config));
    EXPECT(!AcceptsSetting("window", "width = 159", &config));
    EXPECT(AcceptsSetting("window", "width = 160", &config) && config.windowWidth == 160);
    EXPECT(AcceptsSetting("window", "height = 16384", &config) && config.windowHeight == 16384);
    EXPECT(!AcceptsSetting("window", "height = 16385", &config));
    EXPECT(!AcceptsSetting("window", "height = -160", &config));
}

static void TestWindowPosition(void) {
    Config config;

    EXPECT(AcceptsSetting("window", "position = 12, -34", &config));
    EXPECT(config.hasWindowPosition && config.windowX == 12 && config.windowY == -34);
    EXPECT(AcceptsSetting("window", "position = -65536,65536", &config));
    EXPECT(config.windowX == -65536 && config.windowY == 65536);
    EXPECT(!AcceptsSetting("window", "position = -65537, 0", &config));
    EXPECT(!AcceptsSetting("window", "position = 0, 65537", &config));
    EXPECT(!config.hasWindowPosition);
    EXPECT(!AcceptsSetting("window", "position = 12", &config));
    EXPECT(!AcceptsSetting("window", "position = 12,", &config));
    EXPECT(!AcceptsSetting("window", "position = ,12", &config));
    EXPECT(!AcceptsSetting("window", "position = 1, 2, 3", &config));
    EXPECT(!config.hasWindowPosition);

    // An empty position lets the system place the window again
    EXPECT(AcceptsSetting("window", "position = 5, 6\nposition =", &config));
    EXPECT(!config.hasWindowPosition && config.windowX == 0 && config.windowY == 0);
}

static void TestPresentChoices(void) {
    Config config;

    EXPECT(AcceptsSetting("video", "present = vsync", &config));
    EXPECT(config.present == PRESENT_VSYNC);
    EXPECT(AcceptsSetting("video", "present = Immediate", &config));
    EXPECT(config.present == PRESENT_IMMEDIATE);
    EXPECT(AcceptsSetting("video", "present = vsync\npresent = mailbox", &config));
    EXPECT(config.present == PRESENT_MAILBOX);
    EXPECT(!AcceptsSetting("video", "present = fifo", &config));
}

static void TestAntialiasingChoices(void) {
    Config config;

    EXPECT(AcceptsSetting("video", "antialiasing = 2x", &config));
    EXPECT(config.antialiasing == ANTIALIASING_2X);
    EXPECT(AcceptsSetting("video", "antialiasing = 4X", &config));
    EXPECT(config.antialiasing == ANTIALIASING_4X);
    EXPECT(AcceptsSetting("video", "antialiasing = 8x", &config));
    EXPECT(config.antialiasing == ANTIALIASING_8X);
    EXPECT(AcceptsSetting("video", "antialiasing = 4x\nantialiasing = off", &config));
    EXPECT(config.antialiasing == ANTIALIASING_OFF);
    EXPECT(!AcceptsSetting("video", "antialiasing = 16x", &config));
    EXPECT(!AcceptsSetting("video", "antialiasing = 4", &config));
}

// The path is the whole value, after the first =, trimmed; the longest
// that fits is CONFIG_PATH_SIZE less its NUL
static void TestRomPath(void) {
    static char text[CONFIG_PATH_SIZE + 32];
    Config config;
    Problems problems;
    size_t prefix;

    EXPECT(AcceptsSetting("game", "rom =  a=b # c.z64  ", &config));
    EXPECT(SDL_strcmp(config.romPath, "a=b # c.z64") == 0);
    EXPECT(AcceptsSetting("game", "rom = x\nrom =", &config) && config.romPath[0] == '\0');

    prefix = (size_t)SDL_snprintf(text, sizeof(text), "[game]\nrom = ");
    SDL_memset(text + prefix, 'p', CONFIG_PATH_SIZE - 1);
    text[prefix + CONFIG_PATH_SIZE - 1] = '\0';
    EXPECT(ParseOverDefaults(text, &config, &problems) == 0);
    EXPECT(SDL_strlen(config.romPath) == CONFIG_PATH_SIZE - 1);

    text[prefix + CONFIG_PATH_SIZE - 1] = 'p';
    text[prefix + CONFIG_PATH_SIZE] = '\0';
    EXPECT(ParseOverDefaults(text, &config, &problems) == 1);
    EXPECT(config.romPath[0] == '\0');
}

// --- Controls ---

static PadSource SourceOf(const Config *config, PadControl control, int index) {
    return config->bindings.sources[control][index];
}

static void TestControlSources(void) {
    Config config;
    PadSource source;

    EXPECT(AcceptsSetting(
        "controls",
        "z = Left Shift, pad:West,PAD:-righttrigger , pad:+leftx",
        &config
    ));
    source = SourceOf(&config, PAD_CONTROL_Z, 0);
    EXPECT(source.kind == PAD_SOURCE_KEY && source.code == SDL_SCANCODE_LSHIFT);
    source = SourceOf(&config, PAD_CONTROL_Z, 1);
    EXPECT(source.kind == PAD_SOURCE_BUTTON && source.code == SDL_GAMEPAD_BUTTON_WEST);
    source = SourceOf(&config, PAD_CONTROL_Z, 2);
    EXPECT(source.kind == PAD_SOURCE_AXIS_MINUS && source.code == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    source = SourceOf(&config, PAD_CONTROL_Z, 3);
    EXPECT(source.kind == PAD_SOURCE_AXIS_PLUS && source.code == SDL_GAMEPAD_AXIS_LEFTX);

    EXPECT(AcceptsSetting("controls", "c_left = comma, Keypad Comma", &config));
    EXPECT(SourceOf(&config, PAD_CONTROL_C_LEFT, 0).code == SDL_SCANCODE_COMMA);
    EXPECT(SourceOf(&config, PAD_CONTROL_C_LEFT, 1).code == SDL_SCANCODE_KP_COMMA);
    EXPECT(SourceOf(&config, PAD_CONTROL_C_LEFT, 2).kind == PAD_SOURCE_NONE);

    EXPECT(AcceptsSetting("controls", "stick_up = pad:misc6", &config));
    EXPECT(SourceOf(&config, PAD_CONTROL_STICK_UP, 0).code == SDL_GAMEPAD_BUTTON_MISC6);

    // An empty value unbinds
    EXPECT(AcceptsSetting("controls", "start =", &config));
    EXPECT(SourceOf(&config, PAD_CONTROL_START, 0).kind == PAD_SOURCE_NONE);
}

// A control with a source that can't be used keeps all its old sources
static void TestBadControlsKeepTheirSources(void) {
    static const char *const bad[] = {
        "a = Q, W, E, R, T",
        "a = Q,",
        "a = , Q",
        "a = Q,,W",
        "a = Nonesuch",
        "a = pad:",
        "a = pad:+",
        "a = pad:-nonesuch",
        "a = pad:a",
        "a = pad:+south",
        "a = pad:leftx",
        "a = ,",
    };
    Config config;
    PadBindings defaults;
    size_t i;

    SetDefaultPadBindings(&defaults);
    for (i = 0; i < SDL_arraysize(bad); i++) {
        EXPECT(!AcceptsSetting("controls", bad[i], &config));
        EXPECT(SDL_memcmp(&config.bindings, &defaults, sizeof(defaults)) == 0);
    }

    EXPECT(AcceptsSetting("controls", "a = Q, W, E, R", &config));
}

// The stick's shape takes each value within its limits, as long as the
// dead zone stays below the full throw; otherwise the file's shape is
// dropped as a whole
static void TestStickShape(void) {
    Config config;
    Problems problems;

    EXPECT(AcceptsSetting(
        "controls",
        "stick_dead_zone = 0\nstick_full_throw = 1\nstick_range = 80",
        &config
    ));
    EXPECT(config.bindings.stick.deadZone == 0 && config.bindings.stick.fullThrow == 1);
    EXPECT(config.bindings.stick.range == 80);
    EXPECT(!AcceptsSetting("controls", "stick_range = 81", &config));
    EXPECT(!AcceptsSetting("controls", "stick_range = 0", &config));
    EXPECT(!AcceptsSetting("controls", "stick_full_throw = 32768", &config));
    EXPECT(AcceptsSetting("controls", "stick_dead_zone = 29999", &config));

    EXPECT(
        ParseOverDefaults(
            "[controls]\nstick_range = 40\nstick_dead_zone = 30000\n",
            &config,
            &problems
        ) == 1
    );
    EXPECT(problems.count == 1 && problems.lines[0] == 0);
    EXPECT(config.bindings.stick.deadZone == 7000 && config.bindings.stick.range == 80);
}

// --- The file's shape ---

// Comments, blank lines, CRLF, space around everything, any case in names,
// and a last line with no line end
static void TestLayout(void) {
    Config config;
    Problems problems;

    EXPECT(
        ParseOverDefaults(
            "# comment\r\n; another\r\n\r\n  [ Video ]  \r\n\tSCALE\t=\t4\t\r\n"
            "[audio]\nvolume=5",
            &config,
            &problems
        ) == 0
    );
    EXPECT(config.renderScale == 4 && config.volume == 5);
}

// Only the size given is read, as the text needn't end in a NUL
static void TestReadsOnlyItsSize(void) {
    const char text[] = "[audio]\nvolume = 55";
    Config config;

    SetDefaultConfig(&config);
    EXPECT(ParseConfig(text, sizeof(text) - 2, &config, NULL, NULL) == 0);
    EXPECT(config.volume == 5);
    EXPECT(ParseConfig(NULL, 0, &config, NULL, NULL) == 0);
}

// Each problem is reported with its line, changing nothing; an unknown
// section's own settings aren't
static void TestProblems(void) {
    Config config;
    Problems problems;

    EXPECT(
        ParseOverDefaults(
            "scale = 2\n"
            "[video]\n"
            "scale = 9\n"
            "scale\n"
            "nonesuch = 1\n"
            "[elsewhere]\n"
            "scale = 3\n"
            "anything\n"
            "[video\n"
            "scale = 4\n"
            "[]\n",
            &config,
            &problems
        ) == 8
    );
    EXPECT(problems.count == 8);
    EXPECT(problems.lines[0] == 1 && problems.lines[1] == 3 && problems.lines[2] == 4);
    EXPECT(problems.lines[3] == 5 && problems.lines[4] == 6 && problems.lines[5] == 9);
    EXPECT(problems.lines[6] == 10 && problems.lines[7] == 11);
    EXPECT(SDL_strcmp(problems.texts[1], "Can't use the value: scale = 9") == 0);
    EXPECT(SDL_strcmp(problems.texts[4], "No such section: elsewhere") == 0);
    EXPECT(config.renderScale == 0);
}

// Parses text from a heap block of just its size, so that reading past
// its end is an error under ASan
static int ParseExactly(const char *text, Config *config, Problems *problems) {
    const size_t size = SDL_strlen(text);
    char *copy = SDL_malloc(size);
    int count;

    SDL_memcpy(copy, text, size);
    SetDefaultConfig(config);
    SDL_zerop(problems);
    count = ParseConfig(copy, size, config, NoteProblem, problems);
    SDL_free(copy);
    return count;
}

// Texts that end mid-name or mid-value are read no further than their ends
static void TestReadsNoFurtherThanItsEnd(void) {
    Config config;
    Problems problems;

    EXPECT(ParseExactly("[controls]\na = pad:", &config, &problems) == 1);
    EXPECT(ParseExactly("[controls]\na = pad:+", &config, &problems) == 1);
    EXPECT(ParseExactly("[controls]\na = Q,", &config, &problems) == 1);
    EXPECT(ParseExactly("[window]\nposition = 1,", &config, &problems) == 1);
    EXPECT(ParseExactly("[audio]\nvolume = 1", &config, &problems) == 0);
    EXPECT(config.volume == 1);
    EXPECT(ParseExactly("[audio", &config, &problems) == 1);
    EXPECT(ParseExactly("[", &config, &problems) == 1);
}

// A value with a NUL in it can't be used
static void TestNulInValue(void) {
    const char text[] = "[game]\nrom = a\0b\n[controls]\na = Q\0\n";
    Config config;
    Problems problems;

    SetDefaultConfig(&config);
    SDL_zero(problems);
    EXPECT(ParseConfig(text, sizeof(text) - 1, &config, NoteProblem, &problems) == 2);
    EXPECT(config.romPath[0] == '\0');
    EXPECT(problems.lines[0] == 2 && problems.lines[1] == 4);
}

// With no handler, the problems are still counted
static void TestCountsWithoutHandler(void) {
    const char text[] = "[video]\nscale = 9\nnonesuch = 1\n";
    Config config;

    SetDefaultConfig(&config);
    EXPECT(ParseConfig(text, sizeof(text) - 1, &config, NULL, NULL) == 2);
}

// The longest path writes and reads back, past the writer's first room
static void TestLongestPathReadsBack(void) {
    Config written;
    Config config;
    char *text;

    SetDefaultConfig(&written);
    SDL_memset(written.romPath, 'q', CONFIG_PATH_SIZE - 1);
    written.romPath[CONFIG_PATH_SIZE - 1] = '\0';
    written.romPath[0] = '/';
    text = FormatConfig(&written);
    EXPECT(text != NULL);
    if (text == NULL) {
        return;
    }

    SetDefaultConfig(&config);
    EXPECT(ParseConfig(text, SDL_strlen(text), &config, NULL, NULL) == 0);
    EXPECT(IsSameConfig(&config, &written));
    SDL_free(text);
}

// A problem quotes printable characters only, and no more than 40
static void TestProblemQuotes(void) {
    Config config;
    Problems problems;

    EXPECT(
        ParseOverDefaults(
            "[video]\nx\x01\xFF"
            "123456789012345678901234567890123456789 = 1\n",
            &config,
            &problems
        ) == 1
    );
    EXPECT(
        SDL_strcmp(
            problems.texts[0],
            "No such setting: x??1234567890123456789012345678901234567..."
        ) == 0
    );

    // 40 characters exactly are quoted whole
    EXPECT(
        ParseOverDefaults(
            "[video]\n1234567890123456789012345678901234567890 = 1\n",
            &config,
            &problems
        ) == 1
    );
    EXPECT(
        SDL_strcmp(
            problems.texts[0],
            "No such setting: 1234567890123456789012345678901234567890"
        ) == 0
    );
}

int main(void) {
    TestDefaults();
    TestDefaultsReadBack();
    TestEverySettingReadsBack();
    TestBooleans();
    TestIntegerLimits();
    TestWindowSizeLimits();
    TestWindowPosition();
    TestPresentChoices();
    TestAntialiasingChoices();
    TestRomPath();
    TestControlSources();
    TestBadControlsKeepTheirSources();
    TestStickShape();
    TestLayout();
    TestReadsOnlyItsSize();
    TestProblems();
    TestProblemQuotes();
    TestReadsNoFurtherThanItsEnd();
    TestNulInValue();
    TestCountsWithoutHandler();
    TestLongestPathReadsBack();
    return ReportTestResults("config_test");
}
