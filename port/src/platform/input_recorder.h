/*
 * The player's input, recorded as an input script that plays it back: a
 * line for each frame at which the first controller changes, as
 * input_script.h describes, so that a session played by hand replays
 * headless.
 *
 * Each line goes out as soon as it is made, so a recording cut short by a
 * crash keeps everything up to the crash.
 */

#ifndef PORT_PLATFORM_INPUT_RECORDER_H
#define PORT_PLATFORM_INPUT_RECORDER_H

#include <SDL3/SDL_iostream.h>
#include <stdbool.h>
#include <ultra64.h>

typedef struct InputRecorder InputRecorder;

// Starts a recording into the stream, which the recorder then owns, with
// the header's text first. The header must be comment lines. Gives NULL,
// closing the stream, if it can't start.
InputRecorder *CreateInputRecorder(SDL_IOStream *stream, const char *header);

// Takes in the pad that the game read at a frame, writing a line if it
// differs from the last frame's. The frames must increase. False once a
// write has failed, and from then on nothing more is written.
bool RecordInputFrame(InputRecorder *recorder, long frame, const OSContPad *pad);

// Notes the frame that the recording ends at and closes it; false if any
// write failed. NULL is ignored.
bool CloseInputRecorder(InputRecorder *recorder);

#endif
