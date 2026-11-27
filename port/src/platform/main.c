/*
 * The port's program: it loads the player's ROM, opens the window, starts
 * the game on NuSystem, and then plays the part of the N64's hardware. Each
 * sixtieth of a second it reads the controllers, raises the video retrace
 * that drives the game, and shows the frame buffer that the retrace chose.
 *
 *     mariogolf64 [--scale N] [--fullscreen] [--save PATH] [--config PATH]
 *                 [--gpu-debug] [--headless] [--frames N] [--game-frames N]
 *                 [--screenshot PATH]
 *                 [--input-script PATH] [--record PATH] [ROM]
 *
 * --headless draws without a window, and --frames ends the program after N
 * retraces, writing the screen it last showed to the --screenshot file, for
 * tests and machines without a display. --game-frames ends it once the game
 * has run N frames, as a script counts them. Headless, each retrace waits
 * for the game to finish with the last, and the next follows at once, with
 * no wait for the clock. A run that stops short of its limit, as when the
 * window closes or the host ends the program, still writes the screen but
 * fails, since the screen isn't the one asked for. --input-script plays the
 * first controller from a script, as input_script.h describes, and --record
 * writes what the game reads from it as such a script, to a new file.
 *
 * Without a ROM, it looks for the settings' ROM, or else baserom.us.z64
 * beside the program, then in the current directory. Without --save, the
 * game saves to the user's data folder, or headless, nowhere. The settings
 * come from the --config file, or else config.ini in the user's data
 * folder, which config.h describes; headless, from the defaults. The
 * command line's options go over them.
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdlib.h>
#include <ultra64.h>

#include "audio_output.h"
#include "config.h"
#include "config_file.h"
#include "input.h"
#include "input_recorder.h"
#include "input_script.h"
#include "lockstep.h"
#include "options.h"
#include "port/nusys_host.h"
#include "port/os_host.h"
#include "presenter.h"
#include "rcp.h"
#include "rom_image.h"
#include "rsp.h"
#include "save_memory.h"
#include "sdl_gpu_backend.h"
#include "stall_watch.h"
#include "system/rom_arrays.h"
#include "window_place.h"

#define WINDOW_TITLE "Mario Golf"
#define DEFAULT_ROM_NAME "baserom.us.z64"
#define WINDOW_SCALE 3
#define RETRACES_PER_SECOND 60
// Retraces behind before the clock gives up catching up, after a stall
#define MAX_LATE_RETRACES 4
#define HALT_TIMEOUT_MS 2000
// How often a paused window shows its frame again
#define PAUSED_REDRAW_MS 100

// A phone's or tablet's game fills the screen, over the system's bars
#if defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_IOS)
#    define IS_ALWAYS_FULLSCREEN true
#else
#    define IS_ALWAYS_FULLSCREEN false
#endif

static SDL_Window *f_Window;
static SDL_GPUDevice *f_Device;
static bool f_IsQuitting;
static const void *f_ShownFramebuffer; // What the last retrace showed
static InputRecorder *f_Recorder;
static Config f_Config;
static char *f_ConfigPath; // Where the settings came from, or NULL
static bool f_StartsFullscreen;
static bool f_IsUnfocused; // Since another window took the focus

// --- What the game asks of the platform ---

void PcRunGfxTask(Gfx *displayList, u32 size) {
    (void)size;
    RunRspTask(displayList);
}

// The development hardware's debugger, which the game enters when told to
// halt; the port has none, so the game just stays halted
void EnterDevelopmentDebugger(s32 argument) {
    static bool isReported;

    if (!isReported) {
        SDL_Log("The game asked for its debugger (%d), which the port lacks", (int)argument);
        isReported = true;
    }
}

// --- Starting ---

// Takes the settings from the --config file, or else from the player's
// own in the user's data folder; a headless run, being a test's, takes the
// defaults without a --config file, lest the player's settings change
// what it shows. The command line's options go over the settings.
static void LoadSettings(Options *options) {
    if (options->configPath != NULL) {
        f_ConfigPath = SDL_strdup(options->configPath);
    }
    else if (!options->isHeadless) {
        f_ConfigPath = FindDefaultConfigPath();
    }

    if (f_ConfigPath != NULL) {
        (void)LoadConfigFile(f_ConfigPath, &f_Config);
    }
    else {
        SetDefaultConfig(&f_Config);
    }

    if (options->romPath == NULL && f_Config.romPath[0] != '\0') {
        options->romPath = f_Config.romPath;
    }

    if (options->renderScale == 0) {
        options->renderScale = f_Config.renderScale;
    }

    options->isFullscreen = options->isFullscreen || f_Config.isFullscreen || IS_ALWAYS_FULLSCREEN;
}

// Reads the ROM that the options name, or else the first found of the
// default name beside the program, in the current directory, and among the
// port's files, where it keeps the one that the player chose. Gives NULL
// with SDL_GetError saying why when there's none to play, and isMissing
// when there was none to try.
static uint8_t *LoadPlayersRom(const Options *options, size_t *size, bool *isMissing) {
    const char *base = SDL_GetBasePath();
    char *places[3] = {NULL, NULL, NULL};
    uint8_t *rom = NULL;
    int found;
    int i;

    *isMissing = false;
    if (options->romPath != NULL) {
        return LoadRomImage(options->romPath, size);
    }

    if (base == NULL || SDL_asprintf(&places[0], "%s%s", base, DEFAULT_ROM_NAME) < 0) {
        places[0] = NULL;
    }

    places[1] = SDL_strdup(DEFAULT_ROM_NAME);
    places[2] = FindStoredRomPath();
    found = FindRomPlace((const char *const *)places, (int)SDL_arraysize(places));
    if (found != ROM_PLACE_NONE) {
        rom = LoadRomImage(places[found], size);
    }
    else {
        *isMissing = true;
        SDL_SetError(
            "No ROM: name one, or put %s, the US release's, beside the program",
            DEFAULT_ROM_NAME
        );
    }

    for (i = 0; i < (int)SDL_arraysize(places); i++) {
        SDL_free(places[i]);
    }

    return rom;
}

// Logs and shows why there's no ROM to play. The message box may set SDL's
// error itself, so it gets a copy.
static void ShowRomProblem(void) {
    char *problem = SDL_strdup(SDL_GetError());

    SDL_Log("%s", problem != NULL ? problem : "No ROM");
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR,
        WINDOW_TITLE,
        problem != NULL ? problem : "The port needs the US release's ROM",
        NULL
    );
    SDL_free(problem);
}

// What the file dialog gave, from its callback, which may run on another
// thread: the path, NULL when the player cancelled, set before isDone. It
// outlives the wait for it, which quitting cuts short, as only one dialog is
// ever open.
typedef struct RomChoice {
    char *path;
    SDL_AtomicInt isDone;
} RomChoice;

static RomChoice f_RomChoice;

static void SDLCALL TakeChosenRom(void *userdata, const char *const *files, int filter) {
    RomChoice *choice = userdata;

    (void)filter;
    if (files == NULL) {
        SDL_Log("The file dialog failed: %s", SDL_GetError());
    }

    choice->path = (files != NULL && files[0] != NULL) ? SDL_strdup(files[0]) : NULL;
    SDL_SetAtomicInt(&choice->isDone, 1);
}

// Shows the system's dialog for choosing a file and waits until the player
// chooses or cancels, or the port is told to quit, handling the events
// meanwhile, as some dialogs need. Gives the path, to be released with
// SDL_free, or NULL.
static char *AskForRomPath(void) {
    static const SDL_DialogFileFilter filters[] = {
        {"N64 ROM images", "z64;n64;v64"},
        {"All files", "*"},
    };
    RomChoice *choice = &f_RomChoice;

    choice->path = NULL;
    SDL_SetAtomicInt(&choice->isDone, 0);
    SDL_ShowOpenFileDialog(
        TakeChosenRom,
        choice,
        NULL,
        filters,
        (int)SDL_arraysize(filters),
        NULL,
        false
    );
    while (SDL_GetAtomicInt(&choice->isDone) == 0 && !f_IsQuitting) {
        SDL_Event event;

        if (SDL_WaitEventTimeout(&event, PAUSED_REDRAW_MS) && event.type == SDL_EVENT_QUIT) {
            f_IsQuitting = true;
        }
    }

    return SDL_GetAtomicInt(&choice->isDone) != 0 ? choice->path : NULL;
}

// Asks the player for the ROM, saying why first, until they choose one that
// the port can play or cancel, and keeps a copy among the port's files,
// where the next start finds it. On Android, the app may read the file
// chosen only while it runs, so the copy is the only way to keep it.
static uint8_t *ChooseRom(size_t *size) {
    char *storedPath = FindStoredRomPath();
    uint8_t *rom = NULL;

    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_INFORMATION,
        WINDOW_TITLE,
        "Choose your copy of Mario Golf's ROM, the US release's, which the port plays. "
        "It keeps a copy of its own.",
        NULL
    );
    while (rom == NULL && !f_IsQuitting) {
        char *chosen = AskForRomPath();

        if (chosen == NULL) {
            break;
        }

        // The player just chose it, and the path may be a URI
        rom = LoadNamedRomImage(chosen, "That file", size);
        SDL_free(chosen);
        if (rom == NULL) {
            ShowRomProblem();
        }
    }

    if (rom == NULL) {
        SDL_SetError("No ROM chosen: the port needs the US release's to play");
    }
    else if (storedPath == NULL || !StoreRomImage(storedPath, rom, *size)) {
        SDL_Log("Not keeping the ROM, so it will be asked for again: %s", SDL_GetError());
    }

    SDL_free(storedPath);
    return rom;
}

// The renderer draws at a whole multiple of the N64's resolution: the
// least that covers the display's height
static int ChooseRenderScale(void) {
    const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    int scale = WINDOW_SCALE;

    if (mode != NULL && mode->h > 0) {
        const float density = (mode->pixel_density > 0.0f) ? mode->pixel_density : 1.0f;
        const int pixels = (int)SDL_ceilf((float)mode->h * density);

        scale = (pixels + SCREEN_HEIGHT - 1) / SCREEN_HEIGHT;
    }

    return SDL_clamp(scale, 1, CONFIG_SCALE_MAX);
}

// The save goes to the --save file, or else to the player's own, in the
// user's data folder. A headless run, being a test's, keeps none but a
// --save file, lest it overwrite the player's progress.
static void UseSaveMemoryPath(const Options *options) {
    char *path;

    if (options->savePath != NULL) {
        SetSaveMemoryPath(options->savePath);
        return;
    }

    if (options->isHeadless) {
        SetSaveMemoryPath(NULL);
        SDL_Log("Keeping no save, as no --save file is named");
        return;
    }

    path = FindDefaultSaveMemoryPath();
    SetSaveMemoryPath(path);
    if (path != NULL) {
        SDL_Log("Saving to %s", path);
    }

    SDL_free(path);
}

static SDL_GPUPresentMode PresentModeOf(PresentChoice choice) {
    switch (choice) {
        case PRESENT_VSYNC:
            return SDL_GPU_PRESENTMODE_VSYNC;
        case PRESENT_IMMEDIATE:
            return SDL_GPU_PRESENTMODE_IMMEDIATE;
        default:
            return SDL_GPU_PRESENTMODE_MAILBOX;
    }
}

// Whether the window would show on the displays there are now
static bool IsOnDisplays(const SDL_Rect *window) {
    SDL_Rect areas[16];
    SDL_DisplayID *displays;
    int displayCount = 0;
    int areaCount = 0;
    int i;

    displays = SDL_GetDisplays(&displayCount);
    for (i = 0; displays != NULL && i < displayCount && areaCount < (int)SDL_arraysize(areas);
        i++) {
        if (SDL_GetDisplayUsableBounds(displays[i], &areas[areaCount])) {
            areaCount++;
        }
    }

    SDL_free(displays);
    return IsWindowPlaceVisible(window, areas, areaCount);
}

// Opens the window at the size and place that the settings remember, when
// they do and the place is still on a display
static SDL_Window *CreateGameWindow(bool isFullscreen) {
    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_Rect place;
    SDL_Window *window;

    place.w = (f_Config.windowWidth > 0) ? f_Config.windowWidth : SCREEN_WIDTH * WINDOW_SCALE;
    place.h = (f_Config.windowHeight > 0) ? f_Config.windowHeight : SCREEN_HEIGHT * WINDOW_SCALE;
    place.x = f_Config.windowX;
    place.y = f_Config.windowY;
    // A phone or tablet turns the game sideways, whatever the player's
    // rotation lock says, as the N64's screen is wider than it is tall
    (void)SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    (void)SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, WINDOW_TITLE);
    (void)SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, place.w);
    (void)SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, place.h);
    (void)SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    (void)
        SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    (void)
        SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, isFullscreen);
    if (f_Config.hasWindowPosition) {
        if (IsOnDisplays(&place)) {
            (void)SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_X_NUMBER, place.x);
            (void)SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, place.y);
        }
        else {
            SDL_Log(
                "The window's remembered place is off the displays; letting the system "
                "place it"
            );
        }
    }

    window = SDL_CreateWindowWithProperties(properties);
    SDL_DestroyProperties(properties);
    return window;
}

static bool OpenWindow(const Options *options) {
    int scale;

    f_Device = CreateGpuDevice(options->isGpuDebug);
    if (f_Device == NULL) {
        SDL_Log("No GPU that the port can draw with: %s", SDL_GetError());
        return false;
    }

    if (!options->isHeadless) {
        f_Window = CreateGameWindow(options->isFullscreen);
        if (f_Window == NULL) {
            SDL_Log("Can't open the window: %s", SDL_GetError());
            return false;
        }

        f_StartsFullscreen = options->isFullscreen;
        if (!InitPresenter(f_Device, f_Window, PresentModeOf(f_Config.present))) {
            return false;
        }
    }

    scale = (options->renderScale > 0) ? options->renderScale : ChooseRenderScale();
    SDL_Log(
        "Drawing with %s at %dx%d",
        SDL_GetGPUDeviceDriver(f_Device),
        SCREEN_WIDTH * scale,
        SCREEN_HEIGHT * scale
    );
    InitGpuBackend(f_Device, (uint32_t)scale, SDL_GPU_SAMPLECOUNT_1);
    InitRdp();
    InitRsp();
    return true;
}

// --- Running ---

static void ToggleFullscreen(void) {
    const bool isFullscreen = (SDL_GetWindowFlags(f_Window) & SDL_WINDOW_FULLSCREEN) != 0;

    if (!IS_ALWAYS_FULLSCREEN) {
        (void)SDL_SetWindowFullscreen(f_Window, !isFullscreen);
    }
}

// Lays the touch controls out on the window's pixels, inside the area that
// the system's bars and the camera's cutout leave clear
static void FitTouchControls(void) {
    TouchScreen screen;
    SDL_Rect safe;
    int pixelWidth = 0;
    int pixelHeight = 0;
    int width = 0;
    int height = 0;
    float density;

    if (f_Window == NULL ||
        !SDL_GetWindowSizeInPixels(f_Window, &pixelWidth, &pixelHeight) ||
        !SDL_GetWindowSize(f_Window, &width, &height) ||
        width <= 0 ||
        height <= 0) {
        return;
    }

    if (!SDL_GetWindowSafeArea(f_Window, &safe)) {
        safe.x = 0;
        safe.y = 0;
        safe.w = width;
        safe.h = height;
    }

    // The safe area is in the window's coordinates, which may not be pixels
    density = (float)pixelWidth / (float)width;
    SDL_zero(screen);
    screen.width = (float)pixelWidth;
    screen.height = (float)pixelHeight;
    screen.pixelsPerDp = SDL_GetWindowDisplayScale(f_Window);
    if (screen.pixelsPerDp <= 0.0f) {
        screen.pixelsPerDp = 1.0f;
    }

    screen.safeLeft = (float)safe.x * density;
    screen.safeTop = (float)safe.y * density;
    screen.safeRight = (float)(width - safe.x - safe.w) * density;
    screen.safeBottom = (float)(height - safe.y - safe.h) * density;
    SetTouchScreen(&screen);
}

// Whether a touch device is a touchpad, whose fingers point rather than
// play. A screen that has gone may still lift its fingers.
static bool IsTouchpad(SDL_TouchID touch) {
    const SDL_TouchDeviceType type = SDL_GetTouchDeviceType(touch);

    return type == SDL_TOUCH_DEVICE_INDIRECT_ABSOLUTE || type == SDL_TOUCH_DEVICE_INDIRECT_RELATIVE;
}

// Shows the frame that the game last gave, with the touch controls over it
static void PresentShownFrame(void) {
    OverlayShape shapes[OVERLAY_MAX_SHAPES];

    PresentFramebuffer(f_ShownFramebuffer, shapes, DescribeShownTouchControls(shapes));
}

static void HandleEvent(const SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_QUIT:
            f_IsQuitting = true;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            f_IsUnfocused = true;
            LiftTouchControls();
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            FitTouchControls();
            break;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            if (!IsTouchpad(event->tfinger.touchID)) {
                HandleTouchEvent(event);
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            f_IsUnfocused = false;
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            ConnectGamepad(event->gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            DisconnectGamepad(event->gdevice.which);
            break;
        case SDL_EVENT_KEY_DOWN:
            HandleKeyEvent(event);
            if (!event->key.repeat &&
                (event->key.scancode == SDL_SCANCODE_F11 ||
                    (event->key.scancode == SDL_SCANCODE_RETURN &&
                        (event->key.mod & SDL_KMOD_ALT) != 0))) {
                ToggleFullscreen();
            }
            break;
        default:
            break;
    }
}

// Handles the events that have come, and tells whether one asked to quit
static bool PollQuitEvents(void) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        HandleEvent(&event);
    }

    return f_IsQuitting;
}

// Logs the stalls that a retrace ends in a run in real time, for a
// player's report of a freeze
static void ReportStalls(Stalls stalls, long frameCount, long gameFrame) {
    if (stalls.hostNs != 0) {
        SDL_Log(
            "Stall: retrace %ld came %" SDL_PRIu64 " ms after the last, at game frame %ld",
            frameCount,
            stalls.hostNs / SDL_NS_PER_MS,
            gameFrame
        );
    }

    if (stalls.gameNs != 0) {
        SDL_Log(
            "Stall: game frame %ld came %" SDL_PRIu64 " ms after the last",
            gameFrame,
            stalls.gameNs / SDL_NS_PER_MS
        );
    }
}

// While another window has the focus, no retraces come, so the game and
// its music wait, and the window shows the last frame. A window that never
// had the focus, as under a display without a window manager, runs.
static void WaitWhileUnfocused(void) {
    SDL_Log("Paused at game frame %ld while another window has the focus", CountGameFrames());
    while (!f_IsQuitting && f_IsUnfocused) {
        SDL_Event event;

        if (SDL_WaitEventTimeout(&event, PAUSED_REDRAW_MS)) {
            HandleEvent(&event);
        }

        PresentShownFrame();
    }

    SDL_Log("Resumed at game frame %ld", CountGameFrames());
}

// Raises the retraces at their own pace, whatever the display's rate. After
// a stall the clock starts afresh rather than rush the game to catch up. In
// lockstep, each retrace instead waits for the game to finish with it, so
// that the game sees every one, wherever the host is slow, and the next
// follows at once: what the game computes doesn't depend on the clock, which
// only its debugging meters read. A run in real time logs its stalls, and
// pauses, if the settings say so, while the window is unfocused. Returns the
// retraces raised.
static long RunRetraces(long frameLimit, long gameFrameLimit, bool isLockstep) {
    const Uint64 period = SDL_NS_PER_SECOND / RETRACES_PER_SECOND;
    Uint64 next = SDL_GetTicksNS();
    long frameCount = 0;
    StallWatch stallWatch;

    // The game boots to its first wait before the first retrace, so that it
    // sees that retrace however long the host takes to get to it
    if (isLockstep) {
        (void)AwaitGameInLockstep(LOCKSTEP_BOOT, PollQuitEvents);
    }

    StartStallWatch(&stallWatch, next, CountGameFrames());

    while (!f_IsQuitting &&
        (frameLimit == 0 || frameCount < frameLimit) &&
        (gameFrameLimit == 0 || CountGameFrames() < gameFrameLimit)) {
        SDL_Event event;
        Uint64 now;
        long gameFrame;

        while (SDL_PollEvent(&event)) {
            HandleEvent(&event);
        }

        if (!isLockstep && f_IsUnfocused && f_Config.pausesWhenUnfocused) {
            WaitWhileUnfocused();
            next = SDL_GetTicksNS();
            StartStallWatch(&stallWatch, next, CountGameFrames());
            continue;
        }

        UpdateControllers();

        f_ShownFramebuffer = PcRaiseRetrace();
        if (isLockstep) {
            (void)AwaitGameInLockstep(frameCount, PollQuitEvents);
        }

        if (f_Window != NULL) {
            PresentShownFrame();
        }

        frameCount++;
        if (isLockstep) {
            continue;
        }

        now = SDL_GetTicksNS();
        gameFrame = CountGameFrames();
        ReportStalls(WatchRetrace(&stallWatch, now, gameFrame), frameCount, gameFrame);
        next += period;
        if (now < next) {
            SDL_DelayPrecise(next - now);
        }
        else if (now - next > period * MAX_LATE_RETRACES) {
            next = now;
        }
    }

    return frameCount;
}

// --- Ending ---

static bool IsVideoDriver(const char *name) {
    const char *driver = SDL_GetCurrentVideoDriver();

    return driver != NULL && SDL_strcmp(driver, name) == 0;
}

// Remembers in the settings file where the player left the window: its
// size and place when it's neither fullscreen, maximized nor minimized,
// and whether it's fullscreen when the player changed that. A size left
// at the default stays the default.
static void RememberWindow(void) {
    const SDL_WindowFlags states =
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED;
    const int limit = CONFIG_WINDOW_POSITION_LIMIT;
    Config window = f_Config;
    SDL_WindowFlags flags;
    bool isFullscreen;
    int width;
    int height;
    int x;
    int y;

    if (f_Window == NULL || f_ConfigPath == NULL) {
        return;
    }

    flags = SDL_GetWindowFlags(f_Window);
    isFullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    if (isFullscreen != f_StartsFullscreen) {
        window.isFullscreen = isFullscreen;
    }

    if ((flags & states) == 0 &&
        SDL_GetWindowSize(f_Window, &width, &height) &&
        (window.windowWidth != 0 ||
            width != SCREEN_WIDTH * WINDOW_SCALE ||
            height != SCREEN_HEIGHT * WINDOW_SCALE)) {
        window.windowWidth = SDL_clamp(width, CONFIG_WINDOW_SIZE_MIN, CONFIG_WINDOW_SIZE_MAX);
        window.windowHeight = SDL_clamp(height, CONFIG_WINDOW_SIZE_MIN, CONFIG_WINDOW_SIZE_MAX);
    }

    // Wayland tells no window where it is
    if ((flags & states) == 0 &&
        !IsVideoDriver("wayland") &&
        SDL_GetWindowPosition(f_Window, &x, &y)) {
        window.hasWindowPosition = true;
        window.windowX = SDL_clamp(x, -limit, limit);
        window.windowY = SDL_clamp(y, -limit, limit);
    }

    (void)StoreWindowSettings(f_ConfigPath, &window);
}

// With the game halted, nothing else uses the renderer or the audio, so
// they go in the order that each needs: the renderer's targets before the
// backend, and the backend before the device. A game that won't halt
// still holds them, so the program leaves without releasing anything.
static bool ShutDown(const char *screenshotPath) {
    bool isOutputWhole = true;

    RememberWindow();
    SDL_free(f_ConfigPath);
    f_ConfigPath = NULL;
    if (!PcHaltGame(HALT_TIMEOUT_MS)) {
        SDL_Log("The game didn't stop; leaving without cleaning up");
        _Exit(EXIT_FAILURE);
    }

    RecordFirstPort(NULL);
    if (!CloseInputRecorder(f_Recorder)) {
        SDL_Log("The input recording is incomplete: a write failed");
        isOutputWhole = false;
    }

    CloseAudioOutput();
    if (screenshotPath != NULL && !SaveScreenImage(f_ShownFramebuffer, screenshotPath)) {
        isOutputWhole = false;
    }

    ShutdownRdp();
    ShutdownGpuBackend();
    if (f_Window != NULL) {
        ShutdownPresenter();
    }

    SDL_DestroyGPUDevice(f_Device);

    ShutdownControllers();
    SDL_DestroyWindow(f_Window);
    SDL_Quit();
    return isOutputWhole;
}

// Reads the script that --input-script names, if any; false if it can't
static bool LoadInputScript(const char *path, InputScript **script) {
    char error[128];
    char *text;

    *script = NULL;
    if (path == NULL) {
        return true;
    }

    text = SDL_LoadFile(path, NULL);
    if (text == NULL) {
        SDL_Log("Can't read the input script %s: %s", path, SDL_GetError());
        return false;
    }

    *script = ParseInputScript(text, error, sizeof(error));
    SDL_free(text);
    if (*script == NULL) {
        SDL_Log("The input script %s, %s", path, error);
        return false;
    }

    return true;
}

// Starts the recording that --record names, if any, in a new file, with a
// header saying when and from which save it starts; false if it can't
static bool StartRecording(const char *path) {
    char header[256];
    SDL_DateTime date;
    SDL_Time now = 0;
    SDL_IOStream *stream;
    uint32_t crc;
    int length;

    if (path == NULL) {
        return true;
    }

    if (SDL_GetPathInfo(path, NULL)) {
        SDL_Log("%s already exists; name a new file for the recording", path);
        return false;
    }

    SDL_zero(date);
    (void)SDL_GetCurrentTime(&now);
    (void)SDL_TimeToDateTime(now, &date, true);
    length = SDL_snprintf(
        header,
        sizeof(header),
        "# Recorded by mariogolf64 on %04d-%02d-%02d at %02d:%02d\n",
        date.year,
        date.month,
        date.day,
        date.hour,
        date.minute
    );
    if (ChecksumSaveMemory(PC_SRAM_SIZE, &crc)) {
        SDL_snprintf(
            header + length,
            sizeof(header) - (size_t)length,
            "# Starts from the save with CRC-32 %08" SDL_PRIX32 "\n",
            crc
        );
    }
    else {
        SDL_snprintf(header + length, sizeof(header) - (size_t)length, "# Starts with no save\n");
    }

    stream = SDL_IOFromFile(path, "w");
    f_Recorder = (stream != NULL) ? CreateInputRecorder(stream, header) : NULL;
    if (f_Recorder == NULL) {
        SDL_Log("Can't record to %s: %s", path, SDL_GetError());
        return false;
    }

    SDL_Log("Recording the first controller to %s", path);
    return true;
}

int main(int argc, char *argv[]) {
    Options options;
    InputScript *script;
    uint8_t *rom;
    size_t romSize = 0;
    bool isRomMissing;
    long frames;
    long gameFrames;
    bool isFinished;

    if (!ParseOptions(argc, argv, &options) || !LoadInputScript(options.inputScriptPath, &script)) {
        return EXIT_FAILURE;
    }

    LoadSettings(&options);

    UseSaveMemoryPath(&options);
    if (!StartRecording(options.recordingPath)) {
        FreeInputScript(script);
        return EXIT_FAILURE;
    }

    // Without a window, the GPU still needs a video driver, but not a display
    if (options.isHeadless) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("Can't start SDL: %s", SDL_GetError());
        FreeInputScript(script);
        return EXIT_FAILURE;
    }

    rom = LoadPlayersRom(&options, &romSize, &isRomMissing);
    if (rom == NULL && isRomMissing && !options.isHeadless) {
        rom = ChooseRom(&romSize);
    }

    if (rom == NULL) {
        // Told to quit while choosing: nobody's there to answer a box
        if (f_IsQuitting) {
            SDL_Log("Quit before a ROM was chosen");
        }
        else {
            ShowRomProblem();
        }

        (void)CloseInputRecorder(f_Recorder);
        FreeInputScript(script);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    if (!OpenWindow(&options)) {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            WINDOW_TITLE,
            "Can't draw: no suitable GPU. See the log.",
            NULL
        );
        (void)CloseInputRecorder(f_Recorder);
        FreeInputScript(script);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    InitControllers();
    FitTouchControls();
    SetPadBindings(&f_Config.bindings);
    SetAudioOutputVolume((float)f_Config.volume / (float)CONFIG_VOLUME_MAX);
    PlayInputScript(script);
    RecordFirstPort(f_Recorder);
    PcInsertCartridge(rom, romSize);
    FillGameRomArrays();
    PcInitOs();
    PcBootNuSystem();
    frames = RunRetraces(options.frameLimit, options.gameFrameLimit, options.isHeadless);
    gameFrames = CountGameFrames();
    isFinished = HasRunReachedLimit(&options, frames, gameFrames);
    if (!isFinished) {
        SDL_Log(
            "Stopped short of the limit, after %ld retraces and %ld game frames",
            frames,
            gameFrames
        );
    }

    if (!ShutDown(options.screenshotPath) || !isFinished) {
        FreeInputScript(script);
        SDL_free(rom);
        return EXIT_FAILURE;
    }

    FreeInputScript(script);
    SDL_free(rom);
    return EXIT_SUCCESS;
}
