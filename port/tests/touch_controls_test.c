/*
 * Tests of the touch controls: where they lie on a screen, which one a
 * touch lands on, and what fingers on them play.
 */

#include <SDL3/SDL.h>
#include <ultra64.h>

#include "pad_mapping.h"
#include "test_support.h"
#include "touch_controls.h"

// A Pixel 7 Pro on its side: its camera's cutout leaves 144 pixels clear
// on the left
#define PHONE_WIDTH 3120.0f
#define PHONE_HEIGHT 1440.0f
#define PHONE_DENSITY 3.5f
#define PHONE_CUTOUT 144.0f
// The smallest that a finger can hit surely: 48 dp across
#define MIN_TARGET_RADIUS_DP 24.0f
// How far past its drawn edge a button takes a touch, as a share of its
// radius
#define HIT_MARGIN 1.4f

static const TouchControl f_Buttons[] = {
    TOUCH_CONTROL_A,
    TOUCH_CONTROL_B,
    TOUCH_CONTROL_Z,
    TOUCH_CONTROL_R,
    TOUCH_CONTROL_START,
    TOUCH_CONTROL_C_UP,
    TOUCH_CONTROL_C_DOWN,
    TOUCH_CONTROL_C_LEFT,
    TOUCH_CONTROL_C_RIGHT,
};

static const OSContPad f_ZeroPad;

static TouchScreen MakeScreen(float width, float height, float density) {
    TouchScreen screen;

    SDL_zero(screen);
    screen.width = width;
    screen.height = height;
    screen.pixelsPerDp = density;
    return screen;
}

static TouchScreen MakePhone(void) {
    TouchScreen screen = MakeScreen(PHONE_WIDTH, PHONE_HEIGHT, PHONE_DENSITY);

    screen.safeLeft = PHONE_CUTOUT;
    return screen;
}

static TouchLayout ArrangeOn(const TouchScreen *screen) {
    TouchLayout layout;

    ArrangeTouchControls(screen, &layout);
    return layout;
}

static bool IsInsideSafeArea(const TouchScreen *screen, const TouchCircle *circle) {
    return circle->x - circle->radius >= screen->safeLeft &&
        circle->x + circle->radius <= screen->width - screen->safeRight &&
        circle->y - circle->radius >= screen->safeTop &&
        circle->y + circle->radius <= screen->height - screen->safeBottom;
}

static bool DoOverlap(const TouchCircle *a, const TouchCircle *b) {
    const float dx = a->x - b->x;
    const float dy = a->y - b->y;
    const float reach = a->radius + b->radius;

    return (dx * dx) + (dy * dy) < reach * reach;
}

// Every control on the safe area, no two buttons on each other, and each
// big enough for a finger
static void ExpectSoundLayout(const TouchScreen *screen, float minRadiusDp) {
    const TouchLayout layout = ArrangeOn(screen);
    size_t i;
    size_t j;

    EXPECT(IsInsideSafeArea(screen, &layout.controls[TOUCH_CONTROL_STICK]));
    for (i = 0; i < SDL_arraysize(f_Buttons); i++) {
        const TouchCircle *button = &layout.controls[f_Buttons[i]];

        EXPECT(IsInsideSafeArea(screen, button));
        EXPECT(button->radius * HIT_MARGIN >= minRadiusDp * screen->pixelsPerDp);
        for (j = i + 1; j < SDL_arraysize(f_Buttons); j++) {
            EXPECT(!DoOverlap(button, &layout.controls[f_Buttons[j]]));
        }
    }
}

// --- The layout ---

