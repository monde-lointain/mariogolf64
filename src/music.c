#include "common.h"
#include <lib/libnusys/nusys.h>
#include <PR/libmus.h>

/* Called with NU_SC_PRENMI_MSG on a pre-NMI, then with NU_SC_RETRACE_MSG after each audio task */
typedef void (*AudioEventCallback)(NUScMsg type, s32 frame);

#define SONG_BUFFER_SIZE 0xC000
#define SOUND_EFFECT_BUFFER_SIZE 0x4000

/* The audio frames after a pre-NMI at which the reset handler acts */
#define RESET_FX_OFF_FRAME 20
#define RESET_TASKS_OFF_FRAME 28

/* The bytes of the pointer bank LoadSampleBank prints, and the last column of each row */
#define PRINTED_BANK_SIZE 64
#define PRINTED_ROW_END 7

#define RETRACE_QUEUE_SIZE 2
#define TASK_DONE_QUEUE_SIZE 1

extern NUScTask sAudioTask;
extern OSMesgQueue sTaskDoneQueue;
extern OSMesg sTaskDoneMessages[TASK_DONE_QUEUE_SIZE];
extern OSMesgQueue sRetraceQueue;
extern OSMesg sRetraceMessages[RETRACE_QUEUE_SIZE];
extern NUScClient sAudioClient;
extern long long int n_aspMainTextStart[];

void *__MusIntMemMalloc(int length);
void InstallAudioScheduler(void);
void WaitForAudioFrame(void);
void RunAudioTask(musTask *task);
void StartMusicPlayer(musConfig *config);
void AllocSongBuffer(s32 size);
void AllocSoundEffectBuffer(s32 size);
void SetAudioEventCallback(AudioEventCallback callback);
void HandleAudioReset(NUScMsg type, s32 frame);

u8 *sSongBuffer = NULL;
u8 *sSoundEffectBuffer = NULL;
u8 *sPointerBank = NULL;
/* Cleared to stop sending audio tasks to the scheduler */
u8 sIsAudioTaskEnabled = TRUE;
/* Counts the pre-NMI and the audio tasks after it */
u8 sResetFrame = 0;
AudioEventCallback sAudioEventCallback = NULL;
musSched sAudioScheduler = { InstallAudioScheduler, WaitForAudioFrame, RunAudioTask };
musConfig sDefaultMusicConfig = {
    0, 24, NULL, 70, (u8 *)0x80335000, 0x5A800, NULL, NULL, NULL, 64, 128, 32000, 0x800, 1, 96, 0x400, NULL,
};

/* Sets the routine called on audio events, with interrupts off while it changes */
void SetAudioEventCallback(AudioEventCallback callback) {
    u32 mask = osSetIntMask(OS_IM_NONE);

    sAudioEventCallback = callback;
    osSetIntMask(mask);
}

/* Silences the audio before a reset: effects at once, then the effect type, then the audio tasks */
void HandleAudioReset(NUScMsg type, s32 frame) {
    switch (type) {
        case NU_SC_PRENMI_MSG:
            MusStop(MUSFLAG_EFFECTS, 0);
            MusSetFxType(0);
            break;
        case NU_SC_RETRACE_MSG:
            if (frame == RESET_FX_OFF_FRAME) {
                MusSetFxType(0);
            }
            if (frame == RESET_TASKS_OFF_FRAME) {
                sIsAudioTaskEnabled = FALSE;
            }
            break;
    }
}

/* Reads the sample pointer bank into the music heap and starts the player's banks with it */
void LoadSampleBank(u8 *pointerBankRom, u32 pointerBankSize, u8 *waveBankRom) {
    u32 i;

    sPointerBank = __MusIntMemMalloc(pointerBankSize);
    nuPiReadRom((u32)pointerBankRom, sPointerBank, pointerBankSize);
    osSyncPrintf("nuAuSampleBank_ptr %x, size%d\n", sPointerBank, pointerBankSize);
    for (i = 0; i < PRINTED_BANK_SIZE; i++) {
        osSyncPrintf("%2.2X ", sPointerBank[i]);
        if ((i & PRINTED_ROW_END) == PRINTED_ROW_END) {
            osSyncPrintf("\n");
        }
    }
    MusPtrBankInitialize(sPointerBank, waveBankRom);
}

