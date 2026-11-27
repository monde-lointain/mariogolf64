/*
 * The player's settings, and the text file that holds them.
 */

#include "config.h"

#include <SDL3/SDL.h>
#include <string.h>

// Room for a problem's message, and for the name of a key, button or axis
#define PROBLEM_SIZE 256
#define NAME_SIZE 64
// How much of a line's key or value a problem quotes
#define QUOTE_LIMIT 40
#define PAD_PREFIX "pad:"

typedef enum SettingKind {
    SETTING_ROM_PATH,
    SETTING_BOOL,
    SETTING_INT,
    SETTING_WINDOW_SIZE,
    SETTING_WINDOW_POSITION,
    SETTING_CHOICE,
    SETTING_CONTROL
} SettingKind;

// A setting's place in the file and in Config
typedef struct Setting {
    const char *section;
    const char *key;
    SettingKind kind;
    size_t offset; // Of its field in Config, or its PadControl's number
    int min; // For SETTING_INT, its least and greatest values
    int max;
    const char *comment; // Lines to explain it, or NULL
    // For SETTING_CHOICE, the names of its enum's values in order, then NULL
    const char *const *names;
} Setting;

// A stretch of the text, not ended by a NUL
typedef struct Span {
    const char *start;
    size_t length;
} Span;

// The text that FormatConfig builds; once memory runs out, it stays NULL
typedef struct TextBuilder {
    char *text;
    size_t length;
    size_t capacity;
} TextBuilder;

// The names of the gamepad's buttons, by their places rather than their
// letters, which differ from one maker to the next
static const char *const f_ButtonNames[] = {
    "south",
    "east",
    "west",
    "north",
    "back",
    "guide",
    "start",
    "leftstick",
    "rightstick",
    "leftshoulder",
    "rightshoulder",
    "dpup",
    "dpdown",
    "dpleft",
    "dpright",
    "misc1",
    "paddle1",
    "paddle2",
    "paddle3",
    "paddle4",
    "touchpad",
    "misc2",
    "misc3",
    "misc4",
    "misc5",
    "misc6",
};
SDL_COMPILE_TIME_ASSERT(button_names, SDL_arraysize(f_ButtonNames) == SDL_GAMEPAD_BUTTON_COUNT);

static const char *const f_AxisNames[] = {
    "leftx",
    "lefty",
    "rightx",
    "righty",
    "lefttrigger",
    "righttrigger",
};
SDL_COMPILE_TIME_ASSERT(axis_names, SDL_arraysize(f_AxisNames) == SDL_GAMEPAD_AXIS_COUNT);

// The keys whose names SDL gives as commas, which separate a control's
// sources, go by these
static const struct {
    SDL_Scancode key;
    const char *name;
} f_KeyAliases[] = {
    {SDL_SCANCODE_COMMA, "Comma"},
    {SDL_SCANCODE_KP_COMMA, "Keypad Comma"},
};

// A choice's field holds its value's index, which takes an int's size
static const char *const f_PresentNames[] = {"mailbox", "vsync", "immediate", NULL};
SDL_COMPILE_TIME_ASSERT(present_size, sizeof(PresentChoice) == sizeof(int));
static const char *const f_AntialiasingNames[] = {"off", "2x", "4x", "8x", NULL};
SDL_COMPILE_TIME_ASSERT(antialiasing_size, sizeof(AntialiasingChoice) == sizeof(int));

#define FIELD(name) offsetof(Config, name)
#define CONTROL(key, control) {"controls", (key), SETTING_CONTROL, (control), 0, 0, NULL}

