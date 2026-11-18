/*
 * Fills the game's arrays that hold data from the ROM, which a native build
 * reads from the player's ROM (see system/rom_arrays.h).
 */

#include "system/rom_arrays.h"

#include <ultra64.h>

#define LIST_ROM_ARRAY(name) &name##_ROM_ARRAY,

static const OSPcRomArray *const f_RomArrays[] = {FOR_EACH_ROM_ARRAY(LIST_ROM_ARRAY)};

void FillGameRomArrays(void) {
    size_t i;

    for (i = 0; i < sizeof(f_RomArrays) / sizeof(f_RomArrays[0]); i++) {
        osPcFillRomArray(f_RomArrays[i]);
    }
}
