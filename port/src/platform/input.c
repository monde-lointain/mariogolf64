/*
 * The controllers. The layout follows the N64 controller's shape on a
 * modern pad: the face buttons give A and B, the triggers Z and R, the left
 * shoulder L, and the right stick the C buttons.
 */

#include "input.h"

#include <SDL3/SDL.h>

#include "port/nusys_host.h"
#include "port/os_host.h"

#define PORT_COUNT MAXCONTROLLERS
#define AXIS_MAX 32767
#define STICK_RANGE 80
// The share of the stick's throw, out of AXIS_MAX, ignored around the
// center and counted as full at the edge
#define STICK_DEAD_ZONE 7000
#define STICK_FULL 30000
// How far a trigger or the right stick goes before it counts as pressed
#define PRESS_THRESHOLD 16000
#define RUMBLE_STRENGTH 0xC000
// SDL stops a rumble after this long unless it is renewed, so a crash
// doesn't leave a motor running
#define RUMBLE_RENEW_MS 250

typedef struct ButtonMapping {
    SDL_GamepadButton host;
    u16 n64;
} ButtonMapping;

// A key that presses a gamepad's button
typedef struct KeyButton {
    SDL_Scancode key;
    SDL_GamepadButton button;
} KeyButton;

// A key that pushes a gamepad's axis to a value
typedef struct KeyAxis {
    SDL_Scancode key;
    SDL_GamepadAxis axis;
    int16_t value;
} KeyAxis;

typedef struct ControllerPort {
    SDL_Gamepad *gamepad;
    SDL_JoystickID instanceId;
    OSContPad pad;
    bool isConnected;
    bool canRumble;
    bool wantsRumble;
    bool isRumbling;
} ControllerPort;

static const ButtonMapping f_ButtonMappings[] = {
    {SDL_GAMEPAD_BUTTON_SOUTH, A_BUTTON},
    {SDL_GAMEPAD_BUTTON_WEST, B_BUTTON},
    {SDL_GAMEPAD_BUTTON_EAST, B_BUTTON},
    {SDL_GAMEPAD_BUTTON_START, START_BUTTON},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, L_TRIG},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, R_TRIG},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, U_JPAD},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, D_JPAD},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, L_JPAD},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, R_JPAD},
    {SDL_GAMEPAD_BUTTON_NORTH, U_CBUTTONS},
};

