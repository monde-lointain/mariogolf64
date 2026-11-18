/*
 * The pad mapping: how the host's gamepads and keyboard play an N64
 * controller.
 */

#include "pad_mapping.h"

#include <SDL3/SDL.h>

// How far a control goes before its button counts as pressed
#define PRESS_THRESHOLD 16000
// The default stick's dead zone and full throw, out of PAD_AXIS_MAX
#define DEFAULT_DEAD_ZONE 7000
#define DEFAULT_FULL_THROW 30000

// A control and one of its default sources
typedef struct DefaultBinding {
    PadControl control;
    PadSource source;
} DefaultBinding;

// The N64 button that each control below the stick's presses
static const u16 f_ControlButtons[PAD_CONTROL_STICK_UP] = {
    [PAD_CONTROL_A] = A_BUTTON,
    [PAD_CONTROL_B] = B_BUTTON,
    [PAD_CONTROL_Z] = Z_TRIG,
    [PAD_CONTROL_START] = START_BUTTON,
    [PAD_CONTROL_L] = L_TRIG,
    [PAD_CONTROL_R] = R_TRIG,
    [PAD_CONTROL_C_UP] = U_CBUTTONS,
    [PAD_CONTROL_C_DOWN] = D_CBUTTONS,
    [PAD_CONTROL_C_LEFT] = L_CBUTTONS,
    [PAD_CONTROL_C_RIGHT] = R_CBUTTONS,
    [PAD_CONTROL_D_UP] = U_JPAD,
    [PAD_CONTROL_D_DOWN] = D_JPAD,
    [PAD_CONTROL_D_LEFT] = L_JPAD,
    [PAD_CONTROL_D_RIGHT] = R_JPAD,
};

#define KEY(control, scancode) \
    { \
        (control), { \
            PAD_SOURCE_KEY, (scancode) \
        } \
    }
#define BUTTON(control, button) \
    { \
        (control), { \
            PAD_SOURCE_BUTTON, (button) \
        } \
    }
#define AXIS_PLUS(control, axis) \
    { \
        (control), { \
            PAD_SOURCE_AXIS_PLUS, (axis) \
        } \
    }
#define AXIS_MINUS(control, axis) \
    { \
        (control), { \
            PAD_SOURCE_AXIS_MINUS, (axis) \
        } \
    }

