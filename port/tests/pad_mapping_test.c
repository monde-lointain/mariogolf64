/*
 * Tests of the pad mapping, which binds the N64 controller's buttons and
 * stick to the host's gamepad buttons, axes and keys.
 */

#include <SDL3/SDL.h>
#include <ultra64.h>

#include "pad_mapping.h"
#include "test_support.h"

#define AXIS_MAX PAD_AXIS_MAX

static PadBindings f_Defaults;

// --- The default layout ---

static void TestStick(void) {
    s8 x;
    s8 y;

    // Within the dead zone, the stick rests
    MapHostStick(&f_Defaults.stick, 5000, 5000, &x, &y);
    EXPECT(x == 0 && y == 0);
    // All the way, the N64's full throw
    MapHostStick(&f_Defaults.stick, AXIS_MAX, 0, &x, &y);
    EXPECT(x == 80 && y == 0);
    MapHostStick(&f_Defaults.stick, 0, AXIS_MAX, &x, &y);
    EXPECT(x == 0 && y == 80);
    MapHostStick(&f_Defaults.stick, 0, -AXIS_MAX, &x, &y);
    EXPECT(x == 0 && y == -80);
    // A diagonal keeps its direction and doesn't go past the full throw:
    // 80 over the square root of 2
    MapHostStick(&f_Defaults.stick, AXIS_MAX, AXIS_MAX, &x, &y);
    EXPECT(x == 57 && y == 57);
    // Halfway past the dead zone is about halfway
    MapHostStick(&f_Defaults.stick, 18500, 0, &x, &y);
    EXPECT(x >= 38 && x <= 42);
}

static void TestPadButtons(void) {
    HostPadState host;
    OSContPad pad;

    SDL_zero(host);
    host.buttons = (1u << SDL_GAMEPAD_BUTTON_SOUTH) | (1u << SDL_GAMEPAD_BUTTON_START) |
        (1u << SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    host.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = AXIS_MAX;
    host.axes[SDL_GAMEPAD_AXIS_RIGHTX] = -AXIS_MAX;
    host.axes[SDL_GAMEPAD_AXIS_LEFTY] = -AXIS_MAX;
    MapHostPad(&f_Defaults, &host, NULL, &pad);
    EXPECT(pad.button == (A_BUTTON | START_BUTTON | L_JPAD | Z_TRIG | L_CBUTTONS));
    EXPECT(pad.stick_x == 0 && pad.stick_y == 80);

    // A trigger only partly in doesn't press
    SDL_zero(host);
    host.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = 8000;
    host.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = 8000;
    host.axes[SDL_GAMEPAD_AXIS_RIGHTY] = AXIS_MAX;
    MapHostPad(&f_Defaults, &host, NULL, &pad);
    EXPECT(pad.button == D_CBUTTONS);
}

// A GameCube controller's Z button is the N64's L, and its analog L and R
// triggers are Z and R
static void TestGameCubePad(void) {
    HostPadState host;
    OSContPad pad;

    SDL_zero(host);
    host.buttons = (1u << SDL_GAMEPAD_BUTTON_SOUTH) | (1u << SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    ArrangeGameCubePad(&host);
    MapHostPad(&f_Defaults, &host, NULL, &pad);
    EXPECT(pad.button == (A_BUTTON | L_TRIG));

    SDL_zero(host);
    host.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = AXIS_MAX;
    host.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = AXIS_MAX;
    ArrangeGameCubePad(&host);
    MapHostPad(&f_Defaults, &host, NULL, &pad);
    EXPECT(pad.button == (Z_TRIG | R_TRIG));
}

static void TestKeyboard(void) {
    static bool keys[SDL_SCANCODE_COUNT];
    HostPadState host;
    OSContPad pad;

    SDL_zero(host);
    keys[SDL_SCANCODE_X] = true;
    keys[SDL_SCANCODE_Z] = true;
    keys[SDL_SCANCODE_I] = true;
    keys[SDL_SCANCODE_RIGHT] = true;
    MapHostPad(&f_Defaults, &host, keys, &pad);
    EXPECT(pad.button == (A_BUTTON | Z_TRIG | U_CBUTTONS));
    EXPECT(pad.stick_x == 80 && pad.stick_y == 0);
}

// A host button, axis or key that presses one N64 button
typedef struct PressedButton {
    int code; // The SDL_GamepadButton, SDL_GamepadAxis or SDL_Scancode
    int axisValue; // For an axis, where it is pushed to
    u16 n64;
} PressedButton;

// Each of the gamepad's buttons that the N64's buttons have
static const PressedButton f_GamepadButtons[] = {
    {SDL_GAMEPAD_BUTTON_SOUTH, 0, A_BUTTON},
    {SDL_GAMEPAD_BUTTON_WEST, 0, B_BUTTON},
    {SDL_GAMEPAD_BUTTON_EAST, 0, B_BUTTON},
    {SDL_GAMEPAD_BUTTON_START, 0, START_BUTTON},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 0, L_TRIG},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 0, R_TRIG},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, 0, U_JPAD},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, 0, D_JPAD},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, 0, L_JPAD},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 0, R_JPAD},
    {SDL_GAMEPAD_BUTTON_NORTH, 0, U_CBUTTONS},
};