static const Setting f_Settings[] = {
    {"game",
        "rom",
        SETTING_ROM_PATH,
        FIELD(romPath),
        0,
        0,
        "The ROM to play when none is named on the command line; empty to look\n"
        "for baserom.us.z64 beside the program and in the current folder"},
    {"game",
        "pause_when_unfocused",
        SETTING_BOOL,
        FIELD(pausesWhenUnfocused),
        0,
        0,
        "Whether the game pauses while another window has the focus"},
    {"video",
        "scale",
        SETTING_INT,
        FIELD(renderScale),
        0,
        CONFIG_SCALE_MAX,
        "How many times the N64's 320x240 to draw at, from 1 to 8, or 0 to cover\n"
        "the display's height"},
    {"video", "fullscreen", SETTING_BOOL, FIELD(isFullscreen), 0, 0, "Whether to start fullscreen"},
    {"video",
        "present",
        SETTING_CHOICE,
        FIELD(present),
        0,
        0,
        "mailbox shows the newest frame at each refresh, vsync shows every frame\n"
        "and may wait, and immediate shows each at once and may tear",
        f_PresentNames},
    {"video",
        "antialiasing",
        SETTING_CHOICE,
        FIELD(antialiasing),
        0,
        0,
        "off, 2x, 4x or 8x: how many samples a pixel to draw edges with, or the\n"
        "most that the GPU has",
        f_AntialiasingNames},
    {"window",
        "width",
        SETTING_WINDOW_SIZE,
        FIELD(windowWidth),
        0,
        0,
        "The window's size, from 160 to 16384, or 0 for the default; on a\n"
        "display that scales, in its units rather than pixels. The game\n"
        "remembers the window's size and place here when it ends."},
    {"window", "height", SETTING_WINDOW_SIZE, FIELD(windowHeight), 0, 0, NULL},
    {"window",
        "position",
        SETTING_WINDOW_POSITION,
        0,
        0,
        0,
        "Where the window's top left goes, as x, y; empty to let the system\n"
        "place it"},
    {"audio", "volume", SETTING_INT, FIELD(volume), 0, CONFIG_VOLUME_MAX, "From 0, silent, to 100"},
    CONTROL("a", PAD_CONTROL_A),
    CONTROL("b", PAD_CONTROL_B),
    CONTROL("z", PAD_CONTROL_Z),
    CONTROL("start", PAD_CONTROL_START),
    CONTROL("l", PAD_CONTROL_L),
    CONTROL("r", PAD_CONTROL_R),
    CONTROL("c_up", PAD_CONTROL_C_UP),
    CONTROL("c_down", PAD_CONTROL_C_DOWN),
    CONTROL("c_left", PAD_CONTROL_C_LEFT),
    CONTROL("c_right", PAD_CONTROL_C_RIGHT),
    CONTROL("d_up", PAD_CONTROL_D_UP),
    CONTROL("d_down", PAD_CONTROL_D_DOWN),
    CONTROL("d_left", PAD_CONTROL_D_LEFT),
    CONTROL("d_right", PAD_CONTROL_D_RIGHT),
    CONTROL("stick_up", PAD_CONTROL_STICK_UP),
    CONTROL("stick_down", PAD_CONTROL_STICK_DOWN),
    CONTROL("stick_left", PAD_CONTROL_STICK_LEFT),
    CONTROL("stick_right", PAD_CONTROL_STICK_RIGHT),
    {"controls",
        "stick_dead_zone",
        SETTING_INT,
        FIELD(bindings.stick.deadZone),
        0,
        PAD_AXIS_MAX - 1,
        "How far the stick goes, out of 32767, before it moves the N64's, which\n"
        "must be less than its full throw"},
    {"controls",
        "stick_full_throw",
        SETTING_INT,
        FIELD(bindings.stick.fullThrow),
        1,
        PAD_AXIS_MAX,
        "How far, out of 32767, it goes to move the N64's all the way"},
    {"controls",
        "stick_range",
        SETTING_INT,
        FIELD(bindings.stick.range),
        1,
        PAD_STICK_RANGE_MAX,
        "How far the N64's stick goes all the way, from 1 to 80"},
};

static const char f_FileComment[] =
    "Mario Golf's settings. Lines that start with # are comments. The game\n"
    "rewrites this file to remember its window, keeping the settings but not\n"
    "comments of your own.";

static const char f_ControlsComment[] =
    "Each N64 control takes up to 4 sources, separated by commas: a key by\n"
    "its name (X, Return, Left Shift, Up, Comma), pad: and a gamepad button\n"
    "(pad:south, pad:east, pad:west, pad:north, pad:start, pad:back,\n"
    "pad:leftshoulder, pad:rightshoulder, pad:dpup, pad:leftstick, ...), or\n"
    "pad:, + or -, and a gamepad axis, for one half of it (pad:-lefty,\n"
    "pad:+righttrigger). An empty value unbinds the control.";

// --- Settings ---

// Phones' GPUs pay more for each sample than desktops' do, so they draw
// with one by default
#if defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_IOS)
#define DEFAULT_ANTIALIASING ANTIALIASING_OFF
#else
#define DEFAULT_ANTIALIASING ANTIALIASING_4X
#endif

