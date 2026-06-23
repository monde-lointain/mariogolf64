/*
 * Frame-buffer swap callback registration.
 *
 * Installs the function the Graphics Manager calls when a graphics task
 * finishes, just before the displayed buffer is switched. nuGfxInit registers
 * the default (nuGfxSwapCfb) already, so most titles never call this directly;
 * override it only to customize how the next frame buffer is chosen.
 */
#include <nusys.h>

/* Register func as the task-completion swap callback (written under int mask).
 */
void nuGfxSwapCfbFuncSet(NUGfxSwapCfbFunc func) {
  OSIntMask mask;

  mask = osSetIntMask(OS_IM_NONE);
  nuGfxSwapCfbFunc = func;
  osSetIntMask(mask);
}
