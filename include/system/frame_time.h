/*
 * Time as the game counts it, in frames, and the clock units it shows
 */

#ifndef FRAME_TIME_H
#define FRAME_TIME_H

// The game counts time in frames
#define FRAMES_PER_SECOND 30
#define SECONDS_PER_MINUTE 60
#define MINUTES_PER_HOUR 60
#define HUNDREDTHS_PER_SECOND 100
#define FRAMES_PER_MINUTE (SECONDS_PER_MINUTE * FRAMES_PER_SECOND)
// The stopwatches stop at an hour
#define STOPWATCH_FRAMES_MAX (MINUTES_PER_HOUR * FRAMES_PER_MINUTE)

#endif
