/*
 * The game's main thread, mainproc, which starts every system and then runs
 * the game from the graphics retrace callback. The file also keeps the debug
 * game flags, reads the controllers and prepares for a reset.
 */

#include "system/main.h"

#include <PR/libmus.h>
#include <string.h>
#include <ultra64.h>

#include "audio/sound.h"
#include "characters/character.h"
#include "characters/character_model.h"
#include "controllers/controller.h"
#include "controllers/transfer_pak.h"
#include "course/hole_loader.h"
#include "course/terrain.h"
#include "course/work_area.h"
#include "debug/debug_text.h"
#include "debug/game_flags.h"
#include "game/game.h"
#include "game/scene.h"
#include "math/math_util.h"
#include "round/play.h"
#include "save/save.h"
#include "screen/screen.h"
#include "shot/ball_simulation.h"
#include "shot/club.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/crash_screen.h"
#include "system/heap.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"
#include "system/segment_symbols.h"

// The audio interface's sample rate, in Hz
#define AUDIO_FREQUENCY 32000

// Frame buffers that the game swaps between
#define FRAMEBUFFER_COUNT 3

// The bytes of one frame buffer's 16-bit texels
#define FRAMEBUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(u16))

// The frame buffers fill the end of the base RDRAM, in order
#define FramebufferAddress(index) \
    ((u16 *)(BASE_RDRAM_END - ((FRAMEBUFFER_COUNT - (index)) * FRAMEBUFFER_SIZE)))

// The first flag in each byte is its top bit
#define GameFlagByte(flag) (f_GameFlagBits[(flag) / GAME_FLAGS_PER_BYTE])
#define GameFlagMask(flag) (0x80 >> ((flag) % GAME_FLAGS_PER_BYTE))
// Every bit but the flag's, as the high byte's ones shift in above it
#define GameFlagClearMask(flag) (0xFF7F >> ((flag) % GAME_FLAGS_PER_BYTE))

// Character slots that get a buffer in g_WorkArea
enum CharacterSlotRole {
    CHARACTER_SLOT_PARTNER = 1,
    CHARACTER_SLOT_CADDIE,
    CHARACTER_SLOT_GOLFER
};

// Iterations of the busy loop between two reports of RunHeartbeatLoop
#define HEARTBEAT_LOOP_COUNT 100000000

// Values of f_MainLoopState
enum MainLoopState {
    MAIN_LOOP_START,
    MAIN_LOOP_RUNNING = -1
};

typedef s32 MainLoopState;

// Values of f_DebugHaltState. Holding HALT_BUTTONS on the first controller
// while halting is allowed halts the game and hands the console over to
// the development debugger.
enum HaltState {
    HALT_STATE_DISALLOWED,
    HALT_STATE_ALLOWED,
    HALT_STATE_HALTED
};

typedef u8 HaltState;

#define HALT_BUTTONS (A_BUTTON | B_BUTTON | L_TRIG | R_TRIG)

// Graphics tasks that may wait for the RDP before a frame is skipped,
// depending on g_FramePacingMode
#define PENDING_TASK_MAX 2
#define PENDING_TASK_MAX_RELAXED 4

// Retraces between two updates of g_FramesPerSecond
#define RETRACES_PER_SECOND 60

// The stick reads as a direction once it is pushed past
// STICK_PRESS_THRESHOLD, and keeps doing so until it falls back below
// STICK_RELEASE_THRESHOLD
#define STICK_PRESS_THRESHOLD 56
#define STICK_RELEASE_THRESHOLD 20

// Stick readings this close to the center count as 0
#define STICK_DEAD_ZONE 16

// The largest stick magnitude in menu readings
#define MENU_STICK_MAX 64

// The largest stick magnitude in a ControllerState, after the dead zone
#define STATE_STICK_MAX 44

// A ControllerState counts the stick as pressing a direction past this
#define STATE_STICK_DIRECTION_MIN 30

// The original's variables are defined in the order of its data and bss