static void TestLayoutFitsPhone(void) {
    const TouchScreen screen = MakePhone();
    const TouchLayout layout = ArrangeOn(&screen);
    size_t i;

    ExpectSoundLayout(&screen, MIN_TARGET_RADIUS_DP);
    // The left thumb's controls on the left, the right thumb's on the right
    EXPECT(layout.controls[TOUCH_CONTROL_STICK].x < PHONE_WIDTH / 2);
    EXPECT(layout.controls[TOUCH_CONTROL_START].x < PHONE_WIDTH / 2);
    for (i = 0; i < SDL_arraysize(f_Buttons); i++) {
        if (f_Buttons[i] != TOUCH_CONTROL_START) {
            EXPECT(layout.controls[f_Buttons[i]].x > PHONE_WIDTH / 2);
        }
    }

    // The shot button is the largest
    for (i = 0; i < SDL_arraysize(f_Buttons); i++) {
        EXPECT(layout.controls[TOUCH_CONTROL_A].radius >= layout.controls[f_Buttons[i]].radius);
    }

    EXPECT(layout.stickZoneRight == PHONE_WIDTH / 2);
}

static void TestLayoutFollowsN64Pad(void) {
    const TouchScreen screen = MakePhone();
    const TouchLayout layout = ArrangeOn(&screen);
    const TouchCircle *controls = layout.controls;

    // B up and left of A, Z left of A, the C buttons above, R higher yet
    EXPECT(controls[TOUCH_CONTROL_B].x < controls[TOUCH_CONTROL_A].x);
    EXPECT(controls[TOUCH_CONTROL_B].y < controls[TOUCH_CONTROL_A].y);
    EXPECT(controls[TOUCH_CONTROL_Z].x < controls[TOUCH_CONTROL_A].x);
    EXPECT(controls[TOUCH_CONTROL_C_DOWN].y < controls[TOUCH_CONTROL_A].y);
    EXPECT(controls[TOUCH_CONTROL_C_UP].y < controls[TOUCH_CONTROL_C_DOWN].y);
    EXPECT(controls[TOUCH_CONTROL_C_LEFT].x < controls[TOUCH_CONTROL_C_RIGHT].x);
    EXPECT(controls[TOUCH_CONTROL_C_LEFT].y == controls[TOUCH_CONTROL_C_RIGHT].y);
    EXPECT(controls[TOUCH_CONTROL_C_UP].x == controls[TOUCH_CONTROL_C_DOWN].x);
    EXPECT(controls[TOUCH_CONTROL_R].y < controls[TOUCH_CONTROL_C_UP].y);
    // START well away from the stick, above it
    EXPECT(controls[TOUCH_CONTROL_START].y < controls[TOUCH_CONTROL_STICK].y);
}

// A tall screen keeps the controls' size, so a finger finds them the same
// on a tablet
static void TestLayoutKeepsSizeOnTallScreen(void) {
    const TouchScreen phone = MakeScreen(PHONE_WIDTH, PHONE_HEIGHT, PHONE_DENSITY);
    const TouchScreen tablet = MakeScreen(2560.0f, 1600.0f, 2.0f);
    const TouchLayout onPhone = ArrangeOn(&phone);
    const TouchLayout onTablet = ArrangeOn(&tablet);

    EXPECT_NEAR(
        onPhone.controls[TOUCH_CONTROL_A].radius / PHONE_DENSITY,
        onTablet.controls[TOUCH_CONTROL_A].radius / 2.0f,
        0.001
    );
    // Held to the bottom corners, where the thumbs are
    EXPECT_NEAR(
        2560.0f - onTablet.controls[TOUCH_CONTROL_A].x,
        (PHONE_WIDTH - onPhone.controls[TOUCH_CONTROL_A].x) * 2.0f / PHONE_DENSITY,
        0.01
    );
    EXPECT_NEAR(
        1600.0f - onTablet.controls[TOUCH_CONTROL_A].y,
        (PHONE_HEIGHT - onPhone.controls[TOUCH_CONTROL_A].y) * 2.0f / PHONE_DENSITY,
        0.01
    );
    ExpectSoundLayout(&tablet, MIN_TARGET_RADIUS_DP);
}

