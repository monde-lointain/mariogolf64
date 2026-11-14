/*
 * The bets that a game can be played for
 */

#ifndef BETS_H
#define BETS_H

#include <ultra64.h>

// The bets of a game, indexing GameState.betRates and Player.betTallies
enum Bet {
    // On marked par 5 holes
    BET_LONGEST_DRIVE,
    // On par 3 holes
    BET_NEAREST_PIN,
    // Paid per stroke, between each pair of players
    BET_STROKES,
    // Paid on the front nine, the back nine and the round
    BET_NASSAU,
    // Settled on each hole; a tie carries the pot over. The Skins Match and Club
    // Slots bet presets stake it.
    BET_SKINS,
    // A hole pot, settled as BET_SKINS is when that bet is not set; of the bet
    // presets, only Match Game's stakes it
    BET_MATCH_POT,
    // Paid per stroke, between the two teams of two
    BET_LAS_VEGAS,
    BET_COUNT
};

typedef s32 Bet;

// The rate of a bet that is not played
#define UNSET_BET_RATE (-1)

#endif
