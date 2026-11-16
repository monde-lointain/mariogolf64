/*
 * The port's music player, in place of libmus: the API that the game calls,
 * over the sequencer and the synthesizer.
 *
 * MusInitialize starts the player's thread, a thread of the game's like any
 * other, which runs the scheduler callbacks that the game set: it waits for
 * each retrace, plays a frame of music into the audio output, and hands the
 * game an audio task, which NuSystem finishes at once, so that the game's
 * reset handling runs as it did on the N64.
 *
 * The game's threads take turns, so the API and the player's thread never
 * run at once, and the API acts on the sequencer straight away rather than
 * through a queue.
 *
 * The player keeps its state in host memory. The heap that the game gives
 * holds only what the game takes from it, through __MusIntMemMalloc.
 */

#include "music_player.h"

#include <PR/libmus.h>
#include <SDL3/SDL.h>
#include <ultra64.h>

#include "audio_output.h"
#include "port/os_host.h"
#include "reverb.h"
#include "sequencer.h"
#include "sound_data.h"
#include "synth.h"

#define TICKS_PER_SECOND 60
#define DEFAULT_SAMPLE_RATE 32000
#define STEREO 2
#define PLAYER_THREAD_ID 9
#define UNITY_SCALE 0x80
#define HEAP_ALIGNMENT 16
// The output is kept this many ticks ahead of the device; past the ceiling
// a tick's frames are dropped, to catch up after a stall
#define PACING_TARGET_TICKS 3
#define PACING_CEILING_TICKS 8
// The most that pacing stretches or squeezes a tick, as a share of it
#define PACING_SHARE_SHIFT 5

// The effects that MusSetFxType and ChangeCustomEffect choose between, in
// the game's numbering: none, then the room, chorus, flange and echo, then
// the editable echo and rooms, which the game leaves at their defaults
static const ReverbKind f_EffectSettings[] = {
    REVERB_NONE,
    REVERB_SMALL_ROOM,
    REVERB_BIG_ROOM,
    REVERB_CHORUS,
    REVERB_FLANGE,
    REVERB_ECHO,
    REVERB_ECHO,
    REVERB_SMALL_ROOM,
    REVERB_BIG_ROOM,
};

unsigned char __libmus_output_muted;
unsigned long _mus_cpu_last;
unsigned long _mus_cpu_worst;

static musSched *f_Scheduler;
static LIBMUScb_marker f_MarkerCallback;
static bool f_SongsChangeEffect;
static bool f_IsRunning;
static OSThread f_Thread;
static int f_SampleRate = DEFAULT_SAMPLE_RATE;
static unsigned long f_FrameCount;
static int f_SampleClock; // The fraction of a frame that ticks have left over
static float f_Mix[SYNTH_MAX_BLOCK_FRAMES * STEREO];
static u8 *f_Heap;
static int f_HeapSize;
static int f_HeapUsed;
static SampleBank f_Samples;
static EffectBank f_Effects;
static bool f_HasEffects;

// --- Each frame ---

// The frames for this tick: its share of the sample rate, nudged to keep
// the output a few ticks ahead of the device. Returns 0 to drop the tick's
// frames.
static int CountTickFrames(void) {
    const int base = f_SampleRate / TICKS_PER_SECOND;
    const int queued = GetQueuedAudioFrames();
    const int target = base * PACING_TARGET_TICKS;
    const int nudgeLimit = base >> PACING_SHARE_SHIFT;
    int frames;

    f_SampleClock += f_SampleRate;
    frames = f_SampleClock / TICKS_PER_SECOND;
    f_SampleClock -= frames * TICKS_PER_SECOND;
    if (queued > base * PACING_CEILING_TICKS) {
        return 0;
    }

    frames += SDL_clamp((target - queued) / 8, -nudgeLimit, nudgeLimit);
    return SDL_clamp(frames, 1, SYNTH_MAX_BLOCK_FRAMES);
}

