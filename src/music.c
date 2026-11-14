#include "music.h"

#include <PR/libmus.h>
#include <PR/n_libaudio_sc.h>
#include <nusys.h>
#include <ultra64.h>

#include "address.h"
#include "compiler.h"
#include "language.h"
#include "sound.h"

typedef void (*AudioEventCallback)(NUScMsg message, s32 resetFrame);

#define SONG_BUFFER_SIZE 0xC000
#define SOUND_EFFECT_BUFFER_SIZE 0x4000

#define RESET_FX_OFF_FRAME 20
#define RESET_TASKS_OFF_FRAME 28

#define PRINTED_BANK_SIZE 64
#define PRINTED_ROW_END 7

#define RETRACE_QUEUE_SIZE 2
#define TASK_DONE_QUEUE_SIZE 1

static OSMesg f_RetraceMessages[RETRACE_QUEUE_SIZE];
static OSMesg f_TaskDoneMessages[TASK_DONE_QUEUE_SIZE];
static NUScClient f_AudioClient;

void *__MusIntMemMalloc(int length);
void InstallAudioScheduler(void);
void WaitForAudioFrame(void);
void RunAudioTask(musTask *task);
void StartMusicPlayer(musConfig *config);
void AllocSongBuffer(s32 size);
void AllocSoundEffectBuffer(s32 size);

FILE_STATIC NUScTask f_AudioTask;
FILE_STATIC OSMesgQueue f_TaskDoneQueue;
FILE_STATIC OSMesgQueue f_RetraceQueue;

static u8 *f_SongBuffer = NULL;
static u8 *f_SoundEffectBuffer = NULL;
static u8 *f_PointerBank = NULL;
static UBoolean8 f_IsAudioTaskEnabled = TRUE;
static u8 f_ResetFrame = 0;
static AudioEventCallback f_AudioEventCallback = NULL;
static musSched f_AudioScheduler = {InstallAudioScheduler, WaitForAudioFrame, RunAudioTask};
static musConfig f_DefaultMusicConfig = {
    0,
    24,
    NULL,
    70,
    (u8 *)0x80335000,
    0x5A800,
    NULL,
    NULL,
    NULL,
    64,
    128,
    32000,
    0x800,
    1,
    96,
    0x400,
    NULL,
};

void SetAudioEventCallback(AudioEventCallback callback) {
    u32 mask = osSetIntMask(OS_IM_NONE);

    f_AudioEventCallback = callback;
    osSetIntMask(mask);
}

void HandleAudioReset(NUScMsg message, s32 resetFrame) {
    switch (message) {
        case NU_SC_PRENMI_MSG:
            MusStop(MUSFLAG_EFFECTS, 0);
            MusSetFxType(0);
            break;
        case NU_SC_RETRACE_MSG:
            if (resetFrame == RESET_FX_OFF_FRAME) {
                MusSetFxType(0);
            }

            if (resetFrame == RESET_TASKS_OFF_FRAME) {
                f_IsAudioTaskEnabled = FALSE;
            }

            break;
    }
}

void LoadSampleBank(u8 *pointerBankRom, u32 pointerBankSize, u8 *waveBankRom) {
    u32 pointerIndex;

    f_PointerBank = __MusIntMemMalloc(pointerBankSize);
    nuPiReadRom((u32)pointerBankRom, f_PointerBank, pointerBankSize);
    osSyncPrintf("nuAuSampleBank_ptr %x, size%d\n", f_PointerBank, pointerBankSize);
    for (pointerIndex = 0; pointerIndex < PRINTED_BANK_SIZE; pointerIndex++) {
        osSyncPrintf("%2.2X ", f_PointerBank[pointerIndex]);
        if ((pointerIndex & PRINTED_ROW_END) == PRINTED_ROW_END) {
            osSyncPrintf("\n");
        }
    }

    MusPtrBankInitialize(f_PointerBank, waveBankRom);
}

void InitMusic(musConfig *config) {
    if (config == NULL) {
        config = &f_DefaultMusicConfig;
    }

    StartMusicPlayer(config);
    AllocSongBuffer(SONG_BUFFER_SIZE);
    AllocSoundEffectBuffer(SOUND_EFFECT_BUFFER_SIZE);
    SetAudioEventCallback(HandleAudioReset);
}

