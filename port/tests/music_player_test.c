/*
 * Tests of the music player's API and thread: the frames it plays for each
 * retrace, its pacing against the device, its heap, its effects and its
 * handles, with banks written for the test and an audio output that keeps
 * what it is given.
 *
 * The game's thread, which runs the checks, plays the scheduler: it sends
 * the player each retrace and waits for the audio task that ends the frame.
 */

#include <PR/libmus.h>
#include <SDL3/SDL.h>
#include <string.h>
#include <ultra64.h>

#include "audio_output.h"
#include "music_player.h"
#include "port/os_host.h"
#include "reverb.h"
#include "test_support.h"

#define SAMPLE_RATE 32000
#define TICK_FRAMES (SAMPLE_RATE / 60)
#define STEREO 2
#define WAVE_OFFSET 0x100
#define WAVE_SAMPLES 64
#define ROM_SIZE 0x400
#define HEAP_SIZE 0x1000
#define BANK_SIZE 256
#define TEST_PRIORITY 10
#define PLAYER_PRIORITY 70

static u8 f_Rom[ROM_SIZE];
static u8 f_Heap[HEAP_SIZE];
static u8 f_PointerBank[BANK_SIZE];
static u8 f_EffectBank[BANK_SIZE];
static SDL_Semaphore *f_TestDone;
static OSThread f_TestThread;

// The scheduler's side
static OSMesgQueue f_RetraceQueue;
static OSMesg f_RetraceMessages[1];
static OSMesgQueue f_TaskQueue;
static OSMesg f_TaskMessages[1];
static int f_InstallCount;

// The audio output's side
static int f_QueuedFrames; // What GetQueuedAudioFrames claims
static long f_FramesReceived;
static int f_LastBlockFrames;
static float f_LastBlock[2048 * STEREO];
static bool f_IsOpen;

bool OpenAudioOutput(int sampleRate) {
    EXPECT(sampleRate == SAMPLE_RATE);
    f_IsOpen = true;
    return true;
}

void CloseAudioOutput(void) {
    f_IsOpen = false;
}

void QueueAudioOutput(const float *frames, int frameCount) {
    f_FramesReceived += frameCount;
    f_LastBlockFrames = frameCount;
    memcpy(f_LastBlock, frames, (size_t)frameCount * STEREO * sizeof(float));
}

int GetQueuedAudioFrames(void) {
    return f_QueuedFrames;
}

static void InstallScheduler(void) {
    osCreateMesgQueue(&f_RetraceQueue, f_RetraceMessages, 1);
    osCreateMesgQueue(&f_TaskQueue, f_TaskMessages, 1);
    f_InstallCount++;
}

static void WaitFrame(void) {
    osRecvMesg(&f_RetraceQueue, NULL, OS_MESG_BLOCK);
}

static void DoTask(musTask *task) {
    (void)task;
    osSendMesg(&f_TaskQueue, NULL, OS_MESG_BLOCK);
}

static musSched f_Scheduler = {InstallScheduler, WaitFrame, DoTask};

static void PlayFrames(int count) {
    int i;

    for (i = 0; i < count; i++) {
        osSendMesg(&f_RetraceQueue, NULL, OS_MESG_BLOCK);
        osRecvMesg(&f_TaskQueue, NULL, OS_MESG_BLOCK);
    }
}

// The loudest value in the last block
static double LastBlockPeak(void) {
    double peak = 0.0;
    int i;

    for (i = 0; i < f_LastBlockFrames * STEREO; i++) {
        peak = SDL_max(peak, SDL_fabs(f_LastBlock[i]));
    }

    return peak;
}

static void WriteBig32(u8 *bytes, u32 value) {
    bytes[0] = (u8)(value >> 24);
    bytes[1] = (u8)(value >> 16);
    bytes[2] = (u8)(value >> 8);
    bytes[3] = (u8)value;
}

// One wave of a steady level that loops forever, in raw 16-bit samples in
// the ROM, at its own pitch
static void MakeBanks(void) {
    static const u8 effect[] = {0, 0xFF, 0xFF, 0x80};
    int i;

    for (i = 0; i < WAVE_SAMPLES; i++) {
        f_Rom[WAVE_OFFSET + (i * 2)] = 0x40;
        f_Rom[WAVE_OFFSET + (i * 2) + 1] = 0x00;
    }

    WriteBig32(f_PointerBank + 32, 1);
    WriteBig32(f_PointerBank + 36, 64); // Base notes
    WriteBig32(f_PointerBank + 40, 68); // Detunes
    WriteBig32(f_PointerBank + 44, 72); // Waves
    f_PointerBank[64] = 48;
    WriteBig32(f_PointerBank + 72, 80);
    WriteBig32(f_PointerBank + 80, 0);
    WriteBig32(f_PointerBank + 84, WAVE_SAMPLES * 2);
    f_PointerBank[88] = AL_RAW16_WAVE;
    WriteBig32(f_PointerBank + 92, 112); // Loop
    WriteBig32(f_PointerBank + 112, 0);
    WriteBig32(f_PointerBank + 116, WAVE_SAMPLES);
    WriteBig32(f_PointerBank + 120, AL_LOOP_FOREVER);

    WriteBig32(f_EffectBank + 0, 1);
    WriteBig32(f_EffectBank + 20, 120); // Waves, all zeros: wave 0 is sample 0
    WriteBig32(f_EffectBank + 24, 128);
    WriteBig32(f_EffectBank + 28, 10);
    memcpy(f_EffectBank + 128, effect, sizeof(effect));
}

