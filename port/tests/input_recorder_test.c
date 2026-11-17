/*
 * Tests the input recorder: the lines that it writes for a session, that
 * the parser reads them back as the frames that the game read, the script
 * lines' writer at its limits, and a recording whose writes fail.
 */

#include <SDL3/SDL.h>
#include <ultra64.h>

#include "input_recorder.h"
#include "input_script.h"
#include "test_support.h"

#define HEADER "# A test\n"

static char f_Text[1024];

static OSContPad MakePad(u16 button, s8 x, s8 y) {
    OSContPad pad;

    SDL_zero(pad);
    pad.button = button;
    pad.stick_x = x;
    pad.stick_y = y;
    return pad;
}

// The frames that the game read in the session, and what it read
static const struct {
    long frame;
    u16 button;
    s8 x;
    s8 y;
} f_Session[] = {
    {0, 0, 0, 0},
    {1, 0, 0, 0},
    {2, A_BUTTON, 0, 0},
    {3, A_BUTTON, 0, 0},
    {4, A_BUTTON, 3, -4},
    {5, A_BUTTON, 3, 5},
    {6, 0, 0, 0},
    {8, START_BUTTON | R_JPAD, 0, 0},
};

// Records the session into f_Text, and tells whether every write went out
static bool RecordSession(size_t space) {
    SDL_IOStream *stream;
    InputRecorder *recorder;
    bool isWhole = true;
    size_t i;

    SDL_zeroa(f_Text);
    stream = SDL_IOFromMem(f_Text, space);
    recorder = CreateInputRecorder(stream, HEADER);
    if (recorder == NULL) {
        return false;
    }

    for (i = 0; i < SDL_arraysize(f_Session); i++) {
        OSContPad pad = MakePad(f_Session[i].button, f_Session[i].x, f_Session[i].y);

        isWhole = RecordInputFrame(recorder, f_Session[i].frame, &pad) && isWhole;
    }

    return CloseInputRecorder(recorder) && isWhole;
}

// A line at the first frame, then one at each change, then the end
static void TestWritesEachChange(void) {
    EXPECT(RecordSession(sizeof(f_Text) - 1));
    EXPECT(
        SDL_strcmp(
            f_Text,
            HEADER "0\n"
                   "2 A\n"
                   "4 A stick=3,-4\n"
                   "5 A stick=3,5\n"
                   "6\n"
                   "8 START RIGHT\n"
                   "# Ends at frame 9\n"
        ) == 0
    );
}

// The recording plays back what the game read at every frame
static void TestPlaysBack(void) {
    char error[64];
    InputScript *script;
    int misses = 0;
    size_t i;

    EXPECT(RecordSession(sizeof(f_Text) - 1));
    script = ParseInputScript(f_Text, error, sizeof(error));
    EXPECT(script != NULL);
    if (script == NULL) {
        return;
    }

    for (i = 0; i < SDL_arraysize(f_Session); i++) {
        OSContPad pad;

        misses += !FindScriptedPad(script, f_Session[i].frame, &pad) ||
            pad.button != f_Session[i].button ||
            pad.stick_x != f_Session[i].x ||
            pad.stick_y != f_Session[i].y;
    }

    EXPECT(misses == 0);
    FreeInputScript(script);
}

