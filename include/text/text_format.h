/*
 * Message IDs, and the flags and font sizes that text slots and the text
 * routines take. They are apart from text.h for a file that declares its text
 * routines itself.
 */

#ifndef TEXT_FORMAT_H
#define TEXT_FORMAT_H

#include <ultra64.h>

// An index into the game's messages, or NO_MESSAGE
typedef s32 MessageId;
typedef s16 MessageId16;
// Given this as its message, SetTextSlot clears the slot
#define NO_MESSAGE (-1)

// Flags of a text slot, and of the routines that draw text
typedef s32 TextFlags;
typedef u8 UTextFlags8;
// The message appears one code more each time the slot is drawn
#define TEXT_FLAG_TYPED 0x01
// A typed message plays a sound every few codes
#define TEXT_FLAG_TYPING_SOUND 0x02
#define TEXT_FLAG_CENTERED 0x10
// DrawTextSlots skips a slot with this flag; its owner draws it itself with
// DrawLargeTextSlot
#define TEXT_FLAG_DRAWN_BY_OWNER 0x20
// Widens the advance of the fonts drawn in text slots
#define TEXT_FLAG_WIDE_SPACING 0x40
// Each glyph of the large font takes the same width
#define TEXT_FLAG_FIXED_WIDTH 0x80

// MeasureText flag: use each character's own width rather than the font size
#define MEASURE_TEXT_PROPORTIONAL 0x02
// MeasureText only tests for MEASURE_TEXT_PROPORTIONAL, so this
// measures each character at the font size
#define MEASURE_TEXT_FIXED_WIDTH 0x01

// The font size, in pixels, of the font whose characters differ in width
#define FONT_SIZE_LARGE 12
#define FONT_SIZE_MEDIUM 10
#define FONT_SIZE_SMALL 8
// A text slot draws in the medium font at FONT_SIZE_MEDIUM and in the large
// font at any other size; most callers pass this one
#define TEXT_SLOT_LARGE_FONT 1
#define MESSAGE_FIRST_RING_SHOT_HOLE 0x4B
#define MESSAGE_HOLE_NUMBER 0x76
// The first club tournament's name; g_ClubTournamentNameIndex picks the tournament
#define MESSAGE_FIRST_CLUB_TOURNAMENT_NAME 0x16F

#endif
