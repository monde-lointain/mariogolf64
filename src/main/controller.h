#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "common.h"

typedef struct {
  /* 0x00 */ u16 buttons;
  /* 0x02 */ u16 held;
  /* 0x04 */ u16 pressed;
  /* 0x06 */ u16 previous;
  /* 0x08 */ u16 repeated;
  /* 0x0A */ u16 repeat_count;
  /* 0x0C */ s8 stick_x;
  /* 0x0D */ s8 stick_y;
} ControllerState; /* 0x0E */

/* Direction and previous-button symbols alias these six-byte repeat slots. */
typedef struct {
  /* 0x00 */ s16 repeat_count;
  /* 0x02 */ s8 horizontal_direction;
  /* 0x03 */ s8 vertical_direction;
  /* 0x04 */ u16 previous_buttons;
} ControllerRepeatState; /* 0x06 */

#endif /* CONTROLLER_H */
