#ifndef FRAMEBUFFER_COUNT_H
#define FRAMEBUFFER_COUNT_H

#include <ultra64.h>

/*
 * SetFramebufferCount's counts. With two framebuffers, the third one's
 * memory holds the depth buffer, which the saved-frame effect draws into.
 */
#define DOUBLE_BUFFERING 2
#define TRIPLE_BUFFERING 3

/* Defined by nugfxsetcfb.c, which the game's NuSystem replaces */
void SetFramebufferCount(u32 count);

#endif
