/*
 * The touch controls. Their places are set in density-independent pixels
 * from the safe area's bottom corners, where the thumbs rest whatever the
 * screen's size, so that they stay the same size under the finger on a
 * phone or a tablet.
 */

#include "touch_controls.h"

#include <SDL3/SDL.h>

#include "pad_mapping.h"

// The height that the controls take, in dp; a screen shorter than this
// shrinks them to fit
#define LAYOUT_HEIGHT_DP 380.0f
// How far past its drawn edge a button takes a touch, as a share of its
// radius
#define HIT_MARGIN 1.4f
// The stick's knob, as a share of its throw
#define KNOB_SHARE 0.45f
// How opaque a control is, from 0 to 255, at rest and held
#define REST_ALPHA 0x60
#define HELD_ALPHA 0xC0

SDL_COMPILE_TIME_ASSERT(touch_shapes_fit_overlay, TOUCH_SHAPE_COUNT <= OVERLAY_MAX_SHAPES);

// The stick's dead zone is an eighth of its throw, and it reaches the
// N64's full range at the throw's end
static const StickShape f_StickShape = {PAD_AXIS_MAX / 8, PAD_AXIS_MAX, PAD_STICK_RANGE_MAX};

typedef enum Corner {
    CORNER_BOTTOM_LEFT,
    CORNER_BOTTOM_RIGHT
} Corner;

typedef struct ControlPlace {
    Corner corner;
    float x; // In dp, in from the corner's side
    float y; // In dp, up from the bottom
    float radius; // In dp
    uint32_t color; // RGB, red lowest
    OverlayLabel label;
    u16 button;
} ControlPlace;

// The N64 controller's colors: blue A, green B, yellow C buttons, red
// START, and gray for the rest
static const ControlPlace f_Places[TOUCH_CONTROL_COUNT] = {
    [TOUCH_CONTROL_STICK] =
        {CORNER_BOTTOM_LEFT, 110.0f, 120.0f, 60.0f, 0xC0C0C0, OVERLAY_LABEL_NONE, 0},
    [TOUCH_CONTROL_A] =
        {CORNER_BOTTOM_RIGHT, 64.0f, 72.0f, 36.0f, 0xE05030, OVERLAY_LABEL_A, A_BUTTON},
    [TOUCH_CONTROL_B] =
        {CORNER_BOTTOM_RIGHT, 146.0f, 118.0f, 28.0f, 0x40A030, OVERLAY_LABEL_B, B_BUTTON},
    [TOUCH_CONTROL_Z] =
        {CORNER_BOTTOM_RIGHT, 150.0f, 48.0f, 26.0f, 0x909090, OVERLAY_LABEL_Z, Z_TRIG},
    [TOUCH_CONTROL_R] =
        {CORNER_BOTTOM_RIGHT, 56.0f, 330.0f, 26.0f, 0x909090, OVERLAY_LABEL_R, R_TRIG},
    [TOUCH_CONTROL_START] =
        {CORNER_BOTTOM_LEFT, 64.0f, 330.0f, 22.0f, 0x3030D0, OVERLAY_LABEL_START, START_BUTTON},
    [TOUCH_CONTROL_C_UP] =
        {CORNER_BOTTOM_RIGHT, 74.0f, 256.0f, 20.0f, 0x20C8E8, OVERLAY_LABEL_ARROW_UP, U_CBUTTONS},
    [TOUCH_CONTROL_C_DOWN] =
        {CORNER_BOTTOM_RIGHT, 74.0f, 172.0f, 20.0f, 0x20C8E8, OVERLAY_LABEL_ARROW_DOWN, D_CBUTTONS},
    [TOUCH_CONTROL_C_LEFT] =
        {CORNER_BOTTOM_RIGHT,
            116.0f,
            214.0f,
            20.0f,
            0x20C8E8,
            OVERLAY_LABEL_ARROW_LEFT,
            L_CBUTTONS},
    [TOUCH_CONTROL_C_RIGHT] = {
        CORNER_BOTTOM_RIGHT,
        32.0f,
        214.0f,
        20.0f,
        0x20C8E8,
        OVERLAY_LABEL_ARROW_RIGHT,
        R_CBUTTONS
    },
};

// --- The layout ---

void ArrangeTouchControls(const TouchScreen *screen, TouchLayout *layout) {
    float safeHeight;
    float scale;
    int i;

    SDL_assert(screen != NULL && layout != NULL && screen->pixelsPerDp > 0.0f);
    safeHeight = screen->height - screen->safeTop - screen->safeBottom;
    scale =
        screen->pixelsPerDp * SDL_min(1.0f, safeHeight / (LAYOUT_HEIGHT_DP * screen->pixelsPerDp));
    for (i = 0; i < TOUCH_CONTROL_COUNT; i++) {
        const ControlPlace *place = &f_Places[i];
        TouchCircle *circle = &layout->controls[i];

        circle->x = (place->corner == CORNER_BOTTOM_LEFT) ?
            screen->safeLeft + (place->x * scale) :
            screen->width - screen->safeRight - (place->x * scale);
        circle->y = screen->height - screen->safeBottom - (place->y * scale);
        circle->radius = place->radius * scale;
    }

    layout->stickZoneRight = screen->width / 2.0f;
}

