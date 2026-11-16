/*
 * Tests of the platform layer's parts that need no window or device: the
 * ROM's byte orders and check, the controllers' mapping, the save memory's
 * file, and the presenter's fit and pixels.
 */

#include <SDL3/SDL.h>
#include <ultra64.h>

#include "input.h"
#include "port/nusys_host.h"
#include "presenter.h"
#include "rom_image.h"
#include "save_memory.h"
#include "test_support.h"

#define AXIS_MAX 32767
#define SUPPORTED_ROM_SIZE 0x2000000u

// --- The ROM ---

// The first two words of an image in the N64's order
static const uint8_t f_BigEndianStart[] = {0x80, 0x37, 0x12, 0x40, 0x01, 0x02, 0x03, 0x04};

static void TestRomByteOrders(void) {
    static const uint8_t halfwordSwapped[] = {0x37, 0x80, 0x40, 0x12, 0x02, 0x01, 0x04, 0x03};
    static const uint8_t little[] = {0x40, 0x12, 0x37, 0x80, 0x04, 0x03, 0x02, 0x01};
    static const uint8_t unknown[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t image[8];

    EXPECT(DetectRomByteOrder(f_BigEndianStart, sizeof(f_BigEndianStart)) == ROM_ORDER_BIG);
    EXPECT(
        DetectRomByteOrder(halfwordSwapped, sizeof(halfwordSwapped)) == ROM_ORDER_HALFWORD_SWAPPED
    );
    EXPECT(DetectRomByteOrder(little, sizeof(little)) == ROM_ORDER_LITTLE);
    EXPECT(DetectRomByteOrder(unknown, sizeof(unknown)) == ROM_ORDER_UNKNOWN);
    EXPECT(DetectRomByteOrder(f_BigEndianStart, 3) == ROM_ORDER_UNKNOWN);

    SDL_memcpy(image, halfwordSwapped, sizeof(image));
    EXPECT(ConvertRomToBigEndian(image, sizeof(image)));
    EXPECT(SDL_memcmp(image, f_BigEndianStart, sizeof(image)) == 0);
    SDL_memcpy(image, little, sizeof(image));
    EXPECT(ConvertRomToBigEndian(image, sizeof(image)));
    EXPECT(SDL_memcmp(image, f_BigEndianStart, sizeof(image)) == 0);
    SDL_memcpy(image, unknown, sizeof(image));
    EXPECT(!ConvertRomToBigEndian(image, sizeof(image)));
    // An image of part words is no dump
    SDL_memcpy(image, little, sizeof(image));
    EXPECT(!ConvertRomToBigEndian(image, 6));
}

// Only an image of the US release's size and code is supported
static void TestSupportedRom(void) {
    uint8_t *rom = SDL_calloc(SUPPORTED_ROM_SIZE, 1);

    SDL_memcpy(rom, f_BigEndianStart, sizeof(f_BigEndianStart));
    SDL_memcpy(rom + 0x3B, "NMFE", 4);
    EXPECT(IsSupportedRom(rom, SUPPORTED_ROM_SIZE));
    EXPECT(!IsSupportedRom(rom, SUPPORTED_ROM_SIZE / 2));
    SDL_memcpy(rom + 0x3B, "NMFJ", 4);
    EXPECT(!IsSupportedRom(rom, SUPPORTED_ROM_SIZE));
    EXPECT(!IsSupportedRom(NULL, SUPPORTED_ROM_SIZE));
    SDL_free(rom);
}

// --- The controllers ---

static void TestStick(void) {
    s8 x;
    s8 y;

    // Within the dead zone, the stick rests
    MapHostStick(5000, -5000, &x, &y);
    EXPECT(x == 0 && y == 0);
    // All the way, the N64's full throw; up is positive on the N64
    MapHostStick(AXIS_MAX, 0, &x, &y);
    EXPECT(x == 80 && y == 0);
    MapHostStick(0, -AXIS_MAX, &x, &y);
    EXPECT(x == 0 && y == 80);
    MapHostStick(0, AXIS_MAX, &x, &y);
    EXPECT(x == 0 && y == -80);
    // A diagonal keeps its direction and doesn't go past the full throw
    MapHostStick(AXIS_MAX, -AXIS_MAX, &x, &y);
    EXPECT(x == y);
    EXPECT(x > 50 && x <= 80);
    // Halfway past the dead zone is about halfway
    MapHostStick(18500, 0, &x, &y);
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
    MapHostPad(&host, &pad);
    EXPECT(pad.button == (A_BUTTON | START_BUTTON | L_JPAD | Z_TRIG | L_CBUTTONS));
    EXPECT(pad.stick_x == 0 && pad.stick_y == 80);

    // A trigger only partly in doesn't press
    SDL_zero(host);
    host.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = 8000;
    host.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = 8000;
    host.axes[SDL_GAMEPAD_AXIS_RIGHTY] = AXIS_MAX;
    MapHostPad(&host, &pad);
    EXPECT(pad.button == D_CBUTTONS);
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
    AddKeyboardToHostPad(keys, &host);
    MapHostPad(&host, &pad);
    EXPECT(pad.button == (A_BUTTON | Z_TRIG | U_CBUTTONS));
    EXPECT(pad.stick_x == 80 && pad.stick_y == 0);
}

// --- The save memory ---

// The file goes in the test's working directory
static void TestSaveMemory(void) {
    const char *path = "platform_test_save.sra";
    u8 stored[PC_SRAM_SIZE];
    u8 loaded[PC_SRAM_SIZE];
    int i;

    (void)SDL_RemovePath(path);
    for (i = 0; i < PC_SRAM_SIZE; i++) {
        stored[i] = (u8)(i * 7);
    }

    // With no file yet, the memory is left as it was
    SetSaveMemoryPath(path);
    SDL_memset(loaded, 0xAA, sizeof(loaded));
    PcLoadSaveMemory(loaded, sizeof(loaded));
    EXPECT(loaded[0] == 0xAA && loaded[PC_SRAM_SIZE - 1] == 0xAA);

    PcStoreSaveMemory(stored, sizeof(stored));
    PcLoadSaveMemory(loaded, sizeof(loaded));
    EXPECT(SDL_memcmp(stored, loaded, sizeof(stored)) == 0);

    // A file of another size isn't loaded
    SDL_memset(loaded, 0xAA, sizeof(loaded));
    PcLoadSaveMemory(loaded, sizeof(loaded) / 2);
    EXPECT(loaded[0] == 0xAA);

    // Without a path, nothing is kept
    SetSaveMemoryPath(NULL);
    PcLoadSaveMemory(loaded, sizeof(loaded));
    EXPECT(loaded[0] == 0xAA);

    (void)SDL_RemovePath(path);
}

// --- The presenter ---

static void TestFitScreen(void) {
    SDL_Rect area = FitScreenToWindow(640, 480);

    EXPECT(area.x == 0 && area.y == 0 && area.w == 640 && area.h == 480);
    // A wide window has bars at the sides
    area = FitScreenToWindow(1920, 1080);
    EXPECT(area.w == 1440 && area.h == 1080 && area.x == 240 && area.y == 0);
    // A tall one at the top and bottom
    area = FitScreenToWindow(800, 1000);
    EXPECT(area.w == 800 && area.h == 600 && area.x == 0 && area.y == 200);
}

static void TestScreenPixels(void) {
    static const uint16_t pixels[] = {0xFFFF, 0xF801, 0x0001, 0x07C0};
    uint32_t rgba[SDL_arraysize(pixels)];

    ExpandScreenPixels(pixels, SDL_arraysize(pixels), rgba);
    EXPECT(rgba[0] == 0xFFFFFFFFu);
    EXPECT(rgba[1] == 0xFF0000FFu);
    EXPECT(rgba[2] == 0xFF000000u);
    EXPECT(rgba[3] == 0xFF00FF00u);
}

int main(void) {
    TestRomByteOrders();
    TestSupportedRom();
    TestStick();
    TestPadButtons();
    TestKeyboard();
    TestSaveMemory();
    TestFitScreen();
    TestScreenPixels();
    return ReportTestResults("platform_test");
}
