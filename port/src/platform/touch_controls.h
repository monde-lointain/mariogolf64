/*
 * The touch controls: an N64 controller drawn on a touch screen, for the
 * first port. They follow the N64 controller's own arrangement, so that a
 * player who knows the game knows where each control is. The left thumb
 * has the stick, which centers wherever the thumb lands in the screen's
 * left half, and START, high up and away from the shot buttons. The right
 * thumb has A, the largest, with B up and to its left, Z to its left,
 * which it holds while the left thumb sets the spin, and above them the
 * C buttons and R.
 *
 * A finger keeps the control that it landed on until it lifts, so that a
 * thumb sliding across the screen doesn't press another by mistake.
 *
 * Everything here is in the window's pixels, from its top left.
 */

#ifndef PORT_PLATFORM_TOUCH_CONTROLS_H
#define PORT_PLATFORM_TOUCH_CONTROLS_H

#include <stdbool.h>
#include <stdint.h>
#include <ultra64.h>

#include "port/overlay_shape.h"

#define TOUCH_MAX_FINGERS 10

typedef enum TouchControl {
    TOUCH_CONTROL_NONE = -1,
    TOUCH_CONTROL_STICK,
    TOUCH_CONTROL_A,
    TOUCH_CONTROL_B,
    TOUCH_CONTROL_Z,
    TOUCH_CONTROL_R,
    TOUCH_CONTROL_START,
    TOUCH_CONTROL_C_UP,
    TOUCH_CONTROL_C_DOWN,
    TOUCH_CONTROL_C_LEFT,
    TOUCH_CONTROL_C_RIGHT,
    TOUCH_CONTROL_COUNT
} TouchControl;

// The shapes that DescribeTouchControls gives: one for each button, and
// the stick's ring and knob
#define TOUCH_SHAPE_COUNT (TOUCH_CONTROL_COUNT + 1)

// The screen that the controls are laid out on: its size in pixels, its
// pixels to a density-independent pixel, a 160th of an inch, and how far
// in from each edge the area that the system leaves clear starts
typedef struct TouchScreen {
    float width;
    float height;
    float pixelsPerDp;
    float safeLeft;
    float safeTop;
    float safeRight;
    float safeBottom;
} TouchScreen;

typedef struct TouchCircle {
    float x;
    float y;
    float radius;
} TouchCircle;

// Where each control is. The stick's circle is where it rests, and its
// radius how far the thumb pushes it to the full.
typedef struct TouchLayout {
    TouchCircle controls[TOUCH_CONTROL_COUNT];
    float stickZoneRight; // A touch left of this on no button takes the stick
} TouchLayout;

typedef struct TouchFinger {
    uint64_t id;
    TouchControl control; // TOUCH_CONTROL_NONE for a free place
    float startX;
    float startY;
    float x;
    float y;
    bool isUnread; // Down since the last read
} TouchFinger;

// The fingers that are down on controls. All zeros isn't a valid state;
// LiftAllTouches makes the first.
typedef struct TouchPad {
    TouchFinger fingers[TOUCH_MAX_FINGERS];
    uint16_t tappedButtons; // Lifted before a read saw them down
} TouchPad;

// Lays the controls out on a screen at least as wide as it is tall,
// shrinking them only when the screen is too short for them
void ArrangeTouchControls(const TouchScreen *screen, TouchLayout *layout);

// The control that a touch at x, y lands on: the button that it is
// nearest, for the button's size, within a margin around it; else the
// stick, in its zone; else none
TouchControl FindTouchedControl(const TouchLayout *layout, float x, float y);

// A finger comes down, moves or lifts. A finger that lands on no control,
// or comes when all of TOUCH_MAX_FINGERS are down, is let be. A button
// lifted before a read saw it is still pressed at the next read, so that a
// quick tap counts; a canceled finger, which the system took back, isn't,
// nor is one that comes down again or that LiftAllTouches lifts.
void PressTouch(TouchPad *pad, const TouchLayout *layout, uint64_t finger, float x, float y);
void MoveTouch(TouchPad *pad, uint64_t finger, float x, float y);
void LiftTouch(TouchPad *pad, uint64_t finger);
void CancelTouch(TouchPad *pad, uint64_t finger);
void LiftAllTouches(TouchPad *pad);

// Whether any finger is down on a control
bool IsTouchPadHeld(const TouchPad *pad);

// What the touch controls play, as an N64 controller's state, with the
// taps since the last read, which it forgets
void ReadTouchPad(TouchPad *pad, const TouchLayout *layout, OSContPad *contPad);

// Adds the touch controls' state to another's: each button pressed on
// either, and the stick that is pushed the further
void MergeTouchPad(const OSContPad *touch, OSContPad *contPad);

// The shapes that show the controls, a held one brighter, with the stick's
// ring and knob where the thumb has them; gives TOUCH_SHAPE_COUNT
int DescribeTouchControls(const TouchPad *pad, const TouchLayout *layout, OverlayShape *shapes);

#endif
