/*
 * Starts the music player and its audio tasks, and loads the songs, the sound
 * effect banks and the sample bank.
 */
#include "common.h"

#include <PR/libmus.h>
#include <lib/libnusys/nusys.h>

/* Called with NU_SC_PRENMI_MSG on a pre-NMI, then with NU_SC_RETRACE_MSG after
 * each audio task */
typedef void (*AudioEventCallback)(NUScMsg type, s32 frame);

#define SONG_BUFFER_SIZE 0xC000
#define SOUND_EFFECT_BUFFER_SIZE 0x4000

/* The audio frames after a pre-NMI at which the reset handler acts */
#define RESET_FX_OFF_FRAME 20
#define RESET_TASKS_OFF_FRAME 28

/* The bytes of the pointer bank LoadSampleBank prints, and the last column of
 * each row */
#define PRINTED_BANK_SIZE 64
#define PRINTED_ROW_END 7

#define RETRACE_QUEUE_SIZE 2
#define TASK_DONE_QUEUE_SIZE 1

extern NUScTask f_AudioTask;
extern OSMesgQueue f_TaskDoneQueue;
extern OSMesg f_TaskDoneMessages[TASK_DONE_QUEUE_SIZE];
extern OSMesgQueue f_RetraceQueue;
extern OSMesg f_RetraceMessages[RETRACE_QUEUE_SIZE];
extern NUScClient f_AudioClient;
extern long long int n_aspMainTextStart[];

void* __MusIntMemMalloc(int length);
void InstallAudioScheduler(void);
void WaitForAudioFrame(void);
void RunAudioTask(musTask* task);
void StartMusicPlayer(musConfig* config);
void AllocSongBuffer(s32 size);
void AllocSoundEffectBuffer(s32 size);
void SetAudioEventCallback(AudioEventCallback callback);
void HandleAudioReset(NUScMsg type, s32 frame);

u8* f_SongBuffer = NULL;
u8* f_SoundEffectBuffer = NULL;
u8* f_PointerBank = NULL;
/* Cleared to stop sending audio tasks to the scheduler */
u8 f_IsAudioTaskEnabled = TRUE;
/* Counts the pre-NMI and the audio tasks after it */
u8 f_ResetFrame = 0;
AudioEventCallback f_AudioEventCallback = NULL;
musSched f_AudioScheduler = {
    InstallAudioScheduler, WaitForAudioFrame, RunAudioTask};
musConfig f_DefaultMusicConfig = {
    0,
    24,
    NULL,
    70,
    (u8*)0x80335000,
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

/* Sets the routine called on audio events, with interrupts off while it changes
 */
void SetAudioEventCallback(AudioEventCallback callback) {
  u32 mask = osSetIntMask(OS_IM_NONE);

  f_AudioEventCallback = callback;
  osSetIntMask(mask);
}

/* Silences the audio before a reset: effects at once, then the effect type,
 * then the audio tasks */
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
        f_IsAudioTaskEnabled = FALSE;
      }
      break;
  }
}

/* Reads the sample pointer bank into the music heap and starts the player's
 * banks with it */
void LoadSampleBank(u8* pointerBankRom, u32 pointerBankSize, u8* waveBankRom) {
  u32 i;

  f_PointerBank = __MusIntMemMalloc(pointerBankSize);
  nuPiReadRom((u32)pointerBankRom, f_PointerBank, pointerBankSize);
  osSyncPrintf(
      "nuAuSampleBank_ptr %x, size%d\n", f_PointerBank, pointerBankSize);
  for (i = 0; i < PRINTED_BANK_SIZE; i++) {
    osSyncPrintf("%2.2X ", f_PointerBank[i]);
    if ((i & PRINTED_ROW_END) == PRINTED_ROW_END) {
      osSyncPrintf("\n");
    }
  }
  MusPtrBankInitialize(f_PointerBank, waveBankRom);
}

/* Starts the music player, with the default configuration when given none */
void InitMusic(musConfig* config) {
  if (config == NULL) {
    config = &f_DefaultMusicConfig;
  }
  StartMusicPlayer(config);
  AllocSongBuffer(SONG_BUFFER_SIZE);
  AllocSoundEffectBuffer(SOUND_EFFECT_BUFFER_SIZE);
  SetAudioEventCallback(HandleAudioReset);
}

/* Sets up the audio task that the player hands to the scheduler, and starts the
 * player */
void StartMusicPlayer(musConfig* config) {
  f_IsAudioTaskEnabled = TRUE;
  f_AudioTask.msgQ = &f_TaskDoneQueue;
  f_AudioTask.list.t.type = M_AUDTASK;
  f_AudioTask.msg = NULL;
  f_AudioTask.list.t.ucode_boot = (u64*)rspbootTextStart;
  f_AudioTask.list.t.ucode_boot_size =
      (u8*)n_aspMainTextStart - (u8*)rspbootTextStart;
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

/* Has the scheduler send its retrace and pre-NMI messages to the player */
void InstallAudioScheduler(void) {
  osCreateMesgQueue(&f_RetraceQueue, f_RetraceMessages, RETRACE_QUEUE_SIZE);
  osCreateMesgQueue(&f_TaskDoneQueue, f_TaskDoneMessages, TASK_DONE_QUEUE_SIZE);
  nuScAddClient(
      &f_AudioClient, &f_RetraceQueue, NU_SC_RETRACE_MSG | NU_SC_PRENMI_MSG);
}

/* Waits for the next retrace, reporting any pre-NMI on the way */
void WaitForAudioFrame(void) {
  NUScMsg* message;

  while (TRUE) {
    osRecvMesg(&f_RetraceQueue, (OSMesg*)&message, OS_MESG_BLOCK);
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

/* Has the scheduler run an audio task and waits for it */
void RunAudioTask(musTask* task) {
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

/* Reads a song into the song buffer; the PI reads even sizes */
void LoadSong(u32 romAddress, u32 size) {
  nuPiReadRom(romAddress, f_SongBuffer, size + (size & 1));
}

void AllocSoundEffectBuffer(s32 size) {
  f_SoundEffectBuffer = __MusIntMemMalloc(size);
}

musHandle StartSoundEffect(s32 soundId) {
  return MusStartEffect(soundId);
}

musHandle StartSoundEffectWith(
    s32 soundId, s32 volume, s32 pan, s32 restartFlag, s32 priority) {
  return MusStartEffect2(soundId, volume, pan, restartFlag, priority);
}

void LoadSoundEffectBank(u8* rom, u32 size) {
  size += size & 1;
  if (size > SOUND_EFFECT_BUFFER_SIZE) {
    osSyncPrintf("SE Seq. Buffer Over\n");
  }
  nuPiReadRom((u32)rom, f_SoundEffectBuffer, size);
  MusFxBankInitialize(f_SoundEffectBuffer);
}