Boolean8 g_IsDebugEnabled = FALSE;
Boolean g_HasExpansionPak = FALSE;
static volatile HaltState f_DebugHaltState = HALT_STATE_DISALLOWED;
Boolean g_HasThreeFrameBuffers = FALSE;
s32 g_FramesPerSecond = 0;

// The original reserves these bytes, but nothing reads them
static u8 f_UnusedBytesAfterFramesPerSecond[4] UNUSED = {0};

static u16 *f_Framebuffers[FRAMEBUFFER_COUNT] = {
    FramebufferAddress(0),
    FramebufferAddress(1),
    FramebufferAddress(2),
};
static s32 f_RetraceCount = 0;
static s32 f_FramesThisSecond = 0;
FrameStep g_FrameStepState = FRAME_STEP_RUNNING;
volatile s32 g_FrameCount = 0;
static Boolean f_IsOddRetrace = FALSE;

void HandleGfxRetrace(s32 pendingTaskCount);
// The development debugger's entry, outside the game's code
void EnterDevelopmentDebugger(s32 argument);

Boolean g_HasOwnControllers;
s32 g_ControllerCount;
NUContData g_ControllerData[MAXCONTROLLERS];
ControllerState g_ControllerStates[MAXCONTROLLERS];
NUContData g_MenuControllerData[MAXCONTROLLERS];
MenuRepeatState g_MenuRepeatStates[MAXCONTROLLERS];
ControllerState g_MergedController;
s32 g_PlayerControllerPorts[MAXCONTROLLERS];
NUContData g_PreviousControllerData[MAXCONTROLLERS];
char g_RomBuildDate[BUILD_DATE_LENGTH_MAX + 1];
char g_SaveBuildDate[SAVE_BUILD_DATE_SIZE];
ControllerState g_TurnController;

FILE_STATIC volatile MainLoopState f_MainLoopState;
FILE_STATIC u8 f_GameFlagBits[GAME_FLAG_BYTE_COUNT];
// The menus' readings from the last frame
FILE_STATIC NUContData f_PreviousMenuControllerData[MAXCONTROLLERS];
// One bit for each port with a controller plugged in, from bit 0
FILE_STATIC u8 f_ConnectedControllerBits;

// Reports a failed assertion to the host, and carries on
void ReportFailedAssertion(const char *expression UNUSED, const char *fileName, s32 lineNumber) {
    osSyncPrintf("Assertion Fault : File %s Line %d\n", fileName, lineNumber);
}

// The game's own version of libultra's guNormalize, which scales the vector
// to a length of 1
void guNormalize(f32 *x, f32 *y, f32 *z) {
    f32 xValue = *x;
    f32 yValue = *y;
    f32 zValue = *z;
    // The squared length, and then the inverse of the length
    f32 length = (xValue * xValue) + (yValue * yValue) + (zValue * zValue);

    if (length < 0.0f) {
        osSyncPrintf("NormalizeError\n");
        *x = 0.0f;
        *y = 1.0f;
        *z = 0.0f;
    }
    else {
        length = 1.0f / sqrtf(length);
        *x = xValue * length;
        *y = yValue * length;
        *z = zValue * length;
    }
}

void InitFramebuffers(void) {
    nuGfxSetCfb(f_Framebuffers, FRAMEBUFFER_COUNT);
}

// The debug game flags. A flag out of range reads as clear and doesn't change.
Boolean IsGameFlagSet(u32 flag) {
    if (flag < GAME_FLAG_COUNT) {
        if (GameFlagByte(flag) & GameFlagMask(flag)) {
            return TRUE;
        }

        return FALSE;
    }

    return FALSE;
}

void SetGameFlag(u32 flag) {
    if (flag < GAME_FLAG_COUNT) {
        GameFlagByte(flag) |= GameFlagMask(flag);
    }
}

void ClearGameFlag(u32 flag) {
    if (flag < GAME_FLAG_COUNT) {
        GameFlagByte(flag) &= GameFlagClearMask(flag);
    }
}

void ToggleGameFlag(u32 flag) {
    if (flag < GAME_FLAG_COUNT) {
        GameFlagByte(flag) ^= GameFlagMask(flag);
    }
}

