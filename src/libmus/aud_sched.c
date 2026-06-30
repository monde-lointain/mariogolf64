/*
 * aud_sched.c
 *
 * libmus's default audio-frame scheduler back-end: it paces the audio thread
 * off the RCP task scheduler the game is already running. The back-end joins
 * that scheduler as a client to learn when each VI retrace occurs, and hands
 * the per-frame audio synthesis task to the RSP, blocking until it completes.
 *
 * The audio thread reaches these routines indirectly through the musSched
 * vtable (__libmus_current_sched), so a game may substitute its own scheduling
 * back-end; this file supplies the stock OSSched-based one.
 */

#include <sched.h>
#include "libmus.h"
#include "lib_memory.h"
#include "aud_sched.h"

/* Depth of the retrace and task-completion message queues. */
#define QUEUE_SIZE 4

/* Stock back-end operations, wired into default_sched below. */
static void __OsSchedInstall(void);
static void __OsSchedWaitFrame(void);
static void __OsSchedDoTask(musTask* task);

/* State this back-end keeps alive for the lifetime of the audio thread. */
typedef struct {
  OSScClient client;       /* scheduler-client registration */
  OSMesgQueue frame_queue; /* VI-retrace notifications land here */
  OSMesg frame_messages[QUEUE_SIZE];
  OSMesgQueue task_queue; /* audio-task completions land here */
  OSMesg task_messages[QUEUE_SIZE];
} ossched_workspace_t;

/*
 * Globals defined in the audio data segment:
 *   audio_sched - the game's OS scheduler, recorded by __MusIntSchedInit
 *   sched_mem   - this back-end's workspace, allocated from the audio heap
 */
extern OSSched* audio_sched;
extern ossched_workspace_t* sched_mem;

/* Default vtable: the three operations the audio thread drives each frame. */
static musSched default_sched = {__OsSchedInstall, __OsSchedWaitFrame,
                                 __OsSchedDoTask};

/* Back-end the audio thread currently uses; a game may point this elsewhere. */
musSched* __libmus_current_sched = &default_sched;

/* Remember which OS scheduler the audio thread should attach to. */
void __MusIntSchedInit(void* sched) { audio_sched = (OSSched*)sched; }

/*
 * Join the OS scheduler as a client; called once before the frame loop.
 *
 * Allocates the workspace, opens the retrace and task-completion queues, and
 * registers so the scheduler posts a message to frame_queue on every retrace.
 */
static void __OsSchedInstall(void) {
  sched_mem = __MusIntMemMalloc(sizeof(ossched_workspace_t));
  osCreateMesgQueue(&sched_mem->frame_queue, &sched_mem->frame_messages[0],
                    QUEUE_SIZE);
  osCreateMesgQueue(&sched_mem->task_queue, &sched_mem->task_messages[0],
                    QUEUE_SIZE);
  osScAddClient(audio_sched, &sched_mem->client, &sched_mem->frame_queue);
}

/*
 * Block the audio thread until the next VI retrace.
 *
 * The blocking receive waits for a message; the non-blocking one discards a
 * second if the queue has backed up, so the thread resumes on the most recent
 * frame instead of chasing a backlog. Non-retrace messages (e.g. pre-NMI) are
 * ignored.
 */
static void __OsSchedWaitFrame(void) {
  OSScMsg* message;
  do {
    osRecvMesg(&sched_mem->frame_queue, (OSMesg*)&message, OS_MESG_BLOCK);
    osRecvMesg(&sched_mem->frame_queue, NULL, OS_MESG_NOBLOCK);
  } while (message->type != OS_SC_RETRACE_MSG);
}

/*
 * Submit one frame's audio task to the RSP and wait for it to finish.
 *
 * Describes the audio command list plus the boot and synthesis microcode as an
 * OSScTask, hands it to the scheduler's command queue, then blocks until the
 * scheduler reports the task done on task_queue.
 */
static void __OsSchedDoTask(musTask* task) {
  OSScTask t;
  OSScMsg message;
  t.next = 0;
  t.msgQ = &sched_mem->task_queue;
  t.msg = &message;
  t.flags = OS_SC_NEEDS_RSP; /* audio drives the RSP only, never the RDP */
  t.list.t.data_ptr = task->data;
  t.list.t.data_size = task->data_size;
  t.list.t.type = M_AUDTASK;
  t.list.t.ucode_boot = (u64*)rspbootTextStart;
  t.list.t.ucode_boot_size = ((int)rspbootTextEnd - (int)rspbootTextStart);
  t.list.t.flags = 0;
  t.list.t.ucode = task->ucode;
  t.list.t.ucode_data = task->ucode_data;
  t.list.t.ucode_size = 4096;
  t.list.t.ucode_data_size = SP_UCODE_DATA_SIZE;
  /* DRAM matrix stack and framebuffer output are graphics-only; an audio
   * task leaves them empty. */
  t.list.t.dram_stack = (u64*)NULL;
  t.list.t.dram_stack_size = 0;
  t.list.t.output_buff = (u64*)NULL;
  t.list.t.output_buff_size = 0;
  /* Audio tasks are short enough that the scheduler never yields them. */
  t.list.t.yield_data_ptr = NULL;
  t.list.t.yield_data_size = 0;

  osSendMesg(osScGetCmdQ(audio_sched), (OSMesg)&t, OS_MESG_BLOCK);
  osRecvMesg(&sched_mem->task_queue, NULL, OS_MESG_BLOCK);
}
