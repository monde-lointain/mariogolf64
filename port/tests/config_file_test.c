/*
 * Tests of the settings file on disk: created when there's none, read with
 * its problems counted, and rewritten to remember the window only when the
 * file has no problems and doesn't already hold the window's settings.
 */

#include <SDL3/SDL.h>

#include "config_file.h"
#include "test_support.h"

#ifndef _WIN32
#    include <sys/stat.h>
#endif

#define TEST_PATH "config_file_test.ini"
#define PLAYERS_COMMENT "# The player's own note\n"

static void WriteText(const char *text) {
    EXPECT(SDL_SaveFile(TEST_PATH, text, SDL_strlen(text)));
}

// Whether the file holds the text, or with NULL, whether there's none
static bool HoldsText(const char *text) {
    size_t size = 0;
    char *held = SDL_LoadFile(TEST_PATH, &size);
    bool isSame;

    if (held == NULL || text == NULL) {
        SDL_free(held);
        return held == NULL && text == NULL;
    }

    isSame = size == SDL_strlen(text) && SDL_memcmp(held, text, size) == 0;
    SDL_free(held);
    return isSame;
}

// Whether the file holds the text somewhere in it
static bool Contains(const char *text) {
    char *held = SDL_LoadFile(TEST_PATH, NULL);
    bool isFound = held != NULL && SDL_strstr(held, text) != NULL;

    SDL_free(held);
    return isFound;
}

static void SetWindow(Config *config, int width, int x) {
    SetDefaultConfig(config);
    config->windowWidth = width;
    config->windowHeight = 480;
    config->hasWindowPosition = true;
    config->windowX = x;
    config->windowY = 20;
    config->isFullscreen = true;
}

// With no file, the defaults are used and written, with no problems
static void TestCreatesMissingFile(void) {
    Config config;
    Config defaults;
    char *text;

    (void)SDL_RemovePath(TEST_PATH);
    SDL_memset(&config, 0x5A, sizeof(config));
    EXPECT(LoadConfigFile(TEST_PATH, &config) == 0);
    SetDefaultConfig(&defaults);
    EXPECT(SDL_memcmp(&config.bindings, &defaults.bindings, sizeof(defaults.bindings)) == 0);
    EXPECT(config.volume == defaults.volume && config.romPath[0] == '\0');
    text = FormatConfig(&defaults);
    EXPECT(HoldsText(text));
    SDL_free(text);
}

// What the file says goes over the defaults, and the rest stay
static void TestReadsFile(void) {
    Config config;

    WriteText("[audio]\nvolume = 30\n[video]\nscale = 2\n");
    SDL_memset(&config, 0x5A, sizeof(config));
    EXPECT(LoadConfigFile(TEST_PATH, &config) == 0);
    EXPECT(config.volume == 30 && config.renderScale == 2);
    EXPECT(config.pausesWhenUnfocused && config.romPath[0] == '\0');
    EXPECT(HoldsText("[audio]\nvolume = 30\n[video]\nscale = 2\n"));
}

static void TestCountsProblems(void) {
    Config config;

    WriteText("[audio]\nvolume = 300\nloudness = 1\n");
    EXPECT(LoadConfigFile(TEST_PATH, &config) == 2);
    EXPECT(config.volume == 100);
}

// A path that's a folder can't be read: one problem, the defaults, and
// nothing written
static void TestUnreadableFile(void) {
    const char *folder = "config_file_test_folder";
    Config config;
    Config window;

    (void)SDL_RemovePath(folder);
    EXPECT(SDL_CreateDirectory(folder));
    SDL_memset(&config, 0x5A, sizeof(config));
    EXPECT(LoadConfigFile(folder, &config) == 1);
    EXPECT(config.volume == 100);
    SetWindow(&window, 640, 10);
    EXPECT(!StoreWindowSettings(folder, &window));
    EXPECT(!SDL_GetPathInfo("config_file_test_folder.new", NULL));

    // Storing over the folder writes the new file, which can't take the
    // folder's place, and so goes
    EXPECT(!StoreConfigFile(folder, &config));
    EXPECT(!SDL_GetPathInfo("config_file_test_folder.new", NULL));
    EXPECT(SDL_RemovePath(folder));
}

#ifndef _WIN32
// A file that the player can't read stays, rather than the defaults
// taking its place
static void TestLeavesUnreadableFile(void) {
    const char *text = "[audio]\nvolume = 30\n";
    Config config;

    WriteText(text);
    EXPECT(chmod(TEST_PATH, 0) == 0);
    EXPECT(LoadConfigFile(TEST_PATH, &config) == 1);
    EXPECT(config.volume == 100);
    EXPECT(chmod(TEST_PATH, S_IRUSR | S_IWUSR) == 0);
    EXPECT(HoldsText(text));
}
#endif

