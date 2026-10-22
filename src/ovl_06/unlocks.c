#include "common.h"
#include "controller.h"
#include "save.h"
#include "sram.h"
#include "unlocks.h"

/*
 * The parts of the save that the title screen's button codes unlock, and a
 * code of this file's own that nothing reads
 */

/* GrantWinsStarsAndBadges gives a win, where there is none, in this many of the first rows of unkAF0 */
#define GRANTED_WIN_ROW_COUNT 14
/* GrantRingShotProgressAndPoints sets this many of each hole's unk1344 entries */
#define GRANTED_UNK1344_COUNT 13
/* Enough to open every course but the last */
#define GRANTED_COURSE_POINTS 3000

/* UpdateUnusedButtonCode's sequence is this long */
#define UNUSED_CODE_LENGTH 13
/* Nothing has been held in that place of the sequence yet */
#define NO_BUTTONS (-1)

/* ovl_01 sets this to the held buttons of each player's menu reading, ORed together */
extern s32 D_800C2000;

void func_80050BF0(s32, s32, s32);
void func_8005C674(s32, s32);

void GrantWinsStarsAndBadges(void);
void GrantRingShotProgressAndPoints(void);

static s32 sUnusedCodeButtons[UNUSED_CODE_LENGTH] = {
    L_CBUTTONS, R_CBUTTONS, D_CBUTTONS, U_CBUTTONS, L_TRIG, R_TRIG, L_TRIG,
    R_TRIG,     U_JPAD,     D_JPAD,     L_JPAD,     R_JPAD, Z_TRIG,
};
static s32 sUnusedCodeEntered[UNUSED_CODE_LENGTH] = {
    NO_BUTTONS, NO_BUTTONS, NO_BUTTONS, NO_BUTTONS, NO_BUTTONS, NO_BUTTONS, NO_BUTTONS,
    NO_BUTTONS, NO_BUTTONS, NO_BUTTONS, NO_BUTTONS, NO_BUTTONS, NO_BUTTONS,
};
static s32 sUnusedCodeLength = 0;

void UnlockEverything(void) {
    SaveData *save = GetSaveData();

    func_80050BF0(0x69, 0xD, 0x50);
    InitSram();
    TransferSaveData(save, FALSE);
    GrantWinsStarsAndBadges();
    GrantRingShotProgressAndPoints();
    TransferSaveData(save, TRUE);
}

/* Ends with func_8005C674, which rebuilds the character grid from the save */
void GrantWinsStarsAndBadges(void) {
    SaveData *save = GetSaveData();
    /* The star and badge loops reuse these for the course and the hole, to match */
    s32 row;
    s32 column;

    for (row = 0; row != GRANTED_WIN_ROW_COUNT; row++) {
        for (column = 0; column != SAVE_UNKAF0_COLUMN_COUNT; column++) {
            if (!(save->unk990.unkAF0[row][column].unk1 & WIN_COUNT_MASK)) {
                save->unk990.unkAF0[row][column].unk1 = 1;
            }
        }
    }
    for (row = 0; row != RING_SHOT_COURSE_COUNT; row++) {
        for (column = 0; column != RING_SHOT_HOLE_COUNT; column++) {
            save->hasRingShotStar[row][column] = TRUE;
        }
    }
    for (row = 0; row != COURSE_COUNT; row++) {
        for (column = 0; column != COURSE_HOLE_COUNT; column++) {
            save->unk990.birdieBadgeCounts[row][column] = 1;
        }
    }
    func_8005C674(1, 1);
}

void GrantRingShotProgressAndPoints(void) {
    SaveData *save = GetSaveData();
    s32 courseIndex;
    s32 holeIndex;
    s32 index;

    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            save->hasRingShotStar[courseIndex][holeIndex] = TRUE;
            for (index = 0; index != GRANTED_UNK1344_COUNT; index++) {
                save->unk1344[courseIndex][holeIndex][index] = 1;
            }
        }
    }
    if (save->coursePoints < GRANTED_COURSE_POINTS) {
        save->coursePoints = GRANTED_COURSE_POINTS;
    }
}

void GrantEveryTournamentPlace(void) {
    SaveData *save = GetSaveData();
    s32 courseIndex;
    s32 place;

    InitSram();
    TransferSaveData(save, FALSE);
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        for (place = 0; place != TOURNAMENT_PLACE_COUNT; place++) {
            save->unkE98[courseIndex].hasPlace[place] = TRUE;
        }
    }
    TransferSaveData(save, TRUE);
}

/*
 * No code calls this. Every frame that any button is held, the held buttons
 * are added to the sequence; once it matches sUnusedCodeButtons, the save
 * is unlocked as by UnlockEverything.
 */
void UpdateUnusedButtonCode(void) {
    s32 index;

    index = 0;
    if (D_800C2000 > 0) {
        if (sUnusedCodeLength < UNUSED_CODE_LENGTH) {
            sUnusedCodeEntered[sUnusedCodeLength] = D_800C2000;
            sUnusedCodeLength++;
        }
        for (; index != sUnusedCodeLength; index++) {
            if (sUnusedCodeButtons[index] != sUnusedCodeEntered[index]) {
                break;
            }
        }
        if (index == sUnusedCodeLength) {
            if (sUnusedCodeLength == UNUSED_CODE_LENGTH) {
                sUnusedCodeLength = 0;
                for (index = 0; index != UNUSED_CODE_LENGTH; index++) {
                    sUnusedCodeEntered[index] = NO_BUTTONS;
                }
                func_80050BF0(0x69, 0xD, 0x50);
                UnlockEverything();
            }
        } else {
            sUnusedCodeLength = 0;
            for (index = 0; index != UNUSED_CODE_LENGTH; index++) {
                sUnusedCodeEntered[index] = NO_BUTTONS;
            }
        }
    }
}
