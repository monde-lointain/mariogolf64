/*
 * Set the vertical scaling factor for the next display.
 *
 * Stretches the rendered lines to fill the screen: the image is shown enlarged
 * by 1/factor. Mainly used to letterbox-correct an NTSC layout shown on PAL.
 */

#include "PR/os_internal.h"
#include "PR/ultraerror.h"
#include "PR/ultralog.h"
#include "viint.h"

/* value is the y-scale factor, valid 0.05..1.0; out-of-range input is clamped
 * (debug build also logs it). Only meaningful in low-res non-interlaced modes.
 */
void osViSetYScale(f32 value) {
  register u32 saveMask;

#ifdef _DEBUG
  if (!__osViDevMgr.active) {
    __osError(ERR_OSVISETYSCALE_VIMGR, 0);
    return;
  }
  if ((value < 0.05f) || (value > 1.0f)) {
    __osError(ERR_OSVISETYSCALE_VALUE, 1, OS_LOG_FLOAT(value));
    if (value < 0.05f) {
      value = 0.05f;
    } else {
      value = 1.0f;
    }
  }
#endif

  // Stage the factor; the retrace handler folds it into the Y_SCALE register
  // when it sees YSCALE_UPDATED.
  saveMask = __osDisableInt();
  __osViNext->y.factor = value;
  __osViNext->state |= VI_STATE_YSCALE_UPDATED;
  __osRestoreInt(saveMask);
}
