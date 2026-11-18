/*
 * The pad mapping: how the host's gamepads and keyboard play an N64
 * controller. Each of the N64's controls, a button or one of the stick's
 * four directions, is bound to up to PAD_SOURCES_PER_CONTROL of the host's
 * keys, gamepad buttons and half-axes.
 *
 * Each source is pushed from 0 to PAD_AXIS_MAX: a key or button all the
 * way when held, a half-axis by how far its axis goes its way. A control
 * takes the furthest of its sources. A button is pressed past half way,
 * and the stick goes right by how far its right control is pushed less its
 * left, and up by its up less its down.
 */

#ifndef PORT_PLATFORM_PAD_MAPPING_H
#define PORT_PLATFORM_PAD_MAPPING_H

#include <stdbool.h>
#include <stdint.h>
#include <ultra64.h>

#define PAD_AXIS_MAX 32767
#define PAD_SOURCES_PER_CONTROL 4
// The N64 stick's full throw, which input scripts hold to
#define PAD_STICK_RANGE_MAX 80

// What a host gamepad holds at one moment, in SDL's terms: a bit for each
// SDL_GamepadButton, and each SDL_GamepadAxis's value
typedef struct HostPadState {
    uint32_t buttons;
    int16_t axes[6];
} HostPadState;

typedef enum PadControl {
    PAD_CONTROL_A,
    PAD_CONTROL_B,
    PAD_CONTROL_Z,
    PAD_CONTROL_START,
    PAD_CONTROL_L,
    PAD_CONTROL_R,
    PAD_CONTROL_C_UP,
    PAD_CONTROL_C_DOWN,
    PAD_CONTROL_C_LEFT,
    PAD_CONTROL_C_RIGHT,
    PAD_CONTROL_D_UP,
    PAD_CONTROL_D_DOWN,
    PAD_CONTROL_D_LEFT,
    PAD_CONTROL_D_RIGHT,
    PAD_CONTROL_STICK_UP,
    PAD_CONTROL_STICK_DOWN,
    PAD_CONTROL_STICK_LEFT,
    PAD_CONTROL_STICK_RIGHT,
    PAD_CONTROL_COUNT
} PadControl;

typedef enum PadSourceKind {
    PAD_SOURCE_NONE,
    PAD_SOURCE_KEY, // code is an SDL_Scancode
    PAD_SOURCE_BUTTON, // code is an SDL_GamepadButton
    PAD_SOURCE_AXIS_PLUS, // code is an SDL_GamepadAxis, pushed positive
    PAD_SOURCE_AXIS_MINUS // The same, pushed negative
} PadSourceKind;

typedef struct PadSource {
    uint16_t kind; // A PadSourceKind
    uint16_t code;
} PadSource;

// How the stick's throw, from 0 to PAD_AXIS_MAX, becomes the N64's: none
// up to deadZone, then in proportion up to range at fullThrow and past it
typedef struct StickShape {
    int deadZone; // From 0, less than fullThrow
    int fullThrow; // Up to PAD_AXIS_MAX
    int range; // From 1 to PAD_STICK_RANGE_MAX
} StickShape;

// Unused sources are PAD_SOURCE_NONE, after the used ones
typedef struct PadBindings {
    PadSource sources[PAD_CONTROL_COUNT][PAD_SOURCES_PER_CONTROL];
    StickShape stick;
} PadBindings;

// The layout follows the N64 controller's shape on a modern pad: the face
// buttons give A and B, the triggers Z and R, the left shoulder L, and the
// right stick the C buttons. On the keyboard, X and C are A and B, Z is Z,
// Return is Start, Q and W are L and R, I, J, K and L the C buttons, T, F,
// G and H the D-pad, and the arrow keys the stick.
void SetDefaultPadBindings(PadBindings *bindings);

// Whether a stick's shape is one that MapHostStick takes
bool IsStickShapeValid(const StickShape *shape);

// Binds one more source to a control, or gives false when it has
// PAD_SOURCES_PER_CONTROL already
bool AddPadSource(PadBindings *bindings, PadControl control, PadSource source);

// Unbinds all of a control's sources
void ClearPadSources(PadBindings *bindings, PadControl control);

// Makes an N64 controller's state from a host gamepad's and, unless keys is
// NULL, the keyboard's, given as SDL_GetKeyboardState's array
void MapHostPad(
    const PadBindings *bindings,
    const HostPadState *host,
    const bool *keys,
    OSContPad *pad
);

// Scales a stick's position, each axis from -PAD_AXIS_MAX to PAD_AXIS_MAX
// with up positive, to the N64's, keeping its direction
void MapHostStick(const StickShape *shape, int x, int y, s8 *stickX, s8 *stickY);

// Puts a GameCube controller's buttons where the N64's are, as Nintendo's
// own N64 games on the GameCube had them: its L trigger, under the left
// index finger as the N64's Z is, is already Z, and its R trigger R, so its
// Z button, which SDL calls the right shoulder, becomes the left shoulder,
// the N64's L
void ArrangeGameCubePad(HostPadState *host);

#endif