// Each of the gamepad's axes that presses a button, at full throw
static const PressedButton f_GamepadAxes[] = {
    {SDL_GAMEPAD_AXIS_LEFT_TRIGGER, AXIS_MAX, Z_TRIG},
    {SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, AXIS_MAX, R_TRIG},
    {SDL_GAMEPAD_AXIS_RIGHTY, -AXIS_MAX - 1, U_CBUTTONS},
    {SDL_GAMEPAD_AXIS_RIGHTY, AXIS_MAX, D_CBUTTONS},
    {SDL_GAMEPAD_AXIS_RIGHTX, -AXIS_MAX - 1, L_CBUTTONS},
    {SDL_GAMEPAD_AXIS_RIGHTX, AXIS_MAX, R_CBUTTONS},
};

// Each key that presses a button
static const PressedButton f_KeyButtons[] = {
    {SDL_SCANCODE_X, 0, A_BUTTON},
    {SDL_SCANCODE_C, 0, B_BUTTON},
    {SDL_SCANCODE_Z, 0, Z_TRIG},
    {SDL_SCANCODE_RETURN, 0, START_BUTTON},
    {SDL_SCANCODE_Q, 0, L_TRIG},
    {SDL_SCANCODE_W, 0, R_TRIG},
    {SDL_SCANCODE_I, 0, U_CBUTTONS},
    {SDL_SCANCODE_K, 0, D_CBUTTONS},
    {SDL_SCANCODE_J, 0, L_CBUTTONS},
    {SDL_SCANCODE_L, 0, R_CBUTTONS},
    {SDL_SCANCODE_T, 0, U_JPAD},
    {SDL_SCANCODE_G, 0, D_JPAD},
    {SDL_SCANCODE_F, 0, L_JPAD},
    {SDL_SCANCODE_H, 0, R_JPAD},
};

// Each key that pushes the stick, and where to
typedef struct StickKey {
    SDL_Scancode key;
    s8 x;
    s8 y;
} StickKey;

static const StickKey f_StickKeys[] = {
    {SDL_SCANCODE_UP, 0, 80},
    {SDL_SCANCODE_DOWN, 0, -80},
    {SDL_SCANCODE_LEFT, -80, 0},
    {SDL_SCANCODE_RIGHT, 80, 0},
};

// Maps a host controller with nothing held but the keyboard's keys
static void MapKeys(const bool *keys, OSContPad *pad) {
    HostPadState host;

    SDL_zero(host);
    MapHostPad(&f_Defaults, &host, keys, pad);
}

// Each control on its own presses its button and nothing else, and leaves
// the stick at rest
static void TestEachControlPressesItsButton(void) {
    static bool keys[SDL_SCANCODE_COUNT];
    HostPadState host;
    OSContPad pad;
    size_t i;

    for (i = 0; i < SDL_arraysize(f_GamepadButtons); i++) {
        SDL_zero(host);
        host.buttons = 1u << f_GamepadButtons[i].code;
        MapHostPad(&f_Defaults, &host, NULL, &pad);
        EXPECT(pad.button == f_GamepadButtons[i].n64);
        EXPECT(pad.stick_x == 0 && pad.stick_y == 0);
    }

    for (i = 0; i < SDL_arraysize(f_GamepadAxes); i++) {
        SDL_zero(host);
        host.axes[f_GamepadAxes[i].code] = (int16_t)f_GamepadAxes[i].axisValue;
        MapHostPad(&f_Defaults, &host, NULL, &pad);
        EXPECT(pad.button == f_GamepadAxes[i].n64);
        EXPECT(pad.stick_x == 0 && pad.stick_y == 0);
    }

    for (i = 0; i < SDL_arraysize(f_KeyButtons); i++) {
        keys[f_KeyButtons[i].code] = true;
        MapKeys(keys, &pad);
        keys[f_KeyButtons[i].code] = false;
        EXPECT(pad.button == f_KeyButtons[i].n64);
        EXPECT(pad.stick_x == 0 && pad.stick_y == 0);
    }

    for (i = 0; i < SDL_arraysize(f_StickKeys); i++) {
        keys[f_StickKeys[i].key] = true;
        MapKeys(keys, &pad);
        keys[f_StickKeys[i].key] = false;
        EXPECT(pad.button == 0);
        EXPECT(pad.stick_x == f_StickKeys[i].x && pad.stick_y == f_StickKeys[i].y);
    }
}

