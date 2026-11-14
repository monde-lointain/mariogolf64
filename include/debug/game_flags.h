/*
 * A set of 256 switches, numbered from 0, which boot clears. Game code
 * sets and clears some of them as it runs, such as HOLE_LOADED and LOADED_PLAY.
 * A flag editor in the code at 1B4A0 sets or clears any of them; the flags that
 * nothing else sets, such as HIDE_BILLBOARDS, are debugging features.
 */

#ifndef GAME_FLAGS_H
#define GAME_FLAGS_H

#include <ultra64.h>

#include "system/c_language.h"

#define GAME_FLAG_COUNT 256
// The flags are stored packed, eight to a byte
#define GAME_FLAGS_PER_BYTE 8
#define GAME_FLAG_BYTE_COUNT (GAME_FLAG_COUNT / GAME_FLAGS_PER_BYTE)

// Prints the paths that the collision tests are given
#define GAME_FLAG_PRINT_HIT_PATHS 0x00

// Logs a stroke's rolling ball's vertical velocity each step
#define GAME_FLAG_LOG_VERTICAL_VELOCITY 0x02

// Lets controller 1's C-left and C-right tilt the aim camera, and controller
// 2's A and B play the drive record movie and movie 15
#define GAME_FLAG_TEST_AIM_TILT_AND_MOVIES 0x21

// Blanks two lines of debug text when the flight camera finishes
#define GAME_FLAG_BLANK_TEXT_AFTER_FLIGHT 0x22

// Adds a ring at a fixed place, once each time the flag is set
#define GAME_FLAG_TEST_RING 0x25

// Lets controller 1's C-up and C-down cycle isRaining and the scene color index
#define GAME_FLAG_CYCLE_SKY_CONDITIONS 0x20

// Lets controller 2 edit the sky colors and fog, and draws the fog from
// g_DebugFog. It also hides the aim guides and the round HUD.
#define GAME_FLAG_SKY_EDITOR 0x2D

// Hides the billboards
#define GAME_FLAG_HIDE_BILLBOARDS 0x0E

// Makes every cell of the billboards' hit masks read as 1
#define GAME_FLAG_FILL_BILLBOARD_MASKS 0x0A

// Lets a rising ball, whose course y falls, hit the flag
#define GAME_FLAG_RISING_FLAG_HITS 0x4E

// Lets every player's hole-outs, and those in Training, enter the records
#define GAME_FLAG_RECORD_ALL_HOLE_OUTS 0x3B

// Lets the ball hit walls outside mini-golf
#define GAME_FLAG_WALL_HITS 0x3A

// Silences the overlay manager's log
#define GAME_FLAG_QUIET_OVERLAY_LOG 0x4D

// Prints the opening's ball speed in km/h each step, down a column of eight
// rows
#define GAME_FLAG_SHOW_OPENING_BALL_SPEED 0x09
// Prints how far the opening's ball is above where it was struck, in yards
#define GAME_FLAG_SHOW_OPENING_BALL_HEIGHT 0x0B
// Lets the second and fourth controllers' D-pad open and close the inset
// view, the second's C buttons start screen effects, and its Z and D-pad
// right call CaptureFrame and CaptureMapImage
#define GAME_FLAG_SCREEN_TEST_BUTTONS 0x2F

// Shows and edits UpdateGame's f_DebugMenuValues in one scene
#define GAME_FLAG_DEBUG_MENU_VALUES 0x57

// Makes every character selectable
#define GAME_FLAG_ALL_CHARACTERS 0x54

// Keeps a saved game in the save data once it is continued
#define GAME_FLAG_KEEP_SAVED_GAMES 0x5D

// Opens every course on the hole and Ring Shot selects
#define GAME_FLAG_ALL_COURSES 0x5E

// Open at least this many courses on the main menu, whatever the Course Points
#define GAME_FLAG_OPEN_TWO_COURSES 0x58
#define GAME_FLAG_OPEN_THREE_COURSES 0x59
#define GAME_FLAG_OPEN_FOUR_COURSES 0x5A
#define GAME_FLAG_OPEN_FIVE_COURSES 0x5B
#define GAME_FLAG_OPEN_SIX_COURSES 0x5C

// Stops the debug text grid from being drawn
#define GAME_FLAG_HIDE_DEBUG_TEXT 0x1D

// Read by no routine; set when play starts from a saved game being continued
// or from a Ring Shot or Best Shot replay, and cleared on entering the Best
// Shots screen
#define GAME_FLAG_LOADED_PLAY 0x1E

