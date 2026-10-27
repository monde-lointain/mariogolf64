/*
 * Sets up NuSystem's access to the cartridge's SRAM.
 */
#include "common.h"

#include "sram.h"

#include <lib/libnusys/nusys.h>

void InitSram(void) {
  nuPiInitSram();
}
