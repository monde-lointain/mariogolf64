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

// zlib's CRC-32, whose check value is that of the digits 1 to 9
static void TestRomChecksum(void) {
    EXPECT(ComputeRomChecksum((const uint8_t *)"123456789", 9) == 0xCBF43926u);
    EXPECT(ComputeRomChecksum(f_BigEndianStart, 0) == 0);
}

// Writes size bytes to path, failing the test when it can't
static void WriteTestFile(const char *path, const void *data, size_t size) {
    EXPECT(SDL_SaveFile(path, data, size));
}

// Whether loading path fails, and SDL's error then names the problem
static bool IsRomRejected(const char *path, const char *problem) {
    size_t size = 1;
    uint8_t *rom = LoadRomImage(path, &size);

    if (rom != NULL) {
        SDL_free(rom);
        return false;
    }

    return SDL_strstr(SDL_GetError(), problem) != NULL && SDL_strstr(SDL_GetError(), path) != NULL;
}

// Writes an image in the N64's order to path in the .v64 order, each
// 16-bit pair swapped
static void WriteSwappedRom(const char *path, const uint8_t *rom, size_t size) {
    uint8_t *swapped = SDL_malloc(size);
    size_t i;

    SDL_assert(swapped != NULL && size % 2 == 0);
    for (i = 0; i < size; i += 2) {
        swapped[i] = rom[i + 1];
        swapped[i + 1] = rom[i];
    }

    WriteTestFile(path, swapped, size);
    SDL_free(swapped);
}

