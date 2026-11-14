/*
 * Passing files to the host computer of development hardware. The commands
 * go nowhere in the released game, so reads always fail.
 */

#ifndef HOST_FILE_H
#define HOST_FILE_H

#include <ultra64.h>

// Results of WriteHostFile and ReadHostFile; ReadHostFile also returns 1
#define HOST_FILE_OK 0
#define HOST_FILE_SIZE_TOO_SMALL 1
#define HOST_FILE_NAME_TOO_SHORT 2
#define HOST_FILE_NAME_TOO_LONG 3

// Ends the text files written to the host
#define END_OF_FILE_CHARACTER 0x1A

s32 WriteHostFile(const char *fileName, u32 size);
s32 ReadHostFile(const char *fileName, u32 *size);

#endif
