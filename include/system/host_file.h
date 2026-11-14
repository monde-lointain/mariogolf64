/*
 * Passing files to the host computer of development hardware. The commands
 * go nowhere in the released game, so reads always fail.
 */

#ifndef HOST_FILE_H
#define HOST_FILE_H

#include <ultra64.h>

// Results of WriteHostFile and ReadHostFile; ReadHostFile also returns 1
enum HostFileResult {
    HOST_FILE_OK,
    HOST_FILE_SIZE_TOO_SMALL,
    HOST_FILE_NAME_TOO_SHORT,
    HOST_FILE_NAME_TOO_LONG
};

typedef s32 HostFileResult;

// Ends the text files written to the host
#define END_OF_FILE_CHARACTER 0x1A

HostFileResult WriteHostFile(const char *fileName, u32 size);
HostFileResult ReadHostFile(const char *fileName, u32 *size);

#endif