void ClearAllGameFlags(void) {
    s32 byteIndex;

    for (byteIndex = 0; byteIndex < GAME_FLAG_BYTE_COUNT; byteIndex++) {
        f_GameFlagBits[byteIndex] = 0;
    }
}

// A thread body that reports to the host, now and then, that it still runs.
// Nothing starts it.
void RunHeartbeatLoop(void) {
    s32 iteration;

    while (TRUE) {
        for (iteration = 0; iteration != HEARTBEAT_LOOP_COUNT; iteration++) {
        }

        osSyncPrintf("thread %d\n", HEARTBEAT_LOOP_COUNT);
    }
}

// Divides the start of g_WorkArea among the character slots
void AssignCharacterBuffers(void) {
    s32 bufferSize = GetCharacterBufferSize();

    SetCharacterBuffer(CHARACTER_SLOT_PARTNER, g_WorkArea.bytes);
    SetCharacterBuffer(CHARACTER_SLOT_GOLFER, g_WorkArea.bytes + bufferSize);
    SetCharacterBuffer(CHARACTER_SLOT_CADDIE, g_WorkArea.bytes + (bufferSize * 2));
    osSyncPrintf("GMemLeft=%x\n", WORK_AREA_SIZE - (bufferSize * 3));
}

// Stops the rumble and mutes the music once the reset button is pressed
void PrepareForReset(void) {
    StopAllRumble();
    __libmus_output_muted = TRUE;
}

// Clears the exception enables of the floating-point control and status
// register, so that float errors give IEEE results instead of trapping.
// The first mask clears bits 8 to 12 and 28 to 31, the second the enables
// in bits 7 to 11, and nothing is set again.
void DisableFloatExceptions(void) {
    __asm__ volatile("cfc1 $4, $31\n"
                     "nop\n"
                     "li $5, 0x0FFFE0FF\n"
                     "and $4, $4, $5\n"
                     "li $5, -0xF81\n"
                     "and $4, $4, $5\n"
                     "li $5, 0\n"
                     "or $4, $4, $5\n"
                     "ctc1 $4, $31\n"
                     "nop\n"
                     "nop\n"
        :
        :
        : "$4", "$5");
}

// Starts every system, then hands the game to HandleGfxRetrace and waits,
// showing any message that g_MessageFramebuffer holds
void mainproc(void *arg UNUSED) {
    u32 buildDateSize;
    s32 portIndex;

    osAiSetFrequency(AUDIO_FREQUENCY);
    // Clear all memory past the main segment's bss: the overlays, the heaps
    // and the framebuffers
    bzero(main_BSS_END, BytesBetween(main_BSS_END, BASE_RDRAM_END));
    osWritebackDCacheAll();
    DisableFloatExceptions();
    RunBootHook();
    osSyncPrintf("MainEntry\n");

    // Read the build date, cut to fit g_RomBuildDate
    buildDateSize = build_date_ROM_END - build_date_ROM_START;
    if (buildDateSize >= BUILD_DATE_LENGTH_MAX) {
        buildDateSize = BUILD_DATE_LENGTH_MAX;
    }

    nuPiReadRom((u32)build_date_ROM_START, g_RomBuildDate, buildDateSize);
    g_RomBuildDate[buildDateSize] = '\0';
    g_SaveBuildDate[0] = '\0';

    nuGfxInitEX2();
    g_FrameCount = 0;
    InitHeapRegions(g_HasExpansionPak);
    ResetHeap(HEAP_BILLBOARD);
    InitAssetFiles();
    ClearAllGameFlags();
    InitOverlayManager();
    LoadOverlay(GetOverlayOfModule(MODULE_BOOT_LOGOS));
    LoadOverlay(GetOverlayOfModule(MODULE_HOLE_BUILDER));
    osSyncPrintf("S-InitSRAM\n");
    InitSaveData();
    osSyncPrintf("E-InitSRAM\n");
    InitFramebuffers();

    // Give the connected controllers to the players in port order
    f_ConnectedControllerBits = nuContInit();
    g_ControllerCount = 0;
    for (portIndex = 0; portIndex < MAXCONTROLLERS; portIndex++) {
        if ((f_ConnectedControllerBits >> portIndex) & 1) {
            g_PlayerControllerPorts[g_ControllerCount++] = portIndex;
        }

#ifdef AVOID_UB
        memset(&g_ControllerStates[portIndex], 0, sizeof(g_ControllerStates[0]));
#else
        // This clears the size of the whole array at each state, which runs
        // past the end of the array for every port but the first
        memset(&g_ControllerStates[portIndex], 0, sizeof(g_ControllerStates));
#endif
    }

    InitRumblePaks();
    osUnmapTLBAll();
    InitAudio();
    MusSetStereo(TRUE);
    FreeAllCharacterSlots();
    InitTerrainTextures();
    InitExpansionPakFeatures();
    ResetLoaderThread();
    ResetBallSimulations();
    ResetCharacterSlots();
    AssignCharacterBuffers();
    InitClubs();

    f_MainLoopState = MAIN_LOOP_START;
    while (TRUE) {
        if (f_MainLoopState == MAIN_LOOP_START) {
            f_MainLoopState = MAIN_LOOP_RUNNING;
            StartGame();
            nuGfxFuncSet((NUGfxFunc)HandleGfxRetrace);
            nuPreNMIFuncSet(PrepareForReset);
        }

        while (f_MainLoopState == MAIN_LOOP_RUNNING) {
            if (g_MessageFramebuffer != NULL) {
                osViSwapBuffer(g_MessageFramebuffer);
            }

            if (f_DebugHaltState == HALT_STATE_HALTED) {
                EnterDevelopmentDebugger(100);
            }
        }

        nuGfxDisplayOff();
    }
}