// A short screen shrinks the controls to fit, a little: at 360 dp, the
// shortest phone that Android's guides plan for, every target is still
// big enough; at 280 dp they all still fit and stay apart
static void TestLayoutShrinksOnShortScreen(void) {
    const TouchScreen phone = MakeScreen(PHONE_WIDTH, PHONE_HEIGHT, PHONE_DENSITY);
    const TouchScreen shortPhone = MakeScreen(1600.0f, 720.0f, 2.0f);
    const TouchScreen shorter = MakeScreen(1280.0f, 560.0f, 2.0f);
    const TouchLayout onPhone = ArrangeOn(&phone);
    const TouchLayout onShort = ArrangeOn(&shortPhone);

    ExpectSoundLayout(&shortPhone, MIN_TARGET_RADIUS_DP);
    EXPECT(
        onShort.controls[TOUCH_CONTROL_A].radius / 2.0f <
        onPhone.controls[TOUCH_CONTROL_A].radius / PHONE_DENSITY
    );
    ExpectSoundLayout(&shorter, 0.0f);
}

static void TestLayoutKeepsToSafeArea(void) {
    TouchScreen screen = MakePhone();
    const TouchLayout open = ArrangeOn(&screen);
    const TouchScreen uncut = MakeScreen(PHONE_WIDTH, PHONE_HEIGHT, PHONE_DENSITY);
    const TouchLayout onUncut = ArrangeOn(&uncut);
    TouchScreen shorter = MakeScreen(1280.0f, 560.0f, 2.0f);
    TouchLayout layout;

    // The cutout moves the left thumb's controls over
    EXPECT_NEAR(
        open.controls[TOUCH_CONTROL_STICK].x,
        onUncut.controls[TOUCH_CONTROL_STICK].x + PHONE_CUTOUT,
        0.01
    );
    EXPECT_NEAR(
        open.controls[TOUCH_CONTROL_START].x,
        onUncut.controls[TOUCH_CONTROL_START].x + PHONE_CUTOUT,
        0.01
    );
    // An inset at the top of a short screen shrinks the controls further
    shorter.safeTop = 100.0f;
    ExpectSoundLayout(&shorter, 0.0f);

    screen.safeRight = 100.0f;
    screen.safeBottom = 60.0f;
    screen.safeTop = 30.0f;
    layout = ArrangeOn(&screen);
    ExpectSoundLayout(&screen, MIN_TARGET_RADIUS_DP);
    EXPECT_NEAR(
        layout.controls[TOUCH_CONTROL_A].x,
        open.controls[TOUCH_CONTROL_A].x - 100.0f,
        0.01
    );
    EXPECT_NEAR(layout.controls[TOUCH_CONTROL_A].y, open.controls[TOUCH_CONTROL_A].y - 60.0f, 0.01);
    EXPECT_NEAR(layout.controls[TOUCH_CONTROL_STICK].x, open.controls[TOUCH_CONTROL_STICK].x, 0.01);
}

// --- Finding the touched control ---

static void TestFindsEachButtonAtItsCenter(void) {
    const TouchScreen screen = MakePhone();
    const TouchLayout layout = ArrangeOn(&screen);
    size_t i;

    for (i = 0; i < SDL_arraysize(f_Buttons); i++) {
        const TouchCircle *button = &layout.controls[f_Buttons[i]];

        EXPECT(FindTouchedControl(&layout, button->x, button->y) == f_Buttons[i]);
    }
}

// A button takes touches out to its margin and no further
static void TestButtonMargin(void) {
    const TouchScreen screen = MakePhone();
    const TouchLayout layout = ArrangeOn(&screen);
    const TouchCircle r = layout.controls[TOUCH_CONTROL_R];
    const TouchCircle start = layout.controls[TOUCH_CONTROL_START];
    const float rReach = r.radius * HIT_MARGIN;
    const float startReach = start.radius * HIT_MARGIN;

    // Up from R, where nothing else is: the right half takes no stick
    EXPECT(FindTouchedControl(&layout, r.x, r.y - (rReach - 1.0f)) == TOUCH_CONTROL_R);
    EXPECT(FindTouchedControl(&layout, r.x, r.y - (rReach + 1.0f)) == TOUCH_CONTROL_NONE);
    // Up from START, past its margin, the stick's zone
    EXPECT(
        FindTouchedControl(&layout, start.x, start.y - (startReach - 1.0f)) == TOUCH_CONTROL_START
    );
    EXPECT(
        FindTouchedControl(&layout, start.x, start.y - (startReach + 1.0f)) == TOUCH_CONTROL_STICK
    );
}

