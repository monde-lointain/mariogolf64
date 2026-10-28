/*
 * The game's entry, mainproc, which sets up the framebuffers and runs the
 * heartbeat loop. The file also keeps the game flags, reads the controllers and
 * prepares for a reset.
 */
#include "common.h"

#include "main.h"

#include <PR/libmus.h>

#include "ball_simulation.h"
#include "character_slot.h"
#include "club.h"
#include "controller.h"
#include "debug_text.h"
#include "functions.h"
#include "game.h"
#include "game_flags.h"
#include "heap.h"
#include "hole_loader.h"
#include "libc.h"
#include "overlay_manager.h"
#include "save.h"
#include "screen.h"
#include "segment_symbols.h"
#include "sound.h"
#include "variables.h"
#include "work_area.h"

/* The audio interface's sample rate, in Hz */
#define AUDIO_FREQUENCY 32000

/* End of the RDRAM that every console has, without the Expansion Pak */
#define BASE_RDRAM_END 0x80400000

/* Frame buffers that the game swaps between */
#define FRAMEBUFFER_COUNT 3

/* The bytes of one frame buffer's 16-bit texels */
#define FRAMEBUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(u16))

/* The frame buffers fill the end of the base RDRAM, in order */
#define FRAMEBUFFER_ADDRESS(index) \
  ((u16*)(BASE_RDRAM_END - ((FRAMEBUFFER_COUNT - (index)) * FRAMEBUFFER_SIZE)))

/* The longest build date that fits g_RomBuildDate with its terminator */
#define BUILD_DATE_LENGTH_MAX 63

/* Character slots that get a buffer in g_WorkArea */
enum CharacterSlotRole {
  CHARACTER_SLOT_PARTNER = 1,
  CHARACTER_SLOT_CADDIE,
  CHARACTER_SLOT_GOLFER
};

/* Iterations of the busy loop between two reports of RunHeartbeatLoop */
#define HEARTBEAT_LOOP_COUNT 100000000

/* Values of g_MainLoopState */
#define MAIN_LOOP_START 0
#define MAIN_LOOP_RUNNING (-1)

/* Values of f_DebugHaltState. Holding HALT_BUTTONS on the first controller
   while halting is allowed halts the game and hands the console over to
   the development debugger. */
enum HaltState {
  HALT_STATE_DISALLOWED,
  HALT_STATE_ALLOWED,
  HALT_STATE_HALTED
};

#define HALT_BUTTONS (A_BUTTON | B_BUTTON | L_TRIG | R_TRIG)

/* Graphics tasks that may wait for the RDP before a frame is skipped,
   depending on g_FramePacingMode */
#define PENDING_TASK_MAX 2
#define PENDING_TASK_MAX_RELAXED 4

/* Retraces between two updates of g_FramesPerSecond */
#define RETRACES_PER_SECOND 60

/* The stick reads as a direction once it is pushed past
   STICK_PRESS_THRESHOLD, and keeps doing so until it falls back below
   STICK_RELEASE_THRESHOLD */
#define STICK_PRESS_THRESHOLD 56
#define STICK_RELEASE_THRESHOLD 20

/* Stick readings this close to the centre count as 0 */
#define STICK_DEAD_ZONE 16

/* The largest stick magnitude in menu readings */
#define MENU_STICK_MAX 64

/* The largest stick magnitude in a ControllerState, after the dead zone */
#define STATE_STICK_MAX 44

/* A ControllerState counts the stick as pressing a direction past this */
#define STATE_STICK_DIRECTION_MIN 30

s8 g_IsDebugEnabled = FALSE;
s32 g_HasExpansionPak = FALSE;
static volatile u8 f_DebugHaltState = HALT_STATE_DISALLOWED;
s32 g_HasThreeFrameBuffers = FALSE;
s32 g_FramesPerSecond = 0;

/* Read by no routine */
static u8 D_800B67D4[4] = {0};