TouchControl FindTouchedControl(const TouchLayout *layout, float x, float y) {
    TouchControl found = TOUCH_CONTROL_NONE;
    float foundReach = HIT_MARGIN;
    int i;

    SDL_assert(layout != NULL);
    // The nearest button for its size, as a share of its radius
    for (i = TOUCH_CONTROL_STICK + 1; i < TOUCH_CONTROL_COUNT; i++) {
        const TouchCircle *circle = &layout->controls[i];
        const float dx = x - circle->x;
        const float dy = y - circle->y;
        const float reach = SDL_sqrtf((dx * dx) + (dy * dy)) / circle->radius;

        if (reach <= foundReach) {
            found = (TouchControl)i;
            foundReach = reach;
        }
    }

    if (found == TOUCH_CONTROL_NONE && x < layout->stickZoneRight) {
        found = TOUCH_CONTROL_STICK;
    }

    return found;
}

// --- Fingers ---

static TouchFinger *FindFinger(TouchPad *pad, uint64_t finger) {
    int i;

    for (i = 0; i < TOUCH_MAX_FINGERS; i++) {
        if (pad->fingers[i].control != TOUCH_CONTROL_NONE && pad->fingers[i].id == finger) {
            return &pad->fingers[i];
        }
    }

    return NULL;
}

static TouchFinger *FindFreeFinger(TouchPad *pad) {
    int i;

    for (i = 0; i < TOUCH_MAX_FINGERS; i++) {
        if (pad->fingers[i].control == TOUCH_CONTROL_NONE) {
            return &pad->fingers[i];
        }
    }

    return NULL;
}

void PressTouch(TouchPad *pad, const TouchLayout *layout, uint64_t finger, float x, float y) {
    const TouchControl control = FindTouchedControl(layout, x, y);
    TouchFinger *place;

    SDL_assert(pad != NULL);
    CancelTouch(pad, finger);
    place = FindFreeFinger(pad);
    if (control == TOUCH_CONTROL_NONE || place == NULL) {
        return;
    }

    place->id = finger;
    place->control = control;
    place->startX = x;
    place->startY = y;
    place->x = x;
    place->y = y;
    place->isUnread = true;
}

void MoveTouch(TouchPad *pad, uint64_t finger, float x, float y) {
    TouchFinger *place;

    SDL_assert(pad != NULL);
    place = FindFinger(pad, finger);
    if (place != NULL) {
        place->x = x;
        place->y = y;
    }
}

void LiftTouch(TouchPad *pad, uint64_t finger) {
    TouchFinger *place;

    SDL_assert(pad != NULL);
    place = FindFinger(pad, finger);
    if (place == NULL) {
        return;
    }

    if (place->isUnread) {
        pad->tappedButtons |= f_Places[place->control].button;
    }

    place->control = TOUCH_CONTROL_NONE;
}

void CancelTouch(TouchPad *pad, uint64_t finger) {
    TouchFinger *place;

    SDL_assert(pad != NULL);
    place = FindFinger(pad, finger);
    if (place != NULL) {
        place->control = TOUCH_CONTROL_NONE;
    }
}

void LiftAllTouches(TouchPad *pad) {
    int i;

    SDL_assert(pad != NULL);
    SDL_zerop(pad);
    for (i = 0; i < TOUCH_MAX_FINGERS; i++) {
        pad->fingers[i].control = TOUCH_CONTROL_NONE;
    }
}

bool IsTouchPadHeld(const TouchPad *pad) {
    int i;

    SDL_assert(pad != NULL);
    for (i = 0; i < TOUCH_MAX_FINGERS; i++) {
        if (pad->fingers[i].control != TOUCH_CONTROL_NONE) {
            return true;
        }
    }

    return false;
}

// The stick's finger, or NULL
static const TouchFinger *FindStickFinger(const TouchPad *pad) {
    int i;

    for (i = 0; i < TOUCH_MAX_FINGERS; i++) {
        if (pad->fingers[i].control == TOUCH_CONTROL_STICK) {
            return &pad->fingers[i];
        }
    }

    return NULL;
}

// Where the knob is from the stick's center: the finger's travel, kept
// within the throw
static void MeasureKnob(const TouchFinger *finger, float throwRadius, float *dx, float *dy) {
    const float travelX = finger->x - finger->startX;
    const float travelY = finger->y - finger->startY;
    const float length = SDL_sqrtf((travelX * travelX) + (travelY * travelY));
    const float scale = (length > throwRadius) ? throwRadius / length : 1.0f;

    *dx = travelX * scale;
    *dy = travelY * scale;
}

