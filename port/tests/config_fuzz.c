/*
 * Fuzzes the reader of the settings file. Whatever the text, the reader
 * reports as many problems as it counts, each on a line the text has, in
 * printable words; it leaves every setting within its limits and every
 * control's sources packed at the front and each one a key, button or
 * half axis that exists; and what the writer writes of the result reads
 * back the same, with no problems.
 */

#include <SDL3/SDL.h>

#include "config.h"
#include "fuzz_target.h"

typedef struct ProblemCount {
    int count;
    int lineCount;
} ProblemCount;

static void CheckProblem(void *context, int line, const char *problem) {
    ProblemCount *problems = context;
    size_t i;

    FUZZ_CHECK(line >= 0 && line <= problems->lineCount);
    FUZZ_CHECK(problem != NULL && problem[0] != '\0');
    for (i = 0; problem[i] != '\0'; i++) {
        FUZZ_CHECK(problem[i] >= 0x20 && problem[i] < 0x7F);
    }

    problems->count++;
}

static int CountLines(const uint8_t *data, size_t size) {
    int lines = 1;
    size_t i;

    for (i = 0; i < size; i++) {
        lines += (data[i] == '\n');
    }

    return lines;
}

static bool IsSourceValid(PadSource source) {
    switch (source.kind) {
        case PAD_SOURCE_KEY:
            return source.code > SDL_SCANCODE_UNKNOWN && source.code < SDL_SCANCODE_COUNT;
        case PAD_SOURCE_BUTTON:
            return source.code < SDL_GAMEPAD_BUTTON_COUNT;
        case PAD_SOURCE_AXIS_PLUS:
        case PAD_SOURCE_AXIS_MINUS:
            return source.code < SDL_GAMEPAD_AXIS_COUNT;
        default:
            return false;
    }
}

static void CheckBindings(const PadBindings *bindings) {
    int control;
    int i;

    FUZZ_CHECK(IsStickShapeValid(&bindings->stick));
    for (control = 0; control < PAD_CONTROL_COUNT; control++) {
        bool isEnded = false;

        for (i = 0; i < PAD_SOURCES_PER_CONTROL; i++) {
            const PadSource source = bindings->sources[control][i];

            if (source.kind == PAD_SOURCE_NONE) {
                FUZZ_CHECK(source.code == 0);
                isEnded = true;
            }
            else {
                FUZZ_CHECK(!isEnded && IsSourceValid(source));
            }
        }
    }
}

static bool IsWindowSizeValid(int size) {
    return size == 0 || (size >= CONFIG_WINDOW_SIZE_MIN && size <= CONFIG_WINDOW_SIZE_MAX);
}

static bool IsPositionValid(int position) {
    return position >= -CONFIG_WINDOW_POSITION_LIMIT && position <= CONFIG_WINDOW_POSITION_LIMIT;
}

static void CheckConfig(const Config *config) {
    FUZZ_CHECK(SDL_strnlen(config->romPath, CONFIG_PATH_SIZE) < CONFIG_PATH_SIZE);
    FUZZ_CHECK(config->renderScale >= 0 && config->renderScale <= CONFIG_SCALE_MAX);
    FUZZ_CHECK((int)config->present >= PRESENT_MAILBOX && config->present <= PRESENT_IMMEDIATE);
    FUZZ_CHECK(IsWindowSizeValid(config->windowWidth));
    FUZZ_CHECK(IsWindowSizeValid(config->windowHeight));
    FUZZ_CHECK(IsPositionValid(config->windowX) && IsPositionValid(config->windowY));
    FUZZ_CHECK(config->hasWindowPosition || (config->windowX == 0 && config->windowY == 0));
    FUZZ_CHECK(config->volume >= 0 && config->volume <= CONFIG_VOLUME_MAX);
    CheckBindings(&config->bindings);
}

static bool IsSameConfig(const Config *a, const Config *b) {
    return SDL_strcmp(a->romPath, b->romPath) == 0 &&
        a->renderScale == b->renderScale &&
        a->isFullscreen == b->isFullscreen &&
        a->present == b->present &&
        a->windowWidth == b->windowWidth &&
        a->windowHeight == b->windowHeight &&
        a->hasWindowPosition == b->hasWindowPosition &&
        a->windowX == b->windowX &&
        a->windowY == b->windowY &&
        a->pausesWhenUnfocused == b->pausesWhenUnfocused &&
        a->volume == b->volume &&
        SDL_memcmp(&a->bindings, &b->bindings, sizeof(a->bindings)) == 0;
}

// What's written reads back the same over settings that differ from it,
// so that each must have been written
static void CheckReadsBack(const Config *config) {
    static Config readBack;
    char *text = FormatConfig(config);
    int control;

    FUZZ_CHECK(text != NULL);
    SetDefaultConfig(&readBack);
    SDL_strlcpy(readBack.romPath, "elsewhere", sizeof(readBack.romPath));
    readBack.renderScale = (config->renderScale + 1) % (CONFIG_SCALE_MAX + 1);
    readBack.isFullscreen = !config->isFullscreen;
    readBack.present = (PresentChoice)((config->present + 1) % (PRESENT_IMMEDIATE + 1));
    readBack.windowWidth = (config->windowWidth == 0) ? CONFIG_WINDOW_SIZE_MIN : 0;
    readBack.windowHeight = (config->windowHeight == 0) ? CONFIG_WINDOW_SIZE_MIN : 0;
    readBack.hasWindowPosition = !config->hasWindowPosition;
    readBack.windowX = readBack.hasWindowPosition ? config->windowX + 1 : 0;
    readBack.windowY = readBack.hasWindowPosition ? 1 : 0;
    readBack.pausesWhenUnfocused = !config->pausesWhenUnfocused;
    readBack.volume = (config->volume + 1) % (CONFIG_VOLUME_MAX + 1);
    for (control = 0; control < PAD_CONTROL_COUNT; control++) {
        ClearPadSources(&readBack.bindings, (PadControl)control);
    }

    FUZZ_CHECK(ParseConfig(text, SDL_strlen(text), &readBack, NULL, NULL) == 0);
    FUZZ_CHECK(IsSameConfig(&readBack, config));
    SDL_free(text);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static Config config;
    static Config silent;
    // A copy of just the input's size, so that reading past it is an error
    char *text = SDL_malloc(size);
    ProblemCount problems;
    int count;

    if (text == NULL && size > 0) {
        return 0;
    }

    if (size > 0) {
        SDL_memcpy(text, data, size);
    }

    problems.count = 0;
    problems.lineCount = CountLines(data, size);
    SetDefaultConfig(&config);
    count = ParseConfig(text, size, &config, CheckProblem, &problems);
    FUZZ_CHECK(count == problems.count);
    CheckConfig(&config);

    // With no handler, the count and the settings are the same
    SetDefaultConfig(&silent);
    FUZZ_CHECK(ParseConfig(text, size, &silent, NULL, NULL) == count);
    FUZZ_CHECK(IsSameConfig(&silent, &config));

    CheckReadsBack(&config);
    SDL_free(text);
    return 0;
}
