/*
 * Screen enable.
 *
 * Re-enables presentation after nuGfxDisplayOff. The actual unblanking is
 * deferred: the trigger state is picked up when the next graphics task finishes
 * and swaps in its frame buffer, so the screen returns in sync with a real
 * frame.
 */
#include <nusys.h>

/* Arm the display so the next completed graphics task brings the screen back.
 */
void nuGfxDisplayOn(void) { nuGfxDisplay = NU_GFX_DISPLAY_ON_TRIGGER; }
