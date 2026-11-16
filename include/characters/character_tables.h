/*
 * The host's copies of a character model's tables, for the native port.
 *
 * A model's file holds the tables that the CPU reads, its joints'
 * keyframes, its animations, its points and its animated triangles, laid
 * out for the N64: their pointers are 32-bit addresses in the TLB's mapping
 * of the model buffer, and their values are big-endian. On the N64 the
 * game reads them in place. A native build unpacks them into the host's
 * layout when it loads the model, and the model's accessors return the
 * copies. The display lists, vertices and textures stay in the buffer, in
 * the ROM's order, where the renderer reads them.
 */

#ifndef CHARACTER_TABLES_H
#define CHARACTER_TABLES_H

#include <ultra64.h>

#include "characters/character_model.h"

// The model's table at the segment address that tableIndex names, as the
// game reads it: the host's copy in a native build, and on the N64 the
// model's own, through the TLB's mapping of the slot's buffer
#ifdef TARGET_PC
#    define CharacterModelTable(model, tableIndex) FindCharacterTable((model), (tableIndex))
#else
#    define CharacterModelTable(model, tableIndex) ((model)->segmentAddresses[tableIndex])
#endif

#ifdef TARGET_PC

// Unpacks the tables of the model loaded into the slot's buffer, which holds
// at least bufferSize bytes and which CHARACTER_MODEL_ADDRESS maps. Replaces
// the copies that the slot held before.
void UnpackCharacterTables(
    s32 slotIndex,
    const CharacterModel *model,
    const void *buffer,
    u32 bufferSize
);

// The copy of the model's table at the segment address that tableIndex
// names, or NULL if the model has none there. Where slots hold the same
// model, the copy of the slot whose buffer CHARACTER_MODEL_ADDRESS maps.
void *FindCharacterTable(const CharacterModel *model, s32 tableIndex);

// Forgets the slot's copies, as its model is freed
void ForgetCharacterTables(s32 slotIndex);

#endif

#endif
