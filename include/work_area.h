#ifndef WORK_AREA_H
#define WORK_AREA_H

#include <ultra64.h>
#include "hole_triangles.h"

/* Bytes in g_WorkArea, part of which holds the characters' buffers */
#define WORK_AREA_SIZE 0x827E0

/*
 * A buffer whose parts serve the characters, the frames' vertices and,
 * while a hole is played, the hole's triangles
 */
typedef union WorkArea {
    u8 bytes[WORK_AREA_SIZE];
    struct {
        /* 0x00000 */ u8 unk0[HOLE_TRIANGLE_OFFSET];
        /* 0x5A520 */ HoleTriangle triangles[HOLE_TRIANGLE_COUNT];
    } hole;
} WorkArea;

extern WorkArea g_WorkArea;

#endif