static u16* f_Framebuffers[FRAMEBUFFER_COUNT] = {
    FRAMEBUFFER_ADDRESS(0),
    FRAMEBUFFER_ADDRESS(1),
    FRAMEBUFFER_ADDRESS(2),
};
static s32 f_RetraceCount = 0;
static s32 f_FramesThisSecond = 0;
s8 g_FrameStepState = FRAME_STEP_RUNNING;
volatile s32 g_FrameCount = 0;
static s32 f_IsOddRetrace = FALSE;
extern char g_RomBuildDate[BUILD_DATE_LENGTH_MAX + 1];
extern char g_SaveBuildDate[];

void HandleGfxRetrace(s32 pendingTaskCount);

/* Not static: the original left these to the linker's common block */
volatile s32 g_MainLoopState;
u8 f_GameFlagBits[GAME_FLAG_COUNT / 8];

void ReportFailedAssertion(
    const char* expression, const char* fileName, s32 lineNumber) {
  osSyncPrintf("Assertion Fault : File %s Line %d\n", fileName, lineNumber);
}

void guNormalize(f32* x, f32* y, f32* z) {
  f32 xValue = *x;
  f32 yValue = *y;
  f32 zValue = *z;

  /* The squared length, and then the inverse of the length */
  f32 length = (xValue * xValue) + (yValue * yValue) + (zValue * zValue);

  if (length < 0.0f) {
    osSyncPrintf("NormalizeError\n");
    *x = 0.0f;
    *y = 1.0f;
    *z = 0.0f;
  } else {
    length = 1.0f / sqrtf(length);
    *x = xValue * length;
    *y = yValue * length;
    *z = zValue * length;
  }
}

void InitFramebuffers(void) {
  nuGfxSetCfb(f_Framebuffers, FRAMEBUFFER_COUNT);
}

s32 IsGameFlagSet(u32 flag) {
  if (flag < GAME_FLAG_COUNT) {
    if (f_GameFlagBits[flag >> 3] & (0x80 >> (flag & 7))) {
      return TRUE;
    }

    return FALSE;
  }

  return FALSE;
}

void SetGameFlag(u32 flag) {
  if (flag < GAME_FLAG_COUNT) {
    f_GameFlagBits[flag >> 3] |= 0x80 >> (flag & 7);
  }
}

void ClearGameFlag(u32 flag) {
  if (flag < GAME_FLAG_COUNT) {
    f_GameFlagBits[flag >> 3] &= 0xFF7F >> (flag & 7);
  }
}

void ToggleGameFlag(u32 flag) {
  if (flag < GAME_FLAG_COUNT) {
    f_GameFlagBits[flag >> 3] ^= 0x80 >> (flag & 7);
  }
}