// Between two buttons whose margins meet, the touch goes to the one that
// it is nearer for their sizes
static void TestNearerButtonWins(void) {
    const TouchScreen screen = MakePhone();
    const TouchLayout layout = ArrangeOn(&screen);
    const TouchCircle b = layout.controls[TOUCH_CONTROL_B];
    const TouchCircle z = layout.controls[TOUCH_CONTROL_Z];
    // The point where the two are as near as each other for their sizes
    const float share = b.radius / (b.radius + z.radius);
    const float x = b.x + ((z.x - b.x) * share);
    const float y = b.y + ((z.y - b.y) * share);
    const float step = 2.0f / SDL_sqrtf(((z.x - b.x) * (z.x - b.x)) + ((z.y - b.y) * (z.y - b.y)));

    EXPECT(
        FindTouchedControl(&layout, x - ((z.x - b.x) * step), y - ((z.y - b.y) * step)) ==
        TOUCH_CONTROL_B
    );
    EXPECT(
        FindTouchedControl(&layout, x + ((z.x - b.x) * step), y + ((z.y - b.y) * step)) ==
        TOUCH_CONTROL_Z
    );
}

static void TestStickZone(void) {
    const TouchScreen screen = MakePhone();
    const TouchLayout layout = ArrangeOn(&screen);

    EXPECT(FindTouchedControl(&layout, 0.0f, 0.0f) == TOUCH_CONTROL_STICK);
    EXPECT(FindTouchedControl(&layout, PHONE_WIDTH / 2 - 1.0f, 700.0f) == TOUCH_CONTROL_STICK);
    EXPECT(FindTouchedControl(&layout, PHONE_WIDTH / 2, 700.0f) == TOUCH_CONTROL_NONE);
    EXPECT(FindTouchedControl(&layout, PHONE_WIDTH - 1.0f, 0.0f) == TOUCH_CONTROL_NONE);
}

// --- Fingers ---

typedef struct Rig {
    TouchLayout layout;
    TouchPad pad;
} Rig;

static void SetUpRig(Rig *rig) {
    const TouchScreen screen = MakePhone();

    ArrangeTouchControls(&screen, &rig->layout);
    LiftAllTouches(&rig->pad);
}

static OSContPad ReadRig(Rig *rig) {
    OSContPad contPad;

    SDL_memset(&contPad, 0xA5, sizeof(contPad));
    ReadTouchPad(&rig->pad, &rig->layout, &contPad);
    return contPad;
}

static void PressOn(Rig *rig, uint64_t finger, TouchControl control) {
    const TouchCircle *circle = &rig->layout.controls[control];

    PressTouch(&rig->pad, &rig->layout, finger, circle->x, circle->y);
}

static void TestNothingHeld(void) {
    Rig rig;
    OSContPad contPad;

    SetUpRig(&rig);
    contPad = ReadRig(&rig);
    // All of it zero, as the N64's controller reads with no error
    EXPECT(SDL_memcmp(&contPad, &f_ZeroPad, sizeof(contPad)) == 0);
    EXPECT(!IsTouchPadHeld(&rig.pad));
}

static void TestEachButtonPresses(void) {
    static const struct {
        TouchControl control;
        u16 button;
    } presses[] = {
        {TOUCH_CONTROL_A, A_BUTTON},
        {TOUCH_CONTROL_B, B_BUTTON},
        {TOUCH_CONTROL_Z, Z_TRIG},
        {TOUCH_CONTROL_R, R_TRIG},
        {TOUCH_CONTROL_START, START_BUTTON},
        {TOUCH_CONTROL_C_UP, U_CBUTTONS},
        {TOUCH_CONTROL_C_DOWN, D_CBUTTONS},
        {TOUCH_CONTROL_C_LEFT, L_CBUTTONS},
        {TOUCH_CONTROL_C_RIGHT, R_CBUTTONS},
    };

    size_t i;

    for (i = 0; i < SDL_arraysize(presses); i++) {
        Rig rig;
        OSContPad contPad;

        SetUpRig(&rig);
        PressOn(&rig, 7, presses[i].control);
        contPad = ReadRig(&rig);
        EXPECT(contPad.button == presses[i].button);
        EXPECT(contPad.stick_x == 0 && contPad.stick_y == 0);
        EXPECT(IsTouchPadHeld(&rig.pad));
        LiftTouch(&rig.pad, 7);
        EXPECT(ReadRig(&rig).button == 0);
        EXPECT(!IsTouchPadHeld(&rig.pad));
    }
}

