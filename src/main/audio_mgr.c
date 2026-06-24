/*
 * Audio manager glue (game audio TU).
 *
 * Sets up and drives the SoundTools (libmus) audio task. The four nuAuStl*
 * routines are the NuSYS audio-manager layer: nuAuStlMgrInit builds the audio
 * RSP task template and hands a scheduler callback trio to libmus, and that
 * trio (Install / WaitFrame / DoTask) is what the NuSYS scheduler calls each
 * frame to run audio. The two bgm_* helpers are the game's background-music
 * wrappers over the libmus heap and song player.
 *
 * The nuAuStl* code is a verbatim mirror of NuSYS nualstl3 (nuaustlmgr.c); its
 * file-scope state is defined elsewhere in the ROM, so it is declared extern
 * here (drop-def) and the scheduler callbacks are given external linkage so the
 * audio task's data table can resolve to them.
 */
#include "common.h"
#include <nusys.h>
#include <nualstl.h>

/* nuaustlmgr.c file-scope state, declared extern here (drop-def). */
extern OSMesgQueue nuAuRtnMesgQ; /* task-completion reply queue */
extern OSMesg nuAuMesgBuf[]; /* scheduler-message backing store for nuAuMesgQ */
extern OSMesg nuAuRtnMesgBuf; /* single-slot backing store for nuAuRtnMesgQ */

/*
 * Scheduler callback trio, registered with libmus through nuAuStlSchedFunc.
 * Declared with external linkage (not static, unlike the upstream source) so
 * the recovered nuAuStlSchedFunc table in ROM resolves to these addresses.
 */
void nuAuStlSchedInstall(void);
void nuAuStlSchedWaitFrame(void);
void nuAuStlSchedDoTask(musTask* task);

extern musSched nuAuStlSchedFunc;

/*
 * Initialize the audio manager: build the audio task template, register the
 * scheduler callbacks, and start libmus. Returns the size of the heap libmus
 * claimed from the supplied configuration.
 */
s32 nuAuStlMgrInit(musConfig* config) {
  nuAuTaskStop = NU_AU_TASK_RUN;

  /* Fill in the parts of the audio task that never change frame to frame. The
   * boot microcode is the shared RSP boot stub; its size is the span between
   * the two boundary symbols. The remaining task fields are filled per frame in
   * nuAuStlSchedDoTask. */
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

/*
 * Scheduler "install" callback: create the audio manager's message queues and
 * subscribe to retrace and pre-NMI notifications from the NuSYS scheduler.
 */
void nuAuStlSchedInstall(void) {
  osCreateMesgQueue(&nuAuMesgQ, nuAuMesgBuf, NU_AU_MESG_MAX);
  osCreateMesgQueue(&nuAuRtnMesgQ, &nuAuRtnMesgBuf, 1);
  nuScAddClient(&nuAuClient, &nuAuMesgQ, NU_SC_RETRACE_MSG | NU_SC_PRENMI_MSG);
}

/*
 * Scheduler "wait frame" callback: block until the next retrace, servicing any
 * pre-NMI messages that arrive first.
 *
 * A retrace message means the frame is ready, so return. A pre-NMI message
 * means the system is about to reset; invoke the user callback (if registered)
 * and keep waiting. nuAuPreNMI counts the pre-NMI notifications passed to that
 * callback.
 */
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

/*
 * Scheduler "do task" callback: dispatch one audio task to the NuSYS scheduler,
 * unless audio has been stopped.
 *
 * The per-frame task fields (the command list and microcode pointers) come from
 * the libmus task; the request is posted to the scheduler's audio queue and we
 * block on the reply queue until it completes. After a pre-NMI has occurred,
 * the user callback is then driven on every subsequent frame.
 */
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

/* Allocate the background-music song buffer from the libmus heap. */
void bgm_alloc_song_buffer(s32 size) {
  g_bgm_song_buffer = (u32)__MusIntMemMalloc(size);
}

/* Start playback of the song in the background-music buffer. */
musHandle bgm_start_current(void) {
  return MusStartSong((void*)g_bgm_song_buffer);
}
