/*
 * The arrays of the game's code that hold data from the ROM: the textures
 * that textures.txt lists.
 *
 * The N64 build initializes each from a file that tools/extract_textures.py
 * cuts from the ROM, so that the array lies in the code's data in the ROM.
 * A native build holds no copy of the original's data: its initializers
 * hold zeros, and FillGameRomArrays fills the arrays from the player's ROM
 * at start-up with the values that the N64 build's give them.
 *
 * DEFINE_ROM_ARRAY(NAME, array) gives a native build the array of the
 * texture NAME.FORMAT.png, which it fills from the offset that textures.txt
 * gives it. On the N64 it declares nothing.
 */

#ifndef ROM_ARRAYS_H
#define ROM_ARRAYS_H

#include <ultra64.h>

#ifdef TARGET_PC
#    include <rom_file_offsets.h>

#    define DEFINE_ROM_ARRAY(name, array) \
        const OSPcRomArray name##_ROM_ARRAY = \
            {(array), sizeof(array), sizeof((array)[0]), name##_ROM_OFFSET}

#    define DECLARE_ROM_ARRAY(name) extern const OSPcRomArray name##_ROM_ARRAY;
FOR_EACH_ROM_ARRAY(DECLARE_ROM_ARRAY)
#    undef DECLARE_ROM_ARRAY

// Fills every array that textures.txt lists from the inserted cartridge.
// Call it before PcInitOs, which keeps the overlays' arrays as they are
// then, to put back when the game loads an overlay again.
void FillGameRomArrays(void);
#else
#    define DEFINE_ROM_ARRAY(name, array) extern int g_UnusedRomArrayDeclaration
#endif

#endif