// Nothing held presses nothing; no other button or key does anything
static void TestOtherControlsPressNothing(void) {
    static bool keys[SDL_SCANCODE_COUNT];
    HostPadState host;
    OSContPad pad;
    size_t i;

    SDL_zero(host);
    MapHostPad(&f_Defaults, &host, NULL, &pad);
    EXPECT(pad.button == 0 && pad.stick_x == 0 && pad.stick_y == 0);

    host.buttons = (1u << SDL_GAMEPAD_BUTTON_BACK) | (1u << SDL_GAMEPAD_BUTTON_GUIDE) |
        (1u << SDL_GAMEPAD_BUTTON_LEFT_STICK) | (1u << SDL_GAMEPAD_BUTTON_RIGHT_STICK) |
        (1u << SDL_GAMEPAD_BUTTON_MISC1);
    MapHostPad(&f_Defaults, &host, NULL, &pad);
    EXPECT(pad.button == 0);

    for (i = 0; i < SDL_SCANCODE_COUNT; i++) {
        keys[i] = true;
    }

    for (i = 0; i < SDL_arraysize(f_KeyButtons); i++) {
        keys[f_KeyButtons[i].code] = false;
    }

    for (i = 0; i < SDL_arraysize(f_StickKeys); i++) {
        keys[f_StickKeys[i].key] = false;
    }

    MapKeys(keys, &pad);
    EXPECT(pad.button == 0 && pad.stick_x == 0 && pad.stick_y == 0);
}

// --- Rebinding ---

// The defaults replace whatever bindings held before
static void TestDefaultsReplaceEverything(void) {
    PadBindings bindings;

    SDL_memset(&bindings, 0xA5, sizeof(bindings));
    SetDefaultPadBindings(&bindings);
    EXPECT(SDL_memcmp(&bindings, &f_Defaults, sizeof(bindings)) == 0);
}

// A source added to a control presses it, besides its defaults, and a
// control cleared of its sources presses no more
static void TestAddedSourcePresses(void) {
    static bool keys[SDL_SCANCODE_COUNT];
    PadBindings bindings = f_Defaults;
    HostPadState host;
    OSContPad pad;

    SDL_zero(host);
    EXPECT(
        AddPadSource(&bindings, PAD_CONTROL_A, (PadSource) {PAD_SOURCE_KEY, SDL_SCANCODE_SPACE})
    );
    keys[SDL_SCANCODE_SPACE] = true;
    MapHostPad(&bindings, &host, keys, &pad);
    keys[SDL_SCANCODE_SPACE] = false;
    EXPECT(pad.button == A_BUTTON);
    keys[SDL_SCANCODE_X] = true;
    MapHostPad(&bindings, &host, keys, &pad);
    EXPECT(pad.button == A_BUTTON);

    ClearPadSources(&bindings, PAD_CONTROL_A);
    host.buttons = 1u << SDL_GAMEPAD_BUTTON_SOUTH;
    MapHostPad(&bindings, &host, keys, &pad);
    keys[SDL_SCANCODE_X] = false;
    EXPECT(pad.button == 0);
}

