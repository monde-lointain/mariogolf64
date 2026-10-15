/* SoundTools task setup and retrace/pre-NMI scheduler callbacks. */
#include "common.h"
#include <nusys.h>
#include <nualstl.h>

extern OSMesgQueue nuAuRtnMesgQ;
extern OSMesg nuAuMesgBuf[];
extern OSMesg nuAuRtnMesgBuf;

/* These callbacks are referenced by the libmus scheduler table. */
void nuAuStlSchedInstall(void);
void nuAuStlSchedWaitFrame(void);
void nuAuStlSchedDoTask(musTask* task);

extern musSched nuAuStlSchedFunc;

s32 nuAuStlMgrInit(musConfig* config) {
  nuAuTaskStop = NU_AU_TASK_RUN;

  nuAuTask.msg = 0;
  nuAuTask.msgQ = &nuAuRtnMesgQ;
  nuAuTask.list.t.type = M_AUDTASK;
  nuAuTask.list.t.ucode_boot = (u64*)rspbootTextStart;
  nuAuTask.list.t.ucode_boot_size =
      ((s32)rspbootTextEnd - (s32)rspbootTextStart);
  nuAuTask.list.t.ucode_data_size = SP_UCODE_DATA_SIZE;
  nuAuTask.list.t.dram_stack = NULL;
  nuAuTask.list.t.dram_stack_size = 0;
  nuAuTask.list.t.output_buff = NULL;
  nuAuTask.list.t.output_buff_size = 0;
  nuAuTask.list.t.yield_data_ptr = NULL;
  nuAuTask.list.t.yield_data_size = 0;

  MusSetScheduler(&nuAuStlSchedFunc);
  return MusInitialize(config);
}

void nuAuStlSchedInstall(void) {
  osCreateMesgQueue(&nuAuMesgQ, nuAuMesgBuf, NU_AU_MESG_MAX);
  osCreateMesgQueue(&nuAuRtnMesgQ, &nuAuRtnMesgBuf, 1);
  nuScAddClient(&nuAuClient, &nuAuMesgQ, NU_SC_RETRACE_MSG | NU_SC_PRENMI_MSG);
}

/* Block for retrace, servicing any pre-NMI notifications received first. */
void nuAuStlSchedWaitFrame(void) {
  while (1) {
    NUScMsg* message_type;

    osRecvMesg(&nuAuMesgQ, (OSMesg*)&message_type, OS_MESG_BLOCK);
    switch (*message_type) {
      case NU_SC_RETRACE_MSG:
        return;
      case NU_SC_PRENMI_MSG:
        if (nuAuPreNMIFunc != NULL) {
          nuAuPreNMIFunc(NU_SC_PRENMI_MSG, nuAuPreNMI);
        }
        nuAuPreNMI++;
        break;
      default:
        break;
    }
  }
}

/* Task storage remains owned by libmus until the scheduler acknowledges it. */
void nuAuStlSchedDoTask(musTask* task) {
  if (nuAuTaskStop == NU_AU_TASK_RUN) {
    nuAuTask.list.t.data_ptr = task->data;
    nuAuTask.list.t.data_size = task->data_size;
    nuAuTask.list.t.ucode = (u64*)task->ucode;
    nuAuTask.list.t.ucode_data = (u64*)task->ucode_data;
    osSendMesg(&nusched.audioRequestMQ, (OSMesg*)&nuAuTask, OS_MESG_BLOCK);
    osRecvMesg(&nuAuRtnMesgQ, NULL, OS_MESG_BLOCK);
  }

  if (nuAuPreNMIFunc != NULL && nuAuPreNMI != 0) {
    nuAuPreNMIFunc(NU_SC_RETRACE_MSG, nuAuPreNMI);
    nuAuPreNMI++;
  }
}

extern u32 g_bgm_song_buffer;
extern void* __MusIntMemMalloc(s32 size);

void bgm_alloc_song_buffer(s32 size) {
  g_bgm_song_buffer = (u32)__MusIntMemMalloc(size);
}

musHandle bgm_start_current(void) {
  return MusStartSong((void*)g_bgm_song_buffer);
}
