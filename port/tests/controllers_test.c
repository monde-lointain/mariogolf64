/*
 * Tests of the controllers' ports with SDL's virtual gamepads: which port
 * each gamepad takes as gamepads come and go, what the game reads from
 * each, and the motors that it starts. Real devices are kept out, so the
 * tests see the same on any machine.
 */

#include <SDL3/SDL.h>
#include <ultra64.h>

#include "input.h"
#include "port/nusys_host.h"
#include "test_support.h"
#include "touch_controls.h"

#define VIRTUAL_VENDOR 0x1209
#define VIRTUAL_PRODUCT 0x0001
#define MOTOR_STRENGTH 0xC000

typedef struct VirtualPad {
    SDL_JoystickID id;
    SDL_Joystick *joystick; // Opened to press its buttons
    int rumbleCount;
    Uint16 lastRumble;
} VirtualPad;

static bool RecordRumble(void *userdata, Uint16 low, Uint16 high) {
    VirtualPad *pad = userdata;

    pad->rumbleCount++;
    pad->lastRumble = (low == high) ? low : 1;
    return true;
}

// Plugs in a gamepad with every standard button and axis, and a motor
// when asked
static void AttachPad(VirtualPad *pad, bool hasMotor) {
    SDL_VirtualJoystickDesc desc;

    SDL_zerop(pad);
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.vendor_id = VIRTUAL_VENDOR;
    desc.product_id = VIRTUAL_PRODUCT;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    desc.name = "controllers_test pad";
    desc.userdata = pad;
    if (hasMotor) {
        desc.Rumble = RecordRumble;
    }

    pad->id = SDL_AttachVirtualJoystick(&desc);
    EXPECT(pad->id != 0);
    pad->joystick = SDL_OpenJoystick(pad->id);
    EXPECT(pad->joystick != NULL);
}

static void DetachPad(VirtualPad *pad) {
    SDL_CloseJoystick(pad->joystick);
    EXPECT(SDL_DetachVirtualJoystick(pad->id));
    pad->joystick = NULL;
}

static void PressSouth(VirtualPad *pad, bool isDown) {
    EXPECT(SDL_SetJoystickVirtualButton(pad->joystick, SDL_GAMEPAD_BUTTON_SOUTH, isDown));
    SDL_UpdateJoysticks();
}

static u16 ReadButtons(int port) {
    OSContPad pad;

    PcReadControllerPad(port, &pad);
    return pad.button;
}

// Gamepads present at the start take the ports in order, and those that
// come later take the lowest free port; a fifth waits. The first port
// stays connected without a gamepad, as the keyboard plays it.
static void TestPortsFillInOrder(void) {
    VirtualPad pads[6];
    int i;

    // Without gamepads, the keyboard's port alone
    InitControllers();
    EXPECT(PcGetConnectedControllers() == 0x1);
    ShutdownControllers();

    AttachPad(&pads[0], false);
    AttachPad(&pads[1], false);
    InitControllers();
    EXPECT(PcGetConnectedControllers() == 0x3);

    // SDL announces the gamepads found at the start, too, which takes no
    // more ports
    ConnectGamepad(pads[0].id);
    ConnectGamepad(pads[1].id);
    EXPECT(PcGetConnectedControllers() == 0x3);

    for (i = 2; i < 5; i++) {
        AttachPad(&pads[i], false);
        ConnectGamepad(pads[i].id);
    }

    EXPECT(PcGetConnectedControllers() == 0xF);

    // A gamepad connected twice takes one port
    ConnectGamepad(pads[2].id);
    EXPECT(PcGetConnectedControllers() == 0xF);

    // Each port reads its own gamepad
    PressSouth(&pads[3], true);
    UpdateControllers();
    EXPECT(ReadButtons(3) == A_BUTTON);
    EXPECT(ReadButtons(0) == 0 && ReadButtons(1) == 0 && ReadButtons(2) == 0);

    // A port that loses its gamepad is unplugged and reads nothing, and a
    // gamepad plugged in next takes it
    DisconnectGamepad(pads[3].id);
    DetachPad(&pads[3]);
    EXPECT(PcGetConnectedControllers() == 0x7);
    EXPECT(ReadButtons(3) == 0);
    UpdateControllers();
    EXPECT(ReadButtons(3) == 0);
    AttachPad(&pads[5], false);
    ConnectGamepad(pads[5].id);
    EXPECT(PcGetConnectedControllers() == 0xF);
    PressSouth(&pads[5], true);
    UpdateControllers();
    EXPECT(ReadButtons(3) == A_BUTTON);

    // The first port keeps the keyboard
    DisconnectGamepad(pads[0].id);
    EXPECT(PcGetConnectedControllers() == 0xF);
    DisconnectGamepad(pads[1].id);
    EXPECT(PcGetConnectedControllers() == 0xD);

    // A gamepad that the ports never took leaves them alone
    DisconnectGamepad(pads[4].id);
    EXPECT(PcGetConnectedControllers() == 0xD);

    ShutdownControllers();
    for (i = 0; i < 6; i++) {
        if (pads[i].joystick != NULL) {
            DetachPad(&pads[i]);
        }
    }
}