// The direction that a stick axis reads: 1, -1 or 0. Past releaseThreshold
// the axis keeps the previous direction, and only past pressThreshold does it
// take a new one.
s32 GetStickDirection(s32 stick, s32 previousDirection, u32 pressThreshold, u32 releaseThreshold) {
    u32 magnitude = stick;

    if (stick < 0) {
        magnitude = -stick;
    }

    if (magnitude > releaseThreshold) {
        if (magnitude < pressThreshold) {
            stick = previousDirection;
        }

        // A jump to the shared return, to match: returning the direction here,
        // or as a ternary, or leaving early on each failed test gives 6 words
        // differing, 4 in shape, and a direction local 12, 8 in shape
        if (stick != 0) {
            goto HAS_DIRECTION;
        }
    }

    return 0;

HAS_DIRECTION:
    return (stick >> 31) | 1;
}

// Reads the menus' view of a controller: the stick with a dead zone, the
// stick's directions added to the control pad, and buttons held down repeated
// at intervals. previousMenuControllerData is unused; the caller has already
// saved the last reading there.
void UpdateMenuController(
    NUContData *controllerData,
    NUContData *previousMenuControllerData UNUSED,
    s32 playerIndex
) {
    s32 ApplyStickDeadZone(s32 stick) {
        s32 magnitude = Abs(stick);

        if (magnitude < STICK_DEAD_ZONE) {
            return 0;
        }

        if (magnitude > MENU_STICK_MAX) {
            magnitude = MENU_STICK_MAX;
        }

        return (stick < 0) ? -magnitude : magnitude;
    }

    s32 repeatDelay = g_MenuRepeatStates[playerIndex].repeatDelay;
    NUContData *menuControllerData = &g_MenuControllerData[playerIndex];

    menuControllerData->stick_x = ApplyStickDeadZone(controllerData->stick_x);
    menuControllerData->stick_y = ApplyStickDeadZone(controllerData->stick_y);
    g_MenuRepeatStates[playerIndex].stickDirectionX = GetStickDirection(
        controllerData->stick_x,
        g_MenuRepeatStates[playerIndex].stickDirectionX,
        STICK_PRESS_THRESHOLD,
        STICK_RELEASE_THRESHOLD
    );
    g_MenuRepeatStates[playerIndex].stickDirectionY = GetStickDirection(
        controllerData->stick_y,
        g_MenuRepeatStates[playerIndex].stickDirectionY,
        STICK_PRESS_THRESHOLD,
        STICK_RELEASE_THRESHOLD
    );

    menuControllerData->button = controllerData->button;
    if (g_MenuRepeatStates[playerIndex].stickDirectionX == 1) {
        menuControllerData->button |= R_JPAD;
    }
    else if (g_MenuRepeatStates[playerIndex].stickDirectionX == -1) {
        menuControllerData->button |= L_JPAD;
    }

    if (g_MenuRepeatStates[playerIndex].stickDirectionY == 1) {
        menuControllerData->button |= U_JPAD;
    }
    else if (g_MenuRepeatStates[playerIndex].stickDirectionY == -1) {
        menuControllerData->button |= D_JPAD;
    }

    menuControllerData->trigger = controllerData->trigger;

    // While the buttons stay the same, report them only when the delay runs
    // out, which is shorter after the first time
    if (menuControllerData->button == g_MenuRepeatStates[playerIndex].previousButtons) {
        g_MenuRepeatStates[playerIndex].previousButtons = menuControllerData->button;
        if (repeatDelay > BUTTON_REPEAT_DELAY_FIRST) {
            repeatDelay = BUTTON_REPEAT_DELAY_FIRST;
        }

        if (repeatDelay == 0) {
            repeatDelay = BUTTON_REPEAT_DELAY_NEXT;
        }
        else {
            repeatDelay--;
            menuControllerData->button = 0;
        }
    }
    else {
        g_MenuRepeatStates[playerIndex].previousButtons = menuControllerData->button;
        repeatDelay = BUTTON_REPEAT_DELAY_FIRST;
    }

    g_MenuRepeatStates[playerIndex].repeatDelay = repeatDelay;
}

