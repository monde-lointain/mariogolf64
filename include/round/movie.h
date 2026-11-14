/*
 * The short movies that the round plays, such as Lakitu's countdown.
 */

#ifndef MOVIE_H
#define MOVIE_H

#include <ultra64.h>

#include "system/c_language.h"

// Movies that StartMovie plays
enum Movie {
    MOVIE_NONE = -1, // Stops the movie
    // Toad, when the ball goes out of bounds
    MOVIE_TOAD_OUT_OF_BOUNDS,
    MOVIE_LAKITU, // Lakitu, during the first hole's intro
    // Never started: each would load the Monty Mole movie, but nothing draws
    // them
    MOVIE_UNUSED_MONTY_MOLE_A,
    MOVIE_UNUSED_MONTY_MOLE_B,
    MOVIE_UNUSED_MONTY_MOLE_C,
    MOVIE_GOOMBA,
    MOVIE_ALBATROSS_PUTT,
    MOVIE_EAGLE_PUTT,
    MOVIE_BIRDIE_PUTT,
    MOVIE_SINK_TO_WIN,
    MOVIE_SINK_TO_TIE,
    MOVIE_MISS_AND_LOSE,
    MOVIE_LONGEST_DRIVE, // A longest drive contest's winner
    MOVIE_NEAREST_PIN, // A nearest pin contest's winner
    MOVIE_DRIVE_RECORD, // A tournament's longest drive yet
    // "Ready... Go!", during a later hole's intro
    MOVIE_READY_GO,
    MOVIE_MONTY_MOLE
};

typedef s32 Movie;

void AllocMovieBuffer(void);
void FreeMovieBuffer(void);
Boolean IsMovieDone(void);
void StartMovie(Movie movie);
Boolean IsMoviePastCue(void);
void DrawMovie(Gfx **displayListHead);

#endif