// Makes InitClubs generate every club's parameters, then call
// WriteClubParameters
#define GAME_FLAG_GENERATE_CLUBS 0x1F

// Logs the ID of each encoded asset that is opened
#define GAME_FLAG_LOG_ENCODED_ASSETS 0x49

// Lets the player with the fewest strokes play next, instead of the farthest
// from the cup
#define GAME_FLAG_FEWEST_STROKES_FIRST 0x3F
// Draws through two or three framebuffers at random, while aiming. It also
// prints the water probe's ratio as GetMiniGolfSurface tests for water;
// the ball position readout sets it around its GetSurface call to show that.
#define GAME_FLAG_RANDOM_FRAMEBUFFER_COUNT 0x9F
// Cuts every round to two holes
#define GAME_FLAG_TWO_HOLE_ROUNDS 0x3D
// Logs LoadCharacter's progress
#define GAME_FLAG_LOG_CHARACTER_SETUP 0x41
// Measures ball distances from the character models instead of reading them
// from a table
#define GAME_FLAG_GENERATE_BALL_DISTANCES 0x36
// Prints each character slot's animation frame
#define GAME_FLAG_SHOW_ANIMATION_FRAMES 0x38
// Prints LaunchBallSimulation's results for a stroke
#define GAME_FLAG_SHOW_LAUNCH_VALUES 0x08
// Shows DrawColorTest's screen
#define GAME_FLAG_COLOR_TEST 0x46

// Brings on flourishes their conditions would hold back: EndHoleFlyover shows
// the wind's Fly Guy as for a strong wind, a shot from the green that drops
// plays SOUND_LONG_PUTT_HOLED however short it was, and a shot that drops
// in the cup view with the pin out plays the Goomba movie for any score from
// eagle to bogey, whatever the shot mode
#define GAME_FLAG_FORCE_FLOURISHES 0x48

// Turns the character sound queue into a delay editor, and with debugging
// enabled prints the flight camera's kind, phase and change timing
#define GAME_FLAG_TIMING_DEBUG 0x5F

// Stops the noise texture from changing
#define GAME_FLAG_PAUSE_NOISE_TEXTURE 0x0C

// Makes every club usable on every surface
#define GAME_FLAG_ANY_CLUB_ANYWHERE 0x98

// Keeps sound effects from starting
#define GAME_FLAG_SILENCE_SOUND_EFFECTS 0x9C

// Keeps songs from starting
#define GAME_FLAG_SILENCE_SONGS 0x9D

// Prints how long DrawGround took, in units of GROUND_TIMING_TICKS
#define GAME_FLAG_TIME_GROUND 0x04

// Keeps DrawGround from drawing the ground
#define GAME_FLAG_HIDE_GROUND 0x0D

// Has DrawHoleEditorGround draw in G_PM_NPRIMITIVE pipeline mode
#define GAME_FLAG_EDITOR_GROUND_NPRIMITIVE 0x05

// Shows the ground's clip ratio, which the second controller's A button cycles
#define GAME_FLAG_CYCLE_CLIP_RATIO 0x10

// Rebuilds the ground texture coordinates every frame and tries other noise
// texture scales
#define GAME_FLAG_TEST_GROUND_TEXTURE 0x27

// Logs the detail of each fine row of ground tiles, and has the rumble
// manager removed on leaving the flag panel
#define GAME_FLAG_LOG_GROUND_ROWS 0x9E

// Cleared while the loader thread loads a hole, and set once it is done
#define GAME_FLAG_HOLE_LOADED 0x07

// Halts the loader thread after it sets up the hole's state
#define GAME_FLAG_HALT_HOLE_LOADER 0x43

// Ends the round after the current hole
#define GAME_FLAG_END_ROUND_AFTER_HOLE 0x44

// Lets the second controller's L and R pick the cup position the hole intro
// circles
#define GAME_FLAG_CHOOSE_CUP 0x47

// Starts the round over from its first hole instead of ending it
#define GAME_FLAG_REPEAT_ROUND 0x4F

// Shows the staff roll after the hole even if the final challenge is not won
#define GAME_FLAG_SHOW_STAFF_ROLL 0x65

// Keeps the menus, as the exit screen leaves them, from swapping to the play
// module; they read their resources again instead
#define GAME_FLAG_STAY_IN_MENUS 0x2E

