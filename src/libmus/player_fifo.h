/*
 * player_fifo.h
 *
 * Command FIFO that carries control commands from the caller (game thread)
 * into the audio-thread side of the libmus player. A producer on the game
 * thread enqueues a command; the player drains the queue once per audio
 * frame and acts on it, so playback state is only ever changed from the
 * audio thread.
 */
#ifndef _PLAYER_FIFO_H_
#define _PLAYER_FIFO_H_

/* One queued control command. The three padding bytes exist only to align
 * `data` onto a 4-byte boundary. */
typedef struct {
  unsigned char command; /* command code, one of the FIFOCMD_* values */
  unsigned char padding1;
  unsigned char padding2;
  unsigned char padding3;
  /* Argument for `command`: a musHandle for pause/unpause, an effect type
   * for change-fx. */
  unsigned long data;
} fifo_t;

/* Bounds on the ring capacity (in entries) requested at init; __MusIntFifoOpen
 * clamps the caller's count into this range. */
#define MIN_FIFO_COMMANDS (64)
#define MAX_FIFO_COMMANDS (1024)

/* Command codes stored in fifo_t.command. */
enum {
  FIFOCMD_PAUSE,    /* pause the handle in `data` */
  FIFOCMD_UNPAUSE,  /* resume the handle in `data` */
  FIFOCMD_CHANGEFX, /* switch the custom effect to type `data` */
  FIFOCMD_LAST      /* count of command codes (sentinel) */
};

/* Allocate the ring and clamp `commands` into [MIN..MAX] entries. */
static void __MusIntFifoOpen(int commands);

/* Audio-thread consumer: drain and dispatch every queued command. */
static void __MusIntFifoProcess(void);

/* Dispatch a single command entry. */
static void __MusIntFifoProcessCommand(fifo_t* command);

/* Caller-side producer: enqueue one command. Returns 1 on success, 0 when
 * the ring is full. */
static int __MusIntFifoAddCommand(fifo_t* command);

#endif