// Z held by the right thumb while the left pushes the stick, to set the
// spin
static void TestFingersTogether(void) {
    Rig rig;
    const float stickX = 500.0f;
    const float stickY = 900.0f;
    OSContPad contPad;

    SetUpRig(&rig);
    PressOn(&rig, 1, TOUCH_CONTROL_Z);
    PressTouch(&rig.pad, &rig.layout, 2, stickX, stickY);
    MoveTouch(&rig.pad, 2, stickX + rig.layout.controls[TOUCH_CONTROL_STICK].radius, stickY);
    PressOn(&rig, 3, TOUCH_CONTROL_A);
    contPad = ReadRig(&rig);
    EXPECT(contPad.button == (Z_TRIG | A_BUTTON));
    EXPECT(contPad.stick_x == PAD_STICK_RANGE_MAX && contPad.stick_y == 0);
    LiftTouch(&rig.pad, 1);
    contPad = ReadRig(&rig);
    EXPECT(contPad.button == A_BUTTON && contPad.stick_x == PAD_STICK_RANGE_MAX);
}

// A finger keeps its button wherever it slides, and a finger that came
// down on nothing presses nothing wherever it goes
static void TestFingerKeepsItsControl(void) {
    Rig rig;
    const TouchCircle *a;
    const TouchCircle *b;

    SetUpRig(&rig);
    a = &rig.layout.controls[TOUCH_CONTROL_A];
    b = &rig.layout.controls[TOUCH_CONTROL_B];
    PressOn(&rig, 1, TOUCH_CONTROL_A);
    MoveTouch(&rig.pad, 1, b->x, b->y);
    EXPECT(ReadRig(&rig).button == A_BUTTON);
    MoveTouch(&rig.pad, 1, 10.0f, 10.0f);
    EXPECT(ReadRig(&rig).button == A_BUTTON && ReadRig(&rig).stick_x == 0);
    LiftTouch(&rig.pad, 1);
    PressTouch(&rig.pad, &rig.layout, 2, PHONE_WIDTH - 1.0f, 1.0f);
    EXPECT(!IsTouchPadHeld(&rig.pad));
    MoveTouch(&rig.pad, 2, a->x, a->y);
    EXPECT(ReadRig(&rig).button == 0);
}