void ReadTouchPad(TouchPad *pad, const TouchLayout *layout, OSContPad *contPad) {
    const TouchFinger *stick;
    int i;

    SDL_assert(pad != NULL && layout != NULL && contPad != NULL);
    SDL_zerop(contPad);
    contPad->button = pad->tappedButtons;
    pad->tappedButtons = 0;
    for (i = 0; i < TOUCH_MAX_FINGERS; i++) {
        const TouchControl control = pad->fingers[i].control;

        pad->fingers[i].isUnread = false;
        if (control != TOUCH_CONTROL_NONE) {
            contPad->button |= f_Places[control].button;
        }
    }

    stick = FindStickFinger(pad);
    if (stick != NULL) {
        const float throwRadius = layout->controls[TOUCH_CONTROL_STICK].radius;
        float dx;
        float dy;

        MeasureKnob(stick, throwRadius, &dx, &dy);
        // Up the screen is up the stick
        MapHostStick(
            &f_StickShape,
            (int)SDL_lroundf(dx / throwRadius * PAD_AXIS_MAX),
            (int)SDL_lroundf(-dy / throwRadius * PAD_AXIS_MAX),
            &contPad->stick_x,
            &contPad->stick_y
        );
    }
}

// Stick lengths compare squared, which the N64's range keeps well within
// an int
static int MeasureStickSquared(const OSContPad *contPad) {
    return (contPad->stick_x * contPad->stick_x) + (contPad->stick_y * contPad->stick_y);
}

void MergeTouchPad(const OSContPad *touch, OSContPad *contPad) {
    SDL_assert(touch != NULL && contPad != NULL);
    contPad->button |= touch->button;
    if (MeasureStickSquared(touch) > MeasureStickSquared(contPad)) {
        contPad->stick_x = touch->stick_x;
        contPad->stick_y = touch->stick_y;
    }
}

// --- The shapes ---

static OverlayShape MakeShape(
    float x,
    float y,
    float radius,
    uint32_t color,
    bool isHeld,
    OverlayStyle style,
    OverlayLabel label
) {
    OverlayShape shape;

    SDL_zero(shape);
    shape.x = x;
    shape.y = y;
    shape.radius = radius;
    shape.color = color | ((uint32_t)(isHeld ? HELD_ALPHA : REST_ALPHA) << 24);
    shape.style = (uint8_t)style;
    shape.label = (uint8_t)label;
    return shape;
}

int DescribeTouchControls(const TouchPad *pad, const TouchLayout *layout, OverlayShape *shapes) {
    const ControlPlace *stickPlace = &f_Places[TOUCH_CONTROL_STICK];
    const TouchCircle *stick;
    const TouchFinger *stickFinger;
    bool isHeld[TOUCH_CONTROL_COUNT];
    float centerX;
    float centerY;
    float knobX = 0.0f;
    float knobY = 0.0f;
    int count = 0;
    int i;

    SDL_assert(pad != NULL && layout != NULL && shapes != NULL);
    stick = &layout->controls[TOUCH_CONTROL_STICK];
    centerX = stick->x;
    centerY = stick->y;
    SDL_zeroa(isHeld);
    for (i = 0; i < TOUCH_MAX_FINGERS; i++) {
        if (pad->fingers[i].control != TOUCH_CONTROL_NONE) {
            isHeld[pad->fingers[i].control] = true;
        }
    }

    for (i = TOUCH_CONTROL_STICK + 1; i < TOUCH_CONTROL_COUNT; i++) {
        const TouchCircle *circle = &layout->controls[i];

        shapes[count++] = MakeShape(
            circle->x,
            circle->y,
            circle->radius,
            f_Places[i].color,
            isHeld[i],
            OVERLAY_STYLE_DISC,
            f_Places[i].label
        );
    }

    // The stick sits where the thumb landed while it's held
    stickFinger = FindStickFinger(pad);
    if (stickFinger != NULL) {
        centerX = stickFinger->startX;
        centerY = stickFinger->startY;
        MeasureKnob(stickFinger, stick->radius, &knobX, &knobY);
    }

    shapes[count++] = MakeShape(
        centerX,
        centerY,
        stick->radius,
        stickPlace->color,
        isHeld[TOUCH_CONTROL_STICK],
        OVERLAY_STYLE_RING,
        OVERLAY_LABEL_NONE
    );
    shapes[count++] = MakeShape(
        centerX + knobX,
        centerY + knobY,
        stick->radius * KNOB_SHARE,
        stickPlace->color,
        isHeld[TOUCH_CONTROL_STICK],
        OVERLAY_STYLE_DISC,
        OVERLAY_LABEL_NONE
    );
    SDL_assert(count == TOUCH_SHAPE_COUNT);
    return count;
}
