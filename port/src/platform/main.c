/*
 * The port's program: it loads the player's ROM, opens the window, starts
 * the game on NuSystem, and then plays the part of the N64's hardware. Each
 * sixtieth of a second it reads the controllers, raises the video retrace
 * that drives the game, and shows the frame buffer that the retrace chose.
 *
 *     mariogolf64 [--scale N] [--fullscreen] [--save PATH] [--gpu-debug]
 *                 [--headless] [--frames N] [--game-frames N]
 *                 [--screenshot PATH]
 *                 [--input-script PATH] [--record PATH] [ROM]
 *
 * --headless draws without a window, and --frames ends the program after N
 * retraces, writing the screen it last showed to the --screenshot file, for
 * tests and machines without a display. --game-frames ends it once the game
 * has run N frames, as a script counts them. Headless, each retrace waits for
 * the game to finish with the last, and the next follows at once, with no
 * wait for the clock. --input-script plays the first
 * controller from a script, as input_script.h describes, and --record
 * writes what the game reads from it as such a script, to a new file.
 *
 * Without a ROM, it looks for baserom.us.z64 beside the program, then in
 * the current directory. Without --save, the game saves to the user's data
 * folder, or headless, nowhere.
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdlib.h>
#include <ultra64.h>

#include "audio_output.h"
#include "input.h"
#include "input_recorder.h"
#include "input_script.h"
#include "port/nusys_host.h"
#include "port/os_host.h"
#include "presenter.h"
#include "rcp.h"
#include "rom_image.h"
#include "rsp.h"
#include "save_memory.h"
#include "sdl_gpu_backend.h"

#define WINDOW_TITLE "Mario Golf"
#define DEFAULT_ROM_NAME "baserom.us.z64"
#define WINDOW_SCALE 3
#define MAX_RENDER_SCALE 8
#define RETRACES_PER_SECOND 60
// Retraces behind before the clock gives up catching up, after a stall
#define MAX_LATE_RETRACES 4
#define HALT_TIMEOUT_MS 2000
// How long a headless retrace waits for the game to finish with the last
#define LOCKSTEP_TIMEOUT_MS 10000

typedef struct Options {
    const char *romPath;
    const char *savePath;
    const char *screenshotPath;
    const char *inputScriptPath;
    const char *recordingPath;
    int renderScale; // 0 to choose from the display
    long frameLimit; // Retraces to run, or 0 for no limit
    long gameFrameLimit; // The game's frames to run, or 0 for no limit
    bool isFullscreen;
    bool isGpuDebug;
    bool isHeadless;
} Options;

static SDL_Window *f_Window;
static SDL_GPUDevice *f_Device;
static bool f_IsQuitting;
static const void *f_ShownFramebuffer; // What the last retrace showed
static InputRecorder *f_Recorder;

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

static void PrintUsage(const char *program) {
    SDL_Log(
        "Usage: %s [--scale N] [--fullscreen] [--save PATH] [--gpu-debug] [--headless] "
        "[--frames N] [--game-frames N] [--screenshot PATH] [--input-script PATH] "
        "[--record PATH] [ROM]",
        program
    );
}

static bool ParseOptions(int argc, char **argv, Options *options) {
    int i;

    SDL_zerop(options);
    for (i = 1; i < argc; i++) {
        const char *argument = argv[i];

        if (SDL_strcmp(argument, "--scale") == 0 && i + 1 < argc) {
            options->renderScale = SDL_atoi(argv[++i]);
            if (options->renderScale < 1 || options->renderScale > MAX_RENDER_SCALE) {
                SDL_Log("The scale must be from 1 to %d", MAX_RENDER_SCALE);
                return false;
            }
        }
        else if (SDL_strcmp(argument, "--save") == 0 && i + 1 < argc) {
            options->savePath = argv[++i];
        }
        else if (SDL_strcmp(argument, "--frames") == 0 && i + 1 < argc) {
            options->frameLimit = SDL_strtol(argv[++i], NULL, 10);
            if (options->frameLimit < 1) {
                SDL_Log("The frame count must be at least 1");
                return false;
            }
        }
        else if (SDL_strcmp(argument, "--game-frames") == 0 && i + 1 < argc) {
            options->gameFrameLimit = SDL_strtol(argv[++i], NULL, 10);
            if (options->gameFrameLimit < 1) {
                SDL_Log("The game frame count must be at least 1");
                return false;
            }
        }
        else if (SDL_strcmp(argument, "--screenshot") == 0 && i + 1 < argc) {
            options->screenshotPath = argv[++i];
        }
        else if (SDL_strcmp(argument, "--input-script") == 0 && i + 1 < argc) {
            options->inputScriptPath = argv[++i];
        }
        else if (SDL_strcmp(argument, "--record") == 0 && i + 1 < argc) {
            options->recordingPath = argv[++i];
        }
        else if (SDL_strcmp(argument, "--headless") == 0) {
            options->isHeadless = true;
        }
        else if (SDL_strcmp(argument, "--fullscreen") == 0) {
            options->isFullscreen = true;
        }
        else if (SDL_strcmp(argument, "--gpu-debug") == 0) {
            options->isGpuDebug = true;
        }
        else if (argument[0] != '-' && options->romPath == NULL) {
            options->romPath = argument;
        }
        else {
            PrintUsage(argv[0]);
            return false;
        }
    }

    return true;
}

// Reads the ROM that the options name, or else the first found of the
// default name beside the program and in the current directory. Gives NULL
// with SDL_GetError saying why when there's none to play.
static uint8_t *LoadPlayersRom(const Options *options, size_t *size) {
    const char *base = SDL_GetBasePath();
    char *besideProgram = NULL;
    uint8_t *rom;

    if (options->romPath != NULL) {
        return LoadRomImage(options->romPath, size);
    }

    if (base != NULL && SDL_asprintf(&besideProgram, "%s%s", base, DEFAULT_ROM_NAME) >= 0) {
        if (SDL_GetPathInfo(besideProgram, NULL)) {
            rom = LoadRomImage(besideProgram, size);
            SDL_free(besideProgram);
            return rom;
        }

        SDL_free(besideProgram);
    }

    if (SDL_GetPathInfo(DEFAULT_ROM_NAME, NULL)) {
        return LoadRomImage(DEFAULT_ROM_NAME, size);
    }

    SDL_SetError(
        "No ROM: name one, or put %s, the US release's, beside the program",
        DEFAULT_ROM_NAME
    );
    return NULL;
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

    return SDL_clamp(scale, 1, MAX_RENDER_SCALE);
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

static bool OpenWindow(const Options *options) {
    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
        (options->isFullscreen ? SDL_WINDOW_FULLSCREEN : 0);
    int scale;

    f_Device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL,
        options->isGpuDebug,
        NULL
    );
    if (f_Device == NULL) {
        SDL_Log("No GPU that the port can draw with: %s", SDL_GetError());
        return false;
    }

    if (!options->isHeadless) {
        f_Window = SDL_CreateWindow(
            WINDOW_TITLE,
            SCREEN_WIDTH * WINDOW_SCALE,
            SCREEN_HEIGHT * WINDOW_SCALE,
            flags
        );
        if (f_Window == NULL) {
            SDL_Log("Can't open the window: %s", SDL_GetError());
            return false;
        }

        if (!InitPresenter(f_Device, f_Window)) {
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
    InitGpuBackend(f_Device, (uint32_t)scale);
    InitRdp();
    InitRsp();
    return true;
}

// --- Running ---

static void ToggleFullscreen(void) {
    const bool isFullscreen = (SDL_GetWindowFlags(f_Window) & SDL_WINDOW_FULLSCREEN) != 0;

    (void)SDL_SetWindowFullscreen(f_Window, !isFullscreen);
}

static void HandleEvent(const SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_QUIT:
            f_IsQuitting = true;
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            ConnectGamepad(event->gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            DisconnectGamepad(event->gdevice.which);
            break;
        case SDL_EVENT_KEY_DOWN:
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

// Raises the retraces at their own pace, whatever the display's rate. After
// a stall the clock starts afresh rather than rush the game to catch up. In
// lockstep, each retrace instead waits for the game to finish with it, so
// that the game sees every one, wherever the host is slow, and the next
// follows at once: what the game computes doesn't depend on the clock, which
// only its debugging meters read.
static void RunRetraces(long frameLimit, long gameFrameLimit, bool isLockstep) {
    const Uint64 period = SDL_NS_PER_SECOND / RETRACES_PER_SECOND;
    Uint64 next = SDL_GetTicksNS();
    long frameCount = 0;

    // The game boots to its first wait before the first retrace, so that it
    // sees that retrace however long the host takes to get to it
    if (isLockstep && !PcWaitForIdleCpu(LOCKSTEP_TIMEOUT_MS)) {
        SDL_Log("The game didn't finish booting in time");
    }

    while (!f_IsQuitting &&
        (frameLimit == 0 || frameCount < frameLimit) &&
        (gameFrameLimit == 0 || CountGameFrames() < gameFrameLimit)) {
        SDL_Event event;
        Uint64 now;

        while (SDL_PollEvent(&event)) {
            HandleEvent(&event);
        }

        UpdateControllers();

        f_ShownFramebuffer = PcRaiseRetrace();
        if (isLockstep && !PcWaitForIdleCpu(LOCKSTEP_TIMEOUT_MS)) {
            SDL_Log("The game didn't finish retrace %ld in time", frameCount);
        }

        if (f_Window != NULL) {
            PresentFramebuffer(f_ShownFramebuffer);
        }

        frameCount++;
        if (isLockstep) {
            continue;
        }

        next += period;
        now = SDL_GetTicksNS();
        if (now < next) {
            SDL_DelayPrecise(next - now);
        }
        else if (now - next > period * MAX_LATE_RETRACES) {
            next = now;
        }
    }
}

// --- Ending ---

// With the game halted, nothing else uses the renderer or the audio, so
// they go in the order that each needs: the renderer's targets before the
// backend, and the backend before the device. A game that won't halt
// still holds them, so the program leaves without releasing anything.
static bool ShutDown(const char *screenshotPath) {
    bool isOutputWhole = true;

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

    if (!ParseOptions(argc, argv, &options) || !LoadInputScript(options.inputScriptPath, &script)) {
        return EXIT_FAILURE;
    }

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

    rom = LoadPlayersRom(&options, &romSize);
    if (rom == NULL) {
        ShowRomProblem();
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
    PlayInputScript(script);
    RecordFirstPort(f_Recorder);
    PcInitOs();
    PcInsertCartridge(rom, romSize);
    PcBootNuSystem();
    RunRetraces(options.frameLimit, options.gameFrameLimit, options.isHeadless);
    if (!ShutDown(options.screenshotPath)) {
        FreeInputScript(script);
        SDL_free(rom);
        return EXIT_FAILURE;
    }

    FreeInputScript(script);
    SDL_free(rom);
    return EXIT_SUCCESS;
}