// The stick centers where the thumb lands: its push is the thumb's travel
// since, up the screen being up, with a dead zone around the middle and
// the full throw at the stick's radius and past it
static void TestStickThrow(void) {
    Rig rig;
    float throwRadius;
    const float x = 400.0f;
    const float y = 1000.0f;
    OSContPad contPad;

    SetUpRig(&rig);
    throwRadius = rig.layout.controls[TOUCH_CONTROL_STICK].radius;
    PressTouch(&rig.pad, &rig.layout, 4, x, y);
    contPad = ReadRig(&rig);
    EXPECT(contPad.button == 0 && contPad.stick_x == 0 && contPad.stick_y == 0);
    // Within the dead zone, an eighth of the throw
    MoveTouch(&rig.pad, 4, x + (throwRadius / 8.0f) - 1.0f, y);
    EXPECT(ReadRig(&rig).stick_x == 0);
    MoveTouch(&rig.pad, 4, x + (throwRadius / 8.0f) + 4.0f, y);
    EXPECT(ReadRig(&rig).stick_x == 2);
    // Halfway past the dead zone, half the range
    MoveTouch(&rig.pad, 4, x, y - (throwRadius * 9.0f / 16.0f));
    contPad = ReadRig(&rig);
    EXPECT(contPad.stick_x == 0 && contPad.stick_y == PAD_STICK_RANGE_MAX / 2);
    MoveTouch(&rig.pad, 4, x - (throwRadius * 3.0f), y);
    contPad = ReadRig(&rig);
    EXPECT(contPad.stick_x == -PAD_STICK_RANGE_MAX && contPad.stick_y == 0);
    MoveTouch(&rig.pad, 4, x, y + throwRadius);
    contPad = ReadRig(&rig);
    EXPECT(contPad.stick_x == 0 && contPad.stick_y == -PAD_STICK_RANGE_MAX);
    // Diagonally, the throw is round, as the stick's is
    MoveTouch(&rig.pad, 4, x + throwRadius, y - throwRadius);
    contPad = ReadRig(&rig);
    EXPECT(contPad.stick_x == 57 && contPad.stick_y == 57);
    LiftTouch(&rig.pad, 4);
    contPad = ReadRig(&rig);
    EXPECT(contPad.stick_x == 0 && contPad.stick_y == 0);
}

static void TestFingerLimit(void) {
    Rig rig;
    uint64_t finger;

    SetUpRig(&rig);
    for (finger = 0; finger < TOUCH_MAX_FINGERS; finger++) {
        PressOn(&rig, 100 + finger, TOUCH_CONTROL_C_UP);
    }

    // One finger too many is let be
    PressOn(&rig, 99, TOUCH_CONTROL_A);
    EXPECT(ReadRig(&rig).button == U_CBUTTONS);
    for (finger = 0; finger + 1 < TOUCH_MAX_FINGERS; finger++) {
        LiftTouch(&rig.pad, 100 + finger);
    }

    EXPECT(ReadRig(&rig).button == U_CBUTTONS);
    LiftTouch(&rig.pad, 99);
    EXPECT(ReadRig(&rig).button == U_CBUTTONS);
    // A place freed takes a new finger
    PressOn(&rig, 99, TOUCH_CONTROL_A);
    EXPECT(ReadRig(&rig).button == (U_CBUTTONS | A_BUTTON));
    LiftAllTouches(&rig.pad);
    EXPECT(ReadRig(&rig).button == 0 && !IsTouchPadHeld(&rig.pad));
}

// The system may repeat a finger's coming down; the finger is still one
static void TestFingerComesDownTwice(void) {
    Rig rig;

    SetUpRig(&rig);
    PressOn(&rig, 5, TOUCH_CONTROL_A);
    PressOn(&rig, 5, TOUCH_CONTROL_B);
    EXPECT(ReadRig(&rig).button == B_BUTTON);
    LiftTouch(&rig.pad, 5);
    EXPECT(ReadRig(&rig).button == 0);
    // Moving or lifting an unknown finger changes nothing
    PressOn(&rig, 6, TOUCH_CONTROL_A);
    MoveTouch(&rig.pad, 8, 0.0f, 0.0f);
    LiftTouch(&rig.pad, 8);
    EXPECT(ReadRig(&rig).button == A_BUTTON);
}

