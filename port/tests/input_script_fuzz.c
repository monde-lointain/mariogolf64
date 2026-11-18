/*
 * Fuzzes the input scripts' parser. Whatever the text, the parser gives a
 * script or an error that fits its buffer. A script holds, from each line's
 * frame, a pad of the buttons and stick a script can name, which the frame
 * before held only as the line before's; and FormatScriptLine writes that
 * pad as a line that the parser reads back the same.
 */

#include <SDL3/SDL.h>
#include <ultra64.h>

#include "fuzz_target.h"
#include "input_script.h"

// The error buffer's size varies with the input's, up to this
#define ERROR_SIZE_LIMIT 64
// Fills the error buffer past the size given, to show a write there
#define CANARY 0x5A
#define SCRIPT_BUTTONS \
    (A_BUTTON | B_BUTTON | Z_TRIG | START_BUTTON | L_TRIG | R_TRIG | U_CBUTTONS | D_CBUTTONS | \
        L_CBUTTONS | R_CBUTTONS | U_JPAD | D_JPAD | L_JPAD | R_JPAD)
#define STICK_LIMIT 80
#define OUT_OF_MEMORY "out of memory"
#define LINE_PREFIX "line "

static bool IsSamePad(const OSContPad *a, const OSContPad *b) {
    return a->button == b->button && a->stick_x == b->stick_x && a->stick_y == b->stick_y;
}

// The pad's line fits in its length and a NUL; in less, writing it fails,
// leaving the start of the line within the space given; and it reads back
// alone as the pad from its frame
static void CheckWrittenLine(long frame, const OSContPad *pad) {
    char line[96];
    char shortLine[96];
    char error[ERROR_SIZE_LIMIT];
    size_t length;
    size_t lineSize;
    InputScript *script;
    OSContPad readBack;

    FUZZ_CHECK(FormatScriptLine(frame, pad, line, sizeof(line)));
    length = SDL_strlen(line);
    FUZZ_CHECK(length > 0 && line[length - 1] == '\n');
    for (lineSize = 1; lineSize <= length; lineSize++) {
        FUZZ_CHECK(!FormatScriptLine(frame, pad, shortLine, lineSize));
        FUZZ_CHECK(SDL_strnlen(shortLine, lineSize) < lineSize);
        FUZZ_CHECK(SDL_strncmp(shortLine, line, SDL_strlen(shortLine)) == 0);
    }

    FUZZ_CHECK(FormatScriptLine(frame, pad, shortLine, length + 1));
    FUZZ_CHECK(SDL_strcmp(shortLine, line) == 0);
    script = ParseInputScript(line, error, sizeof(error));
    FUZZ_CHECK(script != NULL);
    FUZZ_CHECK(!FindScriptedPad(script, frame - 1, &readBack));
    FUZZ_CHECK(FindScriptedPad(script, frame, &readBack) && IsSamePad(&readBack, pad));
    FreeInputScript(script);
}

// Checks the script against the text's lines, split as the parser splits
// them; the text is changed
static void CheckScript(const InputScript *script, char *text) {
    char *line = text;
    long lastFrame = -1;
    OSContPad lastPad;

    SDL_zero(lastPad);
    while (line != NULL) {
        char *next = SDL_strchr(line, '\n');
        char *comment;
        char *state = NULL;
        char *first;

        if (next != NULL) {
            *next++ = '\0';
        }

        comment = SDL_strchr(line, '#');
        if (comment != NULL) {
            *comment = '\0';
        }

        first = SDL_strtok_r(line, " \t\r", &state);
        if (first != NULL) {
            long frame = (long)SDL_strtoll(first, NULL, 10);
            OSContPad pad;
            OSContPad before;

            FUZZ_CHECK(frame > lastFrame);
            FUZZ_CHECK(FindScriptedPad(script, frame, &pad));
            FUZZ_CHECK((pad.button & ~SCRIPT_BUTTONS) == 0);
            FUZZ_CHECK(pad.stick_x >= -STICK_LIMIT && pad.stick_x <= STICK_LIMIT);
            FUZZ_CHECK(pad.stick_y >= -STICK_LIMIT && pad.stick_y <= STICK_LIMIT);
            if (lastFrame < 0) {
                FUZZ_CHECK(!FindScriptedPad(script, frame - 1, &before));
            }
            else {
                FUZZ_CHECK(FindScriptedPad(script, frame - 1, &before));
                FUZZ_CHECK(IsSamePad(&before, &lastPad));
            }

            CheckWrittenLine(frame, &pad);
            lastFrame = frame;
            lastPad = pad;
        }

        line = next;
    }
}

// The error, cut to its buffer, names a line or says memory ran out
static bool IsErrorMessage(const char *error) {
    size_t length = SDL_strlen(error);

    return SDL_strncmp(error, LINE_PREFIX, SDL_min(length, SDL_strlen(LINE_PREFIX))) == 0 ||
        SDL_strncmp(error, OUT_OF_MEMORY, length) == 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char error[ERROR_SIZE_LIMIT + 16];
    const size_t errorSize = 1 + (size % ERROR_SIZE_LIMIT);
    char *text = SDL_malloc(size + 1);
    InputScript *script;
    size_t i;

    if (text == NULL) {
        return 0;
    }

    SDL_memcpy(text, data, size);
    text[size] = '\0';
    SDL_memset(error, CANARY, sizeof(error));
    script = ParseInputScript(text, error, errorSize);
    FUZZ_CHECK(SDL_strnlen(error, errorSize) < errorSize);
    for (i = errorSize; i < sizeof(error); i++) {
        FUZZ_CHECK(error[i] == CANARY);
    }

    if (script == NULL) {
        FUZZ_CHECK(errorSize == 1 || error[0] != '\0');
        FUZZ_CHECK(IsErrorMessage(error));
    }
    else {
        FUZZ_CHECK(error[0] == '\0');
        CheckScript(script, text);
        FreeInputScript(script);
    }

    SDL_free(text);
    return 0;
}
