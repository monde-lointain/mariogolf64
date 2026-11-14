#ifndef MOVIE_H
#define MOVIE_H

#include <ultra64.h>

#include "language.h"

enum Movie {
    MOVIE_NONE = -1,
    MOVIE_TOAD_OUT_OF_BOUNDS,
    MOVIE_LAKITU,
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
    MOVIE_LONGEST_DRIVE,
    MOVIE_NEAREST_PIN,
    MOVIE_DRIVE_RECORD,
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
