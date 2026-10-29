#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <ultra64.h>

/*
 * SetFramebufferCount's counts. With two framebuffers, the third one's
 * memory holds the depth buffer, which the saved-frame effect draws into.
 */
#define DOUBLE_BUFFERING 2
#define TRIPLE_BUFFERING 3

/* Defined in the modified NuSystem's nugfxsetcfb.c */
void SetFramebufferCount(u32 count);

#endif
