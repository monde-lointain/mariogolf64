/*
 * The parts of the save that the title screen's button codes unlock, and a
 * code of this file's own that nothing reads
 */

#include "menus/unlocks.h"

#include <ultra64.h>

#include "audio/sound.h"
#include "characters/character.h"
#include "course/course.h"
#include "menus/menu.h"
#include "save/course_stats.h"
#include "save/save.h"
#include "save/sram.h"

// GrantWinsStarsAndBadges gives a win, where there is none, in this many of the
// first rows of the matchups
#define GRANTED_WIN_ROW_COUNT 14
// GrantRingShotProgressAndPoints records a clear of each hole for this many of
// the first characters
#define GRANTED_CLEAR_CHARACTER_COUNT 13
// Enough to open every course but the last
#define GRANTED_COURSE_POINTS 3000

// UpdateUnusedButtonCode's sequence is this long
#define UNUSED_CODE_LENGTH 13
// Nothing has been held in that place of the sequence yet
#define NO_BUTTONS (-1)
// The channel of the sound for unlocking everything
#define UNLOCK_SOUND_CHANNEL 13

void GrantWinsStarsAndBadges(void);
void GrantRingShotProgressAndPoints(void);

static s32 f_UnusedCodeButtons[UNUSED_CODE_LENGTH] = {
    L_CBUTTONS,
    R_CBUTTONS,
    D_CBUTTONS,
    U_CBUTTONS,
    L_TRIG,
    R_TRIG,
    L_TRIG,
    R_TRIG,
    U_JPAD,
    D_JPAD,
    L_JPAD,
    R_JPAD,
    Z_TRIG,
};
static s32 f_UnusedCodeEntered[UNUSED_CODE_LENGTH] = {
    NO_BUTTONS,
    NO_BUTTONS,
    NO_BUTTONS,
    NO_BUTTONS,
    NO_BUTTONS,
    NO_BUTTONS,
    NO_BUTTONS,
    NO_BUTTONS,
    NO_BUTTONS,
    NO_BUTTONS,
    NO_BUTTONS,
    NO_BUTTONS,
    NO_BUTTONS,
};
static s32 f_UnusedCodeLength = 0;

void UnlockEverything(void) {
    SaveData *save = GetSaveData();

    PlaySound(SOUND_UNLOCK_EVERYTHING, UNLOCK_SOUND_CHANNEL, MENU_VOLUME_SOFT);
    InitSram();
    TransferSaveData(save, FALSE);
    GrantWinsStarsAndBadges();
    GrantRingShotProgressAndPoints();
    TransferSaveData(save, TRUE);
}

// Ends with BuildCharacterGrid, which refills the grid from the save
void GrantWinsStarsAndBadges(void) {
    SaveData *save = GetSaveData();

    // The star and badge loops reuse these for the course and the hole, to
    // match: a courseIndex and hole of their own, declared before or after
    // these, give 34 words differing, none in shape
    s32 row;
    s32 column;

    for (row = 0; row != GRANTED_WIN_ROW_COUNT; row++) {
        for (column = 0; column != CHARACTER_COUNT; column++) {
            if (!(save->records.awards.matchups[row][column].winBits & WIN_COUNT_MASK)) {
                save->records.awards.matchups[row][column].winBits = 1;
            }
        }
    }

    for (row = 0; row != RING_SHOT_COURSE_COUNT; row++) {
        for (column = 0; column != RING_SHOT_HOLE_COUNT; column++) {
            save->records.hasRingShotStar[row][column] = TRUE;
        }
    }

    for (row = 0; row != COURSE_COUNT; row++) {
        for (column = 0; column != COURSE_HOLE_COUNT; column++) {
            save->records.awards.birdieBadgeCounts[row][column] = 1;
        }
    }

    BuildCharacterGrid(1, TRUE);
}

void GrantRingShotProgressAndPoints(void) {
    SaveData *save = GetSaveData();
    CourseIndex courseIndex;
    s32 holeIndex;
    s32 index;

    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            save->records.hasRingShotStar[courseIndex][holeIndex] = TRUE;
            for (index = 0; index != GRANTED_CLEAR_CHARACTER_COUNT; index++) {
                save->ringShotClearCounts[courseIndex][holeIndex][index] = 1;
            }
        }
    }

    if (save->coursePoints < GRANTED_COURSE_POINTS) {
        save->coursePoints = GRANTED_COURSE_POINTS;
    }
}

void GrantEveryTournamentPlace(void) {
    SaveData *save = GetSaveData();
    CourseIndex courseIndex;
    s32 place;

    InitSram();
    TransferSaveData(save, FALSE);
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        for (place = 0; place != TOURNAMENT_PLACE_COUNT; place++) {
            save->records.courseStats[courseIndex].hasPlace[place] = TRUE;
        }
    }

    TransferSaveData(save, TRUE);
}

// No code calls this. Every frame that any button is held, the held buttons
// are added to the sequence; once it matches f_UnusedCodeButtons, the save
// is unlocked as by UnlockEverything.
void UpdateUnusedButtonCode(void) {
    s32 index;

    index = 0;
    if (g_MenuRepeatButtons > 0) {
        if (f_UnusedCodeLength < UNUSED_CODE_LENGTH) {
            f_UnusedCodeEntered[f_UnusedCodeLength] = g_MenuRepeatButtons;
            f_UnusedCodeLength++;
        }

        for (; index != f_UnusedCodeLength; index++) {
            if (f_UnusedCodeButtons[index] != f_UnusedCodeEntered[index]) {
                break;
            }
        }

        if (index == f_UnusedCodeLength) {
            if (f_UnusedCodeLength == UNUSED_CODE_LENGTH) {
                f_UnusedCodeLength = 0;
                for (index = 0; index != UNUSED_CODE_LENGTH; index++) {
                    f_UnusedCodeEntered[index] = NO_BUTTONS;
                }

                PlaySound(SOUND_UNLOCK_EVERYTHING, UNLOCK_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                UnlockEverything();
            }
        }
        else {
            f_UnusedCodeLength = 0;
            for (index = 0; index != UNUSED_CODE_LENGTH; index++) {
                f_UnusedCodeEntered[index] = NO_BUTTONS;
            }
        }
    }
}
