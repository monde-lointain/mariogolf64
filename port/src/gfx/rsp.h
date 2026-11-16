/*
 * Running a graphics task: the RSP's walk of its display lists, which hands
 * the RDP's commands and primitives to the RDP's half.
 */

#ifndef PORT_GFX_RSP_H
#define PORT_GFX_RSP_H

#include <ultra64.h>

void InitRsp(void);

// Runs the display list to its end and sends what it drew to the GPU
void RunRspTask(const Gfx *displayList);

#endif
