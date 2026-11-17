/*
 * The cartridge's save memory, kept in a file between runs: nusys_host.h's
 * PcLoadSaveMemory and PcStoreSaveMemory.
 */

#ifndef PORT_PLATFORM_SAVE_MEMORY_H
#define PORT_PLATFORM_SAVE_MEMORY_H

#include <stdbool.h>
#include <stdint.h>

// Keeps the save memory in the file at path from now on; NULL turns saving
// off. The path is copied.
void SetSaveMemoryPath(const char *path);

// The usual place for the save memory: the user's data directory for the
// port. Gives a path to release with SDL_free, or NULL when there is none.
char *FindDefaultSaveMemoryPath(void);

// The CRC-32 of the save that the game would load now, size bytes long;
// false when it would find none and start afresh
bool ChecksumSaveMemory(uint32_t size, uint32_t *crc);

#endif
