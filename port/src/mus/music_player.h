/*
 * What the port's music player offers beyond libmus.h: the routines and
 * counters that the game declares for itself, which the N64's player had
 * outside its public header.
 */

#ifndef PORT_MUS_MUSIC_PLAYER_H
#define PORT_MUS_MUSIC_PLAYER_H

#include <ultra64.h>

// Takes length bytes from the heap that MusInitialize was given, zeroed and
// aligned to 16; NULL once the heap runs out
void *__MusIntMemMalloc(int length);
int __MusIntMemRemaining(void);

// Chooses the effect by its number in the game's list; gives 0 if it did,
// and 1 for a number past the list
s32 ChangeCustomEffect(s32 type);

// Set to play silence, with the music going on underneath
extern unsigned char __libmus_output_muted;

// The microseconds that the last frame of music and the slowest took
extern unsigned long _mus_cpu_last;
extern unsigned long _mus_cpu_worst;

#endif
