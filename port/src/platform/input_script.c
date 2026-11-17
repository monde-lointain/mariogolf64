/*
 * Scripted controller input: the parser and the lookup.
 */

#include "input_script.h"

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_stdinc.h>

#define STICK_LIMIT 80
#define STICK_PREFIX "stick="

// A line of the script: the pad that it holds from a frame
typedef struct ScriptStep {
    long frame;
    OSContPad pad;
} ScriptStep;

struct InputScript {
    ScriptStep *steps;
    size_t stepCount;
};

typedef struct ButtonName {
    const char *name;
    u16 button;
} ButtonName;

static const ButtonName f_ButtonNames[] = {
    {"A", A_BUTTON},
    {"B", B_BUTTON},
    {"Z", Z_TRIG},
    {"START", START_BUTTON},
    {"L", L_TRIG},
    {"R", R_TRIG},
    {"CUP", U_CBUTTONS},
    {"CDOWN", D_CBUTTONS},
    {"CLEFT", L_CBUTTONS},
    {"CRIGHT", R_CBUTTONS},
    {"UP", U_JPAD},
    {"DOWN", D_JPAD},
    {"LEFT", L_JPAD},
    {"RIGHT", R_JPAD},
};

static bool FindButton(const char *name, u16 *button) {
    size_t i;

    for (i = 0; i < SDL_arraysize(f_ButtonNames); i++) {
        if (SDL_strcmp(name, f_ButtonNames[i].name) == 0) {
            *button = f_ButtonNames[i].button;
            return true;
        }
    }

    return false;
}

// Reads a whole word as a number within limits
static bool ReadNumber(const char *word, long low, long high, long *value) {
    char *end;

    *value = SDL_strtol(word, &end, 10);
    return end != word && *end == '\0' && *value >= low && *value <= high;
}

// Reads "X,Y" into the pad's stick
static bool ReadStick(const char *word, OSContPad *pad) {
    char axes[32];
    char *comma;
    long x;
    long y;

    if (SDL_strlcpy(axes, word, sizeof(axes)) >= sizeof(axes)) {
        return false;
    }

    comma = SDL_strchr(axes, ',');
    if (comma == NULL) {
        return false;
    }

    *comma = '\0';
    if (!ReadNumber(axes, -STICK_LIMIT, STICK_LIMIT, &x) ||
        !ReadNumber(comma + 1, -STICK_LIMIT, STICK_LIMIT, &y)) {
        return false;
    }

    pad->stick_x = (s8)x;
    pad->stick_y = (s8)y;
    return true;
}

// Reads a line's words after its frame into a pad, or names the word that
// it can't read
static bool ReadPad(char *words, OSContPad *pad, const char **badWord) {
    char *state = NULL;
    char *word;

    SDL_zerop(pad);
    for (word = SDL_strtok_r(words, " \t\r", &state); word != NULL;
        word = SDL_strtok_r(NULL, " \t\r", &state)) {
        u16 button;

        if (FindButton(word, &button)) {
            pad->button |= button;
        }
        else if (SDL_strncmp(word, STICK_PREFIX, SDL_strlen(STICK_PREFIX)) == 0 &&
            ReadStick(word + SDL_strlen(STICK_PREFIX), pad)) {
            continue;
        }
        else {
            *badWord = word;
            return false;
        }
    }

    return true;
}

// Reads a line without its comment into a step; a blank line gives none
static bool ReadLine(
    char *line,
    long lastFrame,
    ScriptStep *step,
    bool *hasStep,
    char *error,
    size_t errorSize,
    int lineNumber
) {
    char *comment = SDL_strchr(line, '#');
    char *state = NULL;
    char *first;
    char *rest;
    const char *badWord = NULL;

    if (comment != NULL) {
        *comment = '\0';
    }

    *hasStep = false;
    first = SDL_strtok_r(line, " \t\r", &state);
    if (first == NULL) {
        return true;
    }

    if (!ReadNumber(first, 0, SDL_MAX_SINT32, &step->frame)) {
        SDL_snprintf(error, errorSize, "line %d: \"%s\" isn't a frame", lineNumber, first);
        return false;
    }

    if (step->frame <= lastFrame) {
        SDL_snprintf(error, errorSize, "line %d: the frames must increase", lineNumber);
        return false;
    }

    rest = state != NULL ? state : "";
    if (!ReadPad(rest, &step->pad, &badWord)) {
        SDL_snprintf(error, errorSize, "line %d: can't read \"%s\"", lineNumber, badWord);
        return false;
    }

    *hasStep = true;
    return true;
}

