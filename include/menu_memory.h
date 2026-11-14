#ifndef MENU_MEMORY_H
#define MENU_MEMORY_H

#include <ultra64.h>

#include "bets.h"
#include "c_language.h"
#include "character.h"
#include "course.h"
#include "player.h"

#define BET_STAKE_SLOT_COUNT (BET_COUNT + 1)

#define BET_PRESET_COUNT 7
#define BET_PRESET_STROKE 0
#define BET_PRESET_STROKE_FOUR_PLAYER 1
#define BET_PRESET_MATCH_GAME 2
#define BET_PRESET_SKINS_MATCH 3
#define BET_PRESET_CLUB_SLOTS 4
#define BET_PRESET_MINI_GOLF 5
#define BET_PRESET_MINI_GOLF_FOUR_PLAYER 6
#define BET_PRESET_NONE 10

enum HolesChoice {
    HOLES_CHOICE_UNUSED,
    HOLES_CHOICE_FRONT_NINE,
    HOLES_CHOICE_BACK_NINE,
    HOLES_CHOICE_ALL
};

typedef s8 HolesChoice;

typedef struct MenuMemory {
    s8 playerCountChoice;
    s8 titleChoice;
    CharacterIndex8 viewerCharacter;
    s8 characterColumns[PLAYER_COUNT_MAX];
    s8 characterRows[PLAYER_COUNT_MAX];
    s8 modeChoice;
    s8 modeSpin;
    s8 betColumn;
    s8 betStakeLevels[BET_STAKE_SLOT_COUNT];
    s8 courseChoice;
    s8 courseSpin;
    HolesChoice holesChoice;
    s8 caddyChoice;
    s8 unusedAfterCaddyChoice;
    s8 mainChoice;
    s8 mainChoiceWithoutTransmit;
    s8 clubhouseChoice;
    s8 optionsChoice;
    s8 eraseChoice;
    s8 helpTopicIndex;
    u8 unusedAfterHelpTopic[0x29 - 0x21];
    s8 handicapPlayer;
    s8 unusedHandicapValue;
    s8 miniGolfRecordChoice;
    s8 miniGolfCourseChoice;
    s8 miniGolfRecordSlide;
    s8 holeIndex;
    CourseIndex8 courseIndex;
    s8 statsCourse;
    u8 unusedAfterStatsCourse;
    s8 statsSpin;
    CourseIndex8 bestTimesCourse;
    s8 bestTimesSpin;
    s8 tableColumn;
    s8 tableRow;
    s8 tableFirstColumn;
    s8 tableFirstRow;
    s8 bestShotColumn;
    s8 bestShotRow;
    s8 ringShotRow;
    s8 ringShotFirstCharacters[RING_SHOT_COURSE_COUNT];
    s8 ringShotReplaySlot;
    s8 savedGameIndex;
    s8 savedGameTurn;
    u8 optionBits;
    UBoolean8 isTransmitShown;
    s8 codeKeyColumn;
    s8 codeKeyRow;
} MenuMemory;

MenuMemory *GetMenuMemory(void);

extern MenuMemory g_MenuMemory;

typedef struct TakenCharacters {
    s16 unusedMenuValue;
    u8 unusedBeforeTakenCharacters[0x8 - 0x2];
    Boolean isCharacterTaken[CHARACTER_COUNT];
    u8 unusedAfterTakenCharacters[0x54 - 0x50];
} TakenCharacters;

extern TakenCharacters g_TakenCharacters;

#define NO_CHOSEN_CHARACTER (-1)
extern s32 g_ChosenCharacters[PLAYER_COUNT_MAX];

extern Boolean8 g_ChosenLefties[PLAYER_COUNT_MAX];

extern UBoolean8 g_IsMenuCharacterLoaded[MAXCONTROLLERS];

#endif
