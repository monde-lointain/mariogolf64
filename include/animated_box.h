#ifndef ANIMATED_BOX_H
#define ANIMATED_BOX_H

#include <ultra64.h>

typedef enum AnimatedBoxState {
    BOX_OPENING,
    BOX_OPEN,
    BOX_CLOSING,
    BOX_CLOSED
} AnimatedBoxState;

typedef struct AnimatedBox {
      s16 left;
      s16 top;
      s16 right;
      s16 bottom;
      u8 red;
      u8 green;
      u8 blue;
      u8 alpha;
      s32 state;
      s32 frameIndex;
} AnimatedBox;

#define QUEUED_BOX_CAPACITY 10

void QueueAnimatedBox(AnimatedBox *box);
void DrawAnimatedBoxes(void);

#endif
