/*
 * Sets up NuSystem's access to the cartridge's SRAM.
 */
#include "sram.h"

#include <nusys.h>
#include <ultra64.h>

void InitSram(void) {
    nuPiInitSram();
}