// Every button and both axes at their limits, and a line one short of room
static void TestFormatsLimits(void) {
    const u16 every = A_BUTTON | B_BUTTON | Z_TRIG | START_BUTTON | L_TRIG | R_TRIG | U_CBUTTONS |
        D_CBUTTONS | L_CBUTTONS | R_CBUTTONS | U_JPAD | D_JPAD | L_JPAD | R_JPAD;
    const char *expected = "2147483647 A B Z START L R CUP CDOWN CLEFT CRIGHT UP DOWN LEFT "
                           "RIGHT stick=-80,80\n";
    OSContPad pad = MakePad(every, -80, 80);
    char line[128];
    size_t room = SDL_strlen(expected) + 1;

    EXPECT(FormatScriptLine(SDL_MAX_SINT32, &pad, line, sizeof(line)));
    EXPECT(SDL_strcmp(line, expected) == 0);
    EXPECT(FormatScriptLine(SDL_MAX_SINT32, &pad, line, room));
    EXPECT(!FormatScriptLine(SDL_MAX_SINT32, &pad, line, room - 1));

    pad = MakePad(0, 0, 0);
    EXPECT(FormatScriptLine(0, &pad, line, sizeof(line)) && SDL_strcmp(line, "0\n") == 0);
    pad = MakePad(0, 0, -1);
    EXPECT(
        FormatScriptLine(9, &pad, line, sizeof(line)) && SDL_strcmp(line, "9 stick=0,-1\n") == 0
    );
}

// A stream whose writes and flushes count, with one of each set to fail
typedef struct FakeStream {
    int writeCount;
    int flushCount;
    int failingWrite; // Counted from 1; 0 for none
    int failingFlush;
} FakeStream;

static size_t WriteFake(void *data, const void *bytes, size_t size, SDL_IOStatus *status) {
    FakeStream *fake = data;

    (void)bytes;
    if (++fake->writeCount == fake->failingWrite) {
        *status = SDL_IO_STATUS_ERROR;
        return 0;
    }

    return size;
}

static bool FlushFake(void *data, SDL_IOStatus *status) {
    FakeStream *fake = data;

    if (++fake->flushCount == fake->failingFlush) {
        *status = SDL_IO_STATUS_ERROR;
        return false;
    }

    return true;
}

static bool CloseFake(void *data) {
    (void)data;
    return true;
}

static SDL_IOStream *OpenFake(FakeStream *fake) {
    SDL_IOStreamInterface interface;

    SDL_INIT_INTERFACE(&interface);
    interface.write = WriteFake;
    interface.flush = FlushFake;
    interface.close = CloseFake;
    return SDL_OpenIO(&interface, fake);
}

// Records a frame that changes the pad, so that it writes a line
static bool RecordChange(InputRecorder *recorder, long frame) {
    OSContPad pad = MakePad((frame % 2) != 0 ? A_BUTTON : 0, 0, 0);

    return RecordInputFrame(recorder, frame, &pad);
}

// From a write or flush that fails, every frame reports it and nothing more
// goes out, though the stream would take it, and the close reports it too
static void TestReportsFailedWrites(void) {
    FakeStream fake = {0, 0, 3, 0};
    InputRecorder *recorder = CreateInputRecorder(OpenFake(&fake), HEADER);

    EXPECT(recorder != NULL);
    EXPECT(RecordChange(recorder, 0));
    EXPECT(!RecordChange(recorder, 1));
    EXPECT(!RecordChange(recorder, 2));
    EXPECT(fake.writeCount == 3);
    EXPECT(!CloseInputRecorder(recorder));
    EXPECT(fake.writeCount == 3);

    fake = (FakeStream) {0, 0, 0, 2};
    recorder = CreateInputRecorder(OpenFake(&fake), HEADER);
    EXPECT(recorder != NULL);
    EXPECT(!RecordChange(recorder, 0));
    EXPECT(!CloseInputRecorder(recorder));
    EXPECT(fake.writeCount == 2);
}

// The start fails if the header can't go out
static void TestReportsFailedHeader(void) {
    FakeStream fake = {0, 0, 1, 0};

    EXPECT(CreateInputRecorder(OpenFake(&fake), HEADER) == NULL);
    fake = (FakeStream) {0, 0, 0, 1};
    EXPECT(CreateInputRecorder(OpenFake(&fake), HEADER) == NULL);
}

int main(void) {
    TestWritesEachChange();
    TestPlaysBack();
    TestFormatsLimits();
    TestReportsFailedWrites();
    TestReportsFailedHeader();
    return ReportTestResults("input_recorder_test");
}
