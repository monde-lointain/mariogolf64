/*
 * Scripted controller input, for runs without a player: tests that reach
 * the game's menus and play, and runs that must repeat exactly.
 *
 * A script is text, a line for each change of the first port's controller:
 *
 *     # Comments run from a hash to the line's end
 *     120 START
 *     126
 *     400 A stick=0,80
 *
 * A line starts with the game's frame, counted from the first, from which
 * the controller holds what the line names until the next line: buttons, from
 * A B Z START L R CUP CDOWN CLEFT CRIGHT UP DOWN LEFT RIGHT, and the stick,
 * each axis from -80 to 80 with up positive. A line naming nothing lets
 * everything go. The frames must increase from line to line.
 *
 * The game reads the controller once a frame, however many retraces the
 * frame takes, so a script plays the same on any host at any pace.
 */

#ifndef PORT_PLATFORM_INPUT_SCRIPT_H
#define PORT_PLATFORM_INPUT_SCRIPT_H

#include <stdbool.h>
#include <stddef.h>
#include <ultra64.h>

typedef struct InputScript InputScript;

// Reads a script's text, or returns NULL with what was wrong, and on which
// line, in error
InputScript *ParseInputScript(const char *text, char *error, size_t errorSize);
void FreeInputScript(InputScript *script);

// The controller's state that the script holds at a frame; false before
// its first line, when the player's devices play
bool FindScriptedPad(const InputScript *script, long frame, OSContPad *pad);

// Writes the script's line that holds the pad from a frame, with its line
// break, as ParseInputScript reads it back; false if the line doesn't fit
bool FormatScriptLine(long frame, const OSContPad *pad, char *line, size_t lineSize);

#endif
