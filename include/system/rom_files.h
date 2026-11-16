/*
 * The files that the game reads from ROM, by the names of the symbols that
 * the linker script defines at their bounds.
 *
 * On the N64 a file's NAME_ROM_START and NAME_ROM_END are symbols whose
 * addresses are its offsets in ROM, and the code reads those addresses as
 * numbers. The native port reads the files from the original ROM, so it
 * defines the names as those offsets instead (see port/cmake), and the
 * declarations below declare nothing.
 */

#ifndef ROM_FILES_H
#define ROM_FILES_H

#include <ultra64.h>

#ifdef TARGET_PC
#    include <rom_file_offsets.h>

// Declares the bounds of the file at assets/PATH.bin, PATH's slashes
// written as underscores
#    define DECLARE_ROM_FILE(name) extern int g_UnusedRomFileDeclaration
#else
#    define DECLARE_ROM_FILE(name) extern u8 name##_ROM_START[], name##_ROM_END[]

// The names that the game's code gives some of the files
extern u8 g_SoundPointerBankRomStart[];
extern u8 g_SoundPointerBankRomEnd[];
extern u8 g_SoundWaveBankRomStart[];
extern u8 g_SoundEffectBankRomStart[];
extern u8 g_SoundEffectBankRomEnd[];
extern u8 g_AssetArchiveRomStart[];
extern u8 g_GlyphFileRomStart[];
extern u8 g_MessageFileRomStart[];
extern u8 g_StaffTextRomStart[];
#endif

#endif