// A motor starts at the update after the game asks, and stops the same
// way; a gamepad without one has no rumble
static void TestMotors(void) {
    VirtualPad still;
    VirtualPad shaker;

    AttachPad(&still, false);
    AttachPad(&shaker, true);
    InitControllers();
    EXPECT(!PcHasRumble(0));
    EXPECT(PcHasRumble(1));
    EXPECT(!PcHasRumble(2) && !PcHasRumble(-1) && !PcHasRumble(MAXCONTROLLERS));

    PcSetRumble(1, 1);
    EXPECT(shaker.rumbleCount == 0);
    UpdateControllers();
    EXPECT(shaker.rumbleCount >= 1 && shaker.lastRumble == MOTOR_STRENGTH);

    PcSetRumble(1, 0);
    UpdateControllers();
    EXPECT(shaker.lastRumble == 0);

    // Asking a port without a motor does nothing
    PcSetRumble(0, 1);
    PcSetRumble(MAXCONTROLLERS, 1);
    UpdateControllers();
    EXPECT(shaker.lastRumble == 0);

    // A gamepad that leaves takes its motor with it
    DisconnectGamepad(shaker.id);
    EXPECT(!PcHasRumble(1));
    ShutdownControllers();
    DetachPad(&still);
    DetachPad(&shaker);
}

// The game reads the ports at its own pace, slower than the updates when
// it lags, so a press that an update saw lasts until the game's next read
// of its port, even when the next update finds it let go
static void TestBriefPressLastsToRead(void) {
    VirtualPad pads[2];

    AttachPad(&pads[0], false);
    AttachPad(&pads[1], false);
    InitControllers();
    PressSouth(&pads[1], true);
    UpdateControllers();
    PressSouth(&pads[1], false);
    UpdateControllers();
    UpdateControllers();
    EXPECT(ReadButtons(0) == 0);
    EXPECT(ReadButtons(1) == A_BUTTON);
    EXPECT(ReadButtons(1) == 0);

    // A press that the game saw goes as it's let go
    PressSouth(&pads[0], true);
    UpdateControllers();
    EXPECT(ReadButtons(0) == A_BUTTON);
    PressSouth(&pads[0], false);
    UpdateControllers();
    EXPECT(ReadButtons(0) == 0);

    // A gamepad that leaves takes its presses with it
    PressSouth(&pads[1], true);
    UpdateControllers();
    PressSouth(&pads[1], false);
    UpdateControllers();
    DisconnectGamepad(pads[1].id);
    EXPECT(ReadButtons(1) == 0);
    ShutdownControllers();
    DetachPad(&pads[0]);
    DetachPad(&pads[1]);
}

// --- The touch controls ---

// A phone-sized screen, and a layout of it to find the controls by
static TouchScreen MakeTouchScreen(TouchLayout *layout) {
    TouchScreen screen;

    SDL_zero(screen);
    screen.width = 2400.0f;
    screen.height = 1080.0f;
    screen.pixelsPerDp = 2.75f;
    ArrangeTouchControls(&screen, layout);
    return screen;
}

static void SendFinger(
    Uint32 type,
    SDL_FingerID finger,
    const TouchScreen *screen,
    const TouchCircle *place
) {
    SDL_Event event;

    SDL_zero(event);
    event.type = type;
    event.tfinger.touchID = 1;
    event.tfinger.fingerID = finger;
    event.tfinger.x = place->x / screen->width;
    event.tfinger.y = place->y / screen->height;
    HandleTouchEvent(&event);
}

static int CountShownShapes(void) {
    OverlayShape shapes[OVERLAY_MAX_SHAPES];

    return DescribeShownTouchControls(shapes);
}