// Reads the game's view of a controller, as UpdateMenuController does but
// with a narrower stick range and the pressed and held buttons as well
void UpdateControllerState(NUContData *controllerData, ControllerState *controller) {
    s32 repeatDelay = controller->repeatDelay;

    controller->buttons = controllerData->button;
    controller->stickX = controllerData->stick_x;
    controller->stickY = controllerData->stick_y;

    // Take the dead zone off each axis, and count a stick pushed far enough
    // as the control pad's direction
    if (controller->stickX < 0) {
        controller->stickX += STICK_DEAD_ZONE;
        if (controller->stickX > 0) {
            controller->stickX = 0;
        }
        else {
            if (controller->stickX < -STATE_STICK_MAX) {
                controller->stickX = -STATE_STICK_MAX;
            }

            if (controller->stickX < -STATE_STICK_DIRECTION_MIN) {
                controller->buttons |= L_JPAD;
            }
        }
    }
    else {
        controller->stickX -= STICK_DEAD_ZONE;
        if (controller->stickX < 0) {
            controller->stickX = 0;
        }
        else {
            if (controller->stickX > STATE_STICK_MAX) {
                controller->stickX = STATE_STICK_MAX;
            }

            if (controller->stickX > STATE_STICK_DIRECTION_MIN) {
                controller->buttons |= R_JPAD;
            }
        }
    }

    if (controller->stickY < 0) {
        controller->stickY += STICK_DEAD_ZONE;
        if (controller->stickY > 0) {
            controller->stickY = 0;
        }
        else {
            if (controller->stickY < -STATE_STICK_MAX) {
                controller->stickY = -STATE_STICK_MAX;
            }

            if (controller->stickY < -STATE_STICK_DIRECTION_MIN) {
                controller->buttons |= D_JPAD;
            }
        }
    }
    else {
        controller->stickY -= STICK_DEAD_ZONE;
        if (controller->stickY < 0) {
            controller->stickY = 0;
        }
        else {
            if (controller->stickY > STATE_STICK_MAX) {
                controller->stickY = STATE_STICK_MAX;
            }

            if (controller->stickY > STATE_STICK_DIRECTION_MIN) {
                controller->buttons |= U_JPAD;
            }
        }
    }

    // The buttons pressed since the last reading, and those held down repeated
    // as UpdateMenuController repeats them
    controller->pressedButtons =
        controllerData->button & (controller->previousButtons ^ controllerData->button);
    controller->repeatedButtons = controller->buttons;
    if (controller->buttons == controller->previousButtons) {
        if (repeatDelay > BUTTON_REPEAT_DELAY_FIRST) {
            repeatDelay = BUTTON_REPEAT_DELAY_FIRST;
        }

        if (repeatDelay == 0) {
            repeatDelay = BUTTON_REPEAT_DELAY_NEXT;
        }
        else {
            repeatDelay--;
            controller->repeatedButtons = 0;
        }
    }
    else {
        repeatDelay = BUTTON_REPEAT_DELAY_FIRST;
    }

    controller->repeatDelay = repeatDelay;
    controller->previousButtons = controller->buttons;
    controller->heldButtons = controller->buttons;
    controller->buttons = controllerData->button;
}