// How ShadeGroundTiles fades the ground above height 0 toward white
#define GAME_FLAG_FADE_HIGH_GROUND 0x12
#define GAME_FLAG_FADE_HIGH_GROUND_FULLY 0x60
#define GAME_FLAG_NO_HIGH_GROUND_FADE 0x69

// Has the character select show each character's alternate face animation
#define GAME_FLAG_ALTERNATE_SELECT_FACES 0x64

// Has the main menu's help choice open the animation viewer in place of the
// help board, set up for two players
#define GAME_FLAG_HELP_ANIMATION_VIEWER 0x56

// Replaces a round's first hole with the next course's first hole, which
// BuildHole reads past the end of the course's row of hole assets, and fades
// the high ground on it
#define GAME_FLAG_NEXT_COURSE_FIRST_HOLE 0x51

// Has PrepareHoleScene skip ShadeGround, leaving the ground tiles unshaded
// and without the billboards' shadows
#define GAME_FLAG_UNSHADED_GROUND 0x11

// Lets R with A give the character select's last player to the computer even
// when no other player has a controller
#define GAME_FLAG_ALLOW_ALL_COMPUTER_PLAYERS 0x67

// Has every hole use the driving range's scene colors, without rain
#define GAME_FLAG_DRIVING_RANGE_COLORS 0x7C
// Has every hole use the round end's scene colors, without rain, overriding
// GAME_FLAG_DRIVING_RANGE_COLORS
#define GAME_FLAG_ROUND_END_COLORS 0x7D

// Debug cheats that pad the first player's round points
#define GAME_FLAG_ADD_500_POINTS 0x66
#define GAME_FLAG_ADD_9999_POINTS 0x6C
// Lets the match results choose a character with the D-Pad and show its unlock
// notice with B
#define GAME_FLAG_CHOOSE_UNLOCK_NOTICE 0x6E

// Each moves the paint logic of a hole read from the host on by one, as to
// a mini golf course's next
#define GAME_FLAG_NEXT_HOST_PAINT_LOGIC_A 0x6A
#define GAME_FLAG_NEXT_HOST_PAINT_LOGIC_B 0x6B

// Has BuildHole read the hole's files from the host instead of its assets
#define GAME_FLAG_READ_HOST_HOLE 0x50
// Has BuildHole keep the hole already in memory once instead of reading the
// host files
#define GAME_FLAG_SKIP_HOST_HOLE_READ 0x52

// Prints the frame number of the golfer's reaction
#define GAME_FLAG_SHOW_REACTION_FRAME 0x3E

// Lets the four C buttons on controller 1 dump the cartridge RAM from the
// Transmit menu
#define GAME_FLAG_ALLOW_CARTRIDGE_DUMP 0x7E

// Has the reaction views drop the eye and raise what they look at, as in
// sudden death
#define GAME_FLAG_LOW_REACTION_VIEWS 0x7F

// Keeps the staff roll from shuffling its cast
#define GAME_FLAG_UNSHUFFLED_CAST 0x99

// Set by the staff roll's timeline as the cast takes to the course; it has
// SCENE_STAFF_ROLL draw in DRAW_MODE_COURSE. The staff roll never clears it.
#define GAME_FLAG_STAFF_ROLL_ON_COURSE 0x9A

// Flags under which the game plays itself: outside replays, the
// computer golfer plans every shot, and the hole intro, flyover and results
// move on without A. FAST_AUTOPLAY also takes the power gauge's frame from
// g_ScriptedShot, turns on FAST_FLIGHT, skips a HUD animation, shortens the
// wait after the reaction and stops Club Slots' reels at once.
#define GAME_FLAG_FAST_AUTOPLAY 0x32
#define GAME_FLAG_AUTOPLAY 0x9B
// Adds the sound test's choices to the options screen. Club Slots' logo
// also reads it, but to no visible effect
#define GAME_FLAG_OPTIONS_SOUND_TEST 0x55

// Has the computer golfers aim to hole out in no strokes, take 20 trials at
// each level of PlanShot, and keep the evaluation a trial shot must reach
// from loosening with each trial
#define GAME_FLAG_COMPUTER_PLAYS_BEST 0x3C

// Makes each Game Boy character read through the Transfer Pak left-handed,
// whatever its save says
#define GAME_FLAG_GAME_BOY_LEFTIES 0x6F

// Give Game Boy characters read through the Transfer Pak that ball type, the
// lowest set flag winning
#define GAME_FLAG_BALL_TYPE_1 0x88
#define GAME_FLAG_BALL_TYPE_2 0x89
#define GAME_FLAG_BALL_TYPE_3 0x8A
#define GAME_FLAG_BALL_TYPE_4 0x8B
#define GAME_FLAG_BALL_TYPE_5 0x8C
#define GAME_FLAG_BALL_TYPE_6 0x8D
#define GAME_FLAG_BALL_TYPE_7 0x8E