// The window's settings go over what the file holds now, keeping the rest
// of what the player wrote there
static void TestStoresWindowOverFile(void) {
    Config window;
    Config config;

    WriteText(PLAYERS_COMMENT "[audio]\nvolume = 30\n");
    SetWindow(&window, 640, -10);
    window.volume = 77; // Not the window's, so not stored
    EXPECT(StoreWindowSettings(TEST_PATH, &window));
    EXPECT(!Contains(PLAYERS_COMMENT));
    EXPECT(LoadConfigFile(TEST_PATH, &config) == 0);
    EXPECT(config.volume == 30);
    EXPECT(config.windowWidth == 640 && config.windowHeight == 480);
    EXPECT(config.hasWindowPosition && config.windowX == -10 && config.windowY == 20);
    EXPECT(config.isFullscreen);
    EXPECT(!SDL_GetPathInfo(TEST_PATH ".new", NULL));
}

// A file that already holds the window's settings stays as it is, as does
// one with problems
static void TestLeavesFileAlone(void) {
    const char *same = PLAYERS_COMMENT "[video]\nfullscreen = true\n[window]\nwidth = 640\n"
                                       "height = 480\nposition = 10, 20\n";
    const char *troubled = PLAYERS_COMMENT "[window]\nwidth = 3\n";
    Config window;

    WriteText(same);
    SetWindow(&window, 640, 10);
    EXPECT(StoreWindowSettings(TEST_PATH, &window));
    EXPECT(HoldsText(same));

    // Each of the window's settings changed alone is stored
    window.windowY = 21;
    EXPECT(StoreWindowSettings(TEST_PATH, &window));
    EXPECT(!HoldsText(same));
    WriteText(same);
    SetWindow(&window, 640, 10);
    window.isFullscreen = false;
    EXPECT(StoreWindowSettings(TEST_PATH, &window));
    EXPECT(!HoldsText(same));
    WriteText(same);
    SetWindow(&window, 640, 10);
    window.hasWindowPosition = false;
    window.windowX = 0;
    window.windowY = 0;
    EXPECT(StoreWindowSettings(TEST_PATH, &window));
    EXPECT(!HoldsText(same));
    WriteText(same);
    SetWindow(&window, 640, 10);
    window.windowHeight = 481;
    EXPECT(StoreWindowSettings(TEST_PATH, &window));
    EXPECT(!HoldsText(same));

    // A place at the origin isn't the system's choice
    WriteText(PLAYERS_COMMENT "[window]\nposition = 0, 0\n");
    SetDefaultConfig(&window);
    EXPECT(StoreWindowSettings(TEST_PATH, &window));
    EXPECT(!Contains(PLAYERS_COMMENT));

    WriteText(troubled);
    SetWindow(&window, 640, 10);
    EXPECT(!StoreWindowSettings(TEST_PATH, &window));
    EXPECT(HoldsText(troubled));
}

// A file gone since the start is written afresh with the window's settings
static void TestStoresWindowWithNoFile(void) {
    Config window;
    Config config;

    (void)SDL_RemovePath(TEST_PATH);
    SetWindow(&window, 800, 10);
    EXPECT(StoreWindowSettings(TEST_PATH, &window));
    EXPECT(LoadConfigFile(TEST_PATH, &config) == 0);
    EXPECT(config.windowWidth == 800);

    // Even when the window's settings are the defaults
    (void)SDL_RemovePath(TEST_PATH);
    SetDefaultConfig(&window);
    EXPECT(StoreWindowSettings(TEST_PATH, &window));
    EXPECT(SDL_GetPathInfo(TEST_PATH, NULL));
}

// A file in a folder that isn't there can't be stored, and leaves nothing
static void TestStoreFails(void) {
    Config config;

    SetDefaultConfig(&config);
    EXPECT(!StoreConfigFile("config_file_test_nowhere/config.ini", &config));
    EXPECT(!SDL_GetPathInfo("config_file_test_nowhere", NULL));
}

int main(void) {
    TestCreatesMissingFile();
    TestReadsFile();
    TestCountsProblems();
    TestUnreadableFile();
#ifndef _WIN32
    TestLeavesUnreadableFile();
#endif
    TestStoresWindowOverFile();
    TestLeavesFileAlone();
    TestStoresWindowWithNoFile();
    TestStoreFails();
    (void)SDL_RemovePath(TEST_PATH);
    return ReportTestResults("config_file_test");
}
