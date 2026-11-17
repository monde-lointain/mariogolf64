/*
 * Tests of the port's NuSystem: the ring of frame buffers that the game
 * switches between two and three of, the pacing of the frames shown, the
 * screen coming on, the save memory and the controllers.
 *
 * The test's main thread plays the hardware: it raises the retraces, as
 * the platform layer does, and the game's main thread runs the checks.
 */

#include <SDL3/SDL.h>
#include <framebuffer_count.h>
#include <nusys.h>

#include "port/nusys_host.h"
#include "port/os_host.h"
#include "test_support.h"

#define FRAMEBUFFER_SIZE (SCREEN_WIDTH_FOR_TEST * SCREEN_HEIGHT_FOR_TEST)
#define SCREEN_WIDTH_FOR_TEST 320
#define SCREEN_HEIGHT_FOR_TEST 240
#define FAKE_ROM_SIZE 0x1000

// The depth buffer that NuSystem moves, which game.h declares for the game
extern u16 *g_DepthBuffer;

static u16 f_Framebuffers[3][FRAMEBUFFER_SIZE];
static u16 *f_FramebufferList[3] = {f_Framebuffers[0], f_Framebuffers[1], f_Framebuffers[2]};
static SDL_Semaphore *f_TestDone;
static SDL_AtomicInt f_RetracesWanted;
static int f_GfxTaskCount;
static void *f_DrawnFramebuffer;
static int f_ShownDrawCount;
static u8 f_StoredSram[PC_SRAM_SIZE];
static int f_StoreCount;
static OSContPad f_Pads[4];
static int f_MotorOn[4];
static u8 f_Rom[FAKE_ROM_SIZE];

// Link seams: what the platform provides
void PcRunGfxTask(Gfx *displayList, u32 size) {
    (void)displayList;
    (void)size;
    f_GfxTaskCount++;
    if (osViGetCurrentFramebuffer() == f_DrawnFramebuffer ||
        osViGetNextFramebuffer() == f_DrawnFramebuffer) {
        f_ShownDrawCount++;
    }
}

u8 PcGetConnectedControllers(void) {
    return 0x05;
}

void PcReadControllerPad(int port, OSContPad *pad) {
    *pad = f_Pads[port];
}

int PcHasRumble(int port) {
    return port == 0;
}

void PcSetRumble(int port, int isOn) {
    f_MotorOn[port] = isOn;
}

void PcLoadSaveMemory(u8 *image, u32 size) {
    SDL_memcpy(image, f_StoredSram, size);
}

void PcStoreSaveMemory(const u8 *image, u32 size) {
    SDL_memcpy(f_StoredSram, image, size);
    f_StoreCount++;
}

// Asks the hardware for retraces and waits until the scheduler has counted
// them. The test runs below every NuSystem thread, so when it goes on, the
// threads that the retraces woke have done with them.
static void WaitForRetraces(int count) {
    u32 target = nuScRetraceCounter + (u32)count;

    SDL_AddAtomicInt(&f_RetracesWanted, count);
    while (nuScRetraceCounter < target) {
        osPcWaitForInterrupt();
    }
}

// nuGfxTaskAllEndWait, with the retraces that retiring the tasks takes
static void RetireAllTasks(void) {
    while (nuGfxTaskSpool != 0) {
        WaitForRetraces(1);
    }

    nuGfxTaskAllEndWait();
}

static int FramebufferIndex(void *framebuffer) {
    int i;

    for (i = 0; i < 3; i++) {
        if (framebuffer == f_Framebuffers[i]) {
            return i;
        }
    }

    return -1;
}

