/*
 * The graphics thread: a scheduler client that, each frame, invokes the
 * application's registered draw callback on retrace and its pre-NMI callback
 * on reset. Plus the helper that creates and launches that thread.
 */

#include <nusys.h>

extern OSMesg nuGfxMesgBuf[NU_GFX_MESGS];
extern u64 GfxStack[NU_GFX_STACK_SIZE / 8];

/*
 * Graphics thread body. Registers for both retrace and pre-NMI events, then
 * loops forever dispatching whichever event arrives to the matching user hook.
 */
static void gfxThread(void* arg) {
  NUScClient gfx_client;
  NUScMsg* mesg_type;

  osCreateMesgQueue(&nuGfxMesgQ, nuGfxMesgBuf, NU_GFX_MESGS);
  nuScAddClient(&gfx_client, &nuGfxMesgQ, NU_SC_RETRACE_MSG | NU_SC_PRENMI_MSG);
  while (1) {
    (void)osRecvMesg(&nuGfxMesgQ, (OSMesg*)&mesg_type, OS_MESG_BLOCK);
    switch (*mesg_type) {
      case NU_SC_RETRACE_MSG:
        // Per-frame draw callback; passed the count of tasks still in flight.
        if (nuGfxFunc != NULL) {
          (*nuGfxFunc)(nuGfxTaskSpool);
        }
        break;

      case NU_SC_PRENMI_MSG:
        // Reset is imminent; let the application clean up.
        if (nuGfxPreNMIFunc != NULL) {
          (*nuGfxPreNMIFunc)();
        }
        break;

      default:
        break;
    }
  }
}

/* Create and start the graphics thread. */
void nuGfxThreadStart(void) {
  osCreateThread(&nuGfxThread, NU_GFX_THREAD_ID, gfxThread, (void*)NULL,
                 (GfxStack + NU_GFX_STACK_SIZE / 8), NU_GFX_THREAD_PRI);
  osStartThread(&nuGfxThread);
}