// A tap too quick for any read still presses its button, at the next
// read alone; a finger that a read saw down adds nothing as it lifts
static void TestQuickTap(void) {
    Rig rig;
    OSContPad contPad;

    SetUpRig(&rig);
    PressOn(&rig, 1, TOUCH_CONTROL_A);
    LiftTouch(&rig.pad, 1);
    // The system's repeating the lift takes nothing more
    LiftTouch(&rig.pad, 1);
    EXPECT(!IsTouchPadHeld(&rig.pad));
    EXPECT(ReadRig(&rig).button == A_BUTTON);
    EXPECT(ReadRig(&rig).button == 0);

    PressOn(&rig, 1, TOUCH_CONTROL_A);
    EXPECT(ReadRig(&rig).button == A_BUTTON);
    LiftTouch(&rig.pad, 1);
    EXPECT(ReadRig(&rig).button == 0);

    // With another button held, and two taps
    PressOn(&rig, 2, TOUCH_CONTROL_B);
    PressOn(&rig, 3, TOUCH_CONTROL_C_UP);
    LiftTouch(&rig.pad, 3);
    PressOn(&rig, 3, TOUCH_CONTROL_START);
    LiftTouch(&rig.pad, 3);
    EXPECT(ReadRig(&rig).button == (B_BUTTON | U_CBUTTONS | START_BUTTON));
    EXPECT(ReadRig(&rig).button == B_BUTTON);
    LiftTouch(&rig.pad, 2);

    // A quick tap on the stick pushes nothing
    PressTouch(&rig.pad, &rig.layout, 4, 400.0f, 1000.0f);
    MoveTouch(&rig.pad, 4, 400.0f + rig.layout.controls[TOUCH_CONTROL_STICK].radius, 1000.0f);
    LiftTouch(&rig.pad, 4);
    contPad = ReadRig(&rig);
    EXPECT(contPad.button == 0 && contPad.stick_x == 0 && contPad.stick_y == 0);
}

// A finger that the system takes back, for a gesture of its own, or that
// all fingers' lifting takes, taps nothing
static void TestCanceledTouch(void) {
    Rig rig;

    SetUpRig(&rig);
    PressOn(&rig, 1, TOUCH_CONTROL_A);
    PressOn(&rig, 2, TOUCH_CONTROL_Z);
    CancelTouch(&rig.pad, 1);
    EXPECT(ReadRig(&rig).button == Z_TRIG);
    CancelTouch(&rig.pad, 2);
    EXPECT(ReadRig(&rig).button == 0 && !IsTouchPadHeld(&rig.pad));
    // Canceling an unknown finger changes nothing
    PressOn(&rig, 3, TOUCH_CONTROL_B);
    CancelTouch(&rig.pad, 9);
    EXPECT(ReadRig(&rig).button == B_BUTTON);

    PressOn(&rig, 4, TOUCH_CONTROL_R);
    LiftTouch(&rig.pad, 4);
    LiftAllTouches(&rig.pad);
    EXPECT(ReadRig(&rig).button == 0);
}

// --- Merging with the other controllers ---

static OSContPad MakeContPad(u16 button, s8 x, s8 y) {
    OSContPad contPad;

    SDL_zero(contPad);
    contPad.button = button;
    contPad.stick_x = x;
    contPad.stick_y = y;
    return contPad;
}

static void TestMerge(void) {
    OSContPad touch = MakeContPad(A_BUTTON, 10, -10);
    OSContPad other = MakeContPad(Z_TRIG, 0, 20);

    MergeTouchPad(&touch, &other);
    EXPECT(other.button == (A_BUTTON | Z_TRIG));
    EXPECT(other.stick_x == 0 && other.stick_y == 20);
    // The further stick wins, by length, not by either axis alone
    touch = MakeContPad(0, 15, -15);
    other = MakeContPad(0, 0, 20);
    MergeTouchPad(&touch, &other);
    EXPECT(other.stick_x == 15 && other.stick_y == -15);
    // As far as each other: the other controller's stays
    touch = MakeContPad(0, -20, 0);
    other = MakeContPad(0, 0, 20);
    MergeTouchPad(&touch, &other);
    EXPECT(other.stick_x == 0 && other.stick_y == 20);
    touch = MakeContPad(0, -80, -80);
    other = MakeContPad(0, 79, 79);
    MergeTouchPad(&touch, &other);
    EXPECT(other.stick_x == -80 && other.stick_y == -80);
}

// --- The shapes ---

static const OverlayShape *FindShape(const OverlayShape *shapes, int count, OverlayLabel label) {
    int i;

    for (i = 0; i < count; i++) {
        if (shapes[i].label == label) {
            return &shapes[i];
        }
    }

    return NULL;
}