static void TestFramebufferRing(void) {
    nuGfxSetCfb(f_FramebufferList, TRIPLE_BUFFERING);
    EXPECT(nuGfxCfb_ptr == f_Framebuffers[0]);
    EXPECT(g_DepthBuffer == (u16 *)PcRdramAddress(0x802EA000));

    // Two buffers: the one about to be drawn into holds the depth, and
    // the other two take turns
    SetFramebufferCount(DOUBLE_BUFFERING);
    EXPECT(g_DepthBuffer == f_Framebuffers[0]);
    EXPECT(nuGfxCfbCounter == 1);
    nuGfxTaskStart(NULL, 0, NU_GFX_UCODE_F3DEX, NU_SC_SWAPBUFFER);
    EXPECT(nuGfxCfbCounter == 2);
    nuGfxTaskStart(NULL, 0, NU_GFX_UCODE_F3DEX, NU_SC_SWAPBUFFER);
    EXPECT(nuGfxCfbCounter == 1);

    // Three again: the spare is drawn into next, then the ring goes on
    // in the order that the other two had
    SetFramebufferCount(TRIPLE_BUFFERING);
    EXPECT(nuGfxCfbCounter == 0);
    EXPECT(g_DepthBuffer == (u16 *)PcRdramAddress(0x802EA000));
    nuGfxTaskStart(NULL, 0, NU_GFX_UCODE_F3DEX, NU_SC_SWAPBUFFER);
    EXPECT(nuGfxCfbCounter == 1);
    nuGfxTaskStart(NULL, 0, NU_GFX_UCODE_F3DEX, NU_SC_SWAPBUFFER);
    EXPECT(nuGfxCfbCounter == 2);
    nuGfxTaskStart(NULL, 0, NU_GFX_UCODE_F3DEX, NU_SC_SWAPBUFFER);
    EXPECT(nuGfxCfbCounter == 0);
    EXPECT(f_GfxTaskCount == 5);
    RetireAllTasks();
}

// Frames finished together are shown two retraces apart, in order, and
// the screen comes on with the first
static void TestSwapPacing(void) {
    RetireAllTasks();
    WaitForRetraces(2);
    nuGfxSetCfb(f_FramebufferList, TRIPLE_BUFFERING);
    nuGfxDisplayOff();
    nuGfxDisplayOn();
    EXPECT(nuGfxDisplay == NU_GFX_DISPLAY_ON_TRIGGER);
    nuGfxTaskStart(NULL, 0, NU_GFX_UCODE_F3DEX, NU_SC_SWAPBUFFER);
    nuGfxTaskStart(NULL, 0, NU_GFX_UCODE_F3DEX, NU_SC_SWAPBUFFER);
    // The first goes out at once, since retraces have passed since the last
    EXPECT(FramebufferIndex(osViGetNextFramebuffer()) == 0);
    EXPECT(nuGfxDisplay == NU_GFX_DISPLAY_ON);
    EXPECT(nuGfxTaskSpool == 1);
    WaitForRetraces(1);
    EXPECT(FramebufferIndex(osViGetNextFramebuffer()) == 0);
    WaitForRetraces(1);
    EXPECT(FramebufferIndex(osViGetNextFramebuffer()) == 1);
    EXPECT(nuGfxTaskSpool == 0);
}

static void TestSaveMemory(void) {
    u8 written[16];
    u8 read[16];
    int i;

    for (i = 0; i < 16; i++) {
        written[i] = (u8)(i * 7);
        f_StoredSram[0x4000 + i] = 0xEE;
    }

    nuPiInitSram();
    nuPiReadSram(PI_DOM2_ADDR2 + 0x4000, read, sizeof(read));
    EXPECT(read[0] == 0xEE && read[15] == 0xEE);
    nuPiWriteSram(PI_DOM2_ADDR2 + 0x10, written, sizeof(written));
    EXPECT(f_StoreCount == 1);
    EXPECT(f_StoredSram[0x10 + 15] == written[15]);
    nuPiReadSram(PI_DOM2_ADDR2 + 0x10, read, sizeof(read));
    EXPECT(SDL_memcmp(read, written, sizeof(read)) == 0);
}

static void TestRomReads(void) {
    u8 read[4];

    nuPiReadRom(0x100, read, sizeof(read));
    EXPECT(read[0] == f_Rom[0x100] && read[3] == f_Rom[0x103]);
}

