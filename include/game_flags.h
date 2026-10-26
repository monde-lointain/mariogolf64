#ifndef GAME_FLAGS_H
#define GAME_FLAGS_H

#include <ultra64.h>

/*
 * A set of 256 switches, numbered from 0, which boot clears. Game code
 * sets and clears some of them as it runs, such as 0x07 and UNK1E. A flag
 * editor in the code at 1B4A0 sets or clears any of them; the flags that
 * nothing else sets, such as HIDE_BILLBOARDS, are debugging features.
 */

#define GAME_FLAG_COUNT 256

/* Prints the paths that the collision tests are given */
#define GAME_FLAG_UNK0 0x00

/* Logs a stroke's rolling ball's vertical velocity each step */
#define GAME_FLAG_LOG_VERTICAL_VELOCITY 0x02

/* Blanks two lines of debug text when the flight camera finishes */
#define GAME_FLAG_UNK22 0x22

/* Adds a ring at a fixed place, once each time the flag is set */
#define GAME_FLAG_TEST_RING 0x25

/* Lets controller 2 edit the sky colors and fog, and draws the fog from g_DebugFog */
#define GAME_FLAG_SKY_EDITOR 0x2D

/* Hides the billboards */
#define GAME_FLAG_HIDE_BILLBOARDS 0x0E

/* Makes every cell of the billboards' hit masks read as 1 */
#define GAME_FLAG_FILL_BILLBOARD_MASKS 0x0A

/* Lets a rising ball, whose course y falls, hit the flag */
#define GAME_FLAG_RISING_FLAG_HITS 0x4E

/* Lets the ball hit walls outside mini-golf */
#define GAME_FLAG_WALL_HITS 0x3A

/* Silences the overlay manager's log */
#define GAME_FLAG_QUIET_OVERLAY_LOG 0x4D

/* Lets the second and fourth controllers open and close the inset view */
#define GAME_FLAG_INSET_VIEW_BUTTONS 0x2F

/* Shows and edits UpdateGame's sDebugMenuValues in one scene */
#define GAME_FLAG_DEBUG_MENU_VALUES 0x57

/* Makes every character selectable */
#define GAME_FLAG_ALL_CHARACTERS 0x54

/* Opens every course on the hole and Ring Shot selects */
#define GAME_FLAG_ALL_COURSES 0x5E

/* Stops the debug text grid from being drawn */
#define GAME_FLAG_HIDE_DEBUG_TEXT 0x1D

/* Set when a saved game is continued, among other places */
#define GAME_FLAG_UNK1E 0x1E

/* Makes InitClubs generate every club's parameters, then call WriteClubParameters */
#define GAME_FLAG_GENERATE_CLUBS 0x1F

/* Logs the ID of each encoded asset that is opened */
#define GAME_FLAG_LOG_ENCODED_ASSETS 0x49

/* Cuts every round to two holes */
#define GAME_FLAG_TWO_HOLE_ROUNDS 0x3D
/* Logs LoadCharacter's progress */
#define GAME_FLAG_LOG_CHARACTER_SETUP 0x41
/* Measures ball distances from the character models instead of reading them from a table */
#define GAME_FLAG_GENERATE_BALL_DISTANCES 0x36
/* Prints each character slot's animation frame */
#define GAME_FLAG_SHOW_ANIMATION_FRAMES 0x38
/* Prints LaunchBallSimulation's results for a stroke */
#define GAME_FLAG_SHOW_LAUNCH_VALUES 0x08
/* Shows DrawColorTest's screen */
#define GAME_FLAG_COLOR_TEST 0x46

/* Among other uses, makes EndHoleFlyover act as it does for a strong wind */
#define GAME_FLAG_UNK48 0x48

/* Turns the character sound queue into a delay editor, among other uses */
#define GAME_FLAG_UNK5F 0x5F

/* Stops the noise texture from changing */
#define GAME_FLAG_PAUSE_NOISE_TEXTURE 0x0C

/* Makes every club usable on every surface */
#define GAME_FLAG_ANY_CLUB_ANYWHERE 0x98

/* Keeps sound effects from starting */
#define GAME_FLAG_SILENCE_SOUND_EFFECTS 0x9C

/* Keeps songs from starting */
#define GAME_FLAG_SILENCE_SONGS 0x9D

/* Cleared while the loader thread loads a hole, and set once it is done */
#define GAME_FLAG_HOLE_LOADED 0x07

/* Halts the loader thread after it sets up the hole's state */
#define GAME_FLAG_HALT_HOLE_LOADER 0x43

/* Flags that the hole loader reads or sets, whose purpose is not yet known */
#define GAME_FLAG_UNK11 0x11
#define GAME_FLAG_UNK12 0x12
#define GAME_FLAG_UNK51 0x51
#define GAME_FLAG_UNK60 0x60
#define GAME_FLAG_UNK69 0x69
#define GAME_FLAG_UNK6A 0x6A
#define GAME_FLAG_UNK6B 0x6B
#define GAME_FLAG_UNK7C 0x7C
#define GAME_FLAG_UNK7D 0x7D

/* Has BuildHole read the hole's files from the host instead of its assets */
#define GAME_FLAG_READ_HOST_HOLE 0x50
/* Has BuildHole keep the hole already in memory once instead of reading the host files */
#define GAME_FLAG_SKIP_HOST_HOLE_READ 0x52

/* Prints the frame number of the golfer's reaction */
#define GAME_FLAG_SHOW_REACTION_FRAME 0x3E

/* Lets the four C buttons on controller 1 dump the cartridge RAM from the Transmit menu */
#define GAME_FLAG_ALLOW_CARTRIDGE_DUMP 0x7E

/* Read by the reaction framing, whose purpose is not yet known */
#define GAME_FLAG_UNK7F 0x7F

/* Keeps the staff roll from shuffling its cast */
#define GAME_FLAG_UNK99 0x99

/* Set by the staff roll's timeline as it starts */
#define GAME_FLAG_UNK9A 0x9A

/* Flags that Club Slots reads, whose purpose is not yet known */
#define GAME_FLAG_UNK32 0x32
#define GAME_FLAG_UNK55 0x55

/* Makes ChooseHoleStrokeTarget plan no strokes for each hole; its purpose is not yet known */
#define GAME_FLAG_UNK3C 0x3C

/* Sets unk1A of each Game Boy character read through the Transfer Pak to 1; its purpose is not yet known */
#define GAME_FLAG_UNK6F 0x6F

/* Give Game Boy characters read through the Transfer Pak that ball type, the lowest set flag winning */
#define GAME_FLAG_BALL_TYPE_1 0x88
#define GAME_FLAG_BALL_TYPE_2 0x89
#define GAME_FLAG_BALL_TYPE_3 0x8A
#define GAME_FLAG_BALL_TYPE_4 0x8B
#define GAME_FLAG_BALL_TYPE_5 0x8C
#define GAME_FLAG_BALL_TYPE_6 0x8D
#define GAME_FLAG_BALL_TYPE_7 0x8E

s32 IsGameFlagSet(u32 flag);
void SetGameFlag(u32 flag);
void ClearGameFlag(u32 flag);
void ToggleGameFlag(u32 flag);
void ClearAllGameFlags(void);

#endif
