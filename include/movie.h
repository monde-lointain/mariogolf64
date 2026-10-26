#ifndef MOVIE_H
#define MOVIE_H

#include <ultra64.h>

/* Movies that StartMovie plays */
#define MOVIE_NONE (-1)            /* Stops the movie */
#define MOVIE_TOAD_OUT_OF_BOUNDS 0 /* Toad, when the ball goes out of bounds */
#define MOVIE_UNK1 1
#define MOVIE_UNK5 5
#define MOVIE_UNK6 6
#define MOVIE_UNK7 7
#define MOVIE_UNK8 8
#define MOVIE_UNK9 9
#define MOVIE_UNK10 10
#define MOVIE_UNK11 11
#define MOVIE_LONGEST_DRIVE 12 /* A longest drive contest's winner */
#define MOVIE_NEAREST_PIN 13   /* A nearest pin contest's winner */
#define MOVIE_DRIVE_RECORD 14  /* A tournament's longest drive yet */
#define MOVIE_UNK15 15
#define MOVIE_UNK16 16

void AllocMovieBuffer(void);
void FreeMovieBuffer(void);
s32 IsMovieFinished(void);
void StartMovie(s32 movie);
s32 IsMoviePastCue(void);
void DrawMovie(Gfx **displayListHead);

#endif
