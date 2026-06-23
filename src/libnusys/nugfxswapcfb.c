/*
 * Default frame-buffer swap callback.
 *
 * The Graphics Manager's stock task-completion handler: it queues the frame
 * buffer the just-finished task rendered into to be shown at the next retrace.
 * nuGfxInit registers this automatically; it is also the model a custom swap
 * callback follows.
 */
#include <nusys.h>

/*
 * Present the completed task's frame buffer.
 *
 * gfxTask is the NUScTask of the finished graphics task; its framebuffer
 * pointer is handed to the VI to become the next displayed image.
 */
void nuGfxSwapCfb(void* gfxTask) {
  NUScTask* gfxTaskPtr;

  gfxTaskPtr = (NUScTask*)gfxTask;
  osViSwapBuffer(gfxTaskPtr->framebuffer);
}