static const DefaultBinding f_DefaultBindings[] = {
    KEY(PAD_CONTROL_A, SDL_SCANCODE_X),
    BUTTON(PAD_CONTROL_A, SDL_GAMEPAD_BUTTON_SOUTH),
    KEY(PAD_CONTROL_B, SDL_SCANCODE_C),
    BUTTON(PAD_CONTROL_B, SDL_GAMEPAD_BUTTON_WEST),
    BUTTON(PAD_CONTROL_B, SDL_GAMEPAD_BUTTON_EAST),
    KEY(PAD_CONTROL_Z, SDL_SCANCODE_Z),
    AXIS_PLUS(PAD_CONTROL_Z, SDL_GAMEPAD_AXIS_LEFT_TRIGGER),
    KEY(PAD_CONTROL_START, SDL_SCANCODE_RETURN),
    BUTTON(PAD_CONTROL_START, SDL_GAMEPAD_BUTTON_START),
    KEY(PAD_CONTROL_L, SDL_SCANCODE_Q),
    BUTTON(PAD_CONTROL_L, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER),
    KEY(PAD_CONTROL_R, SDL_SCANCODE_W),
    BUTTON(PAD_CONTROL_R, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER),
    AXIS_PLUS(PAD_CONTROL_R, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER),
    KEY(PAD_CONTROL_C_UP, SDL_SCANCODE_I),
    BUTTON(PAD_CONTROL_C_UP, SDL_GAMEPAD_BUTTON_NORTH),
    AXIS_MINUS(PAD_CONTROL_C_UP, SDL_GAMEPAD_AXIS_RIGHTY),
    KEY(PAD_CONTROL_C_DOWN, SDL_SCANCODE_K),
    AXIS_PLUS(PAD_CONTROL_C_DOWN, SDL_GAMEPAD_AXIS_RIGHTY),
    KEY(PAD_CONTROL_C_LEFT, SDL_SCANCODE_J),
    AXIS_MINUS(PAD_CONTROL_C_LEFT, SDL_GAMEPAD_AXIS_RIGHTX),
    KEY(PAD_CONTROL_C_RIGHT, SDL_SCANCODE_L),
    AXIS_PLUS(PAD_CONTROL_C_RIGHT, SDL_GAMEPAD_AXIS_RIGHTX),
    KEY(PAD_CONTROL_D_UP, SDL_SCANCODE_T),
    BUTTON(PAD_CONTROL_D_UP, SDL_GAMEPAD_BUTTON_DPAD_UP),
    KEY(PAD_CONTROL_D_DOWN, SDL_SCANCODE_G),
    BUTTON(PAD_CONTROL_D_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN),
    KEY(PAD_CONTROL_D_LEFT, SDL_SCANCODE_F),
    BUTTON(PAD_CONTROL_D_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT),
    KEY(PAD_CONTROL_D_RIGHT, SDL_SCANCODE_H),
    BUTTON(PAD_CONTROL_D_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT),
    KEY(PAD_CONTROL_STICK_UP, SDL_SCANCODE_UP),
    AXIS_MINUS(PAD_CONTROL_STICK_UP, SDL_GAMEPAD_AXIS_LEFTY),
    KEY(PAD_CONTROL_STICK_DOWN, SDL_SCANCODE_DOWN),
    AXIS_PLUS(PAD_CONTROL_STICK_DOWN, SDL_GAMEPAD_AXIS_LEFTY),
    KEY(PAD_CONTROL_STICK_LEFT, SDL_SCANCODE_LEFT),
    AXIS_MINUS(PAD_CONTROL_STICK_LEFT, SDL_GAMEPAD_AXIS_LEFTX),
    KEY(PAD_CONTROL_STICK_RIGHT, SDL_SCANCODE_RIGHT),
    AXIS_PLUS(PAD_CONTROL_STICK_RIGHT, SDL_GAMEPAD_AXIS_LEFTX),
};

// --- Bindings ---

void SetDefaultPadBindings(PadBindings *bindings) {
    size_t i;

    SDL_assert(bindings != NULL);
    SDL_zerop(bindings);
    for (i = 0; i < SDL_arraysize(f_DefaultBindings); i++) {
        const bool isAdded =
            AddPadSource(bindings, f_DefaultBindings[i].control, f_DefaultBindings[i].source);

        SDL_assert(isAdded);
        (void)isAdded;
    }

    bindings->stick.deadZone = DEFAULT_DEAD_ZONE;
    bindings->stick.fullThrow = DEFAULT_FULL_THROW;
    bindings->stick.range = PAD_STICK_RANGE_MAX;
}

bool IsStickShapeValid(const StickShape *shape) {
    return shape->deadZone >= 0 &&
        shape->deadZone < shape->fullThrow &&
        shape->fullThrow <= PAD_AXIS_MAX &&
        shape->range >= 1 &&
        shape->range <= PAD_STICK_RANGE_MAX;
}

bool AddPadSource(PadBindings *bindings, PadControl control, PadSource source) {
    PadSource *sources;
    int i;

    SDL_assert(bindings != NULL && control < PAD_CONTROL_COUNT);
    SDL_assert(source.kind != PAD_SOURCE_NONE);
    sources = bindings->sources[control];
    for (i = 0; i < PAD_SOURCES_PER_CONTROL; i++) {
        if (sources[i].kind == PAD_SOURCE_NONE) {
            sources[i] = source;
            return true;
        }
    }

    return false;
}

