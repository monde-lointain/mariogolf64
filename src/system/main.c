#include "system/main.h"

#include <PR/libmus.h>
#include <string.h>
#include <ultra64.h>

#include "audio/sound.h"
#include "ball_simulation.h"
#include "character.h"
#include "character_model.h"
#include "club.h"
#include "controllers/controller.h"
#include "controllers/transfer_pak.h"
#include "debug_text.h"
#include "game.h"
#include "game_flags.h"
#include "hole_loader.h"
#include "math/math_util.h"
#include "play.h"
#include "save.h"
#include "scene.h"
#include "screen/screen.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/crash_screen.h"
#include "system/heap.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"
#include "system/segment_symbols.h"
#include "terrain.h"
#include "work_area.h"

#define AUDIO_FREQUENCY 32000

#define FRAMEBUFFER_COUNT 3

#define FRAMEBUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(u16))

#define FramebufferAddress(index) \
    ((u16 *)(BASE_RDRAM_END - ((FRAMEBUFFER_COUNT - (index)) * FRAMEBUFFER_SIZE)))

enum CharacterSlotRole {
    CHARACTER_SLOT_PARTNER = 1,
    CHARACTER_SLOT_CADDIE,
    CHARACTER_SLOT_GOLFER
};

#define HEARTBEAT_LOOP_COUNT 100000000

enum MainLoopState {
    MAIN_LOOP_START,
    MAIN_LOOP_RUNNING = -1
};

typedef s32 MainLoopState;

enum HaltState {
    HALT_STATE_DISALLOWED,
    HALT_STATE_ALLOWED,
    HALT_STATE_HALTED
};

typedef u8 HaltState;

#define HALT_BUTTONS (A_BUTTON | B_BUTTON | L_TRIG | R_TRIG)

#define PENDING_TASK_MAX 2
#define PENDING_TASK_MAX_RELAXED 4

#define RETRACES_PER_SECOND 60

#define STICK_PRESS_THRESHOLD 56
#define STICK_RELEASE_THRESHOLD 20

#define STICK_DEAD_ZONE 16

#define MENU_STICK_MAX 64

#define STATE_STICK_MAX 44

#define STATE_STICK_DIRECTION_MIN 30

Boolean8 g_IsDebugEnabled = FALSE;
Boolean g_HasExpansionPak = FALSE;
static volatile HaltState f_DebugHaltState = HALT_STATE_DISALLOWED;
Boolean g_HasThreeFrameBuffers = FALSE;
s32 g_FramesPerSecond = 0;

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

FILE_STATIC volatile MainLoopState g_MainLoopState;
FILE_STATIC u8 f_GameFlagBits[GAME_FLAG_BYTE_COUNT];
#define GameFlagByte(flag) (f_GameFlagBits[(flag) / GAME_FLAGS_PER_BYTE])
#define GameFlagMask(flag) (0x80 >> ((flag) % GAME_FLAGS_PER_BYTE))
#define GameFlagClearMask(flag) (0xFF7F >> ((flag) % GAME_FLAGS_PER_BYTE))
FILE_STATIC NUContData g_PreviousMenuControllerData[MAXCONTROLLERS];
FILE_STATIC u8 g_ConnectedControllerBits;

void ReportFailedAssertion(const char *expression UNUSED, const char *fileName, s32 lineNumber) {
    osSyncPrintf("Assertion Fault : File %s Line %d\n", fileName, lineNumber);
}

