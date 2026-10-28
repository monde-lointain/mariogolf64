#ifndef MOVIE_H
#define MOVIE_H

#include <ultra64.h>

/* Movies that StartMovie plays */
enum Movie {
  MOVIE_NONE = -1, /* Stops the movie */
  /* Toad, when the ball goes out of bounds */
  MOVIE_TOAD_OUT_OF_BOUNDS,
  MOVIE_LAKITU, /* Lakitu, during the first hole's intro */
  /* Loads the Monty Mole movie, but nothing draws it */
  MOVIE_MONTY_MOLE_UNK2,
  MOVIE_MONTY_MOLE_UNK3,
  MOVIE_MONTY_MOLE_UNK4,
  MOVIE_GOOMBA,
  MOVIE_ALBATROSS_PUTT,
  MOVIE_EAGLE_PUTT,
  MOVIE_BIRDIE_PUTT,
  MOVIE_SINK_TO_WIN,
  MOVIE_SINK_TO_TIE,
  MOVIE_MISS_AND_LOSE,
  MOVIE_LONGEST_DRIVE, /* A longest drive contest's winner */
  MOVIE_NEAREST_PIN, /* A nearest pin contest's winner */
  MOVIE_DRIVE_RECORD, /* A tournament's longest drive yet */
  /* "Ready... Go!", during a later hole's intro */
  MOVIE_READY_GO,
  MOVIE_MONTY_MOLE
};

void AllocMovieBuffer(void);
void FreeMovieBuffer(void);
Boolean IsMovieFinished(void);
void StartMovie(s32 movie);
Boolean IsMoviePastCue(void);
void DrawMovie(Gfx** displayListHead);

#endif