// Lets the second controller edit the club tournament code and skips checking
// it
#define GAME_FLAG_EDIT_TOURNAMENT_CODE 0x90

// Sets the stopwatch to an hour when the scorecard starts a Speed Golf hole's
// tally
#define GAME_FLAG_STOPWATCH_HOUR 0x91

// Stops the opening's script so that the first and third controllers can move
// its camera. The debug build sets it when the third presses A
#define GAME_FLAG_EDIT_OPENING_CAMERA 0x62

Boolean IsGameFlagSet(u32 flag);
void SetGameFlag(u32 flag);
void ClearGameFlag(u32 flag);
void ToggleGameFlag(u32 flag);
void ClearAllGameFlags(void);

// Prints the framebuffer count and the frames drawn each second
#define GAME_FLAG_SHOW_FRAME_RATE 0x01
// Keeps the frame from calling DrawGround; HIDE_GROUND makes it return
#define GAME_FLAG_SKIP_GROUND_DRAWING 0x06
// Steps the flight three times a frame. Each shot clears it, and the shot's
// player's Z or L toggles it.
#define GAME_FLAG_FAST_FLIGHT 0x0F

// Keep the frame from drawing these
#define GAME_FLAG_HIDE_ROUND_HUD 0x13
#define GAME_FLAG_HIDE_BACKDROP 0x15
#define GAME_FLAG_HIDE_PIN 0x17
#define GAME_FLAG_HIDE_GOLF_BALLS 0x18
#define GAME_FLAG_HIDE_CHARACTERS 0x19

// Prints the name of the surface under the shot's targetPoint
#define GAME_FLAG_SHOW_SURFACE_NAME 0x1A
// Lets controller 2's Z toggle HIDE_DEBUG_TEXT
#define GAME_FLAG_DEBUG_TEXT_BUTTON 0x1B
// Prints libmus's last and worst CPU use, and its DMA use
#define GAME_FLAG_SHOW_AUDIO_CPU_USE 0x28
// Prints libmus's free memory, and lets controller 2's B stop the song and
// its A play three sounds
#define GAME_FLAG_AUDIO_TEST 0x29
// Prints the swing's tempo
#define GAME_FLAG_SHOW_SWING_TEMPO 0x2A
// Prints each player's state, in hex
#define GAME_FLAG_SHOW_PLAYER_STATES 0x2C
// Shows the debug log
#define GAME_FLAG_SHOW_DEBUG_LOG 0x39
// Keeps a ball out of bounds or out of course from costing a stroke
#define GAME_FLAG_NO_OUT_OF_BOUNDS_PENALTY 0x45
// Lets controller 2's stick turn a camera framing the ball and its shot
#define GAME_FLAG_FRAME_BALL_AND_SHOT 0x4A
// Draws the ball's afterimages at the hole's route points
#define GAME_FLAG_SHOW_ROUTE_POINTS 0x4C
// Prints each player's Course Points
#define GAME_FLAG_SHOW_COURSE_POINTS 0x68

// InitPlay's second kind of round reads these as two-bit numbers, the first
// flag of each the high bit: the round's kind, which picks Stroke, Training,
// or Skins Match with three players, and Stroke's extra players
#define GAME_FLAG_ROUND_KIND_HIGH_BIT 0x33
#define GAME_FLAG_ROUND_KIND_LOW_BIT 0x34
#define GAME_FLAG_EXTRA_PLAYERS_HIGH_BIT 0x30
#define GAME_FLAG_EXTRA_PLAYERS_LOW_BIT 0x31
// Has InitPlay's first kind of round play Ring Shot instead of Stroke
#define GAME_FLAG_INIT_RING_SHOT 0x35

// Read as a five-bit number, from the high bit down, by
// GetFlaggedFiveBitNumber, which nothing calls
#define GAME_FLAG_FLAGGED_NUMBER_SIXTEENS 0x82
#define GAME_FLAG_FLAGGED_NUMBER_EIGHTS 0x83
#define GAME_FLAG_FLAGGED_NUMBER_FOURS 0x84
#define GAME_FLAG_FLAGGED_NUMBER_TWOS 0x85
#define GAME_FLAG_FLAGGED_NUMBER_ONES 0x86

#endif
