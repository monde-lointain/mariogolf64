/*
 * Retrace delay.
 *
 * Blocks the caller for a fixed number of vertical retraces, a simple way to
 * pace startup or pause logic to the video frame rate without running a render
 * loop.
 */
#include <nusys.h>

/*
 * Wait for retrace_num vertical retraces, then return.
 *
 * A temporary scheduler client is registered on a one-slot queue that receives
 * a message every retrace; the loop consumes one message per retrace until the
 * count is exhausted. The client is removed before returning so it does not
 * keep receiving messages.
 */
void nuGfxRetraceWait(u32 retrace_num) {
  NUScClient client;
  OSMesg mesgBuf;
  OSMesgQueue mesgQ;

  osCreateMesgQueue(&mesgQ, &mesgBuf, 1);
  nuScAddClient(&client, &mesgQ, NU_SC_RETRACE_MSG);
  while (retrace_num) {
    osRecvMesg(&mesgQ, NULL, OS_MESG_BLOCK);
    retrace_num--;
  }
  nuScRemoveClient(&client);
}
