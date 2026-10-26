#ifndef MOVIE_H
#define MOVIE_H

#include <ultra64.h>

/* Movies that StartMovie plays */
#define MOVIE_NONE (-1)            /* Stops the movie */
#define MOVIE_TOAD_OUT_OF_BOUNDS 0 /* Toad, when the ball goes out of bounds */
#define MOVIE_LAKITU 1             /* Lakitu, during the first hole's intro */
#define MOVIE_MONTY_MOLE_UNK2 2    /* Loads the Monty Mole movie, but nothing draws it */
#define MOVIE_MONTY_MOLE_UNK3 3
#define MOVIE_MONTY_MOLE_UNK4 4
#define MOVIE_GOOMBA 5
#define MOVIE_ALBATROSS_PUTT 6
#define MOVIE_EAGLE_PUTT 7
#define MOVIE_BIRDIE_PUTT 8
#define MOVIE_SINK_TO_WIN 9
#define MOVIE_SINK_TO_TIE 10
#define MOVIE_MISS_AND_LOSE 11
#define MOVIE_LONGEST_DRIVE 12     /* A longest drive contest's winner */
#define MOVIE_NEAREST_PIN 13       /* A nearest pin contest's winner */
#define MOVIE_DRIVE_RECORD 14      /* A tournament's longest drive yet */
#define MOVIE_READY_GO 15          /* "Ready... Go!", during a later hole's intro */
#define MOVIE_MONTY_MOLE 16

void AllocMovieBuffer(void);
void FreeMovieBuffer(void);
s32 IsMovieFinished(void);
void StartMovie(s32 movie);
s32 IsMoviePastCue(void);
void DrawMovie(Gfx **displayListHead);

#endif