// A control takes PAD_SOURCES_PER_CONTROL sources and no more, and the
// last of them counts
static void TestControlHoldsItsSourcesOnly(void) {
    static bool keys[SDL_SCANCODE_COUNT];
    PadBindings bindings;
    HostPadState host;
    OSContPad pad;
    int i;

    SDL_zero(bindings);
    bindings.stick = f_Defaults.stick;
    SDL_zero(host);
    for (i = 0; i < PAD_SOURCES_PER_CONTROL; i++) {
        EXPECT(AddPadSource(
            &bindings,
            PAD_CONTROL_Z,
            (PadSource) {PAD_SOURCE_KEY, (uint16_t)(SDL_SCANCODE_A + i)}
        ));
    }

    EXPECT(!AddPadSource(&bindings, PAD_CONTROL_Z, (PadSource) {PAD_SOURCE_KEY, SDL_SCANCODE_Y}));
    keys[SDL_SCANCODE_A + PAD_SOURCES_PER_CONTROL - 1] = true;
    MapHostPad(&bindings, &host, keys, &pad);
    keys[SDL_SCANCODE_A + PAD_SOURCES_PER_CONTROL - 1] = false;
    EXPECT(pad.button == Z_TRIG);
    keys[SDL_SCANCODE_Y] = true;
    MapHostPad(&bindings, &host, keys, &pad);
    keys[SDL_SCANCODE_Y] = false;
    EXPECT(pad.button == 0);

    // The control beside it is untouched
    keys[SDL_SCANCODE_A] = true;
    ClearPadSources(&bindings, PAD_CONTROL_START);
    MapHostPad(&bindings, &host, keys, &pad);
    keys[SDL_SCANCODE_A] = false;
    EXPECT(pad.button == Z_TRIG);
}

// A button may be bound to the stick, and a half-axis to a button: an
// axis presses a button past half way, and not at half way
static void TestSourcesOfAnyKind(void) {
    PadBindings bindings = f_Defaults;
    HostPadState host;
    OSContPad pad;

    EXPECT(AddPadSource(
        &bindings,
        PAD_CONTROL_STICK_LEFT,
        (PadSource) {PAD_SOURCE_BUTTON, SDL_GAMEPAD_BUTTON_BACK}
    ));
    EXPECT(AddPadSource(
        &bindings,
        PAD_CONTROL_START,
        (PadSource) {PAD_SOURCE_AXIS_MINUS, SDL_GAMEPAD_AXIS_LEFT_TRIGGER}
    ));
    SDL_zero(host);
    host.buttons = 1u << SDL_GAMEPAD_BUTTON_BACK;
    MapHostPad(&bindings, &host, NULL, &pad);
    EXPECT(pad.button == 0 && pad.stick_x == -80 && pad.stick_y == 0);

    SDL_zero(host);
    host.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = -16001;
    MapHostPad(&bindings, &host, NULL, &pad);
    EXPECT(pad.button == START_BUTTON);
    host.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = -16000;
    MapHostPad(&bindings, &host, NULL, &pad);
    EXPECT(pad.button == 0);
}

// --- Combining sources ---

// Opposed keys cancel, on the stick and on its own
static void TestOpposedKeysCancel(void) {
    static bool keys[SDL_SCANCODE_COUNT];
    HostPadState host;
    OSContPad pad;

    SDL_zero(host);
    keys[SDL_SCANCODE_LEFT] = true;
    keys[SDL_SCANCODE_RIGHT] = true;
    keys[SDL_SCANCODE_UP] = true;
    MapHostPad(&f_Defaults, &host, keys, &pad);
    EXPECT(pad.stick_x == 0 && pad.stick_y == 80);
}

// A key and the gamepad's stick on one control push it as far as the
// further of them, and opposed, the difference
static void TestKeyAndStickCombine(void) {
    static bool keys[SDL_SCANCODE_COUNT];
    HostPadState host;
    OSContPad pad;

    SDL_zero(host);
    host.axes[SDL_GAMEPAD_AXIS_LEFTX] = 18500;
    MapHostPad(&f_Defaults, &host, keys, &pad);
    EXPECT(pad.stick_x >= 38 && pad.stick_x <= 42);
    keys[SDL_SCANCODE_RIGHT] = true;
    MapHostPad(&f_Defaults, &host, keys, &pad);
    keys[SDL_SCANCODE_RIGHT] = false;
    EXPECT(pad.stick_x == 80);

    // The key's full throw less the stick's 18500 leaves 14267, and past
    // the dead zone of 7000, 7267 of 23000 is 25 of 80
    keys[SDL_SCANCODE_LEFT] = true;
    MapHostPad(&f_Defaults, &host, keys, &pad);
    keys[SDL_SCANCODE_LEFT] = false;
    EXPECT(pad.stick_x == -25);
}

