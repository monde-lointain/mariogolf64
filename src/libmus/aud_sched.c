/*
 * aud_sched.c
 *
 * Built-in libmus audio scheduler: it bridges the RSP audio task to the
 * libultra OS scheduler (OSSched). It registers as a scheduler client to
 * receive video-retrace notifications, blocks the audio thread until each
 * retrace, and submits one audio RSP job per frame, waiting for it to finish.
 * The three routines are wired into a musSched vtable so the audio thread
 * reaches them indirectly through __libmus_current_sched.
 */
#include <sched.h>
#include "libmus.h"
#include "lib_memory.h"
#include "aud_sched.h"

// Depth of the per-client retrace and task-done message queues.
#define QUEUE_SIZE 4

// The default vtable's three entry points (see __libmus_current_sched).
static void __OsSchedInstall(void);
static void __OsSchedWaitFrame(void);
static void __OsSchedDoTask(musTask* task);

/*
 * Working set for the built-in scheduler client: the OSSched client
 * registration plus two message queues, one fed retrace notifications and one
 * that receives the acknowledgement when a dispatched RSP task completes.
 */
typedef struct {
  OSScClient client;
  OSMesgQueue frame_queue;
  OSMesg frame_messages[QUEUE_SIZE];
  OSMesgQueue task_queue;
  OSMesg task_messages[QUEUE_SIZE];
} ossched_workspace_t;

extern OSSched* g_mus_sched_ptr;
extern ossched_workspace_t* sched_mem;

// Built-in scheduler implementation, installed unless a game overrides it.
static musSched default_sched = {__OsSchedInstall, __OsSchedWaitFrame,
                                 __OsSchedDoTask};
musSched* __libmus_current_sched = &default_sched;

/* Scheduler binding: remember the host game's OS scheduler so install() can
 * register against it. */
void __MusIntSchedInit(void* sched) { g_mus_sched_ptr = (OSSched*)sched; }

/*
 * Install handler: allocate the client workspace, create the retrace and
 * task-done queues, and join the OS scheduler so this client starts receiving
 * retrace messages.
 */
static void __OsSchedInstall(void) {
  sched_mem = __MusIntMemMalloc(sizeof(ossched_workspace_t));
  osCreateMesgQueue(&sched_mem->frame_queue, &sched_mem->frame_messages[0],
                    QUEUE_SIZE);
  osCreateMesgQueue(&sched_mem->task_queue, &sched_mem->task_messages[0],
                    QUEUE_SIZE);
  osScAddClient(g_mus_sched_ptr, &sched_mem->client, &sched_mem->frame_queue);
}

/*
 * Wait-frame handler: block the audio thread until the next video retrace. Each
 * pass takes one message (blocking) and then drains a second one (non-blocking)
 * so the fixed-size queue cannot back up, looping until the message taken is a
 * retrace notification.
 */
static void __OsSchedWaitFrame(void) {
  OSScMsg* message;
  do {
    osRecvMesg(&sched_mem->frame_queue, (OSMesg*)&message, OS_MESG_BLOCK);
    osRecvMesg(&sched_mem->frame_queue, NULL, OS_MESG_NOBLOCK);
  } while (message->type != OS_SC_RETRACE_MSG);
}

/*
 * Do-task handler: describe one audio RSP job and submit it to the scheduler,
 * then block until the scheduler reports the task done on task_queue. The job
 * runs the rspboot loader followed by the audio synthesis microcode over the
 * caller's command list; it needs the RSP but no DRAM stack, output buffer, or
 * yield buffer.
 */
static void __OsSchedDoTask(musTask* task) {
  OSScTask t;
  OSScMsg message;

  t.next = 0;
  t.msgQ = &sched_mem->task_queue;
  t.msg = &message;
  t.flags = OS_SC_NEEDS_RSP;

  // The generated audio command list to execute.
  t.list.t.data_ptr = task->data;
  t.list.t.data_size = task->data_size;
  t.list.t.type = M_AUDTASK;

  // rspboot loader: the bytes between its start and end symbols.
  t.list.t.ucode_boot = (u64*)rspbootTextStart;
  t.list.t.ucode_boot_size = ((int)rspbootTextEnd - (int)rspbootTextStart);
  t.list.t.flags = 0;

  // Audio synthesis microcode (fixed 4 KB text, standard data size).
  t.list.t.ucode = task->ucode;
  t.list.t.ucode_data = task->ucode_data;
  t.list.t.ucode_size = 4096;
  t.list.t.ucode_data_size = SP_UCODE_DATA_SIZE;

  // Audio tasks use none of the stack/output/yield facilities.
  t.list.t.dram_stack = (u64*)NULL;
  t.list.t.dram_stack_size = 0;
  t.list.t.output_buff = (u64*)NULL;
  t.list.t.output_buff_size = 0;
  t.list.t.yield_data_ptr = NULL;
  t.list.t.yield_data_size = 0;

  // Hand the job to the scheduler and wait for it to finish.
  osSendMesg(osScGetCmdQ(g_mus_sched_ptr), (OSMesg)&t, OS_MESG_BLOCK);
  osRecvMesg(&sched_mem->task_queue, NULL, OS_MESG_BLOCK);
}
