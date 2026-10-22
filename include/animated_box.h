#ifndef ANIMATED_BOX_H
#define ANIMATED_BOX_H

#include "common.h"

typedef enum AnimatedBoxState {
    BOX_OPENING,
    BOX_OPEN,
    BOX_CLOSING,
    BOX_CLOSED
} AnimatedBoxState;

/* A filled rectangle that opens out from its middle and closes back in */
typedef struct AnimatedBox {
    /* 0x00 */ s16 left;
    /* 0x02 */ s16 top;
    /* 0x04 */ s16 right;
    /* 0x06 */ s16 bottom;
    /* 0x08 */ u8 red;
    /* 0x09 */ u8 green;
    /* 0x0A */ u8 blue;
    /* 0x0B */ u8 alpha;
    /* 0x0C */ s32 state;
    /* 0x10 */ s32 frameIndex;
} AnimatedBox; /* size = 0x14 */

void QueueAnimatedBox(AnimatedBox *box);
void DrawAnimatedBoxes(void);

#endif
