/*
 * The bets that a game can be played for
 */

#ifndef BETS_H
#define BETS_H

#include <ultra64.h>

// The bets of a game, indexing GameState.betRates and Player.betTallies
// On marked par 5 holes
#define BET_LONGEST_DRIVE 0
// On par 3 holes
#define BET_NEAREST_PIN 1
// Paid per stroke, between each pair of players
#define BET_STROKES 2
// Paid on the front nine, the back nine and the round
#define BET_NASSAU 3
// Settled on each hole; a tie carries the pot over. The Skins Match and Club
// Slots bet presets stake it.
#define BET_SKINS 4
// A hole pot, settled as BET_SKINS is when that bet is not set; of the bet
// presets, only Match Game's stakes it
#define BET_MATCH_POT 5
// Paid per stroke, between the two teams of two
#define BET_LAS_VEGAS 6
#define BET_COUNT 7
// The rate of a bet that is not played
#define UNSET_BET_RATE (-1)

#endif