// Each image that isn't the unmodified US release is refused, saying why
static void TestRomRejections(void) {
    const char *path = "platform_test_rom.v64";
    uint8_t *rom = SDL_calloc(SUPPORTED_ROM_SIZE, 1);
    char expected[32];

    (void)SDL_RemovePath(path);
    EXPECT(IsRomRejected(path, "Can't read the ROM"));

    WriteTestFile(path, f_BigEndianStart, sizeof(f_BigEndianStart));
    EXPECT(IsRomRejected(path, "isn't an N64 ROM image"));

    SDL_memcpy(rom, f_BigEndianStart, sizeof(f_BigEndianStart));
    SDL_memcpy(rom + 0x3B, "NMFJ", 4);
    WriteSwappedRom(path, rom, SUPPORTED_ROM_SIZE);
    EXPECT(IsRomRejected(path, "game code NMFE"));

    // The US release's header on other contents. The message gives the
    // image's checksum in the N64's order, so the dump was converted.
    SDL_memcpy(rom + 0x3B, "NMFE", 4);
    WriteSwappedRom(path, rom, SUPPORTED_ROM_SIZE);
    SDL_snprintf(
        expected,
        sizeof(expected),
        "CRC-32 is %08" SDL_PRIX32 ",",
        ComputeRomChecksum(rom, SUPPORTED_ROM_SIZE)
    );
    EXPECT(IsRomRejected(path, expected));
    EXPECT(IsRomRejected(path, "2FC4C216"));

    (void)SDL_RemovePath(path);
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

// A GameCube controller's Z button is the N64's L, and its analog L and R
// triggers are Z and R
static void TestGameCubePad(void) {
    HostPadState host;
    OSContPad pad;

    SDL_zero(host);
    host.buttons = (1u << SDL_GAMEPAD_BUTTON_SOUTH) | (1u << SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    ArrangeGameCubePad(&host);
    MapHostPad(&host, &pad);
    EXPECT(pad.button == (A_BUTTON | L_TRIG));

    SDL_zero(host);
    host.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = AXIS_MAX;
    host.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = AXIS_MAX;
    ArrangeGameCubePad(&host);
    MapHostPad(&host, &pad);
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
    AddKeyboardToHostPad(keys, &host);
    MapHostPad(&host, &pad);
    EXPECT(pad.button == (A_BUTTON | Z_TRIG | U_CBUTTONS));
    EXPECT(pad.stick_x == 80 && pad.stick_y == 0);
}

// Reads the first port's buttons as the game does once a frame
static u16 ReadFirstPortButtons(void) {
    OSContPad pad;

    PcReadControllerPad(0, &pad);
    return pad.button;
}

// The game's nth read of the first port gets the script's pad at frame n.
// Reads of the other ports count no frames, and a script played afresh
// counts from frame 0 again.
static void TestScriptedFrames(void) {
    char error[64];
    InputScript *script = ParseInputScript("2 A\n3\n", error, sizeof(error));
    OSContPad pad;

    EXPECT(script != NULL);
    // Without SDL's gamepads, no device joins the ports
    InitControllers();
    PlayInputScript(script);
    EXPECT(ReadFirstPortButtons() == 0);
    PcReadControllerPad(1, &pad);
    EXPECT(ReadFirstPortButtons() == 0);
    EXPECT(ReadFirstPortButtons() == A_BUTTON);
    EXPECT(ReadFirstPortButtons() == 0);

    PlayInputScript(script);
    EXPECT(ReadFirstPortButtons() == 0);
    EXPECT(ReadFirstPortButtons() == 0);
    EXPECT(ReadFirstPortButtons() == A_BUTTON);
    PlayInputScript(NULL);
    ShutdownControllers();
    FreeInputScript(script);
}

// What the game reads from the first port, scripted or not, is recorded
// by the same frames
static void TestRecordsReadFrames(void) {
    char error[64];
    char text[256] = {0};
    InputScript *script = ParseInputScript("1 B\n", error, sizeof(error));
    InputRecorder *recorder = CreateInputRecorder(SDL_IOFromMem(text, sizeof(text) - 1), "");
    OSContPad pad;

    EXPECT(script != NULL && recorder != NULL);
    InitControllers();
    PlayInputScript(script);
    RecordFirstPort(recorder);
    (void)ReadFirstPortButtons();
    PcReadControllerPad(2, &pad);
    (void)ReadFirstPortButtons();
    (void)ReadFirstPortButtons();
    RecordFirstPort(NULL);
    (void)ReadFirstPortButtons();
    EXPECT(CloseInputRecorder(recorder));
    EXPECT(SDL_strcmp(text, "0\n1 B\n# Ends at frame 3\n") == 0);
    PlayInputScript(NULL);
    ShutdownControllers();
    FreeInputScript(script);
}

// --- The save memory ---

// The file goes in the test's working directory
static void TestSaveMemory(void) {
    const char *path = "platform_test_save.sra";
    u8 stored[PC_SRAM_SIZE];
    u8 loaded[PC_SRAM_SIZE];
    uint32_t crc;
    int i;

    (void)SDL_RemovePath(path);
    for (i = 0; i < PC_SRAM_SIZE; i++) {
        stored[i] = (u8)(i * 7);
    }

    // With no file yet, the memory is left as it was, and has no checksum
    SetSaveMemoryPath(path);
    EXPECT(!ChecksumSaveMemory(PC_SRAM_SIZE, &crc));
    SDL_memset(loaded, 0xAA, sizeof(loaded));
    PcLoadSaveMemory(loaded, sizeof(loaded));
    EXPECT(loaded[0] == 0xAA && loaded[PC_SRAM_SIZE - 1] == 0xAA);

    PcStoreSaveMemory(stored, sizeof(stored));
    PcLoadSaveMemory(loaded, sizeof(loaded));
    EXPECT(SDL_memcmp(stored, loaded, sizeof(stored)) == 0);
    EXPECT(ChecksumSaveMemory(PC_SRAM_SIZE, &crc) && crc == SDL_crc32(0, stored, sizeof(stored)));

    // A file of another size isn't loaded
    SDL_memset(loaded, 0xAA, sizeof(loaded));
    PcLoadSaveMemory(loaded, sizeof(loaded) / 2);
    EXPECT(loaded[0] == 0xAA);
    EXPECT(!ChecksumSaveMemory(PC_SRAM_SIZE / 2, &crc));

    // Without a path, nothing is kept
    SetSaveMemoryPath(NULL);
    PcLoadSaveMemory(loaded, sizeof(loaded));
    EXPECT(loaded[0] == 0xAA);
    EXPECT(!ChecksumSaveMemory(PC_SRAM_SIZE, &crc));

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
    TestRomChecksum();
    TestRomRejections();
    TestStick();
    TestPadButtons();
    TestGameCubePad();
    TestKeyboard();
    TestScriptedFrames();
    TestRecordsReadFrames();
    TestSaveMemory();
    TestFitScreen();
    TestScreenPixels();
    return ReportTestResults("platform_test");
}