static void TestControllers(void) {
    NUContData data;

    EXPECT(nuContInit() == 0x05);
    f_Pads[0].button = A_BUTTON;
    f_Pads[0].stick_x = 40;
    nuContDataGetEx(&data, 0);
    EXPECT(data.button == A_BUTTON && data.trigger == A_BUTTON && data.stick_x == 40);
    f_Pads[0].button = A_BUTTON | B_BUTTON;
    nuContDataGetEx(&data, 0);
    EXPECT(data.trigger == B_BUTTON);

    EXPECT(nuContRmbCheck(0) == 0);
    EXPECT(nuContRmbCheck(1) == PFS_ERR_NOPACK);
    EXPECT(nuContRmbCheck(2) == PFS_ERR_NOPACK);
    nuContRmbModeSet(0, NU_CONT_RMB_MODE_AUTORUN);
    nuContRmbStart(0, 256, 2);
    WaitForRetraces(1);
    EXPECT(f_MotorOn[0]);
    WaitForRetraces(2);
    EXPECT(!f_MotorOn[0]);

    // Held off until the game lets it go again
    nuContRmbStart(0, 256, 10);
    nuContRmbForceStop();
    WaitForRetraces(1);
    EXPECT(!f_MotorOn[0]);
    nuContRmbStart(0, 256, 10);
    EXPECT(!f_MotorOn[0]);
    nuSiSendMesg(NU_CONT_RMB_FORCESTOPEND_MSG, NULL);
    nuContRmbStart(0, 256, 10);
    WaitForRetraces(1);
    EXPECT(f_MotorOn[0]);
}

// Starts a task that draws into the frame buffer next in the ring
static void StartFrame(void) {
    f_DrawnFramebuffer = nuGfxCfb_ptr;
    nuGfxTaskStart(NULL, 0, NU_GFX_UCODE_F3DEX, NU_SC_SWAPBUFFER);
}

// A task whose frame buffer the screen shows, or is to show, draws only
// once the retraces have taken it off
static void TestDrawingWaitsForHiddenFramebuffer(void) {
    RetireAllTasks();
    WaitForRetraces(2);
    nuGfxSetCfb(f_FramebufferList, TRIPLE_BUFFERING);
    f_ShownDrawCount = 0;
    StartFrame();
    WaitForRetraces(1);
    EXPECT(FramebufferIndex(osViGetCurrentFramebuffer()) == 0);
    StartFrame();
    StartFrame();
    // The fourth frame's buffer is the first's, which the screen shows, so
    // the task needs the retraces that show the second
    SDL_AddAtomicInt(&f_RetracesWanted, 4);
    StartFrame();
    EXPECT(f_ShownDrawCount == 0);
    EXPECT(FramebufferIndex(osViGetCurrentFramebuffer()) == 1);
    while (SDL_GetAtomicInt(&f_RetracesWanted) > 0) {
        osPcWaitForInterrupt();
    }

    RetireAllTasks();
}

void mainproc(void *argument) {
    (void)argument;
    nuGfxInitEX2();
    TestFramebufferRing();
    TestSwapPacing();
    TestDrawingWaitsForHiddenFramebuffer();
    TestSaveMemory();
    TestRomReads();
    TestControllers();
    SDL_SignalSemaphore(f_TestDone);
}

int main(int argc, char **argv) {
    int i;

    (void)argc;
    (void)argv;
    for (i = 0; i < FAKE_ROM_SIZE; i++) {
        f_Rom[i] = (u8)(i ^ 0x5A);
    }

    f_TestDone = SDL_CreateSemaphore(0);
    PcInitOs();
    PcInsertCartridge(f_Rom, sizeof(f_Rom));
    PcBootNuSystem();
    // Raises only the retraces that the test asks for, so that it knows
    // how many have passed. A software interrupt now and then wakes a wait
    // that began after the retrace that it waited for.
    while (!SDL_WaitSemaphoreTimeout(f_TestDone, 1)) {
        if (SDL_GetAtomicInt(&f_RetracesWanted) > 0) {
            SDL_AddAtomicInt(&f_RetracesWanted, -1);
            PcRaiseRetrace();
        }
        else {
            PcRaiseEvent(OS_EVENT_SW1);
        }
    }

    return ReportTestResults("nusys_test");
}