/* Starts the music player, with the default configuration when given none */
void InitMusic(musConfig *config) {
    if (config == NULL) {
        config = &sDefaultMusicConfig;
    }
    StartMusicPlayer(config);
    AllocSongBuffer(SONG_BUFFER_SIZE);
    AllocSoundEffectBuffer(SOUND_EFFECT_BUFFER_SIZE);
    SetAudioEventCallback(HandleAudioReset);
}

/* Sets up the audio task that the player hands to the scheduler, and starts the player */
void StartMusicPlayer(musConfig *config) {
    sIsAudioTaskEnabled = TRUE;
    sAudioTask.msgQ = &sTaskDoneQueue;
    sAudioTask.list.t.type = M_AUDTASK;
    sAudioTask.msg = NULL;
    sAudioTask.list.t.ucode_boot = (u64 *)rspbootTextStart;
    sAudioTask.list.t.ucode_boot_size = (u8 *)n_aspMainTextStart - (u8 *)rspbootTextStart;
    sAudioTask.list.t.ucode_data_size = SP_UCODE_DATA_SIZE;
    sAudioTask.list.t.dram_stack = NULL;
    sAudioTask.list.t.dram_stack_size = 0;
    sAudioTask.list.t.output_buff = NULL;
    sAudioTask.list.t.output_buff_size = 0;
    sAudioTask.list.t.yield_data_ptr = NULL;
    sAudioTask.list.t.yield_data_size = 0;
    MusSetScheduler(&sAudioScheduler);
    MusInitialize(config);
}

/* Has the scheduler send its retrace and pre-NMI messages to the player */
void InstallAudioScheduler(void) {
    osCreateMesgQueue(&sRetraceQueue, sRetraceMessages, RETRACE_QUEUE_SIZE);
    osCreateMesgQueue(&sTaskDoneQueue, sTaskDoneMessages, TASK_DONE_QUEUE_SIZE);
    nuScAddClient(&sAudioClient, &sRetraceQueue, NU_SC_RETRACE_MSG | NU_SC_PRENMI_MSG);
}

/* Waits for the next retrace, reporting any pre-NMI on the way */
void WaitForAudioFrame(void) {
    NUScMsg *message;

    while (TRUE) {
        osRecvMesg(&sRetraceQueue, (OSMesg *)&message, OS_MESG_BLOCK);
        if (*message == NU_SC_RETRACE_MSG) {
            break;
        }
        if (*message == NU_SC_PRENMI_MSG) {
            if (sAudioEventCallback != NULL) {
                sAudioEventCallback(NU_SC_PRENMI_MSG, sResetFrame);
            }
            sResetFrame++;
        }
    }
}

/* Has the scheduler run an audio task and waits for it */
void RunAudioTask(musTask *task) {
    if (sIsAudioTaskEnabled == TRUE) {
        sAudioTask.list.t.data_ptr = task->data;
        sAudioTask.list.t.data_size = task->data_size;
        sAudioTask.list.t.ucode = task->ucode;
        sAudioTask.list.t.ucode_data = task->ucode_data;
        osSendMesg(&nusched.audioRequestMQ, &sAudioTask, OS_MESG_BLOCK);
        osRecvMesg(&sTaskDoneQueue, NULL, OS_MESG_BLOCK);
    }
    if (sAudioEventCallback != NULL && sResetFrame != 0) {
        sAudioEventCallback(NU_SC_RETRACE_MSG, sResetFrame);
        sResetFrame++;
    }
}

void AllocSongBuffer(s32 size) {
    sSongBuffer = __MusIntMemMalloc(size);
}

musHandle StartLoadedSong(void) {
    return MusStartSong(sSongBuffer);
}

/* Reads a song into the song buffer; the PI reads even sizes */
void LoadSong(u32 romAddress, u32 size) {
    nuPiReadRom(romAddress, sSongBuffer, size + (size & 1));
}

void AllocSoundEffectBuffer(s32 size) {
    sSoundEffectBuffer = __MusIntMemMalloc(size);
}

musHandle StartSoundEffect(s32 soundId) {
    return MusStartEffect(soundId);
}

musHandle StartSoundEffectWith(s32 soundId, s32 volume, s32 pan, s32 restartFlag, s32 priority) {
    return MusStartEffect2(soundId, volume, pan, restartFlag, priority);
}

void LoadSoundEffectBank(u8 *rom, u32 size) {
    size += size & 1;
    if (size > SOUND_EFFECT_BUFFER_SIZE) {
        osSyncPrintf("SE Seq. Buffer Over\n");
    }
    nuPiReadRom((u32)rom, sSoundEffectBuffer, size);
    MusFxBankInitialize(sSoundEffectBuffer);
}
