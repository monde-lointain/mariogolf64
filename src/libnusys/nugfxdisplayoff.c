/*
 * Screen blanking.
 *
 * Forces the video output to black and tells the Graphics Manager not to
 * present frames. Pair with nuGfxDisplayOn to bring the screen back.
 */
#include <nusys.h>

/* Mark the display off and immediately black the VI output. */
void nuGfxDisplayOff(void) {
  nuGfxDisplay = NU_GFX_DISPLAY_OFF;
  osViBlack(TRUE);
}