void SetDefaultConfig(Config *config) {
    SDL_assert(config != NULL);
    SDL_zerop(config);
    config->renderScale = 0;
    config->isFullscreen = false;
    config->present = PRESENT_MAILBOX;
    config->antialiasing = DEFAULT_ANTIALIASING;
    config->pausesWhenUnfocused = true;
    config->volume = CONFIG_VOLUME_MAX;
    SetDefaultPadBindings(&config->bindings);
}

static void *SettingField(Config *config, const Setting *setting) {
    return (char *)config + setting->offset;
}

static const void *ConstSettingField(const Config *config, const Setting *setting) {
    return (const char *)config + setting->offset;
}

// --- Spans ---

static bool IsLineSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static Span TrimSpan(Span span) {
    while (span.length > 0 && IsLineSpace(span.start[0])) {
        span.start++;
        span.length--;
    }

    while (span.length > 0 && IsLineSpace(span.start[span.length - 1])) {
        span.length--;
    }

    return span;
}

static bool IsSpanIgnoringCase(Span span, const char *text) {
    return SDL_strlen(text) == span.length && SDL_strncasecmp(span.start, text, span.length) == 0;
}

// Copies the span to a NUL-ended name, or gives false when it holds a NUL
// or doesn't fit
static bool CopySpan(Span span, char *name, size_t size) {
    if (span.length >= size || memchr(span.start, '\0', span.length) != NULL) {
        return false;
    }

    SDL_memcpy(name, span.start, span.length);
    name[span.length] = '\0';
    return true;
}

// --- Values ---

static bool ParseInt(Span span, int min, int max, int *value) {
    char digits[16];
    char *end;
    long long number;

    if (!CopySpan(span, digits, sizeof(digits)) || digits[0] == '\0') {
        return false;
    }

    number = SDL_strtoll(digits, &end, 10);
    if (*end != '\0' || number < min || number > max) {
        return false;
    }

    *value = (int)number;
    return true;
}

static bool ParseBool(Span span, bool *value) {
    if (IsSpanIgnoringCase(span, "true") ||
        IsSpanIgnoringCase(span, "yes") ||
        IsSpanIgnoringCase(span, "on")) {
        *value = true;
        return true;
    }

    if (IsSpanIgnoringCase(span, "false") ||
        IsSpanIgnoringCase(span, "no") ||
        IsSpanIgnoringCase(span, "off")) {
        *value = false;
        return true;
    }

    return false;
}

// Finds a name in a table, ignoring case, and gives its index or -1
static int FindName(Span span, const char *const *names, int count) {
    int i;

    for (i = 0; i < count; i++) {
        if (IsSpanIgnoringCase(span, names[i])) {
            return i;
        }
    }

    return -1;
}

// Sets a choice's field to the index of the name that the span gives
static bool ParseChoice(Span span, const char *const *names, void *field) {
    int i;

    for (i = 0; names[i] != NULL; i++) {
        if (IsSpanIgnoringCase(span, names[i])) {
            SDL_memcpy(field, &i, sizeof(i));
            return true;
        }
    }

    return false;
}

static bool ParseWindowSize(Span span, int *value) {
    int size;

    if (!ParseInt(span, 0, CONFIG_WINDOW_SIZE_MAX, &size) ||
        (size != 0 && size < CONFIG_WINDOW_SIZE_MIN)) {
        return false;
    }

    *value = size;
    return true;
}

// Takes the item before the first comma, trimmed, leaving the rest after
// it; with no comma, takes all that's left and says it was the last
static Span TakeItem(Span *rest, bool *isLast) {
    const char *comma = memchr(rest->start, ',', rest->length);
    Span item = *rest;

    *isLast = (comma == NULL);
    if (comma != NULL) {
        item.length = (size_t)(comma - rest->start);
        rest->start = comma + 1;
        rest->length -= item.length + 1;
    }
    else {
        rest->start += rest->length;
        rest->length = 0;
    }

    return TrimSpan(item);
}

static bool ParseWindowPosition(Span span, Config *config) {
    const int limit = CONFIG_WINDOW_POSITION_LIMIT;
    Span rest = span;
    bool isLast;
    Span x;
    Span y;
    int windowX;
    int windowY;

    if (span.length == 0) {
        config->hasWindowPosition = false;
        config->windowX = 0;
        config->windowY = 0;
        return true;
    }

    // With no comma, y is empty, and can't be read
    x = TakeItem(&rest, &isLast);
    y = TakeItem(&rest, &isLast);
    if (!isLast || !ParseInt(x, -limit, limit, &windowX) || !ParseInt(y, -limit, limit, &windowY)) {
        return false;
    }

    config->hasWindowPosition = true;
    config->windowX = windowX;
    config->windowY = windowY;
    return true;
}