static void TestShapesShowButtons(void) {
    Rig rig;
    OverlayShape shapes[TOUCH_SHAPE_COUNT];
    const OverlayShape *a;
    const OverlayShape *b;
    OverlayShape restA;
    OverlayShape restB;
    int count;

    SetUpRig(&rig);
    count = DescribeTouchControls(&rig.pad, &rig.layout, shapes);
    EXPECT(count == TOUCH_SHAPE_COUNT);
    a = FindShape(shapes, count, OVERLAY_LABEL_A);
    b = FindShape(shapes, count, OVERLAY_LABEL_B);
    EXPECT(a != NULL && b != NULL);
    EXPECT(FindShape(shapes, count, OVERLAY_LABEL_START) != NULL);
    EXPECT(FindShape(shapes, count, OVERLAY_LABEL_ARROW_LEFT) != NULL);
    if (a == NULL || b == NULL) {
        return;
    }

    EXPECT(a->x == rig.layout.controls[TOUCH_CONTROL_A].x);
    EXPECT(a->y == rig.layout.controls[TOUCH_CONTROL_A].y);
    EXPECT(a->radius == rig.layout.controls[TOUCH_CONTROL_A].radius);
    EXPECT(a->style == OVERLAY_STYLE_DISC);
    // A held button shows brighter: its alpha, the color's top byte, rises
    restA = *a;
    restB = *b;
    PressOn(&rig, 1, TOUCH_CONTROL_A);
    (void)DescribeTouchControls(&rig.pad, &rig.layout, shapes);
    EXPECT((a->color >> 24) > (restA.color >> 24));
    EXPECT((a->color & 0xFFFFFF) == (restA.color & 0xFFFFFF));
    EXPECT(b->color == restB.color);
}

// At rest, the stick shows its ring with the knob in the middle; held,
// both move to where the thumb landed, and the knob follows the thumb out
// to the ring
static void TestShapesShowStick(void) {
    Rig rig;
    OverlayShape shapes[TOUCH_SHAPE_COUNT];
    const OverlayShape *ring = NULL;
    const OverlayShape *knob = NULL;
    TouchCircle stick;
    int count;
    int i;

    SetUpRig(&rig);
    stick = rig.layout.controls[TOUCH_CONTROL_STICK];
    count = DescribeTouchControls(&rig.pad, &rig.layout, shapes);
    for (i = 0; i < count; i++) {
        if (shapes[i].style == OVERLAY_STYLE_RING) {
            ring = &shapes[i];
        }
        else if (shapes[i].label == OVERLAY_LABEL_NONE) {
            knob = &shapes[i];
        }
    }

    EXPECT(ring != NULL && knob != NULL);
    if (ring == NULL || knob == NULL) {
        return;
    }

    EXPECT(ring->x == stick.x && ring->y == stick.y && ring->radius == stick.radius);
    EXPECT(knob->x == stick.x && knob->y == stick.y && knob->radius < stick.radius);
    PressTouch(&rig.pad, &rig.layout, 3, 300.0f, 800.0f);
    MoveTouch(&rig.pad, 3, 300.0f + (stick.radius * 2.0f), 800.0f);
    (void)DescribeTouchControls(&rig.pad, &rig.layout, shapes);
    EXPECT(ring->x == 300.0f && ring->y == 800.0f);
    EXPECT_NEAR(knob->x, 300.0f + stick.radius, 0.01);
    EXPECT(knob->y == 800.0f);
}

int main(void) {
    TestLayoutFitsPhone();
    TestLayoutFollowsN64Pad();
    TestLayoutKeepsSizeOnTallScreen();
    TestLayoutShrinksOnShortScreen();
    TestLayoutKeepsToSafeArea();
    TestFindsEachButtonAtItsCenter();
    TestButtonMargin();
    TestNearerButtonWins();
    TestStickZone();
    TestNothingHeld();
    TestEachButtonPresses();
    TestFingersTogether();
    TestFingerKeepsItsControl();
    TestStickThrow();
    TestFingerLimit();
    TestFingerComesDownTwice();
    TestQuickTap();
    TestCanceledTouch();
    TestMerge();
    TestShapesShowButtons();
    TestShapesShowStick();
    return ReportTestResults("touch_controls_test");
}