void ClearAllGameFlags(void) {
  s32 byteIndex;

  for (byteIndex = 0; byteIndex < GAME_FLAG_COUNT / 8; byteIndex++) {
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
  SetCharacterBuffer(
      CHARACTER_SLOT_CADDIE, g_WorkArea.bytes + (bufferSize * 2));
  osSyncPrintf("GMemLeft=%x\n", WORK_AREA_SIZE - (bufferSize * 3));
}

void PrepareForReset(void) {
  StopAllRumble();
  __libmus_output_muted = TRUE;
}

/* Clears the exception enables of the floating-point control and status
   register, so that float errors give IEEE results instead of trapping.
   The first mask clears bits 8 to 12 and 28 to 31, the second the enables
   in bits 7 to 11, and nothing is set again. */
void DisableFloatExceptions(void) {
  __asm__ volatile(
      "cfc1 $4, $31\n"
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

void mainproc(void* arg) {
  u32 buildDateSize;
  s32 portIndex;

  osAiSetFrequency(AUDIO_FREQUENCY);
  bzero(main_BSS_END, (u8*)BASE_RDRAM_END - main_BSS_END);
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

    /* This clears the size of the whole array at each state, which
       runs past the end of the array for every port but the first */
    memset(&g_ControllerStates[portIndex], 0, sizeof(g_ControllerStates));
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

s32 GetStickDirection(s32 stick, s32 previousDirection, u32 pressThreshold,
    u32 releaseThreshold) {
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

/* PREVIOUSMENUPAD is unused; the caller has already saved the last reading
 * there */
void UpdateMenuController(
    NUContData* pad, NUContData* previousMenuPad, s32 portIndex) {
  s32 ApplyStickDeadZone(s32 stick) {
    s32 magnitude = ABS(stick);

    if (magnitude < STICK_DEAD_ZONE) {
      return 0;
    }

    if (magnitude > MENU_STICK_MAX) {
      magnitude = MENU_STICK_MAX;
    }

    return (stick < 0) ? -magnitude : magnitude;
  }

  s32 repeatDelay = g_MenuRepeatStates[portIndex].repeatDelay;
  NUContData* menuPad = &g_MenuControllerData[portIndex];

  menuPad->stick_x = ApplyStickDeadZone(pad->stick_x);
  menuPad->stick_y = ApplyStickDeadZone(pad->stick_y);
  g_MenuRepeatStates[portIndex].stickDirectionX = GetStickDirection(
      pad->stick_x, g_MenuRepeatStates[portIndex].stickDirectionX,
      STICK_PRESS_THRESHOLD, STICK_RELEASE_THRESHOLD);
  g_MenuRepeatStates[portIndex].stickDirectionY = GetStickDirection(
      pad->stick_y, g_MenuRepeatStates[portIndex].stickDirectionY,
      STICK_PRESS_THRESHOLD, STICK_RELEASE_THRESHOLD);

  menuPad->button = pad->button;
  if (g_MenuRepeatStates[portIndex].stickDirectionX == 1) {
    menuPad->button |= R_JPAD;
  } else if (g_MenuRepeatStates[portIndex].stickDirectionX == -1) {
    menuPad->button |= L_JPAD;
  }

  if (g_MenuRepeatStates[portIndex].stickDirectionY == 1) {
    menuPad->button |= U_JPAD;
  } else if (g_MenuRepeatStates[portIndex].stickDirectionY == -1) {
    menuPad->button |= D_JPAD;
  }

  menuPad->trigger = pad->trigger;

  if (menuPad->button == g_MenuRepeatStates[portIndex].previousButtons) {
    g_MenuRepeatStates[portIndex].previousButtons = menuPad->button;
    if (repeatDelay > BUTTON_REPEAT_DELAY_FIRST) {
      repeatDelay = BUTTON_REPEAT_DELAY_FIRST;
    }

    if (repeatDelay == 0) {
      repeatDelay = BUTTON_REPEAT_DELAY_NEXT;
    } else {
      repeatDelay--;
      menuPad->button = 0;
    }
  } else {
    g_MenuRepeatStates[portIndex].previousButtons = menuPad->button;
    repeatDelay = BUTTON_REPEAT_DELAY_FIRST;
  }

  g_MenuRepeatStates[portIndex].repeatDelay = repeatDelay;
}

void UpdateControllerState(NUContData* pad, ControllerState* state) {
  s32 repeatDelay = state->repeatDelay;

  state->buttons = pad->button;
  state->stickX = pad->stick_x;
  state->stickY = pad->stick_y;

  if (state->stickX < 0) {
    state->stickX += STICK_DEAD_ZONE;
    if (state->stickX > 0) {
      state->stickX = 0;
    } else {
      if (state->stickX < -STATE_STICK_MAX) {
        state->stickX = -STATE_STICK_MAX;
      }

      if (state->stickX < -STATE_STICK_DIRECTION_MIN) {
        state->buttons |= L_JPAD;
      }
    }
  } else {
    state->stickX -= STICK_DEAD_ZONE;
    if (state->stickX < 0) {
      state->stickX = 0;
    } else {
      if (state->stickX > STATE_STICK_MAX) {
        state->stickX = STATE_STICK_MAX;
      }

      if (state->stickX > STATE_STICK_DIRECTION_MIN) {
        state->buttons |= R_JPAD;
      }
    }
  }

  if (state->stickY < 0) {
    state->stickY += STICK_DEAD_ZONE;
    if (state->stickY > 0) {
      state->stickY = 0;
    } else {
      if (state->stickY < -STATE_STICK_MAX) {
        state->stickY = -STATE_STICK_MAX;
      }

      if (state->stickY < -STATE_STICK_DIRECTION_MIN) {
        state->buttons |= D_JPAD;
      }
    }
  } else {
    state->stickY -= STICK_DEAD_ZONE;
    if (state->stickY < 0) {
      state->stickY = 0;
    } else {
      if (state->stickY > STATE_STICK_MAX) {
        state->stickY = STATE_STICK_MAX;
      }

      if (state->stickY > STATE_STICK_DIRECTION_MIN) {
        state->buttons |= U_JPAD;
      }
    }
  }

  state->pressedButtons = pad->button & (state->previousButtons ^ pad->button);
  state->repeatedButtons = state->buttons;
  if (state->buttons == state->previousButtons) {
    if (repeatDelay > BUTTON_REPEAT_DELAY_FIRST) {
      repeatDelay = BUTTON_REPEAT_DELAY_FIRST;
    }

    if (repeatDelay == 0) {
      repeatDelay = BUTTON_REPEAT_DELAY_NEXT;
    } else {
      repeatDelay--;
      state->repeatedButtons = 0;
    }
  } else {
    repeatDelay = BUTTON_REPEAT_DELAY_FIRST;
  }

  state->repeatDelay = repeatDelay;
  state->previousButtons = state->buttons;
  state->heldButtons = state->buttons;
  state->buttons = pad->button;
}

void ReadControllers(void) {
  s32 playerIndex = 0;
  s32 portIndex = 0;
  ControllerState* state = g_ControllerStates;
  NUContData* previousMenuPad = g_PreviousMenuControllerData;
  NUContData* menuPad = g_MenuControllerData;
  NUContData* previousPad = g_PreviousControllerData;
  NUContData* pad = g_ControllerData;

  for (; portIndex < MAXCONTROLLERS; portIndex++) {
    if ((g_ConnectedControllerBits >> portIndex) & 1) {
      *previousPad = *pad;
      *previousMenuPad = *menuPad;
      nuContDataGetEx(pad, portIndex);
      UpdateMenuController(pad, previousMenuPad, playerIndex);
      UpdateControllerState(pad, state);
      state++;
      previousMenuPad++;
      menuPad++;
      previousPad++;
      pad++;
      playerIndex++;
    }
  }
}

void HandleGfxRetrace(s32 pendingTaskCount) {
  if (nuScPreNMIFlag & NU_SC_PRENMI_GET) {
    f_IsOddRetrace ^= 1;
    if (!f_IsOddRetrace) {
      DrawResetFade();
    }

    return;
  }

  f_IsOddRetrace ^= 1;
  if (!f_IsOddRetrace) {
    if ((g_FramePacingMode != PACING_MODE_NORMAL)
            ? pendingTaskCount < PENDING_TASK_MAX_RELAXED
            : pendingTaskCount < PENDING_TASK_MAX) {
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
      } else {
        ReadControllers();
        if (g_MenuControllerData[1].trigger & L_JPAD) {
          g_FrameStepState = FRAME_STEP_ONE_FRAME;
        }

        if (g_FrameStepState != FRAME_STEP_ONE_FRAME &&
            (g_MenuControllerData[1].trigger & R_JPAD)) {
          g_FrameStepState = FRAME_STEP_RUNNING;
        }
      }
    } else {
      /* Skip this frame and try again on the next retrace */
      f_IsOddRetrace ^= 1;
    }
  }

  if (++f_RetraceCount >= RETRACES_PER_SECOND) {
    g_FramesPerSecond = f_FramesThisSecond;
    f_RetraceCount = 0;
    f_FramesThisSecond = 0;
  }
}