// Reads a key's name, or one of the gamepad's buttons or half axes
static bool ParsePadSource(Span span, PadSource *source) {
    const size_t prefixLength = SDL_strlen(PAD_PREFIX);
    char name[NAME_SIZE];
    SDL_Scancode key;
    size_t i;
    int index;

    if (span.length > prefixLength && SDL_strncasecmp(span.start, PAD_PREFIX, prefixLength) == 0) {
        Span pad = {span.start + prefixLength, span.length - prefixLength};

        if (pad.start[0] == '+' || pad.start[0] == '-') {
            const Span axis = {pad.start + 1, pad.length - 1};

            index = FindName(axis, f_AxisNames, (int)SDL_arraysize(f_AxisNames));
            source->kind = (pad.start[0] == '+') ? PAD_SOURCE_AXIS_PLUS : PAD_SOURCE_AXIS_MINUS;
        }
        else {
            index = FindName(pad, f_ButtonNames, (int)SDL_arraysize(f_ButtonNames));
            source->kind = PAD_SOURCE_BUTTON;
        }

        source->code = (uint16_t)index;
        return index >= 0;
    }

    for (i = 0; i < SDL_arraysize(f_KeyAliases); i++) {
        if (IsSpanIgnoringCase(span, f_KeyAliases[i].name)) {
            source->kind = PAD_SOURCE_KEY;
            source->code = (uint16_t)f_KeyAliases[i].key;
            return true;
        }
    }

    if (!CopySpan(span, name, sizeof(name)) || SDL_strchr(name, ',') != NULL) {
        return false;
    }

    key = SDL_GetScancodeFromName(name);
    if (key == SDL_SCANCODE_UNKNOWN) {
        return false;
    }

    source->kind = PAD_SOURCE_KEY;
    source->code = (uint16_t)key;
    return true;
}

// Reads a control's sources, all or none; an empty item, as a comma at
// the end leaves, is no source
static bool ParseControl(Span span, PadBindings *bindings, PadControl control) {
    PadBindings parsed = *bindings;
    Span rest = span;
    bool isLast = (span.length == 0);

    ClearPadSources(&parsed, control);
    while (!isLast) {
        const Span item = TakeItem(&rest, &isLast);
        PadSource source;

        if (!ParsePadSource(item, &source) || !AddPadSource(&parsed, control, source)) {
            return false;
        }
    }

    SDL_memcpy(
        bindings->sources[control],
        parsed.sources[control],
        sizeof(parsed.sources[control])
    );
    return true;
}

// Sets a setting from its value, or gives false and changes nothing
static bool ParseSetting(const Setting *setting, Span value, Config *config) {
    switch (setting->kind) {
        case SETTING_ROM_PATH:
            return CopySpan(value, SettingField(config, setting), CONFIG_PATH_SIZE);
        case SETTING_BOOL:
            return ParseBool(value, SettingField(config, setting));
        case SETTING_INT:
            return ParseInt(value, setting->min, setting->max, SettingField(config, setting));
        case SETTING_WINDOW_SIZE:
            return ParseWindowSize(value, SettingField(config, setting));
        case SETTING_WINDOW_POSITION:
            return ParseWindowPosition(value, config);
        case SETTING_CHOICE:
            return ParseChoice(value, setting->names, SettingField(config, setting));
        case SETTING_CONTROL:
            return ParseControl(value, &config->bindings, (PadControl)setting->offset);
        default:
            return false;
    }
}

// --- Reading ---