// The keyboard plays only where it is given, and the axis's far negative
// end, one past PAD_AXIS_MAX, is the full throw
static void TestKeysOnlyWhereGiven(void) {
    static bool keys[SDL_SCANCODE_COUNT];
    HostPadState host;
    OSContPad pad;

    SDL_zero(host);
    keys[SDL_SCANCODE_X] = true;
    keys[SDL_SCANCODE_UP] = true;
    MapHostPad(&f_Defaults, &host, NULL, &pad);
    EXPECT(pad.button == 0 && pad.stick_y == 0);
    MapHostPad(&f_Defaults, &host, keys, &pad);
    EXPECT(pad.button == A_BUTTON && pad.stick_y == 80);

    host.axes[SDL_GAMEPAD_AXIS_LEFTX] = -AXIS_MAX - 1;
    MapHostPad(&f_Defaults, &host, NULL, &pad);
    EXPECT(pad.stick_x == -80 && pad.stick_y == 0);
}

// A source whose code is past its kind's, as no config gives, does nothing
static void TestCodesPastTheirKindsDoNothing(void) {
    static bool keys[SDL_SCANCODE_COUNT + 1];
    PadBindings bindings;
    HostPadState host;
    OSContPad pad;

    SDL_zero(bindings);
    bindings.stick = f_Defaults.stick;
    (void)AddPadSource(&bindings, PAD_CONTROL_A, (PadSource) {PAD_SOURCE_KEY, SDL_SCANCODE_COUNT});
    (void)AddPadSource(&bindings, PAD_CONTROL_B, (PadSource) {PAD_SOURCE_BUTTON, 32});
    (void)AddPadSource(&bindings, PAD_CONTROL_Z, (PadSource) {PAD_SOURCE_AXIS_PLUS, 6});
    (void)AddPadSource(&bindings, PAD_CONTROL_R, (PadSource) {PAD_SOURCE_AXIS_MINUS, 6});
    (void)AddPadSource(&bindings, PAD_CONTROL_L, (PadSource) {PAD_SOURCE_AXIS_MINUS + 1, 0});
    SDL_memset(keys, 1, sizeof(keys));
    SDL_memset(&host, 0x80, sizeof(host));
    MapHostPad(&bindings, &host, keys, &pad);
    EXPECT(pad.button == 0);
}

// --- The stick's shape ---

// A shape's dead zone, full throw and range go where they say
static void TestStickShape(void) {
    const StickShape shape = {1000, 11000, 40};
    s8 x;
    s8 y;

    MapHostStick(&shape, 1000, 0, &x, &y);
    EXPECT(x == 0 && y == 0);
    MapHostStick(&shape, 0, -6000, &x, &y);
    EXPECT(x == 0 && y == -20);
    MapHostStick(&shape, 11000, 0, &x, &y);
    EXPECT(x == 40);
    MapHostStick(&shape, -AXIS_MAX, 0, &x, &y);
    EXPECT(x == -40);
}

// The shapes that MapHostStick takes, and at each edge, those just past
static void TestStickShapeLimits(void) {
    StickShape shape = {0, AXIS_MAX, PAD_STICK_RANGE_MAX};

    EXPECT(IsStickShapeValid(&shape));
    EXPECT(IsStickShapeValid(&f_Defaults.stick));
    shape.deadZone = -1;
    EXPECT(!IsStickShapeValid(&shape));
    shape.deadZone = AXIS_MAX - 1;
    EXPECT(IsStickShapeValid(&shape));
    shape.deadZone = AXIS_MAX;
    EXPECT(!IsStickShapeValid(&shape));
    shape.deadZone = 0;
    shape.fullThrow = AXIS_MAX + 1;
    EXPECT(!IsStickShapeValid(&shape));
    shape.fullThrow = 1;
    EXPECT(IsStickShapeValid(&shape));
    shape.range = PAD_STICK_RANGE_MAX + 1;
    EXPECT(!IsStickShapeValid(&shape));
    shape.range = 1;
    EXPECT(IsStickShapeValid(&shape));
    shape.range = 0;
    EXPECT(!IsStickShapeValid(&shape));
}

int main(void) {
    SetDefaultPadBindings(&f_Defaults);
    TestStick();
    TestPadButtons();
    TestGameCubePad();
    TestKeyboard();
    TestEachControlPressesItsButton();
    TestOtherControlsPressNothing();
    TestDefaultsReplaceEverything();
    TestAddedSourcePresses();
    TestControlHoldsItsSourcesOnly();
    TestSourcesOfAnyKind();
    TestOpposedKeysCancel();
    TestKeyAndStickCombine();
    TestKeysOnlyWhereGiven();
    TestCodesPastTheirKindsDoNothing();
    TestStickShape();
    TestStickShapeLimits();
    return ReportTestResults("pad_mapping_test");
}
