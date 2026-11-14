/*
 * What the menus remember between screens and rounds: the choices made, the bet
 * settings and the players.
 */

#ifndef MENU_MEMORY_H
#define MENU_MEMORY_H

#include <ultra64.h>

#include "characters/character.h"
#include "course/course.h"
#include "round/bets.h"
#include "round/player.h"
#include "system/c_language.h"

// The values of betStakeLevels, which ApplyBetPreset also turns into
// GameState's betRates: one past the BET_COUNT bets
#define BET_STAKE_SLOT_COUNT (BET_COUNT + 1)

// The presets of ApplyBetPreset; two of them have a variant that four players
// take instead
#define BET_PRESET_COUNT 7
#define BET_PRESET_STROKE 0
#define BET_PRESET_STROKE_FOUR_PLAYER 1
#define BET_PRESET_MATCH_GAME 2
#define BET_PRESET_SKINS_MATCH 3
#define BET_PRESET_CLUB_SLOTS 4
#define BET_PRESET_MINI_GOLF 5
#define BET_PRESET_MINI_GOLF_FOUR_PLAYER 6
// Past ApplyBetPreset's presets, so that it clears the values instead
#define BET_PRESET_NONE 10

// What holesChoice holds once the C buttons choose the holes to play
enum HolesChoice {
    // Never stored, only cleared: ResetMenuChoices sets HOLES_CHOICE_ALL right
    // after clearing g_MenuMemory, and SetUpChosenGame reads it as all holes
    HOLES_CHOICE_UNUSED,
    HOLES_CHOICE_FRONT_NINE,
    HOLES_CHOICE_BACK_NINE,
    HOLES_CHOICE_ALL
};

typedef s8 HolesChoice;

// What the menus keep between visits, such as the last choices made
typedef struct MenuMemory {
    s8 playerCountChoice; // The title screen's player count, less one
    s8 titleChoice; // The title screen's row: a new game, the saved games or the menus
    CharacterIndex8 viewerCharacter; // The animation viewer's character
    s8 characterColumns[PLAYER_COUNT_MAX]; // Each player's character select cursor column
    s8 characterRows[PLAYER_COUNT_MAX]; // And row
    s8 modeChoice; // The mode select cursor's place in the player count's modes
    s8 modeSpin; // How far the modes still have to turn to reach it
    // The bet settings screen's chosen column of the bet set
    s8 betColumn;
    s8 betStakeLevels[BET_STAKE_SLOT_COUNT]; // Set from one of ApplyBetPreset's presets
    s8 courseChoice; // The course select's course: an open one, or the next one still shut
    s8 courseSpin; // How far the courses still have to turn to reach it
    HolesChoice holesChoice; // HOLES_CHOICE_*
    s8 caddyChoice; // The debug caddy select's caddy
    s8 unusedAfterCaddyChoice;
    s8 mainChoice;
    s8 mainChoiceWithoutTransmit;
    s8 clubhouseChoice;
    s8 optionsChoice;
    s8 eraseChoice; // The erase window's yes or no
    s8 helpTopicIndex;
    u8 unusedAfterHelpTopic[0x29 - 0x21];
    // The player whose handicap the handicap screen is choosing
    s8 handicapPlayer;
    // Only ever cleared, with handicapPlayer
    s8 unusedHandicapValue;
    // The mini-golf course select's record of three courses
    s8 miniGolfRecordChoice;
    // The mini-golf course select's course on its record's disc
    s8 miniGolfCourseChoice;
    // How far the mini-golf course select still has to slide to the record
    s8 miniGolfRecordSlide;
    s8 holeIndex; // The hole the Ring Shot select points at
    CourseIndex8 courseIndex; // And its course
    s8 statsCourse; // The course whose statistics show; one past the open courses totals them
    u8 unusedAfterStatsCourse;
    s8 statsSpin; // How far the courses still have to turn to reach it
    CourseIndex8 bestTimesCourse; // The open course whose best times show
    s8 bestTimesSpin; // How far the courses still have to turn to reach it
    s8 tableColumn; // The cursor's place among a table's shown columns
    s8 tableRow; // And among its shown rows
    s8 tableFirstColumn; // The first column shown of a table wider than the screen
    s8 tableFirstRow; // And the first row shown of one taller
    s8 bestShotColumn; // The Best Shots cursor's cell
    s8 bestShotRow;
    s8 ringShotRow; // The Ring Shot records' row: a course, or the replays after them
    s8 ringShotFirstCharacters[RING_SHOT_COURSE_COUNT]; // Each course's first character shown
    s8 ringShotReplaySlot; // The Ring Shot replay chosen
    s8 savedGameIndex;
    // Updates left of the saved games' boards turning, negative when turning right
    s8 savedGameTurn;
    u8 optionBits; // Each OPTION_BIT_* mirrors a saved option
    // The menus were entered with the transmit buttons, so the main menu offers it
    UBoolean8 isTransmitShown;
    s8 codeKeyColumn; // The club tournament code keyboard's chosen key
    s8 codeKeyRow;
} MenuMemory;

MenuMemory *GetMenuMemory(void);

extern MenuMemory g_MenuMemory;

// The character select's record of which characters are taken
typedef struct TakenCharacters {
    // Only ever cleared, as the menu choices reset and with modeChoice
    s16 unusedMenuValue;
    u8 unusedBeforeTakenCharacters[0x8 - 0x2];
    Boolean isCharacterTaken[CHARACTER_COUNT];
    u8 unusedAfterTakenCharacters[0x54 - 0x50];
} TakenCharacters;

extern TakenCharacters g_TakenCharacters;

// The character each player has chosen, or NO_CHOSEN_CHARACTER
#define NO_CHOSEN_CHARACTER (-1)
extern s32 g_ChosenCharacters[PLAYER_COUNT_MAX];

// Whether each player swings left-handed, which becomes g_Players[].isLefty
extern Boolean8 g_ChosenLefties[PLAYER_COUNT_MAX];

// Whether the menus have loaded each character slot's character
extern UBoolean8 g_IsMenuCharacterLoaded[MAXCONTROLLERS];

#endif