// Reads every connected controller, keeping the last reading of each
void ReadControllers(void) {
    s32 playerIndex = 0;
    s32 portIndex = 0;
    ControllerState *controller = g_ControllerStates;
    NUContData *previousMenuControllerData = f_PreviousMenuControllerData;
    NUContData *menuControllerData = g_MenuControllerData;
    NUContData *previousControllerData = g_PreviousControllerData;
    NUContData *controllerData = g_ControllerData;

    for (; portIndex < MAXCONTROLLERS; portIndex++) {
        if ((f_ConnectedControllerBits >> portIndex) & 1) {
            *previousControllerData = *controllerData;
            *previousMenuControllerData = *menuControllerData;
            nuContDataGetEx(controllerData, portIndex);
            UpdateMenuController(controllerData, previousMenuControllerData, playerIndex);
            UpdateControllerState(controllerData, controller);
            controller++;
            previousMenuControllerData++;
            menuControllerData++;
            previousControllerData++;
            controllerData++;
            playerIndex++;
        }
    }
}

// Runs a frame of the game on every other retrace, unless the RDP is too far
// behind. The second controller can pause the game and step it a frame at a
// time.
void HandleGfxRetrace(s32 pendingTaskCount) {
    // Once the reset button is pressed, only fade the screen out
    if (nuScPreNMIFlag & NU_SC_PRENMI_GET) {
        f_IsOddRetrace ^= TRUE;
        if (!f_IsOddRetrace) {
            DrawResetFade();
        }

        return;
    }

    f_IsOddRetrace ^= TRUE;
    if (!f_IsOddRetrace) {
        if ((g_FramePacingMode != PACING_MODE_NORMAL) ?
                pendingTaskCount < PENDING_TASK_MAX_RELAXED :
                pendingTaskCount < PENDING_TASK_MAX) {
            UpdateAudio();
            if (g_FrameStepState < FRAME_STEP_PAUSED) {
                ReadControllers();
                if (f_DebugHaltState != HALT_STATE_HALTED) {
                    UpdateGame();
                    DrawGame();
                    UpdateOverlays();
                    if (f_DebugHaltState == HALT_STATE_ALLOWED &&
                        (g_MenuControllerData[0].button & HALT_BUTTONS) == HALT_BUTTONS) {
                        f_DebugHaltState = HALT_STATE_HALTED;
                    }
                }

                g_FrameCount++;
                f_FramesThisSecond++;
                if (g_FrameStepState == FRAME_STEP_ONE_FRAME) {
                    g_FrameStepState = FRAME_STEP_PAUSED;
                }
            }
            else {
                ReadControllers();
                if (g_MenuControllerData[1].trigger & L_JPAD) {
                    g_FrameStepState = FRAME_STEP_ONE_FRAME;
                }

                if (g_FrameStepState != FRAME_STEP_ONE_FRAME &&
                    (g_MenuControllerData[1].trigger & R_JPAD)) {
                    g_FrameStepState = FRAME_STEP_RUNNING;
                }
            }
        }
        else {
            // Skip this frame and try again on the next retrace
            f_IsOddRetrace ^= TRUE;
        }
    }

    // Measure the frame rate once a second
    if (++f_RetraceCount >= RETRACES_PER_SECOND) {
        g_FramesPerSecond = f_FramesThisSecond;
        f_RetraceCount = 0;
        f_FramesThisSecond = 0;
    }
}