void StartMusicPlayer(musConfig *config) {
    f_IsAudioTaskEnabled = TRUE;
    f_AudioTask.msgQ = &f_TaskDoneQueue;
    f_AudioTask.list.t.type = M_AUDTASK;
    f_AudioTask.msg = NULL;
    f_AudioTask.list.t.ucode_boot = (u64 *)rspbootTextStart;
    f_AudioTask.list.t.ucode_boot_size = BytesBetween(rspbootTextStart, n_aspMainTextStart);
    f_AudioTask.list.t.ucode_data_size = SP_UCODE_DATA_SIZE;
    f_AudioTask.list.t.dram_stack = NULL;
    f_AudioTask.list.t.dram_stack_size = 0;
    f_AudioTask.list.t.output_buff = NULL;
    f_AudioTask.list.t.output_buff_size = 0;
    f_AudioTask.list.t.yield_data_ptr = NULL;
    f_AudioTask.list.t.yield_data_size = 0;
    MusSetScheduler(&f_AudioScheduler);
    MusInitialize(config);
}

void InstallAudioScheduler(void) {
    osCreateMesgQueue(&f_RetraceQueue, f_RetraceMessages, RETRACE_QUEUE_SIZE);
    osCreateMesgQueue(&f_TaskDoneQueue, f_TaskDoneMessages, TASK_DONE_QUEUE_SIZE);
    nuScAddClient(&f_AudioClient, &f_RetraceQueue, NU_SC_RETRACE_MSG | NU_SC_PRENMI_MSG);
}

void WaitForAudioFrame(void) {
    NUScMsg *message;

    while (TRUE) {
        osRecvMesg(&f_RetraceQueue, (OSMesg *)&message, OS_MESG_BLOCK);
        if (*message == NU_SC_RETRACE_MSG) {
            break;
        }

        if (*message == NU_SC_PRENMI_MSG) {
            if (f_AudioEventCallback != NULL) {
                f_AudioEventCallback(NU_SC_PRENMI_MSG, f_ResetFrame);
            }

            f_ResetFrame++;
        }
    }
}

void RunAudioTask(musTask *task) {
    if (f_IsAudioTaskEnabled == TRUE) {
        f_AudioTask.list.t.data_ptr = task->data;
        f_AudioTask.list.t.data_size = task->data_size;
        f_AudioTask.list.t.ucode = task->ucode;
        f_AudioTask.list.t.ucode_data = task->ucode_data;
        osSendMesg(&nusched.audioRequestMQ, &f_AudioTask, OS_MESG_BLOCK);
        osRecvMesg(&f_TaskDoneQueue, NULL, OS_MESG_BLOCK);
    }

    if (f_AudioEventCallback != NULL && f_ResetFrame != 0) {
        f_AudioEventCallback(NU_SC_RETRACE_MSG, f_ResetFrame);
        f_ResetFrame++;
    }
}

void AllocSongBuffer(s32 size) {
    f_SongBuffer = __MusIntMemMalloc(size);
}

musHandle StartLoadedSong(void) {
    return MusStartSong(f_SongBuffer);
}

void LoadSong(u32 romAddress, u32 size) {
    nuPiReadRom(romAddress, f_SongBuffer, size + (size & 1));
}

void AllocSoundEffectBuffer(s32 size) {
    f_SoundEffectBuffer = __MusIntMemMalloc(size);
}

musHandle StartSoundEffect(SoundId soundId) {
    return MusStartEffect(soundId);
}

musHandle StartSoundEffectWith(
    SoundId soundId,
    SoundVolume volume,
    SoundPan pan,
    Boolean restarts,
    s32 priority
) {
    return MusStartEffect2(soundId, volume, pan, restarts, priority);
}

void LoadSoundEffectBank(u8 *bankRom, u32 bankSize) {
    bankSize += bankSize & 1;
    if (bankSize > SOUND_EFFECT_BUFFER_SIZE) {
        osSyncPrintf("SE Seq. Buffer Over\n");
    }

    nuPiReadRom((u32)bankRom, f_SoundEffectBuffer, bankSize);
    MusFxBankInitialize(f_SoundEffectBuffer);
}