static void StartPlayer(void) {
    musConfig config;

    memset(&config, 0, sizeof(config));
    config.channels = 4;
    config.thread_priority = PLAYER_PRIORITY;
    config.heap = f_Heap;
    config.heap_length = HEAP_SIZE;
    config.syn_output_rate = SAMPLE_RATE;
    f_QueuedFrames = 3 * TICK_FRAMES;
    MusSetScheduler(&f_Scheduler);
    MusInitialize(&config);
    MusPtrBankInitialize(f_PointerBank, (void *)(uintptr_t)WAVE_OFFSET);
    MusFxBankInitialize(f_EffectBank);
}

// Each retrace plays its share of the sample rate, a second's in sixty
static void TestFramesFollowRetraces(void) {
    const long before = f_FramesReceived;

    PlayFrames(60);
    EXPECT(f_InstallCount == 1);
    EXPECT(f_IsOpen);
    EXPECT(MusGetFrameCount() == 60);
    EXPECT(f_FramesReceived - before == SAMPLE_RATE);
}

// Short of the device, the player plays a little more each frame; far ahead
// of it, it drops frames until it catches up
static void TestPacing(void) {
    f_QueuedFrames = 0;
    PlayFrames(1);
    EXPECT(f_LastBlockFrames > TICK_FRAMES + 1);
    EXPECT(f_LastBlockFrames <= TICK_FRAMES + 1 + (TICK_FRAMES / 32));
    f_LastBlockFrames = -1;
    f_QueuedFrames = 20 * TICK_FRAMES;
    PlayFrames(1);
    EXPECT(f_LastBlockFrames == -1);
    f_QueuedFrames = 3 * TICK_FRAMES;
}

// The heap gives aligned blocks of what the game asks, and no more than it has
static void TestHeap(void) {
    const int before = __MusIntMemRemaining();
    u8 *block = __MusIntMemMalloc(100);

    EXPECT(block >= f_Heap && block + 100 <= f_Heap + HEAP_SIZE);
    EXPECT(((uintptr_t)block & 15) == 0);
    EXPECT(__MusIntMemRemaining() == before - 112);
    EXPECT(__MusIntMemMalloc(HEAP_SIZE) == NULL);
    EXPECT(__MusIntMemRemaining() == before - 112);
}

// An effect sounds until its handle stops it; muted, the player plays
// silence
static void TestEffectHandle(void) {
    const musHandle handle = MusStartEffect(0);

    EXPECT(handle != 0);
    EXPECT(MusHandleAsk(handle) == 1);
    EXPECT(MusAsk(MUSFLAG_EFFECTS) == 1);
    EXPECT(MusAsk(MUSFLAG_SONGS) == 0);
    PlayFrames(2);
    EXPECT(LastBlockPeak() > 0.1);
    __libmus_output_muted = TRUE;
    PlayFrames(1);
    EXPECT(LastBlockPeak() == 0.0);
    __libmus_output_muted = FALSE;
    EXPECT(MusHandleStop(handle, 2) == 1);
    PlayFrames(4);
    EXPECT(MusHandleAsk(handle) == 0);
    EXPECT(LastBlockPeak() < 1e-4);
}

// The effect numbers choose from the game's list, and past it change nothing
static void TestEffectTypes(void) {
    EXPECT(ChangeCustomEffect(5) == 0);
    EXPECT(GetReverbKind() == REVERB_ECHO);
    EXPECT(ChangeCustomEffect(9) == 1);
    EXPECT(GetReverbKind() == REVERB_ECHO);
    EXPECT(MusSetFxType(0) != 0);
    EXPECT(GetReverbKind() == REVERB_NONE);
    EXPECT(MusSetFxType(8) != 0);
    EXPECT(GetReverbKind() == REVERB_BIG_ROOM);
}

static void RunTests(void *argument) {
    (void)argument;
    MakeBanks();
    StartPlayer();
    TestFramesFollowRetraces();
    TestPacing();
    TestHeap();
    TestEffectHandle();
    TestEffectTypes();
    SDL_SignalSemaphore(f_TestDone);
    for (;;) {
        osPcWaitForInterrupt();
    }
}

int main(void) {
    f_TestDone = SDL_CreateSemaphore(0);
    PcInitOs();
    PcInsertCartridge(f_Rom, sizeof(f_Rom));
    PcStartBootThread(&f_TestThread, RunTests, NULL, TEST_PRIORITY);
    while (!SDL_WaitSemaphoreTimeout(f_TestDone, 1)) {
        PcRaiseEvent(OS_EVENT_SW1);
    }

    return ReportTestResults("music_player_test");
}