static void PlayMusicFrame(void) {
    const Uint64 start = SDL_GetPerformanceCounter();
    const int frames = CountTickFrames();
    Uint64 microseconds;

    f_FrameCount++;
    TickSequencer();
    if (frames > 0) {
        RenderSynth(f_Mix, frames);
        if (__libmus_output_muted) {
            SDL_memset(f_Mix, 0, (size_t)frames * STEREO * sizeof(float));
        }

        QueueAudioOutput(f_Mix, frames);
    }

    microseconds = (SDL_GetPerformanceCounter() - start) * 1000000 / SDL_GetPerformanceFrequency();
    _mus_cpu_last = (unsigned long)microseconds;
    _mus_cpu_worst = SDL_max(_mus_cpu_worst, _mus_cpu_last);
}

static void RunMusicPlayer(void *argument) {
    musTask task;

    (void)argument;
    SDL_zero(task);
    if (f_Scheduler->install != NULL) {
        f_Scheduler->install();
    }

    for (;;) {
        f_Scheduler->waitframe();
        PlayMusicFrame();
        f_Scheduler->dotask(&task);
    }
}

// --- Effects and markers ---

s32 ChangeCustomEffect(s32 type) {
    if (type < 0 || type >= (s32)SDL_arraysize(f_EffectSettings)) {
        return 1;
    }

    SelectReverb(f_EffectSettings[type]);
    return 0;
}

int MusSetFxType(int fxtype) {
    (void)ChangeCustomEffect(fxtype);
    return 1;
}

int MusSetSongFxChange(musBool onoff) {
    f_SongsChangeEffect = onoff != MUSBOOL_OFF;
    return 1;
}

static void OnSongEffectChange(int effect) {
    if (f_SongsChangeEffect) {
        (void)ChangeCustomEffect(effect);
    }
}

void MusSetMarkerCallback(void *callback) {
    f_MarkerCallback = (LIBMUScb_marker)callback;
}

static void OnMarker(uint32_t handle, int marker) {
    if (f_MarkerCallback != NULL) {
        f_MarkerCallback(handle, marker);
    }
}

// --- Starting the player ---

void MusSetScheduler(musSched *sched_list) {
    f_Scheduler = sched_list;
}

