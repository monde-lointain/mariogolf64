// clang-format off
#include "common.h"
#include <nusys.h>
#include <nualstl.h>
// clang-format on

/*
 * Game audio TU fragment carved from the embedded nualstl3 region (vram
 * 0x8005EDD0..0x8005F090). The four nuAuStl* functions are a verbatim mirror of
 * nusys-2.05 nuaustlmgr.c; file-scope data/bss is drop-def to externs
 * (symbol_addrs) and the scheduler callbacks are global so the recovered .data
 * nuAuStlSchedFunc table resolves to them. The two bgm_* helpers are game code
 * (classical).
 */

/* nuaustlmgr.c file-scope defs that nualstl.h does not export (drop-def) */
extern OSMesgQueue nuAuRtnMesgQ;
extern OSMesg nuAuMesgBuf[];
extern OSMesg nuAuRtnMesgBuf;

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
      ((int)rspbootTextEnd - (int)rspbootTextStart);
  nuAuTask.list.t.ucode_data_size = SP_UCODE_DATA_SIZE;
  nuAuTask.list.t.dram_stack = (u64*)NULL;
  nuAuTask.list.t.dram_stack_size = 0;
  nuAuTask.list.t.output_buff = (u64*)NULL;
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

void nuAuStlSchedWaitFrame(void) {
  NUScMsg* mesg_type;
  while (1) {
    osRecvMesg(&nuAuMesgQ, (OSMesg*)&mesg_type, OS_MESG_BLOCK);
    switch (*mesg_type) {
      case NU_SC_RETRACE_MSG:
        return;
      case NU_SC_PRENMI_MSG:
        if (nuAuPreNMIFunc) {
          (*nuAuPreNMIFunc)(NU_SC_PRENMI_MSG, nuAuPreNMI);
        }
        nuAuPreNMI++;
        break;
      default:
        break;
    }
  }
}

void nuAuStlSchedDoTask(musTask* task) {
  if (nuAuTaskStop == NU_AU_TASK_RUN) {
    nuAuTask.list.t.data_ptr = task->data;
    nuAuTask.list.t.data_size = task->data_size;

    nuAuTask.list.t.ucode = (u64*)task->ucode;
    nuAuTask.list.t.ucode_data = (u64*)task->ucode_data;
    osSendMesg(&nusched.audioRequestMQ, (OSMesg*)&nuAuTask, OS_MESG_BLOCK);
    osRecvMesg(&nuAuRtnMesgQ, NULL, OS_MESG_BLOCK);
  }

  if ((u32)nuAuPreNMIFunc && nuAuPreNMI) {
    (*nuAuPreNMIFunc)(NU_SC_RETRACE_MSG, nuAuPreNMI);
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
