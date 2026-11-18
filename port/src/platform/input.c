/*
 * The controllers, which pad_mapping.c's bindings map to the N64's.
 */

#include "input.h"

#include <SDL3/SDL.h>

#include "port/nusys_host.h"
#include "port/os_host.h"

#define PORT_COUNT MAXCONTROLLERS
#define RUMBLE_STRENGTH 0xC000
// A phone or tablet shows its touch controls from the start, unless a
// gamepad plays the first port; a computer waits for a touch
#if defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_IOS)
#    define IS_TOUCH_FIRST true
#else
#    define IS_TOUCH_FIRST false
#endif
// The system's Back, as Android's key and gesture come, is the game's way
// back, B, whatever the bindings; it plays the first port after the touch
// controls, as it doesn't mean that the player took up another controller
#define BACK_KEY SDL_SCANCODE_AC_BACK
// SDL stops a rumble after this long unless it is renewed, so a crash
// doesn't leave a motor running
#define RUMBLE_RENEW_MS 250

typedef struct ControllerPort {
    SDL_Gamepad *gamepad;
    SDL_JoystickID instanceId;
    OSContPad pad;
    u16 unreadButtons; // Pressed at an update since the game last read
    bool isConnected;
    bool canRumble;
    bool wantsRumble;
    bool isRumbling;
} ControllerPort;

static SDL_Mutex *f_Lock; // Guards f_Ports between the platform and the game
static PadBindings f_Bindings; // The platform's thread's alone
static ControllerPort f_Ports[PORT_COUNT];
static Uint64 f_LastRumbleRenewal;
static const InputScript *f_Script;
static InputRecorder *f_Recorder;
static bool f_HasRecordingFailed;
static long f_FirstPortReadCount; // The game's frames, as it reads once each
// The keys pressed since the last update, which may be let go already,
// and the keyboard as the update plays it; the platform's thread's alone
static bool f_TappedKeys[SDL_SCANCODE_COUNT];
static bool f_Keys[SDL_SCANCODE_COUNT];
// The touch controls, the platform's thread's alone
static TouchScreen f_TouchScreen;
static TouchLayout f_TouchLayout;
static TouchPad f_TouchPad;
static bool f_IsTouchShown;

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

    SetDefaultPadBindings(&f_Bindings);
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
    SDL_zeroa(f_TappedKeys);
    LiftAllTouches(&f_TouchPad);
    f_IsTouchShown = IS_TOUCH_FIRST && f_Ports[0].gamepad == NULL;
}

void ShutdownControllers(void) {
    int i;

    for (i = 0; i < PORT_COUNT; i++) {
        if (f_Ports[i].gamepad != NULL) {
            SDL_CloseGamepad(f_Ports[i].gamepad);
        }
    }

    SDL_zeroa(f_Ports);
    LiftAllTouches(&f_TouchPad);
    SDL_zero(f_TouchScreen);
    f_IsTouchShown = false;
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
            port->unreadButtons = 0;
            SDL_UnlockMutex(f_Lock);
        }
    }
}

// The keyboard's state, with the keys pressed since the last update
static const bool *ReadKeys(void) {
    int count = 0;
    const bool *state = SDL_GetKeyboardState(&count);
    int i;

    for (i = 0; i < SDL_SCANCODE_COUNT; i++) {
        f_Keys[i] = (i < count && state[i]) || f_TappedKeys[i];
    }

    SDL_zeroa(f_TappedKeys);
    return f_Keys;
}

// Adds what the touch controls play to the first port's other devices,
// and hides them while another device plays
static void PlayTouchControls(OSContPad *pad) {
    OSContPad touch;

    if (pad->button != 0 || pad->stick_x != 0 || pad->stick_y != 0) {
        f_IsTouchShown = false;
    }

    ReadTouchPad(&f_TouchPad, &f_TouchLayout, &touch);
    MergeTouchPad(&touch, pad);
}

void UpdateControllers(void) {
    const bool *keys = ReadKeys();
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

        // The keyboard and the touch controls play the first port
        MapHostPad(&f_Bindings, &host, (i == 0) ? keys : NULL, &pad);
        if (i == 0) {
            PlayTouchControls(&pad);
            if (keys[BACK_KEY]) {
                pad.button |= B_BUTTON;
            }
        }

        SDL_LockMutex(f_Lock);
        port->pad = pad;
        port->unreadButtons |= pad.button;
        ApplyRumble(port, isRenewal);
        SDL_UnlockMutex(f_Lock);
    }

    if (isRenewal) {
        f_LastRumbleRenewal = now;
    }
}

void SetPadBindings(const PadBindings *bindings) {
    SDL_assert(bindings != NULL && IsStickShapeValid(&bindings->stick));
    f_Bindings = *bindings;
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
long CountGameFrames(void) {
    long count;

    SDL_LockMutex(f_Lock);
    count = f_FirstPortReadCount;
    SDL_UnlockMutex(f_Lock);
    return count;
}

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
        pad->button |= f_Ports[port].unreadButtons;
        f_Ports[port].unreadButtons = 0;
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

void HandleKeyEvent(const SDL_Event *event) {
    SDL_assert(event != NULL);
    if (event->type == SDL_EVENT_KEY_DOWN && (unsigned)event->key.scancode < SDL_SCANCODE_COUNT) {
        f_TappedKeys[event->key.scancode] = true;
    }
}

// --- The touch controls ---

void SetTouchScreen(const TouchScreen *screen) {
    SDL_assert(screen != NULL);
    f_TouchScreen = *screen;
    ArrangeTouchControls(&f_TouchScreen, &f_TouchLayout);
}

void HandleTouchEvent(const SDL_Event *event) {
    float x;
    float y;

    SDL_assert(event != NULL);
    // Without a screen, there are no controls to touch
    if (f_TouchScreen.width <= 0.0f) {
        return;
    }

    x = event->tfinger.x * f_TouchScreen.width;
    y = event->tfinger.y * f_TouchScreen.height;

    switch (event->type) {
        case SDL_EVENT_FINGER_DOWN:
            PressTouch(&f_TouchPad, &f_TouchLayout, event->tfinger.fingerID, x, y);
            f_IsTouchShown = true;
            break;
        case SDL_EVENT_FINGER_MOTION:
            MoveTouch(&f_TouchPad, event->tfinger.fingerID, x, y);
            break;
        case SDL_EVENT_FINGER_UP:
            LiftTouch(&f_TouchPad, event->tfinger.fingerID);
            break;
        case SDL_EVENT_FINGER_CANCELED:
            CancelTouch(&f_TouchPad, event->tfinger.fingerID);
            break;
        default:
            break;
    }
}

void LiftTouchControls(void) {
    LiftAllTouches(&f_TouchPad);
}

int DescribeShownTouchControls(OverlayShape *shapes) {
    return f_IsTouchShown ? DescribeTouchControls(&f_TouchPad, &f_TouchLayout, shapes) : 0;
}
