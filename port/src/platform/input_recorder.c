/*
 * Records the first controller's input as an input script.
 */

#include "input_recorder.h"

#include <SDL3/SDL.h>

#include "input_script.h"

#define LINE_SIZE 128

struct InputRecorder {
    SDL_IOStream *stream;
    OSContPad lastPad;
    long lastFrame; // -1 before the first
    bool hasFailed;
};

// Writes text and passes it on to the system at once
static bool WriteText(InputRecorder *recorder, const char *text) {
    size_t length = SDL_strlen(text);

    if (recorder->hasFailed) {
        return false;
    }

    if (SDL_WriteIO(recorder->stream, text, length) != length || !SDL_FlushIO(recorder->stream)) {
        recorder->hasFailed = true;
    }

    return !recorder->hasFailed;
}

static bool IsSamePad(const OSContPad *a, const OSContPad *b) {
    return a->button == b->button && a->stick_x == b->stick_x && a->stick_y == b->stick_y;
}

InputRecorder *CreateInputRecorder(SDL_IOStream *stream, const char *header) {
    InputRecorder *recorder;

    SDL_assert(stream != NULL && header != NULL);
    recorder = SDL_calloc(1, sizeof(*recorder));
    if (recorder == NULL) {
        SDL_CloseIO(stream);
        return NULL;
    }

    recorder->stream = stream;
    recorder->lastFrame = -1;
    if (!WriteText(recorder, header)) {
        SDL_CloseIO(stream);
        SDL_free(recorder);
        return NULL;
    }

    return recorder;
}

bool RecordInputFrame(InputRecorder *recorder, long frame, const OSContPad *pad) {
    char line[LINE_SIZE];

    SDL_assert(recorder != NULL && pad != NULL && frame > recorder->lastFrame);
    if (recorder->lastFrame < 0 || !IsSamePad(pad, &recorder->lastPad)) {
        // LINE_SIZE holds the longest line: every button and both axes
        bool isFormatted = FormatScriptLine(frame, pad, line, sizeof(line));

        SDL_assert(isFormatted);
        if (isFormatted) {
            (void)WriteText(recorder, line);
        }
    }

    recorder->lastPad = *pad;
    recorder->lastFrame = frame;
    return !recorder->hasFailed;
}

bool CloseInputRecorder(InputRecorder *recorder) {
    char line[LINE_SIZE];
    bool isWhole;

    if (recorder == NULL) {
        return true;
    }

    SDL_snprintf(line, sizeof(line), "# Ends at frame %ld\n", recorder->lastFrame + 1);
    (void)WriteText(recorder, line);
    isWhole = SDL_CloseIO(recorder->stream) && !recorder->hasFailed;
    SDL_free(recorder);
    return isWhole;
}