// On a computer the controls wait for a touch to show; a finger on one
// plays the first port alone, at the next update
static void TestTouchPlaysFirstPort(void) {
    TouchLayout layout;
    const TouchScreen screen = MakeTouchScreen(&layout);
    VirtualPad pad;

    AttachPad(&pad, false);
    InitControllers();
    SetTouchScreen(&screen);
    EXPECT(CountShownShapes() == 0);
    SendFinger(SDL_EVENT_FINGER_DOWN, 1, &screen, &layout.controls[TOUCH_CONTROL_A]);
    EXPECT(CountShownShapes() == TOUCH_SHAPE_COUNT);
    EXPECT(ReadButtons(0) == 0);
    UpdateControllers();
    EXPECT(ReadButtons(0) == A_BUTTON && ReadButtons(1) == 0);

    // With the gamepad's own presses
    PressSouth(&pad, true);
    SendFinger(SDL_EVENT_FINGER_DOWN, 2, &screen, &layout.controls[TOUCH_CONTROL_Z]);
    UpdateControllers();
    EXPECT(ReadButtons(0) == (A_BUTTON | Z_TRIG));
    PressSouth(&pad, false);
    SendFinger(SDL_EVENT_FINGER_UP, 1, &screen, &layout.controls[TOUCH_CONTROL_A]);
    UpdateControllers();
    EXPECT(ReadButtons(0) == Z_TRIG);
    // A finger that the system cancels lifts, and one canceled before any
    // update saw it presses nothing
    SendFinger(SDL_EVENT_FINGER_CANCELED, 2, &screen, &layout.controls[TOUCH_CONTROL_Z]);
    UpdateControllers();
    EXPECT(ReadButtons(0) == 0);
    SendFinger(SDL_EVENT_FINGER_DOWN, 5, &screen, &layout.controls[TOUCH_CONTROL_B]);
    SendFinger(SDL_EVENT_FINGER_CANCELED, 5, &screen, &layout.controls[TOUCH_CONTROL_B]);
    UpdateControllers();
    EXPECT(ReadButtons(0) == 0);
    ShutdownControllers();
    DetachPad(&pad);
}

// The stick follows a finger's motion, in the screen's pixels
static void TestTouchStick(void) {
    TouchLayout layout;
    const TouchScreen screen = MakeTouchScreen(&layout);
    const TouchCircle start = {300.0f, 700.0f, 0.0f};
    TouchCircle pushed = start;
    OSContPad contPad;

    // Without a screen, as when there's no window, a touch does nothing,
    // even after the screen of an earlier start
    InitControllers();
    SendFinger(SDL_EVENT_FINGER_DOWN, 3, &screen, &start);
    UpdateControllers();
    PcReadControllerPad(0, &contPad);
    EXPECT(contPad.stick_x == 0 && CountShownShapes() == 0);
    SendFinger(SDL_EVENT_FINGER_UP, 3, &screen, &start);

    SetTouchScreen(&screen);
    SendFinger(SDL_EVENT_FINGER_DOWN, 3, &screen, &start);
    pushed.x += layout.controls[TOUCH_CONTROL_STICK].radius;
    SendFinger(SDL_EVENT_FINGER_MOTION, 3, &screen, &pushed);
    UpdateControllers();
    PcReadControllerPad(0, &contPad);
    EXPECT(contPad.stick_x == PAD_STICK_RANGE_MAX && contPad.stick_y == 0);
    ShutdownControllers();
}

// Another controller played on the first port hides the controls, as the
// player has picked it up; one on another port leaves them. Losing the
// window's focus lifts every finger.
static void TestTouchHidesForOtherController(void) {
    TouchLayout layout;
    const TouchScreen screen = MakeTouchScreen(&layout);
    VirtualPad pads[2];

    AttachPad(&pads[0], false);
    AttachPad(&pads[1], false);
    InitControllers();
    SetTouchScreen(&screen);
    SendFinger(SDL_EVENT_FINGER_DOWN, 1, &screen, &layout.controls[TOUCH_CONTROL_B]);
    PressSouth(&pads[1], true);
    UpdateControllers();
    EXPECT(CountShownShapes() == TOUCH_SHAPE_COUNT);
    EXPECT(ReadButtons(0) == B_BUTTON);
    PressSouth(&pads[0], true);
    UpdateControllers();
    EXPECT(CountShownShapes() == 0);
    // Still played while hidden
    EXPECT(ReadButtons(0) == (A_BUTTON | B_BUTTON));
    PressSouth(&pads[0], false);
    LiftTouchControls();
    UpdateControllers();
    EXPECT(ReadButtons(0) == 0);
    // A touch shows them again
    SendFinger(SDL_EVENT_FINGER_DOWN, 4, &screen, &layout.controls[TOUCH_CONTROL_R]);
    EXPECT(CountShownShapes() == TOUCH_SHAPE_COUNT);
    UpdateControllers();
    EXPECT(ReadButtons(0) == R_TRIG);
    ShutdownControllers();
    DetachPad(&pads[0]);
    DetachPad(&pads[1]);
}

int main(void) {
    char except[32];

    // Only the virtual gamepads, whose maker and product no device has
    (void)SDL_snprintf(except, sizeof(except), "0x%04x/0x%04x", 0xFFFF, 0xFFFF);
    (void)SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT, except);
    (void)SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        SDL_Log("Can't start SDL's gamepads: %s", SDL_GetError());
        return 77;
    }

    TestPortsFillInOrder();
    TestMotors();
    TestBriefPressLastsToRead();
    TestTouchPlaysFirstPort();
    TestTouchStick();
    TestTouchHidesForOtherController();
    SDL_Quit();
    return ReportTestResults("controllers_test");
}