// Tells the handler of a problem, quoting a stretch of the line
static int ReportProblem(
    ConfigProblemHandler handler,
    void *context,
    int line,
    const char *what,
    Span quoted
) {
    char problem[PROBLEM_SIZE];
    char quote[QUOTE_LIMIT + 1];
    size_t i;

    if (handler == NULL) {
        return 1;
    }

    // The quote keeps to printable ASCII, whatever the file holds
    for (i = 0; i < quoted.length && i < QUOTE_LIMIT; i++) {
        const unsigned char c = (unsigned char)quoted.start[i];

        quote[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }

    quote[i] = '\0';
    (void)SDL_snprintf(
        problem,
        sizeof(problem),
        "%s: %s%s",
        what,
        quote,
        (quoted.length > QUOTE_LIMIT) ? "..." : ""
    );
    handler(context, line, problem);
    return 1;
}

static const Setting *FindSetting(Span section, Span key) {
    size_t i;

    for (i = 0; i < SDL_arraysize(f_Settings); i++) {
        if (IsSpanIgnoringCase(section, f_Settings[i].section) &&
            IsSpanIgnoringCase(key, f_Settings[i].key)) {
            return &f_Settings[i];
        }
    }

    return NULL;
}

static bool IsSection(Span name) {
    size_t i;

    for (i = 0; i < SDL_arraysize(f_Settings); i++) {
        if (IsSpanIgnoringCase(name, f_Settings[i].section)) {
            return true;
        }
    }

    return false;
}

int ParseConfig(
    const char *text,
    size_t size,
    Config *config,
    ConfigProblemHandler handler,
    void *context
) {
    const StickShape stickBefore = config->bindings.stick;
    const char *end = text + size;
    const char *next = text;
    Span section = {NULL, 0};
    bool isSectionKnown = false;
    int problems = 0;
    int line = 0;

    SDL_assert((text != NULL || size == 0) && config != NULL);
    while (next < end) {
        const char *lineEnd = memchr(next, '\n', (size_t)(end - next));
        Span span;
        const char *equals;

        lineEnd = (lineEnd != NULL) ? lineEnd : end;
        span.start = next;
        span.length = (size_t)(lineEnd - next);
        span = TrimSpan(span);
        next = (lineEnd < end) ? lineEnd + 1 : end;
        line++;
        if (span.length == 0 || span.start[0] == '#' || span.start[0] == ';') {
            continue;
        }

        if (span.start[0] == '[') {
            if (span.start[span.length - 1] != ']' || span.length < 2) {
                problems += ReportProblem(handler, context, line, "Not a section", span);
                section.start = NULL;
                section.length = 0;
                isSectionKnown = true;
                continue;
            }

            section.start = span.start + 1;
            section.length = span.length - 2;
            section = TrimSpan(section);
            isSectionKnown = IsSection(section);
            if (!isSectionKnown) {
                problems += ReportProblem(handler, context, line, "No such section", section);
            }

            continue;
        }

        // An unknown section's lines are its header's problem alone
        if (!isSectionKnown && section.start != NULL) {
            continue;
        }

        equals = memchr(span.start, '=', span.length);
        if (equals == NULL) {
            problems += ReportProblem(handler, context, line, "Not a setting", span);
        }
        else {
            const Span key = TrimSpan((Span) {span.start, (size_t)(equals - span.start)});
            const Span value =
                TrimSpan((Span) {equals + 1, (size_t)(span.start + span.length - equals - 1)});
            const Setting *setting = FindSetting(section, key);

            if (setting == NULL) {
                problems += ReportProblem(handler, context, line, "No such setting", key);
            }
            else if (!ParseSetting(setting, value, config)) {
                problems += ReportProblem(handler, context, line, "Can't use the value", span);
            }
        }
    }

    if (!IsStickShapeValid(&config->bindings.stick)) {
        config->bindings.stick = stickBefore;
        problems += ReportProblem(
            handler,
            context,
            0,
            "The stick's dead zone must be less than its full throw",
            (Span) {"", 0}
        );
    }

    return problems;
}

// --- Writing ---

static void AppendText(TextBuilder *builder, const char *format, ...) SDL_PRINTF_VARARG_FUNC(2);

static void AppendText(TextBuilder *builder, const char *format, ...) {
    va_list arguments;
    int length;

    if (builder->text == NULL) {
        return;
    }

    va_start(arguments, format);
    length = SDL_vsnprintf(
        builder->text + builder->length,
        builder->capacity - builder->length,
        format,
        arguments
    );
    va_end(arguments);
    if (length < 0) {
        SDL_free(builder->text);
        builder->text = NULL;
        return;
    }

    if ((size_t)length >= builder->capacity - builder->length) {
        const size_t capacity = (builder->length + (size_t)length + 1) * 2;
        char *grown = SDL_realloc(builder->text, capacity);

        if (grown == NULL) {
            SDL_free(builder->text);
            builder->text = NULL;
            return;
        }

        builder->text = grown;
        builder->capacity = capacity;
        va_start(arguments, format);
        (void)SDL_vsnprintf(
            builder->text + builder->length,
            builder->capacity - builder->length,
            format,
            arguments
        );
        va_end(arguments);
    }

    builder->length += (size_t)length;
}

// Writes each line of a comment after a "# "
static void AppendComment(TextBuilder *builder, const char *comment) {
    const char *line = comment;

    while (line != NULL && *line != '\0') {
        const char *lineEnd = SDL_strchr(line, '\n');
        const size_t length = (lineEnd != NULL) ? (size_t)(lineEnd - line) : SDL_strlen(line);

        AppendText(builder, "# %.*s\n", (int)length, line);
        line = (lineEnd != NULL) ? lineEnd + 1 : NULL;
    }
}

static const char *NameKey(SDL_Scancode key) {
    size_t i;

    for (i = 0; i < SDL_arraysize(f_KeyAliases); i++) {
        if (f_KeyAliases[i].key == key) {
            return f_KeyAliases[i].name;
        }
    }

    return SDL_GetScancodeName(key);
}

static void AppendPadSource(TextBuilder *builder, PadSource source) {
    switch (source.kind) {
        case PAD_SOURCE_KEY:
            AppendText(builder, "%s", NameKey((SDL_Scancode)source.code));
            break;
        case PAD_SOURCE_BUTTON:
            SDL_assert(source.code < SDL_arraysize(f_ButtonNames));
            AppendText(builder, PAD_PREFIX "%s", f_ButtonNames[source.code]);
            break;
        case PAD_SOURCE_AXIS_PLUS:
        case PAD_SOURCE_AXIS_MINUS:
            SDL_assert(source.code < SDL_arraysize(f_AxisNames));
            AppendText(
                builder,
                PAD_PREFIX "%c%s",
                (source.kind == PAD_SOURCE_AXIS_PLUS) ? '+' : '-',
                f_AxisNames[source.code]
            );
            break;
        default:
            break;
    }
}

// The name of the value that a choice's field holds
static const char *ChoiceName(const char *const *names, const void *field) {
    int index;
    int i;

    SDL_memcpy(&index, field, sizeof(index));
    for (i = 0; names[i] != NULL; i++) {
        if (i == index) {
            return names[i];
        }
    }

    SDL_assert(!"a choice's field holds no name's index");
    return names[0];
}

static void AppendSettingValue(TextBuilder *builder, const Setting *setting, const Config *config) {
    const void *field = ConstSettingField(config, setting);
    int i;

    switch (setting->kind) {
        case SETTING_ROM_PATH:
            AppendText(builder, "%s", (const char *)field);
            break;
        case SETTING_BOOL:
            AppendText(builder, "%s", *(const bool *)field ? "true" : "false");
            break;
        case SETTING_INT:
        case SETTING_WINDOW_SIZE:
            AppendText(builder, "%d", *(const int *)field);
            break;
        case SETTING_WINDOW_POSITION:
            if (config->hasWindowPosition) {
                AppendText(builder, "%d, %d", config->windowX, config->windowY);
            }
            break;
        case SETTING_CHOICE:
            AppendText(builder, "%s", ChoiceName(setting->names, field));
            break;
        case SETTING_CONTROL:
            for (i = 0; i < PAD_SOURCES_PER_CONTROL; i++) {
                const PadSource source = config->bindings.sources[setting->offset][i];

                if (source.kind != PAD_SOURCE_NONE) {
                    AppendText(builder, "%s", (i > 0) ? ", " : "");
                    AppendPadSource(builder, source);
                }
            }
            break;
        default:
            break;
    }
}

char *FormatConfig(const Config *config) {
    TextBuilder builder;
    const char *section = "";
    size_t keyEnd;
    size_t i;

    SDL_assert(config != NULL);
    builder.capacity = 4096;
    builder.length = 0;
    builder.text = SDL_malloc(builder.capacity);
    AppendComment(&builder, f_FileComment);
    for (i = 0; i < SDL_arraysize(f_Settings); i++) {
        const Setting *setting = &f_Settings[i];

        if (SDL_strcmp(setting->section, section) != 0) {
            section = setting->section;
            AppendText(&builder, "\n[%s]\n", section);
            if (setting->kind == SETTING_CONTROL) {
                AppendComment(&builder, f_ControlsComment);
            }
        }

        if (setting->comment != NULL) {
            AppendComment(&builder, setting->comment);
        }

        AppendText(&builder, "%s = ", setting->key);
        keyEnd = builder.length;
        AppendSettingValue(&builder, setting, config);
        // An empty value has no space after its =
        if (builder.text != NULL && builder.length == keyEnd) {
            builder.length--;
        }

        AppendText(&builder, "\n");
    }

    return builder.text;
}
