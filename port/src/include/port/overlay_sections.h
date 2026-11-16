/*
 * Where the linker put each overlay's variables (see
 * port/cmake/OverlaySections.cmake).
 */

#ifndef PORT_OVERLAY_SECTIONS_H
#define PORT_OVERLAY_SECTIONS_H

typedef struct OverlaySections {
    unsigned int segmentNumber; /* As rom_file_offsets.h numbers it */
    unsigned char *dataStart;
    unsigned char *dataEnd;
    unsigned char *bssStart;
    unsigned char *bssEnd;
} OverlaySections;

/* Gives the overlays' sections and sets count to how many there are */
const OverlaySections *GetOverlaySections(int *count);

#endif