void *__MusIntMemMalloc(int length) {
    const int size = (length + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
    u8 *block;

    if (f_Heap == NULL || length < 0 || size > f_HeapSize - f_HeapUsed) {
        osSyncPrintf("Music heap can't give %d bytes\n", length);
        return NULL;
    }

    block = f_Heap + f_HeapUsed;
    f_HeapUsed += size;
    SDL_memset(block, 0, (size_t)size);
    return block;
}

int __MusIntMemRemaining(void) {
    return f_HeapSize - f_HeapUsed;
}

// Gives the heap the game's memory, aligned for the PI's reads
static void SetHeap(u8 *heap, int length) {
    const uintptr_t address = (uintptr_t)heap;
    const uintptr_t aligned = (address + HEAP_ALIGNMENT - 1) & ~(uintptr_t)(HEAP_ALIGNMENT - 1);
    const int skipped = (int)(aligned - address);

    f_Heap = NULL;
    f_HeapSize = 0;
    f_HeapUsed = 0;
    if (heap != NULL && length > skipped) {
        f_Heap = (u8 *)aligned;
        f_HeapSize = length - skipped;
    }
}

static void StartPlayerThread(int priority) {
    if (f_IsRunning) {
        return;
    }

    if (f_Scheduler == NULL || f_Scheduler->waitframe == NULL || f_Scheduler->dotask == NULL) {
        PcFatalError("MusInitialize needs MusSetScheduler first");
    }

    f_IsRunning = true;
    osCreateThread(&f_Thread, PLAYER_THREAD_ID, RunMusicPlayer, NULL, NULL, priority);
    osStartThread(&f_Thread);
}

int MusInitialize(musConfig *config) {
    int voices;

    SDL_assert(config != NULL);
    SetHeap(config->heap, config->heap_length);
    f_SampleRate = (config->syn_output_rate > 0) ? config->syn_output_rate : DEFAULT_SAMPLE_RATE;
    f_SampleClock = 0;
    f_FrameCount = 0;
    voices = SDL_clamp(config->channels, 0, SYNTH_MAX_VOICES);
    InitSynth(f_SampleRate, voices);
    InitSequencer(voices, TICKS_PER_SECOND);
    SetSequencerHandlers(OnMarker, OnSongEffectChange);
    SelectReverb(REVERB_BIG_ROOM);
    if (config->ptr != NULL) {
        MusPtrBankInitialize(config->ptr, config->wbk);
    }

    if (config->default_fxbank != NULL) {
        MusFxBankInitialize(config->default_fxbank);
    }

    (void)OpenAudioOutput(f_SampleRate);
    StartPlayerThread(config->thread_priority);
    return f_HeapUsed;
}

// --- Banks ---

void MusPtrBankInitialize(void *pbank, void *wbank) {
    size_t romSize = 0;
    const u8 *rom = PcGetCartridgeRom(&romSize);
    const uintptr_t waveOffset = (uintptr_t)wbank;
    WaveSource waves = {NULL, 0};

    // The wave bank is in ROM, at the offset that wbank holds
    if (rom != NULL && waveOffset < romSize) {
        waves.start = rom + waveOffset;
        waves.size = romSize - waveOffset;
    }

    ReleaseSampleBank(&f_Samples);
    if (!ParseSampleBank(pbank, waves, &f_Samples)) {
        osSyncPrintf("The sample bank at %p isn't one\n", pbank);
    }
}

void MusFxBankInitialize(void *fxbank) {
    f_HasEffects = ParseEffectBank(fxbank, &f_Effects);
    if (!f_HasEffects) {
        osSyncPrintf("The effect bank at %p isn't one\n", fxbank);
    }
}

// --- Songs and effects ---

musHandle MusStartSong(void *addr) {
    SongLayout layout;

    if (!ParseSong(addr, &layout)) {
        osSyncPrintf("The song at %p isn't one\n", addr);
        return 0;
    }

    return StartSong(&layout, &f_Samples);
}

musHandle MusStartEffect2(int number, int volume, int pan, int restartflag, int priority) {
    if (!f_HasEffects) {
        return 0;
    }

    return StartEffect(&f_Effects, number, &f_Samples, volume, pan, priority, restartflag != 0);
}

musHandle MusStartEffect(int number) {
    return MusStartEffect2(number, UNITY_SCALE, UNITY_SCALE, 0, EFFECT_BANK_PRIORITY);
}

void MusStop(unsigned long flags, int speed) {
    (void)StopSoundKinds((uint32_t)flags, speed);
}

int MusAsk(unsigned long flags) {
    return CountSoundKinds((uint32_t)flags);
}

int MusHandleAsk(musHandle handle) {
    return CountSoundChannels((uint32_t)handle);
}

int MusHandleStop(musHandle handle, int speed) {
    return StopSound((uint32_t)handle, speed);
}

int MusHandleSetVolume(musHandle handle, int volume) {
    return SetSoundVolumeScale((uint32_t)handle, volume);
}

int MusHandleSetPan(musHandle handle, int pan) {
    return SetSoundPanScale((uint32_t)handle, pan);
}

int MusHandleSetFreqOffset(musHandle handle, float offset) {
    return SetSoundPitchOffset((uint32_t)handle, offset);
}

int MusHandleSetTempo(musHandle handle, int tempo) {
    return SetSoundTempoScale((uint32_t)handle, tempo);
}

int MusHandleSetReverb(musHandle handle, int reverb) {
    return SetSoundReverbBase((uint32_t)handle, reverb);
}

// --- Settings and reports ---

void MusSetMasterVolume(unsigned long flags, int volume) {
    SetMasterVolume((uint32_t)flags, SDL_clamp(volume, 0, SYNTH_MAX_VOLUME));
}

void MusSetStereo(musBool stereo) {
    SetStereoOutput(stereo != MUSBOOL_OFF);
}

unsigned long MusGetFrameCount(void) {
    return f_FrameCount;
}

// The N64's player streamed samples through DMA buffers; the port reads them
// from memory, so there is no use to report
void MusPrintDmaUsage(void) {
}
