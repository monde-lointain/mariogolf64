#include "common.h"

extern OSMesgQueue fault_event_queue;
extern OSThread* current_faulted_thread;
extern s32 D_800DF0B4;
extern const char FAULT_MONITOR_WAIT_MESSAGE[];
extern const char FAULT_MONITOR_EVENT_MESSAGE[];
extern const char thread_context_report_buffer[];

extern OSThread* __osGetCurrFaultedThread(void);
extern void play_sound_effect(s32 sound_id, s32 channel, s32 volume);
extern void func_8005E380(OSThread* thread);

/* Preserve the existing empty body and volatile frame reservation. */
void fault_monitor_noop(void) { volatile u8 reserved_frame[0x18]; }

void monitor_thread_faults(void) {
  osSetEventMesg(OS_EVENT_FAULT, &fault_event_queue, (OSMesg)0x10);
  D_800DF0B4 = 0;
  while (1) {
    OSMesg event_message;

    osSyncPrintf(FAULT_MONITOR_WAIT_MESSAGE);
    osRecvMesg(&fault_event_queue, &event_message, OS_MESG_BLOCK);
    osSyncPrintf(FAULT_MONITOR_EVENT_MESSAGE);
    current_faulted_thread = __osGetCurrFaultedThread();
    if (current_faulted_thread != NULL) {
      play_sound_effect(0x30, 0xB, 0x7F);
      func_8005E380(current_faulted_thread);
    }
  }
}

void print_thread_context_buffer(void) {
  osSyncPrintf(thread_context_report_buffer);
}