void ClearPadSources(PadBindings *bindings, PadControl control) {
    SDL_assert(bindings != NULL && control < PAD_CONTROL_COUNT);
    SDL_zeroa(bindings->sources[control]);
}

// --- Mapping ---

// How far a source is pushed, from 0 to PAD_AXIS_MAX
static int MeasureSource(PadSource source, const HostPadState *host, const bool *keys) {
    switch (source.kind) {
        case PAD_SOURCE_KEY:
            return (keys != NULL && source.code < SDL_SCANCODE_COUNT && keys[source.code]) ?
                PAD_AXIS_MAX :
                0;
        case PAD_SOURCE_BUTTON:
            return (source.code < 32 && (host->buttons & (1u << source.code)) != 0) ? PAD_AXIS_MAX :
                                                                                      0;
        case PAD_SOURCE_AXIS_PLUS:
            return (source.code < SDL_arraysize(host->axes)) ? SDL_max(host->axes[source.code], 0) :
                                                               0;
        case PAD_SOURCE_AXIS_MINUS:
            return (source.code < SDL_arraysize(host->axes)) ?
                SDL_clamp(-(int)host->axes[source.code], 0, PAD_AXIS_MAX) :
                0;
        default:
            return 0;
    }
}

// How far a control is pushed: the furthest of its sources
static int MeasureControl(
    const PadBindings *bindings,
    PadControl control,
    const HostPadState *host,
    const bool *keys
) {
    int furthest = 0;
    int i;

    for (i = 0; i < PAD_SOURCES_PER_CONTROL; i++) {
        furthest = SDL_max(furthest, MeasureSource(bindings->sources[control][i], host, keys));
    }

    return furthest;
}

void MapHostStick(const StickShape *shape, int x, int y, s8 *stickX, s8 *stickY) {
    const float hostX = (float)x;
    const float hostY = (float)y;
    const float length = SDL_sqrtf((hostX * hostX) + (hostY * hostY));
    const float span = (float)(shape->fullThrow - shape->deadZone);
    float scale;

    SDL_assert(IsStickShapeValid(shape));
    if (length <= (float)shape->deadZone) {
        *stickX = 0;
        *stickY = 0;
        return;
    }

    // The throw past the dead zone, as a share of the rest, along the
    // stick's own direction, so that neither axis goes past the range
    scale = SDL_min((length - (float)shape->deadZone) / span, 1.0f) * (float)shape->range / length;
    *stickX = (s8)SDL_lroundf(hostX * scale);
    *stickY = (s8)SDL_lroundf(hostY * scale);
}

void MapHostPad(
    const PadBindings *bindings,
    const HostPadState *host,
    const bool *keys,
    OSContPad *pad
) {
    u16 buttons = 0;
    int control;

    SDL_assert(bindings != NULL && host != NULL && pad != NULL);
    for (control = 0; control < PAD_CONTROL_STICK_UP; control++) {
        if (MeasureControl(bindings, (PadControl)control, host, keys) > PRESS_THRESHOLD) {
            buttons |= f_ControlButtons[control];
        }
    }

    pad->button = buttons;
    MapHostStick(
        &bindings->stick,
        MeasureControl(bindings, PAD_CONTROL_STICK_RIGHT, host, keys) -
            MeasureControl(bindings, PAD_CONTROL_STICK_LEFT, host, keys),
        MeasureControl(bindings, PAD_CONTROL_STICK_UP, host, keys) -
            MeasureControl(bindings, PAD_CONTROL_STICK_DOWN, host, keys),
        &pad->stick_x,
        &pad->stick_y
    );
    pad->errno = 0;
}

void ArrangeGameCubePad(HostPadState *host) {
    const uint32_t zButton = 1u << SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;

    SDL_assert(host != NULL);
    if ((host->buttons & zButton) != 0) {
        host->buttons &= ~zButton;
        host->buttons |= 1u << SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    }
}