// The keyboard plays a gamepad: X and C are A and B, Z is Z, Q and W are L
// and R, I, J, K and L the C buttons, T, F, G and H the D-pad, and the
// arrow keys the stick
static const KeyButton f_KeyButtons[] = {
    {SDL_SCANCODE_X, SDL_GAMEPAD_BUTTON_SOUTH},
    {SDL_SCANCODE_C, SDL_GAMEPAD_BUTTON_WEST},
    {SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_START},
    {SDL_SCANCODE_Q, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {SDL_SCANCODE_W, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {SDL_SCANCODE_T, SDL_GAMEPAD_BUTTON_DPAD_UP},
    {SDL_SCANCODE_G, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {SDL_SCANCODE_F, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {SDL_SCANCODE_H, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
};

static const KeyAxis f_KeyAxes[] = {
    {SDL_SCANCODE_Z, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, AXIS_MAX},
    {SDL_SCANCODE_I, SDL_GAMEPAD_AXIS_RIGHTY, -AXIS_MAX},
    {SDL_SCANCODE_K, SDL_GAMEPAD_AXIS_RIGHTY, AXIS_MAX},
    {SDL_SCANCODE_J, SDL_GAMEPAD_AXIS_RIGHTX, -AXIS_MAX},
    {SDL_SCANCODE_L, SDL_GAMEPAD_AXIS_RIGHTX, AXIS_MAX},
    {SDL_SCANCODE_LEFT, SDL_GAMEPAD_AXIS_LEFTX, -AXIS_MAX},
    {SDL_SCANCODE_RIGHT, SDL_GAMEPAD_AXIS_LEFTX, AXIS_MAX},
    {SDL_SCANCODE_UP, SDL_GAMEPAD_AXIS_LEFTY, -AXIS_MAX},
    {SDL_SCANCODE_DOWN, SDL_GAMEPAD_AXIS_LEFTY, AXIS_MAX},
};

static SDL_Mutex *f_Lock; // Guards f_Ports between the platform and the game
static ControllerPort f_Ports[PORT_COUNT];
static Uint64 f_LastRumbleRenewal;
static const InputScript *f_Script;
static InputRecorder *f_Recorder;
static bool f_HasRecordingFailed;
static long f_FirstPortReadCount; // The game's frames, as it reads once each

// --- Mapping ---

static bool IsHostButtonDown(const HostPadState *host, SDL_GamepadButton button) {
    return (host->buttons & (1u << button)) != 0;
}

static void SetHostButton(HostPadState *host, SDL_GamepadButton button) {
    host->buttons |= 1u << button;
}

void ArrangeGameCubePad(HostPadState *host) {
    SDL_assert(host != NULL);
    if (IsHostButtonDown(host, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        host->buttons &= ~(1u << SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        SetHostButton(host, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    }
}

void MapHostStick(int16_t x, int16_t y, s8 *stickX, s8 *stickY) {
    // Up is negative to SDL and positive to the N64
    const float hostX = (float)x;
    const float hostY = -(float)y;
    const float length = SDL_sqrtf((hostX * hostX) + (hostY * hostY));
    float scale;

    if (length <= STICK_DEAD_ZONE) {
        *stickX = 0;
        *stickY = 0;
        return;
    }

    // The throw past the dead zone, as a share of the rest, along the
    // stick's own direction
    scale = SDL_min((length - STICK_DEAD_ZONE) / (STICK_FULL - STICK_DEAD_ZONE), 1.0f) *
        STICK_RANGE / length;
    *stickX = (s8)SDL_clamp((int)SDL_lroundf(hostX * scale), -STICK_RANGE, STICK_RANGE);
    *stickY = (s8)SDL_clamp((int)SDL_lroundf(hostY * scale), -STICK_RANGE, STICK_RANGE);
}

void MapHostPad(const HostPadState *host, OSContPad *pad) {
    const int16_t cameraX = host->axes[SDL_GAMEPAD_AXIS_RIGHTX];
    const int16_t cameraY = host->axes[SDL_GAMEPAD_AXIS_RIGHTY];
    u16 buttons = 0;
    size_t i;

    SDL_assert(host != NULL && pad != NULL);
    for (i = 0; i < SDL_arraysize(f_ButtonMappings); i++) {
        if (IsHostButtonDown(host, f_ButtonMappings[i].host)) {
            buttons |= f_ButtonMappings[i].n64;
        }
    }

    buttons |= (host->axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] > PRESS_THRESHOLD) ? Z_TRIG : 0;
    buttons |= (host->axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] > PRESS_THRESHOLD) ? R_TRIG : 0;
    buttons |= (cameraY < -PRESS_THRESHOLD) ? U_CBUTTONS : 0;
    buttons |= (cameraY > PRESS_THRESHOLD) ? D_CBUTTONS : 0;
    buttons |= (cameraX < -PRESS_THRESHOLD) ? L_CBUTTONS : 0;
    buttons |= (cameraX > PRESS_THRESHOLD) ? R_CBUTTONS : 0;
    pad->button = buttons;
    MapHostStick(
        host->axes[SDL_GAMEPAD_AXIS_LEFTX],
        host->axes[SDL_GAMEPAD_AXIS_LEFTY],
        &pad->stick_x,
        &pad->stick_y
    );
    pad->errno = 0;
}

// A key overrides the gamepad's axis; of two opposed keys, the later wins
void AddKeyboardToHostPad(const bool *keys, HostPadState *host) {
    size_t i;

    SDL_assert(keys != NULL && host != NULL);
    for (i = 0; i < SDL_arraysize(f_KeyButtons); i++) {
        if (keys[f_KeyButtons[i].key]) {
            SetHostButton(host, f_KeyButtons[i].button);
        }
    }

    for (i = 0; i < SDL_arraysize(f_KeyAxes); i++) {
        if (keys[f_KeyAxes[i].key]) {
            host->axes[f_KeyAxes[i].axis] = f_KeyAxes[i].value;
        }
    }
}

// --- The devices ---

static void ReadGamepad(SDL_Gamepad *gamepad, HostPadState *host) {
    int i;

    for (i = 0; i < SDL_GAMEPAD_BUTTON_COUNT && i < 32; i++) {
        if (SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)i)) {
            host->buttons |= 1u << i;
        }
    }

    for (i = 0; i < SDL_GAMEPAD_AXIS_COUNT; i++) {
        host->axes[i] = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)i);
    }

    if (SDL_GetGamepadType(gamepad) == SDL_GAMEPAD_TYPE_GAMECUBE) {
        ArrangeGameCubePad(host);
    }
}

static void ApplyRumble(ControllerPort *port, bool isRenewal) {
    const Uint16 strength = port->wantsRumble ? RUMBLE_STRENGTH : 0;

    if (port->gamepad == NULL || !port->canRumble) {
        return;
    }

    if (port->wantsRumble != port->isRumbling || (port->wantsRumble && isRenewal)) {
        (void)SDL_RumbleGamepad(port->gamepad, strength, strength, RUMBLE_RENEW_MS * 2);
        port->isRumbling = port->wantsRumble;
    }
}

void InitControllers(void) {
    SDL_JoystickID *gamepads;
    int count = 0;
    int i;

    f_Lock = SDL_CreateMutex();
    if (f_Lock == NULL) {
        PcFatalError("Can't create the controllers' lock: %s", SDL_GetError());
    }

    SDL_zeroa(f_Ports);
    // The keyboard keeps the first port's controller connected
    f_Ports[0].isConnected = true;
    gamepads = SDL_GetGamepads(&count);
    for (i = 0; gamepads != NULL && i < count; i++) {
        ConnectGamepad(gamepads[i]);
    }

    SDL_free(gamepads);
}

void ShutdownControllers(void) {
    int i;

    for (i = 0; i < PORT_COUNT; i++) {
        if (f_Ports[i].gamepad != NULL) {
            SDL_CloseGamepad(f_Ports[i].gamepad);
        }
    }

    SDL_zeroa(f_Ports);
    SDL_DestroyMutex(f_Lock);
    f_Lock = NULL;
}

void ConnectGamepad(uint32_t instanceId) {
    SDL_Gamepad *gamepad;
    int i;

    for (i = 0; i < PORT_COUNT; i++) {
        if (f_Ports[i].gamepad != NULL && f_Ports[i].instanceId == instanceId) {
            return;
        }
    }

    for (i = 0; i < PORT_COUNT && f_Ports[i].gamepad != NULL; i++) {
    }

    if (i == PORT_COUNT) {
        return;
    }

    gamepad = SDL_OpenGamepad(instanceId);
    if (gamepad == NULL) {
        SDL_Log("Can't open gamepad %u: %s", instanceId, SDL_GetError());
        return;
    }

    SDL_LockMutex(f_Lock);
    f_Ports[i].gamepad = gamepad;
    f_Ports[i].instanceId = instanceId;
    f_Ports[i].isConnected = true;
    f_Ports[i].canRumble = SDL_GetBooleanProperty(
        SDL_GetGamepadProperties(gamepad),
        SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN,
        false
    );
    SDL_UnlockMutex(f_Lock);
    SDL_Log("Controller %d: %s", i + 1, SDL_GetGamepadName(gamepad));
}

void DisconnectGamepad(uint32_t instanceId) {
    int i;

    for (i = 0; i < PORT_COUNT; i++) {
        ControllerPort *port = &f_Ports[i];

        if (port->gamepad != NULL && port->instanceId == instanceId) {
            SDL_CloseGamepad(port->gamepad);
            SDL_LockMutex(f_Lock);
            port->gamepad = NULL;
            port->canRumble = false;
            port->isRumbling = false;
            port->isConnected = (i == 0);
            SDL_zero(port->pad);
            SDL_UnlockMutex(f_Lock);
        }
    }
}

void UpdateControllers(void) {
    const bool *keys = SDL_GetKeyboardState(NULL);
    const Uint64 now = SDL_GetTicks();
    const bool isRenewal = now - f_LastRumbleRenewal >= RUMBLE_RENEW_MS;
    int i;

    for (i = 0; i < PORT_COUNT; i++) {
        ControllerPort *port = &f_Ports[i];
        HostPadState host;
        OSContPad pad;

        SDL_zero(host);
        if (port->gamepad != NULL) {
            ReadGamepad(port->gamepad, &host);
        }

        if (i == 0 && keys != NULL) {
            AddKeyboardToHostPad(keys, &host);
        }

        MapHostPad(&host, &pad);
        SDL_LockMutex(f_Lock);
        port->pad = pad;
        ApplyRumble(port, isRenewal);
        SDL_UnlockMutex(f_Lock);
    }

    if (isRenewal) {
        f_LastRumbleRenewal = now;
    }
}

void PlayInputScript(const InputScript *script) {
    SDL_LockMutex(f_Lock);
    f_Script = script;
    f_FirstPortReadCount = 0;
    SDL_UnlockMutex(f_Lock);
}

// --- NuSystem's side ---

u8 PcGetConnectedControllers(void) {
    u8 bits = 0;
    int i;

    SDL_LockMutex(f_Lock);
    for (i = 0; i < PORT_COUNT; i++) {
        bits |= f_Ports[i].isConnected ? (u8)(1u << i) : 0;
    }

    SDL_UnlockMutex(f_Lock);
    return bits;
}

// The first port's read also counts the game's frame, plays the script and
// records
void RecordFirstPort(InputRecorder *recorder) {
    SDL_LockMutex(f_Lock);
    f_Recorder = recorder;
    f_HasRecordingFailed = false;
    SDL_UnlockMutex(f_Lock);
}

// Records a frame's pad, and says so the first time that it can't
static void RecordFirstPortFrame(long frame, const OSContPad *pad) {
    if (!RecordInputFrame(f_Recorder, frame, pad) && !f_HasRecordingFailed) {
        SDL_Log("Can't write the input recording: %s", SDL_GetError());
        f_HasRecordingFailed = true;
    }
}

void PcReadControllerPad(int port, OSContPad *pad) {
    SDL_assert(pad != NULL);
    SDL_zerop(pad);
    if (port >= 0 && port < PORT_COUNT) {
        SDL_LockMutex(f_Lock);
        *pad = f_Ports[port].pad;
        if (port == 0) {
            if (f_Script != NULL) {
                (void)FindScriptedPad(f_Script, f_FirstPortReadCount, pad);
            }

            if (f_Recorder != NULL) {
                RecordFirstPortFrame(f_FirstPortReadCount, pad);
            }

            f_FirstPortReadCount++;
        }

        SDL_UnlockMutex(f_Lock);
    }
}

int PcHasRumble(int port) {
    int canRumble = 0;

    if (port >= 0 && port < PORT_COUNT) {
        SDL_LockMutex(f_Lock);
        canRumble = f_Ports[port].canRumble;
        SDL_UnlockMutex(f_Lock);
    }

    return canRumble;
}

// The platform's thread starts the motor at its next update: SDL's gamepads
// belong to it
void PcSetRumble(int port, int isOn) {
    if (port >= 0 && port < PORT_COUNT) {
        SDL_LockMutex(f_Lock);
        f_Ports[port].wantsRumble = isOn != 0;
        SDL_UnlockMutex(f_Lock);
    }
}