static size_t CountLines(const char *text) {
    size_t count = 1;

    for (; *text != '\0'; text++) {
        count += *text == '\n';
    }

    return count;
}

InputScript *ParseInputScript(const char *text, char *error, size_t errorSize) {
    InputScript *script;
    char *copy;
    char *line;
    long lastFrame = -1;
    int lineNumber = 0;

    SDL_assert(text != NULL && error != NULL && errorSize != 0);
    error[0] = '\0';
    script = SDL_calloc(1, sizeof(*script));
    copy = SDL_strdup(text);
    if (script != NULL) {
        script->steps = SDL_calloc(CountLines(text), sizeof(*script->steps));
    }

    if (script == NULL || copy == NULL || script->steps == NULL) {
        SDL_snprintf(error, errorSize, "out of memory");
        SDL_free(copy);
        FreeInputScript(script);
        return NULL;
    }

    // Lines are split by hand, as strtok would merge empty ones and miscount
    line = copy;
    while (line != NULL) {
        char *next = SDL_strchr(line, '\n');
        bool hasStep;

        if (next != NULL) {
            *next++ = '\0';
        }

        lineNumber++;
        if (!ReadLine(
                line,
                lastFrame,
                &script->steps[script->stepCount],
                &hasStep,
                error,
                errorSize,
                lineNumber
            )) {
            SDL_free(copy);
            FreeInputScript(script);
            return NULL;
        }

        if (hasStep) {
            lastFrame = script->steps[script->stepCount].frame;
            script->stepCount++;
        }

        line = next;
    }

    SDL_free(copy);
    return script;
}

void FreeInputScript(InputScript *script) {
    if (script != NULL) {
        SDL_free(script->steps);
        SDL_free(script);
    }
}

bool FindScriptedPad(const InputScript *script, long frame, OSContPad *pad) {
    size_t low = 0;
    size_t high;

    SDL_assert(script != NULL && pad != NULL);
    high = script->stepCount;
    // The last step at or before the frame: low ends one past it
    while (low < high) {
        size_t middle = low + ((high - low) / 2);

        if (script->steps[middle].frame <= frame) {
            low = middle + 1;
        }
        else {
            high = middle;
        }
    }

    if (low == 0) {
        return false;
    }

    *pad = script->steps[low - 1].pad;
    return true;
}

// Each piece is added while it fits, so a short line leaves no part word
static bool AppendText(char *line, size_t lineSize, size_t *length, const char *text) {
    size_t textLength = SDL_strlen(text);

    if (*length + textLength >= lineSize) {
        return false;
    }

    SDL_memcpy(line + *length, text, textLength + 1);
    *length += textLength;
    return true;
}

bool FormatScriptLine(long frame, const OSContPad *pad, char *line, size_t lineSize) {
    char word[32];
    size_t length = 0;
    size_t i;

    SDL_assert(frame >= 0 && pad != NULL && line != NULL && lineSize != 0);
    line[0] = '\0';
    SDL_snprintf(word, sizeof(word), "%ld", frame);
    if (!AppendText(line, lineSize, &length, word)) {
        return false;
    }

    for (i = 0; i < SDL_arraysize(f_ButtonNames); i++) {
        if ((pad->button & f_ButtonNames[i].button) != 0 &&
            (!AppendText(line, lineSize, &length, " ") ||
                !AppendText(line, lineSize, &length, f_ButtonNames[i].name))) {
            return false;
        }
    }

    if (pad->stick_x != 0 || pad->stick_y != 0) {
        SDL_snprintf(word, sizeof(word), " " STICK_PREFIX "%d,%d", pad->stick_x, pad->stick_y);
        if (!AppendText(line, lineSize, &length, word)) {
            return false;
        }
    }

    return AppendText(line, lineSize, &length, "\n");
}