void guNormalize(f32 *x, f32 *y, f32 *z) {
    f32 xValue = *x;
    f32 yValue = *y;
    f32 zValue = *z;

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

void RunHeartbeatLoop(void) {
    s32 iteration;

    while (TRUE) {
        for (iteration = 0; iteration != HEARTBEAT_LOOP_COUNT; iteration++) {
        }

        osSyncPrintf("thread %d\n", HEARTBEAT_LOOP_COUNT);
    }
}

void AssignCharacterBuffers(void) {
    s32 bufferSize = GetCharacterBufferSize();

    SetCharacterBuffer(CHARACTER_SLOT_PARTNER, g_WorkArea.bytes);
    SetCharacterBuffer(CHARACTER_SLOT_GOLFER, g_WorkArea.bytes + bufferSize);
    SetCharacterBuffer(CHARACTER_SLOT_CADDIE, g_WorkArea.bytes + (bufferSize * 2));
    osSyncPrintf("GMemLeft=%x\n", WORK_AREA_SIZE - (bufferSize * 3));
}

void PrepareForReset(void) {
    StopAllRumble();
    __libmus_output_muted = TRUE;
}

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

void mainproc(void *arg UNUSED) {
    u32 buildDateSize;
    s32 portIndex;

    osAiSetFrequency(AUDIO_FREQUENCY);
    bzero(main_BSS_END, BytesBetween(main_BSS_END, BASE_RDRAM_END));
    osWritebackDCacheAll();
    DisableFloatExceptions();
    RunBootHook();
    osSyncPrintf("MainEntry\n");

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
    ResetHeap(1);
    InitAssetFiles();
    ClearAllGameFlags();
    InitOverlayManager();
    LoadOverlay(GetOverlayOfModule(MODULE_BOOT_LOGOS));
    LoadOverlay(GetOverlayOfModule(MODULE_HOLE_BUILDER));
    osSyncPrintf("S-InitSRAM\n");
    InitSaveData();
    osSyncPrintf("E-InitSRAM\n");
    InitFramebuffers();

    g_ConnectedControllerBits = nuContInit();
    g_ControllerCount = 0;
    for (portIndex = 0; portIndex < MAXCONTROLLERS; portIndex++) {
        if ((g_ConnectedControllerBits >> portIndex) & 1) {
            g_PlayerControllerPorts[g_ControllerCount++] = portIndex;
        }

#ifdef AVOID_UB
        memset(&g_ControllerStates[portIndex], 0, sizeof(g_ControllerStates[0]));
#else
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

    g_MainLoopState = MAIN_LOOP_START;
    while (TRUE) {
        if (g_MainLoopState == MAIN_LOOP_START) {
            g_MainLoopState = MAIN_LOOP_RUNNING;
            StartGame();
            nuGfxFuncSet((NUGfxFunc)HandleGfxRetrace);
            nuPreNMIFuncSet(PrepareForReset);
        }
        while (g_MainLoopState == MAIN_LOOP_RUNNING) {
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

s32 GetStickDirection(s32 stick, s32 previousDirection, u32 pressThreshold, u32 releaseThreshold) {
    u32 magnitude = stick;

    if (stick < 0) {
        magnitude = -stick;
    }

    if (magnitude > releaseThreshold) {
        if (magnitude < pressThreshold) {
            stick = previousDirection;
        }

        if (stick != 0) {
            goto HAS_DIRECTION;
        }
    }

    return 0;

HAS_DIRECTION:

    return (stick >> 31) | 1;
}

void UpdateMenuController(
    NUContData *controllerData,
    NUContData *previousMenuControllerData UNUSED,
    s32 portIndex
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

    s32 repeatDelay = g_MenuRepeatStates[portIndex].repeatDelay;
    NUContData *menuControllerData = &g_MenuControllerData[portIndex];

    menuControllerData->stick_x = ApplyStickDeadZone(controllerData->stick_x);
    menuControllerData->stick_y = ApplyStickDeadZone(controllerData->stick_y);
    g_MenuRepeatStates[portIndex].stickDirectionX = GetStickDirection(
        controllerData->stick_x,
        g_MenuRepeatStates[portIndex].stickDirectionX,
        STICK_PRESS_THRESHOLD,
        STICK_RELEASE_THRESHOLD
    );
    g_MenuRepeatStates[portIndex].stickDirectionY = GetStickDirection(
        controllerData->stick_y,
        g_MenuRepeatStates[portIndex].stickDirectionY,
        STICK_PRESS_THRESHOLD,
        STICK_RELEASE_THRESHOLD
    );

    menuControllerData->button = controllerData->button;
    if (g_MenuRepeatStates[portIndex].stickDirectionX == 1) {
        menuControllerData->button |= R_JPAD;
    }
    else if (g_MenuRepeatStates[portIndex].stickDirectionX == -1) {
        menuControllerData->button |= L_JPAD;
    }

    if (g_MenuRepeatStates[portIndex].stickDirectionY == 1) {
        menuControllerData->button |= U_JPAD;
    }
    else if (g_MenuRepeatStates[portIndex].stickDirectionY == -1) {
        menuControllerData->button |= D_JPAD;
    }

    menuControllerData->trigger = controllerData->trigger;

    if (menuControllerData->button == g_MenuRepeatStates[portIndex].previousButtons) {
        g_MenuRepeatStates[portIndex].previousButtons = menuControllerData->button;
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
        g_MenuRepeatStates[portIndex].previousButtons = menuControllerData->button;
        repeatDelay = BUTTON_REPEAT_DELAY_FIRST;
    }

    g_MenuRepeatStates[portIndex].repeatDelay = repeatDelay;
}

void UpdateControllerState(NUContData *controllerData, ControllerState *controller) {
    s32 repeatDelay = controller->repeatDelay;

    controller->buttons = controllerData->button;
    controller->stickX = controllerData->stick_x;
    controller->stickY = controllerData->stick_y;

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

void ReadControllers(void) {
    s32 playerIndex = 0;
    s32 portIndex = 0;
    ControllerState *controller = g_ControllerStates;
    NUContData *previousMenuControllerData = g_PreviousMenuControllerData;
    NUContData *menuControllerData = g_MenuControllerData;
    NUContData *previousControllerData = g_PreviousControllerData;
    NUContData *controllerData = g_ControllerData;

    for (; portIndex < MAXCONTROLLERS; portIndex++) {
        if ((g_ConnectedControllerBits >> portIndex) & 1) {
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

void HandleGfxRetrace(s32 pendingTaskCount) {
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
            f_IsOddRetrace ^= TRUE;
        }
    }

    if (++f_RetraceCount >= RETRACES_PER_SECOND) {
        g_FramesPerSecond = f_FramesThisSecond;
        f_RetraceCount = 0;
        f_FramesThisSecond = 0;
    }
}
