#include "common.h"

#define NO_CONTEST_WINNER -1

/*
 * The player who won the hole's contest: the longest drive on a marked par
 * 5, or the shot nearest the pin on a par 3
 */
extern s32 sContestWinner;

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006CD50);

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006CE88);

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006D058);

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006D164);

s32 GetContestWinner(void) {
    return sContestWinner;
}

void SetContestWinner(s32 playerIndex) {
    sContestWinner = playerIndex;
}

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006D214);

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006D38C);

void ClearContestWinner(void) {
    sContestWinner = NO_CONTEST_WINNER;
}

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006D4EC);

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006D6D0);

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006DDCC);

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006DE44);

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006DEB4);

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006DF84);

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006DFF0);

INCLUDE_ASM("asm/nonmatchings/round_stats", func_8006E210);
